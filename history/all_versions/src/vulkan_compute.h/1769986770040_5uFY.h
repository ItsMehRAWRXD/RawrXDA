#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <expected>
#include <mutex>
#include <atomic>
#include <chrono>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
// Include VMA
#include <vma/vk_mem_alloc.h>
#include <shaderc/shaderc.hpp>

namespace RawrXD {

enum class VulkanError {
    Success = 0,
    InstanceCreationFailed,
    DeviceSelectionFailed,
    PipelineCreationFailed,
    MemoryAllocationFailed,
    KernelExecutionFailed,
    SynchronizationFailed,
    ShaderCompilationFailed,
    DescriptorSetFailed,
    CommandBufferFailed
};

struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mappedMemory = nullptr;
    size_t size = 0;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    std::atomic<bool> isValid{false};
    std::chrono::steady_clock::time_point lastUsed;
};

struct VulkanPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    std::string name;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::atomic<bool> isValid{false};
};

class VulkanCompute {
public:
    VulkanCompute();
    ~VulkanCompute();
    
    // Non-copyable
    VulkanCompute(const VulkanCompute&) = delete;
    VulkanCompute& operator=(const VulkanCompute&) = delete;
    
    // Real Vulkan initialization with performance optimizations
    std::expected<void, VulkanError> initialize();
    void shutdown();
    
    // Real memory management with zero-copy
    std::expected<VulkanBuffer, VulkanError> createBuffer(
        size_t size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        bool zeroCopy = true
    );
    
    void destroyBuffer(VulkanBuffer& buffer);
    
    // Real kernel execution with wavefront optimization
    std::expected<void, VulkanError> executeMatrixMultiplication(
        const VulkanBuffer& a,
        const VulkanBuffer& b,
        VulkanBuffer& result,
        size_t dim,
        bool useWavefront = true
    );
    
    std::expected<void, VulkanError> executeSoftmax(
        VulkanBuffer& logits,
        float temperature,
        size_t vocabSize,
        bool useWavefront = true
    );
    
    // Real batched execution for performance
    std::expected<void, VulkanError> executeBatched(
        const std::vector<VulkanBuffer>& inputs,
        const std::vector<VulkanBuffer>& outputs,
        const std::string& kernelName,
        const std::vector<uint32_t>& pushConstants
    );
    
    // Real synchronization with performance counters
    std::expected<void, VulkanError> synchronize(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)
    );

    // Helpers exposed for token generator
    VkDevice getDevice() const { return m_device; }
    
    // Performance metrics
    double getGFlops() const;
    double getMemoryBandwidth() const;
    size_t getPeakMemoryUsage() const;
    nlohmann::json getPerformanceMetrics() const;

    // Public friends or exposed allocators for integration
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    
private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_computeQueue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_deviceProperties{};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
    
    // Real pipeline cache for performance
    std::unordered_map<std::string, VulkanPipeline> m_pipelines;
    std::unordered_map<std::string, VkDescriptorSetLayout> m_descriptorSetLayouts;
    mutable std::mutex m_pipelineMutex;
    
    std::unordered_map<void*, VulkanBuffer> m_bufferCache;
    mutable std::mutex m_bufferMutex;
    
    // Performance counters
    std::atomic<uint64_t> m_totalOperations{0};
    std::atomic<uint64_t> m_totalMemoryTransferred{0};
    std::atomic<uint64_t> m_totalExecutionTime{0}; // nanoseconds
    std::atomic<size_t> m_peakMemoryUsage{0};
    
    // Real implementation methods
    std::expected<VulkanPipeline, VulkanError> createComputePipeline(
        const std::string& name,
        const std::vector<uint32_t>& shaderCode,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings,
        bool optimizeForWavefront = true
    );
    
    std::expected<std::vector<uint32_t>, VulkanError> compileGLSLToSPIRV(
        const std::string& glslCode,
        const std::string& entryPoint,
        bool optimize = true
    );
    
    std::expected<VkCommandBuffer, VulkanError> beginCommandBuffer();
    std::expected<void, VulkanError> endCommandBuffer(VkCommandBuffer cmdBuffer);
    
    // Real descriptor management
    std::expected<VkDescriptorSet, VulkanError> allocateDescriptorSet(
        VkDescriptorSetLayout layout
    );
    
    // Real memory management
    std::expected<uint32_t, VulkanError> findMemoryType(
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties
    );
    
    void trackMemoryUsage(size_t bytes);
    void updatePeakMemoryUsage(size_t bytes);
    
    // Performance monitoring
    void recordPerformanceMetrics(
        const std::string& operation,
        std::chrono::nanoseconds duration,
        size_t memoryTransferred
    );
};

} // namespace RawrXD
