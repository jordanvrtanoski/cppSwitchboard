/**
 * @file middleware_factory.h
 * @brief Middleware factory for configuration-driven middleware instantiation
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-01-08
 * @version 1.2.0
 * 
 * This file defines the middleware factory system that enables configuration-driven
 * middleware instantiation using a registry pattern. It supports both built-in
 * middleware and custom middleware registration.
 */
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <cppSwitchboard/plugin_manager.h>

namespace cppSwitchboard {

// Forward declarations
class Middleware;
class MiddlewarePipeline;
struct MiddlewareInstanceConfig;

/**
 * @brief Middleware factory interface for configuration-driven instantiation
 * 
 * Defines the interface for creating middleware instances from configuration.
 * Implementations should be registered with the MiddlewareFactory registry.
 */
class MiddlewareCreator {
public:
    virtual ~MiddlewareCreator() = default;
    
    /**
     * @brief Create middleware instance from configuration
     * 
     * @param config Middleware instance configuration
     * @return std::shared_ptr<Middleware> Created middleware instance or nullptr if failed
     */
    virtual std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) = 0;
    
    /**
     * @brief Get the name of middleware this creator handles
     * 
     * @return std::string Middleware name/type
     */ 
    virtual std::string getMiddlewareName() const = 0;
    
    /**
     * @brief Validate configuration before creating middleware
     * 
     * @param config Configuration to validate
     * @param errorMessage Output parameter for validation errors
     * @return bool True if configuration is valid for this middleware
     */
    virtual bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const = 0;
};

/**
 * @brief Factory for creating middleware instances from configuration
 * 
 * Central registry for middleware creators that enables configuration-driven
 * middleware instantiation. Thread-safe and supports custom middleware registration.
 * 
 * @code{.cpp}
 * MiddlewareFactory& factory = MiddlewareFactory::getInstance();
 * 
 * // Register custom middleware creator
 * factory.registerCreator(std::make_unique<CustomMiddlewareCreator>());
 * 
 * // Create middleware from configuration
 * MiddlewareInstanceConfig config;
 * config.name = "auth";
 * auto middleware = factory.createMiddleware(config);
 * @endcode
 */
class MiddlewareFactory {
public:
    /**
     * @brief Get singleton instance of the factory
     * 
     * @return MiddlewareFactory& Factory instance
     */
    static MiddlewareFactory& getInstance();
    
    /**
     * @brief Register a middleware creator
     * 
     * @param creator Unique pointer to middleware creator
     * @return bool True if registration succeeded
     */
    bool registerCreator(std::unique_ptr<MiddlewareCreator> creator);
    
    /**
     * @brief Unregister a middleware creator
     * 
     * @param middlewareName Name of middleware to unregister
     * @return bool True if middleware was found and unregistered
     */
    bool unregisterCreator(const std::string& middlewareName);
    
    /**
     * @brief Create middleware instance from configuration
     * 
     * @param config Middleware configuration
     * @return std::shared_ptr<Middleware> Created middleware or nullptr if failed
     */
    std::shared_ptr<Middleware> createMiddleware(const MiddlewareInstanceConfig& config);
    
    /**
     * @brief Create middleware pipeline from configuration
     * 
     * @param middlewares Vector of middleware configurations
     * @return std::shared_ptr<MiddlewarePipeline> Created pipeline with middleware
     */
    std::shared_ptr<MiddlewarePipeline> createPipeline(const std::vector<MiddlewareInstanceConfig>& middlewares);
    
    /**
     * @brief Get list of registered middleware names
     * 
     * @return std::vector<std::string> List of available middleware types
     */
    std::vector<std::string> getRegisteredMiddleware() const;
    
    /**
     * @brief Check if a middleware type is registered
     * 
     * @param middlewareName Name of middleware to check
     * @return bool True if middleware is registered
     */
    bool isMiddlewareRegistered(const std::string& middlewareName) const;
    
    /**
     * @brief Validate middleware configuration
     * 
     * @param config Configuration to validate
     * @param errorMessage Output parameter for validation errors
     * @return bool True if configuration is valid
     */
    bool validateMiddlewareConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const;
    
    /**
     * @brief Load plugins from directory and register their creators
     * 
     * @param pluginDirectory Directory containing plugin files
     * @return size_t Number of plugins successfully loaded
     */
    size_t loadPluginsFromDirectory(const std::string& pluginDirectory);
    
    /**
     * @brief Load a specific plugin and register its creators
     * 
     * @param pluginPath Path to plugin file
     * @return bool True if plugin loaded successfully
     */
    bool loadPlugin(const std::string& pluginPath);
    
    /**
     * @brief Enable hot-reload for plugins
     * 
     * When enabled, the factory will periodically check for plugin changes
     * and reload them automatically.
     * 
     * @param enabled Whether to enable hot-reload
     * @param intervalSeconds Check interval in seconds (default: 30)
     */
    void setPluginHotReloadEnabled(bool enabled, int intervalSeconds = 30);
    
    /**
     * @brief Get information about loaded plugins
     * 
     * @return std::vector<std::string> List of loaded plugin names
     */
    std::vector<std::string> getLoadedPlugins() const;

private:
    MiddlewareFactory() = default;
    ~MiddlewareFactory();  // Need custom destructor for thread cleanup
    
    // Non-copyable and non-movable
    MiddlewareFactory(const MiddlewareFactory&) = delete;
    MiddlewareFactory& operator=(const MiddlewareFactory&) = delete;
    MiddlewareFactory(MiddlewareFactory&&) = delete;
    MiddlewareFactory& operator=(MiddlewareFactory&&) = delete;
    
    std::unordered_map<std::string, std::unique_ptr<MiddlewareCreator>> creators_;  ///< Registered middleware creators
    mutable std::mutex creatorsMutex_;                                             ///< Thread safety for creator access
    
    /**
     * @brief Initialize built-in middleware creators
     */
    void initializeBuiltinCreators();
    
    /**
     * @brief Check if built-in creators have been initialized
     */
    bool builtinInitialized_ = false;
    
    /**
     * @brief Plugin creator wrapper to integrate plugins with factory system
     */
    class PluginMiddlewareCreator : public MiddlewareCreator {
    public:
        PluginMiddlewareCreator(std::shared_ptr<MiddlewarePlugin> plugin, const std::string& middlewareType);
        
        std::shared_ptr<Middleware> create(const MiddlewareInstanceConfig& config) override;
        std::string getMiddlewareName() const override;
        bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override;
        
    private:
        std::shared_ptr<MiddlewarePlugin> plugin_;
        std::string middlewareType_;
    };
    
    // Plugin management
    std::unordered_map<std::string, std::string> pluginCreators_;  ///< Maps creator name to plugin name
    std::unique_ptr<std::thread> hotReloadThread_;                 ///< Hot-reload monitoring thread
    std::atomic<bool> hotReloadEnabled_{false};                   ///< Whether hot-reload is enabled  
    std::atomic<int> hotReloadInterval_{30};                      ///< Hot-reload check interval
    std::atomic<bool> shutdownRequested_{false};                  ///< Shutdown flag for hot-reload thread
    
    /**
     * @brief Hot-reload monitoring thread function
     */
    void hotReloadLoop();
    
    /**
     * @brief Register creators from a plugin
     * 
     * @param plugin Plugin to register creators from
     * @param pluginName Name of the plugin
     */
    void registerPluginCreators(std::shared_ptr<MiddlewarePlugin> plugin, const std::string& pluginName);
};

} // namespace cppSwitchboard 