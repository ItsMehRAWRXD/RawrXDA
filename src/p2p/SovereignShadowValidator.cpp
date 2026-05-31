#include "SovereignShadowValidator.h"
#include "EvolutionEventBus.h"
#include <windows.h>
#include <iostream>
#include <sstream>

double SovereignShadowValidator::GetTicks() {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart * 1000.0;
}

void SovereignShadowValidator::EmitShadowMetrics(const ValidationMetrics& m, double baseline_latency) {
    std::stringstream ss;
    ss << "{\"shadow_latency_ms\": " << m.latency_ms 
       << ", \"baseline_latency_ms\": " << baseline_latency
       << ", \"equivalent\": " << (m.output_equivalent ? "true" : "false")
       << ", \"divergence\": " << m.divergence_count << "}";

    EvolutionEventBus::Instance().Emit("ShadowValidationResult", "LocalNode", ss.str().c_str());
    
    if (m.divergence_count > 0) {
        std::cerr << "[ShadowValidator] CRITICAL: Output divergence detected in sovereign kernel!" << std::endl;
    }
}
