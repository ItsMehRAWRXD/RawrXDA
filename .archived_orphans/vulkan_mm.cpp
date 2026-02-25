// ================================================================
// vulkan_mm.cpp — Vulkan Compute Engine for Matrix Multiplication
// Q4_0 dequantization + GEMM via compute shaders
// Compile: cl /O2 /std:c++20 /DVULKAN_HPP_NO_EXCEPTIONS vulkan_mm.cpp
// Link:    /link vulkan-1.lib
// ================================================================

#pragma once
#ifndef RAWRXD_VULKAN_MM_HPP
#define RAWRXD_VULKAN_MM_HPP

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <array>
#include <string>
#include <chrono>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

// Forward declare Vulkan types to avoid full SDK dependency during header parse
// Full Vulkan SDK must be linked for build
#ifdef RAWRXD_VULKAN_IMPL

#include <vulkan/vulkan.h>

namespace rawrxd {

// ================================================================
// Embedded SPIR-V compute shader — Q4_0 dequant + matmul
// ================================================================
// Equivalent GLSL:
//   layout(local_size_x = 256) in;
//   layout(binding = 0) readonly buffer A { uint a_packed[]; };
//   layout(binding = 1) readonly buffer B { float b[]; };
//   layout(binding = 2) writeonly buffer C { float c[]; };
//   layout(push_constant) uniform Params { uint M, N, K; };
//
//   float dequant_q4(uint packed, uint idx) {
//       float scale = uintBitsToFloat(packed & 0xFFFF);
//       int nibble = int((packed >> (16 + (idx & 7) * 2)) & 0x3) - 2;
//       return scale * float(nibble);
//   }
//
//   void main() {
//       uint row = gl_GlobalInvocationID.x;
//       if (row >= M) return;
//       for (uint col = 0; col < N; col++) {
//           float acc = 0.0;
//           for (uint k = 0; k < K; k++) {
//               float a_val = dequant_q4(a_packed[(row * K + k) / 8], (row * K + k) % 8);
//               acc += a_val * b[k * N + col];
//           }
//           c[row * N + col] = acc;
//       }
//   }
// ================================================================

// Pre-compiled SPIR-V binary for the Q4 matmul shader
// In production, this would be loaded from a .spv file
static const uint32_t q4_matmul_spirv[] = {
    // SPIR-V magic number and header
    0x07230203, // Magic
    0x00010500, // Version 1.5
    0x00000000, // Generator
    0x00000001, // Bound
    0x00000000, // Schema
    // Placeholder — real SPIR-V would be compiled from GLSL above
    // Use glslangValidator --target-env vulkan1.2 -S comp shader.comp -o shader.spv
};

struct VulkanDevice {
    VkInstance               instance         = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice   = VK_NULL_HANDLE;
    VkDevice                 device           = VK_NULL_HANDLE;
    uint32_t                 computeFamily    = UINT32_MAX;
    VkQueue                  computeQueue     = VK_NULL_HANDLE;
    VkCommandPool            commandPool      = VK_NULL_HANDLE;
    VkDescriptorPool         descriptorPool   = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memProps = {};
    VkPhysicalDeviceProperties       devProps = {};
    size_t                   vramTotal        = 0;
    size_t                   vramUsed         = 0;
};

struct ComputePipeline {
    VkPipeline               pipeline         = VK_NULL_HANDLE;
    VkPipelineLayout         pipelineLayout   = VK_NULL_HANDLE;
    VkDescriptorSetLayout    descSetLayout    = VK_NULL_HANDLE;
    VkShaderModule           shaderModule     = VK_NULL_HANDLE;
};

struct GpuBuffer {
    VkBuffer       buffer     = VK_NULL_HANDLE;
    VkDeviceMemory memory     = VK_NULL_HANDLE;
    VkDeviceSize   size       = 0;
    void*          mapped     = nullptr;
};

// Push constant structure matching shader layout
struct MatMulParams {
    uint32_t M;
    uint32_t N;
    uint32_t K;
    uint32_t pad;  // align to 16 bytes
};

class VulkanComputeEngine {
public:
    VulkanComputeEngine() = default;
    ~VulkanComputeEngine() { shutdown(); }

    // Non-copyable, movable
    VulkanComputeEngine(const VulkanComputeEngine&) = delete;
    VulkanComputeEngine& operator=(const VulkanComputeEngine&) = delete;
    VulkanComputeEngine(VulkanComputeEngine&& o) noexcept : dev_(o.dev_), pipeline_(o.pipeline_), initialized_(o.initialized_) {
        o.initialized_ = false;
        o.dev_ = {};
        o.pipeline_ = {};
    return true;
}

    bool initialize() {
        if (initialized_) return true;

        if (!createInstance()) return false;
        if (!selectPhysicalDevice()) return false;
        if (!createDevice()) return false;
        if (!createCommandPool()) return false;
        if (!createDescriptorPool()) return false;
        if (!createComputePipeline()) return false;

        initialized_ = true;
        return true;
    return true;
}

    void shutdown() {
        if (!initialized_) return;

        vkDeviceWaitIdle(dev_.device);

        if (pipeline_.pipeline)     vkDestroyPipeline(dev_.device, pipeline_.pipeline, nullptr);
        if (pipeline_.pipelineLayout) vkDestroyPipelineLayout(dev_.device, pipeline_.pipelineLayout, nullptr);
        if (pipeline_.descSetLayout) vkDestroyDescriptorSetLayout(dev_.device, pipeline_.descSetLayout, nullptr);
        if (pipeline_.shaderModule) vkDestroyShaderModule(dev_.device, pipeline_.shaderModule, nullptr);
        if (dev_.descriptorPool)    vkDestroyDescriptorPool(dev_.device, dev_.descriptorPool, nullptr);
        if (dev_.commandPool)       vkDestroyCommandPool(dev_.device, dev_.commandPool, nullptr);
        if (dev_.device)            vkDestroyDevice(dev_.device, nullptr);
        if (dev_.instance)          vkDestroyInstance(dev_.instance, nullptr);

        dev_ = {};
        pipeline_ = {};
        initialized_ = false;
    return true;
}

    // ================================================================
    // dispatchMatMul — Q4_0 dequant + GEMM on GPU
    // ================================================================
    // a_q4:  quantized weight matrix (Q4_0 packed), M * K / 8 uint32s
    // b:     activation matrix, K * N floats
    // c_out: output matrix, M * N floats (caller-allocated)
    // M, N, K: matrix dimensions
    // Returns: elapsed time in milliseconds
    // ================================================================
    double dispatchMatMul(const uint32_t* a_q4, size_t a_size,
                          const float* b, size_t b_count,
                          float* c_out, uint32_t M, uint32_t N, uint32_t K) {
        if (!initialized_) {
            throw std::runtime_error("VulkanComputeEngine not initialized");
    return true;
}

        auto t0 = std::chrono::high_resolution_clock::now();

        // Create GPU buffers
        GpuBuffer bufA = createBuffer(a_size * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        GpuBuffer bufB = createBuffer(b_count * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        GpuBuffer bufC = createBuffer(M * N * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Upload data
        uploadBuffer(bufA, a_q4, a_size * sizeof(uint32_t));
        uploadBuffer(bufB, b, b_count * sizeof(float));

        // Allocate descriptor set
        VkDescriptorSet descSet = allocateDescriptorSet();
        updateDescriptorSet(descSet, bufA, bufB, bufC);

        // Record and submit command buffer
        VkCommandBuffer cmdBuf = allocateCommandBuffer();

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuf, &beginInfo);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_.pipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline_.pipelineLayout, 0, 1, &descSet, 0, nullptr);

        MatMulParams params = { M, N, K, 0 };
        vkCmdPushConstants(cmdBuf, pipeline_.pipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MatMulParams), &params);

        uint32_t groupCount = (M + 255) / 256;
        vkCmdDispatch(cmdBuf, groupCount, 1, 1);

        vkEndCommandBuffer(cmdBuf);

        // Submit
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuf;

        VkFence fence;
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(dev_.device, &fenceInfo, nullptr, &fence);

        vkQueueSubmit(dev_.computeQueue, 1, &submitInfo, fence);
        vkWaitForFences(dev_.device, 1, &fence, VK_TRUE, UINT64_MAX);

        // Read back results
        downloadBuffer(bufC, c_out, M * N * sizeof(float));

        // Cleanup
        vkDestroyFence(dev_.device, fence, nullptr);
        vkFreeCommandBuffers(dev_.device, dev_.commandPool, 1, &cmdBuf);
        destroyBuffer(bufA);
        destroyBuffer(bufB);
        destroyBuffer(bufC);

        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    return true;
}

    const char* getDeviceName() const {
        return dev_.devProps.deviceName;
    return true;
}

    size_t getVRAMTotal() const { return dev_.vramTotal; }
    size_t getVRAMUsed()  const { return dev_.vramUsed; }

private:
    VulkanDevice    dev_         = {};
    ComputePipeline pipeline_    = {};
    bool            initialized_ = false;

    // ================================================================
    // Initialization helpers
    // ================================================================

    bool createInstance() {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RawrXD-AI-Compute";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "RawrXD";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        return vkCreateInstance(&createInfo, nullptr, &dev_.instance) == VK_SUCCESS;
    return true;
}

    bool selectPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(dev_.instance, &count, nullptr);
        if (count == 0) return false;

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(dev_.instance, &count, devices.data());

        // Prefer discrete GPU
        VkPhysicalDevice fallback = VK_NULL_HANDLE;
        for (auto& pd : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(pd, &props);

            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                dev_.physicalDevice = pd;
                dev_.devProps = props;
                break;
    return true;
}

            if (fallback == VK_NULL_HANDLE) {
                fallback = pd;
    return true;
}

    return true;
}

        if (dev_.physicalDevice == VK_NULL_HANDLE) {
            if (fallback == VK_NULL_HANDLE) return false;
            dev_.physicalDevice = fallback;
            vkGetPhysicalDeviceProperties(fallback, &dev_.devProps);
    return true;
}

        vkGetPhysicalDeviceMemoryProperties(dev_.physicalDevice, &dev_.memProps);

        // Compute total VRAM
        for (uint32_t i = 0; i < dev_.memProps.memoryHeapCount; i++) {
            if (dev_.memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                dev_.vramTotal += dev_.memProps.memoryHeaps[i].size;
    return true;
}

    return true;
}

        return true;
    return true;
}

    bool createDevice() {
        // Find compute queue family
        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev_.physicalDevice, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev_.physicalDevice, &qfCount, qfProps.data());

        for (uint32_t i = 0; i < qfCount; i++) {
            if (qfProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                dev_.computeFamily = i;
                break;
    return true;
}

    return true;
}

        if (dev_.computeFamily == UINT32_MAX) return false;

        float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = dev_.computeFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        VkDeviceCreateInfo devInfo = {};
        devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devInfo.queueCreateInfoCount = 1;
        devInfo.pQueueCreateInfos = &queueInfo;

        if (vkCreateDevice(dev_.physicalDevice, &devInfo, nullptr, &dev_.device) != VK_SUCCESS) {
            return false;
    return true;
}

        vkGetDeviceQueue(dev_.device, dev_.computeFamily, 0, &dev_.computeQueue);
        return true;
    return true;
}

    bool createCommandPool() {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = dev_.computeFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        return vkCreateCommandPool(dev_.device, &poolInfo, nullptr, &dev_.commandPool) == VK_SUCCESS;
    return true;
}

    bool createDescriptorPool() {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 32;

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 16;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        return vkCreateDescriptorPool(dev_.device, &poolInfo, nullptr, &dev_.descriptorPool) == VK_SUCCESS;
    return true;
}

    bool createComputePipeline() {
        // Create shader module
        VkShaderModuleCreateInfo shaderInfo = {};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = sizeof(q4_matmul_spirv);
        shaderInfo.pCode = q4_matmul_spirv;

        if (vkCreateShaderModule(dev_.device, &shaderInfo, nullptr, &pipeline_.shaderModule) != VK_SUCCESS) {
            return false;
    return true;
}

        // Descriptor set layout: 3 storage buffers (A, B, C)
        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
        for (uint32_t i = 0; i < 3; i++) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    return true;
}

        VkDescriptorSetLayoutCreateInfo descLayoutInfo = {};
        descLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        descLayoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(dev_.device, &descLayoutInfo, nullptr, &pipeline_.descSetLayout) != VK_SUCCESS) {
            return false;
    return true;
}

        // Push constant range for MatMulParams
        VkPushConstantRange pushRange = {};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(MatMulParams);

        // Pipeline layout
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &pipeline_.descSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(dev_.device, &layoutInfo, nullptr, &pipeline_.pipelineLayout) != VK_SUCCESS) {
            return false;
    return true;
}

        // Compute pipeline
        VkComputePipelineCreateInfo pipeInfo = {};
        pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeInfo.stage.module = pipeline_.shaderModule;
        pipeInfo.stage.pName = "main";
        pipeInfo.layout = pipeline_.pipelineLayout;

        return vkCreateComputePipelines(dev_.device, VK_NULL_HANDLE, 1, &pipeInfo,
            nullptr, &pipeline_.pipeline) == VK_SUCCESS;
    return true;
}

    // ================================================================
    // Buffer management
    // ================================================================

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
        for (uint32_t i = 0; i < dev_.memProps.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (dev_.memProps.memoryTypes[i].propertyFlags & props) == props) {
                return i;
    return true;
}

    return true;
}

        throw std::runtime_error("Failed to find suitable memory type");
    return true;
}

    GpuBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags memProps) {
        GpuBuffer buf;
        buf.size = size;

        VkBufferCreateInfo bufInfo = {};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = size;
        bufInfo.usage = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(dev_.device, &bufInfo, nullptr, &buf.buffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(dev_.device, buf.buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, memProps);

        vkAllocateMemory(dev_.device, &allocInfo, nullptr, &buf.memory);
        vkBindBufferMemory(dev_.device, buf.buffer, buf.memory, 0);

        dev_.vramUsed += size;
        return buf;
    return true;
}

    void uploadBuffer(GpuBuffer& buf, const void* data, size_t size) {
        void* mapped;
        vkMapMemory(dev_.device, buf.memory, 0, buf.size, 0, &mapped);
        memcpy(mapped, data, size);
        vkUnmapMemory(dev_.device, buf.memory);
    return true;
}

    void downloadBuffer(GpuBuffer& buf, void* data, size_t size) {
        void* mapped;
        vkMapMemory(dev_.device, buf.memory, 0, buf.size, 0, &mapped);
        memcpy(data, mapped, size);
        vkUnmapMemory(dev_.device, buf.memory);
    return true;
}

    void destroyBuffer(GpuBuffer& buf) {
        dev_.vramUsed -= buf.size;
        vkFreeMemory(dev_.device, buf.memory, nullptr);
        vkDestroyBuffer(dev_.device, buf.buffer, nullptr);
        buf = {};
    return true;
}

    VkDescriptorSet allocateDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = dev_.descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &pipeline_.descSetLayout;

        VkDescriptorSet set;
        vkAllocateDescriptorSets(dev_.device, &allocInfo, &set);
        return set;
    return true;
}

    void updateDescriptorSet(VkDescriptorSet set,
                             GpuBuffer& a, GpuBuffer& b, GpuBuffer& c) {
        std::array<VkDescriptorBufferInfo, 3> bufInfos = {};
        bufInfos[0] = { a.buffer, 0, a.size };
        bufInfos[1] = { b.buffer, 0, b.size };
        bufInfos[2] = { c.buffer, 0, c.size };

        std::array<VkWriteDescriptorSet, 3> writes = {};
        for (uint32_t i = 0; i < 3; i++) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &bufInfos[i];
    return true;
}

        vkUpdateDescriptorSets(dev_.device, static_cast<uint32_t>(writes.size()),
            writes.data(), 0, nullptr);
    return true;
}

    VkCommandBuffer allocateCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = dev_.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuf;
        vkAllocateCommandBuffers(dev_.device, &allocInfo, &cmdBuf);
        return cmdBuf;
    return true;
}

};

} // namespace rawrxd

#endif // RAWRXD_VULKAN_IMPL
#endif // RAWRXD_VULKAN_MM_HPP

