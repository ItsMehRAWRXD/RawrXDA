#include "gpu/compute_pipeline.h"
#include "gpu/vulkan_context.h"
#include <fstream>
#include <vector>

namespace RawrXD::GPU {
    VkShaderModule ComputePipeline::loadShaderModule(const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) return VK_NULL_HANDLE;

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buffer.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(VulkanContext::getInstance().device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            return VK_NULL_HANDLE;
        return shaderModule;
    }

    VkPipeline ComputePipeline::createComputePipeline(VkShaderModule shader, VkPipelineLayout layout) {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = shader;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = layout;

        VkPipeline pipeline = VK_NULL_HANDLE;
        if (vkCreateComputePipelines(VulkanContext::getInstance().device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
            return VK_NULL_HANDLE;
        return pipeline;
    }

    VkPipelineLayout ComputePipeline::createPipelineLayout(VkDescriptorSetLayout setLayout) {
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = setLayout ? 1 : 0;
        layoutInfo.pSetLayouts = setLayout ? &setLayout : nullptr;

        VkPipelineLayout layout = VK_NULL_HANDLE;
        vkCreatePipelineLayout(VulkanContext::getInstance().device(), &layoutInfo, nullptr, &layout);
        return layout;
    }

    void ComputePipeline::destroyPipeline(VkPipeline pipeline) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(VulkanContext::getInstance().device(), pipeline, nullptr);
        }
    }

    void ComputePipeline::destroyShaderModule(VkShaderModule shader) {
        if (shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(VulkanContext::getInstance().device(), shader, nullptr);
        }
    }

    bool ComputePipeline::compileGLSLToSPIRV(const std::string& glslPath, const std::string& spvPath) {
        // Placeholder for GLSL to SPIRV compilation
        // In production, this would use glslang or similar
        return true;
    }
}
