/**
 * @file logging_middleware.h
 * @brief Structured request/response logging middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#pragma once

#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <chrono>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <mutex>

namespace cppSwitchboard {
namespace middleware {

/**
 * @brief Logging middleware for structured request/response logging
 * 
 * This middleware provides comprehensive logging capabilities for HTTP requests
 * and responses with configurable formats and output destinations.
 * 
 * Features:
 * - Request logging (method, path, headers, body, timing)
 * - Response logging (status, headers, body size, timing)
 * - Configurable log formats (JSON, Apache Common Log, Custom)
 * - Multiple output destinations (console, file, custom logger)
 * - Performance metrics collection
 * - Conditional logging based on status codes or paths
 * - Integration with existing DebugLogger
 * - Thread-safe logging operations
 * - Log rotation support
 * 
 * Log formats supported:
 * - JSON: Structured JSON logging with all request/response data
 * - COMMON: Apache Common Log Format
 * - COMBINED: Apache Combined Log Format
 * - CUSTOM: User-defined format with placeholders
 * 
 * @note This middleware has priority 10 and should run early in the pipeline
 */
class LoggingMiddleware : public Middleware {
public:
    /**
     * @brief Log format enumeration
     */
    enum class LogFormat {
        JSON,      ///< JSON structured logging
        COMMON,    ///< Apache Common Log Format
        COMBINED,  ///< Apache Combined Log Format
        CUSTOM     ///< Custom format with placeholders
    };

    /**
     * @brief Log level enumeration
     */
    enum class LogLevel {
        DEBUG = 0,    ///< Debug level
        INFO = 1,     ///< Info level
        WARN = 2,     ///< Warning level
        ERROR = 3     ///< Error level
    };

    /**
     * @brief Logging configuration
     */
    struct LoggingConfig {
        LogFormat format = LogFormat::JSON;           ///< Log format
        LogLevel level = LogLevel::INFO;              ///< Minimum log level
        bool logRequests = true;                      ///< Log incoming requests
        bool logResponses = true;                     ///< Log outgoing responses
        bool includeHeaders = true;                   ///< Include headers in logs
        bool includeBody = false;                     ///< Include request/response body
        bool includeTimings = true;                   ///< Include timing information
        bool logErrorsOnly = false;                   ///< Only log requests with error responses
        std::vector<int> logStatusCodes;              ///< Specific status codes to log (empty = all)
        std::vector<std::string> excludePaths;       ///< Paths to exclude from logging
        std::string customFormat;                     ///< Custom log format string
        size_t maxBodySize = 1024;                    ///< Maximum body size to log
    };

    /**
     * @brief Log entry structure
     */
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string method;
        std::string path;
        std::string query;
        std::string userAgent;
        std::string referer;
        std::string remoteAddr;
        std::map<std::string, std::string> requestHeaders;
        std::string requestBody;
        int responseStatus = 0;
        std::map<std::string, std::string> responseHeaders;
        size_t responseSize = 0;
        std::chrono::microseconds duration{0};
        std::string userId;
        std::string sessionId;
    };

    /**
     * @brief Custom logger interface
     */
    class Logger {
    public:
        virtual ~Logger() = default;
        virtual void log(LogLevel level, const LogEntry& entry, const std::string& message) = 0;
        virtual void flush() = 0;
    };

    /**
     * @brief Custom log formatter function type
     */
    using LogFormatter = std::function<std::string(const LogEntry&)>;

    /**
     * @brief Constructor with default configuration
     */
    LoggingMiddleware();

    /**
     * @brief Constructor with configuration
     * 
     * @param config Logging configuration
     */
    explicit LoggingMiddleware(const LoggingConfig& config);

    /**
     * @brief Constructor with custom logger
     * 
     * @param config Logging configuration
     * @param logger Custom logger implementation
     */
    LoggingMiddleware(const LoggingConfig& config, std::shared_ptr<Logger> logger);

    /**
     * @brief Destructor
     */
    virtual ~LoggingMiddleware();

    /**
     * @brief Process HTTP request and log request/response
     * 
     * @param request The HTTP request to process
     * @param context Middleware context for sharing state
     * @param next Function to call the next middleware/handler
     * @return HttpResponse The response from downstream handlers
     */
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override;

    /**
     * @brief Get middleware name
     * 
     * @return std::string Middleware name
     */
    std::string getName() const override { return "LoggingMiddleware"; }

    /**
     * @brief Get middleware priority
     * 
     * @return int Priority value (10 for logging)
     */
    int getPriority() const override { return 10; }

    // Configuration methods

    /**
     * @brief Set log format
     * 
     * @param format Log format to use
     */
    void setLogFormat(LogFormat format) { config_.format = format; }

    /**
     * @brief Get log format
     * 
     * @return LogFormat Current log format
     */
    LogFormat getLogFormat() const { return config_.format; }

    /**
     * @brief Set log level
     * 
     * @param level Minimum log level
     */
    void setLogLevel(LogLevel level) { config_.level = level; }

    /**
     * @brief Get log level
     * 
     * @return LogLevel Current log level
     */
    LogLevel getLogLevel() const { return config_.level; }

    /**
     * @brief Set custom log format string
     * 
     * @param format Custom format string with placeholders
     */
    void setCustomFormat(const std::string& format) { 
        config_.customFormat = format; 
        config_.format = LogFormat::CUSTOM;
    }

    /**
     * @brief Set custom logger
     * 
     * @param logger Custom logger implementation
     */
    void setLogger(std::shared_ptr<Logger> logger) { logger_ = logger; }

    /**
     * @brief Set custom formatter
     * 
     * @param formatter Custom formatter function
     */
    void setFormatter(LogFormatter formatter) { customFormatter_ = formatter; }

    /**
     * @brief Enable/disable request logging
     * 
     * @param enabled Whether to log requests
     */
    void setLogRequests(bool enabled) { config_.logRequests = enabled; }

    /**
     * @brief Enable/disable response logging
     * 
     * @param enabled Whether to log responses
     */
    void setLogResponses(bool enabled) { config_.logResponses = enabled; }

    /**
     * @brief Enable/disable header logging
     * 
     * @param enabled Whether to include headers in logs
     */
    void setIncludeHeaders(bool enabled) { config_.includeHeaders = enabled; }

    /**
     * @brief Enable/disable body logging
     * 
     * @param enabled Whether to include request/response body
     */
    void setIncludeBody(bool enabled) { config_.includeBody = enabled; }

    /**
     * @brief Set maximum body size to log
     * 
     * @param maxSize Maximum body size in bytes
     */
    void setMaxBodySize(size_t maxSize) { config_.maxBodySize = maxSize; }

    /**
     * @brief Add status code to log filter
     * 
     * @param statusCode Status code to log
     */
    void addLogStatusCode(int statusCode);

    /**
     * @brief Remove status code from log filter
     * 
     * @param statusCode Status code to remove
     */
    void removeLogStatusCode(int statusCode);

    /**
     * @brief Add path to exclude from logging
     * 
     * @param path Path pattern to exclude
     */
    void addExcludePath(const std::string& path);

    /**
     * @brief Remove path from exclude list
     * 
     * @param path Path pattern to remove
     */
    void removeExcludePath(const std::string& path);

    /**
     * @brief Set error-only logging mode
     * 
     * @param enabled Whether to only log error responses
     */
    void setLogErrorsOnly(bool enabled) { config_.logErrorsOnly = enabled; }

    /**
     * @brief Enable or disable logging
     * 
     * @param enabled Whether logging is enabled
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Check if logging is enabled
     * 
     * @return bool Whether logging is enabled
     */
    bool isEnabled() const override { return enabled_; }

    /**
     * @brief Get logging statistics
     * 
     * @return std::unordered_map<std::string, size_t> Statistics map
     */
    std::unordered_map<std::string, size_t> getStatistics() const;

    /**
     * @brief Reset logging statistics
     */
    void resetStatistics();

    /**
     * @brief Flush pending log entries
     */
    void flush();

protected:
    /**
     * @brief Check if request should be logged
     * 
     * @param request HTTP request
     * @param responseStatus Response status code
     * @return bool True if should be logged
     */
    bool shouldLog(const HttpRequest& request, int responseStatus) const;

    /**
     * @brief Extract client IP address from request
     * 
     * @param request HTTP request
     * @return std::string Client IP address
     */
    std::string extractClientIP(const HttpRequest& request) const;

    /**
     * @brief Extract user information from context
     * 
     * @param context Middleware context
     * @return std::pair<std::string, std::string> User ID and session ID
     */
    std::pair<std::string, std::string> extractUserInfo(const Context& context) const;

    /**
     * @brief Create log entry from request and response
     * 
     * @param request HTTP request
     * @param response HTTP response
     * @param context Middleware context
     * @param duration Request processing duration
     * @return LogEntry Structured log entry
     */
    LogEntry createLogEntry(const HttpRequest& request, const HttpResponse& response, 
                           const Context& context, std::chrono::microseconds duration) const;

    /**
     * @brief Format log entry as JSON
     * 
     * @param entry Log entry to format
     * @return std::string JSON formatted log message
     */
    std::string formatAsJSON(const LogEntry& entry) const;

    /**
     * @brief Format log entry as Common Log Format
     * 
     * @param entry Log entry to format
     * @return std::string Common log formatted message
     */
    std::string formatAsCommon(const LogEntry& entry) const;

    /**
     * @brief Format log entry as Combined Log Format
     * 
     * @param entry Log entry to format
     * @return std::string Combined log formatted message
     */
    std::string formatAsCombined(const LogEntry& entry) const;

    /**
     * @brief Format log entry using custom format
     * 
     * @param entry Log entry to format
     * @return std::string Custom formatted message
     */
    std::string formatAsCustom(const LogEntry& entry) const;

    /**
     * @brief Write log message
     * 
     * @param level Log level
     * @param entry Log entry
     * @param message Formatted log message
     */
    void writeLog(LogLevel level, const LogEntry& entry, const std::string& message);

    /**
     * @brief Format timestamp for logging
     * 
     * @param timestamp Timestamp to format
     * @return std::string Formatted timestamp
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;

private:
    LoggingConfig config_;                          ///< Logging configuration
    std::shared_ptr<Logger> logger_;                ///< Custom logger implementation
    LogFormatter customFormatter_;                  ///< Custom formatter function
    bool enabled_;                                  ///< Whether logging is enabled

    // Statistics
    mutable std::mutex statsMutex_;                 ///< Mutex for statistics
    mutable size_t totalRequests_{0};               ///< Total requests logged
    mutable size_t errorRequests_{0};               ///< Error requests logged
    mutable size_t excludedRequests_{0};            ///< Excluded requests
    mutable std::chrono::microseconds totalDuration_{0}; ///< Total processing time
};

/**
 * @brief Console logger implementation
 */
class ConsoleLogger : public LoggingMiddleware::Logger {
public:
    void log(LoggingMiddleware::LogLevel level, const LoggingMiddleware::LogEntry& entry, 
             const std::string& message) override;
    void flush() override;
};

/**
 * @brief File logger implementation
 */
class FileLogger : public LoggingMiddleware::Logger {
public:
    /**
     * @brief Constructor
     * 
     * @param filename Log file path
     * @param append Whether to append to existing file
     */
    explicit FileLogger(const std::string& filename, bool append = true);

    /**
     * @brief Destructor
     */
    ~FileLogger();

    void log(LoggingMiddleware::LogLevel level, const LoggingMiddleware::LogEntry& entry, 
             const std::string& message) override;
    void flush() override;

private:
    std::ofstream file_;                            ///< Output file stream
    std::mutex fileMutex_;                          ///< File access mutex
};

} // namespace middleware
} // namespace cppSwitchboard 