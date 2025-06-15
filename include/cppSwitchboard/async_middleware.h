/**
 * @file async_middleware.h
 * @brief Asynchronous middleware base classes and interfaces for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-01-08
 * @version 1.2.0
 * 
 * This file defines the core asynchronous middleware interface and pipeline
 * management for the cppSwitchboard framework. It provides the foundation for
 * creating asynchronous middleware pipelines that can process HTTP requests
 * in a configurable chain with callback-based flow control.
 * 
 * @section async_middleware_example Async Middleware Usage Example
 * @code{.cpp}
 * class AsyncLoggingMiddleware : public AsyncMiddleware {
 * public:
 *     void handleAsync(const HttpRequest& request, Context& context, 
 *                      AsyncNextHandler next, ResponseCallback callback) override {
 *         // Log request
 *         std::cout << "Processing: " << request.getPath() << std::endl;
 *         
 *         // Add timing context
 *         context["start_time"] = std::chrono::steady_clock::now();
 *         
 *         // Call next middleware/handler asynchronously
 *         next(request, context, [callback](const HttpResponse& response) {
 *             // Log response
 *             std::cout << "Response: " << response.getStatus() << std::endl;
 *             callback(response);
 *         });
 *     }
 *     
 *     std::string getName() const override { return "AsyncLoggingMiddleware"; }
 * };
 * @endcode
 * 
 * @see AsyncMiddleware
 * @see AsyncMiddlewarePipeline
 * @see Middleware
 */
#pragma once

#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_handler.h>
#include <cppSwitchboard/debug_logger.h>
#include <functional>
#include <memory>
#include <vector>
#include <chrono>
#include <stdexcept>
#include <atomic>
#include <mutex>

namespace cppSwitchboard {

// Forward declarations
class AsyncMiddleware;
class AsyncMiddlewarePipeline;

/**
 * @brief Callback function type for asynchronous middleware responses
 * 
 * This callback is invoked when an asynchronous middleware operation completes
 * and the response is ready to be sent to the client or passed to the next
 * middleware in the pipeline.
 * 
 * @param response The HTTP response from the middleware/handler
 */
using AsyncResponseCallback = std::function<void(const HttpResponse&)>;

/**
 * @brief Function type for the next async handler in the pipeline
 * 
 * The AsyncNextHandler function represents the next step in the asynchronous
 * middleware pipeline. It can be either another async middleware or the final
 * async request handler. Middleware must call this function to continue the
 * pipeline, unless they want to short-circuit the execution.
 * 
 * @param request The HTTP request being processed
 * @param context The middleware context for sharing state
 * @param callback Function to call when the next handler completes
 */
using AsyncNextHandler = std::function<void(const HttpRequest&, Context&, AsyncResponseCallback)>;

/**
 * @brief Abstract base class for asynchronous middleware components
 * 
 * The AsyncMiddleware class defines the interface that all asynchronous middleware
 * components must implement. Async middleware can inspect and modify requests,
 * handle responses, manage context, and control the flow of the pipeline using
 * callback-based execution.
 * 
 * Key responsibilities:
 * - Process incoming requests before they reach the handler
 * - Modify or validate request data asynchronously
 * - Add information to the context for downstream components
 * - Process responses before they are sent to the client
 * - Handle errors and exceptions in async operations
 * - Control pipeline flow (continue, abort, redirect) via callbacks
 * 
 * Implementation Requirements:
 * - Thread-safe: Middleware may be called from multiple threads
 * - Exception-safe: Should handle exceptions gracefully
 * - Callback-safe: Must invoke callbacks exactly once
 * - Performance-conscious: Minimize overhead in the async request path
 * 
 * @code{.cpp}
 * class AsyncAuthMiddleware : public AsyncMiddleware {
 * public:
 *     void handleAsync(const HttpRequest& request, Context& context,
 *                      AsyncNextHandler next, AsyncResponseCallback callback) override {
 *         // Check authentication asynchronously
 *         std::string token = request.getHeader("Authorization");
 *         if (token.empty()) {
 *             callback(HttpResponse::unauthorized("Missing authorization header"));
 *             return;
 *         }
 *         
 *         // Validate token asynchronously (e.g., database lookup)
 *         validateTokenAsync(token, [this, request, &context, next, callback](bool valid) {
 *             if (!valid) {
 *                 callback(HttpResponse::unauthorized("Invalid token"));
 *                 return;
 *             }
 *             
 *             // Add user info to context
 *             context["user_id"] = extractUserId(token);
 *             context["authenticated"] = true;
 *             
 *             // Continue to next middleware/handler
 *             next(request, context, callback);
 *         });
 *     }
 *     
 *     std::string getName() const override { return "AsyncAuthMiddleware"; }
 *     int getPriority() const override { return 100; } // High priority for auth
 * };
 * @endcode
 * 
 * @see AsyncNextHandler, Context, AsyncMiddlewarePipeline
 */
class AsyncMiddleware {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~AsyncMiddleware() = default;
    
    /**
     * @brief Process an HTTP request through the async middleware
     * 
     * This is the main method that async middleware must implement. It receives
     * the HTTP request, the current context, a function to call the next
     * middleware/handler in the pipeline, and a callback to invoke when
     * processing is complete.
     * 
     * The middleware can:
     * - Inspect and modify the request
     * - Add data to the context
     * - Call next() to continue the pipeline asynchronously
     * - Call callback() early to short-circuit the pipeline
     * - Modify the response in the callback from next()
     * 
     * @param request The HTTP request being processed
     * @param context Shared context for middleware communication
     * @param next Function to call the next middleware/handler
     * @param callback Function to call when processing is complete
     * 
     * @throws std::exception May throw exceptions which will be handled by the pipeline
     * 
     * @note This method must be thread-safe and must invoke callback exactly once
     * @note The callback may be invoked from any thread
     */
    virtual void handleAsync(const HttpRequest& request, Context& context,
                            AsyncNextHandler next, AsyncResponseCallback callback) = 0;
    
    /**
     * @brief Get the name of this async middleware
     * 
     * Returns a unique identifier for this middleware. Used for logging,
     * debugging, and configuration purposes.
     * 
     * @return std::string Unique name of the middleware
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Get the priority of this middleware for ordering
     * 
     * Higher priority middleware are executed earlier in the pipeline.
     * Default priority is 0. Common priority ranges:
     * - 200+: Critical security middleware (CORS, security headers)
     * - 100-199: Authentication and authorization
     * - 50-99: Request validation and parsing
     * - 0-49: Logging, metrics, and other observability
     * - Negative: Response modification and cleanup
     * 
     * @return int Priority value (higher = earlier execution)
     */
    virtual int getPriority() const { return 0; }
    
    /**
     * @brief Check if this middleware should be enabled
     * 
     * Allows runtime enabling/disabling of middleware based on configuration
     * or runtime conditions. Disabled middleware are skipped in the pipeline.
     * 
     * @return bool True if middleware should be executed
     */
    virtual bool isEnabled() const { return true; }
};

/**
 * @brief Exception thrown when async pipeline execution fails
 * 
 * This exception is thrown when the async middleware pipeline encounters
 * an unrecoverable error during execution.
 */
class AsyncPipelineException : public std::runtime_error {
public:
    explicit AsyncPipelineException(const std::string& message) 
        : std::runtime_error("Async pipeline error: " + message) {}
    
    explicit AsyncPipelineException(const std::string& message, const std::string& middlewareName)
        : std::runtime_error("Async pipeline error in " + middlewareName + ": " + message) {}
};

/**
 * @brief Asynchronous middleware pipeline execution engine
 * 
 * The AsyncMiddlewarePipeline class manages the execution of a chain of
 * asynchronous middleware components followed by a final async handler.
 * It provides:
 * 
 * - Sequential async middleware execution with proper ordering
 * - Context propagation through the entire pipeline
 * - Early termination support (middleware can abort pipeline)
 * - Exception handling and error propagation
 * - Performance monitoring and optimization
 * - Thread-safe execution with callback coordination
 * 
 * Key Features:
 * - Automatic middleware ordering by priority
 * - Context isolation between requests
 * - Performance metrics collection
 * - Error recovery and logging
 * - Integration with existing AsyncHttpHandler
 * - Callback-based flow control
 * 
 * @code{.cpp}
 * class AsyncBusinessHandler : public AsyncHttpHandler {
 * public:
 *     void handleAsync(const HttpRequest& request, ResponseCallback callback) override {
 *         // Simulate async business logic
 *         std::async(std::launch::async, [callback]() {
 *             std::this_thread::sleep_for(std::chrono::milliseconds(100));
 *             callback(HttpResponse::ok("Async business logic response"));
 *         });
 *     }
 * };
 * 
 * auto pipeline = std::make_shared<AsyncMiddlewarePipeline>();
 * 
 * // Add middleware (will be sorted by priority)
 * pipeline->addMiddleware(std::make_shared<AsyncAuthMiddleware>()); // Priority 100
 * pipeline->addMiddleware(std::make_shared<AsyncLoggingMiddleware>()); // Priority 0
 * pipeline->addMiddleware(std::make_shared<AsyncCorsMiddleware>()); // Priority 200
 * 
 * // Set the final async business logic handler
 * pipeline->setFinalHandler(std::make_shared<AsyncBusinessHandler>());
 * 
 * // Execute for each request
 * HttpRequest request("GET", "/api/data", "HTTP/1.1");
 * pipeline->executeAsync(request, [](const HttpResponse& response) {
 *     std::cout << "Final response: " << response.getStatus() << std::endl;
 * });
 * @endcode
 * 
 * @see AsyncMiddleware, AsyncHttpHandler, MiddlewarePipeline
 */
class AsyncMiddlewarePipeline {
public:
    /**
     * @brief Default constructor
     * 
     * Creates an empty async pipeline with no middleware or final handler.
     * Initializes the logger with default configuration.
     */
    AsyncMiddlewarePipeline();
    
    /**
     * @brief Destructor
     */
    virtual ~AsyncMiddlewarePipeline() = default;
    
    /**
     * @brief Add async middleware to the pipeline
     * 
     * Adds an async middleware component to the pipeline. Middleware will be
     * automatically sorted by priority before execution. Higher priority
     * middleware execute earlier in the pipeline.
     * 
     * @param middleware Shared pointer to async middleware component
     * @throws std::invalid_argument if middleware is null
     * 
     * @code{.cpp}
     * pipeline->addMiddleware(std::make_shared<AsyncAuthMiddleware>());
     * pipeline->addMiddleware(std::make_shared<AsyncLoggingMiddleware>());
     * @endcode
     */
    void addMiddleware(std::shared_ptr<AsyncMiddleware> middleware);
    
    /**
     * @brief Remove middleware from the pipeline
     * 
     * Removes the first middleware with the specified name from the pipeline.
     * 
     * @param middlewareName Name of the middleware to remove
     * @return bool True if middleware was found and removed
     * 
     * @code{.cpp}
     * bool removed = pipeline->removeMiddleware("AsyncAuthMiddleware");
     * @endcode
     */
    bool removeMiddleware(const std::string& middlewareName);
    
    /**
     * @brief Clear all middleware from the pipeline
     * 
     * Removes all middleware components from the pipeline, leaving only
     * the final handler (if set).
     */
    void clearMiddleware();
    
    /**
     * @brief Set the final async handler for the pipeline
     * 
     * Sets the final async handler that will be called after all middleware
     * have been executed. This is typically the async business logic handler.
     * 
     * @param handler Shared pointer to the final async handler
     * @throws std::invalid_argument if handler is null
     * 
     * @code{.cpp}
     * pipeline->setFinalHandler(std::make_shared<AsyncApiHandler>());
     * @endcode
     */
    void setFinalHandler(std::shared_ptr<AsyncHttpHandler> handler);
    
    /**
     * @brief Execute the async middleware pipeline
     * 
     * Executes the entire async middleware pipeline for the given request.
     * Middleware are executed in priority order, with context propagated
     * through the entire chain. The final callback is invoked when the
     * pipeline completes.
     * 
     * @param request The HTTP request to process
     * @param callback Function to call when pipeline execution completes
     * @throws AsyncPipelineException if execution setup fails
     * @throws std::runtime_error if no final handler is set
     * 
     * @code{.cpp}
     * HttpRequest request("GET", "/api/users", "HTTP/1.1");
     * pipeline->executeAsync(request, [](const HttpResponse& response) {
     *     std::cout << "Response: " << response.getStatusCode() << std::endl;
     * });
     * @endcode
     */
    void executeAsync(const HttpRequest& request, AsyncResponseCallback callback);
    
    /**
     * @brief Execute the async middleware pipeline with custom context
     * 
     * Executes the async middleware pipeline with a pre-populated context.
     * Useful for passing data from previous processing stages.
     * 
     * @param request The HTTP request to process
     * @param context Pre-populated context for the pipeline
     * @param callback Function to call when pipeline execution completes
     * @throws AsyncPipelineException if execution setup fails
     */
    void executeAsync(const HttpRequest& request, Context& context, AsyncResponseCallback callback);
    
    /**
     * @brief Get the number of middleware in the pipeline
     * @return size_t Number of middleware components
     */
    size_t getMiddlewareCount() const { return middlewares_.size(); }
    
    /**
     * @brief Check if pipeline has a final handler
     * @return bool True if final handler is set
     */
    bool hasFinalHandler() const { return finalHandler_ != nullptr; }
    
    /**
     * @brief Get names of all middleware in the pipeline
     * @return std::vector<std::string> List of middleware names in execution order
     */
    std::vector<std::string> getMiddlewareNames() const;
    
    /**
     * @brief Enable or disable performance monitoring
     * 
     * When enabled, the pipeline will collect timing information for each
     * middleware and log performance metrics.
     * 
     * @param enabled True to enable performance monitoring
     */
    void setPerformanceMonitoring(bool enabled) { performanceMonitoring_ = enabled; }
    
    /**
     * @brief Check if performance monitoring is enabled
     * @return bool True if performance monitoring is enabled
     */
    bool isPerformanceMonitoringEnabled() const { return performanceMonitoring_; }

private:
    /// @brief Vector of async middleware components
    std::vector<std::shared_ptr<AsyncMiddleware>> middlewares_;
    
    /// @brief Final async handler for the pipeline
    std::shared_ptr<AsyncHttpHandler> finalHandler_;
    
    /// @brief Flag to enable performance monitoring
    std::atomic<bool> performanceMonitoring_{false};
    
    /// @brief Mutex for thread-safe middleware manipulation
    mutable std::mutex middlewaresMutex_;
    
    /// @brief Debug logger for pipeline events
    std::shared_ptr<DebugLogger> logger_;
    
    /**
     * @brief Sort middleware by priority (internal use)
     * 
     * Sorts middleware components by priority in descending order.
     * Higher priority middleware execute first.
     */
    void sortMiddleware();
    
    /**
     * @brief Execute a single middleware asynchronously (internal use)
     * 
     * @param middleware The middleware to execute
     * @param request The HTTP request
     * @param context The middleware context
     * @param next The next handler function
     * @param callback The final callback
     */
    void executeMiddleware(std::shared_ptr<AsyncMiddleware> middleware,
                          const HttpRequest& request,
                          Context& context,
                          AsyncNextHandler next,
                          AsyncResponseCallback callback);
    
    /**
     * @brief Execute the final handler (internal use)
     * 
     * @param request The HTTP request
     * @param context The middleware context
     * @param callback The final callback
     */
    void executeFinalHandler(const HttpRequest& request, Context& context, AsyncResponseCallback callback);
    
    /**
     * @brief Execute middleware chain recursively (internal use)
     * 
     * @param request The HTTP request
     * @param context The middleware context
     * @param index Current middleware index
     * @param callback The final callback
     */
    void executeMiddlewareChain(const HttpRequest& request, Context& context, 
                               size_t index, AsyncResponseCallback callback);
    
    /**
     * @brief Log performance metrics (internal use)
     * 
     * @param middlewareName Name of the middleware
     * @param duration Execution duration
     */
    void logPerformance(const std::string& middlewareName, std::chrono::milliseconds duration);
};

} // namespace cppSwitchboard 