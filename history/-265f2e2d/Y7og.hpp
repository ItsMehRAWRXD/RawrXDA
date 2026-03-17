#ifndef QT_ASYNC_CALLBACKS_HPP
#define QT_ASYNC_CALLBACKS_HPP

#include <QString>
#include <QByteArray>
#include <functional>
#include <cstdint>

// =============================================================================
// Qt Async Callbacks - Callback Type Definitions
// =============================================================================
// Defines all callback function signatures used by the async thread pool
// All callbacks are thread-safe and can be invoked from worker threads
// =============================================================================

/**
 * @struct AsyncWorkItem
 * @brief Represents a single asynchronous work item
 */
struct AsyncWorkItem {
    // Work identification
    uint64_t id;
    int operation_type;         // Read, write, copy, etc.
    int status;                 // Pending, running, complete, error, etc.
    
    // File operation details
    int file_handle;
    uint64_t offset;
    uint64_t buffer_size;
    uint8_t* buffer;
    
    // Execution context
    void* user_context;
    int priority;
    
    // Result information
    int result_code;
    uint64_t bytes_processed;
    
    // Timing
    uint64_t start_time_ms;
    uint64_t end_time_ms;
    
    AsyncWorkItem()
        : id(0), operation_type(0), status(0), file_handle(-1),
          offset(0), buffer_size(0), buffer(nullptr), user_context(nullptr),
          priority(0), result_code(0), bytes_processed(0),
          start_time_ms(0), end_time_ms(0) {}
};

/**
 * @struct AsyncProgress
 * @brief Progress information for ongoing operations
 */
struct AsyncProgress {
    uint64_t bytes_processed;           // Bytes processed so far
    uint64_t total_bytes;               // Total bytes to process (-1 = unknown)
    int percentage;                     // Progress percentage (0-100)
    uint64_t elapsed_ms;                // Elapsed time in milliseconds
    uint64_t estimated_remaining_ms;    // Estimated time remaining (-1 = unknown)
    
    // Calculated properties
    double bytes_per_second;            // Current throughput
    
    AsyncProgress()
        : bytes_processed(0), total_bytes(0), percentage(0),
          elapsed_ms(0), estimated_remaining_ms(0), bytes_per_second(0.0) {}
    
    bool isComplete() const {
        return total_bytes > 0 && bytes_processed >= total_bytes;
    }
    
    bool isIndeterminate() const {
        return total_bytes == 0 || total_bytes == static_cast<uint64_t>(-1);
    }
};

/**
 * @struct AsyncResult
 * @brief Result of an async operation
 */
struct AsyncResult {
    bool success;
    int error_code;
    QString error_message;
    uint64_t bytes_processed;
    uint64_t elapsed_ms;
    
    AsyncResult()
        : success(true), error_code(0), bytes_processed(0), elapsed_ms(0) {}
    
    static AsyncResult success(uint64_t bytes, uint64_t time) {
        AsyncResult r;
        r.success = true;
        r.error_code = 0;
        r.bytes_processed = bytes;
        r.elapsed_ms = time;
        return r;
    }
    
    static AsyncResult failure(int code, const QString& msg, uint64_t time) {
        AsyncResult r;
        r.success = false;
        r.error_code = code;
        r.error_message = msg;
        r.elapsed_ms = time;
        return r;
    }
};

// =============================================================================
// Callback Type Definitions
// =============================================================================

/**
 * @typedef WorkCompleteCallback
 * @brief Called when a work item completes
 * @param result The result of the operation
 * @param context User-provided context
 * 
 * Thread Safety: Called from worker thread, may require synchronization
 */
using WorkCompleteCallback = std::function<void(const AsyncResult& result, void* context)>;

/**
 * @typedef ProgressCallback
 * @brief Called periodically to report progress
 * @param progress Current progress information
 * @param context User-provided context
 * 
 * Thread Safety: Called from worker thread, use for UI updates with caution
 * Note: Avoid heavy operations in this callback
 */
using ProgressCallback = std::function<void(const AsyncProgress& progress, void* context)>;

/**
 * @typedef ErrorCallback
 * @brief Called when an error occurs during operation
 * @param error_code Error code
 * @param error_message Descriptive error message
 * @param context User-provided context
 * 
 * Thread Safety: Called from worker thread
 * Note: Do not throw exceptions in this callback
 */
using ErrorCallback = std::function<void(int error_code, const QString& error_message, void* context)>;

/**
 * @typedef WorkerThreadFunction
 * @brief Custom work function executed by thread pool
 * @param work_item The work item to process
 * @return Result code (0 = success, non-zero = error)
 * 
 * Thread Safety: Executed from worker thread, must be thread-safe
 * Note: Keep operations relatively quick; very long operations block thread
 */
using WorkerThreadFunction = std::function<int(const AsyncWorkItem& work_item)>;

// =============================================================================
// Callback Wrapper Classes (for Qt Integration)
// =============================================================================

/**
 * @class AsyncCallbackHandler
 * @brief Manages async operation callbacks with thread safety
 */
class AsyncCallbackHandler {
public:
    AsyncCallbackHandler() = default;
    virtual ~AsyncCallbackHandler() = default;
    
    // Callback setters
    void setOnComplete(WorkCompleteCallback cb) {
        m_on_complete = cb;
    }
    
    void setOnProgress(ProgressCallback cb) {
        m_on_progress = cb;
    }
    
    void setOnError(ErrorCallback cb) {
        m_on_error = cb;
    }
    
    // Callback invocation (thread-safe)
    void invokeOnComplete(const AsyncResult& result, void* context) {
        if (m_on_complete) {
            m_on_complete(result, context);
        }
    }
    
    void invokeOnProgress(const AsyncProgress& progress, void* context) {
        if (m_on_progress) {
            m_on_progress(progress, context);
        }
    }
    
    void invokeOnError(int code, const QString& msg, void* context) {
        if (m_on_error) {
            m_on_error(code, msg, context);
        }
    }
    
    bool hasCompleteCallback() const { return static_cast<bool>(m_on_complete); }
    bool hasProgressCallback() const { return static_cast<bool>(m_on_progress); }
    bool hasErrorCallback() const { return static_cast<bool>(m_on_error); }
    
protected:
    WorkCompleteCallback m_on_complete;
    ProgressCallback m_on_progress;
    ErrorCallback m_on_error;
};

// =============================================================================
// Operation Type Enumeration
// =============================================================================

enum class AsyncOperationType {
    None = 0,
    FileRead = 1,
    FileWrite = 2,
    FileCopy = 3,
    FileDelete = 4,
    DirectoryCreate = 5,
    DirectoryDelete = 6,
    DirectoryList = 7,
    CustomWork = 8
};

// =============================================================================
// Error Code Enumeration
// =============================================================================

enum class AsyncErrorCode {
    Success = 0,
    ThreadCreationFailed = 1,
    ThreadJoinFailed = 2,
    LockAcquisitionFailed = 3,
    LockReleaseFailed = 4,
    EventCreationFailed = 5,
    EventSignalFailed = 6,
    QueueFull = 7,
    QueueEmpty = 8,
    PoolNotRunning = 9,
    InvalidWorkItem = 10,
    FileOpenFailed = 11,
    FileReadFailed = 12,
    FileWriteFailed = 13,
    BufferTooSmall = 14,
    TimeoutOccurred = 15,
    OperationCancelled = 16,
    OutOfMemory = 17,
    InvalidParameter = 18,
    NotImplemented = 19,
    Unknown = 99
};

// =============================================================================
// Status Code Enumeration
// =============================================================================

enum class AsyncStatus {
    Pending = 0,        // Waiting to be queued
    Queued = 1,         // In work queue, waiting for worker thread
    Running = 2,        // Currently executing
    Complete = 3,       // Finished successfully
    Cancelled = 4,      // Cancelled by user
    Error = 5           // Completed with error
};

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Convert error code to human-readable string
 */
inline QString asyncErrorCodeString(AsyncErrorCode code) {
    switch (code) {
        case AsyncErrorCode::Success: return "Success";
        case AsyncErrorCode::ThreadCreationFailed: return "Thread creation failed";
        case AsyncErrorCode::ThreadJoinFailed: return "Thread join failed";
        case AsyncErrorCode::LockAcquisitionFailed: return "Lock acquisition failed";
        case AsyncErrorCode::LockReleaseFailed: return "Lock release failed";
        case AsyncErrorCode::EventCreationFailed: return "Event creation failed";
        case AsyncErrorCode::EventSignalFailed: return "Event signal failed";
        case AsyncErrorCode::QueueFull: return "Queue full";
        case AsyncErrorCode::QueueEmpty: return "Queue empty";
        case AsyncErrorCode::PoolNotRunning: return "Thread pool not running";
        case AsyncErrorCode::InvalidWorkItem: return "Invalid work item";
        case AsyncErrorCode::FileOpenFailed: return "File open failed";
        case AsyncErrorCode::FileReadFailed: return "File read failed";
        case AsyncErrorCode::FileWriteFailed: return "File write failed";
        case AsyncErrorCode::BufferTooSmall: return "Buffer too small";
        case AsyncErrorCode::TimeoutOccurred: return "Timeout occurred";
        case AsyncErrorCode::OperationCancelled: return "Operation cancelled";
        case AsyncErrorCode::OutOfMemory: return "Out of memory";
        case AsyncErrorCode::InvalidParameter: return "Invalid parameter";
        case AsyncErrorCode::NotImplemented: return "Not implemented";
        case AsyncErrorCode::Unknown: return "Unknown error";
        default: return QString("Unknown error code: %1").arg((int)code);
    }
}

/**
 * Convert status code to human-readable string
 */
inline QString asyncStatusString(AsyncStatus status) {
    switch (status) {
        case AsyncStatus::Pending: return "Pending";
        case AsyncStatus::Queued: return "Queued";
        case AsyncStatus::Running: return "Running";
        case AsyncStatus::Complete: return "Complete";
        case AsyncStatus::Cancelled: return "Cancelled";
        case AsyncStatus::Error: return "Error";
        default: return "Unknown";
    }
}

#endif // QT_ASYNC_CALLBACKS_HPP
