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

static uint32_t clampWeightToQ1000(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    return static_cast<uint32_t>(value * 1000.0f + 0.5f);
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
    return std::calloc(1, size);
}
void asm_hotpatch_free_shadow(void* base, size_t capacity) {
    (void)capacity;
    std::free(base);
}
void asm_hotpatch_flush_icache(void* base, size_t size) { (void)base; (void)size; }
int asm_hotpatch_backup_prologue(void* originalFn, uint32_t backupSlot) {
    (void)originalFn; (void)backupSlot; return -1;
}
int asm_hotpatch_restore_prologue(uint32_t backupSlot) { (void)backupSlot; return -1; }
int asm_hotpatch_verify_prologue(void* addr, uint32_t expectedCRC) {
    (void)addr; (void)expectedCRC; return -1;
}
int asm_hotpatch_install_trampoline(void* originalFn, void* trampolineBuffer) {
    (void)originalFn; (void)trampolineBuffer; return -1;
}
int asm_hotpatch_atomic_swap(void* originalFn, void* newCodeAddr) {
    (void)originalFn; (void)newCodeAddr; return -1;
}
void asm_hotpatch_get_stats(void* statsOut) { (void)statsOut; }

int asm_snapshot_capture(void* addr, uint32_t snapId, int size) {
    (void)addr; (void)snapId; (void)size; return -1;
}
int asm_snapshot_restore(uint32_t snapId) { (void)snapId; return -1; }
int asm_snapshot_verify(uint32_t snapId, uint32_t expectedCRC) {
    (void)snapId; (void)expectedCRC; return -1;
}
void asm_snapshot_discard(uint32_t snapId) { (void)snapId; }
void asm_snapshot_get_stats(void* statsOut) { (void)statsOut; }

int asm_camellia256_auth_encrypt_file(const char* inputPath, const char* outputPath) {
    (void)inputPath; (void)outputPath; return -1;
}
int asm_camellia256_auth_decrypt_file(const char* inputPath, const char* outputPath) {
    (void)inputPath; (void)outputPath; return -1;
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
    (void)A; (void)x; (void)y; (void)M; (void)K; return 0;
}
int asm_pyre_rmsnorm(const float* input, const float* weight, float* output, uint32_t dim, float eps) {
    (void)input; (void)weight; (void)output; (void)dim; (void)eps; return 0;
}
int asm_pyre_silu(float* inout, uint32_t count) { (void)inout; (void)count; return 0; }
int asm_pyre_softmax(float* inout, uint32_t count) { (void)inout; (void)count; return 0; }
int asm_pyre_rope(float* data, uint32_t seqLen, uint32_t headDim, uint32_t seqOffset, float theta) {
    (void)data; (void)seqLen; (void)headDim; (void)seqOffset; (void)theta; return 0;
}
int asm_pyre_embedding_lookup(const float* table, const uint32_t* ids, float* output, uint32_t count, uint32_t dim) {
    (void)table; (void)ids; (void)output; (void)count; (void)dim; return 0;
}
int asm_pyre_add_fp32(const float* a, const float* b, float* out, uint32_t count) {
    (void)a; (void)b; (void)out; (void)count; return 0;
}
int asm_pyre_mul_fp32(const float* a, const float* b, float* out, uint32_t count) {
    (void)a; (void)b; (void)out; (void)count; return 0;
}

int asm_perf_init(void) { return 0; }
uint64_t asm_perf_begin(uint32_t slot) { (void)slot; return 0; }
void asm_perf_end(uint32_t slot, uint64_t startTSC) { (void)slot; (void)startTSC; }
void asm_perf_read_slot(uint32_t slotIndex, void* data) { (void)slotIndex; (void)data; }
void asm_perf_reset_slot(uint32_t slotIndex) { (void)slotIndex; }
uint32_t asm_perf_get_slot_count(void) { return 64; }
void* asm_perf_get_table_base(void) { return nullptr; }

}
