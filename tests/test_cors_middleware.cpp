/**
 * @file test_cors_middleware.cpp
 * @brief Unit tests for CorsMiddleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#include <gtest/gtest.h>
#include <cppSwitchboard/middleware/cors_middleware.h>
#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <chrono>
#include <thread>

using namespace cppSwitchboard;
using namespace cppSwitchboard::middleware;

class CorsMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create basic request
        request_ = HttpRequest("GET", "/api/users", "HTTP/1.1");
        request_.setHeader("Origin", "https://example.com");
        request_.setHeader("User-Agent", "TestAgent/1.0");
        
        // Create basic response
        response_.setStatus(200);
        response_.setBody("{\"users\": []}");
        response_.setHeader("Content-Type", "application/json");
    }

    HttpRequest request_;
    HttpResponse response_;
    Context context_;
};

// Test 1: Basic Interface Tests
TEST_F(CorsMiddlewareTest, BasicInterface) {
    CorsMiddleware middleware;
    
    EXPECT_EQ(middleware.getName(), "CorsMiddleware");
    EXPECT_EQ(middleware.getPriority(), -10);
    EXPECT_TRUE(middleware.isEnabled());
    
    middleware.setEnabled(false);
    EXPECT_FALSE(middleware.isEnabled());
}

// Test 2: Default Configuration
TEST_F(CorsMiddlewareTest, DefaultConfiguration) {
    CorsMiddleware middleware;
    
    // Default should be permissive
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    // Should have CORS headers
    EXPECT_FALSE(result.getHeader("Access-Control-Allow-Origin").empty());
}

// Test 3: Allow All Origins
TEST_F(CorsMiddlewareTest, AllowAllOrigins) {
    CorsMiddleware::CorsConfig config;
    config.allowAllOrigins = true;
    config.allowedOrigins = {"*"};
    config.allowCredentials = false;  // Must be false to use "*"
    
    CorsMiddleware middleware(config);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    EXPECT_EQ(result.getHeader("Access-Control-Allow-Origin"), "*");
}

// Test 4: Specific Origins
TEST_F(CorsMiddlewareTest, SpecificOrigins) {
    CorsMiddleware::CorsConfig config;
    config.allowedOrigins = {"https://example.com", "https://app.example.com"};
    config.allowAllOrigins = false;
    
    CorsMiddleware middleware(config);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    EXPECT_EQ(result.getHeader("Access-Control-Allow-Origin"), "https://example.com");
}

// Test 5: Blocked Origin
TEST_F(CorsMiddlewareTest, BlockedOrigin) {
    CorsMiddleware::CorsConfig config;
    config.allowedOrigins = {"https://allowed.com"};
    config.allowAllOrigins = false;
    
    CorsMiddleware middleware(config);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    // Should not have CORS headers for blocked origin
    EXPECT_TRUE(result.getHeader("Access-Control-Allow-Origin").empty());
}

// Test 6: Preflight Request Handling
TEST_F(CorsMiddlewareTest, PreflightRequest) {
    CorsMiddleware::CorsConfig config;
    config.allowedOrigins = {"https://example.com"};
    config.allowedMethods = {"GET", "POST", "PUT"};
    config.allowedHeaders = {"Content-Type", "Authorization"};
    config.allowAllOrigins = false;
    
    CorsMiddleware middleware(config);
    
    // Create preflight request
    HttpRequest preflightRequest = HttpRequest("OPTIONS", "/api/users", "HTTP/1.1");
    preflightRequest.setHeader("Origin", "https://example.com");
    preflightRequest.setHeader("Access-Control-Request-Method", "POST");
    preflightRequest.setHeader("Access-Control-Request-Headers", "Content-Type");
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(preflightRequest, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    EXPECT_EQ(result.getHeader("Access-Control-Allow-Origin"), "https://example.com");
    EXPECT_NE(result.getHeader("Access-Control-Allow-Methods").find("POST"), std::string::npos);
    EXPECT_NE(result.getHeader("Access-Control-Allow-Headers").find("Content-Type"), std::string::npos);
    EXPECT_FALSE(result.getHeader("Access-Control-Max-Age").empty());
    EXPECT_TRUE(result.getBody().empty()); // Preflight response should have no body
}

// Test 7: Blocked Preflight Method
TEST_F(CorsMiddlewareTest, BlockedPreflightMethod) {
    CorsMiddleware::CorsConfig config;
    config.allowedOrigins = {"https://example.com"};
    config.allowedMethods = {"GET", "POST"};
    config.allowAllOrigins = false;
    
    CorsMiddleware middleware(config);
    
    // Create preflight request with blocked method
    HttpRequest preflightRequest = HttpRequest("OPTIONS", "/api/users", "HTTP/1.1");
    preflightRequest.setHeader("Origin", "https://example.com");
    preflightRequest.setHeader("Access-Control-Request-Method", "DELETE");
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(preflightRequest, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 403);
    EXPECT_EQ(result.getBody(), "Method not allowed");
}

// Test 8: Blocked Preflight Headers
TEST_F(CorsMiddlewareTest, BlockedPreflightHeaders) {
    CorsMiddleware::CorsConfig config;
    config.allowedOrigins = {"https://example.com"};
    config.allowedMethods = {"GET", "POST"};
    config.allowedHeaders = {"Content-Type"};
    config.allowAllHeaders = false;
    config.allowAllOrigins = false;
    
    CorsMiddleware middleware(config);
    
    // Create preflight request with blocked headers
    HttpRequest preflightRequest = HttpRequest("OPTIONS", "/api/users", "HTTP/1.1");
    preflightRequest.setHeader("Origin", "https://example.com");
    preflightRequest.setHeader("Access-Control-Request-Method", "POST");
    preflightRequest.setHeader("Access-Control-Request-Headers", "Authorization, X-Custom-Header");
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(preflightRequest, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 403);
    EXPECT_EQ(result.getBody(), "Headers not allowed");
}

// Test 9: Credentials Support
TEST_F(CorsMiddlewareTest, CredentialsSupport) {
    CorsMiddleware::CorsConfig config;
    config.allowedOrigins = {"https://example.com"};
    config.allowCredentials = true;
    config.allowAllOrigins = false;
    config.reflectOrigin = true;
    
    CorsMiddleware middleware(config);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    EXPECT_EQ(result.getHeader("Access-Control-Allow-Origin"), "https://example.com");
    EXPECT_EQ(result.getHeader("Access-Control-Allow-Credentials"), "true");
}

// Test 10: Exposed Headers
TEST_F(CorsMiddlewareTest, ExposedHeaders) {
    CorsMiddleware::CorsConfig config;
    config.allowedOrigins = {"https://example.com"};
    config.exposedHeaders = {"X-Total-Count", "X-Page-Count"};
    config.allowAllOrigins = false;
    
    CorsMiddleware middleware(config);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    std::string exposedHeaders = result.getHeader("Access-Control-Expose-Headers");
    EXPECT_NE(exposedHeaders.find("X-Total-Count"), std::string::npos);
    EXPECT_NE(exposedHeaders.find("X-Page-Count"), std::string::npos);
}

// Test 11: Vary Origin Header
TEST_F(CorsMiddlewareTest, VaryOriginHeader) {
    CorsMiddleware::CorsConfig config;
    config.allowedOrigins = {"https://example.com"};
    config.varyOrigin = true;
    config.allowAllOrigins = false;
    
    CorsMiddleware middleware(config);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        HttpResponse resp = response_;
        resp.setHeader("Vary", "Accept-Encoding");
        return resp;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    std::string varyHeader = result.getHeader("Vary");
    EXPECT_NE(varyHeader.find("Origin"), std::string::npos);
    EXPECT_NE(varyHeader.find("Accept-Encoding"), std::string::npos);
}

// Test 12: Configuration Methods
TEST_F(CorsMiddlewareTest, ConfigurationMethods) {
    CorsMiddleware middleware;
    
    // Test origin management
    middleware.addAllowedOrigin("https://test.com");
    middleware.removeAllowedOrigin("https://test.com");
    
    // Test method management
    middleware.addAllowedMethod("PATCH");
    middleware.removeAllowedMethod("PATCH");
    
    // Test header management
    middleware.addAllowedHeader("X-Custom-Header");
    middleware.removeAllowedHeader("X-Custom-Header");
    
    // Test exposed header management
    middleware.addExposedHeader("X-Response-Time");
    middleware.removeExposedHeader("X-Response-Time");
    
    // Test credentials
    middleware.setAllowCredentials(true);
    EXPECT_TRUE(middleware.getAllowCredentials());
    
    // Test max age
    middleware.setMaxAge(3600);
    EXPECT_EQ(middleware.getMaxAge(), 3600);
    
    // Test preflight handling
    middleware.setHandlePreflight(false);
    EXPECT_FALSE(middleware.getHandlePreflight());
    
    // Test allow all flags
    middleware.setAllowAllOrigins(true);
    middleware.setAllowAllMethods(true);
    middleware.setAllowAllHeaders(true);
}

// Test 13: Custom Origin Validator
TEST_F(CorsMiddlewareTest, CustomOriginValidator) {
    CorsMiddleware::CorsConfig config;
    
    // Custom validator that allows only origins ending with ".trusted.com"
    auto validator = [](const std::string& origin) -> bool {
        return origin.find(".trusted.com") != std::string::npos;
    };
    
    CorsMiddleware middleware(config, validator);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    // Test allowed origin
    HttpRequest allowedRequest;
    allowedRequest = HttpRequest("GET", "/api/test", "HTTP/1.1");
    allowedRequest.setHeader("Origin", "https://app.trusted.com");
    
    HttpResponse result1 = middleware.handle(allowedRequest, context_, nextHandler);
    EXPECT_EQ(result1.getStatus(), 200);
    EXPECT_FALSE(result1.getHeader("Access-Control-Allow-Origin").empty());
    
    // Test blocked origin
    HttpRequest blockedRequest;
    blockedRequest = HttpRequest("GET", "/api/test", "HTTP/1.1");
    blockedRequest.setHeader("Origin", "https://malicious.com");
    
    HttpResponse result2 = middleware.handle(blockedRequest, context_, nextHandler);
    EXPECT_EQ(result2.getStatus(), 200);
    EXPECT_TRUE(result2.getHeader("Access-Control-Allow-Origin").empty());
}

// Test 14: No Origin Header
TEST_F(CorsMiddlewareTest, NoOriginHeader) {
    CorsMiddleware middleware;
    
    // Request without Origin header
    HttpRequest noOriginRequest;
    noOriginRequest = HttpRequest("GET", "/api/test", "HTTP/1.1");
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(noOriginRequest, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    // Should not add CORS headers when no Origin is present
    EXPECT_TRUE(result.getHeader("Access-Control-Allow-Origin").empty());
}

// Test 15: Statistics Collection
TEST_F(CorsMiddlewareTest, StatisticsCollection) {
    CorsMiddleware middleware;
    
    // Initial statistics should be zero
    auto stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], 0);
    EXPECT_EQ(stats["preflight_requests"], 0);
    EXPECT_EQ(stats["allowed_requests"], 0);
    EXPECT_EQ(stats["blocked_requests"], 0);
    EXPECT_EQ(stats["credential_requests"], 0);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    // Process regular request
    middleware.handle(request_, context_, nextHandler);
    
    // Process request with credentials
    HttpRequest credentialRequest;
    credentialRequest = HttpRequest("GET", "/api/test", "HTTP/1.1");
    credentialRequest.setHeader("Origin", "https://example.com");
    credentialRequest.setHeader("Authorization", "Bearer token123");
    
    middleware.handle(credentialRequest, context_, nextHandler);
    
    // Process preflight request
    HttpRequest preflightRequest;
    preflightRequest = HttpRequest("OPTIONS", "/api/test", "HTTP/1.1");
    preflightRequest.setHeader("Origin", "https://example.com");
    preflightRequest.setHeader("Access-Control-Request-Method", "POST");
    
    middleware.handle(preflightRequest, context_, nextHandler);
    
    // Check statistics
    stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], 3);
    EXPECT_EQ(stats["preflight_requests"], 1);
    EXPECT_EQ(stats["allowed_requests"], 3); // All should be allowed with default config
    EXPECT_EQ(stats["blocked_requests"], 0);
    EXPECT_EQ(stats["credential_requests"], 1);
    
    // Reset statistics
    middleware.resetStatistics();
    stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], 0);
    EXPECT_EQ(stats["preflight_requests"], 0);
    EXPECT_EQ(stats["allowed_requests"], 0);
    EXPECT_EQ(stats["blocked_requests"], 0);
    EXPECT_EQ(stats["credential_requests"], 0);
}

// Test 16: Disabled Middleware
TEST_F(CorsMiddlewareTest, DisabledMiddleware) {
    CorsMiddleware middleware;
    middleware.setEnabled(false);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    // Should not add CORS headers when disabled
    EXPECT_TRUE(result.getHeader("Access-Control-Allow-Origin").empty());
    
    // Statistics should not be updated
    auto stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], 0);
}

// Test 17: Configuration Presets
TEST_F(CorsMiddlewareTest, ConfigurationPresets) {
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    // Test permissive config
    auto permissiveConfig = CorsMiddleware::createPermissiveConfig();
    CorsMiddleware permissiveMiddleware(permissiveConfig);
    
    HttpResponse result1 = permissiveMiddleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result1.getStatus(), 200);
    // When allowCredentials=true, origin is reflected (not "*") per CORS spec
    EXPECT_EQ(result1.getHeader("Access-Control-Allow-Origin"), "https://example.com");
    EXPECT_EQ(result1.getHeader("Access-Control-Allow-Credentials"), "true");
    
    // Test restrictive config
    auto restrictiveConfig = CorsMiddleware::createRestrictiveConfig();
    CorsMiddleware restrictiveMiddleware(restrictiveConfig);
    
    HttpResponse result2 = restrictiveMiddleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result2.getStatus(), 200);
    // Should block the request since origin is not in allowed list
    EXPECT_TRUE(result2.getHeader("Access-Control-Allow-Origin").empty());
    
    // Test development config
    auto devConfig = CorsMiddleware::createDevelopmentConfig();
    CorsMiddleware devMiddleware(devConfig);
    
    // Test with localhost origin
    HttpRequest localhostRequest;
    localhostRequest = HttpRequest("GET", "/api/test", "HTTP/1.1");
    localhostRequest.setHeader("Origin", "http://localhost:3000");
    
    HttpResponse result3 = devMiddleware.handle(localhostRequest, context_, nextHandler);
    EXPECT_EQ(result3.getStatus(), 200);
    EXPECT_EQ(result3.getHeader("Access-Control-Allow-Origin"), "http://localhost:3000");
}

// Test 18: Performance Benchmark
TEST_F(CorsMiddlewareTest, PerformanceBenchmark) {
    CorsMiddleware middleware;
    
    const int numRequests = 1000;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numRequests; ++i) {
        auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
            return response_;
        };
        
        middleware.handle(request_, context_, nextHandler);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    double avgTimePerRequest = static_cast<double>(duration.count()) / numRequests;
    
    std::cout << "Average time per CORS request: " << avgTimePerRequest << " microseconds" << std::endl;
    
    // Performance should be reasonable (less than 50 microseconds per request)
    EXPECT_LT(avgTimePerRequest, 50.0);
    
    // Check that all requests were processed
    auto stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], numRequests);
} 