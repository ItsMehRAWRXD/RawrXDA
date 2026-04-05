#pragma once
#ifndef RAWRXD_NO_VULKAN
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <expected>
#include <mutex>
#include <unordered_map>
#include <array>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {

enum class VulkanError {
    Success = 0,
    InstanceCreationFailed,
    DeviceSelectionFailed,
    PipelineCreationFailed,
    MemoryAllocationFailed,
    KernelExecutionFailed,
    SynchronizationFailed
};

struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mappedMemory = nullptr;
    size_t size = 0;
};

class VulkanCompute {
public:
    VulkanCompute();
    ~VulkanCompute();
    
    // Non-copyable
    VulkanCompute(const VulkanCompute&) = delete;
    VulkanCompute& operator=(const VulkanCompute&) = delete;
    
    // Real Vulkan initialization
    std::expected<void, VulkanError> initialize();
    void shutdown();
    
    // Real memory management
    std::expected<VulkanBuffer, VulkanError> createBuffer(
        size_t size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties
    );
    
    void destroyBuffer(VulkanBuffer& buffer);
    
    // Real kernel execution
    std::expected<void, VulkanError> executeMatrixMultiplication(
        const VulkanBuffer& a,
        const VulkanBuffer& b,
        VulkanBuffer& result,
        size_t dim,
        bool waitForCompletion = true
    );
    
    std::expected<void, VulkanError> executeSoftmax(
        VulkanBuffer& logits,
        float temperature,
        size_t vocabSize,
        bool waitForCompletion = true
    );
    
    std::expected<void, VulkanError> executeAttention(
        const VulkanBuffer& q,
        const VulkanBuffer& k,
        const VulkanBuffer& v,
        VulkanBuffer& output,
        size_t seqLen,
        size_t headDim
    );
    
    // Real synchronization
    std::expected<void, VulkanError> synchronize();
    
    // Status
    VkDevice getDevice() const { return m_device; }
    json getStatus() const;
    
private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_computeQueue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::unordered_map<std::string, VkDescriptorSetLayout> m_descriptorSetLayouts;

    static constexpr uint32_t kDispatchRingSize = 2;
    static constexpr uint32_t kTokenPipelineDepth = 3;  // Multi-token lookahead slots

    // Reused dispatch resources for hot kernels
    std::array<VkCommandBuffer, kDispatchRingSize> m_matmulCmdBufs{};
    std::array<VkFence, kDispatchRingSize> m_matmulFences{};
    std::array<VkDescriptorSet, kDispatchRingSize> m_matmulDescriptorSets{};
    uint32_t m_matmulSlotCursor = 0;
    
    // Per-slot persistent reuse tracking for matmul (dirty detection)
    std::array<VkBuffer, kDispatchRingSize> m_matmulLastBufferA{};
    std::array<VkBuffer, kDispatchRingSize> m_matmulLastBufferB{};
    std::array<VkBuffer, kDispatchRingSize> m_matmulLastBufferOut{};
    std::array<uint32_t, kDispatchRingSize> m_matmulLastDim{};
    std::array<bool, kDispatchRingSize> m_matmulCmdRecorded{};

    std::array<VkCommandBuffer, kDispatchRingSize> m_softmaxCmdBufs{};
    std::array<VkFence, kDispatchRingSize> m_softmaxFences{};
    std::array<VkDescriptorSet, kDispatchRingSize> m_softmaxDescriptorSets{};
    uint32_t m_softmaxSlotCursor = 0;
    
    // Per-slot persistent reuse tracking for softmax (dirty detection)
    std::array<VkBuffer, kDispatchRingSize> m_softmaxLastBufferLogits{};
    std::array<uint32_t, kDispatchRingSize> m_softmaxLastVocabSize{};
    std::array<bool, kDispatchRingSize> m_softmaxCmdRecorded{};
    
    // Token-level pipelining: multi-slot KV-cache buffers for concurrent token execution
    std::array<VkBuffer, kTokenPipelineDepth> m_kvCacheBuffers{};
    std::array<VkDeviceMemory, kTokenPipelineDepth> m_kvCacheMemory{};
    std::array<VkCommandBuffer, kTokenPipelineDepth> m_tokenCmdBufs{};
    std::array<VkFence, kTokenPipelineDepth> m_tokenFences{};
    std::array<volatile long, kTokenPipelineDepth> m_tokenCompletionFences{};
    uint32_t m_tokenSlotCursor = 0;
    
    // Real pipeline cache
    std::unordered_map<std::string, VkPipeline> m_pipelines;
    std::unordered_map<std::string, VkPipelineLayout> m_pipelineLayouts;
    mutable std::mutex m_mutex;
    
    // Real implementation methods
    std::expected<VkShaderModule, VulkanError> createShaderModule(
        const std::vector<uint32_t>& code
    );
    
    std::expected<void, VulkanError> createComputePipeline(
        const std::string& name,
        const std::vector<uint32_t>& shaderCode,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings
    );
    
    std::expected<VkCommandBuffer, VulkanError> beginCommandBuffer();
    std::expected<void, VulkanError> endCommandBuffer(VkCommandBuffer cmdBuffer);
    
    // Real shader compilation
    std::vector<uint32_t> compileGLSLToSPIRV(
        const std::string& glslCode,
        const std::string& entryPoint
    );
};

} // namespace RawrXD
#endif // RAWRXD_NO_VULKAN
