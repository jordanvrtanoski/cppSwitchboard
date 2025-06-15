/**
 * @file middleware_pipeline.h
 * @brief Middleware pipeline execution engine for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 * 
 * This file defines the MiddlewarePipeline class which manages the execution
 * of middleware chains in the cppSwitchboard framework. It handles sequential
 * middleware execution, context propagation, error handling, and performance
 * optimization.
 * 
 * @section pipeline_example Pipeline Usage Example
 * @code{.cpp}
 * auto pipeline = std::make_shared<MiddlewarePipeline>();
 * 
 * // Add middleware in order
 * pipeline->addMiddleware(std::make_shared<LoggingMiddleware>());
 * pipeline->addMiddleware(std::make_shared<AuthMiddleware>());
 * pipeline->addMiddleware(std::make_shared<ValidationMiddleware>());
 * 
 * // Set final handler
 * pipeline->setFinalHandler(std::make_shared<BusinessLogicHandler>());
 * 
 * // Execute pipeline
 * HttpResponse response = pipeline->execute(request);
 * @endcode
 * 
 * @see Middleware
 * @see Context
 * @see NextHandler
 */
#pragma once

#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_handler.h>
#include <cppSwitchboard/debug_logger.h>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <stdexcept>
#include <algorithm>

namespace cppSwitchboard {

/**
 * @brief Exception thrown when pipeline execution fails
 * 
 * This exception is thrown when the middleware pipeline encounters
 * an unrecoverable error during execution.
 */
class PipelineException : public std::runtime_error {
public:
    explicit PipelineException(const std::string& message) 
        : std::runtime_error("Pipeline error: " + message) {}
    
    explicit PipelineException(const std::string& message, const std::string& middlewareName)
        : std::runtime_error("Pipeline error in " + middlewareName + ": " + message) {}
};

/**
 * @brief Middleware pipeline execution engine
 * 
 * The MiddlewarePipeline class manages the execution of a chain of middleware
 * components followed by a final handler. It provides:
 * 
 * - Sequential middleware execution with proper ordering
 * - Context propagation through the entire pipeline
 * - Early termination support (middleware can abort pipeline)
 * - Exception handling and error propagation
 * - Performance monitoring and optimization
 * - Thread-safe execution
 * 
 * Key Features:
 * - Automatic middleware ordering by priority
 * - Context isolation between requests
 * - Performance metrics collection
 * - Error recovery and logging
 * - Support for both sync and async handlers
 * 
 * @code{.cpp}
 * class BusinessHandler : public HttpHandler {
 * public:
 *     HttpResponse handle(const HttpRequest& request) override {
 *         return HttpResponse::ok("Business logic response");
 *     }
 * };
 * 
 * auto pipeline = std::make_shared<MiddlewarePipeline>();
 * 
 * // Add middleware (will be sorted by priority)
 * pipeline->addMiddleware(std::make_shared<AuthMiddleware>()); // Priority 100
 * pipeline->addMiddleware(std::make_shared<LoggingMiddleware>()); // Priority 0
 * pipeline->addMiddleware(std::make_shared<CorsMiddleware>()); // Priority 200
 * 
 * // Set the final business logic handler
 * pipeline->setFinalHandler(std::make_shared<BusinessHandler>());
 * 
 * // Execute for each request
 * HttpRequest request("GET", "/api/data", "HTTP/1.1");
 * HttpResponse response = pipeline->execute(request);
 * @endcode
 * 
 * @see Middleware, HttpHandler, AsyncHttpHandler
 */
class MiddlewarePipeline {
public:
    /**
     * @brief Default constructor
     * 
     * Creates an empty pipeline with no middleware or final handler.
     */
    MiddlewarePipeline() = default;
    
    /**
     * @brief Destructor
     */
    virtual ~MiddlewarePipeline() = default;
    
    /**
     * @brief Add middleware to the pipeline
     * 
     * Adds a middleware component to the pipeline. Middleware will be
     * automatically sorted by priority before execution. Higher priority
     * middleware execute earlier in the pipeline.
     * 
     * @param middleware Shared pointer to middleware component
     * @throws std::invalid_argument if middleware is null
     * 
     * @code{.cpp}
     * pipeline->addMiddleware(std::make_shared<AuthMiddleware>());
     * pipeline->addMiddleware(std::make_shared<LoggingMiddleware>());
     * @endcode
     */
    void addMiddleware(std::shared_ptr<Middleware> middleware);
    
    /**
     * @brief Remove middleware from the pipeline
     * 
     * Removes the first middleware with the specified name from the pipeline.
     * 
     * @param middlewareName Name of the middleware to remove
     * @return bool True if middleware was found and removed
     * 
     * @code{.cpp}
     * bool removed = pipeline->removeMiddleware("AuthMiddleware");
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
     * @brief Set the final handler for the pipeline
     * 
     * Sets the final handler that will be called after all middleware
     * have been executed. This is typically the business logic handler.
     * 
     * @param handler Shared pointer to the final handler
     * @throws std::invalid_argument if handler is null
     * 
     * @code{.cpp}
     * pipeline->setFinalHandler(std::make_shared<ApiHandler>());
     * @endcode
     */
    void setFinalHandler(std::shared_ptr<HttpHandler> handler);
    
    /**
     * @brief Set the final async handler for the pipeline
     * 
     * Sets the final async handler that will be called after all middleware
     * have been executed. Used for asynchronous request processing.
     * 
     * @param handler Shared pointer to the final async handler
     * @throws std::invalid_argument if handler is null
     */
    void setFinalAsyncHandler(std::shared_ptr<AsyncHttpHandler> handler);
    
    /**
     * @brief Execute the middleware pipeline
     * 
     * Executes the entire middleware pipeline for the given request.
     * Middleware are executed in priority order, with context propagated
     * through the entire chain.
     * 
     * @param request The HTTP request to process
     * @return HttpResponse The final response after pipeline execution
     * @throws PipelineException if execution fails
     * @throws std::runtime_error if no final handler is set
     * 
     * @code{.cpp}
     * HttpRequest request("GET", "/api/users", "HTTP/1.1");
     * HttpResponse response = pipeline->execute(request);
     * @endcode
     */
    HttpResponse execute(const HttpRequest& request);
    
    /**
     * @brief Execute the middleware pipeline with custom context
     * 
     * Executes the pipeline with a pre-initialized context. Useful for
     * testing or when context needs to be pre-populated.
     * 
     * @param request The HTTP request to process
     * @param context Pre-initialized context
     * @return HttpResponse The final response after pipeline execution
     * @throws PipelineException if execution fails
     */
    HttpResponse execute(const HttpRequest& request, Context& context);
    
    /**
     * @brief Get the number of middleware in the pipeline
     * 
     * @return size_t Number of middleware components
     */
    size_t getMiddlewareCount() const { return middlewares_.size(); }
    
    /**
     * @brief Check if pipeline has a final handler
     * 
     * @return bool True if a final handler is set
     */
    bool hasFinalHandler() const { return finalHandler_ != nullptr || finalAsyncHandler_ != nullptr; }
    
    /**
     * @brief Get names of all middleware in the pipeline
     * 
     * Returns the names of all middleware in execution order.
     * 
     * @return std::vector<std::string> Middleware names in execution order
     */
    std::vector<std::string> getMiddlewareNames() const;
    
    /**
     * @brief Enable or disable performance monitoring
     * 
     * When enabled, the pipeline will collect timing information
     * for each middleware and the final handler.
     * 
     * @param enabled True to enable performance monitoring
     */
    void setPerformanceMonitoring(bool enabled) { performanceMonitoring_ = enabled; }
    
    /**
     * @brief Check if performance monitoring is enabled
     * 
     * @return bool True if performance monitoring is enabled
     */
    bool isPerformanceMonitoringEnabled() const { return performanceMonitoring_; }

private:
    /**
     * @brief Sort middleware by priority
     * 
     * Sorts the middleware vector by priority in descending order
     * (higher priority first).
     */
    void sortMiddleware();
    
    /**
     * @brief Execute a single middleware with error handling
     * 
     * @param middleware The middleware to execute
     * @param request The HTTP request
     * @param context The request context
     * @param next The next handler function
     * @return HttpResponse The response from the middleware
     * @throws PipelineException if middleware execution fails
     */
    HttpResponse executeMiddleware(std::shared_ptr<Middleware> middleware, 
                                   const HttpRequest& request, 
                                   Context& context, 
                                   NextHandler next);
    
    /**
     * @brief Execute the final handler
     * 
     * @param request The HTTP request
     * @param context The request context (may contain data from middleware)
     * @return HttpResponse The response from the final handler
     * @throws std::runtime_error if no final handler is set
     */
    HttpResponse executeFinalHandler(const HttpRequest& request, Context& context);
    
    /**
     * @brief Execute middleware chain starting from a specific index
     * 
     * @param request The HTTP request
     * @param context The request context
     * @param index Starting index in the middleware vector
     * @return HttpResponse The response after executing the chain
     */
    HttpResponse executeMiddlewareChain(const HttpRequest& request, Context& context, size_t index);
    
    /**
     * @brief Log performance metrics
     * 
     * @param middlewareName Name of the component
     * @param duration Execution duration
     */
    void logPerformance(const std::string& middlewareName, 
                       std::chrono::milliseconds duration);

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;    ///< List of middleware components
    std::shared_ptr<HttpHandler> finalHandler_;              ///< Final synchronous handler
    std::shared_ptr<AsyncHttpHandler> finalAsyncHandler_;    ///< Final asynchronous handler
    bool middlewareSorted_ = false;                          ///< Whether middleware are sorted by priority
    bool performanceMonitoring_ = false;                     ///< Whether to collect performance metrics
};

} // namespace cppSwitchboard 