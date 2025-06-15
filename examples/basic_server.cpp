#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/http_handler.h>
#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>
#include <chrono>

using namespace cppSwitchboard;

// Simple handler class
class HelloHandler : public HttpHandler {
public:
    HttpResponse handle(const HttpRequest& request) override {
        return HttpResponse::json(R"({"message": "Hello from cppSwitchboard!", "protocol": ")" + 
                                 request.getProtocol() + R"("})");
    }
};

// Global server instance for signal handling
std::shared_ptr<HttpServer> globalServer;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (globalServer) {
        globalServer->stop();
    }
    exit(0);
}

int main() {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Create server with default configuration
        auto server = HttpServer::create();
        globalServer = server;
        
        std::cout << "cppSwitchboard Basic Example Server" << std::endl;
        std::cout << "====================================" << std::endl;
        
        // Register handlers
        server->get("/hello", std::make_shared<HelloHandler>());
        
        // Register lambda-based handler
        server->get("/", [](const HttpRequest& request) -> HttpResponse {
            return HttpResponse::html(R"(
<!DOCTYPE html>
<html>
<head>
    <title>cppSwitchboard Example</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; text-align: center; }
        .container { max-width: 600px; margin: 0 auto; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔀 cppSwitchboard</h1>
        <p>High-performance HTTP middleware framework for C++</p>
        <p><strong>Protocol:</strong> )" + request.getProtocol() + R"(</p>
        <p><strong>Stream ID:</strong> )" + std::to_string(request.getStreamId()) + R"(</p>
        <h3>Try these endpoints:</h3>
        <ul style="text-align: left;">
            <li><a href="/hello">/hello</a> - JSON response</li>
            <li><a href="/users/123">/users/123</a> - Path parameters</li>
            <li><a href="/config">/config</a> - Server configuration</li>
        </ul>
    </div>
</body>
</html>
            )");
        });
        
        // Handler with path parameters
        server->get("/users/{id}", [](const HttpRequest& request) -> HttpResponse {
            std::string userId = request.getPathParam("id");
            if (userId.empty()) {
                return HttpResponse::badRequest("User ID is required");
            }
            
            return HttpResponse::json(R"({"user_id": ")" + userId + 
                                    R"(", "name": "User )" + userId + 
                                    R"(", "protocol": ")" + request.getProtocol() + R"("})");
        });
        
        // Configuration endpoint
        server->get("/config", [&server](const HttpRequest& request) -> HttpResponse {
            const auto& config = server->getConfig();
            return HttpResponse::json(R"({
                "application": {
                    "name": ")" + config.application.name + R"(",
                    "version": ")" + config.application.version + R"(",
                    "environment": ")" + config.application.environment + R"("
                },
                "http1_enabled": )" + (config.http1.enabled ? "true" : "false") + R"(,
                "http2_enabled": )" + (config.http2.enabled ? "true" : "false") + R"(,
                "ssl_enabled": )" + (config.ssl.enabled ? "true" : "false") + R"(,
                "protocol": ")" + request.getProtocol() + R"("
            })");
        });
        
        // Start the server
        server->start();
        
        std::cout << "\nServer started! Available endpoints:" << std::endl;
        std::cout << "- http://localhost:8080/        (Welcome page)" << std::endl;
        std::cout << "- http://localhost:8080/hello   (JSON response)" << std::endl;
        std::cout << "- http://localhost:8080/users/123 (Path parameters)" << std::endl;
        std::cout << "- http://localhost:8080/config  (Configuration)" << std::endl;
        std::cout << "\nPress Ctrl+C to stop the server." << std::endl;
        
        // Keep running
        while (server->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 