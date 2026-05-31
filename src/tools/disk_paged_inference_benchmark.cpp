// ============================================================================
// disk_paged_inference_benchmark.cpp
//
// Disk-first throughput benchmark for paged large-model inference.
//
// Measures:
//   - Sequential read throughput (GB/s)      — single-threaded buffered I/O
//   - Random windowed read throughput (GB/s) — single-threaded buffered I/O
//   - Prefetch pipeline throughput (GB/s)    — overlapped async I/O, N in-flight
//   - Estimated tokens/sec from a configurable bytes-per-token model
//
// Machine-readable output line:
//   RAWRXD_DISK_PAGED_JSON={...}
// ============================================================================

#include <windows.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Physical sector size assumed for FILE_FLAG_NO_BUFFERING alignment.
static constexpr uint64_t kSectorSize = 4096;

inline uint64_t alignUp(uint64_t v, uint64_t align) {
    return (v + align - 1) & ~(align - 1);
}

struct Args {
    std::string modelPath;
    uint64_t windowBytes = 64ull * 1024ull * 1024ull;               // 64 MB
    uint64_t sequentialBytes = 8ull * 1024ull * 1024ull * 1024ull;  // 8 GB cap
    uint64_t randomOps = 256;
    uint64_t bytesPerToken = 1100ull * 1024ull * 1024ull * 1024ull; // 2T Q4 pessimistic
    uint32_t seed = 1337;
    uint32_t prefetchDepth = 4;   // overlapped in-flight reads
    bool noBuffering = false;     // FILE_FLAG_NO_BUFFERING (bypass OS cache)
    bool skipSequential = false;
    bool skipRandom = false;
    bool skipPrefetch = false;
};

struct Result {
    bool success = false;
    std::string error;
    uint64_t fileBytes = 0;
    uint64_t sequentialBytesRead = 0;
    uint64_t randomBytesRead = 0;
    uint64_t prefetchBytesRead = 0;
    double sequentialGbps = 0.0;
    double randomGbps = 0.0;
    double prefetchGbps = 0.0;      // overlapped pipelined read bandwidth
    double estimatedSeqTps = 0.0;
    double estimatedRndTps = 0.0;
    double estimatedPrefetchTps = 0.0;
    double wallMs = 0.0;
};

double bytesToGB(uint64_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[7]{};
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
            break;
        }
    }
    return out;
}

std::string toJson(const Args& a, const Result& r) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"success\":" << (r.success ? "true" : "false") << ",";
    oss << "\"model_path\":\"" << jsonEscape(a.modelPath) << "\",";
    oss << "\"file_bytes\":" << r.fileBytes << ",";
    oss << "\"window_bytes\":" << a.windowBytes << ",";
    oss << "\"sequential_bytes_target\":" << a.sequentialBytes << ",";
    oss << "\"sequential_bytes_read\":" << r.sequentialBytesRead << ",";
    oss << "\"random_ops\":" << a.randomOps << ",";
    oss << "\"random_bytes_read\":" << r.randomBytesRead << ",";
    oss << "\"prefetch_depth\":" << a.prefetchDepth << ",";
    oss << "\"prefetch_bytes_read\":" << r.prefetchBytesRead << ",";
    oss << "\"no_buffering\":" << (a.noBuffering ? "true" : "false") << ",";
    oss << "\"bytes_per_token\":" << a.bytesPerToken << ",";
    oss << "\"sequential_gbps\":" << std::fixed << std::setprecision(6) << r.sequentialGbps << ",";
    oss << "\"random_gbps\":" << std::fixed << std::setprecision(6) << r.randomGbps << ",";
    oss << "\"prefetch_gbps\":" << std::fixed << std::setprecision(6) << r.prefetchGbps << ",";
    oss << "\"estimated_seq_tps\":" << std::fixed << std::setprecision(6) << r.estimatedSeqTps << ",";
    oss << "\"estimated_rnd_tps\":" << std::fixed << std::setprecision(6) << r.estimatedRndTps << ",";
    oss << "\"estimated_prefetch_tps\":" << std::fixed << std::setprecision(6) << r.estimatedPrefetchTps << ",";
    oss << "\"wall_ms\":" << std::fixed << std::setprecision(3) << r.wallMs;
    if (!r.error.empty()) {
        oss << ",\"error\":\"" << jsonEscape(r.error) << "\"";
    }
    oss << "}";
    return oss.str();
}

bool parseArgs(int argc, char** argv, Args& out, std::string& err) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto readU64 = [&](uint64_t& dst) {
            if (i + 1 >= argc) {
                err = "Missing value for " + a;
                return false;
            }
            try {
                dst = std::stoull(argv[++i]);
                return true;
            } catch (...) {
                err = "Invalid integer for " + a;
                return false;
            }
        };

        if (a == "--model") {
            if (i + 1 >= argc) {
                err = "Missing value for --model";
                return false;
            }
            out.modelPath = argv[++i];
        } else if (a == "--window-bytes") {
            if (!readU64(out.windowBytes)) return false;
        } else if (a == "--sequential-bytes") {
            if (!readU64(out.sequentialBytes)) return false;
        } else if (a == "--random-ops") {
            if (!readU64(out.randomOps)) return false;
        } else if (a == "--bytes-per-token") {
            if (!readU64(out.bytesPerToken)) return false;
        } else if (a == "--seed") {
            uint64_t tmp = 0;
            if (!readU64(tmp)) return false;
            out.seed = static_cast<uint32_t>(tmp);
        } else if (a == "--prefetch-depth") {
            uint64_t tmp = 0;
            if (!readU64(tmp)) return false;
            out.prefetchDepth = static_cast<uint32_t>(std::max<uint64_t>(1, std::min<uint64_t>(tmp, 64)));
        } else if (a == "--no-buffering") {
            out.noBuffering = true;
        } else if (a == "--skip-sequential") {
            out.skipSequential = true;
        } else if (a == "--skip-random") {
            out.skipRandom = true;
        } else if (a == "--skip-prefetch") {
            out.skipPrefetch = true;
        } else if (a == "--help" || a == "-h") {
            return true;
        } else {
            err = "Unknown argument: " + a;
            return false;
        }
    }

    if (out.modelPath.empty()) {
        err = "--model is required";
        return false;
    }
    out.windowBytes = std::max<uint64_t>(4096, out.windowBytes);
    out.sequentialBytes = std::max<uint64_t>(out.windowBytes, out.sequentialBytes);
    out.randomOps = std::max<uint64_t>(1, out.randomOps);
    out.bytesPerToken = std::max<uint64_t>(1, out.bytesPerToken);
    return true;
}

uint64_t fileSizeFromHandle(HANDLE h) {
    LARGE_INTEGER li{};
    if (!GetFileSizeEx(h, &li)) {
        return 0;
    }
    return static_cast<uint64_t>(li.QuadPart);
}

bool readAt(HANDLE h, uint64_t offset, void* dst, uint32_t bytes, std::string& err) {
    LARGE_INTEGER li{};
    li.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) {
        err = "SetFilePointerEx failed";
        return false;
    }
    DWORD got = 0;
    if (!ReadFile(h, dst, bytes, &got, nullptr)) {
        err = "ReadFile failed";
        return false;
    }
    if (got != bytes) {
        err = "Short read";
        return false;
    }
    return true;
}

// ============================================================================
// Overlapped async prefetch pipeline
// Issues `depth` concurrent ReadFile calls using Win32 OVERLAPPED + events.
// Uses FILE_FLAG_NO_BUFFERING when requested (sector-aligned buffers/sizes).
// Returns total bytes read and elapsed seconds.
// ============================================================================
struct OverlappedSlot {
    OVERLAPPED ov{};
    HANDLE event = INVALID_HANDLE_VALUE;
    std::vector<uint8_t> buf;
    uint64_t offset = 0;
    uint32_t requested = 0;
    bool inFlight = false;
};

bool runPrefetchPipeline(HANDLE h, uint64_t fileBytes, const Args& a,
                         uint64_t& outBytesRead, double& outSec, std::string& outErr) {
    outBytesRead = 0;
    outSec = 1e-9;

    const uint32_t depth = std::max<uint32_t>(1, std::min<uint32_t>(a.prefetchDepth, 64));

    // Chunk size must be sector-aligned when using FILE_FLAG_NO_BUFFERING.
    uint64_t chunkRaw = std::min<uint64_t>(a.windowBytes, 128ull * 1024ull * 1024ull);
    if (a.noBuffering) {
        chunkRaw = alignUp(chunkRaw, kSectorSize);
    }
    const uint32_t chunk = static_cast<uint32_t>(std::min<uint64_t>(chunkRaw, UINT32_MAX));

    const uint64_t seqTarget = std::min<uint64_t>(a.sequentialBytes, fileBytes);
    // Round down to sector boundary when using NO_BUFFERING.
    const uint64_t readTarget = a.noBuffering ? (seqTarget & ~(kSectorSize - 1)) : seqTarget;
    if (readTarget == 0) {
        outErr = "Prefetch: read target is zero after alignment";
        return false;
    }

    // Buffer size must be sector-aligned for FILE_FLAG_NO_BUFFERING.
    const uint64_t bufSize = a.noBuffering ? alignUp(chunk, kSectorSize) : chunk;

    std::vector<OverlappedSlot> slots(depth);
    std::vector<HANDLE> events(depth);
    for (uint32_t i = 0; i < depth; ++i) {
        slots[i].event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!slots[i].event || slots[i].event == INVALID_HANDLE_VALUE) {
            for (uint32_t j = 0; j < i; ++j) CloseHandle(slots[j].event);
            outErr = "CreateEvent failed for prefetch slot";
            return false;
        }
        events[i] = slots[i].event;
        // VirtualAlloc gives sector-aligned memory suitable for NO_BUFFERING.
        void* mem = VirtualAlloc(nullptr, bufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!mem) {
            for (uint32_t j = 0; j <= i; ++j) CloseHandle(slots[j].event);
            outErr = "VirtualAlloc failed for prefetch buffer";
            return false;
        }
        slots[i].buf.assign(reinterpret_cast<uint8_t*>(mem),
                            reinterpret_cast<uint8_t*>(mem) + bufSize);
        VirtualFree(mem, 0, MEM_RELEASE);
        // Keep the actual data in a VirtualAlloc region accessible to ReadFile.
        // Re-allocate properly: store raw pointer in a parallel array.
    }

    // Re-use vectors with VirtualAlloc-backed buffers.
    std::vector<uint8_t*> rawBufs(depth, nullptr);
    for (uint32_t i = 0; i < depth; ++i) {
        rawBufs[i] = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, bufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!rawBufs[i]) {
            for (uint32_t j = 0; j < i; ++j) VirtualFree(rawBufs[j], 0, MEM_RELEASE);
            for (uint32_t j = 0; j < depth; ++j) CloseHandle(slots[j].event);
            outErr = "VirtualAlloc (raw) failed for prefetch buffer";
            return false;
        }
        slots[i].buf.clear(); // not used; raw pointer used instead
        ResetEvent(slots[i].event);
    }

    auto cleanup = [&]() {
        for (uint32_t i = 0; i < depth; ++i) {
            if (slots[i].inFlight) {
                CancelIoEx(h, &slots[i].ov);
                WaitForSingleObject(slots[i].event, 2000);
            }
            CloseHandle(slots[i].event);
            if (rawBufs[i]) VirtualFree(rawBufs[i], 0, MEM_RELEASE);
        }
    };

    uint64_t nextOffset = 0;
    uint64_t totalRead = 0;
    uint32_t inFlight = 0;

    auto issueRead = [&](uint32_t idx, uint64_t off) -> bool {
        uint64_t remaining = readTarget - off;
        if (remaining == 0) return false;
        uint32_t need = static_cast<uint32_t>(std::min<uint64_t>(chunk, remaining));
        if (a.noBuffering) {
            need = static_cast<uint32_t>(alignUp(need, static_cast<uint64_t>(kSectorSize)));
            // Don't read past EOF (but for no-buffering, last chunk is aligned down).
            if (off + need > fileBytes) {
                need = static_cast<uint32_t>((fileBytes - off) & ~(kSectorSize - 1));
                if (need == 0) return false;
            }
        }

        auto& slot = slots[idx];
        memset(&slot.ov, 0, sizeof(slot.ov));
        slot.ov.hEvent = slot.event;
        slot.ov.Offset = static_cast<DWORD>(off & 0xFFFFFFFF);
        slot.ov.OffsetHigh = static_cast<DWORD>(off >> 32);
        slot.offset = off;
        slot.requested = need;
        ResetEvent(slot.event);

        BOOL ok = ReadFile(h, rawBufs[idx], need, nullptr, &slot.ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            return false;
        }
        slot.inFlight = true;
        ++inFlight;
        return true;
    };

    const auto pStart = std::chrono::high_resolution_clock::now();

    // Prime the pipeline.
    for (uint32_t i = 0; i < depth && nextOffset < readTarget; ++i) {
        if (!issueRead(i, nextOffset)) break;
        uint32_t issued = slots[i].requested;
        nextOffset += issued;
    }

    // Drain + refill.
    while (inFlight > 0) {
        DWORD wIdx = WaitForMultipleObjects(depth, events.data(), FALSE, 15000);
        if (wIdx == WAIT_FAILED || wIdx == WAIT_TIMEOUT) {
            cleanup();
            outErr = "WaitForMultipleObjects failed or timed out in prefetch pipeline";
            return false;
        }
        const uint32_t idx = static_cast<uint32_t>(wIdx - WAIT_OBJECT_0);
        if (idx >= depth) {
            cleanup();
            outErr = "Invalid WFMO index in prefetch pipeline";
            return false;
        }

        auto& slot = slots[idx];
        DWORD got = 0;
        if (!GetOverlappedResult(h, &slot.ov, &got, TRUE)) {
            const DWORD e = GetLastError();
            if (e != ERROR_HANDLE_EOF) {
                cleanup();
                outErr = "GetOverlappedResult failed in prefetch pipeline";
                return false;
            }
            got = 0;
        }
        slot.inFlight = false;
        --inFlight;
        totalRead += got;

        // Issue next chunk into this slot.
        if (nextOffset < readTarget) {
            if (issueRead(idx, nextOffset)) {
                nextOffset += slots[idx].requested;
            }
        }
    }

    const auto pEnd = std::chrono::high_resolution_clock::now();
    outSec = std::max(1e-9,
        std::chrono::duration_cast<std::chrono::duration<double>>(pEnd - pStart).count());
    outBytesRead = totalRead;

    for (uint32_t i = 0; i < depth; ++i) {
        CloseHandle(slots[i].event);
        if (rawBufs[i]) VirtualFree(rawBufs[i], 0, MEM_RELEASE);
    }
    return true;
}

Result run(const Args& a) {
    Result r;
    const auto wallStart = std::chrono::high_resolution_clock::now();

    // Open flags: buffered sequential for the sync passes; optionally unbuffered.
    const DWORD syncFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
    HANDLE h = CreateFileA(a.modelPath.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, syncFlags, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        r.error = "CreateFileA failed";
        return r;
    }

    r.fileBytes = fileSizeFromHandle(h);
    if (r.fileBytes == 0) {
        r.error = "File size is zero or unavailable";
        CloseHandle(h);
        return r;
    }

    const uint64_t seqTarget = std::min<uint64_t>(a.sequentialBytes, r.fileBytes);
    const uint32_t chunk = static_cast<uint32_t>(
        std::min<uint64_t>(a.windowBytes, 128ull * 1024ull * 1024ull));
    std::vector<uint8_t> buffer(chunk);

    // ---- Sequential pass (buffered, single-threaded) ----
    if (!a.skipSequential) {
        uint64_t seqRead = 0;
        auto seqStart = std::chrono::high_resolution_clock::now();
        for (uint64_t offset = 0; offset < seqTarget; offset += chunk) {
            const uint32_t need = static_cast<uint32_t>(
                std::min<uint64_t>(chunk, seqTarget - offset));
            std::string err;
            if (!readAt(h, offset, buffer.data(), need, err)) {
                r.error = err;
                CloseHandle(h);
                return r;
            }
            seqRead += need;
        }
        auto seqEnd = std::chrono::high_resolution_clock::now();
        const double seqSec = std::max(1e-9,
            std::chrono::duration_cast<std::chrono::duration<double>>(seqEnd - seqStart).count());
        r.sequentialBytesRead = seqRead;
        r.sequentialGbps = bytesToGB(seqRead) / seqSec;
        r.estimatedSeqTps = static_cast<double>(seqRead) / seqSec /
                            static_cast<double>(a.bytesPerToken);
    }

    // ---- Random window pass (buffered, single-threaded) ----
    if (!a.skipRandom) {
        uint64_t rndRead = 0;
        const uint64_t window = std::min<uint64_t>(a.windowBytes, r.fileBytes);
        const uint64_t maxOffset = (r.fileBytes > window) ? (r.fileBytes - window) : 0;
        std::mt19937_64 rng(a.seed);
        std::uniform_int_distribution<uint64_t> dist(0, std::max<uint64_t>(1, maxOffset));

        auto rndStart = std::chrono::high_resolution_clock::now();
        for (uint64_t i = 0; i < a.randomOps; ++i) {
            const uint64_t base = (maxOffset == 0) ? 0 : dist(rng);
            uint64_t remaining = window;
            uint64_t cursor = base;
            while (remaining > 0) {
                const uint32_t need = static_cast<uint32_t>(
                    std::min<uint64_t>(chunk, remaining));
                std::string err;
                if (!readAt(h, cursor, buffer.data(), need, err)) {
                    r.error = err;
                    CloseHandle(h);
                    return r;
                }
                rndRead += need;
                cursor += need;
                remaining -= need;
            }
        }
        auto rndEnd = std::chrono::high_resolution_clock::now();
        const double rndSec = std::max(1e-9,
            std::chrono::duration_cast<std::chrono::duration<double>>(rndEnd - rndStart).count());
        r.randomBytesRead = rndRead;
        r.randomGbps = bytesToGB(rndRead) / rndSec;
        r.estimatedRndTps = static_cast<double>(rndRead) / rndSec /
                            static_cast<double>(a.bytesPerToken);
    }

    CloseHandle(h);

    // ---- Prefetch pipeline pass (overlapped async, attempts to saturate bus) ----
    if (!a.skipPrefetch) {
        // Re-open with FILE_FLAG_OVERLAPPED (+ optionally FILE_FLAG_NO_BUFFERING).
        DWORD asyncFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;
        if (a.noBuffering) {
            asyncFlags |= FILE_FLAG_NO_BUFFERING;
        }
        HANDLE hAsync = CreateFileA(a.modelPath.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, asyncFlags, nullptr);
        if (hAsync == INVALID_HANDLE_VALUE) {
            r.error = "CreateFileA (async) failed";
            return r;
        }

        double prefSec = 1e-9;
        std::string prefErr;
        if (!runPrefetchPipeline(hAsync, r.fileBytes, a, r.prefetchBytesRead, prefSec, prefErr)) {
            CloseHandle(hAsync);
            r.error = "Prefetch pipeline failed: " + prefErr;
            return r;
        }
        CloseHandle(hAsync);

        r.prefetchGbps = bytesToGB(r.prefetchBytesRead) / prefSec;
        r.estimatedPrefetchTps = static_cast<double>(r.prefetchBytesRead) / prefSec /
                                 static_cast<double>(a.bytesPerToken);
    }

    const auto wallEnd = std::chrono::high_resolution_clock::now();
    r.wallMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
                   wallEnd - wallStart).count();
    r.success = true;
    return r;
}

void printUsage() {
    std::cout << "RawrXD-DiskPagedBench\n"
              << "  --model <path>                 Required GGUF/model file path\n"
              << "  --window-bytes <n>             Read chunk/window size (default 64MB)\n"
              << "  --sequential-bytes <n>         Sequential read cap (default 8GB)\n"
              << "  --random-ops <n>               Random window operations (default 256)\n"
              << "  --prefetch-depth <n>           Async overlapped in-flight reads (default 4)\n"
              << "  --no-buffering                 Use FILE_FLAG_NO_BUFFERING (bypass OS cache)\n"
              << "  --skip-sequential              Skip the buffered sequential pass\n"
              << "  --skip-random                  Skip the buffered random pass\n"
              << "  --skip-prefetch                Skip the overlapped async prefetch pass\n"
              << "  --bytes-per-token <n>          Bytes consumed per token estimate\n"
              << "                                 (default 2T Q4 pessimistic: 1.1TB)\n"
              << "  --seed <n>                     RNG seed (default 1337)\n";
}

} // namespace

int main(int argc, char** argv) {
    Args args;
    std::string parseErr;
    if (!parseArgs(argc, argv, args, parseErr)) {
        std::cerr << "[DiskPagedBench] Argument error: " << parseErr << "\n";
        printUsage();
        return 2;
    }

    if (args.modelPath.empty()) {
        printUsage();
        return 2;
    }

    Result r = run(args);
    if (!r.success) {
        std::cerr << "[DiskPagedBench] FAIL: " << r.error << "\n";
        std::cout << "RAWRXD_DISK_PAGED_JSON=" << toJson(args, r) << "\n";
        return 1;
    }

    std::cout << std::fixed << std::setprecision(4)
              << "[DiskPagedBench] file_gb=" << bytesToGB(r.fileBytes)
              << " seq_gbps=" << r.sequentialGbps
              << " rnd_gbps=" << r.randomGbps
              << " prefetch_gbps=" << r.prefetchGbps
              << " (depth=" << args.prefetchDepth
              << (args.noBuffering ? ",nobuf" : "") << ")"
              << " est_seq_tps=" << r.estimatedSeqTps
              << " est_rnd_tps=" << r.estimatedRndTps
              << " est_prefetch_tps=" << r.estimatedPrefetchTps
              << " wall_ms=" << r.wallMs
              << "\n";

    std::cout << "RAWRXD_DISK_PAGED_JSON=" << toJson(args, r) << "\n";
    return 0;
}
