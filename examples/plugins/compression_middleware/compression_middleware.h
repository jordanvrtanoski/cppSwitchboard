/**
 * @file compression_middleware.h
 * @brief Example compression middleware plugin implementation
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-01-08
 * @version 1.0.0
 * 
 * This file demonstrates how to create a plugin middleware that provides
 * HTTP response compression (gzip/deflate). This is a complete working
 * example of the plugin system.
 */
#pragma once

#include <cppSwitchboard/middleware_plugin.h>
#include <cppSwitchboard/middleware.h>
#include <string>
#include <vector>
#include <unordered_set>

namespace ExamplePlugins {

/**
 * @brief Compression middleware that gzip/deflate compresses HTTP responses
 * 
 * This middleware compresses HTTP response bodies using gzip or deflate
 * compression when the client supports it and the response meets compression
 * criteria (size threshold, content type, etc.).
 * 
 * Configuration options:
 * - enabled: Whether compression is enabled (default: true)
 * - min_size: Minimum response size to compress (default: 1024 bytes)
 * - compression_level: Compression level 1-9 (default: 6)
 * - compression_types: List of content types to compress (default: text/*, application/json, etc.)
 * - excluded_paths: List of paths to exclude from compression
 */
class CompressionMiddleware : public cppSwitchboard::Middleware {
public:
    /**
     * @brief Compression configuration
     */
    struct Config {
        bool enabled = true;
        size_t minSize = 1024;
        int compressionLevel = 6;  // 1-9, 6 is default
        std::unordered_set<std::string> compressibleTypes = {
            "text/html", "text/css", "text/javascript", "text/plain",
            "application/json", "application/xml", "application/javascript"
        };
        std::unordered_set<std::string> excludedPaths;
    };

    /**
     * @brief Constructor with configuration
     * 
     * @param config Compression configuration
     */
    explicit CompressionMiddleware(const Config& config = {});

    /**
     * @brief Process HTTP request and compress response if appropriate
     * 
     * @param request HTTP request
     * @param context Middleware context
     * @param next Next handler in pipeline
     * @return HttpResponse Potentially compressed response
     */
    cppSwitchboard::HttpResponse handle(
        const cppSwitchboard::HttpRequest& request,
        cppSwitchboard::Context& context,
        cppSwitchboard::NextHandler next) override;

    /**
     * @brief Get middleware name
     * 
     * @return std::string Middleware name
     */
    std::string getName() const override;

    /**
     * @brief Get middleware priority (lower number = higher priority)
     * 
     * @return int Priority value (-10 for response processing)
     */
    int getPriority() const override;

private:
    Config config_;

    /**
     * @brief Check if client accepts compression
     * 
     * @param request HTTP request
     * @return std::string Accepted compression type ("gzip", "deflate", or empty)
     */
    std::string getAcceptedCompression(const cppSwitchboard::HttpRequest& request) const;

    /**
     * @brief Check if response should be compressed
     * 
     * @param response HTTP response
     * @param request HTTP request  
     * @return bool True if response should be compressed
     */
    bool shouldCompress(const cppSwitchboard::HttpResponse& response, 
                       const cppSwitchboard::HttpRequest& request) const;

    /**
     * @brief Compress response body using gzip
     * 
     * @param data Data to compress
     * @return std::string Compressed data
     * @throws std::runtime_error If compression fails
     */
    std::string compressGzip(const std::string& data) const;

    /**
     * @brief Compress response body using deflate
     * 
     * @param data Data to compress
     * @return std::string Compressed data
     * @throws std::runtime_error If compression fails
     */
    std::string compressDeflate(const std::string& data) const;
};

/**
 * @brief Plugin implementation for compression middleware
 * 
 * This class implements the MiddlewarePlugin interface to provide
 * the compression middleware through the plugin system.
 */
class CompressionMiddlewarePlugin : public cppSwitchboard::MiddlewarePlugin {
public:
    /**
     * @brief Initialize the plugin
     * 
     * @param frameworkVersion Framework version
     * @return bool True if initialization succeeded
     */
    bool initialize(const PluginVersion& frameworkVersion) override;

    /**
     * @brief Shutdown the plugin
     */
    void shutdown() override;

    /**
     * @brief Create middleware instance
     * 
     * @param config Middleware configuration
     * @return std::shared_ptr<cppSwitchboard::Middleware> Created middleware
     */
    std::shared_ptr<cppSwitchboard::Middleware> createMiddleware(
        const cppSwitchboard::MiddlewareInstanceConfig& config) override;

    /**
     * @brief Validate middleware configuration
     * 
     * @param config Configuration to validate
     * @param errorMessage Error message output
     * @return bool True if configuration is valid
     */
    bool validateConfig(const cppSwitchboard::MiddlewareInstanceConfig& config, 
                       std::string& errorMessage) const override;

    /**
     * @brief Get supported middleware types
     * 
     * @return std::vector<std::string> List of supported types
     */
    std::vector<std::string> getSupportedTypes() const override;

    /**
     * @brief Get plugin metadata
     * 
     * @return const MiddlewarePluginInfo& Plugin information
     */
    const MiddlewarePluginInfo& getInfo() const override;

    /**
     * @brief Get configuration schema
     * 
     * @return std::string JSON schema for configuration
     */
    std::string getConfigSchema() const override;

private:
    static MiddlewarePluginInfo pluginInfo_;
    bool initialized_ = false;
};

} // namespace ExamplePlugins 