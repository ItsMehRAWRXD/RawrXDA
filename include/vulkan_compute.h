#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <queue>
#include <vulkan/vulkan.h>

// GPU compute optional - CPU inference always works
// Vulkan is enabled if system supports it, otherwise CPU fallback

struct VulkanDeviceInfo {
    std::string device_name;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memory_props{};
    uint32_t vendor_id;
    uint32_t device_id;
    bool supports_compute;
    uint32_t compute_queue_family;
};

struct ComputeShader {
    std::string name;
    std::vector<uint32_t> spirv_code;
    VkShaderModule module = nullptr;
    VkPipelineLayout layout = nullptr;
    VkPipeline pipeline = nullptr;
};

struct VulkanTensor {
    std::string name;
    size_t size_bytes{0};
    std::vector<float> host_data;  // Scalar data stored in CPU memory
    VkBuffer device_buffer = nullptr;
    VkDeviceMemory device_memory = nullptr;
};

// Async command buffer pool for high-performance batching
struct CommandBufferPool {
    VkCommandBuffer buffer = nullptr;
    VkFence fence = nullptr;
    bool is_available = true;
};

class VulkanCompute {
public:
    VulkanCompute();
    ~VulkanCompute();

    bool Initialize();
    bool LoadShader(const std::string& name, const std::string& spirv_path);
    bool CreateComputePipeline(const std::string& shader_name);
    VulkanTensor TransferGGUFTensor(const std::string& tensor_name,
                                    const void* data_ptr,
                                    size_t size_bytes,
                                    uint32_t usage = 0);
    void ReleaseTensors();
    bool EnsureMatMulPipeline(const std::string& spirv_path);
    bool DispatchMatMul(uint32_t input_a_idx,
                        uint32_t input_b_idx,
                        uint32_t output_idx,
                        uint32_t M,
                        uint32_t K,
                        uint32_t N);
    
    // High-performance async variant using command buffer pooling
    bool DispatchMatMulAsync(uint32_t input_a_idx,
                             uint32_t input_b_idx,
                             uint32_t output_idx,
                             uint32_t M,
                             uint32_t K,
                             uint32_t N);
    
    VulkanDeviceInfo GetDeviceInfo() const { return device_info_; }
    bool IsAMDDevice() const { return device_info_.vendor_id == 0x1002; }
    bool IsNvidiaDevice() const { return device_info_.vendor_id == 0x10DE; }
    
    bool AllocateBuffer(size_t size, uint32_t& buffer_idx, size_t& memory_size);
    bool AllocateBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory);
    bool CopyBufferToHost(uint32_t buffer_idx, void* host_data, size_t size);
    bool CopyBufferToHost(VkBuffer device_buffer, void* host_data, size_t size);
    bool CopyHostToBuffer(void* host_data, uint32_t buffer_idx, size_t size);
    bool CopyHostToBuffer(void* host_data, VkBuffer device_buffer, size_t size);
    
    // KV Cache management for autoregressive inference
    bool AllocateKVCache(uint32_t num_layers, uint32_t max_seq_len, uint32_t head_dim);
    bool AppendToKVCache(uint32_t layer_idx, const float* k_new, const float* v_new, uint32_t token_pos);
    bool GetKVCacheSlice(uint32_t layer_idx, uint32_t start_pos, uint32_t end_pos, float* k_out, float* v_out);
    void ClearKVCache();
    bool IsKVCacheAllocated() const { return kv_cache_allocated_; }
    
    // Command buffer & synchronization utilities
    bool ExecuteSingleTimeCommands(std::function<void(VkCommandBuffer)> record_func);
    bool ExecuteCommandBuffer(VkCommandBuffer cmd_buffer);
    
    // High-performance async execution
    VkCommandBuffer AcquireAsyncCommandBuffer();
    bool SubmitAsyncCommandBuffer(VkCommandBuffer cmd_buffer);
    bool FlushAsyncCommands();  // Wait for all pending async operations
    bool CheckAsyncCompletion(VkCommandBuffer cmd_buffer);  // Non-blocking check
    
    // Descriptor set management
    bool CreateDescriptorSetLayout(uint32_t binding_count, VkDescriptorSetLayout& layout);
    bool AllocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet& descriptor_set);
    bool UpdateDescriptorSet(VkDescriptorSet descriptor_set, uint32_t binding, VkBuffer buffer, size_t buffer_size);
    
    // Scalar CPU implementations (no GPU)
    bool ExecuteMatMul(const float* input_a, const float* input_b, 
                       float* output, uint32_t m, uint32_t k, uint32_t n);
    bool ExecuteAttention(const float* queries, const float* keys, const float* values,
                         float* output, uint32_t seq_len, uint32_t head_dim);
    bool ExecuteRoPE(float* embeddings, uint32_t dim, uint32_t seq_pos, uint32_t rotation_dim);
    bool ExecuteRMSNorm(float* data, uint32_t size, float epsilon = 1e-5f);
    bool ExecuteSiLU(float* data, uint32_t size);
    bool ExecuteSoftmax(float* data, uint32_t size);
    bool ExecuteDequantize(const uint8_t* quantized, float* output,
                           uint32_t elements, const std::string& quant_type);
    
    void Cleanup();

private:
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateCommandPool();
    bool LoadSPIRVCode(const std::string& path, std::vector<uint32_t>& code);
    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

    VkInstance instance_ = nullptr;
    VkPhysicalDevice physical_device_ = nullptr;
    VkDevice device_ = nullptr;
    VkQueue compute_queue_ = nullptr;
    VkCommandPool command_pool_ = nullptr;
    VkDescriptorPool descriptor_pool_ = nullptr;

    // Async command buffer pooling for high-performance batching
    std::vector<CommandBufferPool> command_buffer_pool_;
    std::queue<size_t> available_buffer_indices_;

    // Permanent descriptor system for MatMul (avoid per-dispatch allocation overhead)
    VkDescriptorSetLayout matmul_descriptor_set_layout_ = nullptr;
    VkDescriptorPool matmul_descriptor_pool_ = nullptr;

    // KV Cache for autoregressive inference
    std::vector<std::pair<VkBuffer, VkDeviceMemory>> kv_cache_buffers_; // 2 per layer (K, V)
    uint32_t kv_cache_num_layers_ = 0;
    uint32_t kv_cache_max_seq_len_ = 0;
    uint32_t kv_cache_head_dim_ = 0;
    bool kv_cache_allocated_ = false;
    
    // Persistent staging buffer for optimized host-to-device transfers
    VkBuffer staging_buffer_ = nullptr;
    VkDeviceMemory staging_memory_ = nullptr;
    size_t staging_buffer_size_ = 0;

    VulkanDeviceInfo device_info_;
    std::unordered_map<std::string, ComputeShader> shaders_;
    std::vector<VulkanTensor> uploaded_tensors_;
    std::vector<std::pair<VkBuffer, VkDeviceMemory>> allocated_buffers_;
    std::unordered_map<std::string, VkDescriptorSetLayout> descriptor_layouts_;
    
    // Helper methods for command buffer pool management
    void InitializeCommandBufferPool(uint32_t pool_size = 4);
    void CleanupCommandBufferPool();
    
    // Helper for offset-based buffer copies (KV cache updates)
    bool CopyHostToBufferOffset(const void* host_data, VkBuffer device_buffer, size_t offset, size_t size);
    bool CopyBufferToHostOffset(VkBuffer device_buffer, size_t offset, void* host_data, size_t size);
    
    // Helper for creating staging buffers (reduces code duplication)
    bool CreateStagingBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory);
};
