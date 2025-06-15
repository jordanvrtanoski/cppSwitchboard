#include <cppSwitchboard/http_handler.h>
#include <iostream>

namespace cppSwitchboard {

HttpResponse DefaultErrorHandler::handleError(const HttpRequest& request, const std::exception& error) {
    std::cerr << "Error handling request " << request.getMethod() << " " << request.getPath() 
              << ": " << error.what() << std::endl;
    
    HttpResponse response;
    response.setStatus(HttpResponse::INTERNAL_SERVER_ERROR);
    response.setContentType("application/json");
    response.setBody("{\"error\": \"Internal Server Error\", \"message\": \"" + std::string(error.what()) + "\"}");
    
    return response;
}

std::shared_ptr<HttpHandler> makeHandler(std::function<HttpResponse(const HttpRequest&)> handler) {
    return std::make_shared<FunctionHandler>(std::move(handler));
}

std::shared_ptr<AsyncHttpHandler> makeAsyncHandler(std::function<void(const HttpRequest&, AsyncHttpHandler::ResponseCallback)> handler) {
    return std::make_shared<AsyncFunctionHandler>(std::move(handler));
}

} // namespace cppSwitchboard 