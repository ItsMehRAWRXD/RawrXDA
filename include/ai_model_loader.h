// AI Model Loader - GGUF Model Loading Interface
// Provides unified model loading across all IDE variants

#ifndef AI_MODEL_LOADER_H_
#define AI_MODEL_LOADER_H_

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace AIModelLoader {

// GGUF model metadata
struct ModelInfo {
    std::string name;
    std::string architecture;        // llama, gpt2, mistral, etc.
    uint64_t parameterCount = 0;
    uint64_t fileSizeBytes = 0;
    int quantizationType = 0;        // Q4_0, Q4_1, Q5_0, Q5_1, Q8_0, F16, F32
    int contextLength = 2048;
    int vocabSize = 32000;
    int embeddingDim = 4096;
    int layerCount = 32;
    int headCount = 32;
    std::string quantName;           // "Q4_K_M", "Q5_K_S", etc.
};

// Model loading result
struct LoadResult {
    bool success = false;
    std::string errorMessage;
    ModelInfo info;
    void* modelHandle = nullptr;     // Opaque handle to loaded model
};

// Model loader interface
class IModelLoader {
public:
    virtual ~IModelLoader() = default;
    virtual LoadResult loadModel(const std::string& path) = 0;
    virtual void unloadModel() = 0;
    virtual bool isLoaded() const = 0;
    virtual ModelInfo getModelInfo() const = 0;
};

// Factory
std::unique_ptr<IModelLoader> createModelLoader();

} // namespace AIModelLoader

#endif // AI_MODEL_LOADER_H_
