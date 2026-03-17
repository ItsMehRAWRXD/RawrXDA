#pragma once

#include <windows.h>
#include <intrin.h>
#include <immintrin.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <mutex>
#include <functional>
#include <filesystem>
#include <thread>
#include <chrono>

// Performance counters structure
struct PerfCounters {
    uint64_t instructions = 0;
    uint64_t cycles = 0;
    uint64_t cache_references = 0;
    uint64_t cache_misses = 0;
    uint64_t branch_misses = 0;
};

// Drive mapping structure
struct DriveMapping {
    HANDLE file_handle = INVALID_HANDLE_VALUE;
    HANDLE mapping_handle = INVALID_HANDLE_VALUE;
    void* mapped_view = nullptr;
    size_t file_size = 0;
};

class SystemsIDECore {
private:
    // Memory pool for optimized allocation
    struct Block {
        size_t size;
        bool in_use;
        Block* next;
    };
    
    class MemoryPool {
    private:
        void* pool_memory_;
        size_t pool_size_;
        Block* free_list_;
        std::mutex pool_mutex_;
    public:
        MemoryPool(size_t size);
        ~MemoryPool();
        void* Allocate(size_t size);
        void Deallocate(void* ptr);
        void* AllocateAligned(size_t size, size_t alignment);
    };
    
    // Core members
    std::unique_ptr<MemoryPool> memory_pool_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};
    std::unordered_map<std::string, std::string> file_cache_;
    PerfCounters perf_counters_;
    DriveMapping drive_mapping_;
    
public:
    SystemsIDECore();
    ~SystemsIDECore();
    
    // Core functions
    bool Initialize();
    void Shutdown();
    
    // File operations
    bool MapDrive(const std::string& drive_letter);
    std::vector<std::string> ListDirectory(const std::string& path);
    std::string ReadFileOptimized(const std::string& path);
    bool WriteFileOptimized(const std::string& path, const std::string& content);
    std::vector<std::string> SearchFiles(const std::string& pattern, const std::string& root_path);
    
    // Performance monitoring
    void ReadPerfCounters();
    void StartPerfCounters();
    double GetCacheHitRatio() const;
    
    // Assembly-optimized functions
    static void DirectSyscall(DWORD syscall_number, ...);
    static bool FastStringCompare(const char* str1, const char* str2, size_t length);
    static void MemCopySSE(void* dest, const void* src, size_t size);
};

// C interface for keychain integration
extern "C" {
    bool ReadFileC(SystemsIDECore* core, const char* path, char** content, size_t* size);
    bool WriteFileC(SystemsIDECore* core, const char* path, const char* content);
    bool ListDirectoryC(SystemsIDECore* core, const char* path, char*** files, size_t* count);
}