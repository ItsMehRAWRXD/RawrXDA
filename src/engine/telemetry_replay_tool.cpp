#include "global_runtime_orchestrator.h"
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

/**
 * RawrXD Telemetry Replay & Sweep Tool
 * 
 * This tool allows offline replaying of telemetry traces against the GlobalRuntimeOrchestrator
 * to validate stability and perform parameter sweeps for tuning sigmoid slopes, 
 * hysteresis bands, and weight dynamics.
 */

namespace RawrXD {

struct TelemetryPoint {
    float pressure;
    float acceptance;
    float tps;
    float reuse;
};

class ReplayHarness {
public:
    void RunReplay(const std::vector<TelemetryPoint>& trace) {
        auto& orch = GlobalRuntimeOrchestrator::Get();
        
        std::cout << "Replaying " << trace.size() << " points..." << std::endl;
        std::cout << "Step | Pressure | N_Target | W_Mem | W_Thr | Score | Zone" << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;

        for (size_t i = 0; i < trace.size(); ++i) {
            orch.UpdateMemoryMetrics(trace[i].pressure);
            orch.UpdateInferenceMetrics(trace[i].acceptance, trace[i].tps);
            orch.UpdateCacheMetrics(trace[i].reuse);

            auto state = orch.GetCurrentState();
            
            if (i % 10 == 0) { // Print sample for visibility
                std::cout << std::setw(4) << i << " | "
                          << std::fixed << std::setprecision(2) << state.current_pressure << " | "
                          << std::setw(7) << state.optimal_speculate_n << " | "
                          << std::setw(5) << state.dyn_w_memory_risk << " | "
                          << std::setw(5) << state.dyn_w_throughput << " | "
                          << std::setw(5) << state.performance_score << " | "
                          << (state.current_pressure > 0.88f ? "CRIT" : (state.current_pressure > 0.82f ? "DANG" : "NORM"))
                          << std::endl;
            }
        }
        
        auto final_state = orch.GetCurrentState();
        std::cout << "--------------------------------------------------------" << std::endl;
        std::cout << "Final Stats - Avg N: " << final_state.telemetry.avg_n 
                  << " | Overrides: " << final_state.telemetry.overrides 
                  << " | Pulses: " << final_state.telemetry.pulses << std::endl;
    }

    void RunValidation(const std::vector<TelemetryPoint>& trace) {
        auto& orch = GlobalRuntimeOrchestrator::Get();
        std::cout << "Replaying with Safety Validation..." << std::endl;
        
        for (const auto& p : trace) {
            orch.UpdateMemoryMetrics(p.pressure);
            orch.UpdateInferenceMetrics(p.acceptance, p.tps);
            float risk = orch.AssessRisk();
            uint32_t n = orch.GetState().optimal_speculate_n;
            
            if (risk > 0.85f && n > 1) {
                std::cout << "[FATAL] Safety Violation: High Risk (" << risk 
                          << ") but N=" << n << " at pressure " << p.pressure << std::endl;
            }
        }
    }

    std::vector<TelemetryPoint> GenerateSynthethicAdversarialTrace() {
        std::vector<TelemetryPoint> trace;
        // Phase 1: Steady normal load
        for (int i = 0; i < 100; ++i) trace.push_back({0.4f, 0.85f, 50.0f, 0.9f});
        // Phase 2: Rapid pressure spike
        for (int i = 0; i < 50; ++i) trace.push_back({0.4f + (i * 0.01f), 0.8f, 45.0f, 0.8f});
        // Phase 3: Critical plateau
        for (int i = 0; i < 100; ++i) trace.push_back({0.92f, 0.7f, 30.0f, 0.5f});
        // Phase 4: Recovery
        for (int i = 0; i < 100; ++i) trace.push_back({0.92f - (i * 0.005f), 0.85f, 55.0f, 0.95f});
        return trace;
    }
};

} // namespace RawrXD

int main() {
    RawrXD::ReplayHarness harness;
    auto trace = harness.GenerateSynthethicAdversarialTrace();
    harness.RunReplay(trace);
    return 0;
}
