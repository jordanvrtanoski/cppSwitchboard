/**
 * @file http_server.h
 * @brief Main HTTP server interface for cppSwitchboard library
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @version 1.0.0
 * @date 2024
 * 
 * This file contains the main HttpServer class which provides a modern C++
 * HTTP/1.1 and HTTP/2 server implementation with routing, middleware support,
 * SSL/TLS, and comprehensive configuration management.
 */

#pragma once

#include <cppSwitchboard/http_handler.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <cppSwitchboard/route_registry.h>
#include <cppSwitchboard/config.h>
#include <cppSwitchboard/middleware.h>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace cppSwitchboard {

/**
 * @class HttpServer
 * @brief Main HTTP server class supporting HTTP/1.1 and HTTP/2 protocols
 * 
 * The HttpServer class is the core component of the cppSwitchboard library,
 * providing a high-performance, modern C++ HTTP server with support for:
 * - HTTP/1.1 and HTTP/2 protocols
 * - Flexible routing with parameter extraction
 * - Middleware chain processing
 * - SSL/TLS encryption
 * - Asynchronous request handling
 * - Comprehensive configuration management
 * 
 * @code{.cpp}
 * // Basic usage example
 * auto server = HttpServer::create();
 * 
 * server->get("/api/hello", [](const HttpRequest& req) {
 *     return HttpResponse::ok("Hello, World!");
 * });
 * 
 * server->start();
 * @endcode
 * 
 * @see HttpRequest, HttpResponse, RouteRegistry
 * @since 1.0.0
 */
class HttpServer {
public:
    /**
     * @brief Create an HTTP server instance with default configuration
     * @return Shared pointer to HttpServer instance
     * @throws std::runtime_error if server creation fails
     * 
     * Creates a new HTTP server with default configuration settings.
     * The server will be configured with standard ports (8080 for HTTP,
     * 8443 for HTTPS) and default middleware.
     * 
     * @code{.cpp}
     * auto server = HttpServer::create();
     * @endcode
     */
    static std::shared_ptr<HttpServer> create();
    
    /**
     * @brief Create an HTTP server instance with custom configuration
     * @param config Server configuration object
     * @return Shared pointer to HttpServer instance
     * @throws std::runtime_error if server creation fails
     * @throws std::invalid_argument if configuration is invalid
     * 
     * Creates a new HTTP server with the specified configuration.
     * The configuration object should be validated before passing.
     * 
     * @code{.cpp}
     * ServerConfig config;
     * config.http1.port = 9090;
     * config.http1.enabled = true;
     * auto server = HttpServer::create(config);
     * @endcode
     */
    static std::shared_ptr<HttpServer> create(const ServerConfig& config);
    
    /**
     * @brief Virtual destructor for proper cleanup
     * 
     * Ensures proper cleanup of server resources when the object is destroyed.
     * Automatically stops the server if it's still running.
     */
    virtual ~HttpServer() = default;
    
    // Handler registration
    
    /**
     * @brief Register a handler for a specific path and HTTP method
     * @param path URL path pattern (supports parameters like "/user/{id}")
     * @param method HTTP method (GET, POST, PUT, DELETE, etc.)
     * @param handler Shared pointer to HttpHandler implementation
     * @throws std::invalid_argument if path is invalid or handler is null
     * 
     * Registers a handler for processing requests matching the specified
     * path pattern and HTTP method. Path patterns support parameter
     * extraction using curly braces.
     * 
     * @code{.cpp}
     * server->registerHandler("/api/users/{id}", HttpMethod::GET, userHandler);
     * @endcode
     * 
     * @see HttpHandler, HttpMethod
     */
    void registerHandler(const std::string& path, HttpMethod method, std::shared_ptr<HttpHandler> handler);
    
    /**
     * @brief Register an asynchronous handler for a specific path and HTTP method
     * @param path URL path pattern (supports parameters like "/user/{id}")
     * @param method HTTP method (GET, POST, PUT, DELETE, etc.)
     * @param handler Shared pointer to AsyncHttpHandler implementation
     * @throws std::invalid_argument if path is invalid or handler is null
     * 
     * Registers an asynchronous handler for processing requests. Async handlers
     * are useful for long-running operations or when integrating with async
     * frameworks.
     * 
     * @see AsyncHttpHandler, HttpMethod
     */
    void registerAsyncHandler(const std::string& path, HttpMethod method, std::shared_ptr<AsyncHttpHandler> handler);
    
    /**
     * @brief Register a route with middleware pipeline
     * @param path URL path pattern
     * @param method HTTP method
     * @param pipeline Middleware pipeline with final handler set
     * @throws std::invalid_argument if path is invalid or pipeline is null
     * @throws std::runtime_error if pipeline has no final handler
     * 
     * Registers a route that will be processed through the specified middleware
     * pipeline. This allows for complex request processing chains including
     * authentication, logging, validation, and other middleware.
     * 
     * @code{.cpp}
     * auto pipeline = std::make_shared<MiddlewarePipeline>();
     * pipeline->addMiddleware(std::make_shared<AuthMiddleware>());
     * pipeline->addMiddleware(std::make_shared<LoggingMiddleware>());
     * pipeline->setFinalHandler(std::make_shared<UserHandler>());
     * server->registerRouteWithMiddleware("/api/users/{id}", HttpMethod::GET, pipeline);
     * @endcode
     */
    void registerRouteWithMiddleware(const std::string& path, HttpMethod method, std::shared_ptr<MiddlewarePipeline> pipeline);
    
    // Convenience methods for common HTTP methods
    
    /**
     * @brief Register a GET request handler
     * @param path URL path pattern
     * @param handler Shared pointer to HttpHandler implementation
     * @throws std::invalid_argument if path is invalid or handler is null
     * 
     * Convenience method for registering GET request handlers.
     * Equivalent to registerHandler(path, HttpMethod::GET, handler).
     * 
     * @code{.cpp}
     * server->get("/api/status", statusHandler);
     * @endcode
     */
    void get(const std::string& path, std::shared_ptr<HttpHandler> handler);
    
    /**
     * @brief Register a POST request handler
     * @param path URL path pattern
     * @param handler Shared pointer to HttpHandler implementation
     * @throws std::invalid_argument if path is invalid or handler is null
     */
    void post(const std::string& path, std::shared_ptr<HttpHandler> handler);
    
    /**
     * @brief Register a PUT request handler
     * @param path URL path pattern
     * @param handler Shared pointer to HttpHandler implementation
     * @throws std::invalid_argument if path is invalid or handler is null
     */
    void put(const std::string& path, std::shared_ptr<HttpHandler> handler);
    
    /**
     * @brief Register a DELETE request handler
     * @param path URL path pattern
     * @param handler Shared pointer to HttpHandler implementation
     * @throws std::invalid_argument if path is invalid or handler is null
     * @note Method name is 'del' because 'delete' is a C++ keyword
     */
    void del(const std::string& path, std::shared_ptr<HttpHandler> handler); // 'delete' is a keyword
    
    // Function-based registration
    
    /**
     * @brief Register a GET request handler using a lambda function
     * @param path URL path pattern
     * @param handler Lambda function that processes the request
     * @throws std::invalid_argument if path is invalid
     * 
     * Convenience method for registering GET handlers using lambda functions
     * or other callable objects. This provides a more functional programming
     * approach to request handling.
     * 
     * @code{.cpp}
     * server->get("/api/hello", [](const HttpRequest& req) {
     *     return HttpResponse::ok("Hello, World!");
     * });
     * @endcode
     */
    void get(const std::string& path, std::function<HttpResponse(const HttpRequest&)> handler);
    
    /**
     * @brief Register a POST request handler using a lambda function
     * @param path URL path pattern
     * @param handler Lambda function that processes the request
     * @throws std::invalid_argument if path is invalid
     */
    void post(const std::string& path, std::function<HttpResponse(const HttpRequest&)> handler);
    
    /**
     * @brief Register a PUT request handler using a lambda function
     * @param path URL path pattern
     * @param handler Lambda function that processes the request
     * @throws std::invalid_argument if path is invalid
     */
    void put(const std::string& path, std::function<HttpResponse(const HttpRequest&)> handler);
    
    /**
     * @brief Register a DELETE request handler using a lambda function
     * @param path URL path pattern
     * @param handler Lambda function that processes the request
     * @throws std::invalid_argument if path is invalid
     */
    void del(const std::string& path, std::function<HttpResponse(const HttpRequest&)> handler);
    
    // Middleware registration
    
    /**
     * @brief Register middleware for request/response processing
     * @param middleware Shared pointer to Middleware implementation
     * @throws std::invalid_argument if middleware is null
     * 
     * Registers middleware that will be executed for all requests in the order
     * they were registered. Middleware can modify requests/responses, implement
     * authentication, logging, CORS, etc.
     * 
     * @code{.cpp}
     * server->registerMiddleware(std::make_shared<CorsMiddleware>());
     * server->registerMiddleware(std::make_shared<AuthMiddleware>());
     * @endcode
     * 
     * @see Middleware
     */
    void registerMiddleware(std::shared_ptr<Middleware> middleware);
    
    // Error handler
    
    /**
     * @brief Set a custom error handler for unhandled exceptions
     * @param errorHandler Shared pointer to ErrorHandler implementation
     * @throws std::invalid_argument if errorHandler is null
     * 
     * Sets a custom error handler that will be called when unhandled
     * exceptions occur during request processing. The error handler
     * can generate custom error responses.
     * 
     * @see ErrorHandler
     */
    void setErrorHandler(std::shared_ptr<ErrorHandler> errorHandler);
    
    // Protocol enablement (legacy methods)
    
    /**
     * @brief Enable HTTP/1.1 protocol support
     * @param port Port number to listen on (default: 8080)
     * @throws std::runtime_error if port is already in use
     * 
     * @deprecated Use configuration object instead
     * @note This is a legacy method. Use ServerConfig for new code.
     */
    void enableHttp1(int port = 8080);
    
    /**
     * @brief Enable HTTP/2 protocol support
     * @param port Port number to listen on (default: 8443)
     * @throws std::runtime_error if port is already in use or SSL not configured
     * 
     * @deprecated Use configuration object instead
     * @note This is a legacy method. Use ServerConfig for new code.
     * @note HTTP/2 requires SSL/TLS to be enabled
     */
    void enableHttp2(int port = 8443);
    
    // Server lifecycle
    
    /**
     * @brief Start the HTTP server
     * @throws std::runtime_error if server is already running or configuration is invalid
     * @throws std::system_error if network binding fails
     * 
     * Starts the HTTP server on the configured ports. This method will
     * validate the configuration, bind to the specified ports, and begin
     * accepting connections. The server runs in background threads.
     * 
     * @code{.cpp}
     * server->start();
     * std::cout << "Server started successfully!" << std::endl;
     * @endcode
     */
    void start();
    
    /**
     * @brief Stop the HTTP server gracefully
     * @throws std::runtime_error if server is not running
     * 
     * Stops the HTTP server gracefully, allowing current requests to complete
     * before shutting down. This method will wait for all worker threads
     * to finish processing.
     */
    void stop();
    
    /**
     * @brief Check if the server is currently running
     * @return True if server is running, false otherwise
     * 
     * Returns the current running state of the server. This can be used
     * to check server status before starting or stopping.
     */
    bool isRunning() const { return running_; }
    
    // Configuration
    
    /**
     * @brief Get the current server configuration
     * @return Const reference to ServerConfig object
     * 
     * Returns the current configuration used by the server. This includes
     * all protocol settings, SSL configuration, and server parameters.
     * 
     * @see ServerConfig
     */
    const ServerConfig& getConfig() const { return config_; }
    
    /**
     * @brief Set the server configuration
     * @param config New configuration object
     * @throws std::invalid_argument if configuration is invalid
     * @throws std::runtime_error if server is currently running
     * 
     * Updates the server configuration. The server must be stopped
     * before changing configuration.
     * 
     * @see ServerConfig
     */
    void setConfig(const ServerConfig& config) { config_ = config; }
    
    // SSL/TLS support
    
    /**
     * @brief Check if SSL/TLS is enabled
     * @return True if SSL is enabled, false otherwise
     * 
     * Returns whether SSL/TLS encryption is currently enabled for
     * secure connections.
     */
    bool isSslEnabled() const { return config_.ssl.enabled; }
    
    /**
     * @brief Enable SSL/TLS with certificate and key files
     * @param certFile Path to SSL certificate file (PEM format)
     * @param keyFile Path to SSL private key file (PEM format)
     * @throws std::runtime_error if certificate files cannot be loaded
     * @throws std::invalid_argument if file paths are invalid
     * 
     * Enables SSL/TLS encryption using the specified certificate and
     * private key files. Both files should be in PEM format.
     * 
     * @code{.cpp}
     * server->enableSsl("/path/to/cert.pem", "/path/to/key.pem");
     * @endcode
     */
    void enableSsl(const std::string& certFile, const std::string& keyFile);
    
    /**
     * @brief Disable SSL/TLS encryption
     * 
     * Disables SSL/TLS encryption. The server will only accept
     * plain HTTP connections after this call.
     */
    void disableSsl();

protected:
    /**
     * @brief Default constructor for derived classes
     * 
     * Protected constructor to prevent direct instantiation.
     * Use create() static methods instead.
     */
    HttpServer() = default;
    
    /**
     * @brief Constructor with configuration for derived classes
     * @param config Server configuration object
     * 
     * Protected constructor with configuration for derived classes.
     */
    HttpServer(const ServerConfig& config) : config_(config) {}

protected:
    ServerConfig config_;                                      ///< Server configuration
    std::unique_ptr<RouteRegistry> routes_;                   ///< Route registry for URL matching
    std::vector<std::shared_ptr<Middleware>> middleware_;     ///< Registered middleware chain
    std::shared_ptr<ErrorHandler> errorHandler_;             ///< Custom error handler
    
    std::atomic<bool> running_{false};                       ///< Server running state
    std::thread http1Thread_;                                ///< HTTP/1.1 server thread
    std::thread http2Thread_;                                ///< HTTP/2 server thread
    
    // Internal request processing
    
    /**
     * @brief Process an HTTP request through the middleware chain
     * @param request HTTP request to process
     * @return HTTP response
     * @throws std::exception for unhandled errors
     * 
     * Internal method that processes requests through the complete
     * middleware chain and routing system.
     */
    HttpResponse processRequest(const HttpRequest& request);
    
    /**
     * @brief Process an HTTP request asynchronously
     * @param request HTTP request to process
     * @param callback Callback function to call with the response
     * 
     * Internal method for asynchronous request processing.
     */
    void processAsyncRequest(const HttpRequest& request, std::function<void(const HttpResponse&)> callback);
    
    // Protocol-specific server implementations
    
    /**
     * @brief Run the HTTP/1.1 server implementation
     * 
     * Pure virtual method that must be implemented by concrete classes
     * to provide HTTP/1.1 protocol support.
     */
    virtual void runHttp1Server() = 0;
    
    /**
     * @brief Run the HTTP/2 server implementation
     * 
     * Pure virtual method that must be implemented by concrete classes
     * to provide HTTP/2 protocol support.
     */
    virtual void runHttp2Server() = 0;
    
    // Helper methods
    
    /**
     * @brief Apply middleware chain to request/response
     * @param request HTTP request
     * @param response HTTP response to modify
     * @param next Callback to continue middleware chain
     * 
     * Internal helper method for applying the middleware chain.
     */
    void applyMiddleware(const HttpRequest& request, HttpResponse& response, std::function<void()> next);
    
    /**
     * @brief Log request/response information
     * @param request HTTP request
     * @param response HTTP response
     * 
     * Internal helper method for logging request/response details.
     */
    void logRequest(const HttpRequest& request, const HttpResponse& response);
    
    // Configuration helpers
    
    /**
     * @brief Print server startup information
     * 
     * Internal helper method that prints server configuration
     * and startup information to the console.
     */
    void printStartupInfo() const;
    
    /**
     * @brief Validate server configuration
     * @throws std::invalid_argument if configuration is invalid
     * 
     * Internal helper method that validates the server configuration
     * before starting the server.
     */
    void validateConfiguration() const;
};

/**
 * @class HttpServerImpl
 * @brief Concrete HTTP server implementation using Boost.Beast and nghttp2
 * 
 * This is the default implementation of HttpServer that uses Boost.Beast
 * for HTTP/1.1 support and nghttp2 for HTTP/2 support.
 * 
 * @see HttpServer
 * @since 1.0.0
 */
class HttpServerImpl : public HttpServer {
public:
    /**
     * @brief Default constructor
     * 
     * Creates an HttpServerImpl with default configuration.
     */
    HttpServerImpl() = default;
    
    /**
     * @brief Constructor with configuration
     * @param config Server configuration object
     * 
     * Creates an HttpServerImpl with the specified configuration.
     */
    HttpServerImpl(const ServerConfig& config) : HttpServer(config) {}
    
private:
    /**
     * @brief HTTP/1.1 server implementation using Boost.Beast
     * 
     * Implements the HTTP/1.1 protocol server using Boost.Beast library.
     */
    void runHttp1Server() override;
    
    /**
     * @brief HTTP/2 server implementation using nghttp2
     * 
     * Implements the HTTP/2 protocol server using nghttp2 library.
     */
    void runHttp2Server() override;
    
    // SSL/TLS helpers
    
    /**
     * @brief Set up SSL context for secure connections
     * 
     * Internal helper method for configuring SSL/TLS context.
     */
    void setupSslContext();
    
    /**
     * @brief Load SSL certificates from files
     * @return True if certificates loaded successfully, false otherwise
     * 
     * Internal helper method for loading SSL certificate and key files.
     */
    bool loadSslCertificates();
};

} // namespace cppSwitchboard 