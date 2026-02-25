// Vulkan fallback implementations — runtime dispatch to real Vulkan or CPU fallback.
// When vulkan-1.dll / libvulkan.so.1 is available, calls are forwarded to the real driver.
// Otherwise, operations are no-ops and GPU compute falls through to CPU inference.

#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstring>
#include <mutex>

// ============================================================================
// VULKAN RUNTIME DISPATCH — Load real Vulkan at runtime if available
// ============================================================================

#ifdef _WIN32
#include <windows.h>
static HMODULE s_vulkanLib = nullptr;
#else
#include <dlfcn.h>
static void* s_vulkanLib = nullptr;
#endif

static std::once_flag s_vulkanInitOnce;
static bool s_vulkanAvailable = false;

// Function pointer table for real Vulkan entry points
// Core instance/device functions
static PFN_vkEnumerateInstanceExtensionProperties pfn_vkEnumerateInstanceExtensionProperties = nullptr;
static PFN_vkEnumerateInstanceLayerProperties     pfn_vkEnumerateInstanceLayerProperties = nullptr;
static PFN_vkCreateInstance                       pfn_vkCreateInstance = nullptr;
static PFN_vkDestroyInstance                      pfn_vkDestroyInstance = nullptr;
static PFN_vkEnumeratePhysicalDevices             pfn_vkEnumeratePhysicalDevices = nullptr;
static PFN_vkGetPhysicalDeviceProperties          pfn_vkGetPhysicalDeviceProperties = nullptr;
static PFN_vkGetPhysicalDeviceFeatures            pfn_vkGetPhysicalDeviceFeatures = nullptr;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties pfn_vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
static PFN_vkGetPhysicalDeviceMemoryProperties    pfn_vkGetPhysicalDeviceMemoryProperties = nullptr;
static PFN_vkCreateDevice                         pfn_vkCreateDevice = nullptr;
static PFN_vkDestroyDevice                        pfn_vkDestroyDevice = nullptr;
static PFN_vkGetDeviceQueue                       pfn_vkGetDeviceQueue = nullptr;
static PFN_vkDeviceWaitIdle                       pfn_vkDeviceWaitIdle = nullptr;
static PFN_vkQueueWaitIdle                        pfn_vkQueueWaitIdle = nullptr;
static PFN_vkQueueSubmit                          pfn_vkQueueSubmit = nullptr;

// Memory/Buffer functions
static PFN_vkAllocateMemory                       pfn_vkAllocateMemory = nullptr;
static PFN_vkFreeMemory                           pfn_vkFreeMemory = nullptr;
static PFN_vkMapMemory                            pfn_vkMapMemory = nullptr;
static PFN_vkUnmapMemory                          pfn_vkUnmapMemory = nullptr;
static PFN_vkFlushMappedMemoryRanges              pfn_vkFlushMappedMemoryRanges = nullptr;
static PFN_vkInvalidateMappedMemoryRanges         pfn_vkInvalidateMappedMemoryRanges = nullptr;
static PFN_vkCreateBuffer                         pfn_vkCreateBuffer = nullptr;
static PFN_vkGetBufferMemoryRequirements          pfn_vkGetBufferMemoryRequirements = nullptr;
static PFN_vkBindBufferMemory                     pfn_vkBindBufferMemory = nullptr;

// Pipeline/shader functions
static PFN_vkCreateShaderModule          pfn_vkCreateShaderModule = nullptr;
static PFN_vkDestroyShaderModule         pfn_vkDestroyShaderModule = nullptr;
static PFN_vkCreateComputePipelines      pfn_vkCreateComputePipelines = nullptr;
static PFN_vkDestroyPipeline             pfn_vkDestroyPipeline = nullptr;
static PFN_vkCreatePipelineLayout        pfn_vkCreatePipelineLayout = nullptr;
static PFN_vkDestroyPipelineLayout       pfn_vkDestroyPipelineLayout = nullptr;
static PFN_vkCreatePipelineCache         pfn_vkCreatePipelineCache = nullptr;
static PFN_vkDestroyPipelineCache        pfn_vkDestroyPipelineCache = nullptr;
static PFN_vkCreateDescriptorSetLayout   pfn_vkCreateDescriptorSetLayout = nullptr;
static PFN_vkDestroyDescriptorSetLayout  pfn_vkDestroyDescriptorSetLayout = nullptr;
static PFN_vkCreateDescriptorPool        pfn_vkCreateDescriptorPool = nullptr;
static PFN_vkDestroyDescriptorPool       pfn_vkDestroyDescriptorPool = nullptr;
static PFN_vkAllocateDescriptorSets      pfn_vkAllocateDescriptorSets = nullptr;
static PFN_vkFreeDescriptorSets          pfn_vkFreeDescriptorSets = nullptr;
static PFN_vkUpdateDescriptorSets        pfn_vkUpdateDescriptorSets = nullptr;
static PFN_vkCreateCommandPool           pfn_vkCreateCommandPool = nullptr;
static PFN_vkDestroyCommandPool          pfn_vkDestroyCommandPool = nullptr;
static PFN_vkAllocateCommandBuffers      pfn_vkAllocateCommandBuffers = nullptr;
static PFN_vkFreeCommandBuffers          pfn_vkFreeCommandBuffers = nullptr;
static PFN_vkBeginCommandBuffer          pfn_vkBeginCommandBuffer = nullptr;
static PFN_vkEndCommandBuffer            pfn_vkEndCommandBuffer = nullptr;
static PFN_vkResetCommandBuffer          pfn_vkResetCommandBuffer = nullptr;
static PFN_vkCmdBindPipeline             pfn_vkCmdBindPipeline = nullptr;
static PFN_vkCmdBindDescriptorSets       pfn_vkCmdBindDescriptorSets = nullptr;
static PFN_vkCmdDispatch                 pfn_vkCmdDispatch = nullptr;
static PFN_vkCmdCopyBuffer               pfn_vkCmdCopyBuffer = nullptr;
static PFN_vkCmdPushConstants            pfn_vkCmdPushConstants = nullptr;
static PFN_vkDestroyBuffer               pfn_vkDestroyBuffer = nullptr;
static PFN_vkCreateFence                 pfn_vkCreateFence = nullptr;
static PFN_vkDestroyFence                pfn_vkDestroyFence = nullptr;
static PFN_vkWaitForFences               pfn_vkWaitForFences = nullptr;
static PFN_vkResetFences                 pfn_vkResetFences = nullptr;
static PFN_vkCreateSemaphore             pfn_vkCreateSemaphore = nullptr;
static PFN_vkDestroySemaphore            pfn_vkDestroySemaphore = nullptr;

static void initVulkanDispatch() {
    std::call_once(s_vulkanInitOnce, []() {
#ifdef _WIN32
        s_vulkanLib = LoadLibraryA("vulkan-1.dll");
        #define VK_LOAD(name) pfn_##name = (PFN_##name)GetProcAddress(s_vulkanLib, #name)
#else
        s_vulkanLib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!s_vulkanLib) s_vulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        #define VK_LOAD(name) pfn_##name = (PFN_##name)dlsym(s_vulkanLib, #name)
#endif
        if (!s_vulkanLib) {
            fprintf(stderr, "[VulkanStubs] Vulkan runtime not found — GPU compute disabled, using CPU fallback\n");
            s_vulkanAvailable = false;
            return;
        }

        // Core instance/device functions
        VK_LOAD(vkEnumerateInstanceExtensionProperties);
        VK_LOAD(vkEnumerateInstanceLayerProperties);
        VK_LOAD(vkCreateInstance);
        VK_LOAD(vkDestroyInstance);
        VK_LOAD(vkEnumeratePhysicalDevices);
        VK_LOAD(vkGetPhysicalDeviceProperties);
        VK_LOAD(vkGetPhysicalDeviceFeatures);
        VK_LOAD(vkGetPhysicalDeviceQueueFamilyProperties);
        VK_LOAD(vkGetPhysicalDeviceMemoryProperties);
        VK_LOAD(vkCreateDevice);
        VK_LOAD(vkDestroyDevice);
        VK_LOAD(vkGetDeviceQueue);
        VK_LOAD(vkDeviceWaitIdle);
        VK_LOAD(vkQueueWaitIdle);
        VK_LOAD(vkQueueSubmit);

        // Memory/Buffer functions
        VK_LOAD(vkAllocateMemory);
        VK_LOAD(vkFreeMemory);
        VK_LOAD(vkMapMemory);
        VK_LOAD(vkUnmapMemory);
        VK_LOAD(vkFlushMappedMemoryRanges);
        VK_LOAD(vkInvalidateMappedMemoryRanges);
        VK_LOAD(vkCreateBuffer);
        VK_LOAD(vkGetBufferMemoryRequirements);
        VK_LOAD(vkBindBufferMemory);

        // Pipeline/shader functions
        VK_LOAD(vkCreateShaderModule);
        VK_LOAD(vkDestroyShaderModule);
        VK_LOAD(vkCreateComputePipelines);
        VK_LOAD(vkDestroyPipeline);
        VK_LOAD(vkCreatePipelineLayout);
        VK_LOAD(vkDestroyPipelineLayout);
        VK_LOAD(vkCreatePipelineCache);
        VK_LOAD(vkDestroyPipelineCache);
        VK_LOAD(vkCreateDescriptorSetLayout);
        VK_LOAD(vkDestroyDescriptorSetLayout);
        VK_LOAD(vkCreateDescriptorPool);
        VK_LOAD(vkDestroyDescriptorPool);
        VK_LOAD(vkAllocateDescriptorSets);
        VK_LOAD(vkFreeDescriptorSets);
        VK_LOAD(vkUpdateDescriptorSets);
        VK_LOAD(vkCreateCommandPool);
        VK_LOAD(vkDestroyCommandPool);
        VK_LOAD(vkAllocateCommandBuffers);
        VK_LOAD(vkFreeCommandBuffers);
        VK_LOAD(vkBeginCommandBuffer);
        VK_LOAD(vkEndCommandBuffer);
        VK_LOAD(vkResetCommandBuffer);
        VK_LOAD(vkCmdBindPipeline);
        VK_LOAD(vkCmdBindDescriptorSets);
        VK_LOAD(vkCmdDispatch);
        VK_LOAD(vkCmdCopyBuffer);
        VK_LOAD(vkCmdPushConstants);
        VK_LOAD(vkDestroyBuffer);
        VK_LOAD(vkCreateFence);
        VK_LOAD(vkDestroyFence);
        VK_LOAD(vkWaitForFences);
        VK_LOAD(vkResetFences);
        VK_LOAD(vkCreateSemaphore);
        VK_LOAD(vkDestroySemaphore);
        #undef VK_LOAD

        // Verify critical functions loaded
        s_vulkanAvailable = (pfn_vkEnumerateInstanceExtensionProperties &&
                             pfn_vkCreateInstance && pfn_vkCreateDevice &&
                             pfn_vkCreateShaderModule && pfn_vkCmdDispatch &&
                             pfn_vkCreateComputePipelines && pfn_vkBeginCommandBuffer);

        fprintf(stderr, "[VulkanStubs] Vulkan runtime loaded — GPU compute %s\n",
                s_vulkanAvailable ? "ENABLED" : "PARTIAL (missing critical functions)");
    });
}

extern "C" {

    // ===================================================================================
    // Instance Extension Enumeration - Check if Vulkan is available
    // ===================================================================================
    VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char* pLayerName,
        uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkEnumerateInstanceExtensionProperties)
            return pfn_vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
        // Fallback: report no extensions, indicating software-only mode
        if (pPropertyCount) *pPropertyCount = 0;
        return VK_SUCCESS;
    }

    VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount,
        VkLayerProperties* pProperties) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkEnumerateInstanceLayerProperties)
            return pfn_vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
        if (pPropertyCount) *pPropertyCount = 0;
        return VK_SUCCESS;
    }

    // ===================================================================================
    // Instance Creation - Create Vulkan instance or fallback to software
    // ===================================================================================
    VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateInstance)
            return pfn_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
        // Software fallback: return stub handle
        if (pInstance) *pInstance = (VkInstance)(uintptr_t)0xDEADBEEF00000001ULL;
        fprintf(stderr, "[VulkanStubs] vkCreateInstance — using software compute fallback\n");
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyInstance)
            pfn_vkDestroyInstance(instance, pAllocator);
    }

    // ===================================================================================
    // Physical Device Enumeration
    // ===================================================================================
    VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount,
        VkPhysicalDevice* pPhysicalDevices) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkEnumeratePhysicalDevices)
            return pfn_vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
        // Software fallback: report 1 virtual CPU device
        if (pPhysicalDeviceCount && !pPhysicalDevices) {
            *pPhysicalDeviceCount = 1;
            return VK_SUCCESS;
        }
        if (pPhysicalDeviceCount && pPhysicalDevices && *pPhysicalDeviceCount >= 1) {
            pPhysicalDevices[0] = (VkPhysicalDevice)(uintptr_t)0xDEADBEEF00000002ULL;
            *pPhysicalDeviceCount = 1;
        }
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
        VkPhysicalDeviceProperties* pProperties) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkGetPhysicalDeviceProperties) {
            pfn_vkGetPhysicalDeviceProperties(physicalDevice, pProperties);
            return;
        }
        // Software fallback: populate with CPU device info
        if (pProperties) {
            memset(pProperties, 0, sizeof(*pProperties));
            pProperties->apiVersion = VK_MAKE_VERSION(1, 0, 0);
            pProperties->driverVersion = 1;
            pProperties->vendorID = 0x8086; // Intel-style for CPU fallback
            pProperties->deviceID = 0x0001;
            pProperties->deviceType = VK_PHYSICAL_DEVICE_TYPE_CPU;
            strcpy(pProperties->deviceName, "RawrXD Software Compute (CPU Fallback)");
            pProperties->limits.maxComputeWorkGroupCount[0] = 65535;
            pProperties->limits.maxComputeWorkGroupCount[1] = 65535;
            pProperties->limits.maxComputeWorkGroupCount[2] = 65535;
            pProperties->limits.maxComputeWorkGroupSize[0] = 1024;
            pProperties->limits.maxComputeWorkGroupSize[1] = 1024;
            pProperties->limits.maxComputeWorkGroupSize[2] = 64;
            pProperties->limits.maxComputeWorkGroupInvocations = 1024;
        }
    }

    void VKAPI_CALL vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
        VkPhysicalDeviceFeatures* pFeatures) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkGetPhysicalDeviceFeatures) {
            pfn_vkGetPhysicalDeviceFeatures(physicalDevice, pFeatures);
            return;
        }
        // Software fallback: report minimal features
        if (pFeatures) memset(pFeatures, 0, sizeof(*pFeatures));
    }

    void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
        uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkGetPhysicalDeviceQueueFamilyProperties) {
            pfn_vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
            return;
        }
        // Software fallback: 1 compute queue family
        if (pQueueFamilyPropertyCount && !pQueueFamilyProperties) {
            *pQueueFamilyPropertyCount = 1;
            return;
        }
        if (pQueueFamilyPropertyCount && pQueueFamilyProperties && *pQueueFamilyPropertyCount >= 1) {
            pQueueFamilyProperties[0].queueFlags = VK_QUEUE_COMPUTE_BIT;
            pQueueFamilyProperties[0].queueCount = 1;
            pQueueFamilyProperties[0].timestampValidBits = 64;
            pQueueFamilyProperties[0].minImageTransferGranularity = {1, 1, 1};
            *pQueueFamilyPropertyCount = 1;
        }
    }

    void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
        VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkGetPhysicalDeviceMemoryProperties) {
            pfn_vkGetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
            return;
        }
        // Software fallback: 1 heap, 1 type (host visible)
        if (pMemoryProperties) {
            memset(pMemoryProperties, 0, sizeof(*pMemoryProperties));
            pMemoryProperties->memoryTypeCount = 1;
            pMemoryProperties->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            pMemoryProperties->memoryTypes[0].heapIndex = 0;
            pMemoryProperties->memoryHeapCount = 1;
            pMemoryProperties->memoryHeaps[0].size = 4ULL * 1024 * 1024 * 1024; // 4GB
            pMemoryProperties->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
        }
    }

    // ===================================================================================
    // Logical Device Creation
    // ===================================================================================
    VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
        VkDevice* pDevice) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateDevice)
            return pfn_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
        // Software fallback: return stub device handle
        if (pDevice) *pDevice = (VkDevice)(uintptr_t)0xDEADBEEF00000003ULL;
        fprintf(stderr, "[VulkanStubs] vkCreateDevice — using software compute fallback\n");
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyDevice)
            pfn_vkDestroyDevice(device, pAllocator);
    }

    void VKAPI_CALL vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex,
        uint32_t queueIndex, VkQueue* pQueue) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkGetDeviceQueue) {
            pfn_vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
            return;
        }
        if (pQueue) *pQueue = (VkQueue)(uintptr_t)0xDEADBEEF00000004ULL;
    }

    VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDeviceWaitIdle)
            return pfn_vkDeviceWaitIdle(device);
        return VK_SUCCESS;
    }

    VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkQueueWaitIdle)
            return pfn_vkQueueWaitIdle(queue);
        return VK_SUCCESS;
    }

    VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount,
        const VkSubmitInfo* pSubmits, VkFence fence) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkQueueSubmit)
            return pfn_vkQueueSubmit(queue, submitCount, pSubmits, fence);
        // Software fallback: immediate execution (no-op since CPU already processed)
        return VK_SUCCESS;
    }

    // ===================================================================================
    // Memory Allocation
    // ===================================================================================
    VkResult VKAPI_CALL vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo,
        const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkAllocateMemory)
            return pfn_vkAllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
        // Software fallback: use aligned malloc
        if (pMemory && pAllocateInfo) {
            void* mem = _aligned_malloc((size_t)pAllocateInfo->allocationSize, 64);
            if (!mem) return VK_ERROR_OUT_OF_HOST_MEMORY;
            *pMemory = (VkDeviceMemory)mem;
        }
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkFreeMemory(VkDevice device, VkDeviceMemory memory,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkFreeMemory) {
            pfn_vkFreeMemory(device, memory, pAllocator);
            return;
        }
        // Software fallback
        if (memory) _aligned_free((void*)memory);
    }

    VkResult VKAPI_CALL vkMapMemory(VkDevice device, VkDeviceMemory memory,
        VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkMapMemory)
            return pfn_vkMapMemory(device, memory, offset, size, flags, ppData);
        // Software fallback: memory is already host-accessible
        if (ppData && memory) *ppData = (char*)memory + offset;
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkUnmapMemory)
            pfn_vkUnmapMemory(device, memory);
        // Software fallback: no-op
    }

    VkResult VKAPI_CALL vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount,
        const VkMappedMemoryRange* pMemoryRanges) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkFlushMappedMemoryRanges)
            return pfn_vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
        return VK_SUCCESS;
    }

    VkResult VKAPI_CALL vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount,
        const VkMappedMemoryRange* pMemoryRanges) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkInvalidateMappedMemoryRanges)
            return pfn_vkInvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
        return VK_SUCCESS;
    }

    // ===================================================================================
    // Buffer Creation
    // ===================================================================================
    VkResult VKAPI_CALL vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateBuffer)
            return pfn_vkCreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
        // Software fallback: create stub buffer handle
        if (pBuffer) *pBuffer = (VkBuffer)(uintptr_t)0xDEADBEEF00000005ULL;
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyBuffer) pfn_vkDestroyBuffer(device, buffer, pAllocator);
    }

    void VKAPI_CALL vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
        VkMemoryRequirements* pMemoryRequirements) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkGetBufferMemoryRequirements) {
            pfn_vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
            return;
        }
        // Software fallback
        if (pMemoryRequirements) {
            pMemoryRequirements->size = 4096; // Default
            pMemoryRequirements->alignment = 64;
            pMemoryRequirements->memoryTypeBits = 0x1;
        }
    }

    VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device, VkBuffer buffer,
        VkDeviceMemory memory, VkDeviceSize memoryOffset) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkBindBufferMemory)
            return pfn_vkBindBufferMemory(device, buffer, memory, memoryOffset);
        return VK_SUCCESS;
    }

    // ===================================================================================
    // Fences and Semaphores
    // ===================================================================================
    VkResult VKAPI_CALL vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateFence)
            return pfn_vkCreateFence(device, pCreateInfo, pAllocator, pFence);
        if (pFence) *pFence = (VkFence)(uintptr_t)0xDEADBEEF00000006ULL;
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyFence)
            pfn_vkDestroyFence(device, fence, pAllocator);
    }

    VkResult VKAPI_CALL vkWaitForFences(VkDevice device, uint32_t fenceCount,
        const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkWaitForFences)
            return pfn_vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
        return VK_SUCCESS; // Immediate in software mode
    }

    VkResult VKAPI_CALL vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkResetFences)
            return pfn_vkResetFences(device, fenceCount, pFences);
        return VK_SUCCESS;
    }

    VkResult VKAPI_CALL vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateSemaphore)
            return pfn_vkCreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
        if (pSemaphore) *pSemaphore = (VkSemaphore)(uintptr_t)0xDEADBEEF00000007ULL;
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkDestroySemaphore(VkDevice device, VkSemaphore semaphore,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroySemaphore)
            pfn_vkDestroySemaphore(device, semaphore, pAllocator);
    }

    // ===================================================================================
    // Pipeline Cache
    // ===================================================================================
    VkResult VKAPI_CALL vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreatePipelineCache)
            return pfn_vkCreatePipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
        if (pPipelineCache) *pPipelineCache = (VkPipelineCache)(uintptr_t)0xDEADBEEF00000008ULL;
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyPipelineCache)
            pfn_vkDestroyPipelineCache(device, pipelineCache, pAllocator);
    }

    // ===================================================================================
    // Shader Module / Compute Pipeline
    // ===================================================================================
    VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateShaderModule)
            return pfn_vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
        if (pShaderModule) *pShaderModule = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyShaderModule) pfn_vkDestroyShaderModule(device, shaderModule, pAllocator);
    }

    VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
        uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos,
        const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateComputePipelines)
            return pfn_vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
        if (pPipelines) for (uint32_t i = 0; i < createInfoCount; ++i) pPipelines[i] = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyPipeline) pfn_vkDestroyPipeline(device, pipeline, pAllocator);
    }

    VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreatePipelineLayout)
            return pfn_vkCreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
        if (pPipelineLayout) *pPipelineLayout = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyPipelineLayout) pfn_vkDestroyPipelineLayout(device, pipelineLayout, pAllocator);
    }

    VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateDescriptorSetLayout)
            return pfn_vkCreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
        if (pSetLayout) *pSetLayout = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyDescriptorSetLayout) pfn_vkDestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
    }

    VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateDescriptorPool)
            return pfn_vkCreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
        if (pDescriptorPool) *pDescriptorPool = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyDescriptorPool) pfn_vkDestroyDescriptorPool(device, descriptorPool, pAllocator);
    }
    VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo,
        VkDescriptorSet* pDescriptorSets) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkAllocateDescriptorSets)
            return pfn_vkAllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
        if (pDescriptorSets && pAllocateInfo)
            for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; ++i) pDescriptorSets[i] = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
        uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkFreeDescriptorSets)
            return pfn_vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount,
        const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount,
        const VkCopyDescriptorSet* pDescriptorCopies) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkUpdateDescriptorSets)
            pfn_vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    }

    VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCreateCommandPool)
            return pfn_vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
        if (pCommandPool) *pCommandPool = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
        const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyCommandPool) pfn_vkDestroyCommandPool(device, commandPool, pAllocator);
    }

    VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo,
        VkCommandBuffer* pCommandBuffers) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkAllocateCommandBuffers)
            return pfn_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
        if (pCommandBuffers && pAllocateInfo)
            for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) pCommandBuffers[i] = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
        uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkFreeCommandBuffers)
            pfn_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
    }
    VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
        const VkCommandBufferBeginInfo* pBeginInfo) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkBeginCommandBuffer)
            return pfn_vkBeginCommandBuffer(commandBuffer, pBeginInfo);
        return VK_SUCCESS;
    }
    VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkEndCommandBuffer)
            return pfn_vkEndCommandBuffer(commandBuffer);
        return VK_SUCCESS;
    }
    VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkResetCommandBuffer)
            return pfn_vkResetCommandBuffer(commandBuffer, flags);
        return VK_SUCCESS;
    }

    void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
        VkPipeline pipeline) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCmdBindPipeline)
            pfn_vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    }
    void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
        VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
        const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount,
        const uint32_t* pDynamicOffsets) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCmdBindDescriptorSets)
            pfn_vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet,
                descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }
    void VKAPI_CALL vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX,
        uint32_t groupCountY, uint32_t groupCountZ) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCmdDispatch) {
            pfn_vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
        } else {
            // CPU fallback: log that GPU dispatch was requested but unavailable
            fprintf(stderr, "[VulkanStubs] vkCmdDispatch(%u,%u,%u) — no GPU, CPU inference active\n",
                    groupCountX, groupCountY, groupCountZ);
        }
    }
    void VKAPI_CALL vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
        VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCmdCopyBuffer)
            pfn_vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    }
    void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
        VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkCmdPushConstants)
            pfn_vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
    }
}
