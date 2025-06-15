/**
 * @file compression_middleware.cpp
 * @brief Implementation of the example compression middleware plugin
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-01-08
 * @version 1.0.0
 */

#include "compression_middleware.h"
#include <zlib.h>
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace ExamplePlugins {

// Plugin metadata
MiddlewarePluginInfo CompressionMiddlewarePlugin::pluginInfo_ = {
    .version = CPPSWITCH_PLUGIN_VERSION,
    .name = "CompressionMiddleware",
    .description = "HTTP response compression middleware (gzip/deflate)",
    .author = "Jordan Vrtanoski",
    .plugin_version = {1, 0, 0},
    .min_framework_version = {1, 2, 0},
    .dependencies = nullptr,
    .dependency_count = 0
};

// CompressionMiddleware implementation

CompressionMiddleware::CompressionMiddleware(const Config& config) : config_(config) {}

cppSwitchboard::HttpResponse CompressionMiddleware::handle(
    const cppSwitchboard::HttpRequest& request,
    cppSwitchboard::Context& context,
    cppSwitchboard::NextHandler next) {
    
    // Call next middleware/handler first
    auto response = next(request, context);
    
    // Only compress if enabled and conditions are met
    if (!config_.enabled || !shouldCompress(response, request)) {
        return response;
    }
    
    // Get accepted compression format
    std::string compressionType = getAcceptedCompression(request);
    if (compressionType.empty()) {
        return response; // Client doesn't support compression
    }
    
    try {
        std::string originalBody = response.getBody();
        std::string compressedBody;
        
        if (compressionType == "gzip") {
            compressedBody = compressGzip(originalBody);
        } else if (compressionType == "deflate") {
            compressedBody = compressDeflate(originalBody);
        } else {
            return response; // Unsupported compression type
        }
        
        // Create new response with compressed body
        auto compressedResponse = response;
        compressedResponse.setBody(compressedBody);
        compressedResponse.setHeader("Content-Encoding", compressionType);
        compressedResponse.setHeader("Content-Length", std::to_string(compressedBody.size()));
        compressedResponse.setHeader("Vary", "Accept-Encoding");
        
        // Add compression info to context
        cppSwitchboard::ContextHelper(context).setString("compression_type", compressionType);
        cppSwitchboard::ContextHelper(context).setString("original_size", std::to_string(originalBody.size()));
        cppSwitchboard::ContextHelper(context).setString("compressed_size", std::to_string(compressedBody.size()));
        
        return compressedResponse;
        
    } catch (const std::exception& e) {
        // Compression failed, return original response
        // In production, you might want to log this error
        return response;
    }
}

std::string CompressionMiddleware::getName() const {
    return "CompressionMiddleware";
}

int CompressionMiddleware::getPriority() const {
    return -10; // Low priority for response processing
}

std::string CompressionMiddleware::getAcceptedCompression(const cppSwitchboard::HttpRequest& request) const {
    std::string acceptEncoding = request.getHeader("Accept-Encoding");
    if (acceptEncoding.empty()) {
        return "";
    }
    
    // Convert to lowercase for case-insensitive comparison
    std::transform(acceptEncoding.begin(), acceptEncoding.end(), acceptEncoding.begin(), ::tolower);
    
    // Check for gzip first (preferred)
    if (acceptEncoding.find("gzip") != std::string::npos) {
        return "gzip";
    }
    
    // Check for deflate
    if (acceptEncoding.find("deflate") != std::string::npos) {
        return "deflate";
    }
    
    return "";
}

bool CompressionMiddleware::shouldCompress(const cppSwitchboard::HttpResponse& response, 
                                          const cppSwitchboard::HttpRequest& request) const {
    
    // Check if path is excluded
    std::string path = request.getPath();
    if (config_.excludedPaths.find(path) != config_.excludedPaths.end()) {
        return false;
    }
    
    // Check response size
    std::string body = response.getBody();
    if (body.size() < config_.minSize) {
        return false;
    }
    
    // Check if already compressed
    if (!response.getHeader("Content-Encoding").empty()) {
        return false;
    }
    
    // Check content type
    std::string contentType = response.getHeader("Content-Type");
    if (contentType.empty()) {
        return false;
    }
    
    // Extract main content type (before semicolon)
    size_t semicolonPos = contentType.find(';');
    if (semicolonPos != std::string::npos) {
        contentType = contentType.substr(0, semicolonPos);
    }
    
    // Check if content type is compressible
    return config_.compressibleTypes.find(contentType) != config_.compressibleTypes.end();
}

std::string CompressionMiddleware::compressGzip(const std::string& data) const {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    
    // Initialize for gzip compression (windowBits = 15 + 16 for gzip)
    if (deflateInit2(&stream, config_.compressionLevel, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("Failed to initialize gzip compression");
    }
    
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in = reinterpret_cast<const Bytef*>(data.c_str());
    
    std::string compressed;
    compressed.reserve(data.size() / 2); // Rough estimate
    
    char buffer[8192];
    int ret;
    
    do {
        stream.avail_out = sizeof(buffer);
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        
        ret = deflate(&stream, Z_FINISH);
        
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&stream);
            throw std::runtime_error("Gzip compression stream error");
        }
        
        size_t compressed_size = sizeof(buffer) - stream.avail_out;
        compressed.append(buffer, compressed_size);
        
    } while (stream.avail_out == 0);
    
    deflateEnd(&stream);
    
    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Gzip compression failed");
    }
    
    return compressed;
}

std::string CompressionMiddleware::compressDeflate(const std::string& data) const {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    
    // Initialize for deflate compression (windowBits = 15 for deflate)
    if (deflateInit2(&stream, config_.compressionLevel, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("Failed to initialize deflate compression");
    }
    
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in = reinterpret_cast<const Bytef*>(data.c_str());
    
    std::string compressed;
    compressed.reserve(data.size() / 2); // Rough estimate
    
    char buffer[8192];
    int ret;
    
    do {
        stream.avail_out = sizeof(buffer);
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        
        ret = deflate(&stream, Z_FINISH);
        
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&stream);
            throw std::runtime_error("Deflate compression stream error");
        }
        
        size_t compressed_size = sizeof(buffer) - stream.avail_out;
        compressed.append(buffer, compressed_size);
        
    } while (stream.avail_out == 0);
    
    deflateEnd(&stream);
    
    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Deflate compression failed");
    }
    
    return compressed;
}

// CompressionMiddlewarePlugin implementation

bool CompressionMiddlewarePlugin::initialize(const PluginVersion& frameworkVersion) {
    // Check framework compatibility
    if (!frameworkVersion.isCompatible(pluginInfo_.min_framework_version)) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

void CompressionMiddlewarePlugin::shutdown() {
    initialized_ = false;
}

std::shared_ptr<cppSwitchboard::Middleware> CompressionMiddlewarePlugin::createMiddleware(
    const cppSwitchboard::MiddlewareInstanceConfig& config) {
    
    if (!initialized_) {
        return nullptr;
    }
    
    CompressionMiddleware::Config compressionConfig;
    
    // Parse configuration
    try {
        for (const auto& [key, value] : config.config) {
            if (key == "enabled") {
                compressionConfig.enabled = std::any_cast<bool>(value);
            } else if (key == "min_size") {
                compressionConfig.minSize = std::any_cast<size_t>(value);
            } else if (key == "compression_level") {
                int level = std::any_cast<int>(value);
                if (level >= 1 && level <= 9) {
                    compressionConfig.compressionLevel = level;
                }
            } else if (key == "compression_types") {
                auto types = std::any_cast<std::vector<std::string>>(value);
                compressionConfig.compressibleTypes.clear();
                for (const auto& type : types) {
                    compressionConfig.compressibleTypes.insert(type);
                }
            } else if (key == "excluded_paths") {
                auto paths = std::any_cast<std::vector<std::string>>(value);
                compressionConfig.excludedPaths.clear();
                for (const auto& path : paths) {
                    compressionConfig.excludedPaths.insert(path);
                }
            }
        }
    } catch (const std::bad_any_cast& e) {
        return nullptr; // Invalid configuration
    }
    
    return std::make_shared<CompressionMiddleware>(compressionConfig);
}

bool CompressionMiddlewarePlugin::validateConfig(
    const cppSwitchboard::MiddlewareInstanceConfig& config, 
    std::string& errorMessage) const {
    
    try {
        for (const auto& [key, value] : config.config) {
            if (key == "enabled") {
                std::any_cast<bool>(value);
            } else if (key == "min_size") {
                size_t size = std::any_cast<size_t>(value);
                if (size == 0) {
                    errorMessage = "min_size must be greater than 0";
                    return false;
                }
            } else if (key == "compression_level") {
                int level = std::any_cast<int>(value);
                if (level < 1 || level > 9) {
                    errorMessage = "compression_level must be between 1 and 9";
                    return false;
                }
            } else if (key == "compression_types") {
                std::any_cast<std::vector<std::string>>(value);
            } else if (key == "excluded_paths") {
                std::any_cast<std::vector<std::string>>(value);
            } else {
                errorMessage = "Unknown configuration key: " + key;
                return false;
            }
        }
    } catch (const std::bad_any_cast& e) {
        errorMessage = "Invalid configuration value type";
        return false;
    }
    
    return true;
}

std::vector<std::string> CompressionMiddlewarePlugin::getSupportedTypes() const {
    return {"compression"};
}

const MiddlewarePluginInfo& CompressionMiddlewarePlugin::getInfo() const {
    return pluginInfo_;
}

std::string CompressionMiddlewarePlugin::getConfigSchema() const {
    return R"({
        "type": "object",
        "properties": {
            "enabled": {
                "type": "boolean",
                "description": "Whether compression is enabled",
                "default": true
            },
            "min_size": {
                "type": "integer",
                "description": "Minimum response size to compress in bytes",
                "minimum": 1,
                "default": 1024
            },
            "compression_level": {
                "type": "integer",
                "description": "Compression level (1-9, higher is better compression)",
                "minimum": 1,
                "maximum": 9,
                "default": 6
            },
            "compression_types": {
                "type": "array",
                "description": "List of content types to compress",
                "items": {
                    "type": "string"
                },
                "default": [
                    "text/html", "text/css", "text/javascript", "text/plain",
                    "application/json", "application/xml", "application/javascript"
                ]
            },
            "excluded_paths": {
                "type": "array",
                "description": "List of paths to exclude from compression",
                "items": {
                    "type": "string"
                },
                "default": []
            }
        }
    })";
}

} // namespace ExamplePlugins

// Plugin C interface exports
extern "C" {
    CPPSWITCH_PLUGIN_EXPORT MiddlewarePluginInfo cppSwitchboard_plugin_info = 
        ExamplePlugins::CompressionMiddlewarePlugin::pluginInfo_;
    
    CPPSWITCH_PLUGIN_EXPORT cppSwitchboard::MiddlewarePlugin* cppSwitchboard_create_plugin() {
        return new ExamplePlugins::CompressionMiddlewarePlugin();
    }
    
    CPPSWITCH_PLUGIN_EXPORT void cppSwitchboard_destroy_plugin(cppSwitchboard::MiddlewarePlugin* plugin) {
        delete plugin;
    }
} 