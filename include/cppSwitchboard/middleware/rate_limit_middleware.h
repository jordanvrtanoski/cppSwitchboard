/**
 * @file rate_limit_middleware.h
 * @brief Rate limiting middleware for request flood control
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#pragma once

#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <functional>
#include <string>
#include <atomic>
#include <vector>

namespace cppSwitchboard {
namespace middleware {

/**
 * @brief Rate limiting middleware using token bucket algorithm
 * 
 * This middleware implements rate limiting to prevent abuse and ensure fair usage
 * of the API. It supports multiple rate limiting strategies and can be configured
 * for various use cases.
 * 
 * Features:
 * - Token bucket algorithm for smooth rate limiting
 * - IP-based rate limiting
 * - User-based rate limiting (requires authentication context)
 * - Configurable limits (requests per second/minute/hour)
 * - Redis backend support for distributed rate limiting
 * - Sliding window implementation
 * - Custom key generators for advanced use cases
 * - Rate limit headers in HTTP responses
 * 
 * Rate limiting strategies:
 * - IP_BASED: Limit based on client IP address
 * - USER_BASED: Limit based on authenticated user ID
 * - COMBINED: Apply both IP and user limits
 * - CUSTOM: Use custom key generator function
 * 
 * @note This middleware has priority 80 and should run after authentication
 */
class RateLimitMiddleware : public Middleware {
public:
    /**
     * @brief Rate limiting strategy enumeration
     */
    enum class Strategy {
        IP_BASED,     ///< Rate limit by IP address
        USER_BASED,   ///< Rate limit by authenticated user ID
        COMBINED,     ///< Apply both IP and user-based limits
        CUSTOM        ///< Use custom key generator
    };

    /**
     * @brief Time window for rate limiting
     */
    enum class TimeWindow {
        SECOND = 1,      ///< Per second
        MINUTE = 60,     ///< Per minute
        HOUR = 3600,     ///< Per hour
        DAY = 86400      ///< Per day
    };

    /**
     * @brief Token bucket configuration
     */
    struct BucketConfig {
        int maxTokens = 100;              ///< Maximum tokens in bucket
        int refillRate = 10;              ///< Tokens added per time window
        TimeWindow refillWindow = TimeWindow::SECOND;  ///< Refill time window
        bool burstAllowed = true;         ///< Allow burst consumption
        int burstSize = 50;               ///< Maximum burst size
    };

    /**
     * @brief Rate limit configuration
     */
    struct RateLimitConfig {
        Strategy strategy = Strategy::IP_BASED;
        BucketConfig bucketConfig;
        std::string userIdKey = "user_id";     ///< Context key for user ID
        bool skipAuthenticated = false;        ///< Skip limits for authenticated users
        std::vector<std::string> whitelist;    ///< IP whitelist (no limits)
        std::vector<std::string> blacklist;    ///< IP blacklist (always blocked)
    };

    /**
     * @brief Token bucket state
     */
    struct BucketState {
        int tokens;                                          ///< Current token count
        std::chrono::steady_clock::time_point lastRefill;   ///< Last refill time
        int totalRequests = 0;                               ///< Total requests processed
        std::chrono::steady_clock::time_point createdAt;    ///< Bucket creation time
        
        BucketState(int initialTokens = 0) 
            : tokens(initialTokens)
            , lastRefill(std::chrono::steady_clock::now())
            , createdAt(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Custom key generator function type
     */
    using KeyGenerator = std::function<std::string(const HttpRequest&, const Context&)>;

    /**
     * @brief Redis backend interface
     */
    class RedisBackend {
    public:
        virtual ~RedisBackend() = default;
        virtual bool getBucket(const std::string& key, BucketState& state) = 0;
        virtual bool setBucket(const std::string& key, const BucketState& state) = 0;
        virtual bool incrementCounter(const std::string& key, int increment, int expiry) = 0;
        virtual int getCounter(const std::string& key) = 0;
        virtual bool isConnected() const = 0;
    };

    /**
     * @brief Constructor with default configuration
     */
    RateLimitMiddleware();

    /**
     * @brief Constructor with configuration
     * 
     * @param config Rate limiting configuration
     */
    explicit RateLimitMiddleware(const RateLimitConfig& config);

    /**
     * @brief Constructor with Redis backend
     * 
     * @param config Rate limiting configuration
     * @param redisBackend Redis backend for distributed rate limiting
     */
    RateLimitMiddleware(const RateLimitConfig& config, std::shared_ptr<RedisBackend> redisBackend);

    /**
     * @brief Destructor
     */
    virtual ~RateLimitMiddleware() = default;

    /**
     * @brief Process HTTP request and apply rate limiting
     * 
     * @param request The HTTP request to process
     * @param context Middleware context for sharing state
     * @param next Function to call the next middleware/handler
     * @return HttpResponse The response (may be 429 if rate limited)
     */
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override;

    /**
     * @brief Get middleware name
     * 
     * @return std::string Middleware name
     */
    std::string getName() const override { return "RateLimitMiddleware"; }

    /**
     * @brief Get middleware priority
     * 
     * @return int Priority value (80 for rate limiting)
     */
    int getPriority() const override { return 80; }

    // Configuration methods

    /**
     * @brief Set rate limiting strategy
     * 
     * @param strategy Rate limiting strategy
     */
    void setStrategy(Strategy strategy) { config_.strategy = strategy; }

    /**
     * @brief Get rate limiting strategy
     * 
     * @return Strategy Current strategy
     */
    Strategy getStrategy() const { return config_.strategy; }

    /**
     * @brief Set bucket configuration
     * 
     * @param bucketConfig Token bucket configuration
     */
    void setBucketConfig(const BucketConfig& bucketConfig) { config_.bucketConfig = bucketConfig; }

    /**
     * @brief Get bucket configuration
     * 
     * @return BucketConfig Current bucket configuration
     */
    const BucketConfig& getBucketConfig() const { return config_.bucketConfig; }

    /**
     * @brief Set rate limit (convenience method)
     * 
     * @param maxRequests Maximum requests allowed
     * @param timeWindow Time window for the limit
     * @param burstSize Optional burst size (default: same as max requests)
     */
    void setRateLimit(int maxRequests, TimeWindow timeWindow, int burstSize = -1);

    /**
     * @brief Set custom key generator
     * 
     * @param keyGenerator Custom key generation function
     */
    void setKeyGenerator(KeyGenerator keyGenerator) { keyGenerator_ = keyGenerator; }

    /**
     * @brief Set Redis backend for distributed rate limiting
     * 
     * @param redisBackend Redis backend instance
     */
    void setRedisBackend(std::shared_ptr<RedisBackend> redisBackend) { redisBackend_ = redisBackend; }

    /**
     * @brief Add IP to whitelist (no rate limiting)
     * 
     * @param ip IP address to whitelist
     */
    void addToWhitelist(const std::string& ip);

    /**
     * @brief Remove IP from whitelist
     * 
     * @param ip IP address to remove
     */
    void removeFromWhitelist(const std::string& ip);

    /**
     * @brief Add IP to blacklist (always blocked)
     * 
     * @param ip IP address to blacklist
     */
    void addToBlacklist(const std::string& ip);

    /**
     * @brief Remove IP from blacklist
     * 
     * @param ip IP address to remove
     */
    void removeFromBlacklist(const std::string& ip);

    /**
     * @brief Check if IP is whitelisted
     * 
     * @param ip IP address to check
     * @return bool True if whitelisted
     */
    bool isWhitelisted(const std::string& ip) const;

    /**
     * @brief Check if IP is blacklisted
     * 
     * @param ip IP address to check
     * @return bool True if blacklisted
     */
    bool isBlacklisted(const std::string& ip) const;

    /**
     * @brief Set user ID context key
     * 
     * @param key Context key for user ID
     */
    void setUserIdKey(const std::string& key) { config_.userIdKey = key; }

    /**
     * @brief Get user ID context key
     * 
     * @return std::string Current user ID key
     */
    const std::string& getUserIdKey() const { return config_.userIdKey; }

    /**
     * @brief Enable or disable rate limiting
     * 
     * @param enabled Whether rate limiting is enabled
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Check if rate limiting is enabled
     * 
     * @return bool Whether rate limiting is enabled
     */
    bool isEnabled() const override { return enabled_; }

    /**
     * @brief Get current bucket state for a key
     * 
     * @param key Rate limiting key
     * @return BucketState Current bucket state (empty if not found)
     */
    BucketState getBucketState(const std::string& key) const;

    /**
     * @brief Reset rate limiting for a key
     * 
     * @param key Rate limiting key to reset
     */
    void resetKey(const std::string& key);

    /**
     * @brief Clear all rate limiting data
     */
    void clearAll();

    /**
     * @brief Get statistics
     * 
     * @return std::unordered_map<std::string, int> Statistics map
     */
    std::unordered_map<std::string, int> getStatistics() const;

protected:
    /**
     * @brief Generate rate limiting key for request
     * 
     * @param request HTTP request
     * @param context Middleware context
     * @return std::string Rate limiting key
     */
    std::string generateKey(const HttpRequest& request, const Context& context) const;

    /**
     * @brief Extract client IP address from request
     * 
     * @param request HTTP request
     * @return std::string Client IP address
     */
    std::string extractClientIP(const HttpRequest& request) const;

    /**
     * @brief Extract user ID from context
     * 
     * @param context Middleware context
     * @return std::string User ID (empty if not found)
     */
    std::string extractUserId(const Context& context) const;

    /**
     * @brief Check if request should be rate limited
     * 
     * @param request HTTP request
     * @param context Middleware context
     * @return bool True if should be rate limited
     */
    bool shouldRateLimit(const HttpRequest& request, const Context& context) const;

    /**
     * @brief Consume tokens from bucket
     * 
     * @param key Rate limiting key
     * @param tokensToConsume Number of tokens to consume
     * @return bool True if tokens were consumed successfully
     */
    bool consumeTokens(const std::string& key, int tokensToConsume = 1);

    /**
     * @brief Refill tokens in bucket
     * 
     * @param state Bucket state to refill
     */
    void refillBucket(BucketState& state) const;

    /**
     * @brief Get or create bucket state
     * 
     * @param key Rate limiting key
     * @return BucketState& Reference to bucket state
     */
    BucketState& getBucketStateRef(const std::string& key);

    /**
     * @brief Create rate limit exceeded response
     * 
     * @param key Rate limiting key
     * @param retryAfter Seconds until retry allowed
     * @return HttpResponse 429 Too Many Requests response
     */
    HttpResponse createRateLimitResponse(const std::string& key, int retryAfter) const;

    /**
     * @brief Add rate limit headers to response
     * 
     * @param response HTTP response to modify
     * @param key Rate limiting key
     * @param remaining Remaining requests in window
     * @param resetTime Time when limit resets
     */
    void addRateLimitHeaders(HttpResponse& response, const std::string& key, 
                           int remaining, std::chrono::steady_clock::time_point resetTime) const;

    /**
     * @brief Calculate retry after seconds
     * 
     * @param state Bucket state
     * @return int Seconds until next token is available
     */
    int calculateRetryAfter(const BucketState& state) const;

private:
    RateLimitConfig config_;                                    ///< Rate limiting configuration
    KeyGenerator keyGenerator_;                                 ///< Custom key generator
    std::shared_ptr<RedisBackend> redisBackend_;               ///< Redis backend for distributed limiting
    bool enabled_;                                             ///< Whether rate limiting is enabled

    // Local storage for token buckets (when not using Redis)
    mutable std::unordered_map<std::string, BucketState> buckets_;  ///< Token buckets map
    mutable std::mutex bucketsMutex_;                          ///< Mutex for thread-safe bucket access

    // Statistics
    mutable std::atomic<int> totalRequests_{0};               ///< Total requests processed
    mutable std::atomic<int> blockedRequests_{0};             ///< Blocked requests count
    mutable std::atomic<int> whitelistedRequests_{0};         ///< Whitelisted requests count
};

} // namespace middleware
} // namespace cppSwitchboard 