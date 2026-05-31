#include "ai_execution_guardrails.hpp"
#include "state_subscription_engine.hpp"

#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <iomanip>

namespace rawrxd::ai {

using namespace std::chrono;
using json = nlohmann::json;

// TokenLimitGuardrail implementation
GuardrailResult TokenLimitGuardrail::evaluate(
    const ExecutionContext& context,
    const std::string& prompt,
    const ExecutionConstraints& constraints
) const {
    GuardrailResult result;
    
    if (constraints.max_tokens.has_value()) {
        const auto max_tokens = constraints.max_tokens.value();
        
        // Estimate prompt tokens (rough approximation)
        const size_t prompt_tokens_estimate = prompt.size() / 4;
        
        if (prompt_tokens_estimate > max_tokens) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Prompt exceeds maximum token limit",
                "token_limit",
                static_cast<double>(prompt_tokens_estimate),
                static_cast<double>(max_tokens)
            ));
            result.allowed = false;
        }
    }
    
    return result;
}

GuardrailResult TokenLimitGuardrail::monitor(
    const ExecutionContext& context,
    const std::string& generated_text,
    const ExecutionConstraints& constraints
) const {
    GuardrailResult result;
    
    if (constraints.max_tokens.has_value()) {
        const auto max_tokens = constraints.max_tokens.value();
        const auto current_tokens = context.tokens_generated.load();
        
        if (current_tokens >= max_tokens) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Execution exceeded maximum token limit",
                "token_limit",
                static_cast<double>(current_tokens),
                static_cast<double>(max_tokens)
            ));
            result.allowed = false;
        }
        
        // Warn at 80% of limit
        if (current_tokens >= max_tokens * 0.8) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Warning,
                "Approaching token limit",
                "token_limit",
                static_cast<double>(current_tokens),
                static_cast<double>(max_tokens)
            ));
        }
    }
    
    return result;
}

// TemperatureGuardrail implementation
GuardrailResult TemperatureGuardrail::evaluate(
    const ExecutionContext& context,
    const std::string& prompt,
    const ExecutionConstraints& constraints
) const {
    GuardrailResult result;
    
    if (constraints.max_temperature.has_value()) {
        const auto max_temp = constraints.max_temperature.value();
        
        // Check if temperature is within bounds
        if (max_temp < 0.0 || max_temp > 2.0) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Invalid temperature constraint",
                "temperature",
                max_temp,
                2.0
            ));
            result.allowed = false;
        }
    }
    
    return result;
}

GuardrailResult TemperatureGuardrail::monitor(
    const ExecutionContext& context,
    const std::string& generated_text,
    const ExecutionConstraints& constraints
) const {
    // Temperature is typically set at execution start and doesn't change
    // during generation, so monitoring is mostly about validation
    return GuardrailResult{};
}

// ContentSafetyGuardrail implementation
GuardrailResult ContentSafetyGuardrail::evaluate(
    const ExecutionContext& context,
    const std::string& prompt,
    const ExecutionConstraints& constraints
) const {
    GuardrailResult result;
    
    // Check for blocked topics in prompt
    static const std::vector<std::regex> blocked_patterns = {
        std::regex("harm|danger|unsafe|illegal", std::regex::icase),
        std::regex("hack|exploit|vulnerability", std::regex::icase),
        std::regex("malware|virus|trojan|ransomware", std::regex::icase),
        std::regex("discriminat|bias|prejudice|stereotype", std::regex::icase),
        std::regex("explicit|porn|adult|nsfw", std::regex::icase),
        std::regex("violence|attack|weapon|kill", std::regex::icase)
    };
    
    for (const auto& pattern : blocked_patterns) {
        if (std::regex_search(prompt, pattern)) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Fatal,
                "Prompt contains blocked content",
                "content_safety"
            ));
            result.allowed = false;
            break;
        }
    }
    
    // Check against blocked topics list
    for (const auto& blocked_topic : constraints.blocked_topics) {
        if (prompt.find(blocked_topic) != std::string::npos) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Prompt contains blocked topic: " + blocked_topic,
                "content_safety"
            ));
            result.allowed = false;
        }
    }
    
    return result;
}

GuardrailResult ContentSafetyGuardrail::monitor(
    const ExecutionContext& context,
    const std::string& generated_text,
    const ExecutionConstraints& constraints
) const {
    GuardrailResult result;
    
    // Similar checks for generated text
    static const std::vector<std::regex> blocked_patterns = {
        std::regex("harm|danger|unsafe|illegal", std::regex::icase),
        std::regex("hack|exploit|vulnerability", std::regex::icase),
        std::regex("malware|virus|trojan|ransomware", std::regex::icase),
        std::regex("discriminat|bias|prejudice|stereotype", std::regex::icase),
        std::regex("explicit|porn|adult|nsfw", std::regex::icase),
        std::regex("violence|attack|weapon|kill", std::regex::icase)
    };
    
    for (const auto& pattern : blocked_patterns) {
        if (std::regex_search(generated_text, pattern)) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Fatal,
                "Generated text contains blocked content",
                "content_safety"
            ));
            result.allowed = false;
            break;
        }
    }
    
    return result;
}

// ResourceUsageGuardrail implementation
GuardrailResult ResourceUsageGuardrail::evaluate(
    const ExecutionContext& context,
    const std::string& prompt,
    const ExecutionConstraints& constraints
) const {
    GuardrailResult result;
    
    // Check memory constraints
    if (constraints.max_memory_mb.has_value()) {
        const auto max_memory = constraints.max_memory_mb.value();
        
        if (max_memory < 1.0) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Invalid memory constraint",
                "memory_limit",
                max_memory,
                1.0
            ));
            result.allowed = false;
        }
    }
    
    // Check duration constraints
    if (constraints.max_duration.has_value()) {
        const auto max_duration = constraints.max_duration.value();
        
        if (max_duration < milliseconds(100)) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Invalid duration constraint",
                "duration_limit",
                max_duration.count(),
                100.0
            ));
            result.allowed = false;
        }
    }
    
    return result;
}

GuardrailResult ResourceUsageGuardrail::monitor(
    const ExecutionContext& context,
    const std::string& generated_text,
    const ExecutionConstraints& constraints
) const {
    GuardrailResult result;
    
    // Check memory usage
    if (constraints.max_memory_mb.has_value()) {
        const auto max_memory = constraints.max_memory_mb.value();
        const auto current_memory = context.memory_usage_mb.load();
        
        if (current_memory > max_memory) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Execution exceeded memory limit",
                "memory_limit",
                current_memory,
                max_memory
            ));
            result.allowed = false;
        }
        
        // Warn at 80% of memory limit
        if (current_memory >= max_memory * 0.8) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Warning,
                "Approaching memory limit",
                "memory_limit",
                current_memory,
                max_memory
            ));
        }
    }
    
    // Check duration
    if (constraints.max_duration.has_value()) {
        const auto max_duration = constraints.max_duration.value();
        const auto elapsed = steady_clock::now() - context.start_time;
        
        if (elapsed > max_duration) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Execution exceeded time limit",
                "duration_limit",
                duration_cast<milliseconds>(elapsed).count(),
                max_duration.count()
            ));
            result.allowed = false;
        }
        
        // Warn at 80% of time limit
        if (elapsed > max_duration * 0.8) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Warning,
                "Approaching time limit",
                "duration_limit",
                duration_cast<milliseconds>(elapsed).count(),
                max_duration.count()
            ));
        }
    }
    
    // Check concurrent requests
    if (constraints.max_concurrent_requests.has_value()) {
        const auto max_concurrent = constraints.max_concurrent_requests.value();
        const auto current_concurrent = context.concurrent_requests.load();
        
        if (current_concurrent > max_concurrent) {
            result.violations.push_back(create_violation(
                ViolationSeverity::Critical,
                "Exceeded concurrent request limit",
                "concurrent_limit",
                static_cast<double>(current_concurrent),
                static_cast<double>(max_concurrent)
            ));
            result.allowed = false;
        }
    }
    
    return result;
}

// AIExecutionGuardrails implementation
AIExecutionGuardrails::AIExecutionGuardrails() {
    // Register built-in guardrails
    register_guardrail(std::make_unique<TokenLimitGuardrail>());
    register_guardrail(std::make_unique<TemperatureGuardrail>());
    register_guardrail(std::make_unique<ContentSafetyGuardrail>());
    register_guardrail(std::make_unique<ResourceUsageGuardrail>());
    
    default_constraints_ = create_default_constraints();
}

AIExecutionGuardrails::~AIExecutionGuardrails() {
    // Clean up all executions
    {
        std::lock_guard lock(mutex_);
        executions_.clear();
    }
}

void AIExecutionGuardrails::register_guardrail(std::unique_ptr<IExecutionGuardrail> guardrail) {
    std::lock_guard lock(mutex_);
    guardrails_[guardrail->id()] = std::move(guardrail);
}

void AIExecutionGuardrails::unregister_guardrail(const GuardrailId& id) {
    std::lock_guard lock(mutex_);
    guardrails_.erase(id);
}

ExecutionId AIExecutionGuardrails::begin_execution(
    const GuardrailId& guardrail_id,
    const ModelId& model_id,
    const std::string& prompt,
    const ExecutionConstraints& constraints
) {
    std::lock_guard lock(mutex_);
    
    const ExecutionId execution_id = generate_execution_id();
    
    ExecutionContext context{
        .id = execution_id,
        .guardrail_id = guardrail_id,
        .model_id = model_id,
        .prompt = prompt,
        .constraints = constraints,
        .start_time = steady_clock::now()
    };
    
    // Create ExecutionState in place to avoid copy/move issues with atomic members
    auto [it, inserted] = executions_.try_emplace(execution_id);
    auto& state = it->second;
    state.context.id = context.id;
    state.context.guardrail_id = context.guardrail_id;
    state.context.model_id = context.model_id;
    state.context.prompt = std::move(context.prompt);
    state.context.constraints = context.constraints;
    state.context.start_time = context.start_time;
    state.context.tokens_generated.store(context.tokens_generated.load());
    state.context.concurrent_requests.store(context.concurrent_requests.load());
    state.context.memory_usage_mb.store(context.memory_usage_mb.load());
    state.context.violations = std::move(context.violations);
    state.context.warnings = std::move(context.warnings);
    state.last_monitor_time = steady_clock::now();
    state.active = true;
    stats_.total_executions++;
    
    return execution_id;
}

GuardrailResult AIExecutionGuardrails::validate_execution(
    ExecutionId execution_id,
    const std::string& prompt_chunk
) {
    std::lock_guard lock(mutex_);
    
    auto it = executions_.find(execution_id);
    if (it == executions_.end()) {
        GuardrailResult result;
        result.violations.push_back(create_violation(
            ViolationSeverity::Critical,
            "Invalid execution ID",
            "validation"
        ));
        result.allowed = false;
        return result;
    }
    
    ExecutionState& state = it->second;
    if (!state.active) {
        GuardrailResult result;
        result.violations.push_back(create_violation(
            ViolationSeverity::Critical,
            "Execution already terminated",
            "validation"
        ));
        result.allowed = false;
        return result;
    }
    
    // Combine original prompt with any additional chunks
    std::string full_prompt = state.context.prompt;
    if (!prompt_chunk.empty()) {
        full_prompt += " " + prompt_chunk;
    }
    
    GuardrailResult result = evaluate_all_guardrails(
        state.context, full_prompt, state.context.constraints, false
    );
    
    if (!result.allowed) {
        state.active = false;
        stats_.blocked_executions++;
    }
    
    for (const auto& violation : result.violations) {
        record_violation(execution_id, violation);
    }
    
    update_stats(state, result);
    
    return result;
}

GuardrailResult AIExecutionGuardrails::monitor_execution(
    ExecutionId execution_id,
    const std::string& generated_text
) {
    std::lock_guard lock(mutex_);
    
    auto it = executions_.find(execution_id);
    if (it == executions_.end()) {
        GuardrailResult result;
        result.violations.push_back(create_violation(
            ViolationSeverity::Critical,
            "Invalid execution ID",
            "monitoring"
        ));
        result.allowed = false;
        return result;
    }
    
    ExecutionState& state = it->second;
    if (!state.active) {
        GuardrailResult result;
        result.violations.push_back(create_violation(
            ViolationSeverity::Critical,
            "Execution already terminated",
            "monitoring"
        ));
        result.allowed = false;
        return result;
    }
    
    state.last_monitor_time = steady_clock::now();
    
    GuardrailResult result = evaluate_all_guardrails(
        state.context, generated_text, state.context.constraints, true
    );
    
    if (!result.allowed) {
        state.active = false;
    }
    
    for (const auto& violation : result.violations) {
        record_violation(execution_id, violation);
    }
    
    update_stats(state, result);
    
    return result;
}

void AIExecutionGuardrails::update_execution_state(
    ExecutionId execution_id,
    TokenCount tokens_generated,
    double memory_usage_mb
) {
    std::lock_guard lock(mutex_);
    
    auto it = executions_.find(execution_id);
    if (it != executions_.end() && it->second.active) {
        it->second.context.tokens_generated = tokens_generated;
        it->second.context.memory_usage_mb = memory_usage_mb;
    }
}

void AIExecutionGuardrails::end_execution(ExecutionId execution_id) {
    std::lock_guard lock(mutex_);
    
    auto it = executions_.find(execution_id);
    if (it != executions_.end()) {
        // Update statistics with final execution data
        const ExecutionState& state = it->second;
        if (state.active) {
            const auto duration = duration_cast<milliseconds>(
                steady_clock::now() - state.context.start_time
            );
            
            stats_.average_duration = milliseconds(
                (stats_.average_duration.count() * stats_.total_executions + duration.count()) / 
                (stats_.total_executions + 1)
            );
            
            stats_.average_memory_usage_mb = (
                stats_.average_memory_usage_mb * stats_.total_executions + 
                state.context.memory_usage_mb
            ) / (stats_.total_executions + 1);
            
            stats_.average_tokens_generated = (
                stats_.average_tokens_generated * stats_.total_executions + 
                state.context.tokens_generated
            ) / (stats_.total_executions + 1);
        }
        
        executions_.erase(it);
    }
    
    cleanup_inactive_executions();
}

AIExecutionGuardrails::ExecutionStats AIExecutionGuardrails::get_stats() const {
    std::lock_guard lock(mutex_);
    return stats_;
}

std::vector<GuardrailViolation> AIExecutionGuardrails::get_recent_violations(size_t max_count) const {
    std::lock_guard lock(mutex_);
    
    if (recent_violations_.size() <= max_count) {
        return recent_violations_;
    }
    
    return std::vector<GuardrailViolation>(
        recent_violations_.end() - max_count,
        recent_violations_.end()
    );
}

void AIExecutionGuardrails::set_default_constraints(const ExecutionConstraints& constraints) {
    std::lock_guard lock(mutex_);
    default_constraints_ = constraints;
}

ExecutionConstraints AIExecutionGuardrails::get_default_constraints() const {
    std::lock_guard lock(mutex_);
    return default_constraints_;
}

void AIExecutionGuardrails::connect_to_state_engine(RawrXD::Core::StateSubscriptionEngine& state_engine) {
    std::lock_guard lock(mutex_);
    
    // Store a pointer to the state engine
    state_engine_ = &state_engine;
    
    // Subscribe to guardrail configuration updates
    state_subscriptions_["guardrail_config"] = state_engine_->subscribe(
        "ai.guardrails.config",
        RawrXD::Core::StateSubscriptionEngine::Subscriber{
            .callback = [this](const RawrXD::Core::StateEvent& event) {
                handle_guardrail_config_update(event.newValue);
            }
        }
    ).value;
    
    // Subscribe to execution state updates
    state_subscriptions_["execution_state"] = state_engine_->subscribe(
        "ai.execution.state",
        RawrXD::Core::StateSubscriptionEngine::Subscriber{
            .callback = [this](const RawrXD::Core::StateEvent& event) {
                handle_state_update(event.key, event.newValue);
            }
        }
    ).value;
}

// void AIExecutionGuardrails::enable_deterministic_mode(
//     std::shared_ptr<RawrXD::Testing::DeterministicRNG> rng,
//     std::shared_ptr<RawrXD::Testing::DeterministicClock> clock
// ) {
//     std::lock_guard lock(mutex_);
//     deterministic_rng_ = std::move极ng);
//     deterministic_clock_ = std::move(clock);
//     deterministic_mode_ = true;
// }

// void AIExecutionGuardrails::disable_deterministic_mode() {
//     std::lock_guard lock(mutex_);
//     deterministic_mode_ = false;
//     deterministic_rng_.reset();
//     deterministic_clock_.reset();
// }

ExecutionId AIExecutionGuardrails::generate_execution_id() {
    return next_execution_id_++;
}

GuardrailResult AIExecutionGuardrails::evaluate_all_guardrails(
    const ExecutionContext& context,
    const std::string& text,
    const ExecutionConstraints& constraints,
    bool is_monitoring
) const {
    GuardrailResult final_result;
    
    for (const auto& [id, guardrail] : guardrails_) {
        GuardrailResult guardrail_result;
        
        if (is_monitoring) {
            guardrail_result = guardrail->monitor(context, text, constraints);
        } else {
            guardrail_result = guardrail->evaluate(context, text, constraints);
        }
        
        if (!guardrail_result.allowed) {
            final_result.allowed = false;
        }
        
        final_result.violations.insert(
            final_result.violations.end(),
            guardrail_result.violations.begin(),
            guardrail_result.violations.end()
        );
        
        // Apply redirections and modifications from the most severe guardrail
        if (guardrail_result.redirect_model.has_value()) {
            final_result.redirect_model = guardrail_result.redirect_model;
        }
        if (guardrail_result.modified_prompt.has_value()) {
            final_result.modified_prompt = guardrail_result.modified_prompt;
        }
        if (guardrail_result.modified_constraints.has_value()) {
            final_result.modified_constraints = guardrail_result.modified_constraints;
        }
    }
    
    return final_result;
}

void AIExecutionGuardrails::record_violation(ExecutionId execution_id, const GuardrailViolation& violation) {
    auto it = executions_.find(execution_id);
    if (it != executions_.end()) {
        it->second.violations.push_back(violation);
    }
    
    recent_violations_.push_back(violation);
    
    // Keep only the most recent violations
    if (recent_violations_.size() > 1000) {
        recent_violations_.erase(
            recent_violations_.begin(),
            recent_violations_.begin() + (recent_violations_.size() - 1000)
        );
    }
    
    // Publish violation to state engine if connected
    if (state_engine_) {
        json violation_json;
        violation_json["execution_id"] = execution_id;
        violation_json["severity"] = static_cast<int>(violation.severity);
        violation_json["message"] = violation.message;
        violation_json["constraint_type"] = violation.constraint_type;
        violation_json["timestamp"] = duration_cast<milliseconds>(
            violation.timestamp.time_since_epoch()
        ).count();
        
        if (violation.measured_value.has_value()) {
            violation_json["measured_value"] = violation.measured_value.value();
        }
        if (violation.limit_value.has_value()) {
            violation_json["limit_value"] = violation.limit_value.value();
        }
        
        state_engine_->set("ai.guardrails.violations." + std::to_string(execution_id), 
                            violation_json.dump());
    }
}

void AIExecutionGuardrails::update_stats(const ExecutionState& state, const GuardrailResult& result) {
    for (const auto& violation : result.violations) {
        switch (violation.severity) {
            case ViolationSeverity::Warning:
                stats_.warnings_issued++;
                break;
            case ViolationSeverity::Critical:
                stats_.critical_violations++;
                break;
            case ViolationSeverity::Fatal:
                stats_.fatal_violations++;
                break;
        }
    }
    
    if (!result.allowed) {
        stats_.blocked_executions++;
    }
}

void AIExecutionGuardrails::cleanup_inactive_executions() {
    const auto now = steady_clock::now();
    
    for (auto it = executions_.begin(); it != executions_.end(); ) {
        const ExecutionState& state = it->second;
        
        // Remove executions that have been inactive for more than 5 minutes
        if (!state.active && (now - state.last_monitor_time) > minutes(5)) {
            it = executions_.erase(it);
        } else {
            ++it;
        }
    }
}

void AIExecutionGuardrails::handle_state_update(const RawrXD::Core::StateKey& key, const RawrXD::Core::StateValue& value) {
    // Handle state updates from the state engine
    // This allows other components to influence guardrail behavior
}

void AIExecutionGuardrails::handle_guardrail_config_update(const std::string& config_json) {
    try {
        json config = json::parse(config_json);
        
        if (config.contains("default_constraints")) {
            default_constraints_ = constraints_from_json(config["default_constraints"].dump());
        }
        
        // Handle other configuration updates as needed
        
    } catch (const std::exception& e) {
        // Log configuration parse error
        if (state_engine_) {
            state_engine_->set("ai.guardrails.config_error", 
                              "Failed to parse configuration: " + std::string(e.what()));
        }
    }
}

// Factory functions
std::unique_ptr<AIExecutionGuardrails> create_default_guardrails_system() {
    return std::make_unique<AIExecutionGuardrails>();
}

std::unique_ptr<AIExecutionGuardrails> create_minimal_guardrails_system() {
    auto guardrails = std::make_unique<AIExecutionGuardrails>();
    
    // Remove all guardrails and add only essential ones
    // Implementation would clear and re-add specific guardrails
    
    return guardrails;
}

std::unique_ptr<AIExecutionGuardrails> create_strict_guardrails_system() {
    auto guardrails = std::make_unique<AIExecutionGuardrails>();
    
    // Add additional strict guardrails
    // Implementation would add more restrictive guardrails
    
    return guardrails;
}

// Utility functions
ExecutionConstraints create_default_constraints() {
    return ExecutionConstraints{
        .max_tokens = 4096,
        .max_temperature = 1.0f,
        .max_duration = minutes(2),
        .max_concurrent_requests = 10,
        .max_memory_mb = 512.0,
        .allow_network_access = false,
        .allow_file_system_access = false,
        .allow_system_calls = false
    };
}

ExecutionConstraints create_strict_constraints() {
    return ExecutionConstraints{
        .max_tokens = 2048,
        .max_temperature = 0.7f,
        .max_duration = minutes(1),
        .max_concurrent_requests = 5,
        .max_memory_mb = 256.0,
        .allow_network_access = false,
        .allow_file_system_access = false,
        .allow_system_calls = false,
        .blocked_topics = {"harm", "danger", "illegal", "exploit"}
    };
}

ExecutionConstraints create_permissive_constraints() {
    return ExecutionConstraints{
        .max_tokens = 8192,
        .max_temperature = 2.0f,
        .max_duration = minutes(5),
        .max_concurrent_requests = 20,
        .max_memory_mb = 1024.0,
        .allow_network_access = true,
        .allow_file_system_access = true,
        .allow_system_calls = false
    };
}

bool validate_constraints(const ExecutionConstraints& constraints) {
    if (constraints.max_tokens.has_value() && constraints.max_tokens.value() == 0) {
        return false;
    }
    
    if (constraints.max_temperature.has_value() && 
        (constraints.max_temperature.value() < 0.0 || constraints.max_temperature.value() > 2.0)) {
        return false;
    }
    
    if (constraints.max_duration.has_value() && constraints.max_duration.value() < milliseconds(10)) {
        return false;
    }
    
    if (constraints.max_memory_mb.has_value() && constraints.max_memory_mb.value() < 1.0) {
        return false;
    }
    
    return true;
}

std::string constraints_to_json(const ExecutionConstraints& constraints) {
    json j;
    
    if (constraints.max_tokens.has_value()) {
        j["max_tokens"] = constraints.max_tokens.value();
    }
    if (constraints.max_temperature.has_value()) {
        j["max_temperature"] = constraints.max_temperature.value();
    }
    if (constraints.max_duration.has_value()) {
        j["max_duration_ms"] = constraints.max_duration.value().count();
    }
    if (constraints.max_concurrent_requests.has_value()) {
        j["max_concurrent_requests"] = constraints.max_concurrent_requests.value();
    }
    if (constraints.max_memory_mb.has_value()) {
        j["max_memory_mb"] = constraints.max_memory_mb.value();
    }
    
    j["allow_network_access"] = constraints.allow_network_access;
    j["allow_file_system_access"] = constraints.allow_file_system_access;
    j["allow_system_calls"] = constraints.allow_system_calls;
    
    if (!constraints.allowed_models.empty()) {
        j["allowed_models"] = constraints.allowed_models;
    }
    if (!constraints.blocked_topics.empty()) {
        j["blocked_topics"] = constraints.blocked_topics;
    }
    if (!constraints.allowed_functions.empty()) {
        j["allowed_functions"] = constraints.allowed_functions;
    }
    
    return j.dump();
}

ExecutionConstraints constraints_from_json(const std::string& json_str) {
    ExecutionConstraints constraints;
    
    try {
        json j = json::parse(json_str);
        
        if (j.contains("max_tokens")) {
            constraints.max_tokens = j["max_tokens"].get<TokenCount>();
        }
        if (j.contains("max_temperature")) {
            constraints.max_temperature = j["max_temperature"].get<Temperature>();
        }
        if (j.contains("max_duration_ms")) {
            constraints.max_duration = milliseconds(j["max_duration_ms"].get<int64_t>());
        }
        if (j.contains("max_concurrent_requests")) {
            constraints.max_concurrent_requests = j["max_concurrent_requests"].get<uint32_t>();
        }
        if (j.contains("max_memory_mb")) {
            constraints.max_memory_mb = j["max_memory_mb"].get<double>();
        }
        
        if (j.contains("allow_network_access")) {
            constraints.allow_network_access = j["allow_network_access"].get<bool>();
        }
        if (j.contains("allow_file_system_access")) {
            constraints.allow_file_system_access = j["allow_file_system_access"].get<bool>();
        }
        if (j.contains("allow_system_calls")) {
            constraints.allow_system_calls = j["allow_system_calls"].get<bool>();
        }
        
        if (j.contains("allowed_models")) {
            constraints.allowed_models = j["allowed_models"].get<std::vector<ModelId>>();
        }
        if (j.contains("blocked_topics")) {
            constraints.blocked_topics = j["blocked_topics"].get<std::vector<std::string>>();
        }
        if (j.contains("allowed_functions")) {
            constraints.allowed_functions = j["allowed_functions"].get<std::vector<std::string>>();
        }
        
    } catch (const std::exception& e) {
        // Return default constraints on parse error
        constraints = create_default_constraints();
    }
    
    return constraints;
}

GuardrailViolation create_violation(
    ViolationSeverity severity,
    const std::string& message,
    const std::string& constraint_type,
    std::optional<double> measured_value,
    std::optional<double> limit_value
) {
    return GuardrailViolation{
        .severity = severity,
        .message = message,
        .constraint_type = constraint_type,
        .measured_value = measured_value,
        .limit_value = limit_value,
        .timestamp = steady_clock::now()
    };
}

bool should_terminate_execution(const std::vector<GuardrailViolation>& violations) {
    for (const auto& violation : violations) {
        if (violation.severity == ViolationSeverity::Fatal) {
            return true;
        }
    }
    return false;
}

} // namespace rawrxd::ai
