/**
 * @file debug_logger.h
 * @brief Debug logging functionality for HTTP requests and responses
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.2.0
 * 
 * This file provides comprehensive debug logging capabilities for HTTP requests
 * and responses. The DebugLogger class allows detailed logging of headers, payloads,
 * and timing information with configurable filtering and output options.
 * 
 * @section debug_example Debug Logger Usage Example
 * @code{.cpp}
 * // Configure debug logging
 * DebugLoggingConfig config;
 * config.enabled = true;
 * config.headers.enabled = true;
 * config.payload.enabled = true;
 * config.outputFile = "debug.log";
 * 
 * // Create debug logger
 * DebugLogger logger(config);
 * 
 * // Log request/response during processing
 * logger.logRequestHeaders(request);
 * logger.logRequestPayload(request);
 * // ... process request ...
 * logger.logResponseHeaders(response, request.getPath(), request.getMethod());
 * logger.logResponsePayload(response);
 * @endcode
 * 
 * @warning Debug logging can significantly impact performance and should be
 *          disabled in production environments. Enable only for debugging purposes.
 * 
 * @see DebugLoggingConfig
 * @see HttpRequest
 * @see HttpResponse
 */
#pragma once

#include <cppSwitchboard/config.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace cppSwitchboard {

/**
 * @brief Advanced debug logger for HTTP requests and responses
 * 
 * The DebugLogger provides comprehensive logging capabilities for HTTP traffic
 * with configurable filtering, output formatting, and security considerations.
 * It can log headers, payloads, timing information, and URL details with
 * fine-grained control over what gets logged.
 * 
 * Key features:
 * - Configurable header and payload logging
 * - Security-aware filtering of sensitive headers
 * - Content-type based payload filtering
 * - Configurable payload size limits
 * - Thread-safe logging operations
 * - Customizable output formatting
 * - File or stdout output options
 * 
 * @code{.cpp}
 * // Setup debug logging configuration
 * DebugLoggingConfig config;
 * config.enabled = true;
 * config.headers.enabled = true;
 * config.headers.logRequestHeaders = true;
 * config.headers.logResponseHeaders = true;
 * config.headers.excludeHeaders = {"authorization", "cookie"};
 * config.payload.enabled = true;
 * config.payload.maxPayloadSizeBytes = 2048;
 * config.outputFile = "/var/log/debug.log";
 * 
 * DebugLogger logger(config);
 * 
 * // Use during request processing
 * if (logger.isHeaderLoggingEnabled()) {
 *     logger.logRequestHeaders(request);
 * }
 * if (logger.isPayloadLoggingEnabled()) {
 *     logger.logRequestPayload(request);
 * }
 * @endcode
 * 
 * @warning This class is not intended for production use. Debug logging can
 *          significantly impact performance and may expose sensitive information.
 * 
 * @see DebugLoggingConfig
 */
class DebugLogger {
public:
    /**
     * @brief Construct a debug logger with specified configuration
     * 
     * Initializes the debug logger with the provided configuration.
     * If file output is configured, opens the log file for writing.
     * 
     * @param config Debug logging configuration
     * 
     * @throws std::runtime_error if configured log file cannot be opened
     * 
     * @code{.cpp}
     * DebugLoggingConfig config;
     * config.enabled = true;
     * config.outputFile = "debug.log";
     * DebugLogger logger(config);
     * @endcode
     */
    explicit DebugLogger(const DebugLoggingConfig& config);
    
    /**
     * @brief Destructor - ensures log file is properly closed
     * 
     * Flushes any pending log entries and closes the log file if opened.
     */
    ~DebugLogger();
    
    /**
     * @brief Log HTTP request headers and URL details
     * 
     * Logs incoming request headers with optional URL details including
     * method, path, query parameters, and protocol version. Sensitive
     * headers specified in the configuration are excluded from logging.
     * 
     * @param request The HTTP request to log headers from
     * 
     * @note Only logs if header logging is enabled in configuration
     * @note Thread-safe operation
     * 
     * @code{.cpp}
     * logger.logRequestHeaders(request);
     * // Output example:
     * // [2024-01-15 10:30:45] REQUEST HEADERS - GET /api/users HTTP/1.1
     * // Host: localhost:8080
     * // User-Agent: Mozilla/5.0...
     * // Accept: application/json
     * @endcode
     * 
     * @see isHeaderLoggingEnabled()
     */
    void logRequestHeaders(const HttpRequest& request);
    
    /**
     * @brief Log HTTP response headers
     * 
     * Logs outgoing response headers with optional URL and method context.
     * Sensitive headers specified in the configuration are excluded from logging.
     * 
     * @param response The HTTP response to log headers from
     * @param url Optional URL for context (default: empty)
     * @param method Optional HTTP method for context (default: empty)
     * 
     * @note Only logs if header logging is enabled in configuration
     * @note Thread-safe operation
     * 
     * @code{.cpp}
     * logger.logResponseHeaders(response, "/api/users", "GET");
     * // Output example:
     * // [2024-01-15 10:30:45] RESPONSE HEADERS - GET /api/users -> 200 OK
     * // Content-Type: application/json
     * // Content-Length: 1024
     * // Cache-Control: no-cache
     * @endcode
     * 
     * @see isHeaderLoggingEnabled()
     */
    void logResponseHeaders(const HttpResponse& response, const std::string& url = "", const std::string& method = "");
    
    /**
     * @brief Log HTTP request payload/body
     * 
     * Logs the request body content with size limits and content-type filtering.
     * Binary content types and large payloads are automatically excluded based on
     * configuration settings.
     * 
     * @param request The HTTP request to log payload from
     * 
     * @note Only logs if payload logging is enabled in configuration
     * @note Respects maxPayloadSizeBytes configuration limit
     * @note Excludes binary content types as configured
     * @note Thread-safe operation
     * 
     * @code{.cpp}
     * logger.logRequestPayload(request);
     * // Output example:
     * // [2024-01-15 10:30:45] REQUEST PAYLOAD (156 bytes):
     * // {"name": "John Doe", "email": "john@example.com"}
     * @endcode
     * 
     * @see isPayloadLoggingEnabled()
     */
    void logRequestPayload(const HttpRequest& request);
    
    /**
     * @brief Log HTTP response payload/body
     * 
     * Logs the response body content with size limits and content-type filtering.
     * Binary content types and large payloads are automatically excluded based on
     * configuration settings.
     * 
     * @param response The HTTP response to log payload from
     * @param url Optional URL for context (default: empty)
     * @param method Optional HTTP method for context (default: empty)
     * 
     * @note Only logs if payload logging is enabled in configuration
     * @note Respects maxPayloadSizeBytes configuration limit
     * @note Excludes binary content types as configured
     * @note Thread-safe operation
     * 
     * @code{.cpp}
     * logger.logResponsePayload(response, "/api/users", "GET");
     * // Output example:
     * // [2024-01-15 10:30:45] RESPONSE PAYLOAD - GET /api/users (245 bytes):
     * // [{"id": 1, "name": "John"}, {"id": 2, "name": "Jane"}]
     * @endcode
     * 
     * @see isPayloadLoggingEnabled()
     */
    void logResponsePayload(const HttpResponse& response, const std::string& url = "", const std::string& method = "");
    
    /**
     * @brief Check if header logging is enabled
     * 
     * Convenience method to check if header logging is enabled in the current
     * configuration. This can be used to avoid expensive header processing
     * when logging is disabled.
     * 
     * @return true if debug logging and header logging are both enabled
     * 
     * @code{.cpp}
     * if (logger.isHeaderLoggingEnabled()) {
     *     logger.logRequestHeaders(request);
     *     logger.logResponseHeaders(response);
     * }
     * @endcode
     */
    bool isHeaderLoggingEnabled() const { return config_.enabled && config_.headers.enabled; }
    
    /**
     * @brief Check if payload logging is enabled
     * 
     * Convenience method to check if payload logging is enabled in the current
     * configuration. This can be used to avoid expensive payload processing
     * when logging is disabled.
     * 
     * @return true if debug logging and payload logging are both enabled
     * 
     * @code{.cpp}
     * if (logger.isPayloadLoggingEnabled()) {
     *     logger.logRequestPayload(request);
     *     logger.logResponsePayload(response);
     * }
     * @endcode
     */
    bool isPayloadLoggingEnabled() const { return config_.enabled && config_.payload.enabled; }

private:
    const DebugLoggingConfig& config_;          ///< Debug logging configuration reference
    std::unique_ptr<std::ofstream> fileStream_; ///< Output file stream (if file output is configured)
    std::mutex logMutex_;                       ///< Mutex for thread-safe logging operations
    
    /**
     * @brief Write a log message to the configured output
     * 
     * Thread-safe method that writes a timestamped log message to either
     * the configured file or stdout.
     * 
     * @param message The log message to write
     */
    void writeLog(const std::string& message);
    
    /**
     * @brief Get current timestamp formatted according to configuration
     * 
     * @return Formatted timestamp string using configured timestamp format
     */
    std::string getCurrentTimestamp() const;
    
    /**
     * @brief Check if a header should be excluded from logging
     * 
     * Checks if the given header name is in the exclusion list configured
     * for security purposes (e.g., authorization, cookies).
     * 
     * @param headerName Name of the header to check (case-insensitive)
     * @return true if header should be excluded from logging
     */
    bool shouldExcludeHeader(const std::string& headerName) const;
    
    /**
     * @brief Check if a content type should be excluded from payload logging
     * 
     * Checks if the given content type matches any of the excluded content
     * type patterns (typically binary types like images, videos).
     * 
     * @param contentType Content-Type header value to check
     * @return true if content type should be excluded from payload logging
     */
    bool shouldExcludeContentType(const std::string& contentType) const;
    
    /**
     * @brief Truncate payload to configured maximum size
     * 
     * Truncates the payload string to the maximum size configured in
     * maxPayloadSizeBytes, adding truncation indicator if needed.
     * 
     * @param payload The payload string to potentially truncate
     * @return Truncated payload string with indicator if truncated
     */
    std::string truncatePayload(const std::string& payload) const;
    
    /**
     * @brief Format URL information for log output
     * 
     * Creates a formatted string containing HTTP method, URL, and protocol
     * information for inclusion in log messages.
     * 
     * @param method HTTP method (GET, POST, etc.)
     * @param url Request URL/path
     * @param protocol HTTP protocol version
     * @return Formatted URL string for logging
     */
    std::string formatUrl(const std::string& method, const std::string& url, const std::string& protocol) const;
};

} // namespace cppSwitchboard 