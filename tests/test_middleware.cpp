#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cppSwitchboard/middleware.h>
#include <cppSwitchboard/http_request.h>
#include <cppSwitchboard/http_response.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

using namespace cppSwitchboard;
using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;

// Mock middleware for testing
class MockMiddleware : public Middleware {
public:
    MockMiddleware(const std::string& name, int priority = 0) 
        : name_(name), priority_(priority), enabled_(true) {}
    
    MOCK_METHOD(HttpResponse, handle, (const HttpRequest& request, Context& context, NextHandler next), (override));
    
    std::string getName() const override { return name_; }
    int getPriority() const override { return priority_; }
    bool isEnabled() const override { return enabled_; }
    
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void setPriority(int priority) { priority_ = priority; }

private:
    std::string name_;
    int priority_;
    bool enabled_;
};

// Test middleware implementation
class TestMiddleware : public Middleware {
public:
    TestMiddleware(const std::string& name, int priority = 0) 
        : name_(name), priority_(priority), enabled_(true), callCount_(0) {}
    
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        callCount_++;
        
        // Add context information
        context[name_ + "_called"] = true;
        context[name_ + "_method"] = request.getMethod();
        context[name_ + "_path"] = request.getPath();
        
        // Call next handler
        HttpResponse response = next(request, context);
        
        // Modify response (add header)
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

// Test middleware that modifies request
class RequestModifyingMiddleware : public Middleware {
public:
    RequestModifyingMiddleware() : callCount_(0) {}
    
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        callCount_++;
        
        // Create modified request (simulated - in practice, you'd need a mutable request)
        context["request_modified"] = true;
        context["original_path"] = request.getPath();
        
        return next(request, context);
    }
    
    std::string getName() const override { return "RequestModifyingMiddleware"; }
    int getCallCount() const { return callCount_; }

private:
    std::atomic<int> callCount_;
};

// Test middleware that can abort pipeline
class AbortMiddleware : public Middleware {
public:
    AbortMiddleware(bool shouldAbort = true) : shouldAbort_(shouldAbort) {}
    
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        context["abort_middleware_called"] = true;
        
        if (shouldAbort_) {
            // Don't call next - abort the pipeline
            HttpResponse response(401);
            response.setBody("Aborted by AbortMiddleware");
            return response;
        }
        
        return next(request, context);
    }
    
    std::string getName() const override { return "AbortMiddleware"; }
    void setShouldAbort(bool abort) { shouldAbort_ = abort; }

private:
    bool shouldAbort_;
};

// Test middleware that throws exceptions
class ExceptionMiddleware : public Middleware {
public:
    ExceptionMiddleware(const std::string& message) : message_(message) {}
    
    HttpResponse handle(const HttpRequest& request, Context& context, NextHandler next) override {
        context["exception_middleware_called"] = true;
        throw std::runtime_error(message_);
    }
    
    std::string getName() const override { return "ExceptionMiddleware"; }

private:
    std::string message_;
};

class MiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        request_ = std::make_unique<HttpRequest>("GET", "/test", "HTTP/1.1");
        request_->setHeader("Content-Type", "application/json");
        request_->setBody("{\"test\": true}");
    }
    
    std::unique_ptr<HttpRequest> request_;
};

// Tests for basic middleware interface
TEST_F(MiddlewareTest, MiddlewareBasicInterface) {
    auto middleware = std::make_shared<TestMiddleware>("TestMiddleware", 10);
    
    EXPECT_EQ(middleware->getName(), "TestMiddleware");
    EXPECT_EQ(middleware->getPriority(), 10);
    EXPECT_TRUE(middleware->isEnabled());
    EXPECT_EQ(middleware->getCallCount(), 0);
}

TEST_F(MiddlewareTest, MiddlewareEnabledState) {
    auto middleware = std::make_shared<TestMiddleware>("TestMiddleware");
    
    EXPECT_TRUE(middleware->isEnabled());
    
    middleware->setEnabled(false);
    EXPECT_FALSE(middleware->isEnabled());
    
    middleware->setEnabled(true);
    EXPECT_TRUE(middleware->isEnabled());
}

TEST_F(MiddlewareTest, MiddlewarePriorityChange) {
    auto middleware = std::make_shared<TestMiddleware>("TestMiddleware", 5);
    
    EXPECT_EQ(middleware->getPriority(), 5);
    
    middleware->setPriority(15);
    EXPECT_EQ(middleware->getPriority(), 15);
    
    middleware->setPriority(-5);
    EXPECT_EQ(middleware->getPriority(), -5);
}

// Tests for Context operations
TEST_F(MiddlewareTest, ContextBasicOperations) {
    Context context;
    
    // Test basic storage and retrieval
    context["string_key"] = std::string("test_value");
    context["int_key"] = 42;
    context["bool_key"] = true;
    
    EXPECT_EQ(std::any_cast<std::string>(context["string_key"]), "test_value");
    EXPECT_EQ(std::any_cast<int>(context["int_key"]), 42);
    EXPECT_EQ(std::any_cast<bool>(context["bool_key"]), true);
}

TEST_F(MiddlewareTest, ContextHelperOperations) {
    Context context;
    ContextHelper helper(context);
    
    // Test string operations
    helper.setString("test_string", "hello_world");
    EXPECT_EQ(helper.getString("test_string"), "hello_world");
    EXPECT_EQ(helper.getString("nonexistent", "default"), "default");
    
    // Test boolean operations
    helper.setBool("test_bool", true);
    EXPECT_TRUE(helper.getBool("test_bool"));
    EXPECT_FALSE(helper.getBool("nonexistent", false));
    
    // Test key existence
    EXPECT_TRUE(helper.hasKey("test_string"));
    EXPECT_TRUE(helper.hasKey("test_bool"));
    EXPECT_FALSE(helper.hasKey("nonexistent"));
    
    // Test key removal
    EXPECT_TRUE(helper.removeKey("test_string"));
    EXPECT_FALSE(helper.hasKey("test_string"));
    EXPECT_FALSE(helper.removeKey("nonexistent"));
}

TEST_F(MiddlewareTest, ContextHelperTypeSafety) {
    Context context;
    ContextHelper helper(context);
    
    // Store an integer as std::any
    context["int_as_any"] = 123;
    
    // Try to get it as string (should return default)
    EXPECT_EQ(helper.getString("int_as_any", "default"), "default");
    
    // Store a string
    helper.setString("valid_string", "test");
    EXPECT_EQ(helper.getString("valid_string"), "test");
}

// Tests for middleware execution
TEST_F(MiddlewareTest, MiddlewareExecution) {
    auto middleware = std::make_shared<TestMiddleware>("TestMiddleware");
    Context context;
    
    // Mock next handler
    NextHandler nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        ctx["next_handler_called"] = true;
        return HttpResponse::ok("Next handler response");
    };
    
    HttpResponse response = middleware->handle(*request_, context, nextHandler);
    
    // Verify middleware was called
    EXPECT_EQ(middleware->getCallCount(), 1);
    
    // Verify context was populated
    EXPECT_TRUE(std::any_cast<bool>(context["TestMiddleware_called"]));
    EXPECT_EQ(std::any_cast<std::string>(context["TestMiddleware_method"]), "GET");
    EXPECT_EQ(std::any_cast<std::string>(context["TestMiddleware_path"]), "/test");
    EXPECT_TRUE(std::any_cast<bool>(context["next_handler_called"]));
    
    // Verify response was modified
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.getBody(), "Next handler response");
    EXPECT_EQ(response.getHeader("X-TestMiddleware"), "processed");
}

TEST_F(MiddlewareTest, MiddlewareContextPropagation) {
    auto middleware1 = std::make_shared<TestMiddleware>("Middleware1");
    auto middleware2 = std::make_shared<TestMiddleware>("Middleware2");
    Context context;
    
    // Set up chained execution
    NextHandler finalHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        ctx["final_handler_called"] = true;
        return HttpResponse::ok("Final response");
    };
    
    NextHandler secondHandler = [middleware2, finalHandler](const HttpRequest& req, Context& ctx) -> HttpResponse {
        return middleware2->handle(req, ctx, finalHandler);
    };
    
    HttpResponse response = middleware1->handle(*request_, context, secondHandler);
    
    // Verify both middleware were called
    EXPECT_EQ(middleware1->getCallCount(), 1);
    EXPECT_EQ(middleware2->getCallCount(), 1);
    
    // Verify context contains data from both middleware
    EXPECT_TRUE(std::any_cast<bool>(context["Middleware1_called"]));
    EXPECT_TRUE(std::any_cast<bool>(context["Middleware2_called"]));
    EXPECT_TRUE(std::any_cast<bool>(context["final_handler_called"]));
    
    // Verify response has headers from both middleware
    EXPECT_EQ(response.getHeader("X-Middleware1"), "processed");
    EXPECT_EQ(response.getHeader("X-Middleware2"), "processed");
}

TEST_F(MiddlewareTest, MiddlewareAbortExecution) {
    auto abortMiddleware = std::make_shared<AbortMiddleware>(true);
    Context context;
    
    NextHandler nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        ctx["should_not_be_called"] = true;
        return HttpResponse::ok("Should not reach here");
    };
    
    HttpResponse response = abortMiddleware->handle(*request_, context, nextHandler);
    
    // Verify abort middleware was called
    EXPECT_TRUE(std::any_cast<bool>(context["abort_middleware_called"]));
    
    // Verify next handler was NOT called
    EXPECT_EQ(context.find("should_not_be_called"), context.end());
    
    // Verify response from abort middleware
    EXPECT_EQ(response.getStatus(), 401);
    EXPECT_EQ(response.getBody(), "Aborted by AbortMiddleware");
}

TEST_F(MiddlewareTest, MiddlewareExceptionHandling) {
    auto exceptionMiddleware = std::make_shared<ExceptionMiddleware>("Test exception");
    Context context;
    
    NextHandler nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        return HttpResponse::ok("Should not reach here");
    };
    
    // Exception should propagate up
    EXPECT_THROW({
        exceptionMiddleware->handle(*request_, context, nextHandler);
    }, std::runtime_error);
    
    // Verify middleware was called before exception
    EXPECT_TRUE(std::any_cast<bool>(context["exception_middleware_called"]));
}

TEST_F(MiddlewareTest, DisabledMiddlewareSkipped) {
    auto middleware = std::make_shared<TestMiddleware>("TestMiddleware");
    middleware->setEnabled(false);
    
    Context context;
    NextHandler nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        ctx["next_handler_called"] = true;
        return HttpResponse::ok("Next handler response");
    };
    
    // Note: This test assumes the pipeline handles disabled middleware
    // In practice, the pipeline checks isEnabled() before calling handle()
    // For this test, we'll verify the middleware itself respects the enabled state
    EXPECT_FALSE(middleware->isEnabled());
}

// Thread safety tests
TEST_F(MiddlewareTest, ContextHelperThreadSafety) {
    Context context;
    ContextHelper helper(context);
    
    const int numThreads = 10;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);
    
    // Launch threads that perform concurrent operations
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&helper, &successCount, i, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                std::string key = "thread_" + std::to_string(i) + "_key_" + std::to_string(j);
                std::string value = "value_" + std::to_string(i) + "_" + std::to_string(j);
                
                // Set value
                helper.setString(key, value);
                
                // Get value
                std::string retrieved = helper.getString(key);
                if (retrieved == value) {
                    successCount++;
                }
                
                // Remove key
                helper.removeKey(key);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All operations should have succeeded
    EXPECT_EQ(successCount.load(), numThreads * operationsPerThread);
}

TEST_F(MiddlewareTest, MiddlewareConcurrentExecution) {
    auto middleware = std::make_shared<TestMiddleware>("ConcurrentMiddleware");
    const int numRequests = 50;
    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);
    
    // Create multiple requests concurrently
    for (int i = 0; i < numRequests; ++i) {
        threads.emplace_back([middleware, &successCount, i]() {
            HttpRequest request("GET", "/test/" + std::to_string(i), "HTTP/1.1");
            Context context;
            
            NextHandler nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
                return HttpResponse::ok("Response for " + req.getPath());
            };
            
            try {
                HttpResponse response = middleware->handle(request, context, nextHandler);
                                 if (response.getStatus() == 200) {
                    successCount++;
                }
            } catch (const std::exception& e) {
                // Unexpected exception
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All requests should have succeeded
    EXPECT_EQ(successCount.load(), numRequests);
    EXPECT_EQ(middleware->getCallCount(), numRequests);
}

// Performance benchmarks
TEST_F(MiddlewareTest, MiddlewarePerformanceBenchmark) {
    auto middleware = std::make_shared<TestMiddleware>("BenchmarkMiddleware");
    const int numIterations = 10000;
    
    Context context;
    NextHandler nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        return HttpResponse::ok("Benchmark response");
    };
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numIterations; ++i) {
        HttpResponse response = middleware->handle(*request_, context, nextHandler);
        
        // Verify response to prevent optimization
        ASSERT_EQ(response.getStatus(), 200);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Performance assertions (adjust based on expected performance)
    double averageTimePerCall = static_cast<double>(duration.count()) / numIterations;
    
    // Should be fast (less than 100 microseconds per call on average)
    EXPECT_LT(averageTimePerCall, 100.0);
    
    std::cout << "Middleware performance: " << averageTimePerCall 
              << " microseconds per call (average over " << numIterations << " calls)" << std::endl;
}

TEST_F(MiddlewareTest, ContextPerformanceBenchmark) {
    Context context;
    ContextHelper helper(context);
    const int numOperations = 50000;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOperations; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        
        helper.setString(key, value);
        std::string retrieved = helper.getString(key);
        
        // Verify to prevent optimization
        ASSERT_EQ(retrieved, value);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    double averageTimePerOperation = static_cast<double>(duration.count()) / numOperations;
    
    // Should be very fast (less than 10 microseconds per operation)
    EXPECT_LT(averageTimePerOperation, 10.0);
    
    std::cout << "Context operation performance: " << averageTimePerOperation 
              << " microseconds per operation (average over " << numOperations << " operations)" << std::endl;
}

// Edge cases and error conditions
TEST_F(MiddlewareTest, EmptyContextOperations) {
    Context context;
    ContextHelper helper(context);
    
    // Test operations on empty context
    EXPECT_EQ(helper.getString("nonexistent"), "");
    EXPECT_EQ(helper.getString("nonexistent", "default"), "default");
    EXPECT_FALSE(helper.getBool("nonexistent"));
    EXPECT_TRUE(helper.getBool("nonexistent", true));
    EXPECT_FALSE(helper.hasKey("nonexistent"));
    EXPECT_FALSE(helper.removeKey("nonexistent"));
}

TEST_F(MiddlewareTest, LargeContextOperations) {
    Context context;
    ContextHelper helper(context);
    
    // Test with large amount of data
    const int numKeys = 1000;
    for (int i = 0; i < numKeys; ++i) {
        std::string key = "large_key_" + std::to_string(i);
        std::string value = "large_value_" + std::to_string(i) + "_with_extra_data_to_make_it_longer";
        helper.setString(key, value);
    }
    
    // Verify all data is accessible
    for (int i = 0; i < numKeys; ++i) {
        std::string key = "large_key_" + std::to_string(i);
        std::string expectedValue = "large_value_" + std::to_string(i) + "_with_extra_data_to_make_it_longer";
        EXPECT_EQ(helper.getString(key), expectedValue);
        EXPECT_TRUE(helper.hasKey(key));
    }
}

// Mock middleware tests using Google Mock
TEST_F(MiddlewareTest, MockMiddlewareInteractions) {
    auto mockMiddleware = std::make_shared<MockMiddleware>("MockMiddleware", 10);
    Context context;
    
    // Set up expectations
    EXPECT_CALL(*mockMiddleware, handle(_, _, _))
        .Times(1)
        .WillOnce([](const HttpRequest& request, Context& context, NextHandler next) -> HttpResponse {
            context["mock_called"] = true;
            return next(request, context);
        });
    
    NextHandler nextHandler = [](const HttpRequest& req, Context& ctx) -> HttpResponse {
        return HttpResponse::ok("Mock test response");
    };
    
    HttpResponse response = mockMiddleware->handle(*request_, context, nextHandler);
    
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_TRUE(std::any_cast<bool>(context["mock_called"]));
} 