<<<<<<< HEAD:history/all_versions/src/vulkan_stubs.cpp/1770892170396_TAOx.cpp
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
static PFN_vkCreateShaderModule          pfn_vkCreateShaderModule = nullptr;
static PFN_vkDestroyShaderModule         pfn_vkDestroyShaderModule = nullptr;
static PFN_vkCreateComputePipelines      pfn_vkCreateComputePipelines = nullptr;
static PFN_vkDestroyPipeline             pfn_vkDestroyPipeline = nullptr;
static PFN_vkCreatePipelineLayout        pfn_vkCreatePipelineLayout = nullptr;
static PFN_vkDestroyPipelineLayout       pfn_vkDestroyPipelineLayout = nullptr;
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

        VK_LOAD(vkCreateShaderModule);
        VK_LOAD(vkDestroyShaderModule);
        VK_LOAD(vkCreateComputePipelines);
        VK_LOAD(vkDestroyPipeline);
        VK_LOAD(vkCreatePipelineLayout);
        VK_LOAD(vkDestroyPipelineLayout);
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
        #undef VK_LOAD

        // Verify critical functions loaded
        s_vulkanAvailable = (pfn_vkCreateShaderModule && pfn_vkCmdDispatch &&
                             pfn_vkCreateComputePipelines && pfn_vkBeginCommandBuffer);

        fprintf(stderr, "[VulkanStubs] Vulkan runtime loaded — GPU compute %s\n",
                s_vulkanAvailable ? "ENABLED" : "PARTIAL (missing critical functions)");
    });
}

extern "C" {
    void VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyBuffer) pfn_vkDestroyBuffer(device, buffer, pAllocator);
    }

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
=======
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
static PFN_vkCreateShaderModule          pfn_vkCreateShaderModule = nullptr;
static PFN_vkDestroyShaderModule         pfn_vkDestroyShaderModule = nullptr;
static PFN_vkCreateComputePipelines      pfn_vkCreateComputePipelines = nullptr;
static PFN_vkDestroyPipeline             pfn_vkDestroyPipeline = nullptr;
static PFN_vkCreatePipelineLayout        pfn_vkCreatePipelineLayout = nullptr;
static PFN_vkDestroyPipelineLayout       pfn_vkDestroyPipelineLayout = nullptr;
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

        VK_LOAD(vkCreateShaderModule);
        VK_LOAD(vkDestroyShaderModule);
        VK_LOAD(vkCreateComputePipelines);
        VK_LOAD(vkDestroyPipeline);
        VK_LOAD(vkCreatePipelineLayout);
        VK_LOAD(vkDestroyPipelineLayout);
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
        #undef VK_LOAD

        // Verify critical functions loaded
        s_vulkanAvailable = (pfn_vkCreateShaderModule && pfn_vkCmdDispatch &&
                             pfn_vkCreateComputePipelines && pfn_vkBeginCommandBuffer);

        fprintf(stderr, "[VulkanStubs] Vulkan runtime loaded — GPU compute %s\n",
                s_vulkanAvailable ? "ENABLED" : "PARTIAL (missing critical functions)");
    });
}

extern "C" {
    void VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
        initVulkanDispatch();
        if (s_vulkanAvailable && pfn_vkDestroyBuffer) pfn_vkDestroyBuffer(device, buffer, pAllocator);
    }

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
>>>>>>> origin/main:src/vulkan_stubs.cpp
