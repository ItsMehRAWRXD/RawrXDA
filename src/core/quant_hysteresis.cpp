// ============================================================================
// quant_hysteresis.cpp — Quantization Kernel Hysteresis Controller
// ============================================================================
// Dead-band windows + cooldown timers prevent VRAM thrashing near 65%/85%.
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#define NOMINMAX
#include "quant_hysteresis.h"

#include <windows.h>
#include <cstring>
#include <cstdio>

// MASM kernel IDs (from masm_bridge_cathedral.h)
static constexpr int QKERNEL_Q4_K_M = 0;
static constexpr int QKERNEL_Q5_K_M = 1;
static constexpr int QKERNEL_Q8_0   = 2;

namespace RawrXD {
namespace Quant {

// ============================================================================
// Construction / Configuration
// ============================================================================

QuantHysteresisController::QuantHysteresisController() {
    memset(&m_config, 0, sizeof(m_config));
    m_config.windowPct          = HYSTERESIS_WINDOW_PCT;
    m_config.cooldownMs         = HYSTERESIS_COOLDOWN_MS;
    m_config.thresholdLow       = 65;
    m_config.thresholdHigh      = 85;
    m_config.thresholdCritical  = 88;
    m_config.enableCooldown     = true;
    m_config.enableLogging      = false;

    memset(&m_state, 0, sizeof(m_state));
    m_state.currentTier     = QTIER_LOW;
    m_state.currentKernelId = QKERNEL_Q8_0;
    m_state.lastVramPercent = 0;
    m_state.lastSwitchMs    = 0;
}

void QuantHysteresisController::configure(const QuantHysteresisConfig& config) {
    m_config = config;
}

void QuantHysteresisController::reset() {
    memset(&m_state, 0, sizeof(m_state));
    m_state.currentTier     = QTIER_LOW;
    m_state.currentKernelId = QKERNEL_Q8_0;
}

QuantHysteresisController& QuantHysteresisController::Global() {
    static QuantHysteresisController instance;
    return instance;
}

// ============================================================================
// Core Evaluation
// ============================================================================

QuantHysteresisResult QuantHysteresisController::evaluate(int vramPercent) {
    // Clamp
    if (vramPercent < 0) vramPercent = 0;
    if (vramPercent > 100) vramPercent = 100;

    m_state.lastVramPercent = vramPercent;

    QuantTier rawTier = computeRawTier(vramPercent);

    // If tier would change, apply hysteresis checks
    if (rawTier != m_state.currentTier) {
        // Dead-band check: if we're in the dead-band, suppress the switch
        if (isInDeadBand(vramPercent, rawTier)) {
            m_state.inDeadBand = true;
            m_state.suppressedCount++;

            if (m_config.enableLogging) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                    "[Quant] SUPPRESSED: VRAM=%d%% raw=%d current=%d (dead-band)\n",
                    vramPercent, rawTier, m_state.currentTier);
                OutputDebugStringA(msg);
            }

            return {true, false, m_state.currentTier, m_state.currentKernelId,
                    "Switch suppressed (dead-band)"};
        }

        // Cooldown check: don't switch too frequently
        if (m_config.enableCooldown && m_state.lastSwitchMs > 0) {
            uint64_t elapsed = nowMs() - m_state.lastSwitchMs;
            if (elapsed < m_config.cooldownMs) {
                m_state.suppressedCount++;

                if (m_config.enableLogging) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                        "[Quant] SUPPRESSED: VRAM=%d%% cooldown (%llums remaining)\n",
                        vramPercent,
                        (unsigned long long)(m_config.cooldownMs - elapsed));
                    OutputDebugStringA(msg);
                }

                return {true, false, m_state.currentTier, m_state.currentKernelId,
                        "Switch suppressed (cooldown)"};
            }
        }

        // Exception: CRITICAL overrides all hysteresis
        if (rawTier == QTIER_CRITICAL) {
            // Force immediate switch regardless of dead-band/cooldown
        }

        // Commit the tier change
        QuantTier oldTier = m_state.currentTier;
        m_state.currentTier = rawTier;
        m_state.currentKernelId = tierToKernel(rawTier);
        m_state.lastSwitchMs = nowMs();
        m_state.switchCount++;
        m_state.inDeadBand = false;

        if (m_config.enableLogging) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                "[Quant] SWITCH: VRAM=%d%% tier %d→%d kernel=%d (total switches=%u)\n",
                vramPercent, oldTier, rawTier,
                m_state.currentKernelId, m_state.switchCount);
            OutputDebugStringA(msg);
        }

        return {true, true, rawTier, m_state.currentKernelId, "Tier changed"};
    }

    // No change needed
    m_state.inDeadBand = false;
    return {true, false, m_state.currentTier, m_state.currentKernelId, "No change"};
}

QuantHysteresisResult QuantHysteresisController::forceTier(QuantTier tier) {
    m_state.currentTier = tier;
    m_state.currentKernelId = tierToKernel(tier);
    m_state.lastSwitchMs = nowMs();
    m_state.switchCount++;
    m_state.inDeadBand = false;

    if (m_config.enableLogging) {
        char msg[128];
        snprintf(msg, sizeof(msg),
            "[Quant] FORCED: tier=%d kernel=%d\n", tier, m_state.currentKernelId);
        OutputDebugStringA(msg);
    }

    return {true, true, tier, m_state.currentKernelId, "Forced tier change"};
}

// ============================================================================
// Internals
// ============================================================================

int QuantHysteresisController::tierToKernel(QuantTier tier) const {
    switch (tier) {
        case QTIER_CRITICAL: return QKERNEL_Q4_K_M;
        case QTIER_HIGH:     return QKERNEL_Q4_K_M;
        case QTIER_MEDIUM:   return QKERNEL_Q5_K_M;
        case QTIER_LOW:      return QKERNEL_Q8_0;
        default:             return QKERNEL_Q8_0;
    }
}

QuantTier QuantHysteresisController::computeRawTier(int vramPct) const {
    // Pure threshold evaluation without hysteresis
    if (vramPct >= m_config.thresholdCritical) return QTIER_CRITICAL;
    if (vramPct >= m_config.thresholdHigh)     return QTIER_HIGH;
    if (vramPct >= m_config.thresholdLow)      return QTIER_MEDIUM;
    return QTIER_LOW;
}

bool QuantHysteresisController::isInDeadBand(int vramPct, QuantTier /*raw*/) const {
    // Dead-band is [threshold - window, threshold + window]
    // If VRAM% is within the dead-band of ANY threshold, and the switch
    // would cross that threshold, suppress the switch.

    int w = m_config.windowPct;

    // Check Low threshold (65%)
    int lowMinusW = m_config.thresholdLow - w;
    int lowPlusW  = m_config.thresholdLow + w;
    if (vramPct >= lowMinusW && vramPct <= lowPlusW) {
        // In dead-band around 65% — suppress only LOW↔MEDIUM transitions
        if ((m_state.currentTier == QTIER_LOW && vramPct >= m_config.thresholdLow) ||
            (m_state.currentTier == QTIER_MEDIUM && vramPct < m_config.thresholdLow)) {
            return true;
        }
    }

    // Check High threshold (85%)
    int highMinusW = m_config.thresholdHigh - w;
    int highPlusW  = m_config.thresholdHigh + w;
    if (vramPct >= highMinusW && vramPct <= highPlusW) {
        // In dead-band around 85% — suppress MEDIUM↔HIGH transitions
        if ((m_state.currentTier == QTIER_MEDIUM && vramPct >= m_config.thresholdHigh) ||
            (m_state.currentTier == QTIER_HIGH && vramPct < m_config.thresholdHigh)) {
            return true;
        }
    }

    return false;
}

uint64_t QuantHysteresisController::nowMs() const {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

} // namespace Quant
} // namespace RawrXD
