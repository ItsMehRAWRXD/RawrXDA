#pragma once
#include <stdint.h>
#include <stddef.h>

// Stub Vulkan header for environments without the Vulkan SDK.
// Provides minimal handle typedefs, structs, constants, and no-op functions
// required by the RawrXD Vulkan compute layer. This preserves buildability
// without external Vulkan dependencies.

#define RAWRXD_VULKAN_STUB 1

typedef struct VkInstance_T* VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkQueue_T* VkQueue;
typedef struct VkCommandPool_T* VkCommandPool;
typedef struct VkCommandBuffer_T* VkCommandBuffer;
typedef struct VkBuffer_T* VkBuffer;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
typedef struct VkDescriptorSetLayout_T* VkDescriptorSetLayout;
typedef struct VkDescriptorSet_T* VkDescriptorSet;
typedef struct VkDescriptorPool_T* VkDescriptorPool;
typedef struct VkShaderModule_T* VkShaderModule;
typedef struct VkPipelineLayout_T* VkPipelineLayout;
typedef struct VkPipeline_T* VkPipeline;
typedef struct VkFence_T* VkFence;

typedef uint64_t VkDeviceSize;
typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef uint32_t VkStructureType;
typedef uint32_t VkQueueFlags;
typedef uint32_t VkBufferUsageFlags;
typedef uint32_t VkCommandPoolCreateFlags;
typedef uint32_t VkCommandBufferUsageFlags;
typedef uint32_t VkDescriptorType;
typedef uint32_t VkShaderStageFlags;
typedef uint32_t VkMemoryPropertyFlags;
typedef uint32_t VkDescriptorPoolCreateFlags;
typedef uint32_t VkDescriptorSetLayoutCreateFlags;
typedef uint32_t VkPipelineCreateFlags;
typedef uint32_t VkSharingMode;

typedef int VkResult;

#define VK_SUCCESS 0
#define VK_NOT_READY 1
#define VK_TRUE 1
#define VK_NULL_HANDLE ((void*)0)

#define VK_MAKE_VERSION(major, minor, patch) (((major) << 22) | ((minor) << 12) | (patch))
#define VK_API_VERSION_1_3 VK_MAKE_VERSION(1, 3, 0)

#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 2
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 3
#define VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO 4
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 5
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO 6
#define VK_STRUCTURE_TYPE_FENCE_CREATE_INFO 7
#define VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO 8
#define VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO 9
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO 10
#define VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO 11
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO 12
#define VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET 13
#define VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO 14
#define VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO 15
#define VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO 16
#define VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO 17
#define VK_STRUCTURE_TYPE_SUBMIT_INFO 18

#define VK_QUEUE_COMPUTE_BIT 0x00000002
#define VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 0x00000020
#define VK_BUFFER_USAGE_TRANSFER_SRC_BIT 0x00000001
#define VK_BUFFER_USAGE_TRANSFER_DST_BIT 0x00000002
#define VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 0x00000002
#define VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 0x00000001
#define VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT 0x00000001
#define VK_DESCRIPTOR_TYPE_STORAGE_BUFFER 7
#define VK_SHADER_STAGE_COMPUTE_BIT 0x00000020
#define VK_PIPELINE_BIND_POINT_COMPUTE 1
#define VK_FENCE_CREATE_SIGNALED_BIT 0x00000001
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 0x00000001
#define VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 0x00000002
#define VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 0x00000004
#define VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 1
#define VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU 2

typedef struct VkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t deviceType;
    char deviceName[256];
} VkPhysicalDeviceProperties;

typedef struct VkPhysicalDeviceMemoryProperties {
    uint32_t memoryTypeCount;
    uint32_t memoryHeapCount;
} VkPhysicalDeviceMemoryProperties;

typedef struct VkApplicationInfo {
    VkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    uint32_t applicationVersion;
    const char* pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkDeviceQueueCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    uint32_t queueFamilyIndex;
    uint32_t queueCount;
    const float* pQueuePriorities;
} VkDeviceQueueCreateInfo;

typedef struct VkDeviceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    uint32_t queueCreateInfoCount;
    const VkDeviceQueueCreateInfo* pQueueCreateInfos;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const void* pEnabledFeatures;
} VkDeviceCreateInfo;

typedef struct VkCommandPoolCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkCommandPoolCreateFlags flags;
    uint32_t queueFamilyIndex;
} VkCommandPoolCreateInfo;

typedef struct VkCommandBufferAllocateInfo {
    VkStructureType sType;
    const void* pNext;
    VkCommandPool commandPool;
    uint32_t level;
    uint32_t commandBufferCount;
} VkCommandBufferAllocateInfo;

typedef struct VkCommandBufferBeginInfo {
    VkStructureType sType;
    const void* pNext;
    VkCommandBufferUsageFlags flags;
    const void* pInheritanceInfo;
} VkCommandBufferBeginInfo;

typedef struct VkFenceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
} VkFenceCreateInfo;

typedef struct VkBufferCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkSharingMode sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;
} VkBufferCreateInfo;

typedef struct VkMemoryAllocateInfo {
    VkStructureType sType;
    const void* pNext;
    VkDeviceSize allocationSize;
    uint32_t memoryTypeIndex;
} VkMemoryAllocateInfo;

typedef struct VkMemoryRequirements {
    VkDeviceSize size;
    VkDeviceSize alignment;
    uint32_t memoryTypeBits;
} VkMemoryRequirements;

typedef struct VkDescriptorPoolSize {
    VkDescriptorType type;
    uint32_t descriptorCount;
} VkDescriptorPoolSize;

typedef struct VkDescriptorPoolCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkDescriptorPoolCreateFlags flags;
    uint32_t maxSets;
    uint32_t poolSizeCount;
    const VkDescriptorPoolSize* pPoolSizes;
} VkDescriptorPoolCreateInfo;

typedef struct VkDescriptorSetLayoutBinding {
    uint32_t binding;
    VkDescriptorType descriptorType;
    uint32_t descriptorCount;
    VkShaderStageFlags stageFlags;
    const void* pImmutableSamplers;
} VkDescriptorSetLayoutBinding;

typedef struct VkDescriptorSetLayoutCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkDescriptorSetLayoutCreateFlags flags;
    uint32_t bindingCount;
    const VkDescriptorSetLayoutBinding* pBindings;
} VkDescriptorSetLayoutCreateInfo;

typedef struct VkDescriptorSetAllocateInfo {
    VkStructureType sType;
    const void* pNext;
    VkDescriptorPool descriptorPool;
    uint32_t descriptorSetCount;
    const VkDescriptorSetLayout* pSetLayouts;
} VkDescriptorSetAllocateInfo;

typedef struct VkDescriptorBufferInfo {
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize range;
} VkDescriptorBufferInfo;

typedef struct VkWriteDescriptorSet {
    VkStructureType sType;
    const void* pNext;
    VkDescriptorSet dstSet;
    uint32_t dstBinding;
    uint32_t dstArrayElement;
    uint32_t descriptorCount;
    VkDescriptorType descriptorType;
    const void* pImageInfo;
    const VkDescriptorBufferInfo* pBufferInfo;
    const void* pTexelBufferView;
} VkWriteDescriptorSet;

typedef struct VkPushConstantRange {
    VkShaderStageFlags stageFlags;
    uint32_t offset;
    uint32_t size;
} VkPushConstantRange;

typedef struct VkPipelineLayoutCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    uint32_t setLayoutCount;
    const VkDescriptorSetLayout* pSetLayouts;
    uint32_t pushConstantRangeCount;
    const VkPushConstantRange* pPushConstantRanges;
} VkPipelineLayoutCreateInfo;

typedef struct VkPipelineShaderStageCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkShaderStageFlags stage;
    VkShaderModule module;
    const char* pName;
    const void* pSpecializationInfo;
} VkPipelineShaderStageCreateInfo;

typedef struct VkComputePipelineCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkPipelineCreateFlags flags;
    VkPipelineShaderStageCreateInfo stage;
    VkPipelineLayout layout;
    VkPipeline basePipelineHandle;
    int32_t basePipelineIndex;
} VkComputePipelineCreateInfo;

typedef struct VkShaderModuleCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    size_t codeSize;
    const uint32_t* pCode;
} VkShaderModuleCreateInfo;

typedef struct VkSubmitInfo {
    VkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreCount;
    const void* pWaitSemaphores;
    const void* pWaitDstStageMask;
    uint32_t commandBufferCount;
    const VkCommandBuffer* pCommandBuffers;
    uint32_t signalSemaphoreCount;
    const void* pSignalSemaphores;
} VkSubmitInfo;

typedef struct VkBufferCopy {
    VkDeviceSize srcOffset;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
} VkBufferCopy;

typedef struct VkQueueFamilyProperties {
    VkQueueFlags queueFlags;
    uint32_t queueCount;
    uint32_t timestampValidBits;
    struct { uint32_t width; uint32_t height; uint32_t depth; } minImageTransferGranularity;
} VkQueueFamilyProperties;

typedef struct VkAllocationCallbacks { int unused; } VkAllocationCallbacks;

// No-op stub functions
static inline VkResult vkCreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*) { return VK_SUCCESS; }
static inline void vkDestroyInstance(VkInstance, const VkAllocationCallbacks*) {}
static inline VkResult vkEnumeratePhysicalDevices(VkInstance, uint32_t*, VkPhysicalDevice*) { return VK_SUCCESS; }
static inline void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties*) {}
static inline void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*) {}
static inline void vkGetPhysicalDeviceProperties(VkPhysicalDevice, VkPhysicalDeviceProperties*) {}
static inline VkResult vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*) { return VK_SUCCESS; }
static inline void vkDestroyDevice(VkDevice, const VkAllocationCallbacks*) {}
static inline void vkGetDeviceQueue(VkDevice, uint32_t, uint32_t, VkQueue*) {}
static inline VkResult vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool*) { return VK_SUCCESS; }
static inline void vkDestroyCommandPool(VkDevice, VkCommandPool, const VkAllocationCallbacks*) {}
static inline VkResult vkAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*) { return VK_SUCCESS; }
static inline VkResult vkFreeCommandBuffers(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*) { return VK_SUCCESS; }
static inline VkResult vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }
static inline VkResult vkEndCommandBuffer(VkCommandBuffer) { return VK_SUCCESS; }
static inline VkResult vkResetCommandBuffer(VkCommandBuffer, VkFlags) { return VK_SUCCESS; }
static inline VkResult vkCreateBuffer(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*) { return VK_SUCCESS; }
static inline void vkDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*) {}
static inline void vkGetBufferMemoryRequirements(VkDevice, VkBuffer, VkMemoryRequirements*) {}
static inline VkResult vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory*) { return VK_SUCCESS; }
static inline void vkFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*) {}
static inline VkResult vkBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize) { return VK_SUCCESS; }
static inline VkResult vkMapMemory(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkFlags, void** data) { if (data) *data = nullptr; return VK_SUCCESS; }
static inline void vkUnmapMemory(VkDevice, VkDeviceMemory) {}
static inline VkResult vkCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence*) { return VK_SUCCESS; }
static inline void vkDestroyFence(VkDevice, VkFence, const VkAllocationCallbacks*) {}
static inline VkResult vkWaitForFences(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t) { return VK_SUCCESS; }
static inline VkResult vkResetFences(VkDevice, uint32_t, const VkFence*) { return VK_SUCCESS; }
static inline VkResult vkQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo*, VkFence) { return VK_SUCCESS; }
static inline VkResult vkQueueWaitIdle(VkQueue) { return VK_SUCCESS; }
static inline VkResult vkDeviceWaitIdle(VkDevice) { return VK_SUCCESS; }
static inline VkResult vkGetFenceStatus(VkDevice, VkFence) { return VK_SUCCESS; }
static inline VkResult vkCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool*) { return VK_SUCCESS; }
static inline void vkDestroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*) {}
static inline VkResult vkCreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout*) { return VK_SUCCESS; }
static inline void vkDestroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*) {}
static inline VkResult vkAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*) { return VK_SUCCESS; }
static inline VkResult vkFreeDescriptorSets(VkDevice, VkDescriptorPool, uint32_t, const VkDescriptorSet*) { return VK_SUCCESS; }
static inline void vkUpdateDescriptorSets(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const void*) {}
static inline VkResult vkCreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout*) { return VK_SUCCESS; }
static inline void vkDestroyPipelineLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*) {}
static inline VkResult vkCreateShaderModule(VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule*) { return VK_SUCCESS; }
static inline void vkDestroyShaderModule(VkDevice, VkShaderModule, const VkAllocationCallbacks*) {}
static inline VkResult vkCreateComputePipelines(VkDevice, VkPipeline, uint32_t, const VkComputePipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*) { return VK_SUCCESS; }
static inline void vkDestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) {}
static inline void vkCmdBindPipeline(VkCommandBuffer, uint32_t, VkPipeline) {}
static inline void vkCmdBindDescriptorSets(VkCommandBuffer, uint32_t, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*) {}
static inline void vkCmdDispatch(VkCommandBuffer, uint32_t, uint32_t, uint32_t) {}
static inline void vkCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*) {}
static inline void vkCmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*) {}
