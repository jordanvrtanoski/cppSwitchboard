/**
 * @file http2_server_impl.h
 * @brief HTTP/2 server implementation using nghttp2 library
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.2.0
 * 
 * This file contains the HTTP/2 server implementation built on top of the nghttp2
 * library and Boost.Asio. It provides full HTTP/2 protocol support including
 * multiplexing, server push, header compression, and SSL/TLS encryption.
 * 
 * @section http2_example HTTP/2 Server Usage Example
 * @code{.cpp}
 * // Create HTTP/2 server configuration
 * ServerConfig config;
 * config.http2.enabled = true;
 * config.http2.port = 8443;
 * config.ssl.enabled = true;
 * config.ssl.certificateFile = "server.crt";
 * config.ssl.privateKeyFile = "server.key";
 * 
 * // Create request processor
 * auto processor = [](const HttpRequest& request) {
 *     HttpResponse response;
 *     response.setStatusCode(200);
 *     response.setBody("HTTP/2 Response");
 *     return response;
 * };
 * 
 * // Start HTTP/2 server
 * boost::asio::io_context ioc;
 * Http2Server server(ioc, config, processor);
 * server.start();
 * ioc.run();
 * @endcode
 * 
 * @note HTTP/2 requires SSL/TLS in most browsers. Ensure proper SSL configuration.
 * 
 * @see ServerConfig
 * @see HttpRequest
 * @see HttpResponse
 * @see DebugLogger
 */
#pragma once

#include <memory>
#include <map>
#include <string>
#include <functional>
#include <nghttp2/nghttp2.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <cppSwitchboard/config.h>
#include <cppSwitchboard/debug_logger.h>

namespace cppSwitchboard {

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

/**
 * @brief HTTP/2 session handler for individual client connections
 * 
 * The Http2Session class manages an individual HTTP/2 connection, handling
 * the HTTP/2 protocol framing, stream multiplexing, and request processing.
 * Each session runs over an SSL/TLS connection and can handle multiple
 * concurrent streams.
 * 
 * Key features:
 * - Full HTTP/2 protocol support via nghttp2
 * - SSL/TLS encryption for secure communication
 * - Concurrent stream processing and multiplexing
 * - Header compression (HPACK) support
 * - Comprehensive debug logging capabilities
 * - Asynchronous I/O using Boost.Asio
 * 
 * @code{.cpp}
 * // Session is typically created by Http2Server
 * auto session = std::make_shared<Http2Session>(
 *     std::move(socket), 
 *     &ssl_context,
 *     request_processor,
 *     debug_logger
 * );
 * session->start();
 * @endcode
 * 
 * @note This class is thread-safe for concurrent stream processing
 * @warning Requires proper SSL/TLS configuration for security
 * 
 * @see Http2Server
 * @see nghttp2 documentation for protocol details
 */
class Http2Session : public std::enable_shared_from_this<Http2Session> {
public:
    /**
     * @brief Construct an HTTP/2 session
     * 
     * Creates a new HTTP/2 session for handling a client connection.
     * The session will manage SSL/TLS handshake, HTTP/2 negotiation,
     * and request processing.
     * 
     * @param socket TCP socket for the client connection
     * @param ssl_ctx SSL context for TLS encryption (must not be null)
     * @param request_processor Function to process HTTP requests
     * @param debugLogger Optional debug logger for detailed logging
     * 
     * @throws std::runtime_error if nghttp2 session creation fails
     * 
     * @code{.cpp}
     * auto session = std::make_shared<Http2Session>(
     *     std::move(client_socket),
     *     &ssl_context,
     *     [](const HttpRequest& req) { return process_request(req); },
     *     debug_logger
     * );
     * @endcode
     */
    Http2Session(tcp::socket socket, ssl::context* ssl_ctx, 
                 std::function<HttpResponse(const HttpRequest&)> request_processor,
                 std::shared_ptr<DebugLogger> debugLogger = nullptr);
    
    /**
     * @brief Destructor - cleans up HTTP/2 session resources
     * 
     * Properly terminates the HTTP/2 session, closes the SSL stream,
     * and frees nghttp2 session resources.
     */
    ~Http2Session();
    
    /**
     * @brief Start the HTTP/2 session
     * 
     * Initiates the SSL/TLS handshake followed by HTTP/2 protocol
     * negotiation. Once established, begins processing HTTP/2 frames
     * and handling client requests.
     * 
     * @note This method returns immediately; processing continues asynchronously
     * @note The session keeps itself alive via shared_ptr until completion
     */
    void start();

private:
    /**
     * @brief Perform SSL/TLS handshake
     * 
     * Asynchronously performs the SSL/TLS handshake with the client.
     * Upon successful handshake, initiates HTTP/2 protocol setup.
     */
    void do_handshake();
    
    /**
     * @brief Read HTTP/2 frames from the client
     * 
     * Asynchronously reads data from the SSL stream and feeds it to
     * the nghttp2 session for frame processing.
     */
    void do_read();
    
    /**
     * @brief Write HTTP/2 frames to the client
     * 
     * Asynchronously writes buffered HTTP/2 frames to the SSL stream.
     * This is triggered by nghttp2 when frames need to be sent.
     */
    void do_write();
    
    /**
     * @brief Send HTTP response for a specific stream
     * 
     * Converts an HttpResponse object to HTTP/2 frames and queues
     * them for transmission to the client.
     * 
     * @param stream_id HTTP/2 stream identifier
     * @param response HTTP response to send
     */
    void send_response(int32_t stream_id, const HttpResponse& response);
    
    // nghttp2 callback functions - these interface with the nghttp2 library
    /**
     * @brief nghttp2 callback for sending data
     * 
     * Called by nghttp2 when data needs to be sent to the client.
     * Buffers the data for asynchronous transmission.
     * 
     * @param session nghttp2 session handle
     * @param data Data to send
     * @param length Length of data
     * @param flags Transmission flags
     * @param user_data Pointer to Http2Session instance
     * @return Number of bytes accepted for sending
     */
    static ssize_t send_callback(nghttp2_session* session, const uint8_t* data,
                               size_t length, int flags, void* user_data);
    
    /**
     * @brief nghttp2 callback for frame reception
     * 
     * Called when a complete HTTP/2 frame is received from the client.
     * Handles different frame types (HEADERS, DATA, etc.).
     * 
     * @param session nghttp2 session handle
     * @param frame Received frame
     * @param user_data Pointer to Http2Session instance
     * @return 0 on success, negative on error
     */
    static int on_frame_recv_callback(nghttp2_session* session,
                                    const nghttp2_frame* frame, void* user_data);
    
    /**
     * @brief nghttp2 callback for header field reception
     * 
     * Called for each header field received in HEADERS frames.
     * Builds the request headers for processing.
     * 
     * @param session nghttp2 session handle
     * @param frame Frame containing the header
     * @param name Header name
     * @param namelen Header name length
     * @param value Header value
     * @param valuelen Header value length
     * @param flags Header flags
     * @param user_data Pointer to Http2Session instance
     * @return 0 on success, negative on error
     */
    static int on_header_callback(nghttp2_session* session,
                                const nghttp2_frame* frame,
                                const uint8_t* name, size_t namelen,
                                const uint8_t* value, size_t valuelen,
                                uint8_t flags, void* user_data);
    
    /**
     * @brief nghttp2 callback for beginning of headers
     * 
     * Called when header processing begins for a stream.
     * Initializes stream data structures.
     * 
     * @param session nghttp2 session handle
     * @param frame HEADERS frame
     * @param user_data Pointer to Http2Session instance
     * @return 0 on success, negative on error
     */
    static int on_begin_headers_callback(nghttp2_session* session,
                                       const nghttp2_frame* frame,
                                       void* user_data);
    
    /**
     * @brief nghttp2 callback for stream closure
     * 
     * Called when an HTTP/2 stream is closed. Cleans up stream resources.
     * 
     * @param session nghttp2 session handle
     * @param stream_id Stream identifier
     * @param error_code Error code (0 for normal closure)
     * @param user_data Pointer to Http2Session instance
     * @return 0 on success, negative on error
     */
    static int on_stream_close_callback(nghttp2_session* session, int32_t stream_id,
                                       uint32_t error_code, void* user_data);
    
    /**
     * @brief nghttp2 callback for reading response data
     * 
     * Called by nghttp2 to read response body data for transmission.
     * 
     * @param session nghttp2 session handle
     * @param stream_id Stream identifier
     * @param buf Buffer to fill with data
     * @param length Buffer size
     * @param data_flags Output flags
     * @param source Data source
     * @param user_data Pointer to Http2Session instance
     * @return Number of bytes read, or error code
     */
    static ssize_t data_source_read_callback(nghttp2_session* session, int32_t stream_id,
                                           uint8_t* buf, size_t length, uint32_t* data_flags,
                                           nghttp2_data_source* source, void* user_data);

    tcp::socket socket_;                                    ///< TCP socket for client connection
    std::unique_ptr<ssl::stream<tcp::socket&>> ssl_stream_; ///< SSL stream wrapper
    nghttp2_session* session_;                              ///< nghttp2 session handle
    std::function<HttpResponse(const HttpRequest&)> request_processor_; ///< Request processing function
    std::shared_ptr<DebugLogger> debugLogger_;              ///< Optional debug logger
    
    /**
     * @brief Stream-specific data storage
     * 
     * Stores request data for each HTTP/2 stream during processing.
     */
    struct StreamData {
        std::string method;                             ///< HTTP method (GET, POST, etc.)
        std::string path;                              ///< Request path
        std::string scheme;                            ///< URI scheme (https)
        std::string authority;                         ///< Host header value
        std::map<std::string, std::string> headers;    ///< Request headers
        std::string body;                              ///< Request body
        bool headers_complete = false;                 ///< Headers completion flag
    };
    
    std::map<int32_t, StreamData> streams_;                              ///< Active streams data
    std::map<int32_t, std::vector<std::string>> header_strings_;        ///< Header string storage
    std::map<int32_t, std::vector<nghttp2_nv>> header_nvs_;             ///< nghttp2 header structures
    std::vector<uint8_t> read_buffer_;                                  ///< Input buffer
    std::vector<uint8_t> write_buffer_;                                 ///< Output buffer
};

/**
 * @brief HTTP/2 server implementation
 * 
 * The Http2Server class provides a complete HTTP/2 server implementation
 * with SSL/TLS support, connection management, and request routing.
 * It accepts client connections and creates Http2Session instances to
 * handle individual connections.
 * 
 * Features:
 * - HTTP/2 protocol support via nghttp2
 * - SSL/TLS encryption with configurable certificates
 * - Concurrent connection handling
 * - Configurable server parameters
 * - Debug logging integration
 * - Graceful shutdown support
 * 
 * @code{.cpp}
 * ServerConfig config;
 * config.http2.enabled = true;
 * config.http2.port = 8443;
 * config.ssl.enabled = true;
 * config.ssl.certificateFile = "server.crt";
 * config.ssl.privateKeyFile = "server.key";
 * 
 * boost::asio::io_context ioc;
 * Http2Server server(ioc, config, [](const HttpRequest& req) {
 *     HttpResponse resp;
 *     resp.setStatusCode(200);
 *     resp.setBody("Hello HTTP/2!");
 *     return resp;
 * });
 * 
 * server.start();
 * ioc.run();
 * @endcode
 * 
 * @see Http2Session
 * @see ServerConfig
 */
class Http2Server {
public:
    /**
     * @brief Construct HTTP/2 server
     * 
     * Creates an HTTP/2 server with the specified configuration and
     * request processing function. Initializes SSL context and network
     * acceptor based on configuration.
     * 
     * @param ioc Boost.Asio I/O context for asynchronous operations
     * @param config Server configuration including HTTP/2 and SSL settings
     * @param request_processor Function to process incoming HTTP requests
     * 
     * @throws std::runtime_error if SSL setup fails or port binding fails
     * 
     * @code{.cpp}
     * boost::asio::io_context ioc;
     * Http2Server server(ioc, config, request_processor);
     * @endcode
     */
    Http2Server(asio::io_context& ioc, const ServerConfig& config,
                std::function<HttpResponse(const HttpRequest&)> request_processor);
    
    /**
     * @brief Start accepting HTTP/2 connections
     * 
     * Begins accepting client connections on the configured port.
     * Each accepted connection is handled by a new Http2Session.
     * 
     * @note This method returns immediately; connections are handled asynchronously
     * @throws std::runtime_error if server is already running
     * 
     * @code{.cpp}
     * server.start();
     * ioc.run(); // Run the I/O context to process connections
     * @endcode
     */
    void start();
    
    /**
     * @brief Stop accepting new connections and shutdown server
     * 
     * Gracefully stops the server by closing the acceptor and stopping
     * new connection acceptance. Existing connections are allowed to complete.
     * 
     * @note Existing sessions will continue until they complete naturally
     * 
     * @code{.cpp}
     * server.stop();
     * @endcode
     */
    void stop();

private:
    /**
     * @brief Accept new client connections
     * 
     * Asynchronously accepts new TCP connections and creates Http2Session
     * instances to handle them. Continues accepting until server is stopped.
     */
    void do_accept();
    
    /**
     * @brief Initialize SSL context
     * 
     * Sets up the SSL context with configured certificates, cipher suites,
     * and HTTP/2 ALPN negotiation.
     * 
     * @throws std::runtime_error if SSL configuration is invalid
     */
    void setup_ssl_context();
    
    asio::io_context& ioc_;                                     ///< I/O context reference
    tcp::acceptor acceptor_;                                   ///< TCP connection acceptor
    ssl::context ssl_ctx_;                                     ///< SSL context for TLS
    std::function<HttpResponse(const HttpRequest&)> request_processor_; ///< Request processor function
    const ServerConfig& config_;                               ///< Server configuration reference
    std::shared_ptr<DebugLogger> debugLogger_;                 ///< Optional debug logger
    bool running_;                                             ///< Server running state
};

} // namespace cppSwitchboard 