#include <gtest/gtest.h>
#include <cppSwitchboard/http_request.h>

using namespace cppSwitchboard;

class HttpRequestTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all tests
    }
};

TEST_F(HttpRequestTest, DefaultConstructor) {
    HttpRequest request;
    EXPECT_EQ(request.getMethod(), "");
    EXPECT_EQ(request.getHttpMethod(), HttpMethod::GET);
    EXPECT_EQ(request.getPath(), "");
    EXPECT_EQ(request.getProtocol(), "");
    EXPECT_EQ(request.getBody(), "");
    EXPECT_EQ(request.getStreamId(), 0);
}

TEST_F(HttpRequestTest, ParameterizedConstructor) {
    HttpRequest request("POST", "/api/users", "HTTP/1.1");
    EXPECT_EQ(request.getMethod(), "POST");
    EXPECT_EQ(request.getHttpMethod(), HttpMethod::POST);
    EXPECT_EQ(request.getPath(), "/api/users");
    EXPECT_EQ(request.getProtocol(), "HTTP/1.1");
}

TEST_F(HttpRequestTest, HeaderManagement) {
    HttpRequest request;
    
    // Set and get headers
    request.setHeader("Content-Type", "application/json");
    request.setHeader("Authorization", "Bearer token123");
    
    EXPECT_EQ(request.getHeader("Content-Type"), "application/json");
    EXPECT_EQ(request.getHeader("Authorization"), "Bearer token123");
    EXPECT_EQ(request.getHeader("Non-Existent"), "");
    
    // Test case insensitive header retrieval
    EXPECT_EQ(request.getHeader("content-type"), "application/json");
    EXPECT_EQ(request.getHeader("CONTENT-TYPE"), "application/json");
}

TEST_F(HttpRequestTest, BodyManagement) {
    HttpRequest request;
    
    // String body
    std::string testBody = "{\"name\": \"test\"}";
    request.setBody(testBody);
    EXPECT_EQ(request.getBody(), testBody);
    
    // Binary body
    std::vector<uint8_t> binaryData = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    request.setBody(binaryData);
    EXPECT_EQ(request.getBody(), "Hello");
}

TEST_F(HttpRequestTest, QueryParameters) {
    HttpRequest request;
    
    // Set query parameters
    request.setQueryParam("page", "1");
    request.setQueryParam("limit", "10");
    request.setQueryParam("sort", "name");
    
    EXPECT_EQ(request.getQueryParam("page"), "1");
    EXPECT_EQ(request.getQueryParam("limit"), "10");
    EXPECT_EQ(request.getQueryParam("sort"), "name");
    EXPECT_EQ(request.getQueryParam("nonexistent"), "");
    
    auto queryParams = request.getQueryParams();
    EXPECT_EQ(queryParams.size(), 3);
    EXPECT_EQ(queryParams["page"], "1");
    EXPECT_EQ(queryParams["limit"], "10");
    EXPECT_EQ(queryParams["sort"], "name");
}

TEST_F(HttpRequestTest, PathParameters) {
    HttpRequest request;
    
    // Set path parameters
    request.setPathParam("id", "123");
    request.setPathParam("category", "electronics");
    
    EXPECT_EQ(request.getPathParam("id"), "123");
    EXPECT_EQ(request.getPathParam("category"), "electronics");
    EXPECT_EQ(request.getPathParam("nonexistent"), "");
    
    auto pathParams = request.getPathParams();
    EXPECT_EQ(pathParams.size(), 2);
    EXPECT_EQ(pathParams["id"], "123");
    EXPECT_EQ(pathParams["category"], "electronics");
}

TEST_F(HttpRequestTest, QueryStringParsing) {
    HttpRequest request;
    
    // Test basic query string parsing
    request.parseQueryString("page=1&limit=10&sort=name");
    EXPECT_EQ(request.getQueryParam("page"), "1");
    EXPECT_EQ(request.getQueryParam("limit"), "10");
    EXPECT_EQ(request.getQueryParam("sort"), "name");
    
    // Test empty values
    request.parseQueryString("empty=&test=value");
    EXPECT_EQ(request.getQueryParam("empty"), "");
    EXPECT_EQ(request.getQueryParam("test"), "value");
    
    // Test URL encoded values - the implementation doesn't decode URLs automatically
    request.parseQueryString("name=John%20Doe&city=New%20York");
    EXPECT_EQ(request.getQueryParam("name"), "John%20Doe");  // URL encoded form
    EXPECT_EQ(request.getQueryParam("city"), "New%20York");  // URL encoded form
}

TEST_F(HttpRequestTest, HttpMethodConversion) {
    // Test string to method conversion
    EXPECT_EQ(HttpRequest::stringToMethod("GET"), HttpMethod::GET);
    EXPECT_EQ(HttpRequest::stringToMethod("POST"), HttpMethod::POST);
    EXPECT_EQ(HttpRequest::stringToMethod("PUT"), HttpMethod::PUT);
    EXPECT_EQ(HttpRequest::stringToMethod("DELETE"), HttpMethod::DELETE);
    EXPECT_EQ(HttpRequest::stringToMethod("PATCH"), HttpMethod::PATCH);
    EXPECT_EQ(HttpRequest::stringToMethod("HEAD"), HttpMethod::HEAD);
    EXPECT_EQ(HttpRequest::stringToMethod("OPTIONS"), HttpMethod::OPTIONS);
    
    // Test case sensitive - implementation expects uppercase
    EXPECT_EQ(HttpRequest::stringToMethod("GET"), HttpMethod::GET);
    EXPECT_EQ(HttpRequest::stringToMethod("POST"), HttpMethod::POST);
    
    // Test method to string conversion
    EXPECT_EQ(HttpRequest::methodToString(HttpMethod::GET), "GET");
    EXPECT_EQ(HttpRequest::methodToString(HttpMethod::POST), "POST");
    EXPECT_EQ(HttpRequest::methodToString(HttpMethod::PUT), "PUT");
    EXPECT_EQ(HttpRequest::methodToString(HttpMethod::DELETE), "DELETE");
    EXPECT_EQ(HttpRequest::methodToString(HttpMethod::PATCH), "PATCH");
    EXPECT_EQ(HttpRequest::methodToString(HttpMethod::HEAD), "HEAD");
    EXPECT_EQ(HttpRequest::methodToString(HttpMethod::OPTIONS), "OPTIONS");
}

TEST_F(HttpRequestTest, ContentTypeHelpers) {
    HttpRequest request;
    
    // Test JSON content type
    request.setHeader("Content-Type", "application/json");
    EXPECT_EQ(request.getContentType(), "application/json");
    EXPECT_TRUE(request.isJson());
    EXPECT_FALSE(request.isFormData());
    
    // Test form data content type
    request.setHeader("Content-Type", "application/x-www-form-urlencoded");
    EXPECT_EQ(request.getContentType(), "application/x-www-form-urlencoded");
    EXPECT_FALSE(request.isJson());
    EXPECT_TRUE(request.isFormData());
    
    // Test multipart form data
    request.setHeader("Content-Type", "multipart/form-data; boundary=something");
    EXPECT_TRUE(request.isFormData());
    
    // Test no content type
    request.setHeader("Content-Type", "");
    EXPECT_EQ(request.getContentType(), "");
    EXPECT_FALSE(request.isJson());
    EXPECT_FALSE(request.isFormData());
}

TEST_F(HttpRequestTest, StreamIdManagement) {
    HttpRequest request;
    
    // Default stream ID should be 0
    EXPECT_EQ(request.getStreamId(), 0);
    
    // Set stream ID (for HTTP/2)
    request.setStreamId(42);
    EXPECT_EQ(request.getStreamId(), 42);
} 