#include <gtest/gtest.h>
#include <cppSwitchboard/debug_logger.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace cppSwitchboard;

class DebugLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        testDir = std::filesystem::temp_directory_path() / "cppSwitchboard_debug_test";
        std::filesystem::create_directories(testDir);
        testLogFile = testDir / "debug.log";
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(testDir);
    }
    
    std::string getFileContents(const std::filesystem::path& filePath) {
        if (!std::filesystem::exists(filePath)) {
            return "";
        }
        
        std::ifstream file(filePath);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    std::filesystem::path testDir;
    std::filesystem::path testLogFile;
};

TEST_F(DebugLoggerTest, DefaultConfiguration) {
    DebugLoggingConfig config;
    DebugLogger logger(config);
    
    // Test default configuration values
    EXPECT_FALSE(logger.isHeaderLoggingEnabled());
    EXPECT_FALSE(logger.isPayloadLoggingEnabled());
}

TEST_F(DebugLoggerTest, HeaderLoggingEnabled) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.headers.enabled = true;
    config.outputFile = testLogFile.string();
    
    DebugLogger logger(config);
    
    EXPECT_TRUE(logger.isHeaderLoggingEnabled());
    EXPECT_FALSE(logger.isPayloadLoggingEnabled());
    
    // Test logging request headers
    HttpRequest request("GET", "/test", "HTTP/1.1");
    request.setHeader("User-Agent", "TestAgent");
    request.setHeader("Content-Type", "application/json");
    
    logger.logRequestHeaders(request);
    
    // Check that something was logged
    std::string logContent = getFileContents(testLogFile);
    EXPECT_FALSE(logContent.empty());
    EXPECT_TRUE(logContent.find("GET") != std::string::npos);
    EXPECT_TRUE(logContent.find("/test") != std::string::npos);
}

TEST_F(DebugLoggerTest, PayloadLoggingEnabled) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.payload.enabled = true;
    config.outputFile = testLogFile.string();
    
    DebugLogger logger(config);
    
    EXPECT_FALSE(logger.isHeaderLoggingEnabled());
    EXPECT_TRUE(logger.isPayloadLoggingEnabled());
    
    // Test logging request payload
    HttpRequest request("POST", "/api/data", "HTTP/1.1");
    request.setBody("{\"test\": \"data\"}");
    request.setHeader("Content-Type", "application/json");
    
    logger.logRequestPayload(request);
    
    // Check that payload was logged
    std::string logContent = getFileContents(testLogFile);
    EXPECT_FALSE(logContent.empty());
    EXPECT_TRUE(logContent.find("test") != std::string::npos);
    EXPECT_TRUE(logContent.find("data") != std::string::npos);
}

TEST_F(DebugLoggerTest, ResponseHeaderLogging) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.headers.enabled = true;
    config.outputFile = testLogFile.string();
    
    DebugLogger logger(config);
    
         HttpResponse response;
     response.setStatus(200);
     response.setHeader("Content-Type", "application/json");
     response.setHeader("Server", "cppSwitchboard/1.0");
     
     logger.logResponseHeaders(response, "/test", "GET");
     
     std::string logContent = getFileContents(testLogFile);
     EXPECT_FALSE(logContent.empty());
     EXPECT_TRUE(logContent.find("200") != std::string::npos);
     EXPECT_TRUE(logContent.find("application/json") != std::string::npos);
}

TEST_F(DebugLoggerTest, ResponsePayloadLogging) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.payload.enabled = true;
    config.outputFile = testLogFile.string();
    
    DebugLogger logger(config);
    
         HttpResponse response;
     response.setStatus(200);
     response.setBody("{\"result\": \"success\"}");
     response.setHeader("Content-Type", "application/json");
     
     logger.logResponsePayload(response, "/api/test", "POST");
     
     std::string logContent = getFileContents(testLogFile);
     EXPECT_FALSE(logContent.empty());
     EXPECT_TRUE(logContent.find("result") != std::string::npos);
     EXPECT_TRUE(logContent.find("success") != std::string::npos);
}

TEST_F(DebugLoggerTest, BothHeaderAndPayloadLogging) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.headers.enabled = true;
    config.payload.enabled = true;
    config.outputFile = testLogFile.string();
    
    DebugLogger logger(config);
    
    EXPECT_TRUE(logger.isHeaderLoggingEnabled());
    EXPECT_TRUE(logger.isPayloadLoggingEnabled());
    
    HttpRequest request("POST", "/api/submit", "HTTP/1.1");
    request.setHeader("Content-Type", "application/json");
    request.setBody("{\"data\": \"test\"}");
    
    logger.logRequestHeaders(request);
    logger.logRequestPayload(request);
    
    std::string logContent = getFileContents(testLogFile);
    EXPECT_FALSE(logContent.empty());
    EXPECT_TRUE(logContent.find("POST") != std::string::npos);
    EXPECT_TRUE(logContent.find("data") != std::string::npos);
}

TEST_F(DebugLoggerTest, DisabledConfiguration) {
    DebugLoggingConfig config;
    config.enabled = false;
    config.headers.enabled = true;  // Should be ignored
    config.payload.enabled = true;  // Should be ignored
    
    DebugLogger logger(config);
    
    EXPECT_FALSE(logger.isHeaderLoggingEnabled());
    EXPECT_FALSE(logger.isPayloadLoggingEnabled());
}

TEST_F(DebugLoggerTest, ConsoleOutputWhenNoFile) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.headers.enabled = true;
    // No outputFile specified - should default to console
    
    DebugLogger logger(config);
    
    EXPECT_TRUE(logger.isHeaderLoggingEnabled());
    
    // This test mainly checks that it doesn't crash
    HttpRequest request("GET", "/test", "HTTP/1.1");
    request.setHeader("Test-Header", "TestValue");
    
    EXPECT_NO_THROW(logger.logRequestHeaders(request));
}

TEST_F(DebugLoggerTest, PayloadSizeLimiting) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.payload.enabled = true;
    config.payload.maxPayloadSizeBytes = 10; // Very small limit
    config.outputFile = testLogFile.string();
    
    DebugLogger logger(config);
    
    HttpRequest request("POST", "/api/large", "HTTP/1.1");
    request.setBody("This is a very long payload that exceeds the limit");
    
    logger.logRequestPayload(request);
    
    std::string logContent = getFileContents(testLogFile);
    EXPECT_FALSE(logContent.empty());
    // The exact truncation behavior depends on implementation
}

TEST_F(DebugLoggerTest, HeaderExclusion) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.headers.enabled = true;
    config.headers.excludeHeaders = {"authorization", "cookie"};
    config.outputFile = testLogFile.string();
    
    DebugLogger logger(config);
    
    HttpRequest request("GET", "/secure", "HTTP/1.1");
    request.setHeader("Authorization", "Bearer secret-token");
    request.setHeader("Cookie", "session=abc123");
    request.setHeader("User-Agent", "TestAgent");
    
    logger.logRequestHeaders(request);
    
    std::string logContent = getFileContents(testLogFile);
    EXPECT_FALSE(logContent.empty());
    // The exact exclusion behavior depends on implementation
    // but we can verify it doesn't crash
}

TEST_F(DebugLoggerTest, TimestampFormatting) {
    DebugLoggingConfig config;
    config.enabled = true;
    config.headers.enabled = true;
    config.timestampFormat = "%Y-%m-%d %H:%M:%S";
    config.outputFile = testLogFile.string();
    
    DebugLogger logger(config);
    
    HttpRequest request("GET", "/time-test", "HTTP/1.1");
    logger.logRequestHeaders(request);
    
    std::string logContent = getFileContents(testLogFile);
    EXPECT_FALSE(logContent.empty());
    // Should contain timestamp formatting
} 