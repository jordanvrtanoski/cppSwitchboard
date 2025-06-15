/**
 * @file plugin_manager.h
 * @brief Plugin manager for dynamic middleware loading and lifecycle management
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-01-08
 * @version 1.2.0
 * 
 * This file defines the plugin manager that handles dynamic loading of middleware
 * plugins from shared libraries. It provides cross-platform support, dependency
 * resolution, version validation, and thread-safe plugin lifecycle management.
 * 
 * @section manager_example Plugin Manager Usage Example
 * @code{.cpp}
 * PluginManager& manager = PluginManager::getInstance();
 * 
 * // Add plugin search directory
 * manager.addPluginDirectory("/opt/myapp/plugins");
 * 
 * // Load specific plugin
 * auto result = manager.loadPlugin("/path/to/my_plugin.so");
 * if (result.first == PluginLoadResult::SUCCESS) {
 *     std::cout << "Plugin loaded: " << result.second << std::endl;
 * }
 * 
 * // Discover and load all plugins
 * manager.discoverPlugins();
 * 
 * // Get loaded plugin
 * auto plugin = manager.getPlugin("MyMiddleware");
 * if (plugin) {
 *     auto middleware = plugin->createMiddleware(config);
 * }
 * @endcode
 */
#pragma once

#include <cppSwitchboard/middleware_plugin.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <filesystem>
#include <atomic>
#include <thread>
#include <functional>

namespace cppSwitchboard {

/**
 * @brief Information about a loaded plugin
 */
struct LoadedPluginInfo {
    std::string filePath;                     ///< Path to plugin file
    std::string name;                         ///< Plugin name
    PluginVersion version;                    ///< Plugin version
    std::shared_ptr<MiddlewarePlugin> plugin; ///< Plugin instance
    void* handle;                             ///< Platform-specific handle (dlopen/LoadLibrary)
    std::atomic<int> refCount{0};            ///< Reference count for safe unloading
    std::chrono::steady_clock::time_point loadTime; ///< When plugin was loaded
    bool hotReloadEnabled = false;            ///< Whether hot-reload is enabled for this plugin
    std::filesystem::file_time_type lastModified; ///< Last modification time for hot-reload
};

/**
 * @brief Plugin discovery configuration
 */
struct PluginDiscoveryConfig {
    std::vector<std::string> searchDirectories; ///< Directories to search for plugins
    std::vector<std::string> fileExtensions;    ///< File extensions to consider (.so, .dll, .dylib)
    bool recursive = true;                       ///< Whether to search recursively
    bool followSymlinks = false;                 ///< Whether to follow symbolic links
    size_t maxDepth = 10;                       ///< Maximum search depth for recursive search
};

/**
 * @brief Plugin manager for dynamic middleware loading
 * 
 * The PluginManager is responsible for:
 * - Dynamic loading/unloading of plugin shared libraries
 * - Plugin discovery in configured directories
 * - Version validation and dependency resolution
 * - Thread-safe plugin lifecycle management
 * - Hot-reload support for development
 * - Reference counting for safe unloading
 * 
 * Thread Safety: All public methods are thread-safe and can be called concurrently.
 * 
 * @code{.cpp}
 * // Singleton access
 * PluginManager& manager = PluginManager::getInstance();
 * 
 * // Configure discovery
 * PluginDiscoveryConfig config;
 * config.searchDirectories = {"/usr/lib/myapp/plugins", "./plugins"};
 * manager.setDiscoveryConfig(config);
 * 
 * // Load all discovered plugins
 * auto results = manager.discoverAndLoadPlugins();
 * for (const auto& [path, result] : results) {
 *     if (result.first != PluginLoadResult::SUCCESS) {
 *         std::cerr << "Failed to load " << path << ": " 
 *                   << pluginLoadResultToString(result.first) << std::endl;
 *     }
 * }
 * @endcode
 */
class PluginManager {
public:
    /**
     * @brief Plugin event callback function type
     * 
     * @param eventType Event type ("loaded", "unloaded", "error", "hot_reload")
     * @param pluginName Name of the plugin
     * @param message Additional event message
     */
    using PluginEventCallback = std::function<void(const std::string& eventType, 
                                                  const std::string& pluginName, 
                                                  const std::string& message)>;

    /**
     * @brief Get singleton instance of the plugin manager
     * 
     * @return PluginManager& Manager instance
     */
    static PluginManager& getInstance();
    
    /**
     * @brief Set plugin discovery configuration
     * 
     * @param config Discovery configuration
     */
    void setDiscoveryConfig(const PluginDiscoveryConfig& config);
    
    /**
     * @brief Get current discovery configuration
     * 
     * @return const PluginDiscoveryConfig& Current configuration
     */
    const PluginDiscoveryConfig& getDiscoveryConfig() const;
    
    /**
     * @brief Add a plugin search directory
     * 
     * @param directory Directory path to add
     * @return bool True if directory exists and was added
     */
    bool addPluginDirectory(const std::string& directory);
    
    /**
     * @brief Remove a plugin search directory
     * 
     * @param directory Directory path to remove
     * @return bool True if directory was found and removed
     */
    bool removePluginDirectory(const std::string& directory);
    
    /**
     * @brief Load a plugin from file
     * 
     * @param filePath Path to plugin file
     * @param hotReload Whether to enable hot-reload for this plugin
     * @return std::pair<PluginLoadResult, std::string> Result and plugin name (if successful)
     */
    std::pair<PluginLoadResult, std::string> loadPlugin(const std::string& filePath, bool hotReload = false);
    
    /**
     * @brief Unload a plugin by name
     * 
     * Plugin will only be unloaded if reference count is zero.
     * 
     * @param pluginName Name of plugin to unload
     * @return bool True if plugin was unloaded
     */
    bool unloadPlugin(const std::string& pluginName);
    
    /**
     * @brief Force unload a plugin (ignoring reference count)
     * 
     * WARNING: This can cause crashes if plugin is still in use!
     * 
     * @param pluginName Name of plugin to force unload
     * @return bool True if plugin was unloaded
     */
    bool forceUnloadPlugin(const std::string& pluginName);
    
    /**
     * @brief Discover plugins in configured directories
     * 
     * @return std::vector<std::string> List of discovered plugin file paths
     */
    std::vector<std::string> discoverPlugins() const;
    
    /**
     * @brief Discover and load all plugins
     * 
     * @return std::unordered_map<std::string, std::pair<PluginLoadResult, std::string>> 
     *         Map of file paths to load results
     */
    std::unordered_map<std::string, std::pair<PluginLoadResult, std::string>> discoverAndLoadPlugins();
    
    /**
     * @brief Get a loaded plugin by name
     * 
     * @param pluginName Name of plugin
     * @return std::shared_ptr<MiddlewarePlugin> Plugin instance or nullptr if not found
     */
    std::shared_ptr<MiddlewarePlugin> getPlugin(const std::string& pluginName);
    
    /**
     * @brief Get information about a loaded plugin
     * 
     * @param pluginName Name of plugin
     * @return std::shared_ptr<LoadedPluginInfo> Plugin info or nullptr if not found
     */
    std::shared_ptr<LoadedPluginInfo> getPluginInfo(const std::string& pluginName);
    
    /**
     * @brief Get list of loaded plugin names
     * 
     * @return std::vector<std::string> List of plugin names
     */
    std::vector<std::string> getLoadedPlugins() const;
    
    /**
     * @brief Check if a plugin is loaded
     * 
     * @param pluginName Name of plugin to check
     * @return bool True if plugin is loaded
     */
    bool isPluginLoaded(const std::string& pluginName) const;
    
    /**
     * @brief Increment reference count for a plugin
     * 
     * Call this when creating middleware instances to prevent plugin unloading.
     * 
     * @param pluginName Name of plugin
     * @return bool True if plugin was found and ref count incremented
     */
    bool incrementPluginRefCount(const std::string& pluginName);
    
    /**
     * @brief Decrement reference count for a plugin
     * 
     * Call this when destroying middleware instances.
     * 
     * @param pluginName Name of plugin
     * @return bool True if plugin was found and ref count decremented
     */
    bool decrementPluginRefCount(const std::string& pluginName);
    
    /**
     * @brief Get reference count for a plugin
     * 
     * @param pluginName Name of plugin
     * @return int Reference count (-1 if plugin not found)
     */
    int getPluginRefCount(const std::string& pluginName) const;
    
    /**
     * @brief Check for plugin file changes and reload if needed
     * 
     * Only affects plugins loaded with hot-reload enabled.
     * 
     * @return std::vector<std::string> List of plugins that were reloaded
     */
    std::vector<std::string> checkAndReloadPlugins();
    
    /**
     * @brief Validate plugin dependencies
     * 
     * Check if all dependencies for a plugin are satisfied.
     * 
     * @param pluginName Name of plugin to validate
     * @param missingDeps Output parameter for missing dependencies
     * @return bool True if all dependencies are satisfied
     */
    bool validatePluginDependencies(const std::string& pluginName, std::vector<std::string>& missingDeps) const;
    
    /**
     * @brief Get plugins that depend on a given plugin
     * 
     * @param pluginName Name of plugin to check
     * @return std::vector<std::string> List of dependent plugin names
     */
    std::vector<std::string> getDependentPlugins(const std::string& pluginName) const;
    
    /**
     * @brief Unload all plugins
     * 
     * Unloads plugins in dependency order (dependents first).
     * 
     * @param force Whether to force unload plugins with non-zero reference counts
     * @return size_t Number of plugins successfully unloaded
     */
    size_t unloadAllPlugins(bool force = false);
    
    /**
     * @brief Set plugin event callback
     * 
     * @param callback Callback function for plugin events
     */
    void setEventCallback(PluginEventCallback callback);
    
    /**
     * @brief Get plugin loading statistics
     * 
     * @return std::unordered_map<std::string, size_t> Statistics map
     */
    std::unordered_map<std::string, size_t> getStatistics() const;
    
    /**
     * @brief Enable/disable plugin health checking
     * 
     * When enabled, plugins are periodically checked for health and
     * unhealthy plugins are unloaded.
     * 
     * @param enabled Whether to enable health checking
     * @param intervalSeconds Interval between health checks (default: 60)
     */
    void setHealthCheckEnabled(bool enabled, int intervalSeconds = 60);

private:
    PluginManager() = default;
    ~PluginManager();
    
    // Non-copyable and non-movable
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager& operator=(PluginManager&&) = delete;
    
    /**
     * @brief Load plugin from handle and validate
     * 
     * @param handle Platform-specific library handle
     * @param filePath Path to plugin file
     * @param hotReload Whether hot-reload is enabled
     * @return std::pair<PluginLoadResult, std::shared_ptr<LoadedPluginInfo>> Result and plugin info
     */
    std::pair<PluginLoadResult, std::shared_ptr<LoadedPluginInfo>> loadPluginFromHandle(
        void* handle, const std::string& filePath, bool hotReload);
    
    /**
     * @brief Validate plugin version compatibility
     * 
     * @param pluginInfo Plugin metadata
     * @return bool True if compatible
     */
    bool validatePluginVersion(const MiddlewarePluginInfo& pluginInfo) const;
    
    /**
     * @brief Check if file is a valid plugin file
     * 
     * @param filePath Path to check
     * @return bool True if file appears to be a plugin
     */
    bool isValidPluginFile(const std::string& filePath) const;
    
    /**
     * @brief Get platform-specific library file extension
     * 
     * @return std::string File extension (.so, .dll, .dylib)
     */
    static std::string getLibraryExtension();
    
    /**
     * @brief Fire plugin event
     * 
     * @param eventType Event type
     * @param pluginName Plugin name
     * @param message Event message
     */
    void fireEvent(const std::string& eventType, const std::string& pluginName, const std::string& message);
    
    /**
     * @brief Health check thread function
     */
    void healthCheckLoop();
    
    mutable std::mutex mutex_;                                          ///< Mutex for thread safety
    std::unordered_map<std::string, std::shared_ptr<LoadedPluginInfo>> loadedPlugins_; ///< Loaded plugins map
    PluginDiscoveryConfig discoveryConfig_;                             ///< Discovery configuration
    PluginEventCallback eventCallback_;                                 ///< Event callback function
    
    // Statistics
    std::atomic<size_t> totalLoadAttempts_{0};      ///< Total plugin load attempts
    std::atomic<size_t> successfulLoads_{0};        ///< Successful plugin loads
    std::atomic<size_t> totalUnloads_{0};           ///< Total plugin unloads
    std::atomic<size_t> hotReloads_{0};             ///< Hot reload count
    
    // Health checking
    std::atomic<bool> healthCheckEnabled_{false};   ///< Whether health checking is enabled
    std::atomic<int> healthCheckInterval_{60};      ///< Health check interval in seconds
    std::unique_ptr<std::thread> healthCheckThread_; ///< Health check thread
    std::atomic<bool> shutdownRequested_{false};    ///< Shutdown flag for health check thread
    
    // Framework version for compatibility checking
    static constexpr PluginVersion FRAMEWORK_VERSION{1, 2, 0};
};

} // namespace cppSwitchboard 