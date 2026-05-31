#include "vram_probe.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_6.h>
#endif

namespace RawrXD {

VRAMInfo QueryVRAM()
{
    VRAMInfo out;

#if defined(_WIN32)
    IDXGIFactory6* factory = nullptr;
    if (CreateDXGIFactory1(IID_PPV_ARGS(&factory)) != S_OK || factory == nullptr) {
        return out;
    }

    UINT bestIndex = UINT_MAX;
    SIZE_T bestDedicated = 0;

    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        HRESULT hr = factory->EnumAdapters1(i, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (hr != S_OK || adapter == nullptr) {
            continue;
        }

        DXGI_ADAPTER_DESC1 desc = {};
        if (adapter->GetDesc1(&desc) == S_OK) {
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 && desc.DedicatedVideoMemory >= bestDedicated) {
                bestDedicated = desc.DedicatedVideoMemory;
                bestIndex = i;
            }
        }

        adapter->Release();
    }

    if (bestIndex != UINT_MAX) {
        IDXGIAdapter1* bestAdapter = nullptr;
        if (factory->EnumAdapters1(bestIndex, &bestAdapter) == S_OK && bestAdapter != nullptr) {
            DXGI_ADAPTER_DESC1 desc = {};
            if (bestAdapter->GetDesc1(&desc) == S_OK) {
                out.dedicated_total = static_cast<uint64_t>(desc.DedicatedVideoMemory);
                out.shared_total = static_cast<uint64_t>(desc.SharedSystemMemory);

                IDXGIAdapter3* adapter3 = nullptr;
                if (bestAdapter->QueryInterface(IID_PPV_ARGS(&adapter3)) == S_OK && adapter3 != nullptr) {
                    DXGI_QUERY_VIDEO_MEMORY_INFO local = {};
                    if (adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local) == S_OK) {
                        out.dedicated_total = static_cast<uint64_t>(local.Budget);
                        const uint64_t usage = static_cast<uint64_t>(local.CurrentUsage);
                        out.dedicated_free = out.dedicated_total > usage ? out.dedicated_total - usage : 0;
                        out.valid = true;
                    }

                    DXGI_QUERY_VIDEO_MEMORY_INFO nonLocal = {};
                    if (adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocal) == S_OK) {
                        out.shared_total = static_cast<uint64_t>(nonLocal.Budget);
                        const uint64_t usage = static_cast<uint64_t>(nonLocal.CurrentUsage);
                        out.shared_free = out.shared_total > usage ? out.shared_total - usage : 0;
                    }

                    adapter3->Release();
                }
            }

            bestAdapter->Release();
        }
    }

    factory->Release();
#endif

    return out;
}

} // namespace RawrXD
