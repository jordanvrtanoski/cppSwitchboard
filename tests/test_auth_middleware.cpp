/**
 * @file test_auth_middleware.cpp
 * @brief Unit tests for JWT authentication middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cppSwitchboard/middleware/auth_middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <nlohmann/json.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <chrono>
#include <string>
#include <vector>

using namespace cppSwitchboard;
using namespace cppSwitchboard::middleware;
using ::testing::_;
using ::testing::Return;

class AuthMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test JWT secret
        jwtSecret_ = "test-secret-key-1234567890";
        
        // Create test requests
        validRequest_ = std::make_unique<HttpRequest>("GET", "/api/test", "HTTP/1.1");
        validRequest_->setHeader("Content-Type", "application/json");
        
        invalidRequest_ = std::make_unique<HttpRequest>("GET", "/api/test", "HTTP/1.1");
        invalidRequest_->setHeader("Content-Type", "application/json");
        
        // Create test middleware
        authMiddleware_ = std::make_shared<AuthMiddleware>(jwtSecret_);
        
        // Create mock next handler
        nextHandler_ = [this](const HttpRequest& request, Context& context) -> HttpResponse {
            nextHandlerCalled_ = true;
            lastContext_ = context;
            
            HttpResponse response(200);
            response.setBody("Success");
            return response;
        };
        
        nextHandlerCalled_ = false;
    }
    
    std::string createValidJWT(const std::string& userId = "test-user", 
                              const std::vector<std::string>& roles = {"user"},
                              const std::string& issuer = "",
                              const std::string& audience = "",
                              int expiresInMinutes = 60) {
        // Create JWT header
        nlohmann::json header;
        header["typ"] = "JWT";
        header["alg"] = "HS256";
        
        // Create JWT payload
        nlohmann::json payload;
        payload["sub"] = userId;
        payload["user_id"] = userId;
        payload["roles"] = roles;
        
        auto now = std::chrono::system_clock::now();
        auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        auto exp = iat + (expiresInMinutes * 60);
        
        payload["iat"] = iat;
        payload["exp"] = exp;
        
        if (!issuer.empty()) {
            payload["iss"] = issuer;
        }
        if (!audience.empty()) {
            payload["aud"] = audience;
        }
        
        // Encode header and payload
        std::string encodedHeader = base64UrlEncode(header.dump());
        std::string encodedPayload = base64UrlEncode(payload.dump());
        std::string headerAndPayload = encodedHeader + "." + encodedPayload;
        
        // Create signature
        std::string signature = hmacSha256(headerAndPayload, jwtSecret_);
        std::string encodedSignature = base64UrlEncode(signature);
        
        return headerAndPayload + "." + encodedSignature;
    }
    
    std::string createExpiredJWT() {
        return createValidJWT("test-user", {"user"}, "", "", -60); // Expired 1 hour ago
    }
    
    std::string createInvalidSignatureJWT() {
        return createValidJWT("test-user", {"user"}) + "invalid";
    }
    
private:
    std::string base64UrlEncode(const std::string& input) {
        BIO* bio = BIO_new(BIO_s_mem());
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_push(b64, bio);
        
        BIO_write(bio, input.data(), static_cast<int>(input.length()));
        BIO_flush(bio);
        
        BUF_MEM* bufferPtr;
        BIO_get_mem_ptr(bio, &bufferPtr);
        
        std::string encoded(bufferPtr->data, bufferPtr->length);
        BIO_free_all(bio);
        
        // Replace standard base64 characters with URL-safe ones
        std::replace(encoded.begin(), encoded.end(), '+', '-');
        std::replace(encoded.begin(), encoded.end(), '/', '_');
        
        // Remove padding
        encoded.erase(std::find(encoded.begin(), encoded.end(), '='), encoded.end());
        
        return encoded;
    }
    
    std::string hmacSha256(const std::string& data, const std::string& key) {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen;
        
        HMAC(EVP_sha256(), key.data(), static_cast<int>(key.length()),
             reinterpret_cast<const unsigned char*>(data.data()), data.length(),
             hash, &hashLen);
        
        return std::string(reinterpret_cast<char*>(hash), hashLen);
    }

protected:
    std::string jwtSecret_;
    std::unique_ptr<HttpRequest> validRequest_;
    std::unique_ptr<HttpRequest> invalidRequest_;
    std::shared_ptr<AuthMiddleware> authMiddleware_;
    NextHandler nextHandler_;
    bool nextHandlerCalled_;
    Context lastContext_;
};

// Test basic middleware interface
TEST_F(AuthMiddlewareTest, BasicInterface) {
    EXPECT_EQ(authMiddleware_->getName(), "AuthMiddleware");
    EXPECT_EQ(authMiddleware_->getPriority(), 100);
    EXPECT_TRUE(authMiddleware_->isEnabled());
}

// Test configuration methods
TEST_F(AuthMiddlewareTest, Configuration) {
    authMiddleware_->setIssuer("test-issuer");
    EXPECT_EQ(authMiddleware_->getIssuer(), "test-issuer");
    
    authMiddleware_->setAudience("test-audience");
    EXPECT_EQ(authMiddleware_->getAudience(), "test-audience");
    
    authMiddleware_->setExpirationTolerance(600);
    EXPECT_EQ(authMiddleware_->getExpirationTolerance(), 600);
    
    authMiddleware_->setAuthScheme(AuthMiddleware::AuthScheme::JWT);
    EXPECT_EQ(authMiddleware_->getAuthScheme(), AuthMiddleware::AuthScheme::JWT);
    
    authMiddleware_->setAuthHeaderName("X-Auth-Token");
    EXPECT_EQ(authMiddleware_->getAuthHeaderName(), "X-Auth-Token");
    
    authMiddleware_->setEnabled(false);
    EXPECT_FALSE(authMiddleware_->isEnabled());
}

// Test missing authorization header
TEST_F(AuthMiddlewareTest, MissingAuthorizationHeader) {
    Context context;
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Check error response format
    std::string body = response.getBody();
    EXPECT_FALSE(body.empty());
    EXPECT_NE(body.find("unauthorized"), std::string::npos);
}

// Test invalid authorization header format
TEST_F(AuthMiddlewareTest, InvalidAuthorizationFormat) {
    Context context;
    
    validRequest_->setHeader("Authorization", "InvalidFormat token123");
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
}

// Test valid JWT token
TEST_F(AuthMiddlewareTest, ValidJWTToken) {
    Context context;
    
    std::string token = createValidJWT("test-user", {"user", "admin"});
    validRequest_->setHeader("Authorization", "Bearer " + token);
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    EXPECT_EQ(response.getBody(), "Success");
    
    // Check context was populated
    EXPECT_EQ(std::any_cast<bool>(lastContext_["authenticated"]), true);
    EXPECT_EQ(std::any_cast<std::string>(lastContext_["user_id"]), "test-user");
    
    auto roles = std::any_cast<std::vector<std::string>>(lastContext_["roles"]);
    EXPECT_EQ(roles.size(), 2);
    EXPECT_EQ(roles[0], "user");
    EXPECT_EQ(roles[1], "admin");
}

// Test expired JWT token
TEST_F(AuthMiddlewareTest, ExpiredJWTToken) {
    Context context;
    
    std::string token = createExpiredJWT();
    validRequest_->setHeader("Authorization", "Bearer " + token);
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
    
    std::string body = response.getBody();
    EXPECT_NE(body.find("expired"), std::string::npos);
}

// Test invalid JWT signature
TEST_F(AuthMiddlewareTest, InvalidJWTSignature) {
    Context context;
    
    std::string token = createInvalidSignatureJWT();
    validRequest_->setHeader("Authorization", "Bearer " + token);
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
    
    std::string body = response.getBody();
    EXPECT_NE(body.find("signature"), std::string::npos);
}

// Test JWT with issuer validation
TEST_F(AuthMiddlewareTest, JWTIssuerValidation) {
    Context context;
    
    authMiddleware_->setIssuer("expected-issuer");
    
    // Test with wrong issuer
    std::string wrongIssuerToken = createValidJWT("test-user", {"user"}, "wrong-issuer");
    validRequest_->setHeader("Authorization", "Bearer " + wrongIssuerToken);
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test with correct issuer
    nextHandlerCalled_ = false;
    std::string correctIssuerToken = createValidJWT("test-user", {"user"}, "expected-issuer");
    validRequest_->setHeader("Authorization", "Bearer " + correctIssuerToken);
    
    response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test JWT with audience validation
TEST_F(AuthMiddlewareTest, JWTAudienceValidation) {
    Context context;
    
    authMiddleware_->setAudience("expected-audience");
    
    // Test with wrong audience
    std::string wrongAudienceToken = createValidJWT("test-user", {"user"}, "", "wrong-audience");
    validRequest_->setHeader("Authorization", "Bearer " + wrongAudienceToken);
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test with correct audience
    nextHandlerCalled_ = false;
    std::string correctAudienceToken = createValidJWT("test-user", {"user"}, "", "expected-audience");
    validRequest_->setHeader("Authorization", "Bearer " + correctAudienceToken);
    
    response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test disabled middleware
TEST_F(AuthMiddlewareTest, DisabledMiddleware) {
    Context context;
    
    authMiddleware_->setEnabled(false);
    
    // Should pass through without authentication
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    EXPECT_EQ(response.getBody(), "Success");
}

// Test custom token validator
TEST_F(AuthMiddlewareTest, CustomTokenValidator) {
    Context context;
    
    // Create middleware with custom validator
    auto customValidator = [](const std::string& token) -> AuthMiddleware::TokenValidationResult {
        AuthMiddleware::TokenValidationResult result;
        if (token == "valid-custom-token") {
            result.isValid = true;
            result.userId = "custom-user";
            result.roles = {"custom-role"};
        } else {
            result.isValid = false;
            result.errorMessage = "Invalid custom token";
        }
        return result;
    };
    
    auto customAuthMiddleware = std::make_shared<AuthMiddleware>(customValidator);
    
    // Test with valid custom token (for CUSTOM scheme, we don't need Bearer prefix)
    validRequest_->setHeader("Authorization", "valid-custom-token");
    HttpResponse response = customAuthMiddleware->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    
    // Check context was populated
    EXPECT_EQ(std::any_cast<bool>(lastContext_["authenticated"]), true);
    EXPECT_EQ(std::any_cast<std::string>(lastContext_["user_id"]), "custom-user");
    
    auto roles = std::any_cast<std::vector<std::string>>(lastContext_["roles"]);
    EXPECT_EQ(roles.size(), 1);
    EXPECT_EQ(roles[0], "custom-role");
    
    // Test with invalid custom token
    nextHandlerCalled_ = false;
    context.clear(); // Clear context for next test
    validRequest_->setHeader("Authorization", "invalid-custom-token");
    response = customAuthMiddleware->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
}

// Test different authentication schemes
TEST_F(AuthMiddlewareTest, AuthenticationSchemes) {
    Context context;
    
    std::string token = createValidJWT();
    
    // Test Bearer scheme
    authMiddleware_->setAuthScheme(AuthMiddleware::AuthScheme::BEARER);
    validRequest_->setHeader("Authorization", "Bearer " + token);
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    
    // Test JWT scheme (accepts raw token)
    nextHandlerCalled_ = false;
    authMiddleware_->setAuthScheme(AuthMiddleware::AuthScheme::JWT);
    validRequest_->setHeader("Authorization", token);
    
    response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test custom header name
TEST_F(AuthMiddlewareTest, CustomHeaderName) {
    Context context;
    
    authMiddleware_->setAuthHeaderName("X-Custom-Auth");
    
    std::string token = createValidJWT();
    validRequest_->setHeader("X-Custom-Auth", "Bearer " + token);
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test expiration tolerance
TEST_F(AuthMiddlewareTest, ExpirationTolerance) {
    Context context;
    
    authMiddleware_->setExpirationTolerance(3600); // 1 hour tolerance
    
    // Create token that expired 30 minutes ago (should be accepted with 1 hour tolerance)
    std::string token = createValidJWT("test-user", {"user"}, "", "", -30);
    validRequest_->setHeader("Authorization", "Bearer " + token);
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test malformed JWT
TEST_F(AuthMiddlewareTest, MalformedJWT) {
    Context context;
    
    // Test with only one part
    validRequest_->setHeader("Authorization", "Bearer invalid.jwt");
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test with invalid base64
    nextHandlerCalled_ = false;
    validRequest_->setHeader("Authorization", "Bearer invalid.base64!@#.signature");
    response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_FALSE(nextHandlerCalled_);
}

// Test error response format
TEST_F(AuthMiddlewareTest, ErrorResponseFormat) {
    Context context;
    
    HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::UNAUTHORIZED);
    EXPECT_EQ(response.getHeader("Content-Type"), "application/json");
    EXPECT_EQ(response.getHeader("WWW-Authenticate"), "Bearer");
    
    // Parse JSON response
    nlohmann::json errorBody = nlohmann::json::parse(response.getBody());
    EXPECT_EQ(errorBody["error"], "unauthorized");
    EXPECT_FALSE(errorBody["message"].get<std::string>().empty());
    EXPECT_TRUE(errorBody.contains("timestamp"));
}

// Test performance with valid tokens
TEST_F(AuthMiddlewareTest, PerformanceBenchmark) {
    Context context;
    
    std::string token = createValidJWT();
    validRequest_->setHeader("Authorization", "Bearer " + token);
    
    const int numIterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numIterations; ++i) {
        HttpResponse response = authMiddleware_->handle(*validRequest_, context, nextHandler_);
        EXPECT_EQ(response.getStatus(), 200);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avgTime = static_cast<double>(duration.count()) / numIterations;
    std::cout << "Auth middleware performance: " << avgTime << " microseconds per authentication" << std::endl;
    
    // Should be reasonably fast (under 100 microseconds per auth)
    EXPECT_LT(avgTime, 100.0);
} 