/**
 * @file middleware_plugin.h
 * @brief Plugin interface for dynamically loaded middleware
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-01-08
 * @version 1.2.0
 * 
 * This file defines the plugin interface for dynamically loading middleware
 * from shared libraries (.so/.dll). The plugin system provides ABI compatibility
 * through C-style exports and supports versioning, dependencies, and lifecycle management.
 * 
 * @section plugin_example Plugin Implementation Example
 * @code{.cpp}
 * // my_plugin.cpp
 * extern "C" {
 *     // Plugin metadata
 *     CPPSWITCH_PLUGIN_EXPORT MiddlewarePluginInfo cppSwitchboard_plugin_info = {
 *         .version = CPPSWITCH_PLUGIN_VERSION,
 *         .name = "MyMiddleware",
 *         .description = "Custom middleware plugin",
 *         .author = "Developer Name",
 *         .plugin_version = {1, 0, 0},
 *         .min_framework_version = {1, 2, 0},
 *         .dependencies = nullptr,
 *         .dependency_count = 0
 *     };
 *     
 *     // Plugin factory function
 *     CPPSWITCH_PLUGIN_EXPORT MiddlewarePlugin* cppSwitchboard_create_plugin() {
 *         return new MyMiddlewarePlugin();
 *     }
 *     
 *     // Plugin cleanup function
 *     CPPSWITCH_PLUGIN_EXPORT void cppSwitchboard_destroy_plugin(MiddlewarePlugin* plugin) {
 *         delete plugin;
 *     }
 * }
 * @endcode
 */
#pragma once

#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/middleware_config.h>
#include <cstdint>

// Plugin ABI version - increment when breaking changes are made
#define CPPSWITCH_PLUGIN_VERSION 1

// Export macro for plugin functions
#ifdef _WIN32
    #define CPPSWITCH_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define CPPSWITCH_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

/**
 * @brief Version structure for semantic versioning
 */
struct PluginVersion {
    uint16_t major = 0;   ///< Major version (breaking changes)
    uint16_t minor = 0;   ///< Minor version (new features)
    uint16_t patch = 0;   ///< Patch version (bug fixes)
    
    /**
     * @brief Compare two versions
     * @param other Version to compare with
     * @return int -1 if this < other, 0 if equal, 1 if this > other
     */
    int compare(const PluginVersion& other) const {
        if (major != other.major) return (major < other.major) ? -1 : 1;
        if (minor != other.minor) return (minor < other.minor) ? -1 : 1;
        if (patch != other.patch) return (patch < other.patch) ? -1 : 1;
        return 0;
    }
    
    /**
     * @brief Check if this version is compatible with required version
     * @param required Required minimum version
     * @return bool True if compatible (major must match, minor.patch >= required)
     */
    bool isCompatible(const PluginVersion& required) const {
        if (major != required.major) return false;
        if (minor < required.minor) return false;
        if (minor == required.minor && patch < required.patch) return false;
        return true;
    }
};

/**
 * @brief Plugin dependency information
 */
struct PluginDependency {
    const char* name;                ///< Name of required plugin
    PluginVersion min_version;       ///< Minimum required version
    bool optional;                   ///< True if dependency is optional
};

/**
 * @brief Plugin metadata structure
 * 
 * This structure contains all metadata about a plugin that is exported
 * via the C interface. It must be exported as 'cppSwitchboard_plugin_info'.
 */
struct MiddlewarePluginInfo {
    uint32_t version;                      ///< Plugin ABI version (must be CPPSWITCH_PLUGIN_VERSION)
    const char* name;                      ///< Plugin name (must be unique)
    const char* description;               ///< Plugin description
    const char* author;                    ///< Plugin author
    PluginVersion plugin_version;          ///< Plugin version
    PluginVersion min_framework_version;   ///< Minimum required framework version
    const PluginDependency* dependencies; ///< Array of plugin dependencies
    size_t dependency_count;               ///< Number of dependencies
};

} // extern "C"

namespace cppSwitchboard {

/**
 * @brief Abstract base class for plugin middleware implementations
 * 
 * Plugin middleware must inherit from this class and implement the required methods.
 * The plugin system manages the lifecycle of these objects through the C interface.
 */
class MiddlewarePlugin {
public:
    virtual ~MiddlewarePlugin() = default;
    
    /**
     * @brief Initialize the plugin
     * 
     * Called after the plugin is loaded and before any middleware instances are created.
     * Use this for one-time initialization, resource allocation, or dependency setup.
     * 
     * @param frameworkVersion Version of the cppSwitchboard framework
     * @return bool True if initialization succeeded
     */
    virtual bool initialize(const PluginVersion& frameworkVersion) = 0;
    
    /**
     * @brief Shutdown the plugin
     * 
     * Called before the plugin is unloaded. Use this to clean up resources,
     * close connections, or perform other cleanup operations.
     */
    virtual void shutdown() = 0;
    
    /**
     * @brief Create a middleware instance
     * 
     * Factory method to create middleware instances based on configuration.
     * This method may be called multiple times to create multiple instances.
     * 
     * @param config Middleware configuration
     * @return std::shared_ptr<Middleware> Created middleware instance or nullptr on failure
     */
    virtual std::shared_ptr<Middleware> createMiddleware(const MiddlewareInstanceConfig& config) = 0;
    
    /**
     * @brief Validate middleware configuration
     * 
     * Validate that the provided configuration is valid for this plugin's middleware.
     * Called before createMiddleware to ensure configuration is correct.
     * 
     * @param config Configuration to validate
     * @param errorMessage Output parameter for validation error details
     * @return bool True if configuration is valid
     */
    virtual bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const = 0;
    
    /**
     * @brief Get supported middleware types
     * 
     * Return a list of middleware type names that this plugin can create.
     * Used for discovery and configuration validation.
     * 
     * @return std::vector<std::string> List of supported middleware type names
     */
    virtual std::vector<std::string> getSupportedTypes() const = 0;
    
    /**
     * @brief Get plugin metadata
     * 
     * Return the plugin metadata. This should match the metadata exported
     * via the C interface.
     * 
     * @return const MiddlewarePluginInfo& Plugin metadata
     */
    virtual const MiddlewarePluginInfo& getInfo() const = 0;
    
    /**
     * @brief Check if plugin is healthy
     * 
     * Called periodically to check plugin health. Return false if the plugin
     * is in an unhealthy state and should be unloaded.
     * 
     * @return bool True if plugin is healthy
     */
    virtual bool isHealthy() const { return true; }
    
    /**
     * @brief Get plugin configuration schema
     * 
     * Return JSON schema or description of the configuration format expected
     * by this plugin. Used for documentation and validation.
     * 
     * @return std::string Configuration schema (JSON Schema format preferred)
     */
    virtual std::string getConfigSchema() const { return "{}"; }
};

/**
 * @brief Plugin loading result
 */
enum class PluginLoadResult {
    SUCCESS,                    ///< Plugin loaded successfully
    FILE_NOT_FOUND,            ///< Plugin file not found
    INVALID_FORMAT,            ///< Invalid plugin file format
    MISSING_EXPORTS,           ///< Required exports not found
    VERSION_MISMATCH,          ///< Plugin version incompatible
    DEPENDENCY_MISSING,        ///< Required dependency not available
    INITIALIZATION_FAILED,     ///< Plugin initialization failed
    ALREADY_LOADED,           ///< Plugin with same name already loaded
    UNKNOWN_ERROR             ///< Unknown error occurred
};

/**
 * @brief Convert plugin load result to string
 * 
 * @param result Plugin load result
 * @return const char* Human-readable error message
 */
inline const char* pluginLoadResultToString(PluginLoadResult result) {
    switch (result) {
        case PluginLoadResult::SUCCESS: return "Success";
        case PluginLoadResult::FILE_NOT_FOUND: return "Plugin file not found";
        case PluginLoadResult::INVALID_FORMAT: return "Invalid plugin file format";
        case PluginLoadResult::MISSING_EXPORTS: return "Required exports not found in plugin";
        case PluginLoadResult::VERSION_MISMATCH: return "Plugin version incompatible with framework";
        case PluginLoadResult::DEPENDENCY_MISSING: return "Required plugin dependency not available";
        case PluginLoadResult::INITIALIZATION_FAILED: return "Plugin initialization failed";
        case PluginLoadResult::ALREADY_LOADED: return "Plugin with same name already loaded";
        case PluginLoadResult::UNKNOWN_ERROR: return "Unknown error occurred";
        default: return "Invalid result code";
    }
}

} // namespace cppSwitchboard

// C interface function signatures that plugins must export
extern "C" {
    /**
     * @brief Plugin info export (must be named exactly 'cppSwitchboard_plugin_info')
     */
    typedef MiddlewarePluginInfo cppSwitchboard_plugin_info_t;
    
    /**
     * @brief Create plugin instance function signature
     * 
     * Plugins must export a function with this signature named 'cppSwitchboard_create_plugin'
     * that returns a new instance of their MiddlewarePlugin implementation.
     * 
     * @return MiddlewarePlugin* New plugin instance
     */
    typedef cppSwitchboard::MiddlewarePlugin* (*cppSwitchboard_create_plugin_t)();
    
    /**
     * @brief Destroy plugin instance function signature
     * 
     * Plugins must export a function with this signature named 'cppSwitchboard_destroy_plugin'
     * that properly destroys the plugin instance.
     * 
     * @param plugin Plugin instance to destroy
     */
    typedef void (*cppSwitchboard_destroy_plugin_t)(cppSwitchboard::MiddlewarePlugin* plugin);
} 