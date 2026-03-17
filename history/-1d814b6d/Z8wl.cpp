#include "unified_engine_coordinator.h"
#include "streaming_engine.h"
#include "streaming_gguf_loader.h"
#include "agentic_engine.h"
#include "hot_patcher.h"
#include <iostream>
#include <omp.h>

/**
 * UnifiedEngineCoordinator: Master orchestration for all AI inference & IDE systems
 * 
 * This coordinator integrates:
 * - Streaming GGUF model loading (on-demand, zero-copy)
 * - Inference kernels (AVX-512, Q4_0 quantization)
 * - Transformer execution (GQA attention, SwiGLU FFN, RoPE)
 * - Token generation (BPE tokenization, beam search, nucleus sampling)
 * - Real-time streaming responses
 * - Live hot-patching for zero-downtime updates
 * - Agentic IDE integration with tool calling
 */

UnifiedEngineCoordinator::UnifiedEngineCoordinator()
    : m_modelLoaded(false), m_inferenceActive(false), m_hotpatchEnabled(true) {
    
    m_logger = std::make_shared<Logger>();
    m_logger->info("UnifiedEngineCoordinator", "Initializing master coordinator");
    
    m_streamingEngine = std::make_shared<StreamingEngine>(m_logger, nullptr, nullptr);
    m_agentic = std::make_unique<AgenticEngine>();
    m_cpuEngine = std::make_unique<CPUInference::CPUInferenceEngine>();
    m_agentic->setInferenceEngine(m_cpuEngine.get());
}

UnifiedEngineCoordinator::~UnifiedEngineCoordinator() {
    if (m_modelLoaded) {
        UnloadModel();
    }
}

bool UnifiedEngineCoordinator::LoadModel(const std::string& model_path) {
    if (m_modelLoaded) {
        m_logger->warn("UnifiedEngineCoordinator", "Model already loaded, unloading first");
        UnloadModel();
    }
    
    m_logger->info("UnifiedEngineCoordinator", "Loading model from: " + model_path);
    
    m_ggufLoader = std::make_unique<CPUInference::StreamingGGUFLoader>();
    if (!m_ggufLoader->Open(model_path)) {
        m_logger->error("UnifiedEngineCoordinator", "Failed to open GGUF model");
        return false;
    }

    if (!m_cpuEngine->LoadModel(model_path)) {
        m_logger->error("UnifiedEngineCoordinator", "CPU inference engine failed to load model");
        return false;
    }

    m_modelConfig.context_length = static_cast<int>(m_cpuEngine->GetContextLimit());
    m_modelConfig.hidden_dim = m_cpuEngine->GetEmbeddingDim();
    m_modelConfig.num_heads = m_cpuEngine->GetNumHeads();
    m_modelConfig.num_layers = m_cpuEngine->GetNumLayers();
    m_modelConfig.vocab_size = m_cpuEngine->GetVocabSize();

    m_logger->info("UnifiedEngineCoordinator", 
        "Model loaded: " + std::to_string(m_modelConfig.num_layers) + " layers, " +
        std::to_string(m_modelConfig.num_heads) + " heads, " +
        std::to_string(m_modelConfig.vocab_size) + " vocab");

    m_modelLoaded = true;
    return true;
}

void UnifiedEngineCoordinator::UnloadModel() {
    if (!m_modelLoaded) return;
    
    m_logger->info("UnifiedEngineCoordinator", "Unloading model");
    
    if (m_ggufLoader) {
        m_ggufLoader->Close();
        m_ggufLoader.reset();
    }
    
    m_modelLoaded = false;
    m_inferenceActive = false;
}

GenerationResult UnifiedEngineCoordinator::GenerateCompletion(
    const std::string& prompt,
    const GenerationConfig& config
) {
    if (!m_modelLoaded) {
        return GenerationResult{false, "Model not loaded", "", 0, 0};
    }
    
    GenerationResult result;
    result.success = true;
    result.text = "";
    
    m_logger->info("UnifiedEngineCoordinator", "Starting completion for prompt: " + prompt.substr(0, 50));
    
    // Set inference state
    m_inferenceActive = true;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<int32_t> input_tokens = m_cpuEngine->Tokenize(prompt);
    result.input_tokens = static_cast<int>(input_tokens.size());

    m_sampler.temp = config.temperature;
    m_sampler.top_p = config.top_p;
    m_sampler.top_k = config.top_k;
    m_sampler.repeat_penalty = config.repeat_penalty;

    int tokens_generated = 0;
    std::vector<int32_t> output_tokens = input_tokens;

    for (int step = 0; step < config.max_tokens && m_inferenceActive; step++) {
        std::vector<float> logits = m_cpuEngine->Eval(output_tokens);
        if (logits.empty()) break;

        int next_token = m_sampler.sample(logits.data(), m_modelConfig.vocab_size);
        output_tokens.push_back(next_token);
        tokens_generated++;

        if (config.onToken) {
            config.onToken(next_token);
        }

        if (next_token == 2) {
            break;
        }
    }

    m_inferenceActive = false;

    result.text = m_cpuEngine->Detokenize(output_tokens);
    result.output_tokens = tokens_generated;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    m_logger->info("UnifiedEngineCoordinator", 
        "Completion finished: " + std::to_string(tokens_generated) + " tokens in " +
        std::to_string(duration.count()) + "ms");
    
    return result;
}

std::string UnifiedEngineCoordinator::ExecuteAgenticTask(const std::string& task_description) {
    if (!m_agentic) {
        return "Error: Agentic engine not initialized";
    }
    
    m_logger->info("UnifiedEngineCoordinator", "Executing agentic task: " + task_description);
    
    // Route task to appropriate agentic handler
    if (task_description.find("analyze") != std::string::npos) {
        return m_agentic->analyzeCode(task_description);
    } else if (task_description.find("generate") != std::string::npos) {
        return m_agentic->generateCode(task_description);
    } else if (task_description.find("refactor") != std::string::npos) {
        return m_agentic->refactorCode(task_description, "optimization");
    } else if (task_description.find("fix") != std::string::npos) {
        return m_agentic->explainError(task_description);
    } else {
        // Default: general chat
        return m_agentic->chat(task_description);
    }
}

bool UnifiedEngineCoordinator::ApplyHotpatch(
    const std::string& patch_name,
    const std::string& target_module,
    const std::string& target_function,
    const std::vector<unsigned char>& new_opcodes
) {
    if (!m_hotpatchEnabled) {
        m_logger->warn("UnifiedEngineCoordinator", "Hot-patching disabled");
        return false;
    }
    
    m_logger->info("UnifiedEngineCoordinator", 
        "Applying hot-patch: " + patch_name + " to " + target_module + "::" + target_function);
    
    // Get function address
    void* target_addr = g_hot_patcher.GetFunctionAddress(target_module, target_function);
    if (!target_addr) {
        m_logger->error("UnifiedEngineCoordinator", "Could not resolve function address");
        return false;
    }
    
    // Apply patch
    if (!g_hot_patcher.ApplyPatch(patch_name, target_addr, new_opcodes)) {
        m_logger->error("UnifiedEngineCoordinator", "Failed to apply patch");
        return false;
    }
    
    // Record patch in history
    m_patchHistory.push_back({
        patch_name,
        std::chrono::system_clock::now(),
        "applied",
        target_module + "::" + target_function
    });
    
    return true;
}

bool UnifiedEngineCoordinator::RevertHotpatch(const std::string& patch_name) {
    if (!g_hot_patcher.RevertPatch(patch_name)) {
        m_logger->error("UnifiedEngineCoordinator", "Failed to revert patch");
        return false;
    }
    
    // Update history
    for (auto& entry : m_patchHistory) {
        if (entry.name == patch_name) {
            entry.status = "reverted";
        }
    }
    
    return true;
}

CoordinatorStats UnifiedEngineCoordinator::GetStats() const {
    CoordinatorStats stats;
    stats.model_loaded = m_modelLoaded;
    stats.inference_active = m_inferenceActive;
    stats.num_active_patches = m_patchHistory.size();
    stats.model_layers = m_modelConfig.num_layers;
    stats.vocab_size = m_modelConfig.vocab_size;
    return stats;
}

// Singleton instance
static UnifiedEngineCoordinator* g_coordinator = nullptr;

UnifiedEngineCoordinator* GetGlobalCoordinator() {
    if (!g_coordinator) {
        g_coordinator = new UnifiedEngineCoordinator();
    }
    return g_coordinator;
}

void DestroyGlobalCoordinator() {
    if (g_coordinator) {
        delete g_coordinator;
        g_coordinator = nullptr;
    }
}
