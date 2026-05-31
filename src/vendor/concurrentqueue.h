// ============================================================================
// Stub for moodycamel::ConcurrentQueue
// Minimal implementation for RawrXD build compatibility
// ============================================================================

#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <chrono>
#include <algorithm>

namespace moodycamel {

// Minimal concurrent queue stub using mutex-based implementation
template<typename T>
class ConcurrentQueue {
public:
    ConcurrentQueue(size_t capacity = 1024) : capacity_(capacity) {}
    
    bool enqueue(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_) {
            return false;
        }
        queue_.push(std::move(item));
        return true;
    }
    
    bool enqueue(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_) {
            return false;
        }
        queue_.push(item);
        return true;
    }
    
    bool try_dequeue(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    size_t size_approx() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    size_t capacity_;
};

// Blocking concurrent queue stub
template<typename T>
class BlockingConcurrentQueue {
public:
    BlockingConcurrentQueue(size_t capacity = 1024) : capacity_(capacity), shutdown_(false) {}
    
    bool enqueue(T&& item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.size() >= capacity_) {
                return false;
            }
            queue_.push(std::move(item));
        }
        cv_.notify_one();
        return true;
    }
    
    bool enqueue(const T& item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.size() >= capacity_) {
                return false;
            }
            queue_.push(item);
        }
        cv_.notify_one();
        return true;
    }
    
    bool try_dequeue(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    bool wait_dequeue_timed(T& item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || shutdown_; })) {
            return false;
        }
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }
    
    size_t size_approx() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    size_t capacity_;
    bool shutdown_;
};

} // namespace moodycamel
