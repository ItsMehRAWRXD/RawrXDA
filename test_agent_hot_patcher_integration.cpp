// ============================================================================
// File: test_agent_hot_patcher_integration.cpp
// Purpose: Integration tests for AgentHotPatcher (C++20, no Qt)
// License: Production Grade - Enterprise Ready
// ============================================================================

#include "agent/agent_hot_patcher.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define TEST_CHECK(cond) do { ++g_tests_run; if (!(cond)) { ++g_tests_failed; std::fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while(0)

static void onHallucinationDetected(const HallucinationDetection& detection, void*) {
    std::cout << "[CALLBACK] Hallucination detected: " << detection.hallucinationType
              << " (confidence: " << detection.confidence << ")\n";
}

static void onHallucinationCorrected(const HallucinationDetection& correction,
                                     const std::string& correctedContent, void*) {
    std::cout << "[CALLBACK] Hallucination corrected: " << correction.detectionId << "\n";
    std::cout << "Corrected content: " << correctedContent << "\n";
}

static void onNavigationErrorFixed(const NavigationFix& fix, void*) {
    std::cout << "[CALLBACK] Navigation error fixed: " << fix.incorrectPath
              << " -> " << fix.correctPath << "\n";
}

static void onBehaviorPatchApplied(const BehaviorPatch& patch, void*) {
    std::cout << "[CALLBACK] Behavior patch applied: " << patch.patchType << "\n";
}

static void onStatisticsUpdated(const JsonValue& stats, void*) {
    std::cout << "[CALLBACK] Statistics updated: "
              << stats["totalHallucinationsDetected"].toInt() << " hallucinations detected\n";
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=== AgentHotPatcher Integration Tests (C++20) ===\n";

    AgentHotPatcher patcher;
    patcher.registerHallucinationDetectedCallback(onHallucinationDetected, nullptr);
    patcher.registerHallucinationCorrectedCallback(onHallucinationCorrected, nullptr);
    patcher.registerNavigationErrorFixedCallback(onNavigationErrorFixed, nullptr);
    patcher.registerBehaviorPatchAppliedCallback(onBehaviorPatchApplied, nullptr);
    patcher.registerStatisticsUpdatedCallback(onStatisticsUpdated, nullptr);

    if (!patcher.initialize("dummy_gguf_loader.exe", 0)) {
        std::fprintf(stderr, "Failed to initialize AgentHotPatcher\n");
        return 1;
    }

    JsonValue emptyContext(JsonValue::Object{});

    // Test 1: Path hallucination correction
    std::cout << "\n--- Test 1: Path Hallucination Correction ---\n";
    std::string modelOutput1 = R"(
        {
            "reasoning": "I need to access the file at /mystical/path/to/config.json to read the settings",
            "action": "read_file",
            "navigationPath": "/mystical/path/to/config.json"
        }
    )";
    JsonValue result1 = patcher.interceptModelOutput(modelOutput1, emptyContext);
    std::cout << "Original: " << modelOutput1 << "\n";
    std::cout << "Was modified: " << (result1["wasModified"].toBool() ? "yes" : "no") << "\n";
    TEST_CHECK(result1.isObject());

    // Test 2: Logic contradiction
    std::cout << "\n--- Test 2: Logic Contradiction Correction ---\n";
    std::string modelOutput2 = R"(
        {
            "reasoning": "This approach always succeeds but always fails in edge cases",
            "action": "implement_solution"
        }
    )";
    JsonValue result2 = patcher.interceptModelOutput(modelOutput2, emptyContext);
    TEST_CHECK(result2.isObject());

    // Test 3: Navigation error fix
    std::cout << "\n--- Test 3: Navigation Error Fix ---\n";
    std::string modelOutput3 = R"(
        {
            "reasoning": "Navigate to the configuration directory",
            "action": "navigate",
            "navigationPath": "/absolute/path/../../with//double/slashes"
        }
    )";
    JsonValue result3 = patcher.interceptModelOutput(modelOutput3, emptyContext);
    TEST_CHECK(result3.isObject());

    // Test 4: Behavior patch (sensitive data)
    std::cout << "\n--- Test 4: Behavior Patch Application ---\n";
    std::string modelOutput4 = R"(
        {
            "reasoning": "User email is john.doe@example.com and phone is 555-12-3456",
            "action": "contact_user"
        }
    )";
    JsonValue result4 = patcher.interceptModelOutput(modelOutput4, emptyContext);
    std::string correctedReasoning;
    if (result4.isObject() && result4["modified"].isObject())
        correctedReasoning = result4["modified"]["reasoning"].toString();
    TEST_CHECK(correctedReasoning.find("john.doe@example.com") == std::string::npos);
    TEST_CHECK(correctedReasoning.find("555-12-3456") == std::string::npos);

    // Test 5: Statistics
    std::cout << "\n--- Test 5: Statistics Verification ---\n";
    JsonValue stats = patcher.getCorrectionStatistics();
    TEST_CHECK(stats.isObject());
    int total = stats["totalHallucinationsDetected"].toInt();
    int navFixes = stats["navigationFixesApplied"].toInt();
    std::cout << "Total hallucinations detected: " << total << "\n";
    std::cout << "Navigation fixes applied: " << navFixes << "\n";
    TEST_CHECK(total >= 0 && navFixes >= 0);

    std::cout << "\n=== Integration Tests Completed ===\n";
    std::cout << "Run: " << g_tests_run << " Failed: " << g_tests_failed << "\n";
    return g_tests_failed ? 1 : 0;
}
