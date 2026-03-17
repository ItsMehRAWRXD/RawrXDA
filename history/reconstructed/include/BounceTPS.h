#pragma once
// =============================================================================
// BounceTPS.h — Bounce Tensor Ping-Pong TPS Acceleration Engine
//
// Tensors bounce between HOT (memory-mapped, zero-copy) and COLD (descriptor
// only) pools. Access frequency drives promotion/demotion. A feedback loop
// measures tokens-per-second and auto-tunes the bounce rate to maximize TPS.
//
// ALL parameters are dynamic — configurable at runtime via API calls.
//
// Usage:
//   auto ctx = Bounce_Init(0);
//   Bounce_SetTargetTPS(ctx, 85);            // Dynamic: aim for 85 tok/sec
//   Bounce_SetPoolRatio(ctx, 70, 30);        // Dynamic: 70% hot, 30% cold
//   Bounce_AttachModel(ctx, hFile, fileSize, nTensors, offsets, sizes);
//   while (inferring) {
//       Bounce_Tick(ctx);                     // Rebalance + measure TPS
//       auto ptr = Bounce_GetTensor(ctx, tid);// HOT=instant, COLD=promote
//       // ... inference step ...
//       Bounce_NotifyTokenGen(ctx);           // Feed TPS measurement
//   }
//   float tps = Bounce_GetTPS(ctx) / 100.0f; // e.g., 8520 → 85.20 tok/sec
//   Bounce_Destroy(ctx);
// =============================================================================

#include <cstdint>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Pool Entry States ──────────────────────────────────────────────────────
constexpr int BOUNCE_PE_EMPTY    = 0;
constexpr int BOUNCE_PE_HOT      = 1;   // Memory-mapped, zero-copy access
constexpr int BOUNCE_PE_COLD     = 2;   // Descriptor only, no data mapped
constexpr int BOUNCE_PE_BOUNCING = 3;   // In transit between pools
constexpr int BOUNCE_PE_PREFETCH = 4;   // Being prefetched (async promotion)

// ─── Defaults (all overridable at runtime) ──────────────────────────────────
constexpr int BOUNCE_DEFAULT_MAX_HOT        = 64;
constexpr int BOUNCE_DEFAULT_MAX_COLD       = 128;
constexpr int BOUNCE_DEFAULT_TARGET_TPS     = 70;
constexpr int BOUNCE_DEFAULT_BOUNCE_RATE_MS = 0;    // 0 = auto-tune
constexpr int BOUNCE_DEFAULT_HEAT_DECAY     = 3;
constexpr int BOUNCE_DEFAULT_PREFETCH_DEPTH = 4;
constexpr int BOUNCE_MAX_POOL_SLOTS         = 256;
constexpr int BOUNCE_MAX_TENSORS            = 8192;

// ─── Opaque Context ─────────────────────────────────────────────────────────
typedef void* BounceContext;

// =============================================================================
// API Functions (implemented in RawrXD_BounceTPS.asm)
// =============================================================================

/// Initialize the Bounce TPS engine. All parameters start at defaults.
/// @param flags  Reserved (pass 0)
/// @return       Engine context pointer, or NULL on failure
BounceContext Bounce_Init(uint32_t flags);

/// Release all resources and destroy the engine.
/// @param ctx  Engine context (NULL-safe)
void Bounce_Destroy(BounceContext ctx);

/// Attach a GGUF model file to the engine.
/// @param ctx            Engine context
/// @param hFile          File handle (from CreateFileA/W)
/// @param fileSize       Total file size in bytes
/// @param tensorCount    Number of tensors in the model
/// @param tensorOffsets  Array of file offsets per tensor
/// @param tensorSizes    Array of byte sizes per tensor
/// @return               1 on success, 0 on failure
int Bounce_AttachModel(BounceContext ctx, HANDLE hFile, uint64_t fileSize,
                       uint32_t tensorCount, const uint64_t* tensorOffsets,
                       const uint64_t* tensorSizes);

/// Advance one cycle: decay heat, measure TPS, auto-tune, rebalance pools.
/// @param ctx  Engine context
/// @return     Number of pool changes this tick
int Bounce_Tick(BounceContext ctx);

/// Fetch tensor data pointer by ID.
/// - HOT pool: zero-copy instant return
/// - COLD pool: demand-promote to HOT, then return
/// - Not in any pool: direct-load from file
/// @param ctx       Engine context
/// @param tensorID  Tensor index (0-based)
/// @return          Pointer to tensor data, or NULL if unavailable
const void* Bounce_GetTensor(BounceContext ctx, uint32_t tensorID);

/// Get current measured TPS (×100 for 2 decimal places).
/// Example: return 7050 means 70.50 tokens/sec
/// @param ctx  Engine context
/// @return     TPS × 100
int Bounce_GetTPS(BounceContext ctx);

/// Set target TPS threshold (dynamic, changeable at any time).
/// @param ctx       Engine context
/// @param targetTPS Target tokens/sec (e.g., 70)
/// @return          Previous target
int Bounce_SetTargetTPS(BounceContext ctx, int targetTPS);

/// Set HOT:COLD pool ratio (dynamic, adjusts pool capacities immediately).
/// @param ctx      Engine context
/// @param hotPct   Hot pool percentage (0-100)
/// @param coldPct  Cold pool percentage (0-100)
/// @return         1 on success
int Bounce_SetPoolRatio(BounceContext ctx, int hotPct, int coldPct);

/// Set bounce frequency in milliseconds (dynamic).
/// Pass 0 for auto-tune (engine adjusts based on TPS feedback).
/// @param ctx         Engine context
/// @param bounceMs    Bounce rate in ms (0 = auto)
/// @return            Previous rate
int Bounce_SetBounceRate(BounceContext ctx, int bounceMs);

/// Set heat decay rate per tick (dynamic).
/// Higher decay = tensors cool faster = more bouncing.
/// @param ctx     Engine context
/// @param decay   Heat units to subtract per tick
/// @return        Previous decay rate
int Bounce_SetHeatDecay(BounceContext ctx, int decay);

/// Set prefetch depth (dynamic).
/// How many tensors to pre-promote from COLD to HOT on attach.
/// @param ctx     Engine context
/// @param depth   Number of tensors to prefetch
/// @return        Previous depth
int Bounce_SetPrefetchDepth(BounceContext ctx, int depth);

/// Notify the engine that a token was generated.
/// Call from inference loop to feed TPS measurement.
/// @param ctx  Engine context
/// @return     Total tokens generated
uint64_t Bounce_NotifyTokenGen(BounceContext ctx);

// =============================================================================
// Statistics
// =============================================================================

/// Stats buffer filled by Bounce_GetStats (128 bytes)
struct BounceStats {
    uint64_t tickCount;         // 0x00
    uint64_t totalBounces;      // 0x08
    uint64_t totalPromotions;   // 0x10
    uint64_t totalDemotions;    // 0x18
    uint64_t tokensGenerated;   // 0x20
    uint32_t currentTPS;        // 0x28  (×100)
    uint32_t peakTPS;           // 0x2C  (×100)
    uint32_t hotCount;          // 0x30
    uint32_t coldCount;         // 0x34
    uint64_t hotHits;           // 0x38
    uint64_t coldHits;          // 0x40
    uint64_t misses;            // 0x48
    uint64_t prefetches;        // 0x50
    uint64_t bytesLoaded;       // 0x58
    uint64_t bytesEvicted;      // 0x60
    uint32_t targetTPS;         // 0x68
    uint32_t bounceRateMs;      // 0x6C
    uint32_t autoBounceMs;      // 0x70
    uint32_t heatDecayRate;     // 0x74
    uint32_t prefetchDepth;     // 0x78
    uint32_t _pad;              // 0x7C
};

/// Extended stats including SuperStrength (available after rotation/randomization)
struct BounceExtStats : public BounceStats {
    uint32_t rotateDir;         // Current rotation direction (0-3)
    uint32_t rotatePhase;       // Current rotation phase counter
    uint32_t superStrength;     // SuperStrength % (can exceed 100 — target 300%+)
    uint32_t boostMultiplier;   // Active boost multiplier (×100)
};

/// Read engine statistics into a BounceStats buffer.
/// @param ctx     Engine context
/// @param stats   Pointer to BounceStats (128 bytes)
/// @return        1 on success, 0 on failure
int Bounce_GetStats(BounceContext ctx, BounceStats* stats);

/// Read SuperStrength from the Bounce context.
/// SuperStrength = base_pct + bounce_bonus + heat_bonus + tps_bonus
/// Can exceed 100% — e.g., 350 means 3.5x strength.
/// @param ctx  Engine context
/// @return     SuperStrength percentage
inline uint32_t Bounce_GetSuperStrength(BounceContext ctx) {
    // BC_SuperStrength is at offset 0x200 in the Bounce context
    if (!ctx) return 0;
    return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctx) + 0x200);
}

/// Read current rotation direction.
/// 0 = HOT→COLD, 1 = COLD→HOT, 2 = HOT→HOT (reshuffle), 3 = COLD→COLD (compact)
inline uint32_t Bounce_GetRotateDir(BounceContext ctx) {
    if (!ctx) return 0;
    return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctx) + 0x1F8);
}

#ifdef __cplusplus
} // extern "C"
#endif
