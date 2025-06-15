/**
 * @file test_rate_limit_middleware.cpp
 * @brief Unit tests for rate limiting middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cppSwitchboard/middleware/rate_limit_middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using namespace cppSwitchboard;
using namespace cppSwitchboard::middleware;
using ::testing::_;
using ::testing::Return;

// Mock Redis backend for testing
class MockRedisBackend : public RateLimitMiddleware::RedisBackend {
public:
    MOCK_METHOD(bool, getBucket, (const std::string& key, RateLimitMiddleware::BucketState& state), (override));
    MOCK_METHOD(bool, setBucket, (const std::string& key, const RateLimitMiddleware::BucketState& state), (override));
    MOCK_METHOD(bool, incrementCounter, (const std::string& key, int increment, int expiry), (override));
    MOCK_METHOD(int, getCounter, (const std::string& key), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
};

class RateLimitMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test requests
        request_ = std::make_unique<HttpRequest>("GET", "/api/test", "HTTP/1.1");
        request_->setHeader("Content-Type", "application/json");
        
        // Set a mock client IP
        request_->setHeader("X-Forwarded-For", "192.168.1.100");
        
        // Create test middleware with low limits for easy testing
        RateLimitMiddleware::RateLimitConfig config;
        config.strategy = RateLimitMiddleware::Strategy::IP_BASED;
        config.bucketConfig.maxTokens = 5;
        config.bucketConfig.refillRate = 5;
        config.bucketConfig.refillWindow = RateLimitMiddleware::TimeWindow::SECOND;
        config.bucketConfig.burstAllowed = true;
        config.bucketConfig.burstSize = 3;
        
        rateLimitMiddleware_ = std::make_shared<RateLimitMiddleware>(config);
        
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
    
    Context createAuthenticatedContext(const std::string& userId = "test-user") {
        Context context;
        context["authenticated"] = true;
        context["user_id"] = userId;
        return context;
    }
    
    Context createUnauthenticatedContext() {
        Context context;
        context["authenticated"] = false;
        return context;
    }

protected:
    std::unique_ptr<HttpRequest> request_;
    std::shared_ptr<RateLimitMiddleware> rateLimitMiddleware_;
    NextHandler nextHandler_;
    bool nextHandlerCalled_;
    std::string lastRequest_;
};

// Test basic middleware interface
TEST_F(RateLimitMiddlewareTest, BasicInterface) {
    EXPECT_EQ(rateLimitMiddleware_->getName(), "RateLimitMiddleware");
    EXPECT_EQ(rateLimitMiddleware_->getPriority(), 80);
    EXPECT_TRUE(rateLimitMiddleware_->isEnabled());
}

// Test configuration methods
TEST_F(RateLimitMiddlewareTest, Configuration) {
    rateLimitMiddleware_->setStrategy(RateLimitMiddleware::Strategy::USER_BASED);
    EXPECT_EQ(rateLimitMiddleware_->getStrategy(), RateLimitMiddleware::Strategy::USER_BASED);
    
    RateLimitMiddleware::BucketConfig config;
    config.maxTokens = 100;
    config.refillRate = 50;
    config.refillWindow = RateLimitMiddleware::TimeWindow::MINUTE;
    
    rateLimitMiddleware_->setBucketConfig(config);
    const auto& retrievedConfig = rateLimitMiddleware_->getBucketConfig();
    EXPECT_EQ(retrievedConfig.maxTokens, 100);
    EXPECT_EQ(retrievedConfig.refillRate, 50);
    EXPECT_EQ(retrievedConfig.refillWindow, RateLimitMiddleware::TimeWindow::MINUTE);
    
    rateLimitMiddleware_->setEnabled(false);
    EXPECT_FALSE(rateLimitMiddleware_->isEnabled());
}

// Test disabled middleware
TEST_F(RateLimitMiddlewareTest, DisabledMiddleware) {
    Context context;
    
    rateLimitMiddleware_->setEnabled(false);
    
    // Should pass through without rate limiting
    HttpResponse response = rateLimitMiddleware_->handle(*request_, context, nextHandler_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
    EXPECT_EQ(response.getBody(), "Success");
}

// Test IP-based rate limiting
TEST_F(RateLimitMiddlewareTest, IPBasedRateLimiting) {
    Context context;
    
    // First 5 requests should succeed
    for (int i = 0; i < 5; ++i) {
        nextHandlerCalled_ = false;
        HttpResponse response = rateLimitMiddleware_->handle(*request_, context, nextHandler_);
        EXPECT_EQ(response.getStatus(), 200);
        EXPECT_TRUE(nextHandlerCalled_);
    }
    
    // 6th request should be rate limited
    nextHandlerCalled_ = false;
    HttpResponse response = rateLimitMiddleware_->handle(*request_, context, nextHandler_);
    EXPECT_EQ(response.getStatus(), 429);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Check rate limit headers
    EXPECT_FALSE(response.getHeader("X-RateLimit-Limit").empty());
    EXPECT_EQ(response.getHeader("X-RateLimit-Remaining"), "0");
    EXPECT_FALSE(response.getHeader("Retry-After").empty());
}

// Test user-based rate limiting
TEST_F(RateLimitMiddlewareTest, UserBasedRateLimiting) {
    rateLimitMiddleware_->setStrategy(RateLimitMiddleware::Strategy::USER_BASED);
    
    Context userContext = createAuthenticatedContext("user1");
    Context otherUserContext = createAuthenticatedContext("user2");
    
    // Consume all tokens for user1
    for (int i = 0; i < 5; ++i) {
        HttpResponse response = rateLimitMiddleware_->handle(*request_, userContext, nextHandler_);
        EXPECT_EQ(response.getStatus(), 200);
    }
    
    // user1 should be rate limited
    nextHandlerCalled_ = false;
    HttpResponse response = rateLimitMiddleware_->handle(*request_, userContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 429);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // user2 should still be allowed
    nextHandlerCalled_ = false;
    response = rateLimitMiddleware_->handle(*request_, otherUserContext, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test whitelist functionality
TEST_F(RateLimitMiddlewareTest, WhitelistFunctionality) {
    Context context;
    
    // Add IP to whitelist
    rateLimitMiddleware_->addToWhitelist("192.168.1.100");
    EXPECT_TRUE(rateLimitMiddleware_->isWhitelisted("192.168.1.100"));
    
    // Requests should always succeed regardless of rate limits
    for (int i = 0; i < 10; ++i) {
        HttpResponse response = rateLimitMiddleware_->handle(*request_, context, nextHandler_);
        EXPECT_EQ(response.getStatus(), 200);
    }
    
    // Remove from whitelist
    rateLimitMiddleware_->removeFromWhitelist("192.168.1.100");
    EXPECT_FALSE(rateLimitMiddleware_->isWhitelisted("192.168.1.100"));
}

// Test blacklist functionality
TEST_F(RateLimitMiddlewareTest, BlacklistFunctionality) {
    Context context;
    
    // Add IP to blacklist
    rateLimitMiddleware_->addToBlacklist("192.168.1.100");
    EXPECT_TRUE(rateLimitMiddleware_->isBlacklisted("192.168.1.100"));
    
    // Request should be blocked immediately
    HttpResponse response = rateLimitMiddleware_->handle(*request_, context, nextHandler_);
    EXPECT_EQ(response.getStatus(), 429);
    EXPECT_FALSE(nextHandlerCalled_);
    
    // Remove from blacklist
    rateLimitMiddleware_->removeFromBlacklist("192.168.1.100");
    EXPECT_FALSE(rateLimitMiddleware_->isBlacklisted("192.168.1.100"));
    
    // Should work normally now
    nextHandlerCalled_ = false;
    response = rateLimitMiddleware_->handle(*request_, context, nextHandler_);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(nextHandlerCalled_);
}

// Test rate limit convenience method
TEST_F(RateLimitMiddlewareTest, RateLimitConvenienceMethod) {
    rateLimitMiddleware_->setRateLimit(10, RateLimitMiddleware::TimeWindow::MINUTE, 5);
    
    const auto& config = rateLimitMiddleware_->getBucketConfig();
    EXPECT_EQ(config.maxTokens, 10);
    EXPECT_EQ(config.refillRate, 10);
    EXPECT_EQ(config.refillWindow, RateLimitMiddleware::TimeWindow::MINUTE);
    EXPECT_EQ(config.burstSize, 5);
}

// Test performance benchmark
TEST_F(RateLimitMiddlewareTest, PerformanceBenchmark) {
    Context context;
    
    const int numIterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numIterations; ++i) {
        // Reset to avoid rate limiting
        if (i % 5 == 0) {
            rateLimitMiddleware_->resetKey("ip:192.168.1.100");
        }
        
        HttpResponse response = rateLimitMiddleware_->handle(*request_, context, nextHandler_);
        // Most should succeed due to periodic resets
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avgTime = static_cast<double>(duration.count()) / numIterations;
    std::cout << "Rate limit middleware performance: " << avgTime << " microseconds per request" << std::endl;
    
    // Should be reasonably fast (under 100 microseconds per request)
    EXPECT_LT(avgTime, 100.0);
} 