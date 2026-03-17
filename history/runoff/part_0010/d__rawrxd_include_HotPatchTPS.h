#pragma once
// =============================================================================
// HotPatchTPS.h — HotPatch TPS Multiplier Engine API
//
// Runtime TPS hotpatching engine that dynamically tunes execution parameters
// via explore/exploit strategy. Feeds from Sloloris and Bounce engines to
// compute a composite multiplier targeting 300%+.
//
// Usage:
//   auto hp = HotPatch_Init(0);
//   HotPatch_FeedSloloris(hp, slolorisStrength);
//   HotPatch_FeedBounce(hp, bounceSuper);
//   int mult = HotPatch_Tick(hp);  // Returns composite multiplier x100
//   // ... generate token ...
//   HotPatch_NotifyToken(hp);
//   HotPatch_Destroy(hp);
// =============================================================================

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Patch Slot IDs ─────────────────────────────────────────────────────────
constexpr int HP_PATCH_BATCH_SIZE      = 0;
constexpr int HP_PATCH_PREFETCH_DEPTH  = 1;
constexpr int HP_PATCH_QUANT_MODE      = 2;
constexpr int HP_PATCH_CACHE_MODE      = 3;
constexpr int HP_PATCH_THREAD_COUNT    = 4;
constexpr int HP_PATCH_SPEC_DECODE     = 5;
constexpr int HP_PATCH_RING_AGGRESSION = 6;
constexpr int HP_PATCH_HEAT_INJECT     = 7;
constexpr int HP_MAX_PATCH_SLOTS       = 16;

// ─── Opaque Context ─────────────────────────────────────────────────────────
typedef void* HotPatchContext;

// =============================================================================
// API Functions (implemented in RawrXD_HotPatchTPS.asm)
// =============================================================================

/// Initialize the HotPatch engine.
/// @param flags  Reserved (pass 0)
/// @return       Engine context, or NULL
HotPatchContext HotPatch_Init(uint32_t flags);

/// Destroy the engine and free resources.
void HotPatch_Destroy(HotPatchContext ctx);

/// Advance one optimization cycle: measure → tune → compute composite.
/// @return Composite multiplier (×100, e.g., 350 = 3.50×)
int HotPatch_Tick(HotPatchContext ctx);

/// Signal that a token was generated.
/// @return Total tokens generated
uint64_t HotPatch_NotifyToken(HotPatchContext ctx);

/// Get current composite multiplier (×100).
int HotPatch_GetMultiplier(HotPatchContext ctx);

/// Get measured TPS (×100).
int HotPatch_GetTPS(HotPatchContext ctx);

/// Set a dynamic patch parameter (clamped to min/max).
/// @param slotID   Patch slot (0–15)
/// @param value    New value
/// @return         Previous value
int HotPatch_SetParam(HotPatchContext ctx, int slotID, int value);

/// Get a patch parameter's current value.
int HotPatch_GetParam(HotPatchContext ctx, int slotID);

/// Feed Sloloris engine strength (0–100) into the multiplier chain.
void HotPatch_FeedSloloris(HotPatchContext ctx, int strength);

/// Feed Bounce engine superstrength (0–999) into the multiplier chain.
void HotPatch_FeedBounce(HotPatchContext ctx, int superStrength);

// =============================================================================
// Statistics
// =============================================================================

struct HotPatchStats {
    uint64_t tickCount;         // 0x00
    uint64_t tokensGenerated;   // 0x08
    uint32_t currentTPS;        // 0x10  (×100)
    uint32_t peakTPS;           // 0x14  (×100)
    uint32_t compositeMult;     // 0x18  (×100)
    uint32_t bestComposite;     // 0x1C  (×100)
    uint32_t slolorisInput;     // 0x20
    uint32_t bounceInput;       // 0x24
    uint32_t patchBonus;        // 0x28  (0–100)
    uint32_t feedbackBonus;     // 0x2C  (0–50)
    uint64_t tuneSteps;         // 0x30
    uint64_t explores;          // 0x38
    uint64_t exploits;          // 0x40
    uint64_t improved;          // 0x48
    uint64_t regressed;         // 0x50
    uint32_t patchValues[8];    // 0x58  Current patch slot values
};

/// Read engine statistics.
/// @param ctx     Engine context
/// @param stats   Pointer to HotPatchStats (128 bytes)
/// @return        1 on success
int HotPatch_GetStats(HotPatchContext ctx, HotPatchStats* stats);

#ifdef __cplusplus
} // extern "C"
#endif
