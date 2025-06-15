#include <cppSwitchboard/http2_server_impl.h>
#include <iostream>
#include <fstream>
#include <boost/asio/ssl/error.hpp>

namespace cppSwitchboard {

// Http2Session implementation
Http2Session::Http2Session(tcp::socket socket, ssl::context* ssl_ctx,
                          std::function<HttpResponse(const HttpRequest&)> request_processor,
                          std::shared_ptr<DebugLogger> debugLogger)
    : socket_(std::move(socket)), request_processor_(request_processor), debugLogger_(debugLogger),
      read_buffer_(8192), write_buffer_(8192) {
    
    if (ssl_ctx) {
        ssl_stream_ = std::make_unique<ssl::stream<tcp::socket&>>(socket_, *ssl_ctx);
    }
    
    // Setup nghttp2 session
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    
    nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
    nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers_callback);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
    
    nghttp2_session_server_new(&session_, callbacks, this);
    nghttp2_session_callbacks_del(callbacks);
    
    // Send initial settings
    nghttp2_settings_entry settings[] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
        {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 65535}
    };
    nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, settings, 2);
}

Http2Session::~Http2Session() {
    if (session_) {
        nghttp2_session_del(session_);
    }
}

void Http2Session::start() {
    if (ssl_stream_) {
        do_handshake();
    } else {
        do_read();
    }
}

void Http2Session::do_handshake() {
    auto self = shared_from_this();
    ssl_stream_->async_handshake(ssl::stream_base::server,
        [this, self](boost::system::error_code ec) {
            if (!ec) {
                // Check what protocol was negotiated via ALPN
                SSL* ssl = ssl_stream_->native_handle();
                const unsigned char* alpn_proto = nullptr;
                unsigned int alpn_len = 0;
                
                SSL_get0_alpn_selected(ssl, &alpn_proto, &alpn_len);
                
                if (alpn_proto && alpn_len == 2 && memcmp(alpn_proto, "h2", 2) == 0) {
                    std::cout << "HTTP/2 negotiated via ALPN" << std::endl;
                    do_read();
                } else if (alpn_proto && alpn_len == 8 && memcmp(alpn_proto, "http/1.1", 8) == 0) {
                    std::cerr << "HTTP/1.1 negotiated, but this is an HTTP/2 only server" << std::endl;
                    // We could handle HTTP/1.1 here if needed, but for now reject
                } else {
                    std::cerr << "No supported protocol negotiated via ALPN" << std::endl;
                    if (alpn_proto) {
                        std::cerr << "Client offered: " << std::string(reinterpret_cast<const char*>(alpn_proto), alpn_len) << std::endl;
                    }
                }
            } else {
                std::cerr << "SSL handshake failed: " << ec.message() << std::endl;
            }
        });
}

void Http2Session::do_read() {
    auto self = shared_from_this();
    
    auto read_handler = [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
            ssize_t readlen = nghttp2_session_mem_recv(session_, 
                read_buffer_.data(), bytes_transferred);
            if (readlen < 0) {
                std::cerr << "nghttp2_session_mem_recv failed: " << nghttp2_strerror(readlen) << std::endl;
                return;
            }
            
            if (nghttp2_session_want_write(session_)) {
                do_write();
            } else {
                do_read();
            }
        } else if (ec != asio::error::eof) {
            std::cerr << "Read error: " << ec.message() << std::endl;
        }
    };
    
    if (ssl_stream_) {
        ssl_stream_->async_read_some(asio::buffer(read_buffer_), read_handler);
    } else {
        socket_.async_read_some(asio::buffer(read_buffer_), read_handler);
    }
}

void Http2Session::do_write() {
    const uint8_t* data;
    ssize_t datalen = nghttp2_session_mem_send(session_, &data);
    std::cout << "DEBUG: do_write() called, nghttp2_session_mem_send returned: " << datalen << " bytes" << std::endl;
    
    if (datalen > 0) {
        auto self = shared_from_this();
        
        std::cout << "DEBUG: Sending " << datalen << " bytes via async_write" << std::endl;
        
        auto write_handler = [this, self](boost::system::error_code ec, std::size_t bytes_written) {
            std::cout << "DEBUG: Write completed, bytes written: " << bytes_written << ", error: " << ec.message() << std::endl;
            if (!ec) {
                if (nghttp2_session_want_read(session_)) {
                    std::cout << "DEBUG: Session wants to read, calling do_read()" << std::endl;
                    do_read();
                } else if (nghttp2_session_want_write(session_)) {
                    std::cout << "DEBUG: Session wants to write more, calling do_write()" << std::endl;
                    do_write();
                } else {
                    std::cout << "DEBUG: Session doesn't want to read or write, operation complete" << std::endl;
                }
            } else {
                std::cerr << "Write error: " << ec.message() << std::endl;
            }
        };
        
        if (ssl_stream_) {
            asio::async_write(*ssl_stream_, asio::buffer(data, datalen), write_handler);
        } else {
            asio::async_write(socket_, asio::buffer(data, datalen), write_handler);
        }
    } else if (datalen == 0) {
        std::cout << "DEBUG: No data to send" << std::endl;
    } else {
        std::cerr << "ERROR: nghttp2_session_mem_send failed: " << nghttp2_strerror(datalen) << std::endl;
    }
}

void Http2Session::send_response(int32_t stream_id, const HttpResponse& response) {
    // Store header strings in session first to ensure lifetime
    std::vector<std::string>& header_storage = header_strings_[stream_id];
    header_storage.clear();
    
    // Reserve space to prevent reallocation and pointer invalidation
    // We need: 2 for :status (name + value) + 2 * response.getHeaders().size() for other headers
    header_storage.reserve(2 + 2 * response.getHeaders().size());
    
    // Also store nghttp2_nv structures persistently
    std::vector<nghttp2_nv>& headers = header_nvs_[stream_id];
    headers.clear();
    headers.reserve(1 + response.getHeaders().size()); // 1 for :status + other headers
    
    // Add status - ensure it's valid
    int status = response.getStatus();
    std::cout << "DEBUG: Original status from response: " << status << std::endl;
    
    if (status == 0) {
        status = 200; // Default to 200 OK if status is unset
        std::cout << "DEBUG: Status was 0, setting to 200" << std::endl;
    }
    
    // Store both status header name and value persistently
    header_storage.push_back(":status");          // index 0: header name
    header_storage.push_back(std::to_string(status)); // index 1: header value
    
    std::cout << "DEBUG: Status string stored: '" << header_storage[1] << "'" << std::endl;
    std::cout << "DEBUG: Status string length: " << header_storage[1].length() << std::endl;
    
    headers.push_back({(uint8_t*)header_storage[0].c_str(), (uint8_t*)header_storage[1].c_str(), 
                      header_storage[0].length(), header_storage[1].length(), NGHTTP2_NV_FLAG_NONE});
    
    std::cout << "DEBUG: :status header name pointer: " << (void*)header_storage[0].c_str() << std::endl;
    std::cout << "DEBUG: :status header value pointer: " << (void*)header_storage[1].c_str() << std::endl;
    std::cout << "DEBUG: :status header name: '" << std::string((char*)headers[0].name, headers[0].namelen) << "'" << std::endl;
    std::cout << "DEBUG: :status header value: '" << std::string((char*)headers[0].value, headers[0].valuelen) << "'" << std::endl;
    
    // Add response headers
    for (const auto& header : response.getHeaders()) {
        size_t name_idx = header_storage.size();
        size_t value_idx = header_storage.size() + 1;
        
        header_storage.push_back(header.first);
        header_storage.push_back(header.second);
        
        headers.push_back({(uint8_t*)header_storage[name_idx].c_str(), 
                          (uint8_t*)header_storage[value_idx].c_str(),
                          header_storage[name_idx].length(), 
                          header_storage[value_idx].length(), 
                          NGHTTP2_NV_FLAG_NONE});
        
        std::cout << "DEBUG: Added header: " << header_storage[name_idx] << ": " << header_storage[value_idx] << std::endl;
    }
    
    std::cout << "DEBUG: Total headers count: " << headers.size() << std::endl;
    
    // Double-check that our pointers are still valid after all the push_backs
    std::cout << "DEBUG: Final :status header name: '" << std::string((char*)headers[0].name, headers[0].namelen) << "'" << std::endl;
    std::cout << "DEBUG: Final :status header value: '" << std::string((char*)headers[0].value, headers[0].valuelen) << "'" << std::endl;
    
    std::string* response_body = new std::string(response.getBody());
    std::cout << "DEBUG: Response body length: " << response_body->length() << std::endl;
    
    if (!response_body->empty()) {
        nghttp2_data_provider data_prd;
        data_prd.source.ptr = response_body;
        data_prd.read_callback = data_source_read_callback;
        
        int rv = nghttp2_submit_response(session_, stream_id, headers.data(), headers.size(), &data_prd);
        std::cout << "DEBUG: nghttp2_submit_response (with body) returned: " << rv << std::endl;
        if (rv != 0) {
            std::cerr << "nghttp2_submit_response failed: " << nghttp2_strerror(rv) << std::endl;
            delete response_body;
        }
    } else {
        int rv = nghttp2_submit_response(session_, stream_id, headers.data(), headers.size(), nullptr);
        std::cout << "DEBUG: nghttp2_submit_response (no body) returned: " << rv << std::endl;
        if (rv != 0) {
            std::cerr << "nghttp2_submit_response failed: " << nghttp2_strerror(rv) << std::endl;
        }
        delete response_body;
    }
    
    // CRITICAL: After submitting the response, we need to trigger the write operation
    // to actually send the queued HTTP/2 frames to the client
    std::cout << "DEBUG: Triggering do_write() to send HTTP/2 frames" << std::endl;
    do_write();
}

// Static callbacks
ssize_t Http2Session::send_callback(nghttp2_session* session, const uint8_t* data,
                                   size_t length, int flags, void* user_data) {
    (void)session;
    (void)data;
    (void)length;
    (void)flags;
    (void)user_data;
    // Not used with mem_send
    return NGHTTP2_ERR_WOULDBLOCK;
}

int Http2Session::on_begin_headers_callback(nghttp2_session* session,
                                           const nghttp2_frame* frame,
                                           void* user_data) {
    (void)session;
    auto* sess = static_cast<Http2Session*>(user_data);
    if (frame->hd.type == NGHTTP2_HEADERS && 
        frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
        sess->streams_[frame->hd.stream_id] = StreamData{};
    }
    return 0;
}

int Http2Session::on_header_callback(nghttp2_session* session,
                                    const nghttp2_frame* frame,
                                    const uint8_t* name, size_t namelen,
                                    const uint8_t* value, size_t valuelen,
                                    uint8_t flags, void* user_data) {
    (void)session;
    (void)flags;
    auto* sess = static_cast<Http2Session*>(user_data);
    std::string header_name(reinterpret_cast<const char*>(name), namelen);
    std::string header_value(reinterpret_cast<const char*>(value), valuelen);
    
    auto& stream = sess->streams_[frame->hd.stream_id];
    
    if (header_name == ":method") {
        stream.method = header_value;
    } else if (header_name == ":path") {
        stream.path = header_value;
    } else if (header_name == ":scheme") {
        stream.scheme = header_value;
    } else if (header_name == ":authority") {
        stream.authority = header_value;
    } else {
        stream.headers[header_name] = header_value;
    }
    
    return 0;
}

int Http2Session::on_frame_recv_callback(nghttp2_session* session,
                                        const nghttp2_frame* frame,
                                        void* user_data) {
    (void)session;
    auto* sess = static_cast<Http2Session*>(user_data);
    
    if (frame->hd.type == NGHTTP2_HEADERS && 
        frame->headers.cat == NGHTTP2_HCAT_REQUEST &&
        (frame->hd.flags & NGHTTP2_FLAG_END_HEADERS)) {
        
        auto& stream = sess->streams_[frame->hd.stream_id];
        stream.headers_complete = true;
        
        std::cout << "DEBUG: Received HEADERS frame for stream " << frame->hd.stream_id << std::endl;
        std::cout << "DEBUG: Method: '" << stream.method << "'" << std::endl;
        std::cout << "DEBUG: Path: '" << stream.path << "'" << std::endl;
        std::cout << "DEBUG: Scheme: '" << stream.scheme << "'" << std::endl;
        std::cout << "DEBUG: Authority: '" << stream.authority << "'" << std::endl;
        
        // If no body expected or END_STREAM flag, process request
        if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM) || stream.method == "GET") {
            // Create HttpRequest
            HttpRequest request(stream.method, stream.path, "HTTP/2");
            request.setStreamId(frame->hd.stream_id);
            
            for (const auto& header : stream.headers) {
                request.setHeader(header.first, header.second);
            }
            
            if (!stream.body.empty()) {
                request.setBody(stream.body);
            }
            
            std::cout << "DEBUG: Processing request for path: " << stream.path << std::endl;
            
            // Debug log request headers and payload
            if (sess->debugLogger_) {
                sess->debugLogger_->logRequestHeaders(request);
                sess->debugLogger_->logRequestPayload(request);
            }
            
            // Process request
            HttpResponse response;
            try {
                response = sess->request_processor_(request);
                
                std::cout << "DEBUG: Response from processor - Status: " << response.getStatus() << std::endl;
                std::cout << "DEBUG: Response from processor - Body length: " << response.getBody().length() << std::endl;
                
                // Ensure response has a valid status
                if (response.getStatus() == 0) {
                    std::cout << "DEBUG: Response status was 0, setting to 200" << std::endl;
                    response.setStatus(200);
                }
                
                // Ensure content-type is set if there's a body
                if (!response.getBody().empty() && response.getHeader("content-type").empty()) {
                    std::cout << "DEBUG: Setting content-type to text/plain" << std::endl;
                    response.setHeader("content-type", "text/plain");
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Error processing request: " << e.what() << std::endl;
                response.setStatus(500);
                response.setHeader("content-type", "text/plain");
                response.setBody("Internal Server Error");
            }
            
            std::cout << "DEBUG: Final response status before send_response: " << response.getStatus() << std::endl;
            
            // Debug log response headers and payload before sending
            if (sess->debugLogger_) {
                sess->debugLogger_->logResponseHeaders(response, request.getPath(), request.getMethod());
                sess->debugLogger_->logResponsePayload(response, request.getPath(), request.getMethod());
            }
            
            sess->send_response(frame->hd.stream_id, response);
            
            // IMPORTANT: Don't clean up stream data until after the response is fully sent
            // Clean up will happen in on_stream_close_callback
            // sess->streams_.erase(frame->hd.stream_id);
            // sess->header_strings_.erase(frame->hd.stream_id);
        }
    } else if (frame->hd.type == NGHTTP2_DATA) {
        // Handle request body data
        // Note: This is a simplified implementation
        // In a full implementation, you'd accumulate the body data
    }
    
    return 0;
}

ssize_t Http2Session::data_source_read_callback(nghttp2_session* session, int32_t stream_id,
                                               uint8_t* buf, size_t length, uint32_t* data_flags,
                                               nghttp2_data_source* source, void* user_data) {
    (void)session;
    (void)stream_id;
    (void)user_data;
    auto* response_body = static_cast<std::string*>(source->ptr);
    size_t copy_len = std::min(length, response_body->length());
    std::memcpy(buf, response_body->data(), copy_len);
    *data_flags |= NGHTTP2_DATA_FLAG_EOF;
    delete response_body;
    return copy_len;
}

int Http2Session::on_stream_close_callback(nghttp2_session* session, int32_t stream_id,
                                          uint32_t error_code, void* user_data) {
    (void)session;
    auto* sess = static_cast<Http2Session*>(user_data);
    
    std::cout << "DEBUG: Stream " << stream_id << " closed with error code: " << error_code << std::endl;
    
    // Clean up stream data
    sess->streams_.erase(stream_id);
    sess->header_strings_.erase(stream_id);
    sess->header_nvs_.erase(stream_id);
    
    return 0;
}

// Http2Server implementation
Http2Server::Http2Server(asio::io_context& ioc, const ServerConfig& config,
                        std::function<HttpResponse(const HttpRequest&)> request_processor)
    : ioc_(ioc), acceptor_(ioc), ssl_ctx_(ssl::context::tlsv12_server),
      request_processor_(request_processor), config_(config), running_(false) {
    
    // Initialize debug logger
    debugLogger_ = std::make_shared<DebugLogger>(config_.monitoring.debugLogging);
    
    // Setup acceptor
    tcp::endpoint endpoint(asio::ip::make_address(config_.http2.bindAddress), 
                          static_cast<unsigned short>(config_.http2.port));
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    
    if (config_.ssl.enabled) {
        setup_ssl_context();
    }
}

void Http2Server::setup_ssl_context() {
    ssl_ctx_.set_options(ssl::context::default_workarounds |
                        ssl::context::no_sslv2 |
                        ssl::context::no_sslv3 |
                        ssl::context::single_dh_use);
    
    // Load certificate and key
    std::string cert_path = config_.ssl.certificateFile;
    std::string key_path = config_.ssl.privateKeyFile;
    
    try {
        ssl_ctx_.use_certificate_chain_file(cert_path);
        ssl_ctx_.use_private_key_file(key_path, ssl::context::pem);
        
        // Set up ALPN for HTTP/2 negotiation
        SSL_CTX* native_ctx = ssl_ctx_.native_handle();
        
        // Set ALPN selection callback
        SSL_CTX_set_alpn_select_cb(native_ctx, 
            [](SSL* ssl, const unsigned char** out, unsigned char* outlen,
               const unsigned char* in, unsigned int inlen, void* arg) -> int {
                (void)ssl;
                (void)arg;
                
                // Our supported protocols: h2, http/1.1
                // Note: These variables are kept for documentation but not used in current implementation
                
                // Try to select h2 first, then http/1.1
                for (unsigned int i = 0; i < inlen; ) {
                    unsigned char proto_len = in[i];
                    if (i + 1 + proto_len > inlen) break;
                    
                    // Check for h2
                    if (proto_len == 2 && memcmp(&in[i + 1], "h2", 2) == 0) {
                        *out = &in[i + 1];
                        *outlen = proto_len;
                        return SSL_TLSEXT_ERR_OK;
                    }
                    
                    // Check for http/1.1
                    if (proto_len == 8 && memcmp(&in[i + 1], "http/1.1", 8) == 0) {
                        *out = &in[i + 1];
                        *outlen = proto_len;
                        return SSL_TLSEXT_ERR_OK;
                    }
                    
                    i += 1 + proto_len;
                }
                
                // If no supported protocol found, reject
                return SSL_TLSEXT_ERR_NOACK;
            }, nullptr);
        
        // Also set the protocols we advertise
        const unsigned char alpn_protos[] = {
            2, 'h', '2',                    // h2
            8, 'h', 't', 't', 'p', '/', '1', '.', '1'  // http/1.1
        };
        
        if (SSL_CTX_set_alpn_protos(native_ctx, alpn_protos, sizeof(alpn_protos)) != 0) {
            std::cerr << "Warning: Failed to set ALPN protocols" << std::endl;
        }
            
    } catch (const std::exception& e) {
        std::cerr << "SSL setup error: " << e.what() << std::endl;
        throw;
    }
}

void Http2Server::start() {
    running_ = true;
    acceptor_.listen();
    do_accept();
}

void Http2Server::stop() {
    running_ = false;
    acceptor_.close();
}

void Http2Server::do_accept() {
    if (!running_) return;
    
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<Http2Session>(
                    std::move(socket), 
                    config_.ssl.enabled ? &ssl_ctx_ : nullptr,
                    request_processor_,
                    debugLogger_);
                session->start();
            }
            
            if (running_) {
                do_accept();
            }
        });
}

} // namespace cppSwitchboard 