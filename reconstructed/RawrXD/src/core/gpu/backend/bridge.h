// ============================================================================
// gpu_backend_bridge.h — Phase 9B: GPU Compute Backend Bridge
// ============================================================================
// DirectX 12 compute bridge that connects the Phase 8B Backend Switcher and
// Phase 9A Streaming Engine Registry to actual GPU hardware.
//
// Responsibilities:
//   1. DX12 device enumeration and initialization (best adapter selection)
//   2. GPU VRAM allocation (DEFAULT heap) and upload (UPLOAD heap)
//   3. Compute command list management (dispatch, fence, sync)
//   4. Integration with StreamingEngineRegistry as "GPU-DX12-Compute" engine
//   5. Capability detection (Shader Model, FP16, INT8, wavefront)
//
// Design:
//   - No exceptions (all methods return bool or error codes)
//   - No STL allocators in hot paths
//   - Thread-safe via std::mutex on state mutations
//   - Graceful fallback: if DX12 unavailable, reports as such without crash
//   - Does NOT include d3d12.h directly — uses forward declarations and
//     runtime loading via LoadLibrary to avoid hard link dependency on
//     systems without DX12 (e.g., Windows 7, Server Core).
//
// Build: Compiled into RawrXD-Win32IDE and optionally RawrEngine.
//        Requires: d3d12.lib dxgi.lib (linked conditionally)
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

// Forward declarations — no d3d12.h required in consumer headers
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12Fence;
struct ID3D12Resource;
struct ID3D12PipelineState;
struct ID3D12RootSignature;
struct IDXGIFactory4;
struct IDXGIAdapter1;

namespace RawrXD {
namespace GPU {

// ============================================================================
// Compute API selection
// ============================================================================
enum class ComputeAPI : uint32_t {
    None        = 0,
    DirectX12   = 1,    // Windows 10+ default
    Vulkan      = 2,    // Cross-platform (future)
    CPU_AVX512  = 3,    // CPU fallback via MASM AVX-512 kernels
};

// ============================================================================
// GPU capability profile (detected at init)
// ============================================================================
struct GPUCapabilities {
    uint64_t    dedicatedVRAM       = 0;    // Bytes
    uint64_t    sharedSystemRAM     = 0;    // Bytes
    uint64_t    currentUsage        = 0;    // Bytes currently in use
    uint32_t    vendorId            = 0;    // PCI vendor (0x10DE=NV, 0x1002=AMD, 0x8086=Intel)
    uint32_t    deviceId            = 0;    // PCI device
    uint32_t    shaderModelMajor    = 0;    // e.g., 6
    uint32_t    shaderModelMinor    = 0;    // e.g., 5
    uint32_t    wavefrontSize       = 32;   // 32 (NV) or 64 (AMD)
    bool        supportsFP16        = false;
    bool        supportsINT8        = false;
    bool        supportsFP64        = false;
    bool        supportsUnorderedAccess = false;
    std::string adapterName;                // e.g., "NVIDIA GeForce RTX 4090"
};

// ============================================================================
// VRAM allocation handle
// ============================================================================
struct VRAMAllocation {
    ID3D12Resource* resource    = nullptr;
    uint64_t        gpuVA       = 0;        // GPU virtual address
    uint64_t        sizeBytes   = 0;
    bool            valid       = false;
};

// ============================================================================
// Compute dispatch descriptor
// ============================================================================
struct ComputeDispatch {
    ID3D12PipelineState*    pso             = nullptr;
    ID3D12RootSignature*    rootSig         = nullptr;
    uint32_t                groupsX         = 1;
    uint32_t                groupsY         = 1;
    uint32_t                groupsZ         = 1;
    // SRV/UAV resources bound via root signature (set externally)
};

// ============================================================================
// Bridge result (no exceptions)
// ============================================================================
struct GPUResult {
    bool        success     = false;
    int32_t     errorCode   = 0;    // HRESULT or custom
    std::string detail;

    static GPUResult ok(const std::string& msg = "") {
        return { true, 0, msg };
    }
    static GPUResult error(int32_t code, const std::string& msg) {
        return { false, code, msg };
    }
};

// ============================================================================
// GPUBackendBridge — DX12 Compute Bridge
// ============================================================================
class GPUBackendBridge {
public:
    using LogCallback = std::function<void(int, const std::string&)>;

    GPUBackendBridge();
    ~GPUBackendBridge();

    // No copy/move (singleton-style usage)
    GPUBackendBridge(const GPUBackendBridge&) = delete;
    GPUBackendBridge& operator=(const GPUBackendBridge&) = delete;

    // ---- Lifecycle ----
    GPUResult initialize(ComputeAPI preferred = ComputeAPI::DirectX12);
    GPUResult shutdown();
    bool isInitialized() const { return initialized_.load(); }
    ComputeAPI activeAPI() const { return activeAPI_; }

    // ---- Capability Detection ----
    GPUCapabilities getCapabilities() const;
    bool checkShaderModel(uint32_t major, uint32_t minor) const;
    std::string getCapabilitiesString() const;

    // ---- VRAM Management (integrates with Quad-Buffer) ----
    VRAMAllocation allocateVRAM(uint64_t sizeBytes);
    void           freeVRAM(VRAMAllocation& alloc);
    uint64_t       getUsedVRAM() const { return usedVRAM_.load(); }
    uint64_t       getTotalVRAM() const { return caps_.dedicatedVRAM; }

    // ---- Host <-> Device Transfers ----
    GPUResult copyHostToDevice(VRAMAllocation& dest, const void* hostSrc, uint64_t sizeBytes);
    GPUResult copyDeviceToHost(void* hostDst, const VRAMAllocation& src, uint64_t sizeBytes);

    // ---- Compute Dispatch ----
    // Submit a compute dispatch. Returns fence value for tracking.
    uint64_t  submitCompute(const ComputeDispatch& dispatch);
    // Wait for a fence value to complete.
    GPUResult waitForFence(uint64_t fenceValue, uint32_t timeoutMs = 5000);
    // Execute and wait (synchronous convenience).
    GPUResult executeSync(const ComputeDispatch& dispatch, uint32_t timeoutMs = 5000);

    // ---- Registry Integration ----
    // Register "GPU-DX12-Compute" engine with the StreamingEngineRegistry
    void registerWithStreamingRegistry();

    // ---- Diagnostics ----
    std::string getDiagnosticsString() const;
    uint64_t    getTotalDispatches() const { return totalDispatches_.load(); }
    uint64_t    getTotalBytesUploaded() const { return totalBytesUploaded_.load(); }
    uint64_t    getTotalBytesDownloaded() const { return totalBytesDownloaded_.load(); }

    // ---- Logging ----
    void setLogCallback(LogCallback cb) { logCb_ = std::move(cb); }

    // ---- Raw Device Access (for advanced usage) ----
    ID3D12Device*             getDevice() const { return device_; }
    ID3D12CommandQueue*       getCommandQueue() const { return cmdQueue_; }

private:
    // DX12 objects
    ID3D12Device*               device_         = nullptr;
    ID3D12CommandQueue*         cmdQueue_       = nullptr;
    ID3D12CommandAllocator*     cmdAlloc_       = nullptr;
    ID3D12GraphicsCommandList*  cmdList_        = nullptr;
    ID3D12Fence*                fence_          = nullptr;
    void*                       fenceEvent_     = nullptr;  // HANDLE
    uint64_t                    fenceValue_     = 0;

    // State
    std::atomic<bool>           initialized_    = false;
    ComputeAPI                  activeAPI_      = ComputeAPI::None;
    GPUCapabilities             caps_           = {};
    mutable std::mutex          mutex_;

    // Stats
    std::atomic<uint64_t>       usedVRAM_           = 0;
    std::atomic<uint64_t>       totalDispatches_    = 0;
    std::atomic<uint64_t>       totalBytesUploaded_ = 0;
    std::atomic<uint64_t>       totalBytesDownloaded_ = 0;

    // Logging
    LogCallback logCb_;
    void log(int level, const std::string& msg);

    // Internal helpers
    GPUResult initDX12();
    GPUResult createCommandInfra();
    void      detectCapabilities();
    GPUResult resetCommandList();
    GPUResult executeAndWait();
};

// ============================================================================
// Global accessor (lazy-initialized)
// ============================================================================
GPUBackendBridge& getGPUBackendBridge();

} // namespace GPU
} // namespace RawrXD
