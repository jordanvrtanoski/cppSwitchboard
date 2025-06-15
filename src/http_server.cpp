#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/http2_server_impl.h>
#include <cppSwitchboard/debug_logger.h>
#include <cppSwitchboard/middleware_pipeline.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <functional>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <boost/asio/signal_set.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace cppSwitchboard {

std::shared_ptr<HttpServer> HttpServer::create() {
    auto server = std::shared_ptr<HttpServerImpl>(new HttpServerImpl());
    server->routes_ = std::make_unique<RouteRegistry>();
    server->errorHandler_ = std::make_shared<DefaultErrorHandler>();
    return server;
}

std::shared_ptr<HttpServer> HttpServer::create(const ServerConfig& config) {
    auto server = std::shared_ptr<HttpServerImpl>(new HttpServerImpl(config));
    server->routes_ = std::make_unique<RouteRegistry>();
    server->errorHandler_ = std::make_shared<DefaultErrorHandler>();
    return server;
}



void HttpServer::registerHandler(const std::string& path, HttpMethod method, std::shared_ptr<HttpHandler> handler) {
    routes_->registerRoute(path, method, handler);
}

void HttpServer::registerAsyncHandler(const std::string& path, HttpMethod method, std::shared_ptr<AsyncHttpHandler> handler) {
    routes_->registerAsyncRoute(path, method, handler);
}

void HttpServer::registerRouteWithMiddleware(const std::string& path, HttpMethod method, std::shared_ptr<MiddlewarePipeline> pipeline) {
    routes_->registerRouteWithMiddleware(path, method, pipeline);
}

void HttpServer::get(const std::string& path, std::shared_ptr<HttpHandler> handler) {
    registerHandler(path, HttpMethod::GET, handler);
}

void HttpServer::post(const std::string& path, std::shared_ptr<HttpHandler> handler) {
    registerHandler(path, HttpMethod::POST, handler);
}

void HttpServer::put(const std::string& path, std::shared_ptr<HttpHandler> handler) {
    registerHandler(path, HttpMethod::PUT, handler);
}

void HttpServer::del(const std::string& path, std::shared_ptr<HttpHandler> handler) {
    registerHandler(path, HttpMethod::DELETE, handler);
}

void HttpServer::get(const std::string& path, std::function<HttpResponse(const HttpRequest&)> handler) {
    registerHandler(path, HttpMethod::GET, makeHandler(handler));
}

void HttpServer::post(const std::string& path, std::function<HttpResponse(const HttpRequest&)> handler) {
    registerHandler(path, HttpMethod::POST, makeHandler(handler));
}

void HttpServer::put(const std::string& path, std::function<HttpResponse(const HttpRequest&)> handler) {
    registerHandler(path, HttpMethod::PUT, makeHandler(handler));
}

void HttpServer::del(const std::string& path, std::function<HttpResponse(const HttpRequest&)> handler) {
    registerHandler(path, HttpMethod::DELETE, makeHandler(handler));
}

void HttpServer::registerMiddleware(std::shared_ptr<Middleware> middleware) {
    middleware_.push_back(middleware);
}

void HttpServer::setErrorHandler(std::shared_ptr<ErrorHandler> errorHandler) {
    errorHandler_ = errorHandler;
}

void HttpServer::enableHttp1(int port) {
    config_.http1.enabled = true;
    config_.http1.port = port;
}

void HttpServer::enableHttp2(int port) {
    config_.http2.enabled = true;
    config_.http2.port = port;
}

void HttpServer::enableSsl(const std::string& certFile, const std::string& keyFile) {
    config_.ssl.enabled = true;
    config_.ssl.certificateFile = certFile;
    config_.ssl.privateKeyFile = keyFile;
}

void HttpServer::disableSsl() {
    config_.ssl.enabled = false;
}

void HttpServer::start() {
    if (running_) {
        return;
    }
    
    validateConfiguration();
    running_ = true;
    
    if (config_.http1.enabled) {
        http1Thread_ = std::thread([this]() { runHttp1Server(); });
    }
    
    if (config_.http2.enabled) {
        http2Thread_ = std::thread([this]() { runHttp2Server(); });
    }
    
    printStartupInfo();
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }
    
    if (config_.general.enableLogging) {
        std::cout << "\nShutting down QoS Manager HTTP Server..." << std::endl;
    }
    
    // Set the running flag to false - this will cause both server threads to exit their loops
    running_ = false;
    
    // Give the threads a moment to see the flag change and start shutting down
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Wait for both threads to finish
    if (http1Thread_.joinable()) {
        http1Thread_.join();
        if (config_.general.enableLogging) {
            std::cout << "HTTP/1.1 server stopped" << std::endl;
        }
    }
    
    if (http2Thread_.joinable()) {
        http2Thread_.join();
        if (config_.general.enableLogging) {
            std::cout << "HTTP/2 server stopped" << std::endl;
        }
    }
    
    if (config_.general.enableLogging) {
        std::cout << "QoS Manager HTTP Server stopped" << std::endl;
    }
}

void HttpServer::printStartupInfo() const {
    if (!config_.general.enableLogging) return;
    
    std::cout << "=== " << config_.application.name << " v" << config_.application.version << " ===" << std::endl;
    std::cout << "Environment: " << config_.application.environment << std::endl;
    std::cout << "Log Level: " << config_.general.logLevel << std::endl;
    
    if (config_.http1.enabled) {
        std::cout << "HTTP/1.1 server: " << config_.http1.bindAddress << ":" << config_.http1.port;
        if (config_.ssl.enabled) std::cout << " (SSL)";
        std::cout << std::endl;
    }
    
    if (config_.http2.enabled) {
        std::cout << "HTTP/2 server: " << config_.http2.bindAddress << ":" << config_.http2.port;
        if (config_.ssl.enabled) std::cout << " (SSL)";
        std::cout << std::endl;
    }
    
    if (config_.ssl.enabled) {
        std::cout << "SSL Certificate: " << config_.ssl.certificateFile << std::endl;
        std::cout << "SSL Private Key: " << config_.ssl.privateKeyFile << std::endl;
        if (!config_.ssl.caCertificateFile.empty()) {
            std::cout << "SSL CA Certificate: " << config_.ssl.caCertificateFile << std::endl;
        }
        std::cout << "Client Certificate Verification: " << (config_.ssl.verifyClient ? "enabled" : "disabled") << std::endl;
    }
    
    std::cout << "Max Connections: " << config_.general.maxConnections << std::endl;
    std::cout << "Worker Threads: " << config_.general.workerThreads << std::endl;
    std::cout << "Request Timeout: " << config_.general.requestTimeout.count() << "s" << std::endl;
    
    if (config_.security.rateLimitEnabled) {
        std::cout << "Rate Limiting: " << config_.security.rateLimitRequestsPerMinute << " requests/minute" << std::endl;
    }
    
    if (config_.monitoring.healthCheck.enabled) {
        std::cout << "Health Check: " << config_.monitoring.healthCheck.endpoint << std::endl;
    }
    
    std::cout << "Server started successfully!" << std::endl;
}

void HttpServer::validateConfiguration() const {
    std::string errorMessage;
    if (!ConfigLoader::validateConfig(config_, errorMessage)) {
        throw std::runtime_error("Configuration validation failed: " + errorMessage);
    }
}

HttpResponse HttpServer::processRequest(const HttpRequest& request) {
    try {
        // Find matching route
        auto match = routes_->findRoute(request.getPath(), request.getHttpMethod());
        
        if (!match.matched) {
            return HttpResponse::notFound("Route not found: " + request.getMethod() + " " + request.getPath());
        }
        
        // Create a mutable copy of the request to add path parameters
        HttpRequest mutableRequest = request;
        for (const auto& param : match.pathParams) {
            mutableRequest.setPathParam(param.first, param.second);
        }
        
        // Process with handler or middleware pipeline
        if (match.hasMiddleware && match.middlewarePipeline) {
            // Execute through middleware pipeline
            return match.middlewarePipeline->execute(mutableRequest);
        } else if (match.isAsync) {
            // For now, we'll handle async requests synchronously
            // In a full implementation, this would use proper async handling
            return HttpResponse::internalServerError("Async handlers not yet supported in synchronous context");
        } else {
            // Execute handler directly (backward compatibility)
            return match.handler->handle(mutableRequest);
        }
    } catch (const std::exception& e) {
        if (errorHandler_) {
            return errorHandler_->handleError(request, e);
        } else {
            return HttpResponse::internalServerError("Internal server error: " + std::string(e.what()));
        }
    }
}

void HttpServer::processAsyncRequest(const HttpRequest& request, std::function<void(const HttpResponse&)> callback) {
    try {
        auto match = routes_->findRoute(request.getPath(), request.getHttpMethod());
        
        if (!match.matched) {
            callback(HttpResponse::notFound("Route not found: " + request.getMethod() + " " + request.getPath()));
            return;
        }
        
        HttpRequest mutableRequest = request;
        for (const auto& param : match.pathParams) {
            mutableRequest.setPathParam(param.first, param.second);
        }
        
        if (match.hasMiddleware && match.middlewarePipeline) {
            // Execute through middleware pipeline (synchronously for now)
            // TODO: Implement async middleware pipeline support in Phase 3
            try {
                HttpResponse response = match.middlewarePipeline->execute(mutableRequest);
                callback(response);
            } catch (const std::exception& e) {
                if (errorHandler_) {
                    callback(errorHandler_->handleError(request, e));
                } else {
                    callback(HttpResponse::internalServerError("Pipeline error: " + std::string(e.what())));
                }
            }
        } else if (match.isAsync) {
            match.asyncHandler->handleAsync(mutableRequest, callback);
        } else {
            HttpResponse response = match.handler->handle(mutableRequest);
            callback(response);
        }
    } catch (const std::exception& e) {
        if (errorHandler_) {
            callback(errorHandler_->handleError(request, e));
        } else {
            callback(HttpResponse::internalServerError("Internal server error: " + std::string(e.what())));
        }
    }
}

void HttpServer::logRequest(const HttpRequest& request, const HttpResponse& response) {
    if (config_.general.enableLogging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
                  << request.getProtocol() << " " << request.getMethod() << " " << request.getPath()
                  << " - " << response.getStatus() << std::endl;
    }
}

// Basic HTTP/1.1 server implementation using Boost.Beast
void HttpServerImpl::runHttp1Server() {
    try {
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {net::ip::make_address(config_.http1.bindAddress), 
                                    static_cast<unsigned short>(config_.http1.port)}};
        
        // Lambda to handle individual connections
        std::function<void(tcp::socket)> handle_connection = [this](tcp::socket socket) {
            // Handle request in a simple synchronous manner
            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            
            try {
                http::read(socket, buffer, req);
                
                // Convert Beast request to our HttpRequest
                HttpRequest qosRequest(std::string(req.method_string()), std::string(req.target()), "HTTP/1.1");
                qosRequest.setBody(req.body());
                
                for (const auto& field : req) {
                    qosRequest.setHeader(std::string(field.name_string()), std::string(field.value()));
                }
                
                // Process request
                HttpResponse qosResponse = processRequest(qosRequest);
                
                // Convert our HttpResponse to Beast response
                http::response<http::string_body> res{
                    static_cast<http::status>(qosResponse.getStatus()),
                    req.version()
                };
                
                // Add server identification
                res.set("Server", config_.application.name + "/" + config_.application.version);
                
                for (const auto& header : qosResponse.getHeaders()) {
                    res.set(header.first, header.second);
                }
                
                res.body() = qosResponse.getBody();
                res.prepare_payload();
                
                // Send response
                http::write(socket, res);
                
                // Log request
                logRequest(qosRequest, qosResponse);
                
            } catch (const std::exception& e) {
                // Send error response
                http::response<http::string_body> res{http::status::internal_server_error, req.version()};
                res.set(http::field::content_type, "application/json");
                res.set("Server", config_.application.name + "/" + config_.application.version);
                res.body() = "{\"error\": \"Internal Server Error\"}";
                res.prepare_payload();
                
                try {
                    http::write(socket, res);
                } catch (...) {
                    // Ignore write errors
                }
            }
            
            socket.shutdown(tcp::socket::shutdown_send);
        };
        
        // Recursive lambda for async accept
        std::function<void()> do_accept = [&]() {
            if (!running_) {
                ioc.stop();
                return;
            }
            
            acceptor.async_accept([&](boost::system::error_code ec, tcp::socket socket) {
                if (!ec && running_) {
                    // Handle connection in a separate thread to avoid blocking the acceptor
                    std::thread connection_thread(handle_connection, std::move(socket));
                    connection_thread.detach();
                    
                    // Continue accepting if still running
                    do_accept();
                } else if (ec && ec != asio::error::operation_aborted) {
                    if (config_.general.enableLogging) {
                        std::cerr << "HTTP/1.1 accept error: " << ec.message() << std::endl;
                    }
                }
                
                // Check if we should stop
                if (!running_) {
                    ioc.stop();
                }
            });
        };
        
        // Start accepting connections
        do_accept();
        
        // Run the IO context with periodic checks for shutdown
        while (running_) {
            ioc.run_for(std::chrono::milliseconds(100));
            if (!running_) {
                ioc.stop();
                break;
            }
        }
        
    } catch (const std::exception& e) {
        if (config_.general.enableLogging) {
            std::cerr << "HTTP/1.1 server error: " << e.what() << std::endl;
        }
    }
}

// Real HTTP/2 server implementation using nghttp2
void HttpServerImpl::runHttp2Server() {
    if (config_.general.enableLogging) {
        std::cout << "Starting HTTP/2 server with nghttp2..." << std::endl;
    }
    
    try {
        net::io_context ioc{static_cast<int>(config_.general.workerThreads)};
        
        // Create HTTP/2 server with request processor
        Http2Server http2Server(ioc, config_, 
            [this](const HttpRequest& request) -> HttpResponse {
                HttpResponse response = processRequest(request);
                logRequest(request, response);
                return response;
            });
        
        http2Server.start();
        
        // Run the IO context with periodic checks for shutdown
        while (running_) {
            ioc.run_for(std::chrono::milliseconds(100));
            if (!running_) {
                http2Server.stop();
                ioc.stop();
                break;
            }
        }
        
    } catch (const std::exception& e) {
        if (config_.general.enableLogging) {
            std::cerr << "HTTP/2 server error: " << e.what() << std::endl;
        }
    }
}

void HttpServerImpl::setupSslContext() {
    // TODO: Implement SSL context setup using Boost.Beast SSL
    if (config_.ssl.enabled) {
        std::cout << "SSL context setup (not yet implemented)" << std::endl;
    }
}

bool HttpServerImpl::loadSslCertificates() {
    // TODO: Implement SSL certificate loading
    if (config_.ssl.enabled) {
        std::cout << "Loading SSL certificates (not yet implemented)" << std::endl;
        return true;
    }
    return false;
}

} // namespace cppSwitchboard 