#include <cppSwitchboard/debug_logger.h>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace cppSwitchboard {

DebugLogger::DebugLogger(const DebugLoggingConfig& config) : config_(config) {
    if (config_.enabled && !config_.outputFile.empty()) {
        fileStream_ = std::make_unique<std::ofstream>(config_.outputFile, std::ios::app);
        if (!fileStream_->is_open()) {
            std::cerr << "Warning: Failed to open debug log file: " << config_.outputFile << std::endl;
            fileStream_.reset();
        }
    }
}

DebugLogger::~DebugLogger() {
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->close();
    }
}

void DebugLogger::logRequestHeaders(const HttpRequest& request) {
    if (!isHeaderLoggingEnabled() || !config_.headers.logRequestHeaders) {
        return;
    }
    
    std::ostringstream oss;
    oss << "[DEBUG-HEADERS] REQUEST ";
    
    if (config_.headers.includeUrlDetails) {
        oss << formatUrl(request.getMethod(), request.getPath(), request.getProtocol()) << "\n";
    } else {
        oss << "\n";
    }
    
    // Log all headers except excluded ones
    const auto& headers = request.getHeaders();
    for (const auto& header : headers) {
        if (!shouldExcludeHeader(header.first)) {
            oss << "  " << header.first << ": " << header.second << "\n";
        } else {
            oss << "  " << header.first << ": [EXCLUDED]\n";
        }
    }
    
    // Add stream ID for HTTP/2
    if (request.getProtocol() == "HTTP/2" && request.getStreamId() > 0) {
        oss << "  X-HTTP2-Stream-ID: " << request.getStreamId() << "\n";
    }
    
    writeLog(oss.str());
}

void DebugLogger::logResponseHeaders(const HttpResponse& response, const std::string& url, const std::string& method) {
    if (!isHeaderLoggingEnabled() || !config_.headers.logResponseHeaders) {
        return;
    }
    
    std::ostringstream oss;
    oss << "[DEBUG-HEADERS] RESPONSE ";
    
    if (config_.headers.includeUrlDetails && !url.empty()) {
        oss << formatUrl(method, url, "") << " -> " << response.getStatus() << "\n";
    } else {
        oss << "-> " << response.getStatus() << "\n";
    }
    
    // Log all headers except excluded ones
    const auto& headers = response.getHeaders();
    for (const auto& header : headers) {
        if (!shouldExcludeHeader(header.first)) {
            oss << "  " << header.first << ": " << header.second << "\n";
        } else {
            oss << "  " << header.first << ": [EXCLUDED]\n";
        }
    }
    
    writeLog(oss.str());
}

void DebugLogger::logRequestPayload(const HttpRequest& request) {
    if (!isPayloadLoggingEnabled() || !config_.payload.logRequestPayload) {
        return;
    }
    
    const std::string& body = request.getBody();
    if (body.empty()) {
        return; // Don't log empty payloads
    }
    
    // Check if content type should be excluded
    std::string contentType = request.getHeader("content-type");
    if (shouldExcludeContentType(contentType)) {
        std::ostringstream oss;
        oss << "[DEBUG-PAYLOAD] REQUEST ";
        if (config_.headers.includeUrlDetails) {
            oss << formatUrl(request.getMethod(), request.getPath(), request.getProtocol());
        }
        oss << " PAYLOAD: [EXCLUDED - " << contentType << "] (" << body.size() << " bytes)\n";
        writeLog(oss.str());
        return;
    }
    
    std::ostringstream oss;
    oss << "[DEBUG-PAYLOAD] REQUEST ";
    if (config_.headers.includeUrlDetails) {
        oss << formatUrl(request.getMethod(), request.getPath(), request.getProtocol());
    }
    oss << " PAYLOAD (" << body.size() << " bytes):\n";
    oss << truncatePayload(body) << "\n";
    
    writeLog(oss.str());
}

void DebugLogger::logResponsePayload(const HttpResponse& response, const std::string& url, const std::string& method) {
    if (!isPayloadLoggingEnabled() || !config_.payload.logResponsePayload) {
        return;
    }
    
    const std::string& body = response.getBody();
    if (body.empty()) {
        return; // Don't log empty payloads
    }
    
    // Check if content type should be excluded
    std::string contentType = response.getHeader("content-type");
    if (shouldExcludeContentType(contentType)) {
        std::ostringstream oss;
        oss << "[DEBUG-PAYLOAD] RESPONSE ";
        if (config_.headers.includeUrlDetails && !url.empty()) {
            oss << formatUrl(method, url, "") << " -> " << response.getStatus();
        } else {
            oss << "-> " << response.getStatus();
        }
        oss << " PAYLOAD: [EXCLUDED - " << contentType << "] (" << body.size() << " bytes)\n";
        writeLog(oss.str());
        return;
    }
    
    std::ostringstream oss;
    oss << "[DEBUG-PAYLOAD] RESPONSE ";
    if (config_.headers.includeUrlDetails && !url.empty()) {
        oss << formatUrl(method, url, "") << " -> " << response.getStatus();
    } else {
        oss << "-> " << response.getStatus();
    }
    oss << " PAYLOAD (" << body.size() << " bytes):\n";
    oss << truncatePayload(body) << "\n";
    
    writeLog(oss.str());
}

void DebugLogger::writeLog(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    std::string timestamp = getCurrentTimestamp();
    std::string fullMessage = timestamp + " " + message;
    
    if (fileStream_ && fileStream_->is_open()) {
        *fileStream_ << fullMessage << std::flush;
    } else {
        std::cout << fullMessage << std::flush;
    }
}

std::string DebugLogger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), config_.timestampFormat.c_str());
    return oss.str();
}

bool DebugLogger::shouldExcludeHeader(const std::string& headerName) const {
    std::string lowerName = headerName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    for (const auto& excludePattern : config_.headers.excludeHeaders) {
        std::string lowerPattern = excludePattern;
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
        
        if (lowerName.find(lowerPattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool DebugLogger::shouldExcludeContentType(const std::string& contentType) const {
    if (contentType.empty()) return false;
    
    std::string lowerContentType = contentType;
    std::transform(lowerContentType.begin(), lowerContentType.end(), lowerContentType.begin(), ::tolower);
    
    for (const auto& excludePattern : config_.payload.excludeContentTypes) {
        std::string lowerPattern = excludePattern;
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
        
        if (lowerContentType.find(lowerPattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string DebugLogger::truncatePayload(const std::string& payload) const {
    if (static_cast<int>(payload.size()) <= config_.payload.maxPayloadSizeBytes) {
        return payload;
    }
    
    std::string truncated = payload.substr(0, config_.payload.maxPayloadSizeBytes);
    truncated += "\n... [TRUNCATED - showing first " + std::to_string(config_.payload.maxPayloadSizeBytes) + 
                 " bytes of " + std::to_string(payload.size()) + " total bytes]";
    return truncated;
}

std::string DebugLogger::formatUrl(const std::string& method, const std::string& url, const std::string& protocol) const {
    std::ostringstream oss;
    if (!method.empty()) {
        oss << method << " ";
    }
    oss << url;
    if (!protocol.empty()) {
        oss << " " << protocol;
    }
    return oss.str();
}

} // namespace cppSwitchboard 