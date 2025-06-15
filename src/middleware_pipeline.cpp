/**
 * @file middleware_pipeline.cpp
 * @brief Implementation of middleware pipeline execution engine
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#include <cppSwitchboard/middleware_pipeline.h>
#include <cppSwitchboard/debug_logger.h>
#include <algorithm>
#include <sstream>

namespace cppSwitchboard {

void MiddlewarePipeline::addMiddleware(std::shared_ptr<Middleware> middleware) {
    if (!middleware) {
        throw std::invalid_argument("Middleware cannot be null");
    }
    
    middlewares_.push_back(middleware);
    middlewareSorted_ = false;  // Mark as needing sort
    
    // Log middleware addition
        // Debug logging removed for compilation - can be re-added later
}

bool MiddlewarePipeline::removeMiddleware(const std::string& middlewareName) {
    auto it = std::find_if(middlewares_.begin(), middlewares_.end(),
        [&middlewareName](const std::shared_ptr<Middleware>& middleware) {
            return middleware->getName() == middlewareName;
        });
    
    if (it != middlewares_.end()) {
        // Debug logging removed for compilation
        middlewares_.erase(it);
        return true;
    }
    
    return false;
}

void MiddlewarePipeline::clearMiddleware() {
    // Debug logging removed for compilation
    middlewares_.clear();
    middlewareSorted_ = true;  // Empty list is technically sorted
}

void MiddlewarePipeline::setFinalHandler(std::shared_ptr<HttpHandler> handler) {
    if (!handler) {
        throw std::invalid_argument("Final handler cannot be null");
    }
    
    finalHandler_ = handler;
    finalAsyncHandler_.reset();  // Clear async handler if set
    
    // Debug logging removed for compilation
}

void MiddlewarePipeline::setFinalAsyncHandler(std::shared_ptr<AsyncHttpHandler> handler) {
    if (!handler) {
        throw std::invalid_argument("Final async handler cannot be null");
    }
    
    finalAsyncHandler_ = handler;
    finalHandler_.reset();  // Clear sync handler if set
    
    // Debug logging removed for compilation
}

HttpResponse MiddlewarePipeline::execute(const HttpRequest& request) {
    Context context;
    return execute(request, context);
}

HttpResponse MiddlewarePipeline::execute(const HttpRequest& request, Context& context) {
    if (!hasFinalHandler()) {
        throw std::runtime_error("No final handler set for pipeline execution");
    }
    
    // Sort middleware by priority if needed
    if (!middlewareSorted_) {
        sortMiddleware();
    }
    
    // Debug logging removed for compilation
    
    try {
        // Start pipeline execution
        if (middlewares_.empty()) {
            // No middleware, execute final handler directly
            return executeFinalHandler(request, context);
        } else {
            // Execute middleware chain starting from index 0
            return executeMiddlewareChain(request, context, 0);
        }
    } catch (const PipelineException&) {
        // Re-throw pipeline exceptions as-is
        throw;
    } catch (const std::exception& e) {
        // Wrap other exceptions as pipeline exceptions
        throw PipelineException(std::string("Unexpected error during pipeline execution: ") + e.what());
    }
}

HttpResponse MiddlewarePipeline::executeMiddlewareChain(const HttpRequest& request, Context& context, size_t index) {
    if (index >= middlewares_.size()) {
        // No more middleware, execute final handler
        return executeFinalHandler(request, context);
    }
    
    // Create next handler that continues the chain
    NextHandler next = [this, index](const HttpRequest& req, Context& ctx) -> HttpResponse {
        return executeMiddlewareChain(req, ctx, index + 1);
    };
    
    // Execute current middleware
    return executeMiddleware(middlewares_[index], request, context, next);
}

std::vector<std::string> MiddlewarePipeline::getMiddlewareNames() const {
    // Ensure middleware are sorted before returning names
    // Note: We need to cast away const to sort, but this is logically const
    // since sorting doesn't change the logical state, just the physical order
    auto* self = const_cast<MiddlewarePipeline*>(this);
    if (!middlewareSorted_) {
        self->sortMiddleware();
    }
    
    std::vector<std::string> names;
    names.reserve(middlewares_.size());
    
    for (const auto& middleware : middlewares_) {
        names.push_back(middleware->getName());
    }
    
    return names;
}

void MiddlewarePipeline::sortMiddleware() {
    // Sort by priority (higher priority first)
    std::sort(middlewares_.begin(), middlewares_.end(),
        [](const std::shared_ptr<Middleware>& a, const std::shared_ptr<Middleware>& b) {
            return a->getPriority() > b->getPriority();
        });
    
    middlewareSorted_ = true;
    
    // Debug logging removed for compilation
}

HttpResponse MiddlewarePipeline::executeMiddleware(std::shared_ptr<Middleware> middleware, 
                                                   const HttpRequest& request, 
                                                   Context& context, 
                                                   NextHandler next) {
    if (!middleware->isEnabled()) {
        // Skip disabled middleware
        // Debug logging removed for compilation
        return next(request, context);
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        // Debug logging removed for compilation
        
        HttpResponse response = middleware->handle(request, context, next);
        
        // Log performance if enabled
        if (performanceMonitoring_) {
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            logPerformance(middleware->getName(), duration);
        }
        
        // Debug logging removed for compilation
        
        return response;
    } catch (const std::exception& e) {
        // Log error and wrap in pipeline exception
        std::string errorMsg = "Error in middleware " + middleware->getName() + ": " + e.what();
        
        // Debug logging removed for compilation
        
        throw PipelineException(e.what(), middleware->getName());
    }
}

HttpResponse MiddlewarePipeline::executeFinalHandler(const HttpRequest& request, Context& context) {
    (void)context; // Context not used in current implementation but part of interface
    if (!hasFinalHandler()) {
        throw std::runtime_error("No final handler available for execution");
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        HttpResponse response;
        
        if (finalHandler_) {
            // Execute synchronous final handler
            // Debug logging removed for compilation
            
            response = finalHandler_->handle(request);
        } else if (finalAsyncHandler_) {
            // For now, we don't support async final handlers in the sync pipeline
            // This will be implemented in Phase 3 (Task 3.2: Async Middleware Support)
            throw std::runtime_error("Async final handlers not yet supported in synchronous pipeline");
        }
        
        // Log performance if enabled
        if (performanceMonitoring_) {
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            logPerformance("FinalHandler", duration);
        }
        
        // Debug logging removed for compilation
        
        return response;
    } catch (const std::exception& e) {
        // Log error and wrap in pipeline exception
        std::string errorMsg = "Error in final handler: " + std::string(e.what());
        
        // Debug logging removed for compilation
        
        throw PipelineException(e.what(), "FinalHandler");
    }
}

void MiddlewarePipeline::logPerformance(const std::string& middlewareName, 
                                       std::chrono::milliseconds duration) {
    (void)middlewareName; // Parameters reserved for future performance logging implementation
    (void)duration;
    // Debug logging removed for compilation
}

} // namespace cppSwitchboard 