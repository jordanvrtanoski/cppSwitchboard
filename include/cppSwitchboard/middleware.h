/**
 * @file middleware.h
 * @brief Middleware base classes and interfaces for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 * 
 * This file defines the core middleware interface and context management
 * for the cppSwitchboard framework. It provides the foundation for creating
 * middleware pipelines that can process HTTP requests in a configurable chain.
 * 
 * @section middleware_example Middleware Usage Example
 * @code{.cpp}
 * class LoggingMiddleware : public Middleware {
 * public:
 *     HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
 *         // Log request
 *         std::cout << "Processing: " << request.getPath() << std::endl;
 *         
 *         // Add timing context
 *         context["start_time"] = std::chrono::steady_clock::now();
 *         
 *         // Call next middleware/handler
 *         HttpResponse response = next(request, context);
 *         
 *         // Log response
 *         std::cout << "Response: " << response.getStatusCode() << std::endl;
 *         
 *         return response;
 *     }
 *     
 *     std::string getName() const override { return "LoggingMiddleware"; }
 * };
 * @endcode
 * 
 * @see Middleware
 * @see MiddlewarePipeline
 */
#pragma once

#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <functional>
#include <memory>
#include <unordered_map>
#include <any>
#include <string>
#include <mutex>

namespace cppSwitchboard {

/**
 * @brief Context type for middleware communication
 * 
 * The middleware context is a key-value store that flows through the entire
 * middleware pipeline. It allows middleware to share state and data with
 * subsequent middleware and the final handler.
 * 
 * Thread Safety: Context operations are thread-safe when properly synchronized
 * by the middleware pipeline.
 * 
 * @code{.cpp}
 * void someMiddleware(const HttpRequest& request, Context& context, NextHandler next) {
 *     // Store user information
 *     context["user_id"] = std::string("12345");
 *     context["roles"] = std::vector<std::string>{"admin", "user"};
 *     
 *     // Retrieve data later
 *     if (context.find("user_id") != context.end()) {
 *         auto userId = std::any_cast<std::string>(context["user_id"]);
 *     }
 * }
 * @endcode
 */
using Context = std::unordered_map<std::string, std::any>;

// Forward declaration
class Middleware;

/**
 * @brief Function type for the next handler in the pipeline
 * 
 * The NextHandler function represents the next step in the middleware pipeline.
 * It can be either another middleware or the final request handler. Middleware
 * must call this function to continue the pipeline, unless they want to
 * short-circuit the execution.
 * 
 * @param request The HTTP request being processed
 * @param context The middleware context for sharing state
 * @return HttpResponse The response from the next handler/middleware
 */
using NextHandler = std::function<HttpResponse(const HttpRequest&, Context&)>;

/**
 * @brief Abstract base class for middleware components
 * 
 * The Middleware class defines the interface that all middleware components
 * must implement. Middleware can inspect and modify requests, handle responses,
 * manage context, and control the flow of the pipeline.
 * 
 * Key responsibilities:
 * - Process incoming requests before they reach the handler
 * - Modify or validate request data
 * - Add information to the context for downstream components
 * - Process responses before they are sent to the client
 * - Handle errors and exceptions
 * - Control pipeline flow (continue, abort, redirect)
 * 
 * Implementation Requirements:
 * - Thread-safe: Middleware may be called from multiple threads
 * - Exception-safe: Should handle exceptions gracefully
 * - Performance-conscious: Minimize overhead in the request path
 * 
 * @code{.cpp}
 * class AuthMiddleware : public Middleware {
 * public:
 *     HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
 *         // Check authentication
 *         std::string token = request.getHeader("Authorization");
 *         if (token.empty()) {
 *             return HttpResponse::unauthorized("Missing authorization header");
 *         }
 *         
 *         // Validate token and extract user info
 *         if (!validateToken(token)) {
 *             return HttpResponse::unauthorized("Invalid token");
 *         }
 *         
 *         // Add user info to context
 *         context["user_id"] = extractUserId(token);
 *         context["authenticated"] = true;
 *         
 *         // Continue to next middleware/handler
 *         return next(request, context);
 *     }
 *     
 *     std::string getName() const override { return "AuthMiddleware"; }
 *     int getPriority() const override { return 100; } // High priority for auth
 * };
 * @endcode
 * 
 * @see NextHandler, Context, MiddlewarePipeline
 */
class Middleware {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~Middleware() = default;
    
    /**
     * @brief Process an HTTP request through the middleware
     * 
     * This is the main method that middleware must implement. It receives
     * the HTTP request, the current context, and a function to call the
     * next middleware/handler in the pipeline.
     * 
     * The middleware can:
     * - Inspect and modify the request
     * - Add data to the context
     * - Call next() to continue the pipeline
     * - Return early to short-circuit the pipeline
     * - Modify the response after calling next()
     * 
     * @param request The HTTP request being processed
     * @param context Shared context for middleware communication
     * @param next Function to call the next middleware/handler
     * @return HttpResponse The final response (may be modified)
     * 
     * @throws std::exception May throw exceptions which will be handled by the pipeline
     * 
     * @note This method must be thread-safe as it may be called concurrently
     */
    virtual HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) = 0;
    
    /**
     * @brief Get the name of this middleware
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
 * @brief Thread-safe context helper class
 * 
 * Provides thread-safe operations on the middleware context.
 * While the basic Context is thread-safe when properly synchronized
 * by the pipeline, this helper provides additional safety for
 * complex operations.
 * 
 * @code{.cpp}
 * ContextHelper helper(context);
 * helper.setString("key", "value");
 * auto value = helper.getString("key", "default");
 * @endcode
 */
class ContextHelper {
public:
    /**
     * @brief Constructor taking a reference to the context
     * @param context Reference to the middleware context
     */
    explicit ContextHelper(Context& context) : context_(context) {}
    
    /**
     * @brief Set a string value in the context
     * @param key The key to store the value under
     * @param value The string value to store
     */
    void setString(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        context_[key] = value;
    }
    
    /**
     * @brief Get a string value from the context
     * @param key The key to look up
     * @param defaultValue Default value if key is not found
     * @return std::string The stored value or default
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = context_.find(key);
        if (it != context_.end()) {
            try {
                return std::any_cast<std::string>(it->second);
            } catch (const std::bad_any_cast&) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
    
    /**
     * @brief Set a boolean value in the context
     * @param key The key to store the value under
     * @param value The boolean value to store
     */
    void setBool(const std::string& key, bool value) {
        std::lock_guard<std::mutex> lock(mutex_);
        context_[key] = value;
    }
    
    /**
     * @brief Get a boolean value from the context
     * @param key The key to look up
     * @param defaultValue Default value if key is not found
     * @return bool The stored value or default
     */
    bool getBool(const std::string& key, bool defaultValue = false) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = context_.find(key);
        if (it != context_.end()) {
            try {
                return std::any_cast<bool>(it->second);
            } catch (const std::bad_any_cast&) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
    
    /**
     * @brief Check if a key exists in the context
     * @param key The key to check
     * @return bool True if the key exists
     */
    bool hasKey(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return context_.find(key) != context_.end();
    }
    
    /**
     * @brief Remove a key from the context
     * @param key The key to remove
     * @return bool True if the key was found and removed
     */
    bool removeKey(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return context_.erase(key) > 0;
    }

private:
    Context& context_;
    mutable std::mutex mutex_;
};

} // namespace cppSwitchboard 