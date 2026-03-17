#include "gpu_backend.h"
#include <cstdio>

// Static member initialization
GpuBackend::Backend GpuBackend::s_currentBackend = GpuBackend::CPU;
bool GpuBackend::s_initialized = false;

bool GpuBackend::initialize(Backend backend)
{
    if (s_initialized && s_currentBackend == backend) {
        fprintf(stderr, "[GpuBackend] Already initialized: %s\n", backendName(backend));
        return true;
    }

    switch (backend) {
    case Vulkan:
        if (initializeVulkan()) {
            s_currentBackend = Vulkan;
            s_initialized = true;
            return true;
        }
        fprintf(stderr, "[GpuBackend] WARNING: Failed to initialize Vulkan, falling back to CPU\n");
        return initializeCpu();

    case CUDA:
        if (initializeCuda()) {
            s_currentBackend = CUDA;
            s_initialized = true;
            return true;
        }
        fprintf(stderr, "[GpuBackend] WARNING: Failed to initialize CUDA, falling back to CPU\n");
        return initializeCpu();

    case CPU:
        return initializeCpu();
    }

    return false;
}

GpuBackend::Backend GpuBackend::currentBackend()
{
    return s_currentBackend;
}

bool GpuBackend::isBackendAvailable(Backend backend)
{
    switch (backend) {
    case Vulkan:
        // In a real implementation, this would check for Vulkan support
        fprintf(stderr, "[GpuBackend] Checking Vulkan availability...\n");
        return true;  // Simplified for this example
    case CUDA:
        // In a real implementation, this would check for CUDA support
        fprintf(stderr, "[GpuBackend] Checking CUDA availability...\n");
        return false;  // Assume not available for this example
    case CPU:
        return true;  // CPU always available
    }
    return false;
}

const char* GpuBackend::backendName(Backend backend)
{
    switch (backend) {
    case CPU:
        return "CPU";
    case Vulkan:
        return "Vulkan";
    case CUDA:
        return "CUDA";
    }
    return "Unknown";
}

bool GpuBackend::initializeWithFallback()
{
    // Try Vulkan first
    if (isBackendAvailable(Vulkan) && initializeVulkan()) {
        s_currentBackend = Vulkan;
        s_initialized = true;
        fprintf(stderr, "[GpuBackend] OK: GPU Backend initialized with Vulkan\n");
        return true;
    }

    // Fall back to CUDA
    if (isBackendAvailable(CUDA) && initializeCuda()) {
        s_currentBackend = CUDA;
        s_initialized = true;
        fprintf(stderr, "[GpuBackend] OK: GPU Backend initialized with CUDA\n");
        return true;
    }

    // Final fallback to CPU
    fprintf(stderr, "[GpuBackend] WARNING: No GPU backends available, falling back to CPU (slower performance)\n");
    return initializeCpu();
}

bool GpuBackend::initializeVulkan()
{
    fprintf(stderr, "[GpuBackend] Attempting to initialize Vulkan...\n");
    
    // In a real implementation, this would:
    // 1. Call ggml_vk_init()
    // 2. Return true on success
    // 3. Return false on failure (no Vulkan support, etc.)
    
    // For this example, we'll simulate success
    fprintf(stderr, "[GpuBackend] OK: Vulkan initialized successfully\n");
    return true;
}

bool GpuBackend::initializeCuda()
{
    fprintf(stderr, "[GpuBackend] Attempting to initialize CUDA...\n");
    
    // In a real implementation, this would check for CUDA support
    // For this example, we'll simulate failure
    fprintf(stderr, "[GpuBackend] WARNING: CUDA not available\n");
    return false;
}

bool GpuBackend::initializeCpu()
{
    fprintf(stderr, "[GpuBackend] Initializing CPU backend...\n");
    s_currentBackend = CPU;
    s_initialized = true;
    fprintf(stderr, "[GpuBackend] OK: CPU backend ready (models will run on CPU - slower performance)\n");
    return true;
}