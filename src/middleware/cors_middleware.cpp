/**
 * @file cors_middleware.cpp
 * @brief Implementation of Cross-Origin Resource Sharing (CORS) middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#include <cppSwitchboard/middleware/cors_middleware.h>
#include <algorithm>
#include <sstream>
#include <iterator>

namespace cppSwitchboard {
namespace middleware {

// CorsMiddleware Implementation

CorsMiddleware::CorsMiddleware() : enabled_(true) {
    // Default permissive configuration
    config_.allowAllOrigins = true;
    config_.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD", "PATCH"};
    config_.allowedHeaders = {"Content-Type", "Authorization", "X-Requested-With"};
    config_.exposedHeaders = {};
    config_.allowCredentials = false;
    config_.maxAge = 86400; // 24 hours
    config_.handlePreflight = true;
    config_.varyOrigin = true;
    config_.reflectOrigin = false;
}

CorsMiddleware::CorsMiddleware(const CorsConfig& config) 
    : config_(config), enabled_(true) {
}

CorsMiddleware::CorsMiddleware(const CorsConfig& config, OriginValidator validator)
    : config_(config), originValidator_(validator), enabled_(true) {
}

HttpResponse CorsMiddleware::handle(const HttpRequest& request, Context& context, NextHandler next) {
    if (!enabled_) {
        return next(request, context);
    }

    std::string origin = extractOrigin(request);
    bool hasOrigin = !origin.empty();
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        totalRequests_++;
        
        // Check for credentials in request
        auto headers = request.getHeaders();
        bool hasCredentials = headers.find("Cookie") != headers.end() || 
                            headers.find("Authorization") != headers.end();
        if (hasCredentials) {
            credentialRequests_++;
        }
    }

    // Handle preflight request (OPTIONS)
    if (config_.handlePreflight && request.getMethod() == "OPTIONS" && hasOrigin) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        preflightRequests_++;
        
        if (isOriginAllowed(origin)) {
            allowedRequests_++;
            return handlePreflightRequest(request, origin);
        } else {
            blockedRequests_++;
            // Return 403 Forbidden for blocked preflight
            HttpResponse response(403);
            response.setBody("CORS preflight request blocked");
            return response;
        }
    }

    // Process normal request
    HttpResponse response = next(request, context);

    // Apply CORS headers if origin is present
    if (hasOrigin) {
        if (isOriginAllowed(origin)) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            allowedRequests_++;
            applyCorsHeaders(response, origin, false);
        } else {
            std::lock_guard<std::mutex> lock(statsMutex_);
            blockedRequests_++;
            // For blocked requests, we don't add CORS headers
            // The browser will block the response
        }
    }

    return response;
}

void CorsMiddleware::setAllowedOrigins(const std::vector<std::string>& origins) {
    config_.allowedOrigins = origins;
    config_.allowAllOrigins = false;
    
    // Check if "*" is in the origins
    for (const auto& origin : origins) {
        if (origin == "*") {
            config_.allowAllOrigins = true;
            break;
        }
    }
    
    // Invalidate origins cache
    std::lock_guard<std::mutex> lock(cacheMutex_);
    originsCacheValid_ = false;
}

void CorsMiddleware::addAllowedOrigin(const std::string& origin) {
    auto it = std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin);
    if (it == config_.allowedOrigins.end()) {
        config_.allowedOrigins.push_back(origin);
        
        if (origin == "*") {
            config_.allowAllOrigins = true;
        }
        
        // Invalidate origins cache
        std::lock_guard<std::mutex> lock(cacheMutex_);
        originsCacheValid_ = false;
    }
}

void CorsMiddleware::removeAllowedOrigin(const std::string& origin) {
    auto it = std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin);
    if (it != config_.allowedOrigins.end()) {
        config_.allowedOrigins.erase(it);
        
        if (origin == "*") {
            config_.allowAllOrigins = false;
        }
        
        // Invalidate origins cache
        std::lock_guard<std::mutex> lock(cacheMutex_);
        originsCacheValid_ = false;
    }
}

void CorsMiddleware::setAllowedMethods(const std::vector<std::string>& methods) {
    config_.allowedMethods = methods;
    
    // Invalidate methods cache
    std::lock_guard<std::mutex> lock(cacheMutex_);
    methodsCacheValid_ = false;
}

void CorsMiddleware::addAllowedMethod(const std::string& method) {
    auto it = std::find(config_.allowedMethods.begin(), config_.allowedMethods.end(), method);
    if (it == config_.allowedMethods.end()) {
        config_.allowedMethods.push_back(method);
        
        // Invalidate methods cache
        std::lock_guard<std::mutex> lock(cacheMutex_);
        methodsCacheValid_ = false;
    }
}

void CorsMiddleware::removeAllowedMethod(const std::string& method) {
    auto it = std::find(config_.allowedMethods.begin(), config_.allowedMethods.end(), method);
    if (it != config_.allowedMethods.end()) {
        config_.allowedMethods.erase(it);
        
        // Invalidate methods cache
        std::lock_guard<std::mutex> lock(cacheMutex_);
        methodsCacheValid_ = false;
    }
}

void CorsMiddleware::setAllowedHeaders(const std::vector<std::string>& headers) {
    config_.allowedHeaders = headers;
    
    // Invalidate headers cache
    std::lock_guard<std::mutex> lock(cacheMutex_);
    headersCacheValid_ = false;
}

void CorsMiddleware::addAllowedHeader(const std::string& header) {
    std::string normalized = normalizeHeader(header);
    
    // Check if already exists (case-insensitive)
    bool found = false;
    for (const auto& existing : config_.allowedHeaders) {
        if (normalizeHeader(existing) == normalized) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        config_.allowedHeaders.push_back(header);
        
        // Invalidate headers cache
        std::lock_guard<std::mutex> lock(cacheMutex_);
        headersCacheValid_ = false;
    }
}

void CorsMiddleware::removeAllowedHeader(const std::string& header) {
    std::string normalized = normalizeHeader(header);
    
    auto it = std::remove_if(config_.allowedHeaders.begin(), config_.allowedHeaders.end(),
        [this, &normalized](const std::string& existing) {
            return normalizeHeader(existing) == normalized;
        });
    
    if (it != config_.allowedHeaders.end()) {
        config_.allowedHeaders.erase(it, config_.allowedHeaders.end());
        
        // Invalidate headers cache
        std::lock_guard<std::mutex> lock(cacheMutex_);
        headersCacheValid_ = false;
    }
}

void CorsMiddleware::setExposedHeaders(const std::vector<std::string>& headers) {
    config_.exposedHeaders = headers;
}

void CorsMiddleware::addExposedHeader(const std::string& header) {
    std::string normalized = normalizeHeader(header);
    
    // Check if already exists (case-insensitive)
    bool found = false;
    for (const auto& existing : config_.exposedHeaders) {
        if (normalizeHeader(existing) == normalized) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        config_.exposedHeaders.push_back(header);
    }
}

void CorsMiddleware::removeExposedHeader(const std::string& header) {
    std::string normalized = normalizeHeader(header);
    
    auto it = std::remove_if(config_.exposedHeaders.begin(), config_.exposedHeaders.end(),
        [this, &normalized](const std::string& existing) {
            return normalizeHeader(existing) == normalized;
        });
    
    if (it != config_.exposedHeaders.end()) {
        config_.exposedHeaders.erase(it, config_.exposedHeaders.end());
    }
}

void CorsMiddleware::setAllowAllOrigins(bool allow) {
    config_.allowAllOrigins = allow;
    if (allow && std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") == config_.allowedOrigins.end()) {
        config_.allowedOrigins.push_back("*");
    }
    
    // Invalidate origins cache
    std::lock_guard<std::mutex> lock(cacheMutex_);
    originsCacheValid_ = false;
}

void CorsMiddleware::setAllowAllMethods(bool allow) {
    config_.allowAllMethods = allow;
}

void CorsMiddleware::setAllowAllHeaders(bool allow) {
    config_.allowAllHeaders = allow;
    
    // Invalidate headers cache
    std::lock_guard<std::mutex> lock(cacheMutex_);
    headersCacheValid_ = false;
}

void CorsMiddleware::addOriginPattern(const std::string& pattern) {
    try {
        config_.originPatterns.emplace_back(pattern);
    } catch (const std::regex_error&) {
        // Invalid regex pattern, ignore
    }
}

void CorsMiddleware::removeOriginPattern(const std::string& pattern) {
    (void)pattern; // Suppress unused parameter warning
    // Note: Regex comparison is complex, so we'll remove by position for now
    // In a real implementation, you might store pattern strings alongside regex objects
    config_.originPatterns.clear(); // Simple implementation - clear all patterns
}

std::unordered_map<std::string, size_t> CorsMiddleware::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    std::unordered_map<std::string, size_t> stats;
    stats["total_requests"] = totalRequests_;
    stats["preflight_requests"] = preflightRequests_;
    stats["allowed_requests"] = allowedRequests_;
    stats["blocked_requests"] = blockedRequests_;
    stats["credential_requests"] = credentialRequests_;
    return stats;
}

void CorsMiddleware::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    totalRequests_ = 0;
    preflightRequests_ = 0;
    allowedRequests_ = 0;
    blockedRequests_ = 0;
    credentialRequests_ = 0;
}

CorsMiddleware::CorsConfig CorsMiddleware::createPermissiveConfig() {
    CorsConfig config;
    config.allowAllOrigins = true;
    config.allowedOrigins = {"*"};
    config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD", "PATCH"};
    config.allowedHeaders = {"*"};
    config.allowAllHeaders = true;
    config.allowCredentials = true;
    config.maxAge = 86400;
    config.handlePreflight = true;
    config.varyOrigin = false;
    config.reflectOrigin = true;
    return config;
}

CorsMiddleware::CorsConfig CorsMiddleware::createRestrictiveConfig() {
    CorsConfig config;
    config.allowAllOrigins = false;
    config.allowedOrigins = {};
    config.allowedMethods = {"GET", "POST"};
    config.allowedHeaders = {"Content-Type"};
    config.exposedHeaders = {};
    config.allowCredentials = false;
    config.maxAge = 300; // 5 minutes
    config.handlePreflight = true;
    config.varyOrigin = true;
    config.reflectOrigin = false;
    return config;
}

CorsMiddleware::CorsConfig CorsMiddleware::createDevelopmentConfig() {
    CorsConfig config;
    config.allowAllOrigins = false;
    config.allowedOrigins = {"http://localhost:3000", "http://localhost:8080", "http://127.0.0.1:3000"};
    config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD", "PATCH"};
    config.allowedHeaders = {"Content-Type", "Authorization", "X-Requested-With", "X-API-Key"};
    config.exposedHeaders = {"X-Total-Count", "X-Page-Count"};
    config.allowCredentials = true;
    config.maxAge = 3600; // 1 hour
    config.handlePreflight = true;
    config.varyOrigin = true;
    config.reflectOrigin = true;
    return config;
}

bool CorsMiddleware::isOriginAllowed(const std::string& origin) const {
    if (origin.empty()) {
        return false;
    }

    // Check if all origins are allowed
    if (config_.allowAllOrigins) {
        return true;
    }

    // Use custom validator if provided
    if (originValidator_) {
        return originValidator_(origin);
    }

    // Update cache if needed
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (!originsCacheValid_) {
            allowedOriginsSet_.clear();
            for (const auto& allowedOrigin : config_.allowedOrigins) {
                allowedOriginsSet_.insert(allowedOrigin);
            }
            originsCacheValid_ = true;
        }
    }

    // Check exact match in cache
    if (allowedOriginsSet_.find(origin) != allowedOriginsSet_.end()) {
        return true;
    }

    // Check wildcard match
    if (allowedOriginsSet_.find("*") != allowedOriginsSet_.end()) {
        return true;
    }

    // Check regex patterns
    for (const auto& pattern : config_.originPatterns) {
        if (std::regex_match(origin, pattern)) {
            return true;
        }
    }

    return false;
}

bool CorsMiddleware::isMethodAllowed(const std::string& method) const {
    if (config_.allowAllMethods) {
        return true;
    }

    // Update cache if needed
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (!methodsCacheValid_) {
            allowedMethodsSet_.clear();
            for (const auto& allowedMethod : config_.allowedMethods) {
                allowedMethodsSet_.insert(allowedMethod);
            }
            methodsCacheValid_ = true;
        }
    }

    bool found = allowedMethodsSet_.find(method) != allowedMethodsSet_.end();
    return found;
}

bool CorsMiddleware::areHeadersAllowed(const std::vector<std::string>& headers) const {
    if (config_.allowAllHeaders) {
        return true;
    }

    // Update cache if needed
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (!headersCacheValid_) {
            allowedHeadersSet_.clear();
            for (const auto& allowedHeader : config_.allowedHeaders) {
                allowedHeadersSet_.insert(normalizeHeader(allowedHeader));
            }
            headersCacheValid_ = true;
        }
    }

    // Check if all requested headers are allowed
    for (const auto& header : headers) {
        std::string normalized = normalizeHeader(header);
        if (allowedHeadersSet_.find(normalized) == allowedHeadersSet_.end() &&
            allowedHeadersSet_.find("*") == allowedHeadersSet_.end()) {
            return false;
        }
    }

    return true;
}

HttpResponse CorsMiddleware::handlePreflightRequest(const HttpRequest& request, const std::string& origin) {
    HttpResponse response(200);

    // Check requested method
    auto headers = request.getHeaders();
    auto methodHeader = headers.find("Access-Control-Request-Method");
    if (methodHeader != headers.end()) {
        if (!isMethodAllowed(methodHeader->second)) {
            response.setStatus(403);
            response.setBody("Method not allowed");
            return response;
        }
    }

    // Check requested headers
    auto headersHeader = headers.find("Access-Control-Request-Headers");
    if (headersHeader != headers.end()) {
        std::vector<std::string> requestedHeaders = parseHeaderValues(headersHeader->second);
        if (!areHeadersAllowed(requestedHeaders)) {
            response.setStatus(403);
            response.setBody("Headers not allowed");
            return response;
        }
    }

    // Apply CORS headers for preflight
    applyCorsHeaders(response, origin, true);

    // Add preflight-specific headers
    if (!config_.allowedMethods.empty()) {
        response.setHeader("Access-Control-Allow-Methods", joinStrings(config_.allowedMethods, ", "));
    }

    if (!config_.allowedHeaders.empty() && !config_.allowAllHeaders) {
        response.setHeader("Access-Control-Allow-Headers", joinStrings(config_.allowedHeaders, ", "));
    } else if (config_.allowAllHeaders && headersHeader != headers.end()) {
        response.setHeader("Access-Control-Allow-Headers", headersHeader->second);
    }

    if (config_.maxAge > 0) {
        response.setHeader("Access-Control-Max-Age", std::to_string(config_.maxAge));
    }

    // Preflight responses should have no body
    response.setBody("");

    return response;
}

void CorsMiddleware::applyCorsHeaders(HttpResponse& response, const std::string& origin, bool isPreflightResponse) {
    // Set Access-Control-Allow-Origin
    if (config_.allowAllOrigins && !config_.allowCredentials) {
        response.setHeader("Access-Control-Allow-Origin", "*");
    } else if (config_.reflectOrigin || config_.allowCredentials || originValidator_) {
        response.setHeader("Access-Control-Allow-Origin", origin);
    } else if (!config_.allowedOrigins.empty()) {
        response.setHeader("Access-Control-Allow-Origin", config_.allowedOrigins[0]);
    }

    // Set Access-Control-Allow-Credentials
    if (config_.allowCredentials) {
        response.setHeader("Access-Control-Allow-Credentials", "true");
    }

    // Set Access-Control-Expose-Headers
    if (!config_.exposedHeaders.empty() && !isPreflightResponse) {
        response.setHeader("Access-Control-Expose-Headers", joinStrings(config_.exposedHeaders, ", "));
    }

    // Set Vary: Origin header if configured
    if (config_.varyOrigin) {
        std::string varyHeader = response.getHeader("Vary");
        if (varyHeader.empty()) {
            response.setHeader("Vary", "Origin");
        } else if (varyHeader.find("Origin") == std::string::npos) {
            response.setHeader("Vary", varyHeader + ", Origin");
        }
    }
}

std::string CorsMiddleware::extractOrigin(const HttpRequest& request) const {
    auto headers = request.getHeaders();
    auto it = headers.find("Origin");
    return (it != headers.end()) ? it->second : "";
}

std::vector<std::string> CorsMiddleware::parseHeaderValues(const std::string& headerValue) const {
    std::vector<std::string> values;
    std::stringstream ss(headerValue);
    std::string item;

    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        
        if (!item.empty()) {
            values.push_back(item);
        }
    }

    return values;
}

std::string CorsMiddleware::joinStrings(const std::vector<std::string>& values, const std::string& delimiter) const {
    if (values.empty()) {
        return "";
    }

    std::ostringstream oss;
    std::copy(values.begin(), values.end() - 1, std::ostream_iterator<std::string>(oss, delimiter.c_str()));
    oss << values.back();

    return oss.str();
}

std::string CorsMiddleware::normalizeHeader(const std::string& header) const {
    std::string normalized = header;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    return normalized;
}

} // namespace middleware
} // namespace cppSwitchboard 