#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "modules/native_memory.hpp"

namespace CPUInference {

class CPUInferenceEngine {
public:
    CPUInferenceEngine();
    ~CPUInferenceEngine();
    void RegisterMemoryPlugin(std::shared_ptr<RawrXD::Modules::NativeMemoryModule> plugin);
    void RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin);
    bool LoadModel(const std::string& path);
    void SetThreadCount(uint32_t count);
    void SetContextLimit(size_t limit);
    void SetContextSize(size_t limit);
    void SetMaxMode(int mode);
    void SetDeepThinking(bool enable);
    void SetDeepResearch(bool enable);
    size_t GetContextLimit() const;
    bool IsModelLoaded() const;
    std::vector<int32_t> Tokenize(const std::string& text) const;
    std::vector<int32_t> Generate(const std::vector<int32_t>& input_ids, size_t max_tokens);
    std::string Detokenize(const std::vector<int32_t>& tokens) const;
    void GenerateStreaming(
        const std::vector<int32_t>& input_ids,
        size_t max_tokens,
        const std::function<void(const std::string&)>& onToken,
        const std::function<void()>& onComplete
    );

private:
    size_t m_contextLimit = 0;
};

} // namespace CPUInference

namespace RawrXD {
using CPUInferenceEngine = CPUInference::CPUInferenceEngine;
}