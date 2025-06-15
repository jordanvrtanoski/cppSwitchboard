/**
 * @file custom_middleware.cpp
 * @brief Custom middleware implementation example
 * @author cppSwitchboard Development Team
 * @date 2025-01-08
 * @version 1.2.0
 * 
 * This example demonstrates how to create custom middleware for the cppSwitchboard
 * framework, including synchronous and asynchronous middleware, middleware with
 * configuration, and integration with the middleware factory system.
 */

#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/async_middleware.h>
#include <cppSwitchboard/middleware_factory.h>
#include <cppSwitchboard/middleware_config.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <csignal>

using namespace cppSwitchboard;

// Global server instance for signal handling
std::shared_ptr<HttpServer> g_server;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

/**
 * @brief Example 1: Simple Request ID Middleware
 * 
 * This middleware adds a unique request ID to each request and response.
 * It demonstrates basic middleware concepts and context usage.
 */
class RequestIdMiddleware : public Middleware {
private:
    std::string prefix_;
    std::atomic<uint64_t> counter_{0};

public:
    explicit RequestIdMiddleware(const std::string& prefix = "req") 
        : prefix_(prefix) {}

    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        // Generate unique request ID
        auto requestId = prefix_ + "_" + std::to_string(++counter_) + "_" + 
                        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count());
        
        // Add to context for other middleware/handlers to use
        ContextHelper helper(context);
        helper.setString("request_id", requestId);
        
        std::cout << "[RequestIdMiddleware] Processing request: " << requestId << std::endl;
        
        // Continue to next middleware
        HttpResponse response = next(request, context);
        
        // Add request ID to response headers
        response.setHeader("X-Request-ID", requestId);
        
        std::cout << "[RequestIdMiddleware] Completed request: " << requestId << std::endl;
        
        return response;
    }

    std::string getName() const override { return "RequestIdMiddleware"; }
    int getPriority() const override { return 150; }  // High priority to run early
};

/**
 * @brief Example 2: Performance Monitoring Middleware
 * 
 * This middleware measures request processing time and logs performance metrics.
 * It demonstrates timing, logging, and response modification.
 */
class PerformanceMiddleware : public Middleware {
private:
    std::chrono::milliseconds warningThreshold_;
    bool logSlowRequests_;

public:
    explicit PerformanceMiddleware(std::chrono::milliseconds warningThreshold = std::chrono::milliseconds(1000),
                                  bool logSlowRequests = true)
        : warningThreshold_(warningThreshold), logSlowRequests_(logSlowRequests) {}

    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        auto startTime = std::chrono::steady_clock::now();
        
        // Get request ID from context (if available)
        ContextHelper helper(context);
        std::string requestId = helper.getString("request_id", "unknown");
        
        std::cout << "[PerformanceMiddleware] Starting timer for request: " << requestId << std::endl;
        
        // Process request
        HttpResponse response = next(request, context);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Add timing information to context
        helper.setInt("processing_time_ms", static_cast<int>(duration.count()));
        
        // Add performance headers
        response.setHeader("X-Processing-Time", std::to_string(duration.count()) + "ms");
        response.setHeader("X-Timestamp", std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
        
        // Log slow requests
        if (logSlowRequests_ && duration > warningThreshold_) {
            std::cout << "[PerformanceMiddleware] SLOW REQUEST DETECTED: " << requestId 
                     << " took " << duration.count() << "ms (threshold: " 
                     << warningThreshold_.count() << "ms)" << std::endl;
        } else {
            std::cout << "[PerformanceMiddleware] Request " << requestId 
                     << " completed in " << duration.count() << "ms" << std::endl;
        }
        
        return response;
    }

    std::string getName() const override { return "PerformanceMiddleware"; }
    int getPriority() const override { return 50; }  // Low priority to measure total time
    
    void setWarningThreshold(std::chrono::milliseconds threshold) { 
        warningThreshold_ = threshold; 
    }
};

/**
 * @brief Example 3: Custom Header Middleware
 * 
 * This middleware adds custom headers based on configuration.
 * It demonstrates configurable middleware and conditional processing.
 */
class CustomHeaderMiddleware : public Middleware {
private:
    std::map<std::string, std::string> headers_;
    bool includeServerInfo_;
    std::string serverVersion_;

public:
    explicit CustomHeaderMiddleware(const std::map<std::string, std::string>& headers = {},
                                   bool includeServerInfo = true,
                                   const std::string& serverVersion = "1.2.0")
        : headers_(headers), includeServerInfo_(includeServerInfo), serverVersion_(serverVersion) {}

    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        ContextHelper helper(context);
        std::string requestId = helper.getString("request_id", "unknown");
        
        std::cout << "[CustomHeaderMiddleware] Adding custom headers for request: " << requestId << std::endl;
        
        // Process request
        HttpResponse response = next(request, context);
        
        // Add configured custom headers
        for (const auto& [name, value] : headers_) {
            response.setHeader(name, value);
        }
        
        // Add server information if enabled
        if (includeServerInfo_) {
            response.setHeader("X-Powered-By", "cppSwitchboard/" + serverVersion_);
            response.setHeader("X-Server-Name", "CustomMiddlewareExample");
        }
        
        // Add security headers
        response.setHeader("X-Content-Type-Options", "nosniff");
        response.setHeader("X-Frame-Options", "DENY");
        response.setHeader("X-XSS-Protection", "1; mode=block");
        
        return response;
    }

    std::string getName() const override { return "CustomHeaderMiddleware"; }
    int getPriority() const override { return 25; }  // Low priority, close to response
    
    void addHeader(const std::string& name, const std::string& value) {
        headers_[name] = value;
    }
    
    void removeHeader(const std::string& name) {
        headers_.erase(name);
    }
};

/**
 * @brief Example 4: Asynchronous Middleware
 * 
 * This demonstrates asynchronous middleware that performs non-blocking operations.
 */
class AsyncValidationMiddleware : public AsyncMiddleware {
private:
    std::mt19937 rng_;
    std::uniform_int_distribution<int> delay_dist_;

public:
    AsyncValidationMiddleware() : rng_(std::random_device{}()), delay_dist_(100, 500) {}

    void handleAsync(const HttpRequest& request, Context& context,
                    NextAsyncHandler next, AsyncCallback callback) override {
        
        ContextHelper helper(context);
        std::string requestId = helper.getString("request_id", "unknown");
        
        std::cout << "[AsyncValidationMiddleware] Starting async validation for request: " 
                 << requestId << std::endl;
        
        // Simulate async validation (e.g., database lookup, external API call)
        int delay = delay_dist_(rng_);
        
        // Simulate async operation with a thread
        std::thread([this, request, &context, next, callback, requestId, delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            
            // Simulate validation result (90% success rate)
            bool isValid = (rng_() % 10) < 9;
            
            if (isValid) {
                std::cout << "[AsyncValidationMiddleware] Validation successful for request: " 
                         << requestId << " (took " << delay << "ms)" << std::endl;
                
                // Add validation info to context
                ContextHelper helper(context);
                helper.setString("validation_status", "passed");
                helper.setInt("validation_time_ms", delay);
                
                // Continue to next middleware
                next(request, context, callback);
            } else {
                std::cout << "[AsyncValidationMiddleware] Validation failed for request: " 
                         << requestId << std::endl;
                
                // Return error response
                HttpResponse errorResponse(400, R"({
                    "error": "Validation failed",
                    "message": "Request validation did not pass async checks",
                    "request_id": ")" + requestId + R"("
                })");
                errorResponse.setHeader("Content-Type", "application/json");
                callback(errorResponse);
            }
        }).detach();
    }

    std::string getName() const override { return "AsyncValidationMiddleware"; }
    int getPriority() const override { return 75; }
};

/**
 * @brief Example 5: Factory-Enabled Custom Middleware Creator
 * 
 * This demonstrates how to create a middleware creator for the factory system.
 */
class CustomMiddlewareCreator : public MiddlewareCreator {
public:
    std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override {
        std::string type = config.getString("type", "request_id");
        
        if (type == "request_id") {
            std::string prefix = config.getString("prefix", "req");
            return std::make_shared<RequestIdMiddleware>(prefix);
            
        } else if (type == "performance") {
            int threshold = config.getInt("warning_threshold_ms", 1000);
            bool logSlow = config.getBool("log_slow_requests", true);
            return std::make_shared<PerformanceMiddleware>(
                std::chrono::milliseconds(threshold), logSlow);
                
        } else if (type == "custom_headers") {
            bool includeServerInfo = config.getBool("include_server_info", true);
            std::string version = config.getString("server_version", "1.2.0");
            
            // Parse custom headers from config
            std::map<std::string, std::string> headers;
            
            auto middleware = std::make_shared<CustomHeaderMiddleware>(headers, includeServerInfo, version);
            return middleware;
        }
        
        return nullptr;
    }

    std::string getMiddlewareName() const override {
        return "custom";
    }

    bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override {
        std::string type = config.getString("type");
        
        if (type.empty()) {
            errorMessage = "Missing required 'type' parameter";
            return false;
        }
        
        if (type != "request_id" && type != "performance" && type != "custom_headers") {
            errorMessage = "Invalid type '" + type + "'. Supported types: request_id, performance, custom_headers";
            return false;
        }
        
        if (type == "performance") {
            int threshold = config.getInt("warning_threshold_ms", -1);
            if (threshold < 0) {
                errorMessage = "warning_threshold_ms must be a positive integer";
                return false;
            }
        }
        
        return true;
    }
};

/**
 * @brief Register custom middleware with factory
 */
void registerCustomMiddleware() {
    MiddlewareFactory& factory = MiddlewareFactory::getInstance();
    
    auto creator = std::make_unique<CustomMiddlewareCreator>();
    bool registered = factory.registerCreator(std::move(creator));
    
    if (registered) {
        std::cout << "Custom middleware creator registered successfully!" << std::endl;
    } else {
        std::cout << "Failed to register custom middleware creator!" << std::endl;
    }
}

/**
 * @brief Setup middleware pipeline with custom middleware
 */
void setupCustomMiddlewarePipeline(std::shared_ptr<HttpServer> server) {
    std::cout << "Setting up custom middleware pipeline..." << std::endl;
    
    // Register custom middleware types with factory
    registerCustomMiddleware();
    
    // Method 1: Direct registration
    auto requestIdMiddleware = std::make_shared<RequestIdMiddleware>("demo");
    server->registerMiddleware(requestIdMiddleware);
    
    auto performanceMiddleware = std::make_shared<PerformanceMiddleware>(
        std::chrono::milliseconds(500), true);
    server->registerMiddleware(performanceMiddleware);
    
    std::map<std::string, std::string> customHeaders = {
        {"X-Custom-App", "MiddlewareExample"},
        {"X-Environment", "Development"}
    };
    auto headerMiddleware = std::make_shared<CustomHeaderMiddleware>(customHeaders);
    server->registerMiddleware(headerMiddleware);
    
    // Method 2: Factory-based creation
    MiddlewareFactory& factory = MiddlewareFactory::getInstance();
    
    MiddlewareInstanceConfig perfConfig;
    perfConfig.name = "custom";
    perfConfig.enabled = true;
    perfConfig.priority = 60;
    perfConfig.config["type"] = std::string("performance");
    perfConfig.config["warning_threshold_ms"] = 800;
    perfConfig.config["log_slow_requests"] = true;
    
    auto factoryMiddleware = factory.createMiddleware(perfConfig);
    if (factoryMiddleware) {
        server->registerMiddleware(factoryMiddleware);
        std::cout << "Factory-created middleware registered!" << std::endl;
    }
    
    std::cout << "Custom middleware pipeline configured!" << std::endl;
}

/**
 * @brief Register demo routes
 */
void registerDemoRoutes(std::shared_ptr<HttpServer> server) {
    std::cout << "Registering demo routes..." << std::endl;
    
    // Fast endpoint
    server->get("/api/fast", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "message": "This is a fast endpoint",
            "processing": "immediate"
        })");
    });
    
    // Slow endpoint (for performance testing)
    server->get("/api/slow", [](const HttpRequest& req) {
        // Simulate slow processing
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        return HttpResponse::json(R"({
            "message": "This is a slow endpoint",
            "processing": "1200ms delay"
        })");
    });
    
    // Context inspection endpoint
    server->get("/api/context", [](const HttpRequest& req) {
        // Note: In real middleware pipeline, context would be available
        return HttpResponse::json(R"({
            "message": "Check response headers for context information",
            "note": "Request ID and timing info are in headers"
        })");
    });
    
    // Error endpoint
    server->get("/api/error", [](const HttpRequest& req) {
        return HttpResponse(500, R"({
            "error": "Simulated server error",
            "message": "This endpoint always returns an error for testing"
        })");
    });
    
    // Root endpoint with documentation
    server->get("/", [](const HttpRequest& req) {
        return HttpResponse::html(R"(
<!DOCTYPE html>
<html>
<head>
    <title>Custom Middleware Example</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .endpoint { background: #f5f5f5; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #2196F3; }
        .fast { border-left-color: #4CAF50; }
        .slow { border-left-color: #FF9800; }
        .error { border-left-color: #F44336; }
        button { background: #2196F3; color: white; padding: 10px 15px; border: none; border-radius: 3px; cursor: pointer; margin: 5px; }
        button:hover { background: #1976D2; }
        .response { background: #f0f0f0; padding: 10px; margin: 10px 0; border-radius: 3px; font-family: monospace; white-space: pre-wrap; }
    </style>
</head>
<body>
    <h1>cppSwitchboard Custom Middleware Example</h1>
    <p>This example demonstrates custom middleware implementation including:</p>
    <ul>
        <li><strong>Request ID Middleware</strong> - Adds unique request tracking</li>
        <li><strong>Performance Middleware</strong> - Measures processing time</li>
        <li><strong>Custom Header Middleware</strong> - Adds security and custom headers</li>
        <li><strong>Async Validation Middleware</strong> - Non-blocking validation</li>
        <li><strong>Factory Integration</strong> - Configuration-driven middleware creation</li>
    </ul>
    
    <h2>Test Endpoints:</h2>
    
    <div class="endpoint fast">
        <strong>GET /api/fast</strong> - Fast endpoint (&lt;100ms)
        <button onclick="testEndpoint('/api/fast')">Test</button>
    </div>
    
    <div class="endpoint slow">
        <strong>GET /api/slow</strong> - Slow endpoint (1200ms) - triggers performance warning
        <button onclick="testEndpoint('/api/slow')">Test</button>
    </div>
    
    <div class="endpoint">
        <strong>GET /api/context</strong> - Context inspection endpoint
        <button onclick="testEndpoint('/api/context')">Test</button>
    </div>
    
    <div class="endpoint error">
        <strong>GET /api/error</strong> - Error endpoint (500 response)
        <button onclick="testEndpoint('/api/error')">Test</button>
    </div>
    
    <div id="response" class="response"></div>
    
    <h2>Middleware Features Demonstrated:</h2>
    <ul>
        <li>Check <strong>X-Request-ID</strong> header for request tracking</li>
        <li>Check <strong>X-Processing-Time</strong> header for timing info</li>
        <li>Check security headers (X-Frame-Options, X-XSS-Protection, etc.)</li>
        <li>Check custom headers (X-Powered-By, X-Custom-App, etc.)</li>
        <li>Monitor server console for middleware logging</li>
    </ul>
    
    <script>
        function testEndpoint(url) {
            const startTime = Date.now();
            
            fetch(url)
            .then(response => {
                const endTime = Date.now();
                const clientTime = endTime - startTime;
                
                // Show response details
                let responseText = `URL: ${url}\n`;
                responseText += `Status: ${response.status} ${response.statusText}\n`;
                responseText += `Client Time: ${clientTime}ms\n\n`;
                responseText += `Headers:\n`;
                
                for (let [key, value] of response.headers.entries()) {
                    responseText += `  ${key}: ${value}\n`;
                }
                
                return response.text().then(body => {
                    responseText += `\nBody:\n${body}`;
                    document.getElementById('response').textContent = responseText;
                });
            })
            .catch(error => {
                document.getElementById('response').textContent = `Error: ${error}`;
            });
        }
    </script>
</body>
</html>
        )");
    });
    
    std::cout << "Demo routes registered!" << std::endl;
}

/**
 * @brief Print usage instructions
 */
void printUsageInstructions() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "cppSwitchboard Custom Middleware Example" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "\nThis example demonstrates:\n" << std::endl;
    
    std::cout << "1. Custom synchronous middleware implementation" << std::endl;
    std::cout << "2. Custom asynchronous middleware with non-blocking operations" << std::endl;
    std::cout << "3. Middleware factory integration for configuration-driven creation" << std::endl;
    std::cout << "4. Context usage for inter-middleware communication" << std::endl;
    std::cout << "5. Performance monitoring and request tracking" << std::endl;
    
    std::cout << "\nCustom middleware implemented:" << std::endl;
    std::cout << "  • RequestIdMiddleware - Unique request tracking" << std::endl;
    std::cout << "  • PerformanceMiddleware - Processing time measurement" << std::endl;
    std::cout << "  • CustomHeaderMiddleware - Security and custom headers" << std::endl;
    std::cout << "  • AsyncValidationMiddleware - Non-blocking validation" << std::endl;
    
    std::cout << "\nOpen http://localhost:8080/ to interact with the examples" << std::endl;
    std::cout << "Press Ctrl+C to stop the server." << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;
}

int main() {
    try {
        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        std::cout << "Starting cppSwitchboard Custom Middleware Example..." << std::endl;
        
        // Create server with default configuration
        auto config = ServerConfig{};
        config.http1.enabled = true;
        config.http1.port = 8080;
        config.http1.bindAddress = "0.0.0.0";
        
        g_server = HttpServer::create(config);
        
        // Setup custom middleware
        setupCustomMiddlewarePipeline(g_server);
        
        // Register demo routes
        registerDemoRoutes(g_server);
        
        // Print usage instructions
        printUsageInstructions();
        
        // Start the server
        std::cout << "Starting server on http://localhost:8080..." << std::endl;
        g_server->start();
        
        // Keep the main thread alive
        while (g_server->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Server stopped successfully." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 