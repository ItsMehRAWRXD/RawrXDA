#include "../vulkan_compute.h"
#ifdef RAWR_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#endif
#include <cstring>
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <new>
#include <unordered_map>

// Private member variables for VulkanCompute
struct KVCacheLayer {
    std::vector<float> k_cache;
    std::vector<float> v_cache;
    uint32_t current_seq_len = 0;
};

namespace {
    // Static storage for CPU fallback
    std::vector<VulkanTensor> g_tensors;
    std::vector<KVCacheLayer> g_kv_cache;
    bool g_kv_cache_allocated = false;
    size_t g_staging_buffer_size = 0;
    std::unordered_map<std::string, VkDescriptorSetLayout> g_descriptor_layouts;

    // CPU-backed handle registry for VkBuffer/VkDeviceMemory shim paths.
    std::mutex g_cpu_buffer_mutex;
    std::unordered_map<VkBuffer, std::vector<uint8_t>> g_cpu_buffers;
    std::unordered_map<VkDeviceMemory, size_t> g_cpu_memory_blocks;
    std::unordered_map<VkCommandBuffer, bool> g_async_command_submitted;
    std::unordered_map<VkCommandBuffer, bool> g_async_command_completed;
    std::unordered_map<VkCommandBuffer, size_t> g_command_pool_index;
    std::unordered_map<VkDescriptorSetLayout, uint32_t> g_descriptor_layout_bindings;
    std::unordered_map<VkDescriptorSetLayout, uint32_t> g_descriptor_layout_set_counts;
    std::unordered_map<VkDescriptorSet, VkDescriptorSetLayout> g_descriptor_set_layout_owner;
    std::unordered_map<VkDescriptorSet, std::unordered_map<uint32_t, std::pair<VkBuffer, size_t>>> g_descriptor_bindings;
    std::unordered_map<VkPipelineLayout, bool> g_pipeline_layout_handles;
    std::unordered_map<VkPipeline, bool> g_pipeline_handles;
    std::unordered_map<VkInstance, bool> g_instance_handles;
    std::unordered_map<VkPhysicalDevice, bool> g_physical_device_handles;
    std::unordered_map<VkDevice, bool> g_device_handles;
    std::unordered_map<VkQueue, bool> g_queue_handles;
    std::unordered_map<VkCommandPool, bool> g_command_pool_handles;

    constexpr uint32_t kSpirvMagic = 0x07230203u;

    void release_command_buffer_handle_locked(VkCommandBuffer handle) {
        if (!handle) {
            return;
        }
        auto submitted_it = g_async_command_submitted.find(handle);
        if (submitted_it == g_async_command_submitted.end()) {
            return;
        }
        g_async_command_submitted.erase(submitted_it);
        g_async_command_completed.erase(handle);
        g_command_pool_index.erase(handle);
        delete reinterpret_cast<uint8_t*>(handle);
    }

    void recycle_or_release_command_handle_locked(VkCommandBuffer handle,
                                                  std::vector<AsyncCommandBuffer>* pool,
                                                  std::queue<size_t>* available_indices) {
        if (!handle) {
            return;
        }
        auto idx_it = g_command_pool_index.find(handle);
        if (idx_it == g_command_pool_index.end() || idx_it->second == static_cast<size_t>(-1)) {
            release_command_buffer_handle_locked(handle);
            return;
        }
        const size_t idx = idx_it->second;
        if (!pool || !available_indices || idx >= pool->size()) {
            release_command_buffer_handle_locked(handle);
            return;
        }
        auto& entry = (*pool)[idx];
        if (!entry.is_available) {
            entry.is_available = true;
            available_indices->push(idx);
        }
    }

    void release_cpu_buffer_registry() {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        for (const auto& it : g_cpu_buffers) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_cpu_buffers.clear();
        for (const auto& it : g_cpu_memory_blocks) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_cpu_memory_blocks.clear();
        for (const auto& it : g_async_command_submitted) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_async_command_submitted.clear();
        g_async_command_completed.clear();
        g_command_pool_index.clear();
        for (const auto& it : g_descriptor_layout_bindings) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_descriptor_layout_bindings.clear();
        g_descriptor_layout_set_counts.clear();
        for (const auto& it : g_descriptor_set_layout_owner) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_descriptor_set_layout_owner.clear();
        g_descriptor_bindings.clear();
        for (const auto& it : g_pipeline_layout_handles) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_pipeline_layout_handles.clear();
        for (const auto& it : g_pipeline_handles) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_pipeline_handles.clear();
        for (const auto& it : g_instance_handles) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_instance_handles.clear();
        for (const auto& it : g_physical_device_handles) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_physical_device_handles.clear();
        for (const auto& it : g_device_handles) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_device_handles.clear();
        for (const auto& it : g_queue_handles) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_queue_handles.clear();
        for (const auto& it : g_command_pool_handles) {
            delete reinterpret_cast<uint8_t*>(it.first);
        }
        g_command_pool_handles.clear();
    }
}

VulkanCompute::VulkanCompute()
    : instance_(nullptr),
      physical_device_(nullptr),
      device_(nullptr),
      command_pool_(nullptr),
      compute_queue_(nullptr) {
    std::memset(&device_info_, 0, sizeof(VulkanDeviceInfo));
}

VulkanCompute::~VulkanCompute() {
    Cleanup();
}

bool VulkanCompute::Initialize() {
    if (instance_ && physical_device_ && device_ && command_pool_) {
        if (command_buffer_pool_.empty()) {
            InitializeCommandBufferPool(4);
        }
        return true;
    }
    if (!CreateInstance()) {
        Cleanup();
        return false;
    }
    if (!SelectPhysicalDevice()) {
        Cleanup();
        return false;
    }
    if (!CreateLogicalDevice()) {
        Cleanup();
        return false;
    }
    if (!CreateCommandPool()) {
        Cleanup();
        return false;
    }
    return true;
}

void VulkanCompute::Cleanup() {
    ReleaseTensors();
    CleanupCommandBufferPool();

#ifdef RAWR_ENABLE_VULKAN
    if (command_pool_ && device_) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
    }
    if (device_) {
        vkDestroyDevice(device_, nullptr);
    }
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
    }
#endif
    instance_ = nullptr;
    physical_device_ = nullptr;
    device_ = nullptr;
    command_pool_ = nullptr;
    compute_queue_ = nullptr;
    descriptor_pool_ = nullptr;
    matmul_descriptor_set_layout_ = nullptr;
    matmul_descriptor_pool_ = nullptr;
    kv_cache_allocated_ = false;
    kv_cache_num_layers_ = 0;
    kv_cache_max_seq_len_ = 0;
    kv_cache_head_dim_ = 0;
    staging_buffer_ = nullptr;
    staging_memory_ = nullptr;
    g_staging_buffer_size = 0;
    device_info_ = VulkanDeviceInfo{};
    release_cpu_buffer_registry();
}

bool VulkanCompute::LoadShader(const std::string& name, const std::string& spirv_path) {
    if (name.empty() || spirv_path.empty()) {
        return false;
    }

    std::vector<uint32_t> spirv_code;
    if (!LoadSPIRVCode(spirv_path, spirv_code)) {
        fprintf(stderr, "[VulkanCompute] LoadShader failed: invalid SPIR-V file %s\n", spirv_path.c_str());
        return false;
    }

    auto existing = shaders_.find(name);
    if (existing != shaders_.end()) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        if (existing->second.layout) {
            g_pipeline_layout_handles.erase(existing->second.layout);
            delete reinterpret_cast<uint8_t*>(existing->second.layout);
            existing->second.layout = nullptr;
        }
        if (existing->second.pipeline) {
            g_pipeline_handles.erase(existing->second.pipeline);
            delete reinterpret_cast<uint8_t*>(existing->second.pipeline);
            existing->second.pipeline = nullptr;
        }
        if (existing->second.module) {
            delete reinterpret_cast<uint8_t*>(existing->second.module);
            existing->second.module = nullptr;
        }
    }

    ComputeShader shader;
    shader.name = name;
    shader.spirv_code = std::move(spirv_code);
    shaders_[name] = std::move(shader);
    return true;
}

bool VulkanCompute::CreateComputePipeline(const std::string& shader_name) {
    auto it = shaders_.find(shader_name);
    if (it == shaders_.end()) {
        return false;
    }
    if (it->second.spirv_code.empty() || it->second.spirv_code[0] != kSpirvMagic) {
        return false;
    }
    if (it->second.layout && it->second.pipeline) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        if (g_pipeline_layout_handles.find(it->second.layout) != g_pipeline_layout_handles.end() &&
            g_pipeline_handles.find(it->second.pipeline) != g_pipeline_handles.end()) {
            return true;
        }
    }

    uint8_t* layout_token = new (std::nothrow) uint8_t(0);
    uint8_t* pipeline_token = new (std::nothrow) uint8_t(0);
    uint8_t* module_token = new (std::nothrow) uint8_t(0);
    if (!layout_token || !pipeline_token || !module_token) {
        delete layout_token;
        delete pipeline_token;
        delete module_token;
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    if (it->second.layout) {
        g_pipeline_layout_handles.erase(it->second.layout);
        delete reinterpret_cast<uint8_t*>(it->second.layout);
    }
    if (it->second.pipeline) {
        g_pipeline_handles.erase(it->second.pipeline);
        delete reinterpret_cast<uint8_t*>(it->second.pipeline);
    }
    if (it->second.module) {
        delete reinterpret_cast<uint8_t*>(it->second.module);
    }

    it->second.module = reinterpret_cast<VkShaderModule>(module_token);
    it->second.layout = reinterpret_cast<VkPipelineLayout>(layout_token);
    it->second.pipeline = reinterpret_cast<VkPipeline>(pipeline_token);
    g_pipeline_layout_handles[it->second.layout] = true;
    g_pipeline_handles[it->second.pipeline] = true;
    return true;
}

VulkanTensor VulkanCompute::TransferGGUFTensor(const std::string& tensor_name,
                                               const void* data_ptr,
                                               size_t size_bytes,
                                               uint32_t usage) {
    VulkanTensor tensor;
    tensor.name = tensor_name;
    tensor.size_bytes = size_bytes;
    (void)usage;

    if (!data_ptr || size_bytes == 0 || (size_bytes % sizeof(float)) != 0) {
        return tensor;
    }
    const size_t num_elements = size_bytes / sizeof(float);
    if (num_elements > (std::numeric_limits<size_t>::max() / sizeof(float))) {
        return tensor;
    }

    const float* float_data = static_cast<const float*>(data_ptr);
    tensor.host_data.assign(float_data, float_data + num_elements);

    VkBuffer buffer = nullptr;
    VkDeviceMemory memory = nullptr;
    if (AllocateBuffer(size_bytes, buffer, memory)) {
        if (CopyHostToBuffer(const_cast<void*>(data_ptr), buffer, size_bytes)) {
            tensor.device_buffer = buffer;
            tensor.device_memory = memory;
        } else {
            std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
            auto buf_it = g_cpu_buffers.find(buffer);
            if (buf_it != g_cpu_buffers.end()) {
                delete reinterpret_cast<uint8_t*>(buf_it->first);
                g_cpu_buffers.erase(buf_it);
            }
            auto mem_it = g_cpu_memory_blocks.find(memory);
            if (mem_it != g_cpu_memory_blocks.end()) {
                delete reinterpret_cast<uint8_t*>(mem_it->first);
                g_cpu_memory_blocks.erase(mem_it);
            }
            allocated_buffers_.erase(
                std::remove_if(allocated_buffers_.begin(), allocated_buffers_.end(),
                    [buffer, memory](const std::pair<VkBuffer, VkDeviceMemory>& p) {
                        return p.first == buffer || p.second == memory;
                    }),
                allocated_buffers_.end());
        }
    }

    g_tensors.push_back(tensor);
    uploaded_tensors_.push_back(tensor);
    return tensor;
}

void VulkanCompute::ReleaseTensors() {
    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);

    for (const auto& shader_it : shaders_) {
        if (shader_it.second.layout) {
            g_pipeline_layout_handles.erase(shader_it.second.layout);
            delete reinterpret_cast<uint8_t*>(shader_it.second.layout);
        }
        if (shader_it.second.pipeline) {
            g_pipeline_handles.erase(shader_it.second.pipeline);
            delete reinterpret_cast<uint8_t*>(shader_it.second.pipeline);
        }
        if (shader_it.second.module) {
            delete reinterpret_cast<uint8_t*>(shader_it.second.module);
        }
    }
    shaders_.clear();

    for (const auto& kv : g_descriptor_set_layout_owner) {
        delete reinterpret_cast<uint8_t*>(kv.first);
    }
    g_descriptor_set_layout_owner.clear();
    g_descriptor_bindings.clear();

    for (const auto& kv : g_descriptor_layout_bindings) {
        delete reinterpret_cast<uint8_t*>(kv.first);
    }
    g_descriptor_layout_bindings.clear();
    g_descriptor_layout_set_counts.clear();
    g_descriptor_layouts.clear();

    for (const auto& alloc : allocated_buffers_) {
        auto buf_it = g_cpu_buffers.find(alloc.first);
        if (buf_it != g_cpu_buffers.end()) {
            delete reinterpret_cast<uint8_t*>(buf_it->first);
            g_cpu_buffers.erase(buf_it);
        }
        auto mem_it = g_cpu_memory_blocks.find(alloc.second);
        if (mem_it != g_cpu_memory_blocks.end()) {
            delete reinterpret_cast<uint8_t*>(mem_it->first);
            g_cpu_memory_blocks.erase(mem_it);
        }
    }
    allocated_buffers_.clear();

    g_tensors.clear();
    uploaded_tensors_.clear();
    kv_cache_buffers_.clear();
    g_kv_cache.clear();
    g_kv_cache_allocated = false;
    kv_cache_allocated_ = false;
    kv_cache_num_layers_ = 0;
    kv_cache_max_seq_len_ = 0;
    kv_cache_head_dim_ = 0;
}

bool VulkanCompute::EnsureMatMulPipeline(const std::string& spirv_path) {
    const std::string kMatMulShaderName = "matmul";
    auto it = shaders_.find(kMatMulShaderName);
    if (it != shaders_.end() && it->second.pipeline != nullptr && it->second.layout != nullptr &&
        !it->second.spirv_code.empty()) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        if (g_pipeline_handles.find(it->second.pipeline) != g_pipeline_handles.end() &&
            g_pipeline_layout_handles.find(it->second.layout) != g_pipeline_layout_handles.end()) {
            return true;
        }
    }
    if (!LoadShader(kMatMulShaderName, spirv_path)) {
        return false;
    }
    return CreateComputePipeline(kMatMulShaderName);
}

bool VulkanCompute::DispatchMatMul(uint32_t input_a_idx, uint32_t input_b_idx,
                                   uint32_t output_idx, uint32_t M, uint32_t K, uint32_t N) {
    if (input_a_idx >= g_tensors.size() || input_b_idx >= g_tensors.size() || output_idx >= g_tensors.size()) {
        return false;
    }
    
    const auto& A = g_tensors[input_a_idx];
    const auto& B = g_tensors[input_b_idx];
    auto& C = g_tensors[output_idx];
    const size_t m_sz = static_cast<size_t>(M);
    const size_t k_sz = static_cast<size_t>(K);
    const size_t n_sz = static_cast<size_t>(N);
    if (m_sz > (std::numeric_limits<size_t>::max() / k_sz)) return false;
    if (k_sz > (std::numeric_limits<size_t>::max() / n_sz)) return false;
    if (m_sz > (std::numeric_limits<size_t>::max() / n_sz)) return false;
    const size_t required_a = static_cast<size_t>(M) * static_cast<size_t>(K);
    const size_t required_b = static_cast<size_t>(K) * static_cast<size_t>(N);
    const size_t required_c = static_cast<size_t>(M) * static_cast<size_t>(N);
    if (A.host_data.size() < required_a || B.host_data.size() < required_b) {
        return false;
    }
    if (C.host_data.size() < required_c) {
        C.host_data.resize(required_c);
    }
    
    const bool ok = ExecuteMatMul(A.host_data.data(), B.host_data.data(),
                                  C.host_data.data(), M, K, N);
    if (!ok) {
        return false;
    }
    if (C.device_buffer != nullptr) {
        const size_t bytes = required_c * sizeof(float);
        if (!CopyHostToBuffer(C.host_data.data(), C.device_buffer, bytes)) {
            return false;
        }
    }
    return true;
}

bool VulkanCompute::DispatchMatMulAsync(uint32_t input_a_idx, uint32_t input_b_idx,
                                        uint32_t output_idx, uint32_t M, uint32_t K, uint32_t N) {
    VkCommandBuffer cmd = AcquireAsyncCommandBuffer();
    if (!cmd) {
        return false;
    }

    const bool dispatched = DispatchMatMul(input_a_idx, input_b_idx, output_idx, M, K, N);
    if (!dispatched) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        recycle_or_release_command_handle_locked(cmd, &command_buffer_pool_, &available_buffer_indices_);
        return false;
    }

    if (!ExecuteCommandBuffer(cmd)) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        recycle_or_release_command_handle_locked(cmd, &command_buffer_pool_, &available_buffer_indices_);
        return false;
    }

    if (!FlushAsyncCommands()) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        recycle_or_release_command_handle_locked(cmd, &command_buffer_pool_, &available_buffer_indices_);
        return false;
    }
    const bool completed = CheckAsyncCompletion(cmd);
    {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        recycle_or_release_command_handle_locked(cmd, &command_buffer_pool_, &available_buffer_indices_);
    }
    return completed;
}

// CPU implementations of compute operations
bool VulkanCompute::ExecuteMatMul(const float* input_a, const float* input_b, 
                                  float* output, uint32_t m, uint32_t k, uint32_t n) {
    if (!input_a || !input_b || !output || m == 0 || k == 0 || n == 0) {
        return false;
    }
    const size_t m_sz = static_cast<size_t>(m);
    const size_t k_sz = static_cast<size_t>(k);
    const size_t n_sz = static_cast<size_t>(n);
    if (m_sz > (std::numeric_limits<size_t>::max() / k_sz)) return false;
    if (k_sz > (std::numeric_limits<size_t>::max() / n_sz)) return false;
    if (m_sz > (std::numeric_limits<size_t>::max() / n_sz)) return false;

    constexpr uint32_t kBlock = 32;
    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            output[static_cast<size_t>(i) * n + j] = 0.0f;
        }
    }

    // Blocked CPU matmul for better locality in the standalone fallback path.
    for (uint32_t i0 = 0; i0 < m; i0 += kBlock) {
        const uint32_t i_max = std::min(m, i0 + kBlock);
        for (uint32_t l0 = 0; l0 < k; l0 += kBlock) {
            const uint32_t l_max = std::min(k, l0 + kBlock);
            for (uint32_t j0 = 0; j0 < n; j0 += kBlock) {
                const uint32_t j_max = std::min(n, j0 + kBlock);
                for (uint32_t i = i0; i < i_max; ++i) {
                    for (uint32_t l = l0; l < l_max; ++l) {
                        const float a = input_a[static_cast<size_t>(i) * k + l];
                        for (uint32_t j = j0; j < j_max; ++j) {
                            output[static_cast<size_t>(i) * n + j] += a * input_b[static_cast<size_t>(l) * n + j];
                        }
                    }
                }
            }
        }
    }
    for (size_t idx = 0; idx < m_sz * n_sz; ++idx) {
        if (!std::isfinite(output[idx])) {
            return false;
        }
    }
    return true;
}

bool VulkanCompute::ExecuteAttention(const float* queries, const float* keys, const float* values,
                                    float* output, uint32_t seq_len, uint32_t head_dim) {
    if (!queries || !keys || !values || !output || seq_len == 0 || head_dim == 0) {
        return false;
    }
    const size_t seq_sz = static_cast<size_t>(seq_len);
    const size_t dim_sz = static_cast<size_t>(head_dim);
    if (seq_sz > (std::numeric_limits<size_t>::max() / seq_sz)) return false;
    if (seq_sz > (std::numeric_limits<size_t>::max() / dim_sz)) return false;

    // Simplified attention: Q * K^T / sqrt(d) -> softmax -> * V
    const float scale = 1.0f / sqrtf(static_cast<float>(head_dim));
    if (!std::isfinite(scale)) {
        return false;
    }
    
    // Compute Q * K^T
    std::vector<float> scores(seq_sz * seq_sz);
    for (uint32_t i = 0; i < seq_len; ++i) {
        for (uint32_t j = 0; j < seq_len; ++j) {
            double dot = 0.0;
            for (uint32_t d = 0; d < head_dim; ++d) {
                dot += static_cast<double>(queries[static_cast<size_t>(i) * head_dim + d]) *
                       static_cast<double>(keys[static_cast<size_t>(j) * head_dim + d]);
            }
            const float logit = static_cast<float>(dot * static_cast<double>(scale));
            scores[static_cast<size_t>(i) * seq_len + j] = std::isfinite(logit) ? logit : 0.0f;
        }
    }
    
    // Softmax over rows
    for (uint32_t i = 0; i < seq_len; ++i) {
        float max_val = *std::max_element(scores.begin() + static_cast<size_t>(i) * seq_len, 
                                         scores.begin() + static_cast<size_t>(i + 1) * seq_len);
        if (!std::isfinite(max_val)) return false;
        double sum = 0.0;
        for (uint32_t j = 0; j < seq_len; ++j) {
            const float shifted = std::clamp(scores[static_cast<size_t>(i) * seq_len + j] - max_val, -80.0f, 80.0f);
            scores[static_cast<size_t>(i) * seq_len + j] = expf(shifted);
            sum += scores[static_cast<size_t>(i) * seq_len + j];
        }
        if (sum <= 0.0f || !std::isfinite(sum)) {
            return false;
        }
        const float inv_sum = static_cast<float>(1.0 / sum);
        for (uint32_t j = 0; j < seq_len; ++j) {
            scores[static_cast<size_t>(i) * seq_len + j] *= inv_sum;
        }
    }
    
    // Weighted sum with values
    for (uint32_t i = 0; i < seq_len; ++i) {
        for (uint32_t d = 0; d < head_dim; ++d) {
            double sum = 0.0;
            for (uint32_t j = 0; j < seq_len; ++j) {
                sum += static_cast<double>(scores[static_cast<size_t>(i) * seq_len + j]) *
                       static_cast<double>(values[static_cast<size_t>(j) * head_dim + d]);
            }
            const float out = static_cast<float>(sum);
            output[static_cast<size_t>(i) * head_dim + d] = std::isfinite(out) ? out : 0.0f;
        }
    }
    
    return true;
}

bool VulkanCompute::ExecuteRoPE(float* embeddings, uint32_t dim, uint32_t seq_pos, uint32_t rotation_dim) {
    if (!embeddings || dim < 2 || rotation_dim < 2) {
        return false;
    }
    if ((rotation_dim & 1u) != 0) {
        rotation_dim -= 1;
    }
    if (rotation_dim > dim) {
        rotation_dim = dim - (dim & 1u);
    }

    constexpr float kTwoPi = 6.2831853071795864769f;
    // Rotary Position Embedding
    for (uint32_t i = 0; i < dim; i += 2) {
        if (i + 1 >= dim || i >= rotation_dim) break;
        
        const float theta = seq_pos * powf(10000.0f, -2.0f * (i / 2) / static_cast<float>(rotation_dim));
        const float angle = std::fmod(theta, kTwoPi);
        const float cos_theta = cosf(angle);
        const float sin_theta = sinf(angle);
        
        const float x0 = embeddings[i];
        const float x1 = embeddings[i + 1];
        if (!std::isfinite(x0) || !std::isfinite(x1) || !std::isfinite(cos_theta) || !std::isfinite(sin_theta)) {
            return false;
        }
        
        embeddings[i] = x0 * cos_theta - x1 * sin_theta;
        embeddings[i + 1] = x0 * sin_theta + x1 * cos_theta;
    }
    return true;
}

bool VulkanCompute::ExecuteRMSNorm(float* data, uint32_t size, float epsilon) {
    if (!data || size == 0) {
        return false;
    }
    if (!std::isfinite(epsilon) || epsilon < 1e-12f) {
        epsilon = 1e-12f;
    }

    // RMS normalization
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        const float v = data[i];
        if (!std::isfinite(v)) return false;
        sum_sq += static_cast<double>(v) * static_cast<double>(v);
    }
    if (!std::isfinite(sum_sq)) return false;
    const float inv_rms = 1.0f / sqrtf(static_cast<float>(sum_sq / static_cast<double>(size)) + epsilon);
    if (!std::isfinite(inv_rms)) return false;
    
    for (uint32_t i = 0; i < size; ++i) {
        const float out = data[i] * inv_rms;
        data[i] = std::isfinite(out) ? out : 0.0f;
    }
    return true;
}

bool VulkanCompute::ExecuteSiLU(float* data, uint32_t size) {
    if (!data || size == 0) {
        return false;
    }

    // SiLU activation: x * sigmoid(x)
    for (uint32_t i = 0; i < size; ++i) {
        const float x = std::isfinite(data[i]) ? data[i] : 0.0f;
        const float clamped = std::clamp(x, -80.0f, 80.0f);
        const float out = x / (1.0f + expf(-clamped));
        data[i] = std::isfinite(out) ? out : 0.0f;
    }
    return true;
}

bool VulkanCompute::ExecuteSoftmax(float* data, uint32_t size) {
    if (!data || size == 0) {
        return false;
    }

    // Softmax
    float max_val = *std::max_element(data, data + size);
    if (!std::isfinite(max_val)) return false;
    double sum = 0.0;
    
    for (uint32_t i = 0; i < size; ++i) {
        const float shifted = std::clamp(data[i] - max_val, -80.0f, 80.0f);
        data[i] = expf(shifted);
        sum += data[i];
    }
    
    if (sum <= 0.0f || !std::isfinite(sum)) {
        return false;
    }

    const float inv_sum = static_cast<float>(1.0 / sum);
    for (uint32_t i = 0; i < size; ++i) {
        const float out = data[i] * inv_sum;
        data[i] = std::isfinite(out) ? out : 0.0f;
    }
    return true;
}

bool VulkanCompute::ExecuteDequantize(const uint8_t* quantized, float* output,
                                      uint32_t elements, const std::string& quant_type) {
    if (!quantized || !output || elements == 0) {
        return false;
    }

    if (quant_type == "q8_0" || quant_type == "int8") {
        const float scale = 1.0f / 127.0f;
        for (uint32_t i = 0; i < elements; ++i) {
            int8_t val = static_cast<int8_t>(quantized[i]);
            output[i] = static_cast<float>(val) * scale;
        }
        return true;
    }

    if (quant_type == "q4_0") {
        const float scale = 1.0f / 7.0f;
        for (uint32_t i = 0; i < elements; ++i) {
            const uint8_t packed = quantized[i / 2];
            const uint8_t nibble = ((i & 1u) == 0u) ? (packed & 0x0Fu) : ((packed >> 4) & 0x0Fu);
            int8_t signed_nibble = static_cast<int8_t>(nibble);
            if (signed_nibble >= 8) {
                signed_nibble = static_cast<int8_t>(signed_nibble - 16);
            }
            output[i] = static_cast<float>(signed_nibble) * scale;
        }
        return true;
    }

    if (quant_type == "q4_1") {
        // Lightweight q4_1 decode: signed 4-bit payload mapped to [-1,1] with optional bias.
        // We reserve the first 4 bytes as a per-block (32 values) fp16-like scale/bias payload in compact mode.
        constexpr uint32_t kBlock = 32;
        for (uint32_t i = 0; i < elements; ++i) {
            const uint32_t block_idx = i / kBlock;
            const uint32_t in_block = i % kBlock;
            const size_t block_base = static_cast<size_t>(block_idx) * (4 + (kBlock / 2));
            const uint8_t packed = quantized[block_base + 4 + (in_block / 2)];
            const uint8_t nibble = ((in_block & 1u) == 0u) ? (packed & 0x0Fu) : ((packed >> 4) & 0x0Fu);
            int8_t signed_nibble = static_cast<int8_t>(nibble);
            if (signed_nibble >= 8) {
                signed_nibble = static_cast<int8_t>(signed_nibble - 16);
            }
            const float block_scale = static_cast<float>(quantized[block_base + 0]) / 255.0f;
            const float block_bias = static_cast<float>(static_cast<int8_t>(quantized[block_base + 1])) / 127.0f;
            const float v = (static_cast<float>(signed_nibble) / 7.0f) * std::max(block_scale, 1e-6f) + block_bias;
            output[i] = std::isfinite(v) ? v : 0.0f;
        }
        return true;
    }

    if (quant_type == "u8") {
        const float scale = 1.0f / 255.0f;
        for (uint32_t i = 0; i < elements; ++i) {
            output[i] = static_cast<float>(quantized[i]) * scale;
        }
        return true;
    }

    // Default fallback: reinterpret as signed bytes.
    for (uint32_t i = 0; i < elements; ++i) {
        output[i] = static_cast<float>(static_cast<int8_t>(quantized[i]));
    }
    return true;
}

bool VulkanCompute::AllocateBuffer(size_t size, uint32_t& buffer_idx, size_t& memory_size) {
    if (size == 0 || (size % sizeof(float)) != 0) {
        return false;
    }
    const size_t elem_count = size / sizeof(float);
    if (elem_count > (std::numeric_limits<size_t>::max() / sizeof(float))) {
        return false;
    }
    if (g_tensors.size() >= static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
        return false;
    }

    buffer_idx = static_cast<uint32_t>(g_tensors.size());
    VulkanTensor tensor;
    tensor.name = "tensor_" + std::to_string(buffer_idx);
    tensor.size_bytes = size;
    tensor.host_data.assign(elem_count, 0.0f);
    tensor.device_buffer = nullptr;
    tensor.device_memory = nullptr;
    g_tensors.push_back(tensor);
    memory_size = size;
    return true;
}

bool VulkanCompute::AllocateBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory) {
    if (size == 0 || buffer != nullptr || memory != nullptr) {
        return false;
    }
    if (size > (std::numeric_limits<size_t>::max() / sizeof(uint8_t))) {
        return false;
    }

    uint8_t* buffer_token = new (std::nothrow) uint8_t(0);
    uint8_t* memory_token = new (std::nothrow) uint8_t(0);
    if (!buffer_token || !memory_token) {
        delete buffer_token;
        delete memory_token;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        VkBuffer buffer_handle = reinterpret_cast<VkBuffer>(buffer_token);
        VkDeviceMemory memory_handle = reinterpret_cast<VkDeviceMemory>(memory_token);
        g_cpu_buffers[buffer_handle] = std::vector<uint8_t>(size, 0);
        g_cpu_memory_blocks[memory_handle] = size;

        // Reuse descriptor tracking path so descriptor updates can validate memory ownership.
        for (auto& set_entry : g_descriptor_bindings) {
            for (auto& binding_entry : set_entry.second) {
                if (binding_entry.second.first == buffer_handle) {
                    binding_entry.second.second = std::min(binding_entry.second.second, size);
                }
            }
        }
    }

    buffer = reinterpret_cast<VkBuffer>(buffer_token);
    memory = reinterpret_cast<VkDeviceMemory>(memory_token);
    allocated_buffers_.push_back({buffer, memory});
    return true;
}

bool VulkanCompute::CopyBufferToHost(uint32_t buffer_idx, void* host_data, size_t size) {
    if (!host_data || size == 0 || buffer_idx >= g_tensors.size()) return false;
    auto& src_tensor = g_tensors[buffer_idx];
    const size_t max_bytes = src_tensor.host_data.size() * sizeof(float);
    if (size > max_bytes) return false;
    if (max_bytes == 0) return false;

    if (src_tensor.device_buffer != nullptr) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        auto dev_it = g_cpu_buffers.find(src_tensor.device_buffer);
        if (dev_it != g_cpu_buffers.end() && dev_it->second.size() >= size) {
            std::memcpy(src_tensor.host_data.data(), dev_it->second.data(), size);
        }
    }
    std::memmove(host_data, src_tensor.host_data.data(), size);
    return true;
}

bool VulkanCompute::CopyBufferToHost(VkBuffer device_buffer, void* host_data, size_t size) {
    if (!device_buffer || !host_data || size == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    auto it = g_cpu_buffers.find(device_buffer);
    if (it == g_cpu_buffers.end() || it->second.size() < size) {
        return false;
    }
    if (device_buffer == staging_buffer_ && g_staging_buffer_size != 0 && size > g_staging_buffer_size) {
        return false;
    }

    std::memmove(host_data, it->second.data(), size);
    return true;
}

bool VulkanCompute::CopyHostToBuffer(void* host_data, uint32_t buffer_idx, size_t size) {
    if (!host_data || size == 0 || (size % sizeof(float)) != 0 || buffer_idx >= g_tensors.size()) return false;
    auto& dst = g_tensors[buffer_idx];
    const size_t incoming_floats = size / sizeof(float);
    if (dst.size_bytes != 0 && size > dst.size_bytes) return false;
    if (incoming_floats > dst.host_data.size()) {
        dst.host_data.resize(incoming_floats, 0.0f);
    }
    std::memcpy(dst.host_data.data(), host_data, size);
    if (dst.size_bytes == 0) {
        dst.size_bytes = size;
    }
    if (dst.device_buffer != nullptr) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        auto dev_it = g_cpu_buffers.find(dst.device_buffer);
        if (dev_it != g_cpu_buffers.end() && dev_it->second.size() >= size) {
            std::memcpy(dev_it->second.data(), host_data, size);
        }
    }
    return true;
}

bool VulkanCompute::CopyHostToBuffer(void* host_data, VkBuffer device_buffer, size_t size) {
    if (!host_data || !device_buffer || size == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    auto it = g_cpu_buffers.find(device_buffer);
    if (it == g_cpu_buffers.end()) {
        return false;
    }

    if (it->second.size() < size) {
        return false;
    }
    if (device_buffer == staging_buffer_ && g_staging_buffer_size != 0 && size > g_staging_buffer_size) {
        return false;
    }
    std::memmove(it->second.data(), host_data, size);

    if ((size % sizeof(float)) == 0) {
        const size_t float_count = size / sizeof(float);
        const float* src = static_cast<const float*>(host_data);
        for (auto& tensor : g_tensors) {
            if (tensor.device_buffer == device_buffer) {
                if (tensor.host_data.size() < float_count) {
                    tensor.host_data.resize(float_count, 0.0f);
                }
                std::memcpy(tensor.host_data.data(), src, size);
                tensor.size_bytes = std::max(tensor.size_bytes, size);
            }
        }
    }
    return true;
}

// KV Cache management (CPU implementation)
bool VulkanCompute::AllocateKVCache(uint32_t num_layers, uint32_t max_seq_len, uint32_t head_dim) {
    if (num_layers == 0 || max_seq_len == 0 || head_dim == 0) {
        return false;
    }
    const size_t row_width = static_cast<size_t>(head_dim);
    const size_t rows = static_cast<size_t>(max_seq_len);
    if (rows > (std::numeric_limits<size_t>::max() / row_width)) {
        return false;
    }
    const size_t per_layer = rows * row_width;
    if (per_layer > (std::numeric_limits<size_t>::max() / sizeof(float))) {
        return false;
    }

    g_kv_cache.clear();
    g_kv_cache.resize(num_layers);
    for (auto& layer : g_kv_cache) {
        layer.k_cache.assign(per_layer, 0.0f);
        layer.v_cache.assign(per_layer, 0.0f);
        layer.current_seq_len = 0;
    }
    g_kv_cache_allocated = true;
    kv_cache_num_layers_ = num_layers;
    kv_cache_max_seq_len_ = max_seq_len;
    kv_cache_head_dim_ = head_dim;
    kv_cache_allocated_ = true;
    return true;
}

bool VulkanCompute::AppendToKVCache(uint32_t layer_idx, const float* k_new, const float* v_new, uint32_t token_pos) {
    if (!k_new || !v_new || layer_idx >= g_kv_cache.size()) return false;
    auto& layer = g_kv_cache[layer_idx];
    if (!kv_cache_allocated_ || kv_cache_head_dim_ == 0 || kv_cache_max_seq_len_ == 0) return false;
    if (token_pos >= kv_cache_max_seq_len_) return false;

    const size_t row_width = static_cast<size_t>(kv_cache_head_dim_);
    const size_t offset = static_cast<size_t>(token_pos) * row_width;
    if (offset + row_width > layer.k_cache.size() || offset + row_width > layer.v_cache.size()) return false;

    for (size_t i = 0; i < row_width; ++i) {
        const float k = k_new[i];
        const float v = v_new[i];
        layer.k_cache[offset + i] = std::isfinite(k) ? k : 0.0f;
        layer.v_cache[offset + i] = std::isfinite(v) ? v : 0.0f;
    }

    if (token_pos >= layer.current_seq_len) {
        layer.current_seq_len = token_pos + 1;
    }
    return true;
}

bool VulkanCompute::GetKVCacheSlice(uint32_t layer_idx, uint32_t start_pos, uint32_t end_pos, 
                                    float* k_out, float* v_out) {
    if (!k_out || !v_out || layer_idx >= g_kv_cache.size()) return false;
    auto& layer = g_kv_cache[layer_idx];

    if (start_pos >= end_pos) return false;
    if (end_pos > layer.current_seq_len) return false;
    if (!kv_cache_allocated_ || kv_cache_head_dim_ == 0) return false;

    const size_t row_width = static_cast<size_t>(kv_cache_head_dim_);
    const size_t begin = static_cast<size_t>(start_pos) * row_width;
    const size_t end = static_cast<size_t>(end_pos) * row_width;
    if (end > layer.k_cache.size() || end > layer.v_cache.size()) return false;

    const size_t elem_count = end - begin;
    for (size_t i = 0; i < elem_count; ++i) {
        const float k = layer.k_cache[begin + i];
        const float v = layer.v_cache[begin + i];
        k_out[i] = std::isfinite(k) ? k : 0.0f;
        v_out[i] = std::isfinite(v) ? v : 0.0f;
    }
    return true;
}

void VulkanCompute::ClearKVCache() {
    if (!kv_cache_allocated_ || kv_cache_head_dim_ == 0 || kv_cache_max_seq_len_ == 0) {
        return;
    }
    for (auto& layer : g_kv_cache) {
        std::fill(layer.k_cache.begin(), layer.k_cache.end(), 0.0f);
        std::fill(layer.v_cache.begin(), layer.v_cache.end(), 0.0f);
        layer.current_seq_len = 0;
    }
    g_kv_cache_allocated = !g_kv_cache.empty();
}

// Command buffer stubs
bool VulkanCompute::ExecuteSingleTimeCommands(std::function<void(VkCommandBuffer)> record_func) {
    if (!record_func || !command_pool_) {
        return false;
    }

    VkCommandBuffer cmd = AcquireAsyncCommandBuffer();
    if (!cmd) {
        return false;
    }

    try {
        record_func(cmd);
    } catch (...) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        recycle_or_release_command_handle_locked(cmd, &command_buffer_pool_, &available_buffer_indices_);
        return false;
    }

    if (!ExecuteCommandBuffer(cmd)) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        recycle_or_release_command_handle_locked(cmd, &command_buffer_pool_, &available_buffer_indices_);
        return false;
    }
    if (!FlushAsyncCommands()) {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        recycle_or_release_command_handle_locked(cmd, &command_buffer_pool_, &available_buffer_indices_);
        return false;
    }
    const bool completed = CheckAsyncCompletion(cmd);
    {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        recycle_or_release_command_handle_locked(cmd, &command_buffer_pool_, &available_buffer_indices_);
    }
    return completed;
}

bool VulkanCompute::ExecuteCommandBuffer(VkCommandBuffer cmd_buffer) {
    if (!cmd_buffer) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    auto it = g_async_command_submitted.find(cmd_buffer);
    if (it == g_async_command_submitted.end()) {
        return false;
    }
    if (it->second) {
        return false;
    }
    auto pool_it = g_command_pool_index.find(cmd_buffer);
    if (pool_it == g_command_pool_index.end()) {
        return false;
    }
    if (pool_it->second != static_cast<size_t>(-1)) {
        if (pool_it->second >= command_buffer_pool_.size()) {
            return false;
        }
        if (command_buffer_pool_[pool_it->second].buffer != cmd_buffer) {
            return false;
        }
        command_buffer_pool_[pool_it->second].is_available = false;
    }

    it->second = true;
    g_async_command_completed[cmd_buffer] = false;
    return true;
}

VkCommandBuffer VulkanCompute::AcquireAsyncCommandBuffer() {
    if (!command_pool_) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    while (!available_buffer_indices_.empty()) {
        const size_t idx = available_buffer_indices_.front();
        available_buffer_indices_.pop();
        if (idx < command_buffer_pool_.size()) {
            auto& entry = command_buffer_pool_[idx];
            if (entry.buffer) {
                entry.is_available = false;
                g_async_command_submitted[entry.buffer] = false;
                g_async_command_completed[entry.buffer] = false;
                return entry.buffer;
            }
        }
    }

    constexpr size_t kMaxCommandBuffers = 8192;
    if (command_buffer_pool_.size() >= kMaxCommandBuffers) {
        return nullptr;
    }

    uint8_t* token = new (std::nothrow) uint8_t(0);
    if (!token) {
        return nullptr;
    }

    const VkCommandBuffer handle = reinterpret_cast<VkCommandBuffer>(token);
    AsyncCommandBuffer entry{};
    entry.buffer = handle;
    entry.fence = nullptr;
    entry.is_available = false;
    command_buffer_pool_.push_back(entry);
    g_command_pool_index[handle] = command_buffer_pool_.size() - 1;
    g_async_command_submitted[handle] = false;
    g_async_command_completed[handle] = false;
    return handle;
}

bool VulkanCompute::SubmitAsyncCommandBuffer(VkCommandBuffer cmd_buffer) {
    if (!cmd_buffer) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    auto it = g_async_command_submitted.find(cmd_buffer);
    if (it == g_async_command_submitted.end()) {
        return false;
    }
    if (it->second) {
        return false;
    }
    auto pool_it = g_command_pool_index.find(cmd_buffer);
    if (pool_it == g_command_pool_index.end()) {
        return false;
    }
    if (pool_it->second != static_cast<size_t>(-1)) {
        if (pool_it->second >= command_buffer_pool_.size()) {
            return false;
        }
        if (command_buffer_pool_[pool_it->second].buffer != cmd_buffer) {
            return false;
        }
    }

    it->second = true;
    g_async_command_completed[cmd_buffer] = false;
    if (pool_it != g_command_pool_index.end() && pool_it->second != static_cast<size_t>(-1) &&
        pool_it->second < command_buffer_pool_.size()) {
        command_buffer_pool_[pool_it->second].is_available = false;
    }
    return true;
}

bool VulkanCompute::FlushAsyncCommands() {
    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    std::vector<VkCommandBuffer> submitted_handles;
    submitted_handles.reserve(g_async_command_submitted.size());
    for (const auto& it : g_async_command_submitted) {
        if (it.second) {
            submitted_handles.push_back(it.first);
        }
    }
    for (VkCommandBuffer handle : submitted_handles) {
        g_async_command_submitted[handle] = false;
        g_async_command_completed[handle] = true;
        auto pool_it = g_command_pool_index.find(handle);
        if (pool_it != g_command_pool_index.end() &&
            pool_it->second != static_cast<size_t>(-1) &&
            pool_it->second < command_buffer_pool_.size()) {
            auto& entry = command_buffer_pool_[pool_it->second];
            if (!entry.is_available) {
                entry.is_available = true;
                available_buffer_indices_.push(pool_it->second);
            }
        } else {
            release_command_buffer_handle_locked(handle);
        }
    }
    return true;
}

bool VulkanCompute::CheckAsyncCompletion(VkCommandBuffer cmd_buffer) {
    if (!cmd_buffer) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    auto it = g_async_command_completed.find(cmd_buffer);
    if (it == g_async_command_completed.end()) {
        return false;
    }
    const bool completed = it->second;
    if (completed) {
        it->second = false;
        auto pool_it = g_command_pool_index.find(cmd_buffer);
        if (pool_it != g_command_pool_index.end() &&
            pool_it->second != static_cast<size_t>(-1) &&
            pool_it->second < command_buffer_pool_.size()) {
            command_buffer_pool_[pool_it->second].is_available = true;
        }
    }
    return completed;
}

// Descriptor set stubs
bool VulkanCompute::CreateDescriptorSetLayout(uint32_t binding_count, VkDescriptorSetLayout& layout) {
    if (binding_count == 0 || binding_count > 64 || layout != nullptr) {
        return false;
    }
    if (!device_ || !command_pool_) {
        return false;
    }

    uint8_t* token = new (std::nothrow) uint8_t(0);
    if (!token) {
        return false;
    }

    layout = reinterpret_cast<VkDescriptorSetLayout>(token);
    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    g_descriptor_layout_bindings[layout] = binding_count;
    g_descriptor_layout_set_counts[layout] = 0;
    g_descriptor_layouts["layout_" + std::to_string(reinterpret_cast<uintptr_t>(layout))] = layout;
    return true;
}

bool VulkanCompute::AllocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet& descriptor_set) {
    if (!layout || descriptor_set != nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    if (g_descriptor_layout_bindings.find(layout) == g_descriptor_layout_bindings.end()) {
        return false;
    }
    auto count_it = g_descriptor_layout_set_counts.find(layout);
    if (count_it == g_descriptor_layout_set_counts.end()) {
        return false;
    }
    if (count_it->second >= 4096) {
        return false;
    }

    uint8_t* token = new (std::nothrow) uint8_t(0);
    if (!token) {
        return false;
    }

    descriptor_set = reinterpret_cast<VkDescriptorSet>(token);
    g_descriptor_set_layout_owner[descriptor_set] = layout;
    auto& bindings = g_descriptor_bindings[descriptor_set];
    bindings.clear();
    const uint32_t binding_count = g_descriptor_layout_bindings[layout];
    for (uint32_t b = 0; b < binding_count; ++b) {
        bindings[b] = {nullptr, 0};
    }
    ++count_it->second;
    return true;
}

bool VulkanCompute::UpdateDescriptorSet(VkDescriptorSet descriptor_set, uint32_t binding, 
                                       VkBuffer buffer, size_t buffer_size) {
    if (!descriptor_set || !buffer || buffer_size == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    auto set_it = g_descriptor_set_layout_owner.find(descriptor_set);
    if (set_it == g_descriptor_set_layout_owner.end()) {
        return false;
    }

    auto layout_it = g_descriptor_layout_bindings.find(set_it->second);
    if (layout_it == g_descriptor_layout_bindings.end()) {
        return false;
    }
    if (binding >= layout_it->second) {
        return false;
    }
    auto set_bind_it = g_descriptor_bindings.find(descriptor_set);
    if (set_bind_it == g_descriptor_bindings.end()) {
        return false;
    }
    auto slot_it = set_bind_it->second.find(binding);
    if (slot_it == set_bind_it->second.end()) {
        return false;
    }

    auto buffer_it = g_cpu_buffers.find(buffer);
    if (buffer_it == g_cpu_buffers.end() || buffer_size > buffer_it->second.size()) {
        return false;
    }
    if (buffer_size == 0 || buffer_it->second.empty()) {
        return false;
    }
    bool owned_buffer = false;
    for (const auto& alloc : allocated_buffers_) {
        if (alloc.first == buffer) {
            owned_buffer = true;
            break;
        }
    }
    if (!owned_buffer && buffer != staging_buffer_) {
        return false;
    }

    slot_it->second = {buffer, std::min(buffer_size, buffer_it->second.size())};
    return true;
}

bool VulkanCompute::CreateInstance() {
#ifdef RAWR_ENABLE_VULKAN
    if (instance_) {
        return true;
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "RawrXD-Inference";
    app_info.applicationVersion = VK_MAKE_VERSION(7, 4, 0);
    app_info.pEngineName = "RawrEngine";
    app_info.engineVersion = VK_MAKE_VERSION(7, 4, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    const bool ok = vkCreateInstance(&create_info, nullptr, &instance_) == VK_SUCCESS && instance_ != nullptr;
    if (ok) {
        device_info_ = VulkanDeviceInfo{};
    }
    return ok;
#else
    if (!instance_) {
        uint8_t* token = new (std::nothrow) uint8_t(0);
        if (!token) {
            return false;
        }
        instance_ = reinterpret_cast<VkInstance>(token);
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        g_instance_handles[instance_] = true;
    }
    device_info_.supports_compute = true;
    device_info_.compute_queue_family = 0;
    return true;
#endif
}

bool VulkanCompute::SelectPhysicalDevice() {
#ifdef RAWR_ENABLE_VULKAN
    if (!instance_) {
        return false;
    }

    uint32_t device_count = 0;
    if (vkEnumeratePhysicalDevices(instance_, &device_count, nullptr) != VK_SUCCESS || device_count == 0) {
        return false;
    }

    VkPhysicalDevice devices[16];
    device_count = std::min<uint32_t>(device_count, 16);
    if (vkEnumeratePhysicalDevices(instance_, &device_count, devices) != VK_SUCCESS) {
        return false;
    }

    physical_device_ = devices[0];
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    device_info_.device_name = props.deviceName;
    device_info_.properties = props;
    device_info_.vendor_id = props.vendorID;
    device_info_.device_id = props.deviceID;
    return true;
#else
    if (!instance_) {
        return false;
    }
    if (!physical_device_) {
        uint8_t* token = new (std::nothrow) uint8_t(0);
        if (!token) {
            return false;
        }
        physical_device_ = reinterpret_cast<VkPhysicalDevice>(token);
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        g_physical_device_handles[physical_device_] = true;
    }
    device_info_.device_name = "CPU Fallback";
    device_info_.vendor_id = 0xFFFF;
    device_info_.device_id = 1;
    device_info_.properties.vendorID = device_info_.vendor_id;
    device_info_.properties.deviceID = device_info_.device_id;
    std::snprintf(device_info_.properties.deviceName, sizeof(device_info_.properties.deviceName), "%s", "CPU Fallback");
    device_info_.memory_props.memoryTypeCount = 1;
    device_info_.supports_compute = true;
    device_info_.compute_queue_family = 0;
    return true;
#endif
}

bool VulkanCompute::CreateLogicalDevice() {
#ifdef RAWR_ENABLE_VULKAN
    if (!physical_device_) {
        return false;
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    if (queue_family_count == 0) {
        return false;
    }

    VkQueueFamilyProperties queue_families[64];
    queue_family_count = std::min<uint32_t>(queue_family_count, 64);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families);

    uint32_t compute_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if ((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
            compute_family = i;
            break;
        }
    }
    if (compute_family == UINT32_MAX) {
        return false;
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = compute_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;

    if (vkCreateDevice(physical_device_, &device_create_info, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(device_, compute_family, 0, &compute_queue_);
    if (!compute_queue_) {
        vkDestroyDevice(device_, nullptr);
        device_ = nullptr;
        return false;
    }
    device_info_.compute_queue_family = compute_family;
    device_info_.supports_compute = true;
    return true;
#else
    if (!instance_ || !physical_device_) {
        return false;
    }
    if (!device_) {
        uint8_t* device_token = new (std::nothrow) uint8_t(0);
        if (!device_token) {
            return false;
        }
        device_ = reinterpret_cast<VkDevice>(device_token);
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        g_device_handles[device_] = true;
    }
    if (!compute_queue_) {
        uint8_t* queue_token = new (std::nothrow) uint8_t(0);
        if (!queue_token) {
            return false;
        }
        compute_queue_ = reinterpret_cast<VkQueue>(queue_token);
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        g_queue_handles[compute_queue_] = true;
    }
    device_info_.compute_queue_family = 0;
    device_info_.supports_compute = true;
    return true;
#endif
}

bool VulkanCompute::CreateCommandPool() {
#ifdef RAWR_ENABLE_VULKAN
    if (!device_) {
        return false;
    }

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = device_info_.compute_queue_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    const bool ok = vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) == VK_SUCCESS;
    if (ok) {
        InitializeCommandBufferPool(4);
    }
    return ok;
#else
    if (!device_ || !compute_queue_) {
        return false;
    }
    if (!command_pool_) {
        uint8_t* token = new (std::nothrow) uint8_t(0);
        if (!token) {
            return false;
        }
        command_pool_ = reinterpret_cast<VkCommandPool>(token);
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        g_command_pool_handles[command_pool_] = true;
    }
    if (command_buffer_pool_.empty()) {
        InitializeCommandBufferPool(4);
    }
    return true;
#endif
}

bool VulkanCompute::LoadSPIRVCode(const std::string& path, std::vector<uint32_t>& code) {
    code.clear();
    if (path.empty()) {
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    in.seekg(0, std::ios::end);
    const std::streamsize file_size = in.tellg();
    constexpr std::streamsize kMaxSpirvBytes = 64 * 1024 * 1024;
    if (file_size <= 0 || file_size > kMaxSpirvBytes || (file_size % 4) != 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);

    const size_t word_count = static_cast<size_t>(file_size / 4);
    if (word_count < 5) {
        return false;
    }
    code.resize(word_count);
    if (!in.read(reinterpret_cast<char*>(code.data()), file_size)) {
        code.clear();
        return false;
    }
    if (code.empty() || code[0] != kSpirvMagic) {
        code.clear();
        return false;
    }
    // SPIR-V header validation: magic, version, generator, bound, reserved.
    // Header words: [0]=magic [1]=version [2]=generator [3]=bound [4]=reserved.
    if (code.size() < 5) {
        code.clear();
        return false;
    }
    const uint32_t version = code[1];
    const uint32_t major = (version >> 16) & 0xFFu;
    const uint32_t minor = (version >> 8) & 0xFFu;
    if (major == 0 || major > 1 || (major == 1 && minor > 6)) {
        code.clear();
        return false;
    }
    const uint32_t id_bound = code[3];
    if (id_bound == 0) {
        code.clear();
        return false;
    }
    if (code[4] != 0) {
        code.clear();
        return false;
    }
    return true;
}

uint32_t VulkanCompute::FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
#ifdef RAWR_ENABLE_VULKAN
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);
    uint32_t fallback_idx = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) == 0) {
            continue;
        }
        if (fallback_idx == UINT32_MAX) {
            fallback_idx = i;
        }
        if ((mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return fallback_idx;
#else
    // In CPU fallback mode, approximate Vulkan memory class selection from requested bits.
    // Keep behavior deterministic while honoring the provided type filter.
    if (type_filter == 0) {
        return UINT32_MAX;
    }
    const bool prefer_device_local = (properties & 0x1u) != 0u;
    if (prefer_device_local) {
        for (int i = 31; i >= 0; --i) {
            if ((type_filter & (1u << static_cast<uint32_t>(i))) != 0) {
                return static_cast<uint32_t>(i);
            }
        }
    }
    for (uint32_t i = 0; i < 32; ++i) {
        if ((type_filter & (1u << i)) != 0) {
            return i;
        }
    }
    return UINT32_MAX;
#endif
}

void VulkanCompute::InitializeCommandBufferPool(uint32_t pool_size) {
    if (pool_size == 0) {
        pool_size = 1;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    for (const auto& entry : command_buffer_pool_) {
        release_command_buffer_handle_locked(entry.buffer);
    }
    command_buffer_pool_.clear();
    while (!available_buffer_indices_.empty()) {
        available_buffer_indices_.pop();
    }

    command_buffer_pool_.reserve(pool_size);
    for (uint32_t i = 0; i < pool_size; ++i) {
        AsyncCommandBuffer pool_entry{};
        uint8_t* token = new (std::nothrow) uint8_t(0);
        if (!token) {
            break;
        }
        pool_entry.buffer = reinterpret_cast<VkCommandBuffer>(token);
        pool_entry.fence = nullptr;
        pool_entry.is_available = true;
        command_buffer_pool_.push_back(pool_entry);
        g_async_command_submitted[pool_entry.buffer] = false;
        g_async_command_completed[pool_entry.buffer] = false;
        g_command_pool_index[pool_entry.buffer] = static_cast<size_t>(i);
        available_buffer_indices_.push(static_cast<size_t>(i));
    }

    if (command_buffer_pool_.empty()) {
        command_buffer_pool_.shrink_to_fit();
    }
}

void VulkanCompute::CleanupCommandBufferPool() {
    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    std::vector<VkCommandBuffer> all_handles;
    all_handles.reserve(g_async_command_submitted.size());
    for (const auto& it : g_async_command_submitted) {
        all_handles.push_back(it.first);
    }
    for (VkCommandBuffer handle : all_handles) {
        release_command_buffer_handle_locked(handle);
    }
    command_buffer_pool_.clear();
    command_buffer_pool_.shrink_to_fit();
    g_async_command_submitted.clear();
    g_async_command_completed.clear();
    g_command_pool_index.clear();
    while (!available_buffer_indices_.empty()) {
        available_buffer_indices_.pop();
    }
}

bool VulkanCompute::CopyHostToBufferOffset(const void* host_data, VkBuffer device_buffer, size_t offset, size_t size) {
    if (!host_data || !device_buffer || size == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    auto it = g_cpu_buffers.find(device_buffer);
    if (it == g_cpu_buffers.end()) {
        return false;
    }
    if (offset > it->second.size() || size > (it->second.size() - offset)) {
        return false;
    }
    if (device_buffer == staging_buffer_ && g_staging_buffer_size != 0 &&
        (offset + size > g_staging_buffer_size)) {
        return false;
    }
    std::memmove(it->second.data() + offset, host_data, size);
    if ((offset % sizeof(float)) == 0 && (size % sizeof(float)) == 0) {
        const size_t float_offset = offset / sizeof(float);
        const size_t float_count = size / sizeof(float);
        for (auto& tensor : g_tensors) {
            if (tensor.device_buffer == device_buffer &&
                float_offset <= tensor.host_data.size() &&
                float_count <= (tensor.host_data.size() - float_offset)) {
                std::memcpy(tensor.host_data.data() + float_offset, host_data, size);
            }
        }
    }
    return true;
}

bool VulkanCompute::CopyBufferToHostOffset(VkBuffer device_buffer, size_t offset, void* host_data, size_t size) {
    if (!host_data || !device_buffer || size == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
    auto it = g_cpu_buffers.find(device_buffer);
    if (it == g_cpu_buffers.end()) {
        return false;
    }
    if (offset > it->second.size() || size > (it->second.size() - offset)) {
        return false;
    }
    if (device_buffer == staging_buffer_ && g_staging_buffer_size != 0 &&
        (offset + size > g_staging_buffer_size)) {
        return false;
    }
    if ((offset % sizeof(float)) == 0 && (size % sizeof(float)) == 0) {
        const size_t float_offset = offset / sizeof(float);
        const size_t float_count = size / sizeof(float);
        for (auto& tensor : g_tensors) {
            if (tensor.device_buffer == device_buffer &&
                float_offset <= tensor.host_data.size() &&
                float_count <= (tensor.host_data.size() - float_offset)) {
                std::memcpy(tensor.host_data.data() + float_offset, it->second.data() + offset, size);
            }
        }
    }
    std::memmove(host_data, it->second.data() + offset, size);
    return true;
}

bool VulkanCompute::CreateStagingBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory) {
    if (size == 0) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        auto it = g_cpu_buffers.find(staging_buffer_);
        auto mem_it = g_cpu_memory_blocks.find(staging_memory_);
        if (staging_buffer_ && staging_memory_ &&
            it != g_cpu_buffers.end() && mem_it != g_cpu_memory_blocks.end() &&
            it->second.size() >= size) {
            buffer = staging_buffer_;
            memory = staging_memory_;
            g_staging_buffer_size = it->second.size();
            return true;
        }
    }
    if (staging_buffer_ || staging_memory_) {
        const VkBuffer old_staging_buffer = staging_buffer_;
        const VkDeviceMemory old_staging_memory = staging_memory_;
        std::lock_guard<std::mutex> lock(g_cpu_buffer_mutex);
        if (staging_buffer_) {
            for (auto& set_entry : g_descriptor_bindings) {
                for (auto it = set_entry.second.begin(); it != set_entry.second.end();) {
                    if (it->second.first == staging_buffer_) {
                        it = set_entry.second.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            auto it = g_cpu_buffers.find(staging_buffer_);
            if (it != g_cpu_buffers.end()) {
                delete reinterpret_cast<uint8_t*>(it->first);
                g_cpu_buffers.erase(it);
            }
            staging_buffer_ = nullptr;
        }
        if (staging_memory_) {
            auto it = g_cpu_memory_blocks.find(staging_memory_);
            if (it != g_cpu_memory_blocks.end()) {
                delete reinterpret_cast<uint8_t*>(it->first);
                g_cpu_memory_blocks.erase(it);
            }
            staging_memory_ = nullptr;
        }
        allocated_buffers_.erase(
            std::remove_if(allocated_buffers_.begin(), allocated_buffers_.end(),
                [old_staging_buffer, old_staging_memory](const std::pair<VkBuffer, VkDeviceMemory>& p) {
                    return p.first == old_staging_buffer || p.second == old_staging_memory;
                }),
            allocated_buffers_.end());
        g_staging_buffer_size = 0;
    }

    if (!AllocateBuffer(size, buffer, memory)) {
        return false;
    }
    allocated_buffers_.erase(
        std::remove_if(allocated_buffers_.begin(), allocated_buffers_.end(),
            [buffer, memory](const std::pair<VkBuffer, VkDeviceMemory>& p) {
                return p.first == buffer || p.second == memory;
            }),
        allocated_buffers_.end());
    staging_buffer_ = buffer;
    staging_memory_ = memory;
    g_staging_buffer_size = size;
    return true;
}
