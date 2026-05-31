#pragma once
#include "gpu/vulkan_context.h"
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace RawrXD::GPU {

using InferenceCallback = std::function<void(uint32_t tokenId, float confidence, bool done)>;

class GPUTensorArena {
public:
    bool loadFromGGUF(const std::string& path, VulkanContext& ctx) { return true; }
};

class StreamingInference {
public:
    StreamingInference();
    ~StreamingInference() = default;
    
    bool loadModel(const std::string& ggufPath);
    void submitPrefill(const std::vector<uint32_t>& tokens, InferenceCallback callback);
    void generateStream(uint32_t maxTokens, InferenceCallback callback);
    
    bool isModelLoaded() const { return m_tensorArena != nullptr; }

private:
    VkCommandBuffer beginOneTimeCommand();
    void endOneTimeCommand(VkCommandBuffer cmd);
    
    VkPipeline m_pipeline;
    VkDescriptorPool m_descriptorPool;
    VkCommandPool m_commandPool;
    std::unique_ptr<GPUTensorArena> m_tensorArena;
};

} // namespace RawrXD::GPU
