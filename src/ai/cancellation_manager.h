// cancellation_manager.h - Instant cancellation fast-path for inference
// Implements immediate cancellation when user types again
//
// Critical for UX:
//   - When user types: current generation must die INSTANTLY
//   - Not "finish current token" - KILL IT
//   - Ghost text must not lag
//
// Strategy:
//   - Atomic cancellation flag checked every token
//   - GPU kernel abort (if supported)
//   - Clean resource cleanup
//   - No blocking waits
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace RawrXD {

// Cancellation reason
enum class CancellationReason : uint8_t {
    USER_TYPING = 0,      // User started typing again
    USER_CANCEL = 1,      // User pressed ESC
    TIMEOUT = 2,          // Generation timed out
    ERROR = 3,            // Error occurred
    CONTEXT_CHANGE = 4,   // Context changed (file switch, etc.)
    MODEL_SWITCH = 5,      // Model switched
};

// Cancellation statistics
struct CancellationStats {
    int total_cancellations;
    int user_typing_cancellations;
    int timeout_cancellations;
    int error_cancellations;
    std::chrono::microseconds avg_cancel_latency;
    std::chrono::microseconds max_cancel_latency;
};

// Cancellation handle for tracking
struct CancellationHandle {
    uint64_t id;
    std::atomic<bool> cancelled;
    CancellationReason reason;
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point cancelled_at;
};

// Cancellation manager
class CancellationManager {
public:
    CancellationManager();
    ~CancellationManager();
    
    // Create a new cancellation handle
    uint64_t CreateHandle();
    
    // Cancel a specific handle
    void Cancel(uint64_t handle_id, CancellationReason reason = CancellationReason::USER_TYPING);
    
    // Cancel all handles
    void CancelAll(CancellationReason reason = CancellationReason::USER_TYPING);
    
    // Check if handle is cancelled
    bool IsCancelled(uint64_t handle_id) const;
    
    // Get cancellation reason
    CancellationReason GetCancellationReason(uint64_t handle_id) const;
    
    // Destroy handle (cleanup)
    void DestroyHandle(uint64_t handle_id);
    
    // Register callback for cancellation
    void RegisterCallback(uint64_t handle_id, std::function<void()> callback);
    
    // Unregister callback
    void UnregisterCallback(uint64_t handle_id);
    
    // Wait for cancellation (with timeout)
    bool WaitForCancellation(uint64_t handle_id, std::chrono::milliseconds timeout);
    
    // Get statistics
    CancellationStats GetStats() const;
    
    // Reset statistics
    void ResetStats();
    
private:
    // Find handle
    CancellationHandle* FindHandle(uint64_t handle_id);
    const CancellationHandle* FindHandle(uint64_t handle_id) const;
    
    // Execute callbacks for handle
    void ExecuteCallbacks(uint64_t handle_id);
    
    // Members
    mutable std::mutex handles_mutex_;
    std::unordered_map<uint64_t, std::unique_ptr<CancellationHandle>> handles_;
    std::unordered_map<uint64_t, std::vector<std::function<void()>>> callbacks_;
    
    uint64_t next_handle_id_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    CancellationStats stats_;
};

// Inline implementations

inline uint64_t CancellationManager::CreateHandle() {
    std::lock_guard<std::mutex> lock(handles_mutex_);
    
    uint64_t id = next_handle_id_++;
    auto handle = std::make_unique<CancellationHandle>();
    handle->id = id;
    handle->cancelled.store(false);
    handle->reason = CancellationReason::USER_TYPING;
    handle->created = std::chrono::steady_clock::now();
    
    handles_[id] = std::move(handle);
    
    return id;
}

inline void CancellationManager::Cancel(uint64_t handle_id, CancellationReason reason) {
    std::lock_guard<std::mutex> lock(handles_mutex_);
    
    auto it = handles_.find(handle_id);
    if (it == handles_.end()) {
        return;
    }
    
    CancellationHandle& handle = *it->second;
    
    // Set cancellation flag atomically
    handle.cancelled.store(true);
    handle.reason = reason;
    handle.cancelled_at = std::chrono::steady_clock::now();
    
    // Update statistics
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_cancellations++;
        
        switch (reason) {
            case CancellationReason::USER_TYPING:
                stats_.user_typing_cancellations++;
                break;
            case CancellationReason::TIMEOUT:
                stats_.timeout_cancellations++;
                break;
            case CancellationReason::ERROR:
                stats_.error_cancellations++;
                break;
            default:
                break;
        }
        
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            handle.cancelled_at - handle.created);
        
        // Update average latency
        stats_.avg_cancel_latency = (stats_.avg_cancel_latency * (stats_.total_cancellations - 1) + latency)
                                    / stats_.total_cancellations;
        
        // Update max latency
        if (latency > stats_.max_cancel_latency) {
            stats_.max_cancel_latency = latency;
        }
    }
    
    // Execute callbacks
    ExecuteCallbacks(handle_id);
}

inline void CancellationManager::CancelAll(CancellationReason reason) {
    std::lock_guard<std::mutex> lock(handles_mutex_);
    
    for (auto& pair : handles_) {
        CancellationHandle& handle = *pair.second;
        handle.cancelled.store(true);
        handle.reason = reason;
        handle.cancelled_at = std::chrono::steady_clock::now();
        
        ExecuteCallbacks(handle.id);
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_cancellations += static_cast<int>(handles_.size());
        
        switch (reason) {
            case CancellationReason::USER_TYPING:
                stats_.user_typing_cancellations += static_cast<int>(handles_.size());
                break;
            case CancellationReason::TIMEOUT:
                stats_.timeout_cancellations += static_cast<int>(handles_.size());
                break;
            case CancellationReason::ERROR:
                stats_.error_cancellations += static_cast<int>(handles_.size());
                break;
            default:
                break;
        }
    }
}

inline bool CancellationManager::IsCancelled(uint64_t handle_id) const {
    std::lock_guard<std::mutex> lock(handles_mutex_);
    
    auto it = handles_.find(handle_id);
    if (it == handles_.end()) {
        return true;  // Handle doesn't exist, treat as cancelled
    }
    
    return it->second->cancelled.load();
}

inline CancellationReason CancellationManager::GetCancellationReason(uint64_t handle_id) const {
    std::lock_guard<std::mutex> lock(handles_mutex_);
    
    auto it = handles_.find(handle_id);
    if (it == handles_.end()) {
        return CancellationReason::ERROR;
    }
    
    return it->second->reason;
}

inline void CancellationManager::DestroyHandle(uint64_t handle_id) {
    std::lock_guard<std::mutex> lock(handles_mutex_);
    handles_.erase(handle_id);
    callbacks_.erase(handle_id);
}

inline void CancellationManager::RegisterCallback(uint64_t handle_id, std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(handles_mutex_);
    callbacks_[handle_id].push_back(callback);
}

inline void CancellationManager::UnregisterCallback(uint64_t handle_id) {
    std::lock_guard<std::mutex> lock(handles_mutex_);
    callbacks_.erase(handle_id);
}

inline bool CancellationManager::WaitForCancellation(uint64_t handle_id, std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start) < timeout) {
        
        if (IsCancelled(handle_id)) {
            return true;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    return false;
}

inline CancellationStats CancellationManager::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline void CancellationManager::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
}

inline CancellationHandle* CancellationManager::FindHandle(uint64_t handle_id) {
    auto it = handles_.find(handle_id);
    return it != handles_.end() ? it->second.get() : nullptr;
}

inline const CancellationHandle* CancellationManager::FindHandle(uint64_t handle_id) const {
    auto it = handles_.find(handle_id);
    return it != handles_.end() ? it->second.get() : nullptr;
}

inline void CancellationManager::ExecuteCallbacks(uint64_t handle_id) {
    auto it = callbacks_.find(handle_id);
    if (it == callbacks_.end()) {
        return;
    }
    
    for (auto& callback : it->second) {
        callback();
    }
}

} // namespace RawrXD