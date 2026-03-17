#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include "streaming_engine.h"
#include "engine/sampler.h"
#include "cpu_inference_engine.h"
#include "logging/logger.h"

// Forward declarations
class AgenticEngine;
class StreamingGGUFLoader;
class Logger;
class HotPatcher;

// Model configuration from GGUF metadata
struct ModelConfig {
    int context_length = 4096;
    int hidden_dim = 4096;
    int num_heads = 32;
    int num_kv_heads = 8;
    int num_layers = 32;
    int vocab_size = 128000;
    float layer_norm_epsilon = 1e-6f;
    bool use_gqa = true;  // Group Query Attention
};

// Generation configuration
struct GenerationConfig {
    int max_tokens = 2048;
    float temperature = 0.7f;
    float top_p = 0.9f;
    int top_k = 40;
    float repeat_penalty = 1.0f;
    bool use_beam_search = false;
    int beam_size = 4;
    
    // Streaming callback: called for each generated token
    std::function<void(int token_id)> onToken;
    
    // Error callback
    std::function<void(const std::string& error)> onError;
};

// Generation result
struct GenerationResult {
    bool success;
    std::string error;
    std::string text;
    int input_tokens;
    int output_tokens;
};

// Hot-patch history entry
struct PatchHistoryEntry {
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    std::string status;  // "applied", "reverted", "failed"
    std::string description;
};

// Coordinator statistics
struct CoordinatorStats {
    bool model_loaded;
    bool inference_active;
    int num_active_patches;
    int model_layers;
    int vocab_size;
};

/**
 * UnifiedEngineCoordinator: Master orchestration system
 * 
 * Coordinates:
 * - GGUF streaming model loading
 * - Transformer inference with GQA support
 * - Token generation with multiple sampling strategies
 * - Real-time streaming responses
 * - Agentic task execution and tool calling
 * - Live hot-patching without downtime
 */
class UnifiedEngineCoordinator {
public:
    UnifiedEngineCoordinator();
    ~UnifiedEngineCoordinator();
    
    // Model management
    bool LoadModel(const std::string& model_path);
    void UnloadModel();
    bool IsModelLoaded() const { return m_modelLoaded; }
    
    // Generation
    GenerationResult GenerateCompletion(
        const std::string& prompt,
        const GenerationConfig& config = GenerationConfig{}
    );
    
    // Agentic task execution
    std::string ExecuteAgenticTask(const std::string& task_description);
    
    // Hot-patching
    bool ApplyHotpatch(
        const std::string& patch_name,
        const std::string& target_module,
        const std::string& target_function,
        const std::vector<unsigned char>& new_opcodes
    );
    bool RevertHotpatch(const std::string& patch_name);
    
    // Statistics and monitoring
    CoordinatorStats GetStats() const;
    
    // Component access (for advanced usage)
    AgenticEngine* GetAgenticEngine() { return m_agentic.get(); }
    std::shared_ptr<StreamingEngine> GetStreamingEngine() { return m_streamingEngine; }
    
private:
    bool m_modelLoaded;
    bool m_inferenceActive;
    bool m_hotpatchEnabled;
    
    // Components
    std::shared_ptr<Logger> m_logger;
    std::unique_ptr<AgenticEngine> m_agentic;
    std::unique_ptr<CPUInference::StreamingGGUFLoader> m_ggufLoader;
    std::unique_ptr<CPUInference::CPUInferenceEngine> m_cpuEngine;
    std::shared_ptr<StreamingEngine> m_streamingEngine;
    
    // Sampling
    Sampler m_sampler;
    
    // Configuration
    ModelConfig m_modelConfig;
    
    // Hot-patch tracking
    std::vector<PatchHistoryEntry> m_patchHistory;
};

// Global singleton accessors
UnifiedEngineCoordinator* GetGlobalCoordinator();
void DestroyGlobalCoordinator();
