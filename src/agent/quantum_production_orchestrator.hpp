#pragma once

#include "quantum_autonomous_todo_system.hpp"
#include "quantum_multi_model_agent_cycling.hpp"
#include "quantum_dynamic_time_manager.hpp"
#include "agentic_deep_thinking_engine.hpp"
#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations
extern "C" {
    void __stdcall masm_production_orchestrator(const void* system_state, void* optimization_commands);
    void __stdcall masm_quality_optimizer(const void* code_metrics, float* quality_scores, size_t count);
    void __stdcall masm_autonomous_scheduler(const void* task_queue, int* execution_order, size_t task_count);
}

namespace RawrXD::Agent {

/**
 * @class QuantumProductionOrchestrator
 * @brief Master orchestrator for all quantum autonomous systems
 * 
 * Features:
 * - Unified control of all quantum subsystems
 * - Production readiness monitoring and enforcement
 * - Autonomous task generation and execution
 * - Real-time quality/speed/balance optimization
 * - Self-healing and adaptive behavior
 * - MASM-accelerated orchestration for maximum performance
 */
class QuantumProductionOrchestrator {
public:
    enum class OrchestratorMode : uint8_t {
        Conservative = 1,    // Safety first, slower execution
        Balanced = 2,       // Standard production mode
        Aggressive = 3,     // Maximum performance
        Quantum = 4         // Transcendent optimization
    };
    
    enum class SystemStatus : uint8_t {
        Initializing = 1,
        Idle = 2,
        Active = 3,
        Overloaded = 4,
        Self_Healing = 5,
        Emergency_Shutdown = 6
    };
    
    struct ProductionConfig {
        // Core settings
        OrchestratorMode mode;
        bool enable_autonomous_execution;
        bool enable_production_audits;
        bool enable_self_healing;
        bool enable_quantum_optimization;
        
        // Quality enforcement
        float min_production_readiness;
        float min_code_quality;
        float min_performance_score;
        float min_security_score;
        
        // Timing and limits
        std::chrono::minutes audit_interval;
        std::chrono::hours max_autonomous_runtime;
        int max_concurrent_tasks;
        int max_agent_count;
        
        // PowerShell configuration
        bool enable_pwsh_randomization;
        std::chrono::minutes default_pwsh_timeout;
        std::chrono::minutes max_pwsh_timeout;
        
        // Self-optimization
        bool enable_adaptive_thresholds;
        float adaptation_rate;
        bool enable_predictive_scaling;
        
        ProductionConfig(
            OrchestratorMode orchestratorMode = OrchestratorMode::Balanced,
            bool enableAutonomousExecution = true,
            bool enableProductionAudits = true,
            bool enableSelfHealing = true,
            bool enableQuantumOptimization = true,
            float minProductionReadiness = 0.85f,
            float minCodeQuality = 0.80f,
            float minPerformanceScore = 0.75f,
            float minSecurityScore = 0.95f,
            std::chrono::minutes auditInterval = std::chrono::minutes(30),
            std::chrono::hours maxAutonomousRuntime = std::chrono::hours(24),
            int maxConcurrentTasks = 20,
            int maxAgentCount = 99,
            bool enablePwshRandomization = true,
            std::chrono::minutes defaultPwshTimeout = std::chrono::minutes(10),
            std::chrono::minutes maxPwshTimeout = std::chrono::minutes(60),
            bool enableAdaptiveThresholds = true,
            float adaptationRate = 0.1f,
            bool enablePredictiveScaling = true)
            : mode(orchestratorMode),
              enable_autonomous_execution(enableAutonomousExecution),
              enable_production_audits(enableProductionAudits),
              enable_self_healing(enableSelfHealing),
              enable_quantum_optimization(enableQuantumOptimization),
              min_production_readiness(minProductionReadiness),
              min_code_quality(minCodeQuality),
              min_performance_score(minPerformanceScore),
              min_security_score(minSecurityScore),
              audit_interval(auditInterval),
              max_autonomous_runtime(maxAutonomousRuntime),
              max_concurrent_tasks(maxConcurrentTasks),
              max_agent_count(maxAgentCount),
              enable_pwsh_randomization(enablePwshRandomization),
              default_pwsh_timeout(defaultPwshTimeout),
              max_pwsh_timeout(maxPwshTimeout),
              enable_adaptive_thresholds(enableAdaptiveThresholds),
              adaptation_rate(adaptationRate),
              enable_predictive_scaling(enablePredictiveScaling) {}
    };
    
    struct SystemMetrics {
        // Overall system state
        SystemStatus status = SystemStatus::Initializing;
        std::chrono::steady_clock::time_point uptime_start;
        
        // Task statistics
        int total_tasks_generated = 0;
        int tasks_completed_successfully = 0;
        int tasks_failed = 0;
        int autonomous_executions = 0;
        
        // Quality metrics
        float avg_production_readiness = 0.0f;
        float avg_code_quality = 0.0f;
        float avg_performance_score = 0.0f;
        float system_efficiency = 0.0f;
        
        // Resource utilization
        float cpu_usage_percent = 0.0f;
        float memory_usage_gb = 0.0f;
        int active_agents = 0;
        int active_pwsh_sessions = 0;
        
        // Adaptation metrics
        int self_healing_events = 0;
        int optimization_cycles = 0;
        float adaptation_effectiveness = 0.0f;
        
        SystemMetrics() : uptime_start(std::chrono::steady_clock::now()) {}
    };
    
    struct AutonomousRequest {
        std::string request_text;
        int priority = 5;                    // 1-10 scale
        bool requires_production_audit = true;
        int max_agent_count = 8;
        std::chrono::minutes timeout{60};
        
        // Generated task metadata
        std::string request_id;
        std::chrono::system_clock::time_point created_at;
        std::vector<TaskDefinition> generated_tasks;
        
        AutonomousRequest() : created_at(std::chrono::system_clock::now()) {
            request_id = "req_" + std::to_string(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    created_at.time_since_epoch()).count());
        }
    };

public:
    explicit QuantumProductionOrchestrator(ProductionConfig config = ProductionConfig{});
    ~QuantumProductionOrchestrator();
    
    // Core orchestration
    bool initialize();
    void shutdown();
    void startAutonomousOperation();
    void stopAutonomousOperation();
    
    // Request processing (main user interface)
    std::string processRequest(const std::string& request);
    std::vector<TaskDefinition> generateTodosFromRequest(const std::string& request);
    std::vector<ExecutionResult> executeTop20MostDifficult();
    
    // Production auditing
    struct ProductionAuditSummary {
        float overall_readiness;
        int critical_issues;
        int major_issues;
        int minor_issues;
        std::vector<TaskDefinition> improvement_tasks;
        std::chrono::system_clock::time_point audit_time;
        
        ProductionAuditSummary() : overall_readiness(0.0f), critical_issues(0),
                                  major_issues(0), minor_issues(0),
                                  audit_time(std::chrono::system_clock::now()) {}
    };
    
    ProductionAuditSummary performFullProductionAudit();
    void schedulePeriodicAudits(std::chrono::minutes interval);
    
    // Quality/Speed/Balance optimization
    void optimizeSystemBalance();
    void setOptimizationMode(OrchestratorMode mode);
    void enableAdaptiveOptimization(bool enable = true);
    
    // Multi-agent management
    void scaleAgents(int target_count);
    void optimizeAgentDistribution();
    void rebalanceWorkload();
    
    // PowerShell integration
    void configurePowerShellLimits(std::chrono::minutes min_timeout, std::chrono::minutes max_timeout);
    void enablePowerShellRandomization(bool enable, float variation_factor = 0.3f);
    
    // Monitoring and diagnostics
    SystemMetrics getSystemMetrics() const;
    std::string generateStatusReport() const;
    std::vector<std::string> getRecommendations() const;
    
    // Advanced features
    void enableQuantumMode(bool enable = true);
    void setQualityThresholds(float production, float code, float performance, float security);
    void configureSelfHealing(bool enable, float sensitivity = 0.5f);
    
    // Configuration management
    void updateConfig(const ProductionConfig& config);
    ProductionConfig getConfig() const;

private:
    // Core orchestration loops
    void orchestrationMainLoop();
    void productionAuditLoop();
    void systemOptimizationLoop();
    void selfHealingLoop();
    
    // Task management
    void processTaskQueue();
    void prioritizeTaskQueue();
    void monitorTaskExecution();
    
    // Quality assurance
    bool validateProductionReadiness(const ExecutionResult& result);
    std::vector<TaskDefinition> generateImprovementTasks(const ProductionAuditSummary& audit);
    void enforceQualityStandards();
    
    // System optimization
    void adaptiveTuning();
    void predictiveScaling();
    void resourceOptimization();
    
    // Self-healing mechanisms
    void detectSystemIssues();
    void performSelfHealing();
    void emergencyShutdownProtection();
    
    // MASM integration
    void initializeMasmOrchestration();
    void shutdownMasmOrchestration();
    bool useMasmOptimization() const { return m_masm_enabled && m_config.enable_quantum_optimization; }
    
    // Subsystem management
    std::unique_ptr<QuantumAutonomousTodoSystem> m_todo_system;
    std::unique_ptr<QuantumMultiModelAgentCycling> m_agent_cycling;
    std::unique_ptr<QuantumDynamicTimeManager> m_time_manager;
    std::unique_ptr<AgenticDeepThinkingEngine> m_thinking_engine;
    
    // Configuration and state
    ProductionConfig m_config;
    SystemMetrics m_metrics;
    
    // Threading
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_autonomous_enabled{false};
    std::thread m_orchestration_thread;
    std::thread m_audit_thread;
    std::thread m_optimization_thread;
    std::thread m_self_healing_thread;
    
    // Task management
    mutable std::mutex m_task_queue_mutex;
    std::queue<AutonomousRequest> m_request_queue;
    std::queue<TaskDefinition> m_task_queue;
    std::map<std::string, ExecutionResult> m_execution_results;
    
    // Synchronization
    mutable std::mutex m_config_mutex;
    mutable std::mutex m_metrics_mutex;
    std::condition_variable m_task_available;
    
    // MASM acceleration
    bool m_masm_enabled;
    void* m_masm_orchestration_context;
    
    // Self-healing state
    std::atomic<bool> m_self_healing_active{false};
    std::chrono::steady_clock::time_point m_last_health_check;
    
    // Performance monitoring
    std::chrono::steady_clock::time_point m_system_start_time;
    std::vector<float> m_performance_history;
    std::vector<float> m_quality_history;
    
    // Adaptive thresholds
    float m_adaptive_production_threshold;
    float m_adaptive_quality_threshold;
    float m_adaptive_performance_threshold;
};

/**
 * @class GlobalQuantumInterface
 * @brief Global singleton interface for all quantum operations
 * 
 * Provides a single point of access for the entire quantum system
 */
class GlobalQuantumInterface {
public:
    static GlobalQuantumInterface& instance();
    
    // Main interface - this is what users call
    std::string executeRequest(const std::string& request);
    
    // System control
    bool initializeSystem();
    void shutdownSystem();
    
    // Configuration shortcuts
    void setMode(QuantumProductionOrchestrator::OrchestratorMode mode);
    void enableAutonomousMode(bool enable = true);
    void setQualityLevel(float level);  // 0.0-1.0, sets all quality thresholds
    
    // Status and monitoring
    std::string getSystemStatus();
    float getProductionReadiness();
    
private:
    GlobalQuantumInterface();
    ~GlobalQuantumInterface();
    
    std::unique_ptr<QuantumProductionOrchestrator> m_orchestrator;
    mutable std::mutex m_interface_mutex;
    bool m_initialized;
};

} // namespace RawrXD::Agent

// Global convenience functions for easy access
namespace RawrXD {

// Main user interface function - call this with any request
inline std::string ProcessQuantumRequest(const std::string& request) {
    return Agent::GlobalQuantumInterface::instance().executeRequest(request);
}

// Quick system initialization 
inline bool InitializeQuantumSystem() {
    return Agent::GlobalQuantumInterface::instance().initializeSystem();
}

// Get current system status
inline std::string GetQuantumSystemStatus() {
    return Agent::GlobalQuantumInterface::instance().getSystemStatus();
}

} // namespace RawrXD
