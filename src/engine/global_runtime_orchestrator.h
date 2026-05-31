#pragma once
#include <stdint.h>
#include <atomic>
#include <algorithm>
#include <chrono>

namespace RawrXD {

/**
 * GlobalRuntimeOrchestrator
 * 
 * The system "brain" that coordinates the coupled control loops:
 * 1. Speculative Depth (Compute Intensity)
 * 2. Memory Pressure Targets (VRAM Budget)
 * 3. GPU Queue Depth (Throughput vs. Latency)
 * 
 * Goal: Prevent oscillation by enforcing global constraints.
 */
class GlobalRuntimeOrchestrator {
public:
    struct State {
        float avg_acceptance_rate = 0.0f;
        float current_pressure = 0.0f;
        float pressure_derivative = 0.0f;
        float compute_saturation = 0.0f;
        float cache_reuse_prob = 0.0f;   // Likelihood of KV cache hit
        float momentum_n = 4.0f;         // Continuous momentum for n
        uint32_t optimal_speculate_n = 4;
        float performance_score = 0.0f;
        float score_confidence = 1.0f;   // Signal quality metric
        
        // Adaptive Weights (Self-Tuning)
        float dyn_w_throughput = 1.0f;
        float dyn_w_memory_risk = 2.0f;

        // Statistics for HUD/Audit
        struct {
            uint32_t overrides = 0;
            uint32_t pulses = 0;
            float avg_n = 4.0f;
        } telemetry;
    };

    struct Weights {
        float base_w_throughput = 1.0f;
        float base_w_latency_var = 0.5f;
        float base_w_memory_risk = 2.0f;
        float base_w_cache_efficiency = 0.8f;
    };

    static GlobalRuntimeOrchestrator& Get() {
        static GlobalRuntimeOrchestrator instance;
        return instance;
    }

    void UpdateInferenceMetrics(float acceptance_rate, float tps) {
        // Temporal Alignment: EMA window for coherent snapshots
        const float alpha = 0.15f; 
        m_state.avg_acceptance_rate = (m_state.avg_acceptance_rate * (1.0f - alpha)) + (acceptance_rate * alpha);
        m_last_tps = (m_last_tps * (1.0f - alpha)) + (tps * alpha);
        
        // Confidence update for throughput: higher variance reduces confidence
        float thr_jitter = std::abs(tps - m_last_tps) / (m_last_tps + 0.1f);
        m_state.score_confidence = (m_state.score_confidence * 0.95f) + (std::clamp(1.0f - thr_jitter, 0.5f, 1.0f) * 0.05f);

        AdaptWeights();
        Steer();
    }

    void UpdateCacheMetrics(float reuse_probability) {
        // High reuse -> lower cost for speculative verification
        m_state.cache_reuse_prob = (m_state.cache_reuse_prob * 0.9f) + (reuse_probability * 0.1f);
        Steer();
    }

    void UpdateMemoryMetrics(float pressure) {
        float instant_derivative = pressure - m_state.current_pressure;
        
        // Derivative Damping: Clamp and filter dP/dt to prevent micro-spike overreaction
        float clamped_derivative = std::clamp(instant_derivative, -0.05f, 0.05f);
        m_state.pressure_derivative = (m_state.pressure_derivative * 0.7f) + (clamped_derivative * 0.3f);
        m_state.current_pressure = pressure;

        // Signal confidence: Inverse of variance in the derivative
        float stability = 1.0f - std::clamp(std::abs(clamped_derivative - m_state.pressure_derivative) * 20.0f, 0.0f, 1.0f);
        m_state.score_confidence = (m_state.score_confidence * 0.85f) + (stability * 0.15f);

        AdaptWeights();
        Steer();
    }

    State GetCurrentState() const { return m_state; }

private:
    GlobalRuntimeOrchestrator() {
        m_state.dyn_w_throughput = m_weights.base_w_throughput;
        m_state.dyn_w_memory_risk = m_weights.base_w_memory_risk;
    }

    // Zone States for Hysteresis
    enum class Zone { Normal, Danger, Critical };
    Zone m_current_zone = Zone::Normal;

    // Statistics tracking for distribution analysis
    struct Stats {
        uint64_t n_samples = 0;
        double sum_n = 0;
        double sum_sq_n = 0;
        uint32_t overrides = 0;
        uint32_t exploration_pulses = 0;
    } m_stats;

    void AdaptWeights() {
        // Hysteresis-aware Zone Detection
        float p = m_state.current_pressure;
        Zone old_zone = m_current_zone;
        if (m_current_zone == Zone::Normal) {
            if (p > 0.82f) m_current_zone = Zone::Danger;
        } else if (m_current_zone == Zone::Danger) {
            if (p > 0.88f) m_current_zone = Zone::Critical;
            else if (p < 0.78f) m_current_zone = Zone::Normal; // 4% hysteresis band
        } else if (m_current_zone == Zone::Critical) {
            if (p < 0.84f) m_current_zone = Zone::Danger; // 4% hysteresis band
        }

        // Self-Tuning with Decay: prioritizes safety but recovers toward baseline
        float target_w_mem = m_weights.base_w_memory_risk;
        float target_w_thr = m_weights.base_w_throughput;

        if (m_current_zone != Zone::Normal || m_state.pressure_derivative > 0.01f) {
            target_w_mem = 12.0f; // Increased penalty floor
            target_w_thr = 0.15f;
        }

        // Adaptive Step: move toward target
        // Conditional relaxation: faster if we've been stable in Normal zone
        static uint32_t stable_counter = 0;
        if (m_current_zone == Zone::Normal && m_state.score_confidence > 0.9f) stable_counter++;
        else stable_counter = 0;

        float alpha = (target_w_mem > m_state.dyn_w_memory_risk) ? 0.25f : (stable_counter > 100 ? 0.02f : 0.005f);
        m_state.dyn_w_memory_risk = (m_state.dyn_w_memory_risk * (1.0f - alpha)) + (target_w_mem * alpha);
        m_state.dyn_w_throughput = (m_state.dyn_w_throughput * (1.0f - alpha)) + (target_w_thr * alpha);
    }

    void Steer() {
        bool in_danger = m_current_zone != Zone::Normal;
        bool in_critical = m_current_zone == Zone::Critical;

        // Evaluate cost function across a window
        auto evaluate_score = [&](float n) {
            float norm_n = n / 8.0f;
            // Predictive Horizon: factor in both derivative and absolute proximity
            float predicted_pressure = m_state.current_pressure + (m_state.pressure_derivative * 4.0f);
            
            // Proximity Risk: reward staying far from 90 even if derivative is zero
            float proximity_penalty = std::pow(std::max(0.0f, m_state.current_pressure - 0.60f) * 2.5f, 2.0f);
            
            // Soft-Compression Band: exponential penalty
            float base_risk_floor = in_danger ? 0.55f : 0.65f; // Lowered floor for earlier response
            float memory_risk = std::pow(std::max(0.0f, predicted_pressure - base_risk_floor) * 5.0f, 2.2f) + proximity_penalty;
            
            // Confidence-Weighted Throughput (floor at 0.5 to prevent starvation)
            float effective_conf = std::max(0.5f, m_state.score_confidence);
            float est_throughput = m_last_tps * (1.0f + (m_state.avg_acceptance_rate * norm_n)) * effective_conf;
            
            // Cache Bonus logic
            float cache_bonus = 0.0f;
            if (!in_danger) {
                cache_bonus = m_state.cache_reuse_prob * norm_n * m_weights.base_w_cache_efficiency;
            }

            return (est_throughput * m_state.dyn_w_throughput) + cache_bonus - (memory_risk * m_state.dyn_w_memory_risk);
        };

        float base_score = evaluate_score(m_state.momentum_n);
        
        // Gated Search: exploration pulses vs starvation
        static int search_counter = 0;
        // Pulse only if system is stable (high confidence)
        bool stability_gate = m_state.score_confidence > 0.85f;
        bool allow_exp_probing = stability_gate && (!in_danger || (++search_counter % 50 == 0)); 
        if (allow_exp_probing && in_danger) m_stats.exploration_pulses++;

        float score_p1 = evaluate_score(m_state.momentum_n + 1.0f);
        float score_m1 = evaluate_score(m_state.momentum_n - 1.0f);
        float score_p2 = allow_exp_probing ? evaluate_score(m_state.momentum_n + 2.0f) : -1e9f;
        float score_m2 = evaluate_score(m_state.momentum_n - 2.0f);

        float gradient = (score_p1 - score_m1) * 0.5f;
        if (allow_exp_probing && score_p2 > score_p1) gradient += 0.25f; 
        if (score_m2 > score_m1) gradient -= 0.25f;

        // Momentum Gating
        bool expansion_risk = in_danger || (m_state.pressure_derivative > 0.002f && gradient > 0);
        float alpha = expansion_risk ? 0.0f : (0.06f * m_state.score_confidence);
        
        m_state.momentum_n += alpha * gradient;
        
        // Continuous Ceiling Function: avoid discrete steps near limits
        if (m_state.current_pressure > 0.75f) { // Lowered floor for ceiling activation
            // Sigmoidal compression towards N=1 as we approach 95%
            float range = 0.95f - 0.75f;
            float x = (m_state.current_pressure - 0.75f) / range;
            // Sharpened sigmoid tail near critical
            float ceiling = 8.0f * (1.0f / (1.0f + std::exp(12.0f * (x - 0.53f)))); // Modified x-offset
            m_state.momentum_n = std::min(m_state.momentum_n, std::max(1.0f, ceiling));
        }

        // Hard Override Security
        if (m_state.current_pressure > 0.93f) {
            if (m_state.momentum_n > 1.05f) m_stats.overrides++;
            m_state.momentum_n = 1.0f;
        }
        
        m_state.momentum_n = std::clamp(m_state.momentum_n, 1.0f, 8.0f);

        // Update stats for distribution analysis
        m_stats.n_samples++;
        m_stats.sum_n += m_state.momentum_n;
        m_stats.sum_sq_n += (m_state.momentum_n * m_state.momentum_n);
        
        // Sync telemetry state for external visibility
        m_state.telemetry.overrides = m_stats.overrides;
        m_state.telemetry.pulses = m_stats.exploration_pulses;
        m_state.telemetry.avg_n = static_cast<float>(m_stats.sum_n / m_stats.n_samples);

        uint32_t new_n = static_cast<uint32_t>(std::round(m_state.momentum_n));
        
        if (new_n != m_state.optimal_speculate_n) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_change).count();
            
            bool contracting = new_n < m_state.optimal_speculate_n;
            // Instant contraction, metered expansion
            long long required_cooldown = contracting ? 10 : 2000; 
            
            if (elapsed > required_cooldown) {
                m_state.optimal_speculate_n = new_n;
                m_last_change = now;
            }
        }

        m_state.performance_score = base_score;
    }

    State m_state;
    Weights m_weights;
    float m_last_tps = 0.0f;
    std::chrono::steady_clock::time_point m_last_change = std::chrono::steady_clock::now();

public:
    const State& GetState() const { return m_state; }

    /**
     * AssessRisk
     * Returns a normalized risk factor [0, 1] based on current memory pressure,
     * derivative, and score confidence.
     */
    float AssessRisk() const {
        float predicted_pressure = m_state.current_pressure + (m_state.pressure_derivative * 4.0f);
        float memory_risk = std::pow(std::max(0.0f, predicted_pressure - 0.70f) * 4.0f, 2.0f);
        float confidence_penalty = (1.0f - m_state.score_confidence) * 0.5f;
        return std::clamp(memory_risk + confidence_penalty, 0.0f, 1.0f);
    }
};

} // namespace RawrXD
