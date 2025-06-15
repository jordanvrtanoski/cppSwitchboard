# cppSwitchboard Asynchronous Programming Guide

## Overview

Asynchronous programming in cppSwitchboard enables high-performance, non-blocking HTTP server applications that can handle thousands of concurrent connections efficiently. This guide covers the asynchronous programming model, patterns, and best practices for building scalable applications.

## Table of Contents

- [Asynchronous Architecture](#asynchronous-architecture)
- [Async Handlers](#async-handlers)
- [Futures and Promises](#futures-and-promises)
- [Thread Pool Management](#thread-pool-management)
- [Error Handling](#error-handling)
- [Performance Optimization](#performance-optimization)
- [Best Practices](#best-practices)
- [Examples](#examples)

## Asynchronous Architecture

cppSwitchboard implements an event-driven, non-blocking I/O architecture:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Event Loop    │───▶│  Handler Pool   │───▶│  Worker Threads │
│   (Main Thread) │    │  (Async Queue)  │    │  (Background)   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│ Connection Mgmt │    │ Request Routing │    │  Business Logic │
│ Socket Handling │    │ Middleware Exec │    │  Database I/O   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

Key components:
- **Event Loop**: Handles incoming connections and I/O events
- **Handler Pool**: Manages async request/response processing
- **Worker Threads**: Execute background tasks and computations
- **Connection Management**: Maintains WebSocket and HTTP connections

## Async Handlers

### Basic Async Handler

Replace synchronous handlers with async variants for non-blocking operations:

```cpp
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/async_handler.h>
#include <future>

class DatabaseHandler : public AsyncHttpHandler {
public:
    std::future<HttpResponse> handleAsync(const HttpRequest& request) override {
        return std::async(std::launch::async, [this, request]() -> HttpResponse {
            try {
                // Simulate database query
                auto result = queryDatabase(request.getQueryParam("id"));
                return HttpResponse::json(result);
            } catch (const std::exception& e) {
                return HttpResponse::internalServerError(
                    "{\"error\": \"" + std::string(e.what()) + "\"}"
                );
            }
        });
    }

private:
    std::string queryDatabase(const std::string& id) {
        // Simulate async database operation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return "{\"id\": \"" + id + "\", \"data\": \"example\"}";
    }
};

// Usage
auto server = HttpServer::create(config);
server->registerAsyncHandler("/api/data", 
    std::make_shared<DatabaseHandler>());
```

### Lambda-based Async Handlers

For simpler operations, use async lambda functions:

```cpp
server->getAsync("/api/weather", [](const HttpRequest& request) -> std::future<HttpResponse> {
    return std::async(std::launch::async, [request]() -> HttpResponse {
        // Simulate external API call
        std::string city = request.getQueryParam("city");
        std::string weatherData = fetchWeatherData(city);
        
        return HttpResponse::json(weatherData);
    });
});

server->postAsync("/api/upload", [](const HttpRequest& request) -> std::future<HttpResponse> {
    return std::async(std::launch::async, [request]() -> HttpResponse {
        // Process file upload asynchronously
        std::string filename = saveUploadedFile(request.getBody());
        
        return HttpResponse::json(
            "{\"status\": \"uploaded\", \"filename\": \"" + filename + "\"}"
        );
    });
});
```

## Futures and Promises

### Using std::future and std::promise

For complex async operations with multiple stages:

```cpp
class ImageProcessingHandler : public AsyncHttpHandler {
public:
    std::future<HttpResponse> handleAsync(const HttpRequest& request) override {
        auto promise = std::make_shared<std::promise<HttpResponse>>();
        auto future = promise->get_future();
        
        // Stage 1: Download image
        downloadImageAsync(request.getQueryParam("url"))
            .then([this, promise](const std::vector<uint8_t>& imageData) {
                // Stage 2: Process image
                return processImageAsync(imageData);
            })
            .then([this, promise](const std::vector<uint8_t>& processedData) {
                // Stage 3: Upload to storage
                return uploadToStorageAsync(processedData);
            })
            .then([promise](const std::string& storageUrl) {
                // Stage 4: Return response
                std::string response = "{\"processed_url\": \"" + storageUrl + "\"}";
                promise->set_value(HttpResponse::json(response));
            })
            .onError([promise](const std::exception& e) {
                promise->set_value(HttpResponse::internalServerError(
                    "{\"error\": \"" + std::string(e.what()) + "\"}"
                ));
            });
        
        return future;
    }

private:
    std::future<std::vector<uint8_t>> downloadImageAsync(const std::string& url) {
        return std::async(std::launch::async, [url]() {
            // Implement image download
            std::vector<uint8_t> data;
            // ... download logic
            return data;
        });
    }
    
    std::future<std::vector<uint8_t>> processImageAsync(const std::vector<uint8_t>& input) {
        return std::async(std::launch::async, [input]() {
            // Implement image processing
            std::vector<uint8_t> processed = input; // placeholder
            // ... processing logic
            return processed;
        });
    }
    
    std::future<std::string> uploadToStorageAsync(const std::vector<uint8_t>& data) {
        return std::async(std::launch::async, [data]() {
            // Implement storage upload
            return "https://storage.example.com/image123.jpg";
        });
    }
};
```

## Thread Pool Management

### Custom Thread Pool Configuration

Configure thread pools for different types of operations:

```cpp
#include <cppSwitchboard/thread_pool.h>

// Configure in server startup
ServerConfig config;
config.general.workerThreads = 8;          // I/O threads
config.general.computeThreads = 4;         // CPU-intensive tasks
config.general.databaseThreads = 2;        // Database operations

auto server = HttpServer::create(config);

// Access thread pools
auto& ioPool = server->getIOThreadPool();
auto& computePool = server->getComputeThreadPool();
auto& dbPool = server->getDatabaseThreadPool();
```

## Performance Optimization

### Async Connection Pooling

```cpp
class DatabaseConnectionPool {
public:
    DatabaseConnectionPool(size_t poolSize) {
        for (size_t i = 0; i < poolSize; ++i) {
            connections_.push(createConnection());
        }
    }
    
    template<typename Func>
    std::future<typename std::invoke_result<Func, DatabaseConnection&>::type>
    execute(Func func) {
        using ReturnType = typename std::invoke_result<Func, DatabaseConnection&>::type;
        
        return std::async(std::launch::async, [this, func]() -> ReturnType {
            auto connection = acquireConnection();
            try {
                auto result = func(*connection);
                releaseConnection(std::move(connection));
                return result;
            } catch (...) {
                releaseConnection(std::move(connection));
                throw;
            }
        });
    }

private:
    std::queue<std::unique_ptr<DatabaseConnection>> connections_;
    std::mutex mutex_;
    std::condition_variable condition_;
    
    std::unique_ptr<DatabaseConnection> acquireConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !connections_.empty(); });
        
        auto connection = std::move(connections_.front());
        connections_.pop();
        return connection;
    }
    
    void releaseConnection(std::unique_ptr<DatabaseConnection> connection) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push(std::move(connection));
        condition_.notify_one();
    }
    
    std::unique_ptr<DatabaseConnection> createConnection() {
        return std::make_unique<DatabaseConnection>();
    }
};
```

## Best Practices

### 1. Avoid Blocking Operations in Async Context

```cpp
// Bad: Blocking operation in async handler
server->getAsync("/bad", [](const HttpRequest& request) -> std::future<HttpResponse> {
    return std::async(std::launch::async, []() -> HttpResponse {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Blocks thread
        return HttpResponse::ok("done");
    });
});

// Good: Use async I/O operations
server->getAsync("/good", [](const HttpRequest& request) -> std::future<HttpResponse> {
    return asyncHttpClient.get("https://api.example.com/data")
        .then([](const std::string& response) -> HttpResponse {
            return HttpResponse::json(response);
        });
});
```

### 2. Set Reasonable Timeouts

```cpp
class TimeoutHandler : public AsyncHttpHandler {
public:
    std::future<HttpResponse> handleAsync(const HttpRequest& request) override {
        auto promise = std::make_shared<std::promise<HttpResponse>>();
        auto future = promise->get_future();
        
        // Set timeout
        auto timeoutFuture = std::async(std::launch::async, [promise]() {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            promise->set_value(HttpResponse::requestTimeout(
                "{\"error\": \"Request timeout\"}"
            ));
        });
        
        // Main operation
        auto operationFuture = std::async(std::launch::async, [promise, request]() {
            try {
                auto result = performLongOperation(request);
                promise->set_value(HttpResponse::json(result));
            } catch (const std::exception& e) {
                promise->set_value(HttpResponse::internalServerError(
                    "{\"error\": \"" + std::string(e.what()) + "\"}"
                ));
            }
        });
        
        return future;
    }

private:
    std::string performLongOperation(const HttpRequest& request) {
        // Long-running operation
        return "{\"result\": \"computed\"}";
    }
};
```

## Conclusion

Asynchronous programming in cppSwitchboard enables building high-performance, scalable HTTP servers. By leveraging futures, promises, thread pools, and proper error handling, you can create responsive applications that efficiently handle concurrent requests and background operations.

Key takeaways:
- Use async handlers for I/O-bound operations
- Implement proper error handling and timeouts
- Leverage thread pools for different operation types
- Apply caching and connection pooling for performance
- Follow RAII principles for resource management

For more information, see:
- [API Reference](API_REFERENCE.md)
- [Configuration Guide](CONFIGURATION.md)
- [Performance Tuning Guide](PERFORMANCE.md) 