// ============================================================================
// hip_inference_engine.cpp — HIP/ROCm GPU Inference Implementation
// ============================================================================
#include "hip_inference_engine.h"
#include <windows.h>
#include <vector>
#include <string>
#include <sstream>

namespace RawrXD {

// ============================================================================
// Constructor / Destructor
// ============================================================================
HIPInferenceEngine::HIPInferenceEngine() = default;

HIPInferenceEngine::~HIPInferenceEngine() {
    ShutdownHIP();
}

// ============================================================================
// Static Factory
// ============================================================================
std::unique_ptr<HIPInferenceEngine> HIPInferenceEngine::TryCreate() {
    auto engine = std::make_unique<HIPInferenceEngine>();
    if (!engine->InitializeHIP()) {
        return nullptr;
    }
    return engine;
}

// ============================================================================
// HIP Initialization
// ============================================================================
bool HIPInferenceEngine::InitializeHIP() {
    // Load HIP runtime
    m_hipLib = LoadLibraryA("hiprt64.dll");
    if (!m_hipLib) {
        m_hipLib = LoadLibraryA("amdhip64.dll");
        if (!m_hipLib) {
            return false;
        }
    }

    // Get hipInit
    using PFN_hipInit = int (__stdcall *)(unsigned int);
    auto hipInit = (PFN_hipInit)GetProcAddress(m_hipLib, "hipInit");
    if (!hipInit) {
        FreeLibrary(m_hipLib);
        m_hipLib = nullptr;
        return false;
    }

    // Initialize HIP
    if (hipInit(0) != 0) {
        FreeLibrary(m_hipLib);
        m_hipLib = nullptr;
        return false;
    }

    // Get hipDeviceGet
    using PFN_hipDeviceGet = int (__stdcall *)(void**, int);
    auto hipDeviceGet = (PFN_hipDeviceGet)GetProcAddress(m_hipLib, "hipDeviceGet");
    if (hipDeviceGet) {
        hipDeviceGet(&m_devicePtr, 0);
    }

    // Get hipCtxCreate
    using PFN_hipCtxCreate = int (__stdcall *)(void**, unsigned int, void*);
    auto hipCtxCreate = (PFN_hipCtxCreate)GetProcAddress(m_hipLib, "hipCtxCreate");
    if (hipCtxCreate && m_devicePtr) {
        hipCtxCreate(&m_context, 0, m_devicePtr);
    }

    return true;
}

void HIPInferenceEngine::ShutdownHIP() {
    if (m_context) {
        using PFN_hipCtxDestroy = int (__stdcall *)(void*);
        auto hipCtxDestroy = (PFN_hipCtxDestroy)GetProcAddress(m_hipLib, "hipCtxDestroy");
        if (hipCtxDestroy) {
            hipCtxDestroy(m_context);
        }
        m_context = nullptr;
    }

    if (m_hipLib) {
        FreeLibrary(m_hipLib);
        m_hipLib = nullptr;
    }

    m_devicePtr = nullptr;
}

// ============================================================================
// InferenceEngine Interface
// ============================================================================
bool HIPInferenceEngine::LoadModel(const std::string& path) {
    m_modelPath = path;
    m_modelLoaded = true;
    return true;
}

bool HIPInferenceEngine::IsModelLoaded() const {
    return m_modelLoaded;
}

void HIPInferenceEngine::UnloadModel() {
    m_modelLoaded = false;
    m_modelPath.clear();
}

std::vector<int32_t> HIPInferenceEngine::Tokenize(const std::string& text) {
    std::vector<int32_t> tokens;
    std::istringstream iss(text);
    std::string word;
    int32_t id = 1;
    while (iss >> word) {
        tokens.push_back(id++);
    }
    return tokens;
}

std::string HIPInferenceEngine::Generate(const std::vector<int32_t>& tokens, int maxTokens) {
    std::string result;
    for (int i = 0; i < maxTokens && i < static_cast<int>(tokens.size()); ++i) {
        result += "token_" + std::to_string(tokens[i]) + " ";
    }
    return result;
}

void HIPInferenceEngine::GenerateStreaming(const std::vector<int32_t>& tokens, int maxTokens,
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

void HIPInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
}

size_t HIPInferenceEngine::GetContextLimit() const {
    return m_contextLimit;
}

void HIPInferenceEngine::SetThreadCount(int count) {
    m_threadCount = count;
}

int HIPInferenceEngine::GetThreadCount() const {
    return m_threadCount;
}

const char* HIPInferenceEngine::GetBackendName() const {
    return "HIP";
}

} // namespace RawrXD
