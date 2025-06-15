/**
 * @file middleware_config.cpp
 * @brief Implementation of middleware configuration system for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.2.0
 */

#include <cppSwitchboard/middleware_config.h>
#include <cppSwitchboard/middleware_factory.h>
#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/middleware_pipeline.h>

// Include built-in middleware headers
#include <cppSwitchboard/middleware/auth_middleware.h>
#include <cppSwitchboard/middleware/authz_middleware.h>
#include <cppSwitchboard/middleware/rate_limit_middleware.h>
#include <cppSwitchboard/middleware/logging_middleware.h>
#include <cppSwitchboard/middleware/cors_middleware.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <cstdlib>
#include <cassert>
#include <stdexcept>
#include <unordered_set>

namespace cppSwitchboard {

// Simple YAML parser node (reuse existing implementation pattern)
struct YamlNode {
    std::string value;
    std::map<std::string, YamlNode> children;
    std::vector<YamlNode> array;
    bool isArray = false;
    
    bool hasChild(const std::string& key) const {
        return children.find(key) != children.end();
    }
    
    const YamlNode& getChild(const std::string& key) const {
        auto it = children.find(key);
        if (it != children.end()) {
            return it->second;
        }
        static YamlNode empty;
        return empty;
    }
    
    std::string getString(const std::string& defaultValue = "") const {
        return value.empty() ? defaultValue : value;
    }
    
    int getInt(int defaultValue = 0) const {
        if (value.empty()) return defaultValue;
        try {
            return std::stoi(value);
        } catch (...) {
            return defaultValue;
        }
    }
    
    bool getBool(bool defaultValue = false) const {
        if (value.empty()) return defaultValue;
        std::string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "true" || lower == "yes" || lower == "1";
    }
    
    std::vector<std::string> getStringArray() const {
        std::vector<std::string> result;
        for (const auto& item : array) {
            result.push_back(item.value);
        }
        return result;
    }
};

// Simple YAML parser (reuse existing pattern from config.cpp)
class MiddlewareYamlParser {
public:
    static YamlNode parse(const std::string& content) {
        YamlNode root;
        std::istringstream stream(content);
        std::string line;
        std::vector<std::pair<int, std::string>> lines;
        
        // Read all lines with indentation level
        while (std::getline(stream, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            size_t indent = 0;
            while (indent < line.length() && (line[indent] == ' ' || line[indent] == '\t')) {
                indent++;
            }
            
            if (indent < line.length()) {
                lines.push_back({static_cast<int>(indent), line.substr(indent)});
            }
        }
        
        parseLines(lines, 0, lines.size(), 0, root);
        return root;
    }

    static std::string removeQuotes(const std::string& str) {
        if (str.length() >= 2 && 
            ((str.front() == '"' && str.back() == '"') || 
             (str.front() == '\'' && str.back() == '\''))) {
            return str.substr(1, str.length() - 2);
        }
        return str;
    }

private:
    static void parseLines(const std::vector<std::pair<int, std::string>>& lines, 
                          size_t start, size_t end, int baseIndent, YamlNode& node) {
        for (size_t i = start; i < end; ++i) {
            int indent = lines[i].first;
            const std::string& line = lines[i].second;
            
            if (indent < baseIndent) break;
            if (indent > baseIndent) continue;
            
            // Handle array markers
            if (line.front() == '-' && line.length() > 1 && line[1] == ' ') {
                node.isArray = true;
                YamlNode arrayItem;
                std::string itemLine = trim(line.substr(2));
                
                // Parse key-value in array item
                size_t colonPos = itemLine.find(':');
                if (colonPos != std::string::npos) {
                    std::string key = trim(itemLine.substr(0, colonPos));
                    std::string value = trim(itemLine.substr(colonPos + 1));
                    arrayItem.children[key].value = removeQuotes(value);
                }
                
                // Find child lines for this array item
                size_t childStart = i + 1;
                size_t childEnd = childStart;
                int childIndent = indent + 2;
                
                while (childEnd < end && childEnd < lines.size()) {
                    if (lines[childEnd].first < childIndent) break;
                    if (lines[childEnd].first == indent && lines[childEnd].second.front() == '-') break;
                    childEnd++;
                }
                
                if (childStart < childEnd) {
                    parseLines(lines, childStart, childEnd, childIndent, arrayItem);
                    i = childEnd - 1;
                }
                
                node.array.push_back(arrayItem);
                continue;
            }
            
            // Parse key-value pair
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = trim(line.substr(0, colonPos));
                std::string value = trim(line.substr(colonPos + 1));
                
                YamlNode& child = node.children[key];
                
                if (!value.empty() && value != "[]") {
                    // Handle array notation [item1, item2]
                    if (value.front() == '[' && value.back() == ']') {
                        child.isArray = true;
                        std::string arrayContent = value.substr(1, value.length() - 2);
                        std::istringstream arrayStream(arrayContent);
                        std::string item;
                        while (std::getline(arrayStream, item, ',')) {
                            YamlNode arrayItem;
                            arrayItem.value = removeQuotes(trim(item));
                            child.array.push_back(arrayItem);
                        }
                    } else {
                        child.value = removeQuotes(value);
                    }
                }
                
                // Find child lines
                size_t childStart = i + 1;
                size_t childEnd = childStart;
                int childIndent = indent + 2;
                
                while (childEnd < end && 
                       (childEnd < lines.size() && lines[childEnd].first >= childIndent)) {
                    childEnd++;
                }
                
                if (childStart < childEnd) {
                    parseLines(lines, childStart, childEnd, childIndent, child);
                    i = childEnd - 1;
                }
            }
        }
    }
    
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
};

// MiddlewareInstanceConfig implementation
bool MiddlewareInstanceConfig::validate(std::string& errorMessage) const {
    if (name.empty()) {
        errorMessage = "Middleware name cannot be empty";
        return false;
    }
    
    if (priority < -1000 || priority > 1000) {
        errorMessage = "Middleware priority must be between -1000 and 1000, got: " + std::to_string(priority);
        return false;
    }
    
    return true;
}

std::string MiddlewareInstanceConfig::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = config.find(key);
    if (it == config.end()) return defaultValue;
    
    try {
        return std::any_cast<std::string>(it->second);
    } catch (const std::bad_any_cast&) {
        return defaultValue;
    }
}

int MiddlewareInstanceConfig::getInt(const std::string& key, int defaultValue) const {
    auto it = config.find(key);
    if (it == config.end()) return defaultValue;
    
    try {
        return std::any_cast<int>(it->second);
    } catch (const std::bad_any_cast&) {
        try {
            std::string str = std::any_cast<std::string>(it->second);
            return std::stoi(str);
        } catch (...) {
            return defaultValue;
        }
    }
}

bool MiddlewareInstanceConfig::getBool(const std::string& key, bool defaultValue) const {
    auto it = config.find(key);
    if (it == config.end()) return defaultValue;
    
    try {
        return std::any_cast<bool>(it->second);
    } catch (const std::bad_any_cast&) {
        try {
            std::string str = std::any_cast<std::string>(it->second);
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            return str == "true" || str == "yes" || str == "1";
        } catch (...) {
            return defaultValue;
        }
    }
}

std::vector<std::string> MiddlewareInstanceConfig::getStringArray(const std::string& key) const {
    auto it = config.find(key);
    if (it == config.end()) return {};
    
    try {
        return std::any_cast<std::vector<std::string>>(it->second);
    } catch (const std::bad_any_cast&) {
        return {};
    }
}

bool MiddlewareInstanceConfig::hasKey(const std::string& key) const {
    return config.find(key) != config.end();
}

// RouteMiddlewareConfig implementation
bool RouteMiddlewareConfig::validate(std::string& errorMessage) const {
    if (pattern.empty()) {
        errorMessage = "Route pattern cannot be empty";
        return false;
    }
    
    // Validate regex pattern if specified
    if (isRegex) {
        try {
            std::regex testRegex(pattern);
        } catch (const std::regex_error& e) {
            errorMessage = "Invalid regex pattern '" + pattern + "': " + e.what();
            return false;
        }
    }
    
    // Validate all middleware in this route
    for (size_t i = 0; i < middlewares.size(); ++i) {
        std::string middlewareError;
        if (!middlewares[i].validate(middlewareError)) {
            errorMessage = "Route '" + pattern + "' middleware[" + std::to_string(i) + "]: " + middlewareError;
            return false;
        }
    }
    
    return true;
}

bool RouteMiddlewareConfig::matchesPath(const std::string& path) const {
    if (isRegex) {
        try {
            std::regex pathRegex(pattern);
            return std::regex_match(path, pathRegex);
        } catch (const std::regex_error&) {
            return false;
        }
    } else {
        // Simple glob pattern matching
        return globMatch(path, pattern);
    }
}

// Simple glob pattern matching implementation
bool RouteMiddlewareConfig::globMatch(const std::string& text, const std::string& pattern) const {
    size_t textPos = 0, patternPos = 0;
    size_t starIdx = std::string::npos, match = 0;
    
    while (textPos < text.length()) {
        if (patternPos < pattern.length() && 
            (pattern[patternPos] == '?' || pattern[patternPos] == text[textPos])) {
            textPos++;
            patternPos++;
        } else if (patternPos < pattern.length() && pattern[patternPos] == '*') {
            starIdx = patternPos;
            match = textPos;
            patternPos++;
        } else if (starIdx != std::string::npos) {
            patternPos = starIdx + 1;
            match++;
            textPos = match;
        } else {
            return false;
        }
    }
    
    while (patternPos < pattern.length() && pattern[patternPos] == '*') {
        patternPos++;
    }
    
    return patternPos == pattern.length();
}



// GlobalMiddlewareConfig implementation
bool GlobalMiddlewareConfig::validate(std::string& errorMessage) const {
    for (size_t i = 0; i < middlewares.size(); ++i) {
        std::string middlewareError;
        if (!middlewares[i].validate(middlewareError)) {
            errorMessage = "Global middleware[" + std::to_string(i) + "]: " + middlewareError;
            return false;
        }
    }
    return true;
}

// HotReloadConfig implementation
bool HotReloadConfig::validate(std::string& errorMessage) const {
    if (enabled && checkInterval.count() < 1) {
        errorMessage = "Hot reload check interval must be at least 1 second";
        return false;
    }
    
    if (enabled && watchedFiles.empty()) {
        errorMessage = "Hot reload enabled but no files specified to watch";
        return false;
    }
    
    return true;
}

// ComprehensiveMiddlewareConfig implementation
bool ComprehensiveMiddlewareConfig::validate(std::string& errorMessage) const {
    // Validate global middleware
    if (!global.validate(errorMessage)) {
        return false;
    }
    
    // Validate route middleware
    for (size_t i = 0; i < routes.size(); ++i) {
        std::string routeError;
        if (!routes[i].validate(routeError)) {
            errorMessage = "Route[" + std::to_string(i) + "]: " + routeError;
            return false;
        }
    }
    
    // Validate hot reload configuration
    if (!hotReload.validate(errorMessage)) {
        return false;
    }
    
    // Check for duplicate route patterns
    std::unordered_set<std::string> patterns;
    for (const auto& route : routes) {
        if (patterns.find(route.pattern) != patterns.end()) {
            errorMessage = "Duplicate route pattern: " + route.pattern;
            return false;
        }
        patterns.insert(route.pattern);
    }
    
    return true;
}

std::vector<MiddlewareInstanceConfig> ComprehensiveMiddlewareConfig::getMiddlewareForRoute(const std::string& path) const {
    std::vector<MiddlewareInstanceConfig> result;
    
    // Add global middleware first
    for (const auto& middleware : global.middlewares) {
        if (middleware.enabled) {
            result.push_back(middleware);
        }
    }
    
    // Add route-specific middleware
    for (const auto& route : routes) {
        if (route.matchesPath(path)) {
            for (const auto& middleware : route.middlewares) {
                if (middleware.enabled) {
                    result.push_back(middleware);
                }
            }
            break; // Only use first matching route
        }
    }
    
    // Sort by priority (higher priority first)
    std::sort(result.begin(), result.end(), [](const MiddlewareInstanceConfig& a, const MiddlewareInstanceConfig& b) {
        return a.priority > b.priority;
    });
    
    return result;
}

std::vector<std::string> ComprehensiveMiddlewareConfig::getAllMiddlewareNames() const {
    std::unordered_set<std::string> names;
    
    // Collect from global middleware
    for (const auto& middleware : global.middlewares) {
        names.insert(middleware.name);
    }
    
    // Collect from route middleware
    for (const auto& route : routes) {
        for (const auto& middleware : route.middlewares) {
            names.insert(middleware.name);
        }
    }
    
    return std::vector<std::string>(names.begin(), names.end());
}

bool ComprehensiveMiddlewareConfig::hasMiddleware(const std::string& middlewareName) const {
    // Check global middleware
    for (const auto& middleware : global.middlewares) {
        if (middleware.name == middlewareName) {
            return true;
        }
    }
    
    // Check route middleware
    for (const auto& route : routes) {
        for (const auto& middleware : route.middlewares) {
            if (middleware.name == middlewareName) {
                return true;
            }
        }
    }
    
    return false;
}

// MiddlewareConfigLoader implementation
MiddlewareConfigResult MiddlewareConfigLoader::loadFromFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        return MiddlewareConfigResult::failure(
            MiddlewareConfigError::FILE_NOT_FOUND, 
            "Failed to open middleware config file: " + filename
        );
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // Add file to hot reload watch list
    if (std::find(config_.hotReload.watchedFiles.begin(), 
                  config_.hotReload.watchedFiles.end(), 
                  filename) == config_.hotReload.watchedFiles.end()) {
        config_.hotReload.watchedFiles.push_back(filename);
    }
    
    return loadFromString(content);
}

MiddlewareConfigResult MiddlewareConfigLoader::loadFromString(const std::string& yamlContent) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    try {
        YamlNode root = MiddlewareYamlParser::parse(yamlContent);
        
        // Parse middleware section
        if (!root.hasChild("middleware")) {
            return MiddlewareConfigResult::failure(
                MiddlewareConfigError::INVALID_YAML,
                "YAML must contain 'middleware' section"
            );
        }
        
        const YamlNode& middlewareNode = root.getChild("middleware");
        
        ComprehensiveMiddlewareConfig newConfig;
        
        // Parse global middleware
        if (middlewareNode.hasChild("global")) {
            auto result = parseGlobalMiddleware(middlewareNode.getChild("global"), newConfig.global);
            if (!result.isSuccess()) {
                return result;
            }
        }
        
        // Parse route middleware
        if (middlewareNode.hasChild("routes")) {
            auto result = parseRouteMiddleware(middlewareNode.getChild("routes"), newConfig.routes);
            if (!result.isSuccess()) {
                return result;
            }
        }
        
        // Parse hot reload configuration
        if (middlewareNode.hasChild("hot_reload")) {
            auto result = parseHotReloadConfig(middlewareNode.getChild("hot_reload"), newConfig.hotReload);
            if (!result.isSuccess()) {
                return result;
            }
        }
        
        // Validate the complete configuration
        auto validationResult = validateConfiguration(newConfig);
        if (!validationResult.isSuccess()) {
            return validationResult;
        }
        
        config_ = std::move(newConfig);
        loaded_ = true;
        
        return MiddlewareConfigResult::success();
        
    } catch (const std::exception& e) {
        return MiddlewareConfigResult::failure(
            MiddlewareConfigError::INVALID_YAML,
            "YAML parsing error: " + std::string(e.what())
        );
    }
}

MiddlewareConfigResult MiddlewareConfigLoader::mergeFromFile(const std::string& filename) {
    MiddlewareConfigLoader tempLoader;
    auto result = tempLoader.loadFromFile(filename);
    if (!result.isSuccess()) {
        return result;
    }
    
    std::lock_guard<std::mutex> lock(configMutex_);
    if (!loaded_) {
        return MiddlewareConfigResult::failure(
            MiddlewareConfigError::VALIDATION_FAILED,
            "Cannot merge into unloaded configuration"
        );
    }
    
    mergeConfigurations(config_, tempLoader.getConfiguration());
    return MiddlewareConfigResult::success();
}

const ComprehensiveMiddlewareConfig& MiddlewareConfigLoader::getConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    if (!loaded_) {
        throw std::runtime_error("No middleware configuration has been loaded");
    }
    return config_;
}

ComprehensiveMiddlewareConfig MiddlewareConfigLoader::createDefault() {
    ComprehensiveMiddlewareConfig config;
    
    // Add default CORS middleware
    MiddlewareInstanceConfig cors;
    cors.name = "cors";
    cors.enabled = true;
    cors.priority = 200;
    cors.config["origins"] = std::vector<std::string>{"*"};
    cors.config["methods"] = std::vector<std::string>{"GET", "POST", "PUT", "DELETE", "OPTIONS"};
    cors.config["headers"] = std::vector<std::string>{"Content-Type", "Authorization"};
    config.global.middlewares.push_back(cors);
    
    // Add default logging middleware
    MiddlewareInstanceConfig logging;
    logging.name = "logging";
    logging.enabled = true;
    logging.priority = 0;
    logging.config["format"] = std::string("combined");
    logging.config["include_headers"] = false;
    config.global.middlewares.push_back(logging);
    
    return config;
}

MiddlewareConfigResult MiddlewareConfigLoader::validateConfiguration(const ComprehensiveMiddlewareConfig& config) {
    std::string errorMessage;
    if (!config.validate(errorMessage)) {
        return MiddlewareConfigResult::failure(
            MiddlewareConfigError::VALIDATION_FAILED,
            errorMessage
        );
    }
    
    // Validate that all middleware types are known
    MiddlewareFactory& factory = MiddlewareFactory::getInstance();
    auto allNames = config.getAllMiddlewareNames();
    
    for (const auto& name : allNames) {
        if (!factory.isMiddlewareRegistered(name)) {
            return MiddlewareConfigResult::failure(
                MiddlewareConfigError::UNKNOWN_MIDDLEWARE,
                "Unknown middleware type: " + name
            );
        }
    }
    
    return MiddlewareConfigResult::success();
}

// Private helper methods
MiddlewareConfigResult MiddlewareConfigLoader::parseMiddlewareInstance(const YamlNode& yamlNode, MiddlewareInstanceConfig& config) {
    if (!yamlNode.hasChild("name")) {
        return MiddlewareConfigResult::failure(
            MiddlewareConfigError::MISSING_REQUIRED_CONFIG,
            "Middleware instance must have 'name' field"
        );
    }
    
    config.name = yamlNode.getChild("name").getString();
    config.enabled = yamlNode.getChild("enabled").getBool(true);
    config.priority = yamlNode.getChild("priority").getInt(0);
    
    // Parse middleware-specific configuration
    if (yamlNode.hasChild("config")) {
        const YamlNode& configNode = yamlNode.getChild("config");
        parseConfigNode(configNode, config.config);
    }
    
    return MiddlewareConfigResult::success();
}

void MiddlewareConfigLoader::parseConfigNode(const YamlNode& node, std::unordered_map<std::string, std::any>& config) {
    for (const auto& [key, childNode] : node.children) {
        if (childNode.isArray) {
            std::vector<std::string> arrayValues = childNode.getStringArray();
            config[key] = arrayValues;
        } else if (!childNode.children.empty()) {
            std::unordered_map<std::string, std::any> nestedConfig;
            parseConfigNode(childNode, nestedConfig);
            config[key] = nestedConfig;
        } else {
            std::string value = childNode.getString();
            if (environmentSubstitution_) {
                value = substituteEnvironmentVariables(value);
            }
            
            // Try to infer type
            if (value == "true" || value == "false") {
                config[key] = (value == "true");
            } else {
                try {
                    int intVal = std::stoi(value);
                    config[key] = intVal;
                } catch (...) {
                    config[key] = value;
                }
            }
        }
    }
}

MiddlewareConfigResult MiddlewareConfigLoader::parseGlobalMiddleware(const YamlNode& yamlNode, GlobalMiddlewareConfig& config) {
    if (!yamlNode.isArray) {
        return MiddlewareConfigResult::failure(
            MiddlewareConfigError::INVALID_YAML,
            "Global middleware must be an array"
        );
    }
    
    for (const auto& middlewareNode : yamlNode.array) {
        MiddlewareInstanceConfig instance;
        auto result = parseMiddlewareInstance(middlewareNode, instance);
        if (!result.isSuccess()) {
            return result;
        }
        config.middlewares.push_back(instance);
    }
    
    return MiddlewareConfigResult::success();
}

MiddlewareConfigResult MiddlewareConfigLoader::parseRouteMiddleware(const YamlNode& yamlNode, std::vector<RouteMiddlewareConfig>& routes) {
    for (const auto& [pattern, routeNode] : yamlNode.children) {
        RouteMiddlewareConfig route;
        route.pattern = MiddlewareYamlParser::removeQuotes(pattern);
        route.isRegex = route.pattern.find(".*") != std::string::npos || route.pattern.find("\\") != std::string::npos;
        
        if (!routeNode.isArray) {
            return MiddlewareConfigResult::failure(
                MiddlewareConfigError::INVALID_YAML,
                "Route middleware for pattern '" + route.pattern + "' must be an array"
            );
        }
        
        for (const auto& middlewareNode : routeNode.array) {
            MiddlewareInstanceConfig instance;
            auto result = parseMiddlewareInstance(middlewareNode, instance);
            if (!result.isSuccess()) {
                return result;
            }
            route.middlewares.push_back(instance);
        }
        
        routes.push_back(route);
    }
    
    return MiddlewareConfigResult::success();
}

MiddlewareConfigResult MiddlewareConfigLoader::parseHotReloadConfig(const YamlNode& yamlNode, HotReloadConfig& config) {
    config.enabled = yamlNode.getChild("enabled").getBool(false);
    config.checkInterval = std::chrono::seconds(yamlNode.getChild("check_interval").getInt(5));
    config.reloadOnChange = yamlNode.getChild("reload_on_change").getBool(true);
    config.validateBeforeReload = yamlNode.getChild("validate_before_reload").getBool(true);
    
    if (yamlNode.hasChild("watched_files")) {
        config.watchedFiles = yamlNode.getChild("watched_files").getStringArray();
    }
    
    return MiddlewareConfigResult::success();
}

std::string MiddlewareConfigLoader::substituteEnvironmentVariables(const std::string& value) const {
    std::string result = value;
    std::regex envVarRegex(R"(\$\{([^}]+)\})");
    std::smatch match;
    
    while (std::regex_search(result, match, envVarRegex)) {
        std::string envVar = match[1].str();
        const char* envValue = std::getenv(envVar.c_str());
        std::string replacement = envValue ? envValue : "";
        result.replace(match.position(), match.length(), replacement);
    }
    
    return result;
}

void MiddlewareConfigLoader::mergeConfigurations(ComprehensiveMiddlewareConfig& base, const ComprehensiveMiddlewareConfig& overlay) {
    // Merge global middleware (append overlay to base)
    for (const auto& middleware : overlay.global.middlewares) {
        base.global.middlewares.push_back(middleware);
    }
    
    // Merge route middleware
    for (const auto& route : overlay.routes) {
        // Check if route pattern already exists
        bool found = false;
        for (auto& existingRoute : base.routes) {
            if (existingRoute.pattern == route.pattern) {
                // Replace existing route middleware
                existingRoute.middlewares = route.middlewares;
                found = true;
                break;
            }
        }
        
        if (!found) {
            base.routes.push_back(route);
        }
    }
    
    // Merge hot reload configuration (overlay takes precedence)
    if (overlay.hotReload.enabled) {
        base.hotReload = overlay.hotReload;
    }
}

// MiddlewareFactory implementation moved to middleware_factory.cpp

} // namespace cppSwitchboard 