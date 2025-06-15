/**
 * @file http_handler.h
 * @brief HTTP request handler interfaces and implementations for cppSwitchboard
 * @author Jordan Vrtanoski <jordan.vrtanoski@gmail.com>
 * @date 2025-06-14
 * @version 1.2.0
 * 
 * This file defines the core handler interfaces and implementations for processing
 * HTTP requests in the cppSwitchboard framework. It provides both synchronous and
 * asynchronous handler interfaces, along with middleware and error handling capabilities.
 * 
 * @section handler_example Handler Usage Example
 * @code{.cpp}
 * // Synchronous handler
 * auto handler = cppSwitchboard::makeHandler([](const HttpRequest& request) {
 *     HttpResponse response;
 *     response.setStatusCode(200);
 *     response.setBody("Hello, World!");
 *     return response;
 * });
 * 
 * // Asynchronous handler
 * auto asyncHandler = cppSwitchboard::makeAsyncHandler([](const HttpRequest& request, auto callback) {
 *     // Perform async operation
 *     std::thread([request, callback]() {
 *         HttpResponse response;
 *         response.setStatusCode(200);
 *         response.setBody("Async response");
 *         callback(response);
 *     }).detach();
 * });
 * @endcode
 * 
 * @see HttpHandler
 * @see AsyncHttpHandler
 * @see Middleware
 * @see ErrorHandler
 */
#pragma once

#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <functional>
#include <memory>

namespace cppSwitchboard {

/**
 * @brief Base class for synchronous HTTP request handlers
 * 
 * This abstract base class defines the interface for handling HTTP requests
 * synchronously. Implementations should process the request and return a response
 * immediately.
 * 
 * @code{.cpp}
 * class MyHandler : public HttpHandler {
 * public:
 *     HttpResponse handle(const HttpRequest& request) override {
 *         HttpResponse response;
 *         response.setStatusCode(200);
 *         response.setBody("Hello from MyHandler!");
 *         return response;
 *     }
 * };
 * @endcode
 * 
 * @see AsyncHttpHandler
 * @see FunctionHandler
 */
class HttpHandler {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~HttpHandler() = default;
    
    /**
     * @brief Handle an HTTP request synchronously
     * 
     * This method processes an HTTP request and returns a response immediately.
     * The implementation should be thread-safe as it may be called from multiple
     * threads concurrently.
     * 
     * @param request The HTTP request to process
     * @return HttpResponse The HTTP response to send back to the client
     * 
     * @note This method should not block for long periods as it may impact
     *       server performance. For long-running operations, use AsyncHttpHandler.
     */
    virtual HttpResponse handle(const HttpRequest& request) = 0;
};

/**
 * @brief Base class for asynchronous HTTP request handlers
 * 
 * This abstract base class defines the interface for handling HTTP requests
 * asynchronously. Implementations should process the request in a non-blocking
 * manner and invoke the callback when the response is ready.
 * 
 * @code{.cpp}
 * class MyAsyncHandler : public AsyncHttpHandler {
 * public:
 *     void handleAsync(const HttpRequest& request, ResponseCallback callback) override {
 *         // Start async operation
 *         std::thread([request, callback]() {
 *             // Simulate async work
 *             std::this_thread::sleep_for(std::chrono::milliseconds(100));
 *             
 *             HttpResponse response;
 *             response.setStatusCode(200);
 *             response.setBody("Async response");
 *             callback(response);
 *         }).detach();
 *     }
 * };
 * @endcode
 * 
 * @see HttpHandler
 * @see AsyncFunctionHandler
 */
class AsyncHttpHandler {
public:
    /**
     * @brief Callback function type for asynchronous responses
     * 
     * This callback is invoked when an asynchronous operation completes
     * and the response is ready to be sent to the client.
     */
    using ResponseCallback = std::function<void(const HttpResponse&)>;
    
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~AsyncHttpHandler() = default;
    
    /**
     * @brief Handle an HTTP request asynchronously
     * 
     * This method initiates asynchronous processing of an HTTP request.
     * The implementation should return immediately and invoke the callback
     * when the response is ready.
     * 
     * @param request The HTTP request to process
     * @param callback Function to call when the response is ready
     * 
     * @note The callback must be invoked exactly once for each call to handleAsync.
     *       The callback is thread-safe and can be called from any thread.
     */
    virtual void handleAsync(const HttpRequest& request, ResponseCallback callback) = 0;
};

/**
 * @brief Function-based synchronous handler wrapper
 * 
 * This class wraps a function or lambda into an HttpHandler interface.
 * It provides a convenient way to create handlers without defining new classes.
 * 
 * @code{.cpp}
 * auto myFunction = [](const HttpRequest& request) {
 *     HttpResponse response;
 *     response.setStatusCode(200);
 *     response.setBody("Function handler response");
 *     return response;
 * };
 * 
 * FunctionHandler handler(myFunction);
 * HttpResponse response = handler.handle(request);
 * @endcode
 * 
 * @see HttpHandler
 * @see makeHandler()
 */
class FunctionHandler : public HttpHandler {
public:
    /**
     * @brief Function type for handler functions
     * 
     * The function should take an HttpRequest and return an HttpResponse.
     */
    using HandlerFunction = std::function<HttpResponse(const HttpRequest&)>;
    
    /**
     * @brief Construct a function handler
     * 
     * @param handler The function to wrap as a handler
     * 
     * @throws std::invalid_argument if handler is null
     */
    explicit FunctionHandler(HandlerFunction handler) : handler_(std::move(handler)) {}
    
    /**
     * @brief Handle the request using the wrapped function
     * 
     * @param request The HTTP request to process
     * @return HttpResponse The response from the wrapped function
     */
    HttpResponse handle(const HttpRequest& request) override {
        return handler_(request);
    }

private:
    HandlerFunction handler_;  ///< The wrapped handler function
};

/**
 * @brief Function-based asynchronous handler wrapper
 * 
 * This class wraps a function or lambda into an AsyncHttpHandler interface.
 * It provides a convenient way to create async handlers without defining new classes.
 * 
 * @code{.cpp}
 * auto myAsyncFunction = [](const HttpRequest& request, auto callback) {
 *     // Start async operation
 *     std::async(std::launch::async, [request, callback]() {
 *         HttpResponse response;
 *         response.setStatusCode(200);
 *         response.setBody("Async function response");
 *         callback(response);
 *     });
 * };
 * 
 * AsyncFunctionHandler handler(myAsyncFunction);
 * @endcode
 * 
 * @see AsyncHttpHandler
 * @see makeAsyncHandler()
 */
class AsyncFunctionHandler : public AsyncHttpHandler {
public:
    /**
     * @brief Function type for async handler functions
     * 
     * The function should take an HttpRequest and a ResponseCallback.
     * It should invoke the callback when the response is ready.
     */
    using AsyncHandlerFunction = std::function<void(const HttpRequest&, ResponseCallback)>;
    
    /**
     * @brief Construct an async function handler
     * 
     * @param handler The async function to wrap as a handler
     * 
     * @throws std::invalid_argument if handler is null
     */
    explicit AsyncFunctionHandler(AsyncHandlerFunction handler) : handler_(std::move(handler)) {}
    
    /**
     * @brief Handle the request using the wrapped async function
     * 
     * @param request The HTTP request to process
     * @param callback Function to call when the response is ready
     */
    void handleAsync(const HttpRequest& request, ResponseCallback callback) override {
        handler_(request, callback);
    }

private:
    AsyncHandlerFunction handler_;  ///< The wrapped async handler function
};

// Middleware interface moved to middleware.h

/**
 * @brief Error handler interface for handling exceptions
 * 
 * Error handlers are responsible for converting exceptions into appropriate
 * HTTP responses. They provide a way to centralize error handling logic
 * and ensure consistent error responses.
 * 
 * @code{.cpp}
 * class CustomErrorHandler : public ErrorHandler {
 * public:
 *     HttpResponse handleError(const HttpRequest& request, const std::exception& error) override {
 *         HttpResponse response;
 *         response.setStatusCode(500);
 *         response.setBody("Internal Server Error: " + std::string(error.what()));
 *         response.setHeader("Content-Type", "text/plain");
 *         return response;
 *     }
 * };
 * @endcode
 * 
 * @see DefaultErrorHandler
 */
class ErrorHandler {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~ErrorHandler() = default;
    
    /**
     * @brief Handle an exception and generate an error response
     * 
     * This method is called when an exception occurs during request processing.
     * It should generate an appropriate HTTP response based on the exception.
     * 
     * @param request The HTTP request that caused the error
     * @param error The exception that was thrown
     * @return HttpResponse The error response to send to the client
     * 
     * @note This method should not throw exceptions. If it does, a generic
     *       500 error response will be generated.
     */
    virtual HttpResponse handleError(const HttpRequest& request, const std::exception& error) = 0;
};

/**
 * @brief Default error handler implementation
 * 
 * Provides a basic error handler that generates appropriate HTTP responses
 * for common exception types. This can be used as a fallback error handler
 * or as a base class for custom error handlers.
 * 
 * The default handler generates:
 * - 400 Bad Request for invalid_argument exceptions
 * - 404 Not Found for out_of_range exceptions
 * - 500 Internal Server Error for all other exceptions
 * 
 * @see ErrorHandler
 */
class DefaultErrorHandler : public ErrorHandler {
public:
    /**
     * @brief Handle an exception using default error mapping
     * 
     * @param request The HTTP request that caused the error
     * @param error The exception that was thrown
     * @return HttpResponse The appropriate error response
     */
    HttpResponse handleError(const HttpRequest& request, const std::exception& error) override;
};

/**
 * @brief Create a shared HttpHandler from a function
 * 
 * Convenience factory function that creates a FunctionHandler wrapped
 * in a shared_ptr. This is the preferred way to create function-based handlers.
 * 
 * @param handler Function that processes requests and returns responses
 * @return std::shared_ptr<HttpHandler> Shared pointer to the created handler
 * 
 * @code{.cpp}
 * auto handler = makeHandler([](const HttpRequest& request) {
 *     HttpResponse response;
 *     response.setStatusCode(200);
 *     response.setBody("Hello, World!");
 *     return response;
 * });
 * @endcode
 * 
 * @see FunctionHandler
 * @see makeAsyncHandler()
 */
std::shared_ptr<HttpHandler> makeHandler(std::function<HttpResponse(const HttpRequest&)> handler);

/**
 * @brief Create a shared AsyncHttpHandler from a function
 * 
 * Convenience factory function that creates an AsyncFunctionHandler wrapped
 * in a shared_ptr. This is the preferred way to create async function-based handlers.
 * 
 * @param handler Async function that processes requests and invokes callbacks
 * @return std::shared_ptr<AsyncHttpHandler> Shared pointer to the created handler
 * 
 * @code{.cpp}
 * auto handler = makeAsyncHandler([](const HttpRequest& request, auto callback) {
 *     std::async(std::launch::async, [request, callback]() {
 *         HttpResponse response;
 *         response.setStatusCode(200);
 *         response.setBody("Async Hello, World!");
 *         callback(response);
 *     });
 * });
 * @endcode
 * 
 * @see AsyncFunctionHandler
 * @see makeHandler()
 */
std::shared_ptr<AsyncHttpHandler> makeAsyncHandler(std::function<void(const HttpRequest&, AsyncHttpHandler::ResponseCallback)> handler);

} // namespace cppSwitchboard 