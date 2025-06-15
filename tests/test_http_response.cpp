#include <gtest/gtest.h>
#include <cppSwitchboard/http_response.h>

using namespace cppSwitchboard;

class HttpResponseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all tests
    }
};

TEST_F(HttpResponseTest, DefaultConstructor) {
    HttpResponse response;
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.getBody(), "");
    EXPECT_EQ(response.getContentLength(), 0);
    EXPECT_TRUE(response.isSuccess());
}

TEST_F(HttpResponseTest, StatusConstructor) {
    HttpResponse response(404);
    EXPECT_EQ(response.getStatus(), 404);
    EXPECT_TRUE(response.isClientError());
    EXPECT_FALSE(response.isSuccess());
}

TEST_F(HttpResponseTest, StatusManagement) {
    HttpResponse response;
    
    response.setStatus(201);
    EXPECT_EQ(response.getStatus(), 201);
    EXPECT_TRUE(response.isSuccess());
    
    response.setStatus(404);
    EXPECT_EQ(response.getStatus(), 404);
    EXPECT_TRUE(response.isClientError());
    
    response.setStatus(500);
    EXPECT_EQ(response.getStatus(), 500);
    EXPECT_TRUE(response.isServerError());
}

TEST_F(HttpResponseTest, HeaderManagement) {
    HttpResponse response;
    
    // Set and get headers
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Cache-Control", "no-cache");
    
    EXPECT_EQ(response.getHeader("Content-Type"), "application/json");
    EXPECT_EQ(response.getHeader("Cache-Control"), "no-cache");
    EXPECT_EQ(response.getHeader("Non-Existent"), "");
    
    // Remove header
    response.removeHeader("Cache-Control");
    EXPECT_EQ(response.getHeader("Cache-Control"), "");
    
    // Get all headers
    auto headers = response.getHeaders();
    EXPECT_EQ(headers.size(), 1);
    EXPECT_EQ(headers["Content-Type"], "application/json");
}

TEST_F(HttpResponseTest, ContentTypeManagement) {
    HttpResponse response;
    
    response.setContentType("text/html");
    EXPECT_EQ(response.getContentType(), "text/html");
    EXPECT_EQ(response.getHeader("Content-Type"), "text/html");
    
    response.setContentType("application/json; charset=utf-8");
    EXPECT_EQ(response.getContentType(), "application/json; charset=utf-8");
}

TEST_F(HttpResponseTest, BodyManagement) {
    HttpResponse response;
    
    // String body
    std::string testBody = "{\"message\": \"Hello World\"}";
    response.setBody(testBody);
    EXPECT_EQ(response.getBody(), testBody);
    EXPECT_EQ(response.getContentLength(), testBody.length());
    
    // Binary body
    std::vector<uint8_t> binaryData = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    response.setBody(binaryData);
    EXPECT_EQ(response.getBody(), "Hello");
    EXPECT_EQ(response.getContentLength(), 5);
    
    // Append body
    response.setBody("Initial");
    response.appendBody(" Content");
    EXPECT_EQ(response.getBody(), "Initial Content");
    EXPECT_EQ(response.getContentLength(), 15);
}

TEST_F(HttpResponseTest, ConvenienceStaticMethods) {
    // Test ok() method
    auto okResponse = HttpResponse::ok("Success!");
    EXPECT_EQ(okResponse.getStatus(), 200);
    EXPECT_EQ(okResponse.getBody(), "Success!");
    EXPECT_EQ(okResponse.getContentType(), "text/plain");
    EXPECT_TRUE(okResponse.isSuccess());
    
    // Test json() method
    auto jsonResponse = HttpResponse::json("{\"status\": \"ok\"}");
    EXPECT_EQ(jsonResponse.getStatus(), 200);
    EXPECT_EQ(jsonResponse.getBody(), "{\"status\": \"ok\"}");
    EXPECT_EQ(jsonResponse.getContentType(), "application/json");
    EXPECT_TRUE(jsonResponse.isSuccess());
    
    // Test html() method - implementation includes charset
    auto htmlResponse = HttpResponse::html("<h1>Hello</h1>");
    EXPECT_EQ(htmlResponse.getStatus(), 200);
    EXPECT_EQ(htmlResponse.getBody(), "<h1>Hello</h1>");
    EXPECT_EQ(htmlResponse.getContentType(), "text/html; charset=utf-8");
    EXPECT_TRUE(htmlResponse.isSuccess());
    
    // Test notFound() method - implementation returns JSON format
    auto notFoundResponse = HttpResponse::notFound();
    EXPECT_EQ(notFoundResponse.getStatus(), 404);
    EXPECT_EQ(notFoundResponse.getBody(), "{\"error\": \"Not Found\"}");
    EXPECT_TRUE(notFoundResponse.isClientError());
    
    auto customNotFoundResponse = HttpResponse::notFound("Custom not found message");
    EXPECT_EQ(customNotFoundResponse.getStatus(), 404);
    EXPECT_EQ(customNotFoundResponse.getBody(), "{\"error\": \"Custom not found message\"}");
    
    // Test badRequest() method - implementation returns JSON format
    auto badRequestResponse = HttpResponse::badRequest();
    EXPECT_EQ(badRequestResponse.getStatus(), 400);
    EXPECT_EQ(badRequestResponse.getBody(), "{\"error\": \"Bad Request\"}");
    EXPECT_TRUE(badRequestResponse.isClientError());
    
    // Test internalServerError() method - implementation returns JSON format
    auto serverErrorResponse = HttpResponse::internalServerError();
    EXPECT_EQ(serverErrorResponse.getStatus(), 500);
    EXPECT_EQ(serverErrorResponse.getBody(), "{\"error\": \"Internal Server Error\"}");
    EXPECT_TRUE(serverErrorResponse.isServerError());
    
    // Test methodNotAllowed() method - implementation returns JSON format
    auto methodNotAllowedResponse = HttpResponse::methodNotAllowed();
    EXPECT_EQ(methodNotAllowedResponse.getStatus(), 405);
    EXPECT_EQ(methodNotAllowedResponse.getBody(), "{\"error\": \"Method Not Allowed\"}");
    EXPECT_TRUE(methodNotAllowedResponse.isClientError());
}

TEST_F(HttpResponseTest, StatusCodeHelpers) {
    HttpResponse response;
    
    // Test success status codes (200-299)
    response.setStatus(200);
    EXPECT_TRUE(response.isSuccess());
    EXPECT_FALSE(response.isRedirect());
    EXPECT_FALSE(response.isClientError());
    EXPECT_FALSE(response.isServerError());
    
    response.setStatus(201);
    EXPECT_TRUE(response.isSuccess());
    
    response.setStatus(299);
    EXPECT_TRUE(response.isSuccess());
    
    // Test redirect status codes (300-399)
    response.setStatus(301);
    EXPECT_FALSE(response.isSuccess());
    EXPECT_TRUE(response.isRedirect());
    EXPECT_FALSE(response.isClientError());
    EXPECT_FALSE(response.isServerError());
    
    response.setStatus(302);
    EXPECT_TRUE(response.isRedirect());
    
    // Test client error status codes (400-499)
    response.setStatus(400);
    EXPECT_FALSE(response.isSuccess());
    EXPECT_FALSE(response.isRedirect());
    EXPECT_TRUE(response.isClientError());
    EXPECT_FALSE(response.isServerError());
    
    response.setStatus(404);
    EXPECT_TRUE(response.isClientError());
    
    response.setStatus(499);
    EXPECT_TRUE(response.isClientError());
    
    // Test server error status codes (500-599)
    response.setStatus(500);
    EXPECT_FALSE(response.isSuccess());
    EXPECT_FALSE(response.isRedirect());
    EXPECT_FALSE(response.isClientError());
    EXPECT_TRUE(response.isServerError());
    
    response.setStatus(503);
    EXPECT_TRUE(response.isServerError());
}

TEST_F(HttpResponseTest, StatusCodeConstants) {
    // Test that the constants have correct values
    EXPECT_EQ(HttpResponse::OK, 200);
    EXPECT_EQ(HttpResponse::CREATED, 201);
    EXPECT_EQ(HttpResponse::NO_CONTENT, 204);
    EXPECT_EQ(HttpResponse::BAD_REQUEST, 400);
    EXPECT_EQ(HttpResponse::UNAUTHORIZED, 401);
    EXPECT_EQ(HttpResponse::FORBIDDEN, 403);
    EXPECT_EQ(HttpResponse::NOT_FOUND, 404);
    EXPECT_EQ(HttpResponse::METHOD_NOT_ALLOWED, 405);
    EXPECT_EQ(HttpResponse::INTERNAL_SERVER_ERROR, 500);
    EXPECT_EQ(HttpResponse::NOT_IMPLEMENTED, 501);
    EXPECT_EQ(HttpResponse::SERVICE_UNAVAILABLE, 503);
}

TEST_F(HttpResponseTest, ContentLengthAutoCalculation) {
    HttpResponse response;
    
    // Test that content length is automatically calculated
    response.setBody("Hello");
    EXPECT_EQ(response.getContentLength(), 5);
    
    response.setBody("A longer message with more content");
    EXPECT_EQ(response.getContentLength(), 34);
    
    response.setBody("");
    EXPECT_EQ(response.getContentLength(), 0);
    
    // Test with unicode characters
    response.setBody("Hello 世界");
    EXPECT_GT(response.getContentLength(), 8); // UTF-8 encoding makes it longer
} 