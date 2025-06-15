#include <cppSwitchboard/http_request.h>
#include <algorithm>
#include <sstream>

namespace cppSwitchboard {

HttpRequest::HttpRequest(const std::string& method, const std::string& path, const std::string& protocol)
    : method_(method), path_(path), protocol_(protocol) {
    updateHttpMethod();
    
    // Parse query string if present
    size_t queryPos = path_.find('?');
    if (queryPos != std::string::npos) {
        std::string actualPath = path_.substr(0, queryPos);
        std::string queryString = path_.substr(queryPos + 1);
        path_ = actualPath;
        parseQueryString(queryString);
    }
}

std::string HttpRequest::getHeader(const std::string& name) const {
    auto it = headers_.find(name);
    if (it != headers_.end()) {
        return it->second;
    }
    
    // Try case-insensitive search
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    for (const auto& header : headers_) {
        std::string lowerHeaderName = header.first;
        std::transform(lowerHeaderName.begin(), lowerHeaderName.end(), lowerHeaderName.begin(), ::tolower);
        if (lowerHeaderName == lowerName) {
            return header.second;
        }
    }
    
    return "";
}

void HttpRequest::setHeader(const std::string& name, const std::string& value) {
    headers_[name] = value;
}

void HttpRequest::setBody(const std::vector<uint8_t>& body) {
    body_ = std::string(body.begin(), body.end());
}

std::string HttpRequest::getQueryParam(const std::string& name) const {
    auto it = queryParams_.find(name);
    return (it != queryParams_.end()) ? it->second : "";
}

void HttpRequest::setQueryParam(const std::string& name, const std::string& value) {
    queryParams_[name] = value;
}

std::string HttpRequest::getPathParam(const std::string& name) const {
    auto it = pathParams_.find(name);
    return (it != pathParams_.end()) ? it->second : "";
}

void HttpRequest::setPathParam(const std::string& name, const std::string& value) {
    pathParams_[name] = value;
}

std::string HttpRequest::getContentType() const {
    return getHeader("Content-Type");
}

bool HttpRequest::isJson() const {
    std::string contentType = getContentType();
    return contentType.find("application/json") != std::string::npos;
}

bool HttpRequest::isFormData() const {
    std::string contentType = getContentType();
    return contentType.find("application/x-www-form-urlencoded") != std::string::npos ||
           contentType.find("multipart/form-data") != std::string::npos;
}

void HttpRequest::parseQueryString(const std::string& queryString) {
    std::istringstream iss(queryString);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t equalPos = pair.find('=');
        if (equalPos != std::string::npos) {
            std::string key = pair.substr(0, equalPos);
            std::string value = pair.substr(equalPos + 1);
            
            // URL decode (basic implementation)
            // TODO: Implement proper URL decoding
            queryParams_[key] = value;
        }
    }
}

HttpMethod HttpRequest::stringToMethod(const std::string& method) {
    if (method == "GET") return HttpMethod::GET;
    if (method == "POST") return HttpMethod::POST;
    if (method == "PUT") return HttpMethod::PUT;
    if (method == "DELETE") return HttpMethod::DELETE;
    if (method == "PATCH") return HttpMethod::PATCH;
    if (method == "HEAD") return HttpMethod::HEAD;
    if (method == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::GET; // Default
}

std::string HttpRequest::methodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        default: return "GET";
    }
}

void HttpRequest::updateHttpMethod() {
    httpMethod_ = stringToMethod(method_);
}

} // namespace cppSwitchboard 