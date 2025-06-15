/**
 * @file authz_middleware.cpp
 * @brief Implementation of role-based access control middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.1.0
 */

#include <cppSwitchboard/middleware/authz_middleware.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <regex>
#include <chrono>

namespace cppSwitchboard {
namespace middleware {

AuthzMiddleware::AuthzMiddleware()
    : defaultRequireAllRoles_(false)
    , userIdKey_("user_id")
    , userRolesKey_("roles")
    , userPermissionsKey_("permissions")
    , enabled_(true)
    , requireAuthentication_(true) {
}

AuthzMiddleware::AuthzMiddleware(const std::vector<std::string>& requiredRoles, bool requireAllRoles)
    : defaultRoles_(requiredRoles)
    , defaultRequireAllRoles_(requireAllRoles)
    , userIdKey_("user_id")
    , userRolesKey_("roles")
    , userPermissionsKey_("permissions")
    , enabled_(true)
    , requireAuthentication_(true) {
}

AuthzMiddleware::AuthzMiddleware(PermissionChecker permissionChecker)
    : defaultRequireAllRoles_(false)
    , permissionChecker_(permissionChecker)
    , userIdKey_("user_id")
    , userRolesKey_("roles")
    , userPermissionsKey_("permissions")
    , enabled_(true)
    , requireAuthentication_(true) {
}

HttpResponse AuthzMiddleware::handle(const HttpRequest& request, Context& context, NextHandler next) {
    if (!enabled_) {
        return next(request, context);
    }

    // Check if authentication is required and present
    if (requireAuthentication_) {
        auto authIt = context.find("authenticated");
        if (authIt == context.end() || !std::any_cast<bool>(authIt->second)) {
            return createAuthzErrorResponse("Authentication required", "", request.getPath());
        }
    }

    std::string resource = request.getPath();
    std::string userId = "";
    
    // Extract user ID if available
    auto userIdIt = context.find(userIdKey_);
    if (userIdIt != context.end()) {
        try {
            userId = std::any_cast<std::string>(userIdIt->second);
        } catch (const std::bad_any_cast&) {
            // Ignore if user ID is not a string
        }
    }

    // Find matching authorization policy
    const AuthPolicy* policy = findMatchingPolicy(resource);
    
    // If custom permission checker is set, use it
    if (permissionChecker_) {
        std::vector<std::string> requiredPermissions;
        if (policy) {
            requiredPermissions = policy->requiredPermissions;
        }
        
        if (!permissionChecker_(context, resource, requiredPermissions)) {
            return createAuthzErrorResponse("Access denied by custom policy", userId, resource);
        }
    } else {
        // Use role/permission-based authorization
        std::vector<std::string> userRoles = getUserRoles(context);
        std::vector<std::string> userPermissions = getUserPermissions(context);
        
        // Expand roles to include inherited permissions
        std::vector<std::string> effectivePermissions = expandRolesToPermissions(userRoles);
        effectivePermissions.insert(effectivePermissions.end(), userPermissions.begin(), userPermissions.end());
        
        // Check against specific policy or default roles
        if (policy) {
            // Check required roles
            if (!policy->requiredRoles.empty()) {
                if (!hasRequiredRoles(userRoles, policy->requiredRoles, policy->requireAllRoles)) {
                    return createAuthzErrorResponse("Insufficient roles for resource", userId, resource);
                }
            }
            
            // Check required permissions
            if (!policy->requiredPermissions.empty()) {
                if (!hasRequiredPermissions(effectivePermissions, policy->requiredPermissions, policy->requireAllPermissions)) {
                    return createAuthzErrorResponse("Insufficient permissions for resource", userId, resource);
                }
            }
        } else if (!defaultRoles_.empty()) {
            // Check default roles
            if (!hasRequiredRoles(userRoles, defaultRoles_, defaultRequireAllRoles_)) {
                return createAuthzErrorResponse("Insufficient default roles", userId, resource);
            }
        }
        // If no policy and no default roles, allow access
    }

    // Authorization successful, continue to next middleware/handler
    return next(request, context);
}

void AuthzMiddleware::addResourcePolicy(const std::string& resourcePattern, const AuthPolicy& policy) {
    resourcePolicies_[resourcePattern] = policy;
}

void AuthzMiddleware::addResourceRoles(const std::string& resourcePattern, 
                                      const std::vector<std::string>& requiredRoles,
                                      bool requireAllRoles) {
    AuthPolicy policy;
    policy.requiredRoles = requiredRoles;
    policy.requireAllRoles = requireAllRoles;
    policy.description = "Role-based policy for " + resourcePattern;
    
    resourcePolicies_[resourcePattern] = policy;
}

void AuthzMiddleware::addResourcePermissions(const std::string& resourcePattern,
                                            const std::vector<std::string>& requiredPermissions,
                                            bool requireAllPermissions) {
    AuthPolicy policy;
    policy.requiredPermissions = requiredPermissions;
    policy.requireAllPermissions = requireAllPermissions;
    policy.description = "Permission-based policy for " + resourcePattern;
    
    resourcePolicies_[resourcePattern] = policy;
}

void AuthzMiddleware::removeResourcePolicy(const std::string& resourcePattern) {
    resourcePolicies_.erase(resourcePattern);
    
    // Also remove from regex cache if it exists
    regexCache_.erase(resourcePattern);
}

void AuthzMiddleware::clearResourcePolicies() {
    resourcePolicies_.clear();
    regexCache_.clear();
}

void AuthzMiddleware::defineRole(const std::string& role, 
                                const std::vector<std::string>& inheritsFrom,
                                const std::vector<std::string>& permissions,
                                const std::string& description) {
    RoleHierarchy roleHierarchy;
    roleHierarchy.role = role;
    roleHierarchy.inheritsFrom = inheritsFrom;
    roleHierarchy.permissions = permissions;
    roleHierarchy.description = description;
    
    roleHierarchy_[role] = roleHierarchy;
}

void AuthzMiddleware::removeRole(const std::string& role) {
    roleHierarchy_.erase(role);
}

std::vector<std::string> AuthzMiddleware::getEffectivePermissions(const std::string& role) const {
    std::vector<std::string> permissions;
    std::unordered_set<std::string> visited; // Prevent infinite loops
    
    std::function<void(const std::string&)> collectPermissions = [&](const std::string& currentRole) {
        if (visited.count(currentRole)) {
            return; // Already processed
        }
        visited.insert(currentRole);
        
        auto it = roleHierarchy_.find(currentRole);
        if (it != roleHierarchy_.end()) {
            const RoleHierarchy& hierarchy = it->second;
            
            // Add direct permissions
            permissions.insert(permissions.end(), hierarchy.permissions.begin(), hierarchy.permissions.end());
            
            // Recursively add inherited permissions
            for (const std::string& parentRole : hierarchy.inheritsFrom) {
                collectPermissions(parentRole);
            }
        }
    };
    
    collectPermissions(role);
    
    // Remove duplicates
    std::sort(permissions.begin(), permissions.end());
    permissions.erase(std::unique(permissions.begin(), permissions.end()), permissions.end());
    
    return permissions;
}

bool AuthzMiddleware::roleHasPermission(const std::string& role, const std::string& permission) const {
    std::vector<std::string> effectivePermissions = getEffectivePermissions(role);
    return std::find(effectivePermissions.begin(), effectivePermissions.end(), permission) != effectivePermissions.end();
}

void AuthzMiddleware::setDefaultRoles(const std::vector<std::string>& roles, bool requireAllRoles) {
    defaultRoles_ = roles;
    defaultRequireAllRoles_ = requireAllRoles;
}

const AuthzMiddleware::AuthPolicy* AuthzMiddleware::findMatchingPolicy(const std::string& resource) const {
    // First, try exact match
    auto exactIt = resourcePolicies_.find(resource);
    if (exactIt != resourcePolicies_.end()) {
        return &exactIt->second;
    }
    
    // Then try pattern matching
    for (const auto& [pattern, policy] : resourcePolicies_) {
        if (matchesResourcePattern(pattern, resource)) {
            return &policy;
        }
    }
    
    return nullptr;
}

bool AuthzMiddleware::hasRequiredRoles(const std::vector<std::string>& userRoles,
                                      const std::vector<std::string>& requiredRoles,
                                      bool requireAllRoles) const {
    if (requiredRoles.empty()) {
        return true; // No roles required
    }
    
    if (requireAllRoles) {
        // User must have ALL required roles
        for (const std::string& requiredRole : requiredRoles) {
            if (std::find(userRoles.begin(), userRoles.end(), requiredRole) == userRoles.end()) {
                return false;
            }
        }
        return true;
    } else {
        // User must have at least ONE required role
        for (const std::string& requiredRole : requiredRoles) {
            if (std::find(userRoles.begin(), userRoles.end(), requiredRole) != userRoles.end()) {
                return true;
            }
        }
        return false;
    }
}

bool AuthzMiddleware::hasRequiredPermissions(const std::vector<std::string>& userPermissions,
                                            const std::vector<std::string>& requiredPermissions,
                                            bool requireAllPermissions) const {
    if (requiredPermissions.empty()) {
        return true; // No permissions required
    }
    
    if (requireAllPermissions) {
        // User must have ALL required permissions
        for (const std::string& requiredPermission : requiredPermissions) {
            if (std::find(userPermissions.begin(), userPermissions.end(), requiredPermission) == userPermissions.end()) {
                return false;
            }
        }
        return true;
    } else {
        // User must have at least ONE required permission
        for (const std::string& requiredPermission : requiredPermissions) {
            if (std::find(userPermissions.begin(), userPermissions.end(), requiredPermission) != userPermissions.end()) {
                return true;
            }
        }
        return false;
    }
}

std::vector<std::string> AuthzMiddleware::getUserRoles(const Context& context) const {
    std::vector<std::string> roles;
    
    auto it = context.find(userRolesKey_);
    if (it != context.end()) {
        try {
            roles = std::any_cast<std::vector<std::string>>(it->second);
        } catch (const std::bad_any_cast&) {
            // Try to cast as a single string
            try {
                std::string singleRole = std::any_cast<std::string>(it->second);
                roles.push_back(singleRole);
            } catch (const std::bad_any_cast&) {
                // Ignore if not convertible
            }
        }
    }
    
    return roles;
}

std::vector<std::string> AuthzMiddleware::getUserPermissions(const Context& context) const {
    std::vector<std::string> permissions;
    
    auto it = context.find(userPermissionsKey_);
    if (it != context.end()) {
        try {
            permissions = std::any_cast<std::vector<std::string>>(it->second);
        } catch (const std::bad_any_cast&) {
            // Try to cast as a single string
            try {
                std::string singlePermission = std::any_cast<std::string>(it->second);
                permissions.push_back(singlePermission);
            } catch (const std::bad_any_cast&) {
                // Ignore if not convertible
            }
        }
    }
    
    return permissions;
}

std::vector<std::string> AuthzMiddleware::expandRolesToPermissions(const std::vector<std::string>& roles) const {
    std::vector<std::string> allPermissions;
    
    for (const std::string& role : roles) {
        std::vector<std::string> rolePermissions = getEffectivePermissions(role);
        allPermissions.insert(allPermissions.end(), rolePermissions.begin(), rolePermissions.end());
    }
    
    // Remove duplicates
    std::sort(allPermissions.begin(), allPermissions.end());
    allPermissions.erase(std::unique(allPermissions.begin(), allPermissions.end()), allPermissions.end());
    
    return allPermissions;
}

bool AuthzMiddleware::matchesResourcePattern(const std::string& pattern, const std::string& path) const {
    // Simple wildcard matching first
    if (pattern == "*") {
        return true;
    }
    
    // Exact match
    if (pattern == path) {
        return true;
    }
    
    // Wildcard at the end (e.g., "/api/users/*")
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return path.substr(0, prefix.length()) == prefix;
    }
    
    // Try regex matching for more complex patterns
    try {
        auto regexIt = regexCache_.find(pattern);
        std::regex* regex = nullptr;
        
        if (regexIt == regexCache_.end()) {
            // Convert wildcard pattern to regex
            std::string regexPattern = pattern;
            
            // Escape regex special characters except * and ?
            regexPattern = std::regex_replace(regexPattern, std::regex("\\+"), "\\+");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\^"), "\\^");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\$"), "\\$");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\|"), "\\|");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\("), "\\(");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\)"), "\\)");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\["), "\\[");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\]"), "\\]");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\{"), "\\{");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\}"), "\\}");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\."), "\\.");
            
            // Convert wildcards to regex
            regexPattern = std::regex_replace(regexPattern, std::regex("\\*"), ".*");
            regexPattern = std::regex_replace(regexPattern, std::regex("\\?"), ".");
            
            // Add anchors
            regexPattern = "^" + regexPattern + "$";
            
            regexCache_[pattern] = std::make_unique<std::regex>(regexPattern);
            regex = regexCache_[pattern].get();
        } else {
            regex = regexIt->second.get();
        }
        
        return std::regex_match(path, *regex);
        
    } catch (const std::regex_error&) {
        // Fallback to simple string comparison
        return pattern == path;
    }
}

HttpResponse AuthzMiddleware::createAuthzErrorResponse(const std::string& message,
                                                      const std::string& userId,
                                                      const std::string& resource) const {
    HttpResponse response;
    response.setStatus(HttpResponse::FORBIDDEN);
    response.setHeader("Content-Type", "application/json");
    
    nlohmann::json errorBody;
    errorBody["error"] = "forbidden";
    errorBody["message"] = message;
    
    if (!userId.empty()) {
        errorBody["user_id"] = userId;
    }
    
    if (!resource.empty()) {
        errorBody["resource"] = resource;
    }
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    errorBody["timestamp"] = timestamp;
    
    response.setBody(errorBody.dump());
    return response;
}

} // namespace middleware
} // namespace cppSwitchboard 