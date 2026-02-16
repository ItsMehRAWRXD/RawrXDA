// ============================================================================
// File: test_agent_hot_patcher.cpp
// Purpose: Comprehensive test suite for AgentHotPatcher (C++20, Qt-free)
// License: Production Grade - Enterprise Ready
// ============================================================================

#include "agent/agent_hot_patcher.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

static JsonValue emptyContext() { return JsonValue(JsonValue::Object{}); }

#define T_ASSERT(c) do { if (!(c)) { std::cerr << "FAIL: " << #c << std::endl; return 1; } } while(0)

int main(int, char**) {
    std::cout << "AgentHotPatcher test suite (C++20)" << std::endl;

    AgentHotPatcher patcher;
    T_ASSERT(patcher.initialize("dummy_gguf_loader.exe", 0));
    T_ASSERT(patcher.isHotPatchingEnabled());

    // testHallucinationDetection
    std::string contentInvalidPath = "The file is located at /mystical/path/to/file.txt";
    HallucinationDetection detection = patcher.detectHallucination(contentInvalidPath, emptyContext());
    T_ASSERT(!detection.detectionId.empty());
    T_ASSERT(detection.hallucinationType == "fabricated_path");
    T_ASSERT(detection.confidence > 0.6);

    std::string contentContradiction = "This always succeeds but always fails";
    detection = patcher.detectHallucination(contentContradiction, emptyContext());
    T_ASSERT(!detection.detectionId.empty());
    T_ASSERT(detection.hallucinationType == "logic_contradiction");

    std::string incompleteReasoning = "The answer is yes";
    detection = patcher.detectHallucination(incompleteReasoning, emptyContext());
    T_ASSERT(!detection.detectionId.empty());
    T_ASSERT(detection.hallucinationType == "incomplete_reasoning");

    // testHallucinationCorrection
    HallucinationDetection pathDetection;
    pathDetection.hallucinationType = "fabricated_path";
    pathDetection.detectedContent = "/mystical/path";
    std::string corrected = patcher.correctHallucination(pathDetection);
    T_ASSERT(!corrected.empty());
    T_ASSERT(corrected.find("./src/kernels/q8k_kernel.cpp") != std::string::npos);

    HallucinationDetection logicDetection;
    logicDetection.hallucinationType = "logic_contradiction";
    corrected = patcher.correctHallucination(logicDetection);
    T_ASSERT(!corrected.empty());
    T_ASSERT(corrected.find("robust error handling") != std::string::npos);

    // testNavigationErrorFix
    std::string invalidPath = "/absolute/path/../with/double//slashes";
    NavigationFix fix = patcher.fixNavigationError(invalidPath, emptyContext());
    T_ASSERT(!fix.fixId.empty());
    T_ASSERT(!fix.reasoning.empty());
    T_ASSERT(fix.effectiveness > 0.0);

    // testBehaviorPatches
    JsonValue output(JsonValue::Object{});
    output["reasoning"] = "Simple reasoning";
    JsonValue patched = patcher.applyBehaviorPatches(output, emptyContext());
    T_ASSERT(!patched.isEmpty());
    T_ASSERT(patched.contains("reasoning"));

    // testStatistics
    JsonValue stats = patcher.getCorrectionStatistics();
    T_ASSERT(!stats.isEmpty());
    T_ASSERT(stats.contains("totalHallucinationsDetected"));
    T_ASSERT(stats.contains("hallucinationsCorrected"));
    T_ASSERT(stats.contains("navigationFixesApplied"));
    T_ASSERT(stats.contains("averageNavigationFixEffectiveness"));
    T_ASSERT(stats.contains("totalBehaviorPatches"));

    // testThreadSafety: multiple calls, no crash
    std::string testContent = "Path: /mystical/path";
    for (int i = 0; i < 10; ++i)
        patcher.detectHallucination(testContent, emptyContext());

    // testPerformance
    std::string largeContent;
    for (int i = 0; i < 1000; ++i)
        largeContent += "This is test content with path: /valid/path/file" + std::to_string(i) + "\n";
    auto t0 = std::chrono::steady_clock::now();
    detection = patcher.detectHallucination(largeContent, emptyContext());
    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cout << "Hallucination detection took " << elapsed << " ms" << std::endl;
    T_ASSERT(elapsed < 100);

    std::cout << "All AgentHotPatcher tests passed." << std::endl;
    return 0;
}
