# cppSwitchboard Middleware Tutorial

**Complete Step-by-Step Guide to Middleware Development**

Welcome to the comprehensive middleware tutorial for cppSwitchboard! This guide will take you from beginner to advanced middleware development, covering all aspects of the middleware system.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Your First Middleware](#your-first-middleware)
3. [Understanding the Middleware Pipeline](#understanding-the-middleware-pipeline)
4. [Built-in Middleware](#built-in-middleware)
5. [Configuration-Driven Middleware](#configuration-driven-middleware)
6. [Custom Middleware Development](#custom-middleware-development)
7. [Asynchronous Middleware](#asynchronous-middleware)
8. [Middleware Factory System](#middleware-factory-system)
9. [Common Use Cases](#common-use-cases)
10. [Best Practices](#best-practices)
11. [Troubleshooting](#troubleshooting)
12. [Migration Guide](#migration-guide)

---

## Getting Started

### Prerequisites

Before diving into middleware development, ensure you have:

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15 or higher
- cppSwitchboard library installed
- Basic understanding of HTTP concepts
- Familiarity with C++ concepts (classes, inheritance, smart pointers)

### What is Middleware?

Middleware in cppSwitchboard are reusable components that execute during the request-response cycle. They can:

- **Inspect** and modify incoming requests
- **Transform** responses before sending to clients
- **Implement** cross-cutting concerns (authentication, logging, CORS)
- **Short-circuit** the pipeline by returning early responses
- **Share data** between components using context

### Middleware Execution Flow

```
HTTP Request → [Middleware 1] → [Middleware 2] → [Handler] → [Middleware 2] → [Middleware 1] → HTTP Response
                    ↓               ↓               ↓               ↑               ↑
                Pre-process     Pre-process      Process      Post-process    Post-process
```

---

## Your First Middleware

Let's create a simple "Hello World" middleware to understand the basics.

### Step 1: Create the Middleware Class

```cpp
#include <cppSwitchboard/middleware.h>
#include <iostream>

class HelloWorldMiddleware : public cppSwitchboard::Middleware {
public:
    cppSwitchboard::HttpResponse handle(
        const cppSwitchboard::HttpRequest& request, 
        Context& context, 
        NextHandler next) override {
        
        std::cout << "HelloWorldMiddleware: Processing request to " 
                  << request.getPath() << std::endl;
        
        // Call the next middleware/handler
        auto response = next(request, context);
        
        // Add a custom header to the response
        response.setHeader("X-Hello", "World");
        
        std::cout << "HelloWorldMiddleware: Response status " 
                  << response.getStatus() << std::endl;
        
        return response;
    }
    
    std::string getName() const override {
        return "HelloWorldMiddleware";
    }
    
    int getPriority() const override {
        return 100; // Higher values execute first
    }
};
```

### Step 2: Register the Middleware

```cpp
#include <cppSwitchboard/http_server.h>

int main() {
    auto server = cppSwitchboard::HttpServer::create();
    
    // Register your middleware
    auto helloMiddleware = std::make_shared<HelloWorldMiddleware>();
    server->registerMiddleware(helloMiddleware);
    
    // Register a simple route
    server->get("/", [](const auto& req) {
        return cppSwitchboard::HttpResponse::ok("Hello from cppSwitchboard!");
    });
    
    server->start();
    return 0;
}
```

### Step 3: Build and Test

```bash
# Compile your application
g++ -std=c++17 hello_middleware.cpp -lcppSwitchboard -o hello_server

# Run the server
./hello_server

# Test with curl
curl -v http://localhost:8080/
# Look for the "X-Hello: World" header in the response
```

**What happened?**
1. Your middleware intercepted the request before it reached the handler
2. It logged the request path
3. It called `next()` to continue the pipeline
4. It added a custom header to the response
5. It logged the response status

---

## Understanding the Middleware Pipeline

### Execution Order

Middleware executes in **priority order** (highest priority first):

```cpp
// Priority 200 - executes first
class CorsMiddleware : public Middleware {
    int getPriority() const override { return 200; }
};

// Priority 100 - executes second  
class AuthMiddleware : public Middleware {
    int getPriority() const override { return 100; }
};

// Priority 50 - executes third
class LoggingMiddleware : public Middleware {
    int getPriority() const override { return 50; }
};
```

### Context Sharing

Middleware can share data through the `Context`:

```cpp
class RequestIdMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        // Generate unique request ID
        std::string requestId = "req_" + std::to_string(++counter_);
        
        // Add to context for other middleware to use
        ContextHelper helper(context);
        helper.setString("request_id", requestId);
        
        return next(request, context);
    }
private:
    std::atomic<int> counter_{0};
};

class LoggingMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        // Get request ID from context
        ContextHelper helper(context);
        std::string requestId = helper.getString("request_id", "unknown");
        
        std::cout << "[" << requestId << "] " << request.getMethod() 
                  << " " << request.getPath() << std::endl;
        
        return next(request, context);
    }
};
```

### Early Termination

Middleware can stop the pipeline by not calling `next()`:

```cpp
class MaintenanceMiddleware : public Middleware {
private:
    bool maintenanceMode_;
    
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        if (maintenanceMode_) {
            return HttpResponse(503, R"({
                "error": "Service Unavailable",
                "message": "Server is under maintenance"
            })");
        }
        
        return next(request, context);
    }
};
```

---

## Built-in Middleware

cppSwitchboard provides several production-ready middleware components:

### 1. CORS Middleware

```cpp
#include <cppSwitchboard/middleware/cors_middleware.h>

// Create CORS configuration
CorsMiddleware::CorsConfig corsConfig;
corsConfig.allowedOrigins = {"https://example.com", "https://app.example.com"};
corsConfig.allowedMethods = {"GET", "POST", "PUT", "DELETE"};
corsConfig.allowedHeaders = {"Content-Type", "Authorization"};
corsConfig.allowCredentials = true;

// Register middleware
auto corsMiddleware = std::make_shared<CorsMiddleware>(corsConfig);
server->registerMiddleware(corsMiddleware);
```

### 2. Authentication Middleware

```cpp
#include <cppSwitchboard/middleware/auth_middleware.h>

// JWT-based authentication
std::string jwtSecret = "your-secret-key";
auto authMiddleware = std::make_shared<AuthMiddleware>(jwtSecret);
authMiddleware->setIssuer("your-app.com");
authMiddleware->setAudience("api.your-app.com");

server->registerMiddleware(authMiddleware);
```

### 3. Rate Limiting Middleware

```cpp
#include <cppSwitchboard/middleware/rate_limit_middleware.h>

RateLimitMiddleware::RateLimitConfig rateLimitConfig;
rateLimitConfig.strategy = RateLimitMiddleware::Strategy::IP_BASED;
rateLimitConfig.bucketConfig.maxTokens = 100;
rateLimitConfig.bucketConfig.refillRate = 10;

auto rateLimitMiddleware = std::make_shared<RateLimitMiddleware>(rateLimitConfig);
server->registerMiddleware(rateLimitMiddleware);
```

### 4. Logging Middleware

```cpp
#include <cppSwitchboard/middleware/logging_middleware.h>

LoggingMiddleware::LoggingConfig loggingConfig;
loggingConfig.format = LoggingMiddleware::LogFormat::JSON;
loggingConfig.includeHeaders = true;
loggingConfig.includeTimings = true;

auto loggingMiddleware = std::make_shared<LoggingMiddleware>(loggingConfig);
server->registerMiddleware(loggingMiddleware);
```

---

## Configuration-Driven Middleware

For complex applications, configure middleware using YAML:

### Step 1: Create Configuration File (`middleware.yaml`)

```yaml
middleware:
  # Global middleware for all routes
  global:
    - name: "cors"
      enabled: true
      priority: 200
      config:
        origins: ["*"]
        methods: ["GET", "POST", "PUT", "DELETE"]
        credentials: false
        
    - name: "logging"
      enabled: true
      priority: 10
      config:
        format: "json"
        include_headers: true
        
  # Route-specific middleware
  routes:
    # API v1 routes with authentication
    "/api/v1/*":
      - name: "auth"
        enabled: true
        priority: 100
        config:
          type: "jwt"
          secret: "${JWT_SECRET}"
          
      - name: "rate_limit"
        enabled: true
        priority: 80
        config:
          strategy: "user_based"
          max_tokens: 1000
          refill_rate: 100
          
    # Admin routes with authorization
    "/api/admin/*":
      - name: "auth"
        enabled: true
        priority: 100
        config:
          type: "jwt"
          secret: "${JWT_SECRET}"
          
      - name: "authz"
        enabled: true
        priority: 90
        config:
          required_roles: ["admin"]
```

### Step 2: Load Configuration in Your Application

```cpp
#include <cppSwitchboard/middleware_config.h>
#include <cppSwitchboard/middleware_factory.h>

int main() {
    auto server = HttpServer::create();
    
    // Load middleware configuration
    MiddlewareConfigLoader loader;
    auto result = loader.loadFromFile("middleware.yaml");
    
    if (!result.isSuccess()) {
        std::cerr << "Failed to load config: " << result.message << std::endl;
        return 1;
    }
    
    const auto& config = loader.getConfiguration();
    MiddlewareFactory& factory = MiddlewareFactory::getInstance();
    
    // Apply global middleware
    for (const auto& middlewareConfig : config.global.middlewares) {
        if (middlewareConfig.enabled) {
            auto middleware = factory.createMiddleware(middlewareConfig);
            if (middleware) {
                server->registerMiddleware(middleware);
            }
        }
    }
    
    // Apply route-specific middleware
    for (const auto& routeConfig : config.routes) {
        auto pipeline = factory.createPipeline(routeConfig.middlewares);
        server->registerRouteWithMiddleware(routeConfig.pattern, HttpMethod::GET, pipeline);
    }
    
    server->start();
    return 0;
}
```

---

## Custom Middleware Development

### Planning Your Middleware

Before writing code, consider:

1. **Purpose**: What problem does your middleware solve?
2. **Placement**: Where in the pipeline should it execute?
3. **Dependencies**: Does it need data from other middleware?
4. **Configuration**: What options should be configurable?
5. **Performance**: Will it impact request latency?

### Example: API Key Authentication Middleware

```cpp
class ApiKeyMiddleware : public Middleware {
private:
    std::string headerName_;
    std::unordered_set<std::string> validKeys_;
    bool requireForAllRoutes_;
    
public:
    ApiKeyMiddleware(const std::string& headerName = "X-API-Key", 
                    bool requireForAll = true)
        : headerName_(headerName), requireForAllRoutes_(requireForAll) {}
    
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        // Skip authentication for certain paths
        if (!requireForAllRoutes_ && request.getPath().starts_with("/public/")) {
            return next(request, context);
        }
        
        // Extract API key from header
        std::string apiKey = request.getHeader(headerName_);
        
        if (apiKey.empty()) {
            return createErrorResponse("Missing API key", 401);
        }
        
        if (validKeys_.find(apiKey) == validKeys_.end()) {
            return createErrorResponse("Invalid API key", 403);
        }
        
        // Add API key info to context
        ContextHelper helper(context);
        helper.setString("api_key", apiKey);
        helper.setBool("authenticated", true);
        
        return next(request, context);
    }
    
    std::string getName() const override { return "ApiKeyMiddleware"; }
    int getPriority() const override { return 110; }
    
    void addValidKey(const std::string& key) {
        validKeys_.insert(key);
    }
    
    void removeValidKey(const std::string& key) {
        validKeys_.erase(key);
    }
    
private:
    HttpResponse createErrorResponse(const std::string& message, int status) {
        std::string json = R"({"error": ")" + message + R"("})";
        HttpResponse response(status, json);
        response.setHeader("Content-Type", "application/json");
        return response;
    }
};
```

### Using Your Custom Middleware

```cpp
int main() {
    auto server = HttpServer::create();
    
    // Create and configure API key middleware
    auto apiKeyMiddleware = std::make_shared<ApiKeyMiddleware>("X-API-Key", false);
    apiKeyMiddleware->addValidKey("dev-key-123");
    apiKeyMiddleware->addValidKey("prod-key-456");
    
    server->registerMiddleware(apiKeyMiddleware);
    
    // Register routes
    server->get("/public/status", [](const auto& req) {
        return HttpResponse::json(R"({"status": "ok"})");
    });
    
    server->get("/api/data", [](const auto& req) {
        return HttpResponse::json(R"({"data": "secret information"})");
    });
    
    server->start();
    return 0;
}
```

---

## Asynchronous Middleware

For non-blocking operations like database queries or external API calls:

### Basic Async Middleware

```cpp
#include <cppSwitchboard/async_middleware.h>

class AsyncAuthMiddleware : public AsyncMiddleware {
public:
    void handleAsync(const HttpRequest& request, Context& context,
                    NextAsyncHandler next, AsyncCallback callback) override {
        
        std::string token = request.getHeader("Authorization");
        
        if (token.empty()) {
            callback(HttpResponse(401, "Missing token"));
            return;
        }
        
        // Simulate async token validation (e.g., database lookup)
        validateTokenAsync(token, [this, &request, &context, next, callback]
                          (bool isValid, const std::string& userId) {
            if (!isValid) {
                callback(HttpResponse(401, "Invalid token"));
                return;
            }
            
            // Add user info to context
            ContextHelper helper(context);
            helper.setString("user_id", userId);
            
            // Continue to next middleware
            next(request, context, callback);
        });
    }
    
    std::string getName() const override { return "AsyncAuthMiddleware"; }
    int getPriority() const override { return 100; }
    
private:
    void validateTokenAsync(const std::string& token, 
                          std::function<void(bool, const std::string&)> callback) {
        // Simulate async operation with thread pool
        std::thread([token, callback]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simple validation logic
            bool isValid = token.length() > 10;
            std::string userId = isValid ? "user_123" : "";
            
            callback(isValid, userId);
        }).detach();
    }
};
```

### Async Pipeline Usage

```cpp
#include <cppSwitchboard/async_middleware.h>

int main() {
    auto server = HttpServer::create();
    
    // Create async middleware pipeline
    AsyncMiddlewarePipeline asyncPipeline;
    asyncPipeline.addMiddleware(std::make_shared<AsyncAuthMiddleware>());
    
    // Set final async handler
    auto asyncHandler = std::make_shared<AsyncFunctionHandler>(
        [](const HttpRequest& request, AsyncHttpHandler::ResponseCallback callback) {
            // Simulate async processing
            std::thread([callback]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                callback(HttpResponse::json(R"({"message": "Async response"})"));
            }).detach();
        });
    
    asyncPipeline.setFinalHandler(asyncHandler);
    
    // Register async route
    server->registerAsyncRoute("/api/async", HttpMethod::GET, asyncHandler);
    
    server->start();
    return 0;
}
```

---

## Middleware Factory System

For enterprise applications, use the factory system for configuration-driven middleware:

### Step 1: Create Middleware Creator

```cpp
#include <cppSwitchboard/middleware_factory.h>

class CustomMiddlewareCreator : public MiddlewareCreator {
public:
    std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override {
        std::string type = config.getString("type");
        
        if (type == "api_key") {
            std::string headerName = config.getString("header_name", "X-API-Key");
            bool requireForAll = config.getBool("require_for_all", true);
            
            auto middleware = std::make_shared<ApiKeyMiddleware>(headerName, requireForAll);
            
            // Add valid keys from config
            auto keys = config.getStringArray("valid_keys");
            for (const auto& key : keys) {
                middleware->addValidKey(key);
            }
            
            return middleware;
        }
        
        return nullptr;
    }
    
    std::string getMiddlewareName() const override {
        return "custom";
    }
    
    bool validateConfig(const MiddlewareInstanceConfig& config, 
                       std::string& errorMessage) const override {
        std::string type = config.getString("type");
        
        if (type.empty()) {
            errorMessage = "Missing 'type' parameter";
            return false;
        }
        
        if (type == "api_key") {
            auto keys = config.getStringArray("valid_keys");
            if (keys.empty()) {
                errorMessage = "api_key middleware requires 'valid_keys' array";
                return false;
            }
        }
        
        return true;
    }
};
```

### Step 2: Register with Factory

```cpp
int main() {
    // Register custom middleware creator
    MiddlewareFactory& factory = MiddlewareFactory::getInstance();
    factory.registerCreator(std::make_unique<CustomMiddlewareCreator>());
    
    // Create middleware from configuration
    MiddlewareInstanceConfig config;
    config.name = "custom";
    config.enabled = true;
    config.priority = 110;
    config.config["type"] = std::string("api_key");
    config.config["header_name"] = std::string("X-API-Key");
    config.config["require_for_all"] = false;
    
    // Set valid keys
    std::vector<std::string> validKeys = {"key1", "key2", "key3"};
    config.config["valid_keys"] = validKeys;
    
    auto middleware = factory.createMiddleware(config);
    if (middleware) {
        server->registerMiddleware(middleware);
    }
    
    server->start();
    return 0;
}
```

---

## Common Use Cases

### 1. Request/Response Transformation

```cpp
class JsonMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        // Parse JSON request body
        if (request.getHeader("Content-Type") == "application/json") {
            // Parse and validate JSON
            // Add parsed data to context
        }
        
        auto response = next(request, context);
        
        // Ensure JSON responses have correct content type
        if (response.getHeader("Content-Type").empty() && 
            response.getBody().starts_with("{")) {
            response.setHeader("Content-Type", "application/json");
        }
        
        return response;
    }
};
```

### 2. Error Handling

```cpp
class ErrorHandlingMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        try {
            return next(request, context);
        } catch (const std::exception& e) {
            // Log error
            std::cerr << "Error processing request: " << e.what() << std::endl;
            
            // Return user-friendly error response
            std::string error = R"({"error": "Internal server error"})";
            HttpResponse response(500, error);
            response.setHeader("Content-Type", "application/json");
            return response;
        }
    }
    
    int getPriority() const override { return 1; } // Very low priority
};
```

### 3. Caching

```cpp
class CacheMiddleware : public Middleware {
private:
    std::unordered_map<std::string, std::pair<HttpResponse, std::chrono::steady_clock::time_point>> cache_;
    std::chrono::seconds cacheDuration_;
    std::mutex cacheMutex_;
    
public:
    explicit CacheMiddleware(std::chrono::seconds duration = std::chrono::seconds(300))
        : cacheDuration_(duration) {}
    
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        // Only cache GET requests
        if (request.getMethod() != "GET") {
            return next(request, context);
        }
        
        std::string cacheKey = request.getPath() + "?" + request.getQuery();
        
        // Check cache
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            auto it = cache_.find(cacheKey);
            if (it != cache_.end()) {
                auto now = std::chrono::steady_clock::now();
                if (now - it->second.second < cacheDuration_) {
                    // Cache hit
                    auto response = it->second.first;
                    response.setHeader("X-Cache", "HIT");
                    return response;
                }
            }
        }
        
        // Cache miss - process request
        auto response = next(request, context);
        
        // Cache successful responses
        if (response.isSuccess()) {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            cache_[cacheKey] = {response, std::chrono::steady_clock::now()};
            response.setHeader("X-Cache", "MISS");
        }
        
        return response;
    }
};
```

---

## Best Practices

### 1. Middleware Design Principles

- **Single Responsibility**: Each middleware should have one clear purpose
- **Stateless**: Avoid storing request-specific state in middleware instances
- **Thread-Safe**: Use appropriate synchronization for shared data
- **Performance-Conscious**: Minimize processing overhead
- **Fail-Safe**: Handle errors gracefully without crashing

### 2. Priority Guidelines

```cpp
// Recommended priority ranges:
class CorsMiddleware      { int getPriority() { return 200; } }; // Handle preflight first
class AuthMiddleware      { int getPriority() { return 100; } }; // Authentication
class AuthzMiddleware     { int getPriority() { return 90; } };  // Authorization  
class RateLimitMiddleware { int getPriority() { return 80; } };  // Rate limiting
class CacheMiddleware     { int getPriority() { return 70; } };  // Caching
class ValidationMiddleware{ int getPriority() { return 60; } };  // Input validation
class CustomMiddleware    { int getPriority() { return 50; } };  // Custom logic
class LoggingMiddleware   { int getPriority() { return 10; } };  // Logging (low priority)
```

### 3. Context Usage Best Practices

```cpp
// Good: Use ContextHelper for type safety
ContextHelper helper(context);
helper.setString("user_id", userId);
helper.setInt("request_count", count);

// Good: Check for existing values
if (helper.hasKey("user_id")) {
    std::string userId = helper.getString("user_id");
}

// Good: Use defaults for optional values  
std::string theme = helper.getString("theme", "default");

// Avoid: Direct context manipulation
// context["key"] = value; // Type unsafe
```

### 4. Error Handling

```cpp
class SafeMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        try {
            // Your middleware logic here
            return next(request, context);
        } catch (const std::exception& e) {
            // Log error with context
            ContextHelper helper(context);
            std::string requestId = helper.getString("request_id", "unknown");
            
            std::cerr << "[" << getName() << "] Error in request " << requestId 
                     << ": " << e.what() << std::endl;
            
            // Don't break the pipeline - return error response
            return HttpResponse(500, "Internal server error");
        }
    }
};
```

### 5. Testing Your Middleware

```cpp
#include <gtest/gtest.h>

class TestMiddleware : public ::testing::Test {
protected:
    void SetUp() override {
        middleware = std::make_shared<YourMiddleware>();
    }
    
    std::shared_ptr<YourMiddleware> middleware;
};

TEST_F(TestMiddleware, HandlesValidRequest) {
    HttpRequest request("GET", "/test");
    Context context;
    
    bool nextCalled = false;
    auto next = [&](const HttpRequest&, Context&) {
        nextCalled = true;
        return HttpResponse::ok("Success");
    };
    
    auto response = middleware->handle(request, context, next);
    
    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(200, response.getStatus());
}

TEST_F(TestMiddleware, RejectsInvalidRequest) {
    HttpRequest request("GET", "/invalid");
    Context context;
    
    bool nextCalled = false;
    auto next = [&](const HttpRequest&, Context&) {
        nextCalled = true;
        return HttpResponse::ok("Success");
    };
    
    auto response = middleware->handle(request, context, next);
    
    EXPECT_FALSE(nextCalled); // Should not call next
    EXPECT_EQ(400, response.getStatus());
}
```

---

## Troubleshooting

### Common Issues and Solutions

#### 1. Middleware Not Executing

**Problem**: Your middleware isn't being called.

**Solutions**:
- Check if middleware is registered: `server->registerMiddleware(middleware)`
- Verify priority values - higher priorities execute first
- Ensure middleware is enabled: `isEnabled()` returns true
- Check route patterns for route-specific middleware

```cpp
// Debug middleware registration
auto registeredMiddleware = server->getRegisteredMiddleware();
for (const auto& mw : registeredMiddleware) {
    std::cout << "Registered: " << mw->getName() 
              << " (priority: " << mw->getPriority() << ")" << std::endl;
}
```

#### 2. Context Data Not Available

**Problem**: Data set in one middleware isn't available in another.

**Solutions**:
- Ensure middleware that sets data has higher priority
- Use correct context key names
- Check if middleware calls `next()` properly

```cpp
// Debug context contents
class DebugMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        ContextHelper helper(context);
        
        // Log all context keys (requires custom implementation)
        std::cout << "Context keys available: ";
        // for (const auto& [key, value] : context) { ... }
        
        return next(request, context);
    }
};
```

#### 3. Performance Issues

**Problem**: Middleware is slowing down requests.

**Solutions**:
- Profile middleware execution time
- Optimize expensive operations
- Consider async middleware for I/O operations
- Use caching for repeated computations

```cpp
// Performance monitoring middleware
class PerfMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        auto start = std::chrono::steady_clock::now();
        
        auto response = next(request, context);
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (duration.count() > 100) { // Log slow requests
            std::cout << "Slow request: " << request.getPath() 
                     << " took " << duration.count() << "ms" << std::endl;
        }
        
        return response;
    }
};
```

#### 4. Memory Leaks

**Problem**: Memory usage increases over time.

**Solutions**:
- Use smart pointers correctly
- Avoid circular references in context
- Clean up resources in middleware destructors
- Don't store request/response objects in middleware instances

#### 5. Configuration Loading Errors

**Problem**: YAML configuration not loading properly.

**Solutions**:
- Validate YAML syntax
- Check environment variable substitution
- Verify middleware names match factory registrations

```cpp
// Debug configuration loading
MiddlewareConfigLoader loader;
auto result = loader.loadFromFile("config.yaml");

if (!result.isSuccess()) {
    std::cerr << "Config error: " << result.message << std::endl;
    std::cerr << "Context: " << result.context << std::endl;
}
```

### Debugging Tools

#### 1. Middleware Inspection

```cpp
class InspectionMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        std::cout << "\n=== Request ===" << std::endl;
        std::cout << "Method: " << request.getMethod() << std::endl;
        std::cout << "Path: " << request.getPath() << std::endl;
        std::cout << "Headers:" << std::endl;
        
        // Log all headers
        for (const auto& [name, value] : request.getHeaders()) {
            std::cout << "  " << name << ": " << value << std::endl;
        }
        
        auto response = next(request, context);
        
        std::cout << "\n=== Response ===" << std::endl;
        std::cout << "Status: " << response.getStatus() << std::endl;
        std::cout << "Headers:" << std::endl;
        
        for (const auto& [name, value] : response.getHeaders()) {
            std::cout << "  " << name << ": " << value << std::endl;
        }
        
        return response;
    }
    
    int getPriority() const override { return 1; } // Very low priority
};
```

#### 2. Pipeline Visualization

```cpp
void visualizePipeline(const std::vector<std::shared_ptr<Middleware>>& middlewares) {
    std::cout << "Middleware Pipeline:" << std::endl;
    
    // Sort by priority (highest first)
    auto sorted = middlewares;
    std::sort(sorted.begin(), sorted.end(), 
             [](const auto& a, const auto& b) {
                 return a->getPriority() > b->getPriority();
             });
    
    for (size_t i = 0; i < sorted.size(); ++i) {
        std::cout << (i + 1) << ". " << sorted[i]->getName() 
                  << " (priority: " << sorted[i]->getPriority() << ")" << std::endl;
    }
}
```

---

## Migration Guide

### From Traditional Handlers to Middleware

#### Before (Traditional Handler)

```cpp
server->get("/api/data", [](const HttpRequest& req) {
    // CORS handling
    if (req.getMethod() == "OPTIONS") {
        HttpResponse response(200);
        response.setHeader("Access-Control-Allow-Origin", "*");
        return response;
    }
    
    // Authentication
    std::string token = req.getHeader("Authorization");
    if (token.empty()) {
        return HttpResponse(401, "Unauthorized");
    }
    
    // Rate limiting
    if (!checkRateLimit(req.getClientIP())) {
        return HttpResponse(429, "Too Many Requests");
    }
    
    // Logging
    std::cout << "Request: " << req.getPath() << std::endl;
    
    // Business logic
    return HttpResponse::json(R"({"data": "value"})");
});
```

#### After (Middleware-Based)

```cpp
// Configure middleware once
auto corsMiddleware = std::make_shared<CorsMiddleware>(corsConfig);
auto authMiddleware = std::make_shared<AuthMiddleware>(jwtSecret);
auto rateLimitMiddleware = std::make_shared<RateLimitMiddleware>(rateLimitConfig);
auto loggingMiddleware = std::make_shared<LoggingMiddleware>(loggingConfig);

server->registerMiddleware(corsMiddleware);
server->registerMiddleware(authMiddleware);
server->registerMiddleware(rateLimitMiddleware);
server->registerMiddleware(loggingMiddleware);

// Clean handler with just business logic
server->get("/api/data", [](const HttpRequest& req) {
    return HttpResponse::json(R"({"data": "value"})");
});
```

### Migration Steps

1. **Identify Cross-Cutting Concerns**
   - Authentication/authorization
   - CORS handling
   - Rate limiting
   - Logging
   - Input validation
   - Error handling

2. **Replace with Built-in Middleware**
   - Use cppSwitchboard's built-in middleware where possible
   - Configure through YAML for flexibility

3. **Extract Custom Logic**
   - Create custom middleware for application-specific concerns
   - Move reusable logic out of handlers

4. **Simplify Handlers**
   - Focus handlers on business logic only
   - Remove infrastructure code

5. **Test Thoroughly**
   - Verify behavior is unchanged
   - Test middleware interactions
   - Check performance impact

### Backward Compatibility

The middleware system is fully backward compatible:

```cpp
// Old code continues to work
server->get("/legacy", [](const HttpRequest& req) {
    // Traditional handler logic
    return HttpResponse::ok("Legacy endpoint");
});

// New middleware-enabled routes
server->registerRouteWithMiddleware("/modern", HttpMethod::GET, pipeline);
```

---

## Next Steps

Congratulations! You've completed the cppSwitchboard middleware tutorial. Here's what to explore next:

### Advanced Topics
- **Async Middleware Patterns**: Complex async workflows
- **Custom Factory Creators**: Enterprise middleware management
- **Performance Optimization**: High-throughput scenarios
- **Microservices Integration**: Service mesh patterns

### Further Reading
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Middleware Development Guide](MIDDLEWARE.md) - Detailed middleware documentation
- [Configuration Reference](CONFIGURATION.md) - Complete configuration options
- [Performance Guide](PERFORMANCE.md) - Optimization techniques

### Community Resources
- GitHub Issues: Report bugs and request features
- Examples Repository: Real-world usage examples
- Community Forums: Get help and share experiences

Happy coding with cppSwitchboard middleware! 🚀 