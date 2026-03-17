#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <queue>

// Forward declare Vulkan types to avoid including vulkan.h in header
struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkCommandPool_T;
struct VkQueue_T;
struct VkShaderModule_T;
struct VkPipelineLayout_T;
struct VkPipeline_T;
struct VkBuffer_T;
struct VkDeviceMemory_T;
struct VkCommandBuffer_T;
struct VkFence_T;

typedef VkInstance_T* VkInstance;
typedef VkPhysicalDevice_T* VkPhysicalDevice;
typedef VkDevice_T* VkDevice;
typedef VkCommandPool_T* VkCommandPool;
typedef VkQueue_T* VkQueue;
typedef VkShaderModule_T* VkShaderModule;
typedef VkPipelineLayout_T* VkPipelineLayout;
typedef VkPipeline_T* VkPipeline;
typedef VkBuffer_T* VkBuffer;
typedef VkDeviceMemory_T* VkDeviceMemory;
typedef VkCommandBuffer_T* VkCommandBuffer;
typedef VkFence_T* VkFence;

struct VulkanDeviceInfo {
    std::string device_name;
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
    std::vector<float> host_data;
    VkBuffer device_buffer = nullptr;
    VkDeviceMemory device_memory = nullptr;
};

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
    void Cleanup();
    
    bool LoadShaderFromFile(const std::string& filename, const std::string& shader_name);
    bool CreateComputePipeline(const std::string& shader_name);
    
    bool CreateTensor(const std::string& name, size_t size_bytes, const std::vector<float>& data = {});
    bool UploadTensor(const std::string& tensor_name);
    bool DownloadTensor(const std::string& tensor_name);
    
    bool DispatchCompute(const std::string& shader_name, uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1);
    
    const VulkanDeviceInfo& GetDeviceInfo() const { return device_info_; }
    bool IsAMDDevice() const { return device_info_.vendor_id == 0x1002; }
    
private:
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateCommandPool();
    
    void InitializeCommandBufferPool(uint32_t pool_size);
    void CleanupCommandBufferPool();
    VkCommandBuffer AcquireAsyncCommandBuffer();
    void ReleaseAsyncCommandBuffer(VkCommandBuffer buffer);
    
    VkInstance instance_ = nullptr;
    VkPhysicalDevice physical_device_ = nullptr;
    VkDevice device_ = nullptr;
    VkCommandPool command_pool_ = nullptr;
    VkQueue compute_queue_ = nullptr;
    
    VulkanDeviceInfo device_info_;
    std::vector<CommandBufferPool> command_buffer_pool_;
    std::queue<size_t> available_buffer_indices_;
    
    std::unordered_map<std::string, ComputeShader> shaders_;
    std::unordered_map<std::string, VulkanTensor> tensors_;
};