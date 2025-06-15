# Contributing to cppSwitchboard

## Welcome Contributors!

Thank you for your interest in contributing to cppSwitchboard! This document provides guidelines and information for contributors to help maintain code quality and streamline the development process.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Contribution Process](#contribution-process)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Documentation Requirements](#documentation-requirements)
- [Performance Considerations](#performance-considerations)
- [Security Guidelines](#security-guidelines)
- [Review Process](#review-process)
- [Release Process](#release-process)

## Code of Conduct

### Our Pledge
We are committed to providing a friendly, safe, and welcoming environment for all contributors, regardless of experience level, gender identity and expression, sexual orientation, disability, personal appearance, body size, race, ethnicity, age, religion, or nationality.

### Expected Behavior
- Be respectful and inclusive in all interactions
- Provide constructive feedback and criticism
- Focus on what is best for the community and project
- Show empathy towards other community members
- Help maintain a positive learning environment

### Unacceptable Behavior
- Harassment, trolling, or discriminatory language
- Personal attacks or inflammatory comments  
- Publishing private information without consent
- Spam or off-topic discussions
- Any conduct that could reasonably be considered inappropriate

### Enforcement
Project maintainers have the right to remove, edit, or reject comments, commits, code, issues, and other contributions that violate this Code of Conduct.

## Getting Started

### Prerequisites
Before contributing, ensure you have:
- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.12+
- Git for version control
- Basic understanding of HTTP protocols
- Familiarity with modern C++ practices

### Initial Setup
1. **Fork the Repository**
```bash
# Fork on GitHub, then clone your fork
git clone https://github.com/YOUR_USERNAME/cppSwitchboard.git
cd cppSwitchboard
```

2. **Add Upstream Remote**
```bash
git remote add upstream https://github.com/cppswitchboard/cppSwitchboard.git
```

3. **Install Dependencies**
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install libnghttp2-dev libssl-dev libyaml-cpp-dev libboost-system-dev

# Build and test
mkdir build && cd build
cmake ..
make -j$(nproc)
make test
```

### First Contribution
Start with:
- Fixing typos or improving documentation
- Adding test cases for existing functionality
- Addressing "good first issue" labeled items
- Reviewing open pull requests

## Development Environment

### Recommended Setup

#### IDE Configuration
**Visual Studio Code**
```json
// .vscode/settings.json
{
    "C_Cpp.default.cppStandard": "c++17",
    "C_Cpp.default.compilerPath": "/usr/bin/g++",
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/include",
        "${workspaceFolder}/lib/cppSwitchboard/include"
    ],
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureArgs": ["-DCMAKE_BUILD_TYPE=Debug"]
}
```

**CLion**
- Import CMake project
- Set C++ standard to C++17
- Enable code formatting with provided `.clang-format`

#### Build Configuration
```bash
# Debug build for development
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..

# Release build for performance testing
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON ..

# With sanitizers for debugging
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined" ..
```

### Development Tools

#### Static Analysis
```bash
# clang-tidy
clang-tidy src/*.cpp -- -I include -std=c++17

# cppcheck
cppcheck --enable=all --std=c++17 src/ include/

# clang-format (automatically applied)
clang-format -i src/*.cpp include/**/*.h
```

#### Memory Debugging
```bash
# Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./tests/unit_tests

# AddressSanitizer (compile with -fsanitize=address)
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
./tests/unit_tests
```

## Contribution Process

### 1. Issue Creation
Before starting work:
- Search existing issues to avoid duplication
- Create detailed issue description with:
  - Clear problem statement
  - Expected vs actual behavior
  - Minimal reproduction steps
  - Environment information

#### Issue Templates

**Bug Report Template**
```markdown
## Bug Description
Brief description of the issue

## Environment
- OS: [e.g., Ubuntu 22.04]
- Compiler: [e.g., GCC 11.4.0]
- cppSwitchboard Version: [e.g., v1.0.0]

## Steps to Reproduce
1. Step one
2. Step two
3. See error

## Expected Behavior
What should happen

## Actual Behavior
What actually happens

## Additional Context
Any other relevant information
```

**Feature Request Template**
```markdown
## Feature Description
Clear description of proposed feature

## Use Case
Why is this feature needed?

## Proposed Implementation
High-level approach (optional)

## Alternatives Considered
Other solutions evaluated

## Additional Information
Any relevant context or examples
```

### 2. Branch Management

#### Branch Naming Convention
```bash
# Feature branches
feature/issue-123-add-http3-support
feature/middleware-authentication

# Bug fix branches  
fix/issue-456-memory-leak
fix/ssl-handshake-timeout

# Documentation branches
docs/api-reference-update
docs/performance-guide

# Hotfix branches (for critical production issues)
hotfix/security-vulnerability-fix
```

#### Branch Workflow
```bash
# Create feature branch from main
git checkout main
git pull upstream main
git checkout -b feature/my-new-feature

# Make changes and commit
git add .
git commit -m "Add new feature: brief description"

# Keep branch updated
git fetch upstream
git rebase upstream/main

# Push to your fork
git push origin feature/my-new-feature
```

### 3. Commit Guidelines

#### Commit Message Format
```
type(scope): brief description

Detailed explanation of changes made, including:
- What was changed and why
- Any breaking changes
- References to issues

Fixes #123
```

#### Commit Types
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code changes that neither fix bugs nor add features
- `perf`: Performance improvements
- `test`: Adding or modifying tests
- `build`: Changes to build system or dependencies
- `ci`: Changes to CI configuration

#### Examples
```bash
feat(http2): add server push support

Implement HTTP/2 server push functionality to improve 
page load performance. Includes:
- Server push API in HttpResponse
- Configuration options for push policies
- Integration tests

Fixes #234

fix(ssl): resolve handshake timeout issue

Fix SSL handshake timeout when connecting to servers
with slow certificate validation. Increase default
timeout from 5s to 30s and make it configurable.

Fixes #456
```

### 4. Pull Request Process

#### Before Creating PR
- [ ] All tests pass locally
- [ ] Code follows style guidelines
- [ ] Documentation updated if needed
- [ ] Performance impact assessed
- [ ] Security considerations reviewed

#### PR Title and Description
```markdown
[Type] Brief description of changes

## Summary
Detailed description of what this PR does

## Changes Made
- List of specific changes
- Include any breaking changes

## Testing
- How was this tested?
- New test cases added?

## Performance Impact
- Any performance implications?
- Benchmark results if applicable

## Documentation
- Documentation updated?
- New examples added?

Closes #123
```

#### PR Checklist
- [ ] Code compiles without warnings
- [ ] All existing tests pass
- [ ] New tests added for new functionality
- [ ] Code coverage maintained or improved
- [ ] Documentation updated
- [ ] CHANGELOG.md updated (for releases)
- [ ] Performance benchmarks run (if applicable)
- [ ] Security review completed (if applicable)

## Coding Standards

### C++ Style Guide

#### General Principles
- Follow modern C++ best practices (C++17)
- Prefer RAII and smart pointers
- Use const-correctness throughout
- Minimize memory allocations in hot paths
- Write self-documenting code with clear names

#### Naming Conventions
```cpp
// Classes: PascalCase
class HttpServer {
    // Public members: camelCase
public:
    void startServer();
    bool isRunning() const;
    
    // Private members: camelCase with underscore suffix
private:
    std::string server_name_;
    std::atomic<bool> is_running_{false};
};

// Functions: camelCase
void processRequest(const HttpRequest& request);

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_CONNECTIONS = 10000;
constexpr char DEFAULT_SERVER_NAME[] = "cppSwitchboard";

// Enums: PascalCase with PascalCase values
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE
};

// Namespaces: lowercase
namespace cppSwitchboard {
namespace internal {
    // implementation details
}
}
```

#### Code Formatting
We use clang-format with the following configuration:

```yaml
# .clang-format
BasedOnStyle: Google
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
AccessModifierOffset: -2
ConstructorInitializerIndentWidth: 4
ContinuationIndentWidth: 4
```

#### Header Organization
```cpp
// 1. System headers
#include <algorithm>
#include <memory>
#include <string>

// 2. Third-party library headers
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>

// 3. Project headers
#include "cppSwitchboard/http_server.h"
#include "cppSwitchboard/config.h"

// 4. Local headers (implementation files only)
#include "internal/server_impl.h"
```

### Error Handling

#### Exception Safety
```cpp
// Prefer RAII for resource management
class ResourceManager {
public:
    ResourceManager() : resource_(acquire_resource()) {
        if (!resource_) {
            throw std::runtime_error("Failed to acquire resource");
        }
    }
    
    ~ResourceManager() {
        if (resource_) {
            release_resource(resource_);
        }
    }
    
    // Non-copyable, movable
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    
    ResourceManager(ResourceManager&& other) noexcept 
        : resource_(std::exchange(other.resource_, nullptr)) {}
    
    ResourceManager& operator=(ResourceManager&& other) noexcept {
        if (this != &other) {
            if (resource_) {
                release_resource(resource_);
            }
            resource_ = std::exchange(other.resource_, nullptr);
        }
        return *this;
    }
};
```

#### Error Propagation
```cpp
// Use exceptions for exceptional conditions
// Use return codes for expected failures
enum class ParseResult {
    SUCCESS,
    INVALID_FORMAT,
    INCOMPLETE_DATA,
    BUFFER_OVERFLOW
};

ParseResult parseHttpHeader(const std::string& input, HttpHeader& result) {
    if (input.empty()) {
        return ParseResult::INCOMPLETE_DATA;
    }
    
    // Parse logic...
    
    return ParseResult::SUCCESS;
}
```

### Performance Guidelines

#### Memory Management
```cpp
// Prefer stack allocation when possible
void processRequest(const HttpRequest& request) {
    HttpResponse response;  // Stack allocated
    
    // Use move semantics to avoid copies
    response.setBody(generateResponseBody(request));
    
    return response;  // Return value optimization
}

// Use object pools for frequent allocations
class RequestPool {
    std::vector<std::unique_ptr<HttpRequest>> pool_;
    std::mutex mutex_;
    
public:
    std::unique_ptr<HttpRequest> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pool_.empty()) {
            auto request = std::move(pool_.back());
            pool_.pop_back();
            return request;
        }
        return std::make_unique<HttpRequest>();
    }
    
    void release(std::unique_ptr<HttpRequest> request) {
        request->reset();  // Clear previous data
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::move(request));
    }
};
```

#### Threading
```cpp
// Use thread-safe patterns
class ThreadSafeCounter {
    std::atomic<int> count_{0};
    
public:
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    int get() const {
        return count_.load(std::memory_order_relaxed);
    }
};

// Minimize lock contention
class OptimizedCache {
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> cache_;
    
public:
    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = cache_.find(key);
        return it != cache_.end() ? it->second : "";
    }
    
    void set(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_[key] = value;
    }
};
```

## Testing Guidelines

### Test Structure

#### Unit Tests
```cpp
// tests/unit/test_http_request.cpp
#include <gtest/gtest.h>
#include "cppSwitchboard/http_request.h"

class HttpRequestTest : public ::testing::Test {
protected:
    void SetUp() override {
        request_ = std::make_unique<HttpRequest>();
    }
    
    void TearDown() override {
        request_.reset();
    }
    
    std::unique_ptr<HttpRequest> request_;
};

TEST_F(HttpRequestTest, SetAndGetHeader) {
    const std::string key = "Content-Type";
    const std::string value = "application/json";
    
    request_->setHeader(key, value);
    
    EXPECT_EQ(request_->getHeader(key), value);
}

TEST_F(HttpRequestTest, GetNonExistentHeader) {
    EXPECT_TRUE(request_->getHeader("Non-Existent").empty());
}
```

#### Integration Tests
```cpp
// tests/integration/test_server_lifecycle.cpp
class ServerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = ConfigLoader::createDefault();
        config_->http1.port = 0;  // Use random available port
        server_ = HttpServer::create(*config_);
    }
    
    std::unique_ptr<ServerConfig> config_;
    std::unique_ptr<HttpServer> server_;
};

TEST_F(ServerIntegrationTest, StartStopServer) {
    EXPECT_NO_THROW(server_->start());
    EXPECT_TRUE(server_->isRunning());
    
    EXPECT_NO_THROW(server_->stop());
    EXPECT_FALSE(server_->isRunning());
}
```

#### Performance Tests
```cpp
// tests/performance/test_throughput.cpp
class ThroughputTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup high-performance server configuration
        config_ = ConfigLoader::createDefault();
        config_->general.workerThreads = std::thread::hardware_concurrency();
        config_->general.maxConnections = 10000;
        
        server_ = HttpServer::create(*config_);
        server_->get("/benchmark", [](const HttpRequest&) {
            return HttpResponse::ok("Hello, World!");
        });
        
        server_->start();
    }
};

TEST_F(ThroughputTest, SimpleHandlerPerformance) {
    const int num_requests = 10000;
    const int concurrent_connections = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate concurrent requests
    std::vector<std::future<void>> futures;
    for (int i = 0; i < concurrent_connections; ++i) {
        futures.push_back(std::async(std::launch::async, [this, num_requests] {
            for (int j = 0; j < num_requests / concurrent_connections; ++j) {
                // Make HTTP request
                auto response = makeRequest("GET", "/benchmark");
                EXPECT_EQ(response.getStatus(), 200);
            }
        }));
    }
    
    // Wait for all requests to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double rps = static_cast<double>(num_requests) / (duration.count() / 1000.0);
    
    std::cout << "Requests per second: " << rps << std::endl;
    EXPECT_GT(rps, 10000);  // Expect at least 10k RPS
}
```

### Test Requirements

#### Coverage Requirements
- Minimum 80% line coverage for new code
- 90% line coverage for critical paths
- All public APIs must have tests
- Error conditions must be tested

#### Test Organization
```
tests/
├── unit/           # Unit tests for individual components
│   ├── test_http_request.cpp
│   ├── test_http_response.cpp
│   └── test_route_registry.cpp
├── integration/    # Integration tests for component interaction
│   ├── test_server_lifecycle.cpp
│   └── test_configuration.cpp
├── performance/    # Performance and load tests
│   ├── test_throughput.cpp
│   └── test_latency.cpp
└── functional/     # End-to-end functional tests
    ├── test_rest_api.cpp
    └── test_static_files.cpp
```

## Documentation Requirements

### Code Documentation

#### Doxygen Comments
```cpp
/**
 * @brief HTTP server class providing both HTTP/1.1 and HTTP/2 support
 * 
 * The HttpServer class is the main entry point for creating HTTP servers.
 * It provides a unified API for both HTTP/1.1 and HTTP/2 protocols and
 * supports middleware, routing, and SSL/TLS termination.
 * 
 * @example
 * @code
 * auto config = ConfigLoader::createDefault();
 * auto server = HttpServer::create(*config);
 * 
 * server->get("/hello", [](const HttpRequest& req) {
 *     return HttpResponse::ok("Hello, World!");
 * });
 * 
 * server->start();
 * @endcode
 * 
 * @see ServerConfig for configuration options
 * @see HttpRequest for request handling
 * @see HttpResponse for response generation
 */
class HttpServer {
public:
    /**
     * @brief Create a new HTTP server instance
     * 
     * @param config Server configuration containing protocol settings,
     *               SSL configuration, and performance tuning options
     * @return std::unique_ptr<HttpServer> Unique pointer to server instance
     * @throws std::invalid_argument if configuration is invalid
     * @throws std::runtime_error if server initialization fails
     */
    static std::unique_ptr<HttpServer> create(const ServerConfig& config);
    
    /**
     * @brief Register a GET route handler
     * 
     * @param path URL path pattern (supports parameters like "/users/{id}")
     * @param handler Function to handle matching requests
     * @throws std::invalid_argument if path pattern is invalid
     * 
     * @example
     * @code
     * server->get("/users/{id}", [](const HttpRequest& req) {
     *     std::string userId = req.getPathParam("id");
     *     return HttpResponse::json("{\"user_id\": \"" + userId + "\"}");
     * });
     * @endcode
     */
    void get(const std::string& path, HandlerFunction handler);
};
```

#### Inline Comments
```cpp
void optimizedFunction() {
    // Use memory pool to avoid frequent allocations
    auto buffer = buffer_pool_.acquire();
    
    // Process data with SIMD operations for performance
    processWithSIMD(buffer->data(), buffer->size());
    
    // Return buffer to pool for reuse
    buffer_pool_.release(std::move(buffer));
}
```

### User Documentation

#### Examples and Tutorials
Every public API must include:
- Basic usage example
- Advanced usage scenarios
- Common pitfalls and solutions
- Performance considerations

#### API Reference
- Complete parameter documentation
- Return value descriptions
- Exception specifications
- Thread safety guarantees
- Performance characteristics

## Security Guidelines

### Security Review Requirements

#### Security-Sensitive Areas
- SSL/TLS implementation
- Input validation and parsing
- Authentication and authorization
- Memory management
- Configuration handling

#### Secure Coding Practices
```cpp
// Input validation
bool validateInput(const std::string& input) {
    // Check length limits
    if (input.length() > MAX_INPUT_LENGTH) {
        return false;
    }
    
    // Validate character set
    return std::all_of(input.begin(), input.end(), [](char c) {
        return std::isalnum(c) || c == '-' || c == '_';
    });
}

// Safe string operations
std::string safeStringOperation(const std::string& input) {
    std::string result;
    result.reserve(input.length() * 2);  // Prevent reallocations
    
    for (char c : input) {
        if (needsEscaping(c)) {
            result += escapeCharacter(c);
        } else {
            result += c;
        }
    }
    
    return result;
}

// Secure memory handling
class SecureBuffer {
    std::vector<uint8_t> data_;
    
public:
    explicit SecureBuffer(size_t size) : data_(size) {}
    
    ~SecureBuffer() {
        // Clear sensitive data before destruction
        std::fill(data_.begin(), data_.end(), 0);
    }
    
    // Non-copyable for security
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;
};
```

### Security Review Process
1. **Automated Security Scanning**: Run static analysis tools
2. **Manual Code Review**: Security-focused review by maintainers
3. **Dependency Audit**: Check for known vulnerabilities in dependencies
4. **Penetration Testing**: Test against common attack vectors
5. **Security Documentation**: Document security considerations

## Review Process

### Review Guidelines

#### For Authors
- Keep pull requests focused and reasonably sized
- Provide clear description and context
- Respond promptly to reviewer feedback
- Test thoroughly before requesting review
- Update documentation as needed

#### For Reviewers
- Be constructive and respectful in feedback
- Focus on correctness, performance, and maintainability
- Check for proper testing and documentation
- Verify security implications
- Ensure coding standards compliance

### Review Checklist

#### Code Quality
- [ ] Code is readable and well-structured
- [ ] Follows established coding standards
- [ ] Includes appropriate error handling
- [ ] Uses modern C++ idioms correctly
- [ ] No obvious performance issues

#### Testing
- [ ] Adequate test coverage for changes
- [ ] Tests are well-written and maintainable
- [ ] All existing tests continue to pass
- [ ] Performance impact assessed if applicable

#### Documentation
- [ ] Public APIs properly documented
- [ ] User-facing documentation updated
- [ ] Code comments explain complex logic
- [ ] Examples provided for new features

#### Security
- [ ] Input validation implemented where needed
- [ ] No obvious security vulnerabilities
- [ ] Sensitive data handled appropriately
- [ ] External dependencies reviewed

### Merge Requirements
- [ ] At least one approval from maintainer
- [ ] All CI checks passing
- [ ] Conflicts resolved with main branch
- [ ] Commit messages follow guidelines
- [ ] Change log updated (for releases)

## Release Process

### Version Numbering
We follow [Semantic Versioning](https://semver.org/):
- **MAJOR**: Breaking changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes (backward compatible)

### Release Preparation
1. **Update Version Numbers**
   - CMakeLists.txt
   - Documentation
   - Package files

2. **Update CHANGELOG.md**
   - Document all changes since last release
   - Include breaking changes prominently
   - Credit contributors

3. **Documentation Update**
   - Ensure all documentation is current
   - Update API reference
   - Review examples and tutorials

4. **Performance Testing**
   - Run comprehensive benchmarks
   - Compare with previous version
   - Document any performance changes

### Release Checklist
- [ ] All tests passing on supported platforms
- [ ] Documentation build successful
- [ ] Performance benchmarks completed
- [ ] Security review completed
- [ ] Version numbers updated
- [ ] CHANGELOG.md updated
- [ ] Release notes prepared
- [ ] Migration guide written (for breaking changes)

### Post-Release
- Tag release in Git
- Create GitHub release with binaries
- Update package managers
- Announce on communication channels
- Monitor for issues and feedback

## Getting Help

### Resources
- **Documentation**: Check existing docs and examples
- **GitHub Issues**: Search for similar problems
- **Discussions**: Ask questions in GitHub Discussions
- **Chat**: Join our Discord/Slack for real-time help

### Questions Welcome
- Implementation questions
- Performance optimization help
- Architecture discussions
- Testing strategies
- Documentation improvements

Thank you for contributing to cppSwitchboard! Your efforts help make this project better for everyone. 