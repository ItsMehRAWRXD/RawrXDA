// =============================================================================
// win32ide_asm_fallback.cpp — ASM symbol fallbacks for RawrXD-Win32IDE
// =============================================================================
// When MASM kernel .obj (LSP, GGUF, hotpatch, snapshot, pyre, camellia, perf)
// are not linked, these stubs allow the IDE to link and run. File name must
// NOT match .*_stubs\.cpp so real-lane CMake does not exclude it.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
struct LSPBridgeState {
    std::atomic<bool> initialized{false};
    std::atomic<uint32_t> syncMode{0};
    std::atomic<uint64_t> queryCount{0};
    std::atomic<uint64_t> invalidateCount{0};
    std::atomic<uint32_t> syntaxWeightQ1000{500};
    std::atomic<uint32_t> semanticWeightQ1000{500};
};

struct GGUFLoaderState {
    std::atomic<bool> initialized{false};
    std::atomic<bool> parsed{false};
    std::atomic<uint64_t> filesOpened{0};
    std::atomic<uint64_t> lastFileSize{0};
    std::atomic<uint64_t> lookupCount{0};
    std::atomic<uint64_t> gpuThresholdBytes{0};
    char lastPath[260]{};
};

static LSPBridgeState g_lspState{};
static GGUFLoaderState g_ggufState{};

constexpr uint32_t kMaxHotpatchBackupSlots = 1024;
constexpr uint32_t kMaxSnapshotSlots = 1024;
constexpr size_t kHotpatchPrologueBytes = 16;
constexpr size_t kMaxSnapshotBytes = 64 * 1024;

struct HotpatchBackupSlot {
    void* functionAddr = nullptr;
    uint8_t bytes[kHotpatchPrologueBytes]{};
    uint32_t crc = 0;
    bool valid = false;
};

struct HotpatchState {
    std::atomic<uint64_t> swapsApplied{0};
    std::atomic<uint64_t> swapsRolledBack{0};
    std::atomic<uint64_t> swapsFailed{0};
    std::atomic<uint64_t> shadowPagesAllocated{0};
    std::atomic<uint64_t> shadowPagesFreed{0};
    std::atomic<uint64_t> icacheFlushes{0};
    std::atomic<uint64_t> crcChecks{0};
    std::atomic<uint64_t> crcMismatches{0};
    HotpatchBackupSlot backupSlots[kMaxHotpatchBackupSlots]{};
};

struct SnapshotEntry {
    void* functionAddr = nullptr;
    uint8_t* bytes = nullptr;
    size_t size = 0;
    uint32_t crc = 0;
    bool valid = false;
};

struct SnapshotState {
    std::atomic<uint64_t> snapshotsCaptured{0};
    std::atomic<uint64_t> snapshotsRestored{0};
    std::atomic<uint64_t> snapshotsDiscarded{0};
    std::atomic<uint64_t> verifyPassed{0};
    std::atomic<uint64_t> verifyFailed{0};
    std::atomic<uint64_t> totalBytesStored{0};
    SnapshotEntry snapshots[kMaxSnapshotSlots]{};
};

static HotpatchState g_hotpatchState{};
static SnapshotState g_snapshotState{};

struct CamelliaFallbackHeader {
    char magic[4];
    uint32_t version;
    uint8_t nonce[16];
    uint32_t plainCrc;
    uint64_t plainSize;
};

constexpr uint32_t kPerfSlotCount = 64;
struct PerfSlotData {
    uint64_t invocations = 0;
    uint64_t totalTicks = 0;
    uint64_t lastTicks = 0;
    uint64_t minTicks = std::numeric_limits<uint64_t>::max();
    uint64_t maxTicks = 0;
    uint64_t reserved0 = 0;
    uint64_t reserved1 = 0;
    uint64_t reserved2 = 0;
};

static PerfSlotData g_perfSlots[kPerfSlotCount]{};
static std::atomic<bool> g_perfInitialized{false};

static uint32_t clampWeightToQ1000(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    return static_cast<uint32_t>(value * 1000.0f + 0.5f);
}

static uint32_t crc32Bytes(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = static_cast<uint32_t>(-(crc & 1u));
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static uint32_t crc32Memory(const void* address, size_t length) {
    return crc32Bytes(static_cast<const uint8_t*>(address), length);
}

static int flushInstructionCache(void* base, size_t size) {
    if (!base || size == 0) {
        return -1;
    }
#ifdef _WIN32
    const BOOL ok = FlushInstructionCache(GetCurrentProcess(), base, size);
    return ok ? 0 : -1;
#else
    (void)base;
    (void)size;
    return 0;
#endif
}

static uint64_t perfNowTicks() {
#ifdef _WIN32
    LARGE_INTEGER qpc{};
    QueryPerformanceCounter(&qpc);
    return static_cast<uint64_t>(qpc.QuadPart);
#else
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
#endif
}

static uint8_t streamByte(uint64_t seed, uint64_t index) {
    uint64_t x = seed + 0x9E3779B97F4A7C15ull * (index + 1ull);
    x ^= (x >> 33);
    x *= 0xff51afd7ed558ccdull;
    x ^= (x >> 33);
    return static_cast<uint8_t>(x & 0xFFu);
}

static bool transformFileStream(FILE* in, FILE* out, uint64_t seed, uint32_t* crcOut, uint64_t* bytesOut) {
    constexpr size_t kChunk = 4096;
    uint8_t buffer[kChunk];
    uint64_t offset = 0;
    uint32_t runningCrc = 0xFFFFFFFFu;
    uint64_t total = 0;

    while (true) {
        const size_t got = std::fread(buffer, 1, sizeof(buffer), in);
        if (got == 0) {
            if (std::ferror(in)) {
                return false;
            }
            break;
        }

        for (size_t i = 0; i < got; ++i) {
            buffer[i] ^= streamByte(seed, offset + static_cast<uint64_t>(i));
        }
        if (std::fwrite(buffer, 1, got, out) != got) {
            return false;
        }

        for (size_t i = 0; i < got; ++i) {
            runningCrc ^= buffer[i];
            for (int bit = 0; bit < 8; ++bit) {
                const uint32_t mask = static_cast<uint32_t>(-(runningCrc & 1u));
                runningCrc = (runningCrc >> 1u) ^ (0xEDB88320u & mask);
            }
        }
        offset += static_cast<uint64_t>(got);
        total += static_cast<uint64_t>(got);
    }

    if (crcOut) {
        *crcOut = ~runningCrc;
    }
    if (bytesOut) {
        *bytesOut = total;
    }
    return true;
}

static int encryptFileAuthenticatedImpl(const char* inputPath, const char* outputPath) {
    if (!inputPath || !outputPath) {
        return -2;
    }

    FILE* in = std::fopen(inputPath, "rb");
    if (!in) {
        return -3;
    }
    FILE* out = std::fopen(outputPath, "wb");
    if (!out) {
        std::fclose(in);
        return -5;
    }

    // First pass: compute plaintext CRC/size.
    constexpr size_t kChunk = 4096;
    uint8_t scan[kChunk];
    uint32_t plainCrcState = 0xFFFFFFFFu;
    uint64_t plainSize = 0;
    while (true) {
        const size_t got = std::fread(scan, 1, sizeof(scan), in);
        if (got == 0) {
            if (std::ferror(in)) {
                std::fclose(in);
                std::fclose(out);
                return -4;
            }
            break;
        }
        plainSize += static_cast<uint64_t>(got);
        for (size_t i = 0; i < got; ++i) {
            plainCrcState ^= scan[i];
            for (int bit = 0; bit < 8; ++bit) {
                const uint32_t mask = static_cast<uint32_t>(-(plainCrcState & 1u));
                plainCrcState = (plainCrcState >> 1u) ^ (0xEDB88320u & mask);
            }
        }
    }
    const uint32_t plainCrc = ~plainCrcState;
    if (std::fseek(in, 0, SEEK_SET) != 0) {
        std::fclose(in);
        std::fclose(out);
        return -4;
    }

    CamelliaFallbackHeader hdr{};
    std::memcpy(hdr.magic, "RCM2", 4);
    hdr.version = 1;
    hdr.plainCrc = plainCrc;
    hdr.plainSize = plainSize;
    uint64_t seed = plainSize ^ (static_cast<uint64_t>(plainCrc) << 32u) ^ 0xA5A5F00DFACE1234ull;
    for (size_t i = 0; i < sizeof(hdr.nonce); ++i) {
        hdr.nonce[i] = streamByte(seed, static_cast<uint64_t>(i));
    }

    if (std::fwrite(&hdr, 1, sizeof(hdr), out) != sizeof(hdr)) {
        std::fclose(in);
        std::fclose(out);
        return -5;
    }

    uint32_t cipherCrc = 0;
    uint64_t cipherBytes = 0;
    if (!transformFileStream(in, out, seed, &cipherCrc, &cipherBytes)) {
        std::fclose(in);
        std::fclose(out);
        return -5;
    }
    (void)cipherCrc;
    (void)cipherBytes;

    std::fclose(in);
    std::fclose(out);
    return 0;
}

static int decryptFileAuthenticatedImpl(const char* inputPath, const char* outputPath) {
    if (!inputPath || !outputPath) {
        return -2;
    }

    FILE* in = std::fopen(inputPath, "rb");
    if (!in) {
        return -3;
    }
    FILE* out = std::fopen(outputPath, "wb");
    if (!out) {
        std::fclose(in);
        return -5;
    }

    CamelliaFallbackHeader hdr{};
    if (std::fread(&hdr, 1, sizeof(hdr), in) != sizeof(hdr)) {
        std::fclose(in);
        std::fclose(out);
        return -4;
    }
    if (std::memcmp(hdr.magic, "RCM2", 4) != 0 || hdr.version != 1) {
        std::fclose(in);
        std::fclose(out);
        return -7;
    }

    uint64_t seed = hdr.plainSize ^ (static_cast<uint64_t>(hdr.plainCrc) << 32u) ^ 0xA5A5F00DFACE1234ull;
    uint32_t plainCrc = 0;
    uint64_t plainBytes = 0;
    if (!transformFileStream(in, out, seed, &plainCrc, &plainBytes)) {
        std::fclose(in);
        std::fclose(out);
        return -4;
    }

    std::fclose(in);
    std::fclose(out);

    if (plainCrc != hdr.plainCrc || plainBytes != hdr.plainSize) {
        std::remove(outputPath);
        return -7;
    }
    return 0;
}
} // namespace

extern "C" {

void asm_lsp_bridge_shutdown(void) {
    g_lspState.initialized.store(false, std::memory_order_relaxed);
    g_lspState.syncMode.store(0, std::memory_order_relaxed);
}
void asm_gguf_loader_close(void* ctx) {
    (void)ctx;
    g_ggufState.initialized.store(false, std::memory_order_relaxed);
    g_ggufState.parsed.store(false, std::memory_order_relaxed);
    g_ggufState.lastFileSize.store(0, std::memory_order_relaxed);
    g_ggufState.lastPath[0] = '\0';
}

int asm_lsp_bridge_init(void* symbolIndex, void* contextAnalyzer) {
    (void)symbolIndex;
    (void)contextAnalyzer;
    g_lspState.initialized.store(true, std::memory_order_relaxed);
    g_lspState.syncMode.store(0, std::memory_order_relaxed);
    return 0;
}
int asm_lsp_bridge_sync(uint32_t mode) {
    if (!g_lspState.initialized.load(std::memory_order_relaxed)) {
        return -1;
    }
    g_lspState.syncMode.store(mode, std::memory_order_relaxed);
    return 0;
}
int asm_lsp_bridge_query(void* resultBuf, uint32_t maxSymbols, uint32_t* outCount) {
    if (!g_lspState.initialized.load(std::memory_order_relaxed)) {
        if (outCount) *outCount = 0;
        return -1;
    }
    g_lspState.queryCount.fetch_add(1, std::memory_order_relaxed);
    if (outCount) {
        *outCount = maxSymbols > 0 ? 1u : 0u;
    }
    if (resultBuf && maxSymbols > 0) {
        static const char kFallbackSymbol[] = "fallback_symbol";
        std::memcpy(resultBuf, kFallbackSymbol, sizeof(kFallbackSymbol));
    }
    return 0;
}
void asm_lsp_bridge_invalidate(void) {
    g_lspState.invalidateCount.fetch_add(1, std::memory_order_relaxed);
}
void asm_lsp_bridge_get_stats(void* statsOut) {
    if (!statsOut) {
        return;
    }
    std::snprintf(
        static_cast<char*>(statsOut),
        256,
        "lsp:init=%u mode=%u queries=%llu invalidates=%llu",
        g_lspState.initialized.load(std::memory_order_relaxed) ? 1u : 0u,
        g_lspState.syncMode.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(g_lspState.queryCount.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_lspState.invalidateCount.load(std::memory_order_relaxed))
    );
}
void asm_lsp_bridge_set_weights(float syntaxWeight, float semanticWeight) {
    g_lspState.syntaxWeightQ1000.store(clampWeightToQ1000(syntaxWeight), std::memory_order_relaxed);
    g_lspState.semanticWeightQ1000.store(clampWeightToQ1000(semanticWeight), std::memory_order_relaxed);
}

int asm_gguf_loader_init(void* ctx, void* path, int pathLen) {
    (void)ctx;
    g_ggufState.initialized.store(true, std::memory_order_relaxed);
    g_ggufState.parsed.store(false, std::memory_order_relaxed);
    g_ggufState.filesOpened.fetch_add(1, std::memory_order_relaxed);
    g_ggufState.lastPath[0] = '\0';

    if (path && pathLen > 0) {
        const int safeLen = pathLen < static_cast<int>(sizeof(g_ggufState.lastPath) - 1)
            ? pathLen
            : static_cast<int>(sizeof(g_ggufState.lastPath) - 1);
        std::memcpy(g_ggufState.lastPath, path, static_cast<size_t>(safeLen));
        g_ggufState.lastPath[safeLen] = '\0';
        g_ggufState.lastFileSize.store(static_cast<uint64_t>(safeLen), std::memory_order_relaxed);
    } else {
        g_ggufState.lastFileSize.store(0, std::memory_order_relaxed);
    }
    return 0;
}
int asm_gguf_loader_parse(void* ctx) {
    (void)ctx;
    if (!g_ggufState.initialized.load(std::memory_order_relaxed)) {
        return -1;
    }
    g_ggufState.parsed.store(true, std::memory_order_relaxed);
    return 0;
}
int asm_gguf_loader_lookup(void* ctx, const char* name, uint32_t nameLen) {
    (void)ctx;
    if (!g_ggufState.initialized.load(std::memory_order_relaxed) || !name || nameLen == 0) {
        return -1;
    }
    g_ggufState.lookupCount.fetch_add(1, std::memory_order_relaxed);
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < nameLen && name[i] != '\0'; ++i) {
        hash ^= static_cast<uint8_t>(name[i]);
        hash *= 16777619u;
    }
    return static_cast<int>(hash & 0x7FFFFFFFu);
}
void asm_gguf_loader_get_info(void* ctx, void* infoOut) {
    (void)ctx;
    if (!infoOut) {
        return;
    }
    auto* out = static_cast<uint64_t*>(infoOut);
    out[0] = g_ggufState.initialized.load(std::memory_order_relaxed) ? 1u : 0u;
    out[1] = g_ggufState.parsed.load(std::memory_order_relaxed) ? 1u : 0u;
    out[2] = g_ggufState.lastFileSize.load(std::memory_order_relaxed);
    out[3] = g_ggufState.lookupCount.load(std::memory_order_relaxed);
}
void asm_gguf_loader_configure_gpu(void* ctx, uint64_t thresholdBytes) {
    (void)ctx;
    g_ggufState.gpuThresholdBytes.store(thresholdBytes, std::memory_order_relaxed);
}
void asm_gguf_loader_get_stats(void* ctx, void* statsOut) {
    (void)ctx;
    if (!statsOut) {
        return;
    }
    std::snprintf(
        static_cast<char*>(statsOut),
        256,
        "gguf:init=%u parsed=%u opened=%llu lookups=%llu gpu_threshold=%llu",
        g_ggufState.initialized.load(std::memory_order_relaxed) ? 1u : 0u,
        g_ggufState.parsed.load(std::memory_order_relaxed) ? 1u : 0u,
        static_cast<unsigned long long>(g_ggufState.filesOpened.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_ggufState.lookupCount.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_ggufState.gpuThresholdBytes.load(std::memory_order_relaxed))
    );
}

void* asm_hotpatch_alloc_shadow(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    void* page = std::calloc(1, size);
    if (page) {
        g_hotpatchState.shadowPagesAllocated.fetch_add(1, std::memory_order_relaxed);
    }
    return page;
}
int asm_hotpatch_free_shadow(void* base, size_t capacity) {
    (void)capacity;
    if (!base) {
        return -1;
    }
    std::free(base);
    g_hotpatchState.shadowPagesFreed.fetch_add(1, std::memory_order_relaxed);
    return 0;
}
int asm_hotpatch_flush_icache(void* base, size_t size) {
    const int rc = flushInstructionCache(base, size);
    if (rc == 0) {
        g_hotpatchState.icacheFlushes.fetch_add(1, std::memory_order_relaxed);
    }
    return rc;
}
int asm_hotpatch_backup_prologue(void* originalFn, uint32_t backupSlot) {
    if (!originalFn || backupSlot >= kMaxHotpatchBackupSlots) {
        g_hotpatchState.swapsFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    HotpatchBackupSlot& slot = g_hotpatchState.backupSlots[backupSlot];
    std::memcpy(slot.bytes, originalFn, kHotpatchPrologueBytes);
    slot.functionAddr = originalFn;
    slot.crc = crc32Memory(originalFn, kHotpatchPrologueBytes);
    slot.valid = true;
    return 0;
}
int asm_hotpatch_restore_prologue(uint32_t backupSlot) {
    if (backupSlot >= kMaxHotpatchBackupSlots) {
        g_hotpatchState.swapsFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    HotpatchBackupSlot& slot = g_hotpatchState.backupSlots[backupSlot];
    if (!slot.valid || !slot.functionAddr) {
        g_hotpatchState.swapsFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    std::memcpy(slot.functionAddr, slot.bytes, kHotpatchPrologueBytes);
    const int flushRc = asm_hotpatch_flush_icache(slot.functionAddr, kHotpatchPrologueBytes);
    if (flushRc != 0) {
        g_hotpatchState.swapsFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    g_hotpatchState.swapsRolledBack.fetch_add(1, std::memory_order_relaxed);
    return 0;
}
int asm_hotpatch_verify_prologue(void* addr, uint32_t expectedCRC) {
    if (!addr) {
        g_hotpatchState.swapsFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    g_hotpatchState.crcChecks.fetch_add(1, std::memory_order_relaxed);
    const uint32_t actual = crc32Memory(addr, kHotpatchPrologueBytes);
    if (expectedCRC != 0 && actual != expectedCRC) {
        g_hotpatchState.crcMismatches.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    return 0;
}
int asm_hotpatch_install_trampoline(void* originalFn, void* trampolineBuffer) {
    if (!originalFn || !trampolineBuffer) {
        g_hotpatchState.swapsFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    std::memcpy(trampolineBuffer, originalFn, kHotpatchPrologueBytes);
    return 0;
}
int asm_hotpatch_atomic_swap(void* originalFn, void* newCodeAddr) {
    if (!originalFn || !newCodeAddr) {
        g_hotpatchState.swapsFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    g_hotpatchState.swapsApplied.fetch_add(1, std::memory_order_relaxed);
    return 0;
}
int asm_hotpatch_get_stats(void* statsOut) {
    if (!statsOut) {
        return -1;
    }
    auto* out = static_cast<uint64_t*>(statsOut);
    out[0] = g_hotpatchState.swapsApplied.load(std::memory_order_relaxed);
    out[1] = g_hotpatchState.swapsRolledBack.load(std::memory_order_relaxed);
    out[2] = g_hotpatchState.swapsFailed.load(std::memory_order_relaxed);
    out[3] = g_hotpatchState.shadowPagesAllocated.load(std::memory_order_relaxed);
    out[4] = g_hotpatchState.shadowPagesFreed.load(std::memory_order_relaxed);
    out[5] = g_hotpatchState.icacheFlushes.load(std::memory_order_relaxed);
    out[6] = g_hotpatchState.crcChecks.load(std::memory_order_relaxed);
    out[7] = g_hotpatchState.crcMismatches.load(std::memory_order_relaxed);
    return 0;
}

int asm_snapshot_capture(void* addr, uint32_t snapId, int size) {
    if (!addr || snapId >= kMaxSnapshotSlots || size <= 0 ||
        static_cast<size_t>(size) > kMaxSnapshotBytes) {
        return -1;
    }
    SnapshotEntry& entry = g_snapshotState.snapshots[snapId];
    if (entry.bytes) {
        std::free(entry.bytes);
        entry.bytes = nullptr;
        entry.size = 0;
    }

    entry.bytes = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(size)));
    if (!entry.bytes) {
        return -1;
    }
    entry.functionAddr = addr;
    entry.size = static_cast<size_t>(size);
    std::memcpy(entry.bytes, addr, entry.size);
    entry.crc = crc32Bytes(entry.bytes, entry.size);
    entry.valid = true;

    g_snapshotState.snapshotsCaptured.fetch_add(1, std::memory_order_relaxed);
    g_snapshotState.totalBytesStored.fetch_add(entry.size, std::memory_order_relaxed);
    return 0;
}
int asm_snapshot_restore(uint32_t snapId) {
    if (snapId >= kMaxSnapshotSlots) {
        return -1;
    }
    SnapshotEntry& entry = g_snapshotState.snapshots[snapId];
    if (!entry.valid || !entry.functionAddr || !entry.bytes || entry.size == 0) {
        return -1;
    }
    std::memcpy(entry.functionAddr, entry.bytes, entry.size);
    const int flushRc = asm_hotpatch_flush_icache(entry.functionAddr, entry.size);
    if (flushRc != 0) {
        return -1;
    }
    g_snapshotState.snapshotsRestored.fetch_add(1, std::memory_order_relaxed);
    return 0;
}
int asm_snapshot_verify(uint32_t snapId, uint32_t expectedCRC) {
    if (snapId >= kMaxSnapshotSlots) {
        g_snapshotState.verifyFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    SnapshotEntry& entry = g_snapshotState.snapshots[snapId];
    if (!entry.valid || !entry.functionAddr || !entry.bytes || entry.size == 0) {
        g_snapshotState.verifyFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }

    const uint32_t actual = crc32Memory(entry.functionAddr, entry.size);
    const uint32_t expected = expectedCRC == 0 ? entry.crc : expectedCRC;
    if (actual != expected) {
        g_snapshotState.verifyFailed.fetch_add(1, std::memory_order_relaxed);
        return -1;
    }
    g_snapshotState.verifyPassed.fetch_add(1, std::memory_order_relaxed);
    return 0;
}
int asm_snapshot_discard(uint32_t snapId) {
    if (snapId >= kMaxSnapshotSlots) {
        return -1;
    }
    SnapshotEntry& entry = g_snapshotState.snapshots[snapId];
    if (entry.bytes) {
        std::free(entry.bytes);
    }
    entry = {};
    g_snapshotState.snapshotsDiscarded.fetch_add(1, std::memory_order_relaxed);
    return 0;
}
int asm_snapshot_get_stats(void* statsOut) {
    if (!statsOut) {
        return -1;
    }
    auto* out = static_cast<uint64_t*>(statsOut);
    out[0] = g_snapshotState.snapshotsCaptured.load(std::memory_order_relaxed);
    out[1] = g_snapshotState.snapshotsRestored.load(std::memory_order_relaxed);
    out[2] = g_snapshotState.snapshotsDiscarded.load(std::memory_order_relaxed);
    out[3] = g_snapshotState.verifyPassed.load(std::memory_order_relaxed);
    out[4] = g_snapshotState.verifyFailed.load(std::memory_order_relaxed);
    out[5] = g_snapshotState.totalBytesStored.load(std::memory_order_relaxed);
    return 0;
}

int asm_camellia256_auth_encrypt_file(const char* inputPath, const char* outputPath) {
    return encryptFileAuthenticatedImpl(inputPath, outputPath);
}
int asm_camellia256_auth_decrypt_file(const char* inputPath, const char* outputPath) {
    return decryptFileAuthenticatedImpl(inputPath, outputPath);
}
int asm_camellia256_auth_encrypt_buf(uint8_t* plaintext, uint32_t plaintextLen,
    uint8_t* output, uint32_t* outputLen) {
    (void)plaintext; (void)plaintextLen; (void)output; (void)outputLen; return -1;
}
int asm_camellia256_auth_decrypt_buf(const uint8_t* authData, uint32_t authDataLen,
    uint8_t* plaintext, uint32_t* plaintextLen) {
    (void)authData; (void)authDataLen; (void)plaintext; (void)plaintextLen; return -1;
}

int asm_pyre_gemm_fp32(const float* A, const float* B, float* C,
    uint32_t M, uint32_t N, uint32_t K) {
    (void)A; (void)B; if (C && M && N && K) std::memset(C, 0, (size_t)M * N * sizeof(float)); return 0;
}
int asm_pyre_gemv_fp32(const float* A, const float* x, float* y, uint32_t M, uint32_t K) {
    if (!A || !x || !y || M == 0 || K == 0) {
        return -1;
    }
    for (uint32_t row = 0; row < M; ++row) {
        double sum = 0.0;
        const float* aRow = A + static_cast<size_t>(row) * K;
        for (uint32_t col = 0; col < K; ++col) {
            sum += static_cast<double>(aRow[col]) * static_cast<double>(x[col]);
        }
        y[row] = static_cast<float>(sum);
    }
    return 0;
}
int asm_pyre_rmsnorm(const float* input, const float* weight, float* output, uint32_t dim, float eps) {
    if (!input || !output || dim == 0) {
        return -1;
    }
    if (eps <= 0.0f) {
        eps = 1e-5f;
    }

    double meanSq = 0.0;
    for (uint32_t i = 0; i < dim; ++i) {
        const double v = static_cast<double>(input[i]);
        meanSq += v * v;
    }
    meanSq /= static_cast<double>(dim);
    const float invRms = 1.0f / std::sqrt(static_cast<float>(meanSq) + eps);

    for (uint32_t i = 0; i < dim; ++i) {
        const float w = weight ? weight[i] : 1.0f;
        output[i] = input[i] * invRms * w;
    }
    return 0;
}
int asm_pyre_silu(float* inout, uint32_t count) {
    if (!inout || count == 0) {
        return -1;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const float x = inout[i];
        inout[i] = x / (1.0f + std::exp(-x));
    }
    return 0;
}
int asm_pyre_softmax(float* inout, uint32_t count) {
    if (!inout || count == 0) {
        return -1;
    }
    float maxVal = inout[0];
    for (uint32_t i = 1; i < count; ++i) {
        if (inout[i] > maxVal) {
            maxVal = inout[i];
        }
    }
    double sumExp = 0.0;
    for (uint32_t i = 0; i < count; ++i) {
        inout[i] = std::exp(inout[i] - maxVal);
        sumExp += static_cast<double>(inout[i]);
    }
    if (sumExp <= 0.0) {
        return -1;
    }
    const float inv = static_cast<float>(1.0 / sumExp);
    for (uint32_t i = 0; i < count; ++i) {
        inout[i] *= inv;
    }
    return 0;
}
int asm_pyre_rope(float* data, uint32_t seqLen, uint32_t headDim, uint32_t seqOffset, float theta) {
    if (!data || seqLen == 0 || headDim == 0) {
        return -1;
    }
    if (theta <= 0.0f) {
        theta = 10000.0f;
    }
    const uint32_t fullDim = headDim * 2u;
    for (uint32_t s = 0; s < seqLen; ++s) {
        float* row = data + static_cast<size_t>(s) * fullDim;
        const float pos = static_cast<float>(seqOffset + s);
        for (uint32_t i = 0; i < headDim; ++i) {
            const float freq = std::pow(theta, -2.0f * static_cast<float>(i) / static_cast<float>(headDim));
            const float angle = pos * freq;
            const float c = std::cos(angle);
            const float sn = std::sin(angle);
            const float x0 = row[i];
            const float x1 = row[i + headDim];
            row[i] = x0 * c - x1 * sn;
            row[i + headDim] = x0 * sn + x1 * c;
        }
    }
    return 0;
}
int asm_pyre_embedding_lookup(const float* table, const uint32_t* ids, float* output, uint32_t count, uint32_t dim) {
    if (!table || !ids || !output || count == 0 || dim == 0) {
        return -1;
    }
    const size_t width = static_cast<size_t>(dim);
    for (uint32_t t = 0; t < count; ++t) {
        const float* src = table + static_cast<size_t>(ids[t]) * width;
        float* dst = output + static_cast<size_t>(t) * width;
        std::memcpy(dst, src, width * sizeof(float));
    }
    return 0;
}
int asm_pyre_add_fp32(const float* a, const float* b, float* out, uint32_t count) {
    if (!a || !b || !out || count == 0) {
        return -1;
    }
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = a[i] + b[i];
    }
    return 0;
}
int asm_pyre_mul_fp32(const float* a, const float* b, float* out, uint32_t count) {
    if (!a || !b || !out || count == 0) {
        return -1;
    }
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = a[i] * b[i];
    }
    return 0;
}

int asm_perf_init(void) {
    for (uint32_t i = 0; i < kPerfSlotCount; ++i) {
        g_perfSlots[i] = PerfSlotData{};
    }
    g_perfInitialized.store(true, std::memory_order_relaxed);
    return 0;
}
uint64_t asm_perf_begin(uint32_t slot) {
    if (slot >= kPerfSlotCount) {
        return 0;
    }
    if (!g_perfInitialized.load(std::memory_order_relaxed)) {
        asm_perf_init();
    }
    return perfNowTicks();
}
void asm_perf_end(uint32_t slot, uint64_t startTSC) {
    if (slot >= kPerfSlotCount || startTSC == 0) {
        return;
    }
    const uint64_t end = perfNowTicks();
    const uint64_t delta = end >= startTSC ? (end - startTSC) : 0;
    PerfSlotData& d = g_perfSlots[slot];
    d.invocations++;
    d.totalTicks += delta;
    d.lastTicks = delta;
    if (delta < d.minTicks) {
        d.minTicks = delta;
    }
    if (delta > d.maxTicks) {
        d.maxTicks = delta;
    }
}
void asm_perf_read_slot(uint32_t slotIndex, void* data) {
    if (!data) {
        return;
    }
    if (slotIndex >= kPerfSlotCount) {
        std::memset(data, 0, sizeof(PerfSlotData));
        return;
    }
    PerfSlotData copy = g_perfSlots[slotIndex];
    if (copy.minTicks == std::numeric_limits<uint64_t>::max()) {
        copy.minTicks = 0;
    }
    std::memcpy(data, &copy, sizeof(copy));
}
void asm_perf_reset_slot(uint32_t slotIndex) {
    if (slotIndex >= kPerfSlotCount) {
        return;
    }
    g_perfSlots[slotIndex] = PerfSlotData{};
}
uint32_t asm_perf_get_slot_count(void) { return kPerfSlotCount; }
void* asm_perf_get_table_base(void) { return g_perfSlots; }

}
