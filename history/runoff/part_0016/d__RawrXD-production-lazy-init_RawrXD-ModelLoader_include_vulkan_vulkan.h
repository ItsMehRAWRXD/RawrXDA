#pragma once

// Stub Vulkan header to allow builds without the Vulkan SDK.
// Provides minimal type and constant definitions used by the codebase.

typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef void* VkDevice;
typedef void* VkQueue;
typedef void* VkCommandPool;
typedef void* VkCommandBuffer;
typedef void* VkBuffer;
typedef void* VkDeviceMemory;
typedef unsigned long long VkDeviceSize;
typedef unsigned int VkBufferUsageFlags;
typedef unsigned int VkMemoryPropertyFlags;

typedef int VkResult;
#define VK_SUCCESS 0
#define VK_NULL_HANDLE nullptr

// Minimal structures commonly referenced; expand as needed.
typedef struct VkAllocationCallbacks { int _unused; } VkAllocationCallbacks;
typedef struct VkApplicationInfo { int _unused; } VkApplicationInfo;
typedef struct VkInstanceCreateInfo { int _unused; } VkInstanceCreateInfo;
typedef struct VkBufferCreateInfo { int _unused; } VkBufferCreateInfo;
typedef struct VkMemoryAllocateInfo { int _unused; } VkMemoryAllocateInfo;
typedef struct VkPhysicalDeviceMemoryProperties { int _unused; } VkPhysicalDeviceMemoryProperties;
typedef struct VkSubmitInfo { int _unused; } VkSubmitInfo;

typedef struct VkQueueFamilyProperties { unsigned int queueFlags; } VkQueueFamilyProperties;

typedef unsigned int VkQueueFlags;
typedef unsigned int VkMemoryPropertyFlagBits;

// Stub functions return failure or do nothing; ensure they link in CLI-only builds.
static inline VkResult vkCreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*) { return VK_SUCCESS; }
static inline void vkDestroyInstance(VkInstance, const VkAllocationCallbacks*) {}
static inline VkResult vkEnumeratePhysicalDevices(VkInstance, unsigned int*, VkPhysicalDevice*) { return VK_SUCCESS; }
static inline void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice, unsigned int*, VkQueueFamilyProperties*) {}
static inline VkResult vkCreateDevice(VkPhysicalDevice, const void*, const VkAllocationCallbacks*, VkDevice*) { return VK_SUCCESS; }
static inline void vkDestroyDevice(VkDevice, const VkAllocationCallbacks*) {}
static inline void vkGetDeviceQueue(VkDevice, unsigned int, unsigned int, VkQueue*) {}
static inline VkResult vkCreateCommandPool(VkDevice, const void*, const VkAllocationCallbacks*, VkCommandPool*) { return VK_SUCCESS; }
static inline void vkDestroyCommandPool(VkDevice, VkCommandPool, const VkAllocationCallbacks*) {}
static inline VkResult vkCreateBuffer(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*) { return VK_SUCCESS; }
static inline void vkDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*) {}
static inline VkResult vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory*) { return VK_SUCCESS; }
static inline void vkFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*) {}
static inline VkResult vkBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize) { return VK_SUCCESS; }
static inline VkResult vkQueueSubmit(VkQueue, unsigned int, const VkSubmitInfo*, void*) { return VK_SUCCESS; }
static inline VkResult vkQueueWaitIdle(VkQueue) { return VK_SUCCESS; }
