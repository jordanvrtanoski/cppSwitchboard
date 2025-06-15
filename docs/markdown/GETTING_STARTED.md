# Getting Started with cppSwitchboard

Welcome to cppSwitchboard, a high-performance C++ HTTP/1.1 and HTTP/2 server library built for modern applications. This guide will help you get up and running quickly.

## Table of Contents

1. [Requirements](#requirements)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Basic HTTP Server](#basic-http-server)
5. [HTTP/2 Server](#http2-server)
6. [Configuration](#configuration)
7. [Next Steps](#next-steps)

## Requirements

### System Requirements
- **Operating System**: Linux, macOS, or Windows (with WSL)
- **Compiler**: GCC 8+ or Clang 7+ with C++17 support
- **CMake**: Version 3.16 or higher

### Dependencies
- **Boost**: Version 1.70 or higher (system, thread, filesystem)
- **nghttp2**: For HTTP/2 support
- **OpenSSL**: For SSL/TLS encryption
- **yaml-cpp**: For configuration file parsing

### Optional Dependencies
- **Doxygen**: For API documentation generation
- **Google Test**: For running unit tests
- **Pandoc**: For PDF documentation generation

## Installation

### Ubuntu/Debian
```bash
# Install system dependencies
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config
sudo apt-get install libboost-all-dev libnghttp2-dev libssl-dev libyaml-cpp-dev

# Clone and build cppSwitchboard
git clone https://github.com/your-org/qos-manager.git
cd qos-manager/lib/cppSwitchboard
mkdir build && cd build
cmake ..
make -j$(nproc)

# Optional: Install system-wide
sudo make install
```

### macOS
```bash
# Install dependencies using Homebrew
brew install cmake boost nghttp2 openssl yaml-cpp

# Clone and build
git clone https://github.com/your-org/qos-manager.git
cd qos-manager/lib/cppSwitchboard
mkdir build && cd build
cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl ..
make -j$(sysctl -n hw.ncpu)
```

### Windows (WSL)
```bash
# Use Ubuntu/Debian instructions within WSL
# Ensure you have WSL 2 for best performance
```

## Quick Start

### 1. Include cppSwitchboard in Your Project

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyHttpServer)

set(CMAKE_CXX_STANDARD 17)

# Find cppSwitchboard
find_package(cppSwitchboard REQUIRED)

# Create your executable
add_executable(my_server main.cpp)
target_link_libraries(my_server cppSwitchboard::cppSwitchboard)
```

### 2. Hello World Server

**main.cpp**:
```cpp
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/config.h>
#include <iostream>

int main() {
    // Create server configuration
    cppSwitchboard::ServerConfig config;
    config.http1.port = 8080;
    config.general.enableLogging = true;
    
    // Create and start server
    cppSwitchboard::HttpServer server(config);
    
    // Add a simple route
    server.get("/", [](const cppSwitchboard::HttpRequest& request) {
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody("Hello, cppSwitchboard!");
        response.setHeader("Content-Type", "text/plain");
        return response;
    });
    
    std::cout << "Server starting on http://localhost:8080" << std::endl;
    server.start();
    
    return 0;
}
```

### 3. Build and Run
```bash
mkdir build && cd build
cmake ..
make
./my_server
```

Visit `http://localhost:8080` in your browser to see "Hello, cppSwitchboard!"

## Basic HTTP Server

### Creating Routes

```cpp
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/config.h>
#include <json/json.h>  // Assume JSON library

int main() {
    cppSwitchboard::ServerConfig config;
    config.http1.port = 8080;
    
    cppSwitchboard::HttpServer server(config);
    
    // GET route
    server.get("/users", [](const cppSwitchboard::HttpRequest& request) {
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody(R"([{"id": 1, "name": "John"}, {"id": 2, "name": "Jane"}])");
        response.setHeader("Content-Type", "application/json");
        return response;
    });
    
    // POST route
    server.post("/users", [](const cppSwitchboard::HttpRequest& request) {
        // Parse JSON body
        std::string body = request.getBody();
        
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(201);
        response.setBody(R"({"id": 3, "name": "New User", "status": "created"})");
        response.setHeader("Content-Type", "application/json");
        return response;
    });
    
    // Route with parameters
    server.get("/users/:id", [](const cppSwitchboard::HttpRequest& request) {
        std::string userId = request.getPathParam("id");
        
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody("User ID: " + userId);
        response.setHeader("Content-Type", "text/plain");
        return response;
    });
    
    server.start();
    return 0;
}
```

### Middleware

```cpp
// Logging middleware
server.use([](const cppSwitchboard::HttpRequest& request, 
              cppSwitchboard::HttpResponse& response, 
              std::function<void()> next) {
    std::cout << request.getMethod() << " " << request.getPath() << std::endl;
    next();
    std::cout << "Response: " << response.getStatusCode() << std::endl;
});

// CORS middleware
server.use([](const cppSwitchboard::HttpRequest& request, 
              cppSwitchboard::HttpResponse& response, 
              std::function<void()> next) {
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
    response.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    next();
});
```

## HTTP/2 Server

### Basic HTTP/2 Setup

```cpp
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/config.h>

int main() {
    cppSwitchboard::ServerConfig config;
    
    // Enable HTTP/2
    config.http2.enabled = true;
    config.http2.port = 8443;
    
    // SSL/TLS is required for HTTP/2
    config.ssl.enabled = true;
    config.ssl.certificateFile = "/path/to/server.crt";
    config.ssl.privateKeyFile = "/path/to/server.key";
    
    cppSwitchboard::HttpServer server(config);
    
    server.get("/", [](const cppSwitchboard::HttpRequest& request) {
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody("Hello, HTTP/2!");
        response.setHeader("Content-Type", "text/plain");
        return response;
    });
    
    std::cout << "HTTP/2 server starting on https://localhost:8443" << std::endl;
    server.start();
    
    return 0;
}
```

### Generating SSL Certificates (Development)

```bash
# Generate self-signed certificate for development
openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes \
    -subj "/C=US/ST=State/L=City/O=Organization/CN=localhost"
```

## Configuration

### Using Configuration Files

**server.yaml**:
```yaml
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
  certificateFile: "/etc/ssl/certs/server.crt"
  privateKeyFile: "/etc/ssl/private/server.key"

general:
  maxConnections: 1000
  requestTimeout: 30
  enableLogging: true
  logLevel: "info"
  workerThreads: 4

security:
  enableCors: true
  corsOrigins: ["https://example.com", "https://app.example.com"]
  maxRequestSizeMb: 10
  rateLimitEnabled: true
  rateLimitRequestsPerMinute: 100
```

**Loading Configuration**:
```cpp
#include <cppSwitchboard/config.h>

int main() {
    // Load configuration from file
    auto config = cppSwitchboard::ConfigLoader::loadFromFile("server.yaml");
    if (!config) {
        std::cerr << "Failed to load configuration" << std::endl;
        return 1;
    }
    
    // Validate configuration
    std::string errorMessage;
    if (!cppSwitchboard::ConfigValidator::validateConfig(*config, errorMessage)) {
        std::cerr << "Configuration error: " << errorMessage << std::endl;
        return 1;
    }
    
    cppSwitchboard::HttpServer server(*config);
    // ... setup routes ...
    server.start();
    
    return 0;
}
```

### Environment Variables

Configuration values can use environment variable substitution:

```yaml
database:
  enabled: true
  host: "${DB_HOST:localhost}"
  port: ${DB_PORT:5432}
  username: "${DB_USER}"
  password: "${DB_PASSWORD}"
```

## Next Steps

Now that you have a basic server running, explore these advanced features:

1. **[Tutorials](TUTORIALS.md)** - Step-by-step guides for common tasks
2. **[Configuration Guide](CONFIGURATION.md)** - Detailed configuration options
3. **[Middleware Development](MIDDLEWARE.md)** - Creating custom middleware
4. **[Async Programming](ASYNC_PROGRAMMING.md)** - Asynchronous request handling
5. **[API Reference](API_REFERENCE.md)** - Complete API documentation

### Examples Repository

Check out the `examples/` directory for more complete examples:

- **REST API Server** - Full RESTful API with database integration
- **Static File Server** - Serving static files with caching
- **WebSocket Server** - Real-time communication
- **Microservice** - Production-ready microservice template
- **Load Balancer** - HTTP load balancer implementation

### Community and Support

- **Documentation**: [Full documentation](https://cppswitchboard.dev/docs)
- **Issues**: [GitHub Issues](https://github.com/your-org/qos-manager/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-org/qos-manager/discussions)
- **Examples**: See the `examples/` directory

Happy coding with cppSwitchboard! 🚀 