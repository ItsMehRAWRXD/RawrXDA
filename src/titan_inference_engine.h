// ============================================================================
// titan_inference_engine.h — Titan Native Inference Engine
// ============================================================================
#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace RawrXD {

// ============================================================================
// Titan Inference Engine
// ============================================================================
class TitanInferenceEngine {
public:
    TitanInferenceEngine();
    ~TitanInferenceEngine();

    // Static factory
    static std::unique_ptr<TitanInferenceEngine> TryCreate();

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
    bool InitializeTitan();
    void ShutdownTitan();

    HMODULE m_titanLib = nullptr;
    void* m_device = nullptr;

    bool m_modelLoaded = false;
    std::string m_modelPath;
    size_t m_contextLimit = 4096;
    int m_threadCount = 4;
};

} // namespace RawrXD
