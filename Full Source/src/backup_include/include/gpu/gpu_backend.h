#ifndef GPU_BACKEND_H
#define GPU_BACKEND_H

#include <cstdio>

// Move ggml_vk_init() into explicit GpuBackend::initialize() – return bool instead of abort().
// Graceful fallback: if no Vulkan → print log warning, switch to CPU and still load Phi-3 (slower but works).
class GpuBackend
{
public:
    enum Backend {
        CPU,
        Vulkan,
        CUDA
    };

    // Initialize GPU backend - returns true on success, false on fallback
    static bool initialize(Backend backend = Vulkan);

    // Get current backend
    static Backend currentBackend();

    // Check if backend is available
    static bool isBackendAvailable(Backend backend);

    // Get backend name as C-string
    static const char* backendName(Backend backend);

    // Gracefully initialize with fallback
    static bool initializeWithFallback();

private:
    static Backend s_currentBackend;
    static bool s_initialized;

    // Initialize Vulkan
    static bool initializeVulkan();

    // Initialize CUDA
    static bool initializeCuda();

    // Initialize CPU (always succeeds)
    static bool initializeCpu();
};

#endif // GPU_BACKEND_H