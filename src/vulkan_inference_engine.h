// ============================================================================
// vulkan_inference_engine.h — Vulkan GPU Inference Engine
// ============================================================================
#pragma once

#include "inference_engine.h"
#include <windows.h>
#include <vector>
#include <string>
#include <memory>

namespace RawrXD {

// Forward declarations for Vulkan types (avoid including vulkan.h)
struct VkInstance_T;
struct VkDevice_T;
struct VkPhysicalDevice_T;
struct VkQueue_T;
struct VkCommandPool_T;
struct VkBuffer_T;
struct VkDeviceMemory_T;
struct VkShaderModule_T;
struct VkPipeline_T;
struct VkPipelineLayout_T;
struct VkDescriptorSetLayout_T;
struct VkDescriptorPool_T;
struct VkDescriptorSet_T;
struct VkCommandBuffer_T;
struct VkFence_T;

using VkInstance = VkInstance_T*;
using VkDevice = VkDevice_T*;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkQueue = VkQueue_T*;
using VkCommandPool = VkCommandPool_T*;
using VkBuffer = VkBuffer_T*;
using VkDeviceMemory = VkDeviceMemory_T*;
using VkShaderModule = VkShaderModule_T*;
using VkPipeline = VkPipeline_T*;
using VkPipelineLayout = VkPipelineLayout_T*;
using VkDescriptorSetLayout = VkDescriptorSetLayout_T*;
using VkDescriptorPool = VkDescriptorPool_T*;
using VkDescriptorSet = VkDescriptorSet_T*;
using VkCommandBuffer = VkCommandBuffer_T*;
using VkFence = VkFence_T*;

// ============================================================================
// Vulkan Inference Engine
// ============================================================================
class VulkanInferenceEngine {
public:
    VulkanInferenceEngine();
    ~VulkanInferenceEngine();

    // Static factory
    static std::unique_ptr<VulkanInferenceEngine> TryCreate();

    // InferenceEngine interface
    bool LoadModel(const std::string& path);
    bool IsModelLoaded() const;
    void UnloadModel();
    std::vector<int32_t> Tokenize(const std::string& text);
    std::vector<int32_t> Generate(const std::vector<int32_t>& tokens, int maxTokens);
    void GenerateStreaming(const std::vector<int32_t>& tokens, int maxTokens,
                          std::function<void(const std::string&)> onToken,
                          std::function<void()> onComplete,
                          void* userData);
    void SetContextLimit(size_t limit);
    size_t GetContextLimit() const;
    void SetThreadCount(int count);
    int GetThreadCount() const;
    const char* GetBackendName() const;

private:
    bool InitializeVulkan();
    void ShutdownVulkan();
    bool CreateComputePipeline();
    bool AllocateGPUBuffer(VkBuffer& buffer, VkDeviceMemory& memory, size_t size);
    void FreeGPUBuffer(VkBuffer buffer, VkDeviceMemory memory);
    bool RunComputeShader(const float* input, float* output, size_t count);

    // Vulkan handles
    HMODULE m_vulkanLib = nullptr;
    VkInstance m_instance = nullptr;
    VkPhysicalDevice m_physicalDevice = nullptr;
    VkDevice m_device = nullptr;
    VkQueue m_queue = nullptr;
    VkCommandPool m_commandPool = nullptr;
    VkDescriptorPool m_descriptorPool = nullptr;

    // Compute pipeline
    VkShaderModule m_shaderModule = nullptr;
    VkPipelineLayout m_pipelineLayout = nullptr;
    VkPipeline m_pipeline = nullptr;
    VkDescriptorSetLayout m_descriptorSetLayout = nullptr;

    // Model data
    bool m_modelLoaded = false;
    std::string m_modelPath;
    size_t m_contextLimit = 4096;
    int m_threadCount = 4;

    // GPU buffers
    VkBuffer m_inputBuffer = nullptr;
    VkDeviceMemory m_inputMemory = nullptr;
    VkBuffer m_outputBuffer = nullptr;
    VkDeviceMemory m_outputMemory = nullptr;
    VkBuffer m_weightBuffer = nullptr;
    VkDeviceMemory m_weightMemory = nullptr;

    size_t m_bufferSize = 0;
};

} // namespace RawrXD
