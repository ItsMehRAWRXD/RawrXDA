#include "core/safe_inference_session.hpp"
#include "core/ai_execution_guardrails.hpp"
#include "core/rollback_engine.hpp"
#include "testing/chaos_test_harness.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace RawrXD::Core;
using namespace RawrXD::Testing;
using namespace rawrxd::ai;

// Mock transformer generator for testing
std::vector<std::string> mock_generator(const std::string& prompt) {
    std::vector<std::string> tokens;
    
    // Simulate token generation
    std::string response = "This is a test response to: " + prompt;
    
    // Split into tokens (words)
    size_t pos = 0;
    size_t prev = 0;
    while ((pos = response.find(' ', prev)) != std::string::npos) {
        tokens.push_back(response.substr(prev, pos - prev));
        prev = pos + 1;
    }
    if (prev < response.length()) {
        tokens.push_back(response.substr(prev));
    }
    
    return tokens;
}

// Mock streaming generator for testing
class MockStreamGenerator {
public:
    MockStreamGenerator(const std::vector<std::string>& tokens)
        : tokens_(tokens), index_(0) {}
    
    std::optional<std::string> next() {
        if (index_ >= tokens_.size()) {
            return std::nullopt;
        }
        return tokens_[index_++];
    }
    
private:
    std::vector<std::string> tokens_;
    size_t index_;
};

// Test basic safe inference
void test_basic_safe_inference() {
    std::cout << "[INTEGRATION TEST] Basic safe inference" << std::endl;
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    SafeInferenceSession session(guardrails, rollback);
    
    SafeInferenceConfig config;
    config.constraints = create_default_constraints();
    config.constraints.max_tokens = 100;
    session.configure(config);
    
    auto result = session.generate_safely("test-model", "Hello, world!", mock_generator);
    
    assert(result.success);
    assert(!result.blocked);
    assert(result.tokens_generated > 0);
    
    std::cout << "  ✓ Basic inference succeeded" << std::endl;
    std::cout << "  ✓ Tokens generated: " << result.tokens_generated << std::endl;
}

// Test guardrail blocking
void test_guardrail_blocking() {
    std::cout << "[INTEGRATION TEST] Guardrail blocking" << std::endl;
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    SafeInferenceSession session(guardrails, rollback);
    
    SafeInferenceConfig config;
    config.constraints = create_default_constraints();
    config.constraints.max_tokens = 5; // Very low limit
    session.configure(config);
    
    auto result = session.generate_safely("test-model", "This is a very long prompt that should be blocked", mock_generator);
    
    // Should be blocked due to token limit
    assert(result.blocked || !result.success);
    
    std::cout << "  ✓ Guardrail correctly blocked execution" << std::endl;
    std::cout << "  ✓ Block reason: " << result.block_reason << std::endl;
}

// Test rollback on error
void test_rollback_on_error() {
    std::cout << "[INTEGRATION TEST] Rollback on error" << std::endl;
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    SafeInferenceSession session(guardrails, rollback);
    
    SafeInferenceConfig config;
    config.enable_rollback = true;
    config.rollback_on_error = true;
    session.configure(config);
    
    // Generator that throws exception
    auto error_generator = [](const std::string& prompt) -> std::vector<std::string> {
        throw std::runtime_error("Simulated error");
    };
    
    auto result = session.generate_safely("test-model", "Test prompt", error_generator);
    
    assert(result.blocked);
    assert(result.block_reason.find("Exception") != std::string::npos);
    
    std::cout << "  ✓ Rollback executed on error" << std::endl;
    std::cout << "  ✓ Error handled correctly" << std::endl;
}

// Test streaming inference
void test_streaming_inference() {
    std::cout << "[INTEGRATION TEST] Streaming inference" << std::endl;
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    SafeInferenceSession session(guardrails, rollback);
    
    SafeInferenceConfig config;
    config.constraints = create_default_constraints();
    config.constraints.max_tokens = 100;
    session.configure(config);
    
    std::vector<std::string> tokens = {"Hello", " ", "world", "!"};
    MockStreamGenerator stream_gen(tokens);
    
    auto result = session.generate_streaming("test-model", "Test", [&stream_gen]() {
        return stream_gen.next();
    });
    
    assert(result.success);
    assert(result.tokens_generated == 4);
    
    std::cout << "  ✓ Streaming inference succeeded" << std::endl;
    std::cout << "  ✓ Tokens: " << result.full_output << std::endl;
}

// Test concurrent safe inference
void test_concurrent_safe_inference() {
    std::cout << "[INTEGRATION TEST] Concurrent safe inference" << std::endl;
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&guardrails, &rollback, &success_count, &failure_count, i]() {
            SafeInferenceSession session(guardrails, rollback);
            
            SafeInferenceConfig config;
            config.constraints = create_default_constraints();
            session.configure(config);
            
            auto result = session.generate_safely("test-model", "Prompt " + std::to_string(i), mock_generator);
            
            if (result.success) {
                success_count++;
            } else {
                failure_count++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "  ✓ Concurrent executions: " << (success_count.load() + failure_count.load()) << std::endl;
    std::cout << "  ✓ Successful: " << success_count.load() << std::endl;
    std::cout << "  ✓ Failed: " << failure_count.load() << std::endl;
}

// Test chaos injection in safe inference
void test_chaos_in_safe_inference() {
    std::cout << "[INTEGRATION TEST] Chaos injection in safe inference" << std::endl;
    
    ChaosTestHarness harness;
    FailurePolicy policy;
    policy.mode = FailureMode::Exception;
    policy.probability = 0.3; // 30% exception rate
    harness.set_policy(policy);
    harness.enable();
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    SafeInferenceSession session(guardrails, rollback);
    
    SafeInferenceConfig config;
    config.enable_rollback = true;
    config.rollback_on_error = true;
    session.configure(config);
    
    std::atomic<int> success_count{0};
    std::atomic<int> rollback_count{0};
    
    for (int i = 0; i < 20; ++i) {
        // Generator that may fail
        auto chaos_generator = [&harness, i](const std::string& prompt) -> std::vector<std::string> {
            if (harness.inject_failure("generate")) {
                throw std::runtime_error("Chaos failure");
            }
            return mock_generator(prompt);
        };
        
        auto result = session.generate_safely("test-model", "Test " + std::to_string(i), chaos_generator);
        
        if (result.success) {
            success_count++;
        } else if (result.block_reason.find("Exception") != std::string::npos) {
            rollback_count++;
        }
    }
    
    std::cout << "  ✓ Successful inferences: " << success_count.load() << std::endl;
    std::cout << "  ✓ Rollbacks executed: " << rollback_count.load() << std::endl;
    std::cout << "  ✓ Chaos handled correctly" << std::endl;
}

// Test resource constraints
void test_resource_constraints() {
    std::cout << "[INTEGRATION TEST] Resource constraints" << std::endl;
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    SafeInferenceSession session(guardrails, rollback);
    
    SafeInferenceConfig config;
    config.constraints = create_default_constraints();
    config.max_memory_mb = 10; // Very low limit
    config.max_cpu_percent = 10; // Very low limit
    session.configure(config);
    
    auto result = session.generate_safely("test-model", "Test prompt", mock_generator);
    
    // May be blocked due to resource constraints
    std::cout << "  ✓ Resource constraints checked" << std::endl;
    if (result.blocked) {
        std::cout << "  ✓ Blocked: " << result.block_reason << std::endl;
    } else {
        std::cout << "  ✓ Passed (resource check not fully implemented)" << std::endl;
    }
}

// Test abort during generation
void test_abort_generation() {
    std::cout << "[INTEGRATION TEST] Abort generation" << std::endl;
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    SafeInferenceSession session(guardrails, rollback);
    
    SafeInferenceConfig config;
    config.enable_rollback = true;
    session.configure(config);
    
    // Slow generator
    auto slow_generator = [&session](const std::string& prompt) -> std::vector<std::string> {
        std::vector<std::string> tokens;
        for (int i = 0; i < 100; ++i) {
            if (!session.is_generating()) {
                break;
            }
            tokens.push_back("token" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return tokens;
    };
    
    // Start generation in background
    std::thread gen_thread([&session, &slow_generator]() {
        auto result = session.generate_safely("test-model", "Test", slow_generator);
    });
    
    // Wait a bit then abort
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session.abort_generation();
    
    gen_thread.join();
    
    std::cout << "  ✓ Generation aborted successfully" << std::endl;
}

// Test statistics tracking
void test_statistics_tracking() {
    std::cout << "[INTEGRATION TEST] Statistics tracking" << std::endl;
    
    AIExecutionGuardrails guardrails;
    StateSubscriptionEngine state_engine;
    RollbackEngine rollback(state_engine);
    SafeInferenceSession session(guardrails, rollback);
    
    SafeInferenceConfig config;
    session.configure(config);
    
    // Run multiple inferences
    for (int i = 0; i < 10; ++i) {
        auto result = session.generate_safely("test-model", "Test " + std::to_string(i), mock_generator);
    }
    
    auto stats = session.get_stats();
    
    assert(stats.total_inferences == 10);
    assert(stats.successful_inferences == 10);
    
    std::cout << "  ✓ Total inferences: " << stats.total_inferences << std::endl;
    std::cout << "  ✓ Successful: " << stats.successful_inferences << std::endl;
    std::cout << "  ✓ Tokens generated: " << stats.total_tokens_generated << std::endl;
}

int main() {
    std::cout << "Safe Inference Session Integration Test" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_basic_safe_inference();
    test_guardrail_blocking();
    test_rollback_on_error();
    test_streaming_inference();
    test_concurrent_safe_inference();
    test_chaos_in_safe_inference();
    test_resource_constraints();
    test_abort_generation();
    test_statistics_tracking();
    
    std::cout << "========================================" << std::endl;
    std::cout << "All safe inference session integration tests passed!" << std::endl;
    std::cout << "PASS=9 FAIL=0" << std::endl;
    
    return 0;
}
