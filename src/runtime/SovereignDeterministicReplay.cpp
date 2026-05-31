// =============================================================================
// SovereignDeterministicReplay.cpp — Phase 54: Real kernel replay + diff validation
// =============================================================================
// recordFrame: captures a baseline input/output pair for a known-good kernel call.
// validateKernelExecution: replays all stored frames for the given functionId
//   against a candidate kernel pointer, comparing outputs byte-for-byte.
//   The candidate is called under SEH; any exception or divergence → false.
//
// Output buffers are capped at kMaxOutputBytes to prevent runaway allocation.
// =============================================================================
#include "SovereignDeterministicReplay.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

namespace RawrXD::Runtime {

static constexpr size_t kMaxInputBytes  = 64 * 1024;
static constexpr size_t kMaxOutputBytes = 64 * 1024;
static constexpr size_t kScratchPad     = 128 * 1024; // per-probe scratch region

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
SovereignDeterministicReplay& SovereignDeterministicReplay::instance() {
    static SovereignDeterministicReplay inst;
    return inst;
}

// ---------------------------------------------------------------------------
// recordFrame — capture a baseline frame (size-guarded)
// ---------------------------------------------------------------------------
bool SovereignDeterministicReplay::recordFrame(uint32_t fid, uint32_t vid,
                                               void* in, size_t inSize,
                                               void* out, size_t outSize) {
    if (!in || !out) return false;
    if (inSize  > kMaxInputBytes)  inSize  = kMaxInputBytes;
    if (outSize > kMaxOutputBytes) outSize = kMaxOutputBytes;

    std::lock_guard<std::mutex> lock(m_mutex);

    ReplayFrame frame;
    frame.functionId = fid;
    frame.versionId  = vid;
    frame.input.assign(reinterpret_cast<uint8_t*>(in),
                       reinterpret_cast<uint8_t*>(in)  + inSize);
    frame.output.assign(reinterpret_cast<uint8_t*>(out),
                        reinterpret_cast<uint8_t*>(out) + outSize);
    frame.timestamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() / 1000u);

    m_history.push_back(std::move(frame));
    if (m_history.size() > MAX_REPLAY_FRAMES)
        m_history.pop_front();
    return true;
}

// ---------------------------------------------------------------------------
// ProbeKernel — SEH-guarded call of candidate kernel with given input.
// Writes result into outBuf (up to outBuf.size() bytes).
// Returns false on any exception.
// ---------------------------------------------------------------------------
// ABI: typedef void (*KernelFn)(const uint8_t* in, size_t inLen,
//                               uint8_t* out, size_t* outLen);
using KernelFn = void(*)(const uint8_t*, size_t, uint8_t*, size_t*);

static bool probeKernel(KernelFn fn,
                        const std::vector<uint8_t>& input,
                        std::vector<uint8_t>& outputBuf) {
    size_t outLen = outputBuf.size();
    bool ok = false;
    __try {
        fn(input.data(), input.size(), outputBuf.data(), &outLen);
        ok = true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    if (ok) outputBuf.resize(std::min(outLen, outputBuf.size()));
    return ok;
}

// ---------------------------------------------------------------------------
// validateKernelExecution
// ---------------------------------------------------------------------------
bool SovereignDeterministicReplay::validateKernelExecution(void* kernelAddr,
                                                           uint32_t fid) {
    if (!kernelAddr) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return true; // no baseline yet → conservative pass

    KernelFn candidate = reinterpret_cast<KernelFn>(kernelAddr);
    uint32_t divergences = 0;
    uint32_t probed      = 0;

    for (const auto& frame : m_history) {
        if (frame.functionId != fid) continue;
        ++probed;

        // Allocate scratch output buffer matching baseline output size
        size_t expectedSize = frame.output.size();
        if (expectedSize == 0 || expectedSize > kMaxOutputBytes) continue;

        std::vector<uint8_t> actual(expectedSize, 0);
        bool called = probeKernel(candidate, frame.input, actual);
        if (!called) { ++divergences; continue; }

        // Byte-level comparison
        if (actual.size() != frame.output.size() ||
            std::memcmp(actual.data(), frame.output.data(), frame.output.size()) != 0) {
            ++divergences;
        }
    }

    // No frames matched this fid → conservative pass
    if (probed == 0) return true;

    // Allow at most 0 divergences for deterministic correctness
    return divergences == 0;
}

} // namespace RawrXD::Runtime
