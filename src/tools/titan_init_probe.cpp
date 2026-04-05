#include <windows.h>
#include <dxgi1_6.h>

#include <cstdio>
#include <cstring>

#pragma comment(lib, "dxgi.lib")

struct RAWRXD_GGML_TITAN_BRIDGE {
    void * ggml_ctx;
    void * model_tensors;
    int n_layers;
    int n_embd;
    int n_head;
    int n_vocab;
    int n_ctx;
    char model_name[256];
    char architecture[64];
};

typedef int  (*TitanInitializeFn)(const char * modelPath);
typedef void (*TitanShutdownFn)(void);
typedef int  (*GetBridgeFn)(RAWRXD_GGML_TITAN_BRIDGE * outBridge);

struct VramSnapshot {
    bool ok;
    UINT64 localUsage;
    UINT64 localBudget;
    UINT64 nonLocalUsage;
    char adapterName[128];
};

static double BytesToGiB(UINT64 bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

static void NarrowAdapterName(const wchar_t * src, char * dst, size_t dstSize) {
    if (!dst || dstSize == 0) {
        return;
    }
    dst[0] = 0;
    if (!src || !src[0]) {
        return;
    }

    int count = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, static_cast<int>(dstSize), nullptr, nullptr);
    if (count <= 0) {
        std::snprintf(dst, dstSize, "(adapter-name-conversion-failed)");
    }
}

static VramSnapshot CaptureVramSnapshot() {
    VramSnapshot snap = {};
    IDXGIFactory6 * factory = nullptr;
    IDXGIAdapter1 * adapter = nullptr;
    IDXGIAdapter3 * adapter3 = nullptr;

    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory6), reinterpret_cast<void **>(&factory));
    if (FAILED(hr) || !factory) {
        return snap;
    }

    hr = factory->EnumAdapterByGpuPreference(
        0,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        __uuidof(IDXGIAdapter1),
        reinterpret_cast<void **>(&adapter));
    if (FAILED(hr) || !adapter) {
        factory->Release();
        return snap;
    }

    DXGI_ADAPTER_DESC1 desc = {};
    if (SUCCEEDED(adapter->GetDesc1(&desc))) {
        NarrowAdapterName(desc.Description, snap.adapterName, sizeof(snap.adapterName));
    }

    hr = adapter->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void **>(&adapter3));
    if (FAILED(hr) || !adapter3) {
        adapter->Release();
        factory->Release();
        return snap;
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO localInfo = {};
    DXGI_QUERY_VIDEO_MEMORY_INFO nonLocalInfo = {};
    hr = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &localInfo);
    HRESULT hr2 = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocalInfo);
    if (SUCCEEDED(hr) && SUCCEEDED(hr2)) {
        snap.ok = true;
        snap.localUsage = localInfo.CurrentUsage;
        snap.localBudget = localInfo.Budget;
        snap.nonLocalUsage = nonLocalInfo.CurrentUsage;
    }

    adapter3->Release();
    adapter->Release();
    factory->Release();
    return snap;
}

static void PrintUsage(const char * exeName) {
    std::printf("Usage: %s <RawrXD_Titan.dll> <model.gguf>\n", exeName ? exeName : "titan_init_probe.exe");
    std::printf("Example: %s d:/rawrxd/build-titan-ggml-v9/bin/RawrXD_Titan.dll d:/phi3mini.gguf\n", exeName ? exeName : "titan_init_probe.exe");
    std::printf("\nOutput includes DXGI VRAM residency deltas when available.\n");
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        PrintUsage(argc > 0 ? argv[0] : "titan_init_probe.exe");
        return 2;
    }

    const char * dllPath = argv[1];
    const char * modelPath = argv[2];

    VramSnapshot before = CaptureVramSnapshot();
    if (before.ok) {
        std::printf("VRAM_ADAPTER:%s\n", before.adapterName[0] ? before.adapterName : "(unknown)");
        std::printf("VRAM_BEFORE:local_usage_gib=%.3f local_budget_gib=%.3f nonlocal_usage_gib=%.3f\n",
                    BytesToGiB(before.localUsage),
                    BytesToGiB(before.localBudget),
                    BytesToGiB(before.nonLocalUsage));
    } else {
        std::printf("VRAM_BEFORE:unavailable\n");
    }

    HMODULE titan = LoadLibraryA(dllPath);
    if (!titan) {
        std::printf("PROBE_FAIL: LoadLibraryA failed for %s (error=%lu)\n", dllPath, GetLastError());
        return 3;
    }

    TitanInitializeFn titanInitialize = reinterpret_cast<TitanInitializeFn>(GetProcAddress(titan, "Titan_Initialize"));
    GetBridgeFn getBridge = reinterpret_cast<GetBridgeFn>(GetProcAddress(titan, "RawrXD_GetGGMLTitanBridge"));
    TitanShutdownFn titanShutdown = reinterpret_cast<TitanShutdownFn>(GetProcAddress(titan, "Titan_Shutdown"));

    if (!titanInitialize || !getBridge) {
        std::printf("PROBE_FAIL: required exports missing (Titan_Initialize=%p RawrXD_GetGGMLTitanBridge=%p)\n",
                    reinterpret_cast<void *>(titanInitialize),
                    reinterpret_cast<void *>(getBridge));
        FreeLibrary(titan);
        return 4;
    }

    const int initRc = titanInitialize(modelPath);
    std::printf("INIT_RC:%d\n", initRc);
    if (initRc != 0) {
        if (titanShutdown) {
            titanShutdown();
        }
        FreeLibrary(titan);
        return 5;
    }

    RAWRXD_GGML_TITAN_BRIDGE bridge = {};
    const int bridgeRc = getBridge(&bridge);
    std::printf("BRIDGE_RC:%d\n", bridgeRc);
    std::printf("GGML_CTX:%p\n", bridge.ggml_ctx);
    std::printf("MODEL_TENSORS:%p\n", bridge.model_tensors);
    std::printf("DIMS:n_layers=%d n_embd=%d n_head=%d n_vocab=%d n_ctx=%d\n",
                bridge.n_layers,
                bridge.n_embd,
                bridge.n_head,
                bridge.n_vocab,
                bridge.n_ctx);
    std::printf("MODEL:model_name=%s architecture=%s\n", bridge.model_name, bridge.architecture);

    VramSnapshot after = CaptureVramSnapshot();
    if (after.ok) {
        std::printf("VRAM_AFTER:local_usage_gib=%.3f local_budget_gib=%.3f nonlocal_usage_gib=%.3f\n",
                    BytesToGiB(after.localUsage),
                    BytesToGiB(after.localBudget),
                    BytesToGiB(after.nonLocalUsage));
        const LONGLONG localDelta = static_cast<LONGLONG>(after.localUsage) - static_cast<LONGLONG>(before.localUsage);
        const LONGLONG nonLocalDelta = static_cast<LONGLONG>(after.nonLocalUsage) - static_cast<LONGLONG>(before.nonLocalUsage);
        std::printf("VRAM_DELTA:local_usage_mib=%.2f nonlocal_usage_mib=%.2f\n",
                    static_cast<double>(localDelta) / (1024.0 * 1024.0),
                    static_cast<double>(nonLocalDelta) / (1024.0 * 1024.0));
        std::printf("VRAM_STATUS:%s\n", localDelta > (64LL * 1024LL * 1024LL) ? "LOCAL_INCREASED" : "LOCAL_FLAT");
    } else {
        std::printf("VRAM_AFTER:unavailable\n");
        std::printf("VRAM_STATUS:UNKNOWN\n");
    }

    const int liveOk = (bridgeRc == 1 && bridge.ggml_ctx != nullptr);
    std::printf("PROBE_STATUS:%s\n", liveOk ? "LIVE" : "NOT_LIVE");

    if (titanShutdown) {
        titanShutdown();
    }

    FreeLibrary(titan);
    return liveOk ? 0 : 6;
}
