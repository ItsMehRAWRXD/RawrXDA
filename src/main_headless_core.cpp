// RawrEngine Lane B: Headless minimal entry point.
// Goal: keep the 274TB streamer/loader core linkable without GUI/hotpatch/omega subsystems.

#include "gguf_loader.h"
#include "rawrxd_model_loader.h"

#include <psapi.h>
#include <windows.h>

#include <memoryapi.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <vector>

extern "C" unsigned __int64 RawrXD_EnableSeLockMemoryPrivilege();
extern "C" unsigned int rawr_cpu_has_avx512();
extern "C" void* RawrXD_MapModelView2MB(HANDLE hMap, uint64_t off, size_t sz, uint64_t* outBaseOrError);
extern "C" void RawrXD_StreamToGPU_AVX512(void* dst, const void* src, unsigned long long blocks64B);

static std::wstring widenUtf8(const char* s)
{
    if (!s)
        return std::wstring();
    const int needed = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (needed <= 0)
        return std::wstring();
    std::wstring out;
    // Allocate including NUL terminator; use &out[0] for mutable storage across standard libs.
    out.resize(static_cast<size_t>(needed));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], needed);
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

static const char* getOptValue(const std::vector<std::string>& args, const char* opt)
{
    for (size_t i = 0; i + 1 < args.size(); ++i)
    {
        if (args[i] == opt)
            return args[i + 1].c_str();
    }
    return nullptr;
}

static int printUsage()
{
    // Minimal and deterministic; no SSOT/CLI layers in Lane B.
    const char* msg =
        "RawrEngine (LaneB headless)\n"
        "Usage:\n"
        "  RawrEngine.exe --compile-only\n"
        "  RawrEngine.exe --gguf-header <path>\n"
        "  RawrEngine.exe --load-model <path>\n"
        "  RawrEngine.exe --streamer-smoke <path> [--offset <u64>] [--size <u64>] [--iterations <N>]\n"
        "  RawrEngine.exe --bench-streamer <path> [--max-mb <N>] [--iters <N>] [--largepages 0|1] [--prefetch 0|1]\n"
        "  RawrEngine.exe --offset-sweep <path> [--window-mb <N>] [--iters <N>] [--seed <u64>] [--largepages 0|1] "
        "[--prefetch 0|1]\n"
        "  RawrEngine.exe --offset-sweep-loader <path> [--window-mb <N>] [--iters <N>] [--seed <u64>] [--prefetch "
        "0|1]\n";
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &written, nullptr);
    return 2;
}

static uint64_t qpcNow();
static double qpcToSeconds(uint64_t delta);
static size_t alignUp(size_t v, size_t a);
static uint64_t xorshift64(uint64_t& s);

static int offsetSweepLoader(const std::wstring& wPath, uint32_t windowMb, uint32_t iters, uint64_t seed, bool prefetch)
{
    if (windowMb < 1)
        windowMb = 1;
    if (iters < 1)
        iters = 1;
    if (seed == 0)
        seed = 0xC0FFEE1234ULL;

    RawrXDModelLoader loader;
    loader.SetPrefetchEnabled(prefetch);
    if (!loader.Load(wPath.c_str(), VK_NULL_HANDLE, VK_NULL_HANDLE))
        return 1;

    const uint64_t fileSize = loader.GetFileSizeBytes();
    if (fileSize == 0)
        return 1;

    const size_t requested = static_cast<size_t>(windowMb) * 1024u * 1024u;
    size_t mapSize = alignUp(requested, 2u * 1024u * 1024u);
    if (mapSize == 0)
        return 1;
    if (mapSize > fileSize)
        mapSize = alignUp(requested, 64u * 1024u);
    if (mapSize == 0 || mapSize > fileSize)
        return 2;

    const uint64_t maxOffset = fileSize - static_cast<uint64_t>(mapSize);
    uint64_t rng = seed;
    volatile uint64_t sink = 0;

    uint64_t mapTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t mapTicksMax = 0;
    uint64_t mapTicksSum = 0;

    uint64_t hintTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t hintTicksMax = 0;
    uint64_t hintTicksSum = 0;
    uint32_t hintOkCount = 0;

    uint64_t touchTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t touchTicksMax = 0;
    uint64_t touchTicksSum = 0;

    for (uint32_t i = 0; i < iters; ++i)
    {
        const uint64_t r = xorshift64(rng);
        const uint64_t off = (maxOffset ? (r % (maxOffset + 1ull)) : 0ull);

        const uint64_t t0 = qpcNow();
        void* src = loader.MapWindow(off, mapSize);
        const uint64_t t1 = qpcNow();
        if (!src)
            return 1;

        const uint64_t mapTicks = (t1 - t0);
        mapTicksMin = (mapTicks < mapTicksMin) ? mapTicks : mapTicksMin;
        mapTicksMax = (mapTicks > mapTicksMax) ? mapTicks : mapTicksMax;
        mapTicksSum += mapTicks;

        if (prefetch)
        {
            // Hint a small lookahead inside this mapping window when possible.
            // We use "off + 64KB" as a deterministic, cheap proxy for "tensor N+1".
            const uint64_t hintOff = off + 64ull * 1024ull;
            const uint64_t h0 = qpcNow();
            const bool ok = loader.HintRange(hintOff, 256ull * 1024ull);
            const uint64_t h1 = qpcNow();
            const uint64_t hintTicks = (h1 - h0);
            hintTicksMin = (hintTicks < hintTicksMin) ? hintTicks : hintTicksMin;
            hintTicksMax = (hintTicks > hintTicksMax) ? hintTicks : hintTicksMax;
            hintTicksSum += hintTicks;
            hintOkCount += ok ? 1u : 0u;
        }

        // First-byte latency proxy.
        const uint64_t u0 = qpcNow();
        const volatile uint64_t* p64 = reinterpret_cast<const volatile uint64_t*>(src);
        sink ^= p64[0];
        sink ^= p64[1];
        sink ^= p64[2];
        sink ^= p64[3];
        sink ^= p64[4];
        sink ^= p64[5];
        sink ^= p64[6];
        sink ^= p64[7];
        const uint64_t u1 = qpcNow();

        const uint64_t touchTicks = (u1 - u0);
        touchTicksMin = (touchTicks < touchTicksMin) ? touchTicks : touchTicksMin;
        touchTicksMax = (touchTicks > touchTicksMax) ? touchTicks : touchTicksMax;
        touchTicksSum += touchTicks;

        loader.UnmapWindow();
    }

    const double mapAvgUs = (iters > 0) ? (qpcToSeconds(mapTicksSum / iters) * 1e6) : 0.0;
    const double mapMinUs = qpcToSeconds(mapTicksMin) * 1e6;
    const double mapMaxUs = qpcToSeconds(mapTicksMax) * 1e6;

    const double hintAvgUs = (prefetch && iters > 0) ? (qpcToSeconds(hintTicksSum / iters) * 1e6) : 0.0;
    const double hintMinUs = prefetch ? (qpcToSeconds(hintTicksMin) * 1e6) : 0.0;
    const double hintMaxUs = prefetch ? (qpcToSeconds(hintTicksMax) * 1e6) : 0.0;

    const double touchAvgUs = (iters > 0) ? (qpcToSeconds(touchTicksSum / iters) * 1e6) : 0.0;
    const double touchMinUs = qpcToSeconds(touchTicksMin) * 1e6;
    const double touchMaxUs = qpcToSeconds(touchTicksMax) * 1e6;

    char line[512]{};
    std::snprintf(line, sizeof(line),
                  "offset-sweep-loader: window=%uMB iters=%u seed=%llu prefetch=%u\n"
                  "  map_us:   avg=%.2f min=%.2f max=%.2f\n"
                  "  hint_us:  avg=%.2f min=%.2f max=%.2f ok=%u/%u\n"
                  "  touch_us: avg=%.2f min=%.2f max=%.2f\n"
                  "  sink=%llu\n",
                  windowMb, iters, static_cast<unsigned long long>(seed), prefetch ? 1u : 0u, mapAvgUs, mapMinUs,
                  mapMaxUs, hintAvgUs, hintMinUs, hintMaxUs, static_cast<unsigned int>(hintOkCount),
                  static_cast<unsigned int>(prefetch ? iters : 0u), touchAvgUs, touchMinUs, touchMaxUs,
                  static_cast<unsigned long long>(sink));
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
    return 0;
}

static int streamerSmoke(const std::wstring& wPath, uint64_t offset, uint64_t size, uint32_t iterations)
{
    if (iterations < 1)
        iterations = 1;
    if (size == 0)
        size = 4096;

    RawrXDModelLoader loader;
    if (!loader.Load(wPath.c_str(), VK_NULL_HANDLE, VK_NULL_HANDLE))
        return 1;

    const uint64_t fileSize = loader.GetFileSizeBytes();
    if (fileSize == 0)
        return 1;

    for (uint32_t i = 0; i < iterations; ++i)
    {
        const uint64_t farOffset = (fileSize > (size + 4096)) ? (fileSize - size - 4096) : 0;
        const uint64_t chosenOffset = (i & 1u) ? farOffset : offset;
        void* p = loader.MapWindow(chosenOffset, static_cast<size_t>(size));
        if (!p)
            return 1;

        if (loader.UsingLargePages())
        {
            const uintptr_t base = reinterpret_cast<uintptr_t>(loader.GetCurrentViewBase());
            if ((base & 0x1FFFFFull) != 0)
                return 1;
        }

        loader.UnmapWindow();
    }

    return 0;
}

struct AlignedVirtualAlloc
{
    void* base = nullptr;     // address returned by VirtualAlloc
    void* aligned = nullptr;  // aligned view inside base
    size_t reserveBytes = 0;
    size_t usableBytes = 0;
};

static AlignedVirtualAlloc allocAlignedVirtual(size_t bytes, size_t alignment)
{
    AlignedVirtualAlloc a{};
    if (bytes == 0)
        return a;
    if (alignment < 16)
        alignment = 16;

    const size_t total = bytes + alignment;
    void* p = VirtualAlloc(nullptr, total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p)
        return a;

    const uintptr_t u = reinterpret_cast<uintptr_t>(p);
    const uintptr_t alignedU = (u + (alignment - 1)) & ~(static_cast<uintptr_t>(alignment - 1));
    a.base = p;
    a.aligned = reinterpret_cast<void*>(alignedU);
    a.reserveBytes = total;
    a.usableBytes = bytes;
    return a;
}

static void freeAlignedVirtual(AlignedVirtualAlloc& a)
{
    if (a.base)
        VirtualFree(a.base, 0, MEM_RELEASE);
    a = {};
}

static uint64_t qpcNow()
{
    LARGE_INTEGER v{};
    QueryPerformanceCounter(&v);
    return static_cast<uint64_t>(v.QuadPart);
}

static uint64_t qpcFreq()
{
    static uint64_t f = []()
    {
        LARGE_INTEGER v{};
        QueryPerformanceFrequency(&v);
        return static_cast<uint64_t>(v.QuadPart);
    }();
    return f;
}

static double qpcToSeconds(uint64_t delta)
{
    const uint64_t f = qpcFreq();
    return f ? (static_cast<double>(delta) / static_cast<double>(f)) : 0.0;
}

static size_t alignUp(size_t v, size_t a)
{
    return (v + (a - 1)) & ~(a - 1);
}

typedef BOOL(WINAPI* PrefetchVirtualMemoryFn)(HANDLE, ULONG_PTR, PWIN32_MEMORY_RANGE_ENTRY, ULONG);

static PrefetchVirtualMemoryFn getPrefetchVirtualMemoryFn()
{
    static PrefetchVirtualMemoryFn fn = []() -> PrefetchVirtualMemoryFn
    {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        if (!k32)
            return nullptr;
        return reinterpret_cast<PrefetchVirtualMemoryFn>(GetProcAddress(k32, "PrefetchVirtualMemory"));
    }();
    return fn;
}

static bool tryPrefetchRange(void* addr, size_t bytes)
{
    auto fn = getPrefetchVirtualMemoryFn();
    if (!fn || !addr || bytes == 0)
        return false;

    WIN32_MEMORY_RANGE_ENTRY entry{};
    entry.VirtualAddress = addr;
    entry.NumberOfBytes = bytes;
    return fn(GetCurrentProcess(), 1, &entry, 0) ? true : false;
}

static uint64_t xorshift64(uint64_t& s)
{
    // Deterministic RNG (no <random> overhead in Lane B).
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
}

static uint64_t getPrivateBytes()
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        return 0;
    return static_cast<uint64_t>(pmc.PrivateUsage);
}

static uint64_t getWorkingSetBytes()
{
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return 0;
    return static_cast<uint64_t>(pmc.WorkingSetSize);
}

static uint64_t getPageFaultCount()
{
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return 0;
    return static_cast<uint64_t>(pmc.PageFaultCount);
}

static IO_COUNTERS getIoCounters()
{
    IO_COUNTERS io{};
    (void)GetProcessIoCounters(GetCurrentProcess(), &io);
    return io;
}

static bool readFileMagicU32(const std::wstring& wPath, uint32_t& out)
{
    out = 0;
    HANDLE h = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD read = 0;
    const BOOL ok = ReadFile(h, &out, sizeof(out), &read, nullptr);
    CloseHandle(h);
    return ok && read == sizeof(out);
}

static int benchStreamer(const std::wstring& wPath, uint32_t maxMb, uint32_t iters, bool tryLargePages, bool prefetch)
{
    if (maxMb < 1)
        maxMb = 1;
    if (iters < 1)
        iters = 1;

    HANDLE hFile = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return 1;

    LARGE_INTEGER fileSizeLi{};
    if (!GetFileSizeEx(hFile, &fileSizeLi) || fileSizeLi.QuadPart <= 0)
    {
        CloseHandle(hFile);
        return 1;
    }
    const uint64_t fileSize = static_cast<uint64_t>(fileSizeLi.QuadPart);

    DWORD protect = PAGE_READONLY;
    DWORD mapFlags = 0;
    if (tryLargePages)
    {
        const bool privOk = (RawrXD_EnableSeLockMemoryPrivilege() == 0);
        if (!privOk)
        {
            static std::once_flag once;
            std::call_once(
                once,
                []()
                {
                    const char* msg = "bench-streamer: large pages unavailable; using standard page mapping\n";
                    DWORD w = 0;
                    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
                });
        }
        if (privOk)
            mapFlags |= SEC_LARGE_PAGES;
    }

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, protect | mapFlags, 0, 0, nullptr);
    if (!hMap && (mapFlags & SEC_LARGE_PAGES))
    {
        // Fall back gracefully when large-page mapping is unavailable.
        hMap = CreateFileMappingW(hFile, nullptr, protect, 0, 0, nullptr);
    }
    if (!hMap)
    {
        CloseHandle(hFile);
        return 1;
    }

    const bool hasAvx512 = (rawr_cpu_has_avx512() != 0);
    const uint32_t mbStart = 1;
    const uint32_t mbEnd = maxMb;
    const uint64_t freq = qpcFreq();

    // Print header.
    char header[320]{};
    std::snprintf(
        header, sizeof(header),
        "bench-streamer: file=%llu bytes  avx512=%u  iters=%u  maxMb=%u  qpcHz=%llu  largepages=%u  prefetch=%u\n",
        static_cast<unsigned long long>(fileSize), hasAvx512 ? 1u : 0u, iters, maxMb,
        static_cast<unsigned long long>(freq), tryLargePages ? 1u : 0u, prefetch ? 1u : 0u);
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), header, static_cast<DWORD>(std::strlen(header)), &written, nullptr);

    bool printedAny = false;
    for (uint32_t mb = mbStart; mb <= mbEnd; mb = (mb < 1024 ? mb * 2 : mb + 1024))
    {
        const size_t requested = static_cast<size_t>(mb) * 1024u * 1024u;
        // Prefer 2MB-aligned mapping sizes for the huge-page/TLB-friendly path, but
        // gracefully fall back for tiny files where a 2MB window is impossible.
        size_t mapSize = alignUp(requested, 2u * 1024u * 1024u);
        const uint64_t offset = 0;  // keep offset 0 to validate 2MB alignment easily
        bool expect2mbAligned = true;
        if (offset + mapSize > fileSize)
        {
            mapSize = alignUp(requested, 64u * 1024u);
            expect2mbAligned = false;
            if (offset + mapSize > fileSize)
                break;
        }

        uint64_t baseOrErr = 0;
        void* src = RawrXD_MapModelView2MB(hMap, offset, mapSize, &baseOrErr);
        if (!src)
            break;

        const uintptr_t srcU = reinterpret_cast<uintptr_t>(src);
        const bool src2mbAligned = ((srcU & 0x1FFFFFull) == 0);
        // Note: MapViewOfFile address alignment is not guaranteed to be 2MB even when
        // offset/size are 2MB-aligned. We record alignment instead of failing.

        // Allocate a destination buffer simulating an upload heap mapping (64B aligned view).
        AlignedVirtualAlloc dstAlloc = allocAlignedVirtual(mapSize, 64);
        if (!dstAlloc.aligned)
        {
            UnmapViewOfFile(reinterpret_cast<void*>(static_cast<uintptr_t>(baseOrErr)));
            break;
        }

        // Optional prefetch hint (best-effort). This is a VMM hint; it may be ignored.
        if (prefetch)
            (void)tryPrefetchRange(src, mapSize);

        // Sentinel integrity: stamp last 4 bytes in the source range and expect it in destination.
        constexpr uint32_t kSentinel = 0xDEADC0DEu;
        const size_t tail = mapSize - sizeof(uint32_t);
        std::memcpy(reinterpret_cast<uint8_t*>(src) + tail, &kSentinel, sizeof(uint32_t));

        // memcpy benchmark
        uint64_t t0 = qpcNow();
        for (uint32_t i = 0; i < iters; ++i)
        {
            std::memcpy(dstAlloc.aligned, src, mapSize);
        }
        uint64_t t1 = qpcNow();
        const double memcpySec = qpcToSeconds(t1 - t0);
        const double memcpyGbps =
            memcpySec > 0.0 ? (static_cast<double>(mapSize) * static_cast<double>(iters) / memcpySec / 1e9) : 0.0;

        // AVX-512 NT copy benchmark (if available)
        double ntSec = 0.0;
        double ntGbps = 0.0;
        bool ntOk = false;
        if (hasAvx512)
        {
            const unsigned long long blocks64B = static_cast<unsigned long long>(mapSize / 64);
            uint64_t a0 = qpcNow();
            for (uint32_t i = 0; i < iters; ++i)
            {
                RawrXD_StreamToGPU_AVX512(dstAlloc.aligned, src, blocks64B);
            }
            uint64_t a1 = qpcNow();
            ntSec = qpcToSeconds(a1 - a0);
            ntGbps = ntSec > 0.0 ? (static_cast<double>(mapSize) * static_cast<double>(iters) / ntSec / 1e9) : 0.0;
            ntOk = true;
        }

        // Fence/sentinel check: verify sentinel reached destination.
        uint32_t got = 0;
        std::memcpy(&got, reinterpret_cast<uint8_t*>(dstAlloc.aligned) + tail, sizeof(uint32_t));
        const bool sentinelOk = (got == kSentinel);

        char line[256]{};
        const char* alignStr = src2mbAligned ? "2MB" : "64K+";
        if (ntOk)
        {
            std::snprintf(line, sizeof(line),
                          "%4uMB  memcpy=%8.2f GB/s  nt_avx512=%8.2f GB/s  sentinel=%s  srcAlign=%s\n", mb, memcpyGbps,
                          ntGbps, sentinelOk ? "ok" : "BAD", alignStr);
        }
        else
        {
            std::snprintf(line, sizeof(line), "%4uMB  memcpy=%8.2f GB/s  nt_avx512= n/a  sentinel=%s  srcAlign=%s\n",
                          mb, memcpyGbps, sentinelOk ? "ok" : "BAD", alignStr);
        }
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
        printedAny = true;

        freeAlignedVirtual(dstAlloc);
        UnmapViewOfFile(reinterpret_cast<void*>(static_cast<uintptr_t>(baseOrErr)));

        if (!sentinelOk)
        {
            CloseHandle(hMap);
            CloseHandle(hFile);
            return 1;
        }
    }

    CloseHandle(hMap);
    CloseHandle(hFile);
    if (!printedAny)
    {
        const char* msg = "bench-streamer: file too small for requested windows\n";
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
        return 2;
    }
    return 0;
}

static int offsetSweep(const std::wstring& wPath, uint32_t windowMb, uint32_t iters, uint64_t seed, bool tryLargePages,
                       bool prefetch)
{
    if (windowMb < 1)
        windowMb = 1;
    if (iters < 1)
        iters = 1;
    if (seed == 0)
        seed = 0xC0FFEE1234ULL;

    HANDLE hFile = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "offset-sweep: CreateFileW failed (err=%lu)\n", GetLastError());
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        return 1;
    }

    LARGE_INTEGER fileSizeLi{};
    if (!GetFileSizeEx(hFile, &fileSizeLi) || fileSizeLi.QuadPart <= 0)
    {
        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "offset-sweep: GetFileSizeEx failed (err=%lu)\n", GetLastError());
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        CloseHandle(hFile);
        return 1;
    }
    const uint64_t fileSize = static_cast<uint64_t>(fileSizeLi.QuadPart);

    DWORD protect = PAGE_READONLY;
    DWORD mapFlags = 0;
    if (tryLargePages)
    {
        const bool privOk = (RawrXD_EnableSeLockMemoryPrivilege() == 0);
        if (!privOk)
        {
            static std::once_flag once;
            std::call_once(once,
                           []()
                           {
                               const char* msg = "offset-sweep: large pages unavailable; using standard page mapping\n";
                               DWORD w = 0;
                               WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w,
                                         nullptr);
                           });
        }
        if (privOk)
            mapFlags |= SEC_LARGE_PAGES;
    }

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, protect | mapFlags, 0, 0, nullptr);
    if (!hMap && (mapFlags & SEC_LARGE_PAGES))
    {
        hMap = CreateFileMappingW(hFile, nullptr, protect, 0, 0, nullptr);
    }
    if (!hMap)
    {
        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "offset-sweep: CreateFileMappingW failed (err=%lu)\n", GetLastError());
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        CloseHandle(hFile);
        return 1;
    }

    const size_t requested = static_cast<size_t>(windowMb) * 1024u * 1024u;
    size_t mapSize = alignUp(requested, 2u * 1024u * 1024u);
    bool expect2mbAligned = true;
    if (mapSize > fileSize)
    {
        mapSize = alignUp(requested, 64u * 1024u);
        expect2mbAligned = false;
    }
    if (mapSize == 0 || mapSize > fileSize)
    {
        const char* msg = "offset-sweep: file too small for requested window\n";
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 2;
    }

    const uint64_t maxOffset = fileSize - static_cast<uint64_t>(mapSize);
    const uint64_t offsetAlign = expect2mbAligned ? (2ull * 1024ull * 1024ull) : (64ull * 1024ull);
    const uint64_t alignedMaxOffset = maxOffset & ~(offsetAlign - 1ull);

    uint64_t mapTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t mapTicksMax = 0;
    uint64_t mapTicksSum = 0;

    uint64_t touchTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t touchTicksMax = 0;
    uint64_t touchTicksSum = 0;

    uint64_t prefetchTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t prefetchTicksMax = 0;
    uint64_t prefetchTicksSum = 0;
    uint32_t prefetchOkCount = 0;

    const uint64_t privBefore = getPrivateBytes();
    const uint64_t wsBefore = getWorkingSetBytes();
    const uint64_t pfBefore = getPageFaultCount();
    const IO_COUNTERS ioBefore = getIoCounters();

    uint64_t rng = seed;
    volatile uint64_t sink = 0;

    for (uint32_t i = 0; i < iters; ++i)
    {
        const uint64_t r = xorshift64(rng);
        const uint64_t offset = alignedMaxOffset ? ((r % (alignedMaxOffset + 1ull)) & ~(offsetAlign - 1ull)) : 0;

        uint64_t baseOrErr = 0;
        const uint64_t t0 = qpcNow();
        void* src = RawrXD_MapModelView2MB(hMap, offset, mapSize, &baseOrErr);
        const uint64_t t1 = qpcNow();
        if (!src)
        {
            char buf[160]{};
            std::snprintf(buf, sizeof(buf), "offset-sweep: MapModelView failed (baseOrErr=%llu)\n",
                          static_cast<unsigned long long>(baseOrErr));
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
            CloseHandle(hMap);
            CloseHandle(hFile);
            return 1;
        }

        const uint64_t mapTicks = (t1 - t0);
        mapTicksMin = (mapTicks < mapTicksMin) ? mapTicks : mapTicksMin;
        mapTicksMax = (mapTicks > mapTicksMax) ? mapTicks : mapTicksMax;
        mapTicksSum += mapTicks;

        const uintptr_t srcU2 = reinterpret_cast<uintptr_t>(src);
        const bool src2mbAligned = ((srcU2 & 0x1FFFFFull) == 0);
        (void)src2mbAligned;

        if (prefetch)
        {
            const uint64_t p0 = qpcNow();
            const bool prefOk = tryPrefetchRange(src, mapSize);
            const uint64_t p1 = qpcNow();
            const uint64_t prefTicks = (p1 - p0);
            prefetchTicksMin = (prefTicks < prefetchTicksMin) ? prefTicks : prefetchTicksMin;
            prefetchTicksMax = (prefTicks > prefetchTicksMax) ? prefTicks : prefetchTicksMax;
            prefetchTicksSum += prefTicks;
            prefetchOkCount += prefOk ? 1u : 0u;
        }

        // First-byte latency proxy: time to read the first cache line after mapping.
        const uint64_t u0 = qpcNow();
        const volatile uint64_t* p64 = reinterpret_cast<const volatile uint64_t*>(src);
        sink ^= p64[0];
        sink ^= p64[1];
        sink ^= p64[2];
        sink ^= p64[3];
        sink ^= p64[4];
        sink ^= p64[5];
        sink ^= p64[6];
        sink ^= p64[7];
        const uint64_t u1 = qpcNow();

        const uint64_t touchTicks = (u1 - u0);
        touchTicksMin = (touchTicks < touchTicksMin) ? touchTicks : touchTicksMin;
        touchTicksMax = (touchTicks > touchTicksMax) ? touchTicks : touchTicksMax;
        touchTicksSum += touchTicks;

        UnmapViewOfFile(reinterpret_cast<void*>(static_cast<uintptr_t>(baseOrErr)));
    }

    const uint64_t privAfter = getPrivateBytes();
    const uint64_t wsAfter = getWorkingSetBytes();
    const uint64_t pfAfter = getPageFaultCount();
    const IO_COUNTERS ioAfter = getIoCounters();

    const double mapAvgUs = (iters > 0) ? (qpcToSeconds(mapTicksSum / iters) * 1e6) : 0.0;
    const double mapMinUs = qpcToSeconds(mapTicksMin) * 1e6;
    const double mapMaxUs = qpcToSeconds(mapTicksMax) * 1e6;

    const double touchAvgUs = (iters > 0) ? (qpcToSeconds(touchTicksSum / iters) * 1e6) : 0.0;
    const double touchMinUs = qpcToSeconds(touchTicksMin) * 1e6;
    const double touchMaxUs = qpcToSeconds(touchTicksMax) * 1e6;

    const double prefAvgUs = (prefetch && iters > 0) ? (qpcToSeconds(prefetchTicksSum / iters) * 1e6) : 0.0;
    const double prefMinUs = prefetch ? (qpcToSeconds(prefetchTicksMin) * 1e6) : 0.0;
    const double prefMaxUs = prefetch ? (qpcToSeconds(prefetchTicksMax) * 1e6) : 0.0;

    char line[512]{};
    std::snprintf(line, sizeof(line),
                  "offset-sweep: window=%uMB iters=%u align=%lluKB seed=%llu\n"
                  "  map_us:   avg=%.2f min=%.2f max=%.2f\n"
                  "  prefetch_us: avg=%.2f min=%.2f max=%.2f ok=%u/%u\n"
                  "  touch_us: avg=%.2f min=%.2f max=%.2f\n"
                  "  vmm: pagefaults_delta=%lld  io_read_bytes_delta=%lld  io_read_ops_delta=%lld\n"
                  "  mem: private_delta=%lld bytes  ws_delta=%lld bytes  sink=%llu\n",
                  windowMb, iters, static_cast<unsigned long long>(offsetAlign / 1024ull),
                  static_cast<unsigned long long>(seed), mapAvgUs, mapMinUs, mapMaxUs, prefAvgUs, prefMinUs, prefMaxUs,
                  static_cast<unsigned int>(prefetchOkCount), static_cast<unsigned int>(prefetch ? iters : 0u),
                  touchAvgUs, touchMinUs, touchMaxUs, static_cast<long long>(pfAfter - pfBefore),
                  static_cast<long long>(ioAfter.ReadTransferCount - ioBefore.ReadTransferCount),
                  static_cast<long long>(ioAfter.ReadOperationCount - ioBefore.ReadOperationCount),
                  static_cast<long long>(privAfter - privBefore), static_cast<long long>(wsAfter - wsBefore),
                  static_cast<unsigned long long>(sink));
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);

    CloseHandle(hMap);
    CloseHandle(hFile);
    return 0;
}

static int loadModel(const std::string& path)
{
    try
    {
        // Quick magic check for diagnostics.
        const std::wstring wPath = widenUtf8(path.c_str());
        uint32_t magic = 0;
        if (readFileMagicU32(wPath, magic))
        {
            char m[96]{};
            std::snprintf(m, sizeof(m), "load-model: magic=0x%08X\n", magic);
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), m, static_cast<DWORD>(std::strlen(m)), &w, nullptr);
        }

        GGUFLoader loader;
        if (!loader.Open(path))
        {
            const char* msg = "load-model: Open failed\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }
        if (!loader.ParseHeader())
        {
            const char* msg = "load-model: ParseHeader failed\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }
        if (!loader.ParseMetadata())
        {
            const char* msg = "load-model: ParseMetadata failed\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }

        const uint64_t sz = loader.GetFileSize();
        const auto tensors = loader.GetTensorInfo();
        if (sz == 0 || tensors.empty())
        {
            const char* msg = "load-model: parsed, but tensors empty (or size=0)\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }

        char buf[256]{};
        std::snprintf(buf, sizeof(buf), "load-model: ok  bytes=%llu  tensors=%llu\n",
                      static_cast<unsigned long long>(sz), static_cast<unsigned long long>(tensors.size()));
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        return 0;
    }
    catch (const std::exception& e)
    {
        char buf[512]{};
        std::snprintf(buf, sizeof(buf), "load-model: exception: %s\n", e.what());
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        return 1;
    }
    catch (...)
    {
        const char* msg = "load-model: exception\n";
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
        return 1;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return printUsage();

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
        args.emplace_back(argv[i] ? argv[i] : "");

    std::string arg1 = args.size() > 1 ? args[1] : "";
    if (arg1 == "--help" || arg1 == "-h")
        return printUsage();

    // Minimal sanity lane:
    // RawrEngine.exe --gguf-header <path>
    if (arg1 == "--gguf-header")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        try
        {
            GGUFLoader loader;
            if (!loader.Open(args[2].c_str()))
            {
                const char* msg = "gguf-header: Open failed\n";
                DWORD w = 0;
                WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
                return 1;
            }
            if (!loader.ParseHeader())
            {
                const char* msg = "gguf-header: ParseHeader failed\n";
                DWORD w = 0;
                WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
                return 1;
            }
            return 0;
        }
        catch (const std::exception& e)
        {
            char buf[512]{};
            std::snprintf(buf, sizeof(buf), "gguf-header: exception: %s\n", e.what());
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
            return 1;
        }
        catch (...)
        {
            const char* msg = "gguf-header: exception\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }
    }

    // Headless model load: parse header + metadata + tensor table.
    // RawrEngine.exe --load-model <path>
    if (arg1 == "--load-model")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();
        return loadModel(args[2]);
    }

    // Minimal loader compilation coverage:
    // RawrEngine.exe --compile-only
    if (arg1 == "--compile-only")
    {
        // Ensure linker keeps RawrXDModelLoader reachable in this lane.
        RawrXDModelLoader dummy;
        (void)dummy;
        return 0;
    }

    // Natural habitat test for the loader/streamer mapping path:
    // RawrEngine.exe --streamer-smoke <path> [--offset <u64>] [--size <u64>] [--iterations <N>]
    if (arg1 == "--streamer-smoke")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        const std::wstring wPath = widenUtf8(args[2].c_str());
        if (wPath.empty())
            return 1;

        const char* offsetStr = getOptValue(args, "--offset");
        const char* sizeStr = getOptValue(args, "--size");
        const char* itStr = getOptValue(args, "--iterations");

        uint64_t offset = offsetStr ? static_cast<uint64_t>(std::strtoull(offsetStr, nullptr, 10)) : 0;
        uint64_t size = sizeStr ? static_cast<uint64_t>(std::strtoull(sizeStr, nullptr, 10)) : 4096;
        uint32_t iterations = itStr ? static_cast<uint32_t>(std::strtoul(itStr, nullptr, 10)) : 8;
        return streamerSmoke(wPath, offset, size, iterations);
    }

    // Throughput benchmark for the MASM streamer:
    // RawrEngine.exe --bench-streamer <path> [--max-mb <N>] [--iters <N>] [--largepages 0|1]
    if (arg1 == "--bench-streamer")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        const std::wstring wPath = widenUtf8(args[2].c_str());
        if (wPath.empty())
            return 1;

        const char* maxMbStr = getOptValue(args, "--max-mb");
        const char* itStr = getOptValue(args, "--iters");
        const char* lpStr = getOptValue(args, "--largepages");
        const char* pfStr = getOptValue(args, "--prefetch");
        uint32_t maxMb = maxMbStr ? static_cast<uint32_t>(std::strtoul(maxMbStr, nullptr, 10)) : 1024;
        uint32_t iters = itStr ? static_cast<uint32_t>(std::strtoul(itStr, nullptr, 10)) : 8;
        bool largePages = lpStr ? (std::strtoul(lpStr, nullptr, 10) != 0) : true;
        bool prefetch = pfStr ? (std::strtoul(pfStr, nullptr, 10) != 0) : false;
        return benchStreamer(wPath, maxMb, iters, largePages, prefetch);
    }

    // Sliding-window churn benchmark (randomized offset sweep):
    // RawrEngine.exe --offset-sweep <path> [--window-mb <N>] [--iters <N>] [--seed <u64>] [--largepages 0|1]
    if (arg1 == "--offset-sweep")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        {
            const char* msg = "offset-sweep: start\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
        }

        const std::wstring wPath = widenUtf8(args[2].c_str());
        if (wPath.empty())
        {
            const char* msg = "offset-sweep: invalid path encoding\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }

        const char* winMbStr = getOptValue(args, "--window-mb");
        const char* itStr = getOptValue(args, "--iters");
        const char* seedStr = getOptValue(args, "--seed");
        const char* lpStr = getOptValue(args, "--largepages");
        const char* pfStr = getOptValue(args, "--prefetch");

        uint32_t windowMb = winMbStr ? static_cast<uint32_t>(std::strtoul(winMbStr, nullptr, 10)) : 64;
        uint32_t iters = itStr ? static_cast<uint32_t>(std::strtoul(itStr, nullptr, 10)) : 1000;
        uint64_t seed = seedStr ? static_cast<uint64_t>(std::strtoull(seedStr, nullptr, 10)) : 0xC0FFEE1234ULL;
        bool largePages = lpStr ? (std::strtoul(lpStr, nullptr, 10) != 0) : true;
        bool prefetch = pfStr ? (std::strtoul(pfStr, nullptr, 10) != 0) : false;
        return offsetSweep(wPath, windowMb, iters, seed, largePages, prefetch);
    }

    // Same benchmark, but routed through RawrXDModelLoader (the shared core path used by Win32IDE).
    // RawrEngine.exe --offset-sweep-loader <path> [--window-mb <N>] [--iters <N>] [--seed <u64>] [--prefetch 0|1]
    if (arg1 == "--offset-sweep-loader")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        const std::wstring wPath = widenUtf8(args[2].c_str());
        if (wPath.empty())
            return 1;

        const char* winMbStr = getOptValue(args, "--window-mb");
        const char* itStr = getOptValue(args, "--iters");
        const char* seedStr = getOptValue(args, "--seed");
        const char* pfStr = getOptValue(args, "--prefetch");

        uint32_t windowMb = winMbStr ? static_cast<uint32_t>(std::strtoul(winMbStr, nullptr, 10)) : 64;
        uint32_t iters = itStr ? static_cast<uint32_t>(std::strtoul(itStr, nullptr, 10)) : 1000;
        uint64_t seed = seedStr ? static_cast<uint64_t>(std::strtoull(seedStr, nullptr, 10)) : 0xC0FFEE1234ULL;
        bool prefetch = pfStr ? (std::strtoul(pfStr, nullptr, 10) != 0) : false;
        return offsetSweepLoader(wPath, windowMb, iters, seed, prefetch);
    }

    return printUsage();
}
