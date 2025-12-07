// Minimal Vulkan stub implementations to satisfy linker when Vulkan library is unavailable.
// These functions provide no‑op behavior and return success codes where applicable.
// They are placed in the global namespace with C linkage to match the Vulkan API.

#include <vulkan/vulkan.h>

extern "C" {
    // Buffer management
    void VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
        // No operation stub
    }

    // Shader module management
    VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
        if (pShaderModule) *pShaderModule = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule,
        const VkAllocationCallbacks* pAllocator) {
        // No operation stub
    }

    // Compute pipeline
    VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
        uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos,
        const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
        if (pPipelines) {
            for (uint32_t i = 0; i < createInfoCount; ++i) pPipelines[i] = VK_NULL_HANDLE;
        }
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
        const VkAllocationCallbacks* pAllocator) {
        // No operation stub
    }

    // Pipeline layout
    VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
        if (pPipelineLayout) *pPipelineLayout = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
        const VkAllocationCallbacks* pAllocator) {
        // No operation stub
    }

    // Descriptor set layout
    VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
        if (pSetLayout) *pSetLayout = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
        const VkAllocationCallbacks* pAllocator) {
        // No operation stub
    }

    // Descriptor pool
    VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
        if (pDescriptorPool) *pDescriptorPool = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
        const VkAllocationCallbacks* pAllocator) {
        // No operation stub
    }
    VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo,
        VkDescriptorSet* pDescriptorSets) {
        // Allocate dummy handles
        if (pDescriptorSets && pAllocateInfo) {
            for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; ++i) {
                pDescriptorSets[i] = VK_NULL_HANDLE;
            }
        }
        return VK_SUCCESS;
    }
    VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
        uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount,
        const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount,
        const VkCopyDescriptorSet* pDescriptorCopies) {
        // No operation stub
    }

    // Command pool
    VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
        if (pCommandPool) *pCommandPool = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
        const VkAllocationCallbacks* pAllocator) {
        // No operation stub
    }

    // Command buffers
    VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo,
        VkCommandBuffer* pCommandBuffers) {
        if (pCommandBuffers && pAllocateInfo) {
            for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
                pCommandBuffers[i] = VK_NULL_HANDLE;
            }
        }
        return VK_SUCCESS;
    }
    void VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
        uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
        // No operation stub
    }
    VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
        const VkCommandBufferBeginInfo* pBeginInfo) {
        return VK_SUCCESS;
    }
    VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
        return VK_SUCCESS;
    }
    VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
        return VK_SUCCESS;
    }

    // Recording commands
    void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
        VkPipeline pipeline) {
        // No operation stub
    }
    void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
        VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
        const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount,
        const uint32_t* pDynamicOffsets) {
        // No operation stub
    }
    void VKAPI_CALL vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX,
        uint32_t groupCountY, uint32_t groupCountZ) {
        // No operation stub
    }
    void VKAPI_CALL vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
        VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
        // No operation stub
    }
    void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
        VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
        // No operation stub
    }
}
