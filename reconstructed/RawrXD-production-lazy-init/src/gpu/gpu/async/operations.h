#pragma once

#include <vulkan/vulkan.h>
#include <functional>
#include <memory>
#include <queue>
#include <vector>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QEvent>

namespace RawrXD {
namespace GPU {

// Event types for GPU operations
enum class GPUEventType {
    COMPUTE_COMPLETE,
    TRANSFER_COMPLETE,
    VALIDATION_COMPLETE,
    ERROR_OCCURRED,
    MEMORY_LOW,
    THERMAL_WARNING
};

// GPU Operation Status
enum class GPUOperationStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

// GPU Event Callback
using GPUEventCallback = std::function<void(GPUEventType event_type, void* user_data)>;
using GPUCompletionCallback = std::function<void(bool success, const std::string& error)>;

// GPU Fence for synchronization
struct GPUFence {
    VkFence fence = VK_NULL_HANDLE;
    uint32_t device_id = 0;
    bool is_signaled = false;
    std::chrono::system_clock::time_point creation_time;
};

// GPU Event
struct GPUEvent {
    VkEvent event = VK_NULL_HANDLE;
    uint32_t device_id = 0;
    GPUEventType event_type = GPUEventType::COMPUTE_COMPLETE;
    std::vector<GPUEventCallback> callbacks;
    void* user_data = nullptr;
};

// GPU Command
struct GPUCommand {
    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
    uint32_t device_id = 0;
    std::string name;
    GPUEventCallback callback;
    void* user_data = nullptr;
    std::chrono::system_clock::time_point submit_time;
    GPUOperationStatus status = GPUOperationStatus::PENDING;
};

// GPU Async Operation
class GPUAsyncOperation {
public:
    explicit GPUAsyncOperation(uint32_t device_id, const std::string& name = "");
    ~GPUAsyncOperation();

    // Submit compute operation
    bool submit_compute(VkCommandBuffer cmd_buffer, GPUCompletionCallback callback);

    // Submit memory transfer
    bool submit_transfer(VkCommandBuffer cmd_buffer, uint64_t size, 
                        GPUCompletionCallback callback);

    // Create fence for synchronization
    GPUFence* create_fence();

    // Wait for fence
    bool wait_fence(GPUFence* fence, uint64_t timeout_ns = 0);

    // Create event
    GPUEvent* create_event(GPUEventType event_type);

    // Set event
    bool set_event(GPUEvent* event);

    // Reset event
    bool reset_event(GPUEvent* event);

    // Register event callback
    void register_callback(GPUEvent* event, GPUEventCallback callback);

    // Unregister event callback
    void unregister_callback(GPUEvent* event, GPUEventCallback callback);

    // Get operation status
    GPUOperationStatus get_status() const;

    // Get device ID
    uint32_t get_device_id() const { return device_id_; }

    // Get operation name
    const std::string& get_name() const { return name_; }

    // Cancel operation
    bool cancel();

    // Get execution time
    uint64_t get_execution_time_ms() const;

private:
    uint32_t device_id_;
    std::string name_;
    GPUOperationStatus status_ = GPUOperationStatus::PENDING;

    std::vector<std::unique_ptr<GPUFence>> fences_;
    std::vector<std::unique_ptr<GPUEvent>> events_;
    std::vector<GPUCommand> commands_;

    std::chrono::system_clock::time_point submit_time_;
    std::chrono::system_clock::time_point completion_time_;

    mutable QMutex mutex_;

    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
};

// GPU Command Stream - manages command buffer recording and submission
class GPUCommandStream : public QObject {
    Q_OBJECT

public:
    explicit GPUCommandStream(uint32_t device_id);
    ~GPUCommandStream();

    // Initialize command stream
    bool initialize(VkDevice device, VkQueue queue, VkCommandPool cmd_pool);

    // Begin recording commands
    bool begin_record();

    // End recording commands
    bool end_record();

    // Submit recorded commands
    bool submit(GPUCompletionCallback callback = nullptr);

    // Wait for completion
    bool wait_completion(uint64_t timeout_ms = 0);

    // Clear recorded commands
    void clear();

    // Get current command buffer
    VkCommandBuffer get_current_buffer() const { return current_cmd_buffer_; }

    // Check if recording
    bool is_recording() const { return is_recording_; }

signals:
    void operationCompleted(bool success, const QString& message);
    void operationProgress(int percentage);

private:
    uint32_t device_id_;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool cmd_pool_ = VK_NULL_HANDLE;
    VkCommandBuffer current_cmd_buffer_ = VK_NULL_HANDLE;

    bool is_recording_ = false;
    std::unique_ptr<GPUAsyncOperation> pending_operation_;

    mutable QMutex mutex_;

    bool allocate_command_buffer();
    void free_command_buffer();
};

// GPU Event Loop - handles async operations and callbacks
class GPUEventLoop : public QObject {
    Q_OBJECT

public:
    static GPUEventLoop& instance();

    // Initialize event loop
    bool initialize();

    // Start processing events
    void start();

    // Stop processing events
    void stop();

    // Register operation
    void register_operation(std::shared_ptr<GPUAsyncOperation> operation);

    // Wait for all pending operations
    bool wait_all_operations(uint64_t timeout_ms = 0);

    // Get number of pending operations
    size_t get_pending_count() const;

    // Enable event tracing
    void enable_tracing(bool enable);

    // Get performance statistics
    struct EventLoopStats {
        uint64_t total_operations = 0;
        uint64_t completed_operations = 0;
        uint64_t failed_operations = 0;
        uint64_t total_compute_time_ms = 0;
        uint64_t total_transfer_time_ms = 0;
        double average_latency_ms = 0.0;
    };

    EventLoopStats get_statistics() const;

signals:
    void operationStarted(const QString& operation_name);
    void operationCompleted(const QString& operation_name, bool success);
    void eventTriggered(int event_type, const QString& data);

private:
    GPUEventLoop() = default;
    ~GPUEventLoop() = default;

    std::queue<std::shared_ptr<GPUAsyncOperation>> pending_operations_;
    std::vector<std::shared_ptr<GPUAsyncOperation>> completed_operations_;

    std::unique_ptr<QThread> worker_thread_;

    bool is_running_ = false;
    bool tracing_enabled_ = false;

    EventLoopStats stats_;

    mutable QMutex mutex_;
    QWaitCondition operation_available_;

    void process_operations();

private slots:
    void on_worker_thread_started();
};

// Async GPU Executor - high-level interface for async GPU operations
class AsyncGPUExecutor {
public:
    static AsyncGPUExecutor& instance();

    // Execute compute kernel asynchronously
    bool async_compute(uint32_t device_id, VkCommandBuffer cmd_buffer,
                      const std::string& kernel_name,
                      GPUCompletionCallback callback);

    // Execute memory transfer asynchronously
    bool async_transfer(uint32_t device_id, uint64_t size,
                       GPUCompletionCallback callback);

    // Wait for specific operation
    bool wait_operation(uint32_t operation_id, uint64_t timeout_ms = 0);

    // Cancel pending operation
    bool cancel_operation(uint32_t operation_id);

    // Get operation status
    GPUOperationStatus get_operation_status(uint32_t operation_id);

    // Create compute pipeline for async execution
    std::shared_ptr<GPUCommandStream> create_compute_stream(uint32_t device_id);

    // Get execution queue for device
    VkQueue get_queue(uint32_t device_id);

private:
    AsyncGPUExecutor() = default;
    ~AsyncGPUExecutor() = default;

    std::map<uint32_t, std::shared_ptr<GPUCommandStream>> compute_streams_;
    std::map<uint32_t, std::shared_ptr<GPUAsyncOperation>> operations_;

    uint32_t next_operation_id_ = 0;

    mutable QMutex mutex_;

    uint32_t allocate_operation_id();
};

} // namespace GPU
} // namespace RawrXD
