#pragma once
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <condition_variable>
#include "copilot_gap_closer.h"

namespace RawrXD {

enum class AgentState {
    IDLE,
    PLANNING,
    EXECUTING,
    MONITORING,
    RECOVERING,
    TERMINATED
};

enum class SafetyLevel {
    UNRESTRICTED = 0,
    BASIC_CHECKS = 1,
    MODERATE_SAFETY = 2,
    HIGH_SAFETY = 3,
    MAXIMUM_SAFETY = 4
};

struct ExecutionPlan {
    std::string id;
    std::string description;
    std::vector<std::string> steps;
    std::vector<std::string> dependencies;
    std::vector<std::string> safety_checks;
    std::chrono::milliseconds estimated_duration{0};
    SafetyLevel safety_level{SafetyLevel::MODERATE_SAFETY};
    bool requires_human_approval{false};
    std::function<bool()> validation_function;
};

struct ExecutionContext {
    std::string workspace_path;
    std::unordered_map<std::string, std::string> environment_vars;
    std::vector<std::string> available_tools;
    std::vector<std::string> restricted_operations;
    size_t max_memory_usage{1024 * 1024 * 1024}; // 1GB
    std::chrono::milliseconds max_execution_time{300000}; // 5 minutes
};

struct SafetyGate {
    std::string name;
    std::function<bool(const ExecutionContext&)> check_function;
    std::string failure_message;
    bool is_critical{false};
};

class AutonomousAgenticOrchestrator {
public:
    AutonomousAgenticOrchestrator();
    ~AutonomousAgenticOrchestrator();

    // Core orchestration
    bool Initialize(const ExecutionContext& context);
    void Shutdown();
    
    // Plan management
    std::string CreatePlan(const std::string& objective, SafetyLevel safety = SafetyLevel::MODERATE_SAFETY);
    bool ExecutePlan(const std::string& plan_id);
    bool CancelPlan(const std::string& plan_id);
    
    // Advanced autonomous features
    void EnableAutonomousMode(bool enable);
    void SetSafetyLevel(SafetyLevel level);
    void AddSafetyGate(const SafetyGate& gate);
    
    // Multi-step execution with safety
    bool ExecuteMultiStepPlan(const std::vector<std::string>& steps, 
                             const std::vector<SafetyGate>& gates = {});
    
    // Real-time monitoring
    AgentState GetCurrentState() const { return current_state_.load(); }
    std::string GetCurrentActivity() const;
    double GetProgressPercentage() const;
    
    // Integration with existing systems
    void SetCopilotGapCloser(std::shared_ptr<CopilotGapCloser> gap_closer);
    void SetTaskDispatcher(std::shared_ptr<TaskDispatcher> dispatcher);
    
    // Callbacks for external integration
    std::function<void(const std::string&)> onPlanCreated;
    std::function<void(const std::string&, bool)> onPlanCompleted;
    std::function<void(const std::string&, const std::string&)> onSafetyViolation;
    std::function<bool(const std::string&)> onHumanApprovalRequired;

private:
    std::atomic<AgentState> current_state_{AgentState::IDLE};
    std::atomic<bool> autonomous_mode_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    ExecutionContext execution_context_;
    SafetyLevel current_safety_level_{SafetyLevel::MODERATE_SAFETY};
    
    std::unordered_map<std::string, ExecutionPlan> plans_;
    std::queue<std::string> execution_queue_;
    std::vector<SafetyGate> safety_gates_;
    
    mutable std::mutex plans_mutex_;
    mutable std::mutex queue_mutex_;
    mutable std::mutex safety_mutex_;
    
    std::thread orchestrator_thread_;
    std::thread monitoring_thread_;
    std::condition_variable execution_cv_;
    
    // Integration components
    std::shared_ptr<CopilotGapCloser> gap_closer_;
    std::shared_ptr<TaskDispatcher> task_dispatcher_;
    
    // Internal methods
    void OrchestratorLoop();
    void MonitoringLoop();
    bool ValidatePlan(const ExecutionPlan& plan);
    bool ExecuteSafetyChecks(const ExecutionPlan& plan);
    std::string GeneratePlanId();
    ExecutionPlan CreateExecutionPlan(const std::string& objective, SafetyLevel safety);
    bool ExecuteStep(const std::string& step, const ExecutionContext& context);
    void HandleSafetyViolation(const std::string& violation);
    void RecoverFromFailure(const std::string& error);
};

} // namespace RawrXD