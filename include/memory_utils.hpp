/**
 * memory_utils.hpp
 * ================
 * Utilities and helpers for smart pointer migration and RAII patterns
 * Used throughout Phase 2 (Memory Management Overhaul)
 * 
 * Features:
 * - Safe unique_ptr creation and transfer
 * - Ownership verification helpers
 * - RAII wrappers for legacy APIs
 * - Memory tracking for debug builds
 */

#ifndef RAWRXD_MEMORY_UTILS_HPP
#define RAWRXD_MEMORY_UTILS_HPP

#include <memory>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <mutex>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Memory {

// ============================================================
// SMART POINTER HELPERS
// ============================================================

/**
 * Create a unique_ptr with guaranteed non-null result
 * @throws std::bad_alloc if allocation fails
 */
template<typename T, typename... Args>
std::unique_ptr<T> make_unique_checked(Args&&... args) {
    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    assert(ptr != nullptr && "Failed to allocate memory");
    return ptr;
}

/**
 * Safe transfer of ownership from raw pointer to unique_ptr
 * Use when receiving ownership from legacy code
 */
template<typename T>
std::unique_ptr<T> adopt_raw_pointer(T* raw_ptr) {
    if (!raw_ptr) {
        return nullptr;
    }
    return std::unique_ptr<T>(raw_ptr);
}

/**
 * Safe conversion from unique_ptr to shared_ptr
 * Transfers ownership
 */
template<typename T>
std::shared_ptr<T> to_shared(std::unique_ptr<T>& unique) {
    return std::shared_ptr<T>(unique.release());
}

// ============================================================
// OWNERSHIP TRACKING (Debug Only)
// ============================================================

#ifdef RAWRXD_DEBUG_MEMORY

class OwnershipTracker {
public:
    static OwnershipTracker& instance() {
        static OwnershipTracker tracker;
        return tracker;
    }

    void track_allocation(void* ptr, const char* type, const char* location) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_allocations.insert(ptr);
#ifndef NDEBUG
        char buf[256];
        (void)snprintf(buf, sizeof(buf), "ALLOC: %s at %s (%zu)\n", type, location, (size_t)reinterpret_cast<uintptr_t>(ptr));
        (void)fwrite(buf, 1, strlen(buf), stderr);
#ifdef _WIN32
        OutputDebugStringA(buf);
#endif
#endif
    }

    void track_deallocation(void* ptr, const char* location) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_allocations.erase(ptr);
#ifndef NDEBUG
        char buf[256];
        (void)snprintf(buf, sizeof(buf), "FREE: at %s (%zu)\n", location, (size_t)reinterpret_cast<uintptr_t>(ptr));
        (void)fwrite(buf, 1, strlen(buf), stderr);
#ifdef _WIN32
        OutputDebugStringA(buf);
#endif
#endif
    }

    size_t active_allocations() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_allocations.size();
    }

private:
    mutable std::mutex m_mutex;
    std::set<void*> m_allocations;
};

#define MEMORY_TRACK_ALLOC(ptr, type) \
    RawrXD::Memory::OwnershipTracker::instance().track_allocation(ptr, #type, __FUNCTION__)

#define MEMORY_TRACK_FREE(ptr) \
    RawrXD::Memory::OwnershipTracker::instance().track_deallocation(ptr, __FUNCTION__)

#else

#define MEMORY_TRACK_ALLOC(ptr, type)
#define MEMORY_TRACK_FREE(ptr)

#endif // RAWRXD_DEBUG_MEMORY

// ============================================================
// RAII WRAPPERS FOR LEGACY APIS
// ============================================================

/**
 * RAII wrapper for raw pointers that need cleanup
 * Automatically calls delete in destructor
 */
template<typename T>
class RawPtrHolder {
public:
    explicit RawPtrHolder(T* ptr = nullptr) : m_ptr(ptr) {
        MEMORY_TRACK_ALLOC(m_ptr, T);
    }

    ~RawPtrHolder() {
        if (m_ptr) {
            MEMORY_TRACK_FREE(m_ptr);
            delete m_ptr;
        }
    }

    // Move semantics
    RawPtrHolder(RawPtrHolder&& other) noexcept : m_ptr(other.release()) {}
    RawPtrHolder& operator=(RawPtrHolder&& other) noexcept {
        reset(other.release());
        return *this;
    }

    // Delete copy semantics
    RawPtrHolder(const RawPtrHolder&) = delete;
    RawPtrHolder& operator=(const RawPtrHolder&) = delete;

    T* get() { return m_ptr; }
    const T* get() const { return m_ptr; }
    T* release() { return std::exchange(m_ptr, nullptr); }
    void reset(T* ptr = nullptr) {
        if (m_ptr) delete m_ptr;
        m_ptr = ptr;
    }

    T& operator*() { return *m_ptr; }
    T* operator->() { return m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }

private:
    T* m_ptr;
};

/**
 * RAII wrapper for array allocations
 */
template<typename T>
class RawArrayHolder {
public:
    explicit RawArrayHolder(T* ptr = nullptr) : m_ptr(ptr) {
        MEMORY_TRACK_ALLOC(m_ptr, T);
    }

    ~RawArrayHolder() {
        if (m_ptr) {
            MEMORY_TRACK_FREE(m_ptr);
            delete[] m_ptr;
        }
    }

    RawArrayHolder(RawArrayHolder&& other) noexcept : m_ptr(other.release()) {}
    RawArrayHolder& operator=(RawArrayHolder&& other) noexcept {
        reset(other.release());
        return *this;
    }

    RawArrayHolder(const RawArrayHolder&) = delete;
    RawArrayHolder& operator=(const RawArrayHolder&) = delete;

    T* get() { return m_ptr; }
    const T* get() const { return m_ptr; }
    T* release() { return std::exchange(m_ptr, nullptr); }
    void reset(T* ptr = nullptr) {
        if (m_ptr) delete[] m_ptr;
        m_ptr = ptr;
    }

    T& operator[](size_t idx) { return m_ptr[idx]; }
    const T& operator[](size_t idx) const { return m_ptr[idx]; }
    explicit operator bool() const { return m_ptr != nullptr; }

private:
    T* m_ptr;
};

// ============================================================
// TYPE TRAITS FOR POINTER TYPES
// ============================================================

template<typename T>
struct is_smart_pointer : std::false_type {};

template<typename T>
struct is_smart_pointer<std::unique_ptr<T>> : std::true_type {};

template<typename T>
struct is_smart_pointer<std::shared_ptr<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_smart_pointer_v = is_smart_pointer<T>::value;

// ============================================================
// CONCEPTS FOR MEMORY SAFETY
// ============================================================

/**
 * Ensure a type has proper cleanup semantics
 * Used for compile-time memory safety checks
 */
template<typename T>
concept HasMemorySemantics = requires(T t) {
    // Should either be managed by parent or use smart pointers
    // This is primarily for documentation
};

} // namespace Memory
} // namespace RawrXD

#endif // RAWRXD_MEMORY_UTILS_HPP
