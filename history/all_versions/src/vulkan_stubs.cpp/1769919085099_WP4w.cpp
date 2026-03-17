// Minimal Vulkan stub implementations to satisfy linker when Vulkan library is unavailable.
// These functions provide no‑op behavior and return success codes where applicable.
// They are placed in the global namespace with C linkage to match the Vulkan API.

#include <vulkan/vulkan.h>

// Dynamic Vulkan Dispatcher
// Replaces static stubs with real dynamic loading of vulkan-1.dll
// This ensures that if the system has Vulkan, we use it. If not, we fail gracefully.

#include <vulkan/vulkan.h>
#include <windows.h>
#include <stdio.h>

extern "C" {

static HMODULE g_hVulkanInfo = NULL;

// Function pointers
typedef void (VKAPI_PTR *PFN_vkDestroyBuffer)(VkDevice, VkBuffer, const VkAllocationCallbacks*);
static PFN_vkDestroyBuffer real_vkDestroyBuffer = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkCreateShaderModule)(VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule*);
static PFN_vkCreateShaderModule real_vkCreateShaderModule = nullptr;

typedef void (VKAPI_PTR *PFN_vkDestroyShaderModule)(VkDevice, VkShaderModule, const VkAllocationCallbacks*);
static PFN_vkDestroyShaderModule real_vkDestroyShaderModule = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkCreateComputePipelines)(VkDevice, VkPipelineCache, uint32_t, const VkComputePipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*);
static PFN_vkCreateComputePipelines real_vkCreateComputePipelines = nullptr;

typedef void (VKAPI_PTR *PFN_vkDestroyPipeline)(VkDevice, VkPipeline, const VkAllocationCallbacks*);
static PFN_vkDestroyPipeline real_vkDestroyPipeline = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkCreatePipelineLayout)(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout*);
static PFN_vkCreatePipelineLayout real_vkCreatePipelineLayout = nullptr;

typedef void (VKAPI_PTR *PFN_vkDestroyPipelineLayout)(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*);
static PFN_vkDestroyPipelineLayout real_vkDestroyPipelineLayout = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkCreateDescriptorSetLayout)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout*);
static PFN_vkCreateDescriptorSetLayout real_vkCreateDescriptorSetLayout = nullptr;

typedef void (VKAPI_PTR *PFN_vkDestroyDescriptorSetLayout)(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*);
static PFN_vkDestroyDescriptorSetLayout real_vkDestroyDescriptorSetLayout = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkCreateDescriptorPool)(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool*);
static PFN_vkCreateDescriptorPool real_vkCreateDescriptorPool = nullptr;

typedef void (VKAPI_PTR *PFN_vkDestroyDescriptorPool)(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*);
static PFN_vkDestroyDescriptorPool real_vkDestroyDescriptorPool = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkAllocateDescriptorSets)(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
static PFN_vkAllocateDescriptorSets real_vkAllocateDescriptorSets = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkFreeDescriptorSets)(VkDevice, VkDescriptorPool, uint32_t, const VkDescriptorSet*);
static PFN_vkFreeDescriptorSets real_vkFreeDescriptorSets = nullptr;

typedef void (VKAPI_PTR *PFN_vkUpdateDescriptorSets)(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const VkCopyDescriptorSet*);
static PFN_vkUpdateDescriptorSets real_vkUpdateDescriptorSets = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkCreateCommandPool)(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool*);
static PFN_vkCreateCommandPool real_vkCreateCommandPool = nullptr;

typedef void (VKAPI_PTR *PFN_vkDestroyCommandPool)(VkDevice, VkCommandPool, const VkAllocationCallbacks*);
static PFN_vkDestroyCommandPool real_vkDestroyCommandPool = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkAllocateCommandBuffers)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
static PFN_vkAllocateCommandBuffers real_vkAllocateCommandBuffers = nullptr;

typedef void (VKAPI_PTR *PFN_vkFreeCommandBuffers)(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*);
static PFN_vkFreeCommandBuffers real_vkFreeCommandBuffers = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkBeginCommandBuffer)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
static PFN_vkBeginCommandBuffer real_vkBeginCommandBuffer = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkEndCommandBuffer)(VkCommandBuffer);
static PFN_vkEndCommandBuffer real_vkEndCommandBuffer = nullptr;

typedef VkResult (VKAPI_PTR *PFN_vkResetCommandBuffer)(VkCommandBuffer, VkCommandBufferResetFlags);
static PFN_vkResetCommandBuffer real_vkResetCommandBuffer = nullptr;

typedef void (VKAPI_PTR *PFN_vkCmdBindPipeline)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
static PFN_vkCmdBindPipeline real_vkCmdBindPipeline = nullptr;

typedef void (VKAPI_PTR *PFN_vkCmdBindDescriptorSets)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
static PFN_vkCmdBindDescriptorSets real_vkCmdBindDescriptorSets = nullptr;

typedef void (VKAPI_PTR *PFN_vkCmdDispatch)(VkCommandBuffer, uint32_t, uint32_t, uint32_t);
static PFN_vkCmdDispatch real_vkCmdDispatch = nullptr;

typedef void (VKAPI_PTR *PFN_vkCmdCopyBuffer)(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*);
static PFN_vkCmdCopyBuffer real_vkCmdCopyBuffer = nullptr;

typedef void (VKAPI_PTR *PFN_vkCmdPushConstants)(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*);
static PFN_vkCmdPushConstants real_vkCmdPushConstants = nullptr;


static void InitVulkanDynamic() {
    if (g_hVulkanInfo) return;
    g_hVulkanInfo = LoadLibraryA("vulkan-1.dll");
    if (!g_hVulkanInfo) {
        // Fallback or just stay null (functions will check)
        return;
    }

    real_vkDestroyBuffer = (PFN_vkDestroyBuffer)GetProcAddress(g_hVulkanInfo, "vkDestroyBuffer");
    real_vkCreateShaderModule = (PFN_vkCreateShaderModule)GetProcAddress(g_hVulkanInfo, "vkCreateShaderModule");
    real_vkDestroyShaderModule = (PFN_vkDestroyShaderModule)GetProcAddress(g_hVulkanInfo, "vkDestroyShaderModule");
    real_vkCreateComputePipelines = (PFN_vkCreateComputePipelines)GetProcAddress(g_hVulkanInfo, "vkCreateComputePipelines");
    real_vkDestroyPipeline = (PFN_vkDestroyPipeline)GetProcAddress(g_hVulkanInfo, "vkDestroyPipeline");
    real_vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)GetProcAddress(g_hVulkanInfo, "vkCreatePipelineLayout");
    real_vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)GetProcAddress(g_hVulkanInfo, "vkDestroyPipelineLayout");
    real_vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)GetProcAddress(g_hVulkanInfo, "vkCreateDescriptorSetLayout");
    real_vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)GetProcAddress(g_hVulkanInfo, "vkDestroyDescriptorSetLayout");
    real_vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)GetProcAddress(g_hVulkanInfo, "vkCreateDescriptorPool");
    real_vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)GetProcAddress(g_hVulkanInfo, "vkDestroyDescriptorPool");
    real_vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)GetProcAddress(g_hVulkanInfo, "vkAllocateDescriptorSets");
    real_vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets)GetProcAddress(g_hVulkanInfo, "vkFreeDescriptorSets");
    real_vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)GetProcAddress(g_hVulkanInfo, "vkUpdateDescriptorSets");
    real_vkCreateCommandPool = (PFN_vkCreateCommandPool)GetProcAddress(g_hVulkanInfo, "vkCreateCommandPool");
    real_vkDestroyCommandPool = (PFN_vkDestroyCommandPool)GetProcAddress(g_hVulkanInfo, "vkDestroyCommandPool");
    real_vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)GetProcAddress(g_hVulkanInfo, "vkAllocateCommandBuffers");
    real_vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)GetProcAddress(g_hVulkanInfo, "vkFreeCommandBuffers");
    real_vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)GetProcAddress(g_hVulkanInfo, "vkBeginCommandBuffer");
    real_vkEndCommandBuffer = (PFN_vkEndCommandBuffer)GetProcAddress(g_hVulkanInfo, "vkEndCommandBuffer");
    real_vkResetCommandBuffer = (PFN_vkResetCommandBuffer)GetProcAddress(g_hVulkanInfo, "vkResetCommandBuffer");
    real_vkCmdBindPipeline = (PFN_vkCmdBindPipeline)GetProcAddress(g_hVulkanInfo, "vkCmdBindPipeline");
    real_vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)GetProcAddress(g_hVulkanInfo, "vkCmdBindDescriptorSets");
    real_vkCmdDispatch = (PFN_vkCmdDispatch)GetProcAddress(g_hVulkanInfo, "vkCmdDispatch");
    real_vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)GetProcAddress(g_hVulkanInfo, "vkCmdCopyBuffer");
    real_vkCmdPushConstants = (PFN_vkCmdPushConstants)GetProcAddress(g_hVulkanInfo, "vkCmdPushConstants");
}


    // Buffer management
    void VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkDestroyBuffer) real_vkDestroyBuffer(device, buffer, pAllocator);
    }

    // Shader module management
    VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkCreateShaderModule) return real_vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
        if (pShaderModule) *pShaderModule = VK_NULL_HANDLE;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    void VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule,
        const VkAllocationCallbacks* pAllocator) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkDestroyShaderModule) real_vkDestroyShaderModule(device, shaderModule, pAllocator);
    }

    // Compute pipeline
    VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
        uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos,
        const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkCreateComputePipelines) return real_vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
        if (pPipelines) {
             for(uint32_t i=0; i<createInfoCount; ++i) pPipelines[i] = VK_NULL_HANDLE;
        }
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    void VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
        const VkAllocationCallbacks* pAllocator) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkDestroyPipeline) real_vkDestroyPipeline(device, pipeline, pAllocator);
    }

    // Pipeline layout
    VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkCreatePipelineLayout) return real_vkCreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
        if (pPipelineLayout) *pPipelineLayout = VK_NULL_HANDLE;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    void VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
        const VkAllocationCallbacks* pAllocator) {
        if (real_vkDestroyPipelineLayout) real_vkDestroyPipelineLayout(device, pipelineLayout, pAllocator);
    }

    // Descriptor set layout
    VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkCreateDescriptorSetLayout) return real_vkCreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
        const VkAllocationCallbacks* pAllocator) {
        if (real_vkDestroyDescriptorSetLayout) real_vkDestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
    }

    // Descriptor pool
    VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkCreateDescriptorPool) return real_vkCreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
        return VK_ERROR_INITIALIZATION_FAILED; 
    }
    void VKAPI_CALL vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
        const VkAllocationCallbacks* pAllocator) {
        if (real_vkDestroyDescriptorPool) real_vkDestroyDescriptorPool(device, descriptorPool, pAllocator);
    }
    VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo,
        VkDescriptorSet* pDescriptorSets) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkAllocateDescriptorSets) return real_vkAllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
        uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
        if (real_vkFreeDescriptorSets) return real_vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount,
        const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount,
        const VkCopyDescriptorSet* pDescriptorCopies) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkUpdateDescriptorSets) real_vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    }

    // Command pool
    VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkCreateCommandPool) return real_vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    void VKAPI_CALL vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
        const VkAllocationCallbacks* pAllocator) {
        if (real_vkDestroyCommandPool) real_vkDestroyCommandPool(device, commandPool, pAllocator);
    }


    // Command buffers
    VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo,
        VkCommandBuffer* pCommandBuffers) {
        if (!g_hVulkanInfo) InitVulkanDynamic();
        if (real_vkAllocateCommandBuffers) return real_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
        if (pCommandBuffers && pAllocateInfo) {
            for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
                pCommandBuffers[i] = VK_NULL_HANDLE;
            }
        }
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    void VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
        uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
        if (real_vkFreeCommandBuffers) real_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
    }
    VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
        const VkCommandBufferBeginInfo* pBeginInfo) {
        if (real_vkBeginCommandBuffer) return real_vkBeginCommandBuffer(commandBuffer, pBeginInfo);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
        if (real_vkEndCommandBuffer) return real_vkEndCommandBuffer(commandBuffer);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
        if (real_vkResetCommandBuffer) return real_vkResetCommandBuffer(commandBuffer, flags);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Recording commands
    void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
        VkPipeline pipeline) {
         if (real_vkCmdBindPipeline) real_vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    }
    void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
        VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
        const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount,
        const uint32_t* pDynamicOffsets) {
        if (real_vkCmdBindDescriptorSets) real_vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }
    void VKAPI_CALL vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX,
        uint32_t groupCountY, uint32_t groupCountZ) {
        if (real_vkCmdDispatch) real_vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
    }
    void VKAPI_CALL vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
        VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
        if (real_vkCmdCopyBuffer) real_vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    }
    void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
        VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
        if (real_vkCmdPushConstants) real_vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
    }
}
