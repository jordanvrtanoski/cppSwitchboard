/**
 * @file http_request.h
 * @brief HTTP request handling and parsing for cppSwitchboard library
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @version 1.0.0
 * @date 2024
 * 
 * This file contains the HttpRequest class and HttpMethod enum for handling
 * HTTP requests in the cppSwitchboard library. It provides comprehensive
 * support for request parsing, header management, body handling, and
 * parameter extraction.
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace cppSwitchboard {

/**
 * @enum HttpMethod
 * @brief HTTP request methods enumeration
 * 
 * Enumeration of supported HTTP methods for request handling.
 * The library supports all standard HTTP methods including REST operations.
 * 
 * @see HttpRequest
 * @since 1.0.0
 */
enum class HttpMethod {
    GET,        ///< HTTP GET method for retrieving resources
    POST,       ///< HTTP POST method for creating resources
    PUT,        ///< HTTP PUT method for updating resources
    DELETE,     ///< HTTP DELETE method for removing resources
    PATCH,      ///< HTTP PATCH method for partial updates
    HEAD,       ///< HTTP HEAD method for metadata retrieval
    OPTIONS     ///< HTTP OPTIONS method for CORS and capability discovery
};

/**
 * @class HttpRequest
 * @brief HTTP request representation and parsing
 * 
 * The HttpRequest class represents an HTTP request with comprehensive
 * support for headers, body content, query parameters, and path parameters.
 * It provides methods for parsing request data and extracting various
 * components of the HTTP request.
 * 
 * Features:
 * - HTTP method handling (GET, POST, PUT, DELETE, etc.)
 * - Header management with case-insensitive access
 * - Body content handling for various content types
 * - Query parameter parsing and access
 * - Path parameter extraction for RESTful routes
 * - HTTP/2 stream ID support
 * - Content type detection and validation
 * 
 * @code{.cpp}
 * // Create and parse a request
 * HttpRequest request("GET", "/api/users/123?sort=name", "HTTP/1.1");
 * 
 * // Access request components
 * std::string method = request.getMethod();        // "GET"
 * std::string path = request.getPath();           // "/api/users/123"
 * std::string sortParam = request.getQueryParam("sort"); // "name"
 * 
 * // Set headers
 * request.setHeader("Content-Type", "application/json");
 * request.setBody("{\"name\": \"John Doe\"}");
 * @endcode
 * 
 * @see HttpResponse, HttpServer, HttpMethod
 * @since 1.0.0
 */
class HttpRequest {
public:
    /**
     * @brief Default constructor
     * 
     * Creates an empty HTTP request with default values.
     * The request will have GET method and empty path by default.
     */
    HttpRequest() = default;
    
    /**
     * @brief Constructor with request line components
     * @param method HTTP method string (e.g., "GET", "POST")
     * @param path Request path (e.g., "/api/users")
     * @param protocol HTTP protocol version (e.g., "HTTP/1.1")
     * @throws std::invalid_argument if method is not recognized
     * 
     * Creates an HTTP request with the specified method, path, and protocol.
     * The method string will be parsed and converted to HttpMethod enum.
     * 
     * @code{.cpp}
     * HttpRequest request("POST", "/api/users", "HTTP/1.1");
     * @endcode
     */
    HttpRequest(const std::string& method, const std::string& path, const std::string& protocol);

    // Basic request information
    
    /**
     * @brief Get the HTTP method as string
     * @return HTTP method string (e.g., "GET", "POST")
     * 
     * Returns the HTTP method as a string representation.
     * This is the original method string provided during construction.
     */
    std::string getMethod() const { return method_; }
    
    /**
     * @brief Get the HTTP method as enum
     * @return HttpMethod enumeration value
     * 
     * Returns the HTTP method as a strongly-typed enum value.
     * This provides type-safe method comparison and switching.
     * 
     * @code{.cpp}
     * if (request.getHttpMethod() == HttpMethod::POST) {
     *     // Handle POST request
     * }
     * @endcode
     */
    HttpMethod getHttpMethod() const { return httpMethod_; }
    
    /**
     * @brief Get the request path
     * @return Request path string
     * 
     * Returns the path component of the request URL, excluding
     * query parameters. For example, "/api/users/123" from
     * "/api/users/123?sort=name".
     */
    std::string getPath() const { return path_; }
    
    /**
     * @brief Get the HTTP protocol version
     * @return Protocol version string (e.g., "HTTP/1.1", "HTTP/2.0")
     * 
     * Returns the HTTP protocol version used for this request.
     */
    std::string getProtocol() const { return protocol_; }
    
    // Headers
    
    /**
     * @brief Get a specific header value
     * @param name Header name (case-insensitive)
     * @return Header value string, empty if not found
     * 
     * Retrieves the value of a specific HTTP header. Header names
     * are matched case-insensitively. Returns empty string if the
     * header is not present.
     * 
     * @code{.cpp}
     * std::string contentType = request.getHeader("Content-Type");
     * std::string userAgent = request.getHeader("user-agent");
     * @endcode
     */
    std::string getHeader(const std::string& name) const;
    
    /**
     * @brief Get all headers
     * @return Map of all header name-value pairs
     * 
     * Returns a copy of all HTTP headers as a map. Header names
     * are stored in their original case.
     * 
     * @code{.cpp}
     * auto headers = request.getHeaders();
     * for (const auto& [name, value] : headers) {
     *     std::cout << name << ": " << value << std::endl;
     * }
     * @endcode
     */
    std::map<std::string, std::string> getHeaders() const { return headers_; }
    
    /**
     * @brief Set a header value
     * @param name Header name
     * @param value Header value
     * 
     * Sets the value of an HTTP header. If the header already exists,
     * it will be replaced with the new value.
     * 
     * @code{.cpp}
     * request.setHeader("Authorization", "Bearer token123");
     * request.setHeader("Content-Type", "application/json");
     * @endcode
     */
    void setHeader(const std::string& name, const std::string& value);
    
    // Body
    
    /**
     * @brief Get the request body
     * @return Request body as string
     * 
     * Returns the HTTP request body content as a string.
     * For binary content, consider the raw bytes may not be
     * properly represented as a string.
     */
    std::string getBody() const { return body_; }
    
    /**
     * @brief Set the request body from string
     * @param body Request body content
     * 
     * Sets the HTTP request body from a string. This is suitable
     * for text-based content like JSON, XML, or form data.
     * 
     * @code{.cpp}
     * request.setBody("{\"name\": \"John\", \"age\": 30}");
     * @endcode
     */
    void setBody(const std::string& body) { body_ = body; }
    
    /**
     * @brief Set the request body from binary data
     * @param body Request body as vector of bytes
     * 
     * Sets the HTTP request body from binary data. This is suitable
     * for file uploads, images, or other binary content.
     * 
     * @code{.cpp}
     * std::vector<uint8_t> imageData = loadImageFile("image.jpg");
     * request.setBody(imageData);
     * @endcode
     */
    void setBody(const std::vector<uint8_t>& body);
    
    // Query parameters
    
    /**
     * @brief Get all query parameters
     * @return Map of all query parameter name-value pairs
     * 
     * Returns all query parameters parsed from the request URL.
     * Query parameters are the key-value pairs after the '?' in the URL.
     * 
     * @code{.cpp}
     * // For URL: /api/users?sort=name&limit=10
     * auto params = request.getQueryParams();
     * // params["sort"] = "name", params["limit"] = "10"
     * @endcode
     */
    std::map<std::string, std::string> getQueryParams() const { return queryParams_; }
    
    /**
     * @brief Get a specific query parameter value
     * @param name Parameter name
     * @return Parameter value string, empty if not found
     * 
     * Retrieves the value of a specific query parameter.
     * Returns empty string if the parameter is not present.
     * 
     * @code{.cpp}
     * std::string sortBy = request.getQueryParam("sort");
     * std::string limit = request.getQueryParam("limit");
     * @endcode
     */
    std::string getQueryParam(const std::string& name) const;
    
    /**
     * @brief Set a query parameter value
     * @param name Parameter name
     * @param value Parameter value
     * 
     * Sets the value of a query parameter. This can be used to
     * programmatically add or modify query parameters.
     * 
     * @code{.cpp}
     * request.setQueryParam("page", "2");
     * request.setQueryParam("per_page", "50");
     * @endcode
     */
    void setQueryParam(const std::string& name, const std::string& value);
    
    // Path parameters (for routes like /users/{id})
    
    /**
     * @brief Get all path parameters
     * @return Map of all path parameter name-value pairs
     * 
     * Returns all path parameters extracted from RESTful route patterns.
     * Path parameters are defined in routes like "/users/{id}" and extracted
     * from actual requests like "/users/123".
     * 
     * @code{.cpp}
     * // Route: "/api/users/{id}/posts/{postId}"
     * // Request: "/api/users/123/posts/456"
     * auto params = request.getPathParams();
     * // params["id"] = "123", params["postId"] = "456"
     * @endcode
     */
    std::map<std::string, std::string> getPathParams() const { return pathParams_; }
    
    /**
     * @brief Get a specific path parameter value
     * @param name Parameter name
     * @return Parameter value string, empty if not found
     * 
     * Retrieves the value of a specific path parameter extracted from
     * the route pattern matching.
     * 
     * @code{.cpp}
     * std::string userId = request.getPathParam("id");
     * std::string postId = request.getPathParam("postId");
     * @endcode
     */
    std::string getPathParam(const std::string& name) const;
    
    /**
     * @brief Set a path parameter value
     * @param name Parameter name
     * @param value Parameter value
     * 
     * Sets the value of a path parameter. This is typically used
     * internally by the routing system during request matching.
     * 
     * @code{.cpp}
     * request.setPathParam("id", "123");
     * request.setPathParam("category", "electronics");
     * @endcode
     */
    void setPathParam(const std::string& name, const std::string& value);
    
    // Protocol-specific information
    
    /**
     * @brief Get the HTTP/2 stream ID
     * @return Stream ID (0 for HTTP/1.1, actual ID for HTTP/2)
     * 
     * Returns the HTTP/2 stream identifier for this request.
     * For HTTP/1.1 requests, this will be 0. For HTTP/2 requests,
     * this is the unique stream ID used for multiplexing.
     */
    int getStreamId() const { return streamId_; }
    
    /**
     * @brief Set the HTTP/2 stream ID
     * @param streamId Stream identifier
     * 
     * Sets the HTTP/2 stream identifier. This is typically used
     * internally by the HTTP/2 implementation.
     */
    void setStreamId(int streamId) { streamId_ = streamId; }
    
    // Content type helpers
    
    /**
     * @brief Get the content type
     * @return Content type string from Content-Type header
     * 
     * Returns the content type from the Content-Type header.
     * This is a convenience method that extracts just the media type
     * without additional parameters like charset.
     * 
     * @code{.cpp}
     * // For header "Content-Type: application/json; charset=utf-8"
     * std::string type = request.getContentType(); // "application/json"
     * @endcode
     */
    std::string getContentType() const;
    
    /**
     * @brief Check if content type is JSON
     * @return True if content type is application/json
     * 
     * Convenience method to check if the request contains JSON data.
     * This checks the Content-Type header for "application/json".
     * 
     * @code{.cpp}
     * if (request.isJson()) {
     *     // Parse JSON body
     *     auto json = parseJsonBody(request.getBody());
     * }
     * @endcode
     */
    bool isJson() const;
    
    /**
     * @brief Check if content type is form data
     * @return True if content type is application/x-www-form-urlencoded or multipart/form-data
     * 
     * Convenience method to check if the request contains form data.
     * This checks for both URL-encoded and multipart form data.
     * 
     * @code{.cpp}
     * if (request.isFormData()) {
     *     // Parse form data
     *     auto formData = parseFormData(request.getBody());
     * }
     * @endcode
     */
    bool isFormData() const;
    
    // Utility methods
    
    /**
     * @brief Parse query string into parameters
     * @param queryString Query string to parse (without leading '?')
     * 
     * Parses a query string and populates the internal query parameters map.
     * The query string should not include the leading '?' character.
     * 
     * @code{.cpp}
     * request.parseQueryString("sort=name&limit=10&active=true");
     * std::string sort = request.getQueryParam("sort"); // "name"
     * @endcode
     */
    void parseQueryString(const std::string& queryString);
    
    /**
     * @brief Convert method string to HttpMethod enum
     * @param method HTTP method string (e.g., "GET", "POST")
     * @return HttpMethod enumeration value
     * @throws std::invalid_argument if method is not recognized
     * 
     * Static utility method to convert HTTP method strings to
     * the corresponding HttpMethod enum value.
     * 
     * @code{.cpp}
     * HttpMethod method = HttpRequest::stringToMethod("POST");
     * // method == HttpMethod::POST
     * @endcode
     */
    static HttpMethod stringToMethod(const std::string& method);
    
    /**
     * @brief Convert HttpMethod enum to string
     * @param method HttpMethod enumeration value
     * @return HTTP method string (e.g., "GET", "POST")
     * 
     * Static utility method to convert HttpMethod enum values to
     * their corresponding string representations.
     * 
     * @code{.cpp}
     * std::string methodStr = HttpRequest::methodToString(HttpMethod::PUT);
     * // methodStr == "PUT"
     * @endcode
     */
    static std::string methodToString(HttpMethod method);

private:
    std::string method_;                                    ///< HTTP method string
    HttpMethod httpMethod_ = HttpMethod::GET;              ///< HTTP method enum
    std::string path_;                                     ///< Request path
    std::string protocol_;                                 ///< HTTP protocol version
    std::map<std::string, std::string> headers_;          ///< HTTP headers
    std::string body_;                                     ///< Request body content
    std::map<std::string, std::string> queryParams_;      ///< Query parameters
    std::map<std::string, std::string> pathParams_;       ///< Path parameters from routing
    int streamId_ = 0;                                     ///< HTTP/2 stream ID (0 for HTTP/1.1)
    
    /**
     * @brief Update HttpMethod enum from method string
     * 
     * Internal method to synchronize the HttpMethod enum with
     * the method string when the method is changed.
     */
    void updateHttpMethod();
};

} // namespace cppSwitchboard 