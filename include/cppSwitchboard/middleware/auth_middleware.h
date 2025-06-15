/**
 * @file auth_middleware.h
 * @brief JWT token-based authentication middleware for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 * 
 * This file provides JWT token-based authentication middleware that validates
 * Bearer tokens and extracts user information for downstream middleware and handlers.
 * 
 * @section auth_example Authentication Middleware Usage Example
 * @code{.cpp}
 * // Create authentication middleware with JWT secret
 * auto authMiddleware = std::make_shared<AuthMiddleware>("your-secret-key");
 * 
 * // Configure JWT settings
 * authMiddleware->setIssuer("your-app");
 * authMiddleware->setAudience("your-api");
 * authMiddleware->setExpirationTolerance(300); // 5 minutes
 * 
 * // Add to pipeline
 * pipeline->addMiddleware(authMiddleware);
 * 
 * // Handler can access user information from context
 * auto handler = [](const HttpRequest& request, Context& context) {
 *     auto userId = std::any_cast<std::string>(context["user_id"]);
 *     auto roles = std::any_cast<std::vector<std::string>>(context["roles"]);
 *     // ... process authenticated request
 * };
 * @endcode
 */

#pragma once

#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

namespace cppSwitchboard {
namespace middleware {

/**
 * @brief JWT token-based authentication middleware
 * 
 * This middleware validates JWT tokens from the Authorization header and
 * extracts user information for use by downstream middleware and handlers.
 * 
 * Features:
 * - JWT token validation with configurable secret
 * - Bearer token extraction from Authorization header
 * - Configurable issuer and audience validation
 * - Expiration time validation with tolerance
 * - User context injection (user_id, roles, etc.)
 * - Flexible authentication schemes (Bearer, JWT, custom)
 * - Custom token validation functions
 * 
 * Security considerations:
 * - Tokens are validated for signature, expiration, issuer, and audience
 * - Sensitive information is not logged
 * - Constant-time comparison for token validation
 * - Configurable clock skew tolerance
 * 
 * @note This middleware has high priority (100) and should run early in the pipeline
 */
class AuthMiddleware : public Middleware {
public:
    /**
     * @brief Authentication scheme enumeration
     */
    enum class AuthScheme {
        BEARER,    ///< Bearer token authentication
        JWT,       ///< JWT token authentication
        CUSTOM     ///< Custom authentication scheme
    };

    /**
     * @brief JWT token validation result
     */
    struct TokenValidationResult {
        bool isValid = false;
        std::string userId;
        std::vector<std::string> roles;
        std::string issuer;
        std::string audience;
        std::chrono::system_clock::time_point expirationTime;
        std::chrono::system_clock::time_point issuedAt;
        std::string errorMessage;
    };

    /**
     * @brief Custom token validator function type
     */
    using TokenValidator = std::function<TokenValidationResult(const std::string& token)>;

    /**
     * @brief Constructor with JWT secret
     * 
     * @param jwtSecret The secret key for JWT token validation
     * @param scheme Authentication scheme (default: BEARER)
     */
    explicit AuthMiddleware(const std::string& jwtSecret, AuthScheme scheme = AuthScheme::BEARER);

    /**
     * @brief Constructor with custom token validator
     * 
     * @param validator Custom token validation function
     * @param scheme Authentication scheme (default: CUSTOM)
     */
    explicit AuthMiddleware(TokenValidator validator, AuthScheme scheme = AuthScheme::CUSTOM);

    /**
     * @brief Destructor
     */
    virtual ~AuthMiddleware() = default;

    /**
     * @brief Process HTTP request and validate authentication
     * 
     * @param request The HTTP request to process
     * @param context Middleware context for sharing state
     * @param next Function to call the next middleware/handler
     * @return HttpResponse The response (may be 401 if authentication fails)
     */
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override;

    /**
     * @brief Get middleware name
     * 
     * @return std::string Middleware name
     */
    std::string getName() const override { return "AuthMiddleware"; }

    /**
     * @brief Get middleware priority (high priority for authentication)
     * 
     * @return int Priority value (100 for authentication)
     */
    int getPriority() const override { return 100; }

    // Configuration methods

    /**
     * @brief Set JWT issuer for validation
     * 
     * @param issuer Expected issuer value
     */
    void setIssuer(const std::string& issuer) { issuer_ = issuer; }

    /**
     * @brief Get JWT issuer
     * 
     * @return std::string Current issuer
     */
    const std::string& getIssuer() const { return issuer_; }

    /**
     * @brief Set JWT audience for validation
     * 
     * @param audience Expected audience value
     */
    void setAudience(const std::string& audience) { audience_ = audience; }

    /**
     * @brief Get JWT audience
     * 
     * @return std::string Current audience
     */
    const std::string& getAudience() const { return audience_; }

    /**
     * @brief Set expiration tolerance in seconds
     * 
     * @param toleranceSeconds Clock skew tolerance for expiration validation
     */
    void setExpirationTolerance(int toleranceSeconds) { expirationTolerance_ = toleranceSeconds; }

    /**
     * @brief Get expiration tolerance
     * 
     * @return int Current expiration tolerance in seconds
     */
    int getExpirationTolerance() const { return expirationTolerance_; }

    /**
     * @brief Set authentication scheme
     * 
     * @param scheme Authentication scheme to use
     */
    void setAuthScheme(AuthScheme scheme) { authScheme_ = scheme; }

    /**
     * @brief Get authentication scheme
     * 
     * @return AuthScheme Current authentication scheme
     */
    AuthScheme getAuthScheme() const { return authScheme_; }

    /**
     * @brief Set custom header name for authentication
     * 
     * @param headerName Custom header name (default: "Authorization")
     */
    void setAuthHeaderName(const std::string& headerName) { authHeaderName_ = headerName; }

    /**
     * @brief Get authentication header name
     * 
     * @return std::string Current auth header name
     */
    const std::string& getAuthHeaderName() const { return authHeaderName_; }

    /**
     * @brief Set custom token validator
     * 
     * @param validator Custom token validation function
     */
    void setTokenValidator(TokenValidator validator) { tokenValidator_ = validator; }

    /**
     * @brief Enable or disable authentication
     * 
     * @param enabled Whether authentication is enabled
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Check if authentication is enabled
     * 
     * @return bool Whether authentication is enabled
     */
    bool isEnabled() const override { return enabled_; }

protected:
    /**
     * @brief Extract token from Authorization header
     * 
     * @param request HTTP request
     * @return std::string Extracted token (empty if no valid token found)
     */
    std::string extractToken(const HttpRequest& request) const;

    /**
     * @brief Validate JWT token
     * 
     * @param token JWT token to validate
     * @return TokenValidationResult Validation result with user information
     */
    TokenValidationResult validateJwtToken(const std::string& token) const;

    /**
     * @brief Parse JWT token payload
     * 
     * @param token JWT token
     * @return std::string Decoded payload JSON
     */
    std::string parseJwtPayload(const std::string& token) const;

    /**
     * @brief Verify JWT signature
     * 
     * @param token JWT token
     * @param secret JWT secret key
     * @return bool Whether signature is valid
     */
    bool verifyJwtSignature(const std::string& token, const std::string& secret) const;

    /**
     * @brief Create authentication error response
     * 
     * @param message Error message
     * @return HttpResponse 401 Unauthorized response
     */
    HttpResponse createAuthErrorResponse(const std::string& message) const;

    /**
     * @brief Add user information to context
     * 
     * @param context Middleware context
     * @param result Token validation result
     */
    void addUserInfoToContext(Context& context, const TokenValidationResult& result) const;

private:
    std::string jwtSecret_;                    ///< JWT secret key
    std::string issuer_;                       ///< Expected JWT issuer
    std::string audience_;                     ///< Expected JWT audience
    int expirationTolerance_;                  ///< Clock skew tolerance in seconds
    AuthScheme authScheme_;                    ///< Authentication scheme
    std::string authHeaderName_;               ///< Authentication header name
    TokenValidator tokenValidator_;            ///< Custom token validator
    bool enabled_;                             ///< Whether authentication is enabled
};

} // namespace middleware
} // namespace cppSwitchboard 