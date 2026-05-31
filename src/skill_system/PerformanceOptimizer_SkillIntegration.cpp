// ============================================================================
// PerformanceOptimizer_SkillIntegration.cpp — Skill injection for Performance
// ============================================================================
// Provides skill context injection for all performance optimization requests.
// Ensures speculative decoding and optimization carry phase4 quality gates.
//
// USAGE:
//   Include in SpeculativeOptimizer.cpp or compile as separate unit.
// ============================================================================

#include "../skill_system/SkillInjectionHooks.h"
#include <string>

namespace RawrXD {
namespace Performance {

// ============================================================================
// PERFORMANCE OPTIMIZER SKILL INTEGRATION
// ============================================================================

// Called before EVERY performance optimization request
std::string EnrichOptimizationWithSkills(
    const std::string& originalPrompt,
    const std::string& optimizationTarget
) {
    return SkillSystem::Hook_Performance_OptimizationRequest(
        originalPrompt,
        optimizationTarget
    );
}

// Called for benchmark execution
std::string EnrichBenchmarkWithSkills(
    const std::string& benchmarkRequest,
    const std::string& benchmarkSuite
) {
    std::string enriched = SkillSystem::Hook_Performance_OptimizationRequest(
        benchmarkRequest,
        "benchmark_" + benchmarkSuite
    );
    
    enriched += "\\n\\n# Benchmark Context\\n";
    enriched += "# Suite: " + benchmarkSuite + "\\n";
    enriched += "# Mode: performance_validation\\n";
    
    return enriched;
}

// Called for memory profiling
std::string EnrichMemoryProfileWithSkills(
    const std::string& profileRequest,
    const std::string& targetComponent
) {
    std::string enriched = SkillSystem::Hook_Performance_OptimizationRequest(
        profileRequest,
        "memory_" + targetComponent
    );
    
    enriched += "\\n\\n# Memory Profile Context\\n";
    enriched += "# Component: " + targetComponent + "\\n";
    enriched += "# Mode: leak_detection\\n";
    
    return enriched;
}

// ============================================================================
// C-API for backward compatibility
// ============================================================================
extern "C" {
    __declspec(dllexport) const char* __stdcall Performance_InjectSkillContext(
        const char* prompt,
        const char* target
    ) {
        static std::string result;
        result = EnrichOptimizationWithSkills(
            prompt ? prompt : "",
            target ? target : ""
        );
        return result.c_str();
    }
}

} // namespace Performance
} // namespace RawrXD
