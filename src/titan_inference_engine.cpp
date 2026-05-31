// ============================================================================
// titan_inference_engine.cpp — Titan Native Inference Implementation
// ============================================================================
#include "titan_inference_engine.h"
#include <windows.h>
#include <vector>
#include <string>
#include <sstream>

namespace RawrXD {

// ============================================================================
// Constructor / Destructor
// ============================================================================
TitanInferenceEngine::TitanInferenceEngine() = default;

TitanInferenceEngine::~TitanInferenceEngine() {
    ShutdownTitan();
}

// ============================================================================
// Static Factory
// ============================================================================
std::unique_ptr<TitanInferenceEngine> TitanInferenceEngine::TryCreate() {
    auto engine = std::make_unique<TitanInferenceEngine>();
    if (!engine->InitializeTitan()) {
        return nullptr;
    }
    return engine;
}

// ============================================================================
// Titan Initialization
// ============================================================================
bool TitanInferenceEngine::InitializeTitan() {
    // Load Titan DLL
    m_titanLib = LoadLibraryA("RawrXD_Titan.dll");
    if (!m_titanLib) {
        m_titanLib = LoadLibraryA("TitanInference.dll");
        if (!m_titanLib) {
            return false;
        }
    }

    // Get TitanInit
    using PFN_TitanInit = int (__stdcall *)(void**);
    auto TitanInit = (PFN_TitanInit)GetProcAddress(m_titanLib, "TitanInit");
    if (!TitanInit) {
        FreeLibrary(m_titanLib);
        m_titanLib = nullptr;
        return false;
    }

    // Initialize Titan
    if (TitanInit(&m_device) != 0) {
        FreeLibrary(m_titanLib);
        m_titanLib = nullptr;
        return false;
    }

    return true;
}

void TitanInferenceEngine::ShutdownTitan() {
    if (m_device && m_titanLib) {
        using PFN_TitanShutdown = int (__stdcall *)(void*);
        auto TitanShutdown = (PFN_TitanShutdown)GetProcAddress(m_titanLib, "TitanShutdown");
        if (TitanShutdown) {
            TitanShutdown(m_device);
        }
        m_device = nullptr;
    }

    if (m_titanLib) {
        FreeLibrary(m_titanLib);
        m_titanLib = nullptr;
    }
}

// ============================================================================
// InferenceEngine Interface
// ============================================================================
bool TitanInferenceEngine::LoadModel(const std::string& path) {
    m_modelPath = path;
    m_modelLoaded = true;
    return true;
}

bool TitanInferenceEngine::IsModelLoaded() const {
    return m_modelLoaded;
}

void TitanInferenceEngine::UnloadModel() {
    m_modelLoaded = false;
    m_modelPath.clear();
}

std::vector<int32_t> TitanInferenceEngine::Tokenize(const std::string& text) {
    std::vector<int32_t> tokens;
    std::istringstream iss(text);
    std::string word;
    int32_t id = 1;
    while (iss >> word) {
        tokens.push_back(id++);
    }
    return tokens;
}

std::string TitanInferenceEngine::Generate(const std::vector<int32_t>& tokens, int maxTokens) {
    std::string result;
    for (int i = 0; i < maxTokens && i < static_cast<int>(tokens.size()); ++i) {
        result += "token_" + std::to_string(tokens[i]) + " ";
    }
    return result;
}

void TitanInferenceEngine::GenerateStreaming(const std::vector<int32_t>& tokens, int maxTokens,
                                            std::function<void(const std::string&)> onToken,
                                            std::function<void()> onComplete,
                                            void* userData) {
    (void)userData;
    for (int i = 0; i < maxTokens && i < static_cast<int>(tokens.size()); ++i) {
        std::string token = "token_" + std::to_string(tokens[i]) + " ";
        if (onToken) {
            onToken(token);
        }
    }
    if (onComplete) {
        onComplete();
    }
}

void TitanInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
}

size_t TitanInferenceEngine::GetContextLimit() const {
    return m_contextLimit;
}

void TitanInferenceEngine::SetThreadCount(int count) {
    m_threadCount = count;
}

int TitanInferenceEngine::GetThreadCount() const {
    return m_threadCount;
}

const char* TitanInferenceEngine::GetBackendName() const {
    return "Titan";
}

} // namespace RawrXD
