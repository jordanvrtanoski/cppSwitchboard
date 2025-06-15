# cppSwitchboard Development Rules

## Project Context

**cppSwitchboard** is a high-performance HTTP middleware framework for C++ supporting HTTP/1.1 and HTTP/2 protocols. The framework emphasizes:

- Modern C++17 design patterns
- Zero-copy operations and memory efficiency  
- Thread-safe middleware pipeline architecture
- Comprehensive testing (66+ tests, 86% pass rate)
- Configuration-driven middleware composition
- Cross-platform compatibility (Linux, macOS, Windows)

## Coding Standards

### Naming Conventions

#### Classes and Types
```cpp
// ✅ GOOD - PascalCase for classes, interfaces, and structs
class HttpServer { };
class MiddlewarePipeline { };
struct ServerConfig { };
enum class HttpMethod { GET, POST };

// ❌ BAD
class httpServer { };
class middleware_pipeline { };
```

#### Methods and Functions
```cpp
// ✅ GOOD - camelCase for methods
void registerHandler(const std::string& path);
HttpResponse processRequest(const HttpRequest& request);
bool isEnabled() const;

// ❌ BAD - snake_case or PascalCase
void register_handler(const std::string& path);
void RegisterHandler(const std::string& path);
```

#### Variables and Members
```cpp
// ✅ GOOD - camelCase with trailing underscore for private members
class HttpServer {
private:
    std::shared_ptr<RouteRegistry> routeRegistry_;
    ServerConfig config_;
    bool running_;
public:
    int connectionCount;  // Public members without underscore
};

// ❌ BAD
private:
    std::shared_ptr<RouteRegistry> route_registry;
    std::shared_ptr<RouteRegistry> m_routeRegistry;
```

#### File Names
```cpp
// ✅ GOOD - snake_case for files, match class names
http_server.h / http_server.cpp
middleware_pipeline.h / middleware_pipeline.cpp
auth_middleware.h / auth_middleware.cpp

// ❌ BAD
HttpServer.h
middlewarePipeline.cpp
```

### Namespace Structure

```cpp
// ✅ GOOD - Consistent namespace hierarchy
namespace cppSwitchboard {
    class HttpServer { };
    
    namespace middleware {
        class AuthMiddleware { };
        class LoggingMiddleware { };
    }
} // namespace cppSwitchboard

// ✅ GOOD - Using declarations in implementation files
using namespace cppSwitchboard;
using namespace cppSwitchboard::middleware;

// ❌ BAD - using namespace in headers
// Never put using namespace in header files
```

### Memory Management

#### Smart Pointers (Required)
```cpp
// ✅ GOOD - Always use smart pointers for ownership
std::shared_ptr<HttpHandler> handler = std::make_shared<MyHandler>();
std::unique_ptr<ServerConfig> config = std::make_unique<ServerConfig>();

// ✅ GOOD - Factory functions return smart pointers
static std::shared_ptr<HttpServer> create(const ServerConfig& config);

// ❌ BAD - Raw pointers for ownership
HttpHandler* handler = new MyHandler();  // Memory leak risk
```

#### RAII Pattern
```cpp
// ✅ GOOD - Resource management with RAII
class HttpServer {
private:
    std::unique_ptr<boost::asio::io_context> ioContext_;
    std::vector<std::thread> workerThreads_;
    
public:
    ~HttpServer() {
        stop();  // Cleanup in destructor
    }
};

// ❌ BAD - Manual resource management
class HttpServer {
    boost::asio::io_context* ioContext_;  // Who owns this?
};
```

### Thread Safety

#### Thread-Safe Patterns
```cpp
// ✅ GOOD - Mutex protection for shared state
class RateLimitMiddleware {
private:
    mutable std::mutex bucketsMutex_;
    std::unordered_map<std::string, BucketState> buckets_;
    
public:
    bool consumeTokens(const std::string& key) {
        std::lock_guard<std::mutex> lock(bucketsMutex_);
        // Safe access to buckets_
    }
};

// ✅ GOOD - Atomic operations for simple counters
std::atomic<int> requestCount_{0};
std::atomic<bool> running_{false};

// ❌ BAD - Unprotected shared state
class Middleware {
    int callCount = 0;  // Race condition!
public:
    void handle() { callCount++; }  // Not thread-safe
};
```

## Security Guidelines

### Input Validation
```cpp
// ✅ GOOD - Always validate inputs
HttpResponse AuthMiddleware::handle(const HttpRequest& request, Context& context, NextHandler next) {
    std::string token = extractToken(request);
    if (token.empty()) {
        return createAuthErrorResponse("Missing authorization token");
    }
    
    if (token.length() > MAX_TOKEN_LENGTH) {
        return createAuthErrorResponse("Token too long");
    }
    // Continue validation...
}

// ❌ BAD - No input validation
HttpResponse handle(const HttpRequest& request) {
    std::string token = request.getHeader("Authorization");
    // Directly use token without validation
}
```

### Sensitive Data Handling
```cpp
// ✅ GOOD - Filter sensitive headers in logging
class DebugLogger {
private:
    std::vector<std::string> excludeHeaders_ = {
        "authorization", "cookie", "x-api-key", "x-auth-token"
    };
    
    bool shouldExcludeHeader(const std::string& headerName) const {
        std::string lower = headerName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return std::find(excludeHeaders_.begin(), excludeHeaders_.end(), lower) != excludeHeaders_.end();
    }
};

// ❌ BAD - Logging sensitive data
void logRequest(const HttpRequest& request) {
    // This might log Authorization headers!
    for (const auto& [name, value] : request.getHeaders()) {
        std::cout << name << ": " << value << std::endl;
    }
}
```

## Performance Guidelines

### Memory Efficiency
```cpp
// ✅ GOOD - Reserve container capacity when size is known
std::vector<std::string> headers;
headers.reserve(expectedHeaderCount);

// ✅ GOOD - Move semantics for expensive objects
HttpResponse processRequest(HttpRequest request) {  // Pass by value
    // Process request...
    return std::move(response);  // Move return
}

// ❌ BAD - Unnecessary copies
HttpResponse processRequest(const HttpRequest& request) {
    HttpResponse response = createResponse(request);  // Copy
    return response;  // Another copy
}
```

### Avoid Expensive Operations in Hot Paths
```cpp
// ✅ GOOD - Pre-compile regex patterns
class RouteRegistry {
private:
    struct RouteInfo {
        std::regex pathRegex;  // Compiled once
        // ...
    };
};

// ❌ BAD - Compile regex on every request
bool matchesRoute(const std::string& path, const std::string& pattern) {
    std::regex regex(pattern);  // Expensive compilation!
    return std::regex_match(path, regex);
}
```

## Testing Requirements

### Test Structure
```cpp
// ✅ GOOD - Follow Google Test naming conventions
class HttpServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_ = HttpServer::create(config_);
    }
    
    void TearDown() override {
        if (server_->isRunning()) {
            server_->stop();
        }
    }
    
private:
    ServerConfig config_;
    std::shared_ptr<HttpServer> server_;
};

TEST_F(HttpServerTest, ServerStartsAndStopsCorrectly) {
    EXPECT_FALSE(server_->isRunning());
    
    server_->start();
    EXPECT_TRUE(server_->isRunning());
    
    server_->stop();
    EXPECT_FALSE(server_->isRunning());
}
```

### Test Coverage Requirements
- **Unit Tests**: Every public method must have tests
- **Integration Tests**: End-to-end request/response cycles
- **Error Path Tests**: Test all error conditions
- **Performance Tests**: Benchmark critical paths
- **Thread Safety Tests**: Concurrent access patterns

### Mock Objects
```cpp
// ✅ GOOD - Use Google Mock for complex dependencies
class MockHttpHandler : public HttpHandler {
public:
    MOCK_METHOD(HttpResponse, handle, (const HttpRequest& request), (override));
};

// ✅ GOOD - Simple test doubles for basic cases
class TestHandler : public HttpHandler {
public:
    TestHandler(const std::string& response) : response_(response) {}
    HttpResponse handle(const HttpRequest& request) override {
        callCount_++;
        return HttpResponse::ok(response_);
    }
    int getCallCount() const { return callCount_; }
private:
    std::string response_;
    int callCount_ = 0;
};
```

## Git Workflow Rules

### Branch Naming
```bash
# ✅ GOOD
feature/middleware-pipeline-optimization
bugfix/memory-leak-in-http2-session
hotfix/security-token-validation
refactor/config-loading-simplification

# ❌ BAD
my-feature
fix
update
```

### Commit Messages
```bash
# ✅ GOOD - Follow conventional commits
feat(middleware): add rate limiting middleware with Redis backend

- Implement token bucket algorithm for rate limiting
- Add Redis backend support for distributed limiting
- Include comprehensive unit tests (15 test cases)
- Update documentation with configuration examples
- Sign the commit

Closes #123

# ✅ GOOD - Other valid types
fix(http2): resolve memory leak in session cleanup
docs(readme): update installation instructions
test(auth): add JWT validation edge cases
refactor(config): simplify YAML parsing logic
perf(routing): optimize regex compilation for routes

# ❌ BAD
Fixed bug
Added feature
Updated code
```

### Pre-commit Checklist
```bash
# Always run before committing
cd ~/cppSwitchboard # Replace with the absolute path where the library resides
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
make test  # All tests must pass
cd .. && make format  # If clang-format available
```

## Prohibited Actions

### Code Quality
- ❌ **Raw pointers for ownership** - Use smart pointers
- ❌ **Global mutable state** - Use dependency injection
- ❌ **Throwing exceptions in destructors** - Handle cleanup errors
- ❌ **using namespace in headers** - Pollutes client namespace
- ❌ **Uninitialized member variables** - Always initialize
- ❌ **Ignored return values** - Check error conditions

### Security
- ❌ **Logging sensitive data** - Filter authorization headers
- ❌ **Unvalidated input processing** - Always validate
- ❌ **Hardcoded secrets** - Use environment variables
- ❌ **Buffer overflows** - Use safe string operations
- ❌ **SQL injection** - Use parameterized queries (if applicable)

### Performance  
- ❌ **Synchronous I/O in request handlers** - Use async patterns
- ❌ **String concatenation in loops** - Use stringstream or reserve
- ❌ **Unnecessary memory allocations** - Profile and optimize
- ❌ **Blocking operations in middleware** - Keep middleware fast

## Preferred Patterns

### Configuration Pattern
```cpp
// ✅ GOOD - Structured configuration with validation
struct MiddlewareConfig {
    bool enabled = true;
    std::string logLevel = "info";
    int maxConnections = 1000;
    
    // Validation method
    bool validate(std::string& error) const {
        if (maxConnections <= 0) {
            error = "maxConnections must be positive";
            return false;
        }
        return true;
    }
};

// ❌ BAD - Scattered configuration without validation
void setEnabled(bool enabled);
void setLogLevel(const std::string& level);
void setMaxConnections(int max);  // No validation
```

### Error Handling Pattern
```cpp
// ✅ GOOD - Result types for recoverable errors
enum class ConfigError {
    FILE_NOT_FOUND,
    INVALID_YAML,
    VALIDATION_FAILED
};

class ConfigResult {
public:
    static ConfigResult success(std::unique_ptr<ServerConfig> config) {
        return ConfigResult(std::move(config));
    }
    
    static ConfigResult error(ConfigError error, const std::string& message) {
        return ConfigResult(error, message);
    }
    
    bool isSuccess() const { return config_ != nullptr; }
    const ServerConfig& getConfig() const { return *config_; }
    ConfigError getError() const { return error_; }
    const std::string& getMessage() const { return message_; }
};

// ❌ BAD - Exceptions for expected errors
std::unique_ptr<ServerConfig> loadConfig(const std::string& filename) {
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error("File not found");  // Expected error!
    }
}
```

### Factory Pattern
```cpp
// ✅ GOOD - Factory with configuration
class MiddlewareFactory {
public:
    static std::shared_ptr<Middleware> create(const std::string& type, const YAML::Node& config) {
        if (type == "auth") {
            return std::make_shared<AuthMiddleware>(config["jwt_secret"].as<std::string>());
        } else if (type == "cors") {
            CorsConfig corsConfig;
            corsConfig.allowedOrigins = config["origins"].as<std::vector<std::string>>();
            return std::make_shared<CorsMiddleware>(corsConfig);
        }
        return nullptr;
    }
};

// ❌ BAD - Hard to extend factory
std::shared_ptr<Middleware> createMiddleware(int type) {
    switch (type) {
        case 1: return std::make_shared<AuthMiddleware>();
        case 2: return std::make_shared<CorsMiddleware>();
        // Adding new types requires modifying this function
    }
}
```

## Build System Rules

### CMake Structure
```cmake
# ✅ GOOD - Modern CMake with targets
add_library(cppSwitchboard ${SOURCES})
target_include_directories(cppSwitchboard 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
)
target_link_libraries(cppSwitchboard 
    PUBLIC ${NGHTTP2_LIBRARIES} Boost::system
    PRIVATE OpenSSL::SSL OpenSSL::Crypto
)

# ❌ BAD - Legacy CMake with global variables
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
link_libraries(${NGHTTP2_LIBRARIES})
```

### Test Integration
```cmake
# ✅ GOOD - Separate test executable
if(BUILD_TESTING)
    add_executable(cppSwitchboard_tests ${TEST_SOURCES})
    target_link_libraries(cppSwitchboard_tests 
        PRIVATE cppSwitchboard gtest_main gmock_main
    )
    add_test(NAME UnitTests COMMAND cppSwitchboard_tests)
endif()
```

## Task Management

### Task File Maintenance
When working with the task file `lib/cppSwitchboard/tasks/tasklist-features.md`:

1. **Always verify existing features** before creating new ones:
   ```bash
   cd /home/magma/qos-manager/lib/cppSwitchboard
   grep -r "class AuthMiddleware" include/ src/
   ```

2. **Update task progress** in the same file:
   ```markdown
   #### Task 2.1: Authentication Middleware ✅ COMPLETED
   - **Status**: Complete (17/17 tests passing)
   ```

3. **Create corresponding tests** for new features:
   ```bash
   # Always create test file alongside implementation
   touch tests/test_new_middleware.cpp
   ```

## Shell Commands Format

Always use absolute paths and proper directory changes:

```bash
# ✅ GOOD - Absolute path with cd
cd /home/magma/qos-manager/lib/cppSwitchboard
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# ❌ BAD - Relative paths without context
cd build
cmake ..
make
```

## Documentation Requirements

### Code Documentation
```cpp
/**
 * @brief Processes HTTP requests through the middleware pipeline
 * 
 * This method executes all registered middleware in priority order,
 * passing the request context through each middleware component.
 * 
 * @param request The incoming HTTP request
 * @param context Shared context for middleware communication
 * @return HttpResponse The processed response
 * 
 * @throws PipelineException If middleware execution fails
 * 
 * @code{.cpp}
 * auto pipeline = std::make_shared<MiddlewarePipeline>();
 * pipeline->addMiddleware(std::make_shared<AuthMiddleware>());
 * HttpResponse response = pipeline->execute(request, context);
 * @endcode
 */
HttpResponse execute(const HttpRequest& request, Context& context);
```

### Performance Documentation
```cpp
/**
 * @brief Token bucket rate limiting implementation
 * 
 * @performance O(1) for token consumption, uses hash table lookup
 * @memory Approximately 64 bytes per tracked key (IP/user)
 * @thread_safety Thread-safe using mutex protection
 * 
 * @note For high-traffic scenarios (>1000 RPS), consider Redis backend
 */
class RateLimitMiddleware;
```

This document serves as the foundation for maintaining code quality, security, and performance standards in the cppSwitchboard framework. All contributors must follow these rules to ensure consistent, maintainable, and secure code. 