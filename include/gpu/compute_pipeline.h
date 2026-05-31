#pragma once
#include "gpu/vulkan_context.h"
#include <string>

namespace RawrXD::GPU {

class ComputePipeline {
public:
    static VkShaderModule loadShaderModule(const std::string& path);
    static VkPipeline createComputePipeline(VkShaderModule shader, VkPipelineLayout layout);
    
    static VkPipelineLayout createPipelineLayout(VkDescriptorSetLayout setLayout);
    static void destroyPipeline(VkPipeline pipeline);
    static void destroyShaderModule(VkShaderModule shader);
    
    static bool compileGLSLToSPIRV(const std::string& glslPath, const std::string& spvPath);
};

} // namespace RawrXD::GPU
