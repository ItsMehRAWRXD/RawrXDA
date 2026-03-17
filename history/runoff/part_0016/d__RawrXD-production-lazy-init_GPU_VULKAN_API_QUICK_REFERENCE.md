# GPU Vulkan API Quick Reference

## Phase 2: Core Functionality

### GPUDeviceManager
```cpp
// Initialization
bool Initialize(bool enable_validation = true);
void Shutdown();

// Access
VkInstance GetInstance() const;
VkDevice GetDevice() const;
VkQueue GetComputeQueue() const;
VkCommandPool GetCommandPool() const;

// Memory
uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
VkDeviceSize GetAvailableMemory() const;
VkDeviceSize GetTotalMemory() const;

// Properties
const VkPhysicalDeviceProperties& GetDeviceProperties() const;
bool SupportsRayTracing() const;
uint32_t GetMaxWorkGroupSize() const;
```

### GPUMemoryManager
```cpp
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

// Statistics
MemoryStats GetMemoryStats() const;
```

### CommandBufferRecorder
```cpp
// Allocation
CommandBufferFrame* AllocateCommandBuffer();
void FreeCommandBuffer(CommandBufferFrame* cmd_buf);
void ResetCommandBuffer(CommandBufferFrame* cmd_buf);

// Recording
bool BeginRecording(CommandBufferFrame* cmd_buf);
bool EndRecording(CommandBufferFrame* cmd_buf);

// Compute Operations
bool BindComputePipeline(CommandBufferFrame* cmd_buf, const ComputePipeline* pipeline);
bool DispatchCompute(CommandBufferFrame* cmd_buf, uint32_t group_count_x,
                     uint32_t group_count_y, uint32_t group_count_z);
bool PushConstants(CommandBufferFrame* cmd_buf, const ComputePipeline* pipeline,
                   const void* data, uint32_t size);

// Buffer Operations
bool FillBuffer(CommandBufferFrame* cmd_buf, VkBuffer buffer, VkDeviceSize size, uint32_t data);
bool CopyBuffer(CommandBufferFrame* cmd_buf, VkBuffer src, VkBuffer dst, VkDeviceSize size);

// Synchronization
bool PipelineBarrier(CommandBufferFrame* cmd_buf, VkPipelineStageFlags src_stage,
                     VkPipelineStageFlags dst_stage, VkDependencyFlags dependency_flags = 0);

// Submission
bool Submit(CommandBufferFrame* cmd_buf, VkSemaphore wait_sem = VK_NULL_HANDLE,
            VkSemaphore signal_sem = VK_NULL_HANDLE);
bool WaitForCompletion(CommandBufferFrame* cmd_buf, uint64_t timeout_ns = UINT64_MAX);
bool CheckCompletion(CommandBufferFrame* cmd_buf);
```

### ShaderCompiler
```cpp
// Loading
bool LoadSPIRVFromFile(const std::string& path, std::vector<uint32_t>& spirv_code);
bool ValidateSPIRVCode(const std::vector<uint32_t>& code);

// Shader Management
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
```

### SynchronizationManager
```cpp
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

// Tracing
TraceMarker BeginTrace(const std::string& name);
void EndTrace(TraceMarker& marker);
std::vector<TraceMarker> GetTraceHistory() const;
```

---

## Phase 3: Advanced Features

### RayTracingEngine
```cpp
bool Initialize();
bool SupportsRayTracing() const;

// Acceleration Structures
RayTracingAccelerationStructure* CreateAccelerationStructure(
    const std::vector<VkAccelerationStructureGeometryKHR>& geometries,
    const std::vector<uint32_t>& primitive_counts,
    const std::string& name = "");
void DestroyAccelerationStructure(RayTracingAccelerationStructure* accel);

// Pipelines
RayTracingPipeline* CreateRayTracingPipeline(
    const std::vector<std::string>& shader_paths,
    const std::vector<uint32_t>& shader_group_types,
    VkPipelineLayout layout,
    const std::string& name = "");
void DestroyRayTracingPipeline(RayTracingPipeline* pipeline);

// Dispatch
bool DispatchRays(CommandBufferFrame* cmd_buf, RayTracingPipeline* pipeline,
                 uint32_t width, uint32_t height, uint32_t depth = 1);
```

### TensorComputeEngine
```cpp
// Allocation
TensorDescriptor* AllocateTensor(const std::vector<uint32_t>& shape, TensorLayout layout,
                                 VkFormat format = VK_FORMAT_R32_SFLOAT, const std::string& name = "");
void DeallocateTensor(TensorDescriptor* tensor);

// Matrix Operations
bool MatrixMultiply(CommandBufferFrame* cmd_buf,
                   const TensorDescriptor* a, const TensorDescriptor* b,
                   TensorDescriptor* c, const MatrixMultiplyParams& params);

// Element-wise Operations
bool ElementWiseAdd(CommandBufferFrame* cmd_buf, const TensorDescriptor* a,
                   const TensorDescriptor* b, TensorDescriptor* result);
bool ElementWiseMultiply(CommandBufferFrame* cmd_buf, const TensorDescriptor* a,
                        const TensorDescriptor* b, TensorDescriptor* result);
bool ElementWiseScale(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
                     float scale, TensorDescriptor* result);

// Reductions
bool ReduceSum(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
              uint32_t axis, TensorDescriptor* result);
bool ReduceMax(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
              uint32_t axis, TensorDescriptor* result);
bool ReduceMean(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
               uint32_t axis, TensorDescriptor* result);

// Activations
bool ReLU(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
         TensorDescriptor* result);
bool Softmax(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
            uint32_t axis, TensorDescriptor* result);
bool Tanh(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
         TensorDescriptor* result);

// Shape Operations
bool Reshape(const TensorDescriptor* tensor, const std::vector<uint32_t>& new_shape,
            TensorDescriptor* result);
bool Transpose(CommandBufferFrame* cmd_buf, const TensorDescriptor* tensor,
              const std::vector<uint32_t>& axes, TensorDescriptor* result);
bool Concat(CommandBufferFrame* cmd_buf, const std::vector<const TensorDescriptor*>& tensors,
           uint32_t axis, TensorDescriptor* result);
```

### PipelineTuner
```cpp
bool ProfilePipeline(CommandBufferFrame* cmd_buf, ComputePipeline* pipeline,
                    uint32_t iterations = 10);
PipelinePerformanceProfile AutoTune(ComputePipeline* pipeline);
void SetWorkgroupDimensions(ComputePipeline* pipeline, uint32_t x, uint32_t y, uint32_t z);
const PipelinePerformanceProfile& GetProfile(ComputePipeline* pipeline) const;
float EstimateBandwidth(uint64_t data_bytes, uint64_t time_ns) const;
```

### GPUMemoryPool
```cpp
GPUBuffer* Allocate(VkDeviceSize size, VkBufferUsageFlags usage, const std::string& name = "");
void Deallocate(GPUBuffer* buffer);
PoolStats GetStats() const;
bool Defragment(CommandBufferFrame* cmd_buf);
bool Compact();
void EnableAutoDefragmentation(bool enable);
```

### UnifiedMemoryManager
```cpp
GPUBuffer* AllocateUnifiedMemory(VkDeviceSize size, const std::string& name = "");
bool SyncToGPU(CommandBufferFrame* cmd_buf, GPUBuffer* buffer, VkDeviceSize size = 0);
bool SyncToCPU(CommandBufferFrame* cmd_buf, GPUBuffer* buffer, VkDeviceSize size = 0);
void TrackAccess(GPUBuffer* buffer, bool is_gpu_write = true);
void EnsureConsistency(GPUBuffer* buffer);
bool Prefetch(CommandBufferFrame* cmd_buf, GPUBuffer* buffer, bool to_gpu = true);
UnifiedMemoryStats GetStats() const;
```

---

## Phase 4: Multi-GPU & Performance

### MultiGPUManager
```cpp
bool Initialize(GPULoadBalancingStrategy strategy = GPULoadBalancingStrategy::DYNAMIC);
uint32_t GetGPUCount() const;
const GPUDevice& GetGPU(uint32_t index) const;

// Load Balancing
void SetLoadBalancingStrategy(GPULoadBalancingStrategy strategy);
uint32_t SelectGPUForWork(const GPUWorkItem& work_item, size_t data_size = 0);
void UpdateGPUUtilization(uint32_t gpu_index, float utilization);

// Work Distribution
bool SubmitWork(GPUWorkItem& work_item);
bool WaitForCompletion(const GPUWorkItem& work_item, uint64_t timeout_ns = UINT64_MAX);

// P2P Access
bool EnablePeerAccess(uint32_t gpu_a, uint32_t gpu_b);
bool DisablePeerAccess(uint32_t gpu_a, uint32_t gpu_b);
bool TransferBetweenGPUs(uint32_t src_gpu, uint32_t dst_gpu, GPUBuffer* buffer);

// Monitoring
MultiGPUStats GetStats() const;
```

### AsyncExecutor
```cpp
uint32_t SubmitAsyncTask(const std::string& task_name, CompletionCallback callback,
                        ProgressCallback progress_callback = nullptr, int priority = 0);
bool CancelTask(uint32_t task_id);
bool WaitForTask(uint32_t task_id, uint64_t timeout_ns = UINT64_MAX);
bool IsTaskRunning(uint32_t task_id) const;
bool IsTaskCompleted(uint32_t task_id) const;
float GetTaskProgress(uint32_t task_id) const;

// Batch Operations
std::vector<uint32_t> SubmitBatchTasks(const std::vector<std::pair<std::string, CompletionCallback>>& tasks);
bool WaitForBatch(const std::vector<uint32_t>& task_ids, uint64_t timeout_ns = UINT64_MAX);

// Statistics
AsyncStats GetStats() const;
```

### GPUPowerManager
```cpp
// Clock Management
bool SetClockFrequency(ClockDomain domain, uint32_t frequency_mhz);
bool EnableDynamicClockScaling(ClockDomain domain, bool enable);
GPUClockState GetClockState(ClockDomain domain) const;

// Power Mode
bool SetPowerMode(PowerMode mode);
PowerMode GetCurrentPowerMode() const;

// Thermal Management
void UpdateTemperature(float temp_c);
void UpdatePowerDraw(float power_w);
bool CheckThermalThrottling() const;

// Auto Optimization
bool AutoOptimizeForWorkload(uint64_t compute_intensity);
bool AutoOptimizeForThermals();
bool AutoOptimizeForPower();

// Monitoring
PowerState GetPowerState() const;
PowerStats GetPowerStats() const;
```

### GPUNetworkAccelerator
```cpp
// P2P Transfers
bool SendGPUData(uint32_t src_gpu, uint32_t dst_gpu, GPUBuffer* buffer,
                const NetworkTransferConfig& config);
bool ReceiveGPUData(uint32_t dst_gpu, GPUBuffer* buffer_out,
                   const std::string& from_address, const NetworkTransferConfig& config);

// Collective Operations
bool AllReduce(const std::vector<uint32_t>& gpu_indices, GPUBuffer* buffer);
bool AllGather(const std::vector<uint32_t>& gpu_indices, const std::vector<GPUBuffer*>& buffers);
bool Broadcast(const std::vector<uint32_t>& gpu_indices, GPUBuffer* buffer, uint32_t root_gpu);

// Synchronization
bool BarrierSync(const std::vector<uint32_t>& gpu_indices, uint64_t timeout_ns = UINT64_MAX);

// Streaming
bool StartStreamingTransfer(uint32_t src_gpu, uint32_t dst_gpu, GPUBuffer* buffer,
                           CompletionCallback callback);
bool StopStreamingTransfer(uint32_t transfer_id);

// Optimization
bool EnableP2POptimization(uint32_t gpu_a, uint32_t gpu_b);
bool CalibrateNetworkBandwidth(uint32_t gpu_a, uint32_t gpu_b);

// Statistics
NetworkStats GetNetworkStats(uint32_t gpu_a, uint32_t gpu_b) const;
```

---

## Logging Usage

```cpp
Logger::Log(LogLevel::DEBUG, "ComponentName", "Debug message");
Logger::Log(LogLevel::INFO, "ComponentName", "Info message");
Logger::Log(LogLevel::WARNING, "ComponentName", "Warning message");
Logger::Log(LogLevel::ERROR, "ComponentName", "Error message");
```

---

## Error Handling Pattern

```cpp
bool result = someOperation();
if (!result) {
    // Proper error logging has already been done by the component
    return false;
}
// Proceed with next operation
```

---

## Thread Safety

All operations are thread-safe:
- Use the objects from multiple threads safely
- No manual synchronization needed
- Atomic counters handle metrics safely
- Locks used internally where needed

---

## Memory Lifecycle

```cpp
// Allocation
GPUBuffer* buffer = mem_mgr->AllocateBuffer(...);

// Use
void* mapped = mem_mgr->MapBuffer(buffer);
// ... modify data ...
mem_mgr->UnmapBuffer(buffer);

// Cleanup (automatic in RAII or manual)
mem_mgr->DeallocateBuffer(buffer);
```

---

## Complete Integration Example

```cpp
// 1. Initialize devices
auto device_mgr = std::make_unique<GPUDeviceManager>();
device_mgr->Initialize(true);

// 2. Create memory manager
auto mem_mgr = std::make_unique<GPUMemoryManager>(device_mgr.get());

// 3. Allocate GPU memory
auto gpu_buffer = mem_mgr->AllocateBuffer(
    10 * 1024 * 1024,  // 10 MB
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    "ComputeBuffer");

// 4. Load shader
auto shader_compiler = std::make_unique<ShaderCompiler>(device_mgr.get());
auto shader = shader_compiler->CompileAndLoad("matmul", "matmul.spv");
auto pipeline = shader_compiler->CreateComputePipeline("matmul");

// 5. Create command recorder
auto cmd_recorder = std::make_unique<CommandBufferRecorder>(device_mgr.get());
auto cmd_buf = cmd_recorder->AllocateCommandBuffer();

// 6. Record and submit
cmd_recorder->BeginRecording(cmd_buf);
cmd_recorder->BindComputePipeline(cmd_buf, pipeline);
cmd_recorder->DispatchCompute(cmd_buf, 1024, 1, 1);
cmd_recorder->EndRecording(cmd_buf);
cmd_recorder->Submit(cmd_buf);

// 7. Wait for completion
cmd_recorder->WaitForCompletion(cmd_buf, 10000000000ULL);  // 10 seconds

// 8. Cleanup (automatic via unique_ptr)
```

---

**API Version: 1.0**
**Status: Production Ready** ✅
