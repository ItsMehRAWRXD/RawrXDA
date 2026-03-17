#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <cstddef>
#include <cstdint>

// Suppress OpenGL headers for Qt (if not already defined)
#ifndef QT_NO_OPENGL
#define QT_NO_OPENGL
#endif

/**
 * @brief GPU backend abstraction for CUDA, HIP, and Vulkan
 * 
 * Features:
 * - Automatic GPU detection
 * - CUDA support (NVIDIA)
 * - HIP support (AMD ROCm)
 * - Vulkan compute support (cross-platform)
 * - Memory management
 * - Performance monitoring
 * - Fallback to CPU if no GPU available
 */
class GPUBackend : public QObject {
    Q_OBJECT

public:
    enum BackendType {
        None = 0,
        CUDA,
        HIP,
        Vulkan,
        CPU  // Fallback
    };

    enum MemoryType {
        Device,      // GPU VRAM
        Host,        // CPU RAM
        Unified      // Shared memory
    };

    static GPUBackend& instance();
    
    ~GPUBackend();

    /**
     * @brief Initialize GPU backend (auto-detects best available)
     * @return true if GPU backend initialized
     */
    bool initialize();

    /**
     * @brief Shutdown GPU backend
     */
    void shutdown();

    /**
     * @brief Check if GPU is available and initialized
     */
    bool isAvailable() const;

    /**
     * @brief Get current backend type
     */
    BackendType backendType() const;

    /**
     * @brief Get backend name string
     */
    QString backendName() const;

    /**
     * @brief List available GPU devices
     */
    QStringList availableDevices() const;

    /**
     * @brief Select device by index
     */
    bool selectDevice(int deviceIndex);

    /**
     * @brief Get current device index
     */
    int currentDevice() const;

    /**
     * @brief Get device name
     */
    QString deviceName(int deviceIndex = -1) const;

    /**
     * @brief Get total GPU memory in bytes
     */
    size_t totalMemory() const;

    /**
     * @brief Get available GPU memory in bytes
     */
    size_t availableMemory() const;

    /**
     * @brief Get used GPU memory in bytes
     */
    size_t usedMemory() const;

    /**
     * @brief Allocate GPU memory
     * @param size Size in bytes
     * @param type Memory type (Device, Host, Unified)
     * @return Pointer to allocated memory (nullptr on failure)
     */
    void* allocate(size_t size, MemoryType type = Device);

    /**
     * @brief Free GPU memory
     */
    void deallocate(void* ptr);

    /**
     * @brief Copy data to GPU
     */
    bool copyToDevice(void* dst, const void* src, size_t size);

    /**
     * @brief Copy data from GPU
     */
    bool copyFromDevice(void* dst, const void* src, size_t size);

    /**
     * @brief Synchronize GPU (wait for all operations to complete)
     */
    void synchronize();

    /**
     * @brief Get compute capability (CUDA) or feature level
     */
    QString computeCapability() const;

    /**
     * @brief Get expected speedup vs CPU (estimated)
     */
    float expectedSpeedup() const;

signals:
    void backendInitialized(BackendType type);
    void deviceChanged(int deviceIndex);
    void memoryWarning(size_t available, size_t total);
    void error(const QString& message);

private:
    GPUBackend();  // Singleton
    GPUBackend(const GPUBackend&) = delete;
    GPUBackend& operator=(const GPUBackend&) = delete;

    bool initializeCUDA();
    bool initializeHIP();
    bool initializeVulkan();
    void fallbackToCPU();

    BackendType m_backendType = None;
    int m_deviceIndex = 0;
    bool m_initialized = false;
    
    // Backend-specific handles (opaque pointers)
    void* m_cudaContext = nullptr;
    void* m_hipContext = nullptr;
    void* m_vulkanContext = nullptr;
    
    size_t m_totalMemory = 0;
    size_t m_allocatedMemory = 0;
    QStringList m_deviceList;
};
