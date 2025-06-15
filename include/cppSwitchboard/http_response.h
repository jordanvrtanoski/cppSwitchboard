/**
 * @file http_response.h
 * @brief HTTP response handling and generation for cppSwitchboard library
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @version 1.0.0
 * @date 2024
 * 
 * This file contains the HttpResponse class for handling HTTP responses in the
 * cppSwitchboard library. It provides comprehensive support for response generation,
 * header management, body content handling, and convenience methods for common
 * HTTP status codes and response types.
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace cppSwitchboard {

/**
 * @class HttpResponse
 * @brief HTTP response representation and generation
 * 
 * The HttpResponse class represents an HTTP response with comprehensive
 * support for status codes, headers, body content, and various response
 * types. It provides both low-level access to response components and
 * high-level convenience methods for common response patterns.
 * 
 * Features:
 * - HTTP status code management with predefined constants
 * - Header management with case-insensitive access
 * - Body content handling for text and binary data
 * - Convenience methods for common response types (JSON, HTML, plain text)
 * - Status code classification helpers (success, error, redirect, etc.)
 * - Automatic content-length calculation
 * 
 * @code{.cpp}
 * // Create various response types
 * auto okResponse = HttpResponse::ok("Hello, World!");
 * auto jsonResponse = HttpResponse::json("{\"status\": \"success\"}");
 * auto errorResponse = HttpResponse::notFound("Page not found");
 * 
 * // Customize response
 * HttpResponse response(200);
 * response.setContentType("application/json");
 * response.setBody("{\"message\": \"Custom response\"}");
 * response.setHeader("Cache-Control", "no-cache");
 * @endcode
 * 
 * @see HttpRequest, HttpServer
 * @since 1.0.0
 */
class HttpResponse {
public:
    /**
     * @brief Default constructor
     * 
     * Creates an HTTP response with default status code 200 (OK)
     * and empty headers and body.
     */
    HttpResponse() = default;
    
    /**
     * @brief Constructor with status code
     * @param status HTTP status code (e.g., 200, 404, 500)
     * 
     * Creates an HTTP response with the specified status code.
     * Headers and body are initialized as empty.
     * 
     * @code{.cpp}
     * HttpResponse response(404);  // Not Found response
     * HttpResponse success(200);   // OK response
     * @endcode
     */
    HttpResponse(int status);

    // Status
    
    /**
     * @brief Get the HTTP status code
     * @return HTTP status code (e.g., 200, 404, 500)
     * 
     * Returns the current HTTP status code for this response.
     * 
     * @code{.cpp}
     * HttpResponse response(404);
     * int status = response.getStatus(); // Returns 404
     * @endcode
     */
    int getStatus() const { return status_; }
    
    /**
     * @brief Set the HTTP status code
     * @param status HTTP status code to set
     * 
     * Sets the HTTP status code for this response. Use the predefined
     * constants (OK, NOT_FOUND, etc.) or standard HTTP status codes.
     * 
     * @code{.cpp}
     * response.setStatus(HttpResponse::NOT_FOUND);
     * response.setStatus(500);
     * @endcode
     */
    void setStatus(int status) { status_ = status; }
    
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
     * std::string contentType = response.getHeader("Content-Type");
     * std::string cacheControl = response.getHeader("cache-control");
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
     * auto headers = response.getHeaders();
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
     * it will be replaced with the new value. Content-Length is
     * automatically managed when the body is modified.
     * 
     * @code{.cpp}
     * response.setHeader("Content-Type", "application/json");
     * response.setHeader("Cache-Control", "max-age=3600");
     * response.setHeader("Access-Control-Allow-Origin", "*");
     * @endcode
     */
    void setHeader(const std::string& name, const std::string& value);
    
    /**
     * @brief Remove a header
     * @param name Header name to remove
     * 
     * Removes the specified header from the response if it exists.
     * Header names are matched case-insensitively.
     * 
     * @code{.cpp}
     * response.removeHeader("X-Custom-Header");
     * response.removeHeader("Cache-Control");
     * @endcode
     */
    void removeHeader(const std::string& name);
    
    // Content type helper
    
    /**
     * @brief Set the content type
     * @param contentType Content type string (e.g., "application/json")
     * 
     * Convenience method to set the Content-Type header.
     * This is equivalent to setHeader("Content-Type", contentType).
     * 
     * @code{.cpp}
     * response.setContentType("application/json");
     * response.setContentType("text/html; charset=utf-8");
     * response.setContentType("image/png");
     * @endcode
     */
    void setContentType(const std::string& contentType);
    
    /**
     * @brief Get the content type
     * @return Content type string from Content-Type header
     * 
     * Convenience method to get the Content-Type header value.
     * Returns empty string if Content-Type header is not set.
     * 
     * @code{.cpp}
     * response.setContentType("application/json");
     * std::string type = response.getContentType(); // "application/json"
     * @endcode
     */
    std::string getContentType() const;
    
    // Body
    
    /**
     * @brief Get the response body
     * @return Response body as string
     * 
     * Returns the HTTP response body content as a string.
     * For binary content, consider that raw bytes may not be
     * properly represented as a string.
     */
    std::string getBody() const { return body_; }
    
    /**
     * @brief Set the response body from string
     * @param body Response body content
     * 
     * Sets the HTTP response body from a string. This automatically
     * updates the Content-Length header. Suitable for text-based
     * content like JSON, XML, HTML, or plain text.
     * 
     * @code{.cpp}
     * response.setBody("{\"status\": \"success\", \"data\": [1,2,3]}");
     * response.setBody("<html><body><h1>Hello</h1></body></html>");
     * response.setBody("Plain text response");
     * @endcode
     */
    void setBody(const std::string& body);
    
    /**
     * @brief Set the response body from binary data
     * @param body Response body as vector of bytes
     * 
     * Sets the HTTP response body from binary data. This automatically
     * updates the Content-Length header. Suitable for file downloads,
     * images, or other binary content.
     * 
     * @code{.cpp}
     * std::vector<uint8_t> imageData = loadImageFile("logo.png");
     * response.setBody(imageData);
     * response.setContentType("image/png");
     * @endcode
     */
    void setBody(const std::vector<uint8_t>& body);
    
    /**
     * @brief Append data to the response body
     * @param data Data to append to the body
     * 
     * Appends the specified data to the existing response body.
     * This automatically updates the Content-Length header.
     * Useful for streaming responses or building responses incrementally.
     * 
     * @code{.cpp}
     * response.setBody("Initial content\\n");
     * response.appendBody("Additional line 1\\n");
     * response.appendBody("Additional line 2\\n");
     * @endcode
     */
    void appendBody(const std::string& data);
    
    // Content length (automatically calculated)
    
    /**
     * @brief Get the content length
     * @return Content length in bytes
     * 
     * Returns the length of the response body in bytes.
     * This value is automatically calculated and kept in sync
     * with the actual body content.
     * 
     * @code{.cpp}
     * response.setBody("Hello, World!");
     * size_t length = response.getContentLength(); // Returns 13
     * @endcode
     */
    size_t getContentLength() const { return body_.length(); }
    
    // Convenience methods for common responses
    
    /**
     * @brief Create an OK (200) response
     * @param body Response body content (default: empty)
     * @param contentType Content type (default: "text/plain")
     * @return HttpResponse with status 200
     * 
     * Creates a successful HTTP response with status code 200 (OK).
     * This is the most common success response type.
     * 
     * @code{.cpp}
     * auto response1 = HttpResponse::ok("Success!");
     * auto response2 = HttpResponse::ok("Data processed", "text/plain");
     * auto response3 = HttpResponse::ok(); // Empty OK response
     * @endcode
     */
    static HttpResponse ok(const std::string& body = "", const std::string& contentType = "text/plain");
    
    /**
     * @brief Create a JSON response
     * @param jsonBody JSON content as string
     * @return HttpResponse with status 200 and application/json content type
     * 
     * Creates a successful HTTP response with JSON content.
     * Automatically sets the Content-Type to "application/json".
     * 
     * @code{.cpp}
     * auto response = HttpResponse::json("{\"status\": \"success\", \"id\": 123}");
     * auto errorResponse = HttpResponse::json("{\"error\": \"Invalid input\"}");
     * @endcode
     */
    static HttpResponse json(const std::string& jsonBody);
    
    /**
     * @brief Create an HTML response
     * @param htmlBody HTML content as string
     * @return HttpResponse with status 200 and text/html content type
     * 
     * Creates a successful HTTP response with HTML content.
     * Automatically sets the Content-Type to "text/html; charset=utf-8".
     * 
     * @code{.cpp}
     * std::string html = "<html><body><h1>Welcome</h1></body></html>";
     * auto response = HttpResponse::html(html);
     * @endcode
     */
    static HttpResponse html(const std::string& htmlBody);
    
    /**
     * @brief Create a Not Found (404) response
     * @param message Error message (default: "Not Found")
     * @return HttpResponse with status 404
     * 
     * Creates a client error response indicating that the requested
     * resource could not be found on the server.
     * 
     * @code{.cpp}
     * auto response1 = HttpResponse::notFound();
     * auto response2 = HttpResponse::notFound("Page does not exist");
     * auto response3 = HttpResponse::notFound("User not found");
     * @endcode
     */
    static HttpResponse notFound(const std::string& message = "Not Found");
    
    /**
     * @brief Create a Bad Request (400) response
     * @param message Error message (default: "Bad Request")
     * @return HttpResponse with status 400
     * 
     * Creates a client error response indicating that the request
     * was malformed or invalid.
     * 
     * @code{.cpp}
     * auto response1 = HttpResponse::badRequest();
     * auto response2 = HttpResponse::badRequest("Invalid JSON format");
     * auto response3 = HttpResponse::badRequest("Missing required field");
     * @endcode
     */
    static HttpResponse badRequest(const std::string& message = "Bad Request");
    
    /**
     * @brief Create an Internal Server Error (500) response
     * @param message Error message (default: "Internal Server Error")
     * @return HttpResponse with status 500
     * 
     * Creates a server error response indicating that the server
     * encountered an unexpected condition that prevented it from
     * fulfilling the request.
     * 
     * @code{.cpp}
     * auto response1 = HttpResponse::internalServerError();
     * auto response2 = HttpResponse::internalServerError("Database connection failed");
     * auto response3 = HttpResponse::internalServerError("Unexpected error occurred");
     * @endcode
     */
    static HttpResponse internalServerError(const std::string& message = "Internal Server Error");
    
    /**
     * @brief Create a Method Not Allowed (405) response
     * @param message Error message (default: "Method Not Allowed")
     * @return HttpResponse with status 405
     * 
     * Creates a client error response indicating that the HTTP method
     * used in the request is not supported for the requested resource.
     * 
     * @code{.cpp}
     * auto response1 = HttpResponse::methodNotAllowed();
     * auto response2 = HttpResponse::methodNotAllowed("POST not supported for this endpoint");
     * @endcode
     */
    static HttpResponse methodNotAllowed(const std::string& message = "Method Not Allowed");
    
    // Status code helpers
    
    /**
     * @brief Check if response indicates success
     * @return True if status code is in the 2xx range
     * 
     * Returns true if the response status code indicates success
     * (200-299 range). This includes OK, Created, Accepted, etc.
     * 
     * @code{.cpp}
     * auto response = HttpResponse::ok("Success");
     * if (response.isSuccess()) {
     *     // Handle successful response
     * }
     * @endcode
     */
    bool isSuccess() const { return status_ >= 200 && status_ < 300; }
    
    /**
     * @brief Check if response indicates redirection
     * @return True if status code is in the 3xx range
     * 
     * Returns true if the response status code indicates redirection
     * (300-399 range). This includes Found, Moved Permanently, etc.
     * 
     * @code{.cpp}
     * HttpResponse response(301);
     * if (response.isRedirect()) {
     *     // Handle redirect response
     * }
     * @endcode
     */
    bool isRedirect() const { return status_ >= 300 && status_ < 400; }
    
    /**
     * @brief Check if response indicates client error
     * @return True if status code is in the 4xx range
     * 
     * Returns true if the response status code indicates a client error
     * (400-499 range). This includes Bad Request, Not Found, Unauthorized, etc.
     * 
     * @code{.cpp}
     * auto response = HttpResponse::notFound();
     * if (response.isClientError()) {
     *     // Handle client error
     * }
     * @endcode
     */
    bool isClientError() const { return status_ >= 400 && status_ < 500; }
    
    /**
     * @brief Check if response indicates server error
     * @return True if status code is in the 5xx range
     * 
     * Returns true if the response status code indicates a server error
     * (500-599 range). This includes Internal Server Error, Service Unavailable, etc.
     * 
     * @code{.cpp}
     * auto response = HttpResponse::internalServerError();
     * if (response.isServerError()) {
     *     // Handle server error
     * }
     * @endcode
     */
    bool isServerError() const { return status_ >= 500 && status_ < 600; }
    
    // Common status codes
    
    /** @brief HTTP 200 OK - Standard response for successful HTTP requests */
    static constexpr int OK = 200;
    
    /** @brief HTTP 201 Created - Request has been fulfilled and new resource created */
    static constexpr int CREATED = 201;
    
    /** @brief HTTP 204 No Content - Request processed successfully, no content to return */
    static constexpr int NO_CONTENT = 204;
    
    /** @brief HTTP 400 Bad Request - Server cannot process request due to client error */
    static constexpr int BAD_REQUEST = 400;
    
    /** @brief HTTP 401 Unauthorized - Authentication is required and has failed */
    static constexpr int UNAUTHORIZED = 401;
    
    /** @brief HTTP 403 Forbidden - Server understood request but refuses to authorize it */
    static constexpr int FORBIDDEN = 403;
    
    /** @brief HTTP 404 Not Found - Requested resource could not be found */
    static constexpr int NOT_FOUND = 404;
    
    /** @brief HTTP 405 Method Not Allowed - Request method not supported for resource */
    static constexpr int METHOD_NOT_ALLOWED = 405;
    
    /** @brief HTTP 500 Internal Server Error - Generic server error message */
    static constexpr int INTERNAL_SERVER_ERROR = 500;
    
    /** @brief HTTP 501 Not Implemented - Server does not support functionality required */
    static constexpr int NOT_IMPLEMENTED = 501;
    
    /** @brief HTTP 503 Service Unavailable - Server is currently unavailable */
    static constexpr int SERVICE_UNAVAILABLE = 503;

private:
    int status_ = 200;                                     ///< HTTP status code
    std::map<std::string, std::string> headers_;          ///< HTTP headers
    std::string body_;                                     ///< Response body content
    
    /**
     * @brief Update Content-Length header based on body size
     * 
     * Internal method to automatically update the Content-Length header
     * when the response body is modified. This ensures the header
     * always reflects the actual body size.
     */
    void updateContentLength();
};

} // namespace cppSwitchboard 