#include "autonomous_agentic_orchestrator.hpp"
#include <random>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cctype>

namespace RawrXD {

AutonomousAgenticOrchestrator::AutonomousAgenticOrchestrator() {
    // Initialize default safety gates
    AddSafetyGate({
        "FileSystemAccess",
        [](const ExecutionContext& ctx) {
            return std::filesystem::exists(ctx.workspace_path) && 
                   std::filesystem::is_directory(ctx.workspace_path);
        },
        "Workspace path is not accessible or does not exist",
        true
    });
    
    AddSafetyGate({
        "MemoryLimit",
        [](const ExecutionContext& ctx) {
            // Basic memory check - in production would check actual usage
            return ctx.max_memory_usage > 0 && ctx.max_memory_usage <= 8ULL * 1024 * 1024 * 1024; // 8GB max
        },
        "Memory limit exceeds safe threshold",
        false
    });
    
    AddSafetyGate({
        "ExecutionTimeLimit",
        [](const ExecutionContext& ctx) {
            return ctx.max_execution_time.count() > 0 && 
                   ctx.max_execution_time <= std::chrono::hours(1);
        },
        "Execution time limit exceeds safe threshold",
        false
    });
}

AutonomousAgenticOrchestrator::~AutonomousAgenticOrchestrator() {
    Shutdown();
}

bool AutonomousAgenticOrchestrator::Initialize(const ExecutionContext& context) {
    execution_context_ = context;
    current_state_ = AgentState::IDLE;
    shutdown_requested_ = false;
    
    // Validate execution context
    for (const auto& gate : safety_gates_) {
        if (gate.is_critical && !gate.check_function(execution_context_)) {
            if (onSafetyViolation) {
                onSafetyViolation("Initialization", gate.failure_message);
            }
            return false;
        }
    }
    
    // Start orchestrator thread
    orchestrator_thread_ = std::thread(&AutonomousAgenticOrchestrator::OrchestratorLoop, this);
    monitoring_thread_ = std::thread(&AutonomousAgenticOrchestrator::MonitoringLoop, this);
    
    return true;
}

void AutonomousAgenticOrchestrator::Shutdown() {
    shutdown_requested_ = true;
    current_state_ = AgentState::TERMINATED;
    
    execution_cv_.notify_all();
    
    if (orchestrator_thread_.joinable()) {
        orchestrator_thread_.join();
    }
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

std::string AutonomousAgenticOrchestrator::CreatePlan(const std::string& objective, SafetyLevel safety) {
    std::lock_guard<std::mutex> lock(plans_mutex_);
    
    ExecutionPlan plan = CreateExecutionPlan(objective, safety);
    
    if (!ValidatePlan(plan)) {
        return "";
    }
    
    plans_[plan.id] = plan;
    
    if (onPlanCreated) {
        onPlanCreated(plan.id);
    }
    
    return plan.id;
}

bool AutonomousAgenticOrchestrator::ExecutePlan(const std::string& plan_id) {
    std::lock_guard<std::mutex> plans_lock(plans_mutex_);
    
    auto it = plans_.find(plan_id);
    if (it == plans_.end()) {
        return false;
    }
    
    const ExecutionPlan& plan = it->second;
    
    // Check if human approval is required
    if (plan.requires_human_approval) {
        if (onHumanApprovalRequired && !onHumanApprovalRequired(plan_id)) {
            return false;
        }
    }
    
    // Execute safety checks
    if (!ExecuteSafetyChecks(plan)) {
        return false;
    }
    
    // Add to execution queue
    {
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);
        execution_queue_.push(plan_id);
    }
    
    execution_cv_.notify_one();
    return true;
}

bool AutonomousAgenticOrchestrator::CancelPlan(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(plans_mutex_);
    
    auto it = plans_.find(plan_id);
    if (it != plans_.end()) {
        plans_.erase(it);
        return true;
    }
    
    return false;
}

void AutonomousAgenticOrchestrator::EnableAutonomousMode(bool enable) {
    autonomous_mode_ = enable;
    if (enable) {
        current_state_ = AgentState::MONITORING;
    } else {
        current_state_ = AgentState::IDLE;
    }
}

void AutonomousAgenticOrchestrator::SetSafetyLevel(SafetyLevel level) {
    std::lock_guard<std::mutex> lock(safety_mutex_);
    current_safety_level_ = level;
}

void AutonomousAgenticOrchestrator::AddSafetyGate(const SafetyGate& gate) {
    std::lock_guard<std::mutex> lock(safety_mutex_);
    safety_gates_.push_back(gate);
}

bool AutonomousAgenticOrchestrator::ExecuteMultiStepPlan(const std::vector<std::string>& steps, 
                                                        const std::vector<SafetyGate>& gates) {
    current_state_ = AgentState::EXECUTING;
    
    // Add temporary safety gates
    {
        std::lock_guard<std::mutex> lock(safety_mutex_);
        for (const auto& gate : gates) {
            safety_gates_.push_back(gate);
        }
    }
    
    bool success = true;
    
    for (size_t i = 0; i < steps.size(); ++i) {
        // Check safety gates before each step
        for (const auto& gate : gates) {
            if (!gate.check_function(execution_context_)) {
                HandleSafetyViolation(gate.failure_message);
                success = false;
                break;
            }
        }
        
        if (!success) break;
        
        // Execute step
        if (!ExecuteStep(steps[i], execution_context_)) {
            RecoverFromFailure("Step execution failed: " + steps[i]);
            success = false;
            break;
        }
        
        // Brief pause between steps for monitoring
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Remove temporary safety gates
    {
        std::lock_guard<std::mutex> lock(safety_mutex_);
        for (size_t i = 0; i < gates.size(); ++i) {
            if (!safety_gates_.empty()) {
                safety_gates_.pop_back();
            }
        }
    }
    
    current_state_ = autonomous_mode_ ? AgentState::MONITORING : AgentState::IDLE;
    return success;
}

std::string AutonomousAgenticOrchestrator::GetCurrentActivity() const {
    switch (current_state_.load()) {
        case AgentState::IDLE: return "Idle - Waiting for tasks";
        case AgentState::PLANNING: return "Planning - Creating execution strategy";
        case AgentState::EXECUTING: return "Executing - Running planned tasks";
        case AgentState::MONITORING: return "Monitoring - Watching for autonomous opportunities";
        case AgentState::RECOVERING: return "Recovering - Handling errors and failures";
        case AgentState::TERMINATED: return "Terminated - Shutting down";
        default: return "Unknown state";
    }
}

double AutonomousAgenticOrchestrator::GetProgressPercentage() const {
    // Simplified progress calculation
    std::lock_guard<std::mutex> lock(plans_mutex_);
    if (plans_.empty()) return 0.0;
    
    size_t completed = 0;
    for (const auto& [id, plan] : plans_) {
        // In a real implementation, we'd track step completion
        completed++; // Placeholder
    }
    
    return (double)completed / plans_.size() * 100.0;
}

void AutonomousAgenticOrchestrator::SetCopilotGapCloser(std::shared_ptr<CopilotGapCloser> gap_closer) {
    gap_closer_ = gap_closer;
}

void AutonomousAgenticOrchestrator::SetTaskDispatcher(std::shared_ptr<TaskDispatcher> dispatcher) {
    task_dispatcher_ = dispatcher;
}

void AutonomousAgenticOrchestrator::OrchestratorLoop() {
    while (!shutdown_requested_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        execution_cv_.wait(lock, [this] {
            return !execution_queue_.empty() || shutdown_requested_;
        });
        
        if (shutdown_requested_) break;
        
        if (!execution_queue_.empty()) {
            std::string plan_id = execution_queue_.front();
            execution_queue_.pop();
            lock.unlock();
            
            current_state_ = AgentState::EXECUTING;
            
            // Execute the plan
            bool success = false;
            {
                std::lock_guard<std::mutex> plans_lock(plans_mutex_);
                auto it = plans_.find(plan_id);
                if (it != plans_.end()) {
                    const ExecutionPlan& plan = it->second;
                    success = ExecuteMultiStepPlan(plan.steps);
                }
            }
            
            if (onPlanCompleted) {
                onPlanCompleted(plan_id, success);
            }
            
            current_state_ = autonomous_mode_ ? AgentState::MONITORING : AgentState::IDLE;
        }
    }
}

void AutonomousAgenticOrchestrator::MonitoringLoop() {
    uint32_t monitorTicks = 0;
    while (!shutdown_requested_) {
        if (autonomous_mode_ && current_state_ == AgentState::MONITORING) {
            if ((monitorTicks++ % 8) == 0) {
                const std::string planId = CreatePlan(
                    "autonomous parity closure scan and action cycle",
                    current_safety_level_
                );
                if (!planId.empty()) {
                    ExecutePlan(planId);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

bool AutonomousAgenticOrchestrator::ValidatePlan(const ExecutionPlan& plan) {
    if (plan.steps.empty()) return false;
    if (plan.estimated_duration.count() <= 0) return false;
    
    // Validate against current safety level
    if (static_cast<int>(plan.safety_level) < static_cast<int>(current_safety_level_)) {
        return false;
    }
    
    return true;
}

bool AutonomousAgenticOrchestrator::ExecuteSafetyChecks(const ExecutionPlan& plan) {
    std::lock_guard<std::mutex> lock(safety_mutex_);
    
    for (const auto& gate : safety_gates_) {
        if (!gate.check_function(execution_context_)) {
            if (gate.is_critical) {
                HandleSafetyViolation(gate.failure_message);
                return false;
            }
        }
    }
    
    return true;
}

std::string AutonomousAgenticOrchestrator::GeneratePlanId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    return "plan_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

ExecutionPlan AutonomousAgenticOrchestrator::CreateExecutionPlan(const std::string& objective, SafetyLevel safety) {
    ExecutionPlan plan;
    plan.id = GeneratePlanId();
    plan.description = objective;
    plan.safety_level = safety;

    std::string lowerObjective = objective;
    std::transform(lowerObjective.begin(), lowerObjective.end(), lowerObjective.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lowerObjective.find("analyze") != std::string::npos || lowerObjective.find("audit") != std::string::npos) {
        plan.steps = {
            "Initialize analysis context",
            "Scan target files/directories", 
            "Perform static analysis",
            "Generate analysis report",
            "Validate results"
        };
        plan.estimated_duration = std::chrono::seconds(30);
    } else if (lowerObjective.find("refactor") != std::string::npos || lowerObjective.find("rewrite") != std::string::npos) {
        plan.steps = {
            "Backup original files",
            "Analyze code structure",
            "Generate refactoring plan",
            "Apply transformations",
            "Run tests and validation",
            "Commit changes"
        };
        plan.estimated_duration = std::chrono::minutes(5);
        plan.requires_human_approval = (safety >= SafetyLevel::HIGH_SAFETY);
    } else if (lowerObjective.find("optimize") != std::string::npos || lowerObjective.find("performance") != std::string::npos) {
        plan.steps = {
            "Profile current performance",
            "Identify bottlenecks",
            "Generate optimization candidates",
            "Apply optimizations",
            "Measure performance improvements",
            "Validate correctness"
        };
        plan.estimated_duration = std::chrono::minutes(10);
    } else if (lowerObjective.find("parity") != std::string::npos || lowerObjective.find("autonomous") != std::string::npos) {
        plan.steps = {
            "Map current feature parity and agentic capability graph",
            "Detect scaffolded and stubbed execution paths",
            "Generate closure plan for missing autonomous workflows",
            "Execute highest impact closure actions",
            "Validate safety and produce execution report"
        };
        plan.estimated_duration = std::chrono::minutes(4);
    } else {
        // Generic plan
        plan.steps = {
            "Parse objective requirements",
            "Create execution strategy",
            "Execute primary task",
            "Validate results",
            "Generate completion report"
        };
        plan.estimated_duration = std::chrono::minutes(2);
    }
    
    // Add safety checks based on safety level
    if (safety >= SafetyLevel::MODERATE_SAFETY) {
        plan.safety_checks.push_back("Validate file permissions");
        plan.safety_checks.push_back("Check resource limits");
    }
    if (safety >= SafetyLevel::HIGH_SAFETY) {
        plan.safety_checks.push_back("Backup critical files");
        plan.safety_checks.push_back("Create rollback plan");
        plan.requires_human_approval = true;
    }
    if (safety == SafetyLevel::MAXIMUM_SAFETY) {
        plan.safety_checks.push_back("Full system state snapshot");
        plan.safety_checks.push_back("Multi-point validation");
        plan.requires_human_approval = true;
    }
    
    return plan;
}

bool AutonomousAgenticOrchestrator::ExecuteStep(const std::string& step, const ExecutionContext& context) {
    constexpr int32_t kTaskStatusReady = 0;
    constexpr int32_t kTaskStatusProcessing = 1;
    constexpr int32_t kTaskStatusCompleted = 2;
    constexpr int32_t kTaskStatusFailed = 3;
    constexpr int32_t kTaskStatusCancelled = 4;

    // Integration with task dispatcher runtime
    if (task_dispatcher_) {
        int32_t task_id = task_dispatcher_->Submit(step, {});
        if (task_id > 0) {
            const auto started = std::chrono::steady_clock::now();
            while (true) {
                const int32_t status = task_dispatcher_->GetStatus(task_id);
                if (status == kTaskStatusCompleted) {
                    return true;
                }
                if (status == kTaskStatusFailed || status == kTaskStatusCancelled) {
                    return false;
                }
                if (status != kTaskStatusReady && status != kTaskStatusProcessing) {
                    return false;
                }
                if (std::chrono::steady_clock::now() - started > std::chrono::seconds(10)) {
                    task_dispatcher_->Cancel(task_id);
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
            }
        }
    }
    
    // Fallback execution
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate work
    return true; // Simplified success
}

void AutonomousAgenticOrchestrator::HandleSafetyViolation(const std::string& violation) {
    current_state_ = AgentState::RECOVERING;
    
    if (onSafetyViolation) {
        onSafetyViolation("Safety Gate", violation);
    }
    
    // Implement recovery logic here
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    current_state_ = autonomous_mode_ ? AgentState::MONITORING : AgentState::IDLE;
}

void AutonomousAgenticOrchestrator::RecoverFromFailure(const std::string& error) {
    current_state_ = AgentState::RECOVERING;
    
    // Implement failure recovery logic here
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    current_state_ = autonomous_mode_ ? AgentState::MONITORING : AgentState::IDLE;
}

} // namespace RawrXD