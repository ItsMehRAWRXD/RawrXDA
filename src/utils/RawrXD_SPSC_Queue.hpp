#pragma once
#include <atomic>
#include <vector>
#include <optional>

namespace RawrXD {

// RawrXD_SPSC_Queue.hpp - Minimalist Lock-Free Command Buffer
// Single-Producer Single-Consumer
template<typename T, size_t Capacity>
class SPSCQueue {
    alignas(128) std::atomic<size_t> head{0};
    alignas(128) std::atomic<size_t> tail{0};
    T buffer[Capacity];

public:
    bool Push(const T& item) {
        size_t h = head.load(std::memory_order_relaxed);
        if ((h + 1) % Capacity == tail.load(std::memory_order_acquire)) return false; // Full
        buffer[h] = item;
        head.store((h + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool Pop(T& item) {
        size_t t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) return false; // Empty
        item = buffer[t];
        tail.store((t + 1) % Capacity, std::memory_order_release);
        return true;
    }
    
    bool IsEmpty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }
};

} // namespace RawrXD
