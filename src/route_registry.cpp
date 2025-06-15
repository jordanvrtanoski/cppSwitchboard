#include <cppSwitchboard/route_registry.h>
#include <cppSwitchboard/middleware_pipeline.h>
#include <algorithm>
#include <sstream>

namespace cppSwitchboard {

void RouteInfo::compilePattern() {
    // Extract parameter names from pattern like /users/{id}/posts/{postId}
    std::regex paramRegex("\\{([^}]+)\\}");
    std::sregex_iterator iter(pattern.begin(), pattern.end(), paramRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        paramNames.push_back((*iter)[1].str());
    }
    
    // Convert pattern to regex
    std::string regexPattern = pattern;
    
    // Replace {param} with ([^/]+) for regex matching
    regexPattern = std::regex_replace(regexPattern, std::regex("\\{[^}]+\\}"), "([^/]+)");
    
    // Handle wildcard * at the end
    if (!regexPattern.empty() && regexPattern.back() == '*') {
        regexPattern = regexPattern.substr(0, regexPattern.length() - 1) + "(.*)";
        paramNames.push_back("*"); // Special parameter for wildcard
    }
    
    // Ensure exact match
    regexPattern = "^" + regexPattern + "$";
    
    try {
        pathRegex = std::regex(regexPattern);
    } catch (const std::exception& e) {
        // Fallback to exact string match if regex fails
        pathRegex = std::regex("^" + std::regex_replace(pattern, std::regex("([.^$*+?()\\[\\]{}|\\\\])"), "\\$1") + "$");
    }
}

void RouteRegistry::registerRoute(const std::string& path, HttpMethod method, std::shared_ptr<HttpHandler> handler) {
    // Remove existing route with same path and method
    removeRoute(path, method);
    
    routes_.emplace_back(path, method, handler);
}

void RouteRegistry::registerAsyncRoute(const std::string& path, HttpMethod method, std::shared_ptr<AsyncHttpHandler> handler) {
    // Remove existing route with same path and method
    removeRoute(path, method);
    
    routes_.emplace_back(path, method, handler);
}

void RouteRegistry::registerRouteWithMiddleware(const std::string& path, HttpMethod method, std::shared_ptr<MiddlewarePipeline> pipeline) {
    if (!pipeline) {
        throw std::invalid_argument("Middleware pipeline cannot be null");
    }
    
    if (!pipeline->hasFinalHandler()) {
        throw std::runtime_error("Middleware pipeline must have a final handler set before registration");
    }
    
    // Remove existing route with same path and method
    removeRoute(path, method);
    
    routes_.emplace_back(path, method, pipeline);
}

RouteMatch RouteRegistry::findRoute(const std::string& path, HttpMethod method) const {
    RouteMatch match;
    
    for (const auto& route : routes_) {
        if (route.method == method) {
            std::map<std::string, std::string> pathParams;
            if (matchPath(route, path, pathParams)) {
                match.matched = true;
                match.pathParams = pathParams;
                match.handler = route.handler;
                match.asyncHandler = route.asyncHandler;
                match.middlewarePipeline = route.middlewarePipeline;
                match.isAsync = route.isAsync;
                match.hasMiddleware = route.hasMiddleware;
                break;
            }
        }
    }
    
    return match;
}

RouteMatch RouteRegistry::findRoute(const HttpRequest& request) const {
    return findRoute(request.getPath(), request.getHttpMethod());
}

bool RouteRegistry::hasRoute(const std::string& path, HttpMethod method) const {
    return findRoute(path, method).matched;
}

void RouteRegistry::removeRoute(const std::string& path, HttpMethod method) {
    routes_.erase(
        std::remove_if(routes_.begin(), routes_.end(),
            [&](const RouteInfo& route) {
                return route.pattern == path && route.method == method;
            }),
        routes_.end()
    );
}

void RouteRegistry::clear() {
    routes_.clear();
}

std::string RouteRegistry::pathToRegex(const std::string& path, std::vector<std::string>& paramNames) const {
    std::string regexPattern = path;
    
    // Extract parameter names
    std::regex paramRegex("\\{([^}]+)\\}");
    std::sregex_iterator iter(path.begin(), path.end(), paramRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        paramNames.push_back((*iter)[1].str());
    }
    
    // Replace {param} with ([^/]+) for regex matching
    regexPattern = std::regex_replace(regexPattern, std::regex("\\{[^}]+\\}"), "([^/]+)");
    
    // Handle wildcard * at the end
    if (!regexPattern.empty() && regexPattern.back() == '*') {
        regexPattern = regexPattern.substr(0, regexPattern.length() - 1) + "(.*)";
        paramNames.push_back("*"); // Special parameter for wildcard
    }
    
    // Ensure exact match
    regexPattern = "^" + regexPattern + "$";
    
    return regexPattern;
}

bool RouteRegistry::matchPath(const RouteInfo& route, const std::string& path, std::map<std::string, std::string>& pathParams) const {
    std::smatch matches;
    
    try {
        if (std::regex_match(path, matches, route.pathRegex)) {
            // Extract path parameters
            for (size_t i = 1; i < matches.size() && i - 1 < route.paramNames.size(); ++i) {
                pathParams[route.paramNames[i - 1]] = matches[i].str();
            }
            return true;
        }
    } catch (const std::exception& e) {
        // Fallback to exact string match
        return route.pattern == path;
    }
    
    return false;
}

} // namespace cppSwitchboard 