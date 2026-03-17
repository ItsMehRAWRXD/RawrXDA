#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <queue>
#include <cstdint>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>
#include <optional>
#include <array>

// ========================================================================
// PHASE 2: REAL VULKAN LIBRARY INTEGRATION
// ========================================================================
// Full Vulkan implementation with actual GPU operations, memory management,
// command buffer recording, synchronization, and shader compilation.

namespace RawrXD::GPU::Phase2 {

    // Structured logging for observability
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR };
    
    class Logger {
    public:
        static void Log(LogLevel level, const std::string& component, const std::string& message) {
            std::string level_str;
            switch (level) {
                case LogLevel::DEBUG: level_str = "[DEBUG]"; break;
                case LogLevel::INFO: level_str = "[INFO]"; break;
                case LogLevel::WARNING: level_str = "[WARN]"; break;
                case LogLevel::ERROR: level_str = "[ERROR]"; break;
            }
            std::cout << level_str << " [" << component << "] " << message << std::endl;
        }
    };

    // =====================================================================
    // GPU MEMORY STRUCTURES
    // =====================================================================
    
    struct GPUBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        VkMemoryPropertyFlags properties = 0;
        void* mapped_ptr = nullptr;
        uint64_t creation_time = 0;
        uint64_t last_used_time = 0;
        bool is_persistent = false;
        std::string debug_name;
    };

    struct GPUImage {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
        VkImageUsageFlags usage = 0;
        std::string debug_name;
    };

    struct ComputeShader {
        VkShaderModule module = VK_NULL_HANDLE;
        std::vector<uint32_t> spirv_code;
        std::string name;
        std::string path;
        uint64_t load_time = 0;
    };

    struct ComputePipeline {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
        std::string shader_name;
        VkPushConstantRange push_constant_range{};
        bool has_push_constants = false;
    };

    // Fence/Semaphore for synchronization
    struct SyncPrimitive {
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        bool is_signaled = true;
        uint64_t signal_time = 0;
    };

    // Command buffer with tracking
    struct CommandBufferFrame {
        VkCommandBuffer buffer = VK_NULL_HANDLE;
        SyncPrimitive sync;
        bool is_recording = false;
        bool is_submitted = false;
        uint32_t frame_index = 0;
        std::vector<VkBuffer> bound_buffers;
        std::vector<VkImage> bound_images;
    };

    // =====================================================================
    // GPU DEVICE MANAGER
    // =====================================================================
    
    class GPUDeviceManager {
    public:
        GPUDeviceManager();
        ~GPUDeviceManager();

        // Initialization
        bool Initialize(bool enable_validation = true);
        void Shutdown();

        // Device Information
        VkInstance GetInstance() const { return instance_; }
        VkPhysicalDevice GetPhysicalDevice() const { return physical_device_; }
        VkDevice GetDevice() const { return device_; }
        VkQueue GetComputeQueue() const { return compute_queue_; }
        VkCommandPool GetCommandPool() const { return command_pool_; }

        // Device Properties
        const VkPhysicalDeviceProperties& GetDeviceProperties() const { return device_properties_; }
        const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return memory_properties_; }
        uint32_t GetComputeQueueFamily() const { return compute_queue_family_; }

        // Device Capabilities
        bool SupportsRayTracing() const { return supports_ray_tracing_; }
        bool SupportsMeshShading() const { return supports_mesh_shading_; }
        uint32_t GetMaxWorkGroupSize() const { return max_work_group_size_; }

        // Memory Queries
        VkDeviceSize GetAvailableMemory() const;
        VkDeviceSize GetTotalMemory() const;
        uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

    private:
        bool CreateInstance(bool enable_validation);
        bool SelectPhysicalDevice();
        bool CreateLogicalDevice();
        void QueryDeviceCapabilities();
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

        VkInstance instance_ = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue compute_queue_ = VK_NULL_HANDLE;
        VkCommandPool command_pool_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;

        VkPhysicalDeviceProperties device_properties_{};
        VkPhysicalDeviceMemoryProperties memory_properties_{};
        uint32_t compute_queue_family_ = 0;

        bool supports_ray_tracing_ = false;
        bool supports_mesh_shading_ = false;
        uint32_t max_work_group_size_ = 0;
        bool validation_enabled_ = false;
    };

    // =====================================================================
    // GPU MEMORY MANAGER
    // =====================================================================
    
    class GPUMemoryManager {
    public:
        GPUMemoryManager(GPUDeviceManager* device_mgr);
        ~GPUMemoryManager();

        // Buffer Operations
        GPUBuffer* AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags properties, const std::string& name = "");
        void DeallocateBuffer(GPUBuffer* buffer);
        void* MapBuffer(GPUBuffer* buffer);
        void UnmapBuffer(GPUBuffer* buffer);
        void CopyBuffer(GPUBuffer* src, GPUBuffer* dst, VkDeviceSize size);

        // Image Operations
        GPUImage* AllocateImage(uint32_t width, uint32_t height, uint32_t depth,
                                VkFormat format, VkImageUsageFlags usage, const std::string& name = "");
        void DeallocateImage(GPUImage* image);
        void CopyBufferToImage(GPUBuffer* buffer, GPUImage* image, uint32_t width, uint32_t height);

        // Memory Pool Management
        class MemoryPool {
        public:
            MemoryPool(VkDevice device, VkPhysicalDeviceMemoryProperties mem_props, uint32_t type_index);
            ~MemoryPool();
            
            VkDeviceMemory Allocate(VkDeviceSize size);
            void Deallocate(VkDeviceMemory memory, VkDeviceSize size);
            VkDeviceSize GetAvailableSize() const { return available_size_; }
            float GetFragmentation() const;

        private:
            struct AllocationBlock {
                VkDeviceMemory memory = VK_NULL_HANDLE;
                VkDeviceSize size = 0;
                bool is_free = true;
                uint64_t allocation_time = 0;
            };

            VkDevice device_;
            uint32_t memory_type_index_;
            std::vector<AllocationBlock> blocks_;
            VkDeviceSize available_size_ = 0;
            std::mutex mutex_;
        };

        // Statistics
        struct MemoryStats {
            VkDeviceSize total_allocated = 0;
            VkDeviceSize buffer_memory = 0;
            VkDeviceSize image_memory = 0;
            uint32_t buffer_count = 0;
            uint32_t image_count = 0;
            float fragmentation_ratio = 0.0f;
        };
        MemoryStats GetMemoryStats() const;

    private:
        GPUDeviceManager* device_mgr_ = nullptr;
        std::unordered_map<GPUBuffer*, std::unique_ptr<GPUBuffer>> buffers_;
        std::unordered_map<GPUImage*, std::unique_ptr<GPUImage>> images_;
        std::vector<std::unique_ptr<MemoryPool>> memory_pools_;
        mutable std::mutex mutex_;

        VkDeviceSize CalculatePaddedSize(VkDeviceSize size) const;
    };

    // =====================================================================
    // COMMAND BUFFER RECORDER
    // =====================================================================
    
    class CommandBufferRecorder {
    public:
        CommandBufferRecorder(GPUDeviceManager* device_mgr);
        ~CommandBufferRecorder();

        // Command Buffer Management
        CommandBufferFrame* AllocateCommandBuffer();
        void FreeCommandBuffer(CommandBufferFrame* cmd_buf);
        void ResetCommandBuffer(CommandBufferFrame* cmd_buf);

        // Recording Operations
        bool BeginRecording(CommandBufferFrame* cmd_buf);
        bool EndRecording(CommandBufferFrame* cmd_buf);

        bool BindComputePipeline(CommandBufferFrame* cmd_buf, const ComputePipeline* pipeline);
        bool BindBufferSet(CommandBufferFrame* cmd_buf, VkDescriptorSet desc_set, uint32_t first_binding = 0);
        bool DispatchCompute(CommandBufferFrame* cmd_buf, uint32_t group_count_x,
                             uint32_t group_count_y, uint32_t group_count_z);
        bool PushConstants(CommandBufferFrame* cmd_buf, const ComputePipeline* pipeline,
                           const void* data, uint32_t size);

        // Buffer Operations
        bool FillBuffer(CommandBufferFrame* cmd_buf, VkBuffer buffer, VkDeviceSize size, uint32_t data);
        bool CopyBuffer(CommandBufferFrame* cmd_buf, VkBuffer src, VkBuffer dst, VkDeviceSize size);
        bool CopyBufferToImage(CommandBufferFrame* cmd_buf, VkBuffer src, VkImage dst,
                               uint32_t width, uint32_t height);

        // Synchronization
        bool PipelineBarrier(CommandBufferFrame* cmd_buf, VkPipelineStageFlags src_stage,
                             VkPipelineStageFlags dst_stage, VkDependencyFlags dependency_flags = 0);

        // Submission
        bool Submit(CommandBufferFrame* cmd_buf, VkSemaphore wait_sem = VK_NULL_HANDLE,
                    VkSemaphore signal_sem = VK_NULL_HANDLE);
        bool WaitForCompletion(CommandBufferFrame* cmd_buf, uint64_t timeout_ns = UINT64_MAX);
        bool CheckCompletion(CommandBufferFrame* cmd_buf);

    private:
        GPUDeviceManager* device_mgr_ = nullptr;
        std::queue<std::unique_ptr<CommandBufferFrame>> free_buffers_;
        std::vector<std::unique_ptr<CommandBufferFrame>> allocated_buffers_;
        std::mutex mutex_;
    };

    // =====================================================================
    // SHADER COMPILER & LOADER
    // =====================================================================
    
    class ShaderCompiler {
    public:
        ShaderCompiler(GPUDeviceManager* device_mgr);
        ~ShaderCompiler();

        // SPIR-V Loading
        bool LoadSPIRVFromFile(const std::string& path, std::vector<uint32_t>& spirv_code);
        bool ValidateSPIRVCode(const std::vector<uint32_t>& code);

        // Shader Module Creation
        ComputeShader* CompileAndLoad(const std::string& name, const std::string& spirv_path);
        void UnloadShader(ComputeShader* shader);

        // Pipeline Creation
        ComputePipeline* CreateComputePipeline(const std::string& shader_name,
                                               const VkDescriptorSetLayout* desc_layouts = nullptr,
                                               uint32_t desc_layout_count = 0);
        void DestroyPipeline(ComputePipeline* pipeline);

        // Descriptor Management
        VkDescriptorSet AllocateDescriptorSet(const VkDescriptorSetLayout& layout);
        void FreeDescriptorSet(VkDescriptorSet set);
        void UpdateDescriptorSet(VkDescriptorSet set, uint32_t binding, VkBuffer buffer,
                                 VkDeviceSize range = VK_WHOLE_SIZE);
        void UpdateDescriptorSetImage(VkDescriptorSet set, uint32_t binding, VkImageView image_view);

    private:
        GPUDeviceManager* device_mgr_ = nullptr;
        std::unordered_map<std::string, std::unique_ptr<ComputeShader>> shaders_;
        std::unordered_map<std::string, std::unique_ptr<ComputePipeline>> pipelines_;
        VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
        std::mutex mutex_;

        bool CreateDescriptorPool();
    };

    // =====================================================================
    // SYNCHRONIZATION MANAGER
    // =====================================================================
    
    class SynchronizationManager {
    public:
        SynchronizationManager(GPUDeviceManager* device_mgr);
        ~SynchronizationManager();

        // Fence Operations
        SyncPrimitive CreateFence(bool signaled = true);
        void DestroyFence(SyncPrimitive& sync);
        bool WaitForFence(SyncPrimitive& sync, uint64_t timeout_ns = UINT64_MAX);
        bool CheckFence(const SyncPrimitive& sync);
        void ResetFence(SyncPrimitive& sync);

        // Semaphore Operations
        SyncPrimitive CreateSemaphore();
        void DestroySemaphore(SyncPrimitive& sync);

        // Multi-Fence Wait
        bool WaitForFences(std::vector<SyncPrimitive>& syncs, bool wait_all = true,
                           uint64_t timeout_ns = UINT64_MAX);

        // Tracing Integration
        struct TraceMarker {
            std::string name;
            uint64_t start_time = 0;
            uint64_t end_time = 0;
            uint64_t duration_ns = 0;
        };

        TraceMarker BeginTrace(const std::string& name);
        void EndTrace(TraceMarker& marker);
        std::vector<TraceMarker> GetTraceHistory() const;

    private:
        GPUDeviceManager* device_mgr_ = nullptr;
        std::vector<TraceMarker> trace_history_;
        std::mutex mutex_;

        uint64_t GetCurrentTimestamp() const;
    };

}  // namespace RawrXD::GPU::Phase2
