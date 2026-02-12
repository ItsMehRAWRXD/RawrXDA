// ============================================================================
// masm_bridge_cathedral.h — Unified MASM64 Kernel Bridge Declarations
// ============================================================================
// Authoritative extern "C" prototypes for ALL 49 MASM kernel modules.
// Matches PUBLIC PROC exports exactly (name decoration stripped).
//
// Pattern: PatchResult-style, no exceptions, no STL in extern "C" boundary.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdint>
#include <cstddef>

// ============================================================================
// Forward Declarations — Opaque MASM-level context structs
// ============================================================================

// Aligned to 64-byte cache lines for MASM compatibility
#pragma pack(push, 8)

struct GGUF_LOADER_CTX;         // RawrXD_GGUF_Vulkan_Loader.asm
struct QUAD_BUFFER_CTX;         // RawrXD_Streaming_QuadBuffer.asm
struct SPENGINE_STATS;          // RawrXD_SelfPatch_Engine.asm
struct ORCHESTRATOR_METRICS;    // RawrXD_AgenticOrchestrator.asm
struct LSP_BRIDGE_STATS;        // RawrXD_LSP_AI_Bridge.asm

// ============================================================================
// Quant Kernel IDs (must match MASM constants)
// ============================================================================

enum QuantKernelId : uint32_t {
    QKERNEL_Q4_K_M = 0,
    QKERNEL_Q5_K_M = 1,
    QKERNEL_Q8_0   = 2,
    QKERNEL_COUNT  = 3
};

// ============================================================================
// VRAM Pressure Constants (must match MASM)
// ============================================================================

static constexpr uint32_t VRAM_PRESSURE_HIGH   = 85;
static constexpr uint32_t VRAM_PRESSURE_MEDIUM = 65;
static constexpr uint32_t VRAM_PRESSURE_LOW    = 0;

// ============================================================================
// Streaming QuadBuffer Flags (must match MASM)
// ============================================================================

enum StreamingFlags : uint32_t {
    STREAMING_FLAG_VSYNC    = 0x01,
    STREAMING_FLAG_TEARING  = 0x02,
    STREAMING_FLAG_HDR      = 0x04
};

// ============================================================================
// Token Attribute Flags (must match MASM)
// ============================================================================

enum TokenAttr : uint32_t {
    TOKEN_ATTR_NORMAL   = 0x00,
    TOKEN_ATTR_BOLD     = 0x01,
    TOKEN_ATTR_ITALIC   = 0x02,
    TOKEN_ATTR_CODE     = 0x04,
    TOKEN_ATTR_ERROR    = 0x08,
    TOKEN_ATTR_KEYWORD  = 0x10,
    TOKEN_ATTR_GHOST    = 0x20  // Ghost text overlay
};

// ============================================================================
// GPU Residency States (must match MASM)
// ============================================================================

enum GpuResidencyState : uint32_t {
    RESIDENCY_COLD   = 0,
    RESIDENCY_MAPPED = 1,
    RESIDENCY_STAGED = 2,
    RESIDENCY_GPU    = 3
};

// ============================================================================
// LSP Sync Modes (must match MASM)
// ============================================================================

enum LspSyncMode : uint32_t {
    LSP_SYNC_SHALLOW = 0,
    LSP_SYNC_NORMAL  = 1,
    LSP_SYNC_DEEP    = 2
};

// ============================================================================
// Orchestrator Opcodes (must match MASM VTable)
// ============================================================================

enum OrchestratorOpcode : uint32_t {
    OP_GENERATE_COMPLETION = 7001,
    OP_APPLY_EDIT          = 7002,
    OP_RUN_TERMINAL        = 7003,
    OP_SEARCH_WORKSPACE    = 7004,
    OP_READ_FILE           = 7005,
    OP_WRITE_FILE          = 7006,
    OP_LIST_DIR            = 7007,
    OP_EXPLAIN_CODE        = 7008,
    OP_REFACTOR            = 7009,
    OP_FIX_ERROR           = 7010,
    OP_HOTPATCH_MEMORY     = 8001,
    OP_HOTPATCH_BYTE       = 8002,
    OP_HOTPATCH_SERVER     = 8003,
    OP_MODEL_LOAD          = 8101,
    OP_MODEL_UNLOAD        = 8102,
    OP_LSP_SYNC            = 9800,
    OP_QUANT_SWITCH        = 9900
};

#pragma pack(pop)

// ============================================================================
// EXTERN "C" PROTOTYPES
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Module 1: RawrXD_SelfPatch_Engine.asm
// Runtime code morphing, hotpatch table, RWX code caves, CPU feature detect
// --------------------------------------------------------------------------

int     asm_spengine_init(void* patchTable, uint32_t count);
int     asm_spengine_register(uint32_t patchId, const char* name,
                              void* originalAddr, void* patchAddr, uint32_t size);
void*   asm_spengine_apply(uint32_t patchId, void* params);
int     asm_spengine_rollback(uint32_t patchId);
void    asm_spengine_cpu_optimize(void);
void*   asm_spengine_quant_switch(uint32_t kernelId, void* newKernelFn);
int     asm_spengine_quant_switch_adaptive(uint32_t kernelId,
                                           uint32_t vramPercent,
                                           void* kernelFn);
void    asm_spengine_get_stats(void* statsOut);
void    asm_spengine_shutdown(void);

// --------------------------------------------------------------------------
// Module 2: RawrXD_GGUF_Vulkan_Loader.asm
// GGUF parsing, GPU residency classification, DMA staging
// --------------------------------------------------------------------------

int     asm_gguf_loader_init(void* ctx, const wchar_t* path, uint32_t pathLen);
int     asm_gguf_loader_parse(void* ctx);
int     asm_gguf_loader_stage(void* ctx, uint32_t tensorIdx);
int     asm_gguf_loader_stage_all(void* ctx);
int     asm_gguf_loader_lookup(void* ctx, const char* name, uint32_t nameLen);
void    asm_gguf_loader_get_info(void* ctx, void* infoOut);
void    asm_gguf_loader_get_stats(void* ctx, void* statsOut);
void    asm_gguf_loader_close(void* ctx);
void    asm_gguf_loader_configure_gpu(void* ctx, uint64_t thresholdBytes);
void    asm_gguf_loader_get_residency(void* ctx, uint32_t* gpuCount,
                                      uint32_t* mappedCount,
                                      uint32_t* pendingCount);

// --------------------------------------------------------------------------
// Module 3: RawrXD_AgenticOrchestrator.asm
// VTable dispatch, SPSC task queue, pre/post hooks, NUMA-aware alloc
// --------------------------------------------------------------------------

int     asm_orchestrator_init(void* sysCtx, void* telemetryRing);
int     asm_orchestrator_dispatch(uint32_t opcode, void* cmdCtx, void* result);
void    asm_orchestrator_shutdown(void);
void    asm_orchestrator_get_metrics(void* metricsOut);
int     asm_orchestrator_register_hook(uint32_t opcode, void* preHook,
                                       void* postHook);
void    asm_orchestrator_set_vtable(uint32_t opcode, void* handler);
int     asm_orchestrator_queue_async(uint32_t opcode, void* cmdCtx,
                                     void* callback, void* userData);
int     asm_orchestrator_drain_queue(uint32_t maxItems);
void    asm_orchestrator_lsp_sync(void* symbolIndex, void* contextAnalyzer,
                                  uint32_t mode);

// --------------------------------------------------------------------------
// Module 4: RawrXD_Streaming_QuadBuffer.asm
// 128K-slot SPSC ring, GDI double-buffer, adaptive frame pacing
// --------------------------------------------------------------------------

int     asm_quadbuf_init(HWND hwnd, uint32_t width, uint32_t height,
                         uint32_t flags);
void    asm_quadbuf_push_token(const char* utf8Token, uint32_t len,
                               uint32_t attr, uint64_t timestamp);
int     asm_quadbuf_render_frame(void);
int     asm_quadbuf_render_thread(void* param);
void    asm_quadbuf_resize(uint32_t width, uint32_t height);
void    asm_quadbuf_get_stats(void* statsOut);
void    asm_quadbuf_shutdown(void);
void    asm_quadbuf_set_flags(uint32_t flags);
void    asm_quadbuf_set_frame_interval(uint32_t ms);

// --------------------------------------------------------------------------
// Module 5: RawrXD_LSP_AI_Bridge.asm
// Symbol hashing, importance scoring, SRWLock read, merge sort, context emit
// --------------------------------------------------------------------------

int     asm_lsp_bridge_init(void* symbolIndex, void* contextAnalyzer);
int     asm_lsp_bridge_sync(uint32_t mode);
int     asm_lsp_bridge_query(void* resultBuf, uint32_t maxSymbols,
                             uint32_t* outCount);
void    asm_lsp_bridge_invalidate(void);
void    asm_lsp_bridge_get_stats(void* statsOut);
void    asm_lsp_bridge_set_weights(float syntaxWeight, float semanticWeight);
void    asm_lsp_bridge_shutdown(void);

// --------------------------------------------------------------------------
// Module 6: FNV-1a Hash (shared, defined in GGUF_Vulkan_Loader)
// --------------------------------------------------------------------------

uint64_t fnv1a_hash64(const char* data, uint32_t len);

// --------------------------------------------------------------------------
// Pre-existing bridges (declarations in separate headers, listed for
// completeness — canonical prototypes in their own headers)
// --------------------------------------------------------------------------
// gpu_masm_bridge.h          → InitializeGPUBackend, GPU_Detect, etc.
// masm_lsp_bridge.h          → rxd_asm_normalize_completion, etc.
// CoTMASMBridge.hpp          → CoT DLL interface
// quant/nanoquant_bridge.h   → NanoQuant SIMD kernels
// net/net_masm_bridge.h      → Network MASM bridge
// ggml_masm/ggml_masm_bridge.h → GGML MASM bridge

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ Convenience Wrappers (inline, no link dependency)
// ============================================================================

#ifdef __cplusplus

namespace RawrXD {
namespace MASM {

/// Initialize all Tier-2 MASM subsystems in correct dependency order.
/// Returns 0 on success, negative on first failure.
inline int InitializeAllSubsystems(HWND editorHwnd,
                                    void* symbolIndex,
                                    void* contextAnalyzer,
                                    void* telemetryRing,
                                    uint32_t editorWidth = 1920,
                                    uint32_t editorHeight = 1080)
{
    // 1. Self-Patch Engine (optimizes subsequent code)
    int rc = asm_spengine_init(nullptr, 0);
    if (rc < 0) return rc;
    asm_spengine_cpu_optimize();
    asm_spengine_quant_switch_adaptive(QKERNEL_Q8_0, 0, nullptr); // Auto VRAM

    // 2. GGUF Vulkan Loader (model loading infrastructure)
    // Deferred — initialized per-model via asm_gguf_loader_init()

    // 3. Agentic Orchestrator (command routing)
    rc = asm_orchestrator_init(nullptr, telemetryRing);
    if (rc < 0) return rc;

    // 4. Streaming QuadBuffer (editor rendering)
    if (editorHwnd) {
        rc = asm_quadbuf_init(editorHwnd, editorWidth, editorHeight,
                              STREAMING_FLAG_TEARING);
        if (rc < 0) return rc;
        asm_quadbuf_set_frame_interval(16); // 60 FPS
    }

    // 5. LSP-AI Bridge (symbol context fusion)
    if (symbolIndex && contextAnalyzer) {
        rc = asm_lsp_bridge_init(symbolIndex, contextAnalyzer);
        if (rc < 0) return rc;
    }

    return 0;
}

/// Shutdown all Tier-2 subsystems in reverse order.
inline void ShutdownAllSubsystems() {
    asm_lsp_bridge_shutdown();
    asm_quadbuf_shutdown();
    asm_orchestrator_shutdown();
    asm_spengine_shutdown();
}

} // namespace MASM
} // namespace RawrXD

#endif // __cplusplus
