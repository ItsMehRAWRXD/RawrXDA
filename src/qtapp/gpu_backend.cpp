#include "gpu_backend.hpp"
#include <QDebug>
#include <QMutex>
#include <QVector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Check for CUDA availability at compile time
#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif

// Check for HIP availability
#ifdef HAVE_HIP
#include <hip/hip_runtime.h>
#endif

// Check for Vulkan availability
#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
#endif

GPUBackend& GPUBackend::instance() {
    static GPUBackend instance;
    return instance;
}

GPUBackend::GPUBackend()
    : QObject(nullptr)
{
}

GPUBackend::~GPUBackend() {
    shutdown();
}

bool GPUBackend::initialize() {
    if (m_initialized) {
        qInfo() << "[GPUBackend] Already initialized as" << backendName();
        return true;
    }

    qInfo() << "[GPUBackend] Initializing GPU backend...";

    // Try backends in order of preference: CUDA > HIP > Vulkan > CPU
    if (initializeCUDA()) {
        m_backendType = CUDA;
        m_initialized = true;
        emit backendInitialized(CUDA);
        qInfo() << "[GPUBackend] Initialized CUDA backend";
        return true;
    }

    if (initializeHIP()) {
        m_backendType = HIP;
        m_initialized = true;
        emit backendInitialized(HIP);
        qInfo() << "[GPUBackend] Initialized HIP backend";
        return true;
    }

    if (initializeVulkan()) {
        m_backendType = Vulkan;
        m_initialized = true;
        emit backendInitialized(Vulkan);
        qInfo() << "[GPUBackend] Initialized Vulkan backend";
        return true;
    }

    // Fallback to CPU
    fallbackToCPU();
    return false;
}

void GPUBackend::shutdown() {
    if (!m_initialized) return;

#ifdef HAVE_CUDA
    if (m_backendType == CUDA && m_cudaContext) {
        cudaDeviceReset();
        m_cudaContext = nullptr;
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP && m_hipContext) {
        hipDeviceReset();
        m_hipContext = nullptr;
    }
#endif

#ifdef HAVE_VULKAN
    if (m_backendType == Vulkan && m_vulkanContext) {
        VkInstance instance = (VkInstance)m_vulkanContext;
        vkDestroyInstance(instance, nullptr);
        m_vulkanContext = nullptr;
    }
#endif

    m_initialized = false;
    m_backendType = None;
    
    qInfo() << "[GPUBackend] Shutdown complete";
}

bool GPUBackend::isAvailable() const {
    return m_initialized && m_backendType != CPU && m_backendType != None;
}

GPUBackend::BackendType GPUBackend::backendType() const {
    return m_backendType;
}

QString GPUBackend::backendName() const {
    switch (m_backendType) {
        case CUDA: return "CUDA";
        case HIP: return "HIP (ROCm)";
        case Vulkan: return "Vulkan Compute";
        case CPU: return "CPU (Fallback)";
        default: return "None";
    }
}

QStringList GPUBackend::availableDevices() const {
    return m_deviceList;
}

bool GPUBackend::selectDevice(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= m_deviceList.size()) {
        qWarning() << "[GPUBackend] Invalid device index:" << deviceIndex;
        return false;
    }

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaSetDevice(deviceIndex);
        if (err != cudaSuccess) {
            qWarning() << "[GPUBackend] CUDA setDevice failed:" << cudaGetErrorString(err);
            return false;
        }
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipSetDevice(deviceIndex);
        if (err != hipSuccess) {
            qWarning() << "[GPUBackend] HIP setDevice failed";
            return false;
        }
    }
#endif

    m_deviceIndex = deviceIndex;
    emit deviceChanged(deviceIndex);
    qInfo() << "[GPUBackend] Selected device" << deviceIndex;
    return true;
}

int GPUBackend::currentDevice() const {
    return m_deviceIndex;
}

QString GPUBackend::deviceName(int deviceIndex) const {
    int idx = (deviceIndex < 0) ? m_deviceIndex : deviceIndex;
    if (idx >= 0 && idx < m_deviceList.size()) {
        return m_deviceList[idx];
    }
    return "Unknown";
}

size_t GPUBackend::totalMemory() const {
    return m_totalMemory;
}

size_t GPUBackend::availableMemory() const {
    size_t free = 0;

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        size_t total = 0;
        cudaMemGetInfo(&free, &total);
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        size_t total = 0;
        hipMemGetInfo(&free, &total);
    }
#endif

    return free;
}

size_t GPUBackend::usedMemory() const {
    return m_allocatedMemory;
}

void* GPUBackend::allocate(size_t size, MemoryType type) {
    void* ptr = nullptr;

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaSuccess;
        if (type == Device) {
            err = cudaMalloc(&ptr, size);
        } else if (type == Host) {
            err = cudaMallocHost(&ptr, size);
        } else if (type == Unified) {
            err = cudaMallocManaged(&ptr, size);
        }
        
        if (err == cudaSuccess) {
            m_allocatedMemory += size;
        } else {
            qWarning() << "[GPUBackend] CUDA allocation failed:" << cudaGetErrorString(err);
            return nullptr;
        }
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipSuccess;
        if (type == Device) {
            err = hipMalloc(&ptr, size);
        } else if (type == Host) {
            err = hipHostMalloc(&ptr, size);
        } else if (type == Unified) {
            err = hipMallocManaged(&ptr, size);
        }
        
        if (err == hipSuccess) {
            m_allocatedMemory += size;
        } else {
            qWarning() << "[GPUBackend] HIP allocation failed";
            return nullptr;
        }
    }
#endif

    // Check memory warning threshold (80%)
    if (m_allocatedMemory > m_totalMemory * 0.8) {
        emit memoryWarning(availableMemory(), m_totalMemory);
    }

    return ptr;
}

void GPUBackend::deallocate(void* ptr) {
    if (!ptr) return;

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaFree(ptr);
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipFree(ptr);
    }
#endif
}

bool GPUBackend::copyToDevice(void* dst, const void* src, size_t size) {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaMemcpy(dst, src, size, cudaMemcpyHostToDevice);
        return err == cudaSuccess;
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipMemcpy(dst, src, size, hipMemcpyHostToDevice);
        return err == hipSuccess;
    }
#endif

    return false;
}

bool GPUBackend::copyFromDevice(void* dst, const void* src, size_t size) {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaMemcpy(dst, src, size, cudaMemcpyDeviceToHost);
        return err == cudaSuccess;
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipMemcpy(dst, src, size, hipMemcpyDeviceToHost);
        return err == hipSuccess;
    }
#endif

    return false;
}

void GPUBackend::synchronize() {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaDeviceSynchronize();
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipDeviceSynchronize();
    }
#endif
}

QString GPUBackend::computeCapability() const {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, m_deviceIndex);
        return QString("%1.%2").arg(prop.major).arg(prop.minor);
    }
#endif

    return "Unknown";
}

float GPUBackend::expectedSpeedup() const {
    switch (m_backendType) {
        case CUDA: return 50.0f;   // 25-100x typical for NVIDIA
        case HIP: return 40.0f;    // 20-80x typical for AMD
        case Vulkan: return 15.0f; // 10-30x for compute shaders
        case CPU: return 1.0f;
        default: return 1.0f;
    }
}

bool GPUBackend::initializeCUDA() {
#ifdef HAVE_CUDA
    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    
    if (err != cudaSuccess || deviceCount == 0) {
        qInfo() << "[GPUBackend] CUDA not available";
        return false;
    }

    m_deviceList.clear();
    for (int i = 0; i < deviceCount; ++i) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        m_deviceList << QString("%1 (Compute %2.%3)")
            .arg(prop.name)
            .arg(prop.major)
            .arg(prop.minor);
    }

    // Get memory info for device 0
    size_t free, total;
    cudaSetDevice(0);
    cudaMemGetInfo(&free, &total);
    m_totalMemory = total;

    qInfo() << "[GPUBackend] Found" << deviceCount << "CUDA device(s)";
    qInfo() << "[GPUBackend] Device 0:" << m_deviceList[0];
    qInfo() << "[GPUBackend] Total memory:" << (m_totalMemory / (1024*1024)) << "MB";
    
    return true;
#else
    return false;
#endif
}

bool GPUBackend::initializeHIP() {
#ifdef HAVE_HIP
    int deviceCount = 0;
    hipError_t err = hipGetDeviceCount(&deviceCount);
    
    if (err != hipSuccess || deviceCount == 0) {
        qInfo() << "[GPUBackend] HIP/ROCm not available";
        return false;
    }

    m_deviceList.clear();
    for (int i = 0; i < deviceCount; ++i) {
        hipDeviceProp_t prop;
        hipGetDeviceProperties(&prop, i);
        m_deviceList << QString(prop.name);
    }

    // Get memory info for device 0
    size_t free, total;
    hipSetDevice(0);
    hipMemGetInfo(&free, &total);
    m_totalMemory = total;

    qInfo() << "[GPUBackend] Found" << deviceCount << "HIP device(s)";
    qInfo() << "[GPUBackend] Device 0:" << m_deviceList[0];
    qInfo() << "[GPUBackend] Total memory:" << (m_totalMemory / (1024*1024)) << "MB";
    
    return true;
#else
    return false;
#endif
}

bool GPUBackend::initializeVulkan() {
#ifdef HAVE_VULKAN
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RawrXD ModelLoader";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RawrXD";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        qInfo() << "[GPUBackend] Vulkan instance creation failed";
        return false;
    }

    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        qInfo() << "[GPUBackend] No Vulkan-compatible devices found";
        vkDestroyInstance(instance, nullptr);
        return false;
    }

    QVector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Get properties of first device (AMD RX 7800 XT)
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceProperties(devices[0], &deviceProperties);
    vkGetPhysicalDeviceMemoryProperties(devices[0], &memProperties);

    m_deviceList.clear();
    m_deviceList << QString("%1 (Vulkan %2.%3)")
        .arg(deviceProperties.deviceName)
        .arg(VK_VERSION_MAJOR(deviceProperties.apiVersion))
        .arg(VK_VERSION_MINOR(deviceProperties.apiVersion));

    // Calculate total device memory (VRAM)
    m_totalMemory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            m_totalMemory = memProperties.memoryHeaps[i].size;
            break;
        }
    }

    qInfo() << "[GPUBackend] Found" << deviceCount << "Vulkan device(s)";
    qInfo() << "[GPUBackend] Device 0:" << m_deviceList[0];
    qInfo() << "[GPUBackend] Total VRAM:" << (m_totalMemory / (1024*1024)) << "MB";
    qInfo() << "[GPUBackend] Driver Version:" << deviceProperties.driverVersion;
    
    // Store instance for later cleanup
    m_vulkanContext = (void*)instance;
    
    return true;
#else
    return false;
#endif
}

void GPUBackend::fallbackToCPU() {
    m_backendType = CPU;
    m_initialized = true;
    m_deviceList << "CPU (No GPU available)";
    
    qWarning() << "[GPUBackend] No GPU found, falling back to CPU";
    emit backendInitialized(CPU);
}
