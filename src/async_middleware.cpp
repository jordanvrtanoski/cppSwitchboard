/**
 * @file async_middleware.cpp
 * @brief Implementation of asynchronous middleware pipeline for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>  
 * @date 2025-01-08
 * @version 1.2.0
 */

#include <cppSwitchboard/async_middleware.h>
#include <algorithm>
#include <future>
#include <iostream>
#include <sstream>

namespace cppSwitchboard {

// AsyncMiddlewarePipeline Implementation

AsyncMiddlewarePipeline::AsyncMiddlewarePipeline() {
    // Logger initialization removed for now due to API constraints
    logger_ = nullptr;
}

void AsyncMiddlewarePipeline::addMiddleware(std::shared_ptr<AsyncMiddleware> middleware) {
    if (!middleware) {
        throw std::invalid_argument("Middleware cannot be null");
    }
    
    std::lock_guard<std::mutex> lock(middlewaresMutex_);
    middlewares_.push_back(middleware);
    
    // Sort middleware by priority after adding
    sortMiddleware();
    
    // TODO: Add logging when proper logger interface is available
}

bool AsyncMiddlewarePipeline::removeMiddleware(const std::string& middlewareName) {
    std::lock_guard<std::mutex> lock(middlewaresMutex_);
    
    auto it = std::find_if(middlewares_.begin(), middlewares_.end(),
        [&middlewareName](const std::shared_ptr<AsyncMiddleware>& middleware) {
            return middleware->getName() == middlewareName;
        });
    
    if (it != middlewares_.end()) {
        middlewares_.erase(it);
        // TODO: Add logging when proper logger interface is available
        return true;
    }
    
    return false;
}

void AsyncMiddlewarePipeline::clearMiddleware() {
    std::lock_guard<std::mutex> lock(middlewaresMutex_);
    middlewares_.clear();
    
    // TODO: Add logging when proper logger interface is available
}

void AsyncMiddlewarePipeline::setFinalHandler(std::shared_ptr<AsyncHttpHandler> handler) {
    if (!handler) {
        throw std::invalid_argument("Final handler cannot be null");
    }
    
    finalHandler_ = handler;
    
    // TODO: Add logging when proper logger interface is available
}

void AsyncMiddlewarePipeline::executeAsync(const HttpRequest& request, AsyncResponseCallback callback) {
    Context context;
    executeAsync(request, context, callback);
}

void AsyncMiddlewarePipeline::executeAsync(const HttpRequest& request, Context& context, 
                                          AsyncResponseCallback callback) {
    if (!finalHandler_) {
        if (callback) {
            HttpResponse response(500);
            response.setBody("No final handler set in async pipeline");
            callback(response);
        }
        return;
    }
    
    if (!callback) {
        // TODO: Add logging when proper logger interface is available
        return;
    }
    
    try {
        // Start performance monitoring if enabled
        auto startTime = performanceMonitoring_ ? std::chrono::steady_clock::now() : 
                        std::chrono::steady_clock::time_point{};
        
        // TODO: Add logging when proper logger interface is available
        
        // Start middleware chain execution
        executeMiddlewareChain(request, context, 0, [this, callback, startTime](const HttpResponse& response) {
            // Log total execution time if monitoring is enabled
            if (performanceMonitoring_) {
                auto endTime = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                logPerformance("Total Pipeline", duration);
            }
            
            callback(response);
        });
        
    } catch (const std::exception& e) {
        // TODO: Add logging when proper logger interface is available
        
        if (callback) {
            HttpResponse response(500);
            response.setBody("Pipeline execution failed: " + std::string(e.what()));
            callback(response);
        }
    }
}

std::vector<std::string> AsyncMiddlewarePipeline::getMiddlewareNames() const {
    std::lock_guard<std::mutex> lock(middlewaresMutex_);
    std::vector<std::string> names;
    names.reserve(middlewares_.size());
    
    for (const auto& middleware : middlewares_) {
        names.push_back(middleware->getName());
    }
    
    return names;
}

void AsyncMiddlewarePipeline::sortMiddleware() {
    // Sort by priority in descending order (higher priority first)
    std::sort(middlewares_.begin(), middlewares_.end(),
        [](const std::shared_ptr<AsyncMiddleware>& a, const std::shared_ptr<AsyncMiddleware>& b) {
            return a->getPriority() > b->getPriority();
        });
}

void AsyncMiddlewarePipeline::executeMiddleware(std::shared_ptr<AsyncMiddleware> middleware,
                                              const HttpRequest& request,
                                              Context& context,
                                              AsyncNextHandler next,
                                              AsyncResponseCallback callback) {
    try {
        // Check if middleware is enabled
        if (!middleware->isEnabled()) {
            // TODO: Add logging when proper logger interface is available
            next(request, context, callback);
            return;
        }
        
        // Start timing if performance monitoring is enabled
        auto startTime = performanceMonitoring_ ? std::chrono::steady_clock::now() : 
                        std::chrono::steady_clock::time_point{};
        
        // TODO: Add logging when proper logger interface is available
        
        // Execute the middleware with a wrapped callback for performance monitoring
        middleware->handleAsync(request, context, next, 
            [this, middleware, callback, startTime](const HttpResponse& response) {
                // Log execution time if monitoring is enabled
                if (performanceMonitoring_) {
                    auto endTime = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                    logPerformance(middleware->getName(), duration);
                }
                
                callback(response);
            });
            
    } catch (const std::exception& e) {
        // TODO: Add logging when proper logger interface is available
        
        HttpResponse response(500);
        response.setBody("Middleware error in " + middleware->getName() + ": " + e.what());
        callback(response);
    }
}

void AsyncMiddlewarePipeline::executeFinalHandler(const HttpRequest& request, Context& context, 
                                                 AsyncResponseCallback callback) {
    try {
        // TODO: Add logging when proper logger interface is available
        
        // Start timing if performance monitoring is enabled
        auto startTime = performanceMonitoring_ ? std::chrono::steady_clock::now() : 
                        std::chrono::steady_clock::time_point{};
        
        finalHandler_->handleAsync(request, [this, callback, startTime](const HttpResponse& response) {
            // Log execution time if monitoring is enabled
            if (performanceMonitoring_) {
                auto endTime = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                logPerformance("Final Handler", duration);
            }
            
            callback(response);
        });
        
    } catch (const std::exception& e) {
        // TODO: Add logging when proper logger interface is available
        
        HttpResponse response(500);
        response.setBody("Final handler error: " + std::string(e.what()));
        callback(response);
    }
}

void AsyncMiddlewarePipeline::executeMiddlewareChain(const HttpRequest& request, Context& context,
                                                   size_t index, AsyncResponseCallback callback) {
    std::shared_ptr<AsyncMiddleware> middleware;
    bool shouldExecuteFinalHandler = false;
    
    // Scope the lock to avoid manual destructor calls
    {
        std::lock_guard<std::mutex> lock(middlewaresMutex_);
        
        // If we've processed all middleware, execute the final handler
        if (index >= middlewares_.size()) {
            shouldExecuteFinalHandler = true;
        } else {
            // Get current middleware
            middleware = middlewares_[index];
        }
    } // Lock is automatically released here
    
    if (shouldExecuteFinalHandler) {
        executeFinalHandler(request, context, callback);
        return;
    }
    
    // Create next handler that continues the chain
    AsyncNextHandler next = [this, index](const HttpRequest& req, Context& ctx, AsyncResponseCallback cb) {
        executeMiddlewareChain(req, ctx, index + 1, cb);
    };
    
    // Execute current middleware
    executeMiddleware(middleware, request, context, next, callback);
}

void AsyncMiddlewarePipeline::logPerformance(const std::string& middlewareName, 
                                           std::chrono::milliseconds duration) {
    // TODO: Add logging when proper logger interface is available
    (void)middlewareName; // Suppress unused parameter warning
    (void)duration; // Suppress unused parameter warning
}

} // namespace cppSwitchboard 