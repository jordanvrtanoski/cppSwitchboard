/**
 * @file rate_limit_middleware.cpp
 * @brief Implementation of rate limiting middleware with token bucket algorithm
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#include <cppSwitchboard/middleware/rate_limit_middleware.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <regex>

namespace cppSwitchboard {
namespace middleware {

RateLimitMiddleware::RateLimitMiddleware()
    : enabled_(true) {
    // Default configuration: 100 requests per minute for IP-based limiting
    config_.strategy = Strategy::IP_BASED;
    config_.bucketConfig.maxTokens = 100;
    config_.bucketConfig.refillRate = 100;
    config_.bucketConfig.refillWindow = TimeWindow::MINUTE;
    config_.bucketConfig.burstAllowed = true;
    config_.bucketConfig.burstSize = 50;
}

RateLimitMiddleware::RateLimitMiddleware(const RateLimitConfig& config)
    : config_(config)
    , enabled_(true) {
}

RateLimitMiddleware::RateLimitMiddleware(const RateLimitConfig& config, std::shared_ptr<RedisBackend> redisBackend)
    : config_(config)
    , redisBackend_(redisBackend)
    , enabled_(true) {
}

HttpResponse RateLimitMiddleware::handle(const HttpRequest& request, Context& context, NextHandler next) {
    if (!enabled_) {
        return next(request, context);
    }

    totalRequests_++;

    // Check if request should be rate limited
    if (!shouldRateLimit(request, context)) {
        whitelistedRequests_++;
        return next(request, context);
    }

    // Extract client IP for blacklist check
    std::string clientIP = extractClientIP(request);
    if (isBlacklisted(clientIP)) {
        blockedRequests_++;
        return createRateLimitResponse("blacklisted:" + clientIP, 3600); // 1 hour retry
    }

    // Generate rate limiting key
    std::string key = generateKey(request, context);
    if (key.empty()) {
        // If no key can be generated, allow the request
        return next(request, context);
    }

    // Try to consume tokens
    if (!consumeTokens(key, 1)) {
        blockedRequests_++;
        
        // Calculate retry after
        BucketState state = getBucketState(key);
        int retryAfter = calculateRetryAfter(state);
        
        return createRateLimitResponse(key, retryAfter);
    }

    // Request allowed - continue to next middleware/handler
    HttpResponse response = next(request, context);
    
    // Add rate limit headers to response
    BucketState state = getBucketState(key);
    auto resetTime = state.lastRefill + std::chrono::seconds(static_cast<int>(config_.bucketConfig.refillWindow));
    addRateLimitHeaders(response, key, state.tokens, resetTime);
    
    return response;
}

void RateLimitMiddleware::setRateLimit(int maxRequests, TimeWindow timeWindow, int burstSize) {
    config_.bucketConfig.maxTokens = maxRequests;
    config_.bucketConfig.refillRate = maxRequests;
    config_.bucketConfig.refillWindow = timeWindow;
    config_.bucketConfig.burstSize = (burstSize == -1) ? maxRequests : burstSize;
}

void RateLimitMiddleware::addToWhitelist(const std::string& ip) {
    auto& whitelist = config_.whitelist;
    if (std::find(whitelist.begin(), whitelist.end(), ip) == whitelist.end()) {
        whitelist.push_back(ip);
    }
}

void RateLimitMiddleware::removeFromWhitelist(const std::string& ip) {
    auto& whitelist = config_.whitelist;
    whitelist.erase(std::remove(whitelist.begin(), whitelist.end(), ip), whitelist.end());
}

void RateLimitMiddleware::addToBlacklist(const std::string& ip) {
    auto& blacklist = config_.blacklist;
    if (std::find(blacklist.begin(), blacklist.end(), ip) == blacklist.end()) {
        blacklist.push_back(ip);
    }
}

void RateLimitMiddleware::removeFromBlacklist(const std::string& ip) {
    auto& blacklist = config_.blacklist;
    blacklist.erase(std::remove(blacklist.begin(), blacklist.end(), ip), blacklist.end());
}

bool RateLimitMiddleware::isWhitelisted(const std::string& ip) const {
    const auto& whitelist = config_.whitelist;
    return std::find(whitelist.begin(), whitelist.end(), ip) != whitelist.end();
}

bool RateLimitMiddleware::isBlacklisted(const std::string& ip) const {
    const auto& blacklist = config_.blacklist;
    return std::find(blacklist.begin(), blacklist.end(), ip) != blacklist.end();
}

RateLimitMiddleware::BucketState RateLimitMiddleware::getBucketState(const std::string& key) const {
    if (redisBackend_ && redisBackend_->isConnected()) {
        BucketState state;
        if (redisBackend_->getBucket(key, state)) {
            return state;
        }
        return BucketState(config_.bucketConfig.maxTokens);
    }

    std::lock_guard<std::mutex> lock(bucketsMutex_);
    auto it = buckets_.find(key);
    if (it != buckets_.end()) {
        return it->second;
    }
    
    return BucketState(config_.bucketConfig.maxTokens);
}

void RateLimitMiddleware::resetKey(const std::string& key) {
    if (redisBackend_ && redisBackend_->isConnected()) {
        BucketState state(config_.bucketConfig.maxTokens);
        redisBackend_->setBucket(key, state);
        return;
    }

    std::lock_guard<std::mutex> lock(bucketsMutex_);
    buckets_.erase(key);
}

void RateLimitMiddleware::clearAll() {
    if (redisBackend_ && redisBackend_->isConnected()) {
        // Redis backend should provide a clear method, but we can't implement it here
        // since it would require knowing all keys
        return;
    }

    std::lock_guard<std::mutex> lock(bucketsMutex_);
    buckets_.clear();
}

std::unordered_map<std::string, int> RateLimitMiddleware::getStatistics() const {
    std::unordered_map<std::string, int> stats;
    stats["total_requests"] = totalRequests_.load();
    stats["blocked_requests"] = blockedRequests_.load();
    stats["whitelisted_requests"] = whitelistedRequests_.load();
    stats["allowed_requests"] = totalRequests_.load() - blockedRequests_.load();
    
    std::lock_guard<std::mutex> lock(bucketsMutex_);
    stats["active_buckets"] = static_cast<int>(buckets_.size());
    
    return stats;
}

std::string RateLimitMiddleware::generateKey(const HttpRequest& request, const Context& context) const {
    if (keyGenerator_) {
        return keyGenerator_(request, context);
    }

    switch (config_.strategy) {
        case Strategy::IP_BASED: {
            std::string ip = extractClientIP(request);
            return "ip:" + ip;
        }
        
        case Strategy::USER_BASED: {
            std::string userId = extractUserId(context);
            if (userId.empty()) {
                // Fall back to IP-based if no user ID available
                std::string ip = extractClientIP(request);
                return "ip:" + ip;
            }
            return "user:" + userId;
        }
        
        case Strategy::COMBINED: {
            std::string ip = extractClientIP(request);
            std::string userId = extractUserId(context);
            if (!userId.empty()) {
                return "combined:" + ip + ":" + userId;
            } else {
                return "ip:" + ip;
            }
        }
        
        case Strategy::CUSTOM:
            // Custom strategy should have a key generator
            return "default:" + extractClientIP(request);
    }
    
    return "";
}

std::string RateLimitMiddleware::extractClientIP(const HttpRequest& request) const {
    // Check for forwarded IP headers (proxy support)
    std::string xForwardedFor = request.getHeader("X-Forwarded-For");
    if (!xForwardedFor.empty()) {
        // Take the first IP in the list
        size_t commaPos = xForwardedFor.find(',');
        if (commaPos != std::string::npos) {
            return xForwardedFor.substr(0, commaPos);
        }
        return xForwardedFor;
    }
    
    std::string xRealIP = request.getHeader("X-Real-IP");
    if (!xRealIP.empty()) {
        return xRealIP;
    }
    
    std::string xClientIP = request.getHeader("X-Client-IP");
    if (!xClientIP.empty()) {
        return xClientIP;
    }
    
    // Fallback to a default IP if none found in headers
    // In a real implementation, this would come from the socket connection
    return "127.0.0.1";
}

std::string RateLimitMiddleware::extractUserId(const Context& context) const {
    auto it = context.find(config_.userIdKey);
    if (it != context.end()) {
        try {
            return std::any_cast<std::string>(it->second);
        } catch (const std::bad_any_cast&) {
            // Ignore if not convertible to string
        }
    }
    return "";
}

bool RateLimitMiddleware::shouldRateLimit(const HttpRequest& request, const Context& context) const {
    // Check if authenticated users should skip rate limiting
    if (config_.skipAuthenticated) {
        auto authIt = context.find("authenticated");
        if (authIt != context.end()) {
            try {
                bool isAuthenticated = std::any_cast<bool>(authIt->second);
                if (isAuthenticated) {
                    return false;
                }
            } catch (const std::bad_any_cast&) {
                // Ignore if not convertible to bool
            }
        }
    }
    
    // Check whitelist
    std::string clientIP = extractClientIP(request);
    if (isWhitelisted(clientIP)) {
        return false;
    }
    
    return true;
}

bool RateLimitMiddleware::consumeTokens(const std::string& key, int tokensToConsume) {
    if (redisBackend_ && redisBackend_->isConnected()) {
        BucketState state;
        if (!redisBackend_->getBucket(key, state)) {
            // Create new bucket
            state = BucketState(config_.bucketConfig.maxTokens);
        }
        
        // Refill bucket
        refillBucket(state);
        
        // Check if we have enough tokens
        if (state.tokens >= tokensToConsume) {
            state.tokens -= tokensToConsume;
            state.totalRequests++;
            redisBackend_->setBucket(key, state);
            return true;
        }
        
        // Update state even if request is denied
        redisBackend_->setBucket(key, state);
        return false;
    }

    // Use local storage
    std::lock_guard<std::mutex> lock(bucketsMutex_);
    BucketState& state = getBucketStateRef(key);
    
    // Refill bucket
    refillBucket(state);
    
    // Check if we have enough tokens
    if (state.tokens >= tokensToConsume) {
        state.tokens -= tokensToConsume;
        state.totalRequests++;
        return true;
    }
    
    return false;
}

void RateLimitMiddleware::refillBucket(BucketState& state) const {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastRefill = std::chrono::duration_cast<std::chrono::seconds>(now - state.lastRefill);
    
    if (timeSinceLastRefill.count() <= 0) {
        return; // No time has passed
    }
    
    // Calculate tokens to add based on time elapsed
    int windowSeconds = static_cast<int>(config_.bucketConfig.refillWindow);
    double tokensToAdd = (static_cast<double>(config_.bucketConfig.refillRate) * timeSinceLastRefill.count()) / windowSeconds;
    
    if (tokensToAdd >= 1.0) {
        int tokensInt = static_cast<int>(tokensToAdd);
        state.tokens = std::min(state.tokens + tokensInt, config_.bucketConfig.maxTokens);
        state.lastRefill = now;
    }
}

RateLimitMiddleware::BucketState& RateLimitMiddleware::getBucketStateRef(const std::string& key) {
    auto it = buckets_.find(key);
    if (it != buckets_.end()) {
        return it->second;
    }
    
    // Create new bucket
    buckets_[key] = BucketState(config_.bucketConfig.maxTokens);
    return buckets_[key];
}

HttpResponse RateLimitMiddleware::createRateLimitResponse(const std::string& key, int retryAfter) const {
    HttpResponse response;
    response.setStatus(429); // Too Many Requests
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Retry-After", std::to_string(retryAfter));
    
    // Add standard rate limit headers
    response.setHeader("X-RateLimit-Limit", std::to_string(config_.bucketConfig.maxTokens));
    response.setHeader("X-RateLimit-Remaining", "0");
    response.setHeader("X-RateLimit-Reset", std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count() + retryAfter
    ));
    
    nlohmann::json errorBody;
    errorBody["error"] = "rate_limit_exceeded";
    errorBody["message"] = "Rate limit exceeded. Too many requests.";
    errorBody["retry_after"] = retryAfter;
    errorBody["limit"] = config_.bucketConfig.maxTokens;
    errorBody["window"] = static_cast<int>(config_.bucketConfig.refillWindow);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    errorBody["timestamp"] = timestamp;
    
    if (!key.empty()) {
        errorBody["key"] = key;
    }
    
    response.setBody(errorBody.dump());
    return response;
}

void RateLimitMiddleware::addRateLimitHeaders(HttpResponse& response, const std::string& key, 
                                            int remaining, std::chrono::steady_clock::time_point resetTime) const {
    (void)key; // Key parameter reserved for future per-key header customization
    (void)resetTime; // Reset time parameter reserved for future precise timing implementation
    response.setHeader("X-RateLimit-Limit", std::to_string(config_.bucketConfig.maxTokens));
    response.setHeader("X-RateLimit-Remaining", std::to_string(remaining));
    
    // Convert steady_clock to system_clock for HTTP header
    auto resetTimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() + static_cast<int>(config_.bucketConfig.refillWindow);
    
    response.setHeader("X-RateLimit-Reset", std::to_string(resetTimeSeconds));
    response.setHeader("X-RateLimit-Window", std::to_string(static_cast<int>(config_.bucketConfig.refillWindow)));
}

int RateLimitMiddleware::calculateRetryAfter(const BucketState& state) const {
    if (state.tokens > 0) {
        return 1; // Minimum retry time
    }
    
    // Calculate time until next token is available
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastRefill = std::chrono::duration_cast<std::chrono::seconds>(now - state.lastRefill);
    
    int windowSeconds = static_cast<int>(config_.bucketConfig.refillWindow);
    int secondsPerToken = windowSeconds / config_.bucketConfig.refillRate;
    
    if (secondsPerToken <= 0) {
        secondsPerToken = 1;
    }
    
    // Time until next refill
    int timeUntilNextRefill = secondsPerToken - static_cast<int>(timeSinceLastRefill.count());
    
    return std::max(1, timeUntilNextRefill);
}

} // namespace middleware
} // namespace cppSwitchboard 