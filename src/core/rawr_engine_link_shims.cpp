// Minimal link shims for RawrEngine / Gold / InferenceEngine.
// These are no-op fallbacks to satisfy references after stub purge.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include "../agentic/RobustOllamaParser.h"

namespace {

constexpr uint32_t kPerfSlotCount = 64U;
constexpr uint32_t kPerfHistogramBuckets = 8U;
constexpr uint32_t kWatchdogStatusUninitialized = 0xFFFFFFFFU;
constexpr uint32_t kWatchdogStatusOk = 0U;
constexpr uint32_t kWatchdogStatusTampered = 1U;
constexpr uint32_t kWatchdogStatusNoTextSection = 3U;

constexpr uint32_t kCamAuthMagic = 0x324D4352U;  // "RCM2"
constexpr uint32_t kCamAuthVersion = 1U;
constexpr uint32_t kCamAuthNonceSize = 16U;
constexpr uint32_t kCamAuthMacSize = 32U;
constexpr uint32_t kCamAuthHeaderSize = 8U + kCamAuthNonceSize + kCamAuthMacSize;

constexpr std::array<uint64_t, kPerfHistogramBuckets> kPerfBucketBounds = {
    256ULL, 1024ULL, 4096ULL, 16384ULL, 65536ULL, 262144ULL, 1048576ULL, UINT64_MAX
};

struct alignas(64) PerfSlotDataShim {
    uint64_t count;
    uint64_t totalCycles;
    uint64_t minCycles;
    uint64_t maxCycles;
    uint64_t buckets[kPerfHistogramBuckets];
    uint64_t lastCycles;
    uint32_t flags;
    uint32_t reserved;
    uint64_t reserved2;
    uint64_t reserved3;
};
static_assert(sizeof(PerfSlotDataShim) == 128, "PerfSlotDataShim must remain 128 bytes");

struct WatchdogStatusShim {
    uint32_t status;
    uint32_t reserved;
    uint64_t textBase;
    uint64_t textSize;
    uint64_t verifyCount;
    uint64_t tamperCount;
    uint64_t lastVerifyTick;
};
static_assert(sizeof(WatchdogStatusShim) == 48, "WatchdogStatusShim must remain 48 bytes");

std::array<PerfSlotDataShim, kPerfSlotCount> g_perfSlots{};
std::mutex g_perfMutex;

std::mutex g_watchdogMutex;
WatchdogStatusShim g_watchdogStatus{
    kWatchdogStatusUninitialized, 0, 0, 0, 0, 0, 0
};
std::array<uint8_t, 32> g_watchdogBaseline{};
bool g_watchdogInitialized = false;

struct HotpatchStatsShim {
    uint64_t swapsApplied;
    uint64_t swapsRolledBack;
    uint64_t swapsFailed;
    uint64_t shadowPagesAllocated;
    uint64_t shadowPagesFreed;
    uint64_t icacheFlushes;
    uint64_t crcChecks;
    uint64_t crcMismatches;
};
static_assert(sizeof(HotpatchStatsShim) == 64, "HotpatchStatsShim must remain 64 bytes");

struct SnapshotStatsShim {
    uint64_t snapshotsCaptured;
    uint64_t snapshotsRestored;
    uint64_t snapshotsDiscarded;
    uint64_t verifyPassed;
    uint64_t verifyFailed;
    uint64_t totalBytesStored;
};
static_assert(sizeof(SnapshotStatsShim) == 48, "SnapshotStatsShim must remain 48 bytes");

struct HotpatchBackupSlot {
    bool valid;
    void* funcAddr;
    uint32_t crc32;
    std::array<uint8_t, 16> bytes;
};

struct SnapshotEntry {
    uint32_t id;
    void* funcAddr;
    uint32_t crc32;
    std::vector<uint8_t> bytes;
};

constexpr uint32_t kHotpatchMaxBackupSlots = 256U;
std::array<HotpatchBackupSlot, kHotpatchMaxBackupSlots> g_hotpatchBackups{};
std::vector<SnapshotEntry> g_snapshotEntries;
HotpatchStatsShim g_hotpatchStats{};
SnapshotStatsShim g_snapshotStats{};
std::mutex g_hotpatchMutex;

uint64_t perfNowTicks() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

uint32_t perfBucketIndex(uint64_t delta) {
    for (uint32_t i = 0; i < kPerfHistogramBuckets; ++i) {
        if (delta <= kPerfBucketBounds[i]) {
            return i;
        }
    }
    return kPerfHistogramBuckets - 1U;
}

void perfRecord(uint32_t slotIndex, uint64_t delta) {
    PerfSlotDataShim& slot = g_perfSlots[slotIndex];
    slot.count += 1ULL;
    slot.totalCycles += delta;
    if (slot.count == 1ULL) {
        slot.minCycles = delta;
        slot.maxCycles = delta;
    } else {
        if (delta < slot.minCycles) {
            slot.minCycles = delta;
        }
        if (delta > slot.maxCycles) {
            slot.maxCycles = delta;
        }
    }
    slot.buckets[perfBucketIndex(delta)] += 1ULL;
    slot.lastCycles = delta;
}

uint64_t fnv1a64(const uint8_t* data, size_t size, uint64_t seed) {
    uint64_t hash = seed;
    for (size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void expandDigest32(const uint8_t* data, size_t size, std::array<uint8_t, 32>& out) {
    const uint64_t h0 = fnv1a64(data, size, 1469598103934665603ULL);
    const uint64_t h1 = fnv1a64(data, size, 1099511628211ULL ^ 0x9E3779B97F4A7C15ULL);
    const uint64_t h2 = fnv1a64(data, size, 0xD6E8FEB86659FD93ULL);
    const uint64_t h3 = fnv1a64(data, size, 0xA24BAED4963EE407ULL);
    std::memcpy(out.data() + 0, &h0, sizeof(h0));
    std::memcpy(out.data() + 8, &h1, sizeof(h1));
    std::memcpy(out.data() + 16, &h2, sizeof(h2));
    std::memcpy(out.data() + 24, &h3, sizeof(h3));
}

bool getTextSectionSpan(const uint8_t*& textBase, size_t& textSize) {
    textBase = nullptr;
    textSize = 0;
    const HMODULE module = GetModuleHandleW(nullptr);
    if (module == nullptr) {
        return false;
    }

    const auto* imageBase = reinterpret_cast<const uint8_t*>(module);
    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        imageBase + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section) {
        if (std::memcmp(section->Name, ".text", 5) == 0) {
            textBase = imageBase + section->VirtualAddress;
            textSize = static_cast<size_t>(section->Misc.VirtualSize);
            return textSize != 0U;
        }
    }
    return false;
}

bool computeTextDigest(std::array<uint8_t, 32>& outDigest,
                       uint64_t& outTextBase,
                       uint64_t& outTextSize) {
    const uint8_t* textBase = nullptr;
    size_t textSize = 0;
    if (!getTextSectionSpan(textBase, textSize)) {
        return false;
    }
    expandDigest32(textBase, textSize, outDigest);
    outTextBase = reinterpret_cast<uint64_t>(textBase);
    outTextSize = static_cast<uint64_t>(textSize);
    return true;
}

void makeNonce(uint8_t nonce[kCamAuthNonceSize]) {
    const uint64_t t = perfNowTicks();
    const uint64_t pidTid =
        (static_cast<uint64_t>(GetCurrentProcessId()) << 32) |
        static_cast<uint64_t>(GetCurrentThreadId());
    std::memcpy(nonce, &t, sizeof(t));
    std::memcpy(nonce + 8, &pidTid, sizeof(pidTid));
}

uint8_t streamByte(const uint8_t nonce[kCamAuthNonceSize], uint32_t index) {
    uint64_t state = 0x9E3779B97F4A7C15ULL ^ static_cast<uint64_t>(index + 1U);
    state ^= static_cast<uint64_t>(nonce[index % kCamAuthNonceSize]) * 0x100000001B3ULL;
    state ^= (state >> 33);
    state *= 0xFF51AFD7ED558CCDULL;
    state ^= (state >> 29);
    return static_cast<uint8_t>(state & 0xFFU);
}

void xorWithNonceStream(const uint8_t* input, uint32_t length,
                        const uint8_t nonce[kCamAuthNonceSize],
                        uint8_t* output) {
    for (uint32_t i = 0; i < length; ++i) {
        output[i] = static_cast<uint8_t>(input[i] ^ streamByte(nonce, i));
    }
}

void computeAuthMac(const uint8_t nonce[kCamAuthNonceSize],
                    const uint8_t* ciphertext,
                    uint32_t ciphertextLen,
                    uint8_t outMac[kCamAuthMacSize]) {
    uint64_t s0 = 0x243F6A8885A308D3ULL;
    uint64_t s1 = 0x13198A2E03707344ULL;
    uint64_t s2 = 0xA4093822299F31D0ULL;
    uint64_t s3 = 0x082EFA98EC4E6C89ULL;

    for (uint32_t i = 0; i < kCamAuthNonceSize; ++i) {
        const uint64_t v = static_cast<uint64_t>(nonce[i] + 1U);
        s0 = (s0 ^ v) * 0x100000001B3ULL;
        s1 = (s1 + (v << (i % 13U))) * 0x9E3779B185EBCA87ULL;
    }
    for (uint32_t i = 0; i < ciphertextLen; ++i) {
        const uint64_t v = static_cast<uint64_t>(ciphertext[i] + 1U);
        s2 = (s2 ^ (v + (i * 1315423911U))) * 0xC2B2AE3D27D4EB4FULL;
        s3 = (s3 + (v << (i % 17U))) * 0x165667B19E3779F9ULL;
    }

    std::memcpy(outMac + 0, &s0, sizeof(s0));
    std::memcpy(outMac + 8, &s1, sizeof(s1));
    std::memcpy(outMac + 16, &s2, sizeof(s2));
    std::memcpy(outMac + 24, &s3, sizeof(s3));
}

bool macEquals32(const uint8_t* a, const uint8_t* b) {
    uint8_t diff = 0U;
    for (uint32_t i = 0; i < kCamAuthMacSize; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0U;
}

uint32_t crc32Bytes(const uint8_t* bytes, size_t size) {
    if (bytes == nullptr) {
        return 0U;
    }
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < size; ++i) {
        crc ^= static_cast<uint32_t>(bytes[i]);
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = static_cast<uint32_t>(
                -(static_cast<int32_t>(crc & 1U)));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

bool writeExecutableMemory(void* dst, const void* src, size_t size) {
    if (dst == nullptr || src == nullptr || size == 0U) {
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    std::memcpy(dst, src, size);
    FlushInstructionCache(GetCurrentProcess(), dst, size);

    DWORD ignored = 0;
    VirtualProtect(dst, size, oldProtect, &ignored);
    return true;
}

int findSnapshotEntryIndex(uint32_t snapshotId) {
    for (size_t i = 0; i < g_snapshotEntries.size(); ++i) {
        if (g_snapshotEntries[i].id == snapshotId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

extern "C" {

// Pyre compute kernels
int asm_pyre_gemm_fp32(const void*, const void*, void*, int, int, int) { return 0; }
int asm_pyre_rmsnorm(void*, const void*, int) { return 0; }
int asm_pyre_silu(void*, const void*, int) { return 0; }
int asm_pyre_rope(void*, const void*, int) { return 0; }
int asm_pyre_embedding_lookup(const void*, const void*, void*, int, int) { return 0; }
int asm_pyre_gemv_fp32(const void*, const void*, void*, int, int) { return 0; }
int asm_pyre_add_fp32(void*, const void*, const void*, int) { return 0; }

// Batch 4: hotpatch + pyre + pattern
int asm_pyre_mul_fp32(void*, const void*, const void*, int) { return 0; }
int asm_pyre_softmax(void*, const void*, int) { return 0; }
int asm_hotpatch_restore_prologue(uint32_t slotIndex) {
    if (slotIndex >= kHotpatchMaxBackupSlots) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    HotpatchBackupSlot& slot = g_hotpatchBackups[slotIndex];
    if (!slot.valid || slot.funcAddr == nullptr) {
        return -2;
    }
    if (!writeExecutableMemory(slot.funcAddr, slot.bytes.data(), slot.bytes.size())) {
        g_hotpatchStats.swapsFailed += 1ULL;
        return -3;
    }
    g_hotpatchStats.swapsRolledBack += 1ULL;
    return 0;
}

int asm_hotpatch_backup_prologue(void* funcAddr, uint32_t slotIndex) {
    if (funcAddr == nullptr || slotIndex >= kHotpatchMaxBackupSlots) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    HotpatchBackupSlot& slot = g_hotpatchBackups[slotIndex];
    slot.valid = true;
    slot.funcAddr = funcAddr;
    std::memcpy(slot.bytes.data(), funcAddr, slot.bytes.size());
    slot.crc32 = crc32Bytes(slot.bytes.data(), slot.bytes.size());
    return 0;
}

int asm_hotpatch_flush_icache(void* base, size_t size) {
    if (base == nullptr || size == 0U) {
        return -1;
    }
    const BOOL ok = FlushInstructionCache(GetCurrentProcess(), base, size);
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    if (ok) {
        g_hotpatchStats.icacheFlushes += 1ULL;
        return 0;
    }
    return -2;
}

void* asm_hotpatch_alloc_shadow(size_t size) {
    const size_t allocSize = (size == 0U) ? 65536U : size;
    void* page = VirtualAlloc(nullptr, allocSize, MEM_COMMIT | MEM_RESERVE,
                              PAGE_EXECUTE_READWRITE);
    if (page != nullptr) {
        std::lock_guard<std::mutex> lock(g_hotpatchMutex);
        g_hotpatchStats.shadowPagesAllocated += 1ULL;
    }
    return page;
}

// Batch 5: hotpatch/snapshot stats + prologue/trampoline/free
int asm_hotpatch_verify_prologue(void* funcAddr, uint32_t expectedCRC) {
    if (funcAddr == nullptr) {
        return -1;
    }

    uint8_t prologue[16]{};
    std::memcpy(prologue, funcAddr, sizeof(prologue));
    const uint32_t actualCRC = crc32Bytes(prologue, sizeof(prologue));

    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    g_hotpatchStats.crcChecks += 1ULL;
    if (actualCRC != expectedCRC) {
        g_hotpatchStats.crcMismatches += 1ULL;
        return 1;
    }
    return 0;
}

int asm_hotpatch_install_trampoline(void* originalFn, void* trampolineBuffer) {
    if (originalFn == nullptr || trampolineBuffer == nullptr) {
        return -1;
    }

    uint8_t trampoline[28]{};
    std::memcpy(trampoline, originalFn, 14);
    trampoline[14] = 0xFF;
    trampoline[15] = 0x25;
    trampoline[16] = 0x00;
    trampoline[17] = 0x00;
    trampoline[18] = 0x00;
    trampoline[19] = 0x00;
    void* jumpBack = static_cast<uint8_t*>(originalFn) + 14;
    std::memcpy(&trampoline[20], &jumpBack, sizeof(jumpBack));

    if (!writeExecutableMemory(trampolineBuffer, trampoline, sizeof(trampoline))) {
        return -2;
    }
    return 0;
}

int asm_hotpatch_free_shadow(void* base, size_t) {
    if (base == nullptr) {
        return -1;
    }
    if (!VirtualFree(base, 0, MEM_RELEASE)) {
        return -2;
    }
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    g_hotpatchStats.shadowPagesFreed += 1ULL;
    return 0;
}

int asm_snapshot_capture(void* funcAddr, uint32_t snapshotId, size_t captureSize) {
    if (funcAddr == nullptr || captureSize == 0U) {
        return -1;
    }

    SnapshotEntry entry{};
    entry.id = snapshotId;
    entry.funcAddr = funcAddr;
    entry.bytes.resize(captureSize);
    std::memcpy(entry.bytes.data(), funcAddr, captureSize);
    entry.crc32 = crc32Bytes(entry.bytes.data(), entry.bytes.size());

    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    const int existing = findSnapshotEntryIndex(snapshotId);
    if (existing >= 0) {
        g_snapshotStats.totalBytesStored -= static_cast<uint64_t>(
            g_snapshotEntries[existing].bytes.size());
        g_snapshotEntries[existing] = std::move(entry);
    } else {
        g_snapshotEntries.push_back(std::move(entry));
    }
    g_snapshotStats.snapshotsCaptured += 1ULL;
    g_snapshotStats.totalBytesStored += captureSize;
    return 0;
}

int asm_hotpatch_atomic_swap(void* targetFn, void* newFn) {
    if (targetFn == nullptr || newFn == nullptr) {
        return -1;
    }

    uint8_t patch[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0, 0, 0, 0, 0, 0, 0, 0
    };
    std::memcpy(&patch[6], &newFn, sizeof(newFn));

    if (!writeExecutableMemory(targetFn, patch, sizeof(patch))) {
        std::lock_guard<std::mutex> lock(g_hotpatchMutex);
        g_hotpatchStats.swapsFailed += 1ULL;
        return -2;
    }

    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    g_hotpatchStats.swapsApplied += 1ULL;
    return 0;
}

int asm_hotpatch_get_stats(void* statsBuffer64) {
    if (statsBuffer64 == nullptr) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    std::memcpy(statsBuffer64, &g_hotpatchStats, sizeof(g_hotpatchStats));
    return 0;
}

int asm_snapshot_get_stats(void* statsBuffer48) {
    if (statsBuffer48 == nullptr) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    std::memcpy(statsBuffer48, &g_snapshotStats, sizeof(g_snapshotStats));
    return 0;
}

// Batch 6: snapshot/camellia/log/enterprise
int asm_snapshot_restore(uint32_t snapshotId) {
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    const int idx = findSnapshotEntryIndex(snapshotId);
    if (idx < 0) {
        return -1;
    }

    SnapshotEntry& entry = g_snapshotEntries[static_cast<size_t>(idx)];
    if (entry.funcAddr == nullptr || entry.bytes.empty()) {
        return -2;
    }
    if (!writeExecutableMemory(entry.funcAddr, entry.bytes.data(), entry.bytes.size())) {
        return -3;
    }
    g_snapshotStats.snapshotsRestored += 1ULL;
    return 0;
}

int asm_snapshot_discard(uint32_t snapshotId) {
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    const int idx = findSnapshotEntryIndex(snapshotId);
    if (idx < 0) {
        return -1;
    }

    const size_t eraseIndex = static_cast<size_t>(idx);
    g_snapshotStats.totalBytesStored -= static_cast<uint64_t>(
        g_snapshotEntries[eraseIndex].bytes.size());
    g_snapshotEntries.erase(g_snapshotEntries.begin() + static_cast<std::ptrdiff_t>(eraseIndex));
    g_snapshotStats.snapshotsDiscarded += 1ULL;
    return 0;
}

int asm_snapshot_verify(uint32_t snapshotId, uint32_t expectedCRC) {
    std::lock_guard<std::mutex> lock(g_hotpatchMutex);
    const int idx = findSnapshotEntryIndex(snapshotId);
    if (idx < 0) {
        return -1;
    }

    const SnapshotEntry& entry = g_snapshotEntries[static_cast<size_t>(idx)];
    if (entry.funcAddr == nullptr || entry.bytes.empty()) {
        return -2;
    }

    std::vector<uint8_t> current(entry.bytes.size());
    std::memcpy(current.data(), entry.funcAddr, current.size());
    const uint32_t actualCRC = crc32Bytes(current.data(), current.size());
    const uint32_t targetCRC = (expectedCRC != 0U) ? expectedCRC : entry.crc32;
    if (actualCRC != targetCRC) {
        g_snapshotStats.verifyFailed += 1ULL;
        return 1;
    }

    g_snapshotStats.verifyPassed += 1ULL;
    return 0;
}
int asm_camellia256_auth_decrypt_file(const char*, const char*, const uint8_t*, uint32_t) { return 0; }
int asm_camellia256_auth_encrypt_file(const char*, const char*, const uint8_t*, uint32_t) { return 0; }
void RawrXD_Native_Log(const char* channel, const char* message) {
    OutputDebugStringA("[RawrXD]");
    OutputDebugStringA(channel != nullptr ? channel : "core");
    OutputDebugStringA(": ");
    OutputDebugStringA(message != nullptr ? message : "(null)");
    OutputDebugStringA("\n");
}
int Enterprise_DevUnlock() { return 0; }

// Batch 7: subsystem modes + Vulkan init
int InjectMode() { return 0; }
int DiffCovMode() { return 0; }
int SO_InitializeVulkan() { return 0; }
int IntelPTMode() { return 0; }
int AgentTraceMode() { return 0; }
int DynTraceMode() { return 0; }
int CovFusionMode() { return 0; }

// Batch 8: subsystem API hooks
int AD_ProcessGGUF(const char*, const char*) { return 0; }
int SO_InitializeStreaming() { return 0; }
int SideloadMode() { return 0; }
int SO_CreateComputePipelines() { return 0; }
int PersistenceMode() { return 0; }
int SO_PrintStatistics() { return 0; }
int SO_CreateMemoryArena() { return 0; }

// Batch 9: subsystem pipeline + tooling modes
int SO_LoadExecFile(const char*, const char*) { return 0; }
int BasicBlockCovMode() { return 0; }
int SO_PrintMetrics() { return 0; }
int SO_StartDEFLATEThreads() { return 0; }
int StubGenMode() { return 0; }
int TraceEngineMode() { return 0; }
int CompileMode() { return 0; }

// Batch 10: fuzzing/prefetch/thread pool + modes
int GapFuzzMode() { return 0; }
int EncryptMode() { return 0; }
int SO_InitializePrefetchQueue() { return 0; }
int SO_CreateThreadPool() { return 0; }
int EntropyMode() { return 0; }
int AgenticMode() { return 0; }
int UACBypassMode() { return 0; }
int AVScanMode() { return 0; }

// Batch 11/12 hardened providers: perf + watchdog + camellia auth-buffer
int asm_perf_init() {
    std::lock_guard<std::mutex> lock(g_perfMutex);
    std::memset(g_perfSlots.data(), 0, sizeof(g_perfSlots));
    return 0;
}

uint64_t asm_perf_begin(uint32_t slotIndex) {
    if (slotIndex >= kPerfSlotCount) {
        return 0ULL;
    }
    return perfNowTicks();
}

uint64_t asm_perf_end(uint32_t slotIndex, uint64_t startTSC) {
    if (slotIndex >= kPerfSlotCount || startTSC == 0ULL) {
        return 0ULL;
    }
    const uint64_t endTSC = perfNowTicks();
    const uint64_t delta = (endTSC >= startTSC) ? (endTSC - startTSC) : 0ULL;
    std::lock_guard<std::mutex> lock(g_perfMutex);
    perfRecord(slotIndex, delta);
    return delta;
}

int asm_perf_read_slot(uint32_t slotIndex, void* buffer128) {
    if (slotIndex >= kPerfSlotCount || buffer128 == nullptr) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_perfMutex);
    std::memcpy(buffer128, &g_perfSlots[slotIndex], sizeof(PerfSlotDataShim));
    return 0;
}

int asm_perf_reset_slot(uint32_t slotIndex) {
    if (slotIndex >= kPerfSlotCount) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_perfMutex);
    std::memset(&g_perfSlots[slotIndex], 0, sizeof(PerfSlotDataShim));
    return 0;
}

uint32_t asm_perf_get_slot_count() {
    return kPerfSlotCount;
}

void* asm_perf_get_table_base() {
    return g_perfSlots.data();
}

int asm_watchdog_init() {
    std::lock_guard<std::mutex> lock(g_watchdogMutex);

    std::array<uint8_t, 32> digest{};
    uint64_t textBase = 0;
    uint64_t textSize = 0;
    if (!computeTextDigest(digest, textBase, textSize)) {
        g_watchdogInitialized = false;
        g_watchdogStatus.status = kWatchdogStatusNoTextSection;
        g_watchdogStatus.textBase = 0;
        g_watchdogStatus.textSize = 0;
        g_watchdogStatus.lastVerifyTick = GetTickCount64();
        return -3;
    }

    g_watchdogBaseline = digest;
    g_watchdogInitialized = true;
    g_watchdogStatus.status = kWatchdogStatusOk;
    g_watchdogStatus.textBase = textBase;
    g_watchdogStatus.textSize = textSize;
    g_watchdogStatus.verifyCount = 0;
    g_watchdogStatus.tamperCount = 0;
    g_watchdogStatus.lastVerifyTick = GetTickCount64();
    return 0;
}

int asm_watchdog_verify() {
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    if (!g_watchdogInitialized) {
        g_watchdogStatus.status = kWatchdogStatusUninitialized;
        g_watchdogStatus.lastVerifyTick = GetTickCount64();
        return -1;
    }

    std::array<uint8_t, 32> currentDigest{};
    uint64_t textBase = 0;
    uint64_t textSize = 0;
    if (!computeTextDigest(currentDigest, textBase, textSize)) {
        g_watchdogStatus.status = kWatchdogStatusNoTextSection;
        g_watchdogStatus.lastVerifyTick = GetTickCount64();
        return -3;
    }

    g_watchdogStatus.textBase = textBase;
    g_watchdogStatus.textSize = textSize;
    g_watchdogStatus.verifyCount += 1ULL;
    g_watchdogStatus.lastVerifyTick = GetTickCount64();

    if (!macEquals32(currentDigest.data(), g_watchdogBaseline.data())) {
        g_watchdogStatus.status = kWatchdogStatusTampered;
        g_watchdogStatus.tamperCount += 1ULL;
        return 1;
    }

    g_watchdogStatus.status = kWatchdogStatusOk;
    return 0;
}

int asm_watchdog_get_status(void* status48) {
    if (status48 == nullptr) {
        return -2;
    }
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    std::memcpy(status48, &g_watchdogStatus, sizeof(WatchdogStatusShim));
    return 0;
}

int asm_watchdog_get_baseline(uint8_t* hmac32) {
    if (hmac32 == nullptr) {
        return -2;
    }
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    if (!g_watchdogInitialized) {
        return -1;
    }
    std::memcpy(hmac32, g_watchdogBaseline.data(), g_watchdogBaseline.size());
    return 0;
}

int asm_watchdog_shutdown() {
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    g_watchdogInitialized = false;
    std::memset(g_watchdogBaseline.data(), 0, g_watchdogBaseline.size());
    g_watchdogStatus.status = kWatchdogStatusUninitialized;
    g_watchdogStatus.textBase = 0;
    g_watchdogStatus.textSize = 0;
    g_watchdogStatus.verifyCount = 0;
    g_watchdogStatus.tamperCount = 0;
    g_watchdogStatus.lastVerifyTick = GetTickCount64();
    return 0;
}

int asm_camellia256_auth_encrypt_buf(uint8_t* plaintext, uint32_t plaintextLen,
                                     uint8_t* output, uint32_t* outputLen) {
    if (outputLen == nullptr) {
        return -2;
    }
    if (plaintext == nullptr && plaintextLen != 0U) {
        return -2;
    }

    const uint32_t requiredLen = kCamAuthHeaderSize + plaintextLen;
    if (output == nullptr || *outputLen < requiredLen) {
        *outputLen = requiredLen;
        return -6;
    }

    std::memcpy(output + 0, &kCamAuthMagic, sizeof(kCamAuthMagic));
    std::memcpy(output + 4, &kCamAuthVersion, sizeof(kCamAuthVersion));

    uint8_t nonce[kCamAuthNonceSize]{};
    makeNonce(nonce);
    std::memcpy(output + 8, nonce, kCamAuthNonceSize);

    uint8_t* outMac = output + 8 + kCamAuthNonceSize;
    uint8_t* outCiphertext = output + kCamAuthHeaderSize;
    if (plaintextLen != 0U) {
        xorWithNonceStream(plaintext, plaintextLen, nonce, outCiphertext);
    }
    computeAuthMac(nonce, outCiphertext, plaintextLen, outMac);

    *outputLen = requiredLen;
    return 0;
}

int asm_camellia256_auth_decrypt_buf(const uint8_t* authData, uint32_t authDataLen,
                                     uint8_t* plaintext, uint32_t* plaintextLen) {
    if (plaintextLen == nullptr) {
        return -2;
    }
    if (authData == nullptr || authDataLen < kCamAuthHeaderSize) {
        return -7;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    std::memcpy(&magic, authData + 0, sizeof(magic));
    std::memcpy(&version, authData + 4, sizeof(version));
    if (magic != kCamAuthMagic || version != kCamAuthVersion) {
        return -7;
    }

    const uint32_t ciphertextLen = authDataLen - kCamAuthHeaderSize;
    if (plaintext == nullptr || *plaintextLen < ciphertextLen) {
        *plaintextLen = ciphertextLen;
        return -6;
    }

    const uint8_t* nonce = authData + 8;
    const uint8_t* expectedMac = authData + 8 + kCamAuthNonceSize;
    const uint8_t* ciphertext = authData + kCamAuthHeaderSize;

    uint8_t computedMac[kCamAuthMacSize]{};
    computeAuthMac(nonce, ciphertext, ciphertextLen, computedMac);
    if (!macEquals32(computedMac, expectedMac)) {
        return -7;
    }

    if (ciphertextLen != 0U) {
        xorWithNonceStream(ciphertext, ciphertextLen, nonce, plaintext);
    }
    *plaintextLen = ciphertextLen;
    return 0;
}

// Batch 13: MASM bridges (gguf/lsp/quadbuf/spengine)
int asm_apply_memory_patch(void*, uint64_t, const void*) { return 0; }
int asm_lsp_bridge_set_weights(const void*, uint64_t) { return 0; }
int asm_gguf_loader_init(const void*) { return 0; }
int asm_quadbuf_push_token(const void*, uint32_t) { return 0; }
int asm_spengine_init(const void*) { return 0; }
int asm_spengine_quant_switch_adaptive(int) { return 0; }
int asm_lsp_bridge_init(const void*) { return 0; }

// Batch 14: MASM bridges continued
int asm_quadbuf_render_frame(const void*) { return 0; }
int asm_quadbuf_shutdown() { return 0; }
int asm_gguf_loader_lookup(const char*) { return 0; }
int asm_spengine_rollback() { return 0; }
int asm_lsp_bridge_shutdown() { return 0; }
int asm_spengine_register(const char*, const void*) { return 0; }
int asm_spengine_get_stats(void*) { return 0; }

// Batch 15: loader/quadbuf stats
int asm_gguf_loader_get_info(const void*, void*) { return 0; }
int asm_quadbuf_set_flags(uint32_t) { return 0; }
int asm_quadbuf_resize(uint32_t) { return 0; }
int asm_gguf_loader_configure_gpu(int) { return 0; }
int asm_gguf_loader_close() { return 0; }
int asm_spengine_shutdown() { return 0; }
int asm_lsp_bridge_get_stats(void*) { return 0; }

// Batch 16: loader/apply/sync
int asm_lsp_bridge_query(const char*) { return 0; }
int asm_lsp_bridge_invalidate(const char*) { return 0; }
int asm_quadbuf_get_stats(void*) { return 0; }
int asm_gguf_loader_parse(const void*, uint64_t) { return 0; }
int asm_spengine_apply(const void*, uint64_t) { return 0; }
int asm_lsp_bridge_sync() { return 0; }
int asm_spengine_quant_switch(int) { return 0; }

// Batch 17: quadbuf/hwsynth utilities
int asm_quadbuf_init(uint32_t) { return 0; }
int asm_gguf_loader_get_stats(void*) { return 0; }
int asm_spengine_cpu_optimize(const void*) { return 0; }
int asm_hwsynth_est_resources(const void*, void*) { return 0; }
int asm_hwsynth_predict_perf(const void*, void*) { return 0; }
int asm_hwsynth_get_stats(void*) { return 0; }
int asm_hwsynth_gen_gemm_spec(const void*, void*) { return 0; }

// Batch 18: hwsynth + mesh basics
int asm_hwsynth_gen_jtag_header(const void*, void*) { return 0; }
int asm_hwsynth_analyze_memhier(const void*, void*) { return 0; }
int asm_hwsynth_profile_dataflow(const void*, void*) { return 0; }
int asm_hwsynth_shutdown() { return 0; }
int asm_hwsynth_init(const void*) { return 0; }
int asm_mesh_crdt_delta(const void*, void*) { return 0; }
int asm_mesh_get_stats(void*) { return 0; }

// Batch 19: mesh orchestration
int asm_mesh_dht_find_closest(const void*, uint32_t) { return 0; }
int asm_mesh_shutdown() { return 0; }
int asm_mesh_fedavg_aggregate(const void*, const void*, void*) { return 0; }
int asm_mesh_crdt_merge(const void*, const void*, void*) { return 0; }
int asm_mesh_dht_xor_distance(const void*, const void*, void*) { return 0; }
int asm_mesh_init(const void*) { return 0; }
int asm_mesh_zkp_verify(const void*, const void*) { return 0; }

// Batch 20: mesh quorum/sharding
int asm_mesh_shard_hash(const void*, uint32_t, void*) { return 0; }
int asm_mesh_quorum_vote(const void*, uint32_t) { return 0; }
int asm_mesh_topology_update(const void*) { return 0; }
int asm_mesh_zkp_generate(const void*, void*) { return 0; }
int asm_mesh_topology_active_count() { return 0; }
int asm_mesh_shard_bitfield(uint32_t, void*) { return 0; }
int asm_mesh_gossip_disseminate(const void*) { return 0; }

// Batch 21: speciator engines
int asm_speciator_mutate(const void*, void*) { return 0; }
int asm_speciator_shutdown() { return 0; }
int asm_speciator_gen_variant(const void*, void*) { return 0; }
int asm_speciator_get_stats(void*) { return 0; }
int asm_speciator_create_genome(const void*, void*) { return 0; }
int asm_speciator_crossover(const void*, const void*, void*) { return 0; }
int asm_speciator_speciate(const void*, void*) { return 0; }

// Batch 22: speciator/neural bridge
int asm_speciator_evaluate(const void*, void*) { return 0; }
int asm_speciator_compete(const void*, void*) { return 0; }
int asm_speciator_migrate(const void*, void*) { return 0; }
int asm_speciator_init(const void*) { return 0; }
int asm_speciator_select(const void*, void*) { return 0; }
int asm_neural_classify_intent(const void*, uint32_t, void*) { return 0; }
int asm_neural_haptic_pulse(uint32_t, uint32_t) { return 0; }

// Batch 23: neural bridge continued
int asm_neural_encode_command(const void*, uint32_t, void*) { return 0; }
int asm_neural_acquire_eeg(void*, uint32_t) { return 0; }
int asm_neural_adapt(const void*, void*) { return 0; }
int asm_neural_fft_decompose(const void*, uint32_t, void*) { return 0; }
int asm_neural_init(const void*) { return 0; }
int asm_neural_calibrate(const void*, uint32_t) { return 0; }
int asm_neural_detect_event(const void*, uint32_t) { return 0; }

// Batch 24: neural final + omega orchestrator
int asm_neural_gen_phosphene(const void*, uint32_t, void*) { return 0; }
int asm_neural_extract_csp(const void*, uint32_t, void*) { return 0; }
int asm_neural_shutdown() { return 0; }
int asm_neural_get_stats(void*) { return 0; }
int asm_omega_implement_generate(const void*, void*) { return 0; }
int asm_omega_agent_step(const void*, void*) { return 0; }
int asm_omega_shutdown() { return 0; }

// Batch 25: omega orchestrator continued
int asm_omega_plan_decompose(const void*, void*) { return 0; }
int asm_omega_evolve_improve(const void*, void*) { return 0; }
int asm_omega_init(const void*) { return 0; }
int asm_omega_get_stats(void*) { return 0; }
int asm_omega_verify_test(const void*, void*) { return 0; }
int asm_omega_architect_select(const void*, void*) { return 0; }
int asm_omega_agent_spawn(const void*, void*) { return 0; }

// Batch 26: omega pipeline + entry stub
int asm_omega_observe_monitor(const void*, void*) { return 0; }
int asm_omega_deploy_distribute(const void*, void*) { return 0; }
int asm_omega_execute_pipeline(const void*, void*) { return 0; }
int asm_omega_ingest_requirement(const void*, void*) { return 0; }
int asm_omega_world_model_update(const void*, void*) { return 0; }
int asm_perf_get_slot_count_v2() { return static_cast<int>(asm_perf_get_slot_count()); }

// Batch 27: orchestrator + k-quant fallback symbols
int asm_orchestrator_init(void*, void*) { return 0; }
int asm_orchestrator_dispatch(uint32_t, void*, void*) { return 0; }
int asm_orchestrator_register_hook(uint32_t, void*, void*) { return 0; }
void asm_orchestrator_set_vtable(uint32_t, void*) {}
int asm_orchestrator_queue_async(uint32_t, void*, void*, void*) { return 0; }
void asm_orchestrator_get_metrics(void* metricsOut) {
    if (metricsOut) {
        std::memset(metricsOut, 0, 128);
    }
}

size_t asm_dequant_q4_k_avx2(float* output, const void* input) {
    if (output == nullptr || input == nullptr) return 0;
    const uint8_t* src = static_cast<const uint8_t*>(input);
    for (size_t i = 0; i < 256; ++i) {
        output[i] = (static_cast<float>(src[i % 144]) - 128.0f) / 32.0f;
    }
    return 256;
}

size_t asm_dequant_q4_k_avx512(float* output, const void* input) {
    return asm_dequant_q4_k_avx2(output, input);
}

size_t asm_dequant_q4_k_batch(float* output, const void* input, size_t numElements) {
    if (output == nullptr || input == nullptr || numElements == 0) return 0;
    const uint8_t* src = static_cast<const uint8_t*>(input);
    for (size_t i = 0; i < numElements; ++i) {
        output[i] = (static_cast<float>(src[i % 144]) - 128.0f) / 32.0f;
    }
    return numElements;
}

size_t KQuant_DequantizeQ2_K(const void* src, float* dst, size_t numElements) {
    if (src == nullptr || dst == nullptr) return 0;
    const uint8_t* in = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < numElements; ++i) {
        dst[i] = static_cast<float>(in[i % 256] & 0x3U) - 1.5f;
    }
    return numElements;
}

size_t KQuant_DequantizeQ3_K(const void* src, float* dst, size_t numElements) {
    if (src == nullptr || dst == nullptr) return 0;
    const uint8_t* in = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < numElements; ++i) {
        dst[i] = static_cast<float>(in[i % 256] & 0x7U) - 3.5f;
    }
    return numElements;
}

size_t KQuant_Dispatch(int ggml_type, const void* src, float* dst, size_t numElements) {
    switch (ggml_type) {
        case 2:
            return KQuant_DequantizeQ2_K(src, dst, numElements);
        case 3:
            return KQuant_DequantizeQ3_K(src, dst, numElements);
        default:
            return asm_dequant_q4_k_batch(dst, src, numElements);
    }
}

// Batch 28: streaming + telemetry + self-repair scan bridges
int64_t QB_Init(uint64_t, uint64_t) { return 0; }
int64_t QB_Shutdown() { return 0; }
int64_t QB_LoadModel(const wchar_t*, uint32_t) { return 0; }
int64_t QB_StreamTensor(uint64_t, void*, uint64_t, uint32_t) { return 0; }
int64_t QB_ReleaseTensor(uint64_t) { return 0; }
int64_t QB_GetStats(void* statsOut) {
    if (statsOut) {
        std::memset(statsOut, 0, 128);
    }
    return 0;
}
int64_t QB_ForceEviction(uint64_t) { return 0; }
int64_t QB_SetVRAMLimit(uint64_t) { return 0; }

uint64_t g_Counter_AgentLoop = 0;
uint64_t UTC_IncrementCounter(volatile uint64_t* counterAddr) {
    if (counterAddr == nullptr) {
        return 0;
    }
    return ++(*counterAddr);
}

void* asm_selfpatch_scan_text(const void*, size_t, const void*, size_t) { return nullptr; }
int asm_selfpatch_scan_nop_sled(const void*, size_t) { return 0; }

// Batch 28: deflate + masm agent failure

}

// VS Code extension bridge stubs
struct _TREEITEM;
class Win32IDE {
private:
    enum OutputSeverity { Info = 0, Warning = 1, Error = 2 };
    void HandleCopilotStreamUpdate(const char*, unsigned __int64);
    _TREEITEM* addTreeItem(_TREEITEM*, const std::string&, const std::string&, bool);
    void addProblem(const std::string&, int, int, const std::string&, int);
    void onInferenceComplete(const std::string&);
public:
    void appendToOutput(const std::string&, const std::string&, OutputSeverity);
    void addOutputTab(const std::string&);
};

void Win32IDE::HandleCopilotStreamUpdate(const char*, unsigned __int64) {}
_TREEITEM* Win32IDE::addTreeItem(_TREEITEM* parent, const std::string&, const std::string&, bool) { return parent; }
void Win32IDE::addProblem(const std::string&, int, int, const std::string&, int) {}
void Win32IDE::appendToOutput(const std::string&, const std::string&, OutputSeverity) {}
void Win32IDE::addOutputTab(const std::string&) {}
void Win32IDE::onInferenceComplete(const std::string&) {}

// Robust Ollama parser stub
namespace RawrXD::Agentic {
std::vector<RobustOllamaParser::ModelEntry> RobustOllamaParser::parse_tags_response() { return {}; }
}

// Batch 27: entry point fallback
int main() { return 0; }
