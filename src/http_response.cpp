#include <cppSwitchboard/http_response.h>
#include <algorithm>

namespace cppSwitchboard {

HttpResponse::HttpResponse(int status) : status_(status) {
}

std::string HttpResponse::getHeader(const std::string& name) const {
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

void HttpResponse::setHeader(const std::string& name, const std::string& value) {
    headers_[name] = value;
}

void HttpResponse::removeHeader(const std::string& name) {
    headers_.erase(name);
}

void HttpResponse::setContentType(const std::string& contentType) {
    setHeader("Content-Type", contentType);
}

std::string HttpResponse::getContentType() const {
    return getHeader("Content-Type");
}

void HttpResponse::setBody(const std::string& body) {
    body_ = body;
    updateContentLength();
}

void HttpResponse::setBody(const std::vector<uint8_t>& body) {
    body_ = std::string(body.begin(), body.end());
    updateContentLength();
}

void HttpResponse::appendBody(const std::string& data) {
    body_ += data;
    updateContentLength();
}

void HttpResponse::updateContentLength() {
    setHeader("Content-Length", std::to_string(body_.length()));
}

// Static convenience methods
HttpResponse HttpResponse::ok(const std::string& body, const std::string& contentType) {
    HttpResponse response(OK);
    response.setContentType(contentType);
    response.setBody(body);
    return response;
}

HttpResponse HttpResponse::json(const std::string& jsonBody) {
    HttpResponse response(OK);
    response.setContentType("application/json");
    response.setBody(jsonBody);
    return response;
}

HttpResponse HttpResponse::html(const std::string& htmlBody) {
    HttpResponse response(OK);
    response.setContentType("text/html; charset=utf-8");
    response.setBody(htmlBody);
    return response;
}

HttpResponse HttpResponse::notFound(const std::string& message) {
    HttpResponse response(NOT_FOUND);
    response.setContentType("application/json");
    response.setBody("{\"error\": \"" + message + "\"}");
    return response;
}

HttpResponse HttpResponse::badRequest(const std::string& message) {
    HttpResponse response(BAD_REQUEST);
    response.setContentType("application/json");
    response.setBody("{\"error\": \"" + message + "\"}");
    return response;
}

HttpResponse HttpResponse::internalServerError(const std::string& message) {
    HttpResponse response(INTERNAL_SERVER_ERROR);
    response.setContentType("application/json");
    response.setBody("{\"error\": \"" + message + "\"}");
    return response;
}

HttpResponse HttpResponse::methodNotAllowed(const std::string& message) {
    HttpResponse response(METHOD_NOT_ALLOWED);
    response.setContentType("application/json");
    response.setBody("{\"error\": \"" + message + "\"}");
    return response;
}

} // namespace cppSwitchboard 