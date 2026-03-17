#pragma once

#include "vulkan_core_phase3.h"
#include <map>
#include <set>
#include <condition_variable>
#include <functional>

// ========================================================================
// PHASE 4: EXTREME PERFORMANCE & MULTI-GPU FEATURES
// ========================================================================
// Multi-GPU load balancing, async operations with callbacks, GPU clock
// management, power/performance auto-tuning, GPU-accelerated networking.

namespace RawrXD::GPU::Phase4 {

using namespace RawrXD::GPU::Phase2;
using namespace RawrXD::GPU::Phase3;

// =====================================================================
// MULTI-GPU SUPPORT
// =====================================================================

enum class GPULoadBalancingStrategy {
    ROUND_ROBIN,       // Simple round-robin distribution
    LEAST_LOADED,      // Assign to least busy GPU
    MEMORY_AWARE,      // Consider available GPU memory
    PERFORMANCE_AWARE, // Route to fastest GPU for task
    AFFINITY_BASED,    // Keep related work together
    DYNAMIC            // Runtime optimization
};

struct GPUDevice {
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue compute_queue = VK_NULL_HANDLE;
    uint32_t device_index = 0;
    std::string device_name;
    float compute_power = 0.0f;
    VkDeviceSize available_memory = 0;
    VkDeviceSize total_memory = 0;
    float utilization = 0.0f;  // 0.0-1.0
    bool is_active = true;
};

struct GPUWorkItem {
    std::function<bool(VkDevice, VkQueue)> work;
    uint32_t preferred_gpu = 0;
    uint32_t assigned_gpu = 0;
    int priority = 0;  // Higher = more important
    std::atomic<bool> completed{false};
    std::chrono::high_resolution_clock::time_point submission_time;
};

class MultiGPUManager {
public:
    MultiGPUManager();
    ~MultiGPUManager();

    // GPU Discovery & Initialization
    bool Initialize(GPULoadBalancingStrategy strategy = GPULoadBalancingStrategy::DYNAMIC);
    uint32_t GetGPUCount() const { return gpu_devices_.size(); }
    const GPUDevice& GetGPU(uint32_t index) const;
    GPUDevice& GetGPU(uint32_t index);

    // Load Balancing
    void SetLoadBalancingStrategy(GPULoadBalancingStrategy strategy) { strategy_ = strategy; }
    uint32_t SelectGPUForWork(const GPUWorkItem& work_item, size_t data_size = 0);
    void UpdateGPUUtilization(uint32_t gpu_index, float utilization);

    // Multi-GPU Work Distribution
    bool SubmitWork(GPUWorkItem& work_item);
    bool WaitForCompletion(const GPUWorkItem& work_item, uint64_t timeout_ns = UINT64_MAX);

    // GPU Affinity & Communication
    bool EnablePeerAccess(uint32_t gpu_a, uint32_t gpu_b);
    bool DisablePeerAccess(uint32_t gpu_a, uint32_t gpu_b);
    bool TransferBetweenGPUs(uint32_t src_gpu, uint32_t dst_gpu, GPUBuffer* buffer);

    // Monitoring
    struct MultiGPUStats {
        uint32_t total_gpus = 0;
        std::vector<float> gpu_utilizations;
        std::vector<VkDeviceSize> gpu_memory_usage;
        float avg_utilization = 0.0f;
        uint32_t active_gpus = 0;
    };
    MultiGPUStats GetStats() const;

private:
    bool DiscoverGPUs();
    uint32_t SelectGPURoundRobin();
    uint32_t SelectGPULeastLoaded();
    uint32_t SelectGPUMemoryAware(size_t required_memory);
    uint32_t SelectGPUPerformanceAware();

    std::vector<GPUDevice> gpu_devices_;
    GPULoadBalancingStrategy strategy_ = GPULoadBalancingStrategy::ROUND_ROBIN;
    std::atomic<uint32_t> current_round_robin_index_{0};
    std::map<std::pair<uint32_t, uint32_t>, bool> peer_access_enabled_;
    std::mutex mutex_;
};

// =====================================================================
// ASYNCHRONOUS OPERATIONS WITH CALLBACKS
// =====================================================================

using CompletionCallback = std::function<void(bool success, uint64_t execution_time_ns)>;
using ProgressCallback = std::function<void(float progress)>;

struct AsyncTask {
    uint32_t task_id = 0;
    std::string task_name;
    CompletionCallback completion_callback = nullptr;
    ProgressCallback progress_callback = nullptr;
    SyncPrimitive sync;
    std::atomic<bool> is_executing{false};
    std::atomic<bool> is_cancelled{false};
    uint64_t start_time_ns = 0;
    uint64_t end_time_ns = 0;
    float estimated_progress = 0.0f;
    int priority = 0;
};

class AsyncExecutor {
public:
    AsyncExecutor(GPUDeviceManager* device_mgr, SynchronizationManager* sync_mgr, uint32_t worker_threads = 4);
    ~AsyncExecutor();

    // Task submission
    uint32_t SubmitAsyncTask(const std::string& task_name, CompletionCallback callback,
                            ProgressCallback progress_callback = nullptr, int priority = 0);
    bool CancelTask(uint32_t task_id);
    bool WaitForTask(uint32_t task_id, uint64_t timeout_ns = UINT64_MAX);

    // Task tracking
    bool IsTaskRunning(uint32_t task_id) const;
    bool IsTaskCompleted(uint32_t task_id) const;
    float GetTaskProgress(uint32_t task_id) const;

    // Batch operations
    std::vector<uint32_t> SubmitBatchTasks(const std::vector<std::pair<std::string, CompletionCallback>>& tasks);
    bool WaitForBatch(const std::vector<uint32_t>& task_ids, uint64_t timeout_ns = UINT64_MAX);

    // Statistics
    struct AsyncStats {
        uint32_t total_tasks_submitted = 0;
        uint32_t tasks_completed = 0;
        uint32_t tasks_running = 0;
        uint32_t tasks_cancelled = 0;
        float avg_task_execution_time_ms = 0.0f;
        float avg_queue_wait_time_ms = 0.0f;
    };
    AsyncStats GetStats() const;

private:
    void WorkerThreadFunction();
    AsyncTask* GetTask(uint32_t task_id);

    GPUDeviceManager* device_mgr_ = nullptr;
    SynchronizationManager* sync_mgr_ = nullptr;

    std::unordered_map<uint32_t, std::unique_ptr<AsyncTask>> tasks_;
    std::priority_queue<std::pair<int, uint32_t>,  // (priority, task_id)
                       std::vector<std::pair<int, uint32_t>>,
                       std::greater<std::pair<int, uint32_t>>> task_queue_;

    std::vector<std::thread> worker_threads_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<uint32_t> next_task_id_{1};

    std::condition_variable task_available_;
    mutable std::mutex mutex_;
};

// =====================================================================
// GPU CLOCK & POWER MANAGEMENT
// =====================================================================

enum class ClockDomain {
    GRAPHICS,
    MEMORY,
    SHADER,
    VIDEO,
    UVD  // Unified Video Decoder
};

enum class PowerMode {
    POWER_SAVING,      // Minimize power consumption
    BALANCED,          // Balance power and performance
    PERFORMANCE,       // Maximum performance
    THERMALLY_LIMITED  // Throttled due to heat
};

struct GPUClockState {
    ClockDomain domain = ClockDomain::GRAPHICS;
    uint32_t current_clock_mhz = 0;
    uint32_t min_clock_mhz = 0;
    uint32_t max_clock_mhz = 0;
    uint32_t target_clock_mhz = 0;
    float current_voltage_v = 0.0f;
    bool boost_enabled = true;
};

struct PowerState {
    float current_power_draw_w = 0.0f;
    float power_limit_w = 0.0f;
    float current_temp_c = 0.0f;
    float max_safe_temp_c = 85.0f;
    float throttle_temp_c = 90.0f;
    PowerMode mode = PowerMode::BALANCED;
    bool thermal_throttling_active = false;
};

class GPUPowerManager {
public:
    GPUPowerManager(GPUDeviceManager* device_mgr);
    ~GPUPowerManager();

    // Clock management
    bool SetClockFrequency(ClockDomain domain, uint32_t frequency_mhz);
    bool EnableDynamicClockScaling(ClockDomain domain, bool enable);
    GPUClockState GetClockState(ClockDomain domain) const;

    // Power mode control
    bool SetPowerMode(PowerMode mode);
    PowerMode GetCurrentPowerMode() const { return current_power_state_.mode; }

    // Thermal management
    void UpdateTemperature(float temp_c);
    void UpdatePowerDraw(float power_w);
    bool CheckThermalThrottling() const { return current_power_state_.thermal_throttling_active; }

    // Auto-optimization
    bool AutoOptimizeForWorkload(uint64_t compute_intensity);  // ops per byte
    bool AutoOptimizeForThermals();
    bool AutoOptimizeForPower();

    // Monitoring
    PowerState GetPowerState() const { return current_power_state_; }
    struct PowerStats {
        float avg_power_draw_w = 0.0f;
        float peak_power_draw_w = 0.0f;
        float avg_temp_c = 0.0f;
        float peak_temp_c = 0.0f;
        uint64_t thermal_throttle_time_ns = 0;
        float energy_efficiency_gflops_per_w = 0.0f;
    };
    PowerStats GetPowerStats() const;

private:
    GPUDeviceManager* device_mgr_ = nullptr;
    std::map<ClockDomain, GPUClockState> clock_states_;
    PowerState current_power_state_;
    std::deque<float> power_history_;
    std::deque<float> temp_history_;
    mutable std::mutex mutex_;
};

// =====================================================================
// NETWORK-ACCELERATED GPU OPERATIONS
// =====================================================================

enum class NetworkDataType {
    FLOAT32,
    FLOAT16,
    INT8,
    INT32,
    UINT8,
    UINT32
};

struct NetworkTransferConfig {
    NetworkDataType data_type = NetworkDataType::FLOAT32;
    bool compress = false;
    bool encrypt = false;
    uint32_t batch_size = 1024;
    uint32_t num_workers = 4;
    bool use_rdma = true;  // RDMA when available
    uint32_t timeout_ms = 5000;
};

class GPUNetworkAccelerator {
public:
    GPUNetworkAccelerator(GPUDeviceManager* device_mgr, GPUMemoryManager* mem_mgr);
    ~GPUNetworkAccelerator();

    // GPU-to-GPU network transfers
    bool SendGPUData(uint32_t src_gpu, uint32_t dst_gpu, GPUBuffer* buffer, 
                     const NetworkTransferConfig& config);
    bool ReceiveGPUData(uint32_t dst_gpu, GPUBuffer* buffer_out, const std::string& from_address,
                       const NetworkTransferConfig& config);

    // Distributed tensor operations
    bool AllReduce(const std::vector<uint32_t>& gpu_indices, GPUBuffer* buffer);
    bool AllGather(const std::vector<uint32_t>& gpu_indices, const std::vector<GPUBuffer*>& buffers);
    bool Broadcast(const std::vector<uint32_t>& gpu_indices, GPUBuffer* buffer, uint32_t root_gpu);

    // Network-based synchronization
    bool BarrierSync(const std::vector<uint32_t>& gpu_indices, uint64_t timeout_ns = UINT64_MAX);

    // Streaming operations
    bool StartStreamingTransfer(uint32_t src_gpu, uint32_t dst_gpu, GPUBuffer* buffer,
                               CompletionCallback callback);
    bool StopStreamingTransfer(uint32_t transfer_id);

    // Network optimizations
    bool EnableP2POptimization(uint32_t gpu_a, uint32_t gpu_b);
    bool CalibrateNetworkBandwidth(uint32_t gpu_a, uint32_t gpu_b);

    // Monitoring
    struct NetworkStats {
        float achieved_bandwidth_gbps = 0.0f;
        float theoretical_max_bandwidth_gbps = 0.0f;
        uint64_t bytes_transferred = 0;
        uint64_t transfers_completed = 0;
        float avg_transfer_latency_us = 0.0f;
        float transfer_efficiency_percent = 0.0f;
    };
    NetworkStats GetNetworkStats(uint32_t gpu_a, uint32_t gpu_b) const;

private:
    struct TransferContext {
        uint32_t transfer_id = 0;
        uint32_t src_gpu = 0;
        uint32_t dst_gpu = 0;
        GPUBuffer* buffer = nullptr;
        std::atomic<bool> is_active{false};
        CompletionCallback callback = nullptr;
        uint64_t bytes_transferred = 0;
    };

    GPUDeviceManager* device_mgr_ = nullptr;
    GPUMemoryManager* mem_mgr_ = nullptr;

    std::unordered_map<uint32_t, TransferContext> active_transfers_;
    std::map<std::pair<uint32_t, uint32_t>, NetworkStats> network_stats_;
    std::atomic<uint32_t> next_transfer_id_{1};
    std::mutex mutex_;
};

}  // namespace RawrXD::GPU::Phase4
