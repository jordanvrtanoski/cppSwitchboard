#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cppSwitchboard/async_middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <future>
#include <condition_variable>

using namespace cppSwitchboard;
using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;

// Test async middleware implementation
class TestAsyncMiddleware : public AsyncMiddleware {
public:
    TestAsyncMiddleware(const std::string& name, int priority = 0, 
                       std::chrono::milliseconds delay = std::chrono::milliseconds(0)) 
        : name_(name), priority_(priority), enabled_(true), callCount_(0), delay_(delay) {}
    
    void handleAsync(const HttpRequest& request, Context& context,
                    AsyncNextHandler next, AsyncResponseCallback callback) override {
        callCount_++;
        
        // Add context information
        context[name_ + "_called"] = true;
        context[name_ + "_method"] = request.getMethod();
        context[name_ + "_path"] = request.getPath();
        
        // Simulate async work if delay is specified
        if (delay_ > std::chrono::milliseconds(0)) {
            std::thread([this, request, &context, next, callback]() {
                std::this_thread::sleep_for(delay_);
                
                // Call next handler
                next(request, context, [this, callback](const HttpResponse& response) {
                    // Modify response (add header)
                    HttpResponse modifiedResponse = response;
                    modifiedResponse.setHeader("X-" + name_, "processed");
                    callback(modifiedResponse);
                });
            }).detach();
        } else {
            // Call next handler synchronously
            next(request, context, [this, callback](const HttpResponse& response) {
                // Modify response (add header)
                HttpResponse modifiedResponse = response;
                modifiedResponse.setHeader("X-" + name_, "processed");
                callback(modifiedResponse);
            });
        }
    }
    
    std::string getName() const override { return name_; }
    int getPriority() const override { return priority_; }
    bool isEnabled() const override { return enabled_; }
    
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void setPriority(int priority) { priority_ = priority; }
    int getCallCount() const { return callCount_; }

private:
    std::string name_;
    int priority_;
    bool enabled_;
    std::atomic<int> callCount_;
    std::chrono::milliseconds delay_;
};

// Test async handler for final handler
class TestAsyncHandler : public AsyncHttpHandler {
public:
    TestAsyncHandler(const std::string& responseBody = "Test Response") 
        : responseBody_(responseBody), callCount_(0) {}
    
    void handleAsync(const HttpRequest& request, ResponseCallback callback) override {
        callCount_++;
        
        HttpResponse response(200);
        response.setBody(responseBody_);
        response.setHeader("Content-Type", "text/plain");
        response.setHeader("X-Handler", "TestAsyncHandler");
        
        callback(response);
    }
    
    int getCallCount() const { return callCount_; }

private:
    std::string responseBody_;
    std::atomic<int> callCount_;
};

class AsyncMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        request_ = std::make_unique<HttpRequest>("GET", "/test", "HTTP/1.1");
        request_->setHeader("Content-Type", "application/json");
        request_->setBody("{\"test\": true}");
        
        pipeline_ = std::make_shared<AsyncMiddlewarePipeline>();
        handler_ = std::make_shared<TestAsyncHandler>();
    }
    
    std::unique_ptr<HttpRequest> request_;
    std::shared_ptr<AsyncMiddlewarePipeline> pipeline_;
    std::shared_ptr<TestAsyncHandler> handler_;
};

// Tests for async middleware interface
TEST_F(AsyncMiddlewareTest, AsyncMiddlewareBasicInterface) {
    auto middleware = std::make_shared<TestAsyncMiddleware>("TestAsyncMiddleware", 10);
    
    EXPECT_EQ(middleware->getName(), "TestAsyncMiddleware");
    EXPECT_EQ(middleware->getPriority(), 10);
    EXPECT_TRUE(middleware->isEnabled());
    EXPECT_EQ(middleware->getCallCount(), 0);
}

TEST_F(AsyncMiddlewareTest, PipelineBasicOperations) {
    EXPECT_EQ(pipeline_->getMiddlewareCount(), 0);
    EXPECT_FALSE(pipeline_->hasFinalHandler());
    
    pipeline_->setFinalHandler(handler_);
    EXPECT_TRUE(pipeline_->hasFinalHandler());
    
    auto middleware = std::make_shared<TestAsyncMiddleware>("TestMiddleware");
    pipeline_->addMiddleware(middleware);
    EXPECT_EQ(pipeline_->getMiddlewareCount(), 1);
}

TEST_F(AsyncMiddlewareTest, BasicAsyncExecution) {
    pipeline_->setFinalHandler(handler_);
    
    HttpResponse response;
    bool callbackCalled = false;
    
    pipeline_->executeAsync(*request_, [&](const HttpResponse& resp) {
        response = resp;
        callbackCalled = true;
    });
    
    // Wait for callback with timeout
    auto start = std::chrono::steady_clock::now();
    while (!callbackCalled && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.getBody(), "Test Response");
    EXPECT_EQ(handler_->getCallCount(), 1);
}

TEST_F(AsyncMiddlewareTest, MiddlewareExecutionOrder) {
    auto middleware1 = std::make_shared<TestAsyncMiddleware>("Middleware1", 100);
    auto middleware2 = std::make_shared<TestAsyncMiddleware>("Middleware2", 50);
    auto middleware3 = std::make_shared<TestAsyncMiddleware>("Middleware3", 150);
    
    pipeline_->addMiddleware(middleware1);
    pipeline_->addMiddleware(middleware2);
    pipeline_->addMiddleware(middleware3);
    pipeline_->setFinalHandler(handler_);
    
    HttpResponse response;
    bool callbackCalled = false;
    
    pipeline_->executeAsync(*request_, [&](const HttpResponse& resp) {
        response = resp;
        callbackCalled = true;
    });
    
    // Wait for callback
    auto start = std::chrono::steady_clock::now();
    while (!callbackCalled && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(response.getStatus(), 200);
    
    // Verify execution order by checking headers (higher priority first)
    EXPECT_FALSE(response.getHeader("X-Middleware3").empty());
    EXPECT_FALSE(response.getHeader("X-Middleware1").empty());
    EXPECT_FALSE(response.getHeader("X-Middleware2").empty());
    
    // Verify all middleware were called
    EXPECT_EQ(middleware1->getCallCount(), 1);
    EXPECT_EQ(middleware2->getCallCount(), 1);
    EXPECT_EQ(middleware3->getCallCount(), 1);
}

TEST_F(AsyncMiddlewareTest, ContextPropagation) {
    auto middleware1 = std::make_shared<TestAsyncMiddleware>("Middleware1");
    auto middleware2 = std::make_shared<TestAsyncMiddleware>("Middleware2");
    
    pipeline_->addMiddleware(middleware1);
    pipeline_->addMiddleware(middleware2);
    pipeline_->setFinalHandler(handler_);
    
    Context context;
    context["initial_data"] = std::string("test_value");
    
    HttpResponse response;
    bool callbackCalled = false;
    
    pipeline_->executeAsync(*request_, context, [&](const HttpResponse& resp) {
        response = resp;
        callbackCalled = true;
    });
    
    // Wait for callback
    auto start = std::chrono::steady_clock::now();
    while (!callbackCalled && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(response.getStatus(), 200);
    
    // Context should have been populated by middleware
    EXPECT_TRUE(context.find("Middleware1_called") != context.end());
    EXPECT_TRUE(context.find("Middleware2_called") != context.end());
    EXPECT_TRUE(context.find("initial_data") != context.end());
}

TEST_F(AsyncMiddlewareTest, PerformanceMonitoring) {
    pipeline_->setPerformanceMonitoring(true);
    EXPECT_TRUE(pipeline_->isPerformanceMonitoringEnabled());
    
    auto middleware = std::make_shared<TestAsyncMiddleware>("TestMiddleware");
    pipeline_->addMiddleware(middleware);
    pipeline_->setFinalHandler(handler_);
    
    HttpResponse response;
    bool callbackCalled = false;
    
    pipeline_->executeAsync(*request_, [&](const HttpResponse& resp) {
        response = resp;
        callbackCalled = true;
    });
    
    // Wait for callback
    auto start = std::chrono::steady_clock::now();
    while (!callbackCalled && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(response.getStatus(), 200);
    
    // Disable performance monitoring
    pipeline_->setPerformanceMonitoring(false);
    EXPECT_FALSE(pipeline_->isPerformanceMonitoringEnabled());
} 