#include "gpu_backend.hpp"


#include <vector>
#include <cstring>

#ifdef 
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
    : void(nullptr)
{
}

GPUBackend::~GPUBackend() {
    shutdown();
}

bool GPUBackend::initialize() {
    if (m_initialized) {
        return true;
    }


    // Try backends in order of preference: CUDA > HIP > Vulkan > CPU
    if (initializeCUDA()) {
        m_backendType = CUDA;
        m_initialized = true;
        backendInitialized(CUDA);
        return true;
    }

    if (initializeHIP()) {
        m_backendType = HIP;
        m_initialized = true;
        backendInitialized(HIP);
        return true;
    }

    if (initializeVulkan()) {
        m_backendType = Vulkan;
        m_initialized = true;
        backendInitialized(Vulkan);
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
        m_vulkanPhysicalDevice = nullptr;
        m_vulkanBudgetSupported = false;
    }
#endif

    m_initialized = false;
    m_backendType = None;
    
}

bool GPUBackend::isAvailable() const {
    return m_initialized && m_backendType != CPU && m_backendType != None;
}

GPUBackend::BackendType GPUBackend::backendType() const {
    return m_backendType;
}

std::string GPUBackend::backendName() const {
    switch (m_backendType) {
        case CUDA: return "CUDA";
        case HIP: return "HIP (ROCm)";
        case Vulkan: return "Vulkan Compute";
        case CPU: return "CPU (Fallback)";
        default: return "None";
    }
}

std::vector<std::string> GPUBackend::availableDevices() const {
    return m_deviceList;
}

bool GPUBackend::selectDevice(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= m_deviceList.size()) {
        return false;
    }

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaSetDevice(deviceIndex);
        if (err != cudaSuccess) {
            return false;
        }
    }
#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipSetDevice(deviceIndex);
        if (err != hipSuccess) {
            return false;
        }
    }
#endif

    m_deviceIndex = deviceIndex;
    deviceChanged(deviceIndex);
    return true;
}

int GPUBackend::currentDevice() const {
    return m_deviceIndex;
}

std::string GPUBackend::deviceName(int deviceIndex) const {
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

#ifdef HAVE_VULKAN
    if (m_backendType == Vulkan) {
        // Use VK_EXT_memory_budget if available for accurate free VRAM reporting
        if (m_vulkanBudgetSupported && m_vulkanContext && m_vulkanPhysicalDevice) {
            auto instance = reinterpret_cast<VkInstance>(m_vulkanContext);
            auto physical = reinterpret_cast<VkPhysicalDevice>(m_vulkanPhysicalDevice);

            auto fpGetMemProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(
                vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties2"));

            if (fpGetMemProps2) {
                VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps{};
                budgetProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

                VkPhysicalDeviceMemoryProperties2 memProps2{};
                memProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
                memProps2.pNext = &budgetProps;

                fpGetMemProps2(physical, &memProps2);

                for (uint32_t i = 0; i < memProps2.memoryProperties.memoryHeapCount; ++i) {
                    if (memProps2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                        uint64_t budget = budgetProps.heapBudget[i];
                        uint64_t usage  = budgetProps.heapUsage[i];
                        if (budget > usage) {
                            free += static_cast<size_t>(budget - usage);
                        }
                    }
                }
            }
        }

        // Fallback: use total minus tracked allocations if budget not available or query failed
        if (free == 0 && m_totalMemory > m_allocatedMemory) {
            free = m_totalMemory - m_allocatedMemory;
        }
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
            return nullptr;
        }
    }
#endif

    // Check memory warning threshold (80%)
    if (m_allocatedMemory > m_totalMemory * 0.8) {
        memoryWarning(availableMemory(), m_totalMemory);
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

std::string GPUBackend::computeCapability() const {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, m_deviceIndex);
        return std::string("%1.%2");
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
        return false;
    }

    m_deviceList.clear();
    for (int i = 0; i < deviceCount; ++i) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        m_deviceList << std::string("%1 (Compute %2.%3)")


            ;
    }

    // Get memory info for device 0
    size_t free, total;
    cudaSetDevice(0);
    cudaMemGetInfo(&free, &total);
    m_totalMemory = total;

    
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
        return false;
    }

    m_deviceList.clear();
    for (int i = 0; i < deviceCount; ++i) {
        hipDeviceProp_t prop;
        hipGetDeviceProperties(&prop, i);
        m_deviceList << std::string(prop.name);
    }

    // Get memory info for device 0
    size_t free, total;
    hipSetDevice(0);
    hipMemGetInfo(&free, &total);
    m_totalMemory = total;

    
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
        return false;
    }

    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Get properties of first device (AMD RX 7800 XT)
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceProperties(devices[0], &deviceProperties);
    vkGetPhysicalDeviceMemoryProperties(devices[0], &memProperties);

    // Cache selected device handle for later memory queries
    m_vulkanPhysicalDevice = reinterpret_cast<void*>(devices[0]);

    // Detect VK_EXT_memory_budget support
    m_vulkanBudgetSupported = false;
    uint32_t extCount = 0;
    if (vkEnumerateDeviceExtensionProperties(devices[0], nullptr, &extCount, nullptr) == VK_SUCCESS && extCount > 0) {
        std::vector<VkExtensionProperties> exts(extCount);
        if (vkEnumerateDeviceExtensionProperties(devices[0], nullptr, &extCount, exts.data()) == VK_SUCCESS) {
            for (const auto& ext : exts) {
                if (std::strcmp(ext.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0) {
                    m_vulkanBudgetSupported = true;
                    break;
                }
            }
        }
    }

    m_deviceList.clear();
    m_deviceList << std::string("%1 (Vulkan %2.%3)")
        
        )
        );

    // Calculate total device memory (VRAM)
    m_totalMemory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            m_totalMemory = memProperties.memoryHeaps[i].size;
            break;
        }
    }

    
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
    
    backendInitialized(CPU);
}

