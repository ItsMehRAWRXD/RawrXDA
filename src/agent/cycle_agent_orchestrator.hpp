#pragma once
#ifndef CYCLE_AGENT_ORCHESTRATOR_HPP
#define CYCLE_AGENT_ORCHESTRATOR_HPP

/**
 * @file cycle_agent_orchestrator.hpp
 * @brief Ultra-advanced cycle agent system supporting 1x-99x agents
 * 
 * Features:
 * - Dynamic agent scaling from 1 to 99 agents
 * - Round-robin and intelligent scheduling algorithms
 * - Load-aware agent selection and balancing
 * - Agent health monitoring and auto-recovery
 * - Quantum-enhanced cycle optimization
 * - Production-ready failover and redundancy
 * - Advanced performance monitoring and metrics
 * - Autonomous agent lifecycle management
 * 
 * @author RawrXD Quantum Agent Orchestration Team
 * @version 7.0 REV ULTIMATE
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <bitset>
#include <cmath>
#include <algorithm>

// Forward declarations
class AgenticDeepThinkingEngine;
class AgenticFailureDetector;
class TelemetryCollector;

namespace RawrXD::AgentCycle {

// =====================================================================
// CYCLE AGENT CONFIGURATION STRUCTURES
// =====================================================================

/// Agent type classification for specialized processing
enum class AgentType : uint8_t {
    GENERAL_PURPOSE = 0,    ///< General-purpose agent for any task
    CODE_ANALYZER = 1,      ///< Specialized in code analysis
    CODE_GENERATOR = 2,     ///< Specialized in code generation
    DEBUGGER = 3,          ///< Specialized in debugging
    OPTIMIZER = 4,         ///< Specialized in optimization
    TESTER = 5,            ///< Specialized in testing
    DOCUMENTER = 6,        ///< Specialized in documentation
    SECURITY_AUDITOR = 7,  ///< Specialized in security analysis
    PERFORMANCE_TUNER = 8, ///< Specialized in performance tuning
    QUANTUM_ENHANCED = 9   ///< Quantum-enhanced multi-purpose agent
};

/// Agent execution state for lifecycle management
enum class AgentState : uint8_t {
    INITIALIZING = 0,      ///< Agent is initializing
    READY = 1,             ///< Agent ready for tasks
    ACTIVE = 2,            ///< Agent actively processing
    BUSY = 3,              ///< Agent busy with high-priority task
    OVERLOADED = 4,        ///< Agent overloaded, need load balancing
    RECOVERING = 5,        ///< Agent recovering from error
    MAINTENANCE = 6,       ///< Agent in maintenance mode
    SHUTDOWN = 7           ///< Agent shutting down
};

/// Scheduling algorithm for agent selection
enum class SchedulingAlgorithm : uint8_t {
    ROUND_ROBIN = 0,       ///< Simple round-robin scheduling
    LOAD_BALANCED = 1,     ///< Load-aware scheduling
    PRIORITY_BASED = 2,    ///< Priority-based selection
    QUANTUM_OPTIMIZED = 3, ///< Quantum-enhanced selection
    ADAPTIVE = 4,          ///< Adaptive algorithm selection
    CAPABILITY_MATCHED = 5, ///< Match agent capabilities to task
    PERFORMANCE_WEIGHTED = 6 ///< Performance-weighted selection
};

/// Advanced agent configuration
struct AgentConfiguration {
    std::string agent_id;                           ///< Unique agent identifier
    std::string agent_name;                        ///< Human-readable agent name
    AgentType agent_type{AgentType::GENERAL_PURPOSE};
    
    // Capabilities and specializations
    std::bitset<32> capabilities;                   ///< Capability flags
    std::vector<std::string> specializations;       ///< Specialized domains
    double efficiency_rating{1.0};                 ///< Efficiency score (0.0-2.0)
    
    // Resource limits and configuration
    uint32_t max_concurrent_tasks{3};             ///< Max concurrent tasks
    std::chrono::milliseconds task_timeout{60000}; ///< Default task timeout
    size_t memory_limit{512 * 1024 * 1024};      ///< Memory limit (512MB)
    uint32_t cpu_affinity{0};                     ///< CPU core affinity (0=auto)
    
    // Performance characteristics
    std::chrono::milliseconds avg_processing_time{5000}; ///< Average processing time
    double reliability_score{1.0};                 ///< Reliability score (0.0-1.0)
    double quality_score{1.0};                    ///< Quality score (0.0-1.0)
    
    // Quantum enhancement settings
    bool quantum_enhancement_enabled{false};       ///< Enable quantum optimization
    uint32_t quantum_optimization_level{1};       ///< Optimization level (1-10)
    
    // Health and monitoring
    std::chrono::milliseconds health_check_interval{30000}; ///< Health check interval
    uint32_t max_failure_count{5};               ///< Max failures before recovery
    bool auto_recovery_enabled{true};            ///< Enable automatic recovery
    
    // Statistics tracking
    uint32_t total_tasks_processed{0};           ///< Total tasks processed
    uint32_t successful_tasks{0};                ///< Successfully completed tasks
    uint32_t failed_tasks{0};                   ///< Failed tasks
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_used;
};

/// Agent runtime information
struct AgentRuntimeInfo {
    std::string agent_id;
    AgentState current_state{AgentState::INITIALIZING};
    uint32_t active_tasks{0};                    ///< Currently active tasks
    uint32_t queued_tasks{0};                   ///< Tasks in queue
    
    // Performance metrics
    std::chrono::milliseconds current_cycle_time{0}; ///< Current cycle processing time
    std::chrono::milliseconds avg_cycle_time{0};     ///< Average cycle time
    double current_load{0.0};                   ///< Current load percentage
    double avg_load{0.0};                      ///< Average load percentage
    
    // Resource usage
    size_t memory_usage{0};                     ///< Current memory usage
    double cpu_usage{0.0};                     ///< Current CPU usage
    
    // Health status
    bool is_healthy{true};                      ///< Health status flag
    uint32_t failure_count{0};                 ///< Current failure count
    std::chrono::system_clock::time_point last_health_check;
    std::string last_error_message;             ///< Last error encountered
    
    // Execution context
    std::thread::id thread_id;                  ///< Agent thread ID
    std::chrono::system_clock::time_point cycle_start_time;
    std::chrono::system_clock::time_point last_activity;
};

/// Task request for cycle agent processing
struct CycleAgentTaskRequest {
    std::string request_id;                     ///< Unique request identifier
    std::string task_description;               ///< Task description
    std::string task_data;                      ///< Task input data
    
    // Agent selection preferences
    AgentType preferred_agent_type{AgentType::GENERAL_PURPOSE};
    std::vector<std::string> preferred_agents;   ///< Preferred agent IDs
    std::vector<std::string> excluded_agents;    ///< Excluded agent IDs
    
    // Execution configuration
    std::chrono::milliseconds timeout{0};       ///< Custom timeout (0=default)
    bool require_quantum_enhancement{false};    ///< Require quantum-enhanced agent
    uint32_t priority{5};                       ///< Task priority (1-10)
    
    // Callback functions
    std::function<void(const std::string&)> progress_callback;     ///< Progress updates
    std::function<void(const std::string&)> completion_callback;   ///< Task completion
    std::function<void(const std::string&)> error_callback;        ///< Error notifications
    
    // Advanced options
    bool allow_failover{true};                  ///< Allow failover to other agents
    bool require_exclusive_access{false};       ///< Require exclusive agent access
    std::map<std::string, std::string> metadata; ///< Additional task metadata
};

/// Task execution result from cycle agent
struct CycleAgentTaskResult {
    std::string request_id;                     ///< Original request ID
    std::string agent_id;                      ///< Agent that processed the task
    bool success{false};                       ///< Execution success flag
    
    std::string result_data;                   ///< Task result data
    std::string error_message;                 ///< Error message (if failed)
    
    // Performance metrics  
    std::chrono::milliseconds processing_time{0}; ///< Actual processing time
    std::chrono::milliseconds queue_wait_time{0}; ///< Time spent in queue
    size_t memory_used{0};                     ///< Memory used during processing
    
    // Quality metrics
    double quality_score{0.0};                ///< Result quality score
    double confidence_score{0.0};             ///< Agent's confidence in result
    
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    
    static CycleAgentTaskResult success_result(const std::string& req_id, const std::string& agent_id, const std::string& data = "") {
        CycleAgentTaskResult result;
        result.request_id = req_id;
        result.agent_id = agent_id;
        result.success = true;
        result.result_data = data;
        result.quality_score = 1.0;
        result.confidence_score = 1.0;
        return result;
    }
    
    static CycleAgentTaskResult error_result(const std::string& req_id, const std::string& error) {
        CycleAgentTaskResult result;
        result.request_id = req_id;
        result.success = false;
        result.error_message = error;
        return result;
    }
};

// =====================================================================
// CYCLE AGENT ORCHESTRATOR CLASS
// =====================================================================

/**
 * @class CycleAgentOrchestrator  
 * @brief Ultra-advanced orchestrator for 1x-99x cycle agents
 * 
 * Features:
 * - Dynamic scaling from 1 to 99 agents
 * - Intelligent scheduling and load balancing
 * - Quantum-enhanced agent selection
 * - Production-ready health monitoring and recovery
 * - Advanced performance optimization
 * - Autonomous lifecycle management
 */
class CycleAgentOrchestrator {
public:
    // ================================================================
    // CONSTRUCTION AND INITIALIZATION
    // ================================================================
    
    CycleAgentOrchestrator();
    ~CycleAgentOrchestrator();
    
    /// Initialize the orchestrator
    bool initialize(uint8_t initial_agent_count = 4,
                   SchedulingAlgorithm default_algorithm = SchedulingAlgorithm::ADAPTIVE,
                   bool enable_quantum_optimization = true);
    
    /// Shutdown the orchestrator and all agents
    void shutdown();
    
    /// Check if orchestrator is ready
    bool is_ready() const { return initialized_.load(); }
    
    // ================================================================
    // AGENT LIFECYCLE MANAGEMENT
    // ================================================================
    
    /// Add a new agent to the cycle
    std::string add_agent(const AgentConfiguration& config);
    
    /// Remove an agent from the cycle
    bool remove_agent(const std::string& agent_id, bool graceful_shutdown = true);
    
    /// Scale agent count (1-99 agents)
    bool scale_agents(uint8_t target_count, bool preserve_existing = true);
    
    /// Get agent configuration
    AgentConfiguration get_agent_config(const std::string& agent_id);
    
    /// Update agent configuration
    bool update_agent_config(const std::string& agent_id, const AgentConfiguration& config);
    
    /// List all agents
    std::vector<std::string> list_agents(AgentState state_filter = AgentState::READY);
    
    /// Get agent runtime information
    AgentRuntimeInfo get_agent_runtime_info(const std::string& agent_id);
    
    /// Force agent state change
    bool set_agent_state(const std::string& agent_id, AgentState new_state);
    
    // ================================================================
    // TASK EXECUTION AND SCHEDULING
    // ================================================================
    
    /// Submit task for execution by cycle agents
    CycleAgentTaskResult execute_task(const CycleAgentTaskRequest& request);
    
    /// Submit task asynchronously
    std::future<CycleAgentTaskResult> execute_task_async(const CycleAgentTaskRequest& request);
    
    /// Execute on specific agent
    CycleAgentTaskResult execute_on_agent(const std::string& agent_id, const CycleAgentTaskRequest& request);
    
    /// Execute with automatic agent selection
    CycleAgentTaskResult execute_auto_select(const std::string& task_description,
                                            const std::string& task_data,
                                            AgentType preferred_type = AgentType::GENERAL_PURPOSE);
    
    /// Cancel running task
    bool cancel_task(const std::string& request_id);
    
    /// Get task status
    bool get_task_status(const std::string& request_id, CycleAgentTaskResult& result);
    
    // ================================================================
    // SCHEDULING AND LOAD BALANCING
    // ================================================================
    
    /// Set scheduling algorithm
    void set_scheduling_algorithm(SchedulingAlgorithm algorithm);
    
    /// Get next available agent using current algorithm
    std::string get_next_agent(AgentType preferred_type = AgentType::GENERAL_PURPOSE);
    
    /// Balance load across all agents
    void balance_load();
    
    /// Optimize agent placement and configuration
    void optimize_agent_configuration();
    
    /// Enable/disable automatic load balancing
    void enable_auto_load_balancing(bool enabled, std::chrono::milliseconds interval = 10000ms);
    
    /// Get load balancing statistics
    struct LoadBalancingStats {
        double overall_load_balance{0.0};        ///< Overall load balance score
        std::map<std::string, double> agent_loads; ///< Individual agent loads
        uint32_t load_balance_operations{0};     ///< Number of balance operations
        std::chrono::milliseconds last_balance_time{0}; ///< Time since last balance
    };
    LoadBalancingStats get_load_balancing_stats() const;
    
    // ================================================================
    // HEALTH MONITORING AND RECOVERY
    // ================================================================
    
    /// Enable health monitoring
    void enable_health_monitoring(bool enabled, std::chrono::milliseconds check_interval = 30000ms);
    
    /// Perform health check on specific agent
    bool check_agent_health(const std::string& agent_id);
    
    /// Perform health check on all agents
    void check_all_agents_health();
    
    /// Recover failed agent
    bool recover_agent(const std::string& agent_id);
    
    /// Get system health status
    struct SystemHealthStatus {
        uint8_t total_agents{0};
        uint8_t healthy_agents{0};
        uint8_t unhealthy_agents{0};
        uint8_t recovering_agents{0};
        
        double overall_health_score{0.0};
        std::map<std::string, bool> agent_health_status;
        std::vector<std::string> critical_issues;
        
        std::chrono::system_clock::time_point last_health_check;
    };
    SystemHealthStatus get_system_health_status() const;
    
    // ================================================================
    // PERFORMANCE MONITORING AND OPTIMIZATION
    // ================================================================
    
    /// Get comprehensive performance statistics
    struct PerformanceStatistics {
        // Task execution statistics
        uint32_t total_tasks_processed{0};
        uint32_t successful_tasks{0};
        uint32_t failed_tasks{0};
        uint32_t cancelled_tasks{0};
        
        // Timing statistics
        std::chrono::milliseconds avg_processing_time{0};
        std::chrono::milliseconds fastest_processing_time{std::chrono::milliseconds::max()};
        std::chrono::milliseconds slowest_processing_time{0};
        std::chrono::milliseconds avg_queue_wait_time{0};
        
        // Agent statistics
        std::map<std::string, uint32_t> agent_task_counts;
        std::map<std::string, double> agent_success_rates;
        std::map<std::string, std::chrono::milliseconds> agent_avg_times;
        
        // Resource utilization
        double avg_cpu_usage{0.0};
        size_t total_memory_used{0};
        double avg_agent_load{0.0};
        
        // Quantum optimization statistics
        uint32_t quantum_optimizations_applied{0};
        double quantum_performance_gain{0.0};
        
        // System efficiency
        double overall_efficiency{0.0};
        double task_throughput{0.0};            ///< Tasks per second
        double resource_utilization{0.0};       ///< Overall resource utilization
    };
    PerformanceStatistics get_performance_statistics() const;
    
    /// Reset performance statistics
    void reset_performance_statistics();
    
    /// Enable quantum optimization
    void enable_quantum_optimization(bool enabled, uint32_t optimization_level = 5);
    
    /// Optimize system performance
    void optimize_system_performance();
    
    // ================================================================
    // ADVANCED FEATURES
    // ================================================================
    
    /// Enable MAX MODE (bypass all limits)
    void enable_max_mode(bool enabled);
    
    /// Set quality vs speed balance
    void set_quality_speed_balance(double balance);
    
    /// Export agent configuration
    std::string export_agent_configuration() const;
    
    /// Import agent configuration
    bool import_agent_configuration(const std::string& config_data);
    
    /// Get comprehensive system status
    struct SystemStatus {
        bool orchestrator_initialized{false};
        bool quantum_optimization_enabled{false};
        bool max_mode_enabled{false};
        bool health_monitoring_active{false};
        bool auto_load_balancing_active{false};
        
        uint8_t total_agents{0};
        uint8_t active_agents{0};
        uint8_t ready_agents{0};
        uint32_t active_tasks{0};
        uint32_t queued_tasks{0};
        
        SchedulingAlgorithm current_algorithm{SchedulingAlgorithm::ROUND_ROBIN};
        double quality_speed_balance{0.5};
        
        std::chrono::milliseconds uptime{0};
        PerformanceStatistics performance_stats;
        SystemHealthStatus health_status;
        LoadBalancingStats load_balance_stats;
    };
    SystemStatus get_system_status() const;

private:
    // ================================================================
    // INTERNAL IMPLEMENTATION
    // ================================================================
    
    // Initialization and state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};
    std::chrono::system_clock::time_point startup_time_;
    
    // Configuration
    SchedulingAlgorithm current_algorithm_{SchedulingAlgorithm::ROUND_ROBIN};
    bool quantum_optimization_enabled_{true};
    bool max_mode_enabled_{false};
    double quality_speed_balance_{0.5};
    uint32_t quantum_optimization_level_{5};
    
    // Agent management
    std::unordered_map<std::string, AgentConfiguration> agent_configs_;
    std::unordered_map<std::string, AgentRuntimeInfo> agent_runtime_info_;
    std::unordered_map<std::string, std::unique_ptr<std::thread>> agent_threads_;
    mutable std::mutex agents_mutex_;
    
    // Task management
    std::unordered_map<std::string, CycleAgentTaskResult> active_tasks_;
    std::queue<CycleAgentTaskRequest> task_queue_;
    mutable std::mutex tasks_mutex_;
    std::condition_variable task_condition_;
    
    // Scheduling and load balancing
    std::atomic<uint32_t> round_robin_counter_{0};
    std::atomic<bool> auto_load_balancing_enabled_{false};
    std::thread load_balancing_thread_;
    std::chrono::milliseconds load_balance_interval_{10000};
    LoadBalancingStats load_balance_stats_;
    mutable std::mutex load_balance_mutex_;
    
    // Health monitoring
    std::atomic<bool> health_monitoring_enabled_{false};
    std::thread health_monitoring_thread_;
    std::chrono::milliseconds health_check_interval_{30000};
    SystemHealthStatus health_status_;
    mutable std::mutex health_mutex_;
    
    // Performance tracking
    PerformanceStatistics performance_stats_;
    mutable std::mutex performance_mutex_;
    
    // Random number generation
    mutable std::mt19937 random_generator_;
    
    // Internal helper methods
    std::string generate_agent_id();
    std::string generate_request_id();
    
    void agent_thread_function(const std::string& agent_id);
    void load_balancing_thread_function();
    void health_monitoring_thread_function();
    
    std::string select_agent_round_robin(AgentType preferred_type);
    std::string select_agent_load_balanced(AgentType preferred_type);
    std::string select_agent_quantum_optimized(AgentType preferred_type);
    std::string select_agent_capability_matched(AgentType preferred_type);
    
    CycleAgentTaskResult execute_task_on_agent_internal(const std::string& agent_id, 
                                                       const CycleAgentTaskRequest& request);
    
    void update_agent_statistics(const std::string& agent_id, const CycleAgentTaskResult& result);
    void update_performance_statistics(const CycleAgentTaskResult& result);
    
    bool is_agent_available(const std::string& agent_id, AgentType required_type = AgentType::GENERAL_PURPOSE);
    double calculate_agent_load(const std::string& agent_id);
    
    void apply_quantum_optimization();
    void balance_agents_load_internal();
    void recover_failed_agents();
    
    // Integration components
    std::unique_ptr<AgenticDeepThinkingEngine> thinking_engine_;
    std::unique_ptr<AgenticFailureDetector> failure_detector_;
    std::unique_ptr<TelemetryCollector> telemetry_collector_;
};

} // namespace RawrXD::AgentCycle

#endif // CYCLE_AGENT_ORCHESTRATOR_HPP