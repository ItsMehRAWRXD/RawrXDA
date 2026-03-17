#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <QMutex>
#include <QThread>

namespace RawrXD {
namespace GPU {

// GPU Device Information
struct GPUDeviceInfo {
    uint32_t device_id = 0;
    std::string device_name;
    uint64_t total_memory = 0;
    uint64_t available_memory = 0;
    float compute_capability = 0.0f;
    uint32_t compute_units = 0;
    uint32_t max_clock_mhz = 0;
    uint32_t current_clock_mhz = 0;
    float power_usage_watts = 0.0f;
    float temperature_celsius = 0.0f;
    bool is_available = false;
};

// GPU Workload
struct GPUWorkload {
    uint32_t workload_id = 0;
    uint32_t target_device_id = 0;
    uint64_t estimated_compute = 0;
    uint64_t estimated_memory = 0;
    int priority = 0;
    bool is_assigned = false;
    std::chrono::system_clock::time_point submit_time;
};

// Device Load Statistics
struct DeviceLoadStats {
    float utilization = 0.0f;  // 0-100%
    float memory_usage = 0.0f; // 0-100%
    float thermal_load = 0.0f; // 0-100%
    uint64_t pending_operations = 0;
    uint64_t total_throughput_gbs = 0;
};

// Multi-GPU Load Balancer
class MultiGPULoadBalancer {
public:
    static MultiGPULoadBalancer& instance();

    // Initialize load balancer
    bool initialize(const std::vector<GPUDeviceInfo>& devices);

    // Register GPU device
    bool register_device(const GPUDeviceInfo& device);

    // Get device info
    GPUDeviceInfo* get_device(uint32_t device_id);

    // Get all devices
    std::vector<GPUDeviceInfo> get_all_devices() const;

    // Update device statistics
    bool update_device_stats(uint32_t device_id, const DeviceLoadStats& stats);

    // Assign workload to best device
    bool assign_workload(const GPUWorkload& workload, uint32_t& assigned_device_id);

    // Assign workload to specific device
    bool assign_to_device(uint32_t device_id, const GPUWorkload& workload);

    // Get device load
    DeviceLoadStats get_device_load(uint32_t device_id);

    // Get total GPU utilization
    float get_total_utilization();

    // Get device count
    uint32_t get_device_count() const;

    // Enable load balancing
    void enable_load_balancing(bool enable);

    // Get balancing statistics
    struct BalancingStats {
        uint64_t total_workloads = 0;
        uint64_t completed_workloads = 0;
        uint64_t failed_workloads = 0;
        float average_load = 0.0f;
        uint32_t most_utilized_device = 0;
        uint32_t least_utilized_device = 0;
    };

    BalancingStats get_statistics() const;

private:
    MultiGPULoadBalancer() = default;
    ~MultiGPULoadBalancer() = default;

    std::map<uint32_t, GPUDeviceInfo> devices_;
    std::map<uint32_t, DeviceLoadStats> device_stats_;
    std::vector<GPUWorkload> pending_workloads_;

    bool load_balancing_enabled_ = true;
    BalancingStats stats_;

    mutable QMutex mutex_;

    // Find best device for workload
    uint32_t find_best_device(const GPUWorkload& workload);

    // Calculate load score (lower is better)
    float calculate_load_score(uint32_t device_id, const GPUWorkload& workload);
};

// GPU Task Scheduler - schedules tasks across multiple GPUs
class GPUTaskScheduler : public QObject {
    Q_OBJECT

public:
    static GPUTaskScheduler& instance();

    // Initialize scheduler
    bool initialize(uint32_t num_devices);

    // Submit task
    uint32_t submit_task(uint32_t device_id, std::function<bool()> task,
                        int priority = 0);

    // Submit distributed task
    bool submit_distributed_task(std::function<bool(uint32_t)> task,
                                int priority = 0);

    // Cancel task
    bool cancel_task(uint32_t task_id);

    // Wait for task
    bool wait_task(uint32_t task_id, uint64_t timeout_ms = 0);

    // Get task status
    enum class TaskStatus {
        PENDING,
        RUNNING,
        COMPLETED,
        FAILED,
        CANCELLED
    };

    TaskStatus get_task_status(uint32_t task_id);

    // Start scheduler
    void start();

    // Stop scheduler
    void stop();

    // Get scheduler statistics
    struct SchedulerStats {
        uint64_t total_tasks = 0;
        uint64_t completed_tasks = 0;
        uint64_t failed_tasks = 0;
        float average_task_latency_ms = 0.0f;
        uint32_t pending_task_count = 0;
    };

    SchedulerStats get_statistics() const;

signals:
    void taskSubmitted(uint32_t task_id);
    void taskStarted(uint32_t task_id);
    void taskCompleted(uint32_t task_id, bool success);

private:
    GPUTaskScheduler() = default;
    ~GPUTaskScheduler() = default;

    struct Task {
        uint32_t task_id = 0;
        uint32_t device_id = 0;
        std::function<bool()> function;
        TaskStatus status = TaskStatus::PENDING;
        int priority = 0;
        std::chrono::system_clock::time_point submit_time;
    };

    std::map<uint32_t, Task> tasks_;
    std::vector<std::shared_ptr<QThread>> device_threads_;

    uint32_t next_task_id_ = 0;
    bool is_running_ = false;

    SchedulerStats stats_;

    mutable QMutex mutex_;

    void worker_thread_main(uint32_t device_id);
    uint32_t allocate_task_id();
};

// GPU Affinity Manager - manages GPU/CPU affinity for optimal performance
class GPUAffinityManager {
public:
    static GPUAffinityManager& instance();

    // Initialize affinity manager
    bool initialize();

    // Set CPU affinity for GPU device
    bool set_cpu_affinity(uint32_t device_id, const std::vector<uint32_t>& cpu_cores);

    // Get optimal CPU cores for device
    std::vector<uint32_t> get_optimal_cpu_cores(uint32_t device_id);

    // Set NUMA affinity
    bool set_numa_affinity(uint32_t device_id, uint32_t numa_node);

    // Get NUMA node for device
    uint32_t get_numa_node(uint32_t device_id);

    // Pin thread to GPU device
    bool pin_thread_to_device(uint32_t device_id);

    // Unpin thread
    bool unpin_thread();

private:
    GPUAffinityManager() = default;
    ~GPUAffinityManager() = default;

    std::map<uint32_t, std::vector<uint32_t>> device_cpu_affinity_;
    std::map<uint32_t, uint32_t> device_numa_affinity_;

    mutable QMutex mutex_;
};

} // namespace GPU
} // namespace RawrXD
