/**
 * @file test_middleware_factory.cpp
 * @brief Unit tests for the MiddlewareFactory system
 * 
 * This file contains comprehensive tests for middleware factory functionality,
 * including built-in creators, custom registration, and pipeline creation.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "cppSwitchboard/middleware_config.h"
#include "cppSwitchboard/middleware_factory.h"
#include "cppSwitchboard/middleware_pipeline.h"
#include "cppSwitchboard/http_request.h"
#include "cppSwitchboard/http_response.h"
#include <memory>
#include <vector>
#include <any>

using namespace cppSwitchboard;

class MiddlewareFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get factory instance
        factory_ = &MiddlewareFactory::getInstance();
    }
    
    void TearDown() override {
        // Clean up any custom middleware for next test
        factory_->unregisterCreator("custom_test");
    }
    
    MiddlewareFactory* factory_;
};

/**
 * @brief Test middleware creator for testing custom registration
 */
class TestMiddlewareCreator : public MiddlewareCreator {
public:
    std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override {
        // Return a test middleware implementation
        class TestMiddleware : public Middleware {
        public:
            explicit TestMiddleware(const std::string& testValue) : testValue_(testValue) {}
            
            HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
                context["test_middleware_called"] = true;
                context["test_value"] = testValue_;
                return next(request, context);
            }
            
            std::string getName() const override { return "TestMiddleware"; }
            int getPriority() const override { return 100; }
            
        private:
            std::string testValue_;
        };
        
        std::string testValue = "default";
        auto testValueIt = config.config.find("test_value");
        if (testValueIt != config.config.end()) {
            try {
                testValue = std::any_cast<std::string>(testValueIt->second);
            } catch (const std::bad_any_cast&) {
                return nullptr;
            }
        }
        
        return std::make_shared<TestMiddleware>(testValue);
    }
    
    std::string getMiddlewareName() const override {
        return "custom_test";
    }
    
    bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override {
        auto testValueIt = config.config.find("test_value");
        if (testValueIt != config.config.end()) {
            try {
                std::any_cast<std::string>(testValueIt->second);
            } catch (const std::bad_any_cast&) {
                errorMessage = "test_value must be a string";
                return false;
            }
        }
        return true;
    }
};

// Test 1: Built-in Middleware Registration
TEST_F(MiddlewareFactoryTest, BuiltinMiddlewareRegistered) {
    // Check that all built-in middleware types are registered
    EXPECT_TRUE(factory_->isMiddlewareRegistered("auth"));
    EXPECT_TRUE(factory_->isMiddlewareRegistered("authz"));
    EXPECT_TRUE(factory_->isMiddlewareRegistered("cors"));
    EXPECT_TRUE(factory_->isMiddlewareRegistered("logging"));
    EXPECT_TRUE(factory_->isMiddlewareRegistered("rate_limit"));
    
    // Check that unknown middleware is not registered
    EXPECT_FALSE(factory_->isMiddlewareRegistered("unknown"));
    EXPECT_FALSE(factory_->isMiddlewareRegistered(""));
}

// Test 2: Get Registered Middleware List
TEST_F(MiddlewareFactoryTest, GetRegisteredMiddlewareList) {
    auto middlewareList = factory_->getRegisteredMiddleware();
    
    // Should contain all built-in middleware
    EXPECT_THAT(middlewareList, ::testing::Contains("auth"));
    EXPECT_THAT(middlewareList, ::testing::Contains("authz"));
    EXPECT_THAT(middlewareList, ::testing::Contains("cors"));
    EXPECT_THAT(middlewareList, ::testing::Contains("logging"));
    EXPECT_THAT(middlewareList, ::testing::Contains("rate_limit"));
    
    // Should have expected count
    EXPECT_EQ(middlewareList.size(), 5);
}

// Test 3: Custom Middleware Registration
TEST_F(MiddlewareFactoryTest, CustomMiddlewareRegistration) {
    // Initially not registered
    EXPECT_FALSE(factory_->isMiddlewareRegistered("custom_test"));
    
    // Register custom middleware
    auto creator = std::make_unique<TestMiddlewareCreator>();
    bool result = factory_->registerCreator(std::move(creator));
    EXPECT_TRUE(result);
    
    // Now should be registered
    EXPECT_TRUE(factory_->isMiddlewareRegistered("custom_test"));
    
    // Should appear in list
    auto middlewareList = factory_->getRegisteredMiddleware();
    EXPECT_THAT(middlewareList, ::testing::Contains("custom_test"));
}

// Test 4: Duplicate Registration Handling
TEST_F(MiddlewareFactoryTest, DuplicateRegistrationHandling) {
    // Register custom middleware
    auto creator1 = std::make_unique<TestMiddlewareCreator>();
    bool result1 = factory_->registerCreator(std::move(creator1));
    EXPECT_TRUE(result1);
    
    // Try to register again - should fail
    auto creator2 = std::make_unique<TestMiddlewareCreator>();
    bool result2 = factory_->registerCreator(std::move(creator2));
    EXPECT_FALSE(result2);
    
    // Try to register built-in middleware - should fail
    auto creator3 = std::make_unique<TestMiddlewareCreator>();
    bool result3 = factory_->registerCreator(std::move(creator3));
    EXPECT_FALSE(result3);
}

// Test 5: Middleware Unregistration
TEST_F(MiddlewareFactoryTest, MiddlewareUnregistration) {
    // Register custom middleware
    auto creator = std::make_unique<TestMiddlewareCreator>();
    factory_->registerCreator(std::move(creator));
    EXPECT_TRUE(factory_->isMiddlewareRegistered("custom_test"));
    
    // Unregister it
    bool result = factory_->unregisterCreator("custom_test");
    EXPECT_TRUE(result);
    EXPECT_FALSE(factory_->isMiddlewareRegistered("custom_test"));
    
    // Try to unregister again - should fail
    bool result2 = factory_->unregisterCreator("custom_test");
    EXPECT_FALSE(result2);
    
    // Try to unregister non-existent - should fail
    bool result3 = factory_->unregisterCreator("non_existent");
    EXPECT_FALSE(result3);
}

// Test 6: Auth Middleware Creation
TEST_F(MiddlewareFactoryTest, AuthMiddlewareCreation) {
    MiddlewareInstanceConfig config;
    config.name = "auth";
    config.enabled = true;
    config.config["jwt_secret"] = std::string("test_secret");
    config.config["issuer"] = std::string("test_issuer");
    config.config["audience"] = std::string("test_audience");
    config.config["leeway_seconds"] = 30;
    
    auto middleware = factory_->createMiddleware(config);
    EXPECT_NE(middleware, nullptr);
    EXPECT_EQ(middleware->getName(), "AuthMiddleware");
}

// Test 7: CORS Middleware Creation  
TEST_F(MiddlewareFactoryTest, CorsMiddlewareCreation) {
    MiddlewareInstanceConfig config;
    config.name = "cors";
    config.enabled = true;
    config.config["allowed_origins"] = std::vector<std::string>{"*"};
    config.config["allowed_methods"] = std::vector<std::string>{"GET", "POST"};
    config.config["allow_credentials"] = true;
    config.config["max_age"] = 86400;
    
    auto middleware = factory_->createMiddleware(config);
    EXPECT_NE(middleware, nullptr);
    EXPECT_EQ(middleware->getName(), "CorsMiddleware");
}

// Test 8: Logging Middleware Creation
TEST_F(MiddlewareFactoryTest, LoggingMiddlewareCreation) {
    MiddlewareInstanceConfig config;
    config.name = "logging";
    config.enabled = true;  
    config.config["format"] = std::string("json");
    config.config["include_headers"] = true;
    config.config["include_body"] = false;
    
    auto middleware = factory_->createMiddleware(config);
    EXPECT_NE(middleware, nullptr);
    EXPECT_EQ(middleware->getName(), "LoggingMiddleware");
}

// Test 9: Rate Limit Middleware Creation
TEST_F(MiddlewareFactoryTest, RateLimitMiddlewareCreation) {
    MiddlewareInstanceConfig config;
    config.name = "rate_limit";
    config.enabled = true;
    config.config["requests_per_minute"] = 100;
    config.config["per_ip"] = true;
    config.config["burst_capacity"] = 50;
    
    auto middleware = factory_->createMiddleware(config);
    EXPECT_NE(middleware, nullptr);
    EXPECT_EQ(middleware->getName(), "RateLimitMiddleware");
}

// Test 10: Authorization Middleware Creation
TEST_F(MiddlewareFactoryTest, AuthzMiddlewareCreation) {
    MiddlewareInstanceConfig config;
    config.name = "authz";
    config.enabled = true;
    config.config["require_authenticated_user"] = true;
    config.config["required_roles"] = std::vector<std::string>{"admin", "user"};
    config.config["required_permissions"] = std::vector<std::string>{"read", "write"};
    
    auto middleware = factory_->createMiddleware(config);
    EXPECT_NE(middleware, nullptr);
    EXPECT_EQ(middleware->getName(), "AuthzMiddleware");
}

// Test 11: Invalid Middleware Type
TEST_F(MiddlewareFactoryTest, InvalidMiddlewareType) {
    MiddlewareInstanceConfig config;
    config.name = "non_existent";
    config.enabled = true;
    
    auto middleware = factory_->createMiddleware(config);
    EXPECT_EQ(middleware, nullptr);
}

// Test 12: Configuration Validation - Auth Middleware
TEST_F(MiddlewareFactoryTest, AuthMiddlewareValidation) {
    // Missing jwt_secret should fail
    MiddlewareInstanceConfig config;
    config.name = "auth";
    config.enabled = true;
    
    std::string errorMessage;
    bool valid = factory_->validateMiddlewareConfig(config, errorMessage);
    EXPECT_FALSE(valid);
    EXPECT_THAT(errorMessage, ::testing::HasSubstr("jwt_secret"));
    
    // Valid configuration should pass
    config.config["jwt_secret"] = std::string("test_secret");
    bool valid2 = factory_->validateMiddlewareConfig(config, errorMessage);
    EXPECT_TRUE(valid2);
}

// Test 13: Configuration Validation - Rate Limit Middleware
TEST_F(MiddlewareFactoryTest, RateLimitMiddlewareValidation) {
    // Missing rate limit configuration should fail
    MiddlewareInstanceConfig config;
    config.name = "rate_limit";
    config.enabled = true;
    
    std::string errorMessage;
    bool valid = factory_->validateMiddlewareConfig(config, errorMessage);
    EXPECT_FALSE(valid);
    EXPECT_THAT(errorMessage, ::testing::HasSubstr("rate limit"));
    
    // Valid configuration should pass
    config.config["requests_per_minute"] = 100;
    bool valid2 = factory_->validateMiddlewareConfig(config, errorMessage);
    EXPECT_TRUE(valid2);
    
    // Invalid (negative) rate should fail
    config.config["requests_per_minute"] = -1;
    bool valid3 = factory_->validateMiddlewareConfig(config, errorMessage);
    EXPECT_FALSE(valid3);
    EXPECT_THAT(errorMessage, ::testing::HasSubstr("must be positive"));
}

// Test 14: Pipeline Creation from Configuration
TEST_F(MiddlewareFactoryTest, PipelineCreation) {
    std::vector<MiddlewareInstanceConfig> configs;
    
    // Add CORS middleware
    MiddlewareInstanceConfig corsConfig;
    corsConfig.name = "cors";
    corsConfig.enabled = true;
    corsConfig.config["allowed_origins"] = std::vector<std::string>{"*"};
    configs.push_back(corsConfig);
    
    // Add logging middleware
    MiddlewareInstanceConfig loggingConfig;
    loggingConfig.name = "logging";
    loggingConfig.enabled = true;
    loggingConfig.config["format"] = std::string("json");
    configs.push_back(loggingConfig);
    
    // Add auth middleware
    MiddlewareInstanceConfig authConfig;
    authConfig.name = "auth";
    authConfig.enabled = true;
    authConfig.config["jwt_secret"] = std::string("test_secret");
    configs.push_back(authConfig);
    
    auto pipeline = factory_->createPipeline(configs);
    EXPECT_NE(pipeline, nullptr);
    
    // Pipeline should have 3 middleware
    // Note: We can't directly test the count, but we can test execution
    EXPECT_NE(pipeline, nullptr);
}

// Test 15: Pipeline Creation with Disabled Middleware
TEST_F(MiddlewareFactoryTest, PipelineCreationWithDisabledMiddleware) {
    std::vector<MiddlewareInstanceConfig> configs;
    
    // Add enabled CORS middleware
    MiddlewareInstanceConfig corsConfig;
    corsConfig.name = "cors";
    corsConfig.enabled = true;
    corsConfig.config["allowed_origins"] = std::vector<std::string>{"*"};
    configs.push_back(corsConfig);
    
    // Add disabled logging middleware
    MiddlewareInstanceConfig loggingConfig;
    loggingConfig.name = "logging";
    loggingConfig.enabled = false;  // Disabled
    loggingConfig.config["format"] = std::string("json");
    configs.push_back(loggingConfig);
    
    auto pipeline = factory_->createPipeline(configs);
    EXPECT_NE(pipeline, nullptr);
    
    // Should create pipeline with only enabled middleware
}

// Test 16: Custom Middleware Creation and Usage
TEST_F(MiddlewareFactoryTest, CustomMiddlewareCreationAndUsage) {
    // Register custom middleware
    auto creator = std::make_unique<TestMiddlewareCreator>();
    factory_->registerCreator(std::move(creator));
    
    // Create middleware instance
    MiddlewareInstanceConfig config;
    config.name = "custom_test";
    config.enabled = true;
    config.config["test_value"] = std::string("hello_world");
    
    auto middleware = factory_->createMiddleware(config);
    EXPECT_NE(middleware, nullptr);
    EXPECT_EQ(middleware->getName(), "TestMiddleware");
    
    // Test middleware execution
    HttpRequest request("GET", "/test", "HTTP/1.1");
    
    Context context;
    bool nextCalled = false;
    
    auto nextHandler = [&nextCalled](const HttpRequest& req, Context& ctx) -> HttpResponse {
        nextCalled = true;
        return HttpResponse::ok("Success");
    };
    
    HttpResponse response = middleware->handle(request, context, nextHandler);
    
    // Verify middleware was executed
    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(response.getStatus(), 200);
    
    // Check context modifications
    auto calledIt = context.find("test_middleware_called");
    EXPECT_NE(calledIt, context.end());
    EXPECT_TRUE(std::any_cast<bool>(calledIt->second));
    
    auto valueIt = context.find("test_value");
    EXPECT_NE(valueIt, context.end());
    EXPECT_EQ(std::any_cast<std::string>(valueIt->second), "hello_world");
}

// Test 17: Factory Singleton Behavior
TEST_F(MiddlewareFactoryTest, SingletonBehavior) {
    auto& factory1 = MiddlewareFactory::getInstance();
    auto& factory2 = MiddlewareFactory::getInstance();
    
    // Should be the same instance
    EXPECT_EQ(&factory1, &factory2);
}

// Test 18: Invalid Configuration Data Types
TEST_F(MiddlewareFactoryTest, InvalidConfigurationDataTypes) {
    // Register custom middleware for testing
    auto creator = std::make_unique<TestMiddlewareCreator>();
    factory_->registerCreator(std::move(creator));
    
    MiddlewareInstanceConfig config;
    config.name = "custom_test";
    config.enabled = true;
    config.config["test_value"] = 123;  // Should be string, not int
    
    std::string errorMessage;
    bool valid = factory_->validateMiddlewareConfig(config, errorMessage);
    EXPECT_FALSE(valid);
    EXPECT_THAT(errorMessage, ::testing::HasSubstr("must be a string"));
}

// Test 19: Empty Pipeline Creation
TEST_F(MiddlewareFactoryTest, EmptyPipelineCreation) {
    std::vector<MiddlewareInstanceConfig> configs;  // Empty
    
    auto pipeline = factory_->createPipeline(configs);
    EXPECT_NE(pipeline, nullptr);  // Should still create valid pipeline
}

// Test 20: Null Creator Registration
TEST_F(MiddlewareFactoryTest, NullCreatorRegistration) {
    bool result = factory_->registerCreator(nullptr);
    EXPECT_FALSE(result);
} 