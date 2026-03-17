// vulkan_compute.h — Production-Ready Vulkan Compute Engine for RawrXD-Shell
// GPU-accelerated inference: MatMul, Attention, RoPE, RMSNorm, SiLU, Softmax
// Supports: SPIR-V shader loading, async command buffer pools, KV cache,
//           GGUF tensor upload, speculative decoding, Flash Attention v2 dispatch,
//           fused MLP kernels, and hotpatchable pipeline replacement.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <functional>
#include <atomic>
#include <mutex>

// =============================================================================
// Vulkan Type Stubs (when vulkan.h is not on include path)
// =============================================================================
#ifndef VK_DEFINE_HANDLE
typedef struct VkInstance_T*        VkInstance;
typedef struct VkPhysicalDevice_T*  VkPhysicalDevice;
typedef struct VkDevice_T*          VkDevice;
typedef struct VkCommandPool_T*     VkCommandPool;
typedef struct VkQueue_T*           VkQueue;
typedef struct VkCommandBuffer_T*   VkCommandBuffer;
typedef struct VkFence_T*           VkFence;
typedef struct VkBuffer_T*          VkBuffer;
typedef struct VkDeviceMemory_T*    VkDeviceMemory;
typedef struct VkShaderModule_T*    VkShaderModule;
typedef struct VkPipeline_T*        VkPipeline;
typedef struct VkPipelineLayout_T*  VkPipelineLayout;
typedef struct VkDescriptorSetLayout_T* VkDescriptorSetLayout;
typedef struct VkDescriptorPool_T*  VkDescriptorPool;
typedef struct VkDescriptorSet_T*   VkDescriptorSet;
typedef uint32_t VkMemoryPropertyFlags;

// Minimal struct stubs for compilation without Vulkan SDK
struct VkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t deviceType;
    char     deviceName[256];
    uint8_t  pipelineCacheUUID[16];
    // Truncated — full struct in vulkan.h
};

struct VkPhysicalDeviceMemoryProperties {
    uint32_t memoryTypeCount;
    struct {
        uint32_t propertyFlags;
        uint32_t heapIndex;
    } memoryTypes[32];
    uint32_t memoryHeapCount;
    struct {
        uint64_t size;
        uint32_t flags;
    } memoryHeaps[16];
};
#endif

// =============================================================================
// VulkanTensor — GPU-resident tensor descriptor
// =============================================================================
struct VulkanTensor {
    std::string         name;
    VkBuffer            device_buffer  = nullptr;
    VkDeviceMemory      device_memory  = nullptr;
    size_t              size_bytes     = 0;
    std::vector<float>  host_data;
};

// =============================================================================
// VulkanDeviceInfo — Cached physical device properties
// =============================================================================
struct VulkanDeviceInfo {
    VkPhysicalDeviceProperties       properties{};
    VkPhysicalDeviceMemoryProperties memory_props{};
    std::string                      device_name;
    uint32_t                         vendor_id            = 0;
    uint32_t                         device_id            = 0;
    int                              compute_queue_family = -1;
    bool                             supports_compute     = false;
};

// =============================================================================
// ComputeShader — Loaded SPIR-V module + compiled pipeline
// =============================================================================
struct ComputeShader {
    std::string              name;
    VkShaderModule           module   = nullptr;
    VkPipeline               pipeline = nullptr;
    VkPipelineLayout         layout   = nullptr;
    std::vector<uint32_t>    spirv_code;
};

// =============================================================================
// AsyncCommandBuffer — Pooled command buffer with fence tracking
// =============================================================================
struct AsyncCommandBuffer {
    VkCommandBuffer buffer       = nullptr;
    VkFence         fence        = nullptr;
    bool            is_available = true;
};

// =============================================================================
// VulkanKernelStats — Runtime performance counters (atomic, lock-free)
// =============================================================================
struct VulkanKernelStats {
    std::atomic<uint64_t> dispatch_count{0};
    std::atomic<uint64_t> matmul_count{0};
    std::atomic<uint64_t> attention_count{0};
    std::atomic<uint64_t> buffer_alloc_bytes{0};
    std::atomic<uint64_t> shader_load_count{0};
    std::atomic<uint64_t> pipeline_create_count{0};
    std::atomic<uint64_t> total_gpu_ns{0};       // Nanoseconds spent on GPU
    std::atomic<uint64_t> kv_cache_hits{0};
    std::atomic<uint64_t> kv_cache_misses{0};
    std::atomic<uint64_t> staging_alloc_count{0};
    std::atomic<uint64_t> error_count{0};
};

// =============================================================================
// VulkanCompute — Full GPU compute engine
// =============================================================================
class VulkanCompute {
public:
    VulkanCompute();
    ~VulkanCompute();

    // ---- Lifecycle ----
    bool Initialize();
    void Cleanup();

    // ---- Device Info ----
    bool IsAMDDevice() const { return device_info_.vendor_id == 0x1002; }
    bool IsNvidiaDevice() const { return device_info_.vendor_id == 0x10DE; }
    bool IsIntelDevice() const { return device_info_.vendor_id == 0x8086; }
    const VulkanDeviceInfo& GetDeviceInfo() const { return device_info_; }
    const VulkanKernelStats& GetStats() const { return stats_; }

    // ---- Instance/Device Setup ----
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateCommandPool();

    // ---- Shader Management ----
    bool LoadShader(const std::string& name, const std::string& spirv_path);
    bool LoadShaderFromMemory(const std::string& name, const uint32_t* spirv_code, size_t code_size);
    bool CreateComputePipeline(const std::string& shader_name);
    bool ReplacePipeline(const std::string& shader_name, const std::string& new_spirv_path);

    // ---- Buffer Management ----
    bool AllocateBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory);
    bool AllocateBuffer(size_t size, uint32_t& buffer_idx, size_t& memory_size);
    bool CreateStagingBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory);

    // ---- Data Transfer ----
    bool CopyBufferToHost(VkBuffer device_buffer, void* host_data, size_t size);
    bool CopyBufferToHost(uint32_t buffer_idx, void* host_data, size_t size);
    bool CopyHostToBuffer(void* host_data, VkBuffer device_buffer, size_t size);
    bool CopyHostToBuffer(void* host_data, uint32_t buffer_idx, size_t size);
    bool CopyHostToBufferOffset(const void* host_data, VkBuffer device_buffer,
                                size_t offset, size_t size);
    bool CopyBufferToHostOffset(VkBuffer device_buffer, size_t offset,
                                void* host_data, size_t size);

    // ---- GGUF Tensor Upload ----
    VulkanTensor TransferGGUFTensor(const std::string& tensor_name,
                                    const void* data_ptr, size_t size_bytes,
                                    uint32_t usage = 0);
    void ReleaseTensors();

    // ---- MatMul Pipeline (Permanent Descriptor System) ----
    bool EnsureMatMulPipeline(const std::string& spirv_path);
    bool DispatchMatMul(uint32_t input_a_idx, uint32_t input_b_idx,
                        uint32_t output_idx, uint32_t M, uint32_t K, uint32_t N);
    bool DispatchMatMulAsync(uint32_t input_a_idx, uint32_t input_b_idx,
                             uint32_t output_idx, uint32_t M, uint32_t K, uint32_t N);

    // ---- Fused MLP Kernel (Linear → GeLU → Linear) ----
    bool EnsureFusedMLPPipeline(const std::string& spirv_path);
    bool DispatchFusedMLP(uint32_t input_idx, uint32_t weight1_idx,
                          uint32_t weight2_idx, uint32_t output_idx,
                          uint32_t batch_size, uint32_t hidden_dim,
                          uint32_t intermediate_dim);

    // ---- Flash Attention v2 Dispatch ----
    bool EnsureFlashAttentionPipeline(const std::string& spirv_path);
    bool DispatchFlashAttentionV2(uint32_t q_idx, uint32_t k_idx, uint32_t v_idx,
                                  uint32_t output_idx, uint32_t seq_len,
                                  uint32_t head_dim, uint32_t num_heads,
                                  float scale = 0.0f);

    // ---- Speculative Decoding Support ----
    struct SpeculativeResult {
        std::vector<uint32_t> accepted_tokens;
        uint32_t              draft_count;
        uint32_t              accepted_count;
        float                 acceptance_rate;
    };
    bool DispatchSpeculativeVerify(uint32_t draft_logits_idx,
                                   uint32_t target_logits_idx,
                                   uint32_t seq_len, uint32_t vocab_size,
                                   SpeculativeResult* out_result);

    // ---- CPU Fallback Inference Ops ----
    bool ExecuteMatMul(const float* input_a, const float* input_b,
                       float* output, uint32_t m, uint32_t k, uint32_t n);
    bool ExecuteAttention(const float* queries, const float* keys,
                          const float* values, float* output,
                          uint32_t seq_len, uint32_t head_dim);
    bool ExecuteRoPE(float* embeddings, uint32_t dim,
                     uint32_t seq_pos, uint32_t rotation_dim);
    bool ExecuteRMSNorm(float* data, uint32_t size, float epsilon);
    bool ExecuteSiLU(float* data, uint32_t size);
    bool ExecuteSoftmax(float* data, uint32_t size);
    bool ExecuteDequantize(const uint8_t* quantized, float* output,
                           uint32_t elements, const std::string& quant_type);

    // ---- KV Cache Infrastructure ----
    bool AllocateKVCache(uint32_t num_layers, uint32_t max_seq_len, uint32_t head_dim);
    bool AppendToKVCache(uint32_t layer_idx, const float* k_new,
                         const float* v_new, uint32_t token_pos);
    bool GetKVCacheSlice(uint32_t layer_idx, uint32_t start_pos,
                         uint32_t end_pos, float* k_out, float* v_out);
    void ClearKVCache();

    // ---- Async Command Buffer Pool ----
    void InitializeCommandBufferPool(uint32_t pool_size);
    void CleanupCommandBufferPool();
    VkCommandBuffer AcquireAsyncCommandBuffer();
    bool SubmitAsyncCommandBuffer(VkCommandBuffer cmd_buffer);
    bool FlushAsyncCommands();
    bool CheckAsyncCompletion(VkCommandBuffer cmd_buffer);

    // ---- Command Execution ----
    bool ExecuteSingleTimeCommands(std::function<void(VkCommandBuffer)> record_func);
    bool ExecuteCommandBuffer(VkCommandBuffer cmd_buffer);

    // ---- Descriptor Set Management (Deprecated — use EnsureMatMulPipeline) ----
    bool CreateDescriptorSetLayout(uint32_t binding_count, VkDescriptorSetLayout& layout);
    bool AllocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet& descriptor_set);
    bool UpdateDescriptorSet(VkDescriptorSet descriptor_set, uint32_t binding,
                             VkBuffer buffer, size_t buffer_size);

    // ---- Hotpatchable Pipeline Replacement ----
    // Atomically replace a compute pipeline's SPIR-V shader without destroying
    // in-flight work. Used by the UnifiedHotpatchManager to live-swap GPU kernels.
    bool HotswapShader(const std::string& pipeline_name,
                       const uint32_t* new_spirv, size_t spirv_size);

    // ---- C-callable exports for MASM bridge ----
    // extern "C" linkage — defined in vulkan_compute.cpp
    // VulkanKernel_Init, VulkanKernel_Dispatch, VulkanKernel_Cleanup, etc.

private:
    // ---- SPIR-V Loader ----
    bool LoadSPIRVCode(const std::string& path, std::vector<uint32_t>& code);

    // ---- Memory Utilities ----
    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

    // ---- Vulkan Handles ----
    VkInstance          instance_       = nullptr;
    VkPhysicalDevice    physical_device_= nullptr;
    VkDevice            device_         = nullptr;
    VkCommandPool       command_pool_   = nullptr;
    VkQueue             compute_queue_  = nullptr;

    // ---- Device Info ----
    VulkanDeviceInfo    device_info_;

    // ---- Shader Registry ----
    std::map<std::string, ComputeShader>        shaders_;
    std::mutex                                  shader_mutex_;  // Protects hot-swap

    // ---- Buffer Tracking ----
    std::vector<std::pair<VkBuffer, VkDeviceMemory>>  allocated_buffers_;
    std::vector<VulkanTensor>                          uploaded_tensors_;

    // ---- Persistent Staging Buffer ----
    VkBuffer            staging_buffer_  = nullptr;
    VkDeviceMemory      staging_memory_  = nullptr;

    // ---- MatMul Permanent Descriptor System ----
    VkDescriptorSetLayout  matmul_descriptor_set_layout_ = nullptr;
    VkDescriptorPool       matmul_descriptor_pool_       = nullptr;

    // ---- Fused MLP Descriptor System ----
    VkDescriptorSetLayout  fused_mlp_descriptor_layout_  = nullptr;
    VkDescriptorPool       fused_mlp_descriptor_pool_    = nullptr;

    // ---- Flash Attention v2 Descriptor System ----
    VkDescriptorSetLayout  flash_attn_descriptor_layout_ = nullptr;
    VkDescriptorPool       flash_attn_descriptor_pool_   = nullptr;

    // ---- General Descriptor Pool ----
    VkDescriptorPool       descriptor_pool_              = nullptr;

    // ---- Async Command Buffer Pool ----
    std::vector<AsyncCommandBuffer>  command_buffer_pool_;
    std::queue<size_t>               available_buffer_indices_;

    // ---- KV Cache ----
    std::vector<std::pair<VkBuffer, VkDeviceMemory>>  kv_cache_buffers_;
    uint32_t kv_cache_num_layers_  = 0;
    uint32_t kv_cache_max_seq_len_ = 0;
    uint32_t kv_cache_head_dim_    = 0;
    bool     kv_cache_allocated_   = false;

    // ---- Performance Counters ----
    VulkanKernelStats   stats_;
};

// =============================================================================
// C-callable exports for MASM/ASM bridge (defined in vulkan_compute.cpp)
// These allow the MASM Vulkan kernel to call into the C++ compute engine
// without C++ name mangling.
// =============================================================================
extern "C" {
    int  VulkanKernel_Init(void);
    int  VulkanKernel_LoadShader(const char* name, const char* spirv_path);
    int  VulkanKernel_CreatePipeline(const char* shader_name);
    int  VulkanKernel_AllocBuffer(uint64_t size, uint32_t* out_idx);
    int  VulkanKernel_CopyToDevice(uint32_t buf_idx, const void* data, uint64_t size);
    int  VulkanKernel_CopyToHost(uint32_t buf_idx, void* data, uint64_t size);
    int  VulkanKernel_DispatchMatMul(uint32_t a, uint32_t b, uint32_t out,
                                     uint32_t M, uint32_t K, uint32_t N);
    int  VulkanKernel_DispatchFlashAttn(uint32_t q, uint32_t k, uint32_t v,
                                        uint32_t out, uint32_t seq_len,
                                        uint32_t head_dim, uint32_t num_heads);
    int  VulkanKernel_HotswapShader(const char* name,
                                     const uint32_t* spirv, uint64_t size);
    void VulkanKernel_GetStats(uint64_t* dispatches, uint64_t* matmuls,
                               uint64_t* attentions, uint64_t* errors);
    void VulkanKernel_Cleanup(void);
}
