/**
 * @file test_plugin_system.cpp
 * @brief Unit tests for the plugin system
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-01-08
 * @version 1.2.0
 */

#include <gtest/gtest.h>
#include <cppSwitchboard/plugin_manager.h>
#include <cppSwitchboard/middleware_factory.h>
#include <cppSwitchboard/middleware_plugin.h>
#include <cppSwitchboard/middleware.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace cppSwitchboard;

class PluginSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test plugin directory with absolute path
        testPluginDir_ = std::filesystem::absolute("./test_plugins").string();
        std::filesystem::create_directories(testPluginDir_);
        
        // Verify directory was created
        ASSERT_TRUE(std::filesystem::exists(testPluginDir_));
        ASSERT_TRUE(std::filesystem::is_directory(testPluginDir_));
        
        // Get plugin manager instance
        pluginManager_ = &PluginManager::getInstance();
        factory_ = &MiddlewareFactory::getInstance();
    }
    
    void TearDown() override {
        // Cleanup: unload all plugins
        pluginManager_->unloadAllPlugins(true);
        
        // Remove the test directory from plugin manager
        pluginManager_->removePluginDirectory(testPluginDir_);
        
        // Remove test directory
        std::filesystem::remove_all(testPluginDir_);
    }
    
    std::string testPluginDir_;
    PluginManager* pluginManager_;
    MiddlewareFactory* factory_;
};

// Simple test middleware that avoids virtual function issues
class SimpleTestMiddleware : public Middleware {
public:
    explicit SimpleTestMiddleware(const std::string& name) : name_(name) {}
    
    virtual ~SimpleTestMiddleware() = default;
    
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        auto response = next(request, context);
        response.setHeader("X-Test-Middleware", name_);
        return response;
    }
    
    std::string getName() const override { 
        return name_;
    }
    
private:
    std::string name_;
};

// Mock plugin for testing
class MockPlugin : public MiddlewarePlugin {
public:
    MockPlugin(const std::string& name = "MockPlugin") : name_(name) {
        pluginInfo_.version = CPPSWITCH_PLUGIN_VERSION;
        pluginInfo_.name = name.c_str();
        pluginInfo_.description = "Mock plugin for testing";
        pluginInfo_.author = "Test Suite";
        pluginInfo_.plugin_version = {1, 0, 0};
        pluginInfo_.min_framework_version = {1, 2, 0};
        pluginInfo_.dependencies = nullptr;
        pluginInfo_.dependency_count = 0;
    }
    
    bool initialize(const PluginVersion& frameworkVersion) override {
        initialized_ = frameworkVersion.isCompatible(pluginInfo_.min_framework_version);
        return initialized_;
    }
    
    void shutdown() override {
        initialized_ = false;
    }
    
    std::shared_ptr<Middleware> createMiddleware(const MiddlewareInstanceConfig& config) override {
        if (!initialized_) return nullptr;
        return std::make_shared<SimpleTestMiddleware>(config.name);
    }
    
    bool validateConfig(const MiddlewareInstanceConfig& config, std::string& errorMessage) const override {
        if (config.name.empty()) {
            errorMessage = "Middleware name cannot be empty";
            return false;
        }
        return true;
    }
    
    std::vector<std::string> getSupportedTypes() const override {
        return {"mock", "test_middleware"};
    }
    
    const MiddlewarePluginInfo& getInfo() const override {
        return pluginInfo_;
    }
    
    bool isHealthy() const override {
        return initialized_;
    }
    
private:
    std::string name_;
    MiddlewarePluginInfo pluginInfo_;
    bool initialized_ = false;
};

// Test plugin discovery configuration
TEST_F(PluginSystemTest, DiscoveryConfiguration) {
    PluginDiscoveryConfig config;
    config.searchDirectories = {testPluginDir_, "/nonexistent"};
    config.fileExtensions = {".so", ".dll"};
    config.recursive = true;
    
    pluginManager_->setDiscoveryConfig(config);
    
    auto retrievedConfig = pluginManager_->getDiscoveryConfig();
    EXPECT_EQ(retrievedConfig.searchDirectories.size(), 2);
    EXPECT_EQ(retrievedConfig.fileExtensions.size(), 2);
    EXPECT_TRUE(retrievedConfig.recursive);
}

// Test adding and removing plugin directories
TEST_F(PluginSystemTest, PluginDirectoryManagement) {
    // Add valid directory
    EXPECT_TRUE(pluginManager_->addPluginDirectory(testPluginDir_));
    
    // Try to add same directory again
    EXPECT_FALSE(pluginManager_->addPluginDirectory(testPluginDir_));
    
    // Try to add non-existent directory
    EXPECT_FALSE(pluginManager_->addPluginDirectory("/nonexistent/path"));
    
    // Remove directory
    EXPECT_TRUE(pluginManager_->removePluginDirectory(testPluginDir_));
    
    // Try to remove non-existent directory
    EXPECT_FALSE(pluginManager_->removePluginDirectory("/nonexistent/path"));
}

// Test plugin loading result messages
TEST_F(PluginSystemTest, PluginLoadResultMessages) {
    EXPECT_STREQ(pluginLoadResultToString(PluginLoadResult::SUCCESS), "Success");
    EXPECT_STREQ(pluginLoadResultToString(PluginLoadResult::FILE_NOT_FOUND), "Plugin file not found");
    EXPECT_STREQ(pluginLoadResultToString(PluginLoadResult::VERSION_MISMATCH), "Plugin version incompatible with framework");
}

// Test plugin version compatibility
TEST_F(PluginSystemTest, PluginVersionCompatibility) {
    PluginVersion v1_2_0 = {1, 2, 0};
    PluginVersion v1_0_0 = {1, 0, 0};
    PluginVersion v2_0_0 = {2, 0, 0};
    PluginVersion v1_3_0 = {1, 3, 0};
    
    // Same major version, higher minor/patch should be compatible
    EXPECT_TRUE(v1_2_0.isCompatible(v1_0_0));
    EXPECT_TRUE(v1_3_0.isCompatible(v1_2_0));
    
    // Different major version should not be compatible
    EXPECT_FALSE(v2_0_0.isCompatible(v1_0_0));
    EXPECT_FALSE(v1_0_0.isCompatible(v2_0_0));
    
    // Lower version should not be compatible with higher requirement
    EXPECT_FALSE(v1_0_0.isCompatible(v1_2_0));
}

// Test plugin manager statistics
TEST_F(PluginSystemTest, PluginManagerStatistics) {
    auto stats = pluginManager_->getStatistics();
    
    // Should have basic statistics keys
    EXPECT_TRUE(stats.find("total_load_attempts") != stats.end());
    EXPECT_TRUE(stats.find("successful_loads") != stats.end());
    EXPECT_TRUE(stats.find("total_unloads") != stats.end());
    EXPECT_TRUE(stats.find("currently_loaded") != stats.end());
    
    // Initially should be zero
    EXPECT_EQ(stats["currently_loaded"], 0);
}

// Test plugin reference counting
TEST_F(PluginSystemTest, PluginReferenceCounting) {
    // Create a mock plugin and add it manually for testing
    auto mockPlugin = std::make_shared<MockPlugin>("TestPlugin");
    
    // These should return false for non-existent plugin
    EXPECT_FALSE(pluginManager_->incrementPluginRefCount("NonExistent"));
    EXPECT_FALSE(pluginManager_->decrementPluginRefCount("NonExistent"));
    EXPECT_EQ(pluginManager_->getPluginRefCount("NonExistent"), -1);
}

// Test plugin discovery in empty directory
TEST_F(PluginSystemTest, PluginDiscoveryEmpty) {
    pluginManager_->addPluginDirectory(testPluginDir_);
    
    auto discoveredPlugins = pluginManager_->discoverPlugins();
    EXPECT_TRUE(discoveredPlugins.empty());
    
    auto loadResults = pluginManager_->discoverAndLoadPlugins();
    EXPECT_TRUE(loadResults.empty());
}

// Test invalid plugin file loading
TEST_F(PluginSystemTest, InvalidPluginLoading) {
    // Create a dummy file that's not a valid plugin
    std::string dummyPlugin = testPluginDir_ + "/invalid.so";
    std::ofstream file(dummyPlugin);
    file << "This is not a valid plugin file";
    file.close();
    
    auto result = pluginManager_->loadPlugin(dummyPlugin);
    EXPECT_NE(result.first, PluginLoadResult::SUCCESS);
}

// Test factory integration with mock plugin
TEST_F(PluginSystemTest, FactoryIntegration) {
    // Test that factory can load plugins from directory
    auto loadedCount = factory_->loadPluginsFromDirectory(testPluginDir_);
    EXPECT_EQ(loadedCount, 0);  // No valid plugins in test directory
    
    // Test loading specific plugin file
    std::string dummyPlugin = testPluginDir_ + "/invalid.so";
    EXPECT_FALSE(factory_->loadPlugin(dummyPlugin));
    
    // Test getting loaded plugins
    auto loadedPlugins = factory_->getLoadedPlugins();
    EXPECT_TRUE(loadedPlugins.empty());
}

// Test hot-reload functionality
TEST_F(PluginSystemTest, HotReloadFunctionality) {
    // Enable hot-reload with short interval for testing
    factory_->setPluginHotReloadEnabled(true, 1);  // 1 second interval
    
    // Wait a bit and disable
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    factory_->setPluginHotReloadEnabled(false);
    
    // Should not crash or cause issues
    EXPECT_TRUE(true);
}

// Test plugin validation
TEST_F(PluginSystemTest, PluginValidation) {
    MockPlugin plugin;
    
    MiddlewareInstanceConfig config;
    config.name = "test_middleware";
    
    std::string errorMessage;
    EXPECT_TRUE(plugin.validateConfig(config, errorMessage));
    
    // Test with empty name
    config.name = "";
    EXPECT_FALSE(plugin.validateConfig(config, errorMessage));
    EXPECT_FALSE(errorMessage.empty());
}

// Test middleware creation through plugin
TEST_F(PluginSystemTest, MiddlewareCreation) {
    MockPlugin plugin;
    
    // Initialize plugin
    PluginVersion frameworkVersion = {1, 2, 0};
    EXPECT_TRUE(plugin.initialize(frameworkVersion));
    
    // Create middleware
    MiddlewareInstanceConfig config;
    config.name = "test_middleware";
    
    auto middleware = plugin.createMiddleware(config);
    EXPECT_NE(middleware, nullptr);
    
    if (middleware) {
        // Test getName() functionality
        std::string middlewareName = middleware->getName();
        EXPECT_EQ(middlewareName, "test_middleware");
        
        // Test middleware execution
        HttpRequest request("GET", "/test", "HTTP/1.1");
        Context context;
        
        auto nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
            (void)req; (void)ctx;
            HttpResponse response(200);
            response.setBody("Test response");
            return response;
        };
        
        auto response = middleware->handle(request, context, nextHandler);
        EXPECT_EQ(response.getStatus(), 200);
        EXPECT_EQ(response.getBody(), "Test response");
        EXPECT_EQ(response.getHeader("X-Test-Middleware"), "test_middleware");
    }
    
    // Test with uninitialized plugin
    plugin.shutdown();
    auto middlewareUninitialized = plugin.createMiddleware(config);
    EXPECT_EQ(middlewareUninitialized, nullptr);
}

// Test middleware execution
TEST_F(PluginSystemTest, MiddlewareExecution) {
    MockPlugin plugin;
    PluginVersion frameworkVersion = {1, 2, 0};
    plugin.initialize(frameworkVersion);
    
    MiddlewareInstanceConfig config;
    config.name = "test_middleware";
    
    auto middleware = plugin.createMiddleware(config);
    ASSERT_NE(middleware, nullptr);
    
    HttpRequest request("GET", "/test", "HTTP/1.1");
    Context context;
    
    auto nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        (void)req; (void)ctx;
        HttpResponse response(200);
        response.setBody("Test response");
        return response;
    };
    
    auto response = middleware->handle(request, context, nextHandler);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.getBody(), "Test response");
    EXPECT_EQ(response.getHeader("X-Test-Middleware"), "test_middleware");
}

// Performance test for plugin system overhead
TEST_F(PluginSystemTest, PerformanceOverhead) {
    MockPlugin plugin;
    PluginVersion frameworkVersion = {1, 2, 0};
    plugin.initialize(frameworkVersion);
    
    MiddlewareInstanceConfig config;
    config.name = "perf_test_middleware";
    
    auto middleware = plugin.createMiddleware(config);
    ASSERT_NE(middleware, nullptr);
    
    HttpRequest request("GET", "/perf", "HTTP/1.1");
    
    auto nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        (void)req; (void)ctx;
        HttpResponse response(200);
        response.setBody("Performance test");
        return response;
    };
    
    const int iterations = 1000;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        Context context;
        auto response = middleware->handle(request, context, nextHandler);
        EXPECT_EQ(response.getStatus(), 200);
        EXPECT_EQ(response.getHeader("X-Test-Middleware"), "perf_test_middleware");
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    EXPECT_LT(duration.count(), 100000);  // 100ms in microseconds
    
    std::cout << "Plugin system overhead: " << duration.count() / iterations 
              << " microseconds per request" << std::endl;
}

// Test plugin metadata
TEST_F(PluginSystemTest, PluginMetadata) {
    MockPlugin plugin("TestPlugin");
    
    auto& info = plugin.getInfo();
    EXPECT_EQ(std::string(info.name), "TestPlugin");
    EXPECT_EQ(std::string(info.description), "Mock plugin for testing");
    EXPECT_EQ(std::string(info.author), "Test Suite");
    EXPECT_EQ(info.plugin_version.major, 1);
    EXPECT_EQ(info.plugin_version.minor, 0);
    EXPECT_EQ(info.plugin_version.patch, 0);
    
    auto supportedTypes = plugin.getSupportedTypes();
    EXPECT_EQ(supportedTypes.size(), 2);
    EXPECT_TRUE(std::find(supportedTypes.begin(), supportedTypes.end(), "mock") != supportedTypes.end());
    EXPECT_TRUE(std::find(supportedTypes.begin(), supportedTypes.end(), "test_middleware") != supportedTypes.end());
}

// Test plugin health checking
TEST_F(PluginSystemTest, PluginHealthChecking) {
    MockPlugin plugin;
    
    // Plugin should be unhealthy when not initialized
    EXPECT_FALSE(plugin.isHealthy());
    
    // Initialize plugin
    PluginVersion frameworkVersion = {1, 2, 0};
    plugin.initialize(frameworkVersion);
    EXPECT_TRUE(plugin.isHealthy());
    
    // Shutdown plugin
    plugin.shutdown();
    EXPECT_FALSE(plugin.isHealthy());
}

// Test error handling in plugin system
TEST_F(PluginSystemTest, ErrorHandling) {
    // Test loading non-existent plugin
    auto result = pluginManager_->loadPlugin("/nonexistent/plugin.so");
    EXPECT_EQ(result.first, PluginLoadResult::FILE_NOT_FOUND);
    EXPECT_TRUE(result.second.empty());
    
    // Test unloading non-existent plugin
    EXPECT_FALSE(pluginManager_->unloadPlugin("NonExistentPlugin"));
    
    // Test force unloading non-existent plugin
    EXPECT_FALSE(pluginManager_->forceUnloadPlugin("NonExistentPlugin"));
    
    // Test getting non-existent plugin
    auto plugin = pluginManager_->getPlugin("NonExistentPlugin");
    EXPECT_EQ(plugin, nullptr);
    
    // Test getting info for non-existent plugin
    auto pluginInfo = pluginManager_->getPluginInfo("NonExistentPlugin");
    EXPECT_EQ(pluginInfo, nullptr);
    
    // Test checking if non-existent plugin is loaded
    EXPECT_FALSE(pluginManager_->isPluginLoaded("NonExistentPlugin"));
}

// Integration test with middleware factory
TEST_F(PluginSystemTest, FactoryIntegrationTest) {
    // Test that registered middleware names include built-in types
    auto registeredMiddleware = factory_->getRegisteredMiddleware();
    
    // Should have at least some built-in middleware
    EXPECT_FALSE(registeredMiddleware.empty());
    
    // Should include auth middleware
    EXPECT_TRUE(std::find(registeredMiddleware.begin(), registeredMiddleware.end(), "auth") != registeredMiddleware.end());
    
    // Test middleware registration status
    EXPECT_TRUE(factory_->isMiddlewareRegistered("auth"));
    EXPECT_FALSE(factory_->isMiddlewareRegistered("nonexistent_middleware"));
} 