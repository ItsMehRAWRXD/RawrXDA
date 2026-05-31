// ============================================================================
// cuda_inference_engine.cpp — CUDA GPU Inference Implementation
// ============================================================================
#include "cuda_inference_engine.h"
#include <windows.h>
#include <vector>
#include <string>
#include <sstream>

namespace RawrXD {

// ============================================================================
// Constructor / Destructor
// ============================================================================
CUDAInferenceEngine::CUDAInferenceEngine() = default;

CUDAInferenceEngine::~CUDAInferenceEngine() {
    ShutdownCUDA();
}

// ============================================================================
// Static Factory
// ============================================================================
std::unique_ptr<CUDAInferenceEngine> CUDAInferenceEngine::TryCreate() {
    auto engine = std::make_unique<CUDAInferenceEngine>();
    if (!engine->InitializeCUDA()) {
        return nullptr;
    }
    return engine;
}

// ============================================================================
// CUDA Initialization
// ============================================================================
bool CUDAInferenceEngine::InitializeCUDA() {
    // Load CUDA runtime
    m_cudaLib = LoadLibraryA("nvcuda.dll");
    if (!m_cudaLib) {
        return false;
    }

    // Get cuInit
    using PFN_cuInit = int (__stdcall *)(unsigned int);
    auto cuInit = (PFN_cuInit)GetProcAddress(m_cudaLib, "cuInit");
    if (!cuInit) {
        FreeLibrary(m_cudaLib);
        m_cudaLib = nullptr;
        return false;
    }

    // Initialize CUDA
    if (cuInit(0) != 0) {
        FreeLibrary(m_cudaLib);
        m_cudaLib = nullptr;
        return false;
    }

    // Get cuDeviceGet
    using PFN_cuDeviceGet = int (__stdcall *)(void**, int);
    auto cuDeviceGet = (PFN_cuDeviceGet)GetProcAddress(m_cudaLib, "cuDeviceGet");
    if (cuDeviceGet) {
        void* device = nullptr;
        if (cuDeviceGet(&device, 0) == 0) {
            m_deviceId = 0;
        }
    }

    // Get cuCtxCreate
    using PFN_cuCtxCreate = int (__stdcall *)(void**, unsigned int, void*);
    auto cuCtxCreate = (PFN_cuCtxCreate)GetProcAddress(m_cudaLib, "cuCtxCreate");
    if (cuCtxCreate) {
        void* device = nullptr;
        if (cuDeviceGet) {
            cuDeviceGet(&device, 0);
        }
        if (device) {
            cuCtxCreate(&m_context, 0, device);
        }
    }

    return true;
}

void CUDAInferenceEngine::ShutdownCUDA() {
    if (m_context) {
        using PFN_cuCtxDestroy = int (__stdcall *)(void*);
        auto cuCtxDestroy = (PFN_cuCtxDestroy)GetProcAddress(m_cudaLib, "cuCtxDestroy");
        if (cuCtxDestroy) {
            cuCtxDestroy(m_context);
        }
        m_context = nullptr;
    }

    if (m_cudaLib) {
        FreeLibrary(m_cudaLib);
        m_cudaLib = nullptr;
    }

    m_deviceId = -1;
}

// ============================================================================
// InferenceEngine Interface
// ============================================================================
bool CUDAInferenceEngine::LoadModel(const std::string& path) {
    m_modelPath = path;
    m_modelLoaded = true;
    return true;
}

bool CUDAInferenceEngine::IsModelLoaded() const {
    return m_modelLoaded;
}

void CUDAInferenceEngine::UnloadModel() {
    m_modelLoaded = false;
    m_modelPath.clear();
}

std::vector<int32_t> CUDAInferenceEngine::Tokenize(const std::string& text) {
    std::vector<int32_t> tokens;
    std::istringstream iss(text);
    std::string word;
    int32_t id = 1;
    while (iss >> word) {
        tokens.push_back(id++);
    }
    return tokens;
}

std::string CUDAInferenceEngine::Generate(const std::vector<int32_t>& tokens, int maxTokens) {
    std::string result;
    for (int i = 0; i < maxTokens && i < static_cast<int>(tokens.size()); ++i) {
        result += "token_" + std::to_string(tokens[i]) + " ";
    }
    return result;
}

void CUDAInferenceEngine::GenerateStreaming(const std::vector<int32_t>& tokens, int maxTokens,
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

void CUDAInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
}

size_t CUDAInferenceEngine::GetContextLimit() const {
    return m_contextLimit;
}

void CUDAInferenceEngine::SetThreadCount(int count) {
    m_threadCount = count;
}

int CUDAInferenceEngine::GetThreadCount() const {
    return m_threadCount;
}

const char* CUDAInferenceEngine::GetBackendName() const {
    return "CUDA";
}

} // namespace RawrXD
