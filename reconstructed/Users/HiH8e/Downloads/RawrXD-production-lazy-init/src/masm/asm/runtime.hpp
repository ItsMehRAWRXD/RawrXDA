/**
 * asm_runtime.hpp - C++ Interface to MASM x64 Runtime Library
 *
 * Zero-dependency runtime layer providing:
 *  - Memory allocation with alignment
 *  - Thread synchronization (mutexes, events)
 *  - Unicode string handling (UTF-8/UTF-16)
 *  - Event loop with signal routing
 *
 * All functions use Microsoft x64 calling convention.
 * All opaque handles must not be dereferenced from C++ code.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

//=====================================================================
// MEMORY MANAGEMENT (MASM: asm_memory.asm)
//=====================================================================

/**
 * @brief Allocates aligned memory block
 * @param size Size in bytes
 * @param alignment Alignment in bytes (16, 32, 64...)
 * @return Pointer to allocated memory, or NULL if failed
 * @note Hidden metadata (40 bytes) stored before returned pointer
 */
void* asm_malloc(size_t size, size_t alignment);

/**
 * @brief Frees previously allocated memory
 * @param ptr Pointer from asm_malloc
 * @note Safe to call with NULL
 */
void asm_free(void* ptr);

/**
 * @brief Reallocates memory to new size
 * @param ptr Original pointer (or NULL)
 * @param new_size New size in bytes
 * @return New pointer (may differ), or NULL on failure
 * @note On failure, original pointer remains valid
 */
void* asm_realloc(void* ptr, size_t new_size);

/**
 * @brief Memory statistics for diagnostics
 * @return Current live allocation count
 */
uint64_t asm_memory_stats();

//=====================================================================
// THREAD SYNCHRONIZATION (MASM: asm_sync.asm)
//=====================================================================

/// Opaque mutex handle
typedef void* MutexHandle;

/// Opaque event handle
typedef void* EventHandle;

/// Opaque semaphore handle
typedef void* SemaphoreHandle;

/**
 * @brief Creates a new mutex (CRITICAL_SECTION wrapper)
 * @return Handle to mutex, or NULL if failed
 */
MutexHandle asm_mutex_create();

/**
 * @brief Acquires exclusive lock on mutex (blocking)
 * @param handle Mutex handle from asm_mutex_create
 * @note Recursive locks are allowed from same thread
 */
void asm_mutex_lock(MutexHandle handle);

/**
 * @brief Releases mutex lock
 * @param handle Mutex handle
 * @note Must be called by same thread that locked it
 */
void asm_mutex_unlock(MutexHandle handle);

/**
 * @brief Destroys mutex and frees memory
 * @param handle Mutex handle
 * @note Must not be held by any thread when called
 */
void asm_mutex_destroy(MutexHandle handle);

/**
 * @brief Creates an event object
 * @param manual_reset 1 for manual-reset, 0 for auto-reset
 * @return Event handle, or NULL if failed
 */
EventHandle asm_event_create(int manual_reset);

/**
 * @brief Sets event to signaled state
 */
void asm_event_set(EventHandle handle);

/**
 * @brief Resets event to unsignaled state
 */
void asm_event_reset(EventHandle handle);

/**
 * @brief Waits for event to be signaled
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return 0 on success, 258 on timeout, -1 on error
 */
int64_t asm_event_wait(EventHandle handle, int64_t timeout_ms);

/**
 * @brief Destroys event object
 */
void asm_event_destroy(EventHandle handle);

/**
 * @brief Creates a semaphore
 */
SemaphoreHandle asm_semaphore_create(int64_t initial_count, int64_t max_count);

/**
 * @brief Waits for semaphore
 */
int64_t asm_semaphore_wait(SemaphoreHandle handle, int64_t timeout_ms);

/**
 * @brief Releases semaphore
 */
int64_t asm_semaphore_release(SemaphoreHandle handle, int64_t count);

/**
 * @brief Destroys semaphore
 */
void asm_semaphore_destroy(SemaphoreHandle handle);

//=====================================================================
// THREAD POOL (MASM: asm_thread_pool.asm)
//=====================================================================

/// Opaque thread pool handle
typedef void* ThreadPoolHandle;

/// Task procedure type
typedef void (*TaskProc)(void* context);

/**
 * @brief Creates a thread pool
 * @param num_threads Number of worker threads
 * @return Pool handle, or NULL if failed
 */
ThreadPoolHandle asm_thread_pool_create(size_t num_threads);

/**
 * @brief Enqueues a task to the thread pool
 * @param pool Pool handle
 * @param proc Function to execute
 * @param context User data passed to proc
 * @return 1 on success, 0 on failure
 */
int64_t asm_thread_pool_enqueue(ThreadPoolHandle pool, TaskProc proc, void* context);

/**
 * @brief Shuts down and destroys the thread pool
 * @param pool Pool handle
 */
void asm_thread_pool_destroy(ThreadPoolHandle pool);

/**
 * @brief Sets event to signaled state
 * @param handle Event handle
 */
void asm_event_set(EventHandle handle);

/**
 * @brief Resets event to unsignaled state
 * @param handle Event handle
 */
void asm_event_reset(EventHandle handle);

/**
 * @brief Waits for event to be signaled
 * @param handle Event handle
 * @param timeout_ms Timeout in milliseconds (-1 = infinite, 0 = non-blocking)
 * @return 0 if signaled, 258 if timeout, -1 if error
 */
int asm_event_wait(EventHandle handle, int timeout_ms);

/**
 * @brief Destroys event and frees memory
 * @param handle Event handle
 */
void asm_event_destroy(EventHandle handle);

//=====================================================================
// ATOMIC OPERATIONS (MASM: asm_sync.asm)
//=====================================================================

/**
 * @brief Atomically increments qword value at ptr
 * @param ptr Pointer to 64-bit value
 * @return New value after increment
 */
uint64_t asm_atomic_increment(volatile uint64_t* ptr);

/**
 * @brief Atomically decrements qword value at ptr
 * @param ptr Pointer to 64-bit value
 * @return New value after decrement
 */
uint64_t asm_atomic_decrement(volatile uint64_t* ptr);

/**
 * @brief Atomically adds value to qword at ptr
 * @param ptr Pointer to 64-bit value
 * @param value Value to add
 * @return New value
 */
uint64_t asm_atomic_add(volatile uint64_t* ptr, uint64_t value);

/**
 * @brief Atomic compare-and-swap
 * @param ptr Pointer to 64-bit value
 * @param old Expected old value
 * @param new New value to store if comparison succeeds
 * @return 1 if swap succeeded, 0 if comparison failed
 */
int asm_atomic_cmpxchg(volatile uint64_t* ptr, uint64_t old, uint64_t new_val);

/**
 * @brief Atomic exchange (swap)
 * @param ptr Pointer to 64-bit value
 * @param value New value
 * @return Old value that was replaced
 */
uint64_t asm_atomic_xchg(volatile uint64_t* ptr, uint64_t value);

//=====================================================================
// STRING HANDLING (MASM: asm_string.asm)
//=====================================================================

/// Opaque string handle (UTF-8 counted string)
typedef void* StringHandle;

/**
 * @brief Creates string from UTF-8 data
 * @param utf8_data Pointer to UTF-8 bytes
 * @param length Byte length of data
 * @return String handle, or NULL if failed
 * @note Returned handle points to string data (metadata is before it)
 */
StringHandle asm_str_create(const char* utf8_data, size_t length);

/**
 * @brief Destroys string and frees memory
 * @param handle String handle
 */
void asm_str_destroy(StringHandle handle);

/**
 * @brief Gets character count of string
 * @param handle String handle
 * @return Number of characters
 */
uint64_t asm_str_length(StringHandle handle);

/**
 * @brief Concatenates two strings
 * @param str1 First string handle
 * @param str2 Second string handle
 * @return New string with concatenated data, or NULL if failed
 */
StringHandle asm_str_concat(StringHandle str1, StringHandle str2);

/**
 * @brief Compares two strings lexicographically
 * @param str1 First string handle
 * @param str2 Second string handle
 * @return -1 if str1 < str2, 0 if equal, 1 if str1 > str2
 */
int asm_str_compare(StringHandle str1, StringHandle str2);

/**
 * @brief Finds first occurrence of needle in haystack
 * @param haystack String to search
 * @param needle Substring to find
 * @return Character offset in haystack, or -1 if not found
 */
int64_t asm_str_find(StringHandle haystack, StringHandle needle);

/**
 * @brief Extracts substring
 * @param str Source string
 * @param start Starting offset (0-based)
 * @param length Number of characters to extract
 * @return New string with substring, or NULL if bounds exceed
 */
StringHandle asm_str_substring(StringHandle str, uint64_t start, uint64_t length);

/**
 * @brief Converts UTF-8 string to UTF-16 (wide char)
 * @param utf8_handle String handle (UTF-8)
 * @return Pointer to UTF-16 data (caller must free with asm_free)
 */
wchar_t* asm_str_to_utf16(StringHandle utf8_handle);

/**
 * @brief Converts UTF-16 (wide char) to UTF-8 string
 * @param utf16_ptr Pointer to null-terminated UTF-16 data
 * @return UTF-8 string handle, or NULL if failed
 */
StringHandle asm_str_from_utf16(const wchar_t* utf16_ptr);

/**
 * @brief Formats string with arguments (sprintf-like)
 * @param format Format string handle with %d, %s, %x specifiers
 * @param args_ptr Pointer to array of qword arguments
 * @param args_count Number of arguments
 * @return New formatted string, or NULL if failed
 */
StringHandle asm_str_format(StringHandle format, const uint64_t* args_ptr, size_t args_count);

/**
 * @brief Gets pointer to string data
 * @param handle String handle
 * @return Pointer to UTF-8 data (const), or NULL
 */
const char* asm_str_data(StringHandle handle);

//=====================================================================
// EVENT LOOP & SIGNAL ROUTING (MASM: asm_events.asm)
//=====================================================================

/// Opaque event loop handle
typedef void* EventLoopHandle;

/// Signal handler function type: (param1, param2, param3) -> void
typedef void (*SignalHandler)(uint64_t param1, uint64_t param2, uint64_t param3);

/**
 * @brief Creates event loop with queue
 * @param queue_size Maximum number of pending events
 * @return Event loop handle, or NULL if failed
 */
EventLoopHandle asm_event_loop_create(size_t queue_size);

/**
 * @brief Registers handler for signal
 * @param loop Event loop handle
 * @param signal_id Unique signal identifier (0-255)
 * @param handler Function pointer to invoke for signal
 * @note Handler will be called from asm_event_loop_process_one/all
 */
void asm_event_loop_register_signal(EventLoopHandle loop, uint32_t signal_id, SignalHandler handler);

/**
 * @brief Queues event for async processing
 * @param loop Event loop handle
 * @param signal_id Signal ID
 * @param param1 First parameter
 * @param param2 Second parameter
 * @param param3 Third parameter
 * @note Does not immediately invoke handler; call process_one/all to dispatch
 */
void asm_event_loop_emit(EventLoopHandle loop, uint32_t signal_id, uint64_t param1, uint64_t param2, uint64_t param3);

/**
 * @brief Processes one pending event from queue
 * @param loop Event loop handle
 * @return 1 if event processed, 0 if queue empty, -1 if error
 * @note Invokes registered handler for the signal
 */
int asm_event_loop_process_one(EventLoopHandle loop);

/**
 * @brief Processes all pending events in queue
 * @param loop Event loop handle
 * @return Count of events processed
 */
int asm_event_loop_process_all(EventLoopHandle loop);

/**
 * @brief Destroys event loop and frees memory
 * @param loop Event loop handle
 * @note Must not be called while events are being processed
 */
void asm_event_loop_destroy(EventLoopHandle loop);

//=====================================================================
// Optional: C++ Wrapper Classes for Convenience
//=====================================================================

#ifdef __cplusplus

/**
 * @brief RAII Mutex wrapper
 *
 * Automatically locks on construction, unlocks on destruction.
 * Example:
 *   {
 *       asm_Mutex lock(mutex_handle);
 *       // Protected section
 *   } // Automatically unlocked
 */
class asm_Mutex {
public:
    explicit asm_Mutex(MutexHandle handle) : m_handle(handle) {
        asm_mutex_lock(m_handle);
    }
    
    ~asm_Mutex() {
        asm_mutex_unlock(m_handle);
    }
    
    // Non-copyable
    asm_Mutex(const asm_Mutex&) = delete;
    asm_Mutex& operator=(const asm_Mutex&) = delete;
    
private:
    MutexHandle m_handle;
};

/**
 * @brief RAII String wrapper
 *
 * Automatically destroys string on scope exit.
 * Example:
 *   {
 *       asm_String name = asm_String(asm_str_create("hello", 5));
 *       // Use name
 *   } // Automatically freed
 */
class asm_String {
public:
    explicit asm_String(StringHandle handle) : m_handle(handle) {}
    
    ~asm_String() {
        if (m_handle) asm_str_destroy(m_handle);
    }
    
    StringHandle get() const { return m_handle; }
    
    // Move support
    asm_String(asm_String&& other) noexcept : m_handle(other.release()) {}
    asm_String& operator=(asm_String&& other) noexcept {
        reset(other.release());
        return *this;
    }
    
    // Non-copyable
    asm_String(const asm_String&) = delete;
    asm_String& operator=(const asm_String&) = delete;
    
private:
    StringHandle m_handle;
    
    StringHandle release() {
        StringHandle h = m_handle;
        m_handle = nullptr;
        return h;
    }
    
    void reset(StringHandle h) {
        if (m_handle) asm_str_destroy(m_handle);
        m_handle = h;
    }
};

/**
 * @brief RAII Event Loop wrapper
 */
class asm_EventLoop {
public:
    explicit asm_EventLoop(size_t queue_size) 
        : m_handle(asm_event_loop_create(queue_size)) {}
    
    ~asm_EventLoop() {
        if (m_handle) asm_event_loop_destroy(m_handle);
    }
    
    EventLoopHandle get() const { return m_handle; }
    
    void register_signal(uint32_t signal_id, SignalHandler handler) {
        asm_event_loop_register_signal(m_handle, signal_id, handler);
    }
    
    void emit(uint32_t signal_id, uint64_t p1 = 0, uint64_t p2 = 0, uint64_t p3 = 0) {
        asm_event_loop_emit(m_handle, signal_id, p1, p2, p3);
    }
    
    int process_one() {
        return asm_event_loop_process_one(m_handle);
    }
    
    int process_all() {
        return asm_event_loop_process_all(m_handle);
    }
    
private:
    EventLoopHandle m_handle;
};

#endif // __cplusplus

#ifdef __cplusplus
}
#endif

#endif // ASM_RUNTIME_HPP
