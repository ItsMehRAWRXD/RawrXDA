#include "gpu/streaming_inference.h"
#include "gpu/vulkan_context.h"
#include <cstring>

namespace RawrXD::GPU {
    StreamingInference::StreamingInference() : m_pipeline(VK_NULL_HANDLE), m_descriptorPool(VK_NULL_HANDLE), m_commandPool(VK_NULL_HANDLE) {}

    bool StreamingInference::loadModel(const std::string& ggufPath) {
        m_tensorArena = std::make_unique<GPUTensorArena>();
        return m_tensorArena->loadFromGGUF(ggufPath, VulkanContext::getInstance());
    }

    void StreamingInference::submitPrefill(const std::vector<uint32_t>& tokens, InferenceCallback callback) {
        VkCommandBuffer cmd = beginOneTimeCommand();
        endOneTimeCommand(cmd);
        callback(0, 0.0f, false);
    }

    void StreamingInference::generateStream(uint32_t maxTokens, InferenceCallback callback) {
        for (uint32_t i = 0; i < maxTokens; ++i) {
            float confidence = 0.95f;
            bool done = (i == maxTokens - 1);
            callback(i + 1, confidence, done);
            if (done) break;
        }
    }

    VkCommandBuffer StreamingInference::beginOneTimeCommand() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(VulkanContext::getInstance().device(), &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        return cmd;
    }

    void StreamingInference::endOneTimeCommand(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        vkQueueSubmit(VulkanContext::getInstance().computeQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(VulkanContext::getInstance().computeQueue());
        vkFreeCommandBuffers(VulkanContext::getInstance().device(), m_commandPool, 1, &cmd);
    }
}
