// =============================================================================
// vulkan_compute_kernel_executor.cpp — Vulkan executeKernel() Implementation
// ==============================================================================
// Production GPU compute pipeline execution for AMD 7800 XT and compatible GPUs
// Features:
//   - Full compute pipeline setup
//   - Descriptor set binding
//   - Push constants support
//   - Async command buffer execution
//   - Result retrieval with memory barriers
//   - Performance profiling
//
// NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "../vulkan_compute.h"
#include <vulkan/vulkan.h>
#include <iostream>
#include <chrono>
#include <cstring>

// =============================================================================
// Kernel Execution Parameters
// =============================================================================

struct KernelExecuteParams {
    std::string shader_name;
    uint32_t workgroup_x;
    uint32_t workgroup_y;
    uint32_t workgroup_z;
    std::vector<uint32_t> buffer_indices;  // Buffer bindings
    std::vector<uint8_t> push_constants;   // Optional push constants
};

// =============================================================================
// VulkanCompute::executeKernel() — Full Implementation
// =============================================================================

bool VulkanCompute::executeKernel(
    const std::string& shader_name,
    uint32_t workgroup_x,
    uint32_t workgroup_y,
    uint32_t workgroup_z,
    const std::vector<uint32_t>& buffer_indices,
    const void* push_constants,
    size_t push_constants_size)
{
    if (!device_ || !compute_queue_) {
        std::cerr << "Vulkan device not initialized" << std::endl;
        return false;
    }

    // Find shader
    auto shader_it = shaders_.find(shader_name);
    if (shader_it == shaders_.end()) {
        std::cerr << "Shader not found: " << shader_name << std::endl;
        return false;
    }

    const ComputeShader& shader = shader_it->second;
    if (!shader.pipeline) {
        std::cerr << "Shader pipeline not created: " << shader_name << std::endl;
        return false;
    }

    // Validate buffers
    for (uint32_t idx : buffer_indices) {
        if (idx >= buffers_.size() || !buffers_[idx].device_buffer) {
            std::cerr << "Invalid buffer index: " << idx << std::endl;
            return false;
        }
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // 1. Acquire async command buffer
    VkCommandBuffer cmd = AcquireAsyncCommandBuffer();
    if (!cmd) {
        std::cerr << "Failed to acquire command buffer" << std::endl;
        return false;
    }

    // 2. Begin command buffer
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
        std::cerr << "Failed to begin command buffer" << std::endl;
        return false;
    }

    // 3. Create descriptor set for this dispatch
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    
    if (!buffer_indices.empty()) {
        // Allocate descriptor set
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &descriptor_set_layout_;

        if (vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set) != VK_SUCCESS) {
            std::cerr << "Failed to allocate descriptor set" << std::endl;
            vkEndCommandBuffer(cmd);
            return false;
        }

        // Update descriptor set with buffers
        std::vector<VkDescriptorBufferInfo> buffer_infos;
        std::vector<VkWriteDescriptorSet> write_sets;

        for (size_t i = 0; i < buffer_indices.size(); ++i) {
            uint32_t buf_idx = buffer_indices[i];
            const VulkanTensor& tensor = buffers_[buf_idx];

            VkDescriptorBufferInfo buf_info{};
            buf_info.buffer = tensor.device_buffer;
            buf_info.offset = 0;
            buf_info.range = tensor.size_bytes;
            buffer_infos.push_back(buf_info);

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptor_set;
            write.dstBinding = static_cast<uint32_t>(i);
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo = &buffer_infos[i];
            write_sets.push_back(write);
        }

        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(write_sets.size()),
                               write_sets.data(), 0, nullptr);

        // Bind descriptor set
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            shader.layout,
            0,
            1,
            &descriptor_set,
            0,
            nullptr
        );
    }

    // 4. Bind compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shader.pipeline);

    // 5. Push constants (if provided)
    if (push_constants && push_constants_size > 0) {
        vkCmdPushConstants(
            cmd,
            shader.layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            static_cast<uint32_t>(push_constants_size),
            push_constants
        );
    }

    // 6. Insert memory barriers before dispatch (ensure previous writes visible)
    if (!buffer_indices.empty()) {
        std::vector<VkBufferMemoryBarrier> barriers;
        for (uint32_t buf_idx : buffer_indices) {
            VkBufferMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = buffers_[buf_idx].device_buffer;
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            barriers.push_back(barrier);
        }

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, nullptr,
            static_cast<uint32_t>(barriers.size()), barriers.data(),
            0, nullptr
        );
    }

    // 7. Dispatch compute shader
    vkCmdDispatch(cmd, workgroup_x, workgroup_y, workgroup_z);

    // 8. Insert memory barriers after dispatch (ensure writes complete)
    if (!buffer_indices.empty()) {
        std::vector<VkBufferMemoryBarrier> barriers;
        for (uint32_t buf_idx : buffer_indices) {
            VkBufferMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = buffers_[buf_idx].device_buffer;
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            barriers.push_back(barrier);
        }

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            static_cast<uint32_t>(barriers.size()), barriers.data(),
            0, nullptr
        );
    }

    // 9. End command buffer
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        std::cerr << "Failed to end command buffer" << std::endl;
        return false;
    }

    // 10. Submit to compute queue
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    VkFence fence;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    if (vkCreateFence(device_, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        std::cerr << "Failed to create fence" << std::endl;
        return false;
    }

    if (vkQueueSubmit(compute_queue_, 1, &submit_info, fence) != VK_SUCCESS) {
        std::cerr << "Failed to submit compute queue" << std::endl;
        vkDestroyFence(device_, fence, nullptr);
        return false;
    }

    // 11. Wait for completion
    if (vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        std::cerr << "Failed to wait for fence" << std::endl;
        vkDestroyFence(device_, fence, nullptr);
        return false;
    }

    vkDestroyFence(device_, fence, nullptr);

    // Free descriptor set
    if (descriptor_set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, descriptor_pool_, 1, &descriptor_set);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    // Update stats
    stats_.dispatch_count.fetch_add(1);
    stats_.total_dispatch_time_us.fetch_add(duration);

    std::cout << "✅ Kernel executed: " << shader_name 
              << " [" << workgroup_x << "x" << workgroup_y << "x" << workgroup_z << "]"
              << " (" << (duration / 1000.0) << " ms)" << std::endl;

    return true;
}

// =============================================================================
// Specialized Kernel Execution Methods
// =============================================================================

bool VulkanCompute::executeMatMulKernel(
    uint32_t buf_a, uint32_t buf_b, uint32_t buf_out,
    uint32_t M, uint32_t K, uint32_t N)
{
    // Push constants for matrix dimensions
    struct MatMulParams {
        uint32_t M, K, N;
        uint32_t pad;
    } params = { M, K, N, 0 };

    // Calculate workgroup counts (assume 16x16 tile size)
    uint32_t wg_x = (N + 15) / 16;
    uint32_t wg_y = (M + 15) / 16;

    return executeKernel(
        "matmul",
        wg_x, wg_y, 1,
        {buf_a, buf_b, buf_out},
        &params,
        sizeof(params)
    );
}

bool VulkanCompute::executeFlashAttention(
    uint32_t buf_q, uint32_t buf_k, uint32_t buf_v, uint32_t buf_out,
    uint32_t seq_len, uint32_t head_dim, uint32_t num_heads)
{
    struct AttentionParams {
        uint32_t seq_len;
        uint32_t head_dim;
        uint32_t num_heads;
        float scale;
    } params = { 
        seq_len, 
        head_dim, 
        num_heads, 
        1.0f / sqrtf(static_cast<float>(head_dim))
    };

    // Workgroups: one per attention head
    return executeKernel(
        "flash_attention",
        num_heads, 1, 1,
        {buf_q, buf_k, buf_v, buf_out},
        &params,
        sizeof(params)
    );
}

bool VulkanCompute::executeRMSNorm(
    uint32_t buf_in, uint32_t buf_weight, uint32_t buf_out,
    uint32_t n_elements, float eps)
{
    struct RMSNormParams {
        uint32_t n_elements;
        float eps;
    } params = { n_elements, eps };

    uint32_t wg_count = (n_elements + 255) / 256;

    return executeKernel(
        "rmsnorm",
        wg_count, 1, 1,
        {buf_in, buf_weight, buf_out},
        &params,
        sizeof(params)
    );
}

} // anonymous namespace

// Add to VulkanCompute class implementation
namespace VulkanCompute {

bool executeKernel(const std::string& shader_name, 
                  uint32_t workgroup_x, uint32_t workgroup_y, uint32_t workgroup_z,
                  const std::vector<uint32_t>& buffer_indices) {
    return executeKernel(shader_name, workgroup_x, workgroup_y, workgroup_z, 
                        buffer_indices, nullptr, 0);
}

} // namespace VulkanCompute
