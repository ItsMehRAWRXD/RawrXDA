#pragma once

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include <functional>
#include <future>
#include <queue>
#include <random>
#include <bitset>
#include <thread>

// Forward declarations
namespace RawrXD::Agent {
    class AgenticDeepThinkingEngine;
    struct TaskDefinition;
    struct ExecutionResult;
}

extern "C" {
    // MASM acceleration for agent coordination
    void __stdcall masm_agent_scheduler(void* agents, int count, void* work_queue, int* assignments);
    void __stdcall masm_consensus_calculator(const void* results, int count, float* consensus_score);
    void __stdcall masm_load_balancer(const void* system_state, int agent_count, void* load_distribution);
}

namespace RawrXD::Agent {

/**
 * @class QuantumMultiModelAgentCycling
 * @brief Ultra-advanced multi-model agent cycling system (1x-99x agents)
 * 
 * Features:
 * - Dynamic agent scaling from 1x to 99x
 * - Intelligent model selection and rotation
 * - Load balancing with MASM acceleration
 * - Consensus-based result merging
 * - Fault tolerance and self-healing
 * - Performance optimization
 * - Resource management
 * - Quality assurance through agent debate
 */
class QuantumMultiModelAgentCycling {
public:
    enum class AgentState : uint8_t {
        Idle = 0,           // Agent available for work
        Initializing = 1,   // Agent starting up
        Working = 2,        // Agent executing task
        Thinking = 3,       // Agent in deep thought
        Debating = 4,       // Agent participating in debate
        Voting = 5,         // Agent casting consensus vote
        Error = 6,          // Agent encountered error
        Recovering = 7,     // Agent self-healing
        Shutting_Down = 8   // Agent terminating
    };

    enum class ModelType : uint16_t {
        // Code-focused models
        Qwen2_5_Coder_32B    = 0x0001,
        Qwen2_5_Coder_14B    = 0x0002,
        Qwen2_5_Coder_7B     = 0x0004,
        DeepSeek_Coder_33B   = 0x0008,
        DeepSeek_Coder_6_7B  = 0x0010,
        Codestral_22B        = 0x0020,
        
        // General reasoning models
        Llama3_1_70B         = 0x0040,
        Llama3_1_8B          = 0x0080,
        Mixtral_8x7B         = 0x0100,
        
        // Specialized models
        Claude_3_5_Sonnet    = 0x0200,
        GPT4_Turbo          = 0x0400,
        Gemini_Pro          = 0x0800,
        
        // Custom/Local models
        Custom_Model_1       = 0x1000,
        Custom_Model_2       = 0x2000,
        Custom_Model_3       = 0x4000,
        
        // Model groups
        All_Code_Models      = 0x003F,
        All_Reasoning_Models = 0x01C0,
        All_Cloud_Models     = 0x0E00,
        All_Models           = 0xFFFF
    };

    enum class CyclingStrategy : uint8_t {
        Round_Robin = 1,        // Simple rotation through models
        Performance_Based = 2,   // Select based on past performance
        Adaptive = 3,           // AI-driven model selection
        Random = 4,             // Random selection with weights
        Quantum_Optimization = 5 // Quantum-optimized selection
    };

    struct AgentConfig {
        int agent_id;
        std::string model_name;
        ModelType model_type;
        std::string endpoint_url;
        std::map<std::string, std::string> parameters;
        
        // Performance characteristics
        float avg_response_time_ms = 0.0f;
        float avg_quality_score = 0.0f;
        float reliability_score = 1.0f;
        float cost_per_token = 0.001f;
        
        // Resource requirements
        int min_memory_mb = 1024;
        int max_memory_mb = 8192;
        int cpu_threads = 2;
        bool requires_gpu = false;
        
        // Specializations
        std::bitset<16> strengths;      // What this model is good at
        std::bitset<16> weaknesses;     // What this model struggles with
        
        AgentConfig() = default;
        AgentConfig(int id, const std::string& name, ModelType type) 
            : agent_id(id), model_name(name), model_type(type) {}
    };

    struct AgentInstance {
        AgentConfig config;
        std::atomic<AgentState> state{AgentState::Idle};
        std::unique_ptr<AgenticDeepThinkingEngine> thinking_engine;
        std::thread worker_thread;
        
        // Runtime state
        std::chrono::steady_clock::time_point last_activity;
        std::atomic<int> tasks_completed{0};
        std::atomic<float> current_load{0.0f};
        std::string current_task_id;
        
        // Performance tracking
        std::vector<float> response_times;
        std::vector<float> quality_scores;
        std::atomic<int> errors_count{0};
        std::atomic<int> recoveries_count{0};
        
        // Communication
        std::mutex task_mutex;
        std::queue<std::string> incoming_tasks;
        std::future<ExecutionResult> current_future;
        
        AgentInstance() : last_activity(std::chrono::steady_clock::now()) {}
        ~AgentInstance() {
            if (worker_thread.joinable()) {
                worker_thread.join();
            }
        }
    };

    struct CyclingConfig {
        // Scaling parameters
        int min_agents = 1;
        int max_agents = 99;
        int default_agents = 3;
        int scale_step = 2;                     // How many agents to add/remove at once
        
        // Model selection
        CyclingStrategy strategy = CyclingStrategy::Adaptive;
        std::bitset<16> enabled_models = 0xFFFF;
        std::map<ModelType, float> model_weights;
        
        // Performance thresholds
        float scale_up_threshold = 0.8f;        // Load percentage to scale up
        float scale_down_threshold = 0.3f;      // Load percentage to scale down
        float min_consensus_threshold = 0.7f;   // Minimum agreement for consensus
        
        // Timing
        std::chrono::milliseconds scale_check_interval{5000};    // 5 seconds
        std::chrono::milliseconds agent_timeout{300000};        // 5 minutes
        std::chrono::milliseconds debate_timeout{120000};       // 2 minutes
        
        // Quality control
        bool enable_agent_debate = true;
        bool enable_consensus_voting = true;
        bool enable_self_healing = true;
        bool enable_dynamic_scaling = true;
        
        // Resource management
        int max_memory_usage_mb = 32768;        // 32GB max
        int max_cpu_usage_percent = 80;
        bool enable_load_balancing = true;
        
        CyclingConfig() {
            // Default model weights (higher = more preferred)
            model_weights[ModelType::Qwen2_5_Coder_32B] = 1.0f;
            model_weights[ModelType::Qwen2_5_Coder_14B] = 0.9f;
            model_weights[ModelType::DeepSeek_Coder_33B] = 0.95f;
            model_weights[ModelType::Codestral_22B] = 0.85f;
            model_weights[ModelType::Llama3_1_70B] = 0.8f;
        }
    };

    struct ConsensusResult {
        bool consensus_reached;
        float consensus_confidence;
        ExecutionResult merged_result;
        std::vector<ExecutionResult> individual_results;
        std::vector<std::string> disagreement_points;
        std::map<int, float> agent_agreement_scores;
        
        ConsensusResult() : consensus_reached(false), consensus_confidence(0.0f) {}
    };

    struct SystemMetrics {
        // Current state
        int active_agents = 0;
        int idle_agents = 0;
        int error_agents = 0;
        float system_load = 0.0f;
        float memory_usage_mb = 0.0f;
        float cpu_usage_percent = 0.0f;
        
        // Performance history
        float avg_response_time_ms = 0.0f;
        float avg_consensus_confidence = 0.0f;
        float system_reliability = 1.0f;
        
        // Statistics
        int total_tasks_processed = 0;
        int successful_consensus = 0;
        int failed_consensus = 0;
        int agent_recoveries = 0;
        int scale_up_events = 0;
        int scale_down_events = 0;
        
        // Cost tracking
        float total_cost_estimate = 0.0f;
        std::map<ModelType, float> cost_per_model;
    };

public:
    explicit QuantumMultiModelAgentCycling(const CyclingConfig& config = CyclingConfig{});
    ~QuantumMultiModelAgentCycling();

    // Core agent management
    bool initializeAgents();
    void shutdownAllAgents();
    int scaleUp(int additional_agents = 0);
    int scaleDown(int agents_to_remove = 0);
    
    // Task execution
    ExecutionResult executeWithCycling(const TaskDefinition& task);
    ConsensusResult executeWithConsensus(const TaskDefinition& task, int min_agents = 3);
    std::vector<ExecutionResult> executeBatch(const std::vector<TaskDefinition>& tasks);
    
    // Agent selection and routing
    std::vector<int> selectOptimalAgents(const TaskDefinition& task, int count);
    AgentInstance* getRoundRobinAgent();
    AgentInstance* getPerformanceBasedAgent(const TaskDefinition& task);
    AgentInstance* getAdaptiveAgent(const TaskDefinition& task);
    
    // Model management
    bool addModelType(ModelType model_type, const AgentConfig& config);
    bool removeModelType(ModelType model_type);
    std::vector<ModelType> getAvailableModels() const;
    std::vector<ModelType> getBestModelsForTask(const TaskDefinition& task, int count = 3);
    
    // Consensus and debate
    ConsensusResult facilitateAgentDebate(const std::vector<ExecutionResult>& results, 
                                         const TaskDefinition& original_task);
    float calculateConsensus(const std::vector<ExecutionResult>& results);
    ExecutionResult mergeResults(const std::vector<ExecutionResult>& results, float consensus_threshold);
    
    // Performance optimization
    void optimizeAgentAllocation();
    void rebalanceWorkload();
    void updateModelWeights();
    void performanceBasedScaling();
    
    // Monitoring and diagnostics
    SystemMetrics getSystemMetrics() const;
    std::vector<AgentInstance*> getAgentStatus() const;
    void generatePerformanceReport(std::ostream& output) const;
    
    // Configuration
    void updateConfig(const CyclingConfig& config);
    CyclingConfig getConfig() const;
    
    // Advanced features
    bool enableQuantumOptimization();
    void setCustomModelWeights(const std::map<ModelType, float>& weights);
    void enableExperimentalFeatures(bool enable);

private:
    // Core implementation
    void agentManagerLoop();
    void loadBalancerLoop();
    void healthMonitorLoop();
    
    // Agent lifecycle
    std::unique_ptr<AgentInstance> createAgent(const AgentConfig& config);
    void startAgent(AgentInstance* agent);
    void stopAgent(AgentInstance* agent);
    bool restartAgent(AgentInstance* agent);
    
    // Work distribution
    void distributeTask(const std::string& task_id, const std::vector<int>& agent_ids);
    ExecutionResult collectResults(const std::vector<int>& agent_ids, 
                                  std::chrono::milliseconds timeout);
    
    // Model selection algorithms
    ModelType selectModelRoundRobin();
    ModelType selectModelByPerformance(const TaskDefinition& task);
    ModelType selectModelAdaptive(const TaskDefinition& task);
    ModelType selectModelQuantum(const TaskDefinition& task);
    
    // Performance tracking
    void updateAgentMetrics(AgentInstance* agent, const ExecutionResult& result);
    void updateSystemMetrics();
    float calculateModelScore(ModelType model, const TaskDefinition& task);
    
    // Resource management
    bool checkResourceConstraints(int additional_agents);
    void optimizeMemoryUsage();
    void manageCpuUsage();
    
    // Fault tolerance
    void detectFailedAgents();
    bool attemptAgentRecovery(AgentInstance* agent);
    void isolateFailedAgent(AgentInstance* agent);
    
    // MASM integration
    void initializeMasmAcceleration();
    void shutdownMasmAcceleration();
    bool useMasmScheduler() const { return m_masm_enabled && m_agents.size() > 4; }
    
    // Consensus algorithms
    float calculateSemanticSimilarity(const std::string& result1, const std::string& result2);
    std::vector<std::string> identifyDisagreements(const std::vector<ExecutionResult>& results);
    ExecutionResult resolveDisagreements(const std::vector<ExecutionResult>& results,
                                        const std::vector<std::string>& disagreements);
    
    // Thread management
    std::atomic<bool> m_running{false};
    std::thread m_manager_thread;
    std::thread m_load_balancer_thread;
    std::thread m_health_monitor_thread;
    
    // State management
    mutable std::mutex m_agents_mutex;
    mutable std::mutex m_config_mutex;
    mutable std::mutex m_metrics_mutex;
    
    std::map<int, std::unique_ptr<AgentInstance>> m_agents;
    std::atomic<int> m_next_agent_id{1};
    std::atomic<int> m_round_robin_index{0};
    
    // Configuration and metrics
    CyclingConfig m_config;
    SystemMetrics m_metrics;
    
    // Model management
    std::map<ModelType, AgentConfig> m_model_configs;
    std::map<ModelType, float> m_model_performance_history;
    
    // Task queue management
    std::queue<std::string> m_pending_tasks;
    std::map<std::string, TaskDefinition> m_task_definitions;
    std::mutex m_task_queue_mutex;
    
    // Performance optimization
    std::random_device m_random_device;
    std::mt19937 m_random_generator;
    
    // MASM acceleration
    bool m_masm_enabled;
    void* m_masm_scheduler_context;
    
    // Timing
    std::chrono::steady_clock::time_point m_last_scale_check;
    std::chrono::steady_clock::time_point m_system_start_time;
    
    // Advanced features
    bool m_quantum_optimization_enabled;
    bool m_experimental_features_enabled;
    
    // Cost tracking
    std::map<std::string, float> m_cost_tracking;
    std::mutex m_cost_mutex;
};

} // namespace RawrXD::Agent