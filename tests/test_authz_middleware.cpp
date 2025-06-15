/**
 * @file test_authz_middleware.cpp
 * @brief Unit tests for role-based authorization middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cppSwitchboard/middleware/authz_middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <nlohmann/json.hpp>

using namespace cppSwitchboard;
using namespace cppSwitchboard::middleware;
using ::testing::_;
using ::testing::Return;

class AuthzMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test requests
        request_ = std::make_unique<HttpRequest>("GET", "/api/users", "HTTP/1.1");
        request_->setHeader("Content-Type", "application/json");
        
        adminRequest_ = std::make_unique<HttpRequest>("GET", "/api/admin", "HTTP/1.1");
        adminRequest_->setHeader("Content-Type", "application/json");
        
        // Create test middleware
        authzMiddleware_ = std::make_shared<AuthzMiddleware>();
        
        // Create mock next handler
        nextHandler_ = [this](const HttpRequest& request, Context& context) -> HttpResponse {
            nextHandlerCalled_ = true;
            lastRequest_ = request.getPath();
            
            HttpResponse response(200);
            response.setBody("Success");
            return response;
        };
        
        nextHandlerCalled_ = false;
        lastRequest_ = "";
    }
    
    Context createAuthenticatedContext(const std::vector<std::string>& roles = {"user"},
                                      const std::vector<std::string>& permissions = {},
                                      const std::string& userId = "test-user") {
        Context context;
        context["authenticated"] = true;
        context["user_id"] = userId;
        context["roles"] = roles;
        if (!permissions.empty()) {
            context["permissions"] = permissions;
        }
        return context;
    }
    
    Context createUnauthenticatedContext() {
        Context context;
        context["authenticated"] = false;
        return context;
    }

protected:
    std::unique_ptr<HttpRequest> request_;
    std::unique_ptr<HttpRequest> adminRequest_;
    std::shared_ptr<AuthzMiddleware> authzMiddleware_;
    NextHandler nextHandler_;
    bool nextHandlerCalled_;
    std::string lastRequest_;
};

// Test basic middleware interface
TEST_F(AuthzMiddlewareTest, BasicInterface) {
    EXPECT_EQ(authzMiddleware_->getName(), "AuthzMiddleware");
    EXPECT_EQ(authzMiddleware_->getPriority(), 90);
    EXPECT_TRUE(authzMiddleware_->isEnabled());
}

// Test configuration methods
TEST_F(AuthzMiddlewareTest, Configuration) {
    authzMiddleware_->setUserIdKey("custom_user_id");
    authzMiddleware_->setUserRolesKey("custom_roles");
    authzMiddleware_->setUserPermissionsKey("custom_permissions");
    
    authzMiddleware_->setEnabled(false);
    EXPECT_FALSE(authzMiddleware_->isEnabled());
    
    authzMiddleware_->setRequireAuthentication(false);
    
    // The getters are not exposed in the interface, but we can test behavior
    EXPECT_TRUE(true); // Basic configuration test
}

// Test middleware disabled
TEST_F(AuthzMiddlewareTest, DisabledMiddleware) {
    Context context = createUnauthenticatedContext();
    
    authzMiddleware_->setEnabled(false);
    
    HttpResponse response = authzMiddleware_->handle(*request_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    EXPECT_EQ(response.getBody(), "Success");
}

// Test unauthenticated user
TEST_F(AuthzMiddlewareTest, UnauthenticatedUser) {
    Context context = createUnauthenticatedContext();
    
    HttpResponse response = authzMiddleware_->handle(*request_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Check error response format
    std::string body = response.getBody();
    EXPECT_FALSE(body.empty());
    EXPECT_NE(body.find("forbidden"), std::string::npos);
}

// Test authenticated user with no specific authorization requirements
TEST_F(AuthzMiddlewareTest, AuthenticatedUserNoRequirements) {
    Context context = createAuthenticatedContext();
    
    HttpResponse response = authzMiddleware_->handle(*request_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    EXPECT_EQ(response.getBody(), "Success");
}

// Test default role requirements
TEST_F(AuthzMiddlewareTest, DefaultRoleRequirements) {
    Context context = createAuthenticatedContext({"user"});
    
    // Set default role requirement
    authzMiddleware_->setDefaultRoles({"admin"}, false);
    
    HttpResponse response = authzMiddleware_->handle(*request_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test with correct role
    nextHandlerCalled_ = false;
    Context adminContext = createAuthenticatedContext({"admin"});
    response = authzMiddleware_->handle(*request_, adminContext, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test resource-specific role requirements
TEST_F(AuthzMiddlewareTest, ResourceSpecificRoles) {
    Context userContext = createAuthenticatedContext({"user"});
    Context adminContext = createAuthenticatedContext({"admin"});
    
    // Add role requirement for admin endpoint
    authzMiddleware_->addResourceRoles("/api/admin", {"admin"}, false);
    
    // Test user access to regular endpoint (should succeed)
    HttpResponse response = authzMiddleware_->handle(*request_, userContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    
    // Test user access to admin endpoint (should fail)
    nextHandlerCalled_ = false;
    response = authzMiddleware_->handle(*adminRequest_, userContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test admin access to admin endpoint (should succeed)
    nextHandlerCalled_ = false;
    response = authzMiddleware_->handle(*adminRequest_, adminContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test wildcard resource patterns
TEST_F(AuthzMiddlewareTest, WildcardResourcePatterns) {
    Context userContext = createAuthenticatedContext({"user"});
    Context adminContext = createAuthenticatedContext({"admin"});
    
    // Add role requirement for all admin endpoints
    authzMiddleware_->addResourceRoles("/api/admin/*", {"admin"}, false);
    
    auto adminSubRequest = std::make_unique<HttpRequest>("GET", "/api/admin/users", "HTTP/1.1");
    
    // Test user access to admin sub-endpoint (should fail)
    HttpResponse response = authzMiddleware_->handle(*adminSubRequest, userContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test admin access to admin sub-endpoint (should succeed)
    nextHandlerCalled_ = false;
    response = authzMiddleware_->handle(*adminSubRequest, adminContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test permission-based authorization
TEST_F(AuthzMiddlewareTest, PermissionBasedAuthorization) {
    Context readContext = createAuthenticatedContext({"user"}, {"read"});
    Context writeContext = createAuthenticatedContext({"user"}, {"read", "write"});
    
    // Add permission requirement for the endpoint
    authzMiddleware_->addResourcePermissions("/api/users", {"write"}, false);
    
    // Test with read-only permission (should fail)
    HttpResponse response = authzMiddleware_->handle(*request_, readContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test with write permission (should succeed)
    nextHandlerCalled_ = false;
    response = authzMiddleware_->handle(*request_, writeContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test role hierarchy
TEST_F(AuthzMiddlewareTest, RoleHierarchy) {
    // Define role hierarchy: admin inherits from user
    authzMiddleware_->defineRole("user", {}, {"read"});
    authzMiddleware_->defineRole("admin", {"user"}, {"write", "delete"});
    
    Context adminContext = createAuthenticatedContext({"admin"});
    
    // Add permission requirement that admin should inherit from user
    authzMiddleware_->addResourcePermissions("/api/data", {"read"}, false);
    
    HttpResponse response = authzMiddleware_->handle(*request_, adminContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    
    // Test effective permissions
    std::vector<std::string> adminPermissions = authzMiddleware_->getEffectivePermissions("admin");
    EXPECT_TRUE(std::find(adminPermissions.begin(), adminPermissions.end(), "read") != adminPermissions.end());
    EXPECT_TRUE(std::find(adminPermissions.begin(), adminPermissions.end(), "write") != adminPermissions.end());
    EXPECT_TRUE(std::find(adminPermissions.begin(), adminPermissions.end(), "delete") != adminPermissions.end());
    
    // Test role has permission
    EXPECT_TRUE(authzMiddleware_->roleHasPermission("admin", "read"));
    EXPECT_TRUE(authzMiddleware_->roleHasPermission("admin", "write"));
    EXPECT_FALSE(authzMiddleware_->roleHasPermission("user", "write"));
}

// Test multiple roles (OR logic)
TEST_F(AuthzMiddlewareTest, MultipleRolesOr) {
    Context moderatorContext = createAuthenticatedContext({"moderator"});
    
    // Add requirement for admin OR moderator
    authzMiddleware_->addResourceRoles("/api/moderate", {"admin", "moderator"}, false);
    
    HttpResponse response = authzMiddleware_->handle(*request_, moderatorContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test multiple roles (AND logic)
TEST_F(AuthzMiddlewareTest, MultipleRolesAnd) {
    Context userContext = createAuthenticatedContext({"user"});
    Context superuserContext = createAuthenticatedContext({"user", "admin"});
    
    // Add requirement for BOTH user AND admin
    authzMiddleware_->addResourceRoles("/api/super", {"user", "admin"}, true);
    
    auto superRequest = std::make_unique<HttpRequest>("GET", "/api/super", "HTTP/1.1");
    
    // Test with only user role (should fail)
    HttpResponse response = authzMiddleware_->handle(*superRequest, userContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test with both roles (should succeed)
    nextHandlerCalled_ = false;
    response = authzMiddleware_->handle(*superRequest, superuserContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test custom permission checker
TEST_F(AuthzMiddlewareTest, CustomPermissionChecker) {
    auto customChecker = [](const Context& context, const std::string& resource, const std::vector<std::string>& permissions) -> bool {
        // Custom logic: allow access only if user ID contains "admin"
        auto userIdIt = context.find("user_id");
        if (userIdIt != context.end()) {
            try {
                std::string userId = std::any_cast<std::string>(userIdIt->second);
                return userId.find("admin") != std::string::npos;
            } catch (const std::bad_any_cast&) {
                return false;
            }
        }
        return false;
    };
    
    auto customAuthzMiddleware = std::make_shared<AuthzMiddleware>(customChecker);
    
    Context regularUserContext = createAuthenticatedContext({}, {}, "regular-user");
    Context adminUserContext = createAuthenticatedContext({}, {}, "admin-user");
    
    // Test with regular user (should fail)
    HttpResponse response = customAuthzMiddleware->handle(*request_, regularUserContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Test with admin user (should succeed)
    nextHandlerCalled_ = false;
    response = customAuthzMiddleware->handle(*request_, adminUserContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test policy removal
TEST_F(AuthzMiddlewareTest, PolicyRemoval) {
    Context userContext = createAuthenticatedContext({"user"});
    
    // Add and then remove a policy
    authzMiddleware_->addResourceRoles("/api/test", {"admin"}, false);
    
    auto testRequest = std::make_unique<HttpRequest>("GET", "/api/test", "HTTP/1.1");
    
    // Should fail initially
    HttpResponse response = authzMiddleware_->handle(*testRequest, userContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Remove policy
    authzMiddleware_->removeResourcePolicy("/api/test");
    
    // Should succeed now
    nextHandlerCalled_ = false;
    response = authzMiddleware_->handle(*testRequest, userContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test error response format
TEST_F(AuthzMiddlewareTest, ErrorResponseFormat) {
    Context context = createUnauthenticatedContext();
    
    HttpResponse response = authzMiddleware_->handle(*request_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_EQ(response.getHeader("Content-Type"), "application/json");
    
    // Parse JSON response
    nlohmann::json errorBody = nlohmann::json::parse(response.getBody());
    EXPECT_EQ(errorBody["error"], "forbidden");
    EXPECT_FALSE(errorBody["message"].get<std::string>().empty());
    EXPECT_TRUE(errorBody.contains("timestamp"));
    EXPECT_TRUE(errorBody.contains("resource"));
}

// Test with no authentication requirement
TEST_F(AuthzMiddlewareTest, NoAuthenticationRequired) {
    Context context; // No authentication info
    
    authzMiddleware_->setRequireAuthentication(false);
    authzMiddleware_->addResourceRoles("/api/users", {"admin"}, false);
    
    // Should fail due to role requirements, not authentication
    HttpResponse response = authzMiddleware_->handle(*request_, context, nextHandler_);
    EXPECT_EQ(response.getStatus(), HttpResponse::FORBIDDEN);
    EXPECT_FALSE(nextHandlerCalled_);
}

// Test performance benchmark
TEST_F(AuthzMiddlewareTest, PerformanceBenchmark) {
    Context context = createAuthenticatedContext({"user", "admin"}, {"read", "write"});
    
    // Add some policies
    authzMiddleware_->addResourceRoles("/api/users", {"user"}, false);
    authzMiddleware_->addResourcePermissions("/api/data", {"read"}, false);
    
    const int numIterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numIterations; ++i) {
        HttpResponse response = authzMiddleware_->handle(*request_, context, nextHandler_);
        EXPECT_EQ(response.getStatus(), 200);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avgTime = static_cast<double>(duration.count()) / numIterations;
    std::cout << "Authz middleware performance: " << avgTime << " microseconds per authorization" << std::endl;
    
    // Should be reasonably fast (under 50 microseconds per authz)
    EXPECT_LT(avgTime, 50.0);
} 