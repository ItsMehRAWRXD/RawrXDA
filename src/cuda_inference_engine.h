// ============================================================================
// cuda_inference_engine.h — CUDA GPU Inference Engine
// ============================================================================
#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace RawrXD {

// ============================================================================
// CUDA Inference Engine
// ============================================================================
class CUDAInferenceEngine {
public:
    CUDAInferenceEngine();
    ~CUDAInferenceEngine();

    // Static factory
    static std::unique_ptr<CUDAInferenceEngine> TryCreate();

    // InferenceEngine interface
    bool LoadModel(const std::string& path);
    bool IsModelLoaded() const;
    void UnloadModel();
    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Generate(const std::vector<int32_t>& tokens, int maxTokens);
    void GenerateStreaming(const std::vector<int32_t>& tokens, int maxTokens,
                          std::function<void(const std::string&)> onToken,
                          std::function<void()> onComplete,
                          void* userData);
    void SetContextLimit(size_t limit);
    size_t GetContextLimit() const;
    void SetThreadCount(int count);
    int GetThreadCount() const;
    const char* GetBackendName() const;

private:
    bool InitializeCUDA();
    void ShutdownCUDA();

    HMODULE m_cudaLib = nullptr;
    int m_deviceId = 0;
    void* m_context = nullptr;

    bool m_modelLoaded = false;
    std::string m_modelPath;
    size_t m_contextLimit = 4096;
    int m_threadCount = 4;
};

} // namespace RawrXD
