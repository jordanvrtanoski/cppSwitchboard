# cppSwitchboard API Reference

**Version:** 1.0.0  
**Modern C++ HTTP/1.1 and HTTP/2 Server Library**

---

## Table of Contents

1. [Overview](#overview)
2. [Core Classes](#core-classes)
3. [HTTP Server](#http-server)
4. [HTTP Request and Response](#http-request-and-response)
5. [Routing System](#routing-system)
6. [Configuration Management](#configuration-management)
7. [Debugging and Logging](#debugging-and-logging)
8. [HTTP/2 Support](#http2-support)
9. [Usage Examples](#usage-examples)
10. [Error Handling](#error-handling)

---

## Overview

The cppSwitchboard library provides a modern, high-performance HTTP server implementation in C++ supporting both HTTP/1.1 and HTTP/2 protocols. It features a flexible routing system, comprehensive configuration management, and built-in debugging capabilities.

### Key Features

- **Dual Protocol Support**: Both HTTP/1.1 and HTTP/2
- **Modern C++**: Built with C++17 standards
- **High Performance**: Asynchronous request handling
- **Flexible Routing**: Pattern-based URL routing with parameter extraction
- **Configuration Driven**: YAML-based configuration with validation
- **Comprehensive Testing**: 100% test coverage
- **Debug Support**: Built-in logging and debugging utilities

---

## Core Classes

### HttpServer

The main server class that handles incoming HTTP connections and routes requests.

**Constructor:**
```cpp
HttpServer(const ServerConfig& config)
```

**Key Methods:**
- `void start()` - Starts the HTTP server
- `void stop()` - Gracefully stops the server
- `void registerRoute(const std::string& pattern, HttpMethod method, HttpHandler handler)` - Registers a route handler
- `bool isRunning() const` - Checks if server is currently running

**Example Usage:**
```cpp
#include <cppSwitchboard/http_server.h>

ServerConfig config;
config.http1.port = 8080;
config.http1.bindAddress = "0.0.0.0";

HttpServer server(config);

server.registerRoute("/api/users", HttpMethod::GET, [](const HttpRequest& req) {
    return HttpResponse::json("{\"users\": []}");
});

server.start();
```

---

## HTTP Request and Response

### HttpRequest Class

Represents an incoming HTTP request with all its components.

**Properties:**
- `std::string getMethod() const` - HTTP method (GET, POST, etc.)
- `std::string getPath() const` - Request path
- `std::string getQuery() const` - Query string
- `std::string getHeader(const std::string& name) const` - Get header value
- `std::string getBody() const` - Request body
- `std::string getQueryParam(const std::string& name) const` - Get query parameter

**Methods:**
- `void parseQueryString(const std::string& query)` - Parse query parameters
- `void addHeader(const std::string& name, const std::string& value)` - Add header
- `bool hasHeader(const std::string& name) const` - Check if header exists

### HttpResponse Class

Represents an HTTP response to be sent back to the client.

**Constructor:**
```cpp
HttpResponse(int status = 200, const std::string& body = "")
```

**Static Factory Methods:**
- `static HttpResponse ok(const std::string& body)` - 200 OK response
- `static HttpResponse json(const std::string& body)` - JSON response with correct headers
- `static HttpResponse html(const std::string& body)` - HTML response with correct headers
- `static HttpResponse notFound()` - 404 Not Found response
- `static HttpResponse internalError()` - 500 Internal Server Error response

**Methods:**
- `void setStatus(int status)` - Set HTTP status code
- `void setBody(const std::string& body)` - Set response body
- `void addHeader(const std::string& name, const std::string& value)` - Add header
- `int getStatus() const` - Get status code
- `std::string getBody() const` - Get response body
- `std::string getContentType() const` - Get content type header

**Example Usage:**
```cpp
// Simple text response
auto response = HttpResponse::ok("Hello, World!");

// JSON response
auto jsonResponse = HttpResponse::json("{\"message\": \"Success\"}");

// Custom response
HttpResponse custom(201);
custom.setBody("Created");
custom.addHeader("Location", "/api/users/123");
```

---

## Routing System

### RouteRegistry Class

Manages URL patterns and route matching for the HTTP server.

**Methods:**
- `void registerRoute(const std::string& pattern, HttpMethod method, HttpHandler handler)` - Register a route
- `RouteMatch findRoute(const std::string& path, HttpMethod method)` - Find matching route
- `RouteMatch findRoute(const HttpRequest& request)` - Find route from request
- `void clearRoutes()` - Remove all registered routes

### Route Patterns

The routing system supports flexible URL patterns:

**Static Routes:**
```cpp
server.registerRoute("/api/users", HttpMethod::GET, handler);
```

**Parameterized Routes:**
```cpp
server.registerRoute("/api/users/{id}", HttpMethod::GET, handler);
server.registerRoute("/api/users/{id}/posts/{postId}", HttpMethod::GET, handler);
```

**HTTP Methods:**
- `HttpMethod::GET`
- `HttpMethod::POST`
- `HttpMethod::PUT`
- `HttpMethod::DELETE`
- `HttpMethod::PATCH`
- `HttpMethod::HEAD`
- `HttpMethod::OPTIONS`

### Handler Functions

Route handlers can be defined as lambda functions or function pointers:

```cpp
// Lambda handler
server.registerRoute("/hello", HttpMethod::GET, [](const HttpRequest& req) {
    return HttpResponse::ok("Hello, " + req.getQueryParam("name"));
});

// Function handler
HttpResponse userHandler(const HttpRequest& request) {
    return HttpResponse::json("{\"user\": \"data\"}");
}
server.registerRoute("/user", HttpMethod::GET, userHandler);
```

---

## Configuration Management

### ServerConfig Structure

The main configuration structure that defines server behavior:

```cpp
struct ServerConfig {
    ApplicationConfig application;
    Http1Config http1;
    Http2Config http2;
    SslConfig ssl;
    DebugLoggingConfig debug;
    SecurityConfig security;
    MonitoringConfig monitoring;
};
```

### Configuration Loading

**ConfigLoader Class:**
- `static std::unique_ptr<ServerConfig> loadFromFile(const std::string& filename)` - Load from YAML file
- `static std::unique_ptr<ServerConfig> loadDefaults()` - Load default configuration
- `static bool validateConfig(const ServerConfig& config)` - Validate configuration

**Example Configuration (YAML):**
```yaml
application:
  name: "My HTTP Server"
  version: "1.0.0"
  environment: "development"

http1:
  enabled: true
  port: 8080
  bindAddress: "0.0.0.0"

http2:
  enabled: true
  port: 8443
  bindAddress: "0.0.0.0"

ssl:
  enabled: true
  certificateFile: "/path/to/cert.pem"
  privateKeyFile: "/path/to/key.pem"

debug:
  enabled: true
  logLevel: "info"
  logFile: "/var/log/server.log"
```

### Configuration Validation

**ConfigValidator Class:**
- `static bool validateConfig(const ServerConfig& config)` - Validate entire configuration
- `static bool validatePorts(const ServerConfig& config)` - Validate port configurations
- `static bool validateSsl(const ServerConfig& config)` - Validate SSL settings

---

## Debugging and Logging

### DebugLogger Class

Provides comprehensive logging capabilities for debugging and monitoring.

**Constructor:**
```cpp
DebugLogger(const DebugLoggingConfig& config)
```

**Methods:**
- `void info(const std::string& message)` - Log info message
- `void warn(const std::string& message)` - Log warning message
- `void error(const std::string& message)` - Log error message
- `void debug(const std::string& message)` - Log debug message
- `void setLogLevel(const std::string& level)` - Set logging level

**Usage Example:**
```cpp
DebugLoggingConfig logConfig;
logConfig.enabled = true;
logConfig.logLevel = "debug";
logConfig.logFile = "/var/log/server.log";

DebugLogger logger(logConfig);
logger.info("Server starting...");
logger.debug("Processing request: " + request.getPath());
```

---

## HTTP/2 Support

### Http2Server Class

Dedicated HTTP/2 server implementation with advanced features.

**Key Features:**
- Stream multiplexing
- Header compression (HPACK)
- Server push capabilities
- Flow control

**Configuration:**
```cpp
Http2Config config;
config.enabled = true;
config.port = 8443;
config.maxConcurrentStreams = 100;
config.initialWindowSize = 65535;
```

---

## Usage Examples

### Basic HTTP Server

```cpp
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/config.h>

int main() {
    // Load configuration
    auto config = ConfigLoader::loadDefaults();
    config->http1.port = 8080;
    
    // Create server
    HttpServer server(*config);
    
    // Register routes
    server.registerRoute("/", HttpMethod::GET, [](const HttpRequest& req) {
        return HttpResponse::html("<h1>Welcome to cppSwitchboard!</h1>");
    });
    
    server.registerRoute("/api/status", HttpMethod::GET, [](const HttpRequest& req) {
        return HttpResponse::json("{\"status\": \"ok\", \"uptime\": 12345}");
    });
    
    // Start server
    server.start();
    
    return 0;
}
```

### RESTful API Example

```cpp
// GET /api/users
server.registerRoute("/api/users", HttpMethod::GET, [](const HttpRequest& req) {
    // Return list of users
    return HttpResponse::json("[{\"id\": 1, \"name\": \"John\"}]");
});

// GET /api/users/{id}
server.registerRoute("/api/users/{id}", HttpMethod::GET, [](const HttpRequest& req) {
    std::string userId = req.getPathParam("id");
    return HttpResponse::json("{\"id\": " + userId + ", \"name\": \"John\"}");
});

// POST /api/users
server.registerRoute("/api/users", HttpMethod::POST, [](const HttpRequest& req) {
    std::string body = req.getBody();
    // Process user creation
    return HttpResponse(201, "{\"id\": 123, \"created\": true}");
});

// PUT /api/users/{id}
server.registerRoute("/api/users/{id}", HttpMethod::PUT, [](const HttpRequest& req) {
    std::string userId = req.getPathParam("id");
    std::string body = req.getBody();
    // Process user update
    return HttpResponse::ok("{\"updated\": true}");
});

// DELETE /api/users/{id}
server.registerRoute("/api/users/{id}", HttpMethod::DELETE, [](const HttpRequest& req) {
    std::string userId = req.getPathParam("id");
    // Process user deletion
    return HttpResponse(204); // No content
});
```

### Synchronous Middleware Example

```cpp
#include <cppSwitchboard/middleware.h>

// Create custom middleware
class AuthMiddleware : public Middleware {
public:
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        std::string token = request.getHeader("Authorization");
        if (token.empty() || token.substr(0, 7) != "Bearer ") {
            return HttpResponse(401, "{\"error\": \"Unauthorized\"}");
        }
        
        // Add user info to context
        ContextHelper helper(context);
        helper.setString("user_id", extractUserId(token));
        
        // Continue to next middleware/handler
        return next(request, context);
    }
    
    std::string getName() const override { return "AuthMiddleware"; }
    int getPriority() const override { return 100; }
};

// Register middleware
server.registerMiddleware(std::make_shared<AuthMiddleware>());
```

### Asynchronous Middleware Example ✅ NEW

```cpp
#include <cppSwitchboard/async_middleware.h>

// Create async middleware
class AsyncAuthMiddleware : public AsyncMiddleware {
public:
    void handleAsync(const HttpRequest& request, 
                    Context& context,
                    NextAsyncHandler next, 
                    AsyncCallback callback) override {
        
        std::string token = request.getHeader("Authorization");
        if (token.empty()) {
            callback(HttpResponse(401, "{\"error\": \"Token required\"}"));
            return;
        }
        
        // Async token validation
        validateTokenAsync(token, [this, &request, &context, next, callback]
                          (bool valid, const std::string& userId) {
            if (!valid) {
                callback(HttpResponse(401, "{\"error\": \"Invalid token\"}"));
                return;
            }
            
            // Add user info to context
            ContextHelper helper(context);
            helper.setString("user_id", userId);
            helper.setString("authenticated", "true");
            
            // Continue to next middleware
            next(request, context, callback);
        });
    }
    
    std::string getName() const override { return "AsyncAuthMiddleware"; }
    int getPriority() const override { return 100; }
};

// Register async middleware
server.registerAsyncMiddleware(std::make_shared<AsyncAuthMiddleware>());
```

### Middleware Factory Example ✅ NEW

```cpp
#include <cppSwitchboard/middleware_factory.h>

// Get factory instance
MiddlewareFactory& factory = MiddlewareFactory::getInstance();

// Create middleware from configuration
MiddlewareInstanceConfig authConfig;
authConfig.name = "auth";
authConfig.enabled = true;
authConfig.priority = 100;
authConfig.setString("jwt_secret", "your-secret-key");
authConfig.setString("algorithm", "HS256");

auto authMiddleware = factory.createMiddleware(authConfig);
if (authMiddleware) {
    server.registerMiddleware(authMiddleware);
}

// Create pipeline from configuration
std::vector<MiddlewareInstanceConfig> configs = {
    createCorsConfig(),
    createAuthConfig(),
    createLoggingConfig()
};

auto pipeline = factory.createPipeline(configs);
server.registerRouteWithMiddleware("/api/*", HttpMethod::GET, handler, pipeline);
```

---

## Error Handling

### Exception Types

The library defines several exception types for different error conditions:

- `ConfigurationException` - Configuration-related errors
- `NetworkException` - Network and connection errors
- `RoutingException` - Route registration and matching errors
- `HttpException` - HTTP protocol errors

### Error Response Helpers

```cpp
// Standard error responses
auto notFound = HttpResponse::notFound(); // 404
auto serverError = HttpResponse::internalError(); // 500

// Custom error responses
HttpResponse badRequest(400, "{\"error\": \"Invalid request\"}");
HttpResponse unauthorized(401, "{\"error\": \"Authentication required\"}");
HttpResponse forbidden(403, "{\"error\": \"Access denied\"}");
```

### Error Handling Best Practices

```cpp
server.registerRoute("/api/data", HttpMethod::GET, [](const HttpRequest& req) {
    try {
        // Process request
        std::string data = processData(req);
        return HttpResponse::json(data);
    } catch (const std::invalid_argument& e) {
        return HttpResponse(400, "{\"error\": \"" + std::string(e.what()) + "\"}");
    } catch (const std::exception& e) {
        // Log error
        logger.error("Unexpected error: " + std::string(e.what()));
        return HttpResponse::internalError();
    }
});
```

---

## Performance and Best Practices

### Threading Model

- The server uses an asynchronous, event-driven architecture
- Request handlers should be thread-safe
- Avoid blocking operations in handlers

### Memory Management

- Use RAII principles for resource management
- Prefer smart pointers for dynamic allocation
- Be mindful of request/response object lifetimes

### Configuration Optimization

```cpp
// Production configuration example
Http1Config prodConfig;
prodConfig.maxConnections = 1000;
prodConfig.keepAliveTimeout = 5;
prodConfig.maxRequestSize = 1024 * 1024; // 1MB

Http2Config http2Config;
http2Config.maxConcurrentStreams = 200;
http2Config.initialWindowSize = 65535;
```

---

## Building and Integration

### CMake Integration

```cmake
find_package(cppSwitchboard REQUIRED)

target_link_libraries(your_target 
    PRIVATE cppSwitchboard::cppSwitchboard
)
```

### Dependencies

- C++17 compatible compiler
- OpenSSL (for HTTPS/HTTP2 support)
- CMake 3.15+

---

This API reference provides comprehensive documentation for the cppSwitchboard library. For additional examples and detailed usage patterns, refer to the examples directory and test suite. 