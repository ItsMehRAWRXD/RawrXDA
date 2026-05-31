// src/core/lockfree_spsc_queue.hpp
#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

template<typename T, size_t Capacity>
class SPSCQueue {
public:
    SPSCQueue() : head_(0), tail_(0) {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
        buffer_.resize(Capacity);
    }

    bool push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) & (Capacity - 1);

        if (next == tail_.load(std::memory_order_acquire))
            return false; // full

        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire))
            return false; // empty

        item = buffer_[tail];
        tail_.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    bool full() const {
        size_t next = (head_.load(std::memory_order_relaxed) + 1) & (Capacity - 1);
        return next == tail_.load(std::memory_order_acquire);
    }

    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        
        if (head >= tail) {
            return head - tail;
        } else {
            return Capacity - tail + head;
        }
    }

private:
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    std::vector<T> buffer_;
};
