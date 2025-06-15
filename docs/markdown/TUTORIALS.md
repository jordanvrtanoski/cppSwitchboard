# cppSwitchboard Tutorials

Step-by-step guides for common use cases and advanced features of cppSwitchboard.

## Table of Contents

1. [Hello World HTTP Server](#tutorial-1-hello-world-http-server)
2. [RESTful API with JSON](#tutorial-2-restful-api-with-json)
3. [Static File Server](#tutorial-3-static-file-server)
4. [HTTP/2 Server with SSL](#tutorial-4-http2-server-with-ssl)
5. [Custom Middleware Development](#tutorial-5-custom-middleware-development)

---

## Tutorial 1: Hello World HTTP Server

**Goal**: Create a basic HTTP server that responds to requests.

### Step 1: Project Setup

```bash
mkdir hello-server && cd hello-server
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.16)
project(HelloServer)
set(CMAKE_CXX_STANDARD 17)
find_package(cppSwitchboard REQUIRED)
add_executable(hello_server main.cpp)
target_link_libraries(hello_server cppSwitchboard::cppSwitchboard)
```

### Step 2: Basic Server Implementation

**main.cpp**:
```cpp
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/config.h>
#include <iostream>

int main() {
    cppSwitchboard::ServerConfig config;
    config.http1.port = 8080;
    config.general.enableLogging = true;
    
    cppSwitchboard::HttpServer server(config);
    
    server.get("/", [](const cppSwitchboard::HttpRequest& request) {
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody("Hello, World!");
        response.setHeader("Content-Type", "text/plain");
        return response;
    });
    
    std::cout << "Server starting on http://localhost:8080" << std::endl;
    server.start();
    return 0;
}
```

### Step 3: Build and Test

```bash
mkdir build && cd build
cmake ..
make
./hello_server
```

Test: `curl http://localhost:8080/`

---

## Tutorial 2: RESTful API with JSON

**Goal**: Build a RESTful API for managing users.

### Step 1: User Service

**user.h**:
```cpp
#pragma once
#include <string>
#include <nlohmann/json.hpp>

struct User {
    int id;
    std::string name;
    std::string email;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(User, id, name, email)
};
```

### Step 2: REST Endpoints

**main.cpp**:
```cpp
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/config.h>
#include "user.h"
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

int main() {
    cppSwitchboard::ServerConfig config;
    config.http1.port = 8080;
    
    cppSwitchboard::HttpServer server(config);
    std::vector<User> users;
    int nextId = 1;
    
    // GET /users
    server.get("/users", [&users](const cppSwitchboard::HttpRequest& request) {
        json response_json = users;
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody(response_json.dump());
        response.setHeader("Content-Type", "application/json");
        return response;
    });
    
    // POST /users
    server.post("/users", [&users, &nextId](const cppSwitchboard::HttpRequest& request) {
        try {
            json request_json = json::parse(request.getBody());
            User newUser{nextId++, request_json["name"], request_json["email"]};
            users.push_back(newUser);
            
            json response_json = newUser;
            cppSwitchboard::HttpResponse response;
            response.setStatusCode(201);
            response.setBody(response_json.dump());
            response.setHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            cppSwitchboard::HttpResponse response;
            response.setStatusCode(400);
            response.setBody(R"({"error": "Invalid JSON"})");
            response.setHeader("Content-Type", "application/json");
            return response;
        }
    });
    
    server.start();
    return 0;
}
```

### Step 3: Testing

```bash
# Create user
curl -X POST http://localhost:8080/users \
     -H "Content-Type: application/json" \
     -d '{"name": "John", "email": "john@example.com"}'

# Get users
curl http://localhost:8080/users
```

---

## Tutorial 3: Static File Server

**Goal**: Serve static files with proper MIME types and caching.

```cpp
#include <cppSwitchboard/http_server.h>
#include <filesystem>
#include <fstream>

std::string getMimeType(const std::string& extension) {
    static const std::map<std::string, std::string> mimeTypes = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"}
    };
    auto it = mimeTypes.find(extension);
    return (it != mimeTypes.end()) ? it->second : "application/octet-stream";
}

int main() {
    cppSwitchboard::ServerConfig config;
    config.http1.port = 8080;
    
    cppSwitchboard::HttpServer server(config);
    const std::string webRoot = "./public";
    
    server.get("/*", [webRoot](const cppSwitchboard::HttpRequest& request) {
        std::string path = request.getPath();
        if (path == "/") path = "/index.html";
        
        std::string fullPath = webRoot + path;
        cppSwitchboard::HttpResponse response;
        
        if (std::filesystem::exists(fullPath)) {
            std::ifstream file(fullPath, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            
            std::string extension = std::filesystem::path(fullPath).extension();
            response.setStatusCode(200);
            response.setBody(content);
            response.setHeader("Content-Type", getMimeType(extension));
            response.setHeader("Cache-Control", "public, max-age=3600");
        } else {
            response.setStatusCode(404);
            response.setBody("File not found");
        }
        return response;
    });
    
    server.start();
    return 0;
}
```

---

## Tutorial 4: HTTP/2 Server with SSL

**Goal**: Set up HTTP/2 with SSL/TLS encryption.

### Step 1: Generate SSL Certificate

```bash
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes \
    -subj "/C=US/ST=CA/L=SF/O=MyOrg/CN=localhost"
```

### Step 2: HTTP/2 Server

```cpp
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/config.h>

int main() {
    cppSwitchboard::ServerConfig config;
    
    // HTTP/2 configuration
    config.http2.enabled = true;
    config.http2.port = 8443;
    
    // SSL configuration
    config.ssl.enabled = true;
    config.ssl.certificateFile = "server.crt";
    config.ssl.privateKeyFile = "server.key";
    
    cppSwitchboard::HttpServer server(config);
    
    server.get("/", [](const cppSwitchboard::HttpRequest& request) {
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody("Hello from HTTP/2!");
        response.setHeader("Content-Type", "text/plain");
        return response;
    });
    
    std::cout << "HTTP/2 server starting on https://localhost:8443" << std::endl;
    server.start();
    return 0;
}
```

### Step 3: Testing

```bash
curl --http2 --insecure https://localhost:8443/
```

---

## Tutorial 5: Custom Middleware Development

**Goal**: Create authentication and logging middleware.

### Step 1: Authentication Middleware

```cpp
class AuthMiddleware : public cppSwitchboard::Middleware {
public:
    void process(const cppSwitchboard::HttpRequest& request, 
                cppSwitchboard::HttpResponse& response, 
                NextCallback next) override {
        
        if (request.getPath() == "/" || request.getPath() == "/login") {
            next();  // Skip auth for public routes
            return;
        }
        
        std::string authHeader = request.getHeader("Authorization");
        if (authHeader.empty() || authHeader != "Bearer valid-token") {
            response.setStatusCode(401);
            response.setBody(R"({"error": "Unauthorized"})");
            response.setHeader("Content-Type", "application/json");
            return;
        }
        
        next();  // Continue processing
    }
};
```

### Step 2: Using Middleware

```cpp
int main() {
    cppSwitchboard::ServerConfig config;
    config.http1.port = 8080;
    
    cppSwitchboard::HttpServer server(config);
    
    // Add middleware
    auto authMiddleware = std::make_shared<AuthMiddleware>();
    server.use(authMiddleware);
    
    // Public route
    server.get("/", [](const cppSwitchboard::HttpRequest& request) {
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody("Public page");
        return response;
    });
    
    // Protected route
    server.get("/protected", [](const cppSwitchboard::HttpRequest& request) {
        cppSwitchboard::HttpResponse response;
        response.setStatusCode(200);
        response.setBody("Protected resource");
        return response;
    });
    
    server.start();
    return 0;
}
```

### Step 3: Testing

```bash
# Public route (should work)
curl http://localhost:8080/

# Protected route without auth (should fail)
curl http://localhost:8080/protected

# Protected route with auth (should work)
curl -H "Authorization: Bearer valid-token" http://localhost:8080/protected
```

---

## Next Steps

Explore more advanced features:
- Asynchronous request handling
- Database integration
- WebSocket support
- Production deployment
- Performance optimization

For complete examples, see the `examples/` directory in the repository. 