// ============================================================================
// multi_gpu.h — Multi-GPU Inference Distribution (Feature 0x80)
// ============================================================================
// Provides multi-GPU dispatch, PCIe topology detection, memory synchronization,
// and tensor parallelism for enterprise-licensed users.
//
// Architecture:
//   MultiGPUManager::Instance()
//    ├─ GPU enumeration & topology detection
//    ├─ Tensor parallelism dispatch
//    ├─ Memory sync across devices
//    └─ Load balancing & health monitoring
//
// PATTERN:   No exceptions. No std::function. Raw function pointers only.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

namespace RawrXD::Enterprise {

// ============================================================================
// GPU Device Info
// ============================================================================
struct GPUDeviceInfo {
    uint32_t    deviceId;           // Logical device index
    const char* name;               // Device name (e.g., "NVIDIA RTX 4090")
    const char* vendor;             // "NVIDIA", "AMD", "Intel"
    uint64_t    vramBytes;          // Total VRAM in bytes
    uint64_t    vramFreeBytes;      // Free VRAM
    uint32_t    computeUnits;       // CU/SM count
    uint32_t    pcieGen;            // PCIe generation (3, 4, 5)
    uint32_t    pcieLanes;          // PCIe lane count (x1, x4, x8, x16)
    float       pcieBandwidthGBs;   // Measured PCIe bandwidth GB/s
    bool        supportsP2P;        // Direct GPU-to-GPU transfers
    bool        available;          // Device is healthy and usable
};

// ============================================================================
// Topology Link — describes connection between two GPUs
// ============================================================================
enum class LinkType : uint32_t {
    PCIe     = 0,   // Standard PCIe bridge
    NVLink   = 1,   // NVIDIA NVLink
    XGMI     = 2,   // AMD Infinity Fabric
    None     = 3,   // No direct connection
};

struct TopologyLink {
    uint32_t srcDevice;
    uint32_t dstDevice;
    LinkType type;
    float    bandwidthGBs;      // Measured bandwidth
    float    latencyUs;         // Measured latency
};

// ============================================================================
// Dispatch Strategy — how tensors are distributed across GPUs
// ============================================================================
enum class DispatchStrategy : uint32_t {
    LayerParallel    = 0,   // Each GPU handles contiguous model layers
    TensorParallel   = 1,   // Tensors split across GPUs (row/column parallel)
    PipelineParallel = 2,   // Pipeline stages across GPUs
    DataParallel     = 3,   // Same model on each GPU, different batches
    Hybrid           = 4,   // Combination based on topology
};

// ============================================================================
// Multi-GPU Result — structured return (no exceptions)
// ============================================================================
struct MultiGPUResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static MultiGPUResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static MultiGPUResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// GPU Load Stats
// ============================================================================
struct GPULoadStats {
    uint32_t deviceId;
    float    utilization;       // 0.0–1.0
    uint64_t layersAssigned;
    uint64_t tensorsProcessed;
    uint64_t memoryUsedBytes;
    float    throughputToksPerSec;
};

// ============================================================================
// Layer Assignment — dispatch plan per device
// ============================================================================
struct LayerAssignment {
    uint32_t         deviceId;
    uint32_t         startLayer;        // Inclusive
    uint32_t         endLayer;          // Inclusive
    uint64_t         vramBudgetBytes;   // Budget used for this assignment
    DispatchStrategy strategy;
    uint32_t         tensorSplitFactor; // Tensor-parallel split factor
};

// ============================================================================
// Callbacks — raw function pointers (NO std::function)
// ============================================================================
using GPUHealthChangeFn  = void(*)(uint32_t deviceId, bool healthy);
using DispatchCompleteFn = void(*)(uint32_t batchId, float elapsedMs);

// ============================================================================
// Multi-GPU Manager — Singleton
// ============================================================================
class MultiGPUManager {
public:
    static MultiGPUManager& Instance();

    // Non-copyable
    MultiGPUManager(const MultiGPUManager&) = delete;
    MultiGPUManager& operator=(const MultiGPUManager&) = delete;

    // ---- Lifecycle ----
    MultiGPUResult Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    // ---- Device Enumeration ----
    uint32_t GetDeviceCount() const;
    const GPUDeviceInfo& GetDeviceInfo(uint32_t deviceId) const;
    const std::vector<GPUDeviceInfo>& GetAllDevices() const;

    // ---- Topology ----
    MultiGPUResult DetectTopology();
    const std::vector<TopologyLink>& GetTopologyLinks() const;
    bool SupportsP2P(uint32_t srcDevice, uint32_t dstDevice) const;

    // ---- Dispatch Configuration ----
    MultiGPUResult SetStrategy(DispatchStrategy strategy);
    DispatchStrategy GetStrategy() const;
    const char* GetStrategyName(DispatchStrategy strategy) const;

    // ---- Dispatch Planning ----
    MultiGPUResult BuildLayerAssignments(uint32_t totalLayers,
                                         uint64_t modelBytes,
                                         DispatchStrategy strategy);
    const std::vector<LayerAssignment>& GetLayerAssignments() const;
    void ClearLayerAssignments();

    // ---- Load Monitoring ----
    std::vector<GPULoadStats> GetLoadStats() const;
    float GetTotalThroughput() const;
    uint64_t GetTotalVRAM() const;
    uint64_t GetFreeVRAM() const;

    // ---- Health ----
    bool AllDevicesHealthy() const;
    MultiGPUResult RunHealthCheck();

    // ---- Status / Reporting ----
    std::string GenerateStatusReport() const;
    std::string GenerateTopologyReport() const;

    // ---- Callbacks ----
    void OnHealthChange(GPUHealthChangeFn fn)       { m_onHealthChange = fn; }
    void OnDispatchComplete(DispatchCompleteFn fn)   { m_onDispatchComplete = fn; }

private:
    MultiGPUManager() = default;
    ~MultiGPUManager() = default;

    MultiGPUResult enumerateDevices();

    bool                        m_initialized = false;
    mutable std::mutex          m_mutex;
    std::vector<GPUDeviceInfo>  m_devices;
    std::vector<TopologyLink>   m_topology;
    std::vector<LayerAssignment> m_assignments;
    DispatchStrategy            m_strategy = DispatchStrategy::LayerParallel;

    // Callbacks — raw pointers
    GPUHealthChangeFn           m_onHealthChange = nullptr;
    DispatchCompleteFn          m_onDispatchComplete = nullptr;
};

} // namespace RawrXD::Enterprise

