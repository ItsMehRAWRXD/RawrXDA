// ============================================================================
// pipeline_exit_guard.hpp — P-Settings Exit Invariants + PID Safety Rails
// ============================================================================
//
// Action Item #13: Enforce invariants at pipeline exit (C++ side):
//   - `final` must be non-empty
//   - max length enforced
//   - visibility policy enforced
//
// Action Item #14: PID Self-Tuning Safety Rails:
//   - min/max depth slew rate (no more than ±2 per step)
//   - cooldown windows (min 5s between adjustments)
//   - bounded integrator (anti-windup, |I| <= 4.0)
//
// Even with intentionally corrupted intermediate results, pipeline exit
// returns a safe final.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_PIPELINE_EXIT_GUARD_H
#define RAWRXD_PIPELINE_EXIT_GUARD_H

#include <string>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cmath>

// Forward
#include "cot_response_schema.hpp"
#include "reasoning_profile.h"

// ============================================================================
// Pipeline Exit Guard — enforces invariants on every pipeline output
// ============================================================================
class PipelineExitGuard {
public:
    struct ExitResult {
        bool        enforced;       // True if any enforcement was applied
        int         repairsApplied; // Count of repairs
        std::string violations;     // Semicolon-seperated list of violations
    };

    /// Enforce all exit invariants on a CoT response.
    /// Repairs in-place to guarantee a safe output regardless of upstream corruption.
    static ExitResult enforce(CotResponseSchema& resp,
                              const std::string& originalInput,
                              ReasoningVisibility visibility) {
        ExitResult result{false, 0, ""};

        // ---- Invariant 1: final must be non-empty ----
        if (resp.final_answer.empty() ||
            resp.final_answer.find_first_not_of(" \t\n\r") == std::string::npos) {
            result.enforced = true;
            result.violations += "empty_final;";

            // Attempt recovery from steps
            for (int i = static_cast<int>(resp.steps.size()) - 1; i >= 0; --i) {
                if (!resp.steps[i].content.empty() &&
                    resp.steps[i].content.find("[Error") != 0 &&
                    resp.steps[i].content.find_first_not_of(" \t\n\r") != std::string::npos) {
                    resp.final_answer = resp.steps[i].content;
                    result.repairsApplied++;
                    break;
                }
            }
            // Last resort: echo input
            if (resp.final_answer.empty() || 
                resp.final_answer.find_first_not_of(" \t\n\r") == std::string::npos) {
                if (!originalInput.empty()) {
                    resp.final_answer = "I received: '" +
                        originalInput.substr(0, 200) + "'. Could you elaborate?";
                } else {
                    resp.final_answer = "I'm here to help. Could you rephrase your question?";
                }
                result.repairsApplied++;
            }
        }

        // ---- Invariant 2: max length enforced ----
        if (resp.final_answer.size() > COT_MAX_FINAL_ANSWER_LENGTH) {
            resp.final_answer.resize(COT_MAX_FINAL_ANSWER_LENGTH);
            resp.meta.truncated = true;
            result.enforced = true;
            result.repairsApplied++;
            result.violations += "final_truncated;";
        }

        // ---- Invariant 3: visibility policy enforced ----
        switch (visibility) {
            case ReasoningVisibility::FinalOnly:
                // Strip all step content — only final is visible
                for (auto& step : resp.steps) {
                    step.content.clear();
                }
                break;

            case ReasoningVisibility::ProgressBar:
                // Strip step content, keep role + latency for progress display
                for (auto& step : resp.steps) {
                    if (step.content.size() > 50) {
                        step.content = step.content.substr(0, 47) + "...";
                    }
                }
                break;

            case ReasoningVisibility::StepSummary:
                // Truncate each step to a summary
                for (auto& step : resp.steps) {
                    if (step.content.size() > 500) {
                        step.content = step.content.substr(0, 497) + "...";
                        result.repairsApplied++;
                    }
                }
                break;

            case ReasoningVisibility::FullCoT:
                // No modification — full visibility (dev mode)
                break;

            default:
                break;
        }

        // ---- Invariant 4: step count cap ----
        if (resp.steps.size() > COT_MAX_STEPS) {
            resp.steps.resize(COT_MAX_STEPS);
            result.enforced = true;
            result.repairsApplied++;
            result.violations += "steps_capped;";
        }

        // ---- Invariant 5: schema version ----
        if (resp.schemaVersion != COT_SCHEMA_VERSION) {
            resp.schemaVersion = COT_SCHEMA_VERSION;
            result.repairsApplied++;
        }

        // ---- Invariant 6: meta completeness ----
        if (resp.meta.route.empty()) {
            resp.meta.route = "unknown";
            result.repairsApplied++;
        }
        if (resp.meta.preset.empty()) {
            resp.meta.preset = "normal";
            result.repairsApplied++;
        }

        return result;
    }
};

// ============================================================================
// PID Safety Rails — bounded self-tuning controller
// ============================================================================
class PIDSafetyRails {
public:
    // Configuration constants
    static constexpr int    MAX_SLEW_RATE = 2;              // Max ±depth change per step
    static constexpr double COOLDOWN_SECONDS = 5.0;         // Min time between adjustments
    static constexpr double INTEGRATOR_MAX = 4.0;           // Anti-windup bound |I|
    static constexpr double INTEGRATOR_MIN = -4.0;
    static constexpr int    MIN_DEPTH = 0;
    static constexpr int    MAX_DEPTH = 8;
    static constexpr double DERIVATIVE_FILTER_ALPHA = 0.3;  // Low-pass on D term

    struct PIDState {
        double  integrator      = 0.0;
        double  prevError       = 0.0;
        double  filteredD       = 0.0;
        int     lastDepth       = 4;
        int     adjustmentCount = 0;
        std::chrono::steady_clock::time_point lastAdjustTime;

        PIDState() : lastAdjustTime(std::chrono::steady_clock::now()) {}
    };

    struct AdjustResult {
        int     newDepth;
        int     requestedDepth;
        bool    slewLimited;
        bool    cooldownActive;
        bool    windupClamped;
        double  integratorValue;
    };

    /// Compute a safe depth adjustment given PID controller output.
    /// Applies slew rate limiting, cooldown, and anti-windup.
    static AdjustResult safeAdjust(PIDState& state, int requestedDepth,
                                    double error, double dt_seconds,
                                    double Kp, double Ki, double Kd) {
        AdjustResult result{};
        result.requestedDepth = requestedDepth;

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(
            now - state.lastAdjustTime).count();

        // ---- Cooldown check ----
        if (elapsed < COOLDOWN_SECONDS) {
            result.newDepth = state.lastDepth;
            result.cooldownActive = true;
            result.integratorValue = state.integrator;
            return result;
        }

        // ---- PID computation with safety rails ----
        // Proportional
        double P = Kp * error;

        // Integral with anti-windup (bounded)
        state.integrator += Ki * error * dt_seconds;
        if (state.integrator > INTEGRATOR_MAX) {
            state.integrator = INTEGRATOR_MAX;
            result.windupClamped = true;
        } else if (state.integrator < INTEGRATOR_MIN) {
            state.integrator = INTEGRATOR_MIN;
            result.windupClamped = true;
        }
        double I = state.integrator;

        // Derivative with low-pass filter
        double rawD = (dt_seconds > 0.001) ? (error - state.prevError) / dt_seconds : 0.0;
        state.filteredD = DERIVATIVE_FILTER_ALPHA * rawD +
                          (1.0 - DERIVATIVE_FILTER_ALPHA) * state.filteredD;
        double D = Kd * state.filteredD;

        state.prevError = error;

        // Combined PID output → depth offset
        double pidOutput = P + I + D;
        int targetDepth = state.lastDepth + static_cast<int>(std::round(pidOutput));

        // ---- Slew rate limiting ----
        int delta = targetDepth - state.lastDepth;
        if (delta > MAX_SLEW_RATE) {
            delta = MAX_SLEW_RATE;
            result.slewLimited = true;
        } else if (delta < -MAX_SLEW_RATE) {
            delta = -MAX_SLEW_RATE;
            result.slewLimited = true;
        }
        int newDepth = state.lastDepth + delta;

        // ---- Absolute bounds ----
        newDepth = (std::max)(MIN_DEPTH, (std::min)(newDepth, MAX_DEPTH));

        // ---- Also clamp the requested depth if it comes from outside the PID ----
        if (requestedDepth != state.lastDepth) {
            int reqDelta = requestedDepth - state.lastDepth;
            if (reqDelta > MAX_SLEW_RATE) {
                requestedDepth = state.lastDepth + MAX_SLEW_RATE;
                result.slewLimited = true;
            } else if (reqDelta < -MAX_SLEW_RATE) {
                requestedDepth = state.lastDepth - MAX_SLEW_RATE;
                result.slewLimited = true;
            }
            // Use the PID-computed depth, not the raw request
            // But if manual override is intended, prefer the clamped request
        }

        // Update state
        state.lastDepth = newDepth;
        state.lastAdjustTime = now;
        state.adjustmentCount++;

        result.newDepth = newDepth;
        result.integratorValue = state.integrator;
        return result;
    }

    /// Reset the PID state (e.g., on profile change)
    static void reset(PIDState& state, int currentDepth = 4) {
        state.integrator = 0.0;
        state.prevError = 0.0;
        state.filteredD = 0.0;
        state.lastDepth = currentDepth;
        state.adjustmentCount = 0;
        state.lastAdjustTime = std::chrono::steady_clock::now();
    }
};

#endif // RAWRXD_PIPELINE_EXIT_GUARD_H
