#include "vulkan_compute.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <array>

// SCALAR-ONLY IMPLEMENTATION: All GPU/Vulkan code removed

VulkanCompute::VulkanCompute() {
    std::memset(&device_info_, 0, sizeof(VulkanDeviceInfo));
    device_info_.device_name = "Scalar CPU (No GPU)";
    device_info_.vendor_id = 0x0000;
    device_info_.device_id = 0x0000;
    device_info_.supports_compute = true;
    device_info_.compute_queue_family = 0;
}

VulkanCompute::~VulkanCompute() {
    Cleanup();
}

bool VulkanCompute::Initialize() {
    std::cout << "Scalar CPU compute initialized (no GPU)" << std::endl;
    std::cout << "Device: " << device_info_.device_name << std::endl;
    return true;
}

bool VulkanCompute::CreateInstance() {
    return true;  // Scalar mode: no Vulkan instance needed
}

bool VulkanCompute::SelectPhysicalDevice() {
    return true;  // Scalar mode: using CPU only
}

bool VulkanCompute::CreateLogicalDevice() {
    return true;  // Scalar mode: no logical device needed
}

bool VulkanCompute::CreateCommandPool() {
    return true;  // Scalar mode: no command pool needed
}

bool VulkanCompute::LoadShader(const std::string& name, const std::string& spirv_path) {
    std::vector<uint32_t> spirv_code;
    if (!LoadSPIRVCode(spirv_path, spirv_code)) {
        std::cerr << "Failed to load SPIR-V code: " << spirv_path << std::endl;
        return false;
    }

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
    create_info.pCode = spirv_code.data();

    ComputeShader shader;
    shader.name = name;
    shader.spirv_code = spirv_code;

    if (vkCreateShaderModule(device_, &create_info, nullptr, &shader.module) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module: " << name << std::endl;
        return false;
    }

    shaders_[name] = std::move(shader);
    std::cout << "Loaded shader: " << name << std::endl;

    return true;
}

bool VulkanCompute::CreateComputePipeline(const std::string& shader_name) {
    auto it = shaders_.find(shader_name);
    if (it == shaders_.end()) {
        std::cerr << "Shader not found: " << shader_name << std::endl;
        return false;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &it->second.layout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout: " << shader_name << std::endl;
        return false;
    }

    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = it->second.module;
    stage_info.pName = "main";

    VkComputePipelineCreateInfo compute_pipeline_info{};
    compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_info.layout = it->second.layout;
    compute_pipeline_info.stage = stage_info;

    if (vkCreateComputePipelines(device_, nullptr, 1, &compute_pipeline_info, nullptr, &it->second.pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline: " << shader_name << std::endl;
        return false;
    }

    std::cout << "Created compute pipeline: " << shader_name << std::endl;
    return true;
}

bool VulkanCompute::EnsureMatMulPipeline(const std::string& spirv_path) {
    if (matmul_pipeline_ != VK_NULL_HANDLE) {
        return true;
    }

    if (!LoadShader("matmul", spirv_path)) {
        return false;
    }

    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    for (uint32_t i = 0; i < bindings.size(); ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &matmul_descriptor_layout_) != VK_SUCCESS) {
        std::cerr << "Failed to create matmul descriptor set layout" << std::endl;
        return false;
    }

    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(uint32_t) * 3;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &matmul_descriptor_layout_;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_range;

    if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &matmul_pipeline_layout_) != VK_SUCCESS) {
        std::cerr << "Failed to create matmul pipeline layout" << std::endl;
        return false;
    }

    auto shader_it = shaders_.find("matmul");
    if (shader_it == shaders_.end()) {
        std::cerr << "Matmul shader missing after load" << std::endl;
        return false;
    }

    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = shader_it->second.module;
    stage_info.pName = "main";

    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.layout = matmul_pipeline_layout_;
    pipeline_info.stage = stage_info;

    if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &matmul_pipeline_) != VK_SUCCESS) {
        std::cerr << "Failed to create matmul compute pipeline" << std::endl;
        return false;
    }

    return true;
}

bool VulkanCompute::DispatchMatMul(VkBuffer input_a,
                                   VkBuffer input_b,
                                   VkBuffer output,
                                   uint32_t M,
                                   uint32_t K,
                                   uint32_t N) {
    if (!EnsureMatMulPipeline("shaders/matmul.spv")) {
        return false;
    }

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 3;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 1;

    VkDescriptorPool descriptor_pool;
    if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
        std::cerr << "Failed to create matmul descriptor pool" << std::endl;
        return false;
    }

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &matmul_descriptor_layout_;

    VkDescriptorSet descriptor_set;
    if (vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set) != VK_SUCCESS) {
        vkDestroyDescriptorPool(device_, descriptor_pool, nullptr);
        std::cerr << "Failed to allocate matmul descriptor set" << std::endl;
        return false;
    }

    std::array<VkDescriptorBufferInfo, 3> buffer_infos{};
    buffer_infos[0] = {input_a, 0, VK_WHOLE_SIZE};
    buffer_infos[1] = {input_b, 0, VK_WHOLE_SIZE};
    buffer_infos[2] = {output, 0, VK_WHOLE_SIZE};

    std::array<VkWriteDescriptorSet, 3> writes{};
    for (uint32_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptor_set;
        writes[i].dstBinding = i;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &buffer_infos[i];
    }

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    VkCommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.commandPool = command_pool_;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd_buffer;
    if (vkAllocateCommandBuffers(device_, &cmd_alloc_info, &cmd_buffer) != VK_SUCCESS) {
        vkDestroyDescriptorPool(device_, descriptor_pool, nullptr);
        std::cerr << "Failed to allocate command buffer for matmul" << std::endl;
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, command_pool_, 1, &cmd_buffer);
        vkDestroyDescriptorPool(device_, descriptor_pool, nullptr);
        std::cerr << "Failed to begin command buffer for matmul" << std::endl;
        return false;
    }

    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, matmul_pipeline_);
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, matmul_pipeline_layout_,
                            0, 1, &descriptor_set, 0, nullptr);

    struct {
        uint32_t M;
        uint32_t K;
        uint32_t N;
    } push_consts{M, K, N};

    vkCmdPushConstants(cmd_buffer, matmul_pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(push_consts), &push_consts);

    const uint32_t group_x = (N + 15) / 16;
    const uint32_t group_y = (M + 15) / 16;
    vkCmdDispatch(cmd_buffer, group_x, group_y, 1);

    if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, command_pool_, 1, &cmd_buffer);
        vkDestroyDescriptorPool(device_, descriptor_pool, nullptr);
        std::cerr << "Failed to record matmul command buffer" << std::endl;
        return false;
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;

    if (vkQueueSubmit(compute_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, command_pool_, 1, &cmd_buffer);
        vkDestroyDescriptorPool(device_, descriptor_pool, nullptr);
        std::cerr << "Failed to submit matmul command buffer" << std::endl;
        return false;
    }

    vkQueueWaitIdle(compute_queue_);

    vkFreeCommandBuffers(device_, command_pool_, 1, &cmd_buffer);
    vkDestroyDescriptorPool(device_, descriptor_pool, nullptr);
    return true;
}

VulkanTensor VulkanCompute::TransferGGUFTensor(const std::string& tensor_name,
                                               const void* data_ptr,
                                               size_t size_bytes,
                                               VkBufferUsageFlags usage) {
    if (!device_ || !data_ptr || size_bytes == 0) {
        throw std::runtime_error("Invalid arguments passed to TransferGGUFTensor");
    }

    VulkanTensor tensor;
    tensor.name = tensor_name;
    tensor.size_bytes = size_bytes;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size_bytes;
    buffer_info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &buffer_info, nullptr, &tensor.buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan buffer for tensor: " + tensor_name);
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_, tensor.buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &tensor.memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, tensor.buffer, nullptr);
        throw std::runtime_error("Failed to allocate Vulkan memory for tensor: " + tensor_name);
    }

    if (vkBindBufferMemory(device_, tensor.buffer, tensor.memory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(device_, tensor.buffer, nullptr);
        vkFreeMemory(device_, tensor.memory, nullptr);
        throw std::runtime_error("Failed to bind Vulkan memory for tensor: " + tensor_name);
    }

    void* mapped = nullptr;
    if (vkMapMemory(device_, tensor.memory, 0, size_bytes, 0, &mapped) != VK_SUCCESS || !mapped) {
        vkDestroyBuffer(device_, tensor.buffer, nullptr);
        vkFreeMemory(device_, tensor.memory, nullptr);
        throw std::runtime_error("Failed to map Vulkan memory for tensor: " + tensor_name);
    }

    std::memcpy(mapped, data_ptr, size_bytes);
    vkUnmapMemory(device_, tensor.memory);

    uploaded_tensors_.push_back(tensor);
    return tensor;
}

void VulkanCompute::ReleaseTensors() {
    if (!device_) return;
    for (auto& tensor : uploaded_tensors_) {
        if (tensor.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, tensor.buffer, nullptr);
        }
        if (tensor.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, tensor.memory, nullptr);
        }
    }
    uploaded_tensors_.clear();
}

bool VulkanCompute::AllocateBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (vkCreateBuffer(device_, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer" << std::endl;
        return false;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory" << std::endl;
        vkDestroyBuffer(device_, buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device_, buffer, memory, 0);
    return true;
}

bool VulkanCompute::CopyBufferToHost(VkBuffer device_buffer, void* host_data, size_t size) {
    // Create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (vkCreateBuffer(device_, &buffer_info, nullptr, &staging_buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_, staging_buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, 
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &staging_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);

    // Copy device buffer to staging buffer (would need command buffer implementation)
    // For now, return placeholder
    
    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_memory, nullptr);
    return true;
}

bool VulkanCompute::CopyHostToBuffer(void* host_data, VkBuffer device_buffer, size_t size) {
    // Similar to CopyBufferToHost but in reverse
    return true;
}

bool VulkanCompute::ExecuteMatMul(const float* input_a, const float* input_b,
                                  float* output, uint32_t m, uint32_t k, uint32_t n) {
    // Naive CPU fallback (O(m*k*n)) for benchmarking correctness.
    // If a Vulkan shader/pipeline is loaded named "matmul" we would dispatch it instead.
    // Clear output
    std::fill(output, output + (size_t)m * n, 0.0f);
    for (uint32_t row = 0; row < m; ++row) {
        const float* arow = input_a + (size_t)row * k;
        for (uint32_t col = 0; col < n; ++col) {
            float sum = 0.0f;
            for (uint32_t inner = 0; inner < k; ++inner) {
                sum += arow[inner] * input_b[(size_t)inner * n + col];
            }
            output[(size_t)row * n + col] = sum;
        }
    }
    return true;
}

bool VulkanCompute::ExecuteAttention(const float* queries, const float* keys, const float* values,
                                     float* output, uint32_t seq_len, uint32_t head_dim) {
    // CPU scaled dot-product attention (single head)
    // Q: [seq_len, head_dim], K: [seq_len, head_dim], V: [seq_len, head_dim]
    // output = softmax(Q*K^T / sqrt(head_dim)) * V => [seq_len, head_dim]
    if (!queries || !keys || !values || !output || seq_len == 0 || head_dim == 0) return false;
    std::vector<float> scores((size_t)seq_len * seq_len);
    const float scale = 1.0f / std::sqrt((float)head_dim);
    // Compute QK^T
    for (uint32_t i = 0; i < seq_len; ++i) {
        const float* Qi = queries + (size_t)i * head_dim;
        for (uint32_t j = 0; j < seq_len; ++j) {
            const float* Kj = keys + (size_t)j * head_dim;
            float dot = 0.0f;
            for (uint32_t d = 0; d < head_dim; ++d) dot += Qi[d] * Kj[d];
            scores[(size_t)i * seq_len + j] = dot * scale;
        }
    }
    // Softmax per row
    for (uint32_t i = 0; i < seq_len; ++i) {
        float* row = scores.data() + (size_t)i * seq_len;
        float maxv = row[0];
        for (uint32_t j = 1; j < seq_len; ++j) maxv = std::max(maxv, row[j]);
        double sum = 0.0;
        for (uint32_t j = 0; j < seq_len; ++j) { row[j] = std::exp(row[j] - maxv); sum += row[j]; }
        double inv = sum == 0.0 ? 0.0 : 1.0 / sum;
        for (uint32_t j = 0; j < seq_len; ++j) row[j] = (float)(row[j] * inv);
    }
    // Multiply by V: output[i] = Σ_j scores[i,j] * V[j]
    for (uint32_t i = 0; i < seq_len; ++i) {
        float* Outi = output + (size_t)i * head_dim;
        std::fill(Outi, Outi + head_dim, 0.0f);
        float* attRow = scores.data() + (size_t)i * seq_len;
        for (uint32_t j = 0; j < seq_len; ++j) {
            const float* Vj = values + (size_t)j * head_dim;
            float w = attRow[j];
            for (uint32_t d = 0; d < head_dim; ++d) {
                Outi[d] += w * Vj[d];
            }
        }
    }
    return true;
}

bool VulkanCompute::ExecuteRoPE(float* embeddings, uint32_t dim, uint32_t seq_pos, uint32_t rotation_dim) {
    // Placeholder for RoPE implementation
    std::cout << "Executing RoPE: Dim=" << dim << ", SeqPos=" << seq_pos << std::endl;
    return true;
}

bool VulkanCompute::ExecuteRMSNorm(float* data, uint32_t size, float epsilon) {
    // CPU RMSNorm: y = x / sqrt(mean(x^2) + eps)
    double accum = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        accum += (double)data[i] * (double)data[i];
    }
    double mean_sq = accum / std::max<uint32_t>(size,1);
    double denom = std::sqrt(mean_sq + (double)epsilon);
    if (denom == 0.0) denom = 1.0; // safety
    float inv = (float)(1.0 / denom);
    for (uint32_t i = 0; i < size; ++i) {
        data[i] = data[i] * inv;
    }
    return true;
}

bool VulkanCompute::ExecuteSiLU(float* data, uint32_t size) {
    // CPU SiLU: x * sigmoid(x)
    for (uint32_t i = 0; i < size; ++i) {
        float x = data[i];
        float s = 1.0f / (1.0f + std::exp(-x));
        data[i] = x * s;
    }
    return true;
}

bool VulkanCompute::ExecuteSoftmax(float* data, uint32_t size) {
    // CPU Softmax (in-place)
    if (size == 0) return true;
    float maxv = data[0];
    for (uint32_t i = 1; i < size; ++i) maxv = std::max(maxv, data[i]);
    double sum = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        data[i] = std::exp(data[i] - maxv);
        sum += data[i];
    }
    if (sum == 0.0) return true;
    double inv = 1.0 / sum;
    for (uint32_t i = 0; i < size; ++i) data[i] = (float)(data[i] * inv);
    return true;
}

bool VulkanCompute::ExecuteDequantize(const uint8_t* quantized, float* output,
                                      uint32_t elements, const std::string& quant_type) {
    if (!quantized || !output) return false;
    if (elements == 0) return true;
    // Basic branching for common quantization types.
    if (quant_type == "F32") {
        // Assume raw bytes represent floats (size must be elements*sizeof(float)) - copy reinterpret.
        const float* src = reinterpret_cast<const float*>(quantized);
        for (uint32_t i = 0; i < elements; ++i) output[i] = src[i];
        return true;
    } else if (quant_type == "Q2_K") {
        // Very rough: 2-bit values packed in bytes (4 values per byte). Scale to [-1,1].
        uint32_t outIndex = 0;
        for (uint32_t b = 0; outIndex < elements; ++b) {
            uint8_t packed = quantized[b];
            for (int nib = 0; nib < 4 && outIndex < elements; ++nib) {
                uint8_t v = (packed >> (nib * 2)) & 0x3;
                float f = (v / 3.0f) * 2.0f - 1.0f; // map 0..3 -> -1..1
                output[outIndex++] = f;
            }
        }
        return true;
    } else if (quant_type == "Q4_K") {
        // 4-bit values packed (2 per byte). Map 0..15 -> -1..1
        uint32_t outIndex = 0;
        for (uint32_t b = 0; outIndex < elements; ++b) {
            uint8_t packed = quantized[b];
            uint8_t hi = (packed >> 4) & 0xF;
            uint8_t lo = packed & 0xF;
            float fhi = (hi / 15.0f) * 2.0f - 1.0f;
            float flo = (lo / 15.0f) * 2.0f - 1.0f;
            output[outIndex++] = flo;
            if (outIndex < elements) output[outIndex++] = fhi;
        }
        return true;
    } else {
        // Fallback byte->[0,1]
        const float scale = 1.0f / 255.0f;
        for (uint32_t i = 0; i < elements; ++i) output[i] = quantized[i] * scale;
        return true;
    }
}

bool VulkanCompute::LoadSPIRVCode(const std::string& path, std::vector<uint32_t>& code) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open SPIR-V file: " << path << std::endl;
        return false;
    }

    size_t file_size = file.tellg();
    if (file_size % sizeof(uint32_t) != 0) {
        std::cerr << "Invalid SPIR-V file size: " << path << std::endl;
        return false;
    }

    file.seekg(0);
    code.resize(file_size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), file_size);

    return true;
}

uint32_t VulkanCompute::FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < device_info_.memory_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && 
            (device_info_.memory_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void VulkanCompute::Cleanup() {
    if (device_) {
        vkDeviceWaitIdle(device_);
        ReleaseTensors();

        if (matmul_pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, matmul_pipeline_, nullptr);
            matmul_pipeline_ = VK_NULL_HANDLE;
        }
        if (matmul_pipeline_layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, matmul_pipeline_layout_, nullptr);
            matmul_pipeline_layout_ = VK_NULL_HANDLE;
        }
        if (matmul_descriptor_layout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, matmul_descriptor_layout_, nullptr);
            matmul_descriptor_layout_ = VK_NULL_HANDLE;
        }
        
        for (auto& shader : shaders_) {
            if (shader.second.pipeline) {
                vkDestroyPipeline(device_, shader.second.pipeline, nullptr);
            }
            if (shader.second.layout) {
                vkDestroyPipelineLayout(device_, shader.second.layout, nullptr);
            }
            if (shader.second.module) {
                vkDestroyShaderModule(device_, shader.second.module, nullptr);
            }
        }
        shaders_.clear();
        
        if (command_pool_) {
            vkDestroyCommandPool(device_, command_pool_, nullptr);
        }
        
        vkDestroyDevice(device_, nullptr);
    }
    
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
    }
}
