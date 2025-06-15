/**
 * @file middleware_example.cpp
 * @brief Comprehensive middleware pipeline example
 * @author cppSwitchboard Development Team
 * @date 2025-01-08
 * @version 1.2.0
 * 
 * This example demonstrates how to use the middleware system in cppSwitchboard
 * with configuration-driven middleware composition, built-in middleware, and
 * custom middleware implementation.
 */

#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/middleware_config.h>
#include <cppSwitchboard/middleware_factory.h>
#include <cppSwitchboard/middleware/cors_middleware.h>
#include <cppSwitchboard/middleware/logging_middleware.h>
#include <cppSwitchboard/middleware/auth_middleware.h>
#include <cppSwitchboard/middleware/rate_limit_middleware.h>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

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
 * @brief Create sample YAML configuration for middleware
 * 
 * This demonstrates how to configure middleware pipelines using YAML.
 * In a real application, this would typically be loaded from a configuration file.
 */
std::string createMiddlewareConfig() {
    return R"(
middleware:
  # Global middleware applied to all routes
  global:
    - name: "cors"
      enabled: true
      priority: 200
      config:
        origins: ["*"]
        methods: ["GET", "POST", "PUT", "DELETE", "OPTIONS"]
        headers: ["Content-Type", "Authorization", "X-Requested-With"]
        credentials: false
        max_age: 86400
        
    - name: "logging"
      enabled: true
      priority: 10
      config:
        format: "json"
        include_headers: true
        include_body: false
        max_body_size: 1024
        
  # Route-specific middleware
  routes:
    # Public API with rate limiting
    "/api/public/*":
      - name: "rate_limit"
        enabled: true
        priority: 80
        config:
          strategy: "ip_based"
          max_tokens: 100
          refill_rate: 10
          refill_window: "second"
          
    # Protected API with authentication and stricter rate limiting
    "/api/v1/*":
      - name: "auth"
        enabled: true
        priority: 100
        config:
          type: "jwt"
          secret: "your-secret-key-here"
          issuer: "cppSwitchboard-example"
          audience: "api.example.com"
          
      - name: "rate_limit"
        enabled: true
        priority: 80
        config:
          strategy: "user_based"
          max_tokens: 1000
          refill_rate: 100
          refill_window: "minute"
          
    # Admin routes with authorization
    "/api/admin/*":
      - name: "auth"
        enabled: true
        priority: 100
        config:
          type: "jwt"
          secret: "your-secret-key-here"
          
      - name: "authz"
        enabled: true
        priority: 90
        config:
          required_roles: ["admin"]
          require_all_roles: true
)";
}

/**
 * @brief Example of programmatic middleware registration
 * 
 * This shows how to register middleware programmatically without YAML configuration.
 */
void registerMiddlewareProgrammatically(std::shared_ptr<HttpServer> server) {
    std::cout << "Registering middleware programmatically..." << std::endl;
    
    // CORS middleware for cross-origin requests
    auto corsConfig = CorsMiddleware::CorsConfig::createDevelopmentConfig();
    corsConfig.allowedOrigins = {"http://localhost:3000", "https://example.com"};
    corsConfig.allowCredentials = true;
    auto corsMiddleware = std::make_shared<CorsMiddleware>(corsConfig);
    server->registerMiddleware(corsMiddleware);
    
    // Logging middleware for request/response tracking
    LoggingMiddleware::LoggingConfig loggingConfig;
    loggingConfig.format = LoggingMiddleware::LogFormat::JSON;
    loggingConfig.includeHeaders = true;
    loggingConfig.includeTimings = true;
    auto loggingMiddleware = std::make_shared<LoggingMiddleware>(loggingConfig);
    server->registerMiddleware(loggingMiddleware);
    
    std::cout << "Middleware registered successfully!" << std::endl;
}

/**
 * @brief Example of configuration-driven middleware setup
 * 
 * This demonstrates loading middleware configuration from YAML and applying it to the server.
 */
void setupConfigurationDrivenMiddleware(std::shared_ptr<HttpServer> server) {
    std::cout << "Setting up configuration-driven middleware..." << std::endl;
    
    // Load middleware configuration
    MiddlewareConfigLoader loader;
    std::string yamlConfig = createMiddlewareConfig();
    auto result = loader.loadFromString(yamlConfig);
    
    if (!result.isSuccess()) {
        std::cerr << "Failed to load middleware configuration: " << result.message << std::endl;
        return;
    }
    
    const auto& config = loader.getConfiguration();
    MiddlewareFactory& factory = MiddlewareFactory::getInstance();
    
    // Apply global middleware
    std::cout << "Applying global middleware..." << std::endl;
    for (const auto& middlewareConfig : config.global.middlewares) {
        if (middlewareConfig.enabled) {
            auto middleware = factory.createMiddleware(middlewareConfig);
            if (middleware) {
                server->registerMiddleware(middleware);
                std::cout << "  - Registered: " << middlewareConfig.name 
                         << " (priority: " << middlewareConfig.priority << ")" << std::endl;
            } else {
                std::cerr << "  - Failed to create middleware: " << middlewareConfig.name << std::endl;
            }
        }
    }
    
    // Apply route-specific middleware
    std::cout << "Applying route-specific middleware..." << std::endl;
    for (const auto& routeConfig : config.routes) {
        auto pipeline = factory.createPipeline(routeConfig.middlewares);
        if (pipeline) {
            // Register routes for different HTTP methods
            server->registerRouteWithMiddleware(routeConfig.pattern, HttpMethod::GET, pipeline);
            server->registerRouteWithMiddleware(routeConfig.pattern, HttpMethod::POST, pipeline);
            server->registerRouteWithMiddleware(routeConfig.pattern, HttpMethod::PUT, pipeline);
            server->registerRouteWithMiddleware(routeConfig.pattern, HttpMethod::DELETE, pipeline);
            
            std::cout << "  - Route: " << routeConfig.pattern 
                     << " (" << routeConfig.middlewares.size() << " middleware)" << std::endl;
        }
    }
    
    std::cout << "Configuration-driven middleware setup complete!" << std::endl;
}

/**
 * @brief Register sample API routes for testing middleware
 */
void registerApiRoutes(std::shared_ptr<HttpServer> server) {
    std::cout << "Registering API routes..." << std::endl;
    
    // Public API routes
    server->get("/api/public/status", [](const HttpRequest& req) {
        return HttpResponse::json(R"({"status": "ok", "endpoint": "public"})");
    });
    
    server->get("/api/public/info", [](const HttpRequest& req) {
        return HttpResponse::json(R"({"name": "cppSwitchboard", "version": "1.2.0", "middleware": "enabled"})");
    });
    
    // Protected API routes
    server->get("/api/v1/user/profile", [](const HttpRequest& req) {
        return HttpResponse::json(R"({"user": "authenticated", "profile": "data"})");
    });
    
    server->post("/api/v1/data", [](const HttpRequest& req) {
        return HttpResponse::json(R"({"message": "Data created", "id": 123})");
    });
    
    // Admin routes
    server->get("/api/admin/users", [](const HttpRequest& req) {
        return HttpResponse::json(R"({"users": [{"id": 1, "name": "admin"}]})");
    });
    
    server->get("/api/admin/stats", [](const HttpRequest& req) {
        return HttpResponse::json(R"({"requests": 1000, "errors": 5, "uptime": "24h"})");
    });
    
    // Root endpoint
    server->get("/", [](const HttpRequest& req) {
        return HttpResponse::html(R"(
<!DOCTYPE html>
<html>
<head>
    <title>cppSwitchboard Middleware Example</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .endpoint { background: #f0f0f0; padding: 10px; margin: 10px 0; border-radius: 5px; }
        .protected { border-left: 4px solid #ff6b6b; }
        .public { border-left: 4px solid #4ecdc4; }
        .admin { border-left: 4px solid #ffe66d; }
    </style>
</head>
<body>
    <h1>cppSwitchboard Middleware Example</h1>
    <p>This server demonstrates comprehensive middleware functionality.</p>
    
    <h2>Available Endpoints:</h2>
    
    <h3>Public API (Rate Limited)</h3>
    <div class="endpoint public">GET /api/public/status - Server status</div>
    <div class="endpoint public">GET /api/public/info - Server information</div>
    
    <h3>Protected API (Authentication Required)</h3>
    <div class="endpoint protected">GET /api/v1/user/profile - User profile</div>
    <div class="endpoint protected">POST /api/v1/data - Create data</div>
    
    <h3>Admin API (Authorization Required)</h3>
    <div class="endpoint admin">GET /api/admin/users - List users</div>
    <div class="endpoint admin">GET /api/admin/stats - Server statistics</div>
    
    <h2>Middleware Active:</h2>
    <ul>
        <li><strong>CORS</strong> - Cross-Origin Resource Sharing</li>
        <li><strong>Logging</strong> - Request/Response logging</li>
        <li><strong>Authentication</strong> - JWT token validation</li>
        <li><strong>Authorization</strong> - Role-based access control</li>
        <li><strong>Rate Limiting</strong> - Request throttling</li>
    </ul>
    
    <p><em>Try the endpoints above to see middleware in action!</em></p>
</body>
</html>
        )");
    });
    
    std::cout << "API routes registered successfully!" << std::endl;
}

/**
 * @brief Print usage instructions
 */
void printUsageInstructions() {
    std::cout << "\n" << "="*60 << std::endl;
    std::cout << "cppSwitchboard Middleware Example Server" << std::endl;
    std::cout << "="*60 << std::endl;
    std::cout << "\nThis example demonstrates:\n" << std::endl;
    
    std::cout << "1. Configuration-driven middleware setup" << std::endl;
    std::cout << "2. Built-in middleware types (CORS, Logging, Auth, Rate Limiting)" << std::endl;
    std::cout << "3. Route-specific middleware pipelines" << std::endl;
    std::cout << "4. Priority-based middleware execution" << std::endl;
    std::cout << "5. Context propagation between middleware" << std::endl;
    
    std::cout << "\nAvailable endpoints:" << std::endl;
    std::cout << "  • http://localhost:8080/ - Main page with documentation" << std::endl;
    std::cout << "  • http://localhost:8080/api/public/* - Public API (rate limited)" << std::endl;
    std::cout << "  • http://localhost:8080/api/v1/* - Protected API (auth required)" << std::endl;
    std::cout << "  • http://localhost:8080/api/admin/* - Admin API (admin role required)" << std::endl;
    
    std::cout << "\nTesting tips:" << std::endl;
    std::cout << "  • Use curl or Postman to test endpoints" << std::endl;
    std::cout << "  • Check server logs to see middleware execution" << std::endl;
    std::cout << "  • Try rapid requests to test rate limiting" << std::endl;
    std::cout << "  • Add 'Authorization: Bearer <token>' for protected endpoints" << std::endl;
    
    std::cout << "\nPress Ctrl+C to stop the server." << std::endl;
    std::cout << "="*60 << "\n" << std::endl;
}

int main() {
    try {
        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        std::cout << "Starting cppSwitchboard Middleware Example..." << std::endl;
        
        // Create server with default configuration
        auto config = ServerConfig{};
        config.http1.enabled = true;
        config.http1.port = 8080;
        config.http1.bindAddress = "0.0.0.0";
        
        g_server = HttpServer::create(config);
        
        // Choose middleware setup method
        std::cout << "\nChoose middleware setup method:" << std::endl;
        std::cout << "1. Configuration-driven (YAML)" << std::endl;
        std::cout << "2. Programmatic registration" << std::endl;
        std::cout << "Enter choice (1 or 2): ";
        
        int choice;
        std::cin >> choice;
        
        if (choice == 1) {
            setupConfigurationDrivenMiddleware(g_server);
        } else {
            registerMiddlewareProgrammatically(g_server);
        }
        
        // Register API routes
        registerApiRoutes(g_server);
        
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