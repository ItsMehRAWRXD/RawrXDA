#include "gpu_backend.h"
#include <QDebug>

// Static member initialization
GpuBackend::Backend GpuBackend::s_currentBackend = GpuBackend::CPU;
bool GpuBackend::s_initialized = false;

bool GpuBackend::initialize(Backend backend)
{
    if (s_initialized && s_currentBackend == backend) {
        qDebug() << "GPU backend already initialized:" << backendName(backend);
        return true;
    }

    switch (backend) {
    case Vulkan:
        if (initializeVulkan()) {
            s_currentBackend = Vulkan;
            s_initialized = true;
            return true;
        }
        qWarning() << "Failed to initialize Vulkan, falling back to CPU";
        return initializeCpu();

    case CUDA:
        if (initializeCuda()) {
            s_currentBackend = CUDA;
            s_initialized = true;
            return true;
        }
        qWarning() << "Failed to initialize CUDA, falling back to CPU";
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
        qDebug() << "Checking Vulkan availability...";
        return true;  // Simplified for this example
    case CUDA:
        // In a real implementation, this would check for CUDA support
        qDebug() << "Checking CUDA availability...";
        return false;  // Assume not available for this example
    case CPU:
        return true;  // CPU always available
    }
    return false;
}

QString GpuBackend::backendName(Backend backend)
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
        qInfo() << "✓ GPU Backend initialized with Vulkan";
        return true;
    }

    // Fall back to CUDA
    if (isBackendAvailable(CUDA) && initializeCuda()) {
        s_currentBackend = CUDA;
        s_initialized = true;
        qInfo() << "✓ GPU Backend initialized with CUDA";
        return true;
    }

    // Final fallback to CPU
    qWarning() << "No GPU backends available, falling back to CPU (slower performance)";
    return initializeCpu();
}

bool GpuBackend::initializeVulkan()
{
    qDebug() << "Attempting to initialize Vulkan...";
    
    // In a real implementation, this would:
    // 1. Call ggml_vk_init()
    // 2. Return true on success
    // 3. Return false on failure (no Vulkan support, etc.)
    
    // For this example, we'll simulate success
    qInfo() << "✓ Vulkan initialized successfully";
    return true;
}

bool GpuBackend::initializeCuda()
{
    qDebug() << "Attempting to initialize CUDA...";
    
    // In a real implementation, this would check for CUDA support
    // For this example, we'll simulate failure
    qWarning() << "CUDA not available";
    return false;
}

bool GpuBackend::initializeCpu()
{
    qDebug() << "Initializing CPU backend...";
    s_currentBackend = CPU;
    s_initialized = true;
    qInfo() << "✓ CPU backend ready (models will run on CPU - slower performance)";
    return true;
}