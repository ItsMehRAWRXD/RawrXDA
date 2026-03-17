/**
 * @file SovereignControlBlock.h
 * @brief Shared Memory Interface for C++/MASM Communication
 * 
 * Defines the SOVEREIGN_CONTROL_BLOCK structure used for inter-process
 * communication between the thermal dashboard C++ logic and the 
 * MASM64 assembly kernel.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rawrxd::thermal {

/**
 * @brief Burst mode enumeration
 */
enum class BurstMode : int32_t {
    SOVEREIGN_MAX = 0,      // Maximum performance, 142μs latency
    THERMAL_GOVERNED = 1,   // Thermal-limited, 237μs latency
    ADAPTIVE_HYBRID = 2     // Dynamic switching, variable latency
};

/**
 * @brief Throttle command flags
 */
enum class ThrottleFlags : uint32_t {
    NONE = 0,
    SOFT_THROTTLE = 0x01,           // Use PAUSE instruction
    HARD_THROTTLE = 0x02,           // Reduce clock
    EMERGENCY_STOP = 0x04,          // Halt all I/O
    PREDICTIVE_ENABLED = 0x08,      // Using predictive data
    DRIVE_SWITCH_PENDING = 0x10,    // Load balancer wants drive change
    SESSION_KEY_VALID = 0x20        // QuantumAuth session established
};

/**
 * @brief Drive selection command
 */
struct DriveCommand {
    int32_t selectedDrive;      // Drive index (0-4)
    int32_t previousDrive;      // Previous drive
    double thermalHeadroom;     // Headroom of selected drive
    uint64_t selectionTime;     // When selection was made
};

/**
 * @brief Thermal prediction data
 */
struct PredictionData {
    double predictedTemp;       // Predicted temperature
    double confidence;          // Prediction confidence (0-1)
    double slope;               // Temperature slope (°C/sec)
    int64_t horizonMs;          // Prediction horizon
    int32_t isValid;            // Prediction validity flag
    int32_t reserved;           // Alignment padding
};

/**
 * @brief Throttle command structure
 */
struct ThrottleCommand {
    int32_t throttlePercent;    // Throttle percentage (0-100)
    uint32_t flags;             // ThrottleFlags
    int32_t pauseLoopCount;     // Number of PAUSE iterations
    int32_t reserved;           // Alignment padding
};

/**
 * @brief Security authentication data
 */
struct AuthData {
    uint64_t sessionKey;        // RDRAND-generated session key
    uint64_t timestamp;         // Key generation timestamp
    uint32_t keyValid;          // Key validity flag
    uint32_t authAttempts;      // Number of auth attempts
};

/**
 * @brief SOVEREIGN_CONTROL_BLOCK - Main shared memory structure
 * 
 * This structure is memory-mapped and shared between:
 * - C++ thermal management logic (writer)
 * - MASM64 kernel dispatch loop (reader)
 * 
 * Layout is carefully designed for cache-line alignment and
 * atomic access patterns.
 */
#pragma pack(push, 1)
struct alignas(64) SovereignControlBlock {
    // ═══════════════════════════════════════════════════════════════════════════
    // Header (64 bytes) - Cache line 0
    // ═══════════════════════════════════════════════════════════════════════════
    uint32_t magic;             // 0x00: Magic number 'RAWR'
    uint32_t version;           // 0x04: Structure version
    uint64_t sequenceNumber;    // 0x08: Update sequence for sync
    uint64_t lastUpdateTime;    // 0x10: Last update timestamp
    int32_t burstMode;          // 0x18: Current BurstMode
    int32_t activeDrives;       // 0x1C: Number of active drives
    uint64_t reserved1[4];      // 0x20-0x3F: Reserved for future use
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Thermal Data (64 bytes) - Cache line 1
    // ═══════════════════════════════════════════════════════════════════════════
    double nvmeTemps[5];        // 0x40-0x67: NVMe temperatures
    double gpuTemp;             // 0x68: GPU temperature
    double cpuTemp;             // 0x70: CPU temperature
    double thermalThreshold;    // 0x78: Current thermal threshold
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Throttle Control (64 bytes) - Cache line 2
    // ═══════════════════════════════════════════════════════════════════════════
    ThrottleCommand throttle;   // 0x80-0x8F: Throttle command
    double softThrottleValue;   // 0x90: Soft throttle value for PWM
    double thermalHeadroom;     // 0x98: Thermal headroom
    int32_t predictionValid;    // 0xA0: Prediction validity
    int32_t reserved2;          // 0xA4: Padding
    uint64_t reserved3[3];      // 0xA8-0xBF: Reserved
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Drive Selection (64 bytes) - Cache line 3
    // ═══════════════════════════════════════════════════════════════════════════
    DriveCommand driveCmd;      // 0xC0-0xDF: Drive command
    double driveLoads[5];       // 0xE0-0xFF + overflow: Drive loads
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Prediction Data (64 bytes) - Cache line 4
    // ═══════════════════════════════════════════════════════════════════════════
    PredictionData prediction;  // 0x100+: Prediction data
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Security (64 bytes) - Cache line 5
    // ═══════════════════════════════════════════════════════════════════════════
    AuthData auth;              // Security/auth data
    
    // Total size: ~384 bytes, rounded to 512 for alignment
    uint8_t padding[128];       // Padding to 512 bytes total
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Constants
    // ═══════════════════════════════════════════════════════════════════════════
    static constexpr uint32_t MAGIC = 0x52415752;  // 'RAWR'
    static constexpr uint32_t VERSION = 0x00010200; // 1.2.0
    static constexpr size_t TOTAL_SIZE = 512;
};
#pragma pack(pop)

// Verify structure size at compile time
static_assert(sizeof(SovereignControlBlock) == SovereignControlBlock::TOTAL_SIZE, 
              "SovereignControlBlock must be 512 bytes");

// ═══════════════════════════════════════════════════════════════════════════════
// Offset constants for MASM access (defined after struct is complete)
// ═══════════════════════════════════════════════════════════════════════════════
namespace SovereignOffsets {
    static constexpr size_t MAGIC = offsetof(SovereignControlBlock, magic);
    static constexpr size_t VERSION = offsetof(SovereignControlBlock, version);
    static constexpr size_t SEQUENCE = offsetof(SovereignControlBlock, sequenceNumber);
    static constexpr size_t TIMESTAMP = offsetof(SovereignControlBlock, lastUpdateTime);
    static constexpr size_t BURST_MODE = offsetof(SovereignControlBlock, burstMode);
    static constexpr size_t ACTIVE_DRIVES = offsetof(SovereignControlBlock, activeDrives);
    static constexpr size_t NVME_TEMPS = offsetof(SovereignControlBlock, nvmeTemps);
    static constexpr size_t GPU_TEMP = offsetof(SovereignControlBlock, gpuTemp);
    static constexpr size_t CPU_TEMP = offsetof(SovereignControlBlock, cpuTemp);
    static constexpr size_t THERMAL_THRESHOLD = offsetof(SovereignControlBlock, thermalThreshold);
    static constexpr size_t THROTTLE = offsetof(SovereignControlBlock, throttle);
    static constexpr size_t SOFT_THROTTLE = offsetof(SovereignControlBlock, softThrottleValue);
    static constexpr size_t HEADROOM = offsetof(SovereignControlBlock, thermalHeadroom);
    static constexpr size_t PREDICTION_VALID = offsetof(SovereignControlBlock, predictionValid);
    static constexpr size_t DRIVE_CMD = offsetof(SovereignControlBlock, driveCmd);
    static constexpr size_t DRIVE_LOADS = offsetof(SovereignControlBlock, driveLoads);
    static constexpr size_t PREDICTION = offsetof(SovereignControlBlock, prediction);
    static constexpr size_t AUTH = offsetof(SovereignControlBlock, auth);
}

/**
 * @brief Shared memory manager for SOVEREIGN_CONTROL_BLOCK
 */
class SharedMemoryManager {
public:
    SharedMemoryManager();
    ~SharedMemoryManager();
    
    /**
     * @brief Create or open shared memory region
     * @param name Shared memory name
     * @return True on success
     */
    bool create(const std::string& name = "RawrXD_ThermalControl");
    
    /**
     * @brief Open existing shared memory region
     * @param name Shared memory name
     * @return True on success
     */
    bool open(const std::string& name = "RawrXD_ThermalControl");
    
    /**
     * @brief Close shared memory
     */
    void close();
    
    /**
     * @brief Check if shared memory is open
     * @return True if open
     */
    bool isOpen() const { return m_controlBlock != nullptr; }
    
    /**
     * @brief Get pointer to control block
     * @return Control block pointer or nullptr
     */
    SovereignControlBlock* getControlBlock() { return m_controlBlock; }
    const SovereignControlBlock* getControlBlock() const { return m_controlBlock; }
    
    /**
     * @brief Initialize control block with defaults
     */
    void initialize();
    
    /**
     * @brief Update sequence number atomically
     */
    void incrementSequence();
    
    /**
     * @brief Update timestamp
     */
    void updateTimestamp();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Convenience methods for common operations
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setNVMeTemperature(int driveIndex, double temp);
    void setGPUTemperature(double temp);
    void setCPUTemperature(double temp);
    void setThrottlePercent(int percent);
    void setSoftThrottle(double value);
    void setBurstMode(BurstMode mode);
    void setSelectedDrive(int driveIndex, double headroom);
    void setPrediction(double predictedTemp, double confidence, bool valid);
    void setSessionKey(uint64_t key);
    
    double getNVMeTemperature(int driveIndex) const;
    double getGPUTemperature() const;
    double getCPUTemperature() const;
    int getThrottlePercent() const;
    BurstMode getBurstMode() const;
    int getSelectedDrive() const;
    uint64_t getSessionKey() const;
    
private:
#ifdef _WIN32
    HANDLE m_hMapping;
#else
    int m_fd;
#endif
    SovereignControlBlock* m_controlBlock;
    std::string m_name;
};

// ═══════════════════════════════════════════════════════════════════════════════
// MASM-compatible offset definitions (for .inc file generation)
// ═══════════════════════════════════════════════════════════════════════════════

// These can be used to generate a .inc file for MASM:
/*
; SovereignControlBlock.inc - Auto-generated from SovereignControlBlock.h
; DO NOT EDIT - Generated by RawrXD build system

SCB_MAGIC               EQU 0
SCB_VERSION             EQU 4
SCB_SEQUENCE            EQU 8
SCB_TIMESTAMP           EQU 16
SCB_BURST_MODE          EQU 24
SCB_ACTIVE_DRIVES       EQU 28
SCB_NVME_TEMPS          EQU 64
SCB_GPU_TEMP            EQU 104
SCB_CPU_TEMP            EQU 112
SCB_THERMAL_THRESHOLD   EQU 120
SCB_THROTTLE            EQU 128
SCB_SOFT_THROTTLE       EQU 144
SCB_HEADROOM            EQU 152
SCB_PREDICTION_VALID    EQU 160
SCB_DRIVE_CMD           EQU 192
SCB_DRIVE_LOADS         EQU 224
SCB_PREDICTION          EQU 256
SCB_AUTH                EQU 320
SCB_TOTAL_SIZE          EQU 512

; Magic value
SCB_MAGIC_VALUE         EQU 052415752h  ; 'RAWR'
SCB_VERSION_VALUE       EQU 000010200h  ; 1.2.0
*/

} // namespace rawrxd::thermal
