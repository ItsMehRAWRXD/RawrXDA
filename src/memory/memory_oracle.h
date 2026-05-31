#pragma once
#include <mutex>
#include <vector>
#include <numeric>
#include <algorithm>
#include "memory_policy_types.h"
#include "memory_oracle_metrics.h"
#include "../telemetry/sovereign_stats_block_v2.h"
#include "../telemetry/SovereignTelemetry_MASM_Bridge.h"
#include "../telemetry/execution_timeline.h"
#include "../engine/global_runtime_orchestrator.h"

namespace RawrXD::Memory {

class MemoryOracle {
public:
    void setStatsBlock(SovereignStatsBlockV2* block) {
        m_statsBlock = block;
    }

    void reportHardwareUsage(uint64_t vram, uint64_t ram, uint64_t vramTotal, uint64_t ramTotal) {
        TimelineScope ts(ExecutionPhase::MEMORY_RECLAIM);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metrics.vram_used = vram;
        m_metrics.ram_used = ram;
        
        // PID-inspired feedback controller for pressure smoothing
        double vram_p = (vramTotal > 0) ? (double)vram / vramTotal : 0.0;
        double ram_p = (ramTotal > 0) ? (double)ram / ramTotal : 0.0;
        float raw_pressure = (float)std::max(vram_p, ram_p);

        // Feedback: Error term vs target (0.7 is our safety ceiling)
        float error = raw_pressure - 0.70f;
        m_errorSum += error;
        float derivative = raw_pressure - m_lastPressure;
        
        // Smoothed controlled pressure = P + I + D (simplified)
        float controlled = raw_pressure + (error * 0.1f) + (m_errorSum * 0.01f) + (derivative * 0.05f);
        m_metrics.pressure = std::clamp(controlled, 0.0f, 1.0f);
        m_lastPressure = raw_pressure;
        
        // Feed the Global Orchestrator to synchronize speculation with memory safety
        GlobalRuntimeOrchestrator::Get().UpdateMemoryMetrics(m_metrics.pressure);

        updateWeights();

        if (m_statsBlock) {
            SovereignTelemetry_Update(m_statsBlock, -1.0f, -1.0f, 0, 0, (float)m_metrics.pressure);
            SovereignTelemetry_UpdateWeights(m_statsBlock, m_metrics.weights[0], m_metrics.weights[1], m_metrics.weights[2]);
        }
    }

    OracleAggregateMetrics getMetricSnapshot() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_metrics;
    }

private:
    void updateWeights() {
        float p = m_metrics.pressure;
        
        // Hysteresis-aware weight adjustment to prevent rapid switching
        // weights[0]: RETAIN, weights[1]: COMPRESS, weights[2]: TIERDOWN/OFFLOAD
        
        if (p < 0.25f) {
            // Cool mode: Prioritize speed
            m_metrics.weights[0] = 0.90f; m_metrics.weights[1] = 0.05f; m_metrics.weights[2] = 0.05f;
        } else if (p < 0.50f) {
            // Warm mode: Balanced compression
            m_metrics.weights[0] = 0.60f; m_metrics.weights[1] = 0.30f; m_metrics.weights[2] = 0.10f;
        } else if (p < 0.80f) {
            // Hot mode: Aggressive tiered storage
            m_metrics.weights[0] = 0.20f; m_metrics.weights[1] = 0.40f; m_metrics.weights[2] = 0.40f;
        } else {
            // Thermal mode: Emergency offloading
            m_metrics.weights[0] = 0.05f; m_metrics.weights[1] = 0.15f; m_metrics.weights[2] = 0.80f;
        }
    }

    OracleAggregateMetrics m_metrics;
    std::mutex m_mutex;
    SovereignStatsBlockV2* m_statsBlock = nullptr;
    float m_lastPressure = 0.0f;
    float m_errorSum = 0.0f;
    uint32_t m_cooldown = 0;
};

class MemoryOrchestrator {
public:
    MemoryOrchestrator(MemoryOracle& oracle) : m_oracle(oracle) {}
private:
    MemoryOracle& m_oracle;
};

}
