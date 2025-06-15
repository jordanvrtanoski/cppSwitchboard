/**
 * @file middleware_config.h
 * @brief Comprehensive middleware configuration system for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.2.0
 * 
 * This file defines the middleware configuration system that supports YAML-based
 * middleware pipeline composition with comprehensive validation and hot-reload
 * capabilities. It enables configuration-driven middleware composition for both
 * global and route-specific middleware stacks.
 * 
 * @section middleware_config_example Configuration Example
 * @code{.yaml}
 * middleware:
 *   global:
 *     - name: "cors"
 *       enabled: true
 *       priority: 200
 *       config:
 *         origins: ["*"]
 *         methods: ["GET", "POST", "PUT", "DELETE"]
 *         headers: ["Content-Type", "Authorization"]
 *         
 *     - name: "logging"
 *       enabled: true
 *       priority: 0
 *       config:
 *         format: "json"
 *         include_headers: true
 *         
 *   routes:
 *     "/api/v1/\*":
 *       - name: "auth"
 *         enabled: true
 *         priority: 100
 *         config:
 *           type: "jwt"
 *           secret: "${JWT_SECRET}"
 *           
 *       - name: "rate_limit"
 *         enabled: true
 *         priority: 50
 *         config:
 *           requests_per_minute: 100
 *           per_ip: true
 * @endcode
 * 
 * @see MiddlewareFactory
 * @see MiddlewarePipeline
 * @see ConfigValidator
 */
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <any>
#include <mutex>

namespace cppSwitchboard {

// Forward declarations
class Middleware;
class MiddlewarePipeline;

// YAML parsing node structure (defined in implementation)
struct YamlNode;

/**
 * @brief Individual middleware configuration
 * 
 * Represents the configuration for a single middleware instance including
 * its name, enabled state, priority, and middleware-specific configuration
 * parameters.
 * 
 * @code{.cpp}
 * MiddlewareInstanceConfig authConfig;
 * authConfig.name = "auth";
 * authConfig.enabled = true;
 * authConfig.priority = 100;
 * authConfig.config["type"] = std::string("jwt");
 * authConfig.config["secret"] = std::string("${JWT_SECRET}");
 * @endcode
 */
struct MiddlewareInstanceConfig {
    std::string name;                                    ///< Middleware name/type identifier
    bool enabled = true;                                 ///< Whether this middleware is enabled
    int priority = 0;                                    ///< Execution priority (higher = earlier)
    std::unordered_map<std::string, std::any> config;   ///< Middleware-specific configuration
    
    /**
     * @brief Validate this middleware instance configuration
     * 
     * @param errorMessage Output parameter for validation error details
     * @return bool True if configuration is valid
     */
    bool validate(std::string& errorMessage) const;
    
    /**
     * @brief Get a string configuration value
     * 
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return std::string Configuration value or default
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    
    /**
     * @brief Get an integer configuration value
     * 
     * @param key Configuration key  
     * @param defaultValue Default value if key not found
     * @return int Configuration value or default
     */
    int getInt(const std::string& key, int defaultValue = 0) const;
    
    /**
     * @brief Get a boolean configuration value
     * 
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return bool Configuration value or default
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    /**
     * @brief Get a string array configuration value
     * 
     * @param key Configuration key
     * @return std::vector<std::string> Configuration value or empty vector
     */
    std::vector<std::string> getStringArray(const std::string& key) const;
    
    /**
     * @brief Check if a configuration key exists
     * 
     * @param key Configuration key to check
     * @return bool True if key exists
     */
    bool hasKey(const std::string& key) const;
};

/**
 * @brief Route-specific middleware configuration
 * 
 * Defines middleware configuration for a specific route pattern.
 * Supports glob patterns and regular expressions for route matching.
 * 
 * @code{.cpp}
 * RouteMiddlewareConfig route;
 * route.pattern = "/api/v1/\*";
 * route.isRegex = false;
 * 
 * MiddlewareInstanceConfig authMiddleware;
 * authMiddleware.name = "auth";
 * authMiddleware.enabled = true;
 * route.middlewares.push_back(authMiddleware);
 * @endcode
 */
struct RouteMiddlewareConfig {
    std::string pattern;                                      ///< Route pattern (glob or regex)
    bool isRegex = false;                                     ///< Whether pattern is a regular expression
    std::vector<MiddlewareInstanceConfig> middlewares;        ///< Middleware stack for this route
    
    /**
     * @brief Validate this route middleware configuration
     * 
     * @param errorMessage Output parameter for validation error details
     * @return bool True if configuration is valid
     */
    bool validate(std::string& errorMessage) const;
    
    /**
     * @brief Check if a path matches this route pattern
     * 
     * @param path HTTP request path to match
     * @return bool True if path matches the pattern
     */
    bool matchesPath(const std::string& path) const;

private:
    /**
     * @brief Simple glob pattern matching helper
     * 
     * @param text Text to match
     * @param pattern Glob pattern
     * @return bool True if text matches pattern
     */
    bool globMatch(const std::string& text, const std::string& pattern) const;
};

/**
 * @brief Global middleware configuration
 * 
 * Defines middleware that applies to all routes unless explicitly overridden.
 * Global middleware is executed before route-specific middleware.
 */
struct GlobalMiddlewareConfig {
    std::vector<MiddlewareInstanceConfig> middlewares;        ///< Global middleware stack
    
    /**
     * @brief Validate this global middleware configuration
     * 
     * @param errorMessage Output parameter for validation error details  
     * @return bool True if configuration is valid
     */
    bool validate(std::string& errorMessage) const;
};

/**
 * @brief Hot-reload configuration settings
 * 
 * Controls automatic configuration reloading capabilities.
 * When enabled, the middleware system monitors configuration files
 * for changes and automatically reloads without server restart.
 */
struct HotReloadConfig {
    bool enabled = false;                                     ///< Enable hot-reload functionality
    std::chrono::seconds checkInterval{5};                    ///< File modification check interval
    std::vector<std::string> watchedFiles;                    ///< Files to monitor for changes
    bool reloadOnChange = true;                               ///< Automatically reload on file change
    bool validateBeforeReload = true;                         ///< Validate configuration before applying
    
    /**
     * @brief Validate hot-reload configuration
     * 
     * @param errorMessage Output parameter for validation error details
     * @return bool True if configuration is valid
     */
    bool validate(std::string& errorMessage) const;
};

/**
 * @brief Complete middleware configuration system
 * 
 * The main configuration structure that encompasses all middleware
 * configuration including global middleware, route-specific middleware,
 * and hot-reload settings.
 * 
 * @code{.cpp}
 * ComprehensiveMiddlewareConfig config;
 * 
 * // Add global logging middleware
 * MiddlewareInstanceConfig logging;
 * logging.name = "logging";
 * logging.enabled = true;
 * config.global.middlewares.push_back(logging);
 * 
 * // Add route-specific auth
 * RouteMiddlewareConfig apiRoute;
 * apiRoute.pattern = "/api/\*";
 * MiddlewareInstanceConfig auth;
 * auth.name = "auth";
 * auth.enabled = true;
 * apiRoute.middlewares.push_back(auth);
 * config.routes.push_back(apiRoute);
 * @endcode
 */
struct ComprehensiveMiddlewareConfig {
    GlobalMiddlewareConfig global;                            ///< Global middleware configuration
    std::vector<RouteMiddlewareConfig> routes;                ///< Route-specific middleware configurations
    HotReloadConfig hotReload;                               ///< Hot-reload settings
    
    /**
     * @brief Validate the complete middleware configuration
     * 
     * Performs comprehensive validation of all middleware configurations
     * including dependency checking and conflict resolution.
     * 
     * @param errorMessage Output parameter for validation error details
     * @return bool True if entire configuration is valid
     */
    bool validate(std::string& errorMessage) const;
    
    /**
     * @brief Get middleware configuration for a specific route
     * 
     * Returns the combined middleware stack for a given route, including
     * global middleware and any route-specific middleware.
     * 
     * @param path HTTP request path
     * @return std::vector<MiddlewareInstanceConfig> Combined middleware stack
     */
    std::vector<MiddlewareInstanceConfig> getMiddlewareForRoute(const std::string& path) const;
    
    /**
     * @brief Get all unique middleware names used in this configuration
     * 
     * @return std::vector<std::string> List of unique middleware names
     */
    std::vector<std::string> getAllMiddlewareNames() const;
    
    /**
     * @brief Check if a specific middleware is configured
     * 
     * @param middlewareName Name of middleware to check
     * @return bool True if middleware is configured somewhere
     */
    bool hasMiddleware(const std::string& middlewareName) const;
};

/**
 * @brief Result type for middleware configuration operations
 * 
 * Encapsulates the result of configuration loading or validation operations
 * with comprehensive error reporting.
 */
enum class MiddlewareConfigError {
    SUCCESS,
    FILE_NOT_FOUND,
    INVALID_YAML,
    VALIDATION_FAILED,
    UNKNOWN_MIDDLEWARE,
    INVALID_PRIORITY,
    DUPLICATE_MIDDLEWARE,
    CIRCULAR_DEPENDENCY,
    MISSING_REQUIRED_CONFIG
};

/**
 * @brief Configuration operation result
 * 
 * Contains the result of a configuration operation along with detailed
 * error information for troubleshooting.
 */
struct MiddlewareConfigResult {
    MiddlewareConfigError error = MiddlewareConfigError::SUCCESS;  ///< Operation result code
    std::string message;                                           ///< Detailed error message
    std::string context;                                          ///< Additional context information
    
    bool isSuccess() const { return error == MiddlewareConfigError::SUCCESS; }
    bool hasError() const { return error != MiddlewareConfigError::SUCCESS; }
    
    static MiddlewareConfigResult success() {
        return MiddlewareConfigResult{MiddlewareConfigError::SUCCESS, "", ""};
    }
    
    static MiddlewareConfigResult failure(MiddlewareConfigError err, const std::string& msg, const std::string& ctx = "") {
        return MiddlewareConfigResult{err, msg, ctx};
    }
};

/**
 * @brief Middleware configuration loader and parser
 * 
 * Handles loading middleware configuration from YAML files or strings,
 * with comprehensive validation and error reporting. Supports environment
 * variable substitution and configuration merging.
 * 
 * @code{.cpp}
 * MiddlewareConfigLoader loader;
 * auto result = loader.loadFromFile("/etc/middleware.yaml");
 * if (result.isSuccess()) {
 *     ComprehensiveMiddlewareConfig config = loader.getConfiguration();
 *     // Use configuration...
 * } else {
 *     std::cerr << "Config error: " << result.message << std::endl;
 * }
 * @endcode
 */
class MiddlewareConfigLoader {
public:
    /**
     * @brief Default constructor
     */
    MiddlewareConfigLoader() = default;
    
    /**
     * @brief Destructor
     */
    virtual ~MiddlewareConfigLoader() = default;
    
    /**
     * @brief Load middleware configuration from YAML file
     * 
     * @param filename Path to YAML configuration file
     * @return MiddlewareConfigResult Operation result
     */
    MiddlewareConfigResult loadFromFile(const std::string& filename);
    
    /**
     * @brief Load middleware configuration from YAML string
     * 
     * @param yamlContent YAML configuration content
     * @return MiddlewareConfigResult Operation result
     */
    MiddlewareConfigResult loadFromString(const std::string& yamlContent);
    
    /**
     * @brief Merge additional configuration file
     * 
     * Merges settings from another configuration file into the current
     * configuration. Later configurations override earlier ones.
     * 
     * @param filename Path to additional YAML configuration file
     * @return MiddlewareConfigResult Operation result
     */
    MiddlewareConfigResult mergeFromFile(const std::string& filename);
    
    /**
     * @brief Get the loaded configuration
     * 
     * @return const ComprehensiveMiddlewareConfig& Current configuration
     * @throws std::runtime_error if no configuration has been loaded
     */
    const ComprehensiveMiddlewareConfig& getConfiguration() const;
    
    /**
     * @brief Create default middleware configuration
     * 
     * Creates a sensible default configuration with basic middleware enabled.
     * 
     * @return ComprehensiveMiddlewareConfig Default configuration
     */
    static ComprehensiveMiddlewareConfig createDefault();
    
    /**
     * @brief Validate middleware configuration
     * 
     * @param config Configuration to validate
     * @return MiddlewareConfigResult Validation result
     */
    static MiddlewareConfigResult validateConfiguration(const ComprehensiveMiddlewareConfig& config);
    
    /**
     * @brief Enable environment variable substitution
     * 
     * When enabled, configuration values containing ${VAR_NAME} will be
     * replaced with environment variable values.
     * 
     * @param enabled Whether to enable substitution
     */
    void setEnvironmentSubstitution(bool enabled) { environmentSubstitution_ = enabled; }
    
    /**
     * @brief Check if environment variable substitution is enabled
     * 
     * @return bool True if substitution is enabled
     */
    bool isEnvironmentSubstitutionEnabled() const { return environmentSubstitution_; }

private:
    ComprehensiveMiddlewareConfig config_;                    ///< Loaded configuration
    bool loaded_ = false;                                     ///< Whether configuration has been loaded
    bool environmentSubstitution_ = true;                     ///< Enable environment variable substitution
    mutable std::mutex configMutex_;                         ///< Thread safety for configuration access
    
    /**
     * @brief Parse YAML node into middleware instance configuration
     * 
     * @param yamlNode YAML node containing middleware configuration
     * @param config Output middleware instance configuration
     * @return MiddlewareConfigResult Parse result
     */
    MiddlewareConfigResult parseMiddlewareInstance(const YamlNode& yamlNode, MiddlewareInstanceConfig& config);
    
    /**
     * @brief Parse global middleware configuration from YAML
     * 
     * @param yamlNode YAML node containing global middleware
     * @param config Output global middleware configuration
     * @return MiddlewareConfigResult Parse result
     */
    MiddlewareConfigResult parseGlobalMiddleware(const YamlNode& yamlNode, GlobalMiddlewareConfig& config);
    
    /**
     * @brief Parse route middleware configuration from YAML
     * 
     * @param yamlNode YAML node containing route middleware
     * @param routes Output route middleware configurations
     * @return MiddlewareConfigResult Parse result
     */
    MiddlewareConfigResult parseRouteMiddleware(const YamlNode& yamlNode, std::vector<RouteMiddlewareConfig>& routes);
    
    /**
     * @brief Parse hot-reload configuration from YAML
     * 
     * @param yamlNode YAML node containing hot-reload settings
     * @param config Output hot-reload configuration
     * @return MiddlewareConfigResult Parse result
     */
    MiddlewareConfigResult parseHotReloadConfig(const YamlNode& yamlNode, HotReloadConfig& config);
    
    /**
     * @brief Parse YAML configuration node into std::any map
     * 
     * @param node YAML node to parse
     * @param config Output configuration map
     */
    void parseConfigNode(const YamlNode& node, std::unordered_map<std::string, std::any>& config);
    
    /**
     * @brief Substitute environment variables in configuration values
     * 
     * @param value Configuration value that may contain ${VAR} patterns
     * @return std::string Value with environment variables substituted
     */
    std::string substituteEnvironmentVariables(const std::string& value) const;
    
    /**
     * @brief Merge two middleware configurations
     * 
     * @param base Base configuration to merge into
     * @param overlay Configuration to overlay on top
     */
    void mergeConfigurations(ComprehensiveMiddlewareConfig& base, const ComprehensiveMiddlewareConfig& overlay);
};

// Forward declarations for middleware factory - full definitions in middleware_factory.h
class MiddlewareCreator;
class MiddlewareFactory;

} // namespace cppSwitchboard
 