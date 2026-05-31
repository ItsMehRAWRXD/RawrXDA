#pragma once

#include "ai_execution_guardrails.hpp"
#include "rollback_engine.hpp"
#include "state_subscription_engine.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace RawrXD::Core {

// Forward declaration for transformer engine
class TransformerExecutionEngine;

// Safe inference result
struct SafeInferenceResult {
    bool success{false};
    bool blocked{false};
    std::string block_reason;
    std::vector<std::string> tokens;
    std::string full_output;
    size_t tokens_generated{0};
    std::chrono::milliseconds duration{0};
    std::string transaction_id;
    std::vector<std::string> rollback_actions_executed;
};

// Inference configuration
struct SafeInferenceConfig {
    // Guardrail constraints
    rawrxd::ai::ExecutionConstraints constraints;
    
    // Resource limits
    size_t max_memory_mb{1024};
    size_t max_cpu_percent{80};
    std::chrono::milliseconds timeout{30000};
    
    // Rollback configuration
    bool enable_rollback{true};
    bool rollback_on_block{true};
    bool rollback_on_timeout{true};
    bool rollback_on_error{true};
    
    // State integration
    bool enable_state_tracking{true};
    std::string state_namespace{"safe_inference"};
    
    // Monitoring
    bool enable_monitoring{true};
    std::chrono::milliseconds monitoring_interval{100};
};

// Inference statistics
struct SafeInferenceStats {
    std::atomic<size_t> total_inferences{0};
    std::atomic<size_t> successful_inferences{0};
    std::atomic<size_t> blocked_inferences{0};
    std::atomic<size_t> rolled_back_inferences{0};
    std::atomic<size_t> timeout_inferences{0};
    std::atomic<size_t> error_inferences{0};
    
    std::atomic<size_t> total_tokens_generated{0};
    std::atomic<size_t> total_tokens_blocked{0};
    
    std::atomic<double> avg_inference_time_ms{0.0};
    std::atomic<double> avg_tokens_per_inference{0.0};
    
    void reset() {
        total_inferences.store(0);
        successful_inferences.store(0);
        blocked_inferences.store(0);
        rolled_back_inferences.store(0);
        timeout_inferences.store(0);
        error_inferences.store(0);
        total_tokens_generated.store(0);
        total_tokens_blocked.store(0);
        avg_inference_time_ms.store(0.0);
        avg_tokens_per_inference.store(0.0);
    }
};

// Copyable snapshot of inference statistics
struct SafeInferenceStatsSnapshot {
    size_t total_inferences{0};
    size_t successful_inferences{0};
    size_t blocked_inferences{0};
    size_t rolled_back_inferences{0};
    size_t timeout_inferences{0};
    size_t error_inferences{0};
    size_t total_tokens_generated{0};
    size_t total_tokens_blocked{0};
    double avg_inference_time_ms{0.0};
    double avg_tokens_per_inference{0.0};
};

// Safe inference session that bridges transformer engine with guardrails and rollback
class SafeInferenceSession {
public:
    SafeInferenceSession(
        rawrxd::ai::AIExecutionGuardrails& guardrails,
        RollbackEngine& rollback,
        StateSubscriptionEngine* state_engine = nullptr
    );
    
    ~SafeInferenceSession();
    
    // Configure the session
    void configure(const SafeInferenceConfig& config);
    
    // Generate tokens safely with guardrail validation
    SafeInferenceResult generate_safely(
        const std::string& model_id,
        const std::string& prompt,
        const std::function<std::vector<std::string>(const std::string&)>& generator
    );
    
    // Generate with streaming (token-by-token validation)
    SafeInferenceResult generate_streaming(
        const std::string& model_id,
        const std::string& prompt,
        const std::function<std::optional<std::string>()>& stream_generator
    );
    
    // Abort current generation
    void abort_generation();
    
    // Get statistics snapshot
    SafeInferenceStatsSnapshot get_stats() const;
    
    // Reset statistics
    void reset_stats();
    
    // Check if generation is in progress
    bool is_generating() const { return generating_.load(); }
    
    // Get current generation ID
    std::optional<rawrxd::ai::ExecutionId> get_current_execution_id() const;
    
private:
    // Validate tokens against guardrails
    rawrxd::ai::GuardrailResult validate_tokens(
        rawrxd::ai::ExecutionId execution_id,
        const std::vector<std::string>& tokens
    );
    
    // Check resource constraints
    bool check_resource_constraints();
    
    // Monitor generation progress
    void monitor_generation(
        rawrxd::ai::ExecutionId execution_id,
        std::atomic<bool>& should_abort
    );
    
    // Rollback state mutations
    void rollback_state_mutations(
        const std::string& transaction_id,
        const std::vector<std::string>& mutated_keys
    );
    
    // Update statistics
    void update_stats(const SafeInferenceResult& result);
    
    // Member variables
    rawrxd::ai::AIExecutionGuardrails& guardrails_;
    RollbackEngine& rollback_;
    StateSubscriptionEngine* state_engine_;
    
    SafeInferenceConfig config_;
    SafeInferenceStats stats_;
    
    std::atomic<bool> generating_{false};
    std::atomic<bool> abort_requested_{false};
    
    std::optional<rawrxd::ai::ExecutionId> current_execution_id_;
    std::string current_transaction_id_;
    std::vector<std::string> current_state_mutations_;
    
    mutable std::mutex mutex_;
};

// Resource monitor for tracking CPU/memory usage during inference
class ResourceMonitor {
public:
    ResourceMonitor();
    
    // Start monitoring
    void start();
    
    // Stop monitoring and get stats
    struct ResourceUsage {
        double peak_cpu_percent{0.0};
        double avg_cpu_percent{0.0};
        size_t peak_memory_mb{0};
        size_t avg_memory_mb{0};
        std::chrono::milliseconds duration{0};
    };
    
    ResourceUsage stop();
    
    // Check if resource limits are exceeded
    bool exceeds_limits(
        size_t max_memory_mb,
        size_t max_cpu_percent
    ) const;
    
    // Get current resource usage
    ResourceUsage get_current_usage() const;
    
private:
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<double> current_cpu_percent_{0.0};
    std::atomic<size_t> current_memory_mb_{0};
    
    std::thread monitor_thread_;
    std::atomic<bool> monitoring_{false};
    mutable std::mutex mutex_;
};

// Token validator for streaming validation
class TokenValidator {
public:
    TokenValidator(rawrxd::ai::AIExecutionGuardrails& guardrails);
    
    // Validate a single token
    bool validate_token(
        rawrxd::ai::ExecutionId execution_id,
        const std::string& token
    );
    
    // Validate accumulated tokens
    rawrxd::ai::GuardrailResult validate_accumulated(
        rawrxd::ai::ExecutionId execution_id,
        const std::vector<std::string>& tokens
    );
    
    // Reset accumulated state
    void reset();
    
    // Get accumulated tokens
    std::vector<std::string> get_accumulated_tokens() const;
    
private:
    rawrxd::ai::AIExecutionGuardrails& guardrails_;
    std::vector<std::string> accumulated_tokens_;
    mutable std::mutex mutex_;
};

} // namespace RawrXD::Core
