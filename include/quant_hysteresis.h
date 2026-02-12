// ============================================================================
// quant_hysteresis.h — Quantization Kernel Hysteresis Controller
// ============================================================================
// Prevents VRAM pressure thrashing near tier boundaries (65%/85%) by
// introducing dead-band windows with cooldown timers.
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// VRAM Pressure Thresholds (matching MASM constants)
// ============================================================================
//
//  Tier       VRAM%   Kernel          Action
//  ─────────  ──────  ──────────────  ──────────────────────
//  Critical   ≥ 88%   Q4_K_M (0)     Force lowest quant
//  High       ≥ 85%   Q4_K_M (0)     Switch down
//  Medium     ≥ 65%   Q5_K_M (1)     Cap at mid-tier
//  Low        < 65%   Q8_0   (2)     Allow full precision
//
//  Hysteresis adds ±HYSTERESIS_WINDOW_PCT dead-band at each boundary.
//  Cooldown prevents switching more than once per cooldownMs.
//

#define HYSTERESIS_WINDOW_PCT    3     // ±3% dead-band
#define HYSTERESIS_COOLDOWN_MS   2000  // 2-second cooldown between switches

// ============================================================================
// Structs
// ============================================================================

enum QuantTier : int {
    QTIER_LOW      = 0,   // < 65% → Q8_0
    QTIER_MEDIUM   = 1,   // 65-85% → Q5_K_M
    QTIER_HIGH     = 2,   // ≥ 85% → Q4_K_M
    QTIER_CRITICAL = 3,   // ≥ 88% → Q4_K_M + alert
};

struct QuantHysteresisConfig {
    int      windowPct;           // Dead-band width (default 3%)
    uint32_t cooldownMs;          // Min ms between switches (default 2000)
    int      thresholdLow;        // Low boundary (default 65)
    int      thresholdHigh;       // High boundary (default 85)
    int      thresholdCritical;   // Critical boundary (default 88)
    bool     enableCooldown;      // Whether to enforce cooldown
    bool     enableLogging;       // Debug output
};

struct QuantHysteresisState {
    QuantTier currentTier;
    int       currentKernelId;    // QKERNEL_Q4_K_M=0, Q5_K_M=1, Q8_0=2
    int       lastVramPercent;    // Last observed VRAM %
    uint64_t  lastSwitchMs;       // Timestamp of last tier change
    uint32_t  switchCount;        // Total tier changes
    uint32_t  suppressedCount;    // Switches suppressed by hysteresis
    bool      inDeadBand;         // Currently in a dead-band region
};

struct QuantHysteresisResult {
    bool     success;
    bool     tierChanged;        // Did the tier actually change?
    QuantTier newTier;
    int       newKernelId;
    const char* detail;
};

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ API
// ============================================================================

#ifdef __cplusplus

namespace RawrXD {
namespace Quant {

class QuantHysteresisController {
public:
    QuantHysteresisController();
    ~QuantHysteresisController() = default;

    // Initialize with config (or defaults)
    void configure(const QuantHysteresisConfig& config);

    // Evaluate VRAM pressure and determine if a quant switch is needed.
    // Returns result indicating whether tier changed and what kernel to use.
    QuantHysteresisResult evaluate(int vramPercent);

    // Force a tier (bypass hysteresis)
    QuantHysteresisResult forceTier(QuantTier tier);

    // Reset state
    void reset();

    // Accessors
    const QuantHysteresisState& state() const { return m_state; }
    QuantTier currentTier() const { return m_state.currentTier; }
    int currentKernelId() const { return m_state.currentKernelId; }

    // Singleton for global VRAM management
    static QuantHysteresisController& Global();

private:
    QuantHysteresisConfig m_config;
    QuantHysteresisState  m_state;

    int tierToKernel(QuantTier tier) const;
    QuantTier computeRawTier(int vramPct) const;
    bool isInDeadBand(int vramPct, QuantTier raw) const;
    uint64_t nowMs() const;
};

} // namespace Quant
} // namespace RawrXD

#endif // __cplusplus
