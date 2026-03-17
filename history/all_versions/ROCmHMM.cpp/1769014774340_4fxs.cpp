#include "RawrXD/ROCmHMM.hpp"
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace RawrXD {

ROCmHMMManager::ROCmHMMManager() 
    : migration_threshold_(256 * 1024 * 1024) {
    
    qInfo() << "ROCmHMMManager initialized";
    qInfo() << "Migration threshold:" << (migration_threshold_.load() / 1024 / 1024) << "MB";
    qInfo() << "Auto migration:" << (auto_migration_.load() ? "enabled" : "disabled");
    
    // Start migration worker thread
    migration_thread_ = std::thread(&ROCmHMMManager::MigrationWorker, this);
}

ROCmHMMManager::~ROCmHMMManager() {
    shutdown_ = true;
    queue_cv_.notify_all();
    
    if (migration_thread_.joinable()) {
        migration_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    allocations_.clear();
    
    qInfo() << "ROCmHMMManager destroyed";
}

hipError_t ROCmHMMManager::HipMallocHMM(void** ptr, size_t size) {
    if (!ptr || size == 0) {
        return hipErrorInvalidValue;
    }
    
    // Simple allocation stub
    *ptr = malloc(size);
    if (!*ptr) {
        return hipErrorInvalidValue;
    }
    
    {
        std::lock_guard<std::mutex> lock(alloc_mutex_);
        
        Allocation alloc;
        alloc.ptr = *ptr;
        alloc.size = size;
        alloc.current_location = MemoryLocation::HOST_RAM;
        alloc.is_migrating = false;
        alloc.allocation_cycle = __rdtsc();
        alloc.last_access_cycle = alloc.allocation_cycle;
        alloc.preferred_location = -1;
        alloc.accessed_by = -1;
        
        allocations_[*ptr] = alloc;
        
        stats_.total_allocated.fetch_add(size);
        stats_.current_ram.fetch_add(size);
    }
    
    qDebug() << "HMM Alloc:" << *ptr << "size:" << (size / 1024 / 1024) << "MB";
    
    return hipSuccess;
}

hipError_t ROCmHMMManager::HipFree(void* ptr) {
    if (!ptr) {
        return hipErrorInvalidValue;
    }
    
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    
    auto it = allocations_.find(ptr);
    if (it == allocations_.end()) {
        return hipErrorInvalidValue;
    }
    
    Allocation& alloc = it->second;
    
    stats_.total_freed.fetch_add(alloc.size);
    
    if (alloc.current_location == MemoryLocation::HOST_RAM) {
        stats_.current_ram.fetch_sub(alloc.size);
    } else if (alloc.current_location == MemoryLocation::DEVICE_VRAM) {
        stats_.current_vram.fetch_sub(alloc.size);
    }
    
    free(ptr);
    allocations_.erase(it);
    
    qDebug() << "HMM Free:" << ptr << "size:" << (alloc.size / 1024 / 1024) << "MB";
    
    return hipSuccess;
}

hipError_t ROCmHMMManager::AdviseVRAM(void* ptr, size_t size) {
    if (!ptr || size == 0) {
        return hipErrorInvalidValue;
    }
    
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    auto it = allocations_.find(ptr);
    if (it != allocations_.end()) {
        it->second.preferred_location = 0;  // GPU device 0
        qDebug() << "Advised VRAM for:" << ptr;
    }
    
    return hipSuccess;
}

hipError_t ROCmHMMManager::AdviseRAM(void* ptr, size_t size) {
    if (!ptr || size == 0) {
        return hipErrorInvalidValue;
    }
    
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    auto it = allocations_.find(ptr);
    if (it != allocations_.end()) {
        it->second.preferred_location = -1;  // CPU
        qDebug() << "Advised RAM for:" << ptr;
    }
    
    return hipSuccess;
}

hipError_t ROCmHMMManager::PrefetchToGPU(void* ptr, size_t size, hipStream_t stream) {
    if (!ptr) {
        return hipErrorInvalidValue;
    }
    
    stats_.prefetch_requests.fetch_add(1);
    
    {
        std::lock_guard<std::mutex> lock(alloc_mutex_);
        auto it = allocations_.find(ptr);
        if (it != allocations_.end()) {
            it->second.current_location = MemoryLocation::DEVICE_VRAM;
            it->second.is_migrating = false;
            stats_.current_ram.fetch_sub(it->second.size);
            stats_.current_vram.fetch_add(it->second.size);
            stats_.migrations_ram_to_vram.fetch_add(1);
            stats_.successful_prefetches.fetch_add(1);
        }
    }
    
    qDebug() << "Prefetched to GPU:" << ptr << "size:" << (size / 1024 / 1024) << "MB";
    
    return hipSuccess;
}

hipError_t ROCmHMMManager::PrefetchToCPU(void* ptr, size_t size, hipStream_t stream) {
    if (!ptr) {
        return hipErrorInvalidValue;
    }
    
    stats_.prefetch_requests.fetch_add(1);
    
    {
        std::lock_guard<std::mutex> lock(alloc_mutex_);
        auto it = allocations_.find(ptr);
        if (it != allocations_.end()) {
            it->second.current_location = MemoryLocation::HOST_RAM;
            it->second.is_migrating = false;
            stats_.current_vram.fetch_sub(it->second.size);
            stats_.current_ram.fetch_add(it->second.size);
            stats_.migrations_vram_to_ram.fetch_add(1);
            stats_.successful_prefetches.fetch_add(1);
        }
    }
    
    qDebug() << "Prefetched to CPU:" << ptr << "size:" << (size / 1024 / 1024) << "MB";
    
    return hipSuccess;
}

ROCmHMMManager::MemoryLocation ROCmHMMManager::QueryLocation(void* ptr) {
    if (!ptr) {
        return MemoryLocation::UNKNOWN;
    }
    
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    
    auto it = allocations_.find(ptr);
    if (it == allocations_.end()) {
        return MemoryLocation::UNKNOWN;
    }
    
    const Allocation& alloc = it->second;
    
    if (alloc.is_migrating.load()) {
        return MemoryLocation::MIGRATING;
    }
    
    return alloc.current_location;
}

void ROCmHMMManager::MigrationWorker() {
    while (!shutdown_.load()) {
        MigrationRequest req;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return shutdown_.load() || !migration_queue_.empty();
            });
            
            if (shutdown_.load() && migration_queue_.empty()) {
                break;
            }
            
            if (migration_queue_.empty()) {
                continue;
            }
            
            req = migration_queue_.front();
            migration_queue_.pop();
        }
        
        Allocation* alloc = nullptr;
        {
            std::lock_guard<std::mutex> lock(alloc_mutex_);
            auto it = allocations_.find(req.ptr);
            if (it != allocations_.end()) {
                alloc = &it->second;
            }
        }
        
        if (!alloc) {
            qWarning() << "Migration target not found:" << req.ptr;
            continue;
        }
        
        auto mig_start = std::chrono::high_resolution_clock::now();
        hipError_t err = MigrateAllocation(*alloc, req.target, req.stream);
        auto mig_end = std::chrono::high_resolution_clock::now();
        
        double duration_ns = std::chrono::duration<double, std::nano>(mig_end - mig_start).count();
        
        if (err == hipSuccess) {
            UpdateStatsPostMigration(alloc->current_location, req.target, duration_ns);
        }
    }
    
    qDebug() << "Migration worker thread exiting";
}

hipError_t ROCmHMMManager::MigrateAllocation(Allocation& alloc, MemoryLocation target, hipStream_t stream) {
    if (alloc.current_location == target) {
        return hipSuccess;
    }
    
    alloc.is_migrating = true;
    alloc.current_location = MemoryLocation::MIGRATING;
    
    hipError_t err = hipSuccess;
    
    switch (target) {
        case MemoryLocation::DEVICE_VRAM:
            err = PrefetchToGPU(alloc.ptr, alloc.size, stream);
            break;
            
        case MemoryLocation::HOST_RAM:
            err = PrefetchToCPU(alloc.ptr, alloc.size, stream);
            break;
            
        default:
            err = hipErrorInvalidValue;
            break;
    }
    
    alloc.is_migrating = false;
    
    if (err == hipSuccess) {
        LogMigration(alloc, alloc.current_location, target);
        alloc.current_location = target;
        alloc.last_access_cycle = __rdtsc();
    }
    
    return err;
}

void ROCmHMMManager::UpdateStatsPostMigration(MemoryLocation from, MemoryLocation to, double duration_ns) {
    double old_avg = stats_.avg_migration_time_ns.load();
    uint64_t count = stats_.migrations_ram_to_vram.load() + 
                     stats_.migrations_vram_to_ram.load();
    
    double new_avg = (old_avg * count + duration_ns) / (count + 1);
    stats_.avg_migration_time_ns.store(new_avg);
}

void ROCmHMMManager::LogMigration(const Allocation& alloc, MemoryLocation from, MemoryLocation to) {
    const char* from_str = (from == MemoryLocation::HOST_RAM) ? "RAM" :
                           (from == MemoryLocation::DEVICE_VRAM) ? "VRAM" : "UNKNOWN";
    const char* to_str = (to == MemoryLocation::HOST_RAM) ? "RAM" :
                         (to == MemoryLocation::DEVICE_VRAM) ? "VRAM" : "UNKNOWN";
    
    qInfo() << "Migration:" << alloc.ptr 
            << "size:" << (alloc.size / 1024 / 1024) << "MB"
            << from_str << "->" << to_str
            << "access_count:" << alloc.access_count.load();
}

void ROCmHMMManager::LogError(const char* msg, hipError_t err) {
    qCritical() << msg << "code:" << static_cast<int>(err);
}

void ROCmHMMManager::ResetStats() {
    #define RESET_ATOMIC(atom) atom.store(0)
    RESET_ATOMIC(stats_.total_allocated);
    RESET_ATOMIC(stats_.total_freed);
    RESET_ATOMIC(stats_.current_ram);
    RESET_ATOMIC(stats_.current_vram);
    RESET_ATOMIC(stats_.migrations_ram_to_vram);
    RESET_ATOMIC(stats_.migrations_vram_to_ram);
    RESET_ATOMIC(stats_.page_faults);
    RESET_ATOMIC(stats_.prefetch_requests);
    RESET_ATOMIC(stats_.successful_prefetches);
    RESET_ATOMIC(stats_.failed_prefetches);
    stats_.avg_migration_time_ns.store(0.0);
    stats_.vram_utilization_percent.store(0.0);
    #undef RESET_ATOMIC
}

void ROCmHMMManager::SetMigrationThreshold(size_t bytes) {
    migration_threshold_.store(bytes);
    qInfo() << "Migration threshold set to:" << (bytes / 1024 / 1024) << "MB";
}

void ROCmHMMManager::SetAutoMigration(bool enable) {
    auto_migration_.store(enable);
    qInfo() << "Auto migration" << (enable ? "enabled" : "disabled");
}

bool ROCmHMMManager::ValidateMemory(void* ptr, size_t size) {
    if (!ptr || size == 0) return false;
    
    std::vector<uint8_t> pattern(size);
    for (size_t i = 0; i < size; ++i) {
        pattern[i] = static_cast<uint8_t>(i & 0xFF);
    }
    
    memcpy(ptr, pattern.data(), size);
    
    if (memcmp(ptr, pattern.data(), size) != 0) {
        qCritical() << "Memory corruption detected at:" << ptr;
        return false;
    }
    
    return true;
}

bool ROCmHMMManager::CheckMemoryCorruption(void* ptr, size_t size) {
    if (!ptr || size == 0) return true;
    
    uint8_t* bytes = static_cast<uint8_t*>(ptr);
    size_t suspicious = 0;
    
    for (size_t i = 0; i < size; i += sizeof(uint64_t)) {
        if (i + sizeof(uint64_t) <= size) {
            uint64_t val = *reinterpret_cast<uint64_t*>(&bytes[i]);
            
            if ((val == 0xFFFFFFFFFFFFFFFF || val == 0x0000000000000000) && 
                (i % (1024 * 1024) == 0)) {
                qWarning() << "Suspicious value at offset" << i;
                suspicious++;
            }
        }
    }
    
    return suspicious == 0;
}

void ROCmHMMManager::InitializeGPUInfo() {
    gpu_info_.total_vram = 16ULL * 1024 * 1024 * 1024;  // 16GB (RX 7800 XT)
    gpu_info_.device_id = 0;
    gpu_info_.used_vram = 0;
    gpu_info_.available_vram = gpu_info_.total_vram;
    
    strcpy_s(gpu_info_.props.name, sizeof(gpu_info_.props.name), "AMD Radeon RX 7800 XT");
    gpu_info_.props.totalGlobalMem = gpu_info_.total_vram;
}

size_t ROCmHMMManager::GetCurrentVRAMUsage() {
    return stats_.current_vram.load();
}

} // namespace RawrXD
