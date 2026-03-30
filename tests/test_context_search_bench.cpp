#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "agentic/context_search_avx512.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

std::uint64_t wall_time_ns() {
#ifdef _WIN32
    FILETIME ft {};
    using PreciseNowFn = VOID(WINAPI*)(LPFILETIME);
    static PreciseNowFn preciseNow = []() -> PreciseNowFn {
        const HMODULE k32 = ::GetModuleHandleA("kernel32.dll");
        if (!k32) {
            return nullptr;
        }
        return reinterpret_cast<PreciseNowFn>(::GetProcAddress(k32, "GetSystemTimePreciseAsFileTime"));
    }();

    if (preciseNow) {
        preciseNow(&ft);
    } else {
        ::GetSystemTimeAsFileTime(&ft);
    }

    ULARGE_INTEGER ticks {};
    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    // FILETIME tick = 100 ns
    return static_cast<std::uint64_t>(ticks.QuadPart) * 100ull;
#else
    return 0;
#endif
}

std::vector<std::uint32_t> scalar_scan_offsets(const std::uint8_t* buffer,
                                               std::size_t len,
                                               const std::uint64_t* symbols,
                                               std::size_t symbolCount,
                                               std::size_t maxMatches,
                                               std::size_t strideBytes) {
    std::vector<std::uint32_t> out;
    if (!buffer || len < 64 || !symbols || symbolCount == 0 || maxMatches == 0 || strideBytes == 0) {
        return out;
    }

    const std::size_t lastStart = len - 64;
    for (std::size_t off = 0; off <= lastStart && out.size() < maxMatches; off += strideBytes) {
        const auto* lanes = reinterpret_cast<const std::uint64_t*>(buffer + off);
        bool hit = false;
        for (std::size_t lane = 0; lane < 8 && !hit; ++lane) {
            const std::uint64_t candidate = lanes[lane];
            // Kernel uses lane-wise compares: each lane only matches symbols
            // in the same lane slot across zmm8-zmm15.
            for (std::size_t group = 0; group < 8; ++group) {
                const std::size_t s = lane + (group * 8);
                if (s >= symbolCount) {
                    continue;
                }
                if (candidate == symbols[s]) {
                    hit = true;
                    break;
                }
            }
        }
        if (hit) {
            out.push_back(static_cast<std::uint32_t>(off));
        }
    }

    return out;
}

bool verify_vector_integrity(const std::uint8_t* buffer,
                             std::size_t len,
                             const std::uint64_t* symbols,
                             std::size_t symbolCount,
                             const std::uint32_t* vectorMatches,
                             std::size_t vectorCount,
                             std::size_t maxMatches,
                             std::size_t strideBytes) {
    const auto scalarMatches = scalar_scan_offsets(buffer, len, symbols, symbolCount, maxMatches, strideBytes);
    if (scalarMatches.size() != vectorCount) {
        return false;
    }
    for (std::size_t i = 0; i < vectorCount; ++i) {
        if (vectorMatches[i] != scalarMatches[i]) {
            return false;
        }
    }
    return true;
}

#ifdef _WIN32
using PrefetchVirtualMemoryFn = BOOL(WINAPI*)(HANDLE, ULONG_PTR, PWIN32_MEMORY_RANGE_ENTRY, ULONG);

bool prefetch_mapped_view(const void* base, std::size_t length) {
    if (!base || length == 0) {
        return false;
    }

    const HMODULE kernel32 = ::GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return false;
    }

    const auto fn = reinterpret_cast<PrefetchVirtualMemoryFn>(
        ::GetProcAddress(kernel32, "PrefetchVirtualMemory"));
    if (!fn) {
        return false;
    }

    WIN32_MEMORY_RANGE_ENTRY range {};
    range.VirtualAddress = const_cast<void*>(base);
    range.NumberOfBytes = length;

    return fn(::GetCurrentProcess(), 1, &range, 0) != FALSE;
}
#else
bool prefetch_mapped_view(const void*, std::size_t) {
    return false;
}
#endif

}  // namespace

int main() {
    using clock = std::chrono::steady_clock;
    using namespace rawrxd::agentic::context_search;

    const std::string tempPath = "D:/rxdn/tests/context_search_bench_input.bin";

    constexpr std::size_t qwordsPerChunk = 8;
    constexpr std::size_t chunkCount = 4096;
    constexpr std::size_t qwordCount = qwordsPerChunk * chunkCount;
    std::vector<std::uint64_t> payload(qwordCount, 0x1111111111111111ull);

    const std::uint64_t sym0 = hash_symbol_rawrxd64("executeStep");
    const std::uint64_t sym1 = hash_symbol_rawrxd64("runLoop");
    const std::uint64_t sym2 = hash_symbol_rawrxd64("AgenticExecutor");

    for (std::size_t c = 0; c < chunkCount; c += 257) {
        payload[(c * qwordsPerChunk) + 0] = sym0;
        payload[(c * qwordsPerChunk) + 1] = sym1;
    }
    for (std::size_t c = 13; c < chunkCount; c += 509) {
        payload[(c * qwordsPerChunk) + 2] = sym2;
    }

    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "Failed to create benchmark input file" << std::endl;
            return 1;
        }
        out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size() * sizeof(std::uint64_t)));
    }

    std::array<std::uint64_t, 64> symbols {};
    symbols[0] = sym0;
    symbols[1] = sym1;
    symbols[2] = sym2;

    if (!context_search_init(symbols.data(), symbols.size())) {
        std::cerr << "context_search_init failed" << std::endl;
        return 1;
    }

    MappedFileView view {};
    if (!context_map_file(tempPath.c_str(), view)) {
        std::cerr << "context_map_file failed" << std::endl;
        return 1;
    }

    constexpr std::size_t maxMatches = 8192;
    std::vector<std::uint32_t> matches(maxMatches, 0);

    // Warm-up mapping and first run.
    const std::size_t warmCount = context_search_scan(view.data, view.length, matches.data(), matches.size());
    if (warmCount == 0) {
        context_unmap(view);
        std::cerr << "No matches found in warm-up run" << std::endl;
        return 1;
    }

    const bool scalarPass = verify_vector_integrity(view.data,
                                                    view.length,
                                                    symbols.data(),
                                                    64,
                                                    matches.data(),
                                                    warmCount,
                                                    matches.size(),
                                                    8);

    constexpr int hotIters = 200;
    std::uint64_t hotAccumMatches = 0;
    const std::size_t mappedLength = view.length;
    const double bytesPerScan = static_cast<double>(mappedLength);

    (void)prefetch_mapped_view(view.data, view.length);

    const std::uint64_t hotT0 = wall_time_ns();
    for (int i = 0; i < hotIters; ++i) {
        hotAccumMatches += context_search_scan(view.data, view.length, matches.data(), matches.size());
    }
    const std::uint64_t hotT1 = wall_time_ns();

    const std::uint64_t hotTotalNsU = (hotT1 >= hotT0) ? (hotT1 - hotT0) : 0;

    context_unmap(view);

    // Cold path: remap each iteration without prefetch hint.
    constexpr int coldIters = 48;
    std::uint64_t coldAccumMatches = 0;
    const std::uint64_t coldT0 = wall_time_ns();
    for (int i = 0; i < coldIters; ++i) {
        MappedFileView coldView {};
        if (!context_map_file(tempPath.c_str(), coldView)) {
            std::cerr << "cold context_map_file failed" << std::endl;
            return 1;
        }
        coldAccumMatches += context_search_scan(coldView.data, coldView.length, matches.data(), matches.size());
        context_unmap(coldView);
    }
    const std::uint64_t coldT1 = wall_time_ns();

    const std::uint64_t coldTotalNsU = (coldT1 >= coldT0) ? (coldT1 - coldT0) : 0;

    const std::size_t matchesLastIter = context_search_scan(payload.data(), payload.size() * sizeof(std::uint64_t),
                                                            matches.data(), matches.size());

    const double hotNsPerIter = (hotIters > 0) ? (static_cast<double>(hotTotalNsU) / static_cast<double>(hotIters)) : 0.0;
    const double hotUsPerIter = hotNsPerIter / 1000.0;
    const double hotGbPerSec = (hotNsPerIter > 0.0)
        ? (bytesPerScan / (hotNsPerIter * 1e-9)) / 1e9
        : 0.0;

    const double coldNsPerIter = (coldIters > 0) ? (static_cast<double>(coldTotalNsU) / static_cast<double>(coldIters)) : 0.0;
    const double coldUsPerIter = coldNsPerIter / 1000.0;
    const double coldGbPerSec = (coldNsPerIter > 0.0)
        ? (bytesPerScan / (coldNsPerIter * 1e-9)) / 1e9
        : 0.0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "context_search_kernel_linked=true\n";
    std::cout << "stride_bytes=8\n";
    std::cout << "chunks_scanned_per_iter=" << (mappedLength / 64) << "\n";
    std::cout << "matches_last_iter=" << matchesLastIter << "\n";
    std::cout << "scalar_ab_pass=" << (scalarPass ? "true" : "false") << "\n";
    std::cout << "hot_iterations=" << hotIters << "\n";
    std::cout << "hot_matches_accum=" << hotAccumMatches << "\n";
    std::cout << "hot_total_ns=" << hotTotalNsU << "\n";
    std::cout << "hot_ns_per_iteration=" << hotNsPerIter << "\n";
    std::cout << "hot_us_per_iteration=" << hotUsPerIter << "\n";
    std::cout << "hot_gbps=" << hotGbPerSec << "\n";
    std::cout << "cold_iterations=" << coldIters << "\n";
    std::cout << "cold_matches_accum=" << coldAccumMatches << "\n";
    std::cout << "cold_total_ns=" << coldTotalNsU << "\n";
    std::cout << "cold_ns_per_iteration=" << coldNsPerIter << "\n";
    std::cout << "cold_us_per_iteration=" << coldUsPerIter << "\n";
    std::cout << "cold_gbps=" << coldGbPerSec << "\n";

    return 0;
}
