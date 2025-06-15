/**
 * @file middleware_factory.cpp
 * @brief Implementation of the MiddlewareFactory system
 * 
 * This file implements the factory pattern for middleware instantiation,
 * including built-in creators for all standard middleware types.
 */

#include "cppSwitchboard/middleware_factory.h"
#include "cppSwitchboard/middleware_config.h"
#include "cppSwitchboard/middleware_pipeline.h"
#include "cppSwitchboard/middleware/auth_middleware.h"
#include "cppSwitchboard/middleware/authz_middleware.h"
#include "cppSwitchboard/middleware/cors_middleware.h"
#include "cppSwitchboard/middleware/logging_middleware.h"
#include "cppSwitchboard/middleware/rate_limit_middleware.h"
#include <mutex>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace cppSwitchboard {

// Import middleware types for cleaner code
using AuthMiddlewareType = cppSwitchboard::middleware::AuthMiddleware;
using AuthzMiddlewareType = cppSwitchboard::middleware::AuthzMiddleware;
using CorsMiddlewareType = cppSwitchboard::middleware::CorsMiddleware;
using LoggingMiddlewareType = cppSwitchboard::middleware::LoggingMiddleware;
using RateLimitMiddlewareType = cppSwitchboard::middleware::RateLimitMiddleware;

// Built-in Middleware Creators

/**
 * @brief Creator for authentication middleware
 */
class AuthMiddlewareCreator : public MiddlewareCreator {
public:
    std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override {
        try {
            // Get JWT secret (required)
            auto jwtSecretIt = config.config.find("jwt_secret");
            if (jwtSecretIt == config.config.end()) {
                return nullptr;
            }
            std::string jwtSecret = std::any_cast<std::string>(jwtSecretIt->second);
            
            // Create middleware with JWT secret
            auto middleware = std::make_shared<AuthMiddlewareType>(jwtSecret);
            
            // Configure additional settings
            for (const auto& [key, value] : config.config) {
                if (key == "issuer") {
                    middleware->setIssuer(std::any_cast<std::string>(value));
                } else if (key == "audience") {
                    middleware->setAudience(std::any_cast<std::string>(value));
                } else if (key == "leeway_seconds") {
                    middleware->setExpirationTolerance(std::any_cast<int>(value));
                } else if (key == "token_header") {
                    middleware->setAuthHeaderName(std::any_cast<std::string>(value));
                }
            }
            
            return middleware;
        } catch (const std::exception& e) {
            // TODO: Replace with actual logging when available
            // logger->error("Failed to create AuthMiddleware: " + std::string(e.what()));
            return nullptr;
        }
    }
    
    std::string getMiddlewareName() const override {
        return "auth";
    }
    
    bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override {
        // Check for required JWT secret
        auto jwtSecretIt = config.config.find("jwt_secret");
        if (jwtSecretIt == config.config.end()) {
            errorMessage = "auth middleware requires 'jwt_secret' configuration";
            return false;
        }
        
        try {
            std::any_cast<std::string>(jwtSecretIt->second);
        } catch (const std::bad_any_cast& e) {
            errorMessage = "auth middleware 'jwt_secret' must be a string";
            return false;
        }
        
        return true;
    }
};

/**
 * @brief Creator for authorization middleware
 */
class AuthzMiddlewareCreator : public MiddlewareCreator {
public:
    std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override {
        try {
            std::shared_ptr<AuthzMiddlewareType> middleware;
            
            // Check if required_roles is specified for constructor
            auto requiredRolesIt = config.config.find("required_roles");
            if (requiredRolesIt != config.config.end()) {
                auto requiredRoles = std::any_cast<std::vector<std::string>>(requiredRolesIt->second);
                bool requireAllRoles = false;
                
                auto requireAllIt = config.config.find("require_all_roles");
                if (requireAllIt != config.config.end()) {
                    requireAllRoles = std::any_cast<bool>(requireAllIt->second);
                }
                
                middleware = std::make_shared<AuthzMiddlewareType>(requiredRoles, requireAllRoles);
            } else {
                // Default constructor
                middleware = std::make_shared<AuthzMiddlewareType>();
            }
            
            // Configure additional settings
            for (const auto& [key, value] : config.config) {
                if (key == "require_authenticated_user") {
                    middleware->setRequireAuthentication(std::any_cast<bool>(value));
                }
                // Skip permission configuration for now to avoid potential issues
            }
            
            return middleware;
        } catch (const std::exception& e) {
            // TODO: Replace with actual logging when available
            return nullptr;
        }
    }
    
    std::string getMiddlewareName() const override {
        return "authz";
    }
    
    bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override {
        // Authorization middleware doesn't have strict required fields
        // Validate that roles and permissions are string arrays if present
        auto rolesIt = config.config.find("required_roles");
        if (rolesIt != config.config.end()) {
            try {
                std::any_cast<std::vector<std::string>>(rolesIt->second);
            } catch (const std::bad_any_cast& e) {
                errorMessage = "authz middleware 'required_roles' must be a string array";
                return false;
            }
        }
        
        auto permsIt = config.config.find("required_permissions");
        if (permsIt != config.config.end()) {
            try {
                std::any_cast<std::vector<std::string>>(permsIt->second);
            } catch (const std::bad_any_cast& e) {
                errorMessage = "authz middleware 'required_permissions' must be a string array";
                return false;
            }
        }
        
        return true;
    }
};

/**
 * @brief Creator for CORS middleware
 */
class CorsMiddlewareCreator : public MiddlewareCreator {
public:
    std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override {
        try {
            CorsMiddlewareType::CorsConfig corsConfig;
            
            // Extract configuration from the instance config
            for (const auto& [key, value] : config.config) {
                if (key == "allowed_origins") {
                    corsConfig.allowedOrigins = std::any_cast<std::vector<std::string>>(value);
                } else if (key == "allowed_methods") {
                    corsConfig.allowedMethods = std::any_cast<std::vector<std::string>>(value);
                } else if (key == "allowed_headers") {
                    corsConfig.allowedHeaders = std::any_cast<std::vector<std::string>>(value);
                } else if (key == "expose_headers") {
                    corsConfig.exposedHeaders = std::any_cast<std::vector<std::string>>(value);
                } else if (key == "allow_credentials") {
                    corsConfig.allowCredentials = std::any_cast<bool>(value);
                } else if (key == "max_age") {
                    corsConfig.maxAge = std::any_cast<int>(value);
                } else if (key == "handle_preflight") {
                    corsConfig.handlePreflight = std::any_cast<bool>(value);
                }
            }
            
            return std::make_shared<CorsMiddlewareType>(corsConfig);
        } catch (const std::exception& e) {
            // TODO: Replace with actual logging when available
            return nullptr;
        }
    }
    
    std::string getMiddlewareName() const override {
        return "cors";
    }
    
    bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override {
        // Validate string arrays if present
        auto originsIt = config.config.find("allowed_origins");
        if (originsIt != config.config.end()) {
            try {
                std::any_cast<std::vector<std::string>>(originsIt->second);
            } catch (const std::bad_any_cast& e) {
                errorMessage = "cors middleware 'allowed_origins' must be a string array";
                return false;
            }
        }
        
        auto methodsIt = config.config.find("allowed_methods");
        if (methodsIt != config.config.end()) {
            try {
                std::any_cast<std::vector<std::string>>(methodsIt->second);
            } catch (const std::bad_any_cast& e) {
                errorMessage = "cors middleware 'allowed_methods' must be a string array";
                return false;
            }
        }
        
        return true;
    }
};

/**
 * @brief Creator for logging middleware
 */
class LoggingMiddlewareCreator : public MiddlewareCreator {
public:
    std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override {
        try {
            LoggingMiddlewareType::LoggingConfig loggingConfig;
            
            // Extract configuration from the instance config
            for (const auto& [key, value] : config.config) {
                if (key == "format") {
                    std::string formatStr = std::any_cast<std::string>(value);
                    if (formatStr == "json") {
                        loggingConfig.format = LoggingMiddlewareType::LogFormat::JSON;
                    } else if (formatStr == "common" || formatStr == "apache_common") {
                        loggingConfig.format = LoggingMiddlewareType::LogFormat::COMMON;
                    } else if (formatStr == "combined" || formatStr == "apache_combined") {
                        loggingConfig.format = LoggingMiddlewareType::LogFormat::COMBINED;
                    } else if (formatStr == "custom") {
                        loggingConfig.format = LoggingMiddlewareType::LogFormat::CUSTOM;
                    }
                } else if (key == "include_headers") {
                    loggingConfig.includeHeaders = std::any_cast<bool>(value);
                } else if (key == "include_body") {
                    loggingConfig.includeBody = std::any_cast<bool>(value);
                } else if (key == "custom_format") {
                    loggingConfig.customFormat = std::any_cast<std::string>(value);
                }
            }
            
            return std::make_shared<LoggingMiddlewareType>(loggingConfig);
        } catch (const std::exception& e) {
            // TODO: Replace with actual logging when available
            return nullptr;
        }
    }
    
    std::string getMiddlewareName() const override {
        return "logging";
    }
    
    bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override {
        // Validate format if specified
        auto formatIt = config.config.find("format");
        if (formatIt != config.config.end()) {
            try {
                std::string formatStr = std::any_cast<std::string>(formatIt->second);
                if (formatStr != "json" && formatStr != "common" && formatStr != "apache_common" && 
                    formatStr != "combined" && formatStr != "apache_combined" && formatStr != "custom") {
                    errorMessage = "logging middleware 'format' must be one of: json, common, apache_common, combined, apache_combined, custom";
                    return false;
                }
            } catch (const std::bad_any_cast& e) {
                errorMessage = "logging middleware 'format' must be a string";
                return false;
            }
        }
        
        return true;
    }
};

/**
 * @brief Creator for rate limiting middleware
 */
class RateLimitMiddlewareCreator : public MiddlewareCreator {
public:
    std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override {
        try {
            // Create middleware with default configuration first
            auto middleware = std::make_shared<RateLimitMiddlewareType>();
            
            // Configure rate limits based on provided settings
            int maxRequests = 100;  // Default
            RateLimitMiddlewareType::TimeWindow timeWindow = RateLimitMiddlewareType::TimeWindow::MINUTE;
            int burstSize = -1;  // Use default
            
            // Extract configuration from the instance config
            for (const auto& [key, value] : config.config) {
                if (key == "requests_per_second") {
                    maxRequests = std::any_cast<int>(value);
                    timeWindow = RateLimitMiddlewareType::TimeWindow::SECOND;
                } else if (key == "requests_per_minute") {
                    maxRequests = std::any_cast<int>(value);
                    timeWindow = RateLimitMiddlewareType::TimeWindow::MINUTE;
                } else if (key == "requests_per_hour") {
                    maxRequests = std::any_cast<int>(value);
                    timeWindow = RateLimitMiddlewareType::TimeWindow::HOUR;
                } else if (key == "requests_per_day") {
                    maxRequests = std::any_cast<int>(value);
                    timeWindow = RateLimitMiddlewareType::TimeWindow::DAY;
                } else if (key == "burst_capacity") {
                    burstSize = std::any_cast<int>(value);
                }
            }
            
            // Set the rate limit configuration
            middleware->setRateLimit(maxRequests, timeWindow, burstSize);
            
            return middleware;
        } catch (const std::exception& e) {
            // TODO: Replace with actual logging when available  
            return nullptr;
        }
    }
    
    std::string getMiddlewareName() const override {
        return "rate_limit";
    }
    
    bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override {
        // Check that at least one rate limit is specified
        bool hasRateLimit = config.config.find("requests_per_second") != config.config.end() ||
                          config.config.find("requests_per_minute") != config.config.end() ||
                          config.config.find("requests_per_hour") != config.config.end() ||
                          config.config.find("requests_per_day") != config.config.end();
                          
        if (!hasRateLimit) {
            errorMessage = "rate_limit middleware requires at least one rate limit configuration (requests_per_second, requests_per_minute, requests_per_hour, or requests_per_day)";
            return false;
        }
        
        // Validate that numeric values are positive
        for (const auto& [key, value] : config.config) {
            if (key.find("requests_per_") == 0 || key == "burst_capacity") {
                try {
                    int intValue = std::any_cast<int>(value);
                    if (intValue <= 0) {
                        errorMessage = "rate_limit middleware '" + key + "' must be positive";
                        return false;
                    }
                } catch (const std::bad_any_cast& e) {
                    errorMessage = "rate_limit middleware '" + key + "' must be an integer";
                    return false;
                }
            }
        }
        
        return true;
    }
};

// MiddlewareFactory Implementation

MiddlewareFactory& MiddlewareFactory::getInstance() {
    static MiddlewareFactory instance;
    
    // Initialize built-in creators on first access
    if (!instance.builtinInitialized_) {
        instance.initializeBuiltinCreators();
    }
    
    return instance;
}

MiddlewareFactory::~MiddlewareFactory() {
    // Shutdown hot-reload thread
    shutdownRequested_ = true;
    if (hotReloadThread_ && hotReloadThread_->joinable()) {
        hotReloadThread_->join();
    }
}

bool MiddlewareFactory::registerCreator(std::unique_ptr<MiddlewareCreator> creator) {
    if (!creator) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(creatorsMutex_);
    
    std::string middlewareName = creator->getMiddlewareName();
    if (creators_.find(middlewareName) != creators_.end()) {
        // Middleware already registered
        return false;
    }
    
    creators_[middlewareName] = std::move(creator);
    return true;
}

bool MiddlewareFactory::unregisterCreator(const std::string& middlewareName) {
    std::lock_guard<std::mutex> lock(creatorsMutex_);
    
    auto it = creators_.find(middlewareName);
    if (it == creators_.end()) {
        return false;
    }
    
    creators_.erase(it);
    return true;
}

std::shared_ptr<Middleware> MiddlewareFactory::createMiddleware(const MiddlewareInstanceConfig& config) {
    std::lock_guard<std::mutex> lock(creatorsMutex_);
    
    auto it = creators_.find(config.name);
    if (it == creators_.end()) {
        // TODO: Replace with actual logging when available
        // logger->error("Unknown middleware type: " + config.name);
        return nullptr;
    }
    
    // Validate configuration first
    std::string errorMessage;
    if (!it->second->validateConfig(config, errorMessage)) {
        // TODO: Replace with actual logging when available
        // logger->error("Invalid configuration for middleware '" + config.name + "': " + errorMessage);
        return nullptr;
    }
    
    return it->second->create(config);
}

std::shared_ptr<MiddlewarePipeline> MiddlewareFactory::createPipeline(const std::vector<MiddlewareInstanceConfig>& middlewares) {
    auto pipeline = std::make_shared<MiddlewarePipeline>();
    
    for (const auto& middlewareConfig : middlewares) {
        if (!middlewareConfig.enabled) {
            continue;
        }
        
        auto middleware = createMiddleware(middlewareConfig);
        if (!middleware) {
            // TODO: Replace with actual logging when available
            // logger->error("Failed to create middleware: " + middlewareConfig.name);
            continue;
        }
        
        pipeline->addMiddleware(middleware);
    }
    
    return pipeline;
}

std::vector<std::string> MiddlewareFactory::getRegisteredMiddleware() const {
    std::lock_guard<std::mutex> lock(creatorsMutex_);
    
    std::vector<std::string> middlewareNames;
    middlewareNames.reserve(creators_.size());
    
    for (const auto& [name, creator] : creators_) {
        middlewareNames.push_back(name);
    }
    
    return middlewareNames;
}

bool MiddlewareFactory::isMiddlewareRegistered(const std::string& middlewareName) const {
    std::lock_guard<std::mutex> lock(creatorsMutex_);
    return creators_.find(middlewareName) != creators_.end();
}

bool MiddlewareFactory::validateMiddlewareConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const {
    std::lock_guard<std::mutex> lock(creatorsMutex_);
    
    auto it = creators_.find(config.name);
    if (it == creators_.end()) {
        errorMessage = "Unknown middleware type: " + config.name;
        return false;
    }
    
    return it->second->validateConfig(config, errorMessage);
}

void MiddlewareFactory::initializeBuiltinCreators() {
    if (builtinInitialized_) {
        return;
    }
    
    // Register built-in middleware creators WITHOUT exception handling to see actual errors
    creators_["auth"] = std::make_unique<AuthMiddlewareCreator>();
    creators_["authz"] = std::make_unique<AuthzMiddlewareCreator>();
    creators_["cors"] = std::make_unique<CorsMiddlewareCreator>();
    creators_["logging"] = std::make_unique<LoggingMiddlewareCreator>();
    creators_["rate_limit"] = std::make_unique<RateLimitMiddlewareCreator>();
    
    builtinInitialized_ = true;
}

// Plugin integration implementation

MiddlewareFactory::PluginMiddlewareCreator::PluginMiddlewareCreator(
    std::shared_ptr<MiddlewarePlugin> plugin, const std::string& middlewareType)
    : plugin_(std::move(plugin)), middlewareType_(middlewareType) {}

std::shared_ptr<Middleware> MiddlewareFactory::PluginMiddlewareCreator::create(const MiddlewareInstanceConfig& config) {
    if (!plugin_) {
        return nullptr;
    }
    
    // Increment plugin reference count for safe usage
    PluginManager::getInstance().incrementPluginRefCount(plugin_->getInfo().name);
    
    auto middleware = plugin_->createMiddleware(config);
    
    // If creation failed, decrement reference count
    if (!middleware) {
        PluginManager::getInstance().decrementPluginRefCount(plugin_->getInfo().name);
    }
    
    return middleware;
}

std::string MiddlewareFactory::PluginMiddlewareCreator::getMiddlewareName() const {
    return middlewareType_;
}

bool MiddlewareFactory::PluginMiddlewareCreator::validateConfig(
    const MiddlewareInstanceConfig& config, std::string& errorMessage) const {
    
    if (!plugin_) {
        errorMessage = "Plugin not available";
        return false;
    }
    
    return plugin_->validateConfig(config, errorMessage);
}

size_t MiddlewareFactory::loadPluginsFromDirectory(const std::string& pluginDirectory) {
    PluginManager& pluginManager = PluginManager::getInstance();
    
    // Add directory to plugin manager search paths
    if (!pluginManager.addPluginDirectory(pluginDirectory)) {
        return 0; // Directory doesn't exist or was already added
    }
    
    // Discover and load plugins
    auto results = pluginManager.discoverAndLoadPlugins();
    
    size_t successCount = 0;
    for (const auto& [pluginPath, result] : results) {
        if (result.first == PluginLoadResult::SUCCESS) {
            // Register middleware creators from this plugin
            auto plugin = pluginManager.getPlugin(result.second);
            if (plugin) {
                registerPluginCreators(plugin, result.second);
                successCount++;
            }
        }
    }
    
    return successCount;
}

bool MiddlewareFactory::loadPlugin(const std::string& pluginPath) {
    PluginManager& pluginManager = PluginManager::getInstance();
    
    auto result = pluginManager.loadPlugin(pluginPath);
    if (result.first != PluginLoadResult::SUCCESS) {
        return false;
    }
    
    // Register middleware creators from this plugin
    auto plugin = pluginManager.getPlugin(result.second);
    if (plugin) {
        registerPluginCreators(plugin, result.second);
        return true;
    }
    
    return false;
}

void MiddlewareFactory::registerPluginCreators(std::shared_ptr<MiddlewarePlugin> plugin, const std::string& pluginName) {
    auto supportedTypes = plugin->getSupportedTypes();
    
    std::lock_guard<std::mutex> lock(creatorsMutex_);
    
    for (const auto& middlewareType : supportedTypes) {
        auto creator = std::make_unique<PluginMiddlewareCreator>(plugin, middlewareType);
        creators_[middlewareType] = std::move(creator);
        pluginCreators_[middlewareType] = pluginName;
    }
}

void MiddlewareFactory::setPluginHotReloadEnabled(bool enabled, int intervalSeconds) {
    hotReloadEnabled_ = enabled;
    hotReloadInterval_ = intervalSeconds;
    
    if (enabled) {
        if (!hotReloadThread_ || !hotReloadThread_->joinable()) {
            shutdownRequested_ = false;
            hotReloadThread_ = std::make_unique<std::thread>(&MiddlewareFactory::hotReloadLoop, this);
        }
    } else {
        if (hotReloadThread_ && hotReloadThread_->joinable()) {
            shutdownRequested_ = true;
            hotReloadThread_->join();
            hotReloadThread_.reset();
        }
    }
    
    // Enable hot-reload in plugin manager as well
    PluginManager::getInstance().setHealthCheckEnabled(enabled, intervalSeconds);
}

std::vector<std::string> MiddlewareFactory::getLoadedPlugins() const {
    return PluginManager::getInstance().getLoadedPlugins();
}

void MiddlewareFactory::hotReloadLoop() {
    while (!shutdownRequested_) {
        // Wait for the specified interval
        auto waitTime = std::chrono::seconds(hotReloadInterval_.load());
        auto startTime = std::chrono::steady_clock::now();
        
        while (!shutdownRequested_ && 
               (std::chrono::steady_clock::now() - startTime) < waitTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (shutdownRequested_) {
            break;
        }
        
        // Check for plugin reloads
        auto reloadedPlugins = PluginManager::getInstance().checkAndReloadPlugins();
        
        // Re-register creators for reloaded plugins
        for (const auto& pluginName : reloadedPlugins) {
            auto plugin = PluginManager::getInstance().getPlugin(pluginName);
            if (plugin) {
                // Remove old creators for this plugin
                std::lock_guard<std::mutex> lock(creatorsMutex_);
                for (auto it = pluginCreators_.begin(); it != pluginCreators_.end();) {
                    if (it->second == pluginName) {
                        creators_.erase(it->first);
                        it = pluginCreators_.erase(it);
                    } else {
                        ++it;
                    }
                }
                
                // Register new creators
                registerPluginCreators(plugin, pluginName);
            }
        }
    }
}

} // namespace cppSwitchboard
 