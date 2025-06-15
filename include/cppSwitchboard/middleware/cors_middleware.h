/**
 * @file cors_middleware.h
 * @brief Cross-Origin Resource Sharing (CORS) middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#pragma once

#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <regex>

namespace cppSwitchboard {
namespace middleware {

/**
 * @brief CORS middleware for handling Cross-Origin Resource Sharing
 * 
 * This middleware implements comprehensive CORS support with configurable
 * policies for origins, methods, headers, and credentials.
 * 
 * Features:
 * - Configurable allowed origins (wildcards supported)
 * - Allowed HTTP methods configuration
 * - Allowed and exposed headers configuration
 * - Credentials support (cookies, authorization headers)
 * - Preflight request handling (OPTIONS)
 * - Max age configuration for preflight caching
 * - Custom origin validation functions
 * - Flexible policy configuration per route
 * - Thread-safe operations
 * - Statistics tracking
 * 
 * CORS headers supported:
 * - Access-Control-Allow-Origin
 * - Access-Control-Allow-Methods
 * - Access-Control-Allow-Headers
 * - Access-Control-Expose-Headers
 * - Access-Control-Allow-Credentials
 * - Access-Control-Max-Age
 * 
 * @note This middleware has priority -10 and should run very early in the pipeline
 */
class CorsMiddleware : public Middleware {
public:
    /**
     * @brief CORS configuration structure
     */
    struct CorsConfig {
        // Origin configuration
        std::vector<std::string> allowedOrigins;        ///< Allowed origins ("*" for all)
        bool allowAllOrigins = false;                   ///< Allow all origins (sets "*")
        bool allowCredentials = false;                  ///< Allow credentials (cookies, auth)
        
        // Methods configuration
        std::vector<std::string> allowedMethods;        ///< Allowed HTTP methods
        bool allowAllMethods = false;                   ///< Allow all methods
        
        // Headers configuration
        std::vector<std::string> allowedHeaders;        ///< Allowed request headers
        std::vector<std::string> exposedHeaders;        ///< Exposed response headers
        bool allowAllHeaders = false;                   ///< Allow all headers
        
        // Preflight configuration
        int maxAge = 86400;                            ///< Max age for preflight cache (seconds)
        bool handlePreflight = true;                   ///< Handle OPTIONS preflight requests
        
        // Advanced configuration
        bool varyOrigin = true;                        ///< Add Vary: Origin header
        bool reflectOrigin = false;                    ///< Reflect origin in Access-Control-Allow-Origin
        std::vector<std::regex> originPatterns;        ///< Regex patterns for origin matching
    };

    /**
     * @brief Origin validation function type
     */
    using OriginValidator = std::function<bool(const std::string&)>;

    /**
     * @brief Default constructor with permissive CORS policy
     */
    CorsMiddleware();

    /**
     * @brief Constructor with specific configuration
     * 
     * @param config CORS configuration
     */
    explicit CorsMiddleware(const CorsConfig& config);

    /**
     * @brief Constructor with custom origin validator
     * 
     * @param config CORS configuration
     * @param validator Custom origin validator function
     */
    CorsMiddleware(const CorsConfig& config, OriginValidator validator);

    /**
     * @brief Destructor
     */
    virtual ~CorsMiddleware() = default;

    /**
     * @brief Process HTTP request and apply CORS headers
     * 
     * @param request The HTTP request to process
     * @param context Middleware context for sharing state
     * @param next Function to call the next middleware/handler
     * @return HttpResponse The response with CORS headers applied
     */
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override;

    /**
     * @brief Get middleware name
     * 
     * @return std::string Middleware name
     */
    std::string getName() const override { return "CorsMiddleware"; }

    /**
     * @brief Get middleware priority
     * 
     * @return int Priority value (-10 for early execution)
     */
    int getPriority() const override { return -10; }

    // Configuration methods

    /**
     * @brief Set allowed origins
     * 
     * @param origins List of allowed origins
     */
    void setAllowedOrigins(const std::vector<std::string>& origins);

    /**
     * @brief Add allowed origin
     * 
     * @param origin Origin to allow
     */
    void addAllowedOrigin(const std::string& origin);

    /**
     * @brief Remove allowed origin
     * 
     * @param origin Origin to remove
     */
    void removeAllowedOrigin(const std::string& origin);

    /**
     * @brief Set allowed HTTP methods
     * 
     * @param methods List of allowed methods
     */
    void setAllowedMethods(const std::vector<std::string>& methods);

    /**
     * @brief Add allowed HTTP method
     * 
     * @param method Method to allow
     */
    void addAllowedMethod(const std::string& method);

    /**
     * @brief Remove allowed HTTP method
     * 
     * @param method Method to remove
     */
    void removeAllowedMethod(const std::string& method);

    /**
     * @brief Set allowed request headers
     * 
     * @param headers List of allowed headers
     */
    void setAllowedHeaders(const std::vector<std::string>& headers);

    /**
     * @brief Add allowed request header
     * 
     * @param header Header to allow
     */
    void addAllowedHeader(const std::string& header);

    /**
     * @brief Remove allowed request header
     * 
     * @param header Header to remove
     */
    void removeAllowedHeader(const std::string& header);

    /**
     * @brief Set exposed response headers
     * 
     * @param headers List of exposed headers
     */
    void setExposedHeaders(const std::vector<std::string>& headers);

    /**
     * @brief Add exposed response header
     * 
     * @param header Header to expose
     */
    void addExposedHeader(const std::string& header);

    /**
     * @brief Remove exposed response header
     * 
     * @param header Header to remove
     */
    void removeExposedHeader(const std::string& header);

    /**
     * @brief Enable/disable credentials support
     * 
     * @param allow Whether to allow credentials
     */
    void setAllowCredentials(bool allow) { config_.allowCredentials = allow; }

    /**
     * @brief Get credentials support status
     * 
     * @return bool Whether credentials are allowed
     */
    bool getAllowCredentials() const { return config_.allowCredentials; }

    /**
     * @brief Set maximum age for preflight caching
     * 
     * @param maxAge Max age in seconds
     */
    void setMaxAge(int maxAge) { config_.maxAge = maxAge; }

    /**
     * @brief Get maximum age for preflight caching
     * 
     * @return int Max age in seconds
     */
    int getMaxAge() const { return config_.maxAge; }

    /**
     * @brief Enable/disable preflight handling
     * 
     * @param handle Whether to handle preflight requests
     */
    void setHandlePreflight(bool handle) { config_.handlePreflight = handle; }

    /**
     * @brief Get preflight handling status
     * 
     * @return bool Whether preflight is handled
     */
    bool getHandlePreflight() const { return config_.handlePreflight; }

    /**
     * @brief Set custom origin validator
     * 
     * @param validator Custom validator function
     */
    void setOriginValidator(OriginValidator validator) { originValidator_ = validator; }

    /**
     * @brief Enable/disable allow all origins
     * 
     * @param allow Whether to allow all origins
     */
    void setAllowAllOrigins(bool allow);

    /**
     * @brief Enable/disable allow all methods
     * 
     * @param allow Whether to allow all methods
     */
    void setAllowAllMethods(bool allow);

    /**
     * @brief Enable/disable allow all headers
     * 
     * @param allow Whether to allow all headers
     */
    void setAllowAllHeaders(bool allow);

    /**
     * @brief Add origin pattern for regex matching
     * 
     * @param pattern Regex pattern for origin matching
     */
    void addOriginPattern(const std::string& pattern);

    /**
     * @brief Remove origin pattern
     * 
     * @param pattern Pattern to remove
     */
    void removeOriginPattern(const std::string& pattern);

    /**
     * @brief Enable or disable middleware
     * 
     * @param enabled Whether middleware is enabled
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Check if middleware is enabled
     * 
     * @return bool Whether middleware is enabled
     */
    bool isEnabled() const override { return enabled_; }

    /**
     * @brief Get CORS statistics
     * 
     * @return std::unordered_map<std::string, size_t> Statistics map
     */
    std::unordered_map<std::string, size_t> getStatistics() const;

    /**
     * @brief Reset CORS statistics
     */
    void resetStatistics();

    /**
     * @brief Create a permissive CORS configuration
     * 
     * @return CorsConfig Permissive configuration
     */
    static CorsConfig createPermissiveConfig();

    /**
     * @brief Create a restrictive CORS configuration
     * 
     * @return CorsConfig Restrictive configuration
     */
    static CorsConfig createRestrictiveConfig();

    /**
     * @brief Create a development CORS configuration
     * 
     * @return CorsConfig Development configuration
     */
    static CorsConfig createDevelopmentConfig();

protected:
    /**
     * @brief Check if origin is allowed
     * 
     * @param origin Origin to check
     * @return bool True if origin is allowed
     */
    bool isOriginAllowed(const std::string& origin) const;

    /**
     * @brief Check if method is allowed
     * 
     * @param method HTTP method to check
     * @return bool True if method is allowed
     */
    bool isMethodAllowed(const std::string& method) const;

    /**
     * @brief Check if headers are allowed
     * 
     * @param headers Headers to check
     * @return bool True if all headers are allowed
     */
    bool areHeadersAllowed(const std::vector<std::string>& headers) const;

    /**
     * @brief Handle preflight request
     * 
     * @param request The OPTIONS request
     * @param origin Request origin
     * @return HttpResponse Preflight response
     */
    HttpResponse handlePreflightRequest(const HttpRequest& request, const std::string& origin);

    /**
     * @brief Apply CORS headers to response
     * 
     * @param response Response to modify
     * @param origin Request origin
     * @param isPreflightResponse Whether this is a preflight response
     */
    void applyCorsHeaders(HttpResponse& response, const std::string& origin, bool isPreflightResponse = false);

    /**
     * @brief Extract origin from request
     * 
     * @param request HTTP request
     * @return std::string Origin header value
     */
    std::string extractOrigin(const HttpRequest& request) const;

    /**
     * @brief Parse comma-separated header values
     * 
     * @param headerValue Header value to parse
     * @return std::vector<std::string> Parsed values
     */
    std::vector<std::string> parseHeaderValues(const std::string& headerValue) const;

    /**
     * @brief Join vector of strings with delimiter
     * 
     * @param values Values to join
     * @param delimiter Delimiter to use
     * @return std::string Joined string
     */
    std::string joinStrings(const std::vector<std::string>& values, const std::string& delimiter) const;

    /**
     * @brief Normalize header name (lowercase)
     * 
     * @param header Header name to normalize
     * @return std::string Normalized header name
     */
    std::string normalizeHeader(const std::string& header) const;

private:
    CorsConfig config_;                                 ///< CORS configuration
    OriginValidator originValidator_;                   ///< Custom origin validator
    bool enabled_;                                      ///< Whether middleware is enabled

    // Statistics
    mutable std::mutex statsMutex_;                     ///< Mutex for statistics
    mutable size_t totalRequests_{0};                   ///< Total requests processed
    mutable size_t preflightRequests_{0};               ///< Preflight requests handled
    mutable size_t allowedRequests_{0};                 ///< Allowed CORS requests
    mutable size_t blockedRequests_{0};                 ///< Blocked CORS requests
    mutable size_t credentialRequests_{0};              ///< Requests with credentials

    // Cache for performance
    mutable std::unordered_set<std::string> allowedOriginsSet_;     ///< Set for fast origin lookup
    mutable std::unordered_set<std::string> allowedMethodsSet_;     ///< Set for fast method lookup
    mutable std::unordered_set<std::string> allowedHeadersSet_;     ///< Set for fast header lookup
    mutable bool originsCacheValid_{false};                        ///< Whether origins cache is valid
    mutable bool methodsCacheValid_{false};                        ///< Whether methods cache is valid
    mutable bool headersCacheValid_{false};                        ///< Whether headers cache is valid
    mutable std::mutex cacheMutex_;                                 ///< Cache mutex
};

} // namespace middleware
} // namespace cppSwitchboard 