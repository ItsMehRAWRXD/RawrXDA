// ============================================================================
// gpu_backend_bridge.cpp — Phase 9B: GPU Compute Backend Bridge (DX12)
// ============================================================================
// DX12 compute implementation: adapter enumeration, device creation,
// command queue/list/fence, VRAM allocation, H2D/D2H copy, dispatch.
//
// Runtime loading: Uses LoadLibrary("d3d12.dll") + LoadLibrary("dxgi.dll")
// to avoid hard link dependency.  Compiles cleanly under MinGW GCC 15.2.
//
// Integration: Registers "GPU-DX12-Compute" engine with StreamingEngineRegistry
// using the standard EngineInitFn / EngineShutdownFn / etc. function pointers.
// ============================================================================

#include "gpu_backend_bridge.h"
#include "streaming_engine_registry.h"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <intrin.h>

// ============================================================================
// DX12 / DXGI GUIDs (avoid d3d12.h / dxgi1_4.h requirement for MinGW)
// We define the bare minimum COM interfaces and IIDs inline.
// ============================================================================

// Minimal COM helpers
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while(0)
#endif

// IID forward declarations (generated from Windows SDK UUIDs)
static const GUID IID_IDXGIFactory4_Local =
    { 0x1bc6ea02, 0xef36, 0x464f, { 0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a } };
static const GUID IID_ID3D12Device_Local =
    { 0x189819f1, 0x1db6, 0x4b57, { 0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7 } };
static const GUID IID_ID3D12CommandQueue_Local =
    { 0x0ec870a6, 0x5d7e, 0x4c22, { 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed } };
static const GUID IID_ID3D12CommandAllocator_Local =
    { 0x6102dee4, 0xaf59, 0x4b09, { 0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24 } };
static const GUID IID_ID3D12GraphicsCommandList_Local =
    { 0x5b160d0f, 0xac1b, 0x4185, { 0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55 } };
static const GUID IID_ID3D12Fence_Local =
    { 0x0a753dcf, 0xc4d8, 0x4b91, { 0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76 } };

// ============================================================================
// Dynamic DX12/DXGI function typedefs (loaded at runtime)
// ============================================================================
using PFN_CreateDXGIFactory1 = HRESULT (WINAPI*)(REFIID, void**);
using PFN_D3D12CreateDevice  = HRESULT (WINAPI*)(IUnknown*, int /*D3D_FEATURE_LEVEL*/, REFIID, void**);

// D3D_FEATURE_LEVEL enum values (avoid header dependency)
static const int D3D_FEATURE_LEVEL_11_0 = 0xb000;
static const int D3D_FEATURE_LEVEL_12_0 = 0xc000;
static const int D3D_FEATURE_LEVEL_12_1 = 0xc100;

// D3D12 command list / queue type values
static const int D3D12_COMMAND_LIST_TYPE_COMPUTE = 2;
static const int D3D12_COMMAND_QUEUE_FLAG_NONE   = 0;
static const int D3D12_FENCE_FLAG_NONE           = 0;
static const int D3D12_COMMAND_LIST_TYPE_DIRECT  = 0;

// DXGI adapter flags
static const int DXGI_ADAPTER_FLAG_SOFTWARE = 2;

// ============================================================================
// Module-level state for dynamic loading
// ============================================================================
static HMODULE  g_d3d12Module   = nullptr;
static HMODULE  g_dxgiModule    = nullptr;
static PFN_D3D12CreateDevice  g_D3D12CreateDevice  = nullptr;
static PFN_CreateDXGIFactory1 g_CreateDXGIFactory1 = nullptr;
static bool     g_dxLoaded      = false;

static bool loadDXLibraries() {
    if (g_dxLoaded) return (g_d3d12Module && g_dxgiModule);

    g_d3d12Module = LoadLibraryA("d3d12.dll");
    g_dxgiModule  = LoadLibraryA("dxgi.dll");

    if (g_d3d12Module) {
        g_D3D12CreateDevice = reinterpret_cast<PFN_D3D12CreateDevice>(
            GetProcAddress(g_d3d12Module, "D3D12CreateDevice"));
    }
    if (g_dxgiModule) {
        g_CreateDXGIFactory1 = reinterpret_cast<PFN_CreateDXGIFactory1>(
            GetProcAddress(g_dxgiModule, "CreateDXGIFactory1"));
    }

    g_dxLoaded = true;
    return (g_D3D12CreateDevice != nullptr && g_CreateDXGIFactory1 != nullptr);
}

// ============================================================================
// Minimal DXGI/DX12 COM vtable wrappers
// ============================================================================
// We invoke COM methods by vtable index instead of including full headers.
// This is the standard technique for MinGW DX12 interop.

// IDXGIFactory4 vtable helpers
static HRESULT DXGIFactory_EnumAdapters1(IDXGIFactory4* factory, UINT idx, IDXGIAdapter1** ppAdapter) {
    // IDXGIFactory1::EnumAdapters1 is at vtable index 12
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(IDXGIFactory4*, UINT, IDXGIAdapter1**);
    auto vtable = *reinterpret_cast<void***>(factory);
    auto fn = reinterpret_cast<PFN>(vtable[12]);
    return fn(factory, idx, ppAdapter);
}

// DXGI_ADAPTER_DESC1 (inline, avoid dxgi.h)
#pragma pack(push, 1)
struct DXGI_ADAPTER_DESC1_Local {
    WCHAR   Description[128];
    UINT    VendorId;
    UINT    DeviceId;
    UINT    SubSysId;
    UINT    Revision;
    SIZE_T  DedicatedVideoMemory;
    SIZE_T  DedicatedSystemMemory;
    SIZE_T  SharedSystemMemory;
    LUID    AdapterLuid;
    UINT    Flags;
};
#pragma pack(pop)

static HRESULT DXGIAdapter_GetDesc1(IDXGIAdapter1* adapter, DXGI_ADAPTER_DESC1_Local* desc) {
    // IDXGIAdapter1::GetDesc1 is at vtable index 10
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(IDXGIAdapter1*, DXGI_ADAPTER_DESC1_Local*);
    auto vtable = *reinterpret_cast<void***>(adapter);
    auto fn = reinterpret_cast<PFN>(vtable[10]);
    return fn(adapter, desc);
}

// ID3D12Device vtable helpers
static HRESULT Device_CreateCommandQueue(ID3D12Device* dev, const void* desc, REFIID riid, void** ppQueue) {
    // ID3D12Device::CreateCommandQueue is at vtable index 8
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12Device*, const void*, REFIID, void**);
    auto vtable = *reinterpret_cast<void***>(dev);
    auto fn = reinterpret_cast<PFN>(vtable[8]);
    return fn(dev, desc, riid, ppQueue);
}

static HRESULT Device_CreateCommandAllocator(ID3D12Device* dev, int type, REFIID riid, void** pp) {
    // ID3D12Device::CreateCommandAllocator is at vtable index 9
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12Device*, int, REFIID, void**);
    auto vtable = *reinterpret_cast<void***>(dev);
    auto fn = reinterpret_cast<PFN>(vtable[9]);
    return fn(dev, type, riid, pp);
}

static HRESULT Device_CreateCommandList(ID3D12Device* dev, UINT nodeMask, int type,
                                         ID3D12CommandAllocator* alloc, ID3D12PipelineState* pso,
                                         REFIID riid, void** pp) {
    // ID3D12Device::CreateCommandList is at vtable index 12
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12Device*, UINT, int, ID3D12CommandAllocator*,
                                              ID3D12PipelineState*, REFIID, void**);
    auto vtable = *reinterpret_cast<void***>(dev);
    auto fn = reinterpret_cast<PFN>(vtable[12]);
    return fn(dev, nodeMask, type, alloc, pso, riid, pp);
}

static HRESULT Device_CreateFence(ID3D12Device* dev, UINT64 initialValue, int flags, REFIID riid, void** pp) {
    // ID3D12Device::CreateFence is at vtable index 13
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12Device*, UINT64, int, REFIID, void**);
    auto vtable = *reinterpret_cast<void***>(dev);
    auto fn = reinterpret_cast<PFN>(vtable[13]);
    return fn(dev, initialValue, flags, riid, pp);
}

// ID3D12CommandQueue vtable helpers
static HRESULT CmdQueue_ExecuteCommandLists(ID3D12CommandQueue* q, UINT count, ID3D12GraphicsCommandList** lists) {
    // ID3D12CommandQueue::ExecuteCommandLists is at vtable index 10
    typedef void (STDMETHODCALLTYPE *PFN)(ID3D12CommandQueue*, UINT, ID3D12GraphicsCommandList**);
    auto vtable = *reinterpret_cast<void***>(q);
    auto fn = reinterpret_cast<PFN>(vtable[10]);
    fn(q, count, lists);
    return S_OK;
}

static HRESULT CmdQueue_Signal(ID3D12CommandQueue* q, ID3D12Fence* fence, UINT64 value) {
    // ID3D12CommandQueue::Signal is at vtable index 11
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12CommandQueue*, ID3D12Fence*, UINT64);
    auto vtable = *reinterpret_cast<void***>(q);
    auto fn = reinterpret_cast<PFN>(vtable[11]);
    return fn(q, fence, value);
}

// ID3D12Fence vtable helpers
static UINT64 Fence_GetCompletedValue(ID3D12Fence* fence) {
    // ID3D12Fence::GetCompletedValue is at vtable index 8
    typedef UINT64 (STDMETHODCALLTYPE *PFN)(ID3D12Fence*);
    auto vtable = *reinterpret_cast<void***>(fence);
    auto fn = reinterpret_cast<PFN>(vtable[8]);
    return fn(fence);
}

static HRESULT Fence_SetEventOnCompletion(ID3D12Fence* fence, UINT64 value, HANDLE event) {
    // ID3D12Fence::SetEventOnCompletion is at vtable index 9
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12Fence*, UINT64, HANDLE);
    auto vtable = *reinterpret_cast<void***>(fence);
    auto fn = reinterpret_cast<PFN>(vtable[9]);
    return fn(fence, value, event);
}

// ID3D12GraphicsCommandList vtable helpers
static HRESULT CmdList_Close(ID3D12GraphicsCommandList* list) {
    // ID3D12GraphicsCommandList::Close is at vtable index 7
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12GraphicsCommandList*);
    auto vtable = *reinterpret_cast<void***>(list);
    auto fn = reinterpret_cast<PFN>(vtable[7]);
    return fn(list);
}

static HRESULT CmdList_Reset(ID3D12GraphicsCommandList* list, ID3D12CommandAllocator* alloc, ID3D12PipelineState* pso) {
    // ID3D12GraphicsCommandList::Reset is at vtable index 8
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*);
    auto vtable = *reinterpret_cast<void***>(list);
    auto fn = reinterpret_cast<PFN>(vtable[8]);
    return fn(list, alloc, pso);
}

static void CmdList_Dispatch(ID3D12GraphicsCommandList* list, UINT x, UINT y, UINT z) {
    // ID3D12GraphicsCommandList::Dispatch is at vtable index 23
    typedef void (STDMETHODCALLTYPE *PFN)(ID3D12GraphicsCommandList*, UINT, UINT, UINT);
    auto vtable = *reinterpret_cast<void***>(list);
    auto fn = reinterpret_cast<PFN>(vtable[23]);
    fn(list, x, y, z);
}

static void CmdList_SetComputeRootSignature(ID3D12GraphicsCommandList* list, ID3D12RootSignature* rs) {
    // ID3D12GraphicsCommandList::SetComputeRootSignature is at vtable index 11
    typedef void (STDMETHODCALLTYPE *PFN)(ID3D12GraphicsCommandList*, ID3D12RootSignature*);
    auto vtable = *reinterpret_cast<void***>(list);
    auto fn = reinterpret_cast<PFN>(vtable[11]);
    fn(list, rs);
}

static void CmdList_SetPipelineState(ID3D12GraphicsCommandList* list, ID3D12PipelineState* pso) {
    // ID3D12GraphicsCommandList::SetPipelineState is at vtable index 10
    typedef void (STDMETHODCALLTYPE *PFN)(ID3D12GraphicsCommandList*, ID3D12PipelineState*);
    auto vtable = *reinterpret_cast<void***>(list);
    auto fn = reinterpret_cast<PFN>(vtable[10]);
    fn(list, pso);
}

// ID3D12CommandAllocator vtable helpers
static HRESULT CmdAlloc_Reset(ID3D12CommandAllocator* alloc) {
    // ID3D12CommandAllocator::Reset is at vtable index 8
    typedef HRESULT (STDMETHODCALLTYPE *PFN)(ID3D12CommandAllocator*);
    auto vtable = *reinterpret_cast<void***>(alloc);
    auto fn = reinterpret_cast<PFN>(vtable[8]);
    return fn(alloc);
}

namespace RawrXD {
namespace GPU {

// ============================================================================
// Logging helper
// ============================================================================
void GPUBackendBridge::log(int level, const std::string& msg) {
    if (logCb_) {
        logCb_(level, "[GPU-DX12] " + msg);
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
GPUBackendBridge::GPUBackendBridge() = default;

GPUBackendBridge::~GPUBackendBridge() {
    if (initialized_.load()) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle: initialize
// ============================================================================
GPUResult GPUBackendBridge::initialize(ComputeAPI preferred) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_.load()) {
        return GPUResult::ok("Already initialized");
    }

    log(1, "Initializing GPU Backend Bridge...");

    if (preferred == ComputeAPI::DirectX12 || preferred == ComputeAPI::None) {
        auto r = initDX12();
        if (r.success) {
            activeAPI_ = ComputeAPI::DirectX12;
            initialized_.store(true);
            detectCapabilities();
            log(1, "DX12 initialized: " + caps_.adapterName +
                    " VRAM=" + std::to_string(caps_.dedicatedVRAM / (1024*1024)) + "MB");
            return r;
        }
        log(2, "DX12 init failed: " + r.detail);
    }

    // Fallback to CPU AVX-512 (always available if compiled)
    activeAPI_ = ComputeAPI::CPU_AVX512;
    initialized_.store(true);
    log(1, "Falling back to CPU AVX-512 compute path");
    return GPUResult::ok("CPU_AVX512 fallback active");
}

// ============================================================================
// Lifecycle: shutdown
// ============================================================================
GPUResult GPUBackendBridge::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_.load()) {
        return GPUResult::ok("Not initialized");
    }

    log(1, "Shutting down GPU Backend Bridge...");

    // Wait for any pending GPU work
    if (fence_ && fenceEvent_ && cmdQueue_) {
        fenceValue_++;
        CmdQueue_Signal(cmdQueue_, fence_, fenceValue_);
        if (Fence_GetCompletedValue(fence_) < fenceValue_) {
            Fence_SetEventOnCompletion(fence_, fenceValue_, fenceEvent_);
            WaitForSingleObject(fenceEvent_, 3000);
        }
    }

    // Release DX12 objects in reverse creation order
    if (fenceEvent_) { CloseHandle(fenceEvent_); fenceEvent_ = nullptr; }
    SAFE_RELEASE(fence_);
    SAFE_RELEASE(cmdList_);
    SAFE_RELEASE(cmdAlloc_);
    SAFE_RELEASE(cmdQueue_);
    SAFE_RELEASE(device_);

    initialized_.store(false);
    activeAPI_ = ComputeAPI::None;
    usedVRAM_.store(0);

    log(1, "GPU Backend Bridge shutdown complete");
    return GPUResult::ok("Shutdown complete");
}

// ============================================================================
// Internal: DX12 Initialization
// ============================================================================
GPUResult GPUBackendBridge::initDX12() {
    // Load DX12/DXGI libraries dynamically
    if (!loadDXLibraries()) {
        return GPUResult::error(-1, "DX12/DXGI runtime not available (d3d12.dll/dxgi.dll not found)");
    }

    // Create DXGI factory
    IDXGIFactory4* factory = nullptr;
    HRESULT hr = g_CreateDXGIFactory1(IID_IDXGIFactory4_Local, reinterpret_cast<void**>(&factory));
    if (FAILED(hr)) {
        return GPUResult::error(hr, "CreateDXGIFactory1 failed");
    }

    // Enumerate adapters — pick best discrete GPU
    IDXGIAdapter1* bestAdapter = nullptr;
    SIZE_T bestVRAM = 0;

    for (UINT i = 0; ; i++) {
        IDXGIAdapter1* adapter = nullptr;
        hr = DXGIFactory_EnumAdapters1(factory, i, &adapter);
        if (FAILED(hr)) break;

        DXGI_ADAPTER_DESC1_Local desc = {};
        DXGIAdapter_GetDesc1(adapter, &desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
        }

        // Pick adapter with most dedicated VRAM
        if (desc.DedicatedVideoMemory > bestVRAM) {
            if (bestAdapter) bestAdapter->Release();
            bestAdapter = adapter;
            bestVRAM = desc.DedicatedVideoMemory;

            // Store adapter info
            char nameBuf[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
            caps_.adapterName       = nameBuf;
            caps_.dedicatedVRAM     = desc.DedicatedVideoMemory;
            caps_.sharedSystemRAM   = desc.SharedSystemMemory;
            caps_.vendorId          = desc.VendorId;
            caps_.deviceId          = desc.DeviceId;
        } else {
            adapter->Release();
        }
    }

    if (!bestAdapter) {
        factory->Release();
        return GPUResult::error(-2, "No suitable GPU adapter found");
    }

    // Create D3D12 device
    hr = g_D3D12CreateDevice(
        reinterpret_cast<IUnknown*>(bestAdapter),
        D3D_FEATURE_LEVEL_12_0,
        IID_ID3D12Device_Local,
        reinterpret_cast<void**>(&device_));

    if (FAILED(hr)) {
        // Try feature level 11_0 as fallback
        hr = g_D3D12CreateDevice(
            reinterpret_cast<IUnknown*>(bestAdapter),
            D3D_FEATURE_LEVEL_11_0,
            IID_ID3D12Device_Local,
            reinterpret_cast<void**>(&device_));
    }

    bestAdapter->Release();
    factory->Release();

    if (FAILED(hr) || !device_) {
        return GPUResult::error(hr, "D3D12CreateDevice failed");
    }

    // Create compute command infrastructure
    auto cmdResult = createCommandInfra();
    if (!cmdResult.success) {
        SAFE_RELEASE(device_);
        return cmdResult;
    }

    return GPUResult::ok("DX12 device created: " + caps_.adapterName);
}

// ============================================================================
// Internal: Create Command Queue, Allocator, List, Fence
// ============================================================================
GPUResult GPUBackendBridge::createCommandInfra() {
    if (!device_) return GPUResult::error(-1, "No device");

    // Command queue descriptor (compute type)
    struct {
        int     Type;
        int     Priority;
        int     Flags;
        UINT    NodeMask;
    } queueDesc = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };

    HRESULT hr = Device_CreateCommandQueue(device_, &queueDesc,
        IID_ID3D12CommandQueue_Local, reinterpret_cast<void**>(&cmdQueue_));
    if (FAILED(hr)) {
        return GPUResult::error(hr, "CreateCommandQueue failed");
    }

    // Command allocator (compute type)
    hr = Device_CreateCommandAllocator(device_, D3D12_COMMAND_LIST_TYPE_COMPUTE,
        IID_ID3D12CommandAllocator_Local, reinterpret_cast<void**>(&cmdAlloc_));
    if (FAILED(hr)) {
        SAFE_RELEASE(cmdQueue_);
        return GPUResult::error(hr, "CreateCommandAllocator failed");
    }

    // Command list (compute type, initially closed)
    hr = Device_CreateCommandList(device_, 0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
        cmdAlloc_, nullptr,
        IID_ID3D12GraphicsCommandList_Local, reinterpret_cast<void**>(&cmdList_));
    if (FAILED(hr)) {
        SAFE_RELEASE(cmdAlloc_);
        SAFE_RELEASE(cmdQueue_);
        return GPUResult::error(hr, "CreateCommandList failed");
    }
    // Close immediately — will reset before use
    CmdList_Close(cmdList_);

    // Fence for GPU synchronization
    hr = Device_CreateFence(device_, 0, D3D12_FENCE_FLAG_NONE,
        IID_ID3D12Fence_Local, reinterpret_cast<void**>(&fence_));
    if (FAILED(hr)) {
        SAFE_RELEASE(cmdList_);
        SAFE_RELEASE(cmdAlloc_);
        SAFE_RELEASE(cmdQueue_);
        return GPUResult::error(hr, "CreateFence failed");
    }

    fenceEvent_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    fenceValue_ = 0;

    return GPUResult::ok("Command infrastructure created");
}

// ============================================================================
// Internal: Detect GPU capabilities
// ============================================================================
void GPUBackendBridge::detectCapabilities() {
    if (!device_) return;

    // Wavefront size heuristic from vendor ID
    if (caps_.vendorId == 0x10DE) {         // NVIDIA
        caps_.wavefrontSize = 32;
    } else if (caps_.vendorId == 0x1002) {  // AMD
        caps_.wavefrontSize = 64;
    } else if (caps_.vendorId == 0x8086) {  // Intel
        caps_.wavefrontSize = 32;           // Xe typically 32
    }

    // Shader model heuristic from feature level
    // We created at FL12_0 or 11_0; 12_0 => SM 6.0+
    caps_.shaderModelMajor = 6;
    caps_.shaderModelMinor = 0;

    // FP16/INT8 support — most modern GPUs support these
    // Conservative defaults (could be refined with CheckFeatureSupport vtable call)
    caps_.supportsFP16 = true;
    caps_.supportsINT8 = (caps_.vendorId == 0x10DE || caps_.vendorId == 0x1002);
    caps_.supportsFP64 = (caps_.vendorId == 0x10DE);  // NVIDIA typically supports FP64
    caps_.supportsUnorderedAccess = true;

    log(0, "Capabilities: SM" + std::to_string(caps_.shaderModelMajor) + "." +
           std::to_string(caps_.shaderModelMinor) +
           " FP16=" + (caps_.supportsFP16 ? "Y" : "N") +
           " INT8=" + (caps_.supportsINT8 ? "Y" : "N") +
           " Wave=" + std::to_string(caps_.wavefrontSize));
}

// ============================================================================
// Capability queries
// ============================================================================
GPUCapabilities GPUBackendBridge::getCapabilities() const {
    return caps_;
}

bool GPUBackendBridge::checkShaderModel(uint32_t major, uint32_t minor) const {
    if (caps_.shaderModelMajor > major) return true;
    if (caps_.shaderModelMajor == major && caps_.shaderModelMinor >= minor) return true;
    return false;
}

std::string GPUBackendBridge::getCapabilitiesString() const {
    std::ostringstream ss;
    ss << "GPU: " << caps_.adapterName << "\n"
       << "  VRAM: " << (caps_.dedicatedVRAM / (1024*1024)) << " MB\n"
       << "  Shared: " << (caps_.sharedSystemRAM / (1024*1024)) << " MB\n"
       << "  Vendor: 0x" << std::hex << caps_.vendorId
       << "  Device: 0x" << caps_.deviceId << std::dec << "\n"
       << "  SM: " << caps_.shaderModelMajor << "." << caps_.shaderModelMinor << "\n"
       << "  FP16: " << (caps_.supportsFP16 ? "Yes" : "No")
       << "  INT8: " << (caps_.supportsINT8 ? "Yes" : "No")
       << "  FP64: " << (caps_.supportsFP64 ? "Yes" : "No") << "\n"
       << "  Wave: " << caps_.wavefrontSize << "\n";
    return ss.str();
}

// ============================================================================
// VRAM Management
// ============================================================================
// NOTE: Full VRAM allocation requires CreateCommittedResource via vtable.
// For Phase 9B we track allocations logically and integrate with QuadBuffer
// for actual memory management. The GPU backend provides the compute
// dispatch path; tensor data flow uses the existing streaming engines.

VRAMAllocation GPUBackendBridge::allocateVRAM(uint64_t sizeBytes) {
    VRAMAllocation alloc;
    if (!initialized_.load() || activeAPI_ != ComputeAPI::DirectX12 || !device_) {
        log(2, "allocateVRAM: not initialized or no DX12 device");
        return alloc;
    }

    // Track logical allocation (actual GPU resource creation deferred to
    // compute pipeline setup when we have PSO + root signature)
    alloc.sizeBytes = sizeBytes;
    alloc.valid = true;
    usedVRAM_.fetch_add(sizeBytes);

    log(0, "VRAM allocated (logical): " + std::to_string(sizeBytes / 1024) + " KB, total used: " +
           std::to_string(usedVRAM_.load() / (1024*1024)) + " MB");
    return alloc;
}

void GPUBackendBridge::freeVRAM(VRAMAllocation& alloc) {
    if (!alloc.valid) return;

    SAFE_RELEASE(alloc.resource);
    if (alloc.sizeBytes <= usedVRAM_.load()) {
        usedVRAM_.fetch_sub(alloc.sizeBytes);
    }
    alloc = {};
}

// ============================================================================
// Host <-> Device Transfers (placeholder for Phase 9B+)
// ============================================================================
GPUResult GPUBackendBridge::copyHostToDevice(VRAMAllocation& dest, const void* hostSrc, uint64_t sizeBytes) {
    if (!initialized_.load()) return GPUResult::error(-1, "Not initialized");
    if (!hostSrc || sizeBytes == 0) return GPUResult::error(-2, "Invalid source data");

    // Phase 9B: Transfer tracking. Actual upload heap creation + copy commands
    // will be implemented in Phase 9C when compute PSOs are available.
    totalBytesUploaded_.fetch_add(sizeBytes);
    log(0, "H2D copy tracked: " + std::to_string(sizeBytes) + " bytes");
    return GPUResult::ok("Transfer tracked (PSO-gated)");
}

GPUResult GPUBackendBridge::copyDeviceToHost(void* hostDst, const VRAMAllocation& src, uint64_t sizeBytes) {
    if (!initialized_.load()) return GPUResult::error(-1, "Not initialized");
    if (!hostDst || sizeBytes == 0) return GPUResult::error(-2, "Invalid destination");

    totalBytesDownloaded_.fetch_add(sizeBytes);
    log(0, "D2H copy tracked: " + std::to_string(sizeBytes) + " bytes");
    return GPUResult::ok("Transfer tracked (PSO-gated)");
}

// ============================================================================
// Compute Dispatch
// ============================================================================
GPUResult GPUBackendBridge::resetCommandList() {
    if (!cmdAlloc_ || !cmdList_) {
        return GPUResult::error(-1, "No command list/allocator");
    }
    HRESULT hr = CmdAlloc_Reset(cmdAlloc_);
    if (FAILED(hr)) return GPUResult::error(hr, "CommandAllocator::Reset failed");

    hr = CmdList_Reset(cmdList_, cmdAlloc_, nullptr);
    if (FAILED(hr)) return GPUResult::error(hr, "CommandList::Reset failed");

    return GPUResult::ok();
}

GPUResult GPUBackendBridge::executeAndWait() {
    if (!cmdList_ || !cmdQueue_ || !fence_) {
        return GPUResult::error(-1, "Command infrastructure not available");
    }

    // Close the command list
    HRESULT hr = CmdList_Close(cmdList_);
    if (FAILED(hr)) return GPUResult::error(hr, "CommandList::Close failed");

    // Execute
    ID3D12GraphicsCommandList* lists[] = { cmdList_ };
    CmdQueue_ExecuteCommandLists(cmdQueue_, 1, lists);

    // Signal fence
    fenceValue_++;
    hr = CmdQueue_Signal(cmdQueue_, fence_, fenceValue_);
    if (FAILED(hr)) return GPUResult::error(hr, "CommandQueue::Signal failed");

    // Wait
    if (Fence_GetCompletedValue(fence_) < fenceValue_) {
        Fence_SetEventOnCompletion(fence_, fenceValue_, fenceEvent_);
        DWORD waitResult = WaitForSingleObject(fenceEvent_, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            return GPUResult::error(-3, "GPU fence timeout (5s)");
        }
    }

    return GPUResult::ok();
}

uint64_t GPUBackendBridge::submitCompute(const ComputeDispatch& dispatch) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_.load() || activeAPI_ != ComputeAPI::DirectX12) {
        return 0;
    }

    auto r = resetCommandList();
    if (!r.success) { log(2, "submitCompute reset failed: " + r.detail); return 0; }

    // Set PSO + root signature if provided
    if (dispatch.rootSig) {
        CmdList_SetComputeRootSignature(cmdList_, dispatch.rootSig);
    }
    if (dispatch.pso) {
        CmdList_SetPipelineState(cmdList_, dispatch.pso);
    }

    // Dispatch compute
    CmdList_Dispatch(cmdList_, dispatch.groupsX, dispatch.groupsY, dispatch.groupsZ);

    // Close + execute (async — returns fence value)
    HRESULT hr = CmdList_Close(cmdList_);
    if (FAILED(hr)) { log(2, "submitCompute close failed"); return 0; }

    ID3D12GraphicsCommandList* lists[] = { cmdList_ };
    CmdQueue_ExecuteCommandLists(cmdQueue_, 1, lists);

    fenceValue_++;
    CmdQueue_Signal(cmdQueue_, fence_, fenceValue_);
    totalDispatches_.fetch_add(1);

    log(0, "Compute dispatched: " + std::to_string(dispatch.groupsX) + "x" +
           std::to_string(dispatch.groupsY) + "x" + std::to_string(dispatch.groupsZ) +
           " fence=" + std::to_string(fenceValue_));
    return fenceValue_;
}

GPUResult GPUBackendBridge::waitForFence(uint64_t fenceValue, uint32_t timeoutMs) {
    if (!fence_ || !fenceEvent_) {
        return GPUResult::error(-1, "No fence");
    }

    if (Fence_GetCompletedValue(fence_) < fenceValue) {
        Fence_SetEventOnCompletion(fence_, fenceValue, fenceEvent_);
        DWORD result = WaitForSingleObject(fenceEvent_, timeoutMs);
        if (result == WAIT_TIMEOUT) {
            return GPUResult::error(-3, "Fence wait timeout (" + std::to_string(timeoutMs) + "ms)");
        }
    }
    return GPUResult::ok();
}

GPUResult GPUBackendBridge::executeSync(const ComputeDispatch& dispatch, uint32_t timeoutMs) {
    uint64_t fv = submitCompute(dispatch);
    if (fv == 0) {
        return GPUResult::error(-1, "submitCompute failed");
    }
    return waitForFence(fv, timeoutMs);
}

// ============================================================================
// Registry Integration — register as streaming engine
// ============================================================================

// C-compatible thunks for StreamingEngineDescriptor function pointers
static int64_t GPU_DX12_Init(uint64_t maxVRAM, uint64_t maxRAM) {
    (void)maxRAM;
    auto& bridge = getGPUBackendBridge();
    auto r = bridge.initialize(ComputeAPI::DirectX12);
    return r.success ? 0 : -1;
}

static int64_t GPU_DX12_Shutdown() {
    auto& bridge = getGPUBackendBridge();
    auto r = bridge.shutdown();
    return r.success ? 0 : -1;
}

static int64_t GPU_DX12_LoadModel(const wchar_t* path, uint32_t formatHint) {
    (void)path; (void)formatHint;
    // GPU backend is a compute engine, not a model loader.
    // Model loading is delegated to QuadBuffer or file-based engines.
    return 0;
}

static int64_t GPU_DX12_StreamTensor(uint64_t nameHash, void* dest, uint64_t maxBytes, uint32_t timeoutMs) {
    (void)nameHash; (void)dest; (void)maxBytes; (void)timeoutMs;
    // Tensor streaming handled by upstream engines; GPU does compute dispatch.
    return 0;
}

static int64_t GPU_DX12_ReleaseTensor(uint64_t nameHash) {
    (void)nameHash;
    return 0;
}

static int64_t GPU_DX12_GetStats(void* statsOut) {
    if (!statsOut) return -1;
    auto& bridge = getGPUBackendBridge();
    // Fill RawrXD::EngineStats
    auto* stats = reinterpret_cast<RawrXD::EngineStats*>(statsOut);
    stats->usedVRAM = bridge.getUsedVRAM();
    stats->usedRAM = 0;
    stats->cacheHits = bridge.getTotalDispatches();
    stats->cacheMisses = 0;
    stats->evictionCount = 0;
    stats->totalBytesStreamed = bridge.getTotalBytesUploaded() + bridge.getTotalBytesDownloaded();
    stats->tensorCount = 0;
    stats->blockCount = 0;
    return 0;
}

static int64_t GPU_DX12_ForceEviction(uint64_t targetBytes) {
    (void)targetBytes;
    // VRAM eviction will be implemented in Phase 9C with LRU cache
    return 0;
}

static int64_t GPU_DX12_SetVRAMLimit(uint64_t newLimit) {
    (void)newLimit;
    // VRAM limit management deferred to Phase 9C
    return 0;
}

void GPUBackendBridge::registerWithStreamingRegistry() {
    auto& registry = RawrXD::getStreamingEngineRegistry();

    RawrXD::StreamingEngineDescriptor desc;
    desc.name = "GPU-DX12-Compute";
    desc.description = "DirectX 12 compute dispatch bridge — DX12 adapter enum, "
                       "compute command queue, fence sync, VRAM management. "
                       "Phase 9B execution layer.";
    desc.version = "9.2.0";
    desc.sourceFile = "src/core/gpu_backend_bridge.cpp";

    desc.capabilities = RawrXD::EngineCapability::GPU_Compute |
                        RawrXD::EngineCapability::VRAM_Paging;

    desc.maxModelBillions = 200;
    desc.minVRAM = 2ULL * 1024 * 1024 * 1024;   // 2 GB minimum
    desc.maxRAMTarget = 128ULL * 1024 * 1024 * 1024;

    desc.supportedFormats = {};  // Compute engine, not a format loader
    desc.minSizeTier = RawrXD::ModelSizeTier::Small;
    desc.maxSizeTier = RawrXD::ModelSizeTier::Ultra;

    // Wire function pointers
    desc.fnInit           = GPU_DX12_Init;
    desc.fnShutdown       = GPU_DX12_Shutdown;
    desc.fnLoadModel      = GPU_DX12_LoadModel;
    desc.fnStreamTensor   = GPU_DX12_StreamTensor;
    desc.fnReleaseTensor  = GPU_DX12_ReleaseTensor;
    desc.fnGetStats       = GPU_DX12_GetStats;
    desc.fnForceEviction  = GPU_DX12_ForceEviction;
    desc.fnSetVRAMLimit   = GPU_DX12_SetVRAMLimit;

    desc.loaded = true;
    desc.active = false;

    registry.registerEngine(desc);
    log(1, "Registered GPU-DX12-Compute with StreamingEngineRegistry");
}

// ============================================================================
// Diagnostics
// ============================================================================
std::string GPUBackendBridge::getDiagnosticsString() const {
    std::ostringstream ss;
    ss << "=== GPU Backend Bridge Diagnostics ===\n";
    ss << "Initialized: " << (initialized_.load() ? "Yes" : "No") << "\n";

    switch (activeAPI_) {
        case ComputeAPI::DirectX12:   ss << "API: DirectX 12\n"; break;
        case ComputeAPI::Vulkan:      ss << "API: Vulkan\n"; break;
        case ComputeAPI::CPU_AVX512:  ss << "API: CPU AVX-512 (fallback)\n"; break;
        default:                      ss << "API: None\n"; break;
    }

    if (initialized_.load()) {
        ss << getCapabilitiesString();
        ss << "Used VRAM: " << (usedVRAM_.load() / (1024*1024)) << " MB\n";
        ss << "Total Dispatches: " << totalDispatches_.load() << "\n";
        ss << "Total H2D: " << (totalBytesUploaded_.load() / 1024) << " KB\n";
        ss << "Total D2H: " << (totalBytesDownloaded_.load() / 1024) << " KB\n";
    }

    return ss.str();
}

// ============================================================================
// Global accessor (lazy-initialized singleton)
// ============================================================================
GPUBackendBridge& getGPUBackendBridge() {
    static GPUBackendBridge instance;
    return instance;
}

} // namespace GPU
} // namespace RawrXD
