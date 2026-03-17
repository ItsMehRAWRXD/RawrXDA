#ifndef RAWRXD_INFERENCE_ENGINE_H
#define RAWRXD_INFERENCE_ENGINE_H

// ============================================================================
// RawrXD Inference Engine Header
// Machine-Code Emitter & Autonomous AI Inference Core
// ============================================================================

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace rawrxd::inference {

// ============================================================================
// InferenceConfig Struct
// ============================================================================
// Configuration container for autonomous inference operations.
// Encapsulates model path, context size, and batch parameters.
struct InferenceConfig {
    std::string modelPath;    ///< Path to GGUF or model file
    int contextSize = 4096;   ///< Maximum context window size (tokens)
    int batchSize = 1;        ///< Batch processing size
};

// ============================================================================
// AutonomousInferenceEngine Class
// ============================================================================
// Core inference engine with automatic model loading and token generation.
// Implements singleton-aware design with proper RAII semantics.
class AutonomousInferenceEngine {
public:
    using InferenceConfig = rawrxd::inference::InferenceConfig;

    /// Constructor with inference configuration
    explicit AutonomousInferenceEngine(const InferenceConfig& config);

    /// Destructor - releases all inference resources
    ~AutonomousInferenceEngine();

    /// Load model automatically from path
    /// @param path Model file path (GGUF format or compatible)
    /// @return true if model loaded successfully, false otherwise
    bool loadModelAutomatic(const std::string& path);

    /// Run inference on token sequence with streaming callback
    /// @param tokens Input token IDs for inference
    /// @param callback Function invoked with generated text fragments
    /// @param maxTokens Maximum tokens to generate
    void infer(const std::vector<int>& tokens,
               std::function<void(const std::string&)> callback,
               uint64_t maxTokens);

private:
    InferenceConfig m_config;  ///< Configuration state
    bool m_loaded = false;     ///< Model load flag

    // Prevent copy operations - singleton-aware design
    AutonomousInferenceEngine(const AutonomousInferenceEngine&) = delete;
    AutonomousInferenceEngine& operator=(const AutonomousInferenceEngine&) = delete;
};

} // namespace rawrxd::inference

#endif // RAWRXD_INFERENCE_ENGINE_H
