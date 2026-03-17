#pragma once

#include "vulkan_core_phase2.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <deque>
#include <thread>
#include <atomic>

// ========================================================================
// PHASE 3: ADVANCED GPU FEATURES
// ========================================================================
// Ray tracing, tensor operations, dynamic pipeline tuning, memory pools,
// and unified CPU/GPU memory management.

namespace RawrXD::GPU::Phase3 {

using namespace RawrXD::GPU::Phase2;

// =====================================================================
// RAY TRACING SUPPORT
// =====================================================================

struct RayTracingAccelerationStructure {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkDeviceAddress device_address = 0;
    GPUBuffer* buffer = nullptr;
    uint32_t primitive_count = 0;
    std::string debug_name;
};

struct RayTracingPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    std::vector<VkShaderModule> shader_modules;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
    GPUBuffer* sbt_buffer = nullptr;  // Shader Binding Table
    VkDeviceSize sbt_stride = 0;
    std::string debug_name;
};

class RayTracingEngine {
public:
    RayTracingEngine(GPUDeviceManager* device_mgr, GPUMemoryManager* mem_mgr);
    ~RayTracingEngine();

    // Initialization
    bool Initialize();

    // Ray tracing capabilities
    bool SupportsRayTracing() const { return supports_ray_tracing_; }
    uint32_t GetMaxRayRecursion() const { return max_ray_recursion_; }
    VkDeviceSize GetSBTAlignment() const { return sbt_alignment_; }

    // Acceleration structure management
    RayTracingAccelerationStructure* CreateAccelerationStructure(
        const std::vector<VkAccelerationStructureGeometryKHR>& geometries,
        const std::vector<uint32_t>& primitive_counts,
        const std::string& name = "");
    void DestroyAccelerationStructure(RayTracingAccelerationStructure* accel);

    // Ray tracing pipeline creation
    RayTracingPipeline* CreateRayTracingPipeline(
        const std::vector<std::string>& shader_paths,
        const std::vector<uint32_t>& shader_group_types,
        VkPipelineLayout layout,
        const std::string& name = "");
    void DestroyRayTracingPipeline(RayTracingPipeline* pipeline);

    // Ray tracing dispatch
    bool DispatchRays(CommandBufferFrame* cmd_buf, RayTracingPipeline* pipeline,
                     uint32_t width, uint32_t height, uint32_t depth = 1);

private:
    bool QueryRayTracingProperties();
    bool CreateShaderBindingTable(RayTracingPipeline* pipeline);

    GPUDeviceManager* device_mgr_ = nullptr;
    GPUMemoryManager* mem_mgr_ = nullptr;

    bool supports_ray_tracing_ = false;
    uint32_t max_ray_recursion_ = 0;
    VkDeviceSize sbt_alignment_ = 0;
    uint32_t sbt_entry_size_ = 0;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_properties_{};

    std::unordered_map<RayTracingAccelerationStructure*, std::unique_ptr<RayTracingAccelerationStructure>> accel_structures_;
    std::unordered_map<RayTracingPipeline*, std::unique_ptr<RayTracingPipeline>> pipelines_;
    std::mutex mutex_;
};

// =====================================================================
// TENSOR OPERATIONS (Matrix Multiply, Element-wise, Reductions)
// =====================================================================

enum class TensorLayout {
    NCHW,      // Batch, Channels, Height, Width
    NHWC,      // Batch, Height, Width, Channels
    NWHC,      // Batch, Width, Height, Channels
    LINEAR     // Flattened
};

struct TensorDescriptor {
    std::vector<uint32_t> shape;
    TensorLayout layout = TensorLayout::LINEAR;
    VkFormat data_format = VK_FORMAT_R32_SFLOAT;
    GPUBuffer* buffer = nullptr;
    uint64_t element_count = 0;
    std::string debug_name;
};

struct MatrixMultiplyParams {
    uint32_t m = 0, n = 0, k = 0;
    float alpha = 1.0f;
    float beta = 0.0f;
    bool transpose_a = false;
    bool transpose_b = false;
};

class TensorComputeEngine {
public:
    TensorComputeEngine(GPUDeviceManager* device_mgr, GPUMemoryManager* mem_mgr, ShaderCompiler* shader_compiler);
    ~TensorComputeEngine();

    // Tensor allocation
    TensorDescriptor* AllocateTensor(const std::vector<uint32_t>& shape, TensorLayout layout,
                                     VkFormat format = VK_FORMAT_R32_SFLOAT, const std::string& name = "");
    void DeallocateTensor(TensorDescriptor* tensor);

    // Matrix operations (GEMM)
    bool MatrixMultiply(CommandBufferFrame* cmd_buf,
                       const TensorDescriptor* a, const TensorDescriptor* b, TensorDescriptor* c,
                       const MatrixMultiplyParams& params);

    // Element-wise operations
    bool ElementWiseAdd(CommandBufferFrame* cmd_buf, const TensorDescriptor* a, const TensorDescriptor* b,
                       TensorDescriptor* result);
    bool ElementWiseMultiply(CommandBufferFrame* cmd_buf, const TensorDescriptor* a, const TensorDescriptor* b,
                            TensorDescriptor* result);
    bool ElementWiseScale(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor, float scale,
                         TensorDescriptor* result);

    // Reduction operations
    bool ReduceSum(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor, uint32_t axis,
                  TensorDescriptor* result);
    bool ReduceMax(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor, uint32_t axis,
                  TensorDescriptor* result);
    bool ReduceMean(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor, uint32_t axis,
                   TensorDescriptor* result);

    // Activation functions
    bool ReLU(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor, TensorDescriptor* result);
    bool Softmax(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor, uint32_t axis,
                TensorDescriptor* result);
    bool Tanh(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor, TensorDescriptor* result);

    // Tensor shape operations
    bool Reshape(const TensorDescriptor* tensor, const std::vector<uint32_t>& new_shape,
                TensorDescriptor* result);
    bool Transpose(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
                  const std::vector<uint32_t>& axes, TensorDescriptor* result);
    bool Concat(CommandBufferFrame* cmd_buf, const std::vector<const TensorDescriptor*>& tensors,
               uint32_t axis, TensorDescriptor* result);

private:
    uint64_t ComputeElementCount(const std::vector<uint32_t>& shape) const;
    uint32_t GetFormatSize(VkFormat format) const;

    GPUDeviceManager* device_mgr_ = nullptr;
    GPUMemoryManager* mem_mgr_ = nullptr;
    ShaderCompiler* shader_compiler_ = nullptr;

    std::unordered_map<TensorDescriptor*, std::unique_ptr<TensorDescriptor>> tensors_;
    std::unordered_map<std::string, ComputePipeline*> pipelines_;
    std::mutex mutex_;
};

// =====================================================================
// DYNAMIC PIPELINE TUNING
// =====================================================================

struct PipelinePerformanceProfile {
    uint32_t workgroup_size_x = 8;
    uint32_t workgroup_size_y = 8;
    uint32_t workgroup_size_z = 1;
    uint32_t optimization_level = 1;  // 0=Conservative, 1=Balanced, 2=Aggressive
    float estimated_bandwidth = 0.0f;
    float measured_throughput = 0.0f;
    uint64_t total_execution_time_ns = 0;
    uint32_t execution_count = 0;
    float cache_hit_ratio = 0.0f;
    bool is_optimized = false;
};

class PipelineTuner {
public:
    PipelineTuner(GPUDeviceManager* device_mgr, SynchronizationManager* sync_mgr);
    ~PipelineTuner();

    // Profile a pipeline
    bool ProfilePipeline(CommandBufferFrame* cmd_buf, ComputePipeline* pipeline,
                        uint32_t iterations = 10);

    // Auto-tune based on profiling
    PipelinePerformanceProfile AutoTune(ComputePipeline* pipeline);

    // Set workgroup dimensions
    void SetWorkgroupDimensions(ComputePipeline* pipeline, uint32_t x, uint32_t y, uint32_t z);

    // Get current profile
    const PipelinePerformanceProfile& GetProfile(ComputePipeline* pipeline) const;

    // Estimate performance
    float EstimateBandwidth(uint64_t data_bytes, uint64_t time_ns) const;

private:
    struct PipelineMetrics {
        std::deque<uint64_t> execution_times;
        std::atomic<uint64_t> total_time{0};
        std::atomic<uint32_t> invocation_count{0};
        float peak_bandwidth = 0.0f;
    };

    GPUDeviceManager* device_mgr_ = nullptr;
    SynchronizationManager* sync_mgr_ = nullptr;

    std::unordered_map<ComputePipeline*, PipelinePerformanceProfile> profiles_;
    std::unordered_map<ComputePipeline*, PipelineMetrics> metrics_;
    std::mutex mutex_;
};

// =====================================================================
// PERSISTENT GPU MEMORY POOLS
// =====================================================================

class GPUMemoryPool {
public:
    GPUMemoryPool(GPUDeviceManager* device_mgr, GPUMemoryManager* mem_mgr, 
                  VkDeviceSize pool_size, const std::string& name = "");
    ~GPUMemoryPool();

    // Memory allocation from pool
    GPUBuffer* Allocate(VkDeviceSize size, VkBufferUsageFlags usage, const std::string& name = "");
    void Deallocate(GPUBuffer* buffer);

    // Pool statistics
    struct PoolStats {
        VkDeviceSize total_size = 0;
        VkDeviceSize used_size = 0;
        VkDeviceSize free_size = 0;
        uint32_t allocation_count = 0;
        uint32_t free_count = 0;
        float fragmentation = 0.0f;
        uint32_t largest_free_block = 0;
    };
    PoolStats GetStats() const;

    // Defragmentation
    bool Defragment(CommandBufferFrame* cmd_buf);
    void EnableAutoDefragmentation(bool enable) { auto_defrag_enabled_ = enable; }

    // Memory compaction
    bool Compact();

    const std::string& GetName() const { return name_; }
    VkDeviceSize GetTotalSize() const { return total_size_; }
    VkDeviceSize GetUsedSize() const;

private:
    struct AllocationBlock {
        GPUBuffer* buffer = nullptr;
        VkDeviceSize offset = 0;
        VkDeviceSize size = 0;
        bool is_free = true;
        uint64_t allocation_time = 0;
        uint32_t allocation_id = 0;
    };

    void CoalesceAdjacentFreeBlocks();
    float CalculateFragmentation() const;

    GPUDeviceManager* device_mgr_ = nullptr;
    GPUMemoryManager* mem_mgr_ = nullptr;

    std::string name_;
    VkDeviceSize total_size_ = 0;
    std::vector<AllocationBlock> blocks_;
    uint32_t next_allocation_id_ = 0;
    bool auto_defrag_enabled_ = false;
    std::mutex mutex_;
};

// =====================================================================
// UNIFIED CPU/GPU MEMORY
// =====================================================================

class UnifiedMemoryManager {
public:
    UnifiedMemoryManager(GPUDeviceManager* device_mgr, GPUMemoryManager* mem_mgr);
    ~UnifiedMemoryManager();

    // Allocate unified memory (accessible from both CPU and GPU)
    GPUBuffer* AllocateUnifiedMemory(VkDeviceSize size, const std::string& name = "");

    // Sync operations
    bool SyncToGPU(CommandBufferFrame* cmd_buf, GPUBuffer* buffer, VkDeviceSize size = 0);
    bool SyncToCPU(CommandBufferFrame* cmd_buf, GPUBuffer* buffer, VkDeviceSize size = 0);

    // Automatic memory tracking
    void TrackAccess(GPUBuffer* buffer, bool is_gpu_write = true);
    void EnsureConsistency(GPUBuffer* buffer);

    // Prefetching
    bool Prefetch(CommandBufferFrame* cmd_buf, GPUBuffer* buffer, bool to_gpu = true);

    // Memory usage profiling
    struct UnifiedMemoryStats {
        uint64_t total_unified_memory = 0;
        uint64_t gpu_resident_memory = 0;
        uint64_t cpu_resident_memory = 0;
        uint32_t sync_count = 0;
        float avg_sync_time_us = 0.0f;
    };
    UnifiedMemoryStats GetStats() const;

private:
    struct AccessTracker {
        bool last_accessed_on_gpu = false;
        uint64_t last_access_time = 0;
        uint32_t total_syncs = 0;
        uint64_t total_sync_time_ns = 0;
    };

    GPUDeviceManager* device_mgr_ = nullptr;
    GPUMemoryManager* mem_mgr_ = nullptr;

    std::unordered_map<GPUBuffer*, AccessTracker> access_tracking_;
    std::unordered_map<GPUBuffer*, GPUBuffer*> mirror_buffers_;
    std::mutex mutex_;
};

}  // namespace RawrXD::GPU::Phase3
