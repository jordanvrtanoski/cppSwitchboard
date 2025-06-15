/**
 * @file auth_example.cpp
 * @brief Authentication and authorization middleware example
 * @author cppSwitchboard Development Team
 * @date 2025-01-08
 * @version 1.2.0
 * 
 * This example demonstrates JWT-based authentication and role-based authorization
 * using the cppSwitchboard middleware system. It includes token generation,
 * validation, and role-based access control.
 */

#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/middleware/auth_middleware.h>
#include <cppSwitchboard/middleware/authz_middleware.h>
#include <cppSwitchboard/middleware/logging_middleware.h>
#include <cppSwitchboard/middleware/cors_middleware.h>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <unordered_map>

using namespace cppSwitchboard;

// Global server instance for signal handling
std::shared_ptr<HttpServer> g_server;

// Simple in-memory user database for demonstration
struct User {
    std::string username;
    std::string password;  // In real applications, store hashed passwords!
    std::vector<std::string> roles;
};

std::unordered_map<std::string, User> g_users = {
    {"admin", {"admin", "admin123", {"admin", "user"}}},
    {"user", {"user", "user123", {"user"}}},
    {"guest", {"guest", "guest123", {"guest"}}}
};

// JWT secret for token signing (in production, use a secure random key!)
const std::string JWT_SECRET = "your-very-secure-secret-key-change-this-in-production";

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

/**
 * @brief Simple JWT token generator for demonstration
 * 
 * In a real application, you would use a proper JWT library like jwt-cpp.
 * This is a simplified implementation for demonstration purposes.
 */
std::string generateSimpleJWT(const std::string& username, const std::vector<std::string>& roles) {
    // This is a simplified JWT-like token for demonstration
    // In production, use a proper JWT library with proper base64url encoding and HMAC signing
    
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    
    std::string payload = R"({"sub":")" + username + R"(",)";
    payload += R"("iss":"cppSwitchboard-auth-example",)";
    payload += R"("aud":"api.example.com",)";
    
    // Add current time + 1 hour expiration
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(1);
    auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto exp_time = std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count();
    
    payload += R"("iat":)" + std::to_string(iat) + R"(,)";
    payload += R"("exp":)" + std::to_string(exp_time) + R"(,)";
    payload += R"("roles":[)";
    
    for (size_t i = 0; i < roles.size(); ++i) {
        payload += R"(")" + roles[i] + R"(")";
        if (i < roles.size() - 1) payload += ",";
    }
    payload += "]}";
    
    // Simple base64-like encoding (not real base64, just for demo)
    std::string token = "demo_" + username + "_" + std::to_string(iat);
    
    return token;
}

/**
 * @brief Setup authentication middleware
 */
void setupAuthenticationMiddleware(std::shared_ptr<HttpServer> server) {
    std::cout << "Setting up authentication middleware..." << std::endl;
    
    // CORS middleware (must be first for preflight requests)
    auto corsConfig = CorsMiddleware::CorsConfig::createDevelopmentConfig();
    corsConfig.allowedOrigins = {"*"};
    corsConfig.allowCredentials = true;
    corsConfig.allowedHeaders = {"Content-Type", "Authorization", "X-Requested-With"};
    auto corsMiddleware = std::make_shared<CorsMiddleware>(corsConfig);
    server->registerMiddleware(corsMiddleware);
    
    // Logging middleware
    LoggingMiddleware::LoggingConfig loggingConfig;
    loggingConfig.format = LoggingMiddleware::LogFormat::JSON;
    loggingConfig.includeHeaders = true;
    auto loggingMiddleware = std::make_shared<LoggingMiddleware>(loggingConfig);
    server->registerMiddleware(loggingMiddleware);
    
    // Custom JWT validation function
    auto tokenValidator = [](const std::string& token) -> AuthMiddleware::TokenValidationResult {
        AuthMiddleware::TokenValidationResult result;
        
        // Simple token validation (in production, use proper JWT library)
        if (token.substr(0, 5) == "demo_") {
            size_t first_underscore = token.find('_');
            size_t second_underscore = token.find('_', first_underscore + 1);
            
            if (first_underscore != std::string::npos && second_underscore != std::string::npos) {
                std::string username = token.substr(5, second_underscore - 5);
                
                auto userIt = g_users.find(username);
                if (userIt != g_users.end()) {
                    result.isValid = true;
                    result.userId = username;
                    result.roles = userIt->second.roles;
                    result.issuer = "cppSwitchboard-auth-example";
                    result.audience = "api.example.com";
                }
            }
        }
        
        if (!result.isValid) {
            result.errorMessage = "Invalid or expired token";
        }
        
        return result;
    };
    
    // Authentication middleware with custom validator
    auto authMiddleware = std::make_shared<AuthMiddleware>(tokenValidator, AuthMiddleware::AuthScheme::BEARER);
    authMiddleware->setIssuer("cppSwitchboard-auth-example");
    authMiddleware->setAudience("api.example.com");
    
    // Authorization middleware for admin routes
    AuthzMiddleware::AuthPolicy adminPolicy;
    adminPolicy.requiredRoles = {"admin"};
    adminPolicy.requireAllRoles = true;
    adminPolicy.description = "Requires admin role";
    
    auto authzMiddleware = std::make_shared<AuthzMiddleware>(std::vector<std::string>{"admin"}, true);
    authzMiddleware->addResourcePolicy("/api/admin/*", adminPolicy);
    
    std::cout << "Authentication middleware configured successfully!" << std::endl;
}

/**
 * @brief Register authentication routes
 */
void registerAuthRoutes(std::shared_ptr<HttpServer> server) {
    std::cout << "Registering authentication routes..." << std::endl;
    
    // Login endpoint (no authentication required)
    server->post("/api/auth/login", [](const HttpRequest& req) {
        std::string body = req.getBody();
        
        // Simple JSON parsing (in production, use proper JSON library)
        std::string username, password;
        
        // Extract username and password from JSON body
        size_t userPos = body.find("\"username\":");
        size_t passPos = body.find("\"password\":");
        
        if (userPos != std::string::npos && passPos != std::string::npos) {
            // Extract username
            size_t userStart = body.find("\"", userPos + 11) + 1;
            size_t userEnd = body.find("\"", userStart);
            username = body.substr(userStart, userEnd - userStart);
            
            // Extract password
            size_t passStart = body.find("\"", passPos + 11) + 1;
            size_t passEnd = body.find("\"", passStart);
            password = body.substr(passStart, passEnd - passStart);
        }
        
        // Validate credentials
        auto userIt = g_users.find(username);
        if (userIt != g_users.end() && userIt->second.password == password) {
            // Generate JWT token
            std::string token = generateSimpleJWT(username, userIt->second.roles);
            
            std::string response = R"({
                "success": true,
                "message": "Login successful",
                "token": ")" + token + R"(",
                "user": {
                    "username": ")" + username + R"(",
                    "roles": [)";
            
            for (size_t i = 0; i < userIt->second.roles.size(); ++i) {
                response += "\"" + userIt->second.roles[i] + "\"";
                if (i < userIt->second.roles.size() - 1) response += ",";
            }
            response += R"(]
                }
            })";
            
            return HttpResponse::json(response);
        } else {
            return HttpResponse(401, R"({
                "success": false,
                "message": "Invalid username or password"
            })");
        }
    });
    
    // User registration endpoint (simplified for demo)
    server->post("/api/auth/register", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "success": false,
            "message": "Registration not implemented in this demo. Use existing users: admin/admin123, user/user123, guest/guest123"
        })");
    });
    
    // Token validation endpoint
    server->get("/api/auth/validate", [](const HttpRequest& req) {
        // This route will be protected by authentication middleware
        return HttpResponse::json(R"({
            "success": true,
            "message": "Token is valid",
            "authenticated": true
        })");
    });
    
    // Logout endpoint (token invalidation would happen client-side in this demo)
    server->post("/api/auth/logout", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "success": true,
            "message": "Logged out successfully"
        })");
    });
    
    std::cout << "Authentication routes registered!" << std::endl;
}

/**
 * @brief Register protected API routes
 */
void registerProtectedRoutes(std::shared_ptr<HttpServer> server) {
    std::cout << "Registering protected routes..." << std::endl;
    
    // User profile endpoint (requires authentication)
    server->get("/api/user/profile", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "success": true,
            "profile": {
                "id": 123,
                "name": "John Doe",
                "email": "john.doe@example.com",
                "lastLogin": "2025-01-08T10:30:00Z"
            }
        })");
    });
    
    // User settings endpoint (requires authentication)
    server->put("/api/user/settings", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "success": true,
            "message": "Settings updated successfully"
        })");
    });
    
    // Admin dashboard (requires admin role)
    server->get("/api/admin/dashboard", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "success": true,
            "dashboard": {
                "totalUsers": 1000,
                "activeUsers": 750,
                "systemStatus": "healthy",
                "uptime": "99.9%"
            }
        })");
    });
    
    // Admin user management (requires admin role)
    server->get("/api/admin/users", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "success": true,
            "users": [
                {"id": 1, "username": "admin", "roles": ["admin", "user"], "active": true},
                {"id": 2, "username": "user", "roles": ["user"], "active": true},
                {"id": 3, "username": "guest", "roles": ["guest"], "active": false}
            ]
        })");
    });
    
    // Admin system settings (requires admin role)
    server->put("/api/admin/settings", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "success": true,
            "message": "System settings updated successfully"
        })");
    });
    
    std::cout << "Protected routes registered!" << std::endl;
}

/**
 * @brief Register public routes
 */
void registerPublicRoutes(std::shared_ptr<HttpServer> server) {
    std::cout << "Registering public routes..." << std::endl;
    
    // Root endpoint with authentication demo interface
    server->get("/", [](const HttpRequest& req) {
        return HttpResponse::html(R"(
<!DOCTYPE html>
<html>
<head>
    <title>cppSwitchboard Authentication Example</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .container { max-width: 800px; margin: 0 auto; }
        .auth-form { background: #f5f5f5; padding: 20px; border-radius: 5px; margin: 20px 0; }
        .endpoint { background: #e8f4f8; padding: 10px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #2196F3; }
        .protected { border-left-color: #ff9800; background: #fff3e0; }
        .admin { border-left-color: #f44336; background: #ffebee; }
        button { background: #2196F3; color: white; padding: 10px 15px; border: none; border-radius: 3px; cursor: pointer; margin: 5px; }
        button:hover { background: #1976D2; }
        input { padding: 8px; margin: 5px; border: 1px solid #ddd; border-radius: 3px; }
        .response { background: #f0f0f0; padding: 10px; margin: 10px 0; border-radius: 3px; font-family: monospace; white-space: pre-wrap; }
        .success { border-left: 4px solid #4caf50; }
        .error { border-left: 4px solid #f44336; }
    </style>
</head>
<body>
    <div class="container">
        <h1>cppSwitchboard Authentication Example</h1>
        <p>This example demonstrates JWT-based authentication and role-based authorization.</p>
        
        <div class="auth-form">
            <h2>Login</h2>
            <div>
                <input type="text" id="username" placeholder="Username" value="admin">
                <input type="password" id="password" placeholder="Password" value="admin123">
                <button onclick="login()">Login</button>
                <button onclick="logout()">Logout</button>
            </div>
            <p><strong>Test Accounts:</strong></p>
            <ul>
                <li><strong>admin/admin123</strong> - Admin user (full access)</li>
                <li><strong>user/user123</strong> - Regular user (limited access)</li>
                <li><strong>guest/guest123</strong> - Guest user (minimal access)</li>
            </ul>
            <div id="authStatus" class="response"></div>
        </div>
        
        <h2>Available Endpoints:</h2>
        
        <h3>Public Endpoints</h3>
        <div class="endpoint">
            <strong>POST /api/auth/login</strong> - User login
            <button onclick="testEndpoint('/api/auth/login', 'POST', {username: document.getElementById('username').value, password: document.getElementById('password').value})">Test</button>
        </div>
        <div class="endpoint">
            <strong>POST /api/auth/register</strong> - User registration (disabled in demo)
            <button onclick="testEndpoint('/api/auth/register', 'POST', {})">Test</button>
        </div>
        
        <h3>Protected Endpoints (Authentication Required)</h3>
        <div class="endpoint protected">
            <strong>GET /api/auth/validate</strong> - Validate token
            <button onclick="testEndpoint('/api/auth/validate', 'GET')">Test</button>
        </div>
        <div class="endpoint protected">
            <strong>GET /api/user/profile</strong> - User profile
            <button onclick="testEndpoint('/api/user/profile', 'GET')">Test</button>
        </div>
        <div class="endpoint protected">
            <strong>PUT /api/user/settings</strong> - Update user settings
            <button onclick="testEndpoint('/api/user/settings', 'PUT', {theme: 'dark'})">Test</button>
        </div>
        
        <h3>Admin Endpoints (Admin Role Required)</h3>
        <div class="endpoint admin">
            <strong>GET /api/admin/dashboard</strong> - Admin dashboard
            <button onclick="testEndpoint('/api/admin/dashboard', 'GET')">Test</button>
        </div>
        <div class="endpoint admin">
            <strong>GET /api/admin/users</strong> - List all users
            <button onclick="testEndpoint('/api/admin/users', 'GET')">Test</button>
        </div>
        <div class="endpoint admin">
            <strong>PUT /api/admin/settings</strong> - Update system settings
            <button onclick="testEndpoint('/api/admin/settings', 'PUT', {maintenance: false})">Test</button>
        </div>
        
        <div id="response" class="response"></div>
    </div>
    
    <script>
        let authToken = localStorage.getItem('authToken') || '';
        
        function updateAuthStatus() {
            const statusDiv = document.getElementById('authStatus');
            if (authToken) {
                statusDiv.innerHTML = 'Status: Authenticated\nToken: ' + authToken.substring(0, 20) + '...';
                statusDiv.className = 'response success';
            } else {
                statusDiv.innerHTML = 'Status: Not authenticated';
                statusDiv.className = 'response error';
            }
        }
        
        function login() {
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            
            fetch('/api/auth/login', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({username, password})
            })
            .then(response => response.json())
            .then(data => {
                if (data.success && data.token) {
                    authToken = data.token;
                    localStorage.setItem('authToken', authToken);
                    updateAuthStatus();
                    showResponse('Login successful!', JSON.stringify(data, null, 2), true);
                } else {
                    showResponse('Login failed!', JSON.stringify(data, null, 2), false);
                }
            })
            .catch(error => {
                showResponse('Error', error.toString(), false);
            });
        }
        
        function logout() {
            authToken = '';
            localStorage.removeItem('authToken');
            updateAuthStatus();
            showResponse('Logged out', 'Token cleared from browser', true);
        }
        
        function testEndpoint(url, method, body = null) {
            const options = {
                method: method,
                headers: {'Content-Type': 'application/json'}
            };
            
            if (authToken) {
                options.headers['Authorization'] = 'Bearer ' + authToken;
            }
            
            if (body && (method === 'POST' || method === 'PUT')) {
                options.body = JSON.stringify(body);
            }
            
            fetch(url, options)
            .then(response => {
                const success = response.ok;
                return response.text().then(text => ({
                    status: response.status,
                    statusText: response.statusText,
                    body: text,
                    success: success
                }));
            })
            .then(data => {
                showResponse(
                    method + ' ' + url + ' (' + data.status + ' ' + data.statusText + ')',
                    data.body,
                    data.success
                );
            })
            .catch(error => {
                showResponse('Error', error.toString(), false);
            });
        }
        
        function showResponse(title, content, success) {
            const responseDiv = document.getElementById('response');
            responseDiv.innerHTML = title + ':\n' + content;
            responseDiv.className = 'response ' + (success ? 'success' : 'error');
        }
        
        // Initialize
        updateAuthStatus();
    </script>
</body>
</html>
        )");
    });
    
    // API status endpoint
    server->get("/api/status", [](const HttpRequest& req) {
        return HttpResponse::json(R"({
            "status": "ok",
            "service": "cppSwitchboard Authentication Example",
            "version": "1.2.0",
            "features": ["JWT Authentication", "Role-based Authorization", "Middleware Pipeline"]
        })");
    });
    
    std::cout << "Public routes registered!" << std::endl;
}

/**
 * @brief Print usage instructions
 */
void printUsageInstructions() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "cppSwitchboard Authentication Example Server" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "\nThis example demonstrates:\n" << std::endl;
    
    std::cout << "1. JWT-based authentication with custom token validation" << std::endl;
    std::cout << "2. Role-based authorization (admin, user, guest)" << std::endl;
    std::cout << "3. Authentication middleware configuration" << std::endl;
    std::cout << "4. Protected route access control" << std::endl;
    std::cout << "5. Context propagation of user information" << std::endl;
    
    std::cout << "\nTest accounts:" << std::endl;
    std::cout << "  • admin/admin123 - Full admin access" << std::endl;
    std::cout << "  • user/user123 - Regular user access" << std::endl;
    std::cout << "  • guest/guest123 - Limited guest access" << std::endl;
    
    std::cout << "\nAccess the demo:" << std::endl;
    std::cout << "  • Open http://localhost:8080/ in your browser" << std::endl;
    std::cout << "  • Try logging in with different accounts" << std::endl;
    std::cout << "  • Test protected endpoints with different roles" << std::endl;
    
    std::cout << "\nPress Ctrl+C to stop the server." << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;
}

int main() {
    try {
        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        std::cout << "Starting cppSwitchboard Authentication Example..." << std::endl;
        
        // Create server with default configuration
        auto config = ServerConfig{};
        config.http1.enabled = true;
        config.http1.port = 8080;
        config.http1.bindAddress = "0.0.0.0";
        
        g_server = HttpServer::create(config);
        
        // Setup middleware
        setupAuthenticationMiddleware(g_server);
        
        // Register routes
        registerPublicRoutes(g_server);
        registerAuthRoutes(g_server);
        registerProtectedRoutes(g_server);
        
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