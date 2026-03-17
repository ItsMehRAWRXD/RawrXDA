#pragma once
// =============================================================================
// SlolorisStreamLoader.h — Slowloris/DoS-Pattern Tensor Cycling Engine
//
// Progressive model saturation through circular tensor load/unload cycling.
//
// Architecture:
//   DRIP   — Slowloris keepalive: trickle-load tensors 64KB at a time
//   BURST  — Load-3 / Unload-1 circular eviction for rapid warmup
//   ORBIT  — Full ring rotation: continuous load→infer→evict cycling
//   GHOST  — Spoof descriptors occupy slots, real data swapped in on demand
//
// Usage:
//   auto ctx = Sloloris_Init(0);                    // Default 4096 tensors
//   Sloloris_AttachModel(ctx, hFile, fileSize, nTensors, offsets, sizes);
//   while (inferring) {
//       Sloloris_Tick(ctx);                          // Advance one cycle
//       auto ptr = Sloloris_GetTensor(ctx, tid);     // On-demand fetch
//       int pct = Sloloris_GetStrength(ctx);         // 0-100% saturation
//   }
//   Sloloris_ForceOrbit(ctx, 3);                    // DoS burst: 3 full orbits
//   Sloloris_Destroy(ctx);
// =============================================================================

#include <cstdint>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Strategy Constants ─────────────────────────────────────────────────────
constexpr int SLOLORIS_STRATEGY_DRIP  = 0;  // Slow keepalive trickle-load
constexpr int SLOLORIS_STRATEGY_BURST = 1;  // Load-3, Evict-1 cycling
constexpr int SLOLORIS_STRATEGY_ORBIT = 2;  // Full ring rotation

// ─── Slot State Constants ───────────────────────────────────────────────────
constexpr int SLOLORIS_SLOT_EMPTY    = 0;
constexpr int SLOLORIS_SLOT_GHOST    = 1;   // Descriptor present, no data (spoof)
constexpr int SLOLORIS_SLOT_DRIP     = 2;   // Partial load in progress
constexpr int SLOLORIS_SLOT_LIVE     = 3;   // Fully loaded and usable
constexpr int SLOLORIS_SLOT_EVICTING = 4;   // Being evicted

// ─── Ring Configuration ─────────────────────────────────────────────────────
constexpr int SLOLORIS_RING_SLOTS    = 32;
constexpr int SLOLORIS_MAX_TENSORS   = 4096;

// ─── Opaque Context ─────────────────────────────────────────────────────────
// The engine context is allocated by Sloloris_Init and freed by Sloloris_Destroy.
// Do not access internal fields directly — use the API functions below.
typedef void* SlolorisContext;

// =============================================================================
// API Functions (implemented in RawrXD_SlolorisStreamLoader.asm)
// =============================================================================

/// Initialize the Sloloris streaming engine.
/// @param maxTensors  Maximum tensor count (0 = default 4096)
/// @return            Engine context pointer, or NULL on failure
SlolorisContext Sloloris_Init(uint32_t maxTensors);

/// Release all resources and destroy the engine.
/// @param ctx  Engine context (may be NULL for no-op)
void Sloloris_Destroy(SlolorisContext ctx);

/// Attach a GGUF model file to the engine.
/// @param ctx            Engine context
/// @param hFile          File handle (from CreateFileA/W)
/// @param fileSize       Total file size in bytes
/// @param tensorCount    Number of tensors in the model
/// @param tensorOffsets  Array of file offsets per tensor (tensorCount entries)
/// @param tensorSizes    Array of byte sizes per tensor (tensorCount entries)
/// @return               1 on success, 0 on failure
int Sloloris_AttachModel(SlolorisContext ctx, HANDLE hFile, uint64_t fileSize,
                         uint32_t tensorCount, const uint64_t* tensorOffsets,
                         const uint64_t* tensorSizes);

/// Advance one streaming cycle (call from inference loop).
///
/// Behavior depends on current strategy:
///   DRIP:  Load 64KB of the current tensor (Slowloris keepalive)
///   BURST: Load-3 / Evict-1 circular pattern
///   ORBIT: Full ring rotation with aggressive cycling
///
/// @param ctx  Engine context
/// @return     Number of tensors that changed state this tick
int Sloloris_Tick(SlolorisContext ctx);

/// Fetch a tensor's data pointer by ID.
///
/// If the tensor is in GHOST or DRIP state, it will be immediately
/// promoted to LIVE via on-demand MapViewOfFile (hot upgrade).
///
/// @param ctx       Engine context
/// @param tensorID  Tensor index (0-based)
/// @return          Pointer to tensor data, or NULL if unavailable
const void* Sloloris_GetTensor(SlolorisContext ctx, uint32_t tensorID);

/// Get current model strength as a saturation percentage.
///
/// Calculated as: (totalBytesLoaded * 100) / totalModelSize
/// Strength grows as tensors cycle through the ring and more
/// unique data passes through the pipeline.
///
/// @param ctx  Engine context
/// @return     0-100 percentage
int Sloloris_GetStrength(SlolorisContext ctx);

/// Force aggressive ring orbits (DoS burst pattern).
///
/// Rapidly cycles load/unload across the entire ring for `orbitCount`
/// full rotations. This saturates the OS file cache and memory mapping
/// tables, warming up subsequent tensor accesses.
///
/// @param ctx         Engine context
/// @param orbitCount  Number of full ring rotations to perform
/// @return            Total tensor state changes performed
int Sloloris_ForceOrbit(SlolorisContext ctx, uint32_t orbitCount);

/// Switch the cycling strategy.
/// @param ctx       Engine context
/// @param strategy  SLOLORIS_STRATEGY_DRIP / BURST / ORBIT
/// @return          Previous strategy value
int Sloloris_SetStrategy(SlolorisContext ctx, int strategy);

// =============================================================================
// Helper Structures (for C++ integration)
// =============================================================================

/// Statistics snapshot (read from context internals)
struct SlolorisStats {
    uint64_t tickCount;
    uint64_t orbitCount;
    uint64_t totalLoaded;
    uint64_t totalModelSize;
    uint32_t liveSlots;
    uint32_t ghostSlots;
    uint32_t strengthPct;
    int      currentStrategy;
    uint64_t statsLoads;
    uint64_t statsEvicts;
    uint64_t statsGhosts;
    uint64_t statsOrbits;
};

/// Read stats from context. Fields map directly to context offsets.
/// NOTE: This reads raw memory — only call while engine is not ticking.
inline SlolorisStats Sloloris_ReadStats(SlolorisContext ctx) {
    SlolorisStats s{};
    if (!ctx) return s;
    auto p = static_cast<const uint8_t*>(ctx);
    s.tickCount      = *reinterpret_cast<const uint64_t*>(p + 0x040);
    s.orbitCount     = *reinterpret_cast<const uint64_t*>(p + 0x048);
    s.totalLoaded    = *reinterpret_cast<const uint64_t*>(p + 0x030);
    s.totalModelSize = *reinterpret_cast<const uint64_t*>(p + 0x038);
    s.liveSlots      = *reinterpret_cast<const uint32_t*>(p + 0x028);
    s.ghostSlots     = *reinterpret_cast<const uint32_t*>(p + 0x02C);
    s.strengthPct    = *reinterpret_cast<const uint32_t*>(p + 0x068);
    s.currentStrategy= *reinterpret_cast<const int*>(p + 0x01C);
    s.statsLoads     = *reinterpret_cast<const uint64_t*>(p + 0x088);
    s.statsEvicts    = *reinterpret_cast<const uint64_t*>(p + 0x090);
    s.statsGhosts    = *reinterpret_cast<const uint64_t*>(p + 0x098);
    s.statsOrbits    = *reinterpret_cast<const uint64_t*>(p + 0x0A0);
    return s;
}

#ifdef __cplusplus
} // extern "C"
#endif
