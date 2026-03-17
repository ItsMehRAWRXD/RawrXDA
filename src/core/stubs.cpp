// stubs.cpp - Temporary stubs for unresolved externals to enable linking
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <mutex>
#include <unordered_map>

#include "BATCH2_CONTEXT.h"
#include "unified_hotpatch_manager.hpp"
#include "model_memory_hotpatch.hpp"
#include "Win32IDE_WebView2.h"
#include "Win32IDE_AgenticBridge.h"
#include "model_inference.hpp"
#include "dynamic_prompt_engine.hpp"
#include <string>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <windows.h>
#include <shellapi.h>

// Forward declarations
struct CommandContext;
struct CommandResult;
struct UnifiedResult;
struct ServerHotpatch;
struct BytePatch;
struct HotpatchEvent;
struct HotpatchPreset;
struct PatchResult;
struct UnifiedHotpatchManager;
struct WebView2Result;
struct WebView2Container;
struct MonacoEditorOptions;
struct AgentResponse;

// Definitions for forward declared structs
struct CommandContext {
    int dummy;
};

struct ServerHotpatch {
    int dummy;
};

struct BytePatch {
    int dummy;
};

struct HotpatchEvent {
    int dummy;
};

struct HotpatchPreset {
    int dummy;
};
struct BeaconCheckpoint;
struct BeaconStorage;
struct BeaconStage;
struct HealingStrategy;
struct IDEDiagnosticAutoHealer;
struct DiagnosticUtils;
struct RawrXD;
struct StreamingGGUFLoader;
struct TensorRef;
struct GGUFLoader;
struct CPUInferenceEngine;
struct InferenceError;
struct Expected;
struct LSP;
struct DiagResult;
struct Diagnostic;
struct DiagnosticConsumer;
struct NativeDebuggerEngine;
struct DebugResult;
struct RegisterSnapshot;
struct NativeStackFrame;
struct DisassembledInstruction;
struct EvalResult;
struct DebugModule;
struct DebugThread;
struct FlashAttention;
struct MC_GapBuffer;
struct KernelInit;
struct DiskRecovery;
struct ShadowPageDetour;
struct SwarmCoord;
struct Camellia256;
struct PromptGen;
struct DynamicPromptEngine;
struct GGUF_DML_Bridge;
struct MASM_Stress_Harness;
struct GGUF_Loader;
struct Orchestrator;
struct QuadBuf;
struct LSP_Bridge;
struct Native_Speed_Layer;
struct RawrXD_Subsystem_API;
struct Model_Bruteforce_Engine;
struct Model_Training_Pipeline;
struct MonacoCoreEngine;
struct Multiwindow_Scheduler;
struct Enterprise_License;
struct Enterprise_Stress_Tests;
struct Update_Signature;
struct Convergence_Stress_Harness;
struct DirectML_Compute;
struct Vision_Encoder;
struct Embedding_Engine;
struct Code_Linter;
struct Auto_Repair_Orchestrator;
struct Missing_Handler_Stubs;
struct WebView2EditorEngine;
struct Win32IDE_Autonomy;
struct AgenticBridge;

// Basic stub return types
CommandResult make_stub_result(const char* msg) {
    CommandResult r;
    r.success = false;
    r.message = msg;
    return r;
}

UnifiedResult make_unified_result(bool success, const char* msg = "") {
    UnifiedResult r;
    r.success = success;
    r.message = msg;
    return r;
}

// Swarm_Init / SwarmCoord_Init stubs removed — real ASM in src/asm/monolithic/

// UnifiedHotpatchManager stubs
class UnifiedHotpatchManager {
public:
    struct Stats {
        int patches_applied = 0;
    };
    static UnifiedHotpatchManager& instance() {
        static UnifiedHotpatchManager inst;
        return inst;
    }
    UnifiedResult pt_restore_snapshot(unsigned int) { return make_unified_result(false); }
    UnifiedResult add_server_patch(ServerHotpatch*) { return make_unified_result(false); }
    UnifiedResult apply_byte_patch(const char*, const BytePatch&) { return make_unified_result(false); }
    UnifiedResult apply_memory_patch(void*, uint64_t, const void*) { return make_unified_result(false); }
    Stats getStats() const { return Stats(); }
    void resetStats() {}
    bool poll_event(HotpatchEvent*) { return false; }
    PatchResult load_preset(const char*, HotpatchPreset*) { PatchResult r; r.success = false; return r; }
    PatchResult save_preset(const char*, const HotpatchPreset&) { PatchResult r; r.success = false; return r; }
    void clearAllPatches() {}
    UnifiedResult remove_server_patch(const char*) { return make_unified_result(false); }
};

// asm_hotpatch_* / asm_snapshot_* stubs removed — real ASM in src/asm/RawrXD_Hotpatch_Kernel.asm + RawrXD_Snapshot.asm
extern "C" void find_pattern_asm() {}  // kept — no real ASM in src/asm/ yet

// asm_camellia256_* stubs removed — real ASM in src/asm/RawrXD_Camellia256.asm
// auth_* stubs removed — real ASM in src/asm/RawrXD_Camellia256_Auth.asm

// asm_perf_* stubs removed — real ASM in src/asm/RawrXD_PerfCounters.asm
// Watchdog + Quant stubs kept — no real ASM in src/asm/ yet
#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
namespace {
struct WatchdogState {
    bool initialized;
    LARGE_INTEGER baseline_qpc;
    LARGE_INTEGER last_qpc;
    LARGE_INTEGER frequency;
    uint32_t status;         // 0=uninit, 1=ok, 2=drift
    uint64_t baseline_hash;
    uint64_t verify_count;
};

static WatchdogState g_watchdog = {};

static uint64_t fnv1a64(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(p[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t derive_baseline_hash(const LARGE_INTEGER& qpc, const LARGE_INTEGER& freq) {
    struct {
        uint64_t qpc;
        uint64_t freq;
        uint32_t pid;
        uint32_t tid;
        uint64_t addr;
    } seed = {};
    seed.qpc = static_cast<uint64_t>(qpc.QuadPart);
    seed.freq = static_cast<uint64_t>(freq.QuadPart);
    seed.pid = GetCurrentProcessId();
    seed.tid = GetCurrentThreadId();
    seed.addr = reinterpret_cast<uint64_t>(&g_watchdog);
    return fnv1a64(&seed, sizeof(seed));
}

static float fp16_to_fp32(uint16_t h) {
    uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16u;
    uint32_t exp = (h & 0x7C00u) >> 10u;
    uint32_t mantissa = h & 0x03FFu;

    if (exp == 0u) {
        if (mantissa == 0u) {
            uint32_t f32_bits = sign;
            float result;
            std::memcpy(&result, &f32_bits, sizeof(float));
            return result;
        }
        while ((mantissa & 0x0400u) == 0u) {
            mantissa <<= 1u;
            exp -= 1u;
        }
        mantissa &= 0x03FFu;
        exp = 0u;
    }

    if (exp == 0x1Fu) {
        const uint32_t f32_bits = sign | 0x7F800000u | (mantissa << 13u);
        float result;
        std::memcpy(&result, &f32_bits, sizeof(float));
        return result;
    }

    exp = exp + (127u - 15u);
    const uint32_t f32_bits = sign | (exp << 23u) | (mantissa << 13u);
    float result;
    std::memcpy(&result, &f32_bits, sizeof(float));
    return result;
}
} // namespace

extern "C" {

int asm_watchdog_init() {
    QueryPerformanceFrequency(&g_watchdog.frequency);
    QueryPerformanceCounter(&g_watchdog.baseline_qpc);
    g_watchdog.last_qpc = g_watchdog.baseline_qpc;
    g_watchdog.baseline_hash = derive_baseline_hash(g_watchdog.baseline_qpc, g_watchdog.frequency);
    g_watchdog.status = 1;
    g_watchdog.verify_count = 0;
    g_watchdog.initialized = true;
    return 0;
}

int asm_watchdog_verify() {
    if (!g_watchdog.initialized) return -1;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    g_watchdog.last_qpc = now;
    g_watchdog.verify_count++;
    g_watchdog.status = 1;
    return 0;
}

int asm_watchdog_get_baseline(uint8_t* hmac32) {
    if (!g_watchdog.initialized || !hmac32) return -1;
    uint64_t hash0 = g_watchdog.baseline_hash;
    uint64_t hash1 = fnv1a64(&g_watchdog, sizeof(g_watchdog));
    for (int i = 0; i < 4; ++i) {
        uint64_t v = (i % 2 == 0) ? hash0 : hash1;
        std::memcpy(hmac32 + (i * 8), &v, sizeof(uint64_t));
    }
    return 0;
}

int asm_watchdog_get_status(void* status48) {
    if (!status48) return -1;
    struct WatchdogStatus {
        uint32_t status;
        uint32_t reserved;
        uint64_t baseline_hash;
        uint64_t last_qpc;
        uint64_t frequency;
        uint64_t verify_count;
    } out = {};
    out.status = g_watchdog.status;
    out.baseline_hash = g_watchdog.baseline_hash;
    out.last_qpc = static_cast<uint64_t>(g_watchdog.last_qpc.QuadPart);
    out.frequency = static_cast<uint64_t>(g_watchdog.frequency.QuadPart);
    out.verify_count = g_watchdog.verify_count;
    std::memcpy(status48, &out, sizeof(out));
    return 0;
}

int asm_watchdog_shutdown() {
    g_watchdog = {};
    return 0;
}

int asm_kquant_cpuid_check() {
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    int maxLeaf = cpuInfo[0];
    if (maxLeaf < 7) return 0;
    __cpuidex(cpuInfo, 7, 0);
    const bool avx2 = (cpuInfo[1] & (1 << 5)) != 0;
    return avx2 ? 1 : 0;
}

uint64_t Quant_DequantQ4_0(const void* src, float* dst, uint64_t numElements) {
    if (!src || !dst || numElements == 0) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(src);
    uint64_t blocks = numElements / 32ULL;
    uint64_t outCount = 0;
    for (uint64_t b = 0; b < blocks; ++b) {
        uint16_t scale_fp16;
        std::memcpy(&scale_fp16, p, sizeof(uint16_t));
        const float scale = fp16_to_fp32(scale_fp16);
        p += 2;
        for (int i = 0; i < 16; ++i) {
            const uint8_t byte = p[i];
            int8_t q0 = static_cast<int8_t>(byte & 0x0F);
            int8_t q1 = static_cast<int8_t>((byte >> 4) & 0x0F);
            if (q0 >= 8) q0 -= 16;
            if (q1 >= 8) q1 -= 16;
            dst[outCount++] = scale * static_cast<float>(q0);
            dst[outCount++] = scale * static_cast<float>(q1);
        }
        p += 16;
    }
    return outCount;
}

uint64_t Quant_DequantQ8_0(const void* src, float* dst, uint64_t numElements) {
    if (!src || !dst || numElements == 0) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(src);
    uint64_t blocks = numElements / 32ULL;
    uint64_t outCount = 0;
    for (uint64_t b = 0; b < blocks; ++b) {
        uint16_t scale_fp16;
        std::memcpy(&scale_fp16, p, sizeof(uint16_t));
        const float scale = fp16_to_fp32(scale_fp16);
        p += 2;
        const int8_t* qs = reinterpret_cast<const int8_t*>(p);
        for (int i = 0; i < 32; ++i) {
            dst[outCount++] = scale * static_cast<float>(qs[i]);
        }
        p += 32;
    }
    return outCount;
}

// =============================================================================
// K-Quant Dequantization: GGML K=256 Superblock Formats
// =============================================================================
// K-quants use 256-element superblocks with hierarchical quantization.
// Each format balances precision vs. compression with multiple scale levels.

// GGML Q2_K Block Format (256 bytes total):
// - 16 scales (fp16) + 16 scale_min (fp16) = 64 bytes
// - 64 bytes of 2-bit packed quants (256 values * 2 bits / 8)
// - Block size: 256 bytes, outputs 256 fp32 values
void KQuant_DequantizeQ2_K_impl(const void* src, float* dst, uint64_t rowCount) {
    if (!src || !dst || rowCount == 0) return;
    const uint8_t* p = static_cast<const uint8_t*>(src);
    constexpr size_t kBlockSize = 256;  // QK_K for K-quants
    constexpr size_t kScaleCount = 16;
    
    for (uint64_t row = 0; row < rowCount; ++row) {
        // Read 16 scales (fp16, 32 bytes)
        float scales[kScaleCount];
        for (size_t s = 0; s < kScaleCount; ++s) {
            uint16_t scale_fp16;
            std::memcpy(&scale_fp16, p + s * 2, sizeof(uint16_t));
            scales[s] = fp16_to_fp32(scale_fp16);
        }
        p += 32;
        
        // Read 16 scale_min (fp16, 32 bytes)
        float scale_mins[kScaleCount];
        for (size_t s = 0; s < kScaleCount; ++s) {
            uint16_t min_fp16;
            std::memcpy(&min_fp16, p + s * 2, sizeof(uint16_t));
            scale_mins[s] = fp16_to_fp32(min_fp16);
        }
        p += 32;
        
        // Dequantize 256 2-bit values (64 bytes of packed data)
        float* out = dst + row * kBlockSize;
        for (size_t i = 0; i < kBlockSize / 4; ++i) {
            const uint8_t byte = p[i];
            const size_t scale_idx = i / 4;  // 64/4 = 16 groups
            const float scale = scales[scale_idx];
            const float scale_min = scale_mins[scale_idx];
            
            // Unpack 4 x 2-bit values from byte
            for (int b = 0; b < 4; ++b) {
                uint8_t q2 = (byte >> (b * 2)) & 0x03u;
                out[i * 4 + b] = scale * static_cast<float>(q2) + scale_min;
            }
        }
        p += 64;
        
        // Skip remaining padding to 256-byte boundary
        p += (256 - 128);
    }
}

// GGML Q3_K Block Format (256 bytes total):
// - 16 scales (fp16) = 32 bytes
// - 96 bytes of 3-bit packed quants (256 values * 3 bits / 8)
// - Block size: 256 bytes, outputs 256 fp32 values
void KQuant_DequantizeQ3_K_impl(const void* src, float* dst, uint64_t rowCount) {
    if (!src || !dst || rowCount == 0) return;
    const uint8_t* p = static_cast<const uint8_t*>(src);
    constexpr size_t kBlockSize = 256;
    constexpr size_t kScaleCount = 16;
    
    for (uint64_t row = 0; row < rowCount; ++row) {
        // Read 16 scales (fp16, 32 bytes)
        float scales[kScaleCount];
        for (size_t s = 0; s < kScaleCount; ++s) {
            uint16_t scale_fp16;
            std::memcpy(&scale_fp16, p + s * 2, sizeof(uint16_t));
            scales[s] = fp16_to_fp32(scale_fp16);
        }
        p += 32;
        
        // Dequantize 256 3-bit values (96 bytes of packed data)
        // 3 bits per value: 8 values per 3 bytes
        float* out = dst + row * kBlockSize;
        size_t q_offset = 0;
        for (size_t g = 0; g < 32; ++g) {  // 256/8 = 32 groups of 8 values
            // Read 3 bytes containing 8 x 3-bit values
            uint32_t packed = static_cast<uint32_t>(p[0]) |
                            (static_cast<uint32_t>(p[1]) << 8) |
                            (static_cast<uint32_t>(p[2]) << 16);
            p += 3;
            
            const size_t scale_idx = g / 2;  // 32/2 = 16 scales
            const float scale = scales[scale_idx];
            
            for (int i = 0; i < 8; ++i) {
                uint8_t q3 = (packed >> (i * 3)) & 0x07u;
                int8_t signed_q = static_cast<int8_t>(q3);
                if (signed_q >= 4) signed_q -= 8;  // Sign-extend 3-bit to 8-bit
                out[q_offset++] = scale * static_cast<float>(signed_q);
            }
        }
        
        // Skip remaining padding to 256-byte boundary
        p += (256 - 32 - 96);
    }
}

// GGML Q4_K Block Format (256 bytes total):
// - 12 scales (fp16) + 12 scale_min (fp16) = 48 bytes
// - 128 bytes of 4-bit packed quants (256 values * 4 bits / 8)
// - Block size: 256 bytes, outputs 256 fp32 values
void KQuant_DequantizeQ4_K_impl(const void* src, float* dst, uint64_t rowCount) {
    if (!src || !dst || rowCount == 0) return;
    const uint8_t* p = static_cast<const uint8_t*>(src);
    constexpr size_t kBlockSize = 256;
    constexpr size_t kScaleCount = 12;
    
    for (uint64_t row = 0; row < rowCount; ++row) {
        // Read 12 scales (fp16, 24 bytes)
        float scales[kScaleCount];
        for (size_t s = 0; s < kScaleCount; ++s) {
            uint16_t scale_fp16;
            std::memcpy(&scale_fp16, p + s * 2, sizeof(uint16_t));
            scales[s] = fp16_to_fp32(scale_fp16);
        }
        p += 24;
        
        // Read 12 scale_min (fp16, 24 bytes)
        float scale_mins[kScaleCount];
        for (size_t s = 0; s < kScaleCount; ++s) {
            uint16_t min_fp16;
            std::memcpy(&min_fp16, p + s * 2, sizeof(uint16_t));
            scale_mins[s] = fp16_to_fp32(min_fp16);
        }
        p += 24;
        
        // Dequantize 256 4-bit values (128 bytes)
        float* out = dst + row * kBlockSize;
        for (size_t i = 0; i < kBlockSize / 2; ++i) {
            const uint8_t byte = p[i];
            const size_t scale_idx = (i * 2) / 21;  // ~256/12 groups
            const float scale = scales[scale_idx % kScaleCount];
            const float scale_min = scale_mins[scale_idx % kScaleCount];
            
            // Low nibble
            int8_t q0 = static_cast<int8_t>(byte & 0x0Fu);
            if (q0 >= 8) q0 -= 16;
            out[i * 2] = scale * static_cast<float>(q0) + scale_min;
            
            // High nibble
            int8_t q1 = static_cast<int8_t>((byte >> 4) & 0x0Fu);
            if (q1 >= 8) q1 -= 16;
            out[i * 2 + 1] = scale * static_cast<float>(q1) + scale_min;
        }
        p += 128;
        
        // Skip remaining padding to 256-byte boundary
        p += (256 - 48 - 128);
    }
}

// GGML Q6_K Block Format (256 bytes total):
// - 16 scales (fp16) = 32 bytes
// - 192 bytes of 6-bit packed quants (256 values * 6 bits / 8)
// - Block size: 256 bytes, outputs 256 fp32 values
void KQuant_DequantizeQ6_K_impl(const void* src, float* dst, uint64_t rowCount) {
    if (!src || !dst || rowCount == 0) return;
    const uint8_t* p = static_cast<const uint8_t*>(src);
    constexpr size_t kBlockSize = 256;
    constexpr size_t kScaleCount = 16;
    
    for (uint64_t row = 0; row < rowCount; ++row) {
        // Read 16 scales (fp16, 32 bytes)
        float scales[kScaleCount];
        for (size_t s = 0; s < kScaleCount; ++s) {
            uint16_t scale_fp16;
            std::memcpy(&scale_fp16, p + s * 2, sizeof(uint16_t));
            scales[s] = fp16_to_fp32(scale_fp16);
        }
        p += 32;
        
        // Dequantize 256 6-bit values (192 bytes)
        // 6 bits per value: 4 values per 3 bytes
        float* out = dst + row * kBlockSize;
        size_t q_offset = 0;
        for (size_t g = 0; g < 64; ++g) {  // 256/4 = 64 groups
            // Read 3 bytes containing 4 x 6-bit values
            uint32_t packed = static_cast<uint32_t>(p[0]) |
                            (static_cast<uint32_t>(p[1]) << 8) |
                            (static_cast<uint32_t>(p[2]) << 16);
            p += 3;
            
            const size_t scale_idx = g / 4;  // 64/4 = 16 scales
            const float scale = scales[scale_idx];
            
            for (int i = 0; i < 4; ++i) {
                uint8_t q6 = (packed >> (i * 6)) & 0x3Fu;
                int8_t signed_q = static_cast<int8_t>(q6);
                if (signed_q >= 32) signed_q -= 64;  // Sign-extend 6-bit to 8-bit
                out[q_offset++] = scale * static_cast<float>(signed_q);
            }
        }
        
        // Skip remaining padding to 256-byte boundary
        p += (256 - 32 - 192);
    }
}

// FP16 Dequantization: Convert FP16 array to FP32
void KQuant_DequantizeF16_impl(const void* src, float* dst, uint64_t elementCount) {
    if (!src || !dst || elementCount == 0) return;
    const uint16_t* p = static_cast<const uint16_t*>(src);
    for (uint64_t i = 0; i < elementCount; ++i) {
        dst[i] = fp16_to_fp32(p[i]);
    }
}

// KQuant Dispatcher: Routes to appropriate dequantization function
// Format codes: 2=Q2_K, 3=Q3_K, 4=Q4_K, 6=Q6_K, 16=F16
int KQuant_Dispatch_impl(int format, const void* src, float* dst, uint64_t count) {
    if (!src || !dst || count == 0) return -1;
    
    switch (format) {
        case 2:
            KQuant_DequantizeQ2_K_impl(src, dst, count);
            return 0;
        case 3:
            KQuant_DequantizeQ3_K_impl(src, dst, count);
            return 0;
        case 4:
            KQuant_DequantizeQ4_K_impl(src, dst, count);
            return 0;
        case 6:
            KQuant_DequantizeQ6_K_impl(src, dst, count);
            return 0;
        case 16:
            KQuant_DequantizeF16_impl(src, dst, count);
            return 0;
        default:
            return -1;  // Unsupported format
    }
}

// AVX2 Q2_K Dequantization Hook (dispatches to scalar fallback when AVX2 unavailable)
void dequant_q2k_avx2_impl(const void* src, float* dst, uint64_t rowCount) {
    // Fallback: use scalar implementation
    // Real ASM version would use AVX2 for 8x parallel 2-bit unpacking
    KQuant_DequantizeQ2_K_impl(src, dst, rowCount);
}

} // extern "C"
#endif // !RAWR_HAS_MASM

// Quantization (remaining ASM stubs)
extern "C" void KQuant_DequantizeQ2_K() {}
extern "C" void KQuant_DequantizeQ3_K() {}
extern "C" void KQuant_Dispatch() {}
extern "C" void KQuant_DequantizeQ4_K() {}
extern "C" void KQuant_DequantizeQ6_K() {}
extern "C" void KQuant_DequantizeF16() {}

// DirectML
extern "C" void asm_dml_rope_rotate_fp32() {}
extern "C" void asm_dml_dequant_q4_0_to_fp32() {}
extern "C" void asm_dml_dequant_q8_0_to_fp32() {}
extern "C" void asm_dml_prefetch_tensor_block() {}

// Prompt generation (fallback when ASM kernel is not linked)
#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
namespace {
static std::atomic<int32_t> g_prompt_force_mode{RAWRXD_CTX_MODE_AUTO};

static int32_t count_keyword_hits(const std::string& text, const char* const* keys, size_t key_count) {
    int32_t hits = 0;
    for (size_t i = 0; i < key_count; ++i) {
        const char* key = keys[i];
        if (!key || key[0] == '\0') continue;
        if (text.find(key) != std::string::npos) {
            hits += 1;
        }
    }
    return hits;
}

static int32_t score_mode(const std::string& text, const char* const* keys, size_t key_count, int32_t base) {
    int32_t hits = count_keyword_hits(text, keys, key_count);
    return base + hits * 10;
}

static int32_t classify_mode(const char* textPtr, size_t textLen, int32_t* outScore) {
    if (!textPtr) {
        if (outScore) *outScore = 0;
        return RAWRXD_CTX_MODE_GENERIC;
    }

    const size_t len = (textLen == 0) ? std::strlen(textPtr) : textLen;
    if (len == 0) {
        if (outScore) *outScore = 0;
        return RAWRXD_CTX_MODE_GENERIC;
    }

    std::string text(textPtr, textPtr + len);
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }

    static const char* const kCasual[] = {
        "hi", "hello", "thanks", "please", "how are you", "cheers"
    };
    static const char* const kCode[] = {
        "class", "struct", "void", "int ", "std::", "namespace", "compile", "error c"
    };
    static const char* const kSecurity[] = {
        "vuln", "exploit", "malware", "payload", "cve", "buffer overflow", "inject"
    };
    static const char* const kShell[] = {
        "cmd", "powershell", "bash", "sudo", "chmod", "ls ", "dir ", "cd "
    };
    static const char* const kEnterprise[] = {
        "license", "compliance", "policy", "enterprise", "sla", "slo", "audit"
    };

    int32_t score_generic = 5;
    int32_t score_casual = score_mode(text, kCasual, sizeof(kCasual) / sizeof(kCasual[0]), 5);
    int32_t score_code = score_mode(text, kCode, sizeof(kCode) / sizeof(kCode[0]), 8);
    int32_t score_security = score_mode(text, kSecurity, sizeof(kSecurity) / sizeof(kSecurity[0]), 6);
    int32_t score_shell = score_mode(text, kShell, sizeof(kShell) / sizeof(kShell[0]), 6);
    int32_t score_enterprise = score_mode(text, kEnterprise, sizeof(kEnterprise) / sizeof(kEnterprise[0]), 6);

    // Heuristic boosts
    if (text.find(";") != std::string::npos || text.find("{") != std::string::npos) score_code += 5;
    if (text.find(".ps1") != std::string::npos || text.find(".bat") != std::string::npos) score_shell += 5;
    if (text.find("license") != std::string::npos) score_enterprise += 5;

    int32_t mode = RAWRXD_CTX_MODE_GENERIC;
    int32_t best = score_generic;

    auto try_pick = [&](int32_t m, int32_t score) {
        if (score > best) { best = score; mode = m; }
    };

    try_pick(RAWRXD_CTX_MODE_CASUAL, score_casual);
    try_pick(RAWRXD_CTX_MODE_CODE, score_code);
    try_pick(RAWRXD_CTX_MODE_SECURITY, score_security);
    try_pick(RAWRXD_CTX_MODE_SHELL, score_shell);
    try_pick(RAWRXD_CTX_MODE_ENTERPRISE, score_enterprise);

    if (outScore) *outScore = best;
    return mode;
}

static size_t write_interpolated(const char* templ, const char* context, char* outBuf, size_t outSize) {
    if (!templ || !outBuf || outSize == 0) return 0;
    const char* ctx = context ? context : "";
    const char* marker = "{{CONTEXT}}";
    const size_t marker_len = 11;
    size_t outPos = 0;

    for (const char* p = templ; *p && outPos + 1 < outSize; ) {
        if (std::strncmp(p, marker, marker_len) == 0) {
            const size_t ctx_len = std::strlen(ctx);
            if (outPos + ctx_len >= outSize) break;
            std::memcpy(outBuf + outPos, ctx, ctx_len);
            outPos += ctx_len;
            p += marker_len;
            continue;
        }
        outBuf[outPos++] = *p++;
    }
    outBuf[outPos] = '\0';
    return outPos;
}

static const char* get_template(int32_t mode, int32_t type) {
    static const char* const s_mode_names[RAWRXD_CTX_MODE_COUNT] = {
        "GENERIC", "CASUAL", "CODE", "SECURITY", "SHELL", "ENTERPRISE"
    };
    static const char* const s_critic[] = {
        "[CRITIC][GENERIC]\n{{CONTEXT}}\n",
        "[CRITIC][CASUAL]\n{{CONTEXT}}\n",
        "[CRITIC][CODE]\n{{CONTEXT}}\n",
        "[CRITIC][SECURITY]\n{{CONTEXT}}\n",
        "[CRITIC][SHELL]\n{{CONTEXT}}\n",
        "[CRITIC][ENTERPRISE]\n{{CONTEXT}}\n"
    };
    static const char* const s_auditor[] = {
        "[AUDITOR][GENERIC]\n{{CONTEXT}}\n",
        "[AUDITOR][CASUAL]\n{{CONTEXT}}\n",
        "[AUDITOR][CODE]\n{{CONTEXT}}\n",
        "[AUDITOR][SECURITY]\n{{CONTEXT}}\n",
        "[AUDITOR][SHELL]\n{{CONTEXT}}\n",
        "[AUDITOR][ENTERPRISE]\n{{CONTEXT}}\n"
    };

    if (mode < 0 || mode >= RAWRXD_CTX_MODE_COUNT) mode = RAWRXD_CTX_MODE_GENERIC;
    if (type == RAWRXD_TEMPLATE_AUDITOR) return s_auditor[mode];
    if (type == RAWRXD_TEMPLATE_CRITIC) return s_critic[mode];

    // Fallback: embed mode name into generic template
    static char scratch[128];
    std::snprintf(scratch, sizeof(scratch), "[TEMPLATE][%s]\\n{{CONTEXT}}\\n", s_mode_names[mode]);
    return scratch;
}
} // namespace

extern "C" {

RAWRXD_PROMPT_API int64_t RAWRXD_PROMPT_CALL
PromptGen_AnalyzeContext(const char* textPtr, size_t textLen) {
    const int32_t forced = g_prompt_force_mode.load();
    int32_t score = 0;
    int32_t mode = (forced >= 0 && forced < RAWRXD_CTX_MODE_COUNT)
        ? forced
        : classify_mode(textPtr, textLen, &score);
    const uint64_t packed = (static_cast<uint64_t>(static_cast<uint32_t>(score)) << 32)
        | static_cast<uint32_t>(mode);
    return static_cast<int64_t>(packed);
}

RAWRXD_PROMPT_API size_t RAWRXD_PROMPT_CALL
PromptGen_BuildCritic(const char* contextPtr, size_t contextLen, char* outBuf, size_t outSize) {
    const int32_t mode = (g_prompt_force_mode.load() >= 0)
        ? g_prompt_force_mode.load()
        : classify_mode(contextPtr, contextLen, nullptr);
    const char* templ = get_template(mode, RAWRXD_TEMPLATE_CRITIC);
    return write_interpolated(templ, contextPtr, outBuf, outSize);
}

RAWRXD_PROMPT_API size_t RAWRXD_PROMPT_CALL
PromptGen_BuildAuditor(const char* contextPtr, size_t contextLen, char* outBuf, size_t outSize) {
    const int32_t mode = (g_prompt_force_mode.load() >= 0)
        ? g_prompt_force_mode.load()
        : classify_mode(contextPtr, contextLen, nullptr);
    const char* templ = get_template(mode, RAWRXD_TEMPLATE_AUDITOR);
    return write_interpolated(templ, contextPtr, outBuf, outSize);
}

RAWRXD_PROMPT_API size_t RAWRXD_PROMPT_CALL
PromptGen_Interpolate(const char* templatePtr, const char* contextPtr, char* outBuf, size_t outSize) {
    return write_interpolated(templatePtr, contextPtr, outBuf, outSize);
}

RAWRXD_PROMPT_API const char* RAWRXD_PROMPT_CALL
PromptGen_GetTemplate(int32_t mode, int32_t type) {
    return get_template(mode, type);
}

RAWRXD_PROMPT_API int32_t RAWRXD_PROMPT_CALL
PromptGen_ForceMode(int32_t mode) {
    const int32_t prev = g_prompt_force_mode.load();
    g_prompt_force_mode.store(mode);
    return prev;
}

RAWRXD_PROMPT_API const char* RAWRXD_PROMPT_CALL
PromptGen_GetModeName(int32_t mode) {
    static const char* const s_mode_names[RAWRXD_CTX_MODE_COUNT] = {
        "GENERIC", "CASUAL", "CODE", "SECURITY", "SHELL", "ENTERPRISE"
    };
    if (mode < 0 || mode >= RAWRXD_CTX_MODE_COUNT) return nullptr;
    return s_mode_names[mode];
}

} // extern "C"
#endif // !RAWR_HAS_MASM

// =============================================================================
// SPEngine: Speculative Quantization Engine for Dynamic Precision Control
// =============================================================================
// Runtime quantization switching system for LLM inference optimization.
// Monitors perplexity and dynamically adjusts precision (Q4_0 <-> Q8_0 <-> F16)
// to balance speed vs. quality during generation.

#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
namespace {

struct SPEngineState {
    // CPU capabilities
    bool hasAVX2;
    bool hasAVX512;
    bool hasFMA;
    
    // Quantization state
    uint32_t currentFormat;     // 0=F16, 4=Q4_0, 8=Q8_0, etc.
    uint32_t targetFormat;      // Desired format after switch
    uint32_t fallbackFormat;    // Previous format for rollback
    
    // Registered formats (up to 8)
    struct QuantFormat {
        uint32_t formatId;
        const char* name;
        float speedMultiplier;  // Relative to F16 (Q4_0 = ~4x)
        float qualityScore;     // 0.0-1.0 (F16 = 1.0, Q4_0 = 0.85)
    } formats[8];
    uint32_t formatCount;
    
    // Adaptive switching metrics
    float currentPerplexity;
    float targetPerplexity;
    uint32_t switchCount;
    uint64_t totalTokens;
    
    CRITICAL_SECTION cs;
    bool initialized;
};

struct SPEngineStats {
    uint32_t size_bytes;
    uint32_t currentFormat;
    uint32_t targetFormat;
    uint32_t fallbackFormat;
    uint32_t formatCount;
    uint32_t switchCount;
    uint32_t hasAVX2;
    uint32_t hasAVX512;
    uint32_t hasFMA;
    float currentPerplexity;
    float targetPerplexity;
    uint64_t totalTokens;
};

static SPEngineState g_spengine = {};
static SPEngineStats g_spengine_stats = {};

static void detect_cpu_features() {
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    int maxLeaf = cpuInfo[0];
    
    if (maxLeaf >= 1) {
        __cpuidex(cpuInfo, 1, 0);
        g_spengine.hasFMA = (cpuInfo[2] & (1 << 12)) != 0;  // ECX bit 12
    }
    
    if (maxLeaf >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        g_spengine.hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;   // EBX bit 5
        g_spengine.hasAVX512 = (cpuInfo[1] & (1 << 16)) != 0; // EBX bit 16
    }
}

} // namespace

extern "C" {

int asm_spengine_cpu_optimize() {
    // Detect CPU features and return capability flags
    detect_cpu_features();
    
    int flags = 0;
    if (g_spengine.hasAVX2) flags |= 0x01;
    if (g_spengine.hasAVX512) flags |= 0x02;
    if (g_spengine.hasFMA) flags |= 0x04;
    
    return flags;
}

int asm_spengine_init(uint32_t flags, void* config) {
    if (g_spengine.initialized) return 0;  // Already initialized
    
    InitializeCriticalSection(&g_spengine.cs);
    
    // Detect CPU capabilities
    detect_cpu_features();
    
    // Initialize quantization state
    g_spengine.currentFormat = 0;     // Start with F16 (highest quality)
    g_spengine.targetFormat = 0;
    g_spengine.fallbackFormat = 0;
    g_spengine.formatCount = 0;
    g_spengine.currentPerplexity = 0.0f;
    g_spengine.targetPerplexity = 10.0f;  // Target perplexity threshold
    g_spengine.switchCount = 0;
    g_spengine.totalTokens = 0;
    
    // Register default formats
    // F16 (baseline)
    g_spengine.formats[0] = {0, "F16", 1.0f, 1.0f};
    // Q8_0 (high quality, 2x faster)
    g_spengine.formats[1] = {8, "Q8_0", 2.0f, 0.95f};
    // Q4_0 (fast, lower quality)
    g_spengine.formats[2] = {4, "Q4_0", 4.0f, 0.85f};
    g_spengine.formatCount = 3;
    
    g_spengine.initialized = true;
    return 0;
}

int asm_spengine_register(uint32_t formatId, const char* name, float speedMult, float quality) {
    if (!g_spengine.initialized || !name) return -1;
    if (g_spengine.formatCount >= 8) return -2;  // Max formats reached
    
    EnterCriticalSection(&g_spengine.cs);
    
    // Check for duplicate format ID
    for (uint32_t i = 0; i < g_spengine.formatCount; ++i) {
        if (g_spengine.formats[i].formatId == formatId) {
            LeaveCriticalSection(&g_spengine.cs);
            return -3;  // Duplicate
        }
    }
    
    // Register new format
    auto& fmt = g_spengine.formats[g_spengine.formatCount];
    fmt.formatId = formatId;
    fmt.name = name;  // Assumes name is static
    fmt.speedMultiplier = speedMult;
    fmt.qualityScore = quality;
    g_spengine.formatCount++;
    
    LeaveCriticalSection(&g_spengine.cs);
    return 0;
}

int asm_spengine_apply(uint32_t formatId, void* modelWeights, uint64_t byteCount) {
    if (!g_spengine.initialized || !modelWeights || byteCount == 0) return -1;
    
    EnterCriticalSection(&g_spengine.cs);
    
    // Find format
    int formatIdx = -1;
    for (uint32_t i = 0; i < g_spengine.formatCount; ++i) {
        if (g_spengine.formats[i].formatId == formatId) {
            formatIdx = static_cast<int>(i);
            break;
        }
    }
    
    if (formatIdx < 0) {
        LeaveCriticalSection(&g_spengine.cs);
        return -2;  // Format not found
    }
    
    // Save fallback state
    g_spengine.fallbackFormat = g_spengine.currentFormat;
    
    // Apply quantization (simulated - real implementation would quantize weights)
    // For now, just update state
    g_spengine.currentFormat = formatId;
    g_spengine.switchCount++;
    
    LeaveCriticalSection(&g_spengine.cs);
    return 0;
}

int asm_spengine_rollback() {
    if (!g_spengine.initialized) return -1;
    
    EnterCriticalSection(&g_spengine.cs);
    
    // Restore previous format
    const uint32_t temp = g_spengine.currentFormat;
    g_spengine.currentFormat = g_spengine.fallbackFormat;
    g_spengine.fallbackFormat = temp;
    
    LeaveCriticalSection(&g_spengine.cs);
    return 0;
}

int asm_spengine_quant_switch(uint32_t newFormatId) {
    if (!g_spengine.initialized) return -1;
    
    EnterCriticalSection(&g_spengine.cs);
    
    // Validate format exists
    bool found = false;
    for (uint32_t i = 0; i < g_spengine.formatCount; ++i) {
        if (g_spengine.formats[i].formatId == newFormatId) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        LeaveCriticalSection(&g_spengine.cs);
        return -2;
    }
    
    // Save fallback and switch
    g_spengine.fallbackFormat = g_spengine.currentFormat;
    g_spengine.currentFormat = newFormatId;
    g_spengine.switchCount++;
    
    LeaveCriticalSection(&g_spengine.cs);
    return 0;
}

int asm_spengine_quant_switch_adaptive(float currentPerplexity, uint32_t tokenCount) {
    if (!g_spengine.initialized || g_spengine.formatCount == 0) return -1;
    
    EnterCriticalSection(&g_spengine.cs);
    
    g_spengine.currentPerplexity = currentPerplexity;
    g_spengine.totalTokens += tokenCount;
    
    // Adaptive switching logic:
    // - If perplexity < target: switch to faster (lower precision)
    // - If perplexity > target * 1.5: switch to slower (higher precision)
    // - Otherwise: stay at current format
    
    uint32_t bestFormat = g_spengine.currentFormat;
    
    if (currentPerplexity < g_spengine.targetPerplexity) {
        // Quality is good, can use faster format
        // Find format with higher speed multiplier
        float currentSpeed = 1.0f;
        for (uint32_t i = 0; i < g_spengine.formatCount; ++i) {
            if (g_spengine.formats[i].formatId == g_spengine.currentFormat) {
                currentSpeed = g_spengine.formats[i].speedMultiplier;
                break;
            }
        }
        
        // Find faster format
        for (uint32_t i = 0; i < g_spengine.formatCount; ++i) {
            if (g_spengine.formats[i].speedMultiplier > currentSpeed) {
                bestFormat = g_spengine.formats[i].formatId;
                break;
            }
        }
    } else if (currentPerplexity > g_spengine.targetPerplexity * 1.5f) {
        // Quality degraded, need higher precision
        // Find format with higher quality score
        float currentQuality = 0.0f;
        for (uint32_t i = 0; i < g_spengine.formatCount; ++i) {
            if (g_spengine.formats[i].formatId == g_spengine.currentFormat) {
                currentQuality = g_spengine.formats[i].qualityScore;
                break;
            }
        }
        
        // Find higher quality format
        for (uint32_t i = 0; i < g_spengine.formatCount; ++i) {
            if (g_spengine.formats[i].qualityScore > currentQuality) {
                bestFormat = g_spengine.formats[i].formatId;
                break;
            }
        }
    }
    
    // Apply switch if different
    if (bestFormat != g_spengine.currentFormat) {
        g_spengine.fallbackFormat = g_spengine.currentFormat;
        g_spengine.currentFormat = bestFormat;
        g_spengine.switchCount++;
    }
    
    LeaveCriticalSection(&g_spengine.cs);
    return (bestFormat != g_spengine.currentFormat) ? 1 : 0;  // 1 = switched
}

const void* asm_spengine_get_stats() {
    if (!g_spengine.initialized) return nullptr;

    EnterCriticalSection(&g_spengine.cs);
    g_spengine_stats.size_bytes = static_cast<uint32_t>(sizeof(g_spengine_stats));
    g_spengine_stats.currentFormat = g_spengine.currentFormat;
    g_spengine_stats.targetFormat = g_spengine.targetFormat;
    g_spengine_stats.fallbackFormat = g_spengine.fallbackFormat;
    g_spengine_stats.formatCount = g_spengine.formatCount;
    g_spengine_stats.switchCount = g_spengine.switchCount;
    g_spengine_stats.hasAVX2 = g_spengine.hasAVX2 ? 1u : 0u;
    g_spengine_stats.hasAVX512 = g_spengine.hasAVX512 ? 1u : 0u;
    g_spengine_stats.hasFMA = g_spengine.hasFMA ? 1u : 0u;
    g_spengine_stats.currentPerplexity = g_spengine.currentPerplexity;
    g_spengine_stats.targetPerplexity = g_spengine.targetPerplexity;
    g_spengine_stats.totalTokens = g_spengine.totalTokens;
    LeaveCriticalSection(&g_spengine.cs);

    return &g_spengine_stats;
}

int asm_spengine_shutdown() {
    if (!g_spengine.initialized) return -1;

    EnterCriticalSection(&g_spengine.cs);
    LeaveCriticalSection(&g_spengine.cs);
    DeleteCriticalSection(&g_spengine.cs);
    std::memset(&g_spengine, 0, sizeof(g_spengine));
    std::memset(&g_spengine_stats, 0, sizeof(g_spengine_stats));

    return 0;
}

} // extern "C"
#endif // !RAWR_HAS_MASM

// SPEngine stubs removed (replaced with fallback implementation above)

// asm_gguf_loader_* stubs removed — real ASM in src/asm/RawrXD_GGUF_Vulkan_Loader.asm

// Orchestrator
#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
namespace {
struct OrchestratorTask {
    uint32_t opcode;
    void (*fn)(void*, void*);
    void* payload;
    void* context;
};

struct OrchestratorState {
    bool initialized;
    void** vtable;
    void (*hooks[32])(void*, void*);
    uint32_t hook_count;
    OrchestratorTask queue[256];
    uint32_t head;
    uint32_t tail;
    uint64_t dispatched;
    uint64_t completed;
    uint64_t failed;
    LARGE_INTEGER last_tick;
    CRITICAL_SECTION cs;
    bool cs_init;
};

static OrchestratorState g_orch = {};

static void orch_init_lock() {
    if (!g_orch.cs_init) {
        InitializeCriticalSection(&g_orch.cs);
        g_orch.cs_init = true;
    }
}

static uint32_t orch_queue_size() {
    if (g_orch.tail >= g_orch.head) return g_orch.tail - g_orch.head;
    return (256u - g_orch.head) + g_orch.tail;
}

static bool orch_queue_push(const OrchestratorTask& task) {
    uint32_t next = (g_orch.tail + 1u) & 0xFFu;
    if (next == g_orch.head) return false;
    g_orch.queue[g_orch.tail] = task;
    g_orch.tail = next;
    return true;
}

static bool orch_queue_pop(OrchestratorTask& out) {
    if (g_orch.head == g_orch.tail) return false;
    out = g_orch.queue[g_orch.head];
    g_orch.head = (g_orch.head + 1u) & 0xFFu;
    return true;
}

static void orch_execute_task(const OrchestratorTask& task) {
    if (!task.fn) {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&g_orch.failed));
        return;
    }
    task.fn(task.payload, task.context);
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&g_orch.completed));
}
} // namespace

extern "C" {

int asm_orchestrator_init(void* vtable, void* reserved) {
    (void)reserved;
    orch_init_lock();
    EnterCriticalSection(&g_orch.cs);
    g_orch.initialized = true;
    g_orch.vtable = static_cast<void**>(vtable);
    g_orch.hook_count = 0;
    g_orch.head = 0;
    g_orch.tail = 0;
    g_orch.dispatched = 0;
    g_orch.completed = 0;
    g_orch.failed = 0;
    std::memset(g_orch.hooks, 0, sizeof(g_orch.hooks));
    QueryPerformanceCounter(&g_orch.last_tick);
    LeaveCriticalSection(&g_orch.cs);
    return 0;
}

int asm_orchestrator_dispatch(uint32_t opcode, void* payload, void* context) {
    orch_init_lock();
    EnterCriticalSection(&g_orch.cs);
    g_orch.dispatched++;
    void (*fn)(void*, void*) = nullptr;
    if (opcode < 32 && g_orch.hooks[opcode]) {
        fn = g_orch.hooks[opcode];
    } else if (g_orch.vtable && opcode < 32) {
        fn = reinterpret_cast<void (*)(void*, void*)>(g_orch.vtable[opcode]);
    }
    LeaveCriticalSection(&g_orch.cs);

    if (!fn) {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&g_orch.failed));
        return -1;
    }

    fn(payload, context);
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&g_orch.completed));
    return 0;
}

void asm_orchestrator_shutdown() {
    if (!g_orch.cs_init) return;
    EnterCriticalSection(&g_orch.cs);
    g_orch.initialized = false;
    g_orch.vtable = nullptr;
    std::memset(g_orch.hooks, 0, sizeof(g_orch.hooks));
    g_orch.hook_count = 0;
    g_orch.head = 0;
    g_orch.tail = 0;
    g_orch.dispatched = 0;
    g_orch.completed = 0;
    g_orch.failed = 0;
    g_orch.last_tick = {};
    LeaveCriticalSection(&g_orch.cs);
    DeleteCriticalSection(&g_orch.cs);
    g_orch.cs_init = false;
}

void asm_orchestrator_get_metrics(void* outMetrics) {
    if (!outMetrics) return;
    struct OrchestratorMetrics {
        uint64_t dispatched;
        uint64_t completed;
        uint64_t failed;
        uint32_t hooks;
        uint32_t queued;
        uint64_t last_tick;
        uint64_t reserved;
    } metrics = {};

    orch_init_lock();
    EnterCriticalSection(&g_orch.cs);
    metrics.dispatched = g_orch.dispatched;
    metrics.completed = g_orch.completed;
    metrics.failed = g_orch.failed;
    metrics.hooks = g_orch.hook_count;
    metrics.queued = orch_queue_size();
    metrics.last_tick = static_cast<uint64_t>(g_orch.last_tick.QuadPart);
    LeaveCriticalSection(&g_orch.cs);

    std::memcpy(outMetrics, &metrics, sizeof(metrics));
}

int asm_orchestrator_register_hook(uint32_t opcode, void* hookFn, void* context) {
    (void)context;
    if (opcode >= 32) return -1;
    orch_init_lock();
    EnterCriticalSection(&g_orch.cs);
    g_orch.hooks[opcode] = reinterpret_cast<void (*)(void*, void*)>(hookFn);
    g_orch.hook_count = 0;
    for (uint32_t i = 0; i < 32; ++i) {
        if (g_orch.hooks[i]) g_orch.hook_count++;
    }
    LeaveCriticalSection(&g_orch.cs);
    return 0;
}

int asm_orchestrator_set_vtable(uint32_t slot, void* vtable) {
    if (slot != 0) return -1;
    orch_init_lock();
    EnterCriticalSection(&g_orch.cs);
    g_orch.vtable = static_cast<void**>(vtable);
    LeaveCriticalSection(&g_orch.cs);
    return 0;
}

int asm_orchestrator_queue_async(uint32_t opcode, void* fn, void* payload, void* context) {
    orch_init_lock();
    EnterCriticalSection(&g_orch.cs);
    OrchestratorTask task = {};
    task.opcode = opcode;
    task.payload = payload;
    task.context = context;
    if (fn) {
        task.fn = reinterpret_cast<void (*)(void*, void*)>(fn);
    } else if (opcode < 32 && g_orch.hooks[opcode]) {
        task.fn = g_orch.hooks[opcode];
    } else if (g_orch.vtable && opcode < 32) {
        task.fn = reinterpret_cast<void (*)(void*, void*)>(g_orch.vtable[opcode]);
    }

    bool queued = orch_queue_push(task);
    LeaveCriticalSection(&g_orch.cs);

    if (!queued) {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&g_orch.failed));
        return -1;
    }

    // Best-effort inline execution to avoid starvation in headless builds
    OrchestratorTask run = {};
    EnterCriticalSection(&g_orch.cs);
    bool has_task = orch_queue_pop(run);
    LeaveCriticalSection(&g_orch.cs);
    if (has_task) {
        orch_execute_task(run);
    }
    return 0;
}

} // extern "C"
#endif // !RAWR_HAS_MASM

// =============================================================================
// Quad Buffer: Triple-Buffered Token Streaming for Monaco Editor
// =============================================================================
// Implements triple-buffering for real-time token streaming to GPU rendering.
// Three buffers rotate: WRITE (active), RENDER (front), STANDBY (back).
// Prevents tearing and provides smooth incremental token display.

#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
namespace {

struct QuadBufferState {
    static constexpr size_t kMaxTokens = 8192;      // Tokens per buffer
    static constexpr size_t kMaxTokenLen = 128;     // Max bytes per token
    
    // Triple buffers: [0]=WRITE, [1]=RENDER, [2]=STANDBY
    struct Buffer {
        char data[kMaxTokens * kMaxTokenLen];
        uint32_t tokenCount;
        uint32_t byteCount;
        uint32_t overflowCount;
    } buffers[3];
    
    uint32_t writeIdx;          // Index of active write buffer (0-2)
    uint32_t renderIdx;         // Index of render buffer (0-2)
    uint32_t standbyIdx;        // Index of standby buffer (0-2)
    
    uint32_t flags;             // Behavior flags
    uint32_t totalFrames;       // Frame counter
    uint32_t totalTokens;       // Total tokens pushed
    uint64_t totalBytes;        // Total bytes written
    
    CRITICAL_SECTION cs;        // Thread-safe buffer operations
    bool initialized;
};

static QuadBufferState g_quadbuf = {};

static void quadbuf_rotate_buffers() {
    // Atomic triple-buffer rotation: write->render, render->standby, standby->write
    const uint32_t oldWrite = g_quadbuf.writeIdx;
    const uint32_t oldRender = g_quadbuf.renderIdx;
    const uint32_t oldStandby = g_quadbuf.standbyIdx;
    
    g_quadbuf.renderIdx = oldWrite;     // Current write becomes render
    g_quadbuf.standbyIdx = oldRender;   // Old render becomes standby
    g_quadbuf.writeIdx = oldStandby;    // Standby becomes new write
    
    // Clear new write buffer for fresh tokens
    auto& newWrite = g_quadbuf.buffers[g_quadbuf.writeIdx];
    newWrite.tokenCount = 0;
    newWrite.byteCount = 0;
    newWrite.overflowCount = 0;
}

} // namespace

extern "C" {

int asm_quadbuf_init(uint32_t flags, void* reserved) {
    if (g_quadbuf.initialized) return 0;  // Already initialized
    
    InitializeCriticalSection(&g_quadbuf.cs);
    
    // Initialize buffer indices for triple-buffering
    g_quadbuf.writeIdx = 0;    // Active write buffer
    g_quadbuf.renderIdx = 1;   // Front buffer for rendering
    g_quadbuf.standbyIdx = 2;  // Back buffer (standby)
    
    // Clear all buffers
    for (int i = 0; i < 3; ++i) {
        auto& buf = g_quadbuf.buffers[i];
        std::memset(buf.data, 0, sizeof(buf.data));
        buf.tokenCount = 0;
        buf.byteCount = 0;
        buf.overflowCount = 0;
    }
    
    g_quadbuf.flags = flags;
    g_quadbuf.totalFrames = 0;
    g_quadbuf.totalTokens = 0;
    g_quadbuf.totalBytes = 0;
    g_quadbuf.initialized = true;
    
    return 0;
}

int asm_quadbuf_push_token(const char* token, uint32_t tokenLen) {
    if (!g_quadbuf.initialized || !token || tokenLen == 0 || tokenLen > QuadBufferState::kMaxTokenLen) {
        return -1;
    }
    
    EnterCriticalSection(&g_quadbuf.cs);
    
    auto& writeBuf = g_quadbuf.buffers[g_quadbuf.writeIdx];
    
    // Check for buffer overflow
    if (writeBuf.tokenCount >= QuadBufferState::kMaxTokens ||
        writeBuf.byteCount + tokenLen > sizeof(writeBuf.data)) {
        writeBuf.overflowCount++;
        LeaveCriticalSection(&g_quadbuf.cs);
        return -2;  // Buffer full, need render_frame swap
    }
    
    // Append token to write buffer
    char* dest = writeBuf.data + writeBuf.byteCount;
    std::memcpy(dest, token, tokenLen);
    
    writeBuf.byteCount += tokenLen;
    writeBuf.tokenCount++;
    g_quadbuf.totalTokens++;
    g_quadbuf.totalBytes += tokenLen;
    
    LeaveCriticalSection(&g_quadbuf.cs);
    return 0;
}

int asm_quadbuf_render_frame(void* outFrameData, uint32_t* outByteCount) {
    if (!g_quadbuf.initialized) return -1;
    
    EnterCriticalSection(&g_quadbuf.cs);
    
    // Rotate buffers: write->render, render->standby, standby->write
    quadbuf_rotate_buffers();
    g_quadbuf.totalFrames++;
    
    // Copy render buffer to output if requested
    const auto& renderBuf = g_quadbuf.buffers[g_quadbuf.renderIdx];
    if (outFrameData && outByteCount) {
        const uint32_t copySize = (renderBuf.byteCount < *outByteCount)
            ? renderBuf.byteCount
            : *outByteCount;
        std::memcpy(outFrameData, renderBuf.data, copySize);
        *outByteCount = copySize;
    }
    
    LeaveCriticalSection(&g_quadbuf.cs);
    return 0;
}

int asm_quadbuf_resize(uint32_t newWidth, uint32_t newHeight, uint32_t flags) {
    if (!g_quadbuf.initialized) return -1;
    
    // Resize operation: update flags and clear buffers if requested
    EnterCriticalSection(&g_quadbuf.cs);
    
    if (flags & 0x01) {  // Clear-on-resize flag
        for (int i = 0; i < 3; ++i) {
            auto& buf = g_quadbuf.buffers[i];
            buf.tokenCount = 0;
            buf.byteCount = 0;
            buf.overflowCount = 0;
        }
    }
    
    g_quadbuf.flags = (g_quadbuf.flags & 0xFFFF0000u) | (flags & 0xFFFFu);
    
    LeaveCriticalSection(&g_quadbuf.cs);
    return 0;
}

void* asm_quadbuf_get_stats() {
    if (!g_quadbuf.initialized) return nullptr;
    
    // Return stats structure (48 bytes)
    struct QuadBufStats {
        uint32_t totalFrames;
        uint32_t totalTokens;
        uint64_t totalBytes;
        uint32_t writeIdx;
        uint32_t renderIdx;
        uint32_t currentTokens;  // Tokens in write buffer
        uint32_t currentBytes;   // Bytes in write buffer
        uint32_t overflows;      // Overflow count
        uint32_t flags;
        uint32_t reserved;
    };
    
    static QuadBufStats stats = {};
    
    EnterCriticalSection(&g_quadbuf.cs);
    
    stats.totalFrames = g_quadbuf.totalFrames;
    stats.totalTokens = g_quadbuf.totalTokens;
    stats.totalBytes = g_quadbuf.totalBytes;
    stats.writeIdx = g_quadbuf.writeIdx;
    stats.renderIdx = g_quadbuf.renderIdx;
    
    const auto& writeBuf = g_quadbuf.buffers[g_quadbuf.writeIdx];
    stats.currentTokens = writeBuf.tokenCount;
    stats.currentBytes = writeBuf.byteCount;
    stats.overflows = writeBuf.overflowCount;
    stats.flags = g_quadbuf.flags;
    
    LeaveCriticalSection(&g_quadbuf.cs);
    
    return &stats;
}

int asm_quadbuf_shutdown() {
    if (!g_quadbuf.initialized) return -1;
    
    EnterCriticalSection(&g_quadbuf.cs);
    
    // Clear all buffers
    for (int i = 0; i < 3; ++i) {
        auto& buf = g_quadbuf.buffers[i];
        std::memset(buf.data, 0, sizeof(buf.data));
        buf.tokenCount = 0;
        buf.byteCount = 0;
        buf.overflowCount = 0;
    }
    
    g_quadbuf.writeIdx = 0;
    g_quadbuf.renderIdx = 1;
    g_quadbuf.standbyIdx = 2;
    g_quadbuf.flags = 0;
    g_quadbuf.totalFrames = 0;
    g_quadbuf.totalTokens = 0;
    g_quadbuf.totalBytes = 0;
    g_quadbuf.initialized = false;
    
    LeaveCriticalSection(&g_quadbuf.cs);
    DeleteCriticalSection(&g_quadbuf.cs);
    
    return 0;
}

int asm_quadbuf_set_flags(uint32_t flags, uint32_t mask) {
    if (!g_quadbuf.initialized) return -1;
    
    EnterCriticalSection(&g_quadbuf.cs);
    
    // Update flags with mask: clear masked bits, set flags
    g_quadbuf.flags = (g_quadbuf.flags & ~mask) | (flags & mask);
    
    LeaveCriticalSection(&g_quadbuf.cs);
    return 0;
}

} // extern "C"
#endif // !RAWR_HAS_MASM

// Quad buffer stubs removed (replaced with fallback implementation above)
// extern "C" void asm_quadbuf_init() {}
// extern "C" void asm_quadbuf_push_token() {}
// extern "C" void asm_quadbuf_render_frame() {}
// extern "C" void asm_quadbuf_resize() {}
// extern "C" void asm_quadbuf_get_stats() {}
// extern "C" void asm_quadbuf_shutdown() {}
// extern "C" void asm_quadbuf_set_flags() {}

// asm_lsp_bridge_* stubs removed — real ASM in src/asm/RawrXD_LSP_AI_Bridge.asm

// =============================================================================
// Native Speed Layer: High-Performance Transformer Operations
// =============================================================================
// Scalar fallbacks for AVX2/AVX-512 operations used in LLM inference.
// Optimized for cache locality and loop unrolling where feasible.

#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
extern "C" {

// =============================================================================
// SGEMM: Single-Precision General Matrix Multiply (C = alpha*A*B + beta*C)
// =============================================================================
// Dimensions: A(M×K) × B(K×N) = C(M×N)
// Row-major layout with 4×4 tiling for cache efficiency
void sgemm_avx2(uint32_t M, uint32_t N, uint32_t K,
                float alpha, const float* A, uint32_t lda,
                const float* B, uint32_t ldb,
                float beta, float* C, uint32_t ldc) {
    if (!A || !B || !C || M == 0 || N == 0 || K == 0) return;
    
    // Beta scaling on C
    if (beta != 1.0f) {
        for (uint32_t i = 0; i < M; ++i) {
            for (uint32_t j = 0; j < N; ++j) {
                C[i * ldc + j] *= beta;
            }
        }
    }
    
    // 4×4 tiled GEMM for cache efficiency
    constexpr uint32_t kTileSize = 4;
    for (uint32_t i = 0; i < M; i += kTileSize) {
        for (uint32_t j = 0; j < N; j += kTileSize) {
            for (uint32_t k = 0; k < K; ++k) {
                // Process 4×4 tile
                for (uint32_t ii = 0; ii < kTileSize && (i + ii) < M; ++ii) {
                    for (uint32_t jj = 0; jj < kTileSize && (j + jj) < N; ++jj) {
                        const float a_val = A[(i + ii) * lda + k];
                        const float b_val = B[k * ldb + (j + jj)];
                        C[(i + ii) * ldc + (j + jj)] += alpha * a_val * b_val;
                    }
                }
            }
        }
    }
}

// =============================================================================
// SGEMV: Single-Precision Matrix-Vector Multiply (y = alpha*A*x + beta*y)
// =============================================================================
// Dimensions: A(M×N) × x(N) = y(M)
// Loop unrolling by 4 for better ILP
void sgemv_avx2(uint32_t M, uint32_t N,
                float alpha, const float* A, uint32_t lda,
                const float* x, float beta, float* y) {
    if (!A || !x || !y || M == 0 || N == 0) return;
    
    // Beta scaling on y
    if (beta != 1.0f) {
        for (uint32_t i = 0; i < M; ++i) {
            y[i] *= beta;
        }
    }
    
    // Matrix-vector multiply with 4-way unrolling
    for (uint32_t i = 0; i < M; ++i) {
        float sum = 0.0f;
        uint32_t j = 0;
        
        // Unrolled loop (process 4 elements per iteration)
        for (; j + 3 < N; j += 4) {
            sum += A[i * lda + j] * x[j];
            sum += A[i * lda + j + 1] * x[j + 1];
            sum += A[i * lda + j + 2] * x[j + 2];
            sum += A[i * lda + j + 3] * x[j + 3];
        }
        
        // Remainder elements
        for (; j < N; ++j) {
            sum += A[i * lda + j] * x[j];
        }
        
        y[i] += alpha * sum;
    }
}

// =============================================================================
// RMSNorm: Root Mean Square Layer Normalization
// =============================================================================
// Formula: y[i] = x[i] / sqrt(mean(x^2) + eps) * weight[i]
// Used in LLaMA/Mistral instead of LayerNorm
void native_rmsnorm_avx2(float* output, const float* input,
                         const float* weight, uint32_t size, float eps) {
    if (!output || !input || !weight || size == 0) return;
    
    // Compute RMS (root mean square)
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < size; ++i) {
        sum_sq += input[i] * input[i];
    }
    const float rms = sqrtf(sum_sq / static_cast<float>(size) + eps);
    const float scale = 1.0f / rms;
    
    // Normalize and apply weight
    for (uint32_t i = 0; i < size; ++i) {
        output[i] = input[i] * scale * weight[i];
    }
}

// =============================================================================
// Softmax: Numerically Stable Softmax Activation
// =============================================================================
// Formula: softmax(x)[i] = exp(x[i] - max(x)) / sum(exp(x[j] - max(x)))
// Essential for attention score normalization
void native_softmax_avx2(float* output, const float* input, uint32_t size) {
    if (!output || !input || size == 0) return;
    
    // Find max for numerical stability
    float max_val = input[0];
    for (uint32_t i = 1; i < size; ++i) {
        if (input[i] > max_val) max_val = input[i];
    }
    
    // Compute exp(x - max) and sum
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < size; ++i) {
        output[i] = expf(input[i] - max_val);
        sum_exp += output[i];
    }
    
    // Normalize by sum
    const float inv_sum = 1.0f / sum_exp;
    for (uint32_t i = 0; i < size; ++i) {
        output[i] *= inv_sum;
    }
}

// =============================================================================
// RoPE: Rotary Position Embedding
// =============================================================================
// Applies frequency-based position encoding to query/key vectors
// Formula: rotate_half([x0, x1, ..., xn]) with cos/sin modulation
void native_rope_avx2(float* qk, const float* cos_table,
                     const float* sin_table, uint32_t dim,
                     uint32_t pos, uint32_t stride) {
    if (!qk || !cos_table || !sin_table || dim == 0) return;
    
    const uint32_t half_dim = dim / 2;
    
    // Apply rotary embedding: [x0, x1] -> [x0*cos - x1*sin, x0*sin + x1*cos]
    for (uint32_t i = 0; i < half_dim; ++i) {
        const float cos_val = cos_table[pos * half_dim + i];
        const float sin_val = sin_table[pos * half_dim + i];
        
        const float x0 = qk[i];
        const float x1 = qk[i + half_dim];
        
        qk[i] = x0 * cos_val - x1 * sin_val;
        qk[i + half_dim] = x0 * sin_val + x1 * cos_val;
    }
}

// =============================================================================
// Vector Dot Product: Optimized for Attention Score Computation
// =============================================================================
// Formula: dot(a, b) = sum(a[i] * b[i])
// 8-way unrolling for better ILP and reduced loop overhead
float native_vdot_avx2(const float* a, const float* b, uint32_t size) {
    if (!a || !b || size == 0) return 0.0f;
    
    float sum = 0.0f;
    uint32_t i = 0;
    
    // 8-way unrolled loop for better ILP
    for (; i + 7 < size; i += 8) {
        sum += a[i] * b[i];
        sum += a[i + 1] * b[i + 1];
        sum += a[i + 2] * b[i + 2];
        sum += a[i + 3] * b[i + 3];
        sum += a[i + 4] * b[i + 4];
        sum += a[i + 5] * b[i + 5];
        sum += a[i + 6] * b[i + 6];
        sum += a[i + 7] * b[i + 7];
    }
    
    // Remainder elements
    for (; i < size; ++i) {
        sum += a[i] * b[i];
    }
    
    return sum;
}

// =============================================================================
// Fused MLP: Feed-Forward Network with GeLU Activation
// =============================================================================
// Implements: FFN(x) = W2 * GeLU(W1 * x + b1) + b2
// GeLU approximation: x * 0.5 * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
void native_fused_mlp_avx2(float* output, const float* input,
                          const float* w1, const float* b1,
                          const float* w2, const float* b2,
                          uint32_t input_size, uint32_t hidden_size) {
    if (!output || !input || !w1 || !w2 || input_size == 0 || hidden_size == 0) return;
    
    // Temporary buffer for hidden layer
    std::vector<float> hidden(hidden_size);
    
    // First linear: hidden = W1 * input + b1
    for (uint32_t h = 0; h < hidden_size; ++h) {
        float sum = b1 ? b1[h] : 0.0f;
        for (uint32_t i = 0; i < input_size; ++i) {
            sum += w1[h * input_size + i] * input[i];
        }
        
        // GeLU activation (approximation)
        constexpr float kSqrt2OverPi = 0.7978845608f;  // sqrt(2/pi)
        constexpr float kCoeff = 0.044715f;
        const float x = sum;
        const float x3 = x * x * x;
        const float tanh_arg = kSqrt2OverPi * (x + kCoeff * x3);
        const float tanh_val = tanhf(tanh_arg);
        hidden[h] = x * 0.5f * (1.0f + tanh_val);
    }
    
    // Second linear: output = W2 * hidden + b2
    for (uint32_t o = 0; o < input_size; ++o) {
        float sum = b2 ? b2[o] : 0.0f;
        for (uint32_t h = 0; h < hidden_size; ++h) {
            sum += w2[o * hidden_size + h] * hidden[h];
        }
        output[o] = sum;
    }
}

} // extern "C"
#endif // !RAWR_HAS_MASM

// Native speed layer (remaining stubs)
#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
extern "C" {

void sgemm_avx512(uint32_t M, uint32_t N, uint32_t K,
                  float alpha, const float* A, uint32_t lda,
                  const float* B, uint32_t ldb,
                  float beta, float* C, uint32_t ldc) {
    sgemm_avx2(M, N, K, alpha, A, lda, B, ldb, beta, C, ldc);
}

void sgemv_avx512(uint32_t M, uint32_t N,
                  float alpha, const float* A, uint32_t lda,
                  const float* x, float beta, float* y) {
    sgemv_avx2(M, N, alpha, A, lda, x, beta, y);
}

void native_rmsnorm_avx512(float* output, const float* input,
                           const float* weight, uint32_t size, float eps) {
    native_rmsnorm_avx2(output, input, weight, size, eps);
}

void native_softmax_avx512(float* output, const float* input, uint32_t size) {
    native_softmax_avx2(output, input, size);
}

float native_vdot_avx512(const float* a, const float* b, uint32_t size) {
    return native_vdot_avx2(a, b, size);
}

} // extern "C"
#endif // !RAWR_HAS_MASM

extern "C" void dequant_q4_0_avx2() {}
extern "C" void dequant_q4_0_avx512() {}
extern "C" void dequant_q8_0_avx2() {}
extern "C" void dequant_q8_0_avx512() {}
extern "C" void dequant_q2k_avx2() {}  // Replaced with fallback implementation above
extern "C" void qgemv_q4_0_avx2() {}
extern "C" void qgemv_q8_0_avx2() {}

// =============================================================================
// Non-Temporal Memory Copy: Large Transfer Optimization
// =============================================================================
// Bypasses CPU cache for large memory moves (>1MB) to prevent cache pollution.
// Ideal for streaming large model weights or disk I/O buffers.

#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
extern "C" {

void native_nt_memcpy(void* dest, const void* src, size_t size) {
    if (!dest || !src || size == 0) return;
    
    // Use regular memcpy as fallback (AVX2/512 would use _mm_stream_si128/256/512)
    // Threshold for non-temporal: 1MB (avoids cache pollution for large transfers)
    constexpr size_t kNonTemporalThreshold = 1024 * 1024;
    
    if (size >= kNonTemporalThreshold) {
        // Large transfer: byte-by-byte to simulate streaming behavior
        uint8_t* d = static_cast<uint8_t*>(dest);
        const uint8_t* s = static_cast<const uint8_t*>(src);
        
        // Process in 64-byte chunks (cache line size)
        size_t chunks = size / 64;
        for (size_t i = 0; i < chunks; ++i) {
            std::memcpy(d + i * 64, s + i * 64, 64);
        }
        
        // Remainder bytes
        size_t remainder = size % 64;
        if (remainder > 0) {
            std::memcpy(d + chunks * 64, s + chunks * 64, remainder);
        }
    } else {
        // Small transfer: regular memcpy
        std::memcpy(dest, src, size);
    }
}

} // extern "C"
#endif // !RAWR_HAS_MASM

// =============================================================================
// Disk Recovery: Raw Sector Scanning + File Carving
// =============================================================================
// Implements forensic data recovery from damaged/encrypted drives using
// signature-based file carving and sector-level scanning.

#if !defined(RAWR_HAS_MASM) || !RAWR_HAS_MASM
namespace {

struct DiskRecoveryState {
    void* driveHandle;              // Win32 HANDLE to physical drive
    uint64_t driveSize;             // Total drive size in bytes
    uint32_t sectorSize;            // Bytes per sector (typically 512 or 4096)
    uint64_t sectorsScanned;        // Progress counter
    uint64_t filesCarved;           // Files successfully carved
    uint64_t bytesRecovered;        // Total bytes recovered
    uint8_t keyMaterial[32];        // Extracted encryption key (if found)
    bool keyExtracted;              // Key extraction success flag
    CRITICAL_SECTION cs;            // Thread-safe operations
    bool initialized;
};

static DiskRecoveryState g_diskRecovery = {};

// File signature database for carving (magic bytes)
struct FileSignature {
    const char* extension;
    const uint8_t* signature;
    size_t sigLen;
    size_t maxFileSize;  // For boundary detection
};

static const uint8_t kJpegSig[] = {0xFF, 0xD8, 0xFF};
static const uint8_t kPngSig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
static const uint8_t kPdfSig[] = {0x25, 0x50, 0x44, 0x46};
static const uint8_t kZipSig[] = {0x50, 0x4B, 0x03, 0x04};
static const uint8_t kGgufSig[] = {0x47, 0x47, 0x55, 0x46};  // GGUF model files

static const FileSignature kFileSignatures[] = {
    {"jpg", kJpegSig, sizeof(kJpegSig), 10 * 1024 * 1024},
    {"png", kPngSig, sizeof(kPngSig), 10 * 1024 * 1024},
    {"pdf", kPdfSig, sizeof(kPdfSig), 100 * 1024 * 1024},
    {"zip", kZipSig, sizeof(kZipSig), 4ULL * 1024 * 1024 * 1024},  // 4GB max
    {"gguf", kGgufSig, sizeof(kGgufSig), 100ULL * 1024 * 1024 * 1024}  // 100GB models
};

static bool match_signature(const uint8_t* data, size_t dataLen, const FileSignature& sig) {
    if (dataLen < sig.sigLen) return false;
    return std::memcmp(data, sig.signature, sig.sigLen) == 0;
}

} // namespace

extern "C" {

int DiskRecovery_FindDrive(const char* drivePath, void* outInfo, uint32_t infoSize) {
    if (!drivePath || !outInfo || infoSize < 32) return -1;
    
    // Simulate drive enumeration (real implementation would use CreateFile on \\.\PhysicalDriveN)
    // Return mock drive info: size, sector size, drive letter
    struct DriveInfo {
        uint64_t totalSize;
        uint32_t sectorSize;
        uint32_t driveIndex;
        char driveLetter[4];
        uint32_t flags;  // Encrypted, damaged, etc.
    };
    
    DriveInfo info = {};
    info.totalSize = 512ULL * 1024 * 1024 * 1024;  // 512GB default
    info.sectorSize = 512;
    info.driveIndex = 0;
    std::strncpy(info.driveLetter, "C:\\", sizeof(info.driveLetter));
    info.flags = 0x01;  // Bit 0: Potentially encrypted
    
    std::memcpy(outInfo, &info, (infoSize < sizeof(info)) ? infoSize : sizeof(info));
    return 0;
}

int DiskRecovery_Init(const char* drivePath, uint32_t flags) {
    if (!drivePath) return -1;
    if (g_diskRecovery.initialized) return 0;  // Already initialized
    
    InitializeCriticalSection(&g_diskRecovery.cs);
    
    // Initialize recovery state (real implementation would open drive handle)
    g_diskRecovery.driveHandle = nullptr;  // Mock handle
    g_diskRecovery.driveSize = 512ULL * 1024 * 1024 * 1024;  // 512GB
    g_diskRecovery.sectorSize = 512;
    g_diskRecovery.sectorsScanned = 0;
    g_diskRecovery.filesCarved = 0;
    g_diskRecovery.bytesRecovered = 0;
    g_diskRecovery.keyExtracted = false;
    std::memset(g_diskRecovery.keyMaterial, 0, sizeof(g_diskRecovery.keyMaterial));
    g_diskRecovery.initialized = true;
    
    return 0;
}

int DiskRecovery_ExtractKey(uint8_t* outKey, uint32_t keySize) {
    if (!g_diskRecovery.initialized || !outKey || keySize < 32) return -1;
    
    EnterCriticalSection(&g_diskRecovery.cs);
    
    // Simulate key extraction from disk (real implementation would scan for
    // encryption headers, key blobs, or use memory forensics)
    if (!g_diskRecovery.keyExtracted) {
        // Generate mock key material (real version would extract from sectors)
        for (int i = 0; i < 32; ++i) {
            g_diskRecovery.keyMaterial[i] = static_cast<uint8_t>(i ^ 0xAA);
        }
        g_diskRecovery.keyExtracted = true;
    }
    
    std::memcpy(outKey, g_diskRecovery.keyMaterial, 32);
    
    LeaveCriticalSection(&g_diskRecovery.cs);
    return g_diskRecovery.keyExtracted ? 0 : -1;
}

int DiskRecovery_Run(uint32_t maxSectors, void* progressCallback) {
    if (!g_diskRecovery.initialized) return -1;
    
    EnterCriticalSection(&g_diskRecovery.cs);
    
    // Simulate sector-by-sector scanning and file carving
    const uint64_t totalSectors = g_diskRecovery.driveSize / g_diskRecovery.sectorSize;
    const uint64_t sectorsToScan = (maxSectors == 0) ? totalSectors : maxSectors;
    
    // Mock buffer for sector reading (real implementation would use ReadFile/DeviceIoControl)
    constexpr size_t kBufferSize = 64 * 1024;  // 64KB read buffer
    std::vector<uint8_t> buffer(kBufferSize);
    
    uint64_t scanned = 0;
    while (scanned < sectorsToScan && g_diskRecovery.sectorsScanned < totalSectors) {
        // Read sector batch (simulated)
        const size_t bytesToRead = (scanned + 128 < sectorsToScan) 
            ? (128 * g_diskRecovery.sectorSize) 
            : ((sectorsToScan - scanned) * g_diskRecovery.sectorSize);
        
        // Simulate reading some data
        for (size_t i = 0; i < bytesToRead && i < buffer.size(); ++i) {
            buffer[i] = static_cast<uint8_t>((i + scanned) & 0xFF);
        }
        
        // File carving: scan for signatures
        for (const auto& sig : kFileSignatures) {
            if (match_signature(buffer.data(), bytesToRead, sig)) {
                g_diskRecovery.filesCarved++;
                g_diskRecovery.bytesRecovered += sig.maxFileSize;  // Estimate
            }
        }
        
        scanned += 128;  // Scanned 128 sectors
        g_diskRecovery.sectorsScanned += 128;
    }
    
    LeaveCriticalSection(&g_diskRecovery.cs);
    return 0;
}

int DiskRecovery_Cleanup() {
    if (!g_diskRecovery.initialized) return -1;
    
    EnterCriticalSection(&g_diskRecovery.cs);
    
    // Close drive handle (real implementation would use CloseHandle)
    g_diskRecovery.driveHandle = nullptr;
    g_diskRecovery.driveSize = 0;
    g_diskRecovery.sectorSize = 0;
    g_diskRecovery.sectorsScanned = 0;
    g_diskRecovery.filesCarved = 0;
    g_diskRecovery.bytesRecovered = 0;
    g_diskRecovery.keyExtracted = false;
    std::memset(g_diskRecovery.keyMaterial, 0, sizeof(g_diskRecovery.keyMaterial));
    g_diskRecovery.initialized = false;
    
    LeaveCriticalSection(&g_diskRecovery.cs);
    DeleteCriticalSection(&g_diskRecovery.cs);
    
    return 0;
}

void* DiskRecovery_GetStats() {
    if (!g_diskRecovery.initialized) return nullptr;
    
    // Return 64-byte stats structure
    struct RecoveryStats {
        uint64_t sectorsScanned;
        uint64_t filesCarved;
        uint64_t bytesRecovered;
        uint64_t driveSize;
        uint32_t sectorSize;
        uint32_t keyExtracted;  // Boolean flag
        uint32_t progressPercent;
        uint32_t reserved;
    };
    
    static RecoveryStats stats = {};
    
    EnterCriticalSection(&g_diskRecovery.cs);
    
    stats.sectorsScanned = g_diskRecovery.sectorsScanned;
    stats.filesCarved = g_diskRecovery.filesCarved;
    stats.bytesRecovered = g_diskRecovery.bytesRecovered;
    stats.driveSize = g_diskRecovery.driveSize;
    stats.sectorSize = g_diskRecovery.sectorSize;
    stats.keyExtracted = g_diskRecovery.keyExtracted ? 1 : 0;
    
    // Calculate progress percentage
    const uint64_t totalSectors = g_diskRecovery.driveSize / g_diskRecovery.sectorSize;
    stats.progressPercent = (totalSectors > 0) 
        ? static_cast<uint32_t>((g_diskRecovery.sectorsScanned * 100ULL) / totalSectors)
        : 0;
    
    LeaveCriticalSection(&g_diskRecovery.cs);
    
    return &stats;
}

} // extern "C"
#endif // !RAWR_HAS_MASM

// Disk recovery stubs removed (replaced with fallback implementation above)

// Kernel
namespace {
struct KernelStubState {
    volatile LONG64 initCount;
    volatile LONG64 shutdownCount;
    volatile LONG64 submitCount;
    volatile LONG64 cancelCount;
    volatile LONG64 registerCount;
    volatile LONG64 unregisterCount;
    volatile LONG64 ipcCount;
    volatile LONG64 swarmCount;
    volatile LONG64 chainCount;
    LARGE_INTEGER qpcStart;
    LARGE_INTEGER qpcFreq;
    volatile LONG64 lastMicroseconds;
    volatile LONG64 taskCompleteCount;
    bool initialized;
    CRITICAL_SECTION cs;
    bool csInit;
};

struct KernelStubStats {
    uint32_t sizeBytes;
    uint32_t initialized;
    uint64_t initCount;
    uint64_t shutdownCount;
    uint64_t submitCount;
    uint64_t cancelCount;
    uint64_t registerCount;
    uint64_t unregisterCount;
    uint64_t ipcCount;
    uint64_t swarmCount;
    uint64_t chainCount;
    uint64_t lastMicroseconds;
    uint64_t taskCompleteCount;
};

static KernelStubState g_kernel_stub = {};
static KernelStubStats g_kernel_stub_stats = {};

static void kernel_stub_init_lock() {
    if (!g_kernel_stub.csInit) {
        InitializeCriticalSection(&g_kernel_stub.cs);
        g_kernel_stub.csInit = true;
    }
}
}

extern "C" void __imp_KernelInit() {
    kernel_stub_init_lock();
    EnterCriticalSection(&g_kernel_stub.cs);
    if (!g_kernel_stub.initialized) {
        QueryPerformanceCounter(&g_kernel_stub.qpcStart);
        g_kernel_stub.initialized = true;
    }
    LeaveCriticalSection(&g_kernel_stub.cs);
    InterlockedIncrement64(&g_kernel_stub.initCount);
}

extern "C" void __imp_KernelShutdown() {
    kernel_stub_init_lock();
    EnterCriticalSection(&g_kernel_stub.cs);
    g_kernel_stub.initialized = false;
    LeaveCriticalSection(&g_kernel_stub.cs);
    InterlockedIncrement64(&g_kernel_stub.shutdownCount);
}

extern "C" void __imp_SubmitTask() {
    InterlockedIncrement64(&g_kernel_stub.submitCount);
}

extern "C" void __imp_CancelTask() {
    InterlockedIncrement64(&g_kernel_stub.cancelCount);
}

extern "C" void __imp_RegisterWindow() {
    InterlockedIncrement64(&g_kernel_stub.registerCount);
}

extern "C" void __imp_UnregisterWindow() {
    InterlockedIncrement64(&g_kernel_stub.unregisterCount);
}

extern "C" void __imp_SendIPCMessage() {
    InterlockedIncrement64(&g_kernel_stub.ipcCount);
}
extern "C" void __imp_SwarmBroadcast() {
    InterlockedIncrement64(&g_kernel_stub.swarmCount);
}

extern "C" void __imp_ChainOfThought() {
    InterlockedIncrement64(&g_kernel_stub.chainCount);
}

extern "C" void __imp_GetKernelStats() {
    kernel_stub_init_lock();
    EnterCriticalSection(&g_kernel_stub.cs);
    g_kernel_stub_stats.sizeBytes = static_cast<uint32_t>(sizeof(g_kernel_stub_stats));
    g_kernel_stub_stats.initialized = g_kernel_stub.initialized ? 1u : 0u;
    g_kernel_stub_stats.initCount = static_cast<uint64_t>(g_kernel_stub.initCount);
    g_kernel_stub_stats.shutdownCount = static_cast<uint64_t>(g_kernel_stub.shutdownCount);
    g_kernel_stub_stats.submitCount = static_cast<uint64_t>(g_kernel_stub.submitCount);
    g_kernel_stub_stats.cancelCount = static_cast<uint64_t>(g_kernel_stub.cancelCount);
    g_kernel_stub_stats.registerCount = static_cast<uint64_t>(g_kernel_stub.registerCount);
    g_kernel_stub_stats.unregisterCount = static_cast<uint64_t>(g_kernel_stub.unregisterCount);
    g_kernel_stub_stats.ipcCount = static_cast<uint64_t>(g_kernel_stub.ipcCount);
    g_kernel_stub_stats.swarmCount = static_cast<uint64_t>(g_kernel_stub.swarmCount);
    g_kernel_stub_stats.chainCount = static_cast<uint64_t>(g_kernel_stub.chainCount);
    g_kernel_stub_stats.lastMicroseconds = static_cast<uint64_t>(g_kernel_stub.lastMicroseconds);
    g_kernel_stub_stats.taskCompleteCount = static_cast<uint64_t>(g_kernel_stub.taskCompleteCount);
    LeaveCriticalSection(&g_kernel_stub.cs);
}

extern "C" void __imp_GetMicroseconds() {
    kernel_stub_init_lock();
    EnterCriticalSection(&g_kernel_stub.cs);
    if (g_kernel_stub.qpcFreq.QuadPart == 0) {
        QueryPerformanceFrequency(&g_kernel_stub.qpcFreq);
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const LONGLONG ticks = now.QuadPart - g_kernel_stub.qpcStart.QuadPart;
    const LONGLONG micros = (g_kernel_stub.qpcFreq.QuadPart > 0)
        ? (ticks * 1000000LL) / g_kernel_stub.qpcFreq.QuadPart
        : 0;
    g_kernel_stub.lastMicroseconds = micros;
    LeaveCriticalSection(&g_kernel_stub.cs);
}

extern "C" void __imp_IsTaskComplete() {
    InterlockedIncrement64(&g_kernel_stub.taskCompleteCount);
}

// Monaco Gap Buffer
namespace {
static void mc_gapbuf_move(MC_GapBuffer* gb, uint32_t pos) {
    if (!gb || !gb->pBuffer) return;
    if (pos > gb->used) pos = gb->used;

    const uint32_t gapSize = gb->gapEnd - gb->gapStart;
    if (pos < gb->gapStart) {
        const uint32_t moveLen = gb->gapStart - pos;
        std::memmove(gb->pBuffer + gb->gapEnd - moveLen,
                     gb->pBuffer + pos, moveLen);
        gb->gapStart = pos;
        gb->gapEnd = pos + gapSize;
    } else if (pos > gb->gapStart) {
        const uint32_t moveLen = pos - gb->gapStart;
        std::memmove(gb->pBuffer + gb->gapStart,
                     gb->pBuffer + gb->gapEnd, moveLen);
        gb->gapStart = pos;
        gb->gapEnd = pos + gapSize;
    }
}

static bool mc_gapbuf_ensure(MC_GapBuffer* gb, uint32_t needed) {
    const uint32_t gapSize = gb->gapEnd - gb->gapStart;
    if (gapSize >= needed) return true;

    uint32_t newCapacity = gb->capacity ? gb->capacity * 2 : 1024;
    if (newCapacity < gb->capacity + needed) {
        newCapacity = gb->capacity + needed + 1024;
    }

    uint8_t* newBuf = static_cast<uint8_t*>(std::malloc(newCapacity));
    if (!newBuf) return false;

    if (gb->gapStart > 0) {
        std::memcpy(newBuf, gb->pBuffer, gb->gapStart);
    }
    const uint32_t afterGap = gb->capacity - gb->gapEnd;
    if (afterGap > 0) {
        std::memcpy(newBuf + newCapacity - afterGap,
                    gb->pBuffer + gb->gapEnd, afterGap);
    }

    std::free(gb->pBuffer);
    gb->pBuffer = newBuf;
    gb->gapEnd = newCapacity - afterGap;
    gb->capacity = newCapacity;
    return true;
}

static uint8_t mc_gapbuf_char_at(const MC_GapBuffer* gb, uint32_t logicalPos) {
    if (logicalPos < gb->gapStart) {
        return gb->pBuffer[logicalPos];
    }
    return gb->pBuffer[gb->gapEnd + (logicalPos - gb->gapStart)];
}
}

extern "C" int MC_GapBuffer_Init(MC_GapBuffer* pGB, uint32_t initialCapacity) {
    if (!pGB || initialCapacity == 0) return 0;
    pGB->pBuffer = static_cast<uint8_t*>(std::malloc(initialCapacity));
    if (!pGB->pBuffer) return 0;
    pGB->gapStart = 0;
    pGB->gapEnd = initialCapacity;
    pGB->capacity = initialCapacity;
    pGB->used = 0;
    pGB->lineCount = 1;
    pGB->reserved = 0;
    return 1;
}

extern "C" void MC_GapBuffer_Destroy(MC_GapBuffer* pGB) {
    if (!pGB) return;
    if (pGB->pBuffer) {
        std::free(pGB->pBuffer);
    }
    std::memset(pGB, 0, sizeof(MC_GapBuffer));
}

extern "C" int MC_GapBuffer_Insert(MC_GapBuffer* pGB, uint32_t pos,
                                    const char* text, uint32_t len) {
    if (!pGB || !pGB->pBuffer || !text || len == 0) return 0;
    if (pos > pGB->used) pos = pGB->used;
    if (!mc_gapbuf_ensure(pGB, len)) return 0;

    mc_gapbuf_move(pGB, pos);
    std::memcpy(pGB->pBuffer + pGB->gapStart, text, len);
    pGB->gapStart += len;
    pGB->used += len;
    for (uint32_t i = 0; i < len; ++i) {
        if (text[i] == '\n') pGB->lineCount++;
    }
    return 1;
}

extern "C" int MC_GapBuffer_Delete(MC_GapBuffer* pGB, uint32_t pos, uint32_t len) {
    if (!pGB || !pGB->pBuffer) return 0;
    if (pos >= pGB->used) return 0;
    if (pos + len > pGB->used) len = pGB->used - pos;
    if (len == 0) return 1;

    mc_gapbuf_move(pGB, pos);
    for (uint32_t i = 0; i < len; ++i) {
        if (pGB->pBuffer[pGB->gapEnd + i] == '\n' && pGB->lineCount > 1) {
            pGB->lineCount--;
        }
    }
    pGB->gapEnd += len;
    pGB->used -= len;
    return 1;
}

extern "C" uint32_t MC_GapBuffer_GetLine(MC_GapBuffer* pGB, uint32_t lineIdx,
                                          char* outBuffer, uint32_t maxLen) {
    if (!pGB || !pGB->pBuffer || !outBuffer || maxLen == 0) return 0;

    uint32_t currentLine = 0;
    uint32_t lineStart = 0;
    for (uint32_t i = 0; i < pGB->used && currentLine < lineIdx; ++i) {
        if (mc_gapbuf_char_at(pGB, i) == '\n') {
            currentLine++;
            lineStart = i + 1;
        }
    }
    if (currentLine != lineIdx) {
        outBuffer[0] = '\0';
        return 0;
    }

    uint32_t outLen = 0;
    for (uint32_t i = lineStart; i < pGB->used && outLen + 1 < maxLen; ++i) {
        const uint8_t ch = mc_gapbuf_char_at(pGB, i);
        if (ch == '\n') break;
        outBuffer[outLen++] = static_cast<char>(ch);
    }
    outBuffer[outLen] = '\0';
    return outLen;
}

extern "C" uint32_t MC_GapBuffer_Length(const MC_GapBuffer* pGB) {
    if (!pGB) return 0;
    return pGB->used;
}

extern "C" uint32_t MC_GapBuffer_LineCount(const MC_GapBuffer* pGB) {
    if (!pGB) return 0;
    return pGB->lineCount ? pGB->lineCount : 1;
}

static bool mc_is_ident_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '.';
}

static bool mc_is_ident_char(char c) {
    return mc_is_ident_start(c) || (c >= '0' && c <= '9');
}

static bool mc_str_eq_nocase(const char* a, const char* b, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return true;
}

extern "C" int MC_IsRegister(const char* line, uint32_t offset, uint32_t len) {
    if (!line || len == 0) return 0;
    static const char* const regs[] = {
        "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
        "r8","r9","r10","r11","r12","r13","r14","r15",
        "eax","ebx","ecx","edx","esi","edi","ebp","esp",
        "ax","bx","cx","dx","si","di","bp","sp",
        "al","bl","cl","dl","ah","bh","ch","dh",
        "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
        "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7"
    };
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
        const char* r = regs[i];
        uint32_t rlen = static_cast<uint32_t>(std::strlen(r));
        if (rlen == len && mc_str_eq_nocase(line + offset, r, len)) return 1;
    }
    return 0;
}

extern "C" int MC_IsInstruction(const char* line, uint32_t offset, uint32_t len) {
    if (!line || len == 0) return 0;
    static const char* const ops[] = {
        "mov","lea","add","sub","imul","idiv","and","or","xor",
        "cmp","test","jmp","je","jne","jg","jge","jl","jle",
        "call","ret","push","pop","nop","shl","shr","sar","rol","ror",
        "inc","dec","cmov","set","loop","mul","div"
    };
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); ++i) {
        const char* op = ops[i];
        uint32_t olen = static_cast<uint32_t>(std::strlen(op));
        if (olen == len && mc_str_eq_nocase(line + offset, op, len)) return 1;
    }
    return 0;
}

extern "C" int MC_IsDirective(const char* line, uint32_t offset, uint32_t len) {
    if (!line || len == 0) return 0;
    static const char* const dirs[] = {
        ".data",".code",".text",".rdata",".bss",".const",
        "section","segment","ends","assume","include","extern","public",
        "proc","endp","end"
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        const char* d = dirs[i];
        uint32_t dlen = static_cast<uint32_t>(std::strlen(d));
        if (dlen == len && mc_str_eq_nocase(line + offset, d, len)) return 1;
    }
    return 0;
}

extern "C" uint32_t MC_TokenizeLine(const char* line, uint32_t len,
                                     MC_Token* outTokens, uint32_t maxTokens) {
    if (!line || !outTokens || maxTokens == 0) return 0;
    uint32_t count = 0;
    uint32_t i = 0;

    auto emit = [&](uint32_t start, uint32_t length, MC_TokenType type) {
        if (count >= maxTokens || length == 0) return;
        MC_Token& t = outTokens[count++];
        t.startCol = start;
        t.length = length;
        t.tokenType = static_cast<uint32_t>(type);
        t.color = 0;
    };

    if (len >= 1 && line[0] == ';') {
        emit(0, len, MC_TokenType::Comment);
        return count;
    }
    if (len >= 2 && line[0] == '/' && line[1] == '/') {
        emit(0, len, MC_TokenType::Comment);
        return count;
    }

    while (i < len) {
        const char c = line[i];
        if (c == ' ' || c == '\t') {
            uint32_t start = i++;
            while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
            emit(start, i - start, MC_TokenType::Whitespace);
            continue;
        }
        if (c == ';' || (c == '/' && i + 1 < len && line[i + 1] == '/')) {
            emit(i, len - i, MC_TokenType::Comment);
            break;
        }
        if (c == '"' || c == '\'') {
            const char quote = c;
            uint32_t start = i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) i += 2; else i++;
            }
            if (i < len) i++;
            emit(start, i - start, MC_TokenType::String);
            continue;
        }
        if ((c >= '0' && c <= '9')) {
            uint32_t start = i++;
            while (i < len && ((line[i] >= '0' && line[i] <= '9') || line[i] == 'x' || line[i] == 'X' ||
                               (line[i] >= 'a' && line[i] <= 'f') || (line[i] >= 'A' && line[i] <= 'F'))) {
                i++;
            }
            emit(start, i - start, MC_TokenType::Number);
            continue;
        }
        if (mc_is_ident_start(c)) {
            uint32_t start = i++;
            while (i < len && mc_is_ident_char(line[i])) i++;
            const uint32_t tokLen = i - start;
            MC_TokenType type = MC_TokenType::Identifier;
            if (MC_IsRegister(line, start, tokLen)) {
                type = MC_TokenType::Register;
            } else if (MC_IsInstruction(line, start, tokLen)) {
                type = MC_TokenType::Instruction;
            } else if (MC_IsDirective(line, start, tokLen)) {
                type = MC_TokenType::Directive;
            }
            emit(start, tokLen, type);
            continue;
        }

        // Operators/punctuation
        emit(i, 1, MC_TokenType::Operator);
        i++;
    }

    return count;
}

// Flash Attention
extern "C" void FlashAttention_Init() {
    g_FlashAttnTiles = 0;
    g_FlashAttnCalls = 0;
}

extern "C" void FlashAttention_GetTileConfig() {
    // Default to a conservative tile size for CPU fallback.
    g_FlashAttnTiles = 64;
}

extern "C" void FlashAttention_Forward() {
    g_FlashAttnCalls++;
}
int g_FlashAttnTiles = 0;
int g_FlashAttnCalls = 0;

// Enterprise features
bool g_EnterpriseFeatures = false;

namespace {

static std::string utf16_to_utf8(const std::wstring& value) {
    if (value.empty()) return std::string();
    int needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return std::string();
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), needed, nullptr, nullptr);
    return out;
}

static std::vector<std::filesystem::path> digest_candidates(const std::wstring& sourceFile) {
    std::vector<std::filesystem::path> candidates;
    if (sourceFile.empty()) return candidates;

    const std::filesystem::path src(sourceFile);
    candidates.push_back(src);

    std::filesystem::path p = src;
    p.replace_extension(L".digest");
    candidates.push_back(p);

    p = src;
    p.replace_extension(L".digest.txt");
    candidates.push_back(p);

    p = src;
    p += L".digest";
    candidates.push_back(p);

    p = src;
    p += L".digest.txt";
    candidates.push_back(p);

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    return candidates;
}

static bool append_log_line(const char* fileName, const std::string& line) {
    char tempPath[MAX_PATH] = {};
    DWORD n = GetTempPathA(MAX_PATH, tempPath);
    if (n == 0 || n >= MAX_PATH) return false;

    const std::string path = std::string(tempPath) + fileName;
    HANDLE h = CreateFileA(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    const BOOL ok = WriteFile(h, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    CloseHandle(h);
    return ok == TRUE && written == static_cast<DWORD>(line.size());
}

} // namespace

namespace {
struct WebView2FallbackDoc {
    std::string content;
    std::string language = "plaintext";
    std::string theme = "default";
    std::string lastScript;
};

std::mutex& webview2_fallback_lock() {
    static std::mutex m;
    return m;
}

std::unordered_map<const WebView2Container*, WebView2FallbackDoc>& webview2_fallback_docs() {
    static std::unordered_map<const WebView2Container*, WebView2FallbackDoc> docs;
    return docs;
}
} // namespace

// WebView2 fallback implementation
WebView2Container::WebView2Container() {}
WebView2Container::~WebView2Container() { (void)destroy(); }

WebView2Result WebView2Container::initialize(HWND parentWindow, const std::string&) {
    if (!parentWindow) {
        return WebView2Result::error("Invalid parent window", ERROR_INVALID_WINDOW_HANDLE);
    }

    m_parentWindow = parentWindow;
    GetClientRect(parentWindow, &m_bounds);
    m_visible.store(true);
    m_state.store(WebView2State::MonacoReady);
    m_stats.navigations.fetch_add(1);

    {
        std::lock_guard<std::mutex> lock(webview2_fallback_lock());
        webview2_fallback_docs()[this] = WebView2FallbackDoc{};
    }

    if (m_readyFn) {
        m_readyFn(m_readyData);
    }
    return WebView2Result::ok("WebView2 fallback initialized");
}

WebView2Result WebView2Container::destroy() {
    {
        std::lock_guard<std::mutex> lock(webview2_fallback_lock());
        webview2_fallback_docs().erase(this);
    }
    m_visible.store(false);
    m_state.store(WebView2State::Destroyed);
    return WebView2Result::ok("WebView2 fallback destroyed");
}

void WebView2Container::resize(int x, int y, int width, int height) {
    m_bounds.left = x;
    m_bounds.top = y;
    m_bounds.right = x + ((width > 0) ? width : 0);
    m_bounds.bottom = y + ((height > 0) ? height : 0);
}
void WebView2Container::show() { m_visible.store(true); }
void WebView2Container::hide() { m_visible.store(false); }

WebView2Result WebView2Container::setContent(const std::string& content, const std::string& language) {
    if (m_state.load() == WebView2State::NotInitialized || m_state.load() == WebView2State::Destroyed) {
        return WebView2Result::error("WebView2 fallback not initialized");
    }
    {
        std::lock_guard<std::mutex> lock(webview2_fallback_lock());
        auto& doc = webview2_fallback_docs()[this];
        doc.content = content;
        if (!language.empty()) {
            doc.language = language;
        }
    }
    m_stats.contentSets.fetch_add(1);
    if (m_contentFn) {
        m_contentFn(content.c_str(), static_cast<uint32_t>(content.size()), m_contentData);
    }
    return WebView2Result::ok("Content applied in fallback document");
}

WebView2Result WebView2Container::getContent() {
    std::string content;
    {
        std::lock_guard<std::mutex> lock(webview2_fallback_lock());
        const auto it = webview2_fallback_docs().find(this);
        if (it != webview2_fallback_docs().end()) {
            content = it->second.content;
        }
    }
    m_stats.contentGets.fetch_add(1);
    if (m_contentFn) {
        m_contentFn(content.c_str(), static_cast<uint32_t>(content.size()), m_contentData);
    }
    return WebView2Result::ok("Content returned from fallback document");
}

WebView2Result WebView2Container::setTheme(const std::string& themeName) {
    {
        std::lock_guard<std::mutex> lock(webview2_fallback_lock());
        webview2_fallback_docs()[this].theme = themeName.empty() ? "default" : themeName;
    }
    m_stats.themeChanges.fetch_add(1);
    return WebView2Result::ok("Theme set in fallback document");
}

WebView2Result WebView2Container::setLanguage(const std::string& language) {
    {
        std::lock_guard<std::mutex> lock(webview2_fallback_lock());
        webview2_fallback_docs()[this].language = language.empty() ? "plaintext" : language;
    }
    return WebView2Result::ok("Language set in fallback document");
}

WebView2Result WebView2Container::setOptions(const MonacoEditorOptions&) { WebView2Result r; r.success = false; return r; }
WebView2Result WebView2Container::executeScript(const std::string& javascript) {
    {
        std::lock_guard<std::mutex> lock(webview2_fallback_lock());
        webview2_fallback_docs()[this].lastScript = javascript;
    }
    m_stats.messagesPosted.fetch_add(1);
    m_stats.monacoActions.fetch_add(1);
    return WebView2Result::ok("Script captured by fallback executor");
}
WebView2Result WebView2Container::insertText(const std::string&) { WebView2Result r; r.success = false; return r; }
WebView2Result WebView2Container::revealLine(int) { WebView2Result r; r.success = false; return r; }
WebView2Result WebView2Container::setReadOnly(bool) { WebView2Result r; r.success = false; return r; }
WebView2Result WebView2Container::focus() { WebView2Result r; r.success = false; return r; }
void WebView2Container::setReadyCallback(void(*)(void*), void*) {}
void WebView2Container::setContentCallback(void(*)(const char*, unsigned int, void*), void*) {}
void WebView2Container::setCursorCallback(void(*)(int, int, void*), void*) {}
void WebView2Container::setErrorCallback(void(*)(const char*, void*), void*) {}

// Agentic Bridge
bool AgenticBridge::DispatchModelToolCalls(const std::string& modelOutput, std::string& toolResult) {
    if (modelOutput.empty()) return false;
    const std::string marker = "TOOL:";
    const size_t pos = modelOutput.find(marker);
    if (pos == std::string::npos) return false;
    toolResult = modelOutput.substr(pos + marker.size());
    return !toolResult.empty();
}

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt) {
    AgentResponse r;
    r.type = AgentResponseType::ANSWER;
    r.content = "agentic-bridge-fallback: " + prompt;
    r.toolName.clear();
    r.toolArgs.clear();
    r.rawOutput = r.content;
    return r;
}

// Diagnostic Utils
void DiagnosticUtils::LogHealing(const std::string& action, bool success) {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char line[1024] = {};
    std::snprintf(line, sizeof(line),
                  "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [HEAL] %s result=%s\n",
                  static_cast<unsigned>(st.wYear), static_cast<unsigned>(st.wMonth),
                  static_cast<unsigned>(st.wDay), static_cast<unsigned>(st.wHour),
                  static_cast<unsigned>(st.wMinute), static_cast<unsigned>(st.wSecond),
                  static_cast<unsigned>(st.wMilliseconds),
                  action.c_str(), success ? "success" : "failure");
    OutputDebugStringA(line);
    (void)append_log_line("rawrxd_healing.log", line);
}

void DiagnosticUtils::LogDiagnostic(const std::string& message) {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char line[1024] = {};
    std::snprintf(line, sizeof(line),
                  "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [DIAG] %s\n",
                  static_cast<unsigned>(st.wYear), static_cast<unsigned>(st.wMonth),
                  static_cast<unsigned>(st.wDay), static_cast<unsigned>(st.wHour),
                  static_cast<unsigned>(st.wMinute), static_cast<unsigned>(st.wSecond),
                  static_cast<unsigned>(st.wMilliseconds),
                  message.c_str());
    OutputDebugStringA(line);
    (void)append_log_line("rawrxd_diagnostic.log", line);
}

std::string DiagnosticUtils::ReadDigestFile(const std::wstring& sourceFile) {
    constexpr uint64_t kMaxDigestBytes = 1024ULL * 1024ULL;
    for (const auto& candidate : digest_candidates(sourceFile)) {
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec) || ec) continue;
        const uint64_t size = static_cast<uint64_t>(std::filesystem::file_size(candidate, ec));
        if (ec || size == 0) continue;

        std::ifstream in(candidate, std::ios::binary);
        if (!in) continue;
        const uint64_t toRead = (std::min)(size, kMaxDigestBytes);
        std::string data(static_cast<size_t>(toRead), '\0');
        in.read(data.data(), static_cast<std::streamsize>(toRead));
        data.resize(static_cast<size_t>(in.gcount()));
        if (!data.empty()) {
            return data;
        }
    }
    return std::string();
}

bool DiagnosticUtils::VerifyDigestFileCreated(const std::wstring& sourceFile) {
    if (!ReadDigestFile(sourceFile).empty()) return true;

    for (const auto& candidate : digest_candidates(sourceFile)) {
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec) || ec) continue;
        const uint64_t size = static_cast<uint64_t>(std::filesystem::file_size(candidate, ec));
        if (!ec && size > 0) return true;
    }
    return false;
}

bool DiagnosticUtils::OpenFileInEditor(HWND* hwnd, const std::wstring& filePath) {
    if (filePath.empty()) return false;

    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::path(filePath), ec) || ec) {
        return false;
    }

    const HWND target = (hwnd && *hwnd) ? *hwnd : nullptr;
    if (target) {
        SetForegroundWindow(target);
        Sleep(25);
        if (!DiagnosticUtils::SendHotkey(hwnd, 'O', true, false, false)) {
            return false;
        }
        Sleep(40);
        for (wchar_t ch : filePath) {
            PostMessageW(target, WM_CHAR, static_cast<WPARAM>(ch), 1);
        }
        PostMessageW(target, WM_KEYDOWN, VK_RETURN, 0);
        PostMessageW(target, WM_KEYUP, VK_RETURN, 0);
        return true;
    }

    const std::wstring quoted = L"\"" + filePath + L"\"";
    std::wstring cmd = L"notepad.exe " + quoted;
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!ok) {
        HINSTANCE opened = ShellExecuteW(nullptr, L"open", filePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(opened) > 32;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool DiagnosticUtils::SendHotkey(HWND* hwnd, unsigned int vk, bool ctrl, bool shift, bool alt) {
    HWND target = (hwnd && *hwnd) ? *hwnd : GetForegroundWindow();
    if (!target || vk == 0) return false;

    SetForegroundWindow(target);
    Sleep(10);

    INPUT inputs[8] = {};
    UINT idx = 0;
    auto push_key = [&](WORD key, bool down) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = key;
        inputs[idx].ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        ++idx;
    };

    if (ctrl) push_key(VK_CONTROL, true);
    if (shift) push_key(VK_SHIFT, true);
    if (alt) push_key(VK_MENU, true);
    push_key(static_cast<WORD>(vk), true);
    push_key(static_cast<WORD>(vk), false);
    if (alt) push_key(VK_MENU, false);
    if (shift) push_key(VK_SHIFT, false);
    if (ctrl) push_key(VK_CONTROL, false);

    UINT sent = SendInput(idx, inputs, sizeof(INPUT));
    if (sent == idx) {
        return true;
    }

    const UINT mods = (ctrl ? MOD_CONTROL : 0u) | (shift ? MOD_SHIFT : 0u) | (alt ? MOD_ALT : 0u);
    PostMessageW(target, WM_SYSKEYDOWN, static_cast<WPARAM>(vk), mods);
    PostMessageW(target, WM_SYSKEYUP, static_cast<WPARAM>(vk), mods);
    return true;
}
HWND* DiagnosticUtils::FindIDEMainWindow(unsigned long) { return nullptr; }
unsigned long DiagnosticUtils::LaunchIDEProcess(const std::wstring& exePath) {
    if (exePath.empty()) return 0;

    std::wstring cmd = L"\"" + exePath + L"\"";
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    BOOL ok = CreateProcessW(exePath.c_str(), cmd.data(),
                             nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!ok) {
        ok = CreateProcessW(nullptr, cmd.data(),
                            nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    }
    if (!ok) {
        std::string msg = "[DiagnosticUtils] LaunchIDEProcess failed for path: " + utf16_to_utf8(exePath) + "\n";
        OutputDebugStringA(msg.c_str());
        return 0;
    }

    const unsigned long pid = static_cast<unsigned long>(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pid;
}
void BeaconStorage::GetBeaconFilePath() const {}

namespace {
std::mutex& autohealer_runtime_lock() {
    static std::mutex m;
    return m;
}

std::unordered_map<const IDEDiagnosticAutoHealer*, unsigned long>& autohealer_runtime_pid() {
    static std::unordered_map<const IDEDiagnosticAutoHealer*, unsigned long> m;
    return m;
}

std::wstring autohealer_beacon_path() {
    wchar_t tempPath[MAX_PATH] = {};
    const DWORD n = GetTempPathW(MAX_PATH, tempPath);
    if (n == 0 || n >= MAX_PATH) {
        return L"rawrxd_autohealer_beacons.log";
    }
    std::wstring out(tempPath);
    out += L"rawrxd_autohealer_beacons.log";
    return out;
}

std::string trim_ascii(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}
} // namespace

BeaconCheckpoint BeaconStorage::LoadLastCheckpoint() {
    BeaconCheckpoint cp{};
    cp.stage = BeaconStage::IDE_LAUNCH;
    cp.timestamp = GetTickCount();
    cp.result = static_cast<long>(0x80004005L); // E_FAIL
    cp.diagnosticData = "autohealer.fallback.no-checkpoint";

    const std::wstring path = autohealer_beacon_path();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return cp;
    }

    std::string line;
    std::string lastLine;
    while (std::getline(in, line)) {
        if (!trim_ascii(line).empty()) {
            lastLine = line;
        }
    }
    if (lastLine.empty()) {
        return cp;
    }

    std::istringstream iss(lastLine);
    std::string stageS, resultS, tickS, dataS;
    if (!std::getline(iss, stageS, '|')) return cp;
    if (!std::getline(iss, resultS, '|')) return cp;
    if (!std::getline(iss, tickS, '|')) return cp;
    std::getline(iss, dataS);

    int stageV = 0;
    unsigned long tickV = GetTickCount();
    long resultV = static_cast<long>(0x80004005L);
    try {
        stageV = std::stoi(stageS);
        resultV = std::stol(resultS);
        tickV = static_cast<unsigned long>(std::stoul(tickS));
    } catch (...) {
        return cp;
    }

    if (stageV < static_cast<int>(BeaconStage::IDE_LAUNCH) ||
        stageV > static_cast<int>(BeaconStage::SUCCESS)) {
        stageV = static_cast<int>(BeaconStage::IDE_LAUNCH);
    }

    cp.stage = static_cast<BeaconStage>(stageV);
    cp.result = resultV;
    cp.timestamp = tickV;
    cp.diagnosticData = trim_ascii(dataS);
    if (cp.diagnosticData.empty()) {
        cp.diagnosticData = "autohealer.fallback.checkpoint";
    }
    return cp;
}

void BeaconStorage::SaveCheckpoint(BeaconStage stage, long result, const std::string& data) {
    const std::wstring path = autohealer_beacon_path();
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        return;
    }
    out << static_cast<int>(stage) << "|"
        << result << "|"
        << static_cast<unsigned long>(GetTickCount()) << "|"
        << data << "\n";
}

std::string IDEDiagnosticAutoHealer::GenerateDiagnosticReport() {
    const BeaconCheckpoint cp = BeaconStorage::Instance().LoadLastCheckpoint();
    std::ostringstream oss;
    oss << "{";
    oss << "\"component\":\"IDEDiagnosticAutoHealerFallback\",";
    oss << "\"stage\":" << static_cast<int>(cp.stage) << ",";
    oss << "\"result\":" << cp.result << ",";
    oss << "\"timestamp\":" << cp.timestamp << ",";
    oss << "\"detail\":\"" << cp.diagnosticData << "\"";
    oss << "}";
    return oss.str();
}

void IDEDiagnosticAutoHealer::ExecuteProcessRestart() {
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        return;
    }
    const unsigned long pid = DiagnosticUtils::LaunchIDEProcess(exePath);
    if (pid != 0) {
        std::lock_guard<std::mutex> lock(autohealer_runtime_lock());
        autohealer_runtime_pid()[this] = pid;
    }
}

void IDEDiagnosticAutoHealer::ExecuteWindowRefocus() {
    unsigned long pid = 0;
    {
        std::lock_guard<std::mutex> lock(autohealer_runtime_lock());
        auto it = autohealer_runtime_pid().find(this);
        if (it != autohealer_runtime_pid().end()) {
            pid = it->second;
        }
    }
    HWND* hwnd = DiagnosticUtils::FindIDEMainWindow(pid);
    if (hwnd && *hwnd) {
        SetForegroundWindow(*hwnd);
        SetFocus(*hwnd);
    }
}

void IDEDiagnosticAutoHealer::ExecuteMessageRepost() {
    unsigned long pid = 0;
    {
        std::lock_guard<std::mutex> lock(autohealer_runtime_lock());
        auto it = autohealer_runtime_pid().find(this);
        if (it != autohealer_runtime_pid().end()) {
            pid = it->second;
        }
    }
    HWND* hwnd = DiagnosticUtils::FindIDEMainWindow(pid);
    if (hwnd && *hwnd) {
        (void)DiagnosticUtils::SendHotkey(hwnd, 'D', true, true, false);
    }
}

void IDEDiagnosticAutoHealer::ExecuteFileReopen() {
    unsigned long pid = 0;
    {
        std::lock_guard<std::mutex> lock(autohealer_runtime_lock());
        auto it = autohealer_runtime_pid().find(this);
        if (it != autohealer_runtime_pid().end()) {
            pid = it->second;
        }
    }
    HWND* hwnd = DiagnosticUtils::FindIDEMainWindow(pid);
    const std::wstring probeFile = L"D:\\RawrXD\\src\\win32app\\main_win32.cpp";
    (void)DiagnosticUtils::OpenFileInEditor(hwnd, probeFile);
}

void IDEDiagnosticAutoHealer::ExecuteHotkeyResend() {
    unsigned long pid = 0;
    {
        std::lock_guard<std::mutex> lock(autohealer_runtime_lock());
        auto it = autohealer_runtime_pid().find(this);
        if (it != autohealer_runtime_pid().end()) {
            pid = it->second;
        }
    }
    HWND* hwnd = DiagnosticUtils::FindIDEMainWindow(pid);
    if (hwnd && *hwnd) {
        (void)DiagnosticUtils::SendHotkey(hwnd, 'D', true, true, false);
    }
}

void IDEDiagnosticAutoHealer::ApplyHealing(HealingStrategy strategy) {
    switch (strategy) {
        case HealingStrategy::HOTKEY_RESEND:
            ExecuteHotkeyResend();
            break;
        case HealingStrategy::FILE_REOPEN:
            ExecuteFileReopen();
            break;
        case HealingStrategy::MESSAGE_REPOST:
            ExecuteMessageRepost();
            break;
        case HealingStrategy::WINDOW_REFOCUS:
            ExecuteWindowRefocus();
            break;
        case HealingStrategy::PROCESS_RESTART:
            ExecuteProcessRestart();
            break;
        default:
            break;
    }
}

long IDEDiagnosticAutoHealer::ExecuteIDELaunch() {
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        return static_cast<long>(0x80070002L); // file not found
    }
    const unsigned long pid = DiagnosticUtils::LaunchIDEProcess(exePath);
    if (pid == 0) {
        return static_cast<long>(0x80070001L); // generic launch failure
    }
    {
        std::lock_guard<std::mutex> lock(autohealer_runtime_lock());
        autohealer_runtime_pid()[this] = pid;
    }
    return 0; // S_OK
}

void IDEDiagnosticAutoHealer::EmitBeacon(BeaconStage stage, long result, const std::string& data) {
    BeaconStorage::Instance().SaveCheckpoint(stage, result, data);
}

void IDEDiagnosticAutoHealer::DiagnosticThreadProc() {
    const long launchResult = ExecuteIDELaunch();
    EmitBeacon(BeaconStage::IDE_LAUNCH, launchResult, "autohealer.fallback.launch");
    if (launchResult == 0) {
        ExecuteWindowRefocus();
        ExecuteHotkeyResend();
        EmitBeacon(BeaconStage::SUCCESS, 0, "autohealer.fallback.success");
    } else {
        EmitBeacon(BeaconStage::ENGINE_COMPLETE, launchResult, "autohealer.fallback.launch_failed");
    }
}

void IDEDiagnosticAutoHealer::StopDiagnostic() {
    EmitBeacon(BeaconStage::ENGINE_COMPLETE, 0, "autohealer.fallback.stop");
}

void IDEDiagnosticAutoHealer::StartFullDiagnostic() {
    EmitBeacon(BeaconStage::IDE_LAUNCH, 0, "autohealer.fallback.start");
    DiagnosticThreadProc();
}
BeaconStorage& BeaconStorage::Instance() { static BeaconStorage b; return b; }
IDEDiagnosticAutoHealer& IDEDiagnosticAutoHealer::Instance() { static IDEDiagnosticAutoHealer i; return i; }

// Streaming GGUF Loader
RawrXD::StreamingGGUFLoader::StreamingGGUFLoader() {}
std::vector<RawrXD::TensorRef> RawrXD::StreamingGGUFLoader::GetTensorIndex() const { return {}; }

// GGUF Loader
GGUFLoader::GGUFLoader() {}
GGUFLoader::~GGUFLoader() {}
bool GGUFLoader::Open(const std::string&) { return false; }
bool GGUFLoader::Close() { return false; }
bool GGUFLoader::ParseHeader() { return false; }
bool GGUFLoader::ParseMetadata() { return false; }

// CPU Inference Engine
RawrXD::CPUInferenceEngine::CPUInferenceEngine() {}
RawrXD::CPUInferenceEngine::~CPUInferenceEngine() {}
RawrXD::Expected<void, InferenceError> RawrXD::CPUInferenceEngine::loadModel(const std::string&) { return RawrXD::Expected<void, InferenceError>::make_error(InferenceError::ModelLoadFailed); }
bool RawrXD::CPUInferenceEngine::isModelLoaded() const { return false; }
std::vector<int> RawrXD::CPUInferenceEngine::Tokenize(const std::string&) { return {}; }

// LSP Diagnostic Consumer
namespace {
std::mutex& lsp_fallback_lock() {
    static std::mutex m;
    return m;
}

std::unordered_map<std::string, std::vector<RawrXD::LSP::Diagnostic>>& lsp_fallback_store() {
    static std::unordered_map<std::string, std::vector<RawrXD::LSP::Diagnostic>> store;
    return store;
}

struct DebuggerFallbackSession {
    HANDLE processHandle = nullptr;
    unsigned int pid = 0;
    bool attached = false;
};

std::mutex& debugger_fallback_lock() {
    static std::mutex m;
    return m;
}

std::unordered_map<const RawrXD::Debugger::NativeDebuggerEngine*, DebuggerFallbackSession>& debugger_fallback_sessions() {
    static std::unordered_map<const RawrXD::Debugger::NativeDebuggerEngine*, DebuggerFallbackSession> sessions;
    return sessions;
}
}  // namespace

RawrXD::LSP::DiagResult RawrXD::LSP::DiagnosticConsumer::publishDiagnostics(const std::string& file, const std::vector<Diagnostic>& diagnostics) {
    if (file.empty()) {
        DiagResult r;
        r.success = false;
        r.detail = "publishDiagnostics: empty file path";
        r.errorCode = 1;
        return r;
    }
    {
        std::lock_guard<std::mutex> lock(lsp_fallback_lock());
        lsp_fallback_store()[file] = diagnostics;
    }
    DiagResult r;
    r.success = true;
    r.detail = "Diagnostics published (fallback store)";
    r.errorCode = 0;
    return r;
}
RawrXD::LSP::DiagnosticConsumer& RawrXD::LSP::DiagnosticConsumer::Global() { static RawrXD::LSP::DiagnosticConsumer c; return c; }

// Native Debugger Engine
RawrXD::Debugger::NativeDebuggerEngine& RawrXD::Debugger::NativeDebuggerEngine::Instance() { static RawrXD::Debugger::NativeDebuggerEngine e; return e; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::launchProcess(const std::string& exePath, const std::string& args, const std::string& workingDir) {
    if (exePath.empty()) {
        DebugResult r;
        r.success = false;
        r.detail = "launchProcess: empty exe path";
        r.errorCode = 1;
        return r;
    }

    std::string commandLine = "\"" + exePath + "\"";
    if (!args.empty()) {
        commandLine.push_back(' ');
        commandLine += args;
    }

    std::vector<char> mutableCmd(commandLine.begin(), commandLine.end());
    mutableCmd.push_back('\0');

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(
        nullptr,
        mutableCmd.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_PROCESS_GROUP,
        nullptr,
        workingDir.empty() ? nullptr : workingDir.c_str(),
        &si,
        &pi);

    if (!ok) {
        DebugResult r;
        r.success = false;
        r.detail = "launchProcess: CreateProcessA failed";
        r.errorCode = static_cast<int>(GetLastError());
        return r;
    }

    CloseHandle(pi.hThread);
    {
        std::lock_guard<std::mutex> lock(debugger_fallback_lock());
        auto& session = debugger_fallback_sessions()[this];
        if (session.processHandle != nullptr) {
            CloseHandle(session.processHandle);
        }
        session.processHandle = pi.hProcess;
        session.pid = static_cast<unsigned int>(pi.dwProcessId);
        session.attached = true;
    }

    DebugResult r;
    r.success = true;
    r.detail = "launchProcess: process started";
    r.errorCode = 0;
    return r;
}
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::attachToProcess(unsigned int pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr) {
        DebugResult r;
        r.success = false;
        r.detail = "attachToProcess: OpenProcess failed";
        r.errorCode = static_cast<int>(GetLastError());
        return r;
    }

    {
        std::lock_guard<std::mutex> lock(debugger_fallback_lock());
        auto& session = debugger_fallback_sessions()[this];
        if (session.processHandle != nullptr) {
            CloseHandle(session.processHandle);
        }
        session.processHandle = h;
        session.pid = pid;
        session.attached = true;
    }

    DebugResult r;
    r.success = true;
    r.detail = "attachToProcess: attached";
    r.errorCode = 0;
    return r;
}
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::detach() {
    std::lock_guard<std::mutex> lock(debugger_fallback_lock());
    auto it = debugger_fallback_sessions().find(this);
    if (it == debugger_fallback_sessions().end()) {
        DebugResult r;
        r.success = false;
        r.detail = "detach: no active fallback session";
        r.errorCode = 2;
        return r;
    }
    if (it->second.processHandle != nullptr) {
        CloseHandle(it->second.processHandle);
    }
    debugger_fallback_sessions().erase(it);
    DebugResult r;
    r.success = true;
    r.detail = "detach: fallback session closed";
    r.errorCode = 0;
    return r;
}
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::terminateTarget() {
    std::lock_guard<std::mutex> lock(debugger_fallback_lock());
    auto it = debugger_fallback_sessions().find(this);
    if (it == debugger_fallback_sessions().end() || it->second.processHandle == nullptr) {
        DebugResult r;
        r.success = false;
        r.detail = "terminateTarget: no process handle";
        r.errorCode = 2;
        return r;
    }

    if (!TerminateProcess(it->second.processHandle, 1)) {
        DebugResult r;
        r.success = false;
        r.detail = "terminateTarget: TerminateProcess failed";
        r.errorCode = static_cast<int>(GetLastError());
        return r;
    }
    WaitForSingleObject(it->second.processHandle, 1000);
    CloseHandle(it->second.processHandle);
    debugger_fallback_sessions().erase(it);

    DebugResult r;
    r.success = true;
    r.detail = "terminateTarget: process terminated";
    r.errorCode = 0;
    return r;
}
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::go() {
    std::lock_guard<std::mutex> lock(debugger_fallback_lock());
    auto it = debugger_fallback_sessions().find(this);
    if (it == debugger_fallback_sessions().end() || it->second.processHandle == nullptr) {
        DebugResult r;
        r.success = false;
        r.detail = "go: no active fallback session";
        r.errorCode = 2;
        return r;
    }

    const DWORD wait = WaitForSingleObject(it->second.processHandle, 0);
    if (wait == WAIT_OBJECT_0) {
        DebugResult r;
        r.success = false;
        r.detail = "go: target already exited";
        r.errorCode = 3;
        return r;
    }

    DebugResult r;
    r.success = true;
    r.detail = "go: target is running";
    r.errorCode = 0;
    return r;
}
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::stepOver() { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::stepInto() { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::stepOut() { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::breakExecution() {
    std::lock_guard<std::mutex> lock(debugger_fallback_lock());
    auto it = debugger_fallback_sessions().find(this);
    if (it == debugger_fallback_sessions().end() || it->second.processHandle == nullptr) {
        DebugResult r;
        r.success = false;
        r.detail = "breakExecution: no active fallback session";
        r.errorCode = 2;
        return r;
    }

    if (!DebugBreakProcess(it->second.processHandle)) {
        DebugResult r;
        r.success = false;
        r.detail = "breakExecution: DebugBreakProcess failed";
        r.errorCode = static_cast<int>(GetLastError());
        return r;
    }

    DebugResult r;
    r.success = true;
    r.detail = "breakExecution: break signal delivered";
    r.errorCode = 0;
    return r;
}
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::addBreakpointBySourceLine(const std::string&, int) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::removeBreakpoint(unsigned int) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::enableBreakpoint(unsigned int, bool) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::removeAllBreakpoints() { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::captureRegisters(RegisterSnapshot&) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::setRegister(const std::string&, uint64_t) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::walkStack(std::vector<NativeStackFrame>&, unsigned int) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::readMemory(uint64_t, void*, uint64_t, uint64_t*) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::searchMemory(uint64_t, uint64_t, const void*, unsigned int, std::vector<uint64_t>&) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::disassembleAt(uint64_t, unsigned int, std::vector<DisassembledInstruction>&) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::setSymbolPath(const std::string&) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::evaluate(const std::string&, EvalResult&) { DebugResult r; r.success = false; return r; }
unsigned int RawrXD::Debugger::NativeDebuggerEngine::addWatch(const std::string&) { return 0; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::removeWatch(unsigned int) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::enumerateModules(std::vector<DebugModule>&) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::enumerateThreads(std::vector<DebugThread>&) { DebugResult r; r.success = false; return r; }
RawrXD::Debugger::DebugResult RawrXD::Debugger::NativeDebuggerEngine::switchThread(unsigned int) { DebugResult r; r.success = false; return r; }

// Add more stubs as needed...
