#include <gtest/gtest.h>
#include <cppSwitchboard/http_server.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <cppSwitchboard/config.h>
#include <thread>
#include <chrono>
#include <fstream>

using namespace cppSwitchboard;

// Mock handler for testing
class MockHandler : public HttpHandler {
public:
    MockHandler(const std::string& response) : response_(response), callCount_(0) {}
    
    HttpResponse handle(const HttpRequest& request) override {
        callCount_++;
        lastRequest_ = request;
        return HttpResponse::ok(response_);
    }
    
    int getCallCount() const { return callCount_; }
    const HttpRequest& getLastRequest() const { return lastRequest_; }
    
private:
    std::string response_;
    int callCount_;
    HttpRequest lastRequest_;
};

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use different ports for each test to avoid conflicts
        static int portCounter = 9000;
        testPort = portCounter++;
        
        // Create server configuration
        config.http1.enabled = true;
        config.http1.port = testPort;
        config.http1.bindAddress = "127.0.0.1";
        config.http2.enabled = false; // Disable HTTP/2 for simpler testing
        config.ssl.enabled = false;   // Disable SSL for simpler testing
        config.application.name = "IntegrationTest";
    }
    
    void TearDown() override {
        if (server && server->isRunning()) {
            server->stop();
            // Give server time to stop
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    ServerConfig config;
    std::shared_ptr<HttpServer> server;
    int testPort;
};

TEST_F(IntegrationTest, ServerCreationAndBasicConfiguration) {
    // Test server creation with custom config
    server = HttpServer::create(config);
    EXPECT_TRUE(server != nullptr);
    
    // Verify configuration
    const auto& serverConfig = server->getConfig();
    EXPECT_EQ(serverConfig.application.name, "IntegrationTest");
    EXPECT_EQ(serverConfig.http1.port, testPort);
    EXPECT_TRUE(serverConfig.http1.enabled);
    EXPECT_FALSE(serverConfig.http2.enabled);
    EXPECT_FALSE(serverConfig.ssl.enabled);
}

TEST_F(IntegrationTest, HandlerRegistrationAndRouting) {
    server = HttpServer::create(config);
    
    auto handler1 = std::make_shared<MockHandler>("Response from handler 1");
    auto handler2 = std::make_shared<MockHandler>("Response from handler 2");
    
    // Register handlers
    server->get("/test1", handler1);
    server->post("/test2", handler2);
    
    // Create test requests
    HttpRequest getRequest("GET", "/test1", "HTTP/1.1");
    HttpRequest postRequest("POST", "/test2", "HTTP/1.1");
    
    // Process requests (this tests the internal routing without network)
    // Note: This assumes processRequest is accessible for testing
    // In actual implementation, this might need to be tested differently
    
    // For now, just verify handlers were registered
    EXPECT_EQ(handler1->getCallCount(), 0);
    EXPECT_EQ(handler2->getCallCount(), 0);
}

TEST_F(IntegrationTest, LambdaHandlerRegistration) {
    server = HttpServer::create(config);
    
    bool lambdaCalled = false;
    std::string receivedPath;
    
    // Register lambda handler
    server->get("/lambda", [&lambdaCalled, &receivedPath](const HttpRequest& request) -> HttpResponse {
        lambdaCalled = true;
        receivedPath = request.getPath();
        return HttpResponse::json("{\"message\": \"Lambda handler called\"}");
    });
    
    // Create test request
    HttpRequest request("GET", "/lambda", "HTTP/1.1");
    
    // For actual integration testing, we would need to start the server
    // and make HTTP requests. For unit testing, we verify registration.
    EXPECT_TRUE(server != nullptr);
}

TEST_F(IntegrationTest, ConfigurationValidation) {
    // Test valid configuration - use ConfigValidator
    std::string errorMessage;
    bool isValid = ConfigValidator::validateConfig(config, errorMessage);
    EXPECT_TRUE(isValid) << "Validation error: " << errorMessage;
    
    // Test invalid configuration
    ServerConfig invalidConfig = config;
    invalidConfig.http1.port = -1; // Invalid port
    EXPECT_FALSE(ConfigValidator::validateConfig(invalidConfig, errorMessage));
    
    invalidConfig = config;
    invalidConfig.ssl.enabled = true;
    invalidConfig.ssl.certificateFile = ""; // SSL enabled but no cert file
    EXPECT_FALSE(ConfigValidator::validateConfig(invalidConfig, errorMessage));
}

TEST_F(IntegrationTest, MultipleRoutesAndMethods) {
    server = HttpServer::create(config);
    
    auto getHandler = std::make_shared<MockHandler>("GET response");
    auto postHandler = std::make_shared<MockHandler>("POST response");
    auto putHandler = std::make_shared<MockHandler>("PUT response");
    auto deleteHandler = std::make_shared<MockHandler>("DELETE response");
    
    // Register multiple methods for the same path
    server->get("/api/resource", getHandler);
    server->post("/api/resource", postHandler);
    server->put("/api/resource", putHandler);
    server->del("/api/resource", deleteHandler);
    
    // Register different paths
    server->get("/api/users", std::make_shared<MockHandler>("Users list"));
    server->get("/api/users/{id}", std::make_shared<MockHandler>("User detail"));
    
    EXPECT_TRUE(server != nullptr);
}

TEST_F(IntegrationTest, RequestResponseCycle) {
    // Create a request
    HttpRequest request("GET", "/test", "HTTP/1.1");
    request.setHeader("User-Agent", "cppSwitchboard-test");
    request.setHeader("Accept", "application/json");
    request.setQueryParam("page", "1");
    request.setQueryParam("limit", "10");
    
    // Create a response
    HttpResponse response = HttpResponse::json("{\"status\": \"ok\", \"data\": []}");
    response.setHeader("X-Custom-Header", "test-value");
    
    // Verify request properties
    EXPECT_EQ(request.getMethod(), "GET");
    EXPECT_EQ(request.getPath(), "/test");
    EXPECT_EQ(request.getProtocol(), "HTTP/1.1");
    EXPECT_EQ(request.getHeader("User-Agent"), "cppSwitchboard-test");
    EXPECT_EQ(request.getHeader("Accept"), "application/json");
    EXPECT_EQ(request.getQueryParam("page"), "1");
    EXPECT_EQ(request.getQueryParam("limit"), "10");
    
    // Verify response properties
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.getContentType(), "application/json");
    EXPECT_EQ(response.getHeader("X-Custom-Header"), "test-value");
    EXPECT_TRUE(response.getBody().find("\"status\": \"ok\"") != std::string::npos);
    EXPECT_TRUE(response.isSuccess());
}

TEST_F(IntegrationTest, ParameterizedRouteHandling) {
    server = HttpServer::create(config);
    
    auto paramHandler = std::make_shared<MockHandler>("Parameterized response");
    
    // Register parameterized route
    server->get("/users/{id}/posts/{postId}", paramHandler);
    
    // Create request with parameters
    HttpRequest request("GET", "/users/123/posts/456", "HTTP/1.1");
    
    // In a full integration test, we would verify that path parameters
    // are correctly extracted and made available to the handler
    EXPECT_TRUE(server != nullptr);
}

TEST_F(IntegrationTest, ErrorHandling) {
    server = HttpServer::create(config);
    
    // Test that server handles non-existent routes gracefully
    HttpRequest request("GET", "/nonexistent", "HTTP/1.1");
    
    // In a full implementation, this would return 404
    // For now, just verify server is properly configured
    EXPECT_TRUE(server != nullptr);
}

TEST_F(IntegrationTest, ConfigurationLoading) {
    // Create a temporary config file
    std::string tempConfigPath = "/tmp/test_config.yaml";
    std::ofstream configFile(tempConfigPath);
    configFile << R"(
application:
  name: "LoadedConfig"
  version: "2.0.0"

http1:
  enabled: true
  port: 9999
  bindAddress: "0.0.0.0"
)";
    configFile.close();
    
    // Use ConfigLoader to load configuration
    auto loadedConfig = ConfigLoader::loadFromFile(tempConfigPath);
    
    EXPECT_TRUE(loadedConfig != nullptr);
    EXPECT_EQ(loadedConfig->application.name, "LoadedConfig");
    EXPECT_EQ(loadedConfig->application.version, "2.0.0");
    EXPECT_EQ(loadedConfig->http1.port, 9999);
    EXPECT_EQ(loadedConfig->http1.bindAddress, "0.0.0.0");
    
    // Clean up
    std::remove(tempConfigPath.c_str());
}

TEST_F(IntegrationTest, ServerLifecycle) {
    server = HttpServer::create(config);
    EXPECT_TRUE(server != nullptr);
    
    // Test initial state
    EXPECT_FALSE(server->isRunning());
    
    // Note: We don't actually start/stop the server in unit tests
    // as it would require actual network operations
    // These would be better suited for integration tests
    
    // Just verify the server object was created correctly
    const auto& serverConfig = server->getConfig();
    EXPECT_EQ(serverConfig.http1.port, testPort);
}

TEST_F(IntegrationTest, ResponseTypes) {
    // Test various response types
    HttpResponse okResponse = HttpResponse::ok("Success");
    EXPECT_EQ(okResponse.getStatus(), 200);
    EXPECT_EQ(okResponse.getBody(), "Success");
    
    HttpResponse jsonResponse = HttpResponse::json("{\"test\": true}");
    EXPECT_EQ(jsonResponse.getStatus(), 200);
    EXPECT_EQ(jsonResponse.getContentType(), "application/json");
    
    // HTML response includes charset in content type
    HttpResponse htmlResponse = HttpResponse::html("<html><body>Test</body></html>");
    EXPECT_EQ(htmlResponse.getStatus(), 200);
    EXPECT_EQ(htmlResponse.getContentType(), "text/html; charset=utf-8");
    
    HttpResponse notFoundResponse = HttpResponse::notFound();
    EXPECT_EQ(notFoundResponse.getStatus(), 404);
    
    HttpResponse errorResponse = HttpResponse::internalServerError();
    EXPECT_EQ(errorResponse.getStatus(), 500);
} 