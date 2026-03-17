#pragma once
// =============================================================================
// TPSBridge.h — Three-Engine TPS Bridge & Compositor
//
// Coordinates Sloloris (saturation streaming), Bounce (hot/cold ping-pong with
// rotation), and HotPatch (runtime tuning) into a unified pipeline targeting
// 300%+ composite strength.
//
// Usage:
//   auto slo = Sloloris_Init(...);
//   auto bnc = Bounce_Init(0);
//   auto hp  = HotPatch_Init(0);
//   auto br  = Bridge_Init(slo, bnc, hp);
//   while (running) {
//       int strength = Bridge_Tick(br);        // Ticks ALL 3 engines
//       // ... generate tokens ...
//       Bridge_NotifyToken(br);
//       if (Bridge_GetStatus(br) >= BRIDGE_SUPERCHARGED) { /* 300%+ */ }
//   }
//   Bridge_Destroy(br);   // Does NOT destroy sub-engines
//   HotPatch_Destroy(hp);
//   Bounce_Destroy(bnc);
//   Sloloris_Destroy(slo);
// =============================================================================

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Status Codes ───────────────────────────────────────────────────────────
constexpr int BRIDGE_COLD         = 0;   // < 100%
constexpr int BRIDGE_WARM         = 1;   // 100–199%
constexpr int BRIDGE_HOT          = 2;   // 200–299%
constexpr int BRIDGE_SUPERCHARGED = 3;   // 300–399%
constexpr int BRIDGE_LEGENDARY    = 4;   // 400%+

// ─── Opaque Context ─────────────────────────────────────────────────────────
typedef void* BridgeContext;

// =============================================================================
// API Functions (implemented in RawrXD_TPSBridge.asm)
// =============================================================================

/// Initialize the bridge with all three engine contexts.
/// @param slolorisCtx  Sloloris engine context (or NULL to skip)
/// @param bounceCtx    Bounce engine context (or NULL to skip)
/// @param hotPatchCtx  HotPatch engine context (or NULL to skip)
/// @return             Bridge context, or NULL
BridgeContext Bridge_Init(void* slolorisCtx, void* bounceCtx, void* hotPatchCtx);

/// Destroy bridge context. Does NOT destroy sub-engine contexts.
void Bridge_Destroy(BridgeContext ctx);

/// Tick all three engines in pipeline order and compute composite strength.
/// @return Composite strength (%, can exceed 300)
int Bridge_Tick(BridgeContext ctx);

/// Feed token generation event to all engines.
/// @return Total bridge-level tokens
uint64_t Bridge_NotifyToken(BridgeContext ctx);

/// Get composite strength (%).
int Bridge_GetStrength(BridgeContext ctx);

/// Get bridge-level measured TPS (×100).
int Bridge_GetTPS(BridgeContext ctx);

/// Get status code (BRIDGE_COLD through BRIDGE_LEGENDARY).
int Bridge_GetStatus(BridgeContext ctx);

// =============================================================================
// Statistics
// =============================================================================

struct BridgeStats {
    uint64_t tickCount;         // 0x00
    uint64_t tokenCount;        // 0x08
    uint32_t compositeStr;      // 0x10   Combined strength (%)
    uint32_t sloStr;            // 0x14   Sloloris strength
    uint32_t bncSuperStr;       // 0x18   Bounce superstrength
    uint32_t hpMultiplier;      // 0x1C   HotPatch multiplier
    uint32_t statusCode;        // 0x20   Status enum
    uint32_t peakStrength;      // 0x24   All-time peak
    uint32_t measuredTPS;       // 0x28   Bridge TPS (×100)
    uint32_t peakTPS;           // 0x2C   Peak TPS (×100)
};

/// Read bridge statistics.
int Bridge_GetStats(BridgeContext ctx, BridgeStats* stats);

#ifdef __cplusplus
} // extern "C"
#endif
