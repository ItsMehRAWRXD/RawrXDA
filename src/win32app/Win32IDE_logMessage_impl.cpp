// Build-compat shim for legacy logMessage stub references.
// The production implementation lives in src/win32app/Win32IDE_logMessage.cpp.

#include "../logging/Logger.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace {
std::atomic<uint64_t> g_win32IdeLogMessageStubHits{0};
std::atomic<uint64_t> g_win32IdeLogMessageInfoCount{0};
std::atomic<uint64_t> g_win32IdeLogMessageWarnCount{0};
std::atomic<uint64_t> g_win32IdeLogMessageErrorCount{0};
std::atomic<uint64_t> g_win32IdeLogMessageSuppressedCount{0};
std::atomic<uint64_t> g_win32IdeLogMessageInvalidPolicyCount{0};
std::atomic<uint64_t> g_win32IdeLogMessageSequence{0};
std::atomic<uint64_t> g_win32IdeLogMessageLastModeMask{0};

enum class StubLogSeverity {
    Info,
    Warn,
    Error,
};

StubLogSeverity configuredSeverity()
{
    if (const char* env = std::getenv("RAWRXD_LOG_STUB_LEVEL")) {
        const std::string v(env);
        if (v == "warn" || v == "WARN" || v == "1") {
            return StubLogSeverity::Warn;
        }
        if (v == "error" || v == "ERROR" || v == "2") {
            return StubLogSeverity::Error;
        }
    }
    return StubLogSeverity::Info;
}

uint64_t configuredSampleRate()
{
    if (const char* env = std::getenv("RAWRXD_LOG_STUB_SAMPLE_RATE")) {
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(env, &end, 10);
        if (end == env || parsed == 0) {
            g_win32IdeLogMessageInvalidPolicyCount.fetch_add(1, std::memory_order_relaxed);
            return 1;
        }
        return parsed > 1024 ? 1024 : static_cast<uint64_t>(parsed);
    }
    return 1;
}

bool shouldEmitForSequence(uint64_t seq, uint64_t sampleRate)
{
    if (sampleRate <= 1) {
        return true;
    }
    return (seq % sampleRate) == 0;
}
}

extern "C" void RawrXD_Win32IDELogMessageStubAnchor() {
    g_win32IdeLogMessageStubHits.fetch_add(1, std::memory_order_relaxed);
    const uint64_t seq = g_win32IdeLogMessageSequence.fetch_add(1, std::memory_order_relaxed) + 1;

    const StubLogSeverity severity = configuredSeverity();
    const uint64_t sampleRate = configuredSampleRate();
    const bool emit = shouldEmitForSequence(seq, sampleRate);
    uint64_t modeMask = 0;
    if (emit) modeMask |= 0x1ULL;

    auto& logger = RawrXD::Logging::Logger::instance();
    if (!emit) {
        g_win32IdeLogMessageSuppressedCount.fetch_add(1, std::memory_order_relaxed);
        modeMask |= (sampleRate > 1 ? 0x2ULL : 0);
        g_win32IdeLogMessageLastModeMask.store(modeMask, std::memory_order_relaxed);
        return;
    }

    switch (severity) {
    case StubLogSeverity::Warn:
        modeMask |= 0x4ULL;
        g_win32IdeLogMessageWarnCount.fetch_add(1, std::memory_order_relaxed);
        logger.warn("Win32IDE logMessage stub anchor invoked", "Win32IDE_logMessage_stub");
        break;
    case StubLogSeverity::Error:
        modeMask |= 0x8ULL;
        g_win32IdeLogMessageErrorCount.fetch_add(1, std::memory_order_relaxed);
        logger.error("Win32IDE logMessage stub anchor invoked", "Win32IDE_logMessage_stub");
        break;
    case StubLogSeverity::Info:
    default:
        modeMask |= 0x10ULL;
        g_win32IdeLogMessageInfoCount.fetch_add(1, std::memory_order_relaxed);
        logger.info("Win32IDE logMessage stub anchor invoked", "Win32IDE_logMessage_stub");
        break;
    }

    if (sampleRate > 1) {
        modeMask |= 0x20ULL;
    }
    g_win32IdeLogMessageLastModeMask.store(modeMask, std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_Win32IDELogMessageStubHitCount() {
    return g_win32IdeLogMessageStubHits.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_Win32IDELogMessageStubInfoCount() {
    return g_win32IdeLogMessageInfoCount.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_Win32IDELogMessageStubWarnCount() {
    return g_win32IdeLogMessageWarnCount.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_Win32IDELogMessageStubErrorCount() {
    return g_win32IdeLogMessageErrorCount.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_Win32IDELogMessageStubSuppressedCount() {
    return g_win32IdeLogMessageSuppressedCount.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_Win32IDELogMessageStubStats() {
    // [63:56] invalid_policy, [55:48] suppressed, [47:40] error,
    // [39:32] warn, [31:24] info, [23:16] hits, [15:8] sequence, [7:0] mode_mask.
    const uint64_t invalid = g_win32IdeLogMessageInvalidPolicyCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t suppressed = g_win32IdeLogMessageSuppressedCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t error = g_win32IdeLogMessageErrorCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t warn = g_win32IdeLogMessageWarnCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t info = g_win32IdeLogMessageInfoCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t hits = g_win32IdeLogMessageStubHits.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t sequence = g_win32IdeLogMessageSequence.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t mode = g_win32IdeLogMessageLastModeMask.load(std::memory_order_relaxed) & 0xFFu;
    return (invalid << 56) | (suppressed << 48) | (error << 40) | (warn << 32) |
           (info << 24) | (hits << 16) | (sequence << 8) | mode;
}
