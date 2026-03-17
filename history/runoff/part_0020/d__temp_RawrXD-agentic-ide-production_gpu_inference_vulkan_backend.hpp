// ============================================================================
// MASM GPU Inference Backend - C++ Integration Header
// ============================================================================
// Bridges MASM assembly GPU backend with C++ InferenceEngine
// File: gpu_inference_vulkan_backend.hpp
// 
// This header provides C++ wrappers for MASM assembly functions that
// manage Vulkan GPU backend initialization and model loading.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>

// ============================================================================
// C LINKAGE FOR MASM FUNCTIONS
// ============================================================================

extern "C" {
    // Initialize Vulkan GPU backend (or fallback to CPU)
    // Return: handle to GPU/CPU backend, or nullptr if both fail
    void* InitializeGPUBackend(void);
    
    // Check if backend is GPU (1) or CPU (0)
    int IsGPUBackendActive(void* backend_handle);
    
    // Get human-readable backend info string
    const char* GetBackendInfo(void* backend_handle);
    
    // Get expected TPS for model on backend
    int PerformanceMetricsForBackend(void* backend_handle, const char* model_name);
    
    // Load model with specified backend
    int LoadModelWithBackend(void* backend_handle, const char* model_path, void* context_ptr);
    
    // Log backend status information
    void LogBackendStatus(void* backend_handle);
}

// ============================================================================
// C++ WRAPPER CLASS
// ============================================================================

class GPUBackendManager {
public:
    // Initialize GPU backend (singleton)
    static GPUBackendManager& Instance() {
        static GPUBackendManager instance;
        return instance;
    }
    
    // Get or initialize backend
    void* GetBackend() {
        if (!m_backend) {
            m_backend = InitializeGPUBackend();
            m_is_gpu = IsGPUBackendActive(m_backend);
        }
        return m_backend;
    }
    
    // Check if using GPU
    bool IsGPU() const {
        return m_is_gpu == 1;
    }
    
    // Get backend info
    std::string GetInfo() const {
        const char* info = GetBackendInfo(m_backend);
        return info ? std::string(info) : "UNKNOWN";
    }
    
    // Get expected TPS for model
    int GetExpectedTPS(const std::string& model_name) const {
        return PerformanceMetricsForBackend(m_backend, model_name.c_str());
    }
    
    // Log backend status
    void LogStatus() const {
        LogBackendStatus(m_backend);
    }
    
    // Load model with backend
    bool LoadModel(const std::string& model_path, void* context) {
        int result = LoadModelWithBackend(m_backend, model_path.c_str(), context);
        return result == 0;
    }
    
private:
    GPUBackendManager() : m_backend(nullptr), m_is_gpu(0) {}
    ~GPUBackendManager() = default;
    
    void* m_backend;
    int m_is_gpu;
};

// ============================================================================
// INLINE USAGE EXAMPLES
// ============================================================================

/*
// In InferenceEngine::loadModel():

bool InferenceEngine::loadModel(const QString& modelPath, const QString& tokenizePath) {
    QMutexLocker lock(&m_mutex);
    qInfo() << "[InferenceEngine] Loading model from:" << modelPath;
    QElapsedTimer timer;
    timer.start();

    // ===== NEW: GPU BACKEND INITIALIZATION (MASM) =====
    auto& backend_mgr = GPUBackendManager::Instance();
    void* backend = backend_mgr.GetBackend();
    
    if (!backend) {
        m_lastError = InferenceErrorCode::TRANSFORMER_ERROR;
        m_lastErrorMessage = "Failed to initialize GPU/CPU backend";
        emit modelLoadFailed(m_lastErrorMessage);
        return false;
    }
    
    // Log backend info
    qInfo() << "[InferenceEngine] Backend:" << QString::fromStdString(backend_mgr.GetInfo());
    qInfo() << "[InferenceEngine] Is GPU:" << (backend_mgr.IsGPU() ? "YES" : "NO");
    
    // Get expected TPS for this model
    std::string model_name = modelPath.toStdString();
    int expected_tps = backend_mgr.GetExpectedTPS(model_name);
    qInfo() << "[InferenceEngine] Expected TPS:" << expected_tps;
    
    // Load model with backend
    ModelContext context;
    if (!backend_mgr.LoadModel(modelPath.toStdString(), &context)) {
        m_lastError = InferenceErrorCode::MODEL_LOAD_FAILED;
        m_lastErrorMessage = "Failed to load model with backend";
        emit modelLoadFailed(m_lastErrorMessage);
        return false;
    }
    // ===== END GPU BACKEND CODE =====

    // Rest of existing model loading code...
    m_modelLoaded = true;
    m_gpuAvailable = backend_mgr.IsGPU();
    
    qint64 loadMs = timer.elapsed();
    qInfo() << "[InferenceEngine] Model loaded in" << loadMs << "ms";
    emit modelLoaded();
    return true;
}
*/

// ============================================================================
// CONTEXT STRUCTURE
// ============================================================================

struct ModelContext {
    void*   backend;          // Backend handle (GPU or CPU)
    int     is_gpu;           // 1 if GPU, 0 if CPU
    void*   model_data;       // Loaded model data pointer
    size_t  model_size;       // Size of model in bytes
    int     tensor_count;     // Number of tensors in model
    int     last_error;       // Error code (0 = success, negative = error)
};

#endif // GPU_INFERENCE_VULKAN_BACKEND_HPP
