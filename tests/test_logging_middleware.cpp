/**
 * @file test_logging_middleware.cpp
 * @brief Unit tests for LoggingMiddleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#include <gtest/gtest.h>
#include <cppSwitchboard/middleware/logging_middleware.h>
#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace cppSwitchboard;
using namespace cppSwitchboard::middleware;

// Mock logger for testing
class MockLogger : public LoggingMiddleware::Logger {
public:
    struct LogRecord {
        LoggingMiddleware::LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };

    void log(LoggingMiddleware::LogLevel level, const LoggingMiddleware::LogEntry& entry, 
             const std::string& message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.push_back({level, message, entry.timestamp});
    }

    void flush() override {
        flushed_ = true;
    }

    std::vector<LogRecord> getLogs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return logs_;
    }

    bool wasFlushed() const { return flushed_; }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.clear();
        flushed_ = false;
    }

private:
    mutable std::mutex mutex_;
    std::vector<LogRecord> logs_;
    bool flushed_ = false;
};

class LoggingMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockLogger_ = std::make_shared<MockLogger>();
        
        // Create test request
        request_ = HttpRequest("GET", "/api/users", "HTTP/1.1");
        request_.setQueryParam("limit", "10");
        request_.setQueryParam("offset", "0");
        request_.setBody("test body");
        request_.setHeader("User-Agent", "TestAgent/1.0");
        request_.setHeader("Authorization", "Bearer token123");
        request_.setHeader("Content-Type", "application/json");
        request_.setHeader("X-Forwarded-For", "192.168.1.100");
        
        // Create test response
        response_.setStatus(200);
        response_.setBody("{\"users\": []}");
        response_.setHeader("Content-Type", "application/json");
        response_.setHeader("X-Total-Count", "0");
        
        // Create test context
        context_["user_id"] = std::string("user123");
        context_["session_id"] = std::string("session456");
    }

    void TearDown() override {
        mockLogger_.reset();
    }

    HttpRequest request_;
    HttpResponse response_;
    Context context_;
    std::shared_ptr<MockLogger> mockLogger_;
};

// Test 1: Basic Interface Tests
TEST_F(LoggingMiddlewareTest, BasicInterface) {
    LoggingMiddleware middleware;
    
    EXPECT_EQ(middleware.getName(), "LoggingMiddleware");
    EXPECT_EQ(middleware.getPriority(), 10);
    EXPECT_TRUE(middleware.isEnabled());
    
    middleware.setEnabled(false);
    EXPECT_FALSE(middleware.isEnabled());
}

// Test 2: Default Configuration
TEST_F(LoggingMiddlewareTest, DefaultConfiguration) {
    LoggingMiddleware middleware;
    
    EXPECT_EQ(middleware.getLogFormat(), LoggingMiddleware::LogFormat::JSON);
    EXPECT_EQ(middleware.getLogLevel(), LoggingMiddleware::LogLevel::INFO);
    
    // Test default logging behavior
    bool called = false;
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        called = true;
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_TRUE(called);
    EXPECT_EQ(result.getStatus(), 200);
}

// Test 3: Configuration Methods
TEST_F(LoggingMiddlewareTest, ConfigurationMethods) {
    LoggingMiddleware middleware;
    
    // Test log format
    middleware.setLogFormat(LoggingMiddleware::LogFormat::COMMON);
    EXPECT_EQ(middleware.getLogFormat(), LoggingMiddleware::LogFormat::COMMON);
    
    // Test log level
    middleware.setLogLevel(LoggingMiddleware::LogLevel::ERROR);
    EXPECT_EQ(middleware.getLogLevel(), LoggingMiddleware::LogLevel::ERROR);
    
    // Test request/response logging
    middleware.setLogRequests(false);
    middleware.setLogResponses(false);
    
    // Test header/body inclusion
    middleware.setIncludeHeaders(false);
    middleware.setIncludeBody(true);
    middleware.setMaxBodySize(512);
    
    // Test status code filtering
    middleware.addLogStatusCode(404);
    middleware.addLogStatusCode(500);
    middleware.removeLogStatusCode(500);
    
    // Test path exclusion
    middleware.addExcludePath("/health");
    middleware.addExcludePath("/metrics");
    middleware.removeExcludePath("/health");
    
    // Test error-only mode
    middleware.setLogErrorsOnly(true);
}

// Test 4: JSON Format Logging
TEST_F(LoggingMiddlewareTest, JsonFormatLogging) {
    LoggingMiddleware::LoggingConfig config;
    config.format = LoggingMiddleware::LogFormat::JSON;
    config.includeHeaders = true;
    config.includeBody = true;
    config.includeTimings = true;
    config.logRequests = true;
    config.logResponses = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    auto logs = mockLogger_->getLogs();
    EXPECT_GE(logs.size(), 1);
    
    // Check JSON format (should contain braces)
    bool foundJson = false;
    for (const auto& log : logs) {
        if (log.message.find("{") != std::string::npos && log.message.find("}") != std::string::npos) {
            foundJson = true;
            EXPECT_NE(log.message.find("\"method\":\"GET\""), std::string::npos);
            EXPECT_NE(log.message.find("\"path\":\"/api/users\""), std::string::npos);
            EXPECT_NE(log.message.find("\"user_id\":\"user123\""), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(foundJson);
}

// Test 5: Common Log Format
TEST_F(LoggingMiddlewareTest, CommonLogFormat) {
    LoggingMiddleware::LoggingConfig config;
    config.format = LoggingMiddleware::LogFormat::COMMON;
    config.logRequests = false;
    config.logResponses = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    auto logs = mockLogger_->getLogs();
    EXPECT_GE(logs.size(), 1);
    
    // Check Common Log Format
    bool foundCommon = false;
    for (const auto& log : logs) {
        if (log.message.find("192.168.1.100 - user123") != std::string::npos) {
            foundCommon = true;
            EXPECT_NE(log.message.find("\"GET /api/users"), std::string::npos);
            EXPECT_NE(log.message.find("200"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(foundCommon);
}

// Test 6: Combined Log Format
TEST_F(LoggingMiddlewareTest, CombinedLogFormat) {
    LoggingMiddleware::LoggingConfig config;
    config.format = LoggingMiddleware::LogFormat::COMBINED;
    config.logRequests = false;
    config.logResponses = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    auto logs = mockLogger_->getLogs();
    EXPECT_GE(logs.size(), 1);
    
    // Check Combined Log Format (includes User-Agent)
    bool foundCombined = false;
    for (const auto& log : logs) {
        if (log.message.find("TestAgent/1.0") != std::string::npos) {
            foundCombined = true;
            EXPECT_NE(log.message.find("192.168.1.100 - user123"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(foundCombined);
}

// Test 7: Custom Log Format
TEST_F(LoggingMiddlewareTest, CustomLogFormat) {
    LoggingMiddleware::LoggingConfig config;
    config.format = LoggingMiddleware::LogFormat::CUSTOM;
    config.customFormat = "{timestamp} {method} {path} {status} {duration}";
    config.logRequests = false;
    config.logResponses = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    auto logs = mockLogger_->getLogs();
    EXPECT_GE(logs.size(), 1);
    
    // Check custom format
    bool foundCustom = false;
    for (const auto& log : logs) {
        if (log.message.find("GET /api/users 200") != std::string::npos) {
            foundCustom = true;
            break;  
        }
    }
    EXPECT_TRUE(foundCustom);
}

// Test 8: Custom Formatter Function
TEST_F(LoggingMiddlewareTest, CustomFormatter) {
    LoggingMiddleware middleware(LoggingMiddleware::LoggingConfig{}, mockLogger_);
    
    // Set custom formatter
    middleware.setFormatter([](const LoggingMiddleware::LogEntry& entry) -> std::string {
        return "CUSTOM: " + entry.method + " " + entry.path + " -> " + std::to_string(entry.responseStatus);
    });
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    auto logs = mockLogger_->getLogs();
    EXPECT_GE(logs.size(), 1);
    
    // Check custom formatter output
    bool foundCustomFormatter = false;
    for (const auto& log : logs) {
        if (log.message.find("CUSTOM: GET /api/users -> 200") != std::string::npos) {
            foundCustomFormatter = true;
            break;
        }
    }
    EXPECT_TRUE(foundCustomFormatter);
}

// Test 9: Status Code Filtering
TEST_F(LoggingMiddlewareTest, StatusCodeFiltering) {
    LoggingMiddleware::LoggingConfig config;
    config.logStatusCodes = {404, 500}; // Only log these status codes
    config.logRequests = false;
    config.logResponses = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    // Test with 200 status (should not log)
    auto nextHandler200 = [&](const HttpRequest&, Context&) -> HttpResponse {
        return HttpResponse(200);
    };
    
    mockLogger_->clear();
    HttpResponse result1 = middleware.handle(request_, context_, nextHandler200);
    EXPECT_EQ(result1.getStatus(), 200);
    EXPECT_EQ(mockLogger_->getLogs().size(), 0);
    
    // Test with 404 status (should log)
    auto nextHandler404 = [&](const HttpRequest&, Context&) -> HttpResponse {
        return HttpResponse(404);
    };
    
    mockLogger_->clear();
    HttpResponse result2 = middleware.handle(request_, context_, nextHandler404);
    EXPECT_EQ(result2.getStatus(), 404);
    EXPECT_GE(mockLogger_->getLogs().size(), 1);
}

// Test 10: Path Exclusion
TEST_F(LoggingMiddlewareTest, PathExclusion) {
    LoggingMiddleware::LoggingConfig config;
    config.excludePaths = {"/health", "/metrics"};
    config.logRequests = false;
    config.logResponses = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    // Test excluded path
    HttpRequest healthRequest;
    healthRequest = HttpRequest("GET", "/health", "HTTP/1.1");
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return HttpResponse(200);
    };
    
    mockLogger_->clear();
    HttpResponse result1 = middleware.handle(healthRequest, context_, nextHandler);
    EXPECT_EQ(result1.getStatus(), 200);
    EXPECT_EQ(mockLogger_->getLogs().size(), 0);
    
    // Test non-excluded path
    mockLogger_->clear();
    HttpResponse result2 = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result2.getStatus(), 200);
    EXPECT_GE(mockLogger_->getLogs().size(), 1);
}

// Test 11: Error-Only Logging
TEST_F(LoggingMiddlewareTest, ErrorOnlyLogging) {
    LoggingMiddleware::LoggingConfig config;
    config.logErrorsOnly = true;
    config.logRequests = false;
    config.logResponses = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    // Test success response (should not log)
    auto nextHandlerSuccess = [&](const HttpRequest&, Context&) -> HttpResponse {
        return HttpResponse(200);
    };
    
    mockLogger_->clear();
    HttpResponse result1 = middleware.handle(request_, context_, nextHandlerSuccess);
    EXPECT_EQ(result1.getStatus(), 200);
    EXPECT_EQ(mockLogger_->getLogs().size(), 0);
    
    // Test error response (should log)
    auto nextHandlerError = [&](const HttpRequest&, Context&) -> HttpResponse {
        return HttpResponse(500);
    };
    
    mockLogger_->clear();
    HttpResponse result2 = middleware.handle(request_, context_, nextHandlerError);
    EXPECT_EQ(result2.getStatus(), 500);
    EXPECT_GE(mockLogger_->getLogs().size(), 1);
}

// Test 12: Log Levels
TEST_F(LoggingMiddlewareTest, LogLevels) {
    LoggingMiddleware::LoggingConfig config;
    config.level = LoggingMiddleware::LogLevel::ERROR; // Only log ERROR and above
    config.logRequests = false;
    config.logResponses = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    // Test INFO level response (should not log due to level filter)
    auto nextHandlerInfo = [&](const HttpRequest&, Context&) -> HttpResponse {
        return HttpResponse(200);
    };
    
    mockLogger_->clear();
    HttpResponse result1 = middleware.handle(request_, context_, nextHandlerInfo);
    EXPECT_EQ(result1.getStatus(), 200);
    EXPECT_EQ(mockLogger_->getLogs().size(), 0);
    
    // Test ERROR level response (should log)
    auto nextHandlerError = [&](const HttpRequest&, Context&) -> HttpResponse {
        return HttpResponse(500);
    };
    
    mockLogger_->clear();
    HttpResponse result2 = middleware.handle(request_, context_, nextHandlerError);
    EXPECT_EQ(result2.getStatus(), 500);
    EXPECT_GE(mockLogger_->getLogs().size(), 1);
    
    // Verify log level
    auto logs = mockLogger_->getLogs();
    for (const auto& log : logs) {
        EXPECT_EQ(log.level, LoggingMiddleware::LogLevel::ERROR);
    }
}

// Test 13: Body Size Limiting
TEST_F(LoggingMiddlewareTest, BodySizeLimiting) {
    LoggingMiddleware::LoggingConfig config;
    config.format = LoggingMiddleware::LogFormat::JSON;
    config.includeBody = true;
    config.maxBodySize = 10; // Very small limit
    config.logRequests = true;
    config.logResponses = false;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
    HttpRequest largeBodyRequest;
    largeBodyRequest = HttpRequest("POST", "/api/data", "HTTP/1.1");
    largeBodyRequest.setBody("This is a very long request body that exceeds the limit");
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return HttpResponse(200);
    };
    
    mockLogger_->clear();
    HttpResponse result = middleware.handle(largeBodyRequest, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    auto logs = mockLogger_->getLogs();
    EXPECT_GE(logs.size(), 1);
    
    // Check that body was truncated
    bool foundTruncated = false;
    for (const auto& log : logs) {
        if (log.message.find("truncated") != std::string::npos) {
            foundTruncated = true;
            break;
        }
    }
    EXPECT_TRUE(foundTruncated);
}

// Test 14: Statistics Collection
TEST_F(LoggingMiddlewareTest, StatisticsCollection) {
    LoggingMiddleware middleware(LoggingMiddleware::LoggingConfig{}, mockLogger_);
    
    // Initial statistics should be zero
    auto stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], 0);
    EXPECT_EQ(stats["error_requests"], 0);
    EXPECT_EQ(stats["success_requests"], 0);
    
    // Process some requests
    auto nextHandlerSuccess = [&](const HttpRequest&, Context&) -> HttpResponse {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return HttpResponse(200);
    };
    
    auto nextHandlerError = [&](const HttpRequest&, Context&) -> HttpResponse {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return HttpResponse(500);
    };
    
    // Process requests
    middleware.handle(request_, context_, nextHandlerSuccess);
    middleware.handle(request_, context_, nextHandlerSuccess);
    middleware.handle(request_, context_, nextHandlerError);
    
    // Check statistics
    stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], 3);
    EXPECT_EQ(stats["success_requests"], 2);
    EXPECT_EQ(stats["error_requests"], 1);
    EXPECT_GT(stats["avg_duration_microseconds"], 0);
    
    // Reset statistics
    middleware.resetStatistics();
    stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], 0);
    EXPECT_EQ(stats["error_requests"], 0);
    EXPECT_EQ(stats["success_requests"], 0);
}

// Test 15: Disabled Middleware
TEST_F(LoggingMiddlewareTest, DisabledMiddleware) {
    LoggingMiddleware middleware(LoggingMiddleware::LoggingConfig{}, mockLogger_);
    middleware.setEnabled(false);
    
    auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
        return response_;
    };
    
    mockLogger_->clear();
    HttpResponse result = middleware.handle(request_, context_, nextHandler);
    EXPECT_EQ(result.getStatus(), 200);
    
    // Should not log anything when disabled
    EXPECT_EQ(mockLogger_->getLogs().size(), 0);
    
    // Statistics should not be updated
    auto stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], 0);
}

// Test 16: File Logger
TEST_F(LoggingMiddlewareTest, FileLogger) {
    std::string testFile = "/tmp/test_logging.log";
    
    try {
        auto fileLogger = std::make_shared<FileLogger>(testFile, false);
        LoggingMiddleware middleware(LoggingMiddleware::LoggingConfig{}, fileLogger);
        
        auto nextHandler = [&](const HttpRequest&, Context&) -> HttpResponse {
            return response_;
        };
        
        HttpResponse result = middleware.handle(request_, context_, nextHandler);
        EXPECT_EQ(result.getStatus(), 200);
        
        // Flush and check file
        middleware.flush();
        
        std::ifstream file(testFile);
        EXPECT_TRUE(file.is_open());
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        EXPECT_FALSE(content.empty());
        EXPECT_NE(content.find("GET"), std::string::npos);
        
        // Clean up
        std::remove(testFile.c_str());
        
    } catch (const std::exception& e) {
        // File operations might fail in test environment, skip
        GTEST_SKIP() << "File logger test skipped: " << e.what();
    }
}

// Test 17: Performance Benchmark
TEST_F(LoggingMiddlewareTest, PerformanceBenchmark) {
    LoggingMiddleware::LoggingConfig config;
    config.format = LoggingMiddleware::LogFormat::JSON;
    config.includeHeaders = true;
    config.includeBody = false;
    config.includeTimings = true;
    
    LoggingMiddleware middleware(config, mockLogger_);
    
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
    
    std::cout << "Average time per request: " << avgTimePerRequest << " microseconds" << std::endl;
    
    // Performance should be reasonable (less than 100 microseconds per request)
    EXPECT_LT(avgTimePerRequest, 100.0);
    
    // Check that all requests were processed
    auto stats = middleware.getStatistics();
    EXPECT_EQ(stats["total_requests"], numRequests);
} 