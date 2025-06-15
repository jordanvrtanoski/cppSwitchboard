/**
 * @file plugin_manager.cpp
 * @brief Implementation of the plugin manager for dynamic middleware loading
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-01-08
 * @version 1.2.0
 */

#include <cppSwitchboard/plugin_manager.h>
#include <cppSwitchboard/debug_logger.h>

#include <algorithm>
#include <thread>
#include <chrono>
#include <sstream>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <libloaderapi.h>
#else
    #include <dlfcn.h>
#endif

namespace cppSwitchboard {

PluginManager& PluginManager::getInstance() {
    static PluginManager instance;
    return instance;
}

PluginManager::~PluginManager() {
    shutdownRequested_ = true;
    if (healthCheckThread_ && healthCheckThread_->joinable()) {
        healthCheckThread_->join();
    }
    unloadAllPlugins(true); // Force unload all plugins
}

void PluginManager::setDiscoveryConfig(const PluginDiscoveryConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    discoveryConfig_ = config;
    
    // Set default file extensions if none provided
    if (discoveryConfig_.fileExtensions.empty()) {
        discoveryConfig_.fileExtensions = {getLibraryExtension()};
    }
}

const PluginDiscoveryConfig& PluginManager::getDiscoveryConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return discoveryConfig_;
}

bool PluginManager::addPluginDirectory(const std::string& directory) {
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto& dirs = discoveryConfig_.searchDirectories;
    if (std::find(dirs.begin(), dirs.end(), directory) == dirs.end()) {
        dirs.push_back(directory);
        return true;
    }
    return false; // Already exists
}

bool PluginManager::removePluginDirectory(const std::string& directory) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& dirs = discoveryConfig_.searchDirectories;
    auto it = std::find(dirs.begin(), dirs.end(), directory);
    if (it != dirs.end()) {
        dirs.erase(it);
        return true;
    }
    return false;
}

std::pair<PluginLoadResult, std::string> PluginManager::loadPlugin(const std::string& filePath, bool hotReload) {
    totalLoadAttempts_++;
    
    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
        fireEvent("error", "", "Plugin file not found: " + filePath);
        return {PluginLoadResult::FILE_NOT_FOUND, ""};
    }
    
    // Check if it's a valid plugin file
    if (!isValidPluginFile(filePath)) {
        fireEvent("error", "", "Invalid plugin file format: " + filePath);
        return {PluginLoadResult::INVALID_FORMAT, ""};
    }
    
    void* handle = nullptr;
    
#ifdef _WIN32
    handle = LoadLibraryA(filePath.c_str());
    if (!handle) {
        DWORD error = GetLastError();
        std::stringstream ss;
        ss << "Failed to load library " << filePath << " (error: " << error << ")";
        fireEvent("error", "", ss.str());
        return {PluginLoadResult::INVALID_FORMAT, ""};
    }
#else
    handle = dlopen(filePath.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        std::string error = dlerror();
        fireEvent("error", "", "Failed to load library " + filePath + ": " + error);
        return {PluginLoadResult::INVALID_FORMAT, ""};
    }
#endif
    
    auto result = loadPluginFromHandle(handle, filePath, hotReload);
    if (result.first != PluginLoadResult::SUCCESS) {
        // Cleanup handle on failure
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return {result.first, ""};
    }
    
    successfulLoads_++;
    fireEvent("loaded", result.second->name, "Plugin loaded from: " + filePath);
    return {PluginLoadResult::SUCCESS, result.second->name};
}

std::pair<PluginLoadResult, std::shared_ptr<LoadedPluginInfo>> PluginManager::loadPluginFromHandle(
    void* handle, const std::string& filePath, bool hotReload) {
    
    // Get plugin info
#ifdef _WIN32
    auto infoSymbol = GetProcAddress(static_cast<HMODULE>(handle), "cppSwitchboard_plugin_info");
#else
    auto infoSymbol = dlsym(handle, "cppSwitchboard_plugin_info");
#endif
    
    if (!infoSymbol) {
        return {PluginLoadResult::MISSING_EXPORTS, nullptr};
    }
    
    auto pluginInfo = static_cast<MiddlewarePluginInfo*>(infoSymbol);
    
    // Validate plugin version
    if (!validatePluginVersion(*pluginInfo)) {
        return {PluginLoadResult::VERSION_MISMATCH, nullptr};
    }
    
    // Check if plugin with same name is already loaded
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (loadedPlugins_.find(pluginInfo->name) != loadedPlugins_.end()) {
            return {PluginLoadResult::ALREADY_LOADED, nullptr};
        }
    }
    
    // Get plugin factory function
#ifdef _WIN32
    auto createFunc = reinterpret_cast<cppSwitchboard_create_plugin_t>(
        GetProcAddress(static_cast<HMODULE>(handle), "cppSwitchboard_create_plugin"));
    auto destroyFunc = reinterpret_cast<cppSwitchboard_destroy_plugin_t>(
        GetProcAddress(static_cast<HMODULE>(handle), "cppSwitchboard_destroy_plugin"));
#else
    auto createFunc = reinterpret_cast<cppSwitchboard_create_plugin_t>(
        dlsym(handle, "cppSwitchboard_create_plugin"));
    auto destroyFunc = reinterpret_cast<cppSwitchboard_destroy_plugin_t>(
        dlsym(handle, "cppSwitchboard_destroy_plugin"));
#endif
    
    if (!createFunc || !destroyFunc) {
        return {PluginLoadResult::MISSING_EXPORTS, nullptr};
    }
    
    // Create plugin instance
    std::unique_ptr<MiddlewarePlugin, cppSwitchboard_destroy_plugin_t> plugin(createFunc(), destroyFunc);
    if (!plugin) {
        return {PluginLoadResult::INITIALIZATION_FAILED, nullptr};
    }
    
    // Initialize plugin
    if (!plugin->initialize(FRAMEWORK_VERSION)) {
        return {PluginLoadResult::INITIALIZATION_FAILED, nullptr};
    }
    
    // Create loaded plugin info
    auto loadedInfo = std::make_shared<LoadedPluginInfo>();
    loadedInfo->filePath = filePath;
    loadedInfo->name = pluginInfo->name;
    loadedInfo->version = pluginInfo->plugin_version;
    loadedInfo->plugin = std::shared_ptr<MiddlewarePlugin>(plugin.release(), destroyFunc);
    loadedInfo->handle = handle;
    loadedInfo->loadTime = std::chrono::steady_clock::now();
    loadedInfo->hotReloadEnabled = hotReload;
    
    if (hotReload) {
        loadedInfo->lastModified = std::filesystem::last_write_time(filePath);
    }
    
    // Check dependencies
    std::vector<std::string> missingDeps;
    for (size_t i = 0; i < pluginInfo->dependency_count; ++i) {
        const auto& dep = pluginInfo->dependencies[i];
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = loadedPlugins_.find(dep.name);
        if (it == loadedPlugins_.end()) {
            if (!dep.optional) {
                missingDeps.push_back(dep.name);
            }
        } else {
            // Check version compatibility
            if (!it->second->version.isCompatible(dep.min_version)) {
                missingDeps.push_back(std::string(dep.name) + " (version mismatch)");
            }
        }
    }
    
    if (!missingDeps.empty()) {
        loadedInfo->plugin->shutdown();
        return {PluginLoadResult::DEPENDENCY_MISSING, nullptr};
    }
    
    // Store in loaded plugins map
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loadedPlugins_[pluginInfo->name] = loadedInfo;
    }
    
    return {PluginLoadResult::SUCCESS, loadedInfo};
}

bool PluginManager::validatePluginVersion(const MiddlewarePluginInfo& pluginInfo) const {
    // Check ABI version
    if (pluginInfo.version != CPPSWITCH_PLUGIN_VERSION) {
        return false;
    }
    
    // Check framework version compatibility
    return FRAMEWORK_VERSION.isCompatible(pluginInfo.min_framework_version);
}

bool PluginManager::unloadPlugin(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedPlugins_.find(pluginName);
    if (it == loadedPlugins_.end()) {
        return false;
    }
    
    auto& pluginInfo = it->second;
    
    // Check reference count
    if (pluginInfo->refCount.load() > 0) {
        return false; // Plugin is still in use
    }
    
    // Check if other plugins depend on this one
    auto dependents = getDependentPlugins(pluginName);
    if (!dependents.empty()) {
        return false; // Other plugins depend on this one
    }
    
    // Shutdown plugin
    pluginInfo->plugin->shutdown();
    
    // Close library handle
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(pluginInfo->handle));
#else
    dlclose(pluginInfo->handle);
#endif
    
    loadedPlugins_.erase(it);
    totalUnloads_++;
    
    fireEvent("unloaded", pluginName, "Plugin unloaded successfully");
    return true;
}

bool PluginManager::forceUnloadPlugin(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedPlugins_.find(pluginName);
    if (it == loadedPlugins_.end()) {
        return false;
    }
    
    auto& pluginInfo = it->second;
    
    // Shutdown plugin
    pluginInfo->plugin->shutdown();
    
    // Close library handle
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(pluginInfo->handle));
#else
    dlclose(pluginInfo->handle);
#endif
    
    loadedPlugins_.erase(it);
    totalUnloads_++;
    
    fireEvent("unloaded", pluginName, "Plugin force unloaded (ref count was " + 
              std::to_string(pluginInfo->refCount.load()) + ")");
    return true;
}

std::vector<std::string> PluginManager::discoverPlugins() const {
    std::vector<std::string> discoveredPlugins;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& directory : discoveryConfig_.searchDirectories) {
        if (!std::filesystem::exists(directory)) {
            continue;
        }
        
        try {
            if (discoveryConfig_.recursive) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(
                    directory, 
                    discoveryConfig_.followSymlinks ? 
                        std::filesystem::directory_options::follow_directory_symlink : 
                        std::filesystem::directory_options::none)) {
                    
                    if (entry.is_regular_file()) {
                        auto path = entry.path().string();
                        if (isValidPluginFile(path)) {
                            discoveredPlugins.push_back(path);
                        }
                    }
                }
            } else {
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_regular_file()) {
                        auto path = entry.path().string();
                        if (isValidPluginFile(path)) {
                            discoveredPlugins.push_back(path);
                        }
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // Log error but continue with other directories
        }
    }
    
    return discoveredPlugins;
}

std::unordered_map<std::string, std::pair<PluginLoadResult, std::string>> PluginManager::discoverAndLoadPlugins() {
    auto discoveredPlugins = discoverPlugins();
    std::unordered_map<std::string, std::pair<PluginLoadResult, std::string>> results;
    
    for (const auto& pluginPath : discoveredPlugins) {
        auto result = loadPlugin(pluginPath);
        results[pluginPath] = result;
    }
    
    return results;
}

std::shared_ptr<MiddlewarePlugin> PluginManager::getPlugin(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedPlugins_.find(pluginName);
    if (it != loadedPlugins_.end()) {
        return it->second->plugin;
    }
    return nullptr;
}

std::shared_ptr<LoadedPluginInfo> PluginManager::getPluginInfo(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedPlugins_.find(pluginName);
    if (it != loadedPlugins_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> PluginManager::getLoadedPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> pluginNames;
    pluginNames.reserve(loadedPlugins_.size());
    
    for (const auto& [name, info] : loadedPlugins_) {
        pluginNames.push_back(name);
    }
    
    return pluginNames;
}

bool PluginManager::isPluginLoaded(const std::string& pluginName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loadedPlugins_.find(pluginName) != loadedPlugins_.end();
}

bool PluginManager::incrementPluginRefCount(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedPlugins_.find(pluginName);
    if (it != loadedPlugins_.end()) {
        it->second->refCount++;
        return true;
    }
    return false;
}

bool PluginManager::decrementPluginRefCount(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedPlugins_.find(pluginName);
    if (it != loadedPlugins_.end()) {
        if (it->second->refCount.load() > 0) {
            it->second->refCount--;
        }
        return true;
    }
    return false;
}

int PluginManager::getPluginRefCount(const std::string& pluginName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedPlugins_.find(pluginName);
    if (it != loadedPlugins_.end()) {
        return it->second->refCount.load();
    }
    return -1;
}

std::vector<std::string> PluginManager::checkAndReloadPlugins() {
    std::vector<std::string> reloadedPlugins;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [name, pluginInfo] : loadedPlugins_) {
        if (!pluginInfo->hotReloadEnabled) {
            continue;
        }
        
        try {
            auto currentModTime = std::filesystem::last_write_time(pluginInfo->filePath);
            if (currentModTime > pluginInfo->lastModified) {
                // File has been modified, attempt reload
                if (pluginInfo->refCount.load() == 0) {
                    // Safe to reload
                    std::string filePath = pluginInfo->filePath;
                    
                    // Shutdown current plugin
                    pluginInfo->plugin->shutdown();
                    
                    // Close handle
#ifdef _WIN32
                    FreeLibrary(static_cast<HMODULE>(pluginInfo->handle));
#else
                    dlclose(pluginInfo->handle);
#endif
                    
                    // Remove from map temporarily
                    loadedPlugins_.erase(name);
                    
                    // Reload plugin
                    mutex_.unlock(); // Unlock to avoid deadlock in loadPlugin
                    auto result = loadPlugin(filePath, true);
                    mutex_.lock();
                    
                    if (result.first == PluginLoadResult::SUCCESS) {
                        reloadedPlugins.push_back(name);
                        hotReloads_++;
                        fireEvent("hot_reload", name, "Plugin hot-reloaded successfully");
                    } else {
                        fireEvent("error", name, "Failed to hot-reload plugin: " + 
                                 std::string(pluginLoadResultToString(result.first)));
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // File may have been deleted or is inaccessible
            fireEvent("error", name, "Failed to check file modification time: " + std::string(e.what()));
        }
    }
    
    return reloadedPlugins;
}

bool PluginManager::validatePluginDependencies(const std::string& pluginName, std::vector<std::string>& missingDeps) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedPlugins_.find(pluginName);
    if (it == loadedPlugins_.end()) {
        return false;
    }
    
    auto& pluginInfo = it->second->plugin->getInfo();
    missingDeps.clear();
    
    for (size_t i = 0; i < pluginInfo.dependency_count; ++i) {
        const auto& dep = pluginInfo.dependencies[i];
        
        auto depIt = loadedPlugins_.find(dep.name);
        if (depIt == loadedPlugins_.end()) {
            if (!dep.optional) {
                missingDeps.push_back(dep.name);
            }
        } else {
            // Check version compatibility
            if (!depIt->second->version.isCompatible(dep.min_version)) {
                missingDeps.push_back(std::string(dep.name) + " (version mismatch)");
            }
        }
    }
    
    return missingDeps.empty();
}

std::vector<std::string> PluginManager::getDependentPlugins(const std::string& pluginName) const {
    std::vector<std::string> dependents;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [name, pluginInfo] : loadedPlugins_) {
        if (name == pluginName) continue;
        
        auto& info = pluginInfo->plugin->getInfo();
        for (size_t i = 0; i < info.dependency_count; ++i) {
            if (std::string(info.dependencies[i].name) == pluginName) {
                dependents.push_back(name);
                break;
            }
        }
    }
    
    return dependents;
}

size_t PluginManager::unloadAllPlugins(bool force) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t unloadedCount = 0;
    
    // Create a list of plugins to unload in dependency order
    std::vector<std::string> unloadOrder;
    std::unordered_set<std::string> processed;
    
    // First, collect all plugin names
    std::vector<std::string> allPlugins;
    for (const auto& [name, info] : loadedPlugins_) {
        allPlugins.push_back(name);
    }
    
    // Build unload order (dependents first)
    std::function<void(const std::string&)> addToUnloadOrder = [&](const std::string& pluginName) {
        if (processed.find(pluginName) != processed.end()) {
            return; // Already processed
        }
        
        // First add all dependents
        for (const auto& dependent : getDependentPlugins(pluginName)) {
            addToUnloadOrder(dependent);
        }
        
        unloadOrder.push_back(pluginName);
        processed.insert(pluginName);
    };
    
    for (const auto& pluginName : allPlugins) {
        addToUnloadOrder(pluginName);
    }
    
    // Unload plugins in order
    for (const auto& pluginName : unloadOrder) {
        auto it = loadedPlugins_.find(pluginName);
        if (it == loadedPlugins_.end()) {
            continue; // Already unloaded
        }
        
        auto& pluginInfo = it->second;
        
        if (!force && pluginInfo->refCount.load() > 0) {
            continue; // Skip plugins still in use
        }
        
        // Shutdown plugin
        pluginInfo->plugin->shutdown();
        
        // Close library handle
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(pluginInfo->handle));
#else
        dlclose(pluginInfo->handle);
#endif
        
        loadedPlugins_.erase(it);
        unloadedCount++;
        totalUnloads_++;
        
        fireEvent("unloaded", pluginName, force ? "Plugin force unloaded during shutdown" : 
                                                 "Plugin unloaded during shutdown");
    }
    
    return unloadedCount;
}

void PluginManager::setEventCallback(PluginEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = std::move(callback);
}

std::unordered_map<std::string, size_t> PluginManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return {
        {"total_load_attempts", totalLoadAttempts_.load()},
        {"successful_loads", successfulLoads_.load()},
        {"total_unloads", totalUnloads_.load()},
        {"hot_reloads", hotReloads_.load()},
        {"currently_loaded", loadedPlugins_.size()}
    };
}

void PluginManager::setHealthCheckEnabled(bool enabled, int intervalSeconds) {
    healthCheckEnabled_ = enabled;
    healthCheckInterval_ = intervalSeconds;
    
    if (enabled) {
        if (!healthCheckThread_ || !healthCheckThread_->joinable()) {
            shutdownRequested_ = false;
            healthCheckThread_ = std::make_unique<std::thread>(&PluginManager::healthCheckLoop, this);
        }
    } else {
        if (healthCheckThread_ && healthCheckThread_->joinable()) {
            shutdownRequested_ = true;
            healthCheckThread_->join();
            healthCheckThread_.reset();
        }
    }
}

bool PluginManager::isValidPluginFile(const std::string& filePath) const {
    auto extension = std::filesystem::path(filePath).extension().string();
    
    // Check if extension matches configured extensions
    for (const auto& validExt : discoveryConfig_.fileExtensions) {
        if (extension == validExt) {
            return true;
        }
    }
    
    // If no extensions configured, check against platform default
    if (discoveryConfig_.fileExtensions.empty()) {
        return extension == getLibraryExtension();
    }
    
    return false;
}

std::string PluginManager::getLibraryExtension() {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

void PluginManager::fireEvent(const std::string& eventType, const std::string& pluginName, const std::string& message) {
    if (eventCallback_) {
        try {
            eventCallback_(eventType, pluginName, message);
        } catch (...) {
            // Ignore callback exceptions to prevent plugin system instability
        }
    }
}

void PluginManager::healthCheckLoop() {
    while (!shutdownRequested_) {
        // Wait for the specified interval
        auto waitTime = std::chrono::seconds(healthCheckInterval_.load());
        auto startTime = std::chrono::steady_clock::now();
        
        while (!shutdownRequested_ && 
               (std::chrono::steady_clock::now() - startTime) < waitTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (shutdownRequested_) {
            break;
        }
        
        // Check health of all loaded plugins
        std::vector<std::string> unhealthyPlugins;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [name, pluginInfo] : loadedPlugins_) {
                try {
                    if (!pluginInfo->plugin->isHealthy()) {
                        unhealthyPlugins.push_back(name);
                    }
                } catch (...) {
                    // Plugin threw exception during health check
                    unhealthyPlugins.push_back(name);
                }
            }
        }
        
        // Unload unhealthy plugins
        for (const auto& pluginName : unhealthyPlugins) {
            fireEvent("error", pluginName, "Plugin failed health check, unloading");
            forceUnloadPlugin(pluginName);
        }
    }
}

} // namespace cppSwitchboard 