// memory_error_real.cpp - PRODUCTION MEMORY MANAGEMENT AND ERROR HANDLING
// RAII wrappers, centralized cleanup, Windows Event Log integration
// Fixes ALL 4 memory leak patterns identified in audit

#include <windows.h>
#include <dstorage.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <string>
#include <cstdio>
#include <cstdarg>

// ============================================================
// ERROR CODES AND LOGGING
// ============================================================

enum class TitanError : HRESULT {
    OK                      = S_OK,
    E_INIT_FAILED           = 0x80040001,
    E_MEMORY_ALLOC          = 0x80040002,
    E_FILE_OPEN             = 0x80040003,
    E_VULKAN_INIT           = 0x80040004,
    E_DSTORAGE_INIT         = 0x80040005,
    E_MODEL_LOAD            = 0x80040006,
    E_INFERENCE_FAILED      = 0x80040007,
    E_PHASE_DEPENDENCY      = 0x80040008,
    E_RESOURCE_LEAKED       = 0x80040009,
    E_INVALID_PARAMETER     = 0x8004000A,
    E_OUT_OF_MEMORY         = 0x8004000B,
    E_GPU_MEMORY            = 0x8004000C,
    E_TIMEOUT               = 0x8004000D,
    E_CANCELLED             = 0x8004000E,
    E_NOT_INITIALIZED       = 0x8004000F
};

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3, Fatal = 4 };

// Global error state
static std::atomic<TitanError> g_last_error{TitanError::OK};
static char g_last_error_message[1024] = {0};
static std::mutex g_error_mutex;
static HANDLE g_event_log = nullptr;

// ============================================================
// ERROR LOGGING IMPLEMENTATION
// ============================================================

static void LogToEventLog(WORD type, const char* message) {
    if (!g_event_log) {
        g_event_log = RegisterEventSourceA(nullptr, "RawrXD_Titan");
    }
    if (g_event_log) {
        const char* strings[1] = { message };
        ReportEventA(g_event_log, type, 0, 0, nullptr, 1, 0, strings, nullptr);
    }
}

static void LogToFile(const char* message) {
    static HANDLE hLogFile = INVALID_HANDLE_VALUE;
    if (hLogFile == INVALID_HANDLE_VALUE) {
        hLogFile = CreateFileA("titan_error.log",
                              FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    }
    if (hLogFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hLogFile, message, (DWORD)strlen(message), &written, nullptr);
        WriteFile(hLogFile, "\r\n", 2, &written, nullptr);
    }
}

void TitanLogError(LogLevel level, TitanError code, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    
    va_list args;
    va_start(args, fmt);
    
    char buffer[2048];
    char timestamp[64];
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(timestamp, sizeof(timestamp), "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    const char* level_str[] = { "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };
    int offset = snprintf(buffer, sizeof(buffer), "%s [%s] (0x%08X) ",
                         timestamp, level_str[(int)level], (unsigned int)code);
    vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    
    va_end(args);
    
    // Store last error
    g_last_error = code;
    strncpy_s(g_last_error_message, buffer, sizeof(g_last_error_message) - 1);
    
    // Output to stderr
    fprintf(stderr, "%s\n", buffer);
    
    // Log to file
    LogToFile(buffer);
    
    // Log to Windows Event Log for ERROR and FATAL
    if (level >= LogLevel::Error) {
        WORD eventType = (level == LogLevel::Fatal) ? EVENTLOG_ERROR_TYPE : EVENTLOG_WARNING_TYPE;
        LogToEventLog(eventType, buffer);
    }
}

TitanError TitanGetLastError() {
    return g_last_error.load();
}

const char* TitanGetLastErrorMessage() {
    return g_last_error_message;
}

// ============================================================
// RAII WRAPPER: VirtualAlloc
// ============================================================

class AutoVirtualAlloc {
private:
    void* m_ptr = nullptr;
    size_t m_size = 0;
    
public:
    AutoVirtualAlloc() = default;
    
    explicit AutoVirtualAlloc(size_t size, DWORD protect = PAGE_READWRITE) {
        Allocate(size, protect);
    }
    
    ~AutoVirtualAlloc() {
        Free();
    }
    
    // No copy
    AutoVirtualAlloc(const AutoVirtualAlloc&) = delete;
    AutoVirtualAlloc& operator=(const AutoVirtualAlloc&) = delete;
    
    // Move OK
    AutoVirtualAlloc(AutoVirtualAlloc&& other) noexcept
        : m_ptr(other.m_ptr), m_size(other.m_size) {
        other.m_ptr = nullptr;
        other.m_size = 0;
    }
    
    AutoVirtualAlloc& operator=(AutoVirtualAlloc&& other) noexcept {
        if (this != &other) {
            Free();
            m_ptr = other.m_ptr;
            m_size = other.m_size;
            other.m_ptr = nullptr;
            other.m_size = 0;
        }
        return *this;
    }
    
    bool Allocate(size_t size, DWORD protect = PAGE_READWRITE, bool large_pages = false) {
        Free();
        DWORD flags = MEM_COMMIT | MEM_RESERVE;
        if (large_pages) flags |= MEM_LARGE_PAGES;
        
        m_ptr = VirtualAlloc(nullptr, size, flags, protect);
        if (m_ptr) {
            m_size = size;
            return true;
        }
        
        TitanLogError(LogLevel::Error, TitanError::E_MEMORY_ALLOC,
                     "VirtualAlloc failed: size=%zu, error=%lu", size, GetLastError());
        return false;
    }
    
    void Free() {
        if (m_ptr) {
            VirtualFree(m_ptr, 0, MEM_RELEASE);
            m_ptr = nullptr;
            m_size = 0;
        }
    }
    
    void* Get() const { return m_ptr; }
    size_t Size() const { return m_size; }
    explicit operator bool() const { return m_ptr != nullptr; }
    
    template<typename T>
    T* As() const { return static_cast<T*>(m_ptr); }
    
    void* Release() {
        void* tmp = m_ptr;
        m_ptr = nullptr;
        m_size = 0;
        return tmp;
    }
};

// ============================================================
// RAII WRAPPER: HeapAlloc
// ============================================================

class AutoHeapAlloc {
private:
    void* m_ptr = nullptr;
    HANDLE m_heap = nullptr;
    size_t m_size = 0;
    
public:
    AutoHeapAlloc() = default;
    
    explicit AutoHeapAlloc(size_t size, HANDLE heap = nullptr) {
        Allocate(size, heap);
    }
    
    ~AutoHeapAlloc() {
        Free();
    }
    
    // No copy
    AutoHeapAlloc(const AutoHeapAlloc&) = delete;
    AutoHeapAlloc& operator=(const AutoHeapAlloc&) = delete;
    
    // Move OK
    AutoHeapAlloc(AutoHeapAlloc&& other) noexcept
        : m_ptr(other.m_ptr), m_heap(other.m_heap), m_size(other.m_size) {
        other.m_ptr = nullptr;
        other.m_heap = nullptr;
        other.m_size = 0;
    }
    
    AutoHeapAlloc& operator=(AutoHeapAlloc&& other) noexcept {
        if (this != &other) {
            Free();
            m_ptr = other.m_ptr;
            m_heap = other.m_heap;
            m_size = other.m_size;
            other.m_ptr = nullptr;
            other.m_heap = nullptr;
            other.m_size = 0;
        }
        return *this;
    }
    
    bool Allocate(size_t size, HANDLE heap = nullptr) {
        Free();
        m_heap = heap ? heap : GetProcessHeap();
        m_ptr = HeapAlloc(m_heap, HEAP_ZERO_MEMORY, size);
        if (m_ptr) {
            m_size = size;
            return true;
        }
        
        TitanLogError(LogLevel::Error, TitanError::E_MEMORY_ALLOC,
                     "HeapAlloc failed: size=%zu, error=%lu", size, GetLastError());
        return false;
    }
    
    void Free() {
        if (m_ptr && m_heap) {
            HeapFree(m_heap, 0, m_ptr);
            m_ptr = nullptr;
            m_heap = nullptr;
            m_size = 0;
        }
    }
    
    void* Get() const { return m_ptr; }
    size_t Size() const { return m_size; }
    explicit operator bool() const { return m_ptr != nullptr; }
    
    template<typename T>
    T* As() const { return static_cast<T*>(m_ptr); }
};

// ============================================================
// RAII WRAPPER: Windows HANDLE
// ============================================================

class AutoHandle {
private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    
public:
    AutoHandle() = default;
    explicit AutoHandle(HANDLE h) : m_handle(h) {}
    
    ~AutoHandle() {
        Close();
    }
    
    // No copy
    AutoHandle(const AutoHandle&) = delete;
    AutoHandle& operator=(const AutoHandle&) = delete;
    
    // Move OK
    AutoHandle(AutoHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    
    AutoHandle& operator=(AutoHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    
    void Close() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }
    
    HANDLE Get() const { return m_handle; }
    HANDLE* GetAddressOf() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }
    explicit operator bool() const { return IsValid(); }
    
    HANDLE Release() {
        HANDLE tmp = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return tmp;
    }
    
    void Reset(HANDLE h = INVALID_HANDLE_VALUE) {
        Close();
        m_handle = h;
    }
};

// ============================================================
// RAII WRAPPER: DirectStorage Request
// ============================================================

class AutoDSRequest {
private:
    DSTORAGE_REQUEST m_request = {};
    IDStorageQueue* m_queue = nullptr;
    bool m_submitted = false;
    
public:
    AutoDSRequest() = default;
    explicit AutoDSRequest(IDStorageQueue* queue) : m_queue(queue) {}
    
    ~AutoDSRequest() {
        // Request memory is stack-allocated, nothing to free
        // But we track submission state for debugging
    }
    
    // No copy
    AutoDSRequest(const AutoDSRequest&) = delete;
    AutoDSRequest& operator=(const AutoDSRequest&) = delete;
    
    DSTORAGE_REQUEST* Get() { return &m_request; }
    const DSTORAGE_REQUEST* Get() const { return &m_request; }
    
    void SetSource(IDStorageFile* file, UINT64 offset, UINT32 size) {
        m_request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        m_request.Source.File.Source = file;
        m_request.Source.File.Offset = offset;
        m_request.Source.File.Size = size;
    }
    
    void SetDestination(void* buffer, UINT32 size) {
        m_request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
        m_request.Destination.Memory.Buffer = buffer;
        m_request.Destination.Memory.Size = size;
    }
    
    HRESULT Submit() {
        if (!m_queue) {
            TitanLogError(LogLevel::Error, TitanError::E_DSTORAGE_INIT,
                         "DSRequest Submit: queue is null");
            return E_INVALIDARG;
        }
        m_queue->EnqueueRequest(&m_request);
        m_submitted = true;
        return S_OK;
    }
    
    bool WasSubmitted() const { return m_submitted; }
};

// ============================================================
// RAII WRAPPER: Vulkan Resources
// ============================================================

class AutoVkBuffer {
private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    
public:
    AutoVkBuffer() = default;
    explicit AutoVkBuffer(VkDevice device) : m_device(device) {}
    
    ~AutoVkBuffer() {
        Destroy();
    }
    
    // No copy
    AutoVkBuffer(const AutoVkBuffer&) = delete;
    AutoVkBuffer& operator=(const AutoVkBuffer&) = delete;
    
    // Move OK
    AutoVkBuffer(AutoVkBuffer&& other) noexcept
        : m_buffer(other.m_buffer), m_device(other.m_device), m_memory(other.m_memory) {
        other.m_buffer = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
    }
    
    AutoVkBuffer& operator=(AutoVkBuffer&& other) noexcept {
        if (this != &other) {
            Destroy();
            m_buffer = other.m_buffer;
            m_device = other.m_device;
            m_memory = other.m_memory;
            other.m_buffer = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
            other.m_memory = VK_NULL_HANDLE;
        }
        return *this;
    }
    
    void Destroy() {
        if (m_device != VK_NULL_HANDLE) {
            if (m_buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device, m_buffer, nullptr);
                m_buffer = VK_NULL_HANDLE;
            }
            if (m_memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device, m_memory, nullptr);
                m_memory = VK_NULL_HANDLE;
            }
        }
    }
    
    VkBuffer Get() const { return m_buffer; }
    VkBuffer* GetAddressOf() { return &m_buffer; }
    VkDeviceMemory GetMemory() const { return m_memory; }
    VkDeviceMemory* GetMemoryAddressOf() { return &m_memory; }
    void SetDevice(VkDevice device) { m_device = device; }
    explicit operator bool() const { return m_buffer != VK_NULL_HANDLE; }
};

// ============================================================
// SCOPE GUARD (Exception-safe cleanup)
// ============================================================

class ScopeGuard {
private:
    std::function<void()> m_cleanup;
    bool m_dismissed = false;
    
public:
    explicit ScopeGuard(std::function<void()> cleanup) : m_cleanup(std::move(cleanup)) {}
    
    ~ScopeGuard() {
        if (!m_dismissed && m_cleanup) {
            try {
                m_cleanup();
            } catch (...) {
                // Never throw from destructor
            }
        }
    }
    
    // No copy or move
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;
    
    void Dismiss() { m_dismissed = true; }
};

#define TITAN_SCOPE_EXIT(code) ScopeGuard TITAN_CONCAT(_scope_guard_, __LINE__)([&]() { code; })
#define TITAN_CONCAT_IMPL(x, y) x##y
#define TITAN_CONCAT(x, y) TITAN_CONCAT_IMPL(x, y)

// ============================================================
// CENTRALIZED CLEANUP REGISTRY
// ============================================================

enum class ResourceType {
    VirtualMemory,
    HeapMemory,
    FileHandle,
    VulkanBuffer,
    VulkanMemory,
    VulkanPipeline,
    DirectStorageQueue,
    DirectStorageFile,
    GGMLContext,
    Other
};

struct RegisteredResource {
    void* ptr;
    ResourceType type;
    std::function<void(void*)> cleanup;
    int priority;  // Higher = cleanup first
    const char* name;
};

class CleanupRegistry {
private:
    std::vector<RegisteredResource> m_resources;
    std::mutex m_mutex;
    std::atomic<bool> m_shutdown{false};
    
public:
    static CleanupRegistry& Instance() {
        static CleanupRegistry instance;
        return instance;
    }
    
    void Register(void* ptr, ResourceType type, std::function<void(void*)> cleanup,
                  int priority = 0, const char* name = nullptr) {
        if (m_shutdown.load()) return;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_resources.push_back({ptr, type, std::move(cleanup), priority, name});
    }
    
    void Unregister(void* ptr) {
        if (m_shutdown.load()) return;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_resources.erase(
            std::remove_if(m_resources.begin(), m_resources.end(),
                          [ptr](const RegisteredResource& r) { return r.ptr == ptr; }),
            m_resources.end());
    }
    
    void CleanupAll() {
        m_shutdown.store(true);
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Sort by priority (descending)
        std::sort(m_resources.begin(), m_resources.end(),
                 [](const RegisteredResource& a, const RegisteredResource& b) {
                     return a.priority > b.priority;
                 });
        
        // Cleanup in order
        for (auto& res : m_resources) {
            if (res.cleanup) {
                try {
                    if (res.name) {
                        TitanLogError(LogLevel::Debug, TitanError::OK,
                                     "Cleaning up: %s (type=%d)", res.name, (int)res.type);
                    }
                    res.cleanup(res.ptr);
                } catch (...) {
                    TitanLogError(LogLevel::Warn, TitanError::E_RESOURCE_LEAKED,
                                 "Exception during cleanup of resource %p", res.ptr);
                }
            }
        }
        m_resources.clear();
    }
    
    size_t Count() const {
        return m_resources.size();
    }
    
    void DumpResources() {
        std::lock_guard<std::mutex> lock(m_mutex);
        TitanLogError(LogLevel::Info, TitanError::OK,
                     "=== Registered Resources: %zu ===", m_resources.size());
        for (const auto& res : m_resources) {
            const char* type_names[] = {
                "VirtualMemory", "HeapMemory", "FileHandle",
                "VulkanBuffer", "VulkanMemory", "VulkanPipeline",
                "DirectStorageQueue", "DirectStorageFile", "GGMLContext", "Other"
            };
            TitanLogError(LogLevel::Info, TitanError::OK,
                         "  %p: %s (priority=%d) %s",
                         res.ptr, type_names[(int)res.type], res.priority,
                         res.name ? res.name : "");
        }
    }
    
private:
    CleanupRegistry() = default;
    ~CleanupRegistry() {
        if (!m_shutdown.load()) {
            CleanupAll();
        }
    }
};

// ============================================================
// CONVENIENCE MACROS FOR REGISTRATION
// ============================================================

#define TITAN_REGISTER_VIRTUALALLOC(ptr, size) \
    CleanupRegistry::Instance().Register(ptr, ResourceType::VirtualMemory, \
        [](void* p) { VirtualFree(p, 0, MEM_RELEASE); }, 100, #ptr)

#define TITAN_REGISTER_HANDLE(handle) \
    CleanupRegistry::Instance().Register((void*)(uintptr_t)handle, ResourceType::FileHandle, \
        [](void* h) { CloseHandle((HANDLE)(uintptr_t)h); }, 50, #handle)

#define TITAN_REGISTER_GGML(ctx) \
    CleanupRegistry::Instance().Register(ctx, ResourceType::GGMLContext, \
        [](void* c) { ggml_free((ggml_context*)c); }, 200, #ctx)

// ============================================================
// ERROR CHECKING HELPERS
// ============================================================

#define TITAN_CHECK_HR(hr, msg) do { \
    HRESULT _hr = (hr); \
    if (FAILED(_hr)) { \
        TitanLogError(LogLevel::Error, TitanError::E_INIT_FAILED, \
                     "%s: HRESULT=0x%08X", msg, (unsigned int)_hr); \
        return _hr; \
    } \
} while(0)

#define TITAN_CHECK_VK(result, msg) do { \
    VkResult _vk = (result); \
    if (_vk != VK_SUCCESS) { \
        TitanLogError(LogLevel::Error, TitanError::E_VULKAN_INIT, \
                     "%s: VkResult=%d", msg, (int)_vk); \
        return (HRESULT)_vk; \
    } \
} while(0)

#define TITAN_CHECK_HANDLE(handle, msg) do { \
    if ((handle) == INVALID_HANDLE_VALUE || (handle) == nullptr) { \
        TitanLogError(LogLevel::Error, TitanError::E_FILE_OPEN, \
                     "%s: GetLastError=%lu", msg, GetLastError()); \
        return E_FAIL; \
    } \
} while(0)

#define TITAN_CHECK_PTR(ptr, msg) do { \
    if ((ptr) == nullptr) { \
        TitanLogError(LogLevel::Error, TitanError::E_MEMORY_ALLOC, \
                     "%s: allocation failed", msg); \
        return E_OUTOFMEMORY; \
    } \
} while(0)

// ============================================================
// GLOBAL CLEANUP FUNCTION
// ============================================================

extern "C" {

void Titan_CleanupAll() {
    TitanLogError(LogLevel::Info, TitanError::OK, "Beginning global cleanup...");
    CleanupRegistry::Instance().DumpResources();
    CleanupRegistry::Instance().CleanupAll();
    
    // Close event log
    if (g_event_log) {
        DeregisterEventSource(g_event_log);
        g_event_log = nullptr;
    }
    
    TitanLogError(LogLevel::Info, TitanError::OK, "Global cleanup complete");
}

void Titan_RegisterResource(void* ptr, int type, void (*cleanup)(void*), int priority, const char* name) {
    CleanupRegistry::Instance().Register(ptr, (ResourceType)type,
                                         cleanup ? std::function<void(void*)>(cleanup) : nullptr,
                                         priority, name);
}

void Titan_UnregisterResource(void* ptr) {
    CleanupRegistry::Instance().Unregister(ptr);
}

HRESULT Titan_GetLastError(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return E_INVALIDARG;
    strncpy_s(buffer, bufferSize, g_last_error_message, _TRUNCATE);
    return (HRESULT)g_last_error.load();
}

void Titan_DumpResourceLeaks() {
    CleanupRegistry::Instance().DumpResources();
}

}  // extern "C"

// ============================================================
// MEMORY TRACKING (Debug builds)
// ============================================================

#ifdef _DEBUG

static std::atomic<size_t> g_total_allocated{0};
static std::atomic<size_t> g_total_allocations{0};
static std::atomic<size_t> g_total_frees{0};

void* Titan_TrackedAlloc(size_t size, const char* file, int line) {
    void* ptr = VirtualAlloc(nullptr, size + sizeof(size_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ptr) {
        *(size_t*)ptr = size;
        g_total_allocated += size;
        g_total_allocations++;
        TitanLogError(LogLevel::Debug, TitanError::OK,
                     "ALLOC: %zu bytes at %s:%d (total: %zu)", 
                     size, file, line, g_total_allocated.load());
        return (char*)ptr + sizeof(size_t);
    }
    return nullptr;
}

void Titan_TrackedFree(void* ptr, const char* file, int line) {
    if (ptr) {
        void* real_ptr = (char*)ptr - sizeof(size_t);
        size_t size = *(size_t*)real_ptr;
        g_total_allocated -= size;
        g_total_frees++;
        TitanLogError(LogLevel::Debug, TitanError::OK,
                     "FREE: %zu bytes at %s:%d (total: %zu)", 
                     size, file, line, g_total_allocated.load());
        VirtualFree(real_ptr, 0, MEM_RELEASE);
    }
}

void Titan_DumpMemoryStats() {
    TitanLogError(LogLevel::Info, TitanError::OK,
                 "=== Memory Stats ===");
    TitanLogError(LogLevel::Info, TitanError::OK,
                 "  Total allocated: %zu bytes", g_total_allocated.load());
    TitanLogError(LogLevel::Info, TitanError::OK,
                 "  Total allocations: %zu", g_total_allocations.load());
    TitanLogError(LogLevel::Info, TitanError::OK,
                 "  Total frees: %zu", g_total_frees.load());
    TitanLogError(LogLevel::Info, TitanError::OK,
                 "  Potential leaks: %zu", 
                 g_total_allocations.load() - g_total_frees.load());
}

#define TITAN_ALLOC(size) Titan_TrackedAlloc(size, __FILE__, __LINE__)
#define TITAN_FREE(ptr) Titan_TrackedFree(ptr, __FILE__, __LINE__)

#else

#define TITAN_ALLOC(size) VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define TITAN_FREE(ptr) VirtualFree(ptr, 0, MEM_RELEASE)

#endif
