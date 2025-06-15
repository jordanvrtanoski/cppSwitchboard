#include <gtest/gtest.h>
#include <cppSwitchboard/middleware_pipeline.h>
#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_handler.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

using namespace cppSwitchboard;

// Test middleware for pipeline testing
class PipelineTestMiddleware : public Middleware {
public:
    PipelineTestMiddleware(const std::string& name, int priority = 0) 
        : name_(name), priority_(priority), enabled_(true), callCount_(0) {}
    
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        callCount_++;
        
        std::cout << "PipelineTestMiddleware " << name_ << " (priority " << priority_ << ") called!" << std::endl;
        
        // Record call order - handle std::any safely
        std::vector<std::string> order;
        if (context.find("call_order") != context.end()) {
            try {
                order = std::any_cast<std::vector<std::string>>(context["call_order"]);
                std::cout << "  Found existing call_order with " << order.size() << " items" << std::endl;
            } catch (const std::bad_any_cast&) {
                // If cast fails, start with empty vector
                order.clear();
                std::cout << "  Found call_order but failed to cast, starting fresh" << std::endl;
            }
        } else {
            std::cout << "  No call_order found, creating new one" << std::endl;
        }
        order.push_back(name_);
        context["call_order"] = order;
        
        // Add context data
        context[name_ + "_called"] = true;
        context[name_ + "_priority"] = priority_;
        
        std::cout << "  Added " << name_ << " to call order, new size: " << order.size() << std::endl;
        
        // Call next handler
        HttpResponse response = next(request, context);
        
        // Modify response
        HttpResponse modifiedResponse = response;
        modifiedResponse.setHeader("X-" + name_, "processed");
        
        return modifiedResponse;
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
};

// Test handler for final pipeline handler
class PipelineTestHandler : public HttpHandler {
public:
    PipelineTestHandler(const std::string& name) : name_(name), callCount_(0) {}
    
    HttpResponse handle(const HttpRequest& request) override {
        callCount_.fetch_add(1);  // Explicit atomic increment
        std::string responseBody = "Handler: " + name_ + " processed " + request.getPath();
        return HttpResponse::ok(responseBody);
    }
    
    std::string getName() const { return name_; }
    int getCallCount() const { return callCount_.load(); }

private:
    std::string name_;
    std::atomic<int> callCount_;
};

class MiddlewarePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        pipeline_ = std::make_shared<MiddlewarePipeline>();
        request_ = std::make_unique<HttpRequest>("GET", "/test", "HTTP/1.1");
        request_->setHeader("Content-Type", "application/json");
        request_->setBody("{\"test\": true}");
        // Create a fresh handler for each test to avoid accumulating call counts
        finalHandler_ = std::make_shared<PipelineTestHandler>("FinalHandler");
    }
    
    void TearDown() override {
        // Clean up for the next test
        pipeline_.reset();
        finalHandler_.reset();
        request_.reset();
    }
    
    std::shared_ptr<MiddlewarePipeline> pipeline_;
    std::unique_ptr<HttpRequest> request_;
    std::shared_ptr<PipelineTestHandler> finalHandler_;
};

// Basic pipeline tests
TEST_F(MiddlewarePipelineTest, EmptyPipelineExecution) {
    pipeline_->setFinalHandler(finalHandler_);
    
    EXPECT_TRUE(pipeline_->hasFinalHandler());
    EXPECT_EQ(pipeline_->getMiddlewareCount(), 0);
    
    HttpResponse response = pipeline_->execute(*request_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.getBody(), "Handler: FinalHandler processed /test");
    EXPECT_EQ(finalHandler_->getCallCount(), 1);
}

TEST_F(MiddlewarePipelineTest, SingleMiddlewareExecution) {
    auto middleware = std::make_shared<PipelineTestMiddleware>("TestMiddleware");
    
    pipeline_->addMiddleware(middleware);
    pipeline_->setFinalHandler(finalHandler_);
    
    EXPECT_EQ(pipeline_->getMiddlewareCount(), 1);
    
    HttpResponse response = pipeline_->execute(*request_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.getBody(), "Handler: FinalHandler processed /test");
    EXPECT_EQ(response.getHeader("X-TestMiddleware"), "processed");
    EXPECT_EQ(middleware->getCallCount(), 1);
    EXPECT_EQ(finalHandler_->getCallCount(), 1);
}

TEST_F(MiddlewarePipelineTest, MultipleMiddlewareExecution) {
    auto middleware1 = std::make_shared<PipelineTestMiddleware>("Middleware1", 10);
    auto middleware2 = std::make_shared<PipelineTestMiddleware>("Middleware2", 20);
    auto middleware3 = std::make_shared<PipelineTestMiddleware>("Middleware3", 5);
    
    // Add in random order - pipeline should sort by priority
    pipeline_->addMiddleware(middleware1);
    pipeline_->addMiddleware(middleware2);
    pipeline_->addMiddleware(middleware3);
    pipeline_->setFinalHandler(finalHandler_);
    
    EXPECT_EQ(pipeline_->getMiddlewareCount(), 3);
    
    HttpResponse response = pipeline_->execute(*request_);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.getBody(), "Handler: FinalHandler processed /test");
    
    // Verify all middleware were called
    EXPECT_EQ(middleware1->getCallCount(), 1);
    EXPECT_EQ(middleware2->getCallCount(), 1);
    EXPECT_EQ(middleware3->getCallCount(), 1);
    EXPECT_EQ(finalHandler_->getCallCount(), 1);
    
    // Verify response headers from all middleware
    EXPECT_EQ(response.getHeader("X-Middleware1"), "processed");
    EXPECT_EQ(response.getHeader("X-Middleware2"), "processed");
    EXPECT_EQ(response.getHeader("X-Middleware3"), "processed");
}

TEST_F(MiddlewarePipelineTest, MiddlewarePriorityOrdering) {
    auto middleware1 = std::make_shared<PipelineTestMiddleware>("Low", 1);
    auto middleware2 = std::make_shared<PipelineTestMiddleware>("High", 100);
    auto middleware3 = std::make_shared<PipelineTestMiddleware>("Medium", 50);
    
    pipeline_->addMiddleware(middleware1);
    pipeline_->addMiddleware(middleware2);
    pipeline_->addMiddleware(middleware3);
    pipeline_->setFinalHandler(finalHandler_);
    
    // Verify middleware names are correctly ordered before execution
    auto names = pipeline_->getMiddlewareNames();
    ASSERT_EQ(names.size(), 3) << "Expected 3 middleware but got " << names.size();
    EXPECT_EQ(names[0], "High") << "First middleware should be High, got " << names[0];
    EXPECT_EQ(names[1], "Medium") << "Second middleware should be Medium, got " << names[1];
    EXPECT_EQ(names[2], "Low") << "Third middleware should be Low, got " << names[2];
    
    Context context;
    HttpResponse response = pipeline_->execute(*request_, context);
    
    // Debug: Print context contents
    std::cout << "Context contains " << context.size() << " keys:" << std::endl;
    for (const auto& [key, value] : context) {
        std::cout << "  Key: " << key << std::endl;
    }
    
    // Check if middleware were called
    EXPECT_EQ(middleware1->getCallCount(), 1) << "Low priority middleware not called";
    EXPECT_EQ(middleware2->getCallCount(), 1) << "High priority middleware not called";
    EXPECT_EQ(middleware3->getCallCount(), 1) << "Medium priority middleware not called";
    
    // Verify execution order by priority (High -> Medium -> Low)
    ASSERT_TRUE(context.find("call_order") != context.end()) << "call_order not found in context";
    
    std::vector<std::string> callOrder;
    try {
        callOrder = std::any_cast<std::vector<std::string>>(context["call_order"]);
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Failed to cast call_order to vector<string>: " << e.what();
    }
    
    ASSERT_EQ(callOrder.size(), 3);
    EXPECT_EQ(callOrder[0], "High");
    EXPECT_EQ(callOrder[1], "Medium");
    EXPECT_EQ(callOrder[2], "Low");
}

// Context propagation tests
TEST_F(MiddlewarePipelineTest, ContextPropagation) {
    auto middleware1 = std::make_shared<PipelineTestMiddleware>("Middleware1", 20);
    auto middleware2 = std::make_shared<PipelineTestMiddleware>("Middleware2", 10);
    
    pipeline_->addMiddleware(middleware1);
    pipeline_->addMiddleware(middleware2);
    pipeline_->setFinalHandler(finalHandler_);
    
    Context context;
    context["initial_value"] = std::string("initial");
    
    HttpResponse response = pipeline_->execute(*request_, context);
    
    // Verify context contains data from all middleware with safer casting
    try {
        EXPECT_TRUE(std::any_cast<bool>(context["Middleware1_called"]));
        EXPECT_TRUE(std::any_cast<bool>(context["Middleware2_called"]));
        EXPECT_EQ(std::any_cast<int>(context["Middleware1_priority"]), 20);
        EXPECT_EQ(std::any_cast<int>(context["Middleware2_priority"]), 10);
        EXPECT_EQ(std::any_cast<std::string>(context["initial_value"]), "initial");
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Failed to cast context values: " << e.what();
    }
    
    // Verify call order
    ASSERT_TRUE(context.find("call_order") != context.end()) << "call_order not found in context";
    
    std::vector<std::string> callOrder;
    try {
        callOrder = std::any_cast<std::vector<std::string>>(context["call_order"]);
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Failed to cast call_order to vector<string>: " << e.what();
    }
    
    ASSERT_EQ(callOrder.size(), 2);
    EXPECT_EQ(callOrder[0], "Middleware1");
    EXPECT_EQ(callOrder[1], "Middleware2");
}

// Performance benchmark tests
TEST_F(MiddlewarePipelineTest, PipelinePerformanceBenchmark) {
    // Create a pipeline with multiple middleware
    auto middleware1 = std::make_shared<PipelineTestMiddleware>("Perf1", 30);
    auto middleware2 = std::make_shared<PipelineTestMiddleware>("Perf2", 20);
    auto middleware3 = std::make_shared<PipelineTestMiddleware>("Perf3", 10);
    
    pipeline_->addMiddleware(middleware1);
    pipeline_->addMiddleware(middleware2);
    pipeline_->addMiddleware(middleware3);
    pipeline_->setFinalHandler(finalHandler_);
    
    const int numIterations = 1000;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numIterations; ++i) {
        HttpResponse response = pipeline_->execute(*request_);
        
        // Verify response to prevent optimization
        ASSERT_EQ(response.getStatus(), 200);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Performance assertions
    double averageTimePerExecution = static_cast<double>(duration.count()) / numIterations;
    
    // Should be fast (less than 1000 microseconds per execution on average)
    EXPECT_LT(averageTimePerExecution, 1000.0);
    
    std::cout << "Pipeline performance: " << averageTimePerExecution 
              << " microseconds per execution (average over " << numIterations << " executions)" << std::endl;
    
    // Verify all components were called
    EXPECT_EQ(middleware1->getCallCount(), numIterations);
    EXPECT_EQ(middleware2->getCallCount(), numIterations);
    EXPECT_EQ(middleware3->getCallCount(), numIterations);
    EXPECT_EQ(finalHandler_->getCallCount(), numIterations);
} 