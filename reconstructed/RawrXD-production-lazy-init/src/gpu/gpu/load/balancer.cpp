#include "gpu_load_balancer.h"
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>

namespace RawrXD {
namespace GPU {

// ============================================================================
// MultiGPULoadBalancer Implementation
// ============================================================================

MultiGPULoadBalancer& MultiGPULoadBalancer::instance() {
    static MultiGPULoadBalancer balancer;
    return balancer;
}

bool MultiGPULoadBalancer::initialize(const std::vector<GPUDeviceInfo>& devices) {
    QMutexLocker lock(&mutex_);

    devices_.clear();
    device_stats_.clear();

    for (const auto& device : devices) {
        devices_[device.device_id] = device;

        DeviceLoadStats stats;
        stats.utilization = 0.0f;
        stats.memory_usage = 0.0f;
        stats.thermal_load = 0.0f;
        stats.pending_operations = 0;

        device_stats_[device.device_id] = stats;
    }

    return !devices_.empty();
}

bool MultiGPULoadBalancer::register_device(const GPUDeviceInfo& device) {
    QMutexLocker lock(&mutex_);

    if (devices_.find(device.device_id) != devices_.end()) {
        return false; // Device already registered
    }

    devices_[device.device_id] = device;

    DeviceLoadStats stats;
    stats.utilization = 0.0f;
    stats.memory_usage = 0.0f;
    stats.thermal_load = 0.0f;

    device_stats_[device.device_id] = stats;

    return true;
}

GPUDeviceInfo* MultiGPULoadBalancer::get_device(uint32_t device_id) {
    QMutexLocker lock(&mutex_);

    auto it = devices_.find(device_id);
    if (it != devices_.end()) {
        return &it->second;
    }

    return nullptr;
}

std::vector<GPUDeviceInfo> MultiGPULoadBalancer::get_all_devices() const {
    QMutexLocker lock(&mutex_);

    std::vector<GPUDeviceInfo> result;
    for (const auto& [device_id, device_info] : devices_) {
        result.push_back(device_info);
    }

    return result;
}

bool MultiGPULoadBalancer::update_device_stats(uint32_t device_id,
                                              const DeviceLoadStats& stats) {
    QMutexLocker lock(&mutex_);

    auto it = device_stats_.find(device_id);
    if (it == device_stats_.end()) {
        return false;
    }

    it->second = stats;
    return true;
}

bool MultiGPULoadBalancer::assign_workload(const GPUWorkload& workload,
                                          uint32_t& assigned_device_id) {
    QMutexLocker lock(&mutex_);

    if (!load_balancing_enabled_) {
        return false;
    }

    uint32_t best_device = find_best_device(workload);
    if (devices_.find(best_device) == devices_.end()) {
        return false;
    }

    assigned_device_id = best_device;
    stats_.total_workloads++;

    return true;
}

bool MultiGPULoadBalancer::assign_to_device(uint32_t device_id,
                                           const GPUWorkload& workload) {
    QMutexLocker lock(&mutex_);

    auto it = devices_.find(device_id);
    if (it == devices_.end()) {
        return false;
    }

    stats_.total_workloads++;
    return true;
}

DeviceLoadStats MultiGPULoadBalancer::get_device_load(uint32_t device_id) {
    QMutexLocker lock(&mutex_);

    auto it = device_stats_.find(device_id);
    if (it != device_stats_.end()) {
        return it->second;
    }

    return DeviceLoadStats();
}

float MultiGPULoadBalancer::get_total_utilization() {
    QMutexLocker lock(&mutex_);

    if (device_stats_.empty()) {
        return 0.0f;
    }

    float total = 0.0f;
    for (const auto& [device_id, stats] : device_stats_) {
        total += stats.utilization;
    }

    return total / device_stats_.size();
}

uint32_t MultiGPULoadBalancer::get_device_count() const {
    QMutexLocker lock(&mutex_);
    return devices_.size();
}

void MultiGPULoadBalancer::enable_load_balancing(bool enable) {
    QMutexLocker lock(&mutex_);
    load_balancing_enabled_ = enable;
}

MultiGPULoadBalancer::BalancingStats MultiGPULoadBalancer::get_statistics() const {
    QMutexLocker lock(&mutex_);

    BalancingStats result = stats_;

    if (!device_stats_.empty()) {
        float total_util = 0.0f;
        float min_util = 100.0f;
        float max_util = 0.0f;
        uint32_t min_device = 0;
        uint32_t max_device = 0;

        for (const auto& [device_id, stats] : device_stats_) {
            total_util += stats.utilization;

            if (stats.utilization < min_util) {
                min_util = stats.utilization;
                min_device = device_id;
            }

            if (stats.utilization > max_util) {
                max_util = stats.utilization;
                max_device = device_id;
            }
        }

        result.average_load = total_util / device_stats_.size();
        result.most_utilized_device = max_device;
        result.least_utilized_device = min_device;
    }

    return result;
}

uint32_t MultiGPULoadBalancer::find_best_device(const GPUWorkload& workload) {
    if (devices_.empty()) {
        return 0xFFFFFFFF;
    }

    uint32_t best_device = devices_.begin()->first;
    float best_score = std::numeric_limits<float>::max();

    for (const auto& [device_id, device_info] : devices_) {
        float score = calculate_load_score(device_id, workload);

        if (score < best_score) {
            best_score = score;
            best_device = device_id;
        }
    }

    return best_device;
}

float MultiGPULoadBalancer::calculate_load_score(uint32_t device_id,
                                                const GPUWorkload& workload) {
    auto it = device_stats_.find(device_id);
    if (it == device_stats_.end()) {
        return std::numeric_limits<float>::max();
    }

    const auto& stats = it->second;

    // Weighted load score: prioritize memory and utilization
    float memory_score = 0.0f;
    if (workload.estimated_memory > 0) {
        memory_score = (stats.memory_usage / 100.0f) * (workload.estimated_memory / 1e9f);
    }

    float utilization_score = (stats.utilization / 100.0f) *
                             (workload.estimated_compute / 1e12f);

    float thermal_score = (stats.thermal_load / 100.0f) * 0.1f;

    return memory_score + utilization_score + thermal_score;
}

// ============================================================================
// GPUTaskScheduler Implementation
// ============================================================================

GPUTaskScheduler& GPUTaskScheduler::instance() {
    static GPUTaskScheduler scheduler;
    return scheduler;
}

bool GPUTaskScheduler::initialize(uint32_t num_devices) {
    QMutexLocker lock(&mutex_);

    device_threads_.reserve(num_devices);
    for (uint32_t i = 0; i < num_devices; ++i) {
        device_threads_.push_back(std::make_shared<QThread>());
    }

    return true;
}

uint32_t GPUTaskScheduler::submit_task(uint32_t device_id,
                                      std::function<bool()> task,
                                      int priority) {
    QMutexLocker lock(&mutex_);

    if (device_id >= device_threads_.size()) {
        return 0xFFFFFFFF;
    }

    uint32_t task_id = allocate_task_id();

    Task t;
    t.task_id = task_id;
    t.device_id = device_id;
    t.function = task;
    t.status = TaskStatus::PENDING;
    t.priority = priority;
    t.submit_time = std::chrono::system_clock::now();

    tasks_[task_id] = t;
    stats_.total_tasks++;

    emit taskSubmitted(task_id);

    return task_id;
}

bool GPUTaskScheduler::submit_distributed_task(std::function<bool(uint32_t)> task,
                                              int priority) {
    QMutexLocker lock(&mutex_);

    for (uint32_t i = 0; i < device_threads_.size(); ++i) {
        auto wrapped_task = [task, i]() { return task(i); };
        submit_task(i, wrapped_task, priority);
    }

    return true;
}

bool GPUTaskScheduler::cancel_task(uint32_t task_id) {
    QMutexLocker lock(&mutex_);

    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return false;
    }

    if (it->second.status == TaskStatus::PENDING || it->second.status == TaskStatus::RUNNING) {
        it->second.status = TaskStatus::CANCELLED;
        return true;
    }

    return false;
}

bool GPUTaskScheduler::wait_task(uint32_t task_id, uint64_t timeout_ms) {
    QMutexLocker lock(&mutex_);

    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return false;
    }

    auto deadline = std::chrono::system_clock::now() +
                   std::chrono::milliseconds(timeout_ms);

    while (it->second.status == TaskStatus::PENDING ||
          it->second.status == TaskStatus::RUNNING) {
        if (timeout_ms > 0 && std::chrono::system_clock::now() > deadline) {
            return false;
        }

        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        lock.relock();

        it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return false;
        }
    }

    return it->second.status == TaskStatus::COMPLETED;
}

GPUTaskScheduler::TaskStatus GPUTaskScheduler::get_task_status(uint32_t task_id) {
    QMutexLocker lock(&mutex_);

    auto it = tasks_.find(task_id);
    if (it != tasks_.end()) {
        return it->second.status;
    }

    return TaskStatus::FAILED;
}

void GPUTaskScheduler::start() {
    QMutexLocker lock(&mutex_);

    if (is_running_) {
        return;
    }

    is_running_ = true;

    for (size_t i = 0; i < device_threads_.size(); ++i) {
        auto thread = device_threads_[i];
        connect(thread.get(), &QThread::started, this,
               [this, i]() { worker_thread_main(i); });
        thread->start();
    }
}

void GPUTaskScheduler::stop() {
    QMutexLocker lock(&mutex_);

    is_running_ = false;

    for (auto& thread : device_threads_) {
        if (thread) {
            thread->quit();
            thread->wait();
        }
    }
}

GPUTaskScheduler::SchedulerStats GPUTaskScheduler::get_statistics() const {
    QMutexLocker lock(&mutex_);

    SchedulerStats result = stats_;

    result.pending_task_count = 0;
    for (const auto& [task_id, task] : tasks_) {
        if (task.status == TaskStatus::PENDING || task.status == TaskStatus::RUNNING) {
            result.pending_task_count++;
        }
    }

    if (stats_.completed_tasks > 0) {
        // Calculate average latency (simplified)
        result.average_task_latency_ms = 1.0f; // Would calculate from task times
    }

    return result;
}

void GPUTaskScheduler::worker_thread_main(uint32_t device_id) {
    while (is_running_) {
        std::vector<uint32_t> tasks_to_run;

        {
            QMutexLocker lock(&mutex_);

            // Find tasks for this device
            for (auto& [task_id, task] : tasks_) {
                if (task.device_id == device_id && task.status == TaskStatus::PENDING) {
                    tasks_to_run.push_back(task_id);
                }
            }
        }

        // Execute tasks
        for (uint32_t task_id : tasks_to_run) {
            {
                QMutexLocker lock(&mutex_);

                auto it = tasks_.find(task_id);
                if (it != tasks_.end() && it->second.function) {
                    it->second.status = TaskStatus::RUNNING;
                    emit taskStarted(task_id);

                    lock.unlock();

                    bool success = it->second.function();

                    lock.relock();

                    it = tasks_.find(task_id);
                    if (it != tasks_.end()) {
                        it->second.status = success ? TaskStatus::COMPLETED : TaskStatus::FAILED;

                        if (success) {
                            stats_.completed_tasks++;
                        } else {
                            stats_.failed_tasks++;
                        }
                    }

                    lock.unlock();
                    emit taskCompleted(task_id, success);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

uint32_t GPUTaskScheduler::allocate_task_id() {
    return ++next_task_id_;
}

// ============================================================================
// GPUAffinityManager Implementation
// ============================================================================

GPUAffinityManager& GPUAffinityManager::instance() {
    static GPUAffinityManager manager;
    return manager;
}

bool GPUAffinityManager::initialize() {
    QMutexLocker lock(&mutex_);

    // Initialize system information
    // In a real implementation, would detect CPU/NUMA topology

    return true;
}

bool GPUAffinityManager::set_cpu_affinity(uint32_t device_id,
                                         const std::vector<uint32_t>& cpu_cores) {
    QMutexLocker lock(&mutex_);

    device_cpu_affinity_[device_id] = cpu_cores;
    return true;
}

std::vector<uint32_t> GPUAffinityManager::get_optimal_cpu_cores(uint32_t device_id) {
    QMutexLocker lock(&mutex_);

    auto it = device_cpu_affinity_.find(device_id);
    if (it != device_cpu_affinity_.end()) {
        return it->second;
    }

    // Return default affinity (all cores)
    std::vector<uint32_t> all_cores;
    uint32_t num_cores = std::thread::hardware_concurrency();
    for (uint32_t i = 0; i < num_cores; ++i) {
        all_cores.push_back(i);
    }

    return all_cores;
}

bool GPUAffinityManager::set_numa_affinity(uint32_t device_id, uint32_t numa_node) {
    QMutexLocker lock(&mutex_);

    device_numa_affinity_[device_id] = numa_node;
    return true;
}

uint32_t GPUAffinityManager::get_numa_node(uint32_t device_id) {
    QMutexLocker lock(&mutex_);

    auto it = device_numa_affinity_.find(device_id);
    if (it != device_numa_affinity_.end()) {
        return it->second;
    }

    return 0; // Default to NUMA node 0
}

bool GPUAffinityManager::pin_thread_to_device(uint32_t device_id) {
    QMutexLocker lock(&mutex_);

    auto cores = get_optimal_cpu_cores(device_id);
    if (cores.empty()) {
        return false;
    }

    // In a real implementation, would pin current thread to CPU cores
    // This is platform-specific (Linux: sched_setaffinity, Windows: SetThreadAffinityMask)

    return true;
}

bool GPUAffinityManager::unpin_thread() {
    QMutexLocker lock(&mutex_);

    // In a real implementation, would unpin thread and restore default affinity
    return true;
}

} // namespace GPU
} // namespace RawrXD
