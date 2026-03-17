// ============================================================================
// multi_gpu.cpp — Multi-GPU Inference Distribution Implementation
// ============================================================================
// Implements MultiGPUManager singleton for GPU enumeration, PCIe topology
// detection, dispatch strategy, and load monitoring. Gated by feature 0x80.
//
// PATTERN:   No exceptions. No std::function. Raw function pointers only.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "enterprise/multi_gpu.h"
#include "enterprise_license.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dxgi.h>
#endif

namespace RawrXD::Enterprise {

// ============================================================================
// Singleton
// ============================================================================
MultiGPUManager& MultiGPUManager::Instance() {
    static MultiGPUManager instance;
    return instance;
}

// ============================================================================
// Default device for single-GPU fallback
// ============================================================================
static GPUDeviceInfo s_defaultDevice = {
    0, "System Default GPU", "Unknown",
    0, 0, 0, 0, 0, 0.0f,
    false, true
};

// ============================================================================
// Initialize
// ============================================================================
MultiGPUResult MultiGPUManager::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return MultiGPUResult::ok("Already initialized");
    }

    // Enterprise gate: feature 0x80
    if (!RawrXD::EnterpriseLicense::isFeatureEnabled(0x80)) {
        // Enumerate for reporting but don't enable multi-GPU dispatch
        enumerateDevices();
        m_initialized = true;
        std::cout << "[MultiGPU] Single-GPU mode (Multi-GPU requires Enterprise license)\n";
        return MultiGPUResult::ok("Single-GPU mode — multi-GPU requires Enterprise license");
    }

    // Full initialization with multi-GPU support
    auto result = enumerateDevices();
    if (!result.success) {
        return result;
    }

    if (m_devices.size() > 1) {
        // Auto-detect topology
        DetectTopology();

        // Choose default strategy based on device count + topology
        if (m_devices.size() >= 4) {
            m_strategy = DispatchStrategy::Hybrid;
        } else if (m_devices.size() >= 2) {
            m_strategy = DispatchStrategy::LayerParallel;
        }
    }

    m_initialized = true;

    std::cout << "[MultiGPU] Initialized: " << m_devices.size() << " GPU(s) detected, "
              << "strategy: " << GetStrategyName(m_strategy) << "\n";

    return MultiGPUResult::ok("Multi-GPU initialized");
}

// ============================================================================
// Shutdown
// ============================================================================
void MultiGPUManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    std::cout << "[MultiGPU] Shutting down — " << m_devices.size() << " device(s)\n";
    m_devices.clear();
    m_topology.clear();
    m_initialized = false;
}

// ============================================================================
// Device Enumeration
// ============================================================================
MultiGPUResult MultiGPUManager::enumerateDevices() {
    m_devices.clear();

#ifdef _WIN32
    // Use DXGI to enumerate GPU adapters
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (SUCCEEDED(hr) && factory) {
        IDXGIAdapter1* adapter = nullptr;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            // Skip software adapters
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                adapter->Release();
                continue;
            }

            GPUDeviceInfo info{};
            info.deviceId = i;

            // Convert wide string to narrow for storage
            static char nameBuffers[8][128];
            if (i < 8) {
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
                                    nameBuffers[i], 128, nullptr, nullptr);
                info.name = nameBuffers[i];
            } else {
                info.name = "GPU";
            }

            // Determine vendor
            if (desc.VendorId == 0x10DE) info.vendor = "NVIDIA";
            else if (desc.VendorId == 0x1002) info.vendor = "AMD";
            else if (desc.VendorId == 0x8086) info.vendor = "Intel";
            else info.vendor = "Unknown";

            info.vramBytes = desc.DedicatedVideoMemory;
            info.vramFreeBytes = desc.DedicatedVideoMemory; // Approximation
            info.computeUnits = 0;  // Would need vendor-specific APIs
            info.pcieGen = 4;       // Default assumption
            info.pcieLanes = 16;
            info.pcieBandwidthGBs = 31.5f; // PCIe 4.0 x16 theoretical
            info.supportsP2P = false;
            info.available = true;

            m_devices.push_back(info);
            adapter->Release();
        }
        factory->Release();
    }
#endif

    // If no devices found via DXGI, add a default entry
    if (m_devices.empty()) {
        s_defaultDevice.deviceId = 0;
        m_devices.push_back(s_defaultDevice);
    }

    return MultiGPUResult::ok("Enumeration complete");
}

uint32_t MultiGPUManager::GetDeviceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_devices.size());
}

const GPUDeviceInfo& MultiGPUManager::GetDeviceInfo(uint32_t deviceId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& d : m_devices) {
        if (d.deviceId == deviceId) return d;
    }
    return s_defaultDevice;
}

const std::vector<GPUDeviceInfo>& MultiGPUManager::GetAllDevices() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices;
}

// ============================================================================
// Topology Detection
// ============================================================================
MultiGPUResult MultiGPUManager::DetectTopology() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_topology.clear();

    if (m_devices.size() <= 1) {
        return MultiGPUResult::ok("Single device — no topology");
    }

    // Build all pairwise links
    for (size_t i = 0; i < m_devices.size(); ++i) {
        for (size_t j = i + 1; j < m_devices.size(); ++j) {
            TopologyLink link{};
            link.srcDevice = static_cast<uint32_t>(i);
            link.dstDevice = static_cast<uint32_t>(j);

            // Detect link type based on vendor
            if (m_devices[i].vendor && m_devices[j].vendor) {
                std::string vendorI(m_devices[i].vendor);
                std::string vendorJ(m_devices[j].vendor);

                if (vendorI == "NVIDIA" && vendorJ == "NVIDIA") {
                    link.type = LinkType::PCIe;  // NVLink detection requires NVML
                    link.bandwidthGBs = 31.5f;
                    link.latencyUs = 5.0f;
                } else if (vendorI == "AMD" && vendorJ == "AMD") {
                    link.type = LinkType::PCIe;  // XGMI detection requires ROCm
                    link.bandwidthGBs = 31.5f;
                    link.latencyUs = 5.0f;
                } else {
                    link.type = LinkType::PCIe;
                    link.bandwidthGBs = 15.75f;  // Cross-vendor penalty
                    link.latencyUs = 10.0f;
                }
            } else {
                link.type = LinkType::None;
                link.bandwidthGBs = 0.0f;
                link.latencyUs = 0.0f;
            }

            m_topology.push_back(link);
        }
    }

    return MultiGPUResult::ok("Topology detected");
}

const std::vector<TopologyLink>& MultiGPUManager::GetTopologyLinks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_topology;
}

bool MultiGPUManager::SupportsP2P(uint32_t srcDevice, uint32_t dstDevice) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& link : m_topology) {
        if ((link.srcDevice == srcDevice && link.dstDevice == dstDevice) ||
            (link.srcDevice == dstDevice && link.dstDevice == srcDevice)) {
            return link.type != LinkType::None;
        }
    }
    return false;
}

// ============================================================================
// Dispatch Configuration
// ============================================================================
MultiGPUResult MultiGPUManager::SetStrategy(DispatchStrategy strategy) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_devices.size() <= 1 && strategy != DispatchStrategy::LayerParallel) {
        return MultiGPUResult::error("Multi-GPU dispatch requires 2+ GPUs", 1);
    }

    m_strategy = strategy;
    std::cout << "[MultiGPU] Strategy set to: " << GetStrategyName(strategy) << "\n";
    return MultiGPUResult::ok("Strategy updated");
}

DispatchStrategy MultiGPUManager::GetStrategy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_strategy;
}

const char* MultiGPUManager::GetStrategyName(DispatchStrategy strategy) const {
    switch (strategy) {
        case DispatchStrategy::LayerParallel:    return "Layer Parallel";
        case DispatchStrategy::TensorParallel:   return "Tensor Parallel";
        case DispatchStrategy::PipelineParallel: return "Pipeline Parallel";
        case DispatchStrategy::DataParallel:     return "Data Parallel";
        case DispatchStrategy::Hybrid:           return "Hybrid";
        default:                                 return "Unknown";
    }
}

// ============================================================================
// Load Monitoring
// ============================================================================
std::vector<GPULoadStats> MultiGPUManager::GetLoadStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<GPULoadStats> stats;
    for (const auto& d : m_devices) {
        GPULoadStats s{};
        s.deviceId = d.deviceId;
        s.utilization = 0.0f;  // Would need vendor-specific query (NVML/ROCm)
        s.layersAssigned = 0;
        s.tensorsProcessed = 0;
        s.memoryUsedBytes = d.vramBytes - d.vramFreeBytes;
        s.throughputToksPerSec = 0.0f;
        stats.push_back(s);
    }
    return stats;
}

float MultiGPUManager::GetTotalThroughput() const {
    auto stats = GetLoadStats();
    float total = 0.0f;
    for (const auto& s : stats) total += s.throughputToksPerSec;
    return total;
}

uint64_t MultiGPUManager::GetTotalVRAM() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t total = 0;
    for (const auto& d : m_devices) total += d.vramBytes;
    return total;
}

uint64_t MultiGPUManager::GetFreeVRAM() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t total = 0;
    for (const auto& d : m_devices) total += d.vramFreeBytes;
    return total;
}

// ============================================================================
// Health
// ============================================================================
bool MultiGPUManager::AllDevicesHealthy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& d : m_devices) {
        if (!d.available) return false;
    }
    return true;
}

MultiGPUResult MultiGPUManager::RunHealthCheck() {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t healthy = 0, unhealthy = 0;

    for (auto& d : m_devices) {
        // Basic availability check — in production this would
        // do actual device queries via Vulkan/DX12/NVML
        if (d.available) {
            ++healthy;
        } else {
            ++unhealthy;
            if (m_onHealthChange) {
                m_onHealthChange(d.deviceId, false);
            }
        }
    }

    if (unhealthy > 0) {
        return MultiGPUResult::error("Some devices unhealthy", static_cast<int>(unhealthy));
    }
    return MultiGPUResult::ok("All devices healthy");
}

// ============================================================================
// Status Reports
// ============================================================================
std::string MultiGPUManager::GenerateStatusReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream ss;
    ss << "\n";
    ss << "┌──────────────────────────────────────────────────────────┐\n";
    ss << "│          Multi-GPU Manager Status                       │\n";
    ss << "├──────────────────────────────────────────────────────────┤\n";

    std::string deviceStr = std::to_string(m_devices.size()) + " device(s)";
    ss << "│ Devices:   " << std::left << std::setw(44) << deviceStr << "│\n";
    ss << "│ Strategy:  " << std::left << std::setw(44) << GetStrategyName(m_strategy) << "│\n";

    uint64_t totalVram = 0;
    for (const auto& d : m_devices) totalVram += d.vramBytes;
    std::string vramStr = std::to_string(totalVram / (1024*1024)) + " MB total VRAM";
    ss << "│ VRAM:      " << std::left << std::setw(44) << vramStr << "│\n";

    bool licensed = RawrXD::EnterpriseLicense::isFeatureEnabled(0x80);
    ss << "│ Licensed:  " << std::left << std::setw(44) << (licensed ? "Yes" : "No (single-GPU mode)") << "│\n";

    ss << "├──────────────────────────────────────────────────────────┤\n";
    ss << "│ ID │ Name                           │ VRAM    │ Status  │\n";
    ss << "├────┼────────────────────────────────┼─────────┼─────────┤\n";

    for (const auto& d : m_devices) {
        std::string name = d.name ? std::string(d.name).substr(0, 30) : "Unknown";
        std::string vram = std::to_string(d.vramBytes / (1024*1024)) + "MB";

        ss << "│ " << std::setw(2) << d.deviceId << " │ "
           << std::setw(30) << name << " │ "
           << std::setw(7) << vram << " │ "
           << std::setw(7) << (d.available ? "OK" : "FAIL") << " │\n";
    }

    ss << "└────┴────────────────────────────────┴─────────┴─────────┘\n";
    ss << std::right;  // Reset alignment

    return ss.str();
}

std::string MultiGPUManager::GenerateTopologyReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_topology.empty()) {
        return "  No topology links (single GPU or not yet detected).\n";
    }

    const char* linkNames[] = { "PCIe", "NVLink", "XGMI", "None" };

    std::ostringstream ss;
    ss << "\n  Src → Dst │ Link   │ Bandwidth  │ Latency\n";
    ss << " ───────────┼────────┼────────────┼─────────\n";

    for (const auto& link : m_topology) {
        uint32_t lt = static_cast<uint32_t>(link.type);
        if (lt > 3) lt = 3;

        ss << "  GPU" << link.srcDevice << " → GPU" << link.dstDevice
           << " │ " << std::setw(6) << linkNames[lt]
           << " │ " << std::fixed << std::setprecision(1) << std::setw(7) << link.bandwidthGBs << " GB/s"
           << " │ " << std::setw(5) << link.latencyUs << " us\n";
    }
    ss << "\n";

    return ss.str();
}

} // namespace RawrXD::Enterprise

