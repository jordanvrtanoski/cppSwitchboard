/**
 * @file logging_middleware.cpp
 * @brief Implementation of structured request/response logging middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#include <cppSwitchboard/middleware/logging_middleware.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

namespace cppSwitchboard {
namespace middleware {

// LoggingMiddleware Implementation

LoggingMiddleware::LoggingMiddleware() 
    : config_(), logger_(std::make_shared<ConsoleLogger>()), enabled_(true) {
}

LoggingMiddleware::LoggingMiddleware(const LoggingConfig& config)
    : config_(config), logger_(std::make_shared<ConsoleLogger>()), enabled_(true) {
}

LoggingMiddleware::LoggingMiddleware(const LoggingConfig& config, std::shared_ptr<Logger> logger)
    : config_(config), logger_(logger), enabled_(true) {
    if (!logger_) {
        logger_ = std::make_shared<ConsoleLogger>();
    }
}

LoggingMiddleware::~LoggingMiddleware() {
    if (logger_) {
        logger_->flush();
    }
}

HttpResponse LoggingMiddleware::handle(const HttpRequest& request, Context& context, NextHandler next) {
    if (!enabled_) {
        return next(request, context);
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::system_clock::now();
    
    // Log request if enabled
    if (config_.logRequests) {
        LogEntry requestEntry = createLogEntry(request, HttpResponse(200), context, std::chrono::microseconds{0});
        requestEntry.timestamp = timestamp;
        
        if (shouldLog(request, 0)) {  // 0 = unknown status for request logging
            std::string message;
            if (customFormatter_) {
                message = customFormatter_(requestEntry);
            } else {
                switch (config_.format) {
                    case LogFormat::JSON:
                        message = formatAsJSON(requestEntry);
                        break;
                    case LogFormat::COMMON:
                        message = formatAsCommon(requestEntry);
                        break;
                    case LogFormat::COMBINED:
                        message = formatAsCombined(requestEntry);
                        break;
                    case LogFormat::CUSTOM:
                        message = formatAsCustom(requestEntry);
                        break;
                }
            }
            writeLog(LogLevel::INFO, requestEntry, message);
        }
    }

    // Process request through pipeline
    HttpResponse response = next(request, context);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        totalRequests_++;
        totalDuration_ += duration;
        if (response.getStatus() >= 400) {
            errorRequests_++;
        }
    }

    // Log response if enabled
    if (config_.logResponses && shouldLog(request, response.getStatus())) {
        LogEntry responseEntry = createLogEntry(request, response, context, duration);
        responseEntry.timestamp = timestamp;
        
        std::string message;
        if (customFormatter_) {
            message = customFormatter_(responseEntry);
        } else {
            switch (config_.format) {
                case LogFormat::JSON:
                    message = formatAsJSON(responseEntry);
                    break;
                case LogFormat::COMMON:
                    message = formatAsCommon(responseEntry);
                    break;
                case LogFormat::COMBINED:
                    message = formatAsCombined(responseEntry);
                    break;
                case LogFormat::CUSTOM:
                    message = formatAsCustom(responseEntry);
                    break;
            }
        }
        
        LogLevel level = LogLevel::INFO;
        if (response.getStatus() >= 500) {
            level = LogLevel::ERROR;
        } else if (response.getStatus() >= 400) {
            level = LogLevel::WARN;
        }
        
        writeLog(level, responseEntry, message);
    } else if (!shouldLog(request, response.getStatus())) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        excludedRequests_++;
    }

    return response;
}

void LoggingMiddleware::addLogStatusCode(int statusCode) {
    auto it = std::find(config_.logStatusCodes.begin(), config_.logStatusCodes.end(), statusCode);
    if (it == config_.logStatusCodes.end()) {
        config_.logStatusCodes.push_back(statusCode);
    }
}

void LoggingMiddleware::removeLogStatusCode(int statusCode) {
    auto it = std::find(config_.logStatusCodes.begin(), config_.logStatusCodes.end(), statusCode);
    if (it != config_.logStatusCodes.end()) {
        config_.logStatusCodes.erase(it);
    }
}

void LoggingMiddleware::addExcludePath(const std::string& path) {
    auto it = std::find(config_.excludePaths.begin(), config_.excludePaths.end(), path);
    if (it == config_.excludePaths.end()) {
        config_.excludePaths.push_back(path);
    }
}

void LoggingMiddleware::removeExcludePath(const std::string& path) {
    auto it = std::find(config_.excludePaths.begin(), config_.excludePaths.end(), path);
    if (it != config_.excludePaths.end()) {
        config_.excludePaths.erase(it);
    }
}

std::unordered_map<std::string, size_t> LoggingMiddleware::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    std::unordered_map<std::string, size_t> stats;
    stats["total_requests"] = totalRequests_;
    stats["error_requests"] = errorRequests_;
    stats["excluded_requests"] = excludedRequests_;
    stats["success_requests"] = totalRequests_ - errorRequests_;
    stats["avg_duration_microseconds"] = totalRequests_ > 0 ? totalDuration_.count() / totalRequests_ : 0;
    return stats;
}

void LoggingMiddleware::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    totalRequests_ = 0;
    errorRequests_ = 0;
    excludedRequests_ = 0;
    totalDuration_ = std::chrono::microseconds{0};
}

void LoggingMiddleware::flush() {
    if (logger_) {
        logger_->flush();
    }
}

bool LoggingMiddleware::shouldLog(const HttpRequest& request, int responseStatus) const {
    // Check if errors-only mode is enabled
    if (config_.logErrorsOnly && responseStatus > 0 && responseStatus < 400) {
        return false;
    }
    
    // Check if specific status codes are configured
    if (!config_.logStatusCodes.empty() && responseStatus > 0) {
        auto it = std::find(config_.logStatusCodes.begin(), config_.logStatusCodes.end(), responseStatus);
        if (it == config_.logStatusCodes.end()) {
            return false;
        }
    }
    
    // Check if path is excluded
    std::string path = request.getPath();
    for (const auto& excludePath : config_.excludePaths) {
        if (path.find(excludePath) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

std::string LoggingMiddleware::extractClientIP(const HttpRequest& request) const {
    // Check for proxy headers first
    std::string clientIP;
    
    auto headers = request.getHeaders();
    auto it = headers.find("X-Forwarded-For");
    if (it != headers.end()) {
        // X-Forwarded-For can contain multiple IPs, take the first one
        size_t commaPos = it->second.find(',');
        clientIP = (commaPos != std::string::npos) ? it->second.substr(0, commaPos) : it->second;
    } else {
        it = headers.find("X-Real-IP");
        if (it != headers.end()) {
            clientIP = it->second;
        } else {
            it = headers.find("X-Client-IP");
            if (it != headers.end()) {
                clientIP = it->second;
            }
        }
    }
    
    // Trim whitespace
    if (!clientIP.empty()) {
        clientIP.erase(0, clientIP.find_first_not_of(" \t"));
        clientIP.erase(clientIP.find_last_not_of(" \t") + 1);
    }
    
    return clientIP.empty() ? "unknown" : clientIP;
}

std::pair<std::string, std::string> LoggingMiddleware::extractUserInfo(const Context& context) const {
    std::string userId = "anonymous";
    std::string sessionId = "";
    
    auto userIt = context.find("user_id");
    if (userIt != context.end()) {
        try {
            userId = std::any_cast<std::string>(userIt->second);
        } catch (const std::bad_any_cast&) {
            // Ignore cast error, keep default
        }
    }
    
    auto sessionIt = context.find("session_id");
    if (sessionIt != context.end()) {
        try {
            sessionId = std::any_cast<std::string>(sessionIt->second);
        } catch (const std::bad_any_cast&) {
            // Ignore cast error, keep default
        }
    }
    
    return std::make_pair(userId, sessionId);
}

LoggingMiddleware::LogEntry LoggingMiddleware::createLogEntry(
    const HttpRequest& request, const HttpResponse& response, 
    const Context& context, std::chrono::microseconds duration) const {
    
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.method = request.getMethod();
    entry.path = request.getPath();
    
    // Construct query string from query parameters
    auto queryParams = request.getQueryParams();
    if (!queryParams.empty()) {
        std::ostringstream queryStream;
        bool first = true;
        for (const auto& param : queryParams) {
            if (!first) queryStream << "&";
            queryStream << param.first << "=" << param.second;
            first = false;
        }
        entry.query = queryStream.str();
    }
    
    entry.remoteAddr = extractClientIP(request);
    entry.responseStatus = response.getStatus();
    entry.responseSize = response.getBody().size();
    entry.duration = duration;
    
    auto userInfo = extractUserInfo(context);
    entry.userId = userInfo.first;
    entry.sessionId = userInfo.second;
    
    // Extract common headers
    auto headers = request.getHeaders();
    auto userAgentIt = headers.find("User-Agent");
    if (userAgentIt != headers.end()) {
        entry.userAgent = userAgentIt->second;
    }
    
    auto refererIt = headers.find("Referer");
    if (refererIt != headers.end()) {
        entry.referer = refererIt->second;
    }
    
    // Include headers if configured
    if (config_.includeHeaders) {
        entry.requestHeaders = request.getHeaders();
        entry.responseHeaders = response.getHeaders();
    }
    
    // Include body if configured
    if (config_.includeBody) {
        std::string body = request.getBody();
        if (body.size() > config_.maxBodySize) {
            body = body.substr(0, config_.maxBodySize) + "... (truncated)";
        }
        entry.requestBody = body;
    }
    
    return entry;
}

std::string LoggingMiddleware::formatAsJSON(const LogEntry& entry) const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"timestamp\":\"" << formatTimestamp(entry.timestamp) << "\",";
    oss << "\"method\":\"" << entry.method << "\",";
    oss << "\"path\":\"" << entry.path << "\",";
    if (!entry.query.empty()) {
        oss << "\"query\":\"" << entry.query << "\",";
    }
    oss << "\"remote_addr\":\"" << entry.remoteAddr << "\",";
    oss << "\"user_id\":\"" << entry.userId << "\",";
    if (!entry.sessionId.empty()) {
        oss << "\"session_id\":\"" << entry.sessionId << "\",";
    }
    if (!entry.userAgent.empty()) {
        oss << "\"user_agent\":\"" << entry.userAgent << "\",";
    }
    if (!entry.referer.empty()) {
        oss << "\"referer\":\"" << entry.referer << "\",";
    }
    if (entry.responseStatus > 0) {
        oss << "\"status\":" << entry.responseStatus << ",";
        oss << "\"response_size\":" << entry.responseSize << ",";
    }
    if (config_.includeTimings && entry.duration.count() > 0) {
        oss << "\"duration_microseconds\":" << entry.duration.count() << ",";
    }
    if (config_.includeHeaders && !entry.requestHeaders.empty()) {
        oss << "\"request_headers\":{";
        bool first = true;
        for (const auto& header : entry.requestHeaders) {
            if (!first) oss << ",";
            oss << "\"" << header.first << "\":\"" << header.second << "\"";
            first = false;
        }
        oss << "},";
    }
    if (config_.includeHeaders && !entry.responseHeaders.empty()) {
        oss << "\"response_headers\":{";
        bool first = true;
        for (const auto& header : entry.responseHeaders) {
            if (!first) oss << ",";
            oss << "\"" << header.first << "\":\"" << header.second << "\"";
            first = false;
        }
        oss << "},";
    }
    if (config_.includeBody && !entry.requestBody.empty()) {
        oss << "\"request_body\":\"" << entry.requestBody << "\",";
    }
    
    std::string result = oss.str();
    if (result.back() == ',') {
        result.pop_back();  // Remove trailing comma
    }
    result += "}";
    
    return result;
}

std::string LoggingMiddleware::formatAsCommon(const LogEntry& entry) const {
    // Common Log Format: remote_addr - user_id [timestamp] "method path HTTP/1.1" status response_size
    std::ostringstream oss;
    oss << entry.remoteAddr << " - " << entry.userId 
        << " [" << formatTimestamp(entry.timestamp) << "] "
        << "\"" << entry.method << " " << entry.path;
    if (!entry.query.empty()) {
        oss << "?" << entry.query;
    }
    oss << " HTTP/1.1\" " << entry.responseStatus << " " << entry.responseSize;
    
    return oss.str();
}

std::string LoggingMiddleware::formatAsCombined(const LogEntry& entry) const {
    // Combined Log Format: Common + "referer" "user_agent"
    std::string common = formatAsCommon(entry);
    std::ostringstream oss;
    oss << common << " \"" << entry.referer << "\" \"" << entry.userAgent << "\"";
    
    return oss.str();
}

std::string LoggingMiddleware::formatAsCustom(const LogEntry& entry) const {
    std::string format = config_.customFormat;
    
    // Replace placeholders with actual values
    std::regex timestampRegex("\\{timestamp\\}");
    format = std::regex_replace(format, timestampRegex, formatTimestamp(entry.timestamp));
    
    std::regex methodRegex("\\{method\\}");
    format = std::regex_replace(format, methodRegex, entry.method);
    
    std::regex pathRegex("\\{path\\}");
    format = std::regex_replace(format, pathRegex, entry.path);
    
    std::regex queryRegex("\\{query\\}");
    format = std::regex_replace(format, queryRegex, entry.query);
    
    std::regex remoteAddrRegex("\\{remote_addr\\}");
    format = std::regex_replace(format, remoteAddrRegex, entry.remoteAddr);
    
    std::regex userIdRegex("\\{user_id\\}");
    format = std::regex_replace(format, userIdRegex, entry.userId);
    
    std::regex sessionIdRegex("\\{session_id\\}");
    format = std::regex_replace(format, sessionIdRegex, entry.sessionId);
    
    std::regex statusRegex("\\{status\\}");
    format = std::regex_replace(format, statusRegex, std::to_string(entry.responseStatus));
    
    std::regex sizeRegex("\\{response_size\\}");
    format = std::regex_replace(format, sizeRegex, std::to_string(entry.responseSize));
    
    std::regex durationRegex("\\{duration\\}");
    format = std::regex_replace(format, durationRegex, std::to_string(entry.duration.count()));
    
    std::regex userAgentRegex("\\{user_agent\\}");
    format = std::regex_replace(format, userAgentRegex, entry.userAgent);
    
    std::regex refererRegex("\\{referer\\}");
    format = std::regex_replace(format, refererRegex, entry.referer);
    
    return format;
}

void LoggingMiddleware::writeLog(LogLevel level, const LogEntry& entry, const std::string& message) {
    if (static_cast<int>(level) >= static_cast<int>(config_.level) && logger_) {
        logger_->log(level, entry, message);
    }
}

std::string LoggingMiddleware::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    
    return oss.str();
}

// ConsoleLogger Implementation

void ConsoleLogger::log(LoggingMiddleware::LogLevel level, const LoggingMiddleware::LogEntry& entry, 
                       const std::string& message) {
    (void)entry; // Entry parameter reserved for future structured logging enhancements
    std::string levelStr;
    switch (level) {
        case LoggingMiddleware::LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LoggingMiddleware::LogLevel::INFO:  levelStr = "INFO";  break;
        case LoggingMiddleware::LogLevel::WARN:  levelStr = "WARN";  break;
        case LoggingMiddleware::LogLevel::ERROR: levelStr = "ERROR"; break;
    }
    
    std::cout << "[" << levelStr << "] " << message << std::endl;
}

void ConsoleLogger::flush() {
    std::cout.flush();
}

// FileLogger Implementation

FileLogger::FileLogger(const std::string& filename, bool append) {
    std::ios_base::openmode mode = std::ios_base::out;
    if (append) {
        mode |= std::ios_base::app;
    } else {
        mode |= std::ios_base::trunc;
    }
    
    file_.open(filename, mode);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
}

FileLogger::~FileLogger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void FileLogger::log(LoggingMiddleware::LogLevel level, const LoggingMiddleware::LogEntry& entry, 
                    const std::string& message) {
    (void)entry; // Entry parameter reserved for future structured logging enhancements
    std::lock_guard<std::mutex> lock(fileMutex_);
    
    if (file_.is_open()) {
        std::string levelStr;
        switch (level) {
            case LoggingMiddleware::LogLevel::DEBUG: levelStr = "DEBUG"; break;
            case LoggingMiddleware::LogLevel::INFO:  levelStr = "INFO";  break;
            case LoggingMiddleware::LogLevel::WARN:  levelStr = "WARN";  break;
            case LoggingMiddleware::LogLevel::ERROR: levelStr = "ERROR"; break;
        }
        
        file_ << "[" << levelStr << "] " << message << std::endl;
    }
}

void FileLogger::flush() {
    std::lock_guard<std::mutex> lock(fileMutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

} // namespace middleware
} // namespace cppSwitchboard 