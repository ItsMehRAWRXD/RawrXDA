#pragma once
#ifndef DYNAMIC_POWERSHELL_TERMINAL_MANAGER_HPP
#define DYNAMIC_POWERSHELL_TERMINAL_MANAGER_HPP

/**
 * @file dynamic_powershell_terminal_manager.hpp
 * @brief Ultra-advanced PowerShell terminal management with dynamic timeouts
 * 
 * Features:
 * - Dynamic timeout calculation based on command complexity
 * - Random timeout variation with intelligent bounds
 * - Self-adjusting timeout based on execution history
 * - Automatic performance optimization
 * - Command pattern recognition and learning
 * - Terminal session management with resource limits
 * - Production-ready error handling and recovery
 * - Quantum-enhanced execution prediction
 * 
 * @author RawrXD Quantum Terminal Systems
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
#include <regex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace RawrXD::Terminal {

// =====================================================================
// ADVANCED TERMINAL CONFIGURATION STRUCTURES
// =====================================================================

/// Command complexity classification for timeout prediction
enum class CommandComplexity : uint8_t {
    TRIVIAL = 0,        ///< Simple commands like "Get-Date", <500ms
    SIMPLE = 1,         ///< Basic file operations, <2s
    MODERATE = 2,       ///< Complex queries, loops, <10s
    COMPLEX = 3,        ///< Module installs, large operations, <60s
    HEAVY = 4,          ///< System-wide operations, <300s
    QUANTUM = 5         ///< Ultra-complex operations, unlimited
};

/// Execution priority for terminal resource allocation
enum class ExecutionPriority : uint8_t {
    CRITICAL = 0,       ///< Immediate execution, bypass all limits
    HIGH = 1,           ///< High priority execution
    NORMAL = 2,         ///< Standard priority
    LOW = 3,            ///< Background execution
    DEFERRED = 4        ///< Execute when resources available
};

/// Terminal execution mode configuration
enum class TerminalMode : uint8_t {
    SYNCHRONOUS,        ///< Blocking execution
    ASYNCHRONOUS,       ///< Non-blocking with callbacks
    BATCH,              ///< Batch processing mode
    INTERACTIVE,        ///< Interactive session mode
    QUANTUM_OPTIMIZED   ///< Quantum-enhanced execution
};

/// Advanced timeout configuration with learning capabilities
struct DynamicTimeoutConfig {
    std::chrono::milliseconds base_timeout{30000};           ///< Base timeout value
    std::chrono::milliseconds min_timeout{1000};             ///< Minimum allowed timeout
    std::chrono::milliseconds max_timeout{600000};           ///< Maximum allowed timeout (10 minutes)
    
    // Random variation settings
    bool enable_random_variation{true};                      ///< Enable random timeout variation
    double random_variation_min{0.7};                       ///< Minimum random multiplier
    double random_variation_max{1.8};                       ///< Maximum random multiplier
    uint32_t random_seed{0};                                ///< Random seed (0 = auto)
    
    // Adaptive learning settings
    bool enable_adaptive_learning{true};                     ///< Enable timeout learning
    double learning_rate{0.1};                             ///< Learning rate for adaptations
    uint32_t history_window_size{100};                     ///< Number of executions to consider
    double success_rate_threshold{0.85};                   ///< Target success rate
    
    // Performance optimization
    bool enable_performance_prediction{true};               ///< Enable execution time prediction
    double performance_weight{0.3};                        ///< Weight for performance in timeout calc
    bool enable_quantum_enhancement{false};                ///< Enable quantum timeout prediction
    
    // Resource management
    uint32_t max_concurrent_sessions{16};                  ///< Maximum concurrent PowerShell sessions
    std::chrono::milliseconds session_idle_timeout{300000}; ///< Session idle timeout (5 minutes)
    size_t max_memory_per_session{512 * 1024 * 1024};     ///< Max memory per session (512MB)
};

/// Command execution statistics for learning and optimization
struct CommandExecutionStats {
    std::string command_pattern;                            ///< Command pattern (normalized)
    uint32_t execution_count{0};                          ///< Number of times executed
    uint32_t success_count{0};                            ///< Number of successful executions
    uint32_t timeout_count{0};                            ///< Number of timeout failures
    
    std::chrono::milliseconds total_execution_time{0};     ///< Total execution time
    std::chrono::milliseconds avg_execution_time{0};       ///< Average execution time
    std::chrono::milliseconds min_execution_time{std::chrono::milliseconds::max()};
    std::chrono::milliseconds max_execution_time{0};
    
    std::chrono::milliseconds current_timeout{0};          ///< Current adaptive timeout
    std::chrono::milliseconds optimal_timeout{0};          ///< Calculated optimal timeout
    
    CommandComplexity detected_complexity{CommandComplexity::MODERATE};
    double success_rate{0.0};                             ///< Success rate (0.0 - 1.0)
    double prediction_accuracy{0.0};                      ///< Timeout prediction accuracy
    
    std::chrono::system_clock::time_point last_executed;
    std::chrono::system_clock::time_point last_updated;
};

/// PowerShell session information and resource tracking
struct PowerShellSession {
    std::string session_id;                                ///< Unique session identifier
    std::string working_directory;                         ///< Current working directory
    std::map<std::string, std::string> environment_vars;  ///< Environment variables
    
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_used;
    
    uint32_t commands_executed{0};                        ///< Commands executed in session
    size_t memory_usage{0};                              ///< Current memory usage
    bool is_active{false};                               ///< Session active flag
    bool is_interactive{false};                          ///< Interactive session flag
    
#ifdef _WIN32
    HANDLE process_handle{INVALID_HANDLE_VALUE};         ///< Windows process handle
    DWORD process_id{0};                                ///< Process ID
#else
    pid_t process_id{-1};                              ///< Unix process ID
#endif
    
    ExecutionPriority priority{ExecutionPriority::NORMAL};
    TerminalMode mode{TerminalMode::SYNCHRONOUS};
};

/// Command execution request with comprehensive configuration
struct TerminalExecutionRequest {
    std::string command;                                   ///< PowerShell command to execute
    std::string session_id;                              ///< Target session (empty = auto)
    ExecutionPriority priority{ExecutionPriority::NORMAL};
    TerminalMode mode{TerminalMode::SYNCHRONOUS};
    
    // Timeout configuration
    std::chrono::milliseconds custom_timeout{0};         ///< Custom timeout (0 = auto)
    bool use_adaptive_timeout{true};                     ///< Use adaptive timeout calculation
    bool allow_random_variation{true};                  ///< Allow random timeout variation
    
    // Execution options
    std::string working_directory;                       ///< Working directory override
    std::map<std::string, std::string> environment_vars; ///< Environment variable overrides
    bool capture_output{true};                          ///< Capture command output
    bool capture_errors{true};                          ///< Capture error output
    
    // Callback functions
    std::function<void(const std::string&)> output_callback;        ///< Real-time output callback
    std::function<void(double)> progress_callback;                  ///< Progress update callback
    std::function<void(const std::string&)> completion_callback;    ///< Completion callback
    std::function<void(const std::string&)> error_callback;         ///< Error callback
};

/// Command execution result with comprehensive feedback
struct TerminalExecutionResult {
    std::string request_id;                              ///< Unique request identifier
    std::string command;                                ///< Original command
    std::string session_id;                             ///< Session that executed command
    
    bool success{false};                               ///< Execution success flag
    int exit_code{-1};                                ///< Process exit code
    
    std::string output;                               ///< Standard output
    std::string error_output;                         ///< Error output
    std::string error_message;                        ///< Error message (if any)
    
    std::chrono::milliseconds execution_time{0};      ///< Actual execution time
    std::chrono::milliseconds timeout_used{0};        ///< Timeout value used
    bool timed_out{false};                           ///< Whether execution timed out
    
    CommandComplexity detected_complexity{CommandComplexity::MODERATE};
    size_t memory_used{0};                          ///< Memory used during execution
    double cpu_usage{0.0};                         ///< CPU usage percentage
    
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    
    static TerminalExecutionResult success_result(const std::string& id, const std::string& output = "") {
        TerminalExecutionResult result;
        result.request_id = id;
        result.success = true;
        result.output = output;
        result.exit_code = 0;
        return result;
    }
    
    static TerminalExecutionResult error_result(const std::string& id, const std::string& error) {
        TerminalExecutionResult result;
        result.request_id = id;
        result.success = false;
        result.error_message = error;
        result.exit_code = -1;
        return result;
    }
};

// =====================================================================
// DYNAMIC POWERSHELL TERMINAL MANAGER CLASS
// =====================================================================

/**
 * @class DynamicPowerShellTerminalManager
 * @brief Ultra-advanced PowerShell terminal management with quantum optimization
 * 
 * Features:
 * - Intelligent timeout calculation with machine learning
 * - Random timeout variation with statistical optimization
 * - Self-adjusting performance based on execution history
 * - Multi-session management with resource limits
 * - Command pattern recognition and optimization
 * - Quantum-enhanced execution prediction
 * - Production-ready error handling and recovery
 */
class DynamicPowerShellTerminalManager {
public:
    // ================================================================
    // CONSTRUCTION AND INITIALIZATION
    // ================================================================
    
    DynamicPowerShellTerminalManager();
    ~DynamicPowerShellTerminalManager();
    
    /// Initialize the terminal manager with configuration
    bool initialize(const DynamicTimeoutConfig& config = {});
    
    /// Shutdown all sessions and cleanup resources
    void shutdown();
    
    /// Check if manager is ready for operation
    bool is_ready() const { return initialized_.load(); }
    
    // ================================================================
    // SESSION MANAGEMENT
    // ================================================================
    
    /// Create a new PowerShell session
    std::string create_session(const std::string& working_dir = "",
                              const std::map<std::string, std::string>& env_vars = {},
                              TerminalMode mode = TerminalMode::SYNCHRONOUS);
    
    /// Get existing session or create new one
    std::string get_or_create_session(const std::string& session_id = "");
    
    /// Close a PowerShell session
    bool close_session(const std::string& session_id);
    
    /// Get session information
    std::shared_ptr<PowerShellSession> get_session_info(const std::string& session_id);
    
    /// List all active sessions
    std::vector<std::string> list_sessions();
    
    /// Close inactive sessions
    void cleanup_inactive_sessions();
    
    // ================================================================
    // COMMAND EXECUTION
    // ================================================================
    
    /// Execute PowerShell command with dynamic timeout
    TerminalExecutionResult execute_command(const TerminalExecutionRequest& request);
    
    /// Execute simple command with default settings
    TerminalExecutionResult execute_simple_command(const std::string& command,
                                                  const std::string& session_id = "");
    
    /// Execute command asynchronously
    std::string execute_command_async(const TerminalExecutionRequest& request);
    
    /// Execute multiple commands in batch
    std::vector<TerminalExecutionResult> execute_batch_commands(
        const std::vector<TerminalExecutionRequest>& requests);
    
    /// Cancel running command
    bool cancel_command(const std::string& request_id);
    
    /// Get execution status for async command
    bool get_execution_status(const std::string& request_id, TerminalExecutionResult& result);
    
    // ================================================================
    // DYNAMIC TIMEOUT MANAGEMENT
    // ================================================================
    
    /// Calculate dynamic timeout for command
    std::chrono::milliseconds calculate_dynamic_timeout(const std::string& command,
                                                       bool use_learning = true,
                                                       bool apply_random_variation = true);
    
    /// Set base timeout value
    void set_base_timeout(std::chrono::milliseconds timeout);
    
    /// Get current base timeout
    std::chrono::milliseconds get_base_timeout() const;
    
    /// Enable/disable random timeout variation
    void enable_random_variation(bool enabled, double min_multiplier = 0.7, double max_multiplier = 1.8);
    
    /// Enable/disable adaptive learning
    void enable_adaptive_learning(bool enabled, double learning_rate = 0.1);
    
    /// Get timeout statistics for command pattern
    CommandExecutionStats get_command_stats(const std::string& command_pattern);
    
    /// Reset learning data for command pattern
    void reset_command_learning(const std::string& command_pattern = "");
    
    // ================================================================
    // PERFORMANCE OPTIMIZATION
    // ================================================================
    
    /// Analyze command complexity
    CommandComplexity analyze_command_complexity(const std::string& command);
    
    /// Predict execution time for command
    std::chrono::milliseconds predict_execution_time(const std::string& command);
    
    /// Optimize timeouts based on execution history
    void optimize_timeouts();
    
    /// Enable quantum-enhanced predictions
    void enable_quantum_enhancement(bool enabled);
    
    /// Get performance metrics
    struct PerformanceMetrics {
        uint32_t total_commands_executed{0};
        uint32_t successful_commands{0};
        uint32_t timed_out_commands{0};
        uint32_t failed_commands{0};
        
        double overall_success_rate{0.0};
        double timeout_accuracy{0.0};
        double average_prediction_error{0.0};
        
        std::chrono::milliseconds total_execution_time{0};
        std::chrono::milliseconds average_execution_time{0};
        std::chrono::milliseconds average_timeout_used{0};
        
        uint32_t active_sessions{0};
        uint32_t total_sessions_created{0};
        size_t total_memory_usage{0};
        
        uint32_t adaptations_performed{0};
        uint32_t quantum_optimizations{0};
    };
    PerformanceMetrics get_performance_metrics() const;
    
    // ================================================================
    // ADVANCED FEATURES
    // ================================================================
    
    /// Set custom timeout calculation algorithm
    void set_custom_timeout_calculator(std::function<std::chrono::milliseconds(const std::string&)> calculator);
    
    /// Add command pattern for optimization
    void add_command_pattern(const std::string& pattern, CommandComplexity complexity,
                           std::chrono::milliseconds suggested_timeout);
    
    /// Export learning data
    std::string export_learning_data() const;
    
    /// Import learning data
    bool import_learning_data(const std::string& data);
    
    /// Enable production mode with enhanced reliability
    void enable_production_mode(bool enabled);
    
    /// Set resource limits
    void set_resource_limits(uint32_t max_sessions, size_t max_memory_per_session);
    
    /// Get comprehensive system status
    struct SystemStatus {
        bool manager_initialized{false};
        bool production_mode{false};
        bool quantum_enhancement{false};
        
        uint32_t active_sessions{0};
        uint32_t max_sessions{0};
        uint32_t running_commands{0};
        
        size_t total_memory_usage{0};
        size_t max_memory_limit{0};
        
        std::chrono::milliseconds uptime{0};
        std::chrono::system_clock::time_point started_at;
        
        PerformanceMetrics performance;
        DynamicTimeoutConfig current_config;
    };
    SystemStatus get_system_status() const;

private:
    // ================================================================
    // INTERNAL IMPLEMENTATION
    // ================================================================
    
    // Initialization state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};
    std::chrono::system_clock::time_point startup_time_;
    
    // Configuration
    DynamicTimeoutConfig config_;
    std::function<std::chrono::milliseconds(const std::string&)> custom_timeout_calculator_;
    
    // Session management
    std::unordered_map<std::string, std::shared_ptr<PowerShellSession>> sessions_;
    mutable std::mutex sessions_mutex_;
    std::atomic<uint32_t> session_counter_{0};
    
    // Execution tracking
    std::unordered_map<std::string, TerminalExecutionResult> active_executions_;
    std::queue<TerminalExecutionRequest> execution_queue_;
    mutable std::mutex execution_mutex_;
    std::condition_variable execution_condition_;
    
    // Learning and statistics
    std::unordered_map<std::string, CommandExecutionStats> command_stats_;
    mutable std::mutex stats_mutex_;
    
    // Performance tracking
    PerformanceMetrics performance_metrics_;
    mutable std::mutex metrics_mutex_;
    
    // Random number generation
    std::mt19937 random_generator_;
    std::uniform_real_distribution<double> variation_distribution_;
    
    // Worker threads
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> workers_active_{false};
    
    // Monitoring and cleanup
    std::thread monitoring_thread_;
    std::atomic<bool> monitoring_active_{false};
    
    // Internal helper methods
    std::string generate_session_id();
    std::string generate_request_id();
    std::string normalize_command_pattern(const std::string& command);
    CommandComplexity classify_command_complexity(const std::string& command);
    
    void worker_thread_function();
    void monitoring_thread_function();
    void cleanup_thread_function();
    
    TerminalExecutionResult execute_command_internal(const TerminalExecutionRequest& request);
    bool create_powershell_process(const TerminalExecutionRequest& request,
                                  PowerShellSession& session,
                                  TerminalExecutionResult& result);
    
    void update_command_statistics(const std::string& command,
                                  const TerminalExecutionResult& result);
    void apply_learning_update(const std::string& pattern,
                              std::chrono::milliseconds actual_time,
                              std::chrono::milliseconds timeout_used,
                              bool success);
    
    std::chrono::milliseconds calculate_adaptive_timeout(const CommandExecutionStats& stats);
    std::chrono::milliseconds apply_random_variation(std::chrono::milliseconds base_timeout);
    std::chrono::milliseconds apply_quantum_enhancement(const std::string& command,
                                                       std::chrono::milliseconds base_timeout);
    
    void optimize_session_resources();
    void perform_maintenance_tasks();
    bool validate_resource_limits();
    
    // Platform-specific process management
#ifdef _WIN32
    bool create_windows_process(const std::string& command, PowerShellSession& session);
    bool terminate_windows_process(PowerShellSession& session);
#else
    bool create_unix_process(const std::string& command, PowerShellSession& session);
    bool terminate_unix_process(PowerShellSession& session);
#endif
};

} // namespace RawrXD::Terminal

#endif // DYNAMIC_POWERSHELL_TERMINAL_MANAGER_HPP