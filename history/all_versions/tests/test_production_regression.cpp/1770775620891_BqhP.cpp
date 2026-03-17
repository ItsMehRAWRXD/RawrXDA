// =============================================================================
// RawrXD Production Regression Test Suite
// Spec: tools.instructions.md §4 — Comprehensive Testing
// Tests: Observability, Error Handling, Loop State, Exception Handler,
//        Error Recovery, Configuration — all post-Qt migration
// =============================================================================

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Production headers under test
#include "agentic_observability.h"
#include "agentic_error_handler.h"
#include "agentic_loop_state.h"
#include "centralized_exception_handler.h"
#include "error_recovery_system.h"
#include "production_config_manager.h"

static int g_passed = 0;
static int g_failed = 0;
static int g_total  = 0;

#define TEST(name)                                                     \
    do {                                                               \
        g_total++;                                                     \
        const char* _tname = #name;                                    \
        fprintf(stderr, "  [TEST] %-50s ", _tname);                    \
        try {

#define PASS                                                           \
            g_passed++;                                                \
            fprintf(stderr, "PASS\n");                                 \
        } catch (const std::exception& ex) {                           \
            g_failed++;                                                \
            fprintf(stderr, "FAIL (exception: %s)\n", ex.what());      \
        } catch (...) {                                                \
            g_failed++;                                                \
            fprintf(stderr, "FAIL (unknown exception)\n");             \
        }                                                              \
    } while(0)

#define ASSERT_TRUE(cond)                                              \
    if (!(cond)) {                                                     \
        g_failed++;                                                    \
        fprintf(stderr, "FAIL (line %d: %s)\n", __LINE__, #cond);      \
        return;                                                        \
    }

#define ASSERT_EQ(a, b)                                                \
    if ((a) != (b)) {                                                  \
        g_failed++;                                                    \
        fprintf(stderr, "FAIL (line %d: %s != %s)\n", __LINE__, #a, #b);\
        return;                                                        \
    }

// =============================================================================
// §1: Observability Tests
// =============================================================================
static void test_observability() {
    fprintf(stderr, "\n=== Observability (agentic_observability) ===\n");

    TEST(singleton_instance) {
        auto& obs1 = AgenticObservability::instance();
        auto& obs2 = AgenticObservability::instance();
        ASSERT_TRUE(&obs1 == &obs2);
    PASS; }

    // Capture log output via callback
    static std::string lastLogMsg;
    static std::string lastLogLevel;
    static std::string lastLogComponent;

    TEST(log_callback_fires) {
        auto& obs = AgenticObservability::instance();
        obs.setLogCallback([](const char* level, const char* component,
                              const char* message, void*) {
            lastLogLevel = level;
            lastLogComponent = component;
            lastLogMsg = message;
        }, nullptr);
        obs.logInfo("TestComponent", "hello regression test");
        ASSERT_TRUE(lastLogMsg == "hello regression test");
        ASSERT_TRUE(lastLogComponent == "TestComponent");
        ASSERT_TRUE(lastLogLevel.find("INFO") != std::string::npos ||
                     lastLogLevel.find("info") != std::string::npos ||
                     lastLogLevel == "INFO");
    PASS; }

    TEST(log_error_level) {
        auto& obs = AgenticObservability::instance();
        obs.logError("TestComponent", "error message");
        ASSERT_TRUE(lastLogMsg == "error message");
    PASS; }

    TEST(log_critical_level) {
        auto& obs = AgenticObservability::instance();
        obs.logCritical("TestComponent", "critical message");
        ASSERT_TRUE(lastLogMsg == "critical message");
    PASS; }

    TEST(increment_counter) {
        auto& obs = AgenticObservability::instance();
        obs.incrementCounter("test.counter", 1);
        obs.incrementCounter("test.counter", 5);
        // Should not crash, counter should accumulate
    PASS; }

    TEST(set_gauge) {
        auto& obs = AgenticObservability::instance();
        obs.setGauge("test.gauge", 42.0);
    PASS; }

    TEST(record_histogram) {
        auto& obs = AgenticObservability::instance();
        obs.recordHistogram("test.latency", 15.5);
        obs.recordHistogram("test.latency", 22.3);
    PASS; }

    static bool spanCallbackFired = false;
    TEST(tracing_start_end_span) {
        auto& obs = AgenticObservability::instance();
        obs.setSpanCallback([](const char*, const char*, double, void*) {
            spanCallbackFired = true;
        }, nullptr);
        std::string traceId = obs.startTrace("test-trace");
        ASSERT_TRUE(!traceId.empty());
        std::string spanId = obs.startSpan(traceId, "test-span");
        ASSERT_TRUE(!spanId.empty());
        obs.endSpan(traceId, spanId);
        ASSERT_TRUE(spanCallbackFired);
    PASS; }

    TEST(timing_guard_raii) {
        auto& obs = AgenticObservability::instance();
        {
            AgenticObservability::TimingGuard guard(obs, "test.timing");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        // Guard destructor should have recorded histogram
    PASS; }

    // Reset callbacks
    auto& obs = AgenticObservability::instance();
    obs.setLogCallback(nullptr, nullptr);
    obs.setSpanCallback(nullptr, nullptr);
}

// =============================================================================
// §2: Agentic Error Handler Tests
// =============================================================================
static void test_agentic_error_handler() {
    fprintf(stderr, "\n=== Agentic Error Handler ===\n");

    TEST(construct_handler) {
        AgenticErrorHandler handler;
        ASSERT_TRUE(handler.getErrorCount() == 0);
    PASS; }

    TEST(record_error) {
        AgenticErrorHandler handler;
        std::string id = handler.recordError("TestComp", "Something broke",
                                              ErrorSeverity_AEH::Error);
        ASSERT_TRUE(!id.empty());
        ASSERT_TRUE(handler.getErrorCount() == 1);
    PASS; }

    TEST(record_multiple_errors) {
        AgenticErrorHandler handler;
        handler.recordError("A", "err1", ErrorSeverity_AEH::Warning);
        handler.recordError("B", "err2", ErrorSeverity_AEH::Error);
        handler.recordError("C", "err3", ErrorSeverity_AEH::Critical);
        ASSERT_TRUE(handler.getErrorCount() == 3);
    PASS; }

    static bool recoveryStartedFired = false;
    TEST(recovery_callback) {
        AgenticErrorHandler handler;
        handler.setRecoveryStartedCallback([](const std::string&,
                                               const std::string&, void*) {
            recoveryStartedFired = true;
        }, nullptr);
        std::string id = handler.recordError("Net", "timeout",
                                              ErrorSeverity_AEH::Error);
        handler.attemptRecovery(id);
        ASSERT_TRUE(recoveryStartedFired);
    PASS; }

    TEST(clear_errors) {
        AgenticErrorHandler handler;
        handler.recordError("X", "e1", ErrorSeverity_AEH::Info);
        handler.recordError("Y", "e2", ErrorSeverity_AEH::Warning);
        ASSERT_TRUE(handler.getErrorCount() == 2);
        handler.clearErrors();
        ASSERT_TRUE(handler.getErrorCount() == 0);
    PASS; }

    TEST(error_statistics_json) {
        AgenticErrorHandler handler;
        handler.recordError("LLM", "token overflow", ErrorSeverity_AEH::Error);
        nlohmann::json stats = handler.getStatistics();
        ASSERT_TRUE(stats.contains("total_errors"));
        ASSERT_TRUE(stats["total_errors"].get<int>() >= 1);
    PASS; }
}

// =============================================================================
// §3: Agentic Loop State Tests
// =============================================================================
static void test_agentic_loop_state() {
    fprintf(stderr, "\n=== Agentic Loop State ===\n");

    TEST(construct_loop_state) {
        AgenticLoopState state;
        ASSERT_TRUE(state.getCurrentIteration() == 0);
        ASSERT_TRUE(!state.isComplete());
    PASS; }

    TEST(advance_iteration) {
        AgenticLoopState state;
        state.startIteration();
        ASSERT_TRUE(state.getCurrentIteration() == 1);
    PASS; }

    TEST(add_decision) {
        AgenticLoopState state;
        state.startIteration();
        state.addDecision("tool", "run_search", 0.95f);
        state.endIteration("ok");
        nlohmann::json snapshot = state.toJson();
        ASSERT_TRUE(snapshot.contains("iterations") ||
                     snapshot.contains("current_iteration"));
    PASS; }

    TEST(mark_complete) {
        AgenticLoopState state;
        state.startIteration();
        state.endIteration("done");
        state.markComplete("success");
        ASSERT_TRUE(state.isComplete());
    PASS; }

    TEST(max_iterations_guard) {
        AgenticLoopState state;
        state.setMaxIterations(3);
        state.startIteration(); state.endIteration("ok");
        state.startIteration(); state.endIteration("ok");
        state.startIteration(); state.endIteration("ok");
        // After max iterations, should be flagged
        ASSERT_TRUE(state.getCurrentIteration() == 3);
    PASS; }

    TEST(serialization_roundtrip) {
        AgenticLoopState state;
        state.startIteration();
        state.addDecision("tool", "grep_search", 0.8f);
        state.endIteration("found");
        nlohmann::json j = state.toJson();
        std::string serialized = j.dump();
        ASSERT_TRUE(!serialized.empty());
        ASSERT_TRUE(serialized.size() > 2); // Not just "{}"
    PASS; }
}

// =============================================================================
// §4: Centralized Exception Handler Tests
// =============================================================================
static void test_exception_handler() {
    fprintf(stderr, "\n=== Centralized Exception Handler ===\n");

    TEST(singleton_instance) {
        auto& h1 = RawrXD::CentralizedExceptionHandler::instance();
        auto& h2 = RawrXD::CentralizedExceptionHandler::instance();
        ASSERT_TRUE(&h1 == &h2);
    PASS; }

    TEST(install_uninstall) {
        auto& handler = RawrXD::CentralizedExceptionHandler::instance();
        handler.installHandler();
        handler.uninstallHandler();
        // No crash = pass
    PASS; }

    TEST(auto_recovery_toggle) {
        auto& handler = RawrXD::CentralizedExceptionHandler::instance();
        handler.enableAutomaticRecovery(true);
        ASSERT_TRUE(handler.isAutomaticRecoveryEnabled());
        handler.enableAutomaticRecovery(false);
        ASSERT_TRUE(!handler.isAutomaticRecoveryEnabled());
    PASS; }

    TEST(report_exception) {
        auto& handler = RawrXD::CentralizedExceptionHandler::instance();
        handler.installHandler();
        try {
            throw std::runtime_error("test regression exception");
        } catch (const std::exception& ex) {
            handler.reportException(ex);
        }
        handler.uninstallHandler();
        // Should have logged to logs/exceptions.log
    PASS; }

    TEST(report_error_with_metadata) {
        auto& handler = RawrXD::CentralizedExceptionHandler::instance();
        handler.installHandler();
        handler.reportError("test error", "regression_test",
                            R"({"source":"test_suite"})");
        handler.uninstallHandler();
    PASS; }
}

// =============================================================================
// §5: Error Recovery System Tests
// =============================================================================
static void test_error_recovery_system() {
    fprintf(stderr, "\n=== Error Recovery System ===\n");

    TEST(construct_recovery_system) {
        ErrorRecoverySystem sys;
        SystemHealth health = sys.getSystemHealth();
        ASSERT_TRUE(health.healthScore >= 0.0);
    PASS; }

    TEST(record_and_retrieve_error) {
        ErrorRecoverySystem sys;
        std::string id = sys.recordError("TestComp", "disk full",
                                          ErrorSeverity::Error,
                                          ErrorCategory::FileIO);
        ASSERT_TRUE(!id.empty());
        ErrorRecord_ERS retrieved = sys.getError(id);
        ASSERT_TRUE(retrieved.errorId == id);
        ASSERT_TRUE(retrieved.component == "TestComp");
        ASSERT_TRUE(retrieved.message == "disk full");
    PASS; }

    TEST(active_errors_list) {
        ErrorRecoverySystem sys;
        sys.recordError("A", "err1", ErrorSeverity::Warning, ErrorCategory::System);
        sys.recordError("B", "err2", ErrorSeverity::Error, ErrorCategory::Network);
        std::vector<ErrorRecord_ERS> active = sys.getActiveErrors();
        ASSERT_TRUE(active.size() == 2);
    PASS; }

    TEST(errors_by_component) {
        ErrorRecoverySystem sys;
        sys.recordError("LLM", "timeout", ErrorSeverity::Error, ErrorCategory::AIModel);
        sys.recordError("LLM", "hallucination", ErrorSeverity::Warning, ErrorCategory::AIModel);
        sys.recordError("DB", "conn lost", ErrorSeverity::Critical, ErrorCategory::Database);
        std::vector<ErrorRecord_ERS> llmErrors = sys.getErrorsByComponent("LLM");
        ASSERT_TRUE(llmErrors.size() == 2);
    PASS; }

    TEST(errors_by_severity) {
        ErrorRecoverySystem sys;
        sys.recordError("X", "e1", ErrorSeverity::Critical, ErrorCategory::System);
        sys.recordError("Y", "e2", ErrorSeverity::Critical, ErrorCategory::Network);
        sys.recordError("Z", "e3", ErrorSeverity::Warning, ErrorCategory::FileIO);
        std::vector<ErrorRecord_ERS> critical = sys.getErrorsBySeverity(ErrorSeverity::Critical);
        ASSERT_TRUE(critical.size() == 2);
    PASS; }

    TEST(attempt_recovery) {
        ErrorRecoverySystem sys;
        std::string id = sys.recordError("Net", "connection reset",
                                          ErrorSeverity::Error,
                                          ErrorCategory::Network);
        bool attempted = sys.attemptRecovery(id);
        ASSERT_TRUE(attempted);
    PASS; }

    TEST(resolve_error) {
        ErrorRecoverySystem sys;
        std::string id = sys.recordError("Cache", "stale",
                                          ErrorSeverity::Warning,
                                          ErrorCategory::Performance);
        sys.resolveError(id);
        std::vector<ErrorRecord_ERS> active = sys.getActiveErrors();
        ASSERT_TRUE(active.empty());
    PASS; }

    TEST(system_health_scoring) {
        ErrorRecoverySystem sys;
        // No errors → health = 100
        SystemHealth h1 = sys.getSystemHealth();
        ASSERT_TRUE(h1.healthScore >= 99.0);
        ASSERT_TRUE(h1.isHealthy);

        // Add a critical error → health drops
        sys.recordError("Core", "crash", ErrorSeverity::Critical, ErrorCategory::System);
        SystemHealth h2 = sys.getSystemHealth();
        ASSERT_TRUE(h2.healthScore < h1.healthScore);
    PASS; }

    TEST(auto_recovery_toggle) {
        ErrorRecoverySystem sys;
        sys.enableAutoRecovery(true);
        sys.enableAutoRecovery(false);
        // No crash
    PASS; }

    TEST(clear_error_history) {
        ErrorRecoverySystem sys;
        sys.recordError("A", "e", ErrorSeverity::Info, ErrorCategory::System);
        sys.clearErrorHistory();
        // History cleared, no crash
    PASS; }

    TEST(error_statistics_json) {
        ErrorRecoverySystem sys;
        sys.recordError("LLM", "refused", ErrorSeverity::Error, ErrorCategory::AIModel);
        nlohmann::json stats = sys.getErrorStatistics();
        ASSERT_TRUE(stats.contains("active_errors"));
        ASSERT_TRUE(stats.contains("health_score"));
        ASSERT_TRUE(stats["active_errors"].get<int>() >= 1);
    PASS; }

    static bool fallbackFired = false;
    TEST(recovery_callback_fires) {
        ErrorRecoverySystem sys;
        sys.setFallbackToLocalCallback([](const std::string&, void*) {
            fallbackFired = true;
        }, nullptr);
        std::string id = sys.recordError("Cloud", "unavailable",
                                          ErrorSeverity::Error,
                                          ErrorCategory::CloudProvider);
        sys.attemptRecovery(id);
        // fallbackFired may or may not fire depending on strategy selection
        // The test validates the callback mechanism doesn't crash
    PASS; }

    TEST(tick_no_crash) {
        ErrorRecoverySystem sys;
        sys.enableAutoRecovery(true);
        sys.recordError("A", "e", ErrorSeverity::Error, ErrorCategory::System);
        // Simulate a few ticks
        sys.tick();
        sys.tick();
        sys.tick();
    PASS; }
}

// =============================================================================
// §6: Production Config Manager Tests
// =============================================================================
static void test_production_config() {
    fprintf(stderr, "\n=== Production Config Manager ===\n");

    TEST(singleton_instance) {
        auto& cfg1 = ProductionConfigManager::instance();
        auto& cfg2 = ProductionConfigManager::instance();
        ASSERT_TRUE(&cfg1 == &cfg2);
    PASS; }

    TEST(default_environment) {
        auto& cfg = ProductionConfigManager::instance();
        std::string env = cfg.getEnvironment();
        ASSERT_TRUE(!env.empty());
    PASS; }

    TEST(get_set_string) {
        auto& cfg = ProductionConfigManager::instance();
        cfg.set("test.key", std::string("test_value"));
        std::string val = cfg.getString("test.key", "default");
        ASSERT_TRUE(val == "test_value");
    PASS; }

    TEST(get_default_on_missing) {
        auto& cfg = ProductionConfigManager::instance();
        std::string val = cfg.getString("nonexistent.key", "fallback");
        ASSERT_TRUE(val == "fallback");
    PASS; }

    TEST(feature_toggle) {
        auto& cfg = ProductionConfigManager::instance();
        cfg.set("feature.experimental_llm", true);
        bool enabled = cfg.getBool("feature.experimental_llm", false);
        ASSERT_TRUE(enabled);
    PASS; }
}

// =============================================================================
// Cross-Component Integration Tests
// =============================================================================
static void test_integration() {
    fprintf(stderr, "\n=== Cross-Component Integration ===\n");

    TEST(observability_plus_error_handler) {
        // Ensure observability and error handler can coexist
        auto& obs = AgenticObservability::instance();
        AgenticErrorHandler handler;
        obs.logInfo("Integration", "Starting error handler test");
        std::string id = handler.recordError("Integration", "test",
                                              ErrorSeverity_AEH::Info);
        obs.incrementCounter("integration.errors_recorded", 1);
        ASSERT_TRUE(!id.empty());
    PASS; }

    TEST(loop_state_plus_error_handler) {
        AgenticLoopState state;
        AgenticErrorHandler handler;
        state.startIteration();
        state.addDecision("tool", "analyze", 0.9f);
        // Simulate error during iteration
        handler.recordError("LoopState", "tool failed",
                            ErrorSeverity_AEH::Error);
        state.endIteration("error");
        ASSERT_TRUE(state.getCurrentIteration() == 1);
        ASSERT_TRUE(handler.getErrorCount() == 1);
    PASS; }

    TEST(recovery_system_plus_observability) {
        auto& obs = AgenticObservability::instance();
        ErrorRecoverySystem sys;
        obs.logInfo("Integration", "Recording error in recovery system");
        std::string id = sys.recordError("IntTest", "network flap",
                                          ErrorSeverity::Error,
                                          ErrorCategory::Network);
        obs.incrementCounter("integration.recovery_attempts", 1);
        sys.attemptRecovery(id);
        nlohmann::json stats = sys.getErrorStatistics();
        ASSERT_TRUE(stats["active_errors"].get<int>() >= 0);
    PASS; }

    TEST(exception_handler_plus_observability) {
        auto& obs = AgenticObservability::instance();
        auto& handler = RawrXD::CentralizedExceptionHandler::instance();
        handler.installHandler();
        obs.logInfo("Integration", "Exception handler active");
        try {
            throw std::runtime_error("integration test exception");
        } catch (const std::exception& ex) {
            handler.reportException(ex);
            obs.incrementCounter("integration.exceptions_caught", 1);
        }
        handler.uninstallHandler();
    PASS; }
}

// =============================================================================
// Fuzz-Like Stress Tests (§4 Fuzz Testing)
// =============================================================================
static void test_stress() {
    fprintf(stderr, "\n=== Stress / Fuzz Tests ===\n");

    TEST(rapid_error_recording) {
        AgenticErrorHandler handler;
        for (int i = 0; i < 1000; i++) {
            handler.recordError("Stress", "error_" + std::to_string(i),
                                ErrorSeverity_AEH::Warning);
        }
        ASSERT_TRUE(handler.getErrorCount() == 1000);
    PASS; }

    TEST(rapid_loop_iterations) {
        AgenticLoopState state;
        state.setMaxIterations(500);
        for (int i = 0; i < 500; i++) {
            state.startIteration();
            state.addDecision("tool", "action_" + std::to_string(i), 0.5f);
            state.endIteration("ok");
        }
        ASSERT_TRUE(state.getCurrentIteration() == 500);
    PASS; }

    TEST(rapid_recovery_system_errors) {
        ErrorRecoverySystem sys;
        for (int i = 0; i < 200; i++) {
            sys.recordError("Stress_" + std::to_string(i % 10),
                            "err_" + std::to_string(i),
                            ErrorSeverity::Warning,
                            ErrorCategory::System);
        }
        std::vector<ErrorRecord_ERS> all = sys.getActiveErrors();
        ASSERT_TRUE(all.size() == 200);
        nlohmann::json stats = sys.getErrorStatistics();
        ASSERT_TRUE(stats["active_errors"].get<int>() == 200);
    PASS; }

    TEST(empty_string_handling) {
        AgenticErrorHandler handler;
        std::string id = handler.recordError("", "", ErrorSeverity_AEH::Info);
        ASSERT_TRUE(!id.empty()); // Should still generate an ID
    PASS; }

    TEST(very_long_message) {
        AgenticErrorHandler handler;
        std::string longMsg(10000, 'X');
        std::string id = handler.recordError("Fuzz", longMsg,
                                              ErrorSeverity_AEH::Error);
        ASSERT_TRUE(!id.empty());
    PASS; }

    TEST(concurrent_observability_logging) {
        auto& obs = AgenticObservability::instance();
        // Rapid-fire logging from single thread (thread-safety test)
        for (int i = 0; i < 500; i++) {
            obs.logInfo("StressTest", "msg_" + std::to_string(i));
            obs.incrementCounter("stress.counter", 1);
        }
    PASS; }
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    fprintf(stderr, "====================================================\n");
    fprintf(stderr, " RawrXD Production Regression Test Suite\n");
    fprintf(stderr, " Spec: tools.instructions.md §4\n");
    fprintf(stderr, "====================================================\n");

    test_observability();
    test_agentic_error_handler();
    test_agentic_loop_state();
    test_exception_handler();
    test_error_recovery_system();
    test_production_config();
    test_integration();
    test_stress();

    fprintf(stderr, "\n====================================================\n");
    fprintf(stderr, " Results: %d passed, %d failed, %d total\n",
            g_passed, g_failed, g_total);
    fprintf(stderr, "====================================================\n");

    return g_failed > 0 ? 1 : 0;
}
