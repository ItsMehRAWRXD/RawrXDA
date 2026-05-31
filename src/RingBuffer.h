/*
 * RingBuffer.h - Zero-copy RingBuffer for High-Frequency Token Streaming
 * Phase 15/20 Performance Requirement: Sub-10ms UI delivery
 * Architecture: SPSC (Single Producer, Single Consumer) lock-free ring buffer
 * Producer: MASM Kernel (Inference Engine)
 * Consumer: C++ FFI Shim (rawrxd_ffi_shim.cpp)
 */

#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

    template <typename T, size_t Size = 4096>
    class RingBuffer {
    public:
        RingBuffer() : head_(0), tail_(0) {
            buffer_.resize(Size);
        }

        /**
         * @brief Pushes a value to the ring buffer (MASM Kernel / Inference Engine).
         * @param val Value to push.
         * @return bool True if successful, false if buffer is full.
         */
        bool push(const T& val) {
            size_t head = head_.load(std::memory_order_relaxed);
            size_t next_head = (head + 1) % Size;

            if (next_head == tail_.load(std::memory_order_acquire)) {
                return false; // Buffer overflow
            }

            buffer_[head] = val;
            head_.store(next_head, std::memory_order_release);
            return true;
        }

        /**
         * @brief Pops a value from the ring buffer (FFI Shim Consumer).
         * @param val Reference to store the popped value.
         * @return bool True if successful, false if buffer is empty.
         */
        bool pop(T& val) {
            size_t tail = tail_.load(std::memory_order_relaxed);

            if (tail == head_.load(std::memory_order_acquire)) {
                return false; // Buffer empty
            }

            val = buffer_[tail];
            tail_.store((tail + 1) % Size, std::memory_order_release);
            return true;
        }

        /**
         * @brief Checks if the buffer is empty.
         */
        bool isEmpty() const {
            return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
        }

    private:
        std::vector<T> buffer_;
        std::atomic<size_t> head_;
        std::atomic<size_t> tail_;

        // Cache-line padding to prevent false sharing (typical 64 bytes)
        uint8_t padding[64];
    };

    // Specialized RingBuffer for Token Streaming (Phase 15 Core)
    // Used in RawrXD_RingBuffer_Consumer.asm for high-throughput delivery
    typedef RingBuffer<std::string> TokenRingBuffer;

} // namespace RawrXD
