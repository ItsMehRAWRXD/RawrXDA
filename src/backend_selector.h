#pragma once

#include <memory>
#include <string>
#include <vector>
#include "inference_engine.h"

namespace RawrXD {

/**
 * @enum BackendType
 * @brief Available inference backends
 */
enum class BackendType {
    CPU,        ///< CPU-only inference (always available)
    DML,        ///< DirectML (Windows GPU acceleration)
    Vulkan,     ///< Vulkan compute (cross-platform GPU)
    HIP,        ///< HIP (AMD GPU)
    CUDA,       ///< CUDA (NVIDIA GPU)
    Titan       ///< Custom Titan assembly acceleration
};

/**
 * @struct BackendInfo
 * @brief Information about an available backend
 */
struct BackendInfo {
    BackendType type;
    std::string name;
    bool available = false;
    std::string deviceName;
    size_t vramBytes = 0;
    std::string computeCapability;
    bool supportsFP16 = false;
    bool supportsInt8 = false;
    double performanceScore = 0.0; // Relative performance score
};

/**
 * @class BackendSelector
 * @brief Hardware-aware backend selection and instantiation
 *
 * Detects available hardware acceleration and creates optimal InferenceEngine
 * instances based on model requirements and hardware capabilities.
 */
class BackendSelector {
public:
    BackendSelector();
    ~BackendSelector() = default;

    /**
     * @brief Detect all available backends on the system
     * @return Vector of available backend information
     */
    std::vector<BackendInfo> detectAvailableBackends();

    /**
     * @brief Select the best backend for a given model
     * @param modelPath Path to the GGUF model file
     * @param preferredType Preferred backend type (optional)
     * @return Selected backend type
     */
    BackendType selectOptimalBackend(const std::string& modelPath,
                                   BackendType preferredType = BackendType::CPU);

    /**
     * @brief Create an InferenceEngine instance for the specified backend
     * @param backendType Backend to create
     * @return Unique pointer to the inference engine
     */
    std::unique_ptr<InferenceEngine> createInferenceEngine(BackendType backendType);

    /**
     * @brief Get information about a specific backend
     * @param type Backend type
     * @return Backend information
     */
    BackendInfo getBackendInfo(BackendType type) const;

    /**
     * @brief Benchmark all available backends with a test model
     * @param modelPath Path to test model
     * @param testPrompt Test prompt for benchmarking
     * @return Vector of benchmark results
     */
    std::vector<std::pair<BackendType, double>> benchmarkBackends(
        const std::string& modelPath, const std::string& testPrompt);

private:
    std::vector<BackendInfo> m_availableBackends;

    // Hardware detection methods
    bool detectDML();
    bool detectVulkan();
    bool detectHIP();
    bool detectCUDA();
    bool detectTitan();

    // Backend creation methods
    std::unique_ptr<InferenceEngine> createCPUEngine();
    std::unique_ptr<InferenceEngine> createDMLEngine();
    std::unique_ptr<InferenceEngine> createVulkanEngine();
    std::unique_ptr<InferenceEngine> createHIPEngine();
    std::unique_ptr<InferenceEngine> createCUDAEngine();
    std::unique_ptr<InferenceEngine> createTitanEngine();

    // Performance scoring
    double scoreBackend(const BackendInfo& info, const std::string& modelPath);
};

} // namespace RawrXD