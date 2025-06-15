/**
 * @file authz_middleware.h
 * @brief Role-based access control middleware for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 * 
 * This file provides role-based access control middleware that works with
 * authentication middleware to enforce authorization policies based on user roles
 * and permissions.
 * 
 * @section authz_example Authorization Middleware Usage Example
 * @code{.cpp}
 * // Create authorization middleware with required roles
 * auto authzMiddleware = std::make_shared<AuthzMiddleware>(std::vector<std::string>{"admin", "moderator"});
 * 
 * // Configure resource-based permissions
 * authzMiddleware->addResourcePermission("/api/users", {"read", "write"});
 * authzMiddleware->addResourcePermission("/api/admin", {"admin"});
 * 
 * // Set custom permission checker
 * authzMiddleware->setPermissionChecker([](const Context& context, const std::string& resource) {
 *     auto userRoles = std::any_cast<std::vector<std::string>>(context.at("roles"));
 *     return std::find(userRoles.begin(), userRoles.end(), "admin") != userRoles.end();
 * });
 * 
 * // Add to pipeline (after authentication middleware)
 * pipeline->addMiddleware(authzMiddleware);
 * @endcode
 */

#pragma once

#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <regex>

namespace cppSwitchboard {
namespace middleware {

/**
 * @brief Role-based access control middleware
 * 
 * This middleware provides authorization capabilities by checking user roles
 * and permissions against configured access policies. It works in conjunction
 * with authentication middleware to enforce security policies.
 * 
 * Features:
 * - Role-based access control (RBAC)
 * - Resource-based permissions
 * - Route-specific authorization rules
 * - Custom permission checking functions
 * - Hierarchical role support
 * - Permission inheritance
 * - Dynamic permission evaluation
 * 
 * Authorization flow:
 * 1. Extract user information from context (set by auth middleware)
 * 2. Determine required permissions for the requested resource
 * 3. Check if user has sufficient roles/permissions
 * 4. Allow or deny access based on policy evaluation
 * 
 * @note This middleware should run after authentication middleware (lower priority ~50-99)
 */
class AuthzMiddleware : public Middleware {
public:
    /**
     * @brief Permission checking function type
     * 
     * Custom function to evaluate permissions for a specific context and resource.
     * 
     * @param context Middleware context containing user information
     * @param resource Requested resource path
     * @param requiredPermissions List of required permissions
     * @return bool Whether access should be granted
     */
    using PermissionChecker = std::function<bool(const Context& context, 
                                                const std::string& resource,
                                                const std::vector<std::string>& requiredPermissions)>;

    /**
     * @brief Authorization policy for a resource
     */
    struct AuthPolicy {
        std::vector<std::string> requiredRoles;        ///< Required user roles
        std::vector<std::string> requiredPermissions;  ///< Required permissions
        bool requireAllRoles = false;                  ///< Whether all roles are required (AND vs OR)
        bool requireAllPermissions = false;            ///< Whether all permissions are required
        std::string description;                       ///< Policy description
    };

    /**
     * @brief Role hierarchy definition
     */
    struct RoleHierarchy {
        std::string role;                              ///< Role name
        std::vector<std::string> inheritsFrom;         ///< Parent roles
        std::vector<std::string> permissions;          ///< Direct permissions
        std::string description;                       ///< Role description
    };

    /**
     * @brief Default constructor
     */
    AuthzMiddleware();

    /**
     * @brief Constructor with required roles
     * 
     * @param requiredRoles List of roles required for access
     * @param requireAllRoles Whether all roles are required (default: false - any role)
     */
    explicit AuthzMiddleware(const std::vector<std::string>& requiredRoles, 
                            bool requireAllRoles = false);

    /**
     * @brief Constructor with custom permission checker
     * 
     * @param permissionChecker Custom permission evaluation function
     */
    explicit AuthzMiddleware(PermissionChecker permissionChecker);

    /**
     * @brief Destructor
     */
    virtual ~AuthzMiddleware() = default;

    /**
     * @brief Process HTTP request and check authorization
     * 
     * @param request The HTTP request to process
     * @param context Middleware context containing user information
     * @param next Function to call the next middleware/handler
     * @return HttpResponse The response (may be 403 if authorization fails)
     */
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override;

    /**
     * @brief Get middleware name
     * 
     * @return std::string Middleware name
     */
    std::string getName() const override { return "AuthzMiddleware"; }

    /**
     * @brief Get middleware priority (medium priority for authorization)
     * 
     * @return int Priority value (90 for authorization - after auth but before business logic)
     */
    int getPriority() const override { return 90; }

    // Policy configuration methods

    /**
     * @brief Add authorization policy for a resource pattern
     * 
     * @param resourcePattern Resource pattern (supports wildcards and regex)
     * @param policy Authorization policy
     */
    void addResourcePolicy(const std::string& resourcePattern, const AuthPolicy& policy);

    /**
     * @brief Add simple role requirement for a resource
     * 
     * @param resourcePattern Resource pattern
     * @param requiredRoles Required roles
     * @param requireAllRoles Whether all roles are required
     */
    void addResourceRoles(const std::string& resourcePattern, 
                         const std::vector<std::string>& requiredRoles,
                         bool requireAllRoles = false);

    /**
     * @brief Add permission requirement for a resource
     * 
     * @param resourcePattern Resource pattern
     * @param requiredPermissions Required permissions
     * @param requireAllPermissions Whether all permissions are required
     */
    void addResourcePermissions(const std::string& resourcePattern,
                               const std::vector<std::string>& requiredPermissions,
                               bool requireAllPermissions = false);

    /**
     * @brief Remove authorization policy for a resource
     * 
     * @param resourcePattern Resource pattern to remove
     */
    void removeResourcePolicy(const std::string& resourcePattern);

    /**
     * @brief Clear all resource policies
     */
    void clearResourcePolicies();

    // Role hierarchy configuration

    /**
     * @brief Define role hierarchy
     * 
     * @param role Role to define
     * @param inheritsFrom Parent roles this role inherits from
     * @param permissions Direct permissions for this role
     * @param description Role description
     */
    void defineRole(const std::string& role, 
                   const std::vector<std::string>& inheritsFrom = {},
                   const std::vector<std::string>& permissions = {},
                   const std::string& description = "");

    /**
     * @brief Remove role definition
     * 
     * @param role Role to remove
     */
    void removeRole(const std::string& role);

    /**
     * @brief Get effective permissions for a role (including inherited)
     * 
     * @param role Role name
     * @return std::vector<std::string> All permissions (direct + inherited)
     */
    std::vector<std::string> getEffectivePermissions(const std::string& role) const;

    /**
     * @brief Check if role has specific permission
     * 
     * @param role Role to check
     * @param permission Permission to verify
     * @return bool Whether role has the permission
     */
    bool roleHasPermission(const std::string& role, const std::string& permission) const;

    // Global configuration

    /**
     * @brief Set default required roles for all resources
     * 
     * @param roles Default roles
     * @param requireAllRoles Whether all roles are required
     */
    void setDefaultRoles(const std::vector<std::string>& roles, bool requireAllRoles = false);

    /**
     * @brief Set custom permission checker function
     * 
     * @param checker Custom permission evaluation function
     */
    void setPermissionChecker(PermissionChecker checker) { permissionChecker_ = checker; }

    /**
     * @brief Set user ID context key (default: "user_id")
     * 
     * @param key Context key containing user ID
     */
    void setUserIdKey(const std::string& key) { userIdKey_ = key; }

    /**
     * @brief Set user roles context key (default: "roles")
     * 
     * @param key Context key containing user roles
     */
    void setUserRolesKey(const std::string& key) { userRolesKey_ = key; }

    /**
     * @brief Set user permissions context key (default: "permissions")
     * 
     * @param key Context key containing user permissions
     */
    void setUserPermissionsKey(const std::string& key) { userPermissionsKey_ = key; }

    /**
     * @brief Enable or disable authorization
     * 
     * @param enabled Whether authorization is enabled
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Check if authorization is enabled
     * 
     * @return bool Whether authorization is enabled
     */
    bool isEnabled() const override { return enabled_; }

    /**
     * @brief Set whether to require authentication before authorization
     * 
     * @param required Whether authentication is required (default: true)
     */
    void setRequireAuthentication(bool required) { requireAuthentication_ = required; }

protected:
    /**
     * @brief Find matching authorization policy for a resource
     * 
     * @param resource Resource path
     * @return AuthPolicy* Matching policy or nullptr if not found
     */
    const AuthPolicy* findMatchingPolicy(const std::string& resource) const;

    /**
     * @brief Check if user has required roles
     * 
     * @param userRoles User's roles
     * @param requiredRoles Required roles
     * @param requireAllRoles Whether all roles are required
     * @return bool Whether user has sufficient roles
     */
    bool hasRequiredRoles(const std::vector<std::string>& userRoles,
                         const std::vector<std::string>& requiredRoles,
                         bool requireAllRoles) const;

    /**
     * @brief Check if user has required permissions
     * 
     * @param userPermissions User's permissions
     * @param requiredPermissions Required permissions
     * @param requireAllPermissions Whether all permissions are required
     * @return bool Whether user has sufficient permissions
     */
    bool hasRequiredPermissions(const std::vector<std::string>& userPermissions,
                               const std::vector<std::string>& requiredPermissions,
                               bool requireAllPermissions) const;

    /**
     * @brief Get user roles from context
     * 
     * @param context Middleware context
     * @return std::vector<std::string> User roles
     */
    std::vector<std::string> getUserRoles(const Context& context) const;

    /**
     * @brief Get user permissions from context
     * 
     * @param context Middleware context
     * @return std::vector<std::string> User permissions
     */
    std::vector<std::string> getUserPermissions(const Context& context) const;

    /**
     * @brief Expand roles to include inherited permissions
     * 
     * @param roles User roles
     * @return std::vector<std::string> All effective permissions
     */
    std::vector<std::string> expandRolesToPermissions(const std::vector<std::string>& roles) const;

    /**
     * @brief Check if resource pattern matches path
     * 
     * @param pattern Resource pattern (may contain wildcards)
     * @param path Actual resource path
     * @return bool Whether pattern matches
     */
    bool matchesResourcePattern(const std::string& pattern, const std::string& path) const;

    /**
     * @brief Create authorization error response
     * 
     * @param message Error message
     * @param userId User ID (if available)
     * @param resource Requested resource
     * @return HttpResponse 403 Forbidden response
     */
    HttpResponse createAuthzErrorResponse(const std::string& message,
                                         const std::string& userId = "",
                                         const std::string& resource = "") const;

private:
    std::unordered_map<std::string, AuthPolicy> resourcePolicies_;  ///< Resource-based policies
    std::unordered_map<std::string, RoleHierarchy> roleHierarchy_;  ///< Role hierarchy definitions
    std::vector<std::string> defaultRoles_;                         ///< Default required roles
    bool defaultRequireAllRoles_;                                   ///< Default role requirement mode
    PermissionChecker permissionChecker_;                           ///< Custom permission checker
    std::string userIdKey_;                                         ///< Context key for user ID
    std::string userRolesKey_;                                      ///< Context key for user roles
    std::string userPermissionsKey_;                                ///< Context key for user permissions
    bool enabled_;                                                  ///< Whether authorization is enabled
    bool requireAuthentication_;                                    ///< Whether authentication is required

    // Cache for compiled regex patterns
    mutable std::unordered_map<std::string, std::unique_ptr<std::regex>> regexCache_;
};

} // namespace middleware
} // namespace cppSwitchboard 