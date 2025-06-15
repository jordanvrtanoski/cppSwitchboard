/**
 * @file route_registry.h
 * @brief URL routing and pattern matching for cppSwitchboard library
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @version 1.0.0
 * @date 2024
 * 
 * This file contains the RouteRegistry class and related structures for handling
 * URL routing and pattern matching in the cppSwitchboard library. It provides
 * comprehensive support for RESTful routing with parameter extraction, both
 * synchronous and asynchronous handler registration, and flexible pattern matching.
 */

#pragma once

#include <cppSwitchboard/http_handler.h>
#include <cppSwitchboard/http_request.h>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <regex>

// Forward declaration for middleware pipeline support
namespace cppSwitchboard {
    class MiddlewarePipeline;
}

namespace cppSwitchboard {

/**
 * @struct RouteInfo
 * @brief Information about a registered route
 * 
 * The RouteInfo structure contains all information about a registered route,
 * including the URL pattern, HTTP method, associated handler, compiled regex
 * pattern, and parameter names for RESTful parameter extraction.
 * 
 * @see RouteRegistry, RouteMatch
 * @since 1.0.0
 */
struct RouteInfo {
    std::string pattern;                                       ///< Original URL pattern (e.g., "/users/{id}")
    HttpMethod method;                                         ///< HTTP method for this route
    std::shared_ptr<HttpHandler> handler;                     ///< Synchronous handler (if not async)
    std::shared_ptr<AsyncHttpHandler> asyncHandler;           ///< Asynchronous handler (if async)
    std::shared_ptr<MiddlewarePipeline> middlewarePipeline;   ///< Middleware pipeline (if middleware enabled)
    std::regex pathRegex;                                     ///< Compiled regex pattern for matching
    std::vector<std::string> paramNames;                      ///< Names of path parameters
    bool isAsync = false;                                     ///< Whether this route uses async handler
    bool hasMiddleware = false;                               ///< Whether this route has middleware
    
    /**
     * @brief Constructor for synchronous route
     * @param p URL pattern (e.g., "/api/users/{id}")
     * @param m HTTP method
     * @param h Synchronous handler
     * 
     * Creates a RouteInfo for a synchronous handler. The pattern will be
     * compiled into a regex for efficient matching.
     * 
     * @code{.cpp}
     * auto handler = std::make_shared<UserHandler>();
     * RouteInfo route("/api/users/{id}", HttpMethod::GET, handler);
     * @endcode
     */
    RouteInfo(const std::string& p, HttpMethod m, std::shared_ptr<HttpHandler> h)
        : pattern(p), method(m), handler(h), isAsync(false) {
        compilePattern();
    }
    
    /**
     * @brief Constructor for asynchronous route
     * @param p URL pattern (e.g., "/api/async/{id}")
     * @param m HTTP method
     * @param h Asynchronous handler
     * 
     * Creates a RouteInfo for an asynchronous handler. The pattern will be
     * compiled into a regex for efficient matching.
     * 
     * @code{.cpp}
     * auto asyncHandler = std::make_shared<AsyncUserHandler>();
     * RouteInfo route("/api/async/{id}", HttpMethod::POST, asyncHandler);
     * @endcode
     */
    RouteInfo(const std::string& p, HttpMethod m, std::shared_ptr<AsyncHttpHandler> h)
        : pattern(p), method(m), asyncHandler(h), isAsync(true) {
        compilePattern();
    }
    
    /**
     * @brief Constructor for route with middleware pipeline
     * @param p URL pattern (e.g., "/api/protected/{id}")
     * @param m HTTP method
     * @param pipeline Middleware pipeline for this route
     * 
     * Creates a RouteInfo with a middleware pipeline. The pipeline should
     * have a final handler set before the route is registered.
     * 
     * @code{.cpp}
     * auto pipeline = std::make_shared<MiddlewarePipeline>();
     * pipeline->addMiddleware(std::make_shared<AuthMiddleware>());
     * pipeline->setFinalHandler(std::make_shared<UserHandler>());
     * RouteInfo route("/api/protected/{id}", HttpMethod::GET, pipeline);
     * @endcode
     */
    RouteInfo(const std::string& p, HttpMethod m, std::shared_ptr<MiddlewarePipeline> pipeline)
        : pattern(p), method(m), middlewarePipeline(pipeline), hasMiddleware(true) {
        compilePattern();
    }
    
private:
    /**
     * @brief Compile the URL pattern into a regex
     * 
     * Internal method that converts the URL pattern with parameters
     * (e.g., "/users/{id}") into a compiled regex pattern for efficient
     * matching and parameter extraction.
     */
    void compilePattern();
};

/**
 * @struct RouteMatch
 * @brief Result of route matching operation
 * 
 * The RouteMatch structure contains the result of attempting to match
 * a request URL against registered routes. It includes whether a match
 * was found, extracted path parameters, and the associated handler.
 * 
 * @see RouteRegistry, RouteInfo
 * @since 1.0.0
 */
struct RouteMatch {
    bool matched = false;                                       ///< Whether a matching route was found
    std::map<std::string, std::string> pathParams;            ///< Extracted path parameters
    std::shared_ptr<HttpHandler> handler;                     ///< Matched synchronous handler (if not async)
    std::shared_ptr<AsyncHttpHandler> asyncHandler;           ///< Matched asynchronous handler (if async)
    std::shared_ptr<MiddlewarePipeline> middlewarePipeline;   ///< Matched middleware pipeline (if middleware enabled)
    bool isAsync = false;                                      ///< Whether the matched route is async
    bool hasMiddleware = false;                               ///< Whether the matched route has middleware
};

/**
 * @class RouteRegistry
 * @brief URL routing and pattern matching system
 * 
 * The RouteRegistry class provides a comprehensive URL routing system with
 * support for RESTful parameter extraction, both synchronous and asynchronous
 * handlers, and flexible pattern matching. It allows registration of routes
 * with URL patterns and efficient lookup of handlers based on incoming requests.
 * 
 * Features:
 * - RESTful URL patterns with parameter extraction (e.g., "/users/{id}")
 * - Support for all HTTP methods (GET, POST, PUT, DELETE, etc.)
 * - Both synchronous and asynchronous handler registration
 * - Efficient regex-based pattern matching
 * - Route introspection and management
 * - Parameter extraction and validation
 * 
 * @code{.cpp}
 * RouteRegistry registry;
 * 
 * // Register synchronous routes
 * registry.registerRoute("/api/users", HttpMethod::GET, userListHandler);
 * registry.registerRoute("/api/users/{id}", HttpMethod::GET, userDetailHandler);
 * 
 * // Register asynchronous routes
 * registry.registerAsyncRoute("/api/async/{id}", HttpMethod::POST, asyncHandler);
 * 
 * // Find matching route
 * auto match = registry.findRoute("/api/users/123", HttpMethod::GET);
 * if (match.matched) {
 *     std::string userId = match.pathParams["id"]; // "123"
 *     auto response = match.handler->handle(request);
 * }
 * @endcode
 * 
 * @see HttpHandler, AsyncHttpHandler, RouteInfo, RouteMatch
 * @since 1.0.0
 */
class RouteRegistry {
public:
    /**
     * @brief Default constructor
     * 
     * Creates an empty route registry with no registered routes.
     */
    RouteRegistry() = default;
    
    // Register synchronous handlers
    
    /**
     * @brief Register a synchronous route handler
     * @param path URL pattern (supports parameters like "/users/{id}")
     * @param method HTTP method for this route
     * @param handler Synchronous handler to process matching requests
     * @throws std::invalid_argument if path is invalid or handler is null
     * 
     * Registers a synchronous handler for the specified URL pattern and HTTP method.
     * The path can include parameters in curly braces which will be extracted
     * and made available to the handler.
     * 
     * @code{.cpp}
     * auto userHandler = std::make_shared<UserHandler>();
     * registry.registerRoute("/api/users/{id}", HttpMethod::GET, userHandler);
     * registry.registerRoute("/api/users", HttpMethod::POST, userHandler);
     * @endcode
     * 
     * @see HttpHandler, HttpMethod
     */
    void registerRoute(const std::string& path, HttpMethod method, std::shared_ptr<HttpHandler> handler);
    
    // Register asynchronous handlers
    
    /**
     * @brief Register an asynchronous route handler
     * @param path URL pattern (supports parameters like "/users/{id}")
     * @param method HTTP method for this route
     * @param handler Asynchronous handler to process matching requests
     * @throws std::invalid_argument if path is invalid or handler is null
     * 
     * Registers an asynchronous handler for the specified URL pattern and HTTP method.
     * Async handlers are useful for long-running operations or when integrating
     * with asynchronous frameworks.
     * 
     * @code{.cpp}
     * auto asyncHandler = std::make_shared<AsyncUserHandler>();
     * registry.registerAsyncRoute("/api/async/{id}", HttpMethod::POST, asyncHandler);
     * @endcode
     * 
     * @see AsyncHttpHandler, HttpMethod
     */
    void registerAsyncRoute(const std::string& path, HttpMethod method, std::shared_ptr<AsyncHttpHandler> handler);
    
    // Register routes with middleware
    
    /**
     * @brief Register a route with middleware pipeline
     * @param path URL pattern (supports parameters like "/users/{id}")
     * @param method HTTP method for this route
     * @param pipeline Middleware pipeline with final handler set
     * @throws std::invalid_argument if path is invalid or pipeline is null
     * @throws std::runtime_error if pipeline has no final handler
     * 
     * Registers a route that will be processed through the specified middleware
     * pipeline. The pipeline must have a final handler set before registration.
     * 
     * @code{.cpp}
     * auto pipeline = std::make_shared<MiddlewarePipeline>();
     * pipeline->addMiddleware(std::make_shared<AuthMiddleware>());
     * pipeline->addMiddleware(std::make_shared<LoggingMiddleware>());
     * pipeline->setFinalHandler(std::make_shared<UserHandler>());
     * registry.registerRouteWithMiddleware("/api/users/{id}", HttpMethod::GET, pipeline);
     * @endcode
     * 
     * @see MiddlewarePipeline, Middleware
     */
    void registerRouteWithMiddleware(const std::string& path, HttpMethod method, std::shared_ptr<MiddlewarePipeline> pipeline);
    
    // Find matching route
    
    /**
     * @brief Find a matching route for the given path and method
     * @param path Request URL path
     * @param method HTTP method
     * @return RouteMatch containing match result and extracted parameters
     * 
     * Searches for a registered route that matches the given path and HTTP method.
     * If a match is found, path parameters are extracted and returned in the
     * RouteMatch structure along with the associated handler.
     * 
     * @code{.cpp}
     * auto match = registry.findRoute("/api/users/123", HttpMethod::GET);
     * if (match.matched) {
     *     std::string userId = match.pathParams["id"]; // "123"
     *     // Process with match.handler
     * }
     * @endcode
     * 
     * @see RouteMatch
     */
    RouteMatch findRoute(const std::string& path, HttpMethod method) const;
    
    /**
     * @brief Find a matching route from an HttpRequest object
     * @param request HTTP request containing path and method
     * @return RouteMatch containing match result and extracted parameters
     * 
     * Convenience method that extracts the path and method from an HttpRequest
     * object and finds the matching route. This is equivalent to calling
     * findRoute(request.getPath(), request.getHttpMethod()).
     * 
     * @code{.cpp}
     * HttpRequest request("GET", "/api/users/123", "HTTP/1.1");
     * auto match = registry.findRoute(request);
     * if (match.matched) {
     *     // Process the matched route
     * }
     * @endcode
     * 
     * @see HttpRequest, RouteMatch
     */
    RouteMatch findRoute(const HttpRequest& request) const;
    
    /**
     * @brief Check if a route exists for the given path and method
     * @param path URL path to check
     * @param method HTTP method to check
     * @return True if a matching route exists, false otherwise
     * 
     * Checks whether a route is registered for the specified path and HTTP method
     * without performing the full matching operation. This is useful for
     * validation or introspection purposes.
     * 
     * @code{.cpp}
     * if (registry.hasRoute("/api/users/123", HttpMethod::GET)) {
     *     // Route exists
     * }
     * @endcode
     */
    bool hasRoute(const std::string& path, HttpMethod method) const;
    
    /**
     * @brief Get all registered routes
     * @return Vector of all RouteInfo objects
     * 
     * Returns a copy of all registered routes for debugging, introspection,
     * or administrative purposes. This can be used to generate route
     * documentation or validate route configurations.
     * 
     * @code{.cpp}
     * auto routes = registry.getAllRoutes();
     * for (const auto& route : routes) {
     *     std::cout << route.pattern << " " << methodToString(route.method) << std::endl;
     * }
     * @endcode
     * 
     * @see RouteInfo
     */
    std::vector<RouteInfo> getAllRoutes() const { return routes_; }
    
    /**
     * @brief Remove a registered route
     * @param path URL pattern to remove
     * @param method HTTP method to remove
     * 
     * Removes the route registration for the specified path and HTTP method.
     * If no matching route is found, this method has no effect.
     * 
     * @code{.cpp}
     * registry.removeRoute("/api/users/{id}", HttpMethod::DELETE);
     * @endcode
     */
    void removeRoute(const std::string& path, HttpMethod method);
    
    /**
     * @brief Clear all registered routes
     * 
     * Removes all route registrations from the registry. This is useful
     * for testing or when reconfiguring the routing system.
     * 
     * @code{.cpp}
     * registry.clear(); // Remove all routes
     * @endcode
     */
    void clear();

private:
    std::vector<RouteInfo> routes_;                    ///< List of registered routes
    
    // Helper methods
    
    /**
     * @brief Convert URL pattern to regex string
     * @param path URL pattern with parameters (e.g., "/users/{id}")
     * @param paramNames Output vector to store parameter names
     * @return Regex pattern string for matching
     * 
     * Internal helper method that converts a URL pattern with parameters
     * into a regex pattern string. Parameter names are extracted and
     * stored in the paramNames vector.
     */
    std::string pathToRegex(const std::string& path, std::vector<std::string>& paramNames) const;
    
    /**
     * @brief Match a path against a route and extract parameters
     * @param route Route information to match against
     * @param path Request path to match
     * @param pathParams Output map to store extracted parameters
     * @return True if the path matches the route pattern
     * 
     * Internal helper method that performs the actual pattern matching
     * and parameter extraction for a specific route.
     */
    bool matchPath(const RouteInfo& route, const std::string& path, std::map<std::string, std::string>& pathParams) const;
};

} // namespace cppSwitchboard 