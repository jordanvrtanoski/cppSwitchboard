/**
 * @file auth_middleware.cpp
 * @brief Implementation of JWT token-based authentication middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#include <cppSwitchboard/middleware/auth_middleware.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <nlohmann/json.hpp>

namespace cppSwitchboard {
namespace middleware {

namespace {
    /**
     * @brief Base64 URL decode
     */
    std::string base64UrlDecode(const std::string& input) {
        std::string base64 = input;
        
        // Replace URL-safe characters
        std::replace(base64.begin(), base64.end(), '-', '+');
        std::replace(base64.begin(), base64.end(), '_', '/');
        
        // Add padding if needed
        size_t padding = (4 - base64.length() % 4) % 4;
        base64.append(padding, '=');
        
        // Base64 decode
        BIO* bio = BIO_new_mem_buf(base64.data(), static_cast<int>(base64.length()));
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_push(b64, bio);
        
        std::string decoded;
        decoded.resize(base64.length());
        int decodedLength = BIO_read(bio, &decoded[0], static_cast<int>(decoded.size()));
        BIO_free_all(bio);
        
        if (decodedLength > 0) {
            decoded.resize(decodedLength);
        } else {
            decoded.clear();
        }
        
        return decoded;
    }

    /**
     * @brief Base64 URL encode
     */
    std::string base64UrlEncode(const std::string& input) {
        BIO* bio = BIO_new(BIO_s_mem());
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_push(b64, bio);
        
        BIO_write(bio, input.data(), static_cast<int>(input.length()));
        BIO_flush(bio);
        
        BUF_MEM* bufferPtr;
        BIO_get_mem_ptr(bio, &bufferPtr);
        
        std::string encoded(bufferPtr->data, bufferPtr->length);
        BIO_free_all(bio);
        
        // Replace standard base64 characters with URL-safe ones
        std::replace(encoded.begin(), encoded.end(), '+', '-');
        std::replace(encoded.begin(), encoded.end(), '/', '_');
        
        // Remove padding
        encoded.erase(std::find(encoded.begin(), encoded.end(), '='), encoded.end());
        
        return encoded;
    }

    /**
     * @brief HMAC SHA256 signature
     */
    std::string hmacSha256(const std::string& data, const std::string& key) {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen;
        
        HMAC(EVP_sha256(), key.data(), static_cast<int>(key.length()),
             reinterpret_cast<const unsigned char*>(data.data()), data.length(),
             hash, &hashLen);
        
        return std::string(reinterpret_cast<char*>(hash), hashLen);
    }

    /**
     * @brief Get current timestamp
     */
    std::chrono::system_clock::time_point getCurrentTime() {
        return std::chrono::system_clock::now();
    }

    /**
     * @brief Convert timestamp to seconds since epoch
     */
    int64_t toEpochSeconds(const std::chrono::system_clock::time_point& timePoint) {
        return std::chrono::duration_cast<std::chrono::seconds>(
            timePoint.time_since_epoch()).count();
    }

    /**
     * @brief Convert seconds since epoch to time point
     */
    std::chrono::system_clock::time_point fromEpochSeconds(int64_t seconds) {
        return std::chrono::system_clock::from_time_t(seconds);
    }
}

AuthMiddleware::AuthMiddleware(const std::string& jwtSecret, AuthScheme scheme)
    : jwtSecret_(jwtSecret)
    , expirationTolerance_(300) // 5 minutes default tolerance
    , authScheme_(scheme)
    , authHeaderName_("Authorization")
    , enabled_(true) {
}

AuthMiddleware::AuthMiddleware(TokenValidator validator, AuthScheme scheme)
    : expirationTolerance_(300)
    , authScheme_(scheme)
    , authHeaderName_("Authorization")
    , tokenValidator_(validator)
    , enabled_(true) {
}

HttpResponse AuthMiddleware::handle(const HttpRequest& request, Context& context, NextHandler next) {
    if (!enabled_) {
        return next(request, context);
    }

    // Extract token from request
    std::string token = extractToken(request);
    if (token.empty()) {
        return createAuthErrorResponse("Missing or invalid authorization header");
    }

    // Validate token
    TokenValidationResult result;
    if (authScheme_ == AuthScheme::CUSTOM && tokenValidator_) {
        result = tokenValidator_(token);
    } else {
        result = validateJwtToken(token);
    }

    if (!result.isValid) {
        return createAuthErrorResponse(result.errorMessage.empty() ? 
            "Invalid authentication token" : result.errorMessage);
    }

    // Add user information to context
    addUserInfoToContext(context, result);

    // Continue to next middleware/handler
    return next(request, context);
}

std::string AuthMiddleware::extractToken(const HttpRequest& request) const {
    std::string authHeader = request.getHeader(authHeaderName_);
    if (authHeader.empty()) {
        return "";
    }

    // Handle Bearer token format
    if (authScheme_ == AuthScheme::BEARER || authScheme_ == AuthScheme::JWT) {
        const std::string bearerPrefix = "Bearer ";
        if (authHeader.length() > bearerPrefix.length() &&
            authHeader.substr(0, bearerPrefix.length()) == bearerPrefix) {
            return authHeader.substr(bearerPrefix.length());
        }
        
        // Also accept raw token without Bearer prefix for JWT scheme
        if (authScheme_ == AuthScheme::JWT) {
            return authHeader;
        }
    } else {
        // Custom scheme - return raw header value
        return authHeader;
    }

    return "";
}

AuthMiddleware::TokenValidationResult AuthMiddleware::validateJwtToken(const std::string& token) const {
    TokenValidationResult result;

    try {
        // Split JWT token into parts
        std::vector<std::string> parts;
        std::stringstream ss(token);
        std::string part;
        
        while (std::getline(ss, part, '.')) {
            parts.push_back(part);
        }

        if (parts.size() != 3) {
            result.errorMessage = "Invalid JWT format";
            return result;
        }

        // Verify signature
        if (!verifyJwtSignature(token, jwtSecret_)) {
            result.errorMessage = "Invalid JWT signature";
            return result;
        }

        // Parse payload
        std::string payloadJson = parseJwtPayload(token);
        if (payloadJson.empty()) {
            result.errorMessage = "Failed to parse JWT payload";
            return result;
        }

        // Parse JSON payload
        nlohmann::json payload;
        try {
            payload = nlohmann::json::parse(payloadJson);
        } catch (const std::exception& e) {
            result.errorMessage = "Invalid JSON in JWT payload";
            return result;
        }

        // Validate expiration
        if (payload.contains("exp")) {
            int64_t exp = payload["exp"].get<int64_t>();
            auto expirationTime = fromEpochSeconds(exp);
            auto currentTime = getCurrentTime();
            
            if (currentTime > expirationTime + std::chrono::seconds(expirationTolerance_)) {
                result.errorMessage = "JWT token has expired";
                return result;
            }
            
            result.expirationTime = expirationTime;
        }

        // Validate issued at time
        if (payload.contains("iat")) {
            int64_t iat = payload["iat"].get<int64_t>();
            result.issuedAt = fromEpochSeconds(iat);
        }

        // Validate issuer
        if (!issuer_.empty() && payload.contains("iss")) {
            std::string tokenIssuer = payload["iss"].get<std::string>();
            if (tokenIssuer != issuer_) {
                result.errorMessage = "Invalid JWT issuer";
                return result;
            }
            result.issuer = tokenIssuer;
        }

        // Validate audience
        if (!audience_.empty() && payload.contains("aud")) {
            if (payload["aud"].is_string()) {
                std::string tokenAudience = payload["aud"].get<std::string>();
                if (tokenAudience != audience_) {
                    result.errorMessage = "Invalid JWT audience";
                    return result;
                }
                result.audience = tokenAudience;
            } else if (payload["aud"].is_array()) {
                auto audiences = payload["aud"].get<std::vector<std::string>>();
                if (std::find(audiences.begin(), audiences.end(), audience_) == audiences.end()) {
                    result.errorMessage = "Invalid JWT audience";
                    return result;
                }
                result.audience = audience_;
            }
        }

        // Extract user information
        if (payload.contains("sub")) {
            result.userId = payload["sub"].get<std::string>();
        }

        if (payload.contains("user_id")) {
            result.userId = payload["user_id"].get<std::string>();
        }

        if (payload.contains("roles")) {
            if (payload["roles"].is_array()) {
                result.roles = payload["roles"].get<std::vector<std::string>>();
            } else if (payload["roles"].is_string()) {
                result.roles.push_back(payload["roles"].get<std::string>());
            }
        }

        result.isValid = true;
        return result;

    } catch (const std::exception& e) {
        result.errorMessage = "JWT validation error: " + std::string(e.what());
        return result;
    }
}

std::string AuthMiddleware::parseJwtPayload(const std::string& token) const {
    // Split token and get payload part
    size_t firstDot = token.find('.');
    if (firstDot == std::string::npos) {
        return "";
    }
    
    size_t secondDot = token.find('.', firstDot + 1);
    if (secondDot == std::string::npos) {
        return "";
    }
    
    std::string payloadPart = token.substr(firstDot + 1, secondDot - firstDot - 1);
    return base64UrlDecode(payloadPart);
}

bool AuthMiddleware::verifyJwtSignature(const std::string& token, const std::string& secret) const {
    // Split token into header.payload and signature
    size_t lastDot = token.rfind('.');
    if (lastDot == std::string::npos) {
        return false;
    }
    
    std::string headerAndPayload = token.substr(0, lastDot);
    std::string signature = token.substr(lastDot + 1);
    
    // Calculate expected signature
    std::string expectedSignatureBinary = hmacSha256(headerAndPayload, secret);
    std::string expectedSignature = base64UrlEncode(expectedSignatureBinary);
    
    // Constant time comparison to prevent timing attacks
    if (signature.length() != expectedSignature.length()) {
        return false;
    }
    
    int result = 0;
    for (size_t i = 0; i < signature.length(); ++i) {
        result |= signature[i] ^ expectedSignature[i];
    }
    
    return result == 0;
}

HttpResponse AuthMiddleware::createAuthErrorResponse(const std::string& message) const {
    HttpResponse response;
    response.setStatus(HttpResponse::UNAUTHORIZED);
    response.setHeader("Content-Type", "application/json");
    response.setHeader("WWW-Authenticate", "Bearer");
    
    nlohmann::json errorBody;
    errorBody["error"] = "unauthorized";
    errorBody["message"] = message;
    errorBody["timestamp"] = toEpochSeconds(getCurrentTime());
    
    response.setBody(errorBody.dump());
    return response;
}

void AuthMiddleware::addUserInfoToContext(Context& context, const TokenValidationResult& result) const {
    context["authenticated"] = true;
    
    if (!result.userId.empty()) {
        context["user_id"] = result.userId;
    }
    
    if (!result.roles.empty()) {
        context["roles"] = result.roles;
    }
    
    if (!result.issuer.empty()) {
        context["jwt_issuer"] = result.issuer;
    }
    
    if (!result.audience.empty()) {
        context["jwt_audience"] = result.audience;
    }
    
    context["jwt_expiration"] = toEpochSeconds(result.expirationTime);
    context["jwt_issued_at"] = toEpochSeconds(result.issuedAt);
    
    // Authentication timestamp
    context["auth_timestamp"] = toEpochSeconds(getCurrentTime());
}

} // namespace middleware
} // namespace cppSwitchboard 