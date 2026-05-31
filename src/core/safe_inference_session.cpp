#include "safe_inference_session.hpp"
#include "../engine/global_runtime_orchestrator.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

namespace RawrXD::Core {

// SafeInferenceSession implementation

SafeInferenceSession::SafeInferenceSession(
    rawrxd::ai::AIExecutionGuardrails& guardrails,
    RollbackEngine& rollback,
    StateSubscriptionEngine* state_engine
)
    : guardrails_(guardrails)
    , rollback_(rollback)
    , state_engine_(state_engine)
{
}

SafeInferenceSession::~SafeInferenceSession() {
    if (generating_.load()) {
        abort_generation();
    }
}

void SafeInferenceSession::configure(const SafeInferenceConfig& config) {
    std::lock_guard lock(mutex_);
    config_ = config;
    
    // Configure guardrails
    guardrails_.set_default_constraints(config.constraints);
    
    // Connect to state engine if provided
    if (state_engine_ && config.enable_state_tracking) {
        guardrails_.connect_to_state_engine(*state_engine_);
    }
}

SafeInferenceResult SafeInferenceSession::generate_safely(
    const std::string& model_id,
    const std::string& prompt,
    const std::function<std::vector<std::string>(const std::string&)>& generator
) {
    SafeInferenceResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    stats_.total_inferences++;
    
    // Check if already generating
    if (generating_.load()) {
        result.blocked = true;
        result.block_reason = "Generation already in progress";
        stats_.blocked_inferences++;
        return result;
    }
    
    generating_.store(true);
    abort_requested_.store(false);
    
    // Begin transaction for rollback
    std::string tx_id;
    if (config_.enable_rollback) {
        tx_id = "inference_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        // Note: RollbackEngine doesn't have begin_transaction in the existing interface
        // We'll track state mutations manually
    }
    current_transaction_id_ = tx_id;
    
    // Begin execution in guardrails
    current_execution_id_ = guardrails_.begin_execution(
        "safe_inference",
        model_id,
        prompt,
        config_.constraints
    );
    
    result.transaction_id = tx_id;
    
    // Check resource constraints
    if (!check_resource_constraints()) {
        result.blocked = true;
        result.block_reason = "Resource constraints exceeded";
        stats_.blocked_inferences++;
        
        guardrails_.end_execution(*current_execution_id_);
        generating_.store(false);
        
        return result;
    }
    
    // Validate prompt
    auto prompt_validation = guardrails_.validate_execution(*current_execution_id_, prompt);
    if (!prompt_validation.allowed) {
        result.blocked = true;
        result.block_reason = "Prompt blocked by guardrails";
        for (const auto& violation : prompt_validation.violations) {
            result.block_reason += ": " + violation.message;
        }
        stats_.blocked_inferences++;
        
        guardrails_.end_execution(*current_execution_id_);
        generating_.store(false);
        
        return result;
    }
    
    // Monitor generation in background thread
    std::atomic<bool> should_abort{false};
    std::thread monitor_thread;
    
    if (config_.enable_monitoring) {
        monitor_thread = std::thread([this, execution_id = *current_execution_id_, &should_abort]() {
            monitor_generation(execution_id, should_abort);
        });
    }
    
    // Generate tokens
    std::vector<std::string> tokens;
    try {
        tokens = generator(prompt);
        
        // Check if generation was aborted
        if (abort_requested_.load() || should_abort.load()) {
            result.blocked = true;
            result.block_reason = "Generation aborted";
            stats_.rolled_back_inferences++;
            
            // Rollback state mutations
            if (config_.enable_rollback && config_.rollback_on_block) {
                rollback_state_mutations(tx_id, current_state_mutations_);
            }
            
            guardrails_.end_execution(*current_execution_id_);
            
            if (monitor_thread.joinable()) {
                monitor_thread.join();
            }
            
            generating_.store(false);
            return result;
        }
        
        // Validate generated tokens
        auto validation = validate_tokens(*current_execution_id_, tokens);
        
        if (!validation.allowed) {
            result.blocked = true;
            result.block_reason = "Tokens blocked by guardrails";
            for (const auto& violation : validation.violations) {
                result.block_reason += ": " + violation.message;
            }
            stats_.blocked_inferences++;
            stats_.total_tokens_blocked += tokens.size();
            
            // Rollback state mutations
            if (config_.enable_rollback && config_.rollback_on_block) {
                rollback_state_mutations(tx_id, current_state_mutations_);
            }
            
            guardrails_.end_execution(*current_execution_id_);
            
            if (monitor_thread.joinable()) {
                monitor_thread.join();
            }
            
            generating_.store(false);
            return result;
        }
        
        // Success
        result.success = true;
        result.tokens = tokens;
        result.tokens_generated = tokens.size();
        
        // Build full output
        std::stringstream ss;
        for (const auto& token : tokens) {
            ss << token;
        }
        result.full_output = ss.str();
        
        stats_.successful_inferences++;
        stats_.total_tokens_generated += tokens.size();
        
        // Commit transaction
        if (config_.enable_rollback) {
            // Note: RollbackEngine doesn't have commit in the existing interface
            // We'll just clear the state mutations
            current_state_mutations_.clear();
        }
        
        guardrails_.end_execution(*current_execution_id_);
        
    } catch (const std::exception& e) {
        result.blocked = true;
        result.block_reason = std::string("Exception: ") + e.what();
        stats_.error_inferences++;
        
        // Rollback on error
        if (config_.enable_rollback && config_.rollback_on_error) {
            rollback_state_mutations(tx_id, current_state_mutations_);
        }
        
        guardrails_.end_execution(*current_execution_id_);
    }
    
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    generating_.store(false);
    
    // Calculate duration
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Update statistics
    update_stats(result);
    
    return result;
}

SafeInferenceResult SafeInferenceSession::generate_streaming(
    const std::string& model_id,
    const std::string& prompt,
    const std::function<std::optional<std::string>()>& stream_generator
) {
    SafeInferenceResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    stats_.total_inferences++;
    
    // Check if already generating
    if (generating_.load()) {
        result.blocked = true;
        result.block_reason = "Generation already in progress";
        stats_.blocked_inferences++;
        return result;
    }
    
    generating_.store(true);
    abort_requested_.store(false);
    
    // Begin transaction
    std::string tx_id;
    if (config_.enable_rollback) {
        tx_id = "streaming_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    current_transaction_id_ = tx_id;
    
    // Begin execution in guardrails
    current_execution_id_ = guardrails_.begin_execution(
        "streaming_inference",
        model_id,
        prompt,
        config_.constraints
    );
    
    result.transaction_id = tx_id;
    
    // Validate prompt
    auto prompt_validation = guardrails_.validate_execution(*current_execution_id_, prompt);
    if (!prompt_validation.allowed) {
        result.blocked = true;
        result.block_reason = "Prompt blocked by guardrails";
        stats_.blocked_inferences++;
        
        guardrails_.end_execution(*current_execution_id_);
        generating_.store(false);
        
        return result;
    }
    
    // Token validator for streaming
    TokenValidator validator(guardrails_);
    
    // Generate tokens one by one
    std::vector<std::string> tokens;
    bool blocked = false;
    
    try {
        while (true) {
            // Check abort
            if (abort_requested_.load()) {
                result.blocked = true;
                result.block_reason = "Generation aborted";
                stats_.rolled_back_inferences++;
                blocked = true;
                break;
            }
            
            // Get next token
            auto token_opt = stream_generator();
            if (!token_opt) {
                // End of stream
                break;
            }
            
            const std::string& token = *token_opt;
            
            // Validate token
            if (!validator.validate_token(*current_execution_id_, token)) {
                result.blocked = true;
                result.block_reason = "Token blocked by guardrails: " + token;
                stats_.blocked_inferences++;
                blocked = true;
                break;
            }
            
            tokens.push_back(token);
            
            // Monitor token count
            if (config_.constraints.max_tokens > 0 && tokens.size() >= config_.constraints.max_tokens) {
                break;
            }
        }
        
        if (blocked) {
            // Rollback
            if (config_.enable_rollback && config_.rollback_on_block) {
                rollback_state_mutations(tx_id, current_state_mutations_);
            }
            
            guardrails_.end_execution(*current_execution_id_);
            generating_.store(false);
            return result;
        }
        
        // Success
        result.success = true;
        result.tokens = tokens;
        result.tokens_generated = tokens.size();
        
        std::stringstream ss;
        for (const auto& token : tokens) {
            ss << token;
        }
        result.full_output = ss.str();
        
        stats_.successful_inferences++;
        stats_.total_tokens_generated += tokens.size();
        
        guardrails_.end_execution(*current_execution_id_);
        
    } catch (const std::exception& e) {
        result.blocked = true;
        result.block_reason = std::string("Exception: ") + e.what();
        stats_.error_inferences++;
        
        if (config_.enable_rollback && config_.rollback_on_error) {
            rollback_state_mutations(tx_id, current_state_mutations_);
        }
        
        guardrails_.end_execution(*current_execution_id_);
    }
    
    generating_.store(false);
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    update_stats(result);
    
    return result;
}

void SafeInferenceSession::abort_generation() {
    abort_requested_.store(true);
}

SafeInferenceStatsSnapshot SafeInferenceSession::get_stats() const {
    SafeInferenceStatsSnapshot snapshot;
    snapshot.total_inferences = stats_.total_inferences.load();
    snapshot.successful_inferences = stats_.successful_inferences.load();
    snapshot.blocked_inferences = stats_.blocked_inferences.load();
    snapshot.rolled_back_inferences = stats_.rolled_back_inferences.load();
    snapshot.timeout_inferences = stats_.timeout_inferences.load();
    snapshot.error_inferences = stats_.error_inferences.load();
    snapshot.total_tokens_generated = stats_.total_tokens_generated.load();
    snapshot.total_tokens_blocked = stats_.total_tokens_blocked.load();
    snapshot.avg_inference_time_ms = stats_.avg_inference_time_ms.load();
    snapshot.avg_tokens_per_inference = stats_.avg_tokens_per_inference.load();
    return snapshot;
}

void SafeInferenceSession::reset_stats() {
    stats_.reset();
}

std::optional<rawrxd::ai::ExecutionId> SafeInferenceSession::get_current_execution_id() const {
    return current_execution_id_;
}

rawrxd::ai::GuardrailResult SafeInferenceSession::validate_tokens(
    rawrxd::ai::ExecutionId execution_id,
    const std::vector<std::string>& tokens
) {
    // Build full text from tokens
    std::stringstream ss;
    for (const auto& token : tokens) {
        ss << token;
    }
    
    return guardrails_.validate_execution(execution_id, ss.str());
}

bool SafeInferenceSession::check_resource_constraints() {
    float global_risk = GlobalRuntimeOrchestrator::Get().AssessRisk();
    if (global_risk > 0.85f) {
        // Critical system risk detected by Global Orchestrator
        return false;
    }
    
    // Check specific memory/CPU limits if monitor is active
    // TODO: Connect ResourceMonitor to actually update current usage
    return true;
}

void SafeInferenceSession::monitor_generation(
    rawrxd::ai::ExecutionId execution_id,
    std::atomic<bool>& should_abort
) {
    auto start_time = std::chrono::steady_clock::now();
    
    while (generating_.load() && !should_abort.load()) {
        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > config_.timeout) {
            should_abort.store(true);
            stats_.timeout_inferences++;
            break;
        }
        
        // Check resource constraints
        if (!check_resource_constraints()) {
            should_abort.store(true);
            break;
        }
        
        // Sleep for monitoring interval
        std::this_thread::sleep_for(config_.monitoring_interval);
    }
}

void SafeInferenceSession::rollback_state_mutations(
    const std::string& transaction_id,
    const std::vector<std::string>& mutated_keys
) {
    // Rollback state mutations through state engine
    if (state_engine_) {
        for (const auto& key : mutated_keys) {
            // Note: StateSubscriptionEngine doesn't have a rollback method
            // We would need to store previous values and restore them
            // For now, we'll just track that rollback was requested
            current_state_mutations_.push_back("rollback:" + key);
        }
    }
}

void SafeInferenceSession::update_stats(const SafeInferenceResult& result) {
    // Update average inference time
    size_t total = stats_.total_inferences.load();
    double current_avg = stats_.avg_inference_time_ms.load();
    double new_avg = (current_avg * (total - 1) + result.duration.count()) / total;
    stats_.avg_inference_time_ms.store(new_avg);
    
    // Update average tokens per inference
    if (result.success) {
        double current_tokens_avg = stats_.avg_tokens_per_inference.load();
        double new_tokens_avg = (current_tokens_avg * (total - 1) + result.tokens_generated) / total;
        stats_.avg_tokens_per_inference.store(new_tokens_avg);
    }
}

// ResourceMonitor implementation

ResourceMonitor::ResourceMonitor() = default;

void ResourceMonitor::start() {
    start_time_ = std::chrono::steady_clock::now();
    monitoring_.store(true);
    
    // TODO: Implement actual resource monitoring
    // This would use platform-specific APIs to get CPU/memory usage
}

ResourceMonitor::ResourceUsage ResourceMonitor::stop() {
    monitoring_.store(false);
    
    ResourceUsage usage;
    usage.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_
    );
    
    // TODO: Implement actual resource usage collection
    usage.peak_cpu_percent = current_cpu_percent_.load();
    usage.avg_cpu_percent = current_cpu_percent_.load();
    usage.peak_memory_mb = current_memory_mb_.load();
    usage.avg_memory_mb = current_memory_mb_.load();
    
    return usage;
}

bool ResourceMonitor::exceeds_limits(
    size_t max_memory_mb,
    size_t max_cpu_percent
) const {
    return current_memory_mb_.load() > max_memory_mb ||
           current_cpu_percent_.load() > max_cpu_percent;
}

ResourceMonitor::ResourceUsage ResourceMonitor::get_current_usage() const {
    ResourceUsage usage;
    usage.peak_cpu_percent = current_cpu_percent_.load();
    usage.avg_cpu_percent = current_cpu_percent_.load();
    usage.peak_memory_mb = current_memory_mb_.load();
    usage.avg_memory_mb = current_memory_mb_.load();
    usage.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_
    );
    return usage;
}

// TokenValidator implementation

TokenValidator::TokenValidator(rawrxd::ai::AIExecutionGuardrails& guardrails)
    : guardrails_(guardrails) {
}

bool TokenValidator::validate_token(
    rawrxd::ai::ExecutionId execution_id,
    const std::string& token
) {
    std::lock_guard lock(mutex_);
    
    // Add token to accumulated
    accumulated_tokens_.push_back(token);
    
    // Validate accumulated tokens
    auto result = guardrails_.validate_execution(execution_id, token);
    
    return result.allowed;
}

rawrxd::ai::GuardrailResult TokenValidator::validate_accumulated(
    rawrxd::ai::ExecutionId execution_id,
    const std::vector<std::string>& tokens
) {
    std::lock_guard lock(mutex_);
    
    // Build full text
    std::stringstream ss;
    for (const auto& token : tokens) {
        ss << token;
    }
    
    return guardrails_.validate_execution(execution_id, ss.str());
}

void TokenValidator::reset() {
    std::lock_guard lock(mutex_);
    accumulated_tokens_.clear();
}

std::vector<std::string> TokenValidator::get_accumulated_tokens() const {
    std::lock_guard lock(mutex_);
    return accumulated_tokens_;
}

} // namespace RawrXD::Core
