#include <cppSwitchboard/config.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <memory>

namespace cppSwitchboard {

// Simple YAML parser for our configuration needs
class SimpleYamlParser {
public:
    struct Node {
        std::string value;
        std::map<std::string, Node> children;
        std::vector<Node> array;
        bool isArray = false;
        
        bool hasChild(const std::string& key) const {
            return children.find(key) != children.end();
        }
        
        const Node& getChild(const std::string& key) const {
            auto it = children.find(key);
            if (it != children.end()) {
                return it->second;
            }
            static Node empty;
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
    
    static Node parse(const std::string& content) {
        Node root;
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

private:
    static void parseLines(const std::vector<std::pair<int, std::string>>& lines, 
                          size_t start, size_t end, int baseIndent, Node& node) {
        for (size_t i = start; i < end; ++i) {
            int indent = lines[i].first;
            const std::string& line = lines[i].second;
            
            if (indent < baseIndent) break;
            if (indent > baseIndent) continue;
            
            // Parse key-value pair
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = trim(line.substr(0, colonPos));
                std::string value = trim(line.substr(colonPos + 1));
                
                Node& child = node.children[key];
                
                if (!value.empty() && value != "[]") {
                    // Handle array notation [item1, item2]
                    if (value.front() == '[' && value.back() == ']') {
                        child.isArray = true;
                        std::string arrayContent = value.substr(1, value.length() - 2);
                        std::istringstream arrayStream(arrayContent);
                        std::string item;
                        while (std::getline(arrayStream, item, ',')) {
                            Node arrayItem;
                            arrayItem.value = trim(item);
                            // Remove quotes if present
                            if (arrayItem.value.front() == '"' && arrayItem.value.back() == '"') {
                                arrayItem.value = arrayItem.value.substr(1, arrayItem.value.length() - 2);
                            }
                            child.array.push_back(arrayItem);
                        }
                    } else {
                        // Remove quotes if present
                        if (value.front() == '"' && value.back() == '"') {
                            value = value.substr(1, value.length() - 2);
                        }
                        child.value = value;
                    }
                }
                
                // Find child lines
                size_t childStart = i + 1;
                size_t childEnd = childStart;
                int childIndent = indent + 2; // Assuming 2-space indentation
                
                while (childEnd < end && 
                       (childEnd < lines.size() && lines[childEnd].first >= childIndent)) {
                    childEnd++;
                }
                
                if (childStart < childEnd) {
                    parseLines(lines, childStart, childEnd, childIndent, child);
                    i = childEnd - 1; // Skip processed lines
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

std::unique_ptr<ServerConfig> ConfigLoader::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return createDefault();
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    return loadFromString(content);
}

std::unique_ptr<ServerConfig> ConfigLoader::loadFromString(const std::string& yamlContent) {
    auto config = std::make_unique<ServerConfig>();
    
    try {
        auto root = SimpleYamlParser::parse(yamlContent);
        
        // Parse server section (support both nested and flat structure)
        const auto& serverNode = root.hasChild("server") ? root.getChild("server") : root;
        
        // HTTP/1.1 configuration
        if (serverNode.hasChild("http1")) {
            const auto& http1Node = serverNode.getChild("http1");
            config->http1.enabled = http1Node.getChild("enabled").getBool(true);
            config->http1.port = http1Node.getChild("port").getInt(8080);
            config->http1.bindAddress = http1Node.getChild("bindAddress").getString(
                http1Node.getChild("bind_address").getString("0.0.0.0"));
        }
        
        // HTTP/2 configuration
        if (serverNode.hasChild("http2")) {
            const auto& http2Node = serverNode.getChild("http2");
            config->http2.enabled = http2Node.getChild("enabled").getBool(true);
            config->http2.port = http2Node.getChild("port").getInt(8443);
            config->http2.bindAddress = http2Node.getChild("bindAddress").getString(
                http2Node.getChild("bind_address").getString("0.0.0.0"));
        }
        
        // SSL configuration
        if (serverNode.hasChild("ssl")) {
            const auto& sslNode = serverNode.getChild("ssl");
            config->ssl.enabled = sslNode.getChild("enabled").getBool(false);
            config->ssl.certificateFile = substituteEnvironmentVariables(
                sslNode.getChild("certificatePath").getString(
                    sslNode.getChild("certificate_file").getString()));
            config->ssl.privateKeyFile = substituteEnvironmentVariables(
                sslNode.getChild("privateKeyPath").getString(
                    sslNode.getChild("private_key_file").getString()));
            config->ssl.caCertificateFile = substituteEnvironmentVariables(
                sslNode.getChild("caCertificatePath").getString(
                    sslNode.getChild("ca_certificate_file").getString()));
            config->ssl.verifyClient = sslNode.getChild("verifyClient").getBool(
                sslNode.getChild("verify_client").getBool(false));
        }
            
        // General configuration
        if (serverNode.hasChild("general")) {
            const auto& generalNode = serverNode.getChild("general");
            config->general.maxConnections = generalNode.getChild("maxConnections").getInt(
                generalNode.getChild("max_connections").getInt(1000));
            config->general.requestTimeout = std::chrono::seconds(
                generalNode.getChild("requestTimeout").getInt(
                    generalNode.getChild("request_timeout_seconds").getInt(30)));
            config->general.enableLogging = generalNode.getChild("enableLogging").getBool(
                generalNode.getChild("enable_logging").getBool(true));
            config->general.logLevel = generalNode.getChild("logLevel").getString(
                generalNode.getChild("log_level").getString("info"));
            config->general.workerThreads = generalNode.getChild("workerThreads").getInt(
                generalNode.getChild("worker_threads").getInt(4));
        }
            
        // Security configuration
        if (serverNode.hasChild("security")) {
            const auto& securityNode = serverNode.getChild("security");
            config->security.enableCors = securityNode.getChild("enableCors").getBool(
                securityNode.getChild("enable_cors").getBool(false));
            config->security.corsOrigins = securityNode.getChild("allowedOrigins").getStringArray();
            if (config->security.corsOrigins.empty()) {
                config->security.corsOrigins = securityNode.getChild("cors_origins").getStringArray();
            }
            config->security.maxRequestSizeMb = securityNode.getChild("maxRequestSize").getInt(
                securityNode.getChild("max_request_size_mb").getInt(10)) / 1048576; // Convert bytes to MB
            config->security.maxHeaderSizeKb = securityNode.getChild("maxHeaderSize").getInt(
                securityNode.getChild("max_header_size_kb").getInt(8));
            config->security.rateLimitEnabled = securityNode.getChild("rateLimitEnabled").getBool(
                securityNode.getChild("rate_limit_enabled").getBool(false));
            config->security.rateLimitRequestsPerMinute = 
                securityNode.getChild("rateLimitRequestsPerMinute").getInt(
                    securityNode.getChild("rate_limit_requests_per_minute").getInt(100));
        }
        
        // Middleware configuration
        if (serverNode.hasChild("middleware")) {
            const auto& middlewareNode = serverNode.getChild("middleware");
            
            if (middlewareNode.hasChild("logging")) {
                const auto& loggingNode = middlewareNode.getChild("logging");
                config->middleware.logging.enabled = loggingNode.getChild("enabled").getBool(true);
                config->middleware.logging.format = loggingNode.getChild("format").getString("combined");
            }
            
            if (middlewareNode.hasChild("compression")) {
                const auto& compressionNode = middlewareNode.getChild("compression");
                config->middleware.compression.enabled = compressionNode.getChild("enabled").getBool(true);
                config->middleware.compression.algorithms = compressionNode.getChild("algorithms").getStringArray();
                config->middleware.compression.minSizeBytes = compressionNode.getChild("min_size_bytes").getInt(1024);
            }
            
            if (middlewareNode.hasChild("static_files")) {
                const auto& staticNode = middlewareNode.getChild("static_files");
                config->middleware.staticFiles.enabled = staticNode.getChild("enabled").getBool(false);
                config->middleware.staticFiles.rootDirectory = staticNode.getChild("root_directory").getString("/var/www/html");
                config->middleware.staticFiles.indexFiles = staticNode.getChild("index_files").getStringArray();
                config->middleware.staticFiles.cacheMaxAgeSeconds = staticNode.getChild("cache_max_age_seconds").getInt(3600);
            }
        }
        
        // Parse application section
        if (root.hasChild("application")) {
            const auto& appNode = root.getChild("application");
            config->application.name = appNode.getChild("name").getString("QoS Manager HTTP Framework");
            config->application.version = appNode.getChild("version").getString("1.0.0");
            config->application.environment = appNode.getChild("environment").getString("development");
            
            if (appNode.hasChild("database")) {
                const auto& dbNode = appNode.getChild("database");
                config->application.database.enabled = dbNode.getChild("enabled").getBool(false);
                config->application.database.type = dbNode.getChild("type").getString("postgresql");
                config->application.database.host = dbNode.getChild("host").getString("localhost");
                config->application.database.port = dbNode.getChild("port").getInt(5432);
                config->application.database.database = dbNode.getChild("database").getString("qos_manager");
                config->application.database.username = substituteEnvironmentVariables(
                    dbNode.getChild("username").getString("qos_user"));
                config->application.database.password = substituteEnvironmentVariables(
                    dbNode.getChild("password").getString("qos_password"));
                config->application.database.poolSize = dbNode.getChild("pool_size").getInt(10);
            }
        }
        
        // Parse monitoring section
        if (root.hasChild("monitoring")) {
            const auto& monitoringNode = root.getChild("monitoring");
            
            if (monitoringNode.hasChild("health_check")) {
                const auto& healthNode = monitoringNode.getChild("health_check");
                config->monitoring.healthCheck.enabled = healthNode.getChild("enabled").getBool(true);
                config->monitoring.healthCheck.endpoint = healthNode.getChild("endpoint").getString("/health");
            }
            
            if (monitoringNode.hasChild("metrics")) {
                const auto& metricsNode = monitoringNode.getChild("metrics");
                config->monitoring.metrics.enabled = metricsNode.getChild("enabled").getBool(false);
                config->monitoring.metrics.endpoint = metricsNode.getChild("endpoint").getString("/metrics");
                config->monitoring.metrics.port = metricsNode.getChild("port").getInt(9090);
            }
            
            // Parse debug logging configuration
            if (monitoringNode.hasChild("debug_logging")) {
                const auto& debugNode = monitoringNode.getChild("debug_logging");
                config->monitoring.debugLogging.enabled = debugNode.getChild("enabled").getBool(false);
                config->monitoring.debugLogging.outputFile = debugNode.getChild("output_file").getString("");
                config->monitoring.debugLogging.timestampFormat = debugNode.getChild("timestamp_format").getString("%Y-%m-%d %H:%M:%S");
                
                // Parse headers section
                if (debugNode.hasChild("headers")) {
                    const auto& headersNode = debugNode.getChild("headers");
                    config->monitoring.debugLogging.headers.enabled = headersNode.getChild("enabled").getBool(false);
                    config->monitoring.debugLogging.headers.logRequestHeaders = headersNode.getChild("log_request_headers").getBool(true);
                    config->monitoring.debugLogging.headers.logResponseHeaders = headersNode.getChild("log_response_headers").getBool(true);
                    config->monitoring.debugLogging.headers.includeUrlDetails = headersNode.getChild("include_url_details").getBool(true);
                    config->monitoring.debugLogging.headers.excludeHeaders = headersNode.getChild("exclude_headers").getStringArray();
                    if (config->monitoring.debugLogging.headers.excludeHeaders.empty()) {
                        config->monitoring.debugLogging.headers.excludeHeaders = {"authorization", "cookie", "set-cookie"};
                    }
                }
                
                // Parse payload section
                if (debugNode.hasChild("payload")) {
                    const auto& payloadNode = debugNode.getChild("payload");
                    config->monitoring.debugLogging.payload.enabled = payloadNode.getChild("enabled").getBool(false);
                    config->monitoring.debugLogging.payload.logRequestPayload = payloadNode.getChild("log_request_payload").getBool(true);
                    config->monitoring.debugLogging.payload.logResponsePayload = payloadNode.getChild("log_response_payload").getBool(true);
                    config->monitoring.debugLogging.payload.maxPayloadSizeBytes = payloadNode.getChild("max_payload_size_bytes").getInt(1024);
                    config->monitoring.debugLogging.payload.excludeContentTypes = payloadNode.getChild("exclude_content_types").getStringArray();
                    if (config->monitoring.debugLogging.payload.excludeContentTypes.empty()) {
                        config->monitoring.debugLogging.payload.excludeContentTypes = {"image/", "video/", "audio/", "application/octet-stream"};
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing configuration: " << e.what() << std::endl;
        return createDefault();
    }
    
    // Validate configuration
    std::string errorMessage;
    if (!validateConfig(*config, errorMessage)) {
        std::cerr << "Configuration validation failed: " << errorMessage << std::endl;
        return createDefault();
    }
    
    return config;
}

std::unique_ptr<ServerConfig> ConfigLoader::createDefault() {
    return std::make_unique<ServerConfig>();
}

bool ConfigLoader::validateConfig(const ServerConfig& config, std::string& errorMessage) {
    // Validate ports
    if (config.http1.enabled && (config.http1.port < 1 || config.http1.port > 65535)) {
        errorMessage = "Invalid HTTP/1.1 port: " + std::to_string(config.http1.port);
        return false;
    }
    
    if (config.http2.enabled && (config.http2.port < 1 || config.http2.port > 65535)) {
        errorMessage = "Invalid HTTP/2 port: " + std::to_string(config.http2.port);
        return false;
    }
    
    if (config.http1.enabled && config.http2.enabled && config.http1.port == config.http2.port) {
        errorMessage = "HTTP/1.1 and HTTP/2 cannot use the same port";
        return false;
    }
    
    // Validate SSL configuration
    if (config.ssl.enabled) {
        if (config.ssl.certificateFile.empty()) {
            errorMessage = "SSL certificate file is required when SSL is enabled";
            return false;
        }
        if (config.ssl.privateKeyFile.empty()) {
            errorMessage = "SSL private key file is required when SSL is enabled";
            return false;
        }
    }
    
    // Validate general settings
    if (config.general.maxConnections < 1) {
        errorMessage = "Max connections must be at least 1";
        return false;
    }
    
    if (config.general.workerThreads < 1) {
        errorMessage = "Worker threads must be at least 1";
        return false;
    }
    
    return true;
}

std::string ConfigLoader::substituteEnvironmentVariables(const std::string& value) {
    std::string result = value;
    size_t pos = 0;
    
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end = result.find("}", pos);
        if (end == std::string::npos) break;
        
        std::string varName = result.substr(pos + 2, end - pos - 2);
        const char* envValue = std::getenv(varName.c_str());
        std::string replacement = envValue ? envValue : "";
        
        result.replace(pos, end - pos + 1, replacement);
        pos += replacement.length();
    }
    
    return result;
}

// ConfigValidator implementation
bool ConfigValidator::validateConfig(const ServerConfig& config, std::string& errorMessage) {
    return ConfigLoader::validateConfig(config, errorMessage);
}

bool ConfigValidator::validatePortRange(int port, const std::string& portType, std::string& errorMessage) {
    if (port < 1 || port > 65535) {
        errorMessage = "Invalid " + portType + " port: " + std::to_string(port) + ". Port must be between 1 and 65535.";
        return false;
    }
    return true;
}

bool ConfigValidator::validateSslConfig(const SslConfig& ssl, std::string& errorMessage) {
    if (ssl.enabled) {
        if (ssl.certificateFile.empty()) {
            errorMessage = "SSL certificate file is required when SSL is enabled";
            return false;
        }
        if (ssl.privateKeyFile.empty()) {
            errorMessage = "SSL private key file is required when SSL is enabled";
            return false;
        }
    }
    return true;
}

bool ConfigValidator::validateGeneralConfig(const GeneralConfig& general, std::string& errorMessage) {
    if (general.maxConnections < 1) {
        errorMessage = "Max connections must be at least 1";
        return false;
    }
    
    if (general.workerThreads < 1) {
        errorMessage = "Worker threads must be at least 1";
        return false;
    }
    
    // Validate log level
    const std::vector<std::string> validLogLevels = {"debug", "info", "warn", "error"};
    bool validLogLevel = false;
    for (const auto& level : validLogLevels) {
        if (general.logLevel == level) {
            validLogLevel = true;
            break;
        }
    }
    
    if (!validLogLevel) {
        errorMessage = "Invalid log level: " + general.logLevel + ". Valid levels are: debug, info, warn, error";
        return false;
    }
    
    return true;
}

} // namespace cppSwitchboard 