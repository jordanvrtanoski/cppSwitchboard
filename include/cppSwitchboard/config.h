/**
 * @file config.h
 * @brief Configuration structures and utilities for cppSwitchboard HTTP server
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.2.0
 * 
 * This file contains all configuration structures and utilities needed to configure
 * the cppSwitchboard HTTP server. It provides comprehensive configuration options
 * for SSL/TLS, HTTP/1.1, HTTP/2, security, middleware, monitoring, and more.
 * 
 * @section config_example Configuration Example
 * @code{.cpp}
 * // Create a basic server configuration
 * cppSwitchboard::ServerConfig config;
 * config.http1.port = 8080;
 * config.http2.port = 8443;
 * config.ssl.enabled = true;
 * config.ssl.certificateFile = "/path/to/cert.pem";
 * config.ssl.privateKeyFile = "/path/to/key.pem";
 * 
 * // Validate the configuration
 * std::string errorMessage;
 * if (!cppSwitchboard::ConfigValidator::validateConfig(config, errorMessage)) {
 *     std::cerr << "Configuration error: " << errorMessage << std::endl;
 * }
 * @endcode
 * 
 * @see ConfigLoader
 * @see ConfigValidator
 * @see ServerConfig
 */
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace cppSwitchboard {

/**
 * @brief SSL/TLS configuration structure
 * 
 * Contains all settings required for SSL/TLS encryption configuration.
 * This structure allows you to enable HTTPS and configure certificate files.
 * 
 * @code{.cpp}
 * SslConfig ssl;
 * ssl.enabled = true;
 * ssl.certificateFile = "/etc/ssl/certs/server.crt";
 * ssl.privateKeyFile = "/etc/ssl/private/server.key";
 * ssl.verifyClient = false;
 * @endcode
 */
struct SslConfig {
    bool enabled = false;                     ///< Enable SSL/TLS encryption
    std::string certificateFile;              ///< Path to SSL certificate file (.crt/.pem)
    std::string privateKeyFile;               ///< Path to private key file (.key/.pem)
    std::string caCertificateFile;            ///< Path to CA certificate file for client verification
    bool verifyClient = false;                ///< Enable client certificate verification
};

/**
 * @brief HTTP/1.1 server configuration
 * 
 * Configuration options specific to HTTP/1.1 protocol support.
 * Controls the binding address and port for HTTP/1.1 connections.
 */
struct Http1Config {
    bool enabled = true;                      ///< Enable HTTP/1.1 support
    int port = 8080;                         ///< HTTP/1.1 listening port
    std::string bindAddress = "0.0.0.0";    ///< IP address to bind to (0.0.0.0 for all interfaces)
};

/**
 * @brief HTTP/2 server configuration
 * 
 * Configuration options specific to HTTP/2 protocol support.
 * HTTP/2 typically runs on port 443 (HTTPS) or 8443 for development.
 */
struct Http2Config {
    bool enabled = true;                      ///< Enable HTTP/2 support
    int port = 8443;                         ///< HTTP/2 listening port (typically HTTPS)
    std::string bindAddress = "0.0.0.0";    ///< IP address to bind to (0.0.0.0 for all interfaces)
};

/**
 * @brief General server configuration settings
 * 
 * Contains general server settings that apply to both HTTP/1.1 and HTTP/2.
 * These settings control connection limits, timeouts, logging, and threading.
 */
struct GeneralConfig {
    int maxConnections = 1000;                ///< Maximum concurrent connections
    std::chrono::seconds requestTimeout{30};  ///< Request timeout in seconds
    bool enableLogging = true;                ///< Enable request/response logging
    std::string logLevel = "info";           ///< Log level: debug, info, warn, error
    int workerThreads = 4;                   ///< Number of worker threads for request processing
};

/**
 * @brief Security configuration options
 * 
 * Comprehensive security settings including CORS, request size limits,
 * and rate limiting to protect against various attacks.
 */
struct SecurityConfig {
    bool enableCors = false;                                    ///< Enable Cross-Origin Resource Sharing
    std::vector<std::string> corsOrigins = {"*"};             ///< Allowed CORS origins
    int maxRequestSizeMb = 10;                                 ///< Maximum request body size in MB
    int maxHeaderSizeKb = 8;                                   ///< Maximum header size in KB
    bool rateLimitEnabled = false;                             ///< Enable rate limiting
    int rateLimitRequestsPerMinute = 100;                      ///< Maximum requests per minute per IP
};

/**
 * @brief Logging middleware configuration
 * 
 * Configuration for HTTP request/response logging middleware.
 * Supports various log formats compatible with common tools.
 */
struct LoggingMiddlewareConfig {
    bool enabled = true;                      ///< Enable logging middleware
    std::string format = "combined";         ///< Log format: combined, common, short
};

/**
 * @brief Compression middleware configuration
 * 
 * Settings for automatic response compression to reduce bandwidth usage.
 * Supports multiple compression algorithms with configurable thresholds.
 */
struct CompressionMiddlewareConfig {
    bool enabled = true;                                         ///< Enable compression middleware
    std::vector<std::string> algorithms = {"gzip", "deflate"};  ///< Supported compression algorithms
    int minSizeBytes = 1024;                                    ///< Minimum response size to compress
};

/**
 * @brief Static files middleware configuration
 * 
 * Configuration for serving static files directly from the filesystem.
 * Includes caching settings and index file handling.
 */
struct StaticFilesMiddlewareConfig {
    bool enabled = false;                                                    ///< Enable static file serving
    std::string rootDirectory = "/var/www/html";                           ///< Root directory for static files
    std::vector<std::string> indexFiles = {"index.html", "index.htm"};    ///< Default index files
    int cacheMaxAgeSeconds = 3600;                                         ///< Cache-Control max-age header value
};

/**
 * @brief Combined middleware configuration
 * 
 * Container for all middleware configurations. Middleware is processed
 * in the order: logging, compression, static files.
 */
struct MiddlewareConfig {
    LoggingMiddlewareConfig logging;          ///< Logging middleware settings
    CompressionMiddlewareConfig compression;  ///< Compression middleware settings
    StaticFilesMiddlewareConfig staticFiles; ///< Static files middleware settings
};

/**
 * @brief Database connection configuration
 * 
 * Configuration for database connectivity. Supports connection pooling
 * and various database types (PostgreSQL, MySQL, SQLite).
 */
struct DatabaseConfig {
    bool enabled = false;                     ///< Enable database connectivity
    std::string type = "postgresql";         ///< Database type: postgresql, mysql, sqlite
    std::string host = "localhost";          ///< Database server hostname
    int port = 5432;                        ///< Database server port
    std::string database = "qos_manager";    ///< Database name
    std::string username = "qos_user";       ///< Database username
    std::string password = "qos_password";   ///< Database password
    int poolSize = 10;                      ///< Connection pool size
};

/**
 * @brief Cache configuration settings
 * 
 * Configuration for caching layer (Redis, Memcached, etc.).
 * Provides configurable TTL and connection settings.
 */
struct CacheConfig {
    bool enabled = false;                     ///< Enable caching
    std::string type = "redis";              ///< Cache type: redis, memcached
    std::string host = "localhost";          ///< Cache server hostname
    int port = 6379;                        ///< Cache server port
    int ttlSeconds = 3600;                  ///< Default time-to-live in seconds
};

/**
 * @brief Application-level configuration
 * 
 * High-level application settings including name, version, environment,
 * and integration with external services like databases and caches.
 */
struct ApplicationConfig {
    std::string name = "cppSwitchboard Application";  ///< Application name
    std::string version = "1.0.0";                   ///< Application version
    std::string environment = "development";          ///< Environment: development, staging, production
    DatabaseConfig database;                          ///< Database configuration
    CacheConfig cache;                               ///< Cache configuration
};

/**
 * @brief Metrics collection configuration
 * 
 * Settings for exposing application metrics in Prometheus format.
 * Metrics include request counts, response times, and system resources.
 */
struct MetricsConfig {
    bool enabled = false;                     ///< Enable metrics collection
    std::string endpoint = "/metrics";       ///< Metrics endpoint path
    int port = 9090;                        ///< Metrics server port
};

/**
 * @brief Health check configuration
 * 
 * Configuration for health check endpoint used by load balancers
 * and monitoring systems to verify service availability.
 */
struct HealthCheckConfig {
    bool enabled = true;                      ///< Enable health check endpoint
    std::string endpoint = "/health";        ///< Health check endpoint path
};

/**
 * @brief Distributed tracing configuration
 * 
 * Settings for distributed tracing using Jaeger or similar systems.
 * Enables request tracing across microservices.
 */
struct TracingConfig {
    bool enabled = false;                                                        ///< Enable distributed tracing
    std::string serviceName = "cppSwitchboard-service";                        ///< Service name for tracing
    std::string jaegerEndpoint = "http://localhost:14268/api/traces";          ///< Jaeger collector endpoint
};

/**
 * @brief Debug logging configuration
 * 
 * Detailed debug logging configuration separate from normal application logging.
 * Provides granular control over what gets logged for debugging purposes.
 * 
 * @warning Debug logging can significantly impact performance and should be 
 *          disabled in production environments.
 */
struct DebugLoggingConfig {
    bool enabled = false;                     ///< Enable debug logging
    
    /**
     * @brief Header logging configuration
     * 
     * Controls logging of HTTP request and response headers.
     * Sensitive headers can be excluded for security.
     */
    struct {
        bool enabled = false;                                                                                    ///< Enable header logging
        bool logRequestHeaders = true;                                                                          ///< Log incoming request headers
        bool logResponseHeaders = true;                                                                         ///< Log outgoing response headers
        bool includeUrlDetails = true;                                                                          ///< Include full URL details
        std::vector<std::string> excludeHeaders = {"authorization", "cookie", "set-cookie"};                  ///< Headers to exclude from logging
    } headers;
    
    /**
     * @brief Payload logging configuration
     * 
     * Controls logging of HTTP request and response bodies.
     * Size limits and content type filters prevent logging of large/binary content.
     */
    struct {
        bool enabled = false;                                                                                   ///< Enable payload logging
        bool logRequestPayload = true;                                                                         ///< Log request body
        bool logResponsePayload = true;                                                                        ///< Log response body
        int maxPayloadSizeBytes = 1024;                                                                        ///< Maximum payload size to log
        std::vector<std::string> excludeContentTypes = {"image/", "video/", "audio/", "application/octet-stream"}; ///< Content types to exclude
    } payload;
    
    std::string outputFile;                   ///< Debug log output file (empty = stdout)
    std::string timestampFormat = "%Y-%m-%d %H:%M:%S";  ///< Timestamp format for debug logs
};

/**
 * @brief Comprehensive monitoring configuration
 * 
 * Container for all monitoring-related configurations including metrics,
 * health checks, tracing, and debug logging.
 */
struct MonitoringConfig {
    MetricsConfig metrics;                    ///< Metrics collection settings
    HealthCheckConfig healthCheck;            ///< Health check endpoint settings
    TracingConfig tracing;                   ///< Distributed tracing settings
    DebugLoggingConfig debugLogging;         ///< Debug logging settings
};

/**
 * @brief Complete server configuration structure
 * 
 * The main configuration structure that contains all server settings.
 * This is the primary configuration object used to initialize the HTTP server.
 * 
 * @code{.cpp}
 * ServerConfig config;
 * config.http1.port = 8080;
 * config.http2.port = 8443;
 * config.ssl.enabled = true;
 * config.general.maxConnections = 1000;
 * config.security.enableCors = true;
 * @endcode
 * 
 * @see ConfigLoader::loadFromFile()
 * @see ConfigValidator::validateConfig()
 */
struct ServerConfig {
    Http1Config http1;                        ///< HTTP/1.1 configuration
    Http2Config http2;                        ///< HTTP/2 configuration
    SslConfig ssl;                           ///< SSL/TLS configuration
    GeneralConfig general;                    ///< General server settings
    SecurityConfig security;                  ///< Security settings
    MiddlewareConfig middleware;              ///< Middleware configuration
    ApplicationConfig application;            ///< Application-level settings
    MonitoringConfig monitoring;              ///< Monitoring and observability settings
    
    // Legacy compatibility methods for backward compatibility
    int http1Port() const { return http1.port; }                              ///< @deprecated Use http1.port directly
    int http2Port() const { return http2.port; }                              ///< @deprecated Use http2.port directly
    std::string bindAddress() const { return http1.bindAddress; }             ///< @deprecated Use http1.bindAddress directly
    int maxConnections() const { return general.maxConnections; }             ///< @deprecated Use general.maxConnections directly
    std::chrono::seconds requestTimeout() const { return general.requestTimeout; } ///< @deprecated Use general.requestTimeout directly
    bool enableLogging() const { return general.enableLogging; }              ///< @deprecated Use general.enableLogging directly
};

/**
 * @brief Configuration validation utilities
 * 
 * Provides static methods to validate server configuration before use.
 * Validates port ranges, file paths, and configuration consistency.
 * 
 * @code{.cpp}
 * ServerConfig config;
 * std::string errorMessage;
 * if (!ConfigValidator::validateConfig(config, errorMessage)) {
 *     std::cerr << "Invalid configuration: " << errorMessage << std::endl;
 *     return false;
 * }
 * @endcode
 */
class ConfigValidator {
public:
    /**
     * @brief Validate complete server configuration
     * 
     * Performs comprehensive validation of all configuration settings
     * including port ranges, file paths, and logical consistency.
     * 
     * @param config The server configuration to validate
     * @param errorMessage Output parameter for error description
     * @return true if configuration is valid, false otherwise
     * 
     * @see validatePortRange()
     * @see validateSslConfig()
     * @see validateGeneralConfig()
     */
    static bool validateConfig(const ServerConfig& config, std::string& errorMessage);
    
private:
    /**
     * @brief Validate port number range
     * 
     * @param port Port number to validate
     * @param portType Description of port type for error messages
     * @param errorMessage Output parameter for error description
     * @return true if port is valid (1-65535), false otherwise
     */
    static bool validatePortRange(int port, const std::string& portType, std::string& errorMessage);
    
    /**
     * @brief Validate SSL configuration
     * 
     * @param ssl SSL configuration to validate
     * @param errorMessage Output parameter for error description
     * @return true if SSL config is valid, false otherwise
     */
    static bool validateSslConfig(const SslConfig& ssl, std::string& errorMessage);
    
    /**
     * @brief Validate general server configuration
     * 
     * @param general General configuration to validate
     * @param errorMessage Output parameter for error description
     * @return true if general config is valid, false otherwise
     */
    static bool validateGeneralConfig(const GeneralConfig& general, std::string& errorMessage);
};

/**
 * @brief Configuration loading utilities
 * 
 * Provides static methods to load server configuration from various sources
 * including YAML files, JSON strings, and environment variables.
 * 
 * @code{.cpp}
 * // Load from YAML file
 * auto config = ConfigLoader::loadFromFile("server.yaml");
 * if (!config) {
 *     std::cerr << "Failed to load configuration" << std::endl;
 *     return;
 * }
 * 
 * // Create default configuration
 * auto defaultConfig = ConfigLoader::createDefault();
 * @endcode
 */
class ConfigLoader {
public:
    /**
     * @brief Load configuration from YAML file
     * 
     * Loads server configuration from a YAML file with support for
     * environment variable substitution.
     * 
     * @param filename Path to YAML configuration file
     * @return Unique pointer to ServerConfig, nullptr on failure
     * 
     * @throws std::runtime_error if file cannot be read or parsed
     */
    static std::unique_ptr<ServerConfig> loadFromFile(const std::string& filename);
    
    /**
     * @brief Load configuration from YAML string
     * 
     * Parses server configuration from a YAML string with support for
     * environment variable substitution.
     * 
     * @param yamlContent YAML configuration content as string
     * @return Unique pointer to ServerConfig, nullptr on failure
     * 
     * @throws std::runtime_error if YAML cannot be parsed
     */
    static std::unique_ptr<ServerConfig> loadFromString(const std::string& yamlContent);
    
    /**
     * @brief Create default server configuration
     * 
     * Creates a new ServerConfig with sensible default values suitable
     * for development environments.
     * 
     * @return Unique pointer to ServerConfig with default values
     */
    static std::unique_ptr<ServerConfig> createDefault();
    
    /**
     * @brief Validate loaded configuration
     * 
     * Validates a loaded configuration for correctness.
     * This is a convenience method that delegates to ConfigValidator.
     * 
     * @param config Configuration to validate
     * @param errorMessage Output parameter for error description
     * @return true if configuration is valid, false otherwise
     * 
     * @see ConfigValidator::validateConfig()
     */
    static bool validateConfig(const ServerConfig& config, std::string& errorMessage);
    
private:
    /**
     * @brief Substitute environment variables in configuration values
     * 
     * Replaces ${VAR_NAME} patterns with environment variable values.
     * 
     * @param value String that may contain environment variable references
     * @return String with environment variables substituted
     */
    static std::string substituteEnvironmentVariables(const std::string& value);
};

} // namespace cppSwitchboard 