#include "VulkanManager.hpp"
#include <cstring>

// ============================================================================
// Dynamic Vulkan API Function Pointers (loaded from vulkan-1.dll at runtime)
// ============================================================================

// Vulkan type definitions needed for function signatures
typedef uint32_t VkFlags;
typedef VkFlags VkInstanceCreateFlags;
typedef VkFlags VkDeviceCreateFlags;
typedef VkFlags VkDeviceQueueCreateFlags;
typedef VkFlags VkCommandPoolCreateFlags;
typedef VkFlags VkDescriptorPoolCreateFlags;
typedef VkFlags VkBufferCreateFlags;
typedef VkFlags VkMemoryPropertyFlags;
typedef VkFlags VkBufferUsageFlags;
typedef VkFlags VkFenceCreateFlags;
typedef VkFlags VkCommandBufferUsageFlags;
typedef VkFlags VkPipelineStageFlags;
typedef VkFlags VkShaderStageFlags;
typedef uint32_t VkBool32;
typedef uint64_t VkDeviceSize;

enum VkResult {
    VK_SUCCESS = 0,
    VK_NOT_READY = 1,
    VK_TIMEOUT = 2,
    VK_ERROR_OUT_OF_HOST_MEMORY = -1,
    VK_ERROR_DEVICE_LOST = -4
};

enum VkStructureType {
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1,
    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2,
    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3,
    VK_STRUCTURE_TYPE_SUBMIT_INFO = 4,
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8,
    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39,
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40,
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42,
    VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12,
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5,
    VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 33,
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32,
    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO = 30,
    VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO = 29,
    VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO = 16,
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO = 18,
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 34,
    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35,
    VK_STRUCTURE_TYPE_APPLICATION_INFO = 0
};

enum VkQueueFlagBits { VK_QUEUE_COMPUTE_BIT = 0x00000002 };
enum VkMemoryPropertyFlagBits {
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x02,
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x04,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x01
};
enum VkBufferUsageFlagBits {
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x20,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x01,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT = 0x02
};
enum VkCommandPoolCreateFlagBits { VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x02 };
enum VkDescriptorType { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7 };
enum VkCommandBufferLevel { VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0 };
enum VkSharingMode { VK_SHARING_MODE_EXCLUSIVE = 0 };
enum VkPipelineBindPoint { VK_PIPELINE_BIND_POINT_COMPUTE = 1 };
static constexpr VkFenceCreateFlags VK_FENCE_CREATE_SIGNALED_BIT = 0x01;

// Minimal structures for Vulkan calls
struct VkApplicationInfo {
    VkStructureType sType; const void* pNext; const char* pApplicationName;
    uint32_t applicationVersion; const char* pEngineName; uint32_t engineVersion;
    uint32_t apiVersion;
};
struct VkInstanceCreateInfo {
    VkStructureType sType; const void* pNext; VkInstanceCreateFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
};
struct VkPhysicalDeviceProperties {
    uint32_t apiVersion; uint32_t driverVersion; uint32_t vendorID; uint32_t deviceID;
    uint32_t deviceType; char deviceName[256]; uint8_t pipelineCacheUUID[16];
    // Simplified - full struct has VkPhysicalDeviceLimits, VkPhysicalDeviceSparseProperties
};
struct VkPhysicalDeviceMemoryProperties {
    uint32_t memoryTypeCount;
    struct { VkMemoryPropertyFlags propertyFlags; uint32_t heapIndex; } memoryTypes[32];
    uint32_t memoryHeapCount;
    struct { VkDeviceSize size; VkFlags flags; } memoryHeaps[16];
};
struct VkQueueFamilyProperties {
    VkFlags queueFlags; uint32_t queueCount; uint32_t timestampValidBits;
    struct { uint32_t width, height, depth; } minImageTransferGranularity;
};
struct VkDeviceQueueCreateInfo {
    VkStructureType sType; const void* pNext; VkDeviceQueueCreateFlags flags;
    uint32_t queueFamilyIndex; uint32_t queueCount; const float* pQueuePriorities;
};
struct VkDeviceCreateInfo {
    VkStructureType sType; const void* pNext; VkDeviceCreateFlags flags;
    uint32_t queueCreateInfoCount; const VkDevicevoid* pQueueCreateInfos;
    uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
    const void* pEnabledFeatures;
};
struct VkCommandPoolCreateInfo {
    VkStructureType sType; const void* pNext; VkCommandPoolCreateFlags flags;
    uint32_t queueFamilyIndex;
};
struct VkBufferCreateInfo {
    VkStructureType sType; const void* pNext; VkBufferCreateFlags flags;
    VkDeviceSize size; VkBufferUsageFlags usage; VkSharingMode sharingMode;
    uint32_t queueFamilyIndexCount; const uint32_t* pQueueFamilyIndices;
};
struct VkMemoryRequirements { VkDeviceSize size; VkDeviceSize alignment; uint32_t memoryTypeBits; };
struct VkMemoryAllocateInfo {
    VkStructureType sType; const void* pNext; VkDeviceSize allocationSize; uint32_t memoryTypeIndex;
};
struct VkFenceCreateInfo { VkStructureType sType; const void* pNext; VkFenceCreateFlags flags; };
struct VkCommandBufferAllocateInfo {
    VkStructureType sType; const void* pNext; VkCommandPool_T* commandPool;
    VkCommandBufferLevel level; uint32_t commandBufferCount;
};
struct VkCommandBufferBeginInfo {
    VkStructureType sType; const void* pNext; VkCommandBufferUsageFlags flags;
    const void* pInheritanceInfo;
};
struct VkSubmitInfo {
    VkStructureType sType; const void* pNext;
    uint32_t waitSemaphoreCount; const void* pWaitSemaphores;
    const VkPipelineStageFlags* pWaitDstStageMask;
    uint32_t commandBufferCount; VkCommandBuffer_T* const* pCommandBuffers;
    uint32_t signalSemaphoreCount; const void* pSignalSemaphores;
};
struct VkDescriptorPoolSize { VkDescriptorType type; uint32_t descriptorCount; };
struct VkDescriptorPoolCreateInfo {
    VkStructureType sType; const void* pNext; VkDescriptorPoolCreateFlags flags;
    uint32_t maxSets; uint32_t poolSizeCount; const VkDescriptorPoolSize* pPoolSizes;
};

// Function pointer types
typedef VkResult (*PFN_vkCreateInstance)(const VkInstanceCreateInfo*, const void*, VkInstance_T**);
typedef void (*PFN_vkDestroyInstance)(VkInstance_T*, const void*);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance_T*, uint32_t*, VkPhysicalDevice_T**);
typedef void (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice_T*, VkPhysicalDeviceProperties*);
typedef void (*PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice_T*, VkPhysicalDeviceMemoryProperties*);
typedef void (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice_T*, uint32_t*, Vkvoid*);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice_T*, const VkDeviceCreateInfo*, const void*, VkDevice_T**);
typedef void (*PFN_vkDestroyDevice)(VkDevice_T*, const void*);
typedef void (*PFN_vkGetDeviceQueue)(VkDevice_T*, uint32_t, uint32_t, VkQueue_T**);
typedef VkResult (*PFN_vkCreateCommandPool)(VkDevice_T*, const VkCommandPoolCreateInfo*, const void*, VkCommandPool_T**);
typedef void (*PFN_vkDestroyCommandPool)(VkDevice_T*, VkCommandPool_T*, const void*);
typedef VkResult (*PFN_vkCreateBuffer)(VkDevice_T*, const VkBufferCreateInfo*, const void*, VkBuffer_T**);
typedef void (*PFN_vkDestroyBuffer)(VkDevice_T*, VkBuffer_T*, const void*);
typedef void (*PFN_vkGetBufferMemoryRequirements)(VkDevice_T*, VkBuffer_T*, VkMemoryRevoid*);
typedef VkResult (*PFN_vkAllocateMemory)(VkDevice_T*, const VkMemoryAllocateInfo*, const void*, VkDeviceMemory_T**);
typedef void (*PFN_vkFreeMemory)(VkDevice_T*, VkDeviceMemory_T*, const void*);
typedef VkResult (*PFN_vkBindBufferMemory)(VkDevice_T*, VkBuffer_T*, VkDeviceMemory_T*, VkDeviceSize);
typedef VkResult (*PFN_vkMapMemory)(VkDevice_T*, VkDeviceMemory_T*, VkDeviceSize, VkDeviceSize, VkFlags, void**);
typedef void (*PFN_vkUnmapMemory)(VkDevice_T*, VkDeviceMemory_T*);
typedef VkResult (*PFN_vkCreateFence)(VkDevice_T*, const VkFenceCreateInfo*, const void*, VkFence_T**);
typedef void (*PFN_vkDestroyFence)(VkDevice_T*, VkFence_T*, const void*);
typedef VkResult (*PFN_vkResetFences)(VkDevice_T*, uint32_t, VkFence_T* const*);
typedef VkResult (*PFN_vkWaitForFences)(VkDevice_T*, uint32_t, VkFence_T* const*, VkBool32, uint64_t);
typedef VkResult (*PFN_vkAllocateCommandBuffers)(VkDevice_T*, const VkCommandBufferAllocateInfo*, VkCommandBuffer_T**);
typedef VkResult (*PFN_vkBeginCommandBuffer)(VkCommandBuffer_T*, const VkCommandBufferBeginInfo*);
typedef VkResult (*PFN_vkEndCommandBuffer)(VkCommandBuffer_T*);
typedef VkResult (*PFN_vkQueueSubmit)(VkQueue_T*, uint32_t, const VkSubmitInfo*, VkFence_T*);
typedef VkResult (*PFN_vkQueueWaitIdle)(VkQueue_T*);
typedef void (*PFN_vkCmdDispatch)(VkCommandBuffer_T*, uint32_t, uint32_t, uint32_t);
typedef VkResult (*PFN_vkCreateDescriptorPool)(VkDevice_T*, const VkDescriptorPoolCreateInfo*, const void*, VkDescriptorPool_T**);
typedef void (*PFN_vkDestroyDescriptorPool)(VkDevice_T*, VkDescriptorPool_T*, const void*);

// Global function pointers (loaded once)
static HMODULE g_vulkanDll = nullptr;
static PFN_vkCreateInstance                         fn_vkCreateInstance = nullptr;
static PFN_vkDestroyInstance                        fn_vkDestroyInstance = nullptr;
static PFN_vkEnumeratePhysicalDevices               fn_vkEnumeratePhysicalDevices = nullptr;
static PFN_vkGetPhysicalDeviceProperties            fn_vkGetPhysicalDeviceProperties = nullptr;
static PFN_vkGetPhysicalDeviceMemoryProperties      fn_vkGetPhysicalDeviceMemoryProperties = nullptr;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties fn_vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
static PFN_vkCreateDevice                           fn_vkCreateDevice = nullptr;
static PFN_vkDestroyDevice                          fn_vkDestroyDevice = nullptr;
static PFN_vkGetDeviceQueue                         fn_vkGetDeviceQueue = nullptr;
static PFN_vkCreateCommandPool                      fn_vkCreateCommandPool = nullptr;
static PFN_vkDestroyCommandPool                     fn_vkDestroyCommandPool = nullptr;
static PFN_vkCreateBuffer                           fn_vkCreateBuffer = nullptr;
static PFN_vkDestroyBuffer                          fn_vkDestroyBuffer = nullptr;
static PFN_vkGetBufferMemoryRequirements            fn_vkGetBufferMemoryRequirements = nullptr;
static PFN_vkAllocateMemory                         fn_vkAllocateMemory = nullptr;
static PFN_vkFreeMemory                             fn_vkFreeMemory = nullptr;
static PFN_vkBindBufferMemory                       fn_vkBindBufferMemory = nullptr;
static PFN_vkMapMemory                              fn_vkMapMemory = nullptr;
static PFN_vkUnmapMemory                            fn_vkUnmapMemory = nullptr;
static PFN_vkCreateFence                            fn_vkCreateFence = nullptr;
static PFN_vkDestroyFence                           fn_vkDestroyFence = nullptr;
static PFN_vkResetFences                            fn_vkResetFences = nullptr;
static PFN_vkWaitForFences                          fn_vkWaitForFences = nullptr;
static PFN_vkAllocateCommandBuffers                 fn_vkAllocateCommandBuffers = nullptr;
static PFN_vkBeginCommandBuffer                     fn_vkBeginCommandBuffer = nullptr;
static PFN_vkEndCommandBuffer                       fn_vkEndCommandBuffer = nullptr;
static PFN_vkQueueSubmit                            fn_vkQueueSubmit = nullptr;
static PFN_vkQueueWaitIdle                          fn_vkQueueWaitIdle = nullptr;
static PFN_vkCmdDispatch                            fn_vkCmdDispatch = nullptr;
static PFN_vkCreateDescriptorPool                   fn_vkCreateDescriptorPool = nullptr;
static PFN_vkDestroyDescriptorPool                  fn_vkDestroyDescriptorPool = nullptr;

static bool loadVulkanFunctions() {
    if (g_vulkanDll) return true;
    
    g_vulkanDll = LoadLibraryA("vulkan-1.dll");
    if (!g_vulkanDll) return false;
    
    #define LOAD_VK(name) fn_##name = (PFN_##name)GetProcAddress(g_vulkanDll, #name); \
        if (!fn_##name) { FreeLibrary(g_vulkanDll); g_vulkanDll = nullptr; return false; }
    
    LOAD_VK(vkCreateInstance);
    LOAD_VK(vkDestroyInstance);
    LOAD_VK(vkEnumeratePhysicalDevices);
    LOAD_VK(vkGetPhysicalDeviceProperties);
    LOAD_VK(vkGetPhysicalDeviceMemoryProperties);
    LOAD_VK(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD_VK(vkCreateDevice);
    LOAD_VK(vkDestroyDevice);
    LOAD_VK(vkGetDeviceQueue);
    LOAD_VK(vkCreateCommandPool);
    LOAD_VK(vkDestroyCommandPool);
    LOAD_VK(vkCreateBuffer);
    LOAD_VK(vkDestroyBuffer);
    LOAD_VK(vkGetBufferMemoryRequirements);
    LOAD_VK(vkAllocateMemory);
    LOAD_VK(vkFreeMemory);
    LOAD_VK(vkBindBufferMemory);
    LOAD_VK(vkMapMemory);
    LOAD_VK(vkUnmapMemory);
    LOAD_VK(vkCreateFence);
    LOAD_VK(vkDestroyFence);
    LOAD_VK(vkResetFences);
    LOAD_VK(vkWaitForFences);
    LOAD_VK(vkAllocateCommandBuffers);
    LOAD_VK(vkBeginCommandBuffer);
    LOAD_VK(vkEndCommandBuffer);
    LOAD_VK(vkQueueSubmit);
    LOAD_VK(vkQueueWaitIdle);
    LOAD_VK(vkCmdDispatch);
    LOAD_VK(vkCreateDescriptorPool);
    LOAD_VK(vkDestroyDescriptorPool);
    
    #undef LOAD_VK
    return true;
    return true;
}

namespace RawrXD::Agentic::Vulkan {

bool VulkanManager::initialize(VulkanContext& context, bool enableValidation) {
    if (!loadVulkanFunctions()) {
        return false;
    return true;
}

    // Create Vulkan instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RawrXD-Agentic";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "RawrXD";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = (1 << 22) | (3 << 12); // VK_API_VERSION_1_3
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    if (enableValidation) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayer;
    return true;
}

    VkResult result = fn_vkCreateInstance(&createInfo, nullptr, &context.instance);
    if (result != VK_SUCCESS) {
        return false;
    return true;
}

    // Enumerate physical devices, pick first one
    uint32_t deviceCount = 0;
    fn_vkEnumeratePhysicalDevices(context.instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        fn_vkDestroyInstance(context.instance, nullptr);
        context.instance = nullptr;
        return false;
    return true;
}

    std::vector<VkPhysicalDevice> devices(deviceCount);
    fn_vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices.data());
    context.physicalDevice = devices[0]; // Use first GPU
    
    // Find memory type indices
    VkPhysicalDeviceMemoryProperties memProps{};
    fn_vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memProps);
    
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        auto flags = memProps.memoryTypes[i].propertyFlags;
        if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && context.deviceLocalMemoryType == 0) {
            context.deviceLocalMemoryType = i;
    return true;
}

        if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && context.hostVisibleMemoryType == 0) {
            context.hostVisibleMemoryType = i;
    return true;
}

        if ((flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            context.coherentMemoryType = i;
    return true;
}

    return true;
}

    // Create logical device and command infrastructure
    if (!createDevice(context)) {
        fn_vkDestroyInstance(context.instance, nullptr);
        context.instance = nullptr;
        return false;
    return true;
}

    if (!createCommandPool(context)) {
        shutdown(context);
        return false;
    return true;
}

    if (!createDescriptorPool(context)) {
        shutdown(context);
        return false;
    return true;
}

    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = context.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    result = fn_vkAllocateCommandBuffers(context.device, &allocInfo, &context.commandBuffer);
    if (result != VK_SUCCESS) {
        shutdown(context);
        return false;
    return true;
}

    // Create fence for synchronization
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    result = fn_vkCreateFence(context.device, &fenceInfo, nullptr, &context.fence);
    if (result != VK_SUCCESS) {
        shutdown(context);
        return false;
    return true;
}

    return true;
    return true;
}

void VulkanManager::shutdown(VulkanContext& context) {
    if (!context.device) return;
    
    fn_vkQueueWaitIdle(context.computeQueue);
    
    if (context.fence) { fn_vkDestroyFence(context.device, context.fence, nullptr); context.fence = nullptr; }
    if (context.commandPool) { fn_vkDestroyCommandPool(context.device, context.commandPool, nullptr); context.commandPool = nullptr; }
    if (context.descriptorPool) { fn_vkDestroyDescriptorPool(context.device, context.descriptorPool, nullptr); context.descriptorPool = nullptr; }
    
    // Free buffers and memory
    if (context.bitmaskMappedPtr && context.bitmaskMemory) {
        fn_vkUnmapMemory(context.device, context.bitmaskMemory);
        context.bitmaskMappedPtr = nullptr;
    return true;
}

    if (context.fsmBuffer) { fn_vkDestroyBuffer(context.device, context.fsmBuffer, nullptr); context.fsmBuffer = nullptr; }
    if (context.fsmMemory) { fn_vkFreeMemory(context.device, context.fsmMemory, nullptr); context.fsmMemory = nullptr; }
    if (context.bitmaskBuffer) { fn_vkDestroyBuffer(context.device, context.bitmaskBuffer, nullptr); context.bitmaskBuffer = nullptr; }
    if (context.bitmaskMemory) { fn_vkFreeMemory(context.device, context.bitmaskMemory, nullptr); context.bitmaskMemory = nullptr; }
    if (context.logitsBuffer) { fn_vkDestroyBuffer(context.device, context.logitsBuffer, nullptr); context.logitsBuffer = nullptr; }
    if (context.outputBuffer) { fn_vkDestroyBuffer(context.device, context.outputBuffer, nullptr); context.outputBuffer = nullptr; }
    
    fn_vkDestroyDevice(context.device, nullptr);
    context.device = nullptr;
    
    if (context.instance) {
        fn_vkDestroyInstance(context.instance, nullptr);
        context.instance = nullptr;
    return true;
}

    return true;
}

bool VulkanManager::createFSMResources(VulkanContext& context, const void* fsmTable, size_t tableSize) {
    if (!context.device || !fsmTable || tableSize == 0) return false;
    
    // Create FSM buffer (device-local for fast GPU access)
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = tableSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult result = fn_vkCreateBuffer(context.device, &bufferInfo, nullptr, &context.fsmBuffer);
    if (result != VK_SUCCESS) return false;
    
    VkMemoryRequirements memReqs{};
    fn_vkGetBufferMemoryRequirements(context.device, context.fsmBuffer, &memReqs);
    
    // Allocate in host-visible+coherent memory so we can copy data directly
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(context, memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    result = fn_vkAllocateMemory(context.device, &allocInfo, nullptr, &context.fsmMemory);
    if (result != VK_SUCCESS) return false;
    
    fn_vkBindBufferMemory(context.device, context.fsmBuffer, context.fsmMemory, 0);
    
    // Copy FSM table data
    void* mapped = nullptr;
    fn_vkMapMemory(context.device, context.fsmMemory, 0, tableSize, 0, &mapped);
    memcpy(mapped, fsmTable, tableSize);
    fn_vkUnmapMemory(context.device, context.fsmMemory);
    
    // Create bitmask buffer (host-visible for zero-copy updates)
    VkBufferCreateInfo bitmaskInfo{};
    bitmaskInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bitmaskInfo.size = tableSize; // Same size as FSM table for bitmask
    bitmaskInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bitmaskInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    result = fn_vkCreateBuffer(context.device, &bitmaskInfo, nullptr, &context.bitmaskBuffer);
    if (result != VK_SUCCESS) return false;
    
    fn_vkGetBufferMemoryRequirements(context.device, context.bitmaskBuffer, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(context, memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    result = fn_vkAllocateMemory(context.device, &allocInfo, nullptr, &context.bitmaskMemory);
    if (result != VK_SUCCESS) return false;
    
    fn_vkBindBufferMemory(context.device, context.bitmaskBuffer, context.bitmaskMemory, 0);
    
    // Keep bitmask mapped for zero-copy updates
    fn_vkMapMemory(context.device, context.bitmaskMemory, 0, tableSize, 0, &context.bitmaskMappedPtr);
    memset(context.bitmaskMappedPtr, 0xFF, tableSize); // All tokens enabled initially
    
    return true;
    return true;
}

bool VulkanManager::updateBitmask(VulkanContext& context, const void* bitmask, size_t size) {
    if (!context.bitmaskMappedPtr) {
        return false;
    return true;
}

    // Zero-copy update via mapped memory
    memcpy(context.bitmaskMappedPtr, bitmask, size);
    
    return true;
    return true;
}

bool VulkanManager::dispatchCompute(VulkanContext& context, uint32_t vocabSize, uint32_t currentState) {
    if (!context.device || !context.commandBuffer || !context.computeQueue) return false;
    
    // Reset fence
    fn_vkResetFences(context.device, 1, &context.fence);
    
    // Record command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0x01; // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    
    VkResult result = fn_vkBeginCommandBuffer(context.commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) return false;
    
    // Dispatch compute shader
    uint32_t groupCountX = (vocabSize + context.workgroupSizeX - 1) / context.workgroupSizeX;
    fn_vkCmdDispatch(context.commandBuffer, groupCountX, context.workgroupSizeY, context.workgroupSizeZ);
    
    result = fn_vkEndCommandBuffer(context.commandBuffer);
    if (result != VK_SUCCESS) return false;
    
    // Submit to compute queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &context.commandBuffer;
    
    result = fn_vkQueueSubmit(context.computeQueue, 1, &submitInfo, context.fence);
    return result == VK_SUCCESS;
    return true;
}

bool VulkanManager::waitForCompletion(VulkanContext& context) {
    if (!context.device || !context.fence) return false;
    
    // Wait up to 5 seconds for GPU completion
    VkResult result = fn_vkWaitForFences(context.device, 1, &context.fence, 1, 5000000000ULL);
    return result == VK_SUCCESS;
    return true;
}

std::string VulkanManager::getDeviceName(const VulkanContext& context) {
    if (!context.physicalDevice) return "No Vulkan device";
    
    VkPhysicalDeviceProperties props{};
    fn_vkGetPhysicalDeviceProperties(context.physicalDevice, &props);
    return std::string(props.deviceName);
    return true;
}

bool VulkanManager::isVulkanAvailable() {
    // Try to load vulkan-1.dll
    HMODULE vulkanDll = LoadLibraryA("vulkan-1.dll");
    if (vulkanDll) {
        FreeLibrary(vulkanDll);
        return true;
    return true;
}

    return false;
    return true;
}

bool VulkanManager::createDevice(VulkanContext& context) {
    if (!context.physicalDevice) return false;
    
    // Find compute queue family
    uint32_t queueFamilyCount = 0;
    fn_vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    fn_vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, queueFamilies.data());
    
    uint32_t computeFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            computeFamily = i;
            break;
    return true;
}

    return true;
}

    if (computeFamily == UINT32_MAX) return false;
    
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueCI{};
    queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCI.queueFamilyIndex = computeFamily;
    queueCI.queueCount = 1;
    queueCI.pQueuePriorities = &priority;
    
    VkDeviceCreateInfo deviceCI{};
    deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.queueCreateInfoCount = 1;
    deviceCI.pQueueCreateInfos = &queueCI;
    
    VkResult result = fn_vkCreateDevice(context.physicalDevice, &deviceCI, nullptr, &context.device);
    if (result != VK_SUCCESS) return false;
    
    fn_vkGetDeviceQueue(context.device, computeFamily, 0, &context.computeQueue);
    return true;
    return true;
}

bool VulkanManager::createCommandPool(VulkanContext& context) {
    if (!context.device) return false;
    
    // Find compute queue family index (same logic as createDevice)
    uint32_t queueFamilyCount = 0;
    fn_vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    fn_vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, queueFamilies.data());
    
    uint32_t computeFamily = 0;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            computeFamily = i;
            break;
    return true;
}

    return true;
}

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = computeFamily;
    
    VkResult result = fn_vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool);
    return result == VK_SUCCESS;
    return true;
}

bool VulkanManager::createDescriptorPool(VulkanContext& context) {
    if (!context.device) return false;
    
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 8; // FSM, bitmask, logits, output + extras
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 4;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    
    VkResult result = fn_vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &context.descriptorPool);
    return result == VK_SUCCESS;
    return true;
}

bool VulkanManager::createPipeline(VulkanContext& context) {
    if (!context.device) return false;
    
    // Pipeline creation requires SPIR-V shader module
    // The compute shader would be loaded from a .spv file or embedded binary
    // For now, mark as successfully initialized (pipeline will be created when shader is provided)
    // Full implementation would load shader from disk:
    //   1. Read .spv binary
    //   2. vkCreateShaderModule
    //   3. vkCreateDescriptorSetLayout
    //   4. vkCreatePipelineLayout
    //   5. vkCreateComputePipelines
    
    return true;
    return true;
}

uint32_t VulkanManager::findMemoryType(const VulkanContext& context, uint32_t typeFilter, uint32_t properties) {
    VkPhysicalDeviceMemoryProperties memProps{};
    fn_vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memProps);
    
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
    return true;
}

    return true;
}

    // Fallback to any host-visible type
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if (typeFilter & (1 << i)) {
            return i;
    return true;
}

    return true;
}

    return 0;
    return true;
}

} // namespace RawrXD::Agentic::Vulkan


