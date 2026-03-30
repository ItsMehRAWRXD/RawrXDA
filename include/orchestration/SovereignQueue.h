#pragma once

#include <atomic>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rawrxd::orchestration {

#if defined(__cpp_lib_hardware_interference_size)
inline constexpr std::size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t kCacheLineSize = 64;
#endif

template <typename T, std::size_t Capacity>
class alignas(kCacheLineSize) SovereignQueue {
    static_assert(Capacity >= 2, "Capacity must be >= 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert(std::is_nothrow_move_constructible_v<T> || std::is_trivially_copyable_v<T>,
                  "T must be noexcept-move or trivially copyable");

public:
    static constexpr std::size_t kMask = Capacity - 1;
    static constexpr std::uint32_t kHybridSpinBudgetUs = 50;

    bool Push(const T& item) noexcept {
        const std::uint64_t head = m_head.load(std::memory_order_relaxed);
        const std::uint64_t tail = m_tail.load(std::memory_order_acquire);
        if ((head - tail) >= Capacity) {
            return false;
        }

        m_buffer[static_cast<std::size_t>(head & kMask)] = item;
        m_head.store(head + 1, std::memory_order_release);
        return true;
    }

    bool Pop(T& out) noexcept {
        const std::uint64_t tail = m_tail.load(std::memory_order_relaxed);
        const std::uint64_t head = m_head.load(std::memory_order_acquire);
        if (tail == head) {
            return false;
        }

        out = m_buffer[static_cast<std::size_t>(tail & kMask)];
        m_tail.store(tail + 1, std::memory_order_release);
        return true;
    }

    bool WaitPush(const T& item, std::uint32_t spinCount = 256, std::uint32_t waitMs = 1) noexcept {
        const auto spinDeadline = std::chrono::steady_clock::now() + std::chrono::microseconds(kHybridSpinBudgetUs);
        for (std::uint32_t i = 0; i < spinCount; ++i) {
            if (Push(item)) {
                return true;
            }
#if defined(_WIN32)
            ::YieldProcessor();
#endif
        }

        while (std::chrono::steady_clock::now() < spinDeadline) {
            if (Push(item)) {
                return true;
            }
#if defined(_WIN32)
            ::YieldProcessor();
#endif
        }

#if defined(_WIN32)
        while (true) {
            std::uint64_t observedTail = m_tail.load(std::memory_order_acquire);
            if (Push(item)) {
                return true;
            }
            (void)observedTail;
            ::Sleep(waitMs);
            ::SwitchToThread();
        }
#else
        return Push(item);
#endif
    }

    bool WaitPop(T& out, std::uint32_t spinCount = 256, std::uint32_t waitMs = 1) noexcept {
        const auto spinDeadline = std::chrono::steady_clock::now() + std::chrono::microseconds(kHybridSpinBudgetUs);
        for (std::uint32_t i = 0; i < spinCount; ++i) {
            if (Pop(out)) {
                return true;
            }
#if defined(_WIN32)
            ::YieldProcessor();
#endif
        }

        while (std::chrono::steady_clock::now() < spinDeadline) {
            if (Pop(out)) {
                return true;
            }
#if defined(_WIN32)
            ::YieldProcessor();
#endif
        }

#if defined(_WIN32)
        while (true) {
            std::uint64_t observedHead = m_head.load(std::memory_order_acquire);
            if (Pop(out)) {
                return true;
            }
            (void)observedHead;
            ::Sleep(waitMs);
            ::SwitchToThread();
        }
#else
        return Pop(out);
#endif
    }

    std::size_t ApproxSize() const noexcept {
        const std::uint64_t head = m_head.load(std::memory_order_acquire);
        const std::uint64_t tail = m_tail.load(std::memory_order_acquire);
        return static_cast<std::size_t>(head - tail);
    }

    bool Empty() const noexcept {
        return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
    }

    bool Full() const noexcept {
        const std::uint64_t head = m_head.load(std::memory_order_acquire);
        const std::uint64_t tail = m_tail.load(std::memory_order_acquire);
        return (head - tail) >= Capacity;
    }

private:
    alignas(kCacheLineSize) std::atomic<std::uint64_t> m_head{0};
    alignas(kCacheLineSize) std::atomic<std::uint64_t> m_tail{0};
    alignas(kCacheLineSize) std::array<T, Capacity> m_buffer{};
};

} // namespace rawrxd::orchestration
