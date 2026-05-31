// gpu_dispatch.cpp
// Vulkan dispatch implementation for FP8 quantizer.
// Compile with: -lvulkan or vulkan-1.lib

#include "gpu_dispatch.hpp"
#include <vulkan/vulkan.h>
#include <windows.h>
#include <fstream>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace rawrxd::gpu {

struct Fp8QuantizerDispatch::Impl {
    VkInstance       instance{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice         device{VK_NULL_HANDLE};
    VkQueue          queue{VK_NULL_HANDLE};
    uint32_t         queueFamilyIndex{0};
    VkCommandPool    cmdPool{VK_NULL_HANDLE};
    VkCommandBuffer  cmdBuf{VK_NULL_HANDLE};
    VkFence          fence{VK_NULL_HANDLE};
    VkShaderModule   shaderModule{VK_NULL_HANDLE};
    VkPipeline       pipeline{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkDescriptorPool descPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
    VkDescriptorSet  descSet{VK_NULL_HANDLE};
    VkBuffer         srcBuf{VK_NULL_HANDLE};
    VkBuffer         dstBuf{VK_NULL_HANDLE};
    VkDeviceMemory   srcMem{VK_NULL_HANDLE};
    VkDeviceMemory   dstMem{VK_NULL_HANDLE};
    size_t           currentSize{0};
};

static std::vector<char> read_file(std::string_view path) {
    std::ifstream f(std::string(path), std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<char> buf(static_cast<size_t>(sz));
    f.read(buf.data(), sz);
    return buf;
}

bool Fp8QuantizerDispatch::initialize(std::string_view spv_path) {
    // Defensive: check Vulkan loader availability
    HMODULE vulkanDll = GetModuleHandleA("vulkan-1.dll");
    if (!vulkanDll) {
        return false; // Vulkan runtime not present
    }

    m_impl = new Impl();

    // 1. Instance
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &appInfo;
    if (vkCreateInstance(&ci, nullptr, &m_impl->instance) != VK_SUCCESS) {
        return false;
    }

    // 2. Physical device (first with compute queue)
    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(m_impl->instance, &devCount, nullptr);
    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(m_impl->instance, &devCount, devices.data());
    for (auto dev : devices) {
        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qfCount, qfs.data());
        for (uint32_t i = 0; i < qfCount; ++i) {
            if (qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                m_impl->physicalDevice = dev;
                m_impl->queueFamilyIndex = i;
                break;
            }
        }
        if (m_impl->physicalDevice != VK_NULL_HANDLE) break;
    }
    if (m_impl->physicalDevice == VK_NULL_HANDLE) return false;

    // 3. Logical device
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = m_impl->queueFamilyIndex;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    if (vkCreateDevice(m_impl->physicalDevice, &dci, nullptr, &m_impl->device) != VK_SUCCESS) {
        return false;
    }
    vkGetDeviceQueue(m_impl->device, m_impl->queueFamilyIndex, 0, &m_impl->queue);

    // 4. Command pool + buffer + fence
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = m_impl->queueFamilyIndex;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(m_impl->device, &cpci, nullptr, &m_impl->cmdPool);

    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = m_impl->cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_impl->device, &cbai, &m_impl->cmdBuf);

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(m_impl->device, &fci, nullptr, &m_impl->fence);

    // 5. Shader module
    auto spv = read_file(spv_path);
    if (spv.empty()) return false;
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spv.size();
    smci.pCode = reinterpret_cast<const uint32_t*>(spv.data());
    vkCreateShaderModule(m_impl->device, &smci, nullptr, &m_impl->shaderModule);

    // 6. Descriptor set layout
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dlci.bindingCount = 2;
    dlci.pBindings = bindings;
    vkCreateDescriptorSetLayout(m_impl->device, &dlci, nullptr, &m_impl->setLayout);

    // 7. Pipeline layout (push constants)
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = sizeof(float) + sizeof(uint32_t);
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_impl->setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pc;
    vkCreatePipelineLayout(m_impl->device, &plci, nullptr, &m_impl->pipelineLayout);

    // 8. Compute pipeline
    VkComputePipelineCreateInfo cpci2{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci2.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci2.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci2.stage.module = m_impl->shaderModule;
    cpci2.stage.pName = "main";
    cpci2.layout = m_impl->pipelineLayout;
    vkCreateComputePipelines(m_impl->device, VK_NULL_HANDLE, 1, &cpci2, nullptr, &m_impl->pipeline);

    // 9. Descriptor pool + set
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;
    vkCreateDescriptorPool(m_impl->device, &dpci, nullptr, &m_impl->descPool);

    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = m_impl->descPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &m_impl->setLayout;
    vkAllocateDescriptorSets(m_impl->device, &dsai, &m_impl->descSet);

    return true;
}

DispatchResult Fp8QuantizerDispatch::dispatch(const float* src, uint8_t* dst, size_t n, float scale) {
    if (!m_impl || m_impl->device == VK_NULL_HANDLE) {
        return {false, 0.0, 0, "not initialized"};
    }

    size_t srcBytes = n * sizeof(float);
    size_t dstBytes = n * sizeof(uint8_t);

    // Recreate buffers if size changed
    if (n != m_impl->currentSize) {
        if (m_impl->srcBuf) { vkDestroyBuffer(m_impl->device, m_impl->srcBuf, nullptr); m_impl->srcBuf = VK_NULL_HANDLE; }
        if (m_impl->dstBuf) { vkDestroyBuffer(m_impl->device, m_impl->dstBuf, nullptr); m_impl->dstBuf = VK_NULL_HANDLE; }
        if (m_impl->srcMem) { vkFreeMemory(m_impl->device, m_impl->srcMem, nullptr); m_impl->srcMem = VK_NULL_HANDLE; }
        if (m_impl->dstMem) { vkFreeMemory(m_impl->device, m_impl->dstMem, nullptr); m_impl->dstMem = VK_NULL_HANDLE; }

        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = srcBytes;
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(m_impl->device, &bci, nullptr, &m_impl->srcBuf);
        bci.size = dstBytes;
        vkCreateBuffer(m_impl->device, &bci, nullptr, &m_impl->dstBuf);

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(m_impl->device, m_impl->srcBuf, &mr);
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(m_impl->physicalDevice, &memProps);
        uint32_t memIndex = 0;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if (mr.memoryTypeBits & (1 << i)) { memIndex = i; break; }
        }
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = memIndex;
        vkAllocateMemory(m_impl->device, &mai, nullptr, &m_impl->srcMem);
        vkAllocateMemory(m_impl->device, &mai, nullptr, &m_impl->dstMem);
        vkBindBufferMemory(m_impl->device, m_impl->srcBuf, m_impl->srcMem, 0);
        vkBindBufferMemory(m_impl->device, m_impl->dstBuf, m_impl->dstMem, 0);

        m_impl->currentSize = n;
    }

    // Upload source data
    void* mapped = nullptr;
    vkMapMemory(m_impl->device, m_impl->srcMem, 0, srcBytes, 0, &mapped);
    std::memcpy(mapped, src, srcBytes);
    vkUnmapMemory(m_impl->device, m_impl->srcMem);

    // Update descriptor set
    VkDescriptorBufferInfo srcInfo{m_impl->srcBuf, 0, srcBytes};
    VkDescriptorBufferInfo dstInfo{m_impl->dstBuf, 0, dstBytes};
    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_impl->descSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &srcInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_impl->descSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &dstInfo;
    vkUpdateDescriptorSets(m_impl->device, 2, writes, 0, nullptr);

    // Record + submit
    vkResetCommandBuffer(m_impl->cmdBuf, 0);
    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_impl->cmdBuf, &cbbi);

    vkCmdBindPipeline(m_impl->cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_impl->pipeline);
    vkCmdBindDescriptorSets(m_impl->cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_impl->pipelineLayout, 0, 1, &m_impl->descSet, 0, nullptr);
    struct Push { float scale; uint32_t count; } push{scale, static_cast<uint32_t>(n)};
    vkCmdPushConstants(m_impl->cmdBuf, m_impl->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    vkCmdDispatch(m_impl->cmdBuf, (n + 255) / 256, 1, 1);

    vkEndCommandBuffer(m_impl->cmdBuf);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &m_impl->cmdBuf;

    auto t0 = std::chrono::high_resolution_clock::now();
    vkQueueSubmit(m_impl->queue, 1, &si, m_impl->fence);
    vkWaitForFences(m_impl->device, 1, &m_impl->fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_impl->device, 1, &m_impl->fence);
    auto t1 = std::chrono::high_resolution_clock::now();

    // Download result
    vkMapMemory(m_impl->device, m_impl->dstMem, 0, dstBytes, 0, &mapped);
    std::memcpy(dst, mapped, dstBytes);
    vkUnmapMemory(m_impl->device, m_impl->dstMem);

    double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return {true, elapsed, srcBytes + dstBytes, nullptr};
}

void Fp8QuantizerDispatch::shutdown() {
    if (!m_impl) return;
    if (m_impl->fence)       vkDestroyFence(m_impl->device, m_impl->fence, nullptr);
    if (m_impl->cmdBuf)      vkFreeCommandBuffers(m_impl->device, m_impl->cmdPool, 1, &m_impl->cmdBuf);
    if (m_impl->cmdPool)     vkDestroyCommandPool(m_impl->device, m_impl->cmdPool, nullptr);
    if (m_impl->pipeline)    vkDestroyPipeline(m_impl->device, m_impl->pipeline, nullptr);
    if (m_impl->pipelineLayout) vkDestroyPipelineLayout(m_impl->device, m_impl->pipelineLayout, nullptr);
    if (m_impl->descPool)    vkDestroyDescriptorPool(m_impl->device, m_impl->descPool, nullptr);
    if (m_impl->setLayout)   vkDestroyDescriptorSetLayout(m_impl->device, m_impl->setLayout, nullptr);
    if (m_impl->shaderModule) vkDestroyShaderModule(m_impl->device, m_impl->shaderModule, nullptr);
    if (m_impl->srcBuf)      vkDestroyBuffer(m_impl->device, m_impl->srcBuf, nullptr);
    if (m_impl->dstBuf)      vkDestroyBuffer(m_impl->device, m_impl->dstBuf, nullptr);
    if (m_impl->srcMem)      vkFreeMemory(m_impl->device, m_impl->srcMem, nullptr);
    if (m_impl->dstMem)      vkFreeMemory(m_impl->device, m_impl->dstMem, nullptr);
    if (m_impl->device)      vkDestroyDevice(m_impl->device, nullptr);
    if (m_impl->instance)   vkDestroyInstance(m_impl->instance, nullptr);
    delete m_impl;
    m_impl = nullptr;
}

} // namespace rawrxd::gpu
