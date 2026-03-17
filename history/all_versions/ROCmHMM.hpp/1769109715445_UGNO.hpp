#pragma once

#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>
#include <condition_variable>
#include <QDebug>

// Forward declare HIP types if not available
#ifndef HIP_RUNTIME_H
typedef int hipError_t;
typedef int hipStream_t;
typedef int hipDevice_t;
struct hipDeviceProp_t {
    char name[256];
    size_t totalGlobalMem;
};
#define hipSuccess 0
#define hipErrorInvalidValue 1
#define hipCpuDeviceId -1
#define hipMemAttachGlobal 0
#endif

namespace RawrXD {

// Reverse-engineered from AMD ROCm 6.0 HMM (Heterogeneous Memory Management)
// Enables transparent migration between RAM and VRAM for 800B models
class ROCmHMMManager {
public:
    ROCmHMMManager();
    ~ROCmHMMManager();
    
    // No copy/move
    ROCmHMMManager(const ROCmHMMManager&) = delete;
    ROCmHMMManager& operator=(const ROCmHMMManager&) = delete;
    ROCmHMMManager(ROCmHMMManager&&) = delete;
    ROCmHMMManager& operator=(ROCmHMMManager&&) = delete;
    
    // Allocate unified memory that can migrate
    hipError_t HipMallocHMM(void** ptr, size_t size);
    
    // Free unified memory
    hipError_t HipFree(void* ptr);
    
    // Memory advice (migrate proactively)
    hipError_t AdviseVRAM(void* ptr, size_t size);
    hipError_t AdviseRAM(void* ptr, size_t size);
    
    // Prefetch (explicit migration)
    hipError_t PrefetchToGPU(void* ptr, size_t size, hipStream_t stream = 0);
    hipError_t PrefetchToCPU(void* ptr, size_t size, hipStream_t stream = 0);
    
    // Query memory location
    enum class MemoryLocation {
        HOST_RAM = 0,      // System DDR5
        DEVICE_VRAM = 1,   // GPU VRAM
        MIGRATING = 2,     // In flight
        UNKNOWN = 3,
        PAGE_FAULT = 4     // Triggers migration
    };
    MemoryLocation QueryLocation(void* ptr);
    
    // Statistics
    struct Stats {
        std::atomic<uint64_t> total_allocated{0};
        std::atomic<uint64_t> total_freed{0};
        std::atomic<uint64_t> current_ram{0};
        std::atomic<uint64_t> current_vram{0};
        std::atomic<uint64_t> migrations_ram_to_vram{0};
        std::atomic<uint64_t> migrations_vram_to_ram{0};
        std::atomic<uint64_t> page_faults{0};
        std::atomic<uint64_t> prefetch_requests{0};
        std::atomic<uint64_t> successful_prefetches{0};
        std::atomic<uint64_t> failed_prefetches{0};
        std::atomic<double> avg_migration_time_ns{0.0};
        std::atomic<double> vram_utilization_percent{0.0};

        Stats() = default;
        Stats(const Stats& other) {
            total_allocated.store(other.total_allocated.load());
            total_freed.store(other.total_freed.load());
            current_ram.store(other.current_ram.load());
            current_vram.store(other.current_vram.load());
            migrations_ram_to_vram.store(other.migrations_ram_to_vram.load());
            migrations_vram_to_ram.store(other.migrations_vram_to_ram.load());
            page_faults.store(other.page_faults.load());
            prefetch_requests.store(other.prefetch_requests.load());
            successful_prefetches.store(other.successful_prefetches.load());
            failed_prefetches.store(other.failed_prefetches.load());
            avg_migration_time_ns.store(other.avg_migration_time_ns.load());
            vram_utilization_percent.store(other.vram_utilization_percent.load());
        }

        Stats& operator=(const Stats& other) {
            if (this != &other) {
                total_allocated.store(other.total_allocated.load());
                total_freed.store(other.total_freed.load());
                current_ram.store(other.current_ram.load());
                current_vram.store(other.current_vram.load());
                migrations_ram_to_vram.store(other.migrations_ram_to_vram.load());
                migrations_vram_to_ram.store(other.migrations_vram_to_ram.load());
                page_faults.store(other.page_faults.load());
                prefetch_requests.store(other.prefetch_requests.load());
                successful_prefetches.store(other.successful_prefetches.load());
                failed_prefetches.store(other.failed_prefetches.load());
                avg_migration_time_ns.store(other.avg_migration_time_ns.load());
                vram_utilization_percent.store(other.vram_utilization_percent.load());
            }
            return *this;
        }
    };
    Stats GetStats() const { return stats_; }
    void ResetStats();
    
    // Performance tuning
    void SetMigrationThreshold(size_t bytes);
    void SetAutoMigration(bool enable);
    bool GetAutoMigration() const { return auto_migration_.load(); }
    
    // Validation
    bool ValidateMemory(void* ptr, size_t size);
    bool CheckMemoryCorruption(void* ptr, size_t size);
    
    private:
    // Allocation tracking
    struct Allocation {
        void* ptr;
        size_t size;
        MemoryLocation current_location;
        std::atomic<bool> is_migrating{false};
        uint64_t allocation_cycle;
        uint64_t last_access_cycle;
        int preferred_location;
        int accessed_by;
        std::atomic<uint32_t> access_count{0};

        Allocation() : ptr(nullptr), size(0), current_location(MemoryLocation::UNKNOWN), allocation_cycle(0), last_access_cycle(0), preferred_location(0), accessed_by(0) {}
        
        Allocation(const Allocation& other) {
            ptr = other.ptr;
            size = other.size;
            current_location = other.current_location;
            is_migrating.store(other.is_migrating.load());
            allocation_cycle = other.allocation_cycle;
            last_access_cycle = other.last_access_cycle;
            preferred_location = other.preferred_location;
            accessed_by = other.accessed_by;
            access_count.store(other.access_count.load());
        }

        Allocation& operator=(const Allocation& other) {
            if (this != &other) {
                ptr = other.ptr;
                size = other.size;
                current_location = other.current_location;
                is_migrating.store(other.is_migrating.load());
                allocation_cycle = other.allocation_cycle;
                last_access_cycle = other.last_access_cycle;
                preferred_location = other.preferred_location;
                accessed_by = other.accessed_by;
                access_count.store(other.access_count.load());
            }
            return *this;
        }
    };
    
    std::unordered_map<void*, Allocation> allocations_;
    mutable std::mutex alloc_mutex_;
    
    // Migration queue
    struct MigrationRequest {
        void* ptr;
        size_t size;
        MemoryLocation target;
        hipStream_t stream;
        std::chrono::high_resolution_clock::time_point enqueue_time;
    };
    
    std::queue<MigrationRequest> migration_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread migration_thread_;
    std::atomic<bool> shutdown_{false};
    std::atomic<bool> auto_migration_{true};
    std::atomic<size_t> migration_threshold_{256 * 1024 * 1024};
    
    // Hardware topology
    struct {
        size_t total_vram;
        size_t used_vram;
        size_t available_vram;
        int device_id;
        hipDeviceProp_t props;
    } gpu_info_;
    
    void InitializeGPUInfo();
    size_t GetCurrentVRAMUsage();
    
    // Statistics
    mutable Stats stats_;
    
    // Migration worker
    void MigrationWorker();
    hipError_t MigrateAllocation(Allocation& alloc, MemoryLocation target, hipStream_t stream);
    void UpdateStatsPostMigration(MemoryLocation from, MemoryLocation to, double duration_ns);
    void LogMigration(const Allocation& alloc, MemoryLocation from, MemoryLocation to);
    void LogError(const char* msg, hipError_t err);
};

} // namespace RawrXD
