#include "MemoryPressureGuard.h"
#include <windows.h>
#include <sstream>
#include <cstdio>
#include <cstring>

using namespace RawrXD::Inference;

// ---------------------------------------------------------------------------
// Static resource-tracking state
// ---------------------------------------------------------------------------
std::mutex            MemoryPressureGuard::s_mu;
std::atomic<uint64_t> MemoryPressureGuard::s_committedRAM{0};
std::atomic<uint64_t> MemoryPressureGuard::s_committedVRAM{0};

// ---------------------------------------------------------------------------
// DXGI VRAM query — raw vtable COM, no dxgi.h dependency
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct DXGI_ADAPTER_DESC1_MPG {
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

static bool queryDXGI_VRAM(uint64_t& outTotal, uint64_t& outAvailable) {
    outTotal = 0;
    outAvailable = 0;

    HMODULE hDXGI = LoadLibraryA("dxgi.dll");
    if (!hDXGI) return false;

    typedef HRESULT(WINAPI* PFN_CreateDXGIFactory1)(const IID&, void**);
    auto fnCreate = (PFN_CreateDXGIFactory1)GetProcAddress(hDXGI, "CreateDXGIFactory1");
    if (!fnCreate) { FreeLibrary(hDXGI); return false; }

    // IID_IDXGIFactory1: {770aae78-f26f-4dba-a829-253c83d1b387}
    static const IID IID_Factory1 = {
        0x770aae78, 0xf26f, 0x4dba,
        { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 }
    };

    void* pFactory = nullptr;
    HRESULT hr = fnCreate(IID_Factory1, &pFactory);
    if (FAILED(hr) || !pFactory) { FreeLibrary(hDXGI); return false; }

    // IDXGIFactory1::EnumAdapters1 is vtable[12]
    auto factoryVtbl = *reinterpret_cast<void***>(pFactory);
    typedef HRESULT(STDMETHODCALLTYPE* PFN_EnumAdapters1)(void*, UINT, void**);
    auto fnEnum = reinterpret_cast<PFN_EnumAdapters1>(factoryVtbl[12]);

    void* pAdapter = nullptr;
    hr = fnEnum(pFactory, 0, &pAdapter);
    if (SUCCEEDED(hr) && pAdapter) {
        // IDXGIAdapter1::GetDesc1 is vtable[10]
        auto adapterVtbl = *reinterpret_cast<void***>(pAdapter);
        typedef HRESULT(STDMETHODCALLTYPE* PFN_GetDesc1)(void*, DXGI_ADAPTER_DESC1_MPG*);
        auto fnDesc = reinterpret_cast<PFN_GetDesc1>(adapterVtbl[10]);

        DXGI_ADAPTER_DESC1_MPG desc;
        memset(&desc, 0, sizeof(desc));
        hr = fnDesc(pAdapter, &desc);
        if (SUCCEEDED(hr)) {
            outTotal = desc.DedicatedVideoMemory;
            // Estimate available: total minus what we've already committed
            uint64_t committed = MemoryPressureGuard::committedVRAM();
            outAvailable = (outTotal > committed) ? (outTotal - committed) : 0;
        }

        // Release adapter: IUnknown::Release is vtable[2]
        typedef ULONG(STDMETHODCALLTYPE* PFN_Release)(void*);
        auto fnRelease = reinterpret_cast<PFN_Release>(adapterVtbl[2]);
        fnRelease(pAdapter);
    }

    // Release factory
    auto factoryRelease = reinterpret_cast<ULONG(STDMETHODCALLTYPE*)(void*)>(factoryVtbl[2]);
    factoryRelease(pFactory);

    FreeLibrary(hDXGI);
    return outTotal > 0;
}

// ---------------------------------------------------------------------------
// query_system — real RAM + DXGI VRAM
// ---------------------------------------------------------------------------
MemoryPressureGuard::SystemMemory MemoryPressureGuard::query_system() {
    SystemMemory mem{};
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        mem.total_ram = statex.ullTotalPhys;
        mem.available_ram = statex.ullAvailPhys;
    }
    
    queryDXGI_VRAM(mem.total_vram, mem.available_vram);
    
    return mem;
}

// ---------------------------------------------------------------------------
// check_load — decides Allow / Warn / Block with margin
// ---------------------------------------------------------------------------
MemoryPressureGuard::Verdict MemoryPressureGuard::check_load(
    const LoadRequest& req, std::string& out_message)
{
    auto sys = query_system();
    uint64_t padded_req = static_cast<uint64_t>(req.required_bytes * req.safety_margin);
    
    if (req.requires_gpu && sys.total_vram > 0) {
        uint64_t committed = s_committedVRAM.load(std::memory_order_relaxed);
        uint64_t effective = (sys.total_vram > committed)
                           ? (sys.total_vram - committed) : 0;
        if (padded_req > effective) {
            std::stringstream ss;
            ss << "Insufficient VRAM: Need " << (padded_req >> 20)
               << "MB, Available: " << (effective >> 20)
               << "MB (total: " << (sys.total_vram >> 20)
               << "MB, committed: " << (committed >> 20) << "MB)";
            out_message = ss.str();
            return Verdict::Block;
        }
        // Warn if less than 15% headroom
        if (padded_req * 100 / effective > 85) {
            std::stringstream ss;
            ss << "VRAM tight: " << (padded_req >> 20)
               << "MB of " << (effective >> 20) << "MB available";
            out_message = ss.str();
            return Verdict::Warn;
        }
    } else {
        uint64_t committed = s_committedRAM.load(std::memory_order_relaxed);
        uint64_t effective = (sys.available_ram > committed)
                           ? (sys.available_ram - committed) : 0;
        if (padded_req > effective) {
            std::stringstream ss;
            ss << "Insufficient RAM: Need " << (padded_req >> 20)
               << "MB, Available: " << (effective >> 20)
               << "MB (committed: " << (committed >> 20) << "MB)";
            out_message = ss.str();
            return Verdict::Block;
        }
        if (padded_req * 100 / (effective ? effective : 1) > 85) {
            std::stringstream ss;
            ss << "RAM tight: " << (padded_req >> 20)
               << "MB of " << (effective >> 20) << "MB available";
            out_message = ss.str();
            return Verdict::Warn;
        }
    }
    
    return Verdict::Allow;
}

// ---------------------------------------------------------------------------
// acquire / release — atomic commitment tracking
// ---------------------------------------------------------------------------
bool MemoryPressureGuard::acquire_resources(uint64_t bytes, bool gpu) {
    auto sys = query_system();
    std::lock_guard<std::mutex> lk(s_mu);
    
    if (gpu) {
        uint64_t committed = s_committedVRAM.load(std::memory_order_relaxed);
        if (sys.total_vram > 0 && committed + bytes > sys.total_vram)
            return false;
        s_committedVRAM.fetch_add(bytes, std::memory_order_relaxed);
    } else {
        uint64_t committed = s_committedRAM.load(std::memory_order_relaxed);
        if (committed + bytes > sys.available_ram)
            return false;
        s_committedRAM.fetch_add(bytes, std::memory_order_relaxed);
    }
    return true;
}

void MemoryPressureGuard::release_resources(uint64_t bytes, bool gpu) {
    if (gpu) {
        uint64_t prev = s_committedVRAM.load(std::memory_order_relaxed);
        uint64_t sub = (bytes <= prev) ? bytes : prev;
        s_committedVRAM.fetch_sub(sub, std::memory_order_relaxed);
    } else {
        uint64_t prev = s_committedRAM.load(std::memory_order_relaxed);
        uint64_t sub = (bytes <= prev) ? bytes : prev;
        s_committedRAM.fetch_sub(sub, std::memory_order_relaxed);
    }
}

uint64_t MemoryPressureGuard::committedRAM() {
    return s_committedRAM.load(std::memory_order_relaxed);
}

uint64_t MemoryPressureGuard::committedVRAM() {
    return s_committedVRAM.load(std::memory_order_relaxed);
}
