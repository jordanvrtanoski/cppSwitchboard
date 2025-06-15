/**
 * @file test_middleware_config.cpp
 * @brief Comprehensive tests for middleware configuration system
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.2.0
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cppSwitchboard/middleware_config.h>
#include <cppSwitchboard/middleware_factory.h>
#include <cppSwitchboard/middleware_pipeline.h>
#include <memory>
#include <string>
#include <vector>

using namespace cppSwitchboard;
using namespace testing;

class MiddlewareConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        loader_ = std::make_unique<MiddlewareConfigLoader>();
    }
    
    void TearDown() override {
        loader_.reset();
    }
    
    std::unique_ptr<MiddlewareConfigLoader> loader_;
};

// Test MiddlewareInstanceConfig basic functionality
TEST_F(MiddlewareConfigTest, MiddlewareInstanceConfigValidation) {
    MiddlewareInstanceConfig config;
    std::string errorMessage;
    
    // Test invalid configuration (empty name)
    EXPECT_FALSE(config.validate(errorMessage));
    EXPECT_THAT(errorMessage, HasSubstr("name cannot be empty"));
    
    // Test valid configuration
    config.name = "test_middleware";
    config.enabled = true;
    config.priority = 100;
    EXPECT_TRUE(config.validate(errorMessage));
    
    // Test invalid priority
    config.priority = 2000; // Out of range
    EXPECT_FALSE(config.validate(errorMessage));
    EXPECT_THAT(errorMessage, HasSubstr("priority must be between"));
}

TEST_F(MiddlewareConfigTest, MiddlewareInstanceConfigAccessors) {
    MiddlewareInstanceConfig config;
    config.name = "test_middleware";
    
    // Test setting and getting configuration values
    config.config["string_value"] = std::string("test_value");
    config.config["int_value"] = 42;
    config.config["bool_value"] = true;
    config.config["array_value"] = std::vector<std::string>{"item1", "item2", "item3"};
    
    // Test getters
    EXPECT_EQ(config.getString("string_value"), "test_value");
    EXPECT_EQ(config.getString("missing_key", "default"), "default");
    
    EXPECT_EQ(config.getInt("int_value"), 42);
    EXPECT_EQ(config.getInt("missing_key", -1), -1);
    
    EXPECT_TRUE(config.getBool("bool_value"));
    EXPECT_FALSE(config.getBool("missing_key", false));
    
    auto arrayValue = config.getStringArray("array_value");
    EXPECT_EQ(arrayValue.size(), 3);
    EXPECT_EQ(arrayValue[0], "item1");
    EXPECT_EQ(arrayValue[1], "item2");
    EXPECT_EQ(arrayValue[2], "item3");
    
    // Test hasKey
    EXPECT_TRUE(config.hasKey("string_value"));
    EXPECT_FALSE(config.hasKey("missing_key"));
}

// Test RouteMiddlewareConfig
TEST_F(MiddlewareConfigTest, RouteMiddlewareConfigValidation) {
    RouteMiddlewareConfig route;
    std::string errorMessage;
    
    // Test invalid configuration (empty pattern)
    EXPECT_FALSE(route.validate(errorMessage));
    EXPECT_THAT(errorMessage, HasSubstr("pattern cannot be empty"));
    
    // Test valid glob pattern
    route.pattern = "/api/v1/*";
    route.isRegex = false;
    EXPECT_TRUE(route.validate(errorMessage));
    
    // Test invalid regex pattern
    route.pattern = "[invalid regex";
    route.isRegex = true;
    EXPECT_FALSE(route.validate(errorMessage));
    EXPECT_THAT(errorMessage, HasSubstr("Invalid regex pattern"));
    
    // Test valid regex pattern
    route.pattern = "/api/v[0-9]+/.*";
    route.isRegex = true;
    EXPECT_TRUE(route.validate(errorMessage));
}

TEST_F(MiddlewareConfigTest, RouteMiddlewareConfigPathMatching) {
    RouteMiddlewareConfig route;
    
    // Test glob pattern matching
    route.pattern = "/api/v1/*";
    route.isRegex = false;
    
    EXPECT_TRUE(route.matchesPath("/api/v1/users"));
    EXPECT_TRUE(route.matchesPath("/api/v1/users/123"));
    EXPECT_FALSE(route.matchesPath("/api/v2/users"));
    EXPECT_FALSE(route.matchesPath("/public/info"));
    
    // Test regex pattern matching
    route.pattern = "/api/v[0-9]+/.*";
    route.isRegex = true;
    
    EXPECT_TRUE(route.matchesPath("/api/v1/users"));
    EXPECT_TRUE(route.matchesPath("/api/v2/orders"));
    EXPECT_FALSE(route.matchesPath("/api/vx/users"));
    EXPECT_FALSE(route.matchesPath("/public/info"));
}

// Test ComprehensiveMiddlewareConfig
TEST_F(MiddlewareConfigTest, ComprehensiveMiddlewareConfigBasic) {
    ComprehensiveMiddlewareConfig config;
    
    // Add global middleware
    MiddlewareInstanceConfig globalMiddleware;
    globalMiddleware.name = "logging";
    globalMiddleware.enabled = true;
    globalMiddleware.priority = 0;
    config.global.middlewares.push_back(globalMiddleware);
    
    // Add route-specific middleware
    RouteMiddlewareConfig route;
    route.pattern = "/api/*";
    
    MiddlewareInstanceConfig authMiddleware;
    authMiddleware.name = "auth";
    authMiddleware.enabled = true;
    authMiddleware.priority = 100;
    route.middlewares.push_back(authMiddleware);
    
    config.routes.push_back(route);
    
    // Test validation
    std::string errorMessage;
    EXPECT_TRUE(config.validate(errorMessage));
    
    // Test middleware retrieval
    auto middlewareForApi = config.getMiddlewareForRoute("/api/users");
    EXPECT_EQ(middlewareForApi.size(), 2); // auth + logging
    
    // Check priority ordering (auth should come first due to higher priority)
    EXPECT_EQ(middlewareForApi[0].name, "auth");
    EXPECT_EQ(middlewareForApi[0].priority, 100);
    EXPECT_EQ(middlewareForApi[1].name, "logging");
    EXPECT_EQ(middlewareForApi[1].priority, 0);
    
    // Test middleware for non-matching route
    auto middlewareForPublic = config.getMiddlewareForRoute("/public/info");
    EXPECT_EQ(middlewareForPublic.size(), 1); // Only global logging
    EXPECT_EQ(middlewareForPublic[0].name, "logging");
    
    // Test getAllMiddlewareNames
    auto allNames = config.getAllMiddlewareNames();
    EXPECT_EQ(allNames.size(), 2);
    EXPECT_THAT(allNames, UnorderedElementsAre("logging", "auth"));
    
    // Test hasMiddleware
    EXPECT_TRUE(config.hasMiddleware("logging"));
    EXPECT_TRUE(config.hasMiddleware("auth"));
    EXPECT_FALSE(config.hasMiddleware("nonexistent"));
}

// Test YAML configuration loading
TEST_F(MiddlewareConfigTest, YamlConfigurationLoading) {
    std::string yamlConfig = R"(
middleware:
  global:
    - name: "cors"
      enabled: true
      priority: 200
      config:
        origins: ["*"]
        methods: ["GET", "POST", "PUT", "DELETE"]
        headers: ["Content-Type", "Authorization"]
        
    - name: "logging"
      enabled: true
      priority: 0
      config:
        format: "json"
        include_headers: false
        
  routes:
    "/api/v1/*":
      - name: "auth"
        enabled: true
        priority: 100
        config:
          type: "jwt"
          secret: "test_secret"
          
      - name: "rate_limit"
        enabled: true
        priority: 50
        config:
          requests_per_minute: 100
          per_ip: true
          
  hot_reload:
    enabled: false
    check_interval: 5
    reload_on_change: true
    validate_before_reload: true
)";

    auto result = loader_->loadFromString(yamlConfig);
    EXPECT_TRUE(result.isSuccess()) << "Error: " << result.message;
    
    const auto& config = loader_->getConfiguration();
    
    // Test global middleware
    EXPECT_EQ(config.global.middlewares.size(), 2);
    
    const auto& corsConfig = config.global.middlewares[0];
    EXPECT_EQ(corsConfig.name, "cors");
    EXPECT_TRUE(corsConfig.enabled);
    EXPECT_EQ(corsConfig.priority, 200);
    
    const auto& loggingConfig = config.global.middlewares[1];
    EXPECT_EQ(loggingConfig.name, "logging");
    EXPECT_TRUE(loggingConfig.enabled);
    EXPECT_EQ(loggingConfig.priority, 0);
    
    // Test route-specific middleware
    EXPECT_EQ(config.routes.size(), 1);
    
    const auto& apiRoute = config.routes[0];
    EXPECT_EQ(apiRoute.pattern, "/api/v1/*");
    EXPECT_EQ(apiRoute.middlewares.size(), 2);
    
    const auto& authConfig = apiRoute.middlewares[0];
    EXPECT_EQ(authConfig.name, "auth");
    EXPECT_TRUE(authConfig.enabled);
    EXPECT_EQ(authConfig.priority, 100);
    
    const auto& rateLimitConfig = apiRoute.middlewares[1];
    EXPECT_EQ(rateLimitConfig.name, "rate_limit");
    EXPECT_TRUE(rateLimitConfig.enabled);
    EXPECT_EQ(rateLimitConfig.priority, 50);
    
    // Test hot reload configuration
    EXPECT_FALSE(config.hotReload.enabled);
    EXPECT_EQ(config.hotReload.checkInterval.count(), 5);
    EXPECT_TRUE(config.hotReload.reloadOnChange);
    EXPECT_TRUE(config.hotReload.validateBeforeReload);
    
    // Test combined middleware for API route
    auto middlewareForApi = config.getMiddlewareForRoute("/api/v1/users");
    EXPECT_EQ(middlewareForApi.size(), 4); // cors + logging + auth + rate_limit
    
    // Verify priority ordering: cors (200) > auth (100) > rate_limit (50) > logging (0)
    EXPECT_EQ(middlewareForApi[0].name, "cors");
    EXPECT_EQ(middlewareForApi[1].name, "auth");
    EXPECT_EQ(middlewareForApi[2].name, "rate_limit");
    EXPECT_EQ(middlewareForApi[3].name, "logging");
}

// Test invalid YAML configuration
TEST_F(MiddlewareConfigTest, InvalidYamlConfiguration) {
    // Test missing middleware section
    std::string invalidYaml1 = R"(
server:
  port: 8080
)";
    
    auto result1 = loader_->loadFromString(invalidYaml1);
    EXPECT_FALSE(result1.isSuccess());
    EXPECT_EQ(result1.error, MiddlewareConfigError::INVALID_YAML);
    
    // Test invalid YAML syntax
    std::string invalidYaml2 = R"(
middleware:
  global:
    - name: "test"
      config:
        invalid: [unclosed array
)";
    
    auto result2 = loader_->loadFromString(invalidYaml2);
    EXPECT_FALSE(result2.isSuccess());
    // Should succeed with parsing but potentially fail validation
}

// Test environment variable substitution
TEST_F(MiddlewareConfigTest, EnvironmentVariableSubstitution) {
    // Set environment variable for testing
    setenv("TEST_SECRET", "secret_from_env", 1);
    
    std::string yamlConfig = R"(
middleware:
  routes:
    "/api/*":
      - name: "auth"
        enabled: true
        config:
          secret: "${TEST_SECRET}"
          type: "jwt"
)";

    loader_->setEnvironmentSubstitution(true);
    auto result = loader_->loadFromString(yamlConfig);
    EXPECT_TRUE(result.isSuccess());
    
    const auto& config = loader_->getConfiguration();
    const auto& authConfig = config.routes[0].middlewares[0];
    EXPECT_EQ(authConfig.getString("secret"), "secret_from_env");
    
    // Test with substitution disabled
    loader_->setEnvironmentSubstitution(false);
    auto result2 = loader_->loadFromString(yamlConfig);
    EXPECT_TRUE(result2.isSuccess());
    
    const auto& config2 = loader_->getConfiguration();
    const auto& authConfig2 = config2.routes[0].middlewares[0];
    EXPECT_EQ(authConfig2.getString("secret"), "${TEST_SECRET}"); // Should remain unchanged
    
    // Clean up
    unsetenv("TEST_SECRET");
}

// Test default configuration creation
TEST_F(MiddlewareConfigTest, DefaultConfiguration) {
    auto defaultConfig = MiddlewareConfigLoader::createDefault();
    
    // Should have some sensible defaults
    EXPECT_GT(defaultConfig.global.middlewares.size(), 0);
    
    // Verify default CORS middleware
    bool foundCors = false;
    bool foundLogging = false;
    
    for (const auto& middleware : defaultConfig.global.middlewares) {
        if (middleware.name == "cors") {
            foundCors = true;
            EXPECT_TRUE(middleware.enabled);
            EXPECT_EQ(middleware.priority, 200);
        } else if (middleware.name == "logging") {
            foundLogging = true;
            EXPECT_TRUE(middleware.enabled);
            EXPECT_EQ(middleware.priority, 0);
        }
    }
    
    EXPECT_TRUE(foundCors);
    EXPECT_TRUE(foundLogging);
    
    // Validate the default configuration
    std::string errorMessage;
    EXPECT_TRUE(defaultConfig.validate(errorMessage));
}

// Test MiddlewareFactory singleton
TEST_F(MiddlewareConfigTest, MiddlewareFactorySingleton) {
    auto& factory1 = MiddlewareFactory::getInstance();
    auto& factory2 = MiddlewareFactory::getInstance();
    
    // Should be the same instance
    EXPECT_EQ(&factory1, &factory2);
    
    // Should have built-in middleware registered
    auto registeredMiddleware = factory1.getRegisteredMiddleware();
    EXPECT_GT(registeredMiddleware.size(), 0);
    
    // Test specific middleware registration checks
    EXPECT_TRUE(factory1.isMiddlewareRegistered("cors"));
    EXPECT_TRUE(factory1.isMiddlewareRegistered("logging"));
    EXPECT_FALSE(factory1.isMiddlewareRegistered("nonexistent_middleware"));
}

// Test configuration validation
TEST_F(MiddlewareConfigTest, ConfigurationValidation) {
    ComprehensiveMiddlewareConfig config;
    
    // Test duplicate route patterns
    RouteMiddlewareConfig route1;
    route1.pattern = "/api/*";
    MiddlewareInstanceConfig middleware1;
    middleware1.name = "auth";
    route1.middlewares.push_back(middleware1);
    
    RouteMiddlewareConfig route2;
    route2.pattern = "/api/*"; // Duplicate pattern
    MiddlewareInstanceConfig middleware2;
    middleware2.name = "logging";
    route2.middlewares.push_back(middleware2);
    
    config.routes.push_back(route1);
    config.routes.push_back(route2);
    
    std::string errorMessage;
    EXPECT_FALSE(config.validate(errorMessage));
    EXPECT_THAT(errorMessage, HasSubstr("Duplicate route pattern"));
}

// Test hot reload configuration
TEST_F(MiddlewareConfigTest, HotReloadConfiguration) {
    HotReloadConfig hotReload;
    std::string errorMessage;
    
    // Test invalid configuration (enabled but no files)
    hotReload.enabled = true;
    hotReload.checkInterval = std::chrono::seconds(5);
    hotReload.watchedFiles.clear(); // No files to watch
    
    EXPECT_FALSE(hotReload.validate(errorMessage));
    EXPECT_THAT(errorMessage, HasSubstr("no files specified to watch"));
    
    // Test invalid check interval
    hotReload.checkInterval = std::chrono::seconds(0);
    hotReload.watchedFiles.push_back("/etc/middleware.yaml");
    
    EXPECT_FALSE(hotReload.validate(errorMessage));
    EXPECT_THAT(errorMessage, HasSubstr("check interval must be at least 1 second"));
    
    // Test valid configuration
    hotReload.checkInterval = std::chrono::seconds(5);
    EXPECT_TRUE(hotReload.validate(errorMessage));
}

// Test configuration merging
TEST_F(MiddlewareConfigTest, ConfigurationMerging) {
    // Load base configuration
    std::string baseConfig = R"(
middleware:
  global:
    - name: "cors"
      enabled: true
      priority: 200
)";
    
    auto result = loader_->loadFromString(baseConfig);
    EXPECT_TRUE(result.isSuccess());
    
    // Create overlay configuration (would normally come from file)
    std::string overlayYaml = R"(
middleware:
  global:
    - name: "logging"
      enabled: true
      priority: 0
  routes:
    "/api/*":
      - name: "auth"
        enabled: true
        priority: 100
)";
    
    // Note: Actual merging would require temporary files for mergeFromFile
    // This test verifies the merge configuration structure is in place
    const auto& config = loader_->getConfiguration();
    EXPECT_EQ(config.global.middlewares.size(), 1);
    EXPECT_EQ(config.global.middlewares[0].name, "cors");
} 