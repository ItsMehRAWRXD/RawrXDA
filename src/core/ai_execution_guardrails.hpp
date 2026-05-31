#pragma once

#include "state_subscription_engine.hpp"
#include "rollback_engine.hpp"

// Forward declarations for testing components
namespace RawrXD::Testing {
    class DeterministicRNG;
    class DeterministicClock;
}

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rawrxd::ai {

// Strong types for AI execution domain
using GuardrailId = std::string;
using ExecutionId = uint64_t;
using ModelId = std::string;
using TokenCount = uint32_t;
using Temperature = float;

// Execution constraints and limits
struct ExecutionConstraints {
    std::optional<TokenCount> max_tokens;
    std::optional<Temperature> max_temperature;
    std::optional<std::chrono::milliseconds> max_duration;
    std::optional<uint32_t> max_concurrent_requests;
    std::optional<double> max_memory_mb;
    
    bool allow_network_access{false};
    bool allow_file_system_access{false};
    bool allow_system_calls{false};
    
    // Model-specific constraints
    std::vector<ModelId> allowed_models;
    std::vector<std::string> blocked_topics;
    std::vector<std::string> allowed_functions;
};

// Execution context and state
struct ExecutionContext {
    ExecutionId id;
    GuardrailId guardrail_id;
    ModelId model_id;
    std::string prompt;
    ExecutionConstraints constraints;
    std::chrono::steady_clock::time_point start_time;
    
    // Runtime monitoring
    std::atomic<TokenCount> tokens_generated{0};
    std::atomic<uint32_t> concurrent_requests{0};
    std::atomic<double> memory_usage_mb{0.0};
    
    // Failure tracking
    std::vector<std::string> violations;
    std::vector<std::string> warnings;
    
    // Deterministic control
    std::shared_ptr<RawrXD::Testing::DeterministicRNG> rng;
    std::shared_ptr<RawrXD::Testing::DeterministicClock> clock;
};

// Guardrail violation types
enum class ViolationSeverity {
    Warning,    // Non-critical deviation
    Critical,   // Critical constraint violation
    Fatal       // Immediate termination required
};

struct GuardrailViolation {
    ViolationSeverity severity;
    std::string message;
    std::string constraint_type;
    std::optional<double> measured_value;
    std::optional<double> limit_value;
    std::chrono::steady_clock::time_point timestamp;
};

// Guardrail evaluation result
struct GuardrailResult {
    bool allowed{true};
    std::vector<GuardrailViolation> violations;
    std::optional<std::string> redirect_model;
    std::optional<std::string> modified_prompt;
    std::optional<ExecutionConstraints> modified_constraints;
};

// Guardrail interface for extensible constraint checking
class IExecutionGuardrail {
public:
    virtual ~IExecutionGuardrail() = default;
    
    virtual GuardrailId id() const = 0;
    virtual std::string description() const = 0;
    virtual ViolationSeverity max_severity() const = 0;
    
    virtual GuardrailResult evaluate(
        const ExecutionContext& context,
        const std::string& prompt,
        const ExecutionConstraints& constraints
    ) const = 0;
    
    virtual GuardrailResult monitor(
        const ExecutionContext& context,
        const std::string& generated_text,
        const ExecutionConstraints& constraints
    ) const = 0;
};

// Built-in guardrail implementations
class TokenLimitGuardrail : public IExecutionGuardrail {
public:
    GuardrailId id() const override { return "token_limit"; }
    std::string description() const override { return "Enforces maximum token generation limits"; }
    ViolationSeverity max_severity() const override { return ViolationSeverity::Critical; }
    
    GuardrailResult evaluate(
        const ExecutionContext& context,
        const std::string& prompt,
        const ExecutionConstraints& constraints
    ) const override;
    
    GuardrailResult monitor(
        const ExecutionContext& context,
        const std::string& generated_text,
        const ExecutionConstraints& constraints
    ) const override;
};

class TemperatureGuardrail : public IExecutionGuardrail {
public:
    GuardrailId id() const override { return "temperature"; }
    std::string description() const override { return "Enforces temperature constraints"; }
    ViolationSeverity max_severity() const override { return ViolationSeverity::Critical; }
    
    GuardrailResult evaluate(
        const ExecutionContext& context,
        const std::string& prompt,
        const ExecutionConstraints& constraints
    ) const override;
    
    GuardrailResult monitor(
        const ExecutionContext& context,
        const std::string& generated_text,
        const ExecutionConstraints& constraints
    ) const override;
};

class ContentSafetyGuardrail : public IExecutionGuardrail {
public:
    GuardrailId id() const override { return "content_safety"; }
    std::string description() const override { return "Enforces content safety policies"; }
    ViolationSeverity max_severity() const override { return ViolationSeverity::Fatal; }
    
    GuardrailResult evaluate(
        const ExecutionContext& context,
        const std::string& prompt,
        const ExecutionConstraints& constraints
    ) const override;
    
    GuardrailResult monitor(
        const ExecutionContext& context,
        const std::string& generated_text,
        const ExecutionConstraints& constraints
    ) const override;
};

class ResourceUsageGuardrail : public IExecutionGuardrail {
public:
    GuardrailId id() const override { return "resource_usage"; }
    std::string description() const override { return "Monitors and limits resource consumption"; }
    ViolationSeverity max_severity() const override { return ViolationSeverity::Critical; }
    
    GuardrailResult evaluate(
        const ExecutionContext& context,
        const std::string& prompt,
        const ExecutionConstraints& constraints
    ) const override;
    
    GuardrailResult monitor(
        const ExecutionContext& context,
        const std::string& generated_text,
        const ExecutionConstraints& constraints
    ) const override;
};

// Main guardrails engine
class AIExecutionGuardrails {
public:
    AIExecutionGuardrails();
    ~AIExecutionGuardrails();
    
    // Registration
    void register_guardrail(std::unique_ptr<IExecutionGuardrail> guardrail);
    void unregister_guardrail(const GuardrailId& id);
    
    // Execution control
    ExecutionId begin_execution(
        const GuardrailId& guardrail_id,
        const ModelId& model_id,
        const std::string& prompt,
        const ExecutionConstraints& constraints
    );
    
    GuardrailResult validate_execution(
        ExecutionId execution_id,
        const std::string& prompt_chunk = ""
    );
    
    GuardrailResult monitor_execution(
        ExecutionId execution_id,
        const std::string& generated_text
    );
    
    void update_execution_state(
        ExecutionId execution_id,
        TokenCount tokens_generated,
        double memory_usage_mb
    );
    
    void end_execution(ExecutionId execution_id);
    
    // Statistics and monitoring
    struct ExecutionStats {
        uint64_t total_executions{0};
        uint64_t blocked_executions{0};
        uint64_t warnings_issued{0};
        uint64_t critical_violations{0};
        uint64_t fatal_violations{0};
        
        std::chrono::milliseconds average_duration{0};
        double average_memory_usage_mb{0.0};
        TokenCount average_tokens_generated{0};
    };
    
    ExecutionStats get_stats() const;
    std::vector<GuardrailViolation> get_recent_violations(size_t max_count = 100) const;
    
    // Configuration
    void set_default_constraints(const ExecutionConstraints& constraints);
    ExecutionConstraints get_default_constraints() const;
    
    // Integration with state system
    void connect_to_state_engine(RawrXD::Core::StateSubscriptionEngine& state_engine);
    
    // Deterministic testing support (commented out for production)
    // void enable_deterministic_mode(
    //     std::shared_ptr<RawrXD::Testing::DeterministicRNG> rng,
    //     std::shared_ptr<RawrXD::Testing::DeterministicClock> clock
    // );
    // 
    // void disable_deterministic_mode();
    
private:
    struct ExecutionState {
        ExecutionContext context;
        std::vector<GuardrailViolation> violations;
        std::chrono::steady_clock::time_point last_monitor_time;
        bool active{true};
    };
    
    mutable std::mutex mutex_;
    
    std::unordered_map<GuardrailId, std::unique_ptr<IExecutionGuardrail>> guardrails_;
    std::unordered_map<ExecutionId, ExecutionState> executions_;
    
    ExecutionConstraints default_constraints_;
    ExecutionStats stats_;
    std::vector<GuardrailViolation> recent_violations_;
    
    std::atomic<ExecutionId> next_execution_id_{1};
    
    // Testing components removed for production compilation
    // std::shared_ptr<RawrXD::Testing::DeterministicRNG> deterministic_rng_;
    // std::shared_ptr<RawrXD::Testing::DeterministicClock> deterministic_clock_;
    // bool deterministic_mode_{false};
    
    // State integration
    RawrXD::Core::StateSubscriptionEngine* state_engine_{nullptr};
    std::unordered_map<std::string, RawrXD::Core::SubscriberId> state_subscriptions_;
    
    ExecutionId generate_execution_id();
    GuardrailResult evaluate_all_guardrails(
        const ExecutionContext& context,
        const std::string& prompt,
        const ExecutionConstraints& constraints,
        bool is_monitoring
    ) const;
    
    void record_violation(ExecutionId execution_id, const GuardrailViolation& violation);
    void update_stats(const ExecutionState& state, const GuardrailResult& result);
    void cleanup_inactive_executions();
    
    // State callback handlers
    void handle_state_update(const RawrXD::Core::StateKey& key, const RawrXD::Core::StateValue& value);
    void handle_guardrail_config_update(const std::string& config_json);
};

// Factory functions
std::unique_ptr<AIExecutionGuardrails> create_default_guardrails_system();
std::unique_ptr<AIExecutionGuardrails> create_minimal_guardrails_system();
std::unique_ptr<AIExecutionGuardrails> create_strict_guardrails_system();

// Utility functions
ExecutionConstraints create_default_constraints();
ExecutionConstraints create_strict_constraints();
ExecutionConstraints create_permissive_constraints();

bool validate_constraints(const ExecutionConstraints& constraints);
std::string constraints_to_json(const ExecutionConstraints& constraints);
ExecutionConstraints constraints_from_json(const std::string& json);

// Violation helpers
GuardrailViolation create_violation(
    ViolationSeverity severity,
    const std::string& message,
    const std::string& constraint_type,
    std::optional<double> measured_value = std::nullopt,
    std::optional<double> limit_value = std::nullopt
);

bool should_terminate_execution(const std::vector<GuardrailViolation>& violations);

} // namespace rawrxd::ai
