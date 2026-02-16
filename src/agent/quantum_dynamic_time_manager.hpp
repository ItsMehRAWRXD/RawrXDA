#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <random>
#include <memory>
#include <thread>
#include <queue>

// Forward declarations
extern "C" {
    void __stdcall masm_time_predictor(const char* task_signature, float complexity, int* time_ms);
    void __stdcall masm_adaptive_scheduler(const void* history, int count, float* adjustment_factor);
    void __stdcall masm_performance_analyzer(const void* metrics, int count, void* recommendations);
}

namespace RawrXD::Agent {

/**
 * @class QuantumDynamicTimeManager
 * @brief Ultra-advanced dynamic time limit adjustment system
 * 
 * Features:
 * - Self-adjusting time limits based on complexity
 * - PowerShell terminal timeout management with randomization
 * - MASM-accelerated time prediction algorithms
 * - Machine learning-based adaptation
 * - Performance history tracking
 * - Real-time optimization based on system load
 * - Quantum temporal optimization
 */
class QuantumDynamicTimeManager {
public:
    enum class TimeCategory : uint8_t {
        Micro = 1,      // < 1 second
        Quick = 2,      // 1-10 seconds
        Short = 3,      // 10-60 seconds
        Medium = 4,     // 1-10 minutes
        Long = 5,       // 10-60 minutes
        Extended = 6,   // 1-6 hours
        Marathon = 7,   // 6-24 hours
        Epic = 8,       // 1-7 days
        Legendary = 9,  // 1-4 weeks
        Mythical = 10   // 1+ months
    };

    enum class AdjustmentStrategy : uint8_t {
        Conservative = 1,   // Generous time limits, lower failure risk
        Balanced = 2,      // Standard approach, good balance
        Aggressive = 3,    // Tight time limits, higher performance
        Adaptive = 4,      // AI-driven adjustment based on patterns
        Quantum = 5        // Quantum-optimized temporal management
    };

    struct TimeProfile {
        std::string task_type;
        TimeCategory category;
        
        // Base timing parameters
        std::chrono::milliseconds base_time{0};
        std::chrono::milliseconds min_time{1000};     // 1 second minimum
        std::chrono::milliseconds max_time{86400000}; // 24 hour maximum
        
        // Complexity multipliers
        float simple_multiplier = 0.5f;
        float moderate_multiplier = 1.0f;
        float complex_multiplier = 2.0f;
        float expert_multiplier = 5.0f;
        float quantum_multiplier = 10.0f;
        
        // Dynamic factors
        float system_load_factor = 1.0f;
        float network_latency_factor = 1.0f;
        float memory_pressure_factor = 1.0f;
        float cpu_contention_factor = 1.0f;
        
        // PowerShell specific
        std::chrono::milliseconds pwsh_base_timeout{300000}; // 5 minutes
        bool randomize_pwsh = true;
        float pwsh_randomization_factor = 0.3f; // ±30% variation
        
        // Learning parameters
        float success_rate = 0.8f;
        float avg_completion_ratio = 0.7f; // actual_time / allocated_time
        int adaptation_samples = 0;
        std::chrono::steady_clock::time_point last_update;
        
        TimeProfile() : last_update(std::chrono::steady_clock::now()) {}
    };

    struct ExecutionContext {
        std::string task_id;
        std::string task_type;
        float complexity_score;
        int agent_count;
        bool requires_pwsh;
        bool is_production_critical;
        bool enable_extensions;
        
        // System state
        float current_system_load;
        float available_memory_gb;
        float network_quality;
        
        // Historical context
        std::vector<std::chrono::milliseconds> recent_completion_times;
        float recent_success_rate;
        
        ExecutionContext() : complexity_score(0.5f), agent_count(1), requires_pwsh(false),
                           is_production_critical(false), enable_extensions(false),
                           current_system_load(0.5f), available_memory_gb(8.0f),
                           network_quality(1.0f), recent_success_rate(0.8f) {}
    };

    struct TimeAllocation {
        std::chrono::milliseconds total_time;
        std::chrono::milliseconds thinking_time;
        std::chrono::milliseconds execution_time;
        std::chrono::milliseconds pwsh_timeout;
        std::chrono::milliseconds buffer_time;
        
        // Extension possibilities
        bool allow_extensions = true;
        int max_extensions = 3;
        float extension_multiplier = 1.5f;
        
        // Checkpoints
        std::vector<std::chrono::milliseconds> checkpoint_times;
        std::vector<std::string> checkpoint_names;
        
        // Quality vs Speed trade-off
        float quality_weight = 0.7f;   // 70% quality, 30% speed
        float speed_weight = 0.3f;
        
        TimeAllocation() : total_time(300000), thinking_time(120000), execution_time(150000),
                          pwsh_timeout(60000), buffer_time(30000) {}
    };

    struct AdaptationMetrics {
        // Performance tracking
        int total_tasks = 0;
        int successful_completions = 0;
        int timeout_failures = 0;
        int early_completions = 0;
        
        // Timing statistics
        std::chrono::milliseconds avg_completion_time{0};
        std::chrono::milliseconds median_completion_time{0};
        std::chrono::milliseconds p95_completion_time{0};
        
        // Accuracy metrics
        float prediction_accuracy = 0.0f;
        float adaptation_effectiveness = 0.0f;
        float system_efficiency = 0.0f;
        
        // PowerShell specific
        int pwsh_executions = 0;
        int pwsh_timeouts = 0;
        float avg_pwsh_duration_ms = 0.0f;
        
        // Learning progress
        std::chrono::steady_clock::time_point learning_start;
        int adaptation_cycles = 0;
        float learning_rate = 0.1f;
        
        AdaptationMetrics() : learning_start(std::chrono::steady_clock::now()) {}
    };

    struct PowerShellConfig {
        // Base configuration
        std::chrono::milliseconds default_timeout{300000}; // 5 minutes
        std::chrono::milliseconds min_timeout{10000};      // 10 seconds
        std::chrono::milliseconds max_timeout{3600000};    // 1 hour
        
        // Randomization settings
        bool enable_randomization = true;
        float min_random_factor = 0.7f;    // -30%
        float max_random_factor = 1.5f;    // +50%
        bool use_gaussian_distribution = true;
        
        // Adaptive adjustment
        bool enable_adaptive_timeout = true;
        float success_rate_threshold = 0.8f;
        float adjustment_sensitivity = 0.2f;
        
        // Command-specific timeouts
        std::map<std::string, std::chrono::milliseconds> command_timeouts;
        std::map<std::string, float> command_risk_factors;
        
        // Terminal management
        int max_concurrent_terminals = 10;
        bool reuse_terminals = true;
        std::chrono::milliseconds terminal_idle_timeout{600000}; // 10 minutes
        
        PowerShellConfig() {
            // Initialize default command timeouts
            command_timeouts["build"] = std::chrono::minutes(15);
            command_timeouts["test"] = std::chrono::minutes(10);
            command_timeouts["deploy"] = std::chrono::minutes(30);
            command_timeouts["compile"] = std::chrono::minutes(5);
            command_timeouts["install"] = std::chrono::minutes(20);
            
            // Initialize risk factors
            command_risk_factors["network-operation"] = 2.0f;
            command_risk_factors["file-operation"] = 1.2f;
            command_risk_factors["system-operation"] = 1.5f;
        }
    };

public:
    explicit QuantumDynamicTimeManager(AdjustmentStrategy strategy = AdjustmentStrategy::Adaptive);
    ~QuantumDynamicTimeManager();

    // Core time allocation
    TimeAllocation calculateTimeAllocation(const ExecutionContext& context);
    TimeAllocation adaptiveTimeAllocation(const ExecutionContext& context);
    TimeAllocation quantumTimeAllocation(const ExecutionContext& context);
    
    // PowerShell integration
    std::chrono::milliseconds getRandomPwshTimeout(const std::string& command_type = "");
    std::chrono::milliseconds getAdaptivePwshTimeout(const ExecutionContext& context);
    void configurePwshLimits(std::chrono::milliseconds min_time, std::chrono::milliseconds max_time);
    void setPwshRandomization(bool enable, float min_factor = 0.7f, float max_factor = 1.5f);
    
    // Profile management
    void addTimeProfile(const std::string& task_type, const TimeProfile& profile);
    void updateTimeProfile(const std::string& task_type, const TimeProfile& profile);
    TimeProfile* getTimeProfile(const std::string& task_type);
    std::vector<TimeProfile> getAllProfiles() const;
    
    // Adaptation and learning
    void recordExecution(const std::string& task_id, const ExecutionContext& context,
                        std::chrono::milliseconds actual_time, bool success);
    void adaptProfilesBasedOnHistory();
    void optimizeGlobalSettings();
    void resetLearning();
    
    // Real-time adjustments
    void adjustForSystemLoad(float load_factor);
    void adjustForMemoryPressure(float memory_factor);
    void adjustForNetworkConditions(float network_factor);
    TimeAllocation extendTimeAllocation(const TimeAllocation& original, const std::string& reason);
    
    // Quantum optimization
    void enableQuantumOptimization(bool enable = true);
    void setQuantumParameters(float temporal_uncertainty = 0.1f, float optimization_strength = 1.0f);
    float calculateQuantumTimeBonus(const ExecutionContext& context);
    
    // Statistics and monitoring
    AdaptationMetrics getMetrics() const;
    void generateTimeReport(std::ostream& output) const;
    std::vector<std::string> getPerformanceRecommendations() const;
    
    // Configuration
    void setAdjustmentStrategy(AdjustmentStrategy strategy);
    AdjustmentStrategy getAdjustmentStrategy() const { return m_strategy; }
    void updatePowerShellConfig(const PowerShellConfig& config);
    PowerShellConfig getPowerShellConfig() const;
    
    // Advanced features
    void enableMasmAcceleration(bool enable = true);
    void setLearningRate(float rate);
    void configureCheckpoints(const std::vector<std::string>& checkpoint_names);

private:
    // Core implementation
    void adaptiveOptimizationLoop();
    void performanceMonitoringLoop();
    
    // Time calculation algorithms
    std::chrono::milliseconds calculateBasicTime(const ExecutionContext& context);
    std::chrono::milliseconds applyComplexityMultiplier(std::chrono::milliseconds base_time, float complexity);
    std::chrono::milliseconds applySystemFactors(std::chrono::milliseconds base_time, const ExecutionContext& context);
    
    // Machine learning adaptation
    void updateProfileWithFeedback(TimeProfile& profile, std::chrono::milliseconds actual_time,
                                  std::chrono::milliseconds predicted_time, bool success);
    float calculatePredictionError(const std::vector<std::chrono::milliseconds>& predicted,
                                  const std::vector<std::chrono::milliseconds>& actual);
    void adaptMultipliers(TimeProfile& profile, const std::vector<float>& performance_data);
    
    // PowerShell management
    std::chrono::milliseconds generateRandomTimeout(std::chrono::milliseconds base_timeout);
    std::chrono::milliseconds getCommandSpecificTimeout(const std::string& command);
    void updatePwshStatistics(const std::string& command, std::chrono::milliseconds duration, bool success);
    
    // MASM integration
    void initializeMasmAcceleration();
    void shutdownMasmAcceleration();
    std::chrono::milliseconds getMasmPrediction(const ExecutionContext& context);
    
    // System monitoring
    float getCurrentSystemLoad();
    float getMemoryPressure();
    float getNetworkQuality();
    void updateSystemMetrics();
    
    // Quantum calculations
    float calculateQuantumUncertainty(const ExecutionContext& context);
    std::chrono::milliseconds applyQuantumOptimization(std::chrono::milliseconds base_time, 
                                                       const ExecutionContext& context);
    
    // Data management
    void saveProfilesToDisk();
    void loadProfilesFromDisk();
    void cleanupOldData();
    
    // Configuration and state
    AdjustmentStrategy m_strategy;
    PowerShellConfig m_pwsh_config;
    
    // Threading
    std::atomic<bool> m_running{false};
    std::thread m_optimization_thread;
    std::thread m_monitoring_thread;
    
    // Data storage
    mutable std::mutex m_profiles_mutex;
    mutable std::mutex m_metrics_mutex;
    mutable std::mutex m_pwsh_mutex;
    
    std::map<std::string, TimeProfile> m_time_profiles;
    AdaptationMetrics m_metrics;
    
    // PowerShell state
    std::map<std::string, std::chrono::milliseconds> m_pwsh_history;
    std::queue<std::pair<std::string, bool>> m_recent_pwsh_results; // command, success
    
    // Random number generation
    mutable std::random_device m_random_device;
    mutable std::mt19937 m_random_generator;
    mutable std::normal_distribution<float> m_gaussian_dist;
    mutable std::uniform_real_distribution<float> m_uniform_dist;
    
    // MASM acceleration
    bool m_masm_enabled;
    void* m_masm_context;
    
    // Quantum optimization
    bool m_quantum_enabled;
    float m_temporal_uncertainty;
    float m_optimization_strength;
    
    // System monitoring
    std::chrono::steady_clock::time_point m_last_system_update;
    float m_cached_system_load;
    float m_cached_memory_pressure;
    float m_cached_network_quality;
    
    // Performance tracking
    std::vector<std::chrono::milliseconds> m_execution_history;
    std::vector<bool> m_success_history;
    std::chrono::steady_clock::time_point m_system_start_time;
    
    // Learning parameters
    float m_learning_rate;
    int m_adaptation_window_size;
    bool m_enable_continuous_learning;
};

/**
 * @class PowerShellTimeoutManager
 * @brief Specialized PowerShell terminal timeout management
 * 
 * Manages individual PowerShell terminals with dynamic timeout adjustment
 */
class PowerShellTimeoutManager {
public:
    struct TerminalSession {
        std::string session_id;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::milliseconds allocated_time;
        std::chrono::milliseconds remaining_time;
        std::string current_command;
        bool is_interactive;
        int extension_count;
        
        TerminalSession(const std::string& id) 
            : session_id(id), start_time(std::chrono::steady_clock::now()),
              allocated_time(300000), remaining_time(300000), is_interactive(false), extension_count(0) {}
    };

    explicit PowerShellTimeoutManager(QuantumDynamicTimeManager* time_manager);
    ~PowerShellTimeoutManager();

    // Terminal management
    std::string createSession(std::chrono::milliseconds timeout);
    void destroySession(const std::string& session_id);
    bool extendSession(const std::string& session_id, std::chrono::milliseconds additional_time);
    
    // Execution
    std::string executeWithTimeout(const std::string& command, std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
    bool isSessionActive(const std::string& session_id) const;
    std::chrono::milliseconds getRemainingTime(const std::string& session_id) const;
    
    // Configuration
    void setRandomizationEnabled(bool enable) { m_randomization_enabled = enable; }
    void setRandomizationRange(float min_factor, float max_factor);

private:
    QuantumDynamicTimeManager* m_time_manager;
    std::map<std::string, TerminalSession> m_sessions;
    mutable std::mutex m_sessions_mutex;
    
    bool m_randomization_enabled;
    float m_min_random_factor;
    float m_max_random_factor;
};

} // namespace RawrXD::Agent