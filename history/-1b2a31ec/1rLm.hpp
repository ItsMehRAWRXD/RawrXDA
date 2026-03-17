#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <random>

// External MASM functions from agentic loop and brutal compression
extern "C" {
    // Agentic loop functions from agentic_loop.asm
    int InitializeAgenticLoop();
    int StartAgenticLoop(const char* msg);
    int StopAgenticLoop();
    int GetAgentStatus();
    int CleanupAgenticLoop();
    
    // Brutal compression from deflate_brutal_masm.asm
    void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);
    
    // GGUF loader functions from MASM loaders
    int GGUF_LoadModel(const char* path);
    int GGUF_IDE_RegisterLoader(const char* name, int loaderType);
    int GGUF_IDE_NotifyProgress(int percent, const char* message);
}

// GGUF Loader Orchestrator - Auto-picks and rotates between loaders and agents
// Supports all MASM IDE GGUF loaders with intelligent load balancing

enum class LoaderType {
    BENCH,          // gguf_bench_loader.asm - Fast benchmarking
    CHAIN,          // gguf_chain_loader_unified.asm - Chain loading
    CLEAN,          // gguf_loader_clean.asm - Clean minimal loader
    COMPLETE,       // gguf_loader_complete.asm - Full featured
    ENHANCED,       // gguf_loader_enhanced.asm - Enhanced performance
    ENTERPRISE,     // gguf_loader_enterprise.asm - Enterprise features
    FINAL,          // gguf_loader_final.asm - Production ready
    INTEGRATION,    // gguf_loader_integration.asm - Integration focused
    MINIMAL         // gguf_loader_minimal.asm - Minimal footprint
};

enum class AgentType {
    PLANNING,       // Planning agent for task decomposition
    EXECUTION,      // Execution agent for running tasks
    MONITORING,     // Monitoring agent for health checks
    OPTIMIZATION    // Optimization agent for performance tuning
};

struct LoaderInstance {
    LoaderType type;
    std::string name;
    void* handle;               // DLL/SO handle for MASM loader
    bool is_active;
    size_t load_count;
    double avg_load_time_ms;
    std::chrono::steady_clock::time_point last_used;
    
    LoaderInstance() : handle(nullptr), is_active(false), load_count(0), avg_load_time_ms(0.0) {}
};

struct AgentInstance {
    AgentType type;
    std::string name;
    bool is_active;
    size_t task_count;
    double avg_response_time_ms;
    std::chrono::steady_clock::time_point last_used;
    
    AgentInstance() : is_active(false), task_count(0), avg_response_time_ms(0.0) {}
};

class GGUFLoaderOrchestrator {
public:
    GGUFLoaderOrchestrator();
    ~GGUFLoaderOrchestrator();
    
    // Initialize all available loaders
    bool initializeLoaders(const std::string& loaders_dir);
    
    // Initialize all agents
    bool initializeAgents();
    
    // Auto-select best loader for a given model
    LoaderType selectBestLoader(const std::string& model_path, size_t model_size_bytes);
    
    // Auto-select best agent for a given task
    AgentType selectBestAgent(const std::string& task_type);
    
    // Load model using selected loader
    bool loadModel(const std::string& model_path, LoaderType loader_type = LoaderType::ENTERPRISE);
    
    // Execute task using selected agent
    bool executeTask(const std::string& task, AgentType agent_type = AgentType::EXECUTION);
    
    // Rotate to next available loader (round-robin)
    LoaderType rotateLoader();
    
    // Rotate to next available agent (round-robin)
    AgentType rotateAgent();
    
    // Get loader statistics
    std::map<LoaderType, LoaderInstance> getLoaderStats() const { return m_loaders; }
    
    // Get agent statistics
    std::map<AgentType, AgentInstance> getAgentStats() const { return m_agents; }
    
    // Set rotation strategy
    void setRotationStrategy(const std::string& strategy) { m_rotation_strategy = strategy; }
    
    // Enable/disable auto-rotation
    void setAutoRotation(bool enabled) { m_auto_rotate = enabled; }
    
private:
    std::map<LoaderType, LoaderInstance> m_loaders;
    std::map<AgentType, AgentInstance> m_agents;
    
    LoaderType m_current_loader;
    AgentType m_current_agent;
    
    std::string m_rotation_strategy;  // "round-robin", "performance", "random"
    bool m_auto_rotate;
    
    std::mt19937 m_random_engine;
    
    // Agentic loop state
    bool m_agentic_loop_initialized;
    bool m_agentic_loop_active;
    
    // Brutal compression state
    bool m_use_compression;
    size_t m_compression_threshold;  // Compress blocks > this size
    
    // Helper methods
    LoaderType selectLoaderByPerformance();
    LoaderType selectLoaderRoundRobin();
    LoaderType selectLoaderRandom();
    
    AgentType selectAgentByPerformance();
    AgentType selectAgentRoundRobin();
    AgentType selectAgentRandom();
    
    void updateLoaderStats(LoaderType type, double load_time_ms);
    void updateAgentStats(AgentType type, double response_time_ms);
    
    // Agentic loop integration
    bool initializeAgenticLoop();
    void startAgenticProcessing(const std::string& task);
    void stopAgenticProcessing();
    
    // Brutal compression integration (for 70B models on 512GB RAM)
    void* compressModelData(const void* data, size_t size, size_t* compressed_size);
    bool shouldCompress(size_t data_size) const;
    
    // MASM loader function pointers
    typedef bool (*LoadModelFunc)(const char* path);
    std::map<LoaderType, LoadModelFunc> m_loader_funcs;
};
