// src/direct_io/nvme_thermal_stressor.h
// ════════════════════════════════════════════════════════════════════════════════
// SovereignControlBlock NVMe Thermal Poller & I/O Stressor
// C++ Bridge to Pure MASM x64 Implementation
// ════════════════════════════════════════════════════════════════════════════════

#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>

// ════════════════════════════════════════════════════════════════════════════════
// External MASM Functions (linked from nvme_thermal_stressor.obj)
// ════════════════════════════════════════════════════════════════════════════════

extern "C" {
    // Temperature acquisition
    int32_t NVMe_GetTemperature(uint32_t driveId);
    int32_t NVMe_PollAllDrives(int32_t* outTemps, const uint32_t* driveIds, uint32_t count);
    int32_t NVMe_GetCoolestDrive(const uint32_t* driveIds, uint32_t count);
    int32_t NVMe_GetCachedTemp(uint32_t driveId);
    uint32_t NVMe_GetLastError();
    
    // Memory management (4KB aligned for direct I/O)
    void* NVMe_AllocAlignedBuffer(size_t sizeBytes);
    void NVMe_FreeAlignedBuffer(void* ptr);
    
    // Unbuffered I/O operations
    uint64_t NVMe_StressWrite(uint32_t driveId, void* buffer, uint64_t size, 
                              uint32_t offsetLow, uint32_t offsetHigh);
    uint64_t NVMe_StressRead(uint32_t driveId, void* buffer, uint64_t size,
                             uint32_t offsetLow, uint32_t offsetHigh);
    
    // Burst I/O with temperature monitoring
    int32_t NVMe_StressBurst(uint32_t driveId, void* buffer, uint64_t totalBytes, uint32_t isWrite);
}

// ════════════════════════════════════════════════════════════════════════════════
// C++ Wrapper Classes
// ════════════════════════════════════════════════════════════════════════════════

namespace sovereign {

// Default SovereignControlBlock drive IDs
constexpr std::array<uint32_t, 5> DEFAULT_DRIVE_IDS = {0, 1, 2, 4, 5};

// Sector alignment for direct I/O
constexpr size_t SECTOR_ALIGN = 4096;

// Temperature sample result
struct ThermalSample {
    uint32_t driveId;
    int32_t temperature;  // Celsius, or -1 on error
    uint64_t timestampMs;
};

// I/O operation result
struct IOResult {
    uint64_t bytesTransferred;
    int32_t tempBefore;
    int32_t tempAfter;
    uint32_t errorCode;
};

// ════════════════════════════════════════════════════════════════════════════════
// AlignedBuffer - RAII wrapper for 4KB-aligned memory
// ════════════════════════════════════════════════════════════════════════════════
class AlignedBuffer {
public:
    explicit AlignedBuffer(size_t size) 
        : ptr_(NVMe_AllocAlignedBuffer(size))
        , size_(size) {}
    
    ~AlignedBuffer() {
        if (ptr_) {
            NVMe_FreeAlignedBuffer(ptr_);
        }
    }
    
    // Non-copyable
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;
    
    // Movable
    AlignedBuffer(AlignedBuffer&& other) noexcept 
        : ptr_(other.ptr_), size_(other.size_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }
    
    AlignedBuffer& operator=(AlignedBuffer&& other) noexcept {
        if (this != &other) {
            if (ptr_) NVMe_FreeAlignedBuffer(ptr_);
            ptr_ = other.ptr_;
            size_ = other.size_;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    
    void* data() { return ptr_; }
    const void* data() const { return ptr_; }
    size_t size() const { return size_; }
    bool valid() const { return ptr_ != nullptr; }
    
    template<typename T>
    T* as() { return static_cast<T*>(ptr_); }
    
private:
    void* ptr_;
    size_t size_;
};

// ════════════════════════════════════════════════════════════════════════════════
// NVMeThermalMonitor - Temperature monitoring for SovereignControlBlock drives
// ════════════════════════════════════════════════════════════════════════════════
class NVMeThermalMonitor {
public:
    NVMeThermalMonitor() = default;
    explicit NVMeThermalMonitor(const std::vector<uint32_t>& driveIds) 
        : driveIds_(driveIds) {}
    
    // Poll temperature from a single drive
    int32_t getTemperature(uint32_t driveId) const {
        return NVMe_GetTemperature(driveId);
    }
    
    // Poll all configured drives, returns samples with timestamps
    std::vector<ThermalSample> pollAll() const;
    
    // Find the coolest drive among configured drives
    uint32_t getCoolestDrive() const {
        if (driveIds_.empty()) return static_cast<uint32_t>(-1);
        return NVMe_GetCoolestDrive(driveIds_.data(), static_cast<uint32_t>(driveIds_.size()));
    }
    
    // Get cached temperature (from last poll via MASM)
    int32_t getCachedTemp(uint32_t driveId) const {
        return NVMe_GetCachedTemp(driveId);
    }
    
    // Configure drive IDs to monitor
    void setDriveIds(const std::vector<uint32_t>& ids) { driveIds_ = ids; }
    const std::vector<uint32_t>& getDriveIds() const { return driveIds_; }
    
    // Use default SovereignControlBlock drives
    void useDefaultDrives() {
        driveIds_.assign(DEFAULT_DRIVE_IDS.begin(), DEFAULT_DRIVE_IDS.end());
    }
    
private:
    std::vector<uint32_t> driveIds_ = {0, 1, 2, 4, 5};
};

// ════════════════════════════════════════════════════════════════════════════════
// NVMeStressor - Unbuffered I/O stress testing
// ════════════════════════════════════════════════════════════════════════════════
class NVMeStressor {
public:
    // Stress write to a drive (buffer must be 4KB aligned, size multiple of 4KB)
    static IOResult stressWrite(uint32_t driveId, void* buffer, size_t size, uint64_t offset = 0);
    
    // Stress read from a drive
    static IOResult stressRead(uint32_t driveId, void* buffer, size_t size, uint64_t offset = 0);
    
    // Burst I/O with automatic temperature monitoring
    // Returns final temperature after burst completes
    static int32_t burstIO(uint32_t driveId, void* buffer, size_t totalBytes, bool write);
    
    // Allocate 4KB-aligned buffer
    static AlignedBuffer allocateBuffer(size_t size) {
        return AlignedBuffer(size);
    }
    
    // Get last error code
    static uint32_t getLastError() {
        return NVMe_GetLastError();
    }
    
    // Round size up to 4KB boundary
    static constexpr size_t alignSize(size_t size) {
        return (size + SECTOR_ALIGN - 1) & ~(SECTOR_ALIGN - 1);
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// SovereignThermalGovernor - Temperature-aware drive rotation
// ════════════════════════════════════════════════════════════════════════════════
class SovereignThermalGovernor {
public:
    explicit SovereignThermalGovernor(int32_t maxTempC = 70)
        : maxTemp_(maxTempC) {
        monitor_.useDefaultDrives();
    }
    
    // Select best drive for next I/O operation based on temperature
    uint32_t selectDrive();
    
    // Check if a specific drive is within thermal limits
    bool isDriveCool(uint32_t driveId) const {
        return monitor_.getTemperature(driveId) < maxTemp_;
    }
    
    // Get thermal status of all drives
    std::vector<ThermalSample> getThermalStatus() {
        return monitor_.pollAll();
    }
    
    // Set maximum allowed temperature
    void setMaxTemp(int32_t tempC) { maxTemp_ = tempC; }
    int32_t getMaxTemp() const { return maxTemp_; }
    
    // Configure which drives to manage
    void setDrives(const std::vector<uint32_t>& drives) {
        monitor_.setDriveIds(drives);
    }
    
private:
    NVMeThermalMonitor monitor_;
    int32_t maxTemp_;
    uint32_t lastSelectedDrive_ = 0;
};

} // namespace sovereign
