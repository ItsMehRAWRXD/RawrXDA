#include "cpu_inference_engine.h"
#include "rawrxd_inference.h"
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace RawrXD {

class CPUInferenceEngine::Impl {
public:
    RawrXDInference engine;
    bool isLoaded = false;

    std::string findFile(const std::string& filename, const std::string& modelPath) {
        namespace fs = std::filesystem;
        fs::path mPath(modelPath);
        fs::path p1 = mPath.parent_path() / filename;
        if (fs::exists(p1)) return p1.string();
        if (fs::exists(filename)) return filename;
        return filename; 
    }
};

CPUInferenceEngine::CPUInferenceEngine() : m_impl(std::make_unique<Impl>()) {}
CPUInferenceEngine::~CPUInferenceEngine() = default;

RawrXD::Expected<void, InferenceError> CPUInferenceEngine::loadModel(const std::string& path) {
    if (path.empty()) return RawrXD::unexpected(InferenceError::ModelNotFound);
    
    std::string vocabPath = m_impl->findFile("tokenizer.json", path);
    std::string mergesPath = m_impl->findFile("merges.txt", path);

    std::wstring wpath;
    try {
        if (!path.empty()) {
            int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
            if (len > 0) {
                wpath.resize(len);
                MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], len);
                if (wpath.back() == L'\0') wpath.pop_back();
            }
        }
    } catch(...) {}

    if (m_impl->engine.Initialize(wpath.c_str(), vocabPath.c_str(), mergesPath.c_str())) {
        m_impl->isLoaded = true;
        return {};
    }

    return RawrXD::unexpected(InferenceError::InternalError);
}

bool CPUInferenceEngine::isModelLoaded() const {
    return m_impl->isLoaded;
}

RawrXD::Expected<CPUInferenceEngine::GenerationResult, InferenceError> CPUInferenceEngine::generate(
    const std::string& prompt,
    float temp,
    float top_p,
    int max_tokens
) {
    if (!m_impl->isLoaded) return RawrXD::unexpected(InferenceError::InternalError);
    
    std::string result = m_impl->engine.Generate(prompt, max_tokens);
    
    GenerationResult res;
    res.text = result;
    res.confidence = 1.0f;
    res.tokens_generated = 0; // TODO
    
    return res;
}

void CPUInferenceEngine::GenerateStreaming(
    const std::vector<int>& tokens,
    int max_tokens,
    StreamCallback on_token,
    DoneCallback on_done
) {
    if (!m_impl->isLoaded) {
        if (on_done) on_done();
        return;
    }
    
    std::string prompt = m_impl->engine.Detokenize((const std::vector<uint32_t>&)tokens);
    
    m_impl->engine.Generate(prompt, max_tokens, [&](const std::string& piece) {
        if (on_token) on_token(piece);
    });
    
    if (on_done) on_done();
}

std::vector<int> CPUInferenceEngine::Tokenize(const std::string& text) {
    if (!m_impl->isLoaded) return {};
    auto u32_toks = m_impl->engine.Tokenize(text);
    std::vector<int> res(u32_toks.begin(), u32_toks.end());
    return res;
}

std::string CPUInferenceEngine::Detokenize(const std::vector<int>& tokens) {
    if (!m_impl->isLoaded) return "";
    std::vector<uint32_t> u32_toks(tokens.begin(), tokens.end());
    return m_impl->engine.Detokenize(u32_toks);
}

nlohmann::json CPUInferenceEngine::getStatus() const {
    return {
        {"loaded", m_impl->isLoaded},
        {"backend", "Vulkan/RawrXD_Real"}, 
        {"device", "GPU"} 
    };
}

}
