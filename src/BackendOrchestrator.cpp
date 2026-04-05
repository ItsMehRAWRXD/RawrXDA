// BackendOrchestrator.cpp — Implementation
#include "BackendOrchestrator.h"
#include "InferenceProfiler.h"
#include "gguf_loader.h"
#include "kernels/kv_accum_avx512.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <iomanip>
#include <numeric>
#include <filesystem>
#include <regex>
#include <unordered_set>
#include <cstdlib>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#ifndef MEM_RESERVE_PLACEHOLDER
#define MEM_RESERVE_PLACEHOLDER 0x00040000
#endif

#ifndef MEM_REPLACE_PLACEHOLDER
#define MEM_REPLACE_PLACEHOLDER 0x00004000
#endif

static constexpr SIZE_T kSlidingApertureSize = 2ULL * 1024ULL * 1024ULL * 1024ULL;
static constexpr SIZE_T kSlidingApertureMinReserve = 8ULL * 1024ULL * 1024ULL;

namespace RawrXD {

namespace {

using PFN_VirtualAlloc2 = PVOID(WINAPI*)(
    HANDLE,
    PVOID,
    SIZE_T,
    ULONG,
    ULONG,
    MEM_EXTENDED_PARAMETER*,
    ULONG);

using PFN_MapViewOfFile3 = PVOID(WINAPI*)(
    HANDLE,
    HANDLE,
    PVOID,
    ULONG64,
    SIZE_T,
    ULONG,
    ULONG,
    MEM_EXTENDED_PARAMETER*,
    ULONG);

using PFN_UnmapViewOfFile2 = BOOL(WINAPI*)(
    HANDLE,
    PVOID,
    ULONG);

using PFN_PrefetchVirtualMemory = BOOL(WINAPI*)(
    HANDLE,
    ULONG_PTR,
    PWIN32_MEMORY_RANGE_ENTRY,
    ULONG);

using PFN_SubmitInference = bool(__stdcall*)(const char*, uint64_t*);
using PFN_GetResult = bool(__stdcall*)(uint64_t, char*, uint32_t);
using PFN_CreateInferenceEngine = void*(__stdcall*)();
using PFN_DestroyInferenceEngine = void(__stdcall*)(void*);
using PFN_InferenceEngineLoadModel = bool(__stdcall*)(void*, const char*);
using PFN_InferenceEngineSubmitInference = bool(__stdcall*)(void*, const char*, uint64_t*);
using PFN_InferenceEngineGetResult = bool(__stdcall*)(void*, uint64_t, char*, uint32_t);

PFN_VirtualAlloc2 GetVirtualAlloc2Ptr() {
    static PFN_VirtualAlloc2 fn = []() -> PFN_VirtualAlloc2 {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!kernel32) {
            return nullptr;
        }
        return reinterpret_cast<PFN_VirtualAlloc2>(GetProcAddress(kernel32, "VirtualAlloc2"));
    }();
    return fn;
}

PFN_MapViewOfFile3 GetMapViewOfFile3Ptr() {
    static PFN_MapViewOfFile3 fn = []() -> PFN_MapViewOfFile3 {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!kernel32) {
            return nullptr;
        }
        return reinterpret_cast<PFN_MapViewOfFile3>(GetProcAddress(kernel32, "MapViewOfFile3"));
    }();
    return fn;
}

PFN_UnmapViewOfFile2 GetUnmapViewOfFile2Ptr() {
    static PFN_UnmapViewOfFile2 fn = []() -> PFN_UnmapViewOfFile2 {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!kernel32) {
            return nullptr;
        }
        return reinterpret_cast<PFN_UnmapViewOfFile2>(GetProcAddress(kernel32, "UnmapViewOfFile2"));
    }();
    return fn;
}

PFN_PrefetchVirtualMemory GetPrefetchVirtualMemoryPtr() {
    static PFN_PrefetchVirtualMemory fn = []() -> PFN_PrefetchVirtualMemory {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!kernel32) {
            return nullptr;
        }
        return reinterpret_cast<PFN_PrefetchVirtualMemory>(GetProcAddress(kernel32, "PrefetchVirtualMemory"));
    }();
    return fn;
}

static constexpr uint64_t kGgufMapTelemetryDecisionBudget = 128;
static constexpr size_t kGgufApertureMinWindowBytes = 2ULL * 1024ULL * 1024ULL;
static constexpr size_t kGgufAperturePrefetchBytes = 2ULL * 1024ULL * 1024ULL;

struct GgufMapTelemetry {
    bool enabled = false;
    std::atomic<uint64_t> remaining_decisions{0};
    std::atomic<uint64_t> reuses{0};
    std::atomic<uint64_t> remaps{0};
    std::atomic<uint64_t> prefetch_bytes{0};
    std::atomic<uint64_t> window_bytes{0};
    std::atomic<uint64_t> align_large_2mb{0};
    std::atomic<uint64_t> align_sys_64kb{0};
    std::atomic<uint64_t> align_legacy_reserve{0};
    std::atomic<uint64_t> align_direct_map{0};
};

GgufMapTelemetry& GetGgufMapTelemetry() {
    static GgufMapTelemetry telemetry;
    static std::once_flag init_once;
    GgufMapTelemetry* telemetry_ptr = &telemetry;
    std::call_once(init_once, [telemetry_ptr]() {
        char value[8] = {};
        const DWORD len = GetEnvironmentVariableA("RAWRXD_GGUF_MAP_TELEMETRY", value, static_cast<DWORD>(sizeof(value)));
        telemetry_ptr->enabled = len > 0 && len < sizeof(value) && value[0] != '0';
        telemetry_ptr->remaining_decisions.store(
            telemetry_ptr->enabled ? kGgufMapTelemetryDecisionBudget : 0,
            std::memory_order_relaxed);
    });
    return telemetry;
}

bool ShouldRecordGgufMapDecision() {
    auto& telemetry = GetGgufMapTelemetry();
    if (!telemetry.enabled) {
        return false;
    }
    uint64_t remaining = telemetry.remaining_decisions.load(std::memory_order_relaxed);
    while (remaining > 0) {
        if (telemetry.remaining_decisions.compare_exchange_weak(
                remaining, remaining - 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void RecordGgufMapReuse(size_t window_bytes) {
    if (!ShouldRecordGgufMapDecision()) {
        return;
    }
    auto& telemetry = GetGgufMapTelemetry();
    telemetry.reuses.fetch_add(1, std::memory_order_relaxed);
    telemetry.window_bytes.fetch_add(static_cast<uint64_t>(window_bytes), std::memory_order_relaxed);
}

void RecordGgufMapRemap(size_t window_bytes) {
    if (!ShouldRecordGgufMapDecision()) {
        return;
    }
    auto& telemetry = GetGgufMapTelemetry();
    telemetry.remaps.fetch_add(1, std::memory_order_relaxed);
    telemetry.window_bytes.fetch_add(static_cast<uint64_t>(window_bytes), std::memory_order_relaxed);
}

void RecordGgufPrefetch(size_t prefetch_bytes) {
    auto& telemetry = GetGgufMapTelemetry();
    if (!telemetry.enabled || prefetch_bytes == 0) {
        return;
    }
    telemetry.prefetch_bytes.fetch_add(static_cast<uint64_t>(prefetch_bytes), std::memory_order_relaxed);
}

enum : uint32_t {
    kAlignModeLarge2MB = 1u << 0,
    kAlignModeSys64KB = 1u << 1,
    kAlignModeLegacyReserve = 1u << 2,
    kAlignModeDirectMap = 1u << 3,
};

void RecordGgufAlignmentMode(uint32_t mode) {
    auto& telemetry = GetGgufMapTelemetry();
    if (!telemetry.enabled) {
        return;
    }
    if ((mode & kAlignModeLarge2MB) != 0u) {
        telemetry.align_large_2mb.fetch_add(1, std::memory_order_relaxed);
    }
    if ((mode & kAlignModeSys64KB) != 0u) {
        telemetry.align_sys_64kb.fetch_add(1, std::memory_order_relaxed);
    }
    if ((mode & kAlignModeLegacyReserve) != 0u) {
        telemetry.align_legacy_reserve.fetch_add(1, std::memory_order_relaxed);
    }
    if ((mode & kAlignModeDirectMap) != 0u) {
        telemetry.align_direct_map.fetch_add(1, std::memory_order_relaxed);
    }
}

bool HasSeLockMemoryPrivilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    LUID luid{};
    if (!LookupPrivilegeValueA(nullptr, SE_LOCK_MEMORY_NAME, &luid)) {
        CloseHandle(token);
        return false;
    }

    PRIVILEGE_SET required{};
    required.PrivilegeCount = 1;
    required.Control = PRIVILEGE_SET_ALL_NECESSARY;
    required.Privilege[0].Luid = luid;
    required.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL hasPrivilege = FALSE;
    const BOOL ok = PrivilegeCheck(token, &required, &hasPrivilege);
    CloseHandle(token);
    return ok && hasPrivilege != FALSE;
}

bool IsLargePageAlignmentAvailable() {
    static std::once_flag once;
    static bool available = false;
    std::call_once(once, []() {
        const SIZE_T largePageMin = GetLargePageMinimum();
        available = (largePageMin >= (2ULL * 1024ULL * 1024ULL)) && HasSeLockMemoryPrivilege();
    });
    return available;
}

void FlushGgufMapTelemetrySummary() {
    auto& telemetry = GetGgufMapTelemetry();
    if (!telemetry.enabled) {
        return;
    }

    const uint64_t reuses = telemetry.reuses.exchange(0, std::memory_order_relaxed);
    const uint64_t remaps = telemetry.remaps.exchange(0, std::memory_order_relaxed);
    const uint64_t prefetch_bytes = telemetry.prefetch_bytes.exchange(0, std::memory_order_relaxed);
    const uint64_t window_bytes = telemetry.window_bytes.exchange(0, std::memory_order_relaxed);
    const uint64_t align_large_2mb = telemetry.align_large_2mb.exchange(0, std::memory_order_relaxed);
    const uint64_t align_sys_64kb = telemetry.align_sys_64kb.exchange(0, std::memory_order_relaxed);
    const uint64_t align_legacy_reserve = telemetry.align_legacy_reserve.exchange(0, std::memory_order_relaxed);
    const uint64_t align_direct_map = telemetry.align_direct_map.exchange(0, std::memory_order_relaxed);
    telemetry.remaining_decisions.store(kGgufMapTelemetryDecisionBudget, std::memory_order_relaxed);

    if (reuses == 0 && remaps == 0 && prefetch_bytes == 0 &&
        align_large_2mb == 0 && align_sys_64kb == 0 && align_legacy_reserve == 0 && align_direct_map == 0) {
        return;
    }

    const uint64_t decisions = reuses + remaps;
    const uint64_t avg_window_kb = decisions == 0 ? 0 : (window_bytes / decisions) / 1024ULL;
    uint32_t alignMask = 0;
    if (align_large_2mb > 0) {
        alignMask |= kAlignModeLarge2MB;
    }
    if (align_sys_64kb > 0) {
        alignMask |= kAlignModeSys64KB;
    }
    if (align_legacy_reserve > 0) {
        alignMask |= kAlignModeLegacyReserve;
    }

    std::string alignMode;
    if ((alignMask & kAlignModeLarge2MB) != 0u) {
        alignMode = "LARGE_2MB";
    }
    if ((alignMask & kAlignModeSys64KB) != 0u) {
        if (!alignMode.empty()) {
            alignMode += "|";
        }
        alignMode += "SYS_64KB";
    }
    if ((alignMask & kAlignModeLegacyReserve) != 0u) {
        if (!alignMode.empty()) {
            alignMode += "|";
        }
        alignMode += "LEGACY_RESERVE";
    }
    if ((alignMask & kAlignModeDirectMap) != 0u) {
        if (!alignMode.empty()) {
            alignMode += "|";
        }
        alignMode += "DIRECT_MAP";
    }
    if (alignMode.empty()) {
        alignMode = "UNSPECIFIED";
    }

    std::ostringstream oss;
    oss << "GGUF_MAP_STATS: reuses=" << reuses
        << ", remaps=" << remaps
        << ", prefetch_kb=" << (prefetch_bytes / 1024ULL)
        << ", avg_win=" << avg_window_kb << "KB"
        << ", ALIGN_MODE=" << alignMode
        << ", ALIGN_MASK=0x" << std::hex << alignMask << std::dec
        << "\n";
    const std::string line = oss.str();
    OutputDebugStringA(line.c_str());
    std::cout << line;
    std::cout.flush();
}

struct NativeInferenceApi {
    HMODULE module = nullptr;
    PFN_SubmitInference submit = nullptr;
    PFN_GetResult getResult = nullptr;
    PFN_CreateInferenceEngine createEngine = nullptr;
    PFN_DestroyInferenceEngine destroyEngine = nullptr;
    PFN_InferenceEngineLoadModel loadModel = nullptr;
    PFN_InferenceEngineSubmitInference submitEngine = nullptr;
    PFN_InferenceEngineGetResult getResultEngine = nullptr;
    void* engineHandle = nullptr;
    bool modelLoaded = false;
    bool sawWin32Symbols = false;
    bool engineCreateFailed = false;
    bool initialized = false;
};

NativeInferenceApi& GetNativeInferenceApi() {
    static NativeInferenceApi api;
    if (api.initialized) {
        return api;
    }

    const wchar_t* kCandidates[] = {
        L"RawrXD_InferenceEngine.dll",
        L"RawrXD_InferenceEngine_Win32.dll"
    };

    for (const wchar_t* candidate : kCandidates) {
        HMODULE module = LoadLibraryW(candidate);
        if (!module) {
            continue;
        }

        PFN_SubmitInference legacySubmit = reinterpret_cast<PFN_SubmitInference>(
            GetProcAddress(module, "SubmitInference"));
        PFN_GetResult legacyGetResult = reinterpret_cast<PFN_GetResult>(
            GetProcAddress(module, "GetResult"));

        if (legacySubmit && legacyGetResult) {
            api.module = module;
            api.submit = legacySubmit;
            api.getResult = legacyGetResult;
            break;
        }

        PFN_CreateInferenceEngine createEngine = reinterpret_cast<PFN_CreateInferenceEngine>(
            GetProcAddress(module, "CreateInferenceEngine"));
        PFN_DestroyInferenceEngine destroyEngine = reinterpret_cast<PFN_DestroyInferenceEngine>(
            GetProcAddress(module, "DestroyInferenceEngine"));
        PFN_InferenceEngineLoadModel loadModel = reinterpret_cast<PFN_InferenceEngineLoadModel>(
            GetProcAddress(module, "InferenceEngine_LoadModel"));
        PFN_InferenceEngineSubmitInference submitEngine = reinterpret_cast<PFN_InferenceEngineSubmitInference>(
            GetProcAddress(module, "InferenceEngine_SubmitInference"));
        PFN_InferenceEngineGetResult getResultEngine = reinterpret_cast<PFN_InferenceEngineGetResult>(
            GetProcAddress(module, "InferenceEngine_GetResult"));

        if (createEngine && submitEngine && getResultEngine) {
            api.sawWin32Symbols = true;
            void* engineHandle = createEngine();
            if (engineHandle) {
                api.module = module;
                api.createEngine = createEngine;
                api.destroyEngine = destroyEngine;
                api.loadModel = loadModel;
                api.submitEngine = submitEngine;
                api.getResultEngine = getResultEngine;
                api.engineHandle = engineHandle;
                break;
            } else {
                api.engineCreateFailed = true;
            }
        }

        FreeLibrary(module);
    }

    api.initialized = true;
    return api;
}

bool RunNativeInferenceSync(const std::string& prompt,
                            uint32_t timeoutMs,
                            std::string& completion,
                            std::string& metadata,
                            std::string& error) {
    NativeInferenceApi& api = GetNativeInferenceApi();
    const bool legacyApiReady = (api.module && api.submit && api.getResult);
    const bool win32ApiReady = (api.module && api.engineHandle && api.submitEngine && api.getResultEngine);

    if (!legacyApiReady && !win32ApiReady) {
        if (api.sawWin32Symbols && api.engineCreateFailed) {
            error = "Native inference API unavailable (Win32 symbols found, CreateInferenceEngine failed)";
        } else {
            error = "Native inference API unavailable (SubmitInference/GetResult missing)";
        }
        return false;
    }

    if (win32ApiReady && api.loadModel && !api.modelLoaded) {
        char modelPath[2048] = {};
        const DWORD len = GetEnvironmentVariableA("RAWRXD_NATIVE_MODEL_PATH", modelPath, static_cast<DWORD>(sizeof(modelPath)));
        if (len > 0 && len < sizeof(modelPath)) {
            api.modelLoaded = api.loadModel(api.engineHandle, modelPath);
        }
    }

    uint64_t requestId = 0;
    if (legacyApiReady) {
        if (!api.submit(prompt.c_str(), &requestId)) {
            error = "SubmitInference failed";
            return false;
        }
    } else {
        if (!api.submitEngine(api.engineHandle, prompt.c_str(), &requestId)) {
            error = "InferenceEngine_SubmitInference failed";
            return false;
        }
    }

    auto start = std::chrono::steady_clock::now();
    std::vector<char> buffer(1024 * 1024, 0);

    while (true) {
        const bool gotResult = legacyApiReady
            ? api.getResult(requestId, buffer.data(), static_cast<uint32_t>(buffer.size()))
            : api.getResultEngine(api.engineHandle, requestId, buffer.data(), static_cast<uint32_t>(buffer.size()));

        if (gotResult) {
            completion = std::string(buffer.data());
            const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count();
            std::ostringstream oss;
            oss << "{\"prompt_eval_count\":0,\"eval_count\":0,\"eval_duration\":"
                << elapsedNs << ",\"total_duration\":" << elapsedNs << "}";
            metadata = oss.str();
            return true;
        }

        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (timeoutMs > 0 && elapsedMs >= timeoutMs) {
            error = "Inference request timed out";
            return false;
        }

        Sleep(10);
    }
}

std::string zeroPadInt(int value, int width) {
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << value;
    return oss.str();
}

bool discoverModelShards(const std::string& model_path,
                         std::vector<std::string>& shards,
                         std::string& reason) {
    namespace fs = std::filesystem;

    shards.clear();
    reason.clear();

    std::error_code ec;
    fs::path model(model_path);
    if (!fs::exists(model, ec) || ec) {
        reason = "model path does not exist";
        return false;
    }

    const std::string filename = model.filename().string();
    std::smatch match;
    const std::regex shard_pattern(R"(^(.+)-(\d+)-of-(\d+)\.gguf$)", std::regex::icase);

    if (!std::regex_match(filename, match, shard_pattern)) {
        shards.push_back(model.string());
        return true;
    }

    const std::string base_name = match[1].str();
    const int idx_width = static_cast<int>(match[2].str().size());
    const int total_width = static_cast<int>(match[3].str().size());
    const int total = std::max(0, std::atoi(match[3].str().c_str()));
    if (total <= 0) {
        reason = "invalid shard count in filename";
        return false;
    }

    const fs::path dir = model.parent_path();
    shards.reserve(static_cast<size_t>(total));
    for (int i = 1; i <= total; ++i) {
        const std::string expected_name =
            base_name + "-" + zeroPadInt(i, idx_width) + "-of-" + zeroPadInt(total, total_width) + ".gguf";
        const fs::path candidate = dir / expected_name;
        if (!fs::exists(candidate, ec) || ec) {
            reason = "missing shard file: " + candidate.string();
            return false;
        }
        shards.push_back(candidate.string());
    }

    return true;
}

int discoverLayerCountFromGguf(const std::string& model_path) {
    GGUFLoader loader;
    if (!loader.Open(model_path)) {
        return 0;
    }
    if (!loader.ParseMetadata()) {
        loader.Close();
        return 0;
    }

    const auto metadata = loader.GetMetadata();
    loader.Close();

    if (metadata.layer_count > 0) {
        return static_cast<int>(metadata.layer_count);
    }

    auto it = metadata.kv_pairs.find("llama.block_count");
    if (it != metadata.kv_pairs.end()) {
        const int parsed = std::atoi(it->second.c_str());
        if (parsed > 0) {
            return parsed;
        }
    }
    return 0;
}

struct MappedShardFile {
    struct WindowStripe {
        void* ptr = nullptr;
        uint64_t offset = 0;
        size_t size = 0;
    };

    std::string path;
    int device_index = -1;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
    void* view = nullptr;
    uint64_t size_bytes = 0;
    size_t reserved_aperture_size = 0;
    bool has_placeholder_reservation = false;
    void* active_window = nullptr;
    uint64_t active_window_offset = 0;
    size_t active_window_size = 0;
    std::vector<WindowStripe> stripes;
    size_t next_stripe_index = 0;
};

bool readEnvFlag(const char* name, bool defaultValue) {
    if (!name || !*name) {
        return defaultValue;
    }
    char v[16] = {};
    const DWORD len = GetEnvironmentVariableA(name, v, static_cast<DWORD>(sizeof(v)));
    if (len == 0 || len >= sizeof(v)) {
        return defaultValue;
    }
    if (v[0] == '0' || v[0] == 'n' || v[0] == 'N' || v[0] == 'f' || v[0] == 'F') {
        return false;
    }
    return true;
}

int readEnvInt(const char* name, int defaultValue, int minValue, int maxValue) {
    if (!name || !*name) {
        return defaultValue;
    }
    char v[32] = {};
    const DWORD len = GetEnvironmentVariableA(name, v, static_cast<DWORD>(sizeof(v)));
    if (len == 0 || len >= sizeof(v)) {
        return defaultValue;
    }

    char* end = nullptr;
    const long parsed = std::strtol(v, &end, 10);
    if (end == v) {
        return defaultValue;
    }
    const long clamped = std::max<long>(minValue, std::min<long>(maxValue, parsed));
    return static_cast<int>(clamped);
}

int getApertureStripeCount() {
    return readEnvInt("RAWRXD_APERTURE_STRIPES", 2, 1, 8);
}

bool useGraphAwarePrefetch() {
    return readEnvFlag("RAWRXD_GRAPH_AWARE_PREFETCH", true);
}

SIZE_T alignDownSize(SIZE_T value, SIZE_T alignment) {
    if (alignment == 0) {
        return value;
    }
    return value - (value % alignment);
}

SIZE_T alignUpSize(SIZE_T value, SIZE_T alignment) {
    if (alignment == 0) {
        return value;
    }
    const SIZE_T rem = value % alignment;
    if (rem == 0) {
        return value;
    }
    if (value > std::numeric_limits<SIZE_T>::max() - (alignment - rem)) {
        return value;
    }
    return value + (alignment - rem);
}

bool reserveSlidingAperture(SIZE_T requested_size,
                           void*& out_view,
                           size_t& out_reserved_size,
                           bool& out_has_placeholder_reservation) {
    out_view = nullptr;
    out_reserved_size = 0;
    out_has_placeholder_reservation = false;

    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    const SIZE_T granularity = static_cast<SIZE_T>(sysInfo.dwAllocationGranularity);
    if (granularity == 0) {
        return false;
    }

    const SIZE_T min_reserve = alignUpSize(kSlidingApertureMinReserve, granularity);
    SIZE_T attempt_size = alignDownSize(requested_size, granularity);
    if (attempt_size < min_reserve) {
        attempt_size = min_reserve;
    }

    bool allow2mb = IsLargePageAlignmentAvailable();
    {
        char v[8] = {};
        const DWORD len = GetEnvironmentVariableA("RAWRXD_PLACEHOLDER_ALIGN_2MB", v, static_cast<DWORD>(sizeof(v)));
        if (len > 0 && len < sizeof(v) && v[0] == '0') {
            allow2mb = false;
        }
    }
    const SIZE_T large_page_alignment = 2ULL * 1024ULL * 1024ULL;

    PFN_VirtualAlloc2 virtualAlloc2 = GetVirtualAlloc2Ptr();

    while (attempt_size >= min_reserve) {
        void* view = nullptr;
        if (virtualAlloc2) {
            std::vector<SIZE_T> alignments;
            alignments.reserve(2);
            if (allow2mb && large_page_alignment >= granularity) {
                alignments.push_back(large_page_alignment);
            }
            alignments.push_back(granularity);

            for (const SIZE_T alignment : alignments) {
                SIZE_T aligned_attempt_size = alignDownSize(attempt_size, alignment);
                if (aligned_attempt_size < min_reserve) {
                    aligned_attempt_size = alignUpSize(min_reserve, alignment);
                }
                if (aligned_attempt_size < min_reserve) {
                    continue;
                }

                MEM_ADDRESS_REQUIREMENTS addrReq{};
                addrReq.LowestStartingAddress = nullptr;
                addrReq.HighestEndingAddress = nullptr;
                addrReq.Alignment = alignment;

                MEM_EXTENDED_PARAMETER param{};
                param.Type = MemExtendedParameterAddressRequirements;
                param.Pointer = &addrReq;

                view = virtualAlloc2(GetCurrentProcess(), nullptr, aligned_attempt_size,
                                     MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
                                     &param, 1);
                if (view) {
                    out_view = view;
                    out_reserved_size = static_cast<size_t>(aligned_attempt_size);
                    out_has_placeholder_reservation = true;
                    if (alignment == large_page_alignment) {
                        RecordGgufAlignmentMode(kAlignModeLarge2MB);
                    } else {
                        RecordGgufAlignmentMode(kAlignModeSys64KB);
                    }
                    return true;
                }
            }
        }

        if (!view) {
            view = VirtualAlloc(nullptr, attempt_size, MEM_RESERVE, PAGE_NOACCESS);
        }

        if (view) {
            out_view = view;
            out_reserved_size = static_cast<size_t>(attempt_size);
            out_has_placeholder_reservation = false;
            RecordGgufAlignmentMode(kAlignModeLegacyReserve | kAlignModeSys64KB);
            return true;
        }

        const SIZE_T next_attempt = alignDownSize(attempt_size / 2, granularity);
        if (next_attempt < min_reserve || next_attempt >= attempt_size) {
            break;
        }
        attempt_size = next_attempt;
    }

    return false;
}

std::mutex g_mapped_shards_mutex;
std::unordered_map<std::string, std::vector<MappedShardFile>> g_mapped_shards_by_tag;

void unmapSlidingWindow(MappedShardFile& file) {
    auto unmap_one = [&](void* ptr) {
        if (!ptr) {
            return;
        }
        if (file.has_placeholder_reservation && GetUnmapViewOfFile2Ptr()) {
            PFN_UnmapViewOfFile2 unmapViewOfFile2 = GetUnmapViewOfFile2Ptr();
            unmapViewOfFile2(GetCurrentProcess(), ptr, MEM_PRESERVE_PLACEHOLDER);
        } else {
            UnmapViewOfFile(ptr);
        }
    };

    if (!file.stripes.empty()) {
        for (auto& stripe : file.stripes) {
            unmap_one(stripe.ptr);
            stripe.ptr = nullptr;
            stripe.offset = 0;
            stripe.size = 0;
        }
    } else if (file.active_window) {
        unmap_one(file.active_window);
    }

    file.active_window = nullptr;
    file.active_window_offset = 0;
    file.active_window_size = 0;
    file.next_stripe_index = 0;
}

void closeMappedShardFile(MappedShardFile& m) {
    unmapSlidingWindow(m);
    if (m.view) {
        // [HOTPATCH] For placeholder reservations, use VirtualFree instead of UnmapViewOfFile
        VirtualFree(m.view, 0, MEM_RELEASE);
        m.view = nullptr;
    }
    if (m.hMapping) {
        CloseHandle(m.hMapping);
        m.hMapping = nullptr;
    }
    if (m.hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m.hFile);
        m.hFile = INVALID_HANDLE_VALUE;
    }
}

void clearTagMappedShardsLocked(const std::string& tag) {
    auto it = g_mapped_shards_by_tag.find(tag);
    if (it == g_mapped_shards_by_tag.end()) {
        return;
    }
    for (auto& mapped : it->second) {
        closeMappedShardFile(mapped);
    }
    g_mapped_shards_by_tag.erase(it);
}

bool mapShardFileReadOnly(const std::string& path, int device_index, MappedShardFile& out, std::string& reason) {
    out = {};
    out.path = path;
    out.device_index = device_index;

    out.hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out.hFile == INVALID_HANDLE_VALUE) {
        reason = "CreateFileA failed for: " + path;
        return false;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(out.hFile, &size) || size.QuadPart <= 0) {
        reason = "GetFileSizeEx failed or empty file: " + path;
        closeMappedShardFile(out);
        return false;
    }
    out.size_bytes = static_cast<uint64_t>(size.QuadPart);

    // Reserve a fixed-size placeholder aperture so MapViewOfFile3 can atomically
    // replace the full placeholder region without needing placeholder splitting.
    const SIZE_T target_aperture = static_cast<SIZE_T>(std::min<uint64_t>(out.size_bytes, kSlidingApertureSize));
    if (!reserveSlidingAperture(target_aperture, out.view, out.reserved_aperture_size, out.has_placeholder_reservation)) {
        out.view = nullptr;
        out.reserved_aperture_size = 0;
        out.has_placeholder_reservation = false;
    }

    const bool allowDirectMap =
        readEnvFlag("RAWRXD_ALLOW_DIRECT_MAP", false) || readEnvFlag("RAWRXD_HEADLESS_MINIMAL", false);
    if (!out.view) {
        if (!allowDirectMap) {
            reason = "placeholder reservation failed for: " + path + " (error=" + std::to_string(GetLastError()) + ")";
            closeMappedShardFile(out);
            return false;
        }

        // Resilience mode: allow direct view mapping without placeholder aperture.
        // This keeps model registration alive even on fragmented VA spaces where placeholder reserve fails.
        out.view = nullptr;
        out.reserved_aperture_size = 0;
        out.has_placeholder_reservation = false;
        RecordGgufAlignmentMode(kAlignModeDirectMap | kAlignModeSys64KB);

        std::ostringstream oss;
        oss << "[LoaderTrace] direct-map fallback enabled for " << path
            << (readEnvFlag("RAWRXD_ALLOW_DIRECT_MAP", false)
                    ? " (RAWRXD_ALLOW_DIRECT_MAP=1)\n"
                    : " (RAWRXD_HEADLESS_MINIMAL=1)\n");
        const std::string line = oss.str();
        OutputDebugStringA(line.c_str());
        std::cout << line;
        std::cout.flush();
    }

    // Create file mapping for sliding window access
    out.hMapping = CreateFileMappingA(out.hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!out.hMapping) {
        reason = "CreateFileMappingA failed for: " + path;
        closeMappedShardFile(out);
        return false;
    }

    // Note: We don't map the entire file here - we reuse a single sliding aperture.

    return true;
}

bool mapShardsForTag(const std::string& tag, const std::vector<ModelShard>& shards, std::string& reason) {
    std::lock_guard<std::mutex> lock(g_mapped_shards_mutex);
    clearTagMappedShardsLocked(tag);

    std::vector<MappedShardFile> mapped;
    for (const auto& shard : shards) {
        for (const auto& shard_file : shard.shard_files) {
            MappedShardFile one;
            if (!mapShardFileReadOnly(shard_file, shard.device_index, one, reason)) {
                for (auto& m : mapped) {
                    closeMappedShardFile(m);
                }
                return false;
            }
            mapped.push_back(std::move(one));
        }
    }

    g_mapped_shards_by_tag[tag] = std::move(mapped);
    return true;
}

void clearTagMappedShards(const std::string& tag) {
    std::lock_guard<std::mutex> lock(g_mapped_shards_mutex);
    clearTagMappedShardsLocked(tag);
}

void clearAllMappedShards() {
    std::lock_guard<std::mutex> lock(g_mapped_shards_mutex);
    for (auto& kv : g_mapped_shards_by_tag) {
        for (auto& mapped : kv.second) {
            closeMappedShardFile(mapped);
        }
    }
    g_mapped_shards_by_tag.clear();
}

// [HOTPATCH] Sliding Window Access: map a 64KB-aligned aperture with a larger logical prefetch runway.
bool mapSlidingWindow(MappedShardFile& file, uint64_t offset, size_t window_size, void*& window_ptr, std::string& reason) {
    window_ptr = nullptr;
    const bool allowDirectMap =
        readEnvFlag("RAWRXD_ALLOW_DIRECT_MAP", false) || readEnvFlag("RAWRXD_HEADLESS_MINIMAL", false);

    // Ensure offset is aligned to allocation granularity (usually 64KB)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    uint64_t aligned_offset = (offset / sysInfo.dwAllocationGranularity) * sysInfo.dwAllocationGranularity;

    if (aligned_offset >= file.size_bytes) {
        reason = "sliding window offset beyond file size";
        return false;
    }
    if (offset >= file.size_bytes) {
        reason = "sliding window request beyond file size";
        return false;
    }

    const uint64_t safe_window_size = std::min<uint64_t>(static_cast<uint64_t>(window_size), file.size_bytes - offset);
    const uint64_t requested_end = offset + safe_window_size;

    // Stripe-aware fast path: reuse any existing mapped stripe that already covers the request.
    if (!file.stripes.empty()) {
        for (const auto& stripe : file.stripes) {
            if (stripe.ptr && offset >= stripe.offset && requested_end <= stripe.offset + stripe.size) {
                window_ptr = static_cast<char*>(stripe.ptr) + static_cast<size_t>(offset - stripe.offset);
                RecordGgufMapReuse(stripe.size);
                return true;
            }
        }
    } else if (file.active_window && offset >= file.active_window_offset && requested_end <= file.active_window_offset + file.active_window_size) {
        window_ptr = static_cast<char*>(file.active_window) + static_cast<size_t>(offset - file.active_window_offset);
        RecordGgufMapReuse(file.active_window_size);
        return true;
    }

    // Calculate how much to map. Keep the physical offset aligned at 64KB, but
    // expand the logical window to include a prefetch runway ahead of the active slice.
    uint64_t desired_window = std::max<uint64_t>(safe_window_size, kGgufApertureMinWindowBytes);
    desired_window = std::max<uint64_t>(desired_window, safe_window_size + kGgufAperturePrefetchBytes);
    desired_window = std::min<uint64_t>(desired_window, file.size_bytes - aligned_offset);
    desired_window = std::min<uint64_t>(desired_window, kSlidingApertureSize);
    if (file.reserved_aperture_size > 0) {
        desired_window = std::min<uint64_t>(desired_window, static_cast<uint64_t>(file.reserved_aperture_size));
    }
    const size_t map_size = static_cast<size_t>(desired_window);
    void* base_address = file.view;

    auto map_one_window = [&](uint64_t map_offset, size_t one_map_size, void* one_base, void*& mapped_out, std::string& map_reason) -> bool {
        mapped_out = nullptr;

        if (PFN_MapViewOfFile3 mapViewOfFile3 = GetMapViewOfFile3Ptr()) {
            ULONG allocationType = 0;
            if (file.has_placeholder_reservation) {
                allocationType = MEM_REPLACE_PLACEHOLDER;
            } else if (!allowDirectMap) {
                allocationType = MEM_REPLACE_PLACEHOLDER;
            }

            void* mapped_view = mapViewOfFile3(
                file.hMapping,
                GetCurrentProcess(),
                file.has_placeholder_reservation ? one_base : nullptr,
                map_offset,
                one_map_size,
                allocationType,
                PAGE_READONLY,
                nullptr,
                0);
            if (mapped_view) {
                mapped_out = mapped_view;
                std::ostringstream oss;
                oss << "[LoaderTrace] MapViewOfFile3 mode="
                    << (file.has_placeholder_reservation ? "placeholder-replace" : "direct")
                    << " base=" << mapped_view
                    << " offset=" << map_offset
                    << " size=" << one_map_size << "\n";
                const std::string line = oss.str();
                OutputDebugStringA(line.c_str());
                std::cout << line;
                std::cout.flush();
            }
        }

        // Legacy fallback for systems without placeholder swap support.
        if (!mapped_out) {
            if (file.has_placeholder_reservation && map_offset == 0 && one_map_size <= kSlidingApertureSize) {
                mapped_out = MapViewOfFileEx(file.hMapping, FILE_MAP_READ,
                    static_cast<DWORD>(map_offset >> 32), static_cast<DWORD>(map_offset),
                    one_map_size, one_base);
            } else if (!file.has_placeholder_reservation && allowDirectMap) {
                mapped_out = MapViewOfFileEx(file.hMapping, FILE_MAP_READ,
                    static_cast<DWORD>(map_offset >> 32), static_cast<DWORD>(map_offset),
                    one_map_size, nullptr);
            } else {
                SetLastError(ERROR_NOT_SUPPORTED);
                mapped_out = nullptr;
            }

            if (mapped_out) {
                std::ostringstream oss;
                oss << "[LoaderTrace] MapViewOfFileEx mode="
                    << (file.has_placeholder_reservation ? "reserved-base" : "direct")
                    << " base=" << mapped_out
                    << " offset=" << map_offset
                    << " size=" << one_map_size << "\n";
                const std::string line = oss.str();
                OutputDebugStringA(line.c_str());
                std::cout << line;
                std::cout.flush();
            }
        }

        if (!mapped_out) {
            map_reason = "sliding window map failed at offset " + std::to_string(map_offset) +
                " (error=" + std::to_string(GetLastError()) + ")";
            return false;
        }

        return true;
    };

    // Placeholder reservations only support one active replace mapping without placeholder splitting.
    int stripe_target = getApertureStripeCount();
    if (file.has_placeholder_reservation && stripe_target > 1) {
        stripe_target = 1;
    }

    const bool graph_prefetch = useGraphAwarePrefetch();
    const size_t prefetch_budget = graph_prefetch
        ? static_cast<size_t>(kGgufAperturePrefetchBytes * 2ULL)
        : kGgufAperturePrefetchBytes;

    // Rebuild stripe set for this region.
    unmapSlidingWindow(file);
    file.stripes.assign(static_cast<size_t>(stripe_target), {});

    for (int i = 0; i < stripe_target; ++i) {
        const uint64_t stripe_offset_raw = aligned_offset + (static_cast<uint64_t>(i) * map_size);
        if (stripe_offset_raw >= file.size_bytes) {
            break;
        }

        const uint64_t stripe_offset = (stripe_offset_raw / sysInfo.dwAllocationGranularity) * sysInfo.dwAllocationGranularity;
        const uint64_t stripe_max_size = file.size_bytes - stripe_offset;
        const size_t stripe_size = static_cast<size_t>(std::min<uint64_t>(map_size, stripe_max_size));
        if (stripe_size == 0) {
            break;
        }

        void* mapped_view = nullptr;
        std::string map_reason;
        if (!map_one_window(stripe_offset, stripe_size, base_address, mapped_view, map_reason)) {
            reason = map_reason;
            unmapSlidingWindow(file);
            file.stripes.clear();
            return false;
        }

        file.stripes[static_cast<size_t>(i)].ptr = mapped_view;
        file.stripes[static_cast<size_t>(i)].offset = stripe_offset;
        file.stripes[static_cast<size_t>(i)].size = stripe_size;
        RecordGgufMapRemap(stripe_size);

        if (PFN_PrefetchVirtualMemory prefetchVirtualMemory = GetPrefetchVirtualMemoryPtr()) {
            const uint64_t stripe_end = stripe_offset + stripe_size;
            const uint64_t prefetch_start = (i == 0) ? requested_end : stripe_offset;
            if (prefetch_start < stripe_end) {
                const size_t prefetch_size = static_cast<size_t>(std::min<uint64_t>(stripe_end - prefetch_start, prefetch_budget));
                if (prefetch_size > 0) {
                    WIN32_MEMORY_RANGE_ENTRY range{};
                    range.VirtualAddress = static_cast<char*>(mapped_view) + static_cast<size_t>(prefetch_start - stripe_offset);
                    range.NumberOfBytes = prefetch_size;
                    if (prefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0)) {
                        RecordGgufPrefetch(prefetch_size);
                    }
                }
            }
        }
    }

    for (const auto& stripe : file.stripes) {
        if (stripe.ptr && offset >= stripe.offset && requested_end <= stripe.offset + stripe.size) {
            file.active_window = stripe.ptr;
            file.active_window_offset = stripe.offset;
            file.active_window_size = stripe.size;
            window_ptr = static_cast<char*>(stripe.ptr) + static_cast<size_t>(offset - stripe.offset);
            return true;
        }
    }

    reason = "sliding stripe map did not cover requested range";
    unmapSlidingWindow(file);
    file.stripes.clear();
    return false;
}

} // namespace

std::string GetApertureAlignmentStrategyLabel() {
    return IsLargePageAlignmentAvailable()
        ? "LARGE_2MB->SYS_64KB"
        : "SYS_64KB_ONLY";
}

// ─── Singleton ────────────────────────────────────────────────────────────────
BackendOrchestrator& BackendOrchestrator::Instance() {
    static BackendOrchestrator inst;
    return inst;
}

BackendOrchestrator::BackendOrchestrator() {
    m_failover_order = { BackendKind::Vulkan, BackendKind::HIP,
                         BackendKind::DML,    BackendKind::CPU };

    for (int i = 0; i < (int)BackendKind::Count; ++i) {
        m_health[i].kind = static_cast<BackendKind>(i);
    }

    ConfigureSovereignScalingFromEnv();
}

BackendOrchestrator::~BackendOrchestrator() {
    Shutdown();
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────
bool BackendOrchestrator::Initialize() {
    if (m_initialized.load()) return true;

    ConfigureSovereignScalingFromEnv();

    {
        bool has_avx512 = RawrXD::KernelOps::HasAVX512Runtime();
        if (has_avx512) {
            alignas(64) float src[16] = {0.0f};
            alignas(64) float dst[16] = {0.0f};
            for (int i = 0; i < 16; ++i) {
                src[i] = 1.0f;
                dst[i] = static_cast<float>(i);
            }
            const bool ok = RawrXD::KernelOps::AccumulateKV_AVX512(src, dst, 16);
            has_avx512 = ok && dst[0] == 1.0f && dst[15] == 16.0f;
        }
        m_supports_avx512_kernels.store(has_avx512, std::memory_order_relaxed);
        std::cout << "[BackendOrchestrator] AVX512 KV kernels "
                  << (has_avx512 ? "enabled" : "disabled") << "\n";
    }

    // Probe available backends
    for (int i = 0; i < (int)BackendKind::Count; ++i) {
        RefreshBackendHealth(static_cast<BackendKind>(i));
    }

    // Pick best available as default
    for (BackendKind k : m_failover_order) {
        if (m_health[(int)k].available && m_health[(int)k].healthy) {
            m_active_backend = k;
            break;
        }
    }

    // Start dispatch thread
    m_dispatch_running.store(true);
    m_dispatch_thread = std::thread(&BackendOrchestrator::DispatchLoop, this);

    m_initialized.store(true);
    return true;
}

void BackendOrchestrator::Shutdown() {
    if (!m_initialized.exchange(false)) return;

    // Stop health thread
    StopHealthCheckThread();

    // Stop dispatch thread
    m_dispatch_running.store(false);
    m_queue_cv.notify_all();
    if (m_dispatch_thread.joinable()) m_dispatch_thread.join();

    // Stop metrics thread
    DisableMetricsExport();

    // Release model shard file mappings.
    clearAllMappedShards();
    FlushGgufMapTelemetrySummary();
}

// ─── Enhancement 1: Dynamic backend selection ─────────────────────────────────
BackendKind BackendOrchestrator::SelectBestBackend(size_t model_bytes, int seq_len) const {
    if (m_forced_backend != BackendKind::Count) return m_forced_backend;

    std::unique_lock<std::shared_mutex> lk(m_backend_mtx);

    float   vram_needed_mb = static_cast<float>(model_bytes) / (1024.f * 1024.f);
    // For very long contexts → prefer Vulkan Flash Attention
    for (BackendKind k : m_failover_order) {
        const auto& h = m_health[(int)k];
        if (!h.available || !h.healthy) continue;
        if (k == BackendKind::Vulkan || k == BackendKind::HIP) {
            if (h.vram_total_mb >= vram_needed_mb * 1.2f)
                return k;
        }
        if (k == BackendKind::CPU) return k;  // always available
    }
    return BackendKind::CPU;
}

void BackendOrchestrator::ForceBackend(BackendKind k) {
    m_forced_backend = k;
    m_active_backend = k;
}

BackendKind BackendOrchestrator::GetActiveBackend() const {
    return m_active_backend;
}

bool BackendOrchestrator::SwitchBackend(BackendKind target) {
    std::unique_lock<std::shared_mutex> lk(m_backend_mtx);
    if (!m_health[(int)target].available) return false;
    m_active_backend = target;
    std::cout << "[BackendOrchestrator] Switched to " << BackendName(target) << "\n";
    return true;
}

// ─── Enhancement 2: Model sharding ────────────────────────────────────────────
bool BackendOrchestrator::ShardModel(const std::string& model_path,
                                      const std::vector<int>& device_indices) {
    std::lock_guard<std::mutex> lk(m_shard_mtx);
    m_shards.clear();

    if (device_indices.empty()) return false;

    std::vector<std::string> shard_files;
    std::string shard_reason;
    if (!discoverModelShards(model_path, shard_files, shard_reason)) {
        std::cerr << "[BackendOrchestrator] ShardModel failed: " << shard_reason << "\n";
        return false;
    }

    uint64_t total_model_bytes = 0;
    std::vector<uint64_t> shard_sizes;
    shard_sizes.reserve(shard_files.size());
    for (const auto& shard : shard_files) {
        std::error_code ec;
        const auto shard_size = std::filesystem::file_size(shard, ec);
        if (ec) {
            std::cerr << "[BackendOrchestrator] Failed to stat shard: " << shard << "\n";
            return false;
        }
        const uint64_t sz = static_cast<uint64_t>(shard_size);
        total_model_bytes += sz;
        shard_sizes.push_back(sz);
    }

    int total_layers = discoverLayerCountFromGguf(shard_files.front());
    if (total_layers <= 0) {
        total_layers = 32; // Fallback for incomplete metadata models.
    }

    int n_dev        = (int)device_indices.size();
    int base_layers  = total_layers / n_dev;
    int extra_layers = total_layers % n_dev;

    // Deterministic size-aware balancing: assign largest files first to the least-loaded device.
    std::vector<size_t> shard_order(shard_files.size());
    std::iota(shard_order.begin(), shard_order.end(), 0);
    std::stable_sort(shard_order.begin(), shard_order.end(), [&](size_t a, size_t b) {
        if (shard_sizes[a] == shard_sizes[b]) {
            return a < b;
        }
        return shard_sizes[a] > shard_sizes[b];
    });

    std::vector<std::vector<size_t>> files_per_device(static_cast<size_t>(n_dev));
    std::vector<uint64_t> bytes_per_device(static_cast<size_t>(n_dev), 0);
    for (size_t shard_idx : shard_order) {
        size_t best_device = 0;
        for (size_t d = 1; d < bytes_per_device.size(); ++d) {
            if (bytes_per_device[d] < bytes_per_device[best_device]) {
                best_device = d;
            } else if (bytes_per_device[d] == bytes_per_device[best_device] &&
                       files_per_device[d].size() < files_per_device[best_device].size()) {
                best_device = d;
            }
        }
        files_per_device[best_device].push_back(shard_idx);
        bytes_per_device[best_device] += shard_sizes[shard_idx];
    }

    // Keep per-device shard list in natural shard order for easier debugging and replay.
    for (auto& v : files_per_device) {
        std::sort(v.begin(), v.end());
    }

    int layer_cursor = 0;

    for (int d = 0; d < n_dev; ++d) {
        ModelShard shard;
        shard.device_index = device_indices[d];
        const int layer_count = base_layers + (d < extra_layers ? 1 : 0);
        if (layer_count > 0) {
            shard.layer_start = layer_cursor;
            shard.layer_end = layer_cursor + layer_count - 1;
            layer_cursor += layer_count;
        } else {
            shard.layer_start = -1;
            shard.layer_end = -1;
        }
        shard.assigned_file_bytes = 0;
        shard.shard_files.clear();
        for (size_t shard_idx : files_per_device[static_cast<size_t>(d)]) {
            shard.shard_files.push_back(shard_files[shard_idx]);
            shard.assigned_file_bytes += static_cast<size_t>(shard_sizes[shard_idx]);
        }

        // Current heuristic: vram budget follows actual assigned shard bytes.
        shard.vram_bytes = shard.assigned_file_bytes;
        shard.loaded       = false;
        m_shards.push_back(shard);
    }

    std::cout << "[BackendOrchestrator] Model sharded across "
              << n_dev << " device(s), layers=" << total_layers
              << ", files=" << shard_files.size()
              << ", bytes=" << total_model_bytes << "\n";
    for (const auto& s : m_shards) {
        std::cout << "  device=" << s.device_index
                  << " layers=" << s.layer_start << "-" << s.layer_end
                  << " files=" << s.shard_files.size()
                  << " assigned_bytes=" << s.assigned_file_bytes << "\n";
    }
    return true;
}

std::vector<ModelShard> BackendOrchestrator::GetShards() const {
    std::lock_guard<std::mutex> lk(m_shard_mtx);
    return m_shards;
}

void BackendOrchestrator::ClearShards() {
    std::lock_guard<std::mutex> lk(m_shard_mtx);
    m_shards.clear();
}

// ─── Enhancement 3: Distributed KV-cache ──────────────────────────────────────
void BackendOrchestrator::SetKVCacheGPUBudgetMB(float mb) { m_kv_gpu_budget_mb.store(mb); }
void BackendOrchestrator::SetKVCacheCPUBudgetMB(float mb) { m_kv_cpu_budget_mb.store(mb); }
float BackendOrchestrator::GetKVCacheGPUUsedMB() const { return m_kv_gpu_used_mb.load(); }
float BackendOrchestrator::GetKVCacheCPUUsedMB() const { return m_kv_cpu_used_mb.load(); }

void BackendOrchestrator::EvictKVCacheForTenant(const std::string& tenant_id) {
    // Evict KV entries associated with this tenant by notifying the active backend
    // Full implementation integrates with PagedKVCache / VulkanCompute::m_kvCache
    std::cout << "[BackendOrchestrator] Evicting KV cache for tenant: "
              << tenant_id << "\n";
}

// ─── Enhancement 4: Speculative decoding ─────────────────────────────────────
bool BackendOrchestrator::EnableSpecDecoding(SpecDecodingConfig cfg) {
    if (cfg.draft_model_path.empty()) return false;
    m_spec_cfg = cfg;
    m_spec_cfg.enabled = true;
    std::cout << "[BackendOrchestrator] Speculative decoding enabled, draft="
              << cfg.draft_model_path << " beam=" << cfg.draft_beam << "\n";
    return true;
}

void BackendOrchestrator::DisableSpecDecoding() {
    m_spec_cfg.enabled = false;
}

double BackendOrchestrator::GetSpecDecodingAcceptRate() const {
    return m_spec_accept_rate.load(std::memory_order_relaxed);
}

bool BackendOrchestrator::SupportsAVX512Kernels() const {
    return m_supports_avx512_kernels.load(std::memory_order_relaxed);
}

// ─── Enhancement 5: Model hot-swapping ───────────────────────────────────────
bool BackendOrchestrator::LoadModel(const std::string& path, const std::string& tag) {
    std::vector<int> device_indices;
    {
        std::lock_guard<std::mutex> shard_lk(m_shard_mtx);
        std::unordered_set<int> unique_devices;
        for (const auto& s : m_shards) {
            if (unique_devices.insert(s.device_index).second) {
                device_indices.push_back(s.device_index);
            }
        }
    }
    if (device_indices.empty()) {
        device_indices.push_back(0);
    }

    if (!ShardModel(path, device_indices)) {
        std::cerr << "[BackendOrchestrator] LoadModel failed: ShardModel failed for path=" << path << "\n";
        return false;
    }

    std::vector<ModelShard> plan;
    {
        std::lock_guard<std::mutex> shard_lk(m_shard_mtx);
        plan = m_shards;
    }

    std::string map_reason;
    if (!mapShardsForTag(tag, plan, map_reason)) {
        std::cerr << "[BackendOrchestrator] LoadModel failed: " << map_reason << "\n";
        return false;
    }

    {
        std::lock_guard<std::mutex> mapped_lk(g_mapped_shards_mutex);
        auto it = g_mapped_shards_by_tag.find(tag);
        if (it != g_mapped_shards_by_tag.end() && !it->second.empty()) {
            void* warm_ptr = nullptr;
            std::string warm_reason;
            mapSlidingWindow(it->second.front(), 0, 64 * 1024, warm_ptr, warm_reason);
        }
    }

    std::lock_guard<std::mutex> lk(m_model_mtx);
    m_model_paths[tag] = path;
    // The actual inference engine load remains delegated to active backend.
    std::cout << "[BackendOrchestrator] Registered model tag='" << tag
              << "' path=" << path << " with " << plan.size() << " shard plan entries\n";
    return true;
}

bool BackendOrchestrator::ForensicMapProbe(const std::string& path, uint64_t offset, size_t window_size, std::string* out_reason) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (out_reason) {
            *out_reason = "CreateFileA failed";
        }
        return false;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0) {
        CloseHandle(hFile);
        if (out_reason) {
            *out_reason = "GetFileSizeEx failed or empty file";
        }
        return false;
    }

    const uint64_t file_size = static_cast<uint64_t>(size.QuadPart);
    HANDLE hMapping = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) {
        CloseHandle(hFile);
        if (out_reason) {
            *out_reason = "CreateFileMappingA failed";
        }
        return false;
    }

    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    const uint64_t granularity = std::max<uint64_t>(sysInfo.dwAllocationGranularity, 64 * 1024ULL);
    const uint64_t max_offset = file_size > 0 ? (file_size - 1) : 0;
    const uint64_t safe_offset = std::min<uint64_t>(offset, max_offset);
    const uint64_t aligned_offset = (safe_offset / granularity) * granularity;

    uint64_t desired_window = std::max<uint64_t>(window_size, kGgufApertureMinWindowBytes);
    desired_window = std::max<uint64_t>(desired_window, static_cast<uint64_t>(64 * 1024));
    desired_window = std::min<uint64_t>(desired_window, file_size - aligned_offset);
    if (desired_window == 0) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        if (out_reason) {
            *out_reason = "computed zero map window";
        }
        return false;
    }

    void* mapped_view = MapViewOfFile(hMapping, FILE_MAP_READ,
        static_cast<DWORD>(aligned_offset >> 32),
        static_cast<DWORD>(aligned_offset & 0xFFFFFFFFULL),
        static_cast<SIZE_T>(desired_window));
    if (!mapped_view) {
        const DWORD err = GetLastError();
        CloseHandle(hMapping);
        CloseHandle(hFile);
        if (out_reason) {
            *out_reason = "MapViewOfFile failed (error=" + std::to_string(err) + ")";
        }
        return false;
    }

    volatile unsigned char touch = *reinterpret_cast<volatile unsigned char*>(mapped_view);
    (void)touch;
    RecordGgufMapRemap(static_cast<size_t>(desired_window));

    std::string forensic_stats_line;

    // Emit deterministic forensic evidence even when decision-budget telemetry is gated.
    {
        const uint64_t avg_window_kb = desired_window / 1024ULL;
        std::ostringstream oss;
        oss << "GGUF_MAP_STATS: reuses=0, remaps=1, prefetch_kb=0, avg_win="
            << avg_window_kb << "KB\n";
        forensic_stats_line = oss.str();
        OutputDebugStringA(forensic_stats_line.c_str());
        std::cout << forensic_stats_line;
        std::cout.flush();
    }

    UnmapViewOfFile(mapped_view);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    FlushGgufMapTelemetrySummary();
    if (out_reason) {
        *out_reason = forensic_stats_line;
    }
    return true;
}

bool BackendOrchestrator::SwapToModel(const std::string& tag) {
    std::lock_guard<std::mutex> lk(m_model_mtx);
    auto it = m_model_paths.find(tag);
    if (it == m_model_paths.end()) {
        std::cerr << "[BackendOrchestrator] SwapToModel: unknown tag " << tag << "\n";
        return false;
    }
    m_active_model_tag = tag;
    std::cout << "[BackendOrchestrator] Hot-swapped to model: " << tag << "\n";
    return true;
}

bool BackendOrchestrator::UnloadModel(const std::string& tag) {
    std::lock_guard<std::mutex> lk(m_model_mtx);
    m_model_paths.erase(tag);
    if (m_active_model_tag == tag) m_active_model_tag.clear();
    clearTagMappedShards(tag);
    FlushGgufMapTelemetrySummary();
    return true;
}

std::vector<std::string> BackendOrchestrator::GetLoadedModelTags() const {
    std::lock_guard<std::mutex> lk(m_model_mtx);
    std::vector<std::string> tags;
    tags.reserve(m_model_paths.size());
    for (const auto& [t, _] : m_model_paths) tags.push_back(t);
    return tags;
}

// ─── Enhancement 6: Multi-tenant ──────────────────────────────────────────────
void BackendOrchestrator::CreateTenant(const std::string& tenant_id, int max_ctx) {
    std::lock_guard<std::mutex> lk(m_tenant_mtx);
    m_tenants[tenant_id] = TenantInfo{ max_ctx, 512.f };
}

void BackendOrchestrator::RemoveTenant(const std::string& tenant_id) {
    std::lock_guard<std::mutex> lk(m_tenant_mtx);
    m_tenants.erase(tenant_id);
    EvictKVCacheForTenant(tenant_id);
}

void BackendOrchestrator::SetTenantKVQuota(const std::string& tenant_id, float mb) {
    std::lock_guard<std::mutex> lk(m_tenant_mtx);
    auto it = m_tenants.find(tenant_id);
    if (it != m_tenants.end()) it->second.kv_quota_mb = mb;
}

std::vector<std::string> BackendOrchestrator::GetActiveTenants() const {
    std::lock_guard<std::mutex> lk(m_tenant_mtx);
    std::vector<std::string> out;
    for (const auto& [id, _] : m_tenants) out.push_back(id);
    return out;
}

// ─── Enhancement 7: Priority queue ────────────────────────────────────────────
uint64_t BackendOrchestrator::Enqueue(InferRequest req) {
    req.id           = m_next_req_id.fetch_add(1, std::memory_order_relaxed);
    req.enqueue_time = std::chrono::steady_clock::now();

    int bucket = static_cast<int>(req.priority);
    {
        std::lock_guard<std::mutex> lk(m_queue_mtx);
        m_queues[bucket].push_back(req);
        m_enqueue_times[req.id] = req.enqueue_time;
    }
    m_queue_cv.notify_one();
    return req.id;
}

void BackendOrchestrator::Cancel(uint64_t request_id) {
    std::lock_guard<std::mutex> lk(m_queue_mtx);
    for (auto& q : m_queues) {
        q.erase(std::remove_if(q.begin(), q.end(),
            [=](const InferRequest& r) { return r.id == request_id; }), q.end());
    }
    m_enqueue_times.erase(request_id);
}

int BackendOrchestrator::GetQueueDepth(RequestPriority p) const {
    std::lock_guard<std::mutex> lk(m_queue_mtx);
    return (int)m_queues[static_cast<int>(p)].size();
}

double BackendOrchestrator::GetQueuedWaitTimeMs(uint64_t request_id) const {
    std::lock_guard<std::mutex> lk(m_queue_mtx);
    auto it = m_enqueue_times.find(request_id);
    if (it == m_enqueue_times.end()) return -1.0;
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - it->second).count();
}

void BackendOrchestrator::DispatchLoop() {
    while (m_dispatch_running.load()) {
        InferRequest req;
        bool found = false;
        {
            std::unique_lock<std::mutex> lk(m_queue_mtx);
            m_queue_cv.wait_for(lk, std::chrono::milliseconds(5), [this]{
                for (const auto& q : m_queues) if (!q.empty()) return true;
                return false;
            });
            // Drain highest priority first
            for (auto& q : m_queues) {
                if (!q.empty()) {
                    req   = q.front();
                    q.pop_front();
                    m_enqueue_times.erase(req.id);
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            RunInference(req);
        }
    }
}

bool BackendOrchestrator::RunInference(const InferRequest& req) {
    auto& prof = InferenceProfiler::Instance();
    auto scoped = prof.MakeScoped("inference");

    // Semantic cache lookup (Enhancement 8)
    if (m_cache_enabled) {
        std::string cached;
        if (LookupCache(req.prompt, cached)) {
            if (req.complete_cb) req.complete_cb(cached, "{}");
            return true;
        }
    }

    std::string completion;
    std::string metadata;
    std::string error;
    const uint32_t timeoutMs = 120000;

    const bool ok = RunNativeInferenceSync(req.prompt, timeoutMs, completion, metadata, error);
    if (!ok) {
        if (req.complete_cb) {
            req.complete_cb("[BackendError] " + error,
                "{\"error\":\"native_inference_failed\"}");
        }
        return false;
    }

    if (m_cache_enabled && !completion.empty()) {
        InsertCache(req.prompt, completion);
    }

    if (req.stream_cb && !completion.empty()) {
        req.stream_cb(completion);
    }
    if (req.complete_cb) {
        req.complete_cb(completion, metadata.empty() ? "{}" : metadata);
    }
    return true;
}

// ─── Enhancement 8: Semantic cache ───────────────────────────────────────────
void BackendOrchestrator::EnableSemanticCache(size_t max_entries) {
    m_cache_max     = max_entries;
    m_cache_enabled = true;
}

void BackendOrchestrator::DisableSemanticCache() {
    m_cache_enabled = false;
}

std::string BackendOrchestrator::HashPrompt(const std::string& prompt) const {
    // FNV-1a 64-bit
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : prompt) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

bool BackendOrchestrator::LookupCache(const std::string& prompt,
                                       std::string& out_completion) const {
    if (!m_cache_enabled) return false;
    m_cache_lookups.store(m_cache_lookups.load(std::memory_order_relaxed) + 1,
                          std::memory_order_relaxed);
    std::string key = HashPrompt(prompt);
    std::unique_lock<std::shared_mutex> lk(m_cache_mtx);
    auto it = m_cache.find(key);
    if (it == m_cache.end()) return false;
    out_completion = it->second.completion;
    const_cast<SemanticCacheEntry&>(it->second).hit_count++;
    const_cast<SemanticCacheEntry&>(it->second).last_accessed =
        std::chrono::steady_clock::now();
    m_cache_hits.store(m_cache_hits.load(std::memory_order_relaxed) + 1,
                       std::memory_order_relaxed);
    return true;
}

void BackendOrchestrator::InsertCache(const std::string& prompt,
                                       const std::string& completion) {
    if (!m_cache_enabled) return;
    std::string key = HashPrompt(prompt);
    std::unique_lock<std::shared_mutex> lk(m_cache_mtx);
    if (m_cache.size() >= m_cache_max) {
        // Evict the least-recently-used entry
        auto victim = std::min_element(m_cache.begin(), m_cache.end(),
            [](const auto& a, const auto& b){
                return a.second.last_accessed < b.second.last_accessed; });
        if (victim != m_cache.end()) m_cache.erase(victim);
    }
    SemanticCacheEntry entry;
    entry.prompt_hash    = key;
    entry.completion     = completion;
    entry.last_accessed  = std::chrono::steady_clock::now();
    m_cache[key]         = std::move(entry);
}

void BackendOrchestrator::ClearCache() {
    std::unique_lock<std::shared_mutex> lk(m_cache_mtx);
    m_cache.clear();
    m_cache_hits.store(0);
    m_cache_lookups.store(0);
}

size_t BackendOrchestrator::GetCacheSize() const {
    std::unique_lock<std::shared_mutex> lk(m_cache_mtx);
    return m_cache.size();
}

double BackendOrchestrator::GetCacheHitRate() const {
    uint64_t looks = m_cache_lookups.load(std::memory_order_relaxed);
    if (!looks) return 0.0;
    return static_cast<double>(m_cache_hits.load()) / looks;
}

// ─── Enhancement 9: Context compression ──────────────────────────────────────
void BackendOrchestrator::SetContextCompression(ContextCompressionConfig cfg) {
    m_ctx_comp = cfg;
}

std::string BackendOrchestrator::CompressContext(const std::string& raw_context) const {
    if (!m_ctx_comp.enabled) return raw_context;

    // Word-level tokenization proxy: split by spaces
    std::istringstream iss(raw_context);
    std::vector<std::string> words;
    { std::string w; while (iss >> w) words.push_back(w); }

    int total = (int)words.size();
    int sink  = std::min(m_ctx_comp.sink_tokens, total);
    int win   = std::min(m_ctx_comp.rolling_window, total - sink);

    std::string out;
    for (int i = 0; i < sink; ++i) out += words[i] + " ";
    if (win < total - sink) out += "[...compressed...] ";
    int start = total - win;
    for (int i = std::max(start, sink); i < total; ++i) out += words[i] + " ";
    return out;
}

int BackendOrchestrator::EstimateCompressedTokenCount(int raw_tokens) const {
    if (!m_ctx_comp.enabled) return raw_tokens;
    return std::min(raw_tokens,
        m_ctx_comp.sink_tokens + m_ctx_comp.rolling_window + m_ctx_comp.summary_budget);
}

// ─── Enhancement 10: Adaptive quantization ────────────────────────────────────
void BackendOrchestrator::SetAdaptiveQuantization(bool enabled) {
    m_adaptive_quant = enabled;
}

int BackendOrchestrator::GetCurrentQuantBits() const {
    return m_quant_bits.load(std::memory_order_relaxed);
}

bool BackendOrchestrator::ForceQuantLevel(int bits) {
    if (bits != 4 && bits != 5 && bits != 6 && bits != 8) return false;
    m_quant_bits.store(bits, std::memory_order_relaxed);
    return true;
}

// ─── Enhancement 11: Kernel plugins ──────────────────────────────────────────
bool BackendOrchestrator::RegisterKernelPlugin(KernelPlugin plugin) {
    std::lock_guard<std::mutex> lk(m_plugin_mtx);
    for (const auto& p : m_plugins)
        if (p.name == plugin.name) return false; // duplicate
    plugin.loaded = true;
    m_plugins.push_back(std::move(plugin));
    return true;
}

bool BackendOrchestrator::ActivateKernelPlugin(const std::string& name) {
    std::lock_guard<std::mutex> lk(m_plugin_mtx);
    for (const auto& p : m_plugins) {
        if (p.name == name && p.loaded) {
            m_active_plugin = name;
            return true;
        }
    }
    return false;
}

std::string BackendOrchestrator::GetActiveKernelPlugin() const {
    std::lock_guard<std::mutex> lk(m_plugin_mtx);
    return m_active_plugin;
}

// ─── Enhancement 12: Health checks / failover ────────────────────────────────
BackendHealth BackendOrchestrator::GetBackendHealth(BackendKind k) const {
    std::lock_guard<std::mutex> lk(m_health_mtx);
    return m_health[static_cast<int>(k)];
}

void BackendOrchestrator::RefreshBackendHealth(BackendKind k) {
    BackendHealth& h  = m_health[static_cast<int>(k)];
    h.kind            = k;
    h.last_check      = std::chrono::steady_clock::now();

    switch (k) {
    case BackendKind::CPU:
        h.available = true; h.healthy = true;
        h.vram_total_mb = 0.f;
        break;
    case BackendKind::Vulkan: {
        // Quick probe: check if Vulkan DLL exists
        HMODULE vk = LoadLibraryA("vulkan-1.dll");
        h.available = (vk != nullptr);
        h.healthy   = h.available;
        if (vk) { FreeLibrary(vk); h.vram_total_mb = 16384.f; /* RX 7800 XT */ }
        break;
    }
    case BackendKind::HIP: {
        HMODULE hip = LoadLibraryA("amdhip64.dll");
        h.available = (hip != nullptr);
        h.healthy   = h.available;
        if (hip) { FreeLibrary(hip); h.vram_total_mb = 16384.f; }
        break;
    }
    case BackendKind::DML: {
        HMODULE dml = LoadLibraryA("DirectML.dll");
        h.available = (dml != nullptr);
        h.healthy   = h.available;
        if (dml) FreeLibrary(dml);
        break;
    }
    case BackendKind::Titan: {
        HMODULE t = LoadLibraryA("RawrXD_Titan.dll");
        h.available = (t != nullptr);
        h.healthy   = h.available;
        if (t) FreeLibrary(t);
        break;
    }
    default: break;
    }
}

void BackendOrchestrator::StartHealthCheckThread(std::chrono::seconds interval) {
    if (m_health_running.exchange(true)) return;
    m_health_thread = std::thread([this, interval](){
        HealthCheckLoop(interval);
    });
}

void BackendOrchestrator::StopHealthCheckThread() {
    m_health_running.store(false);
    m_health_cv.notify_all();
    if (m_health_thread.joinable()) m_health_thread.join();
}

void BackendOrchestrator::SetFailoverOrder(std::vector<BackendKind> order) {
    m_failover_order = std::move(order);
}

void BackendOrchestrator::HealthCheckLoop(std::chrono::seconds interval) {
    while (m_health_running.load()) {
        std::unique_lock<std::mutex> lk(m_health_cv_mtx);
        m_health_cv.wait_for(lk, interval);
        if (!m_health_running.load()) break;

        for (int i = 0; i < (int)BackendKind::Count; ++i) {
            std::lock_guard<std::mutex> hlk(m_health_mtx);
            RefreshBackendHealth(static_cast<BackendKind>(i));
        }

        // Failover if active backend degraded
        BackendKind active = m_active_backend;
        if (!m_health[(int)active].healthy) {
            for (BackendKind k : m_failover_order) {
                if (k != active && m_health[(int)k].available && m_health[(int)k].healthy) {
                    SwitchBackend(k);
                    std::cerr << "[BackendOrchestrator] Failover: "
                              << BackendName(active) << " → " << BackendName(k) << "\n";
                    break;
                }
            }
        }
    }
}

// ─── Enhancement 13: Micro-batching ──────────────────────────────────────────
void BackendOrchestrator::SetMicroBatchWindow(std::chrono::milliseconds window) {
    m_batch_window = window;
}
void BackendOrchestrator::SetMaxBatchSize(int n) { m_max_batch_size = n; }
int  BackendOrchestrator::GetCurrentBatchSize() const { return m_cur_batch_size.load(); }
void BackendOrchestrator::FlushBatch() { m_queue_cv.notify_all(); }

// ─── Enhancement 14: Prometheus metrics export ───────────────────────────────
void BackendOrchestrator::EnableMetricsExport(const std::string& path, int interval_s) {
    m_metrics_path = path;
    if (m_metrics_running.exchange(true)) return;
    m_metrics_thread = std::thread([this, interval_s](){ MetricsExportLoop(interval_s); });
}

void BackendOrchestrator::DisableMetricsExport() {
    m_metrics_running.store(false);
    if (m_metrics_thread.joinable()) m_metrics_thread.join();
}

void BackendOrchestrator::DumpMetricsNow() const {
    if (m_metrics_path.empty()) return;
    std::ostringstream oss;
    oss << InferenceProfiler::Instance().GetPrometheusText();
    const SovereignScalingConfig scaling = GetSovereignScalingConfig();

    // AppendBackendOrchestrator-specific metrics
    oss << "# HELP rawrxd_cache_hit_rate Semantic cache hit rate\n"
        << "rawrxd_cache_hit_rate " << GetCacheHitRate() << "\n";
    oss << "rawrxd_queue_depth_vip "    << GetQueueDepth(RequestPriority::VIP)    << "\n";
    oss << "rawrxd_queue_depth_normal " << GetQueueDepth(RequestPriority::Normal) << "\n";
    oss << "rawrxd_queue_depth_batch "  << GetQueueDepth(RequestPriority::Batch)  << "\n";
    oss << "rawrxd_active_backend "     << (int)GetActiveBackend()                << "\n";
    oss << "rawrxd_aperture_stripes "   << scaling.aperture_stripes               << "\n";
    oss << "rawrxd_prefetch_depth "     << GetRecommendedPrefetchDepth()          << "\n";
    oss << "rawrxd_fusion_width "       << GetRecommendedFusionWidth()            << "\n";
    oss << "rawrxd_cpu_temp_c "         << m_cpu_temp_c.load(std::memory_order_relaxed) << "\n";
    oss << "rawrxd_nvme_temp_c "        << m_nvme_temp_c.load(std::memory_order_relaxed) << "\n";

    std::ofstream f(m_metrics_path, std::ios::trunc);
    if (f.is_open()) f << oss.str();
}

void BackendOrchestrator::MetricsExportLoop(int interval_s) {
    while (m_metrics_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(interval_s));
        if (!m_metrics_running.load()) break;
        DumpMetricsNow();
    }
}

void BackendOrchestrator::ConfigureSovereignScaling(SovereignScalingConfig cfg) {
    cfg.aperture_stripes = std::clamp(cfg.aperture_stripes, 1, 8);
    cfg.prefetch_depth = std::clamp(cfg.prefetch_depth, 1, 8);
    cfg.fusion_width = std::clamp(cfg.fusion_width, 1, 8);
    cfg.kv_recent_tokens = std::max(1, cfg.kv_recent_tokens);
    cfg.kv_mid_tokens = std::max(cfg.kv_recent_tokens + 1, cfg.kv_mid_tokens);

    std::lock_guard<std::mutex> lk(m_scaling_mtx);
    m_scaling_cfg = cfg;
}

void BackendOrchestrator::ConfigureSovereignScalingFromEnv() {
    SovereignScalingConfig cfg;
    {
        std::lock_guard<std::mutex> lk(m_scaling_mtx);
        cfg = m_scaling_cfg;
    }

    cfg.enabled = readEnvFlag("RAWRXD_SOVEREIGN_SCALING", cfg.enabled);
    cfg.graph_aware_prefetch = readEnvFlag("RAWRXD_GRAPH_AWARE_PREFETCH", cfg.graph_aware_prefetch);
    cfg.thermal_aware_scheduler = readEnvFlag("RAWRXD_THERMAL_AWARE_SCHED", cfg.thermal_aware_scheduler);
    cfg.async_token_overlap = readEnvFlag("RAWRXD_ASYNC_TOKEN_OVERLAP", cfg.async_token_overlap);
    cfg.nvme_direct_streaming = readEnvFlag("RAWRXD_NVME_DIRECT_STREAM", cfg.nvme_direct_streaming);
    cfg.cross_layer_fusion = readEnvFlag("RAWRXD_CROSS_LAYER_FUSION", cfg.cross_layer_fusion);
    cfg.aperture_stripes = readEnvInt("RAWRXD_APERTURE_STRIPES", cfg.aperture_stripes, 1, 8);
    cfg.prefetch_depth = readEnvInt("RAWRXD_PREFETCH_DEPTH", cfg.prefetch_depth, 1, 8);
    cfg.fusion_width = readEnvInt("RAWRXD_FUSION_WIDTH", cfg.fusion_width, 1, 8);
    cfg.kv_recent_tokens = readEnvInt("RAWRXD_KV_RECENT_TOKENS", cfg.kv_recent_tokens, 1, 65536);
    cfg.kv_mid_tokens = readEnvInt("RAWRXD_KV_MID_TOKENS", cfg.kv_mid_tokens, cfg.kv_recent_tokens + 1, 262144);

    ConfigureSovereignScaling(cfg);
}

SovereignScalingConfig BackendOrchestrator::GetSovereignScalingConfig() const {
    std::lock_guard<std::mutex> lk(m_scaling_mtx);
    return m_scaling_cfg;
}

KVPrecisionTier BackendOrchestrator::GetKVPrecisionTierForAge(int token_age) const {
    const SovereignScalingConfig cfg = GetSovereignScalingConfig();
    if (token_age <= cfg.kv_recent_tokens) {
        return KVPrecisionTier::FP16;
    }
    if (token_age <= cfg.kv_mid_tokens) {
        return KVPrecisionTier::Q6;
    }
    return KVPrecisionTier::Q4;
}

void BackendOrchestrator::UpdateThermalReadings(float cpu_temp_c, float nvme_temp_c) {
    m_cpu_temp_c.store(std::max(0.0f, cpu_temp_c), std::memory_order_relaxed);
    m_nvme_temp_c.store(std::max(0.0f, nvme_temp_c), std::memory_order_relaxed);
}

int BackendOrchestrator::GetRecommendedPrefetchDepth() const {
    const SovereignScalingConfig cfg = GetSovereignScalingConfig();
    if (!cfg.enabled) {
        return 1;
    }
    if (!cfg.thermal_aware_scheduler) {
        return cfg.prefetch_depth;
    }

    const float thermal_peak = std::max(
        m_cpu_temp_c.load(std::memory_order_relaxed),
        m_nvme_temp_c.load(std::memory_order_relaxed));

    if (thermal_peak >= 82.0f) {
        return std::max(1, cfg.prefetch_depth - 2);
    }
    if (thermal_peak >= 74.0f) {
        return std::max(1, cfg.prefetch_depth - 1);
    }
    return cfg.prefetch_depth;
}

int BackendOrchestrator::GetRecommendedFusionWidth() const {
    const SovereignScalingConfig cfg = GetSovereignScalingConfig();
    if (!cfg.enabled || !cfg.cross_layer_fusion) {
        return 1;
    }
    if (!cfg.thermal_aware_scheduler) {
        return cfg.fusion_width;
    }

    const float thermal_peak = std::max(
        m_cpu_temp_c.load(std::memory_order_relaxed),
        m_nvme_temp_c.load(std::memory_order_relaxed));

    if (thermal_peak >= 82.0f) {
        return std::max(1, cfg.fusion_width - 1);
    }
    return cfg.fusion_width;
}

} // namespace RawrXD
