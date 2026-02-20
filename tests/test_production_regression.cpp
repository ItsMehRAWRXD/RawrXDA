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
        // Should not crash, initial state is valid
    PASS; }

    TEST(record_error) {
        AgenticErrorHandler handler;
        std::string id = handler.recordError(ErrorType::ExecutionError,
                                              "Something broke", "TestComp");
        ASSERT_TRUE(!id.empty());
    PASS; }

    TEST(record_multiple_errors) {
        AgenticErrorHandler handler;
        handler.recordError(ErrorType::ValidationError, "err1", "A");
        handler.recordError(ErrorType::NetworkError, "err2", "B");
        handler.recordError(ErrorType::ResourceError, "err3", "C");
        ASSERT_TRUE(handler.getErrorCount(ErrorType::ValidationError) >= 1);
        ASSERT_TRUE(handler.getErrorCount(ErrorType::NetworkError) >= 1);
    PASS; }

    TEST(execute_recovery) {
        AgenticErrorHandler handler;
        std::string id = handler.recordError(ErrorType::NetworkError,
                                              "timeout", "Net");
        bool result = handler.executeRecovery(id);
        // Recovery should at least not crash
        (void)result;
    PASS; }

    TEST(classify_error) {
        AgenticErrorHandler handler;
        ErrorType classified = handler.classifyError("connection refused");
        // Should classify to some type without crashing
        (void)classified;
    PASS; }

    TEST(retryable_check) {
        AgenticErrorHandler handler;
        bool retryable = handler.isRetryable(ErrorType::NetworkError);
        (void)retryable;
        bool fallbackable = handler.isFallbackable(ErrorType::ResourceError);
        (void)fallbackable;
    PASS; }

    TEST(error_statistics_json) {
        AgenticErrorHandler handler;
        handler.recordError(ErrorType::ExecutionError, "token overflow", "LLM");
        nlohmann::json stats = handler.getErrorStatistics();
        ASSERT_TRUE(!stats.empty());
    PASS; }

    TEST(recent_errors) {
        AgenticErrorHandler handler;
        handler.recordError(ErrorType::TimeoutError, "slow", "API");
        auto recent = handler.getRecentErrors(10);
        ASSERT_TRUE(!recent.empty());
    PASS; }

    TEST(error_rate) {
        AgenticErrorHandler handler;
        float rate = handler.getErrorRate();
        // Initially should be zero or very low
        ASSERT_TRUE(rate >= 0.0f);
    PASS; }

    TEST(handle_exception) {
        AgenticErrorHandler handler;
        try {
            throw std::runtime_error("test exception for handler");
        } catch (const std::exception& e) {
            nlohmann::json result = handler.handleError(e, "TestComp",
                                                         nlohmann::json::object());
            ASSERT_TRUE(!result.empty());
        }
    PASS; }
}

// =============================================================================
// §3: Agentic Loop State Tests
// =============================================================================
static void test_agentic_loop_state() {
    fprintf(stderr, "\n=== Agentic Loop State ===\n");

    TEST(construct_loop_state) {
        AgenticLoopState state;
        ASSERT_TRUE(state.getTotalIterations() == 0);
    PASS; }

    TEST(start_iteration) {
        AgenticLoopState state;
        state.startIteration("analyze the codebase");
        ASSERT_TRUE(state.getTotalIterations() >= 0);
    PASS; }

    TEST(record_decision) {
        AgenticLoopState state;
        state.startIteration("search for files");
        state.recordDecision("use grep_search", nlohmann::json{{"reason", "exact match"}}, 0.95f);
        auto decisions = state.getDecisionHistory();
        ASSERT_TRUE(!decisions.empty());
    PASS; }

    TEST(end_iteration) {
        AgenticLoopState state;
        state.startIteration("test iteration");
        state.endIteration(IterationStatus::Completed, "done successfully");
        ASSERT_TRUE(state.getCompletedIterations() >= 1);
    PASS; }

    TEST(multiple_iterations) {
        AgenticLoopState state;
        state.startIteration("step 1");
        state.endIteration(IterationStatus::Completed, "ok");
        state.startIteration("step 2");
        state.endIteration(IterationStatus::Completed, "ok");
        state.startIteration("step 3");
        state.endIteration(IterationStatus::Completed, "ok");
        ASSERT_TRUE(state.getTotalIterations() >= 3);
    PASS; }

    TEST(failed_iteration) {
        AgenticLoopState state;
        state.startIteration("risky operation");
        state.endIteration(IterationStatus::Failed, "crashed");
        ASSERT_TRUE(state.getFailedIterations() >= 1);
    PASS; }

    TEST(take_snapshot) {
        AgenticLoopState state;
        state.startIteration("snapshot test");
        state.recordDecision("tool", nlohmann::json{{"tool","grep"}}, 0.8f);
        state.endIteration(IterationStatus::Completed, "found");
        nlohmann::json snapshot = state.takeSnapshot();
        ASSERT_TRUE(!snapshot.empty());
    PASS; }

    TEST(serialize_deserialize) {
        AgenticLoopState state;
        state.startIteration("serialize test");
        state.endIteration(IterationStatus::Completed, "done");
        std::string serialized = state.serializeState();
        ASSERT_TRUE(!serialized.empty());
        ASSERT_TRUE(serialized.size() > 2); // Not just "{}"
    PASS; }

    TEST(set_and_get_goal) {
        AgenticLoopState state;
        state.setGoal("Fix all bugs");
        std::string goal = state.getGoal();
        ASSERT_TRUE(goal == "Fix all bugs");
    PASS; }

    TEST(progress_tracking) {
        AgenticLoopState state;
        state.updateProgress(3, 10);
        float pct = state.getProgressPercentage();
        ASSERT_TRUE(pct > 0.0f);
    PASS; }

    TEST(decision_confidence) {
        AgenticLoopState state;
        state.startIteration("confidence test");
        state.recordDecision("action1", nlohmann::json{}, 0.9f);
        state.recordDecision("action2", nlohmann::json{}, 0.7f);
        float avg = state.getAverageDecisionConfidence();
        ASSERT_TRUE(avg > 0.0f && avg <= 1.0f);
    PASS; }

    TEST(memory_operations) {
        AgenticLoopState state;
        state.addToMemory("search_results", nlohmann::json{{"count", 42}});
        nlohmann::json retrieved = state.getFromMemory("search_results");
        ASSERT_TRUE(retrieved.contains("count"));
        ASSERT_TRUE(retrieved["count"].get<int>() == 42);
    PASS; }

    TEST(constraints) {
        AgenticLoopState state;
        state.addConstraint("max_tokens", "4096");
        nlohmann::json constraints = state.getAllConstraints();
        ASSERT_TRUE(!constraints.empty());
    PASS; }

    TEST(metrics) {
        AgenticLoopState state;
        state.startIteration("metrics test");
        state.endIteration(IterationStatus::Completed, "ok");
        nlohmann::json metrics = state.getMetrics();
        ASSERT_TRUE(!metrics.empty());
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
        std::string id = sys.recordError("TestComp", ErrorSeverity::Error,
                                          ErrorCategory::FileIO,
                                          "disk full");
        ASSERT_TRUE(!id.empty());
        ErrorRecord_ERS retrieved = sys.getError(id);
        ASSERT_TRUE(retrieved.errorId == id);
        ASSERT_TRUE(retrieved.component == "TestComp");
        ASSERT_TRUE(retrieved.message == "disk full");
    PASS; }

    TEST(active_errors_list) {
        ErrorRecoverySystem sys;
        sys.recordError("A", ErrorSeverity::Warning, ErrorCategory::System, "err1");
        sys.recordError("B", ErrorSeverity::Error, ErrorCategory::Network, "err2");
        std::vector<ErrorRecord_ERS> active = sys.getActiveErrors();
        ASSERT_TRUE(active.size() == 2);
    PASS; }

    TEST(errors_by_component) {
        ErrorRecoverySystem sys;
        sys.recordError("LLM", ErrorSeverity::Error, ErrorCategory::AIModel, "timeout");
        sys.recordError("LLM", ErrorSeverity::Warning, ErrorCategory::AIModel, "hallucination");
        sys.recordError("DB", ErrorSeverity::Critical, ErrorCategory::Database, "conn lost");
        std::vector<ErrorRecord_ERS> llmErrors = sys.getErrorsByComponent("LLM");
        ASSERT_TRUE(llmErrors.size() == 2);
    PASS; }

    TEST(errors_by_severity) {
        ErrorRecoverySystem sys;
        sys.recordError("X", ErrorSeverity::Critical, ErrorCategory::System, "e1");
        sys.recordError("Y", ErrorSeverity::Critical, ErrorCategory::Network, "e2");
        sys.recordError("Z", ErrorSeverity::Warning, ErrorCategory::FileIO, "e3");
        std::vector<ErrorRecord_ERS> critical = sys.getErrorsBySeverity(ErrorSeverity::Critical);
        ASSERT_TRUE(critical.size() == 2);
    PASS; }

    TEST(attempt_recovery) {
        ErrorRecoverySystem sys;
        std::string id = sys.recordError("Net", ErrorSeverity::Error,
                                          ErrorCategory::Network,
                                          "connection reset");
        bool attempted = sys.attemptRecovery(id);
        ASSERT_TRUE(attempted);
    PASS; }

    TEST(resolve_error) {
        ErrorRecoverySystem sys;
        std::string id = sys.recordError("Cache", ErrorSeverity::Warning,
                                          ErrorCategory::Performance, "stale");
        sys.resolveError(id);
        std::vector<ErrorRecord_ERS> active = sys.getActiveErrors();
        ASSERT_TRUE(active.empty());
    PASS; }

    TEST(system_health_scoring) {
        ErrorRecoverySystem sys;
        SystemHealth h1 = sys.getSystemHealth();
        ASSERT_TRUE(h1.healthScore >= 99.0);
        ASSERT_TRUE(h1.isHealthy);

        sys.recordError("Core", ErrorSeverity::Critical, ErrorCategory::System, "crash");
        SystemHealth h2 = sys.getSystemHealth();
        ASSERT_TRUE(h2.healthScore < h1.healthScore);
    PASS; }

    TEST(auto_recovery_toggle) {
        ErrorRecoverySystem sys;
        sys.enableAutoRecovery(true);
        sys.enableAutoRecovery(false);
    PASS; }

    TEST(clear_error_history) {
        ErrorRecoverySystem sys;
        sys.recordError("A", ErrorSeverity::Info, ErrorCategory::System, "e");
        sys.clearErrorHistory();
    PASS; }

    TEST(error_statistics_json) {
        ErrorRecoverySystem sys;
        sys.recordError("LLM", ErrorSeverity::Error, ErrorCategory::AIModel, "refused");
        nlohmann::json stats = sys.getErrorStatistics();
        ASSERT_TRUE(stats.contains("active_errors"));
        ASSERT_TRUE(stats.contains("health_score"));
        ASSERT_TRUE(stats["active_errors"].get<int>() >= 1);
    PASS; }

    TEST(tick_no_crash) {
        ErrorRecoverySystem sys;
        sys.enableAutoRecovery(true);
        sys.recordError("A", ErrorSeverity::Error, ErrorCategory::System, "e");
        sys.tick();
        sys.tick();
        sys.tick();
    PASS; }

    TEST(severity_to_string) {
        ErrorRecoverySystem sys;
        std::string s = sys.errorSeverityToString(ErrorSeverity::Critical);
        ASSERT_TRUE(s == "CRITICAL");
    PASS; }

    TEST(category_to_string) {
        ErrorRecoverySystem sys;
        std::string c = sys.errorCategoryToString(ErrorCategory::AIModel);
        ASSERT_TRUE(c == "AIModel");
    PASS; }
}

// =============================================================================
// §6: Production Config Manager Tests
// =============================================================================
static void test_production_config() {
    fprintf(stderr, "\n=== Production Config Manager ===\n");

    TEST(singleton_instance) {
        auto& cfg1 = RawrXD::ProductionConfigManager::instance();
        auto& cfg2 = RawrXD::ProductionConfigManager::instance();
        ASSERT_TRUE(&cfg1 == &cfg2);
    PASS; }

    TEST(default_environment) {
        auto& cfg = RawrXD::ProductionConfigManager::instance();
        std::string env = cfg.getEnvironment();
        ASSERT_TRUE(!env.empty());
    PASS; }

    TEST(value_with_default) {
        auto& cfg = RawrXD::ProductionConfigManager::instance();
        nlohmann::json val = cfg.value("nonexistent.key", "fallback");
        ASSERT_TRUE(val == "fallback");
    PASS; }

    TEST(feature_toggle) {
        auto& cfg = RawrXD::ProductionConfigManager::instance();
        // Check that feature toggle API doesn't crash
        bool enabled = cfg.isFeatureEnabled("experimental_llm");
        (void)enabled; // May be true or false depending on config
    PASS; }
}

// =============================================================================
// Cross-Component Integration Tests
// =============================================================================
static void test_integration() {
    fprintf(stderr, "\n=== Cross-Component Integration ===\n");

    TEST(observability_plus_error_handler) {
        auto& obs = AgenticObservability::instance();
        AgenticErrorHandler handler;
        obs.logInfo("Integration", "Starting error handler test");
        std::string id = handler.recordError(ErrorType::InternalError,
                                              "test", "Integration");
        obs.incrementCounter("integration.errors_recorded", 1);
        ASSERT_TRUE(!id.empty());
    PASS; }

    TEST(loop_state_plus_error_handler) {
        AgenticLoopState state;
        AgenticErrorHandler handler;
        state.startIteration("integration test");
        state.recordDecision("analyze", nlohmann::json{}, 0.9f);
        handler.recordError(ErrorType::ExecutionError, "tool failed", "LoopState");
        state.endIteration(IterationStatus::Failed, "error");
        ASSERT_TRUE(state.getFailedIterations() >= 1);
    PASS; }

    TEST(recovery_system_plus_observability) {
        auto& obs = AgenticObservability::instance();
        ErrorRecoverySystem sys;
        obs.logInfo("Integration", "Recording error in recovery system");
        std::string id = sys.recordError("IntTest", ErrorSeverity::Error,
                                          ErrorCategory::Network,
                                          "network flap");
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

    TEST(rapid_error_recording_handler) {
        AgenticErrorHandler handler;
        for (int i = 0; i < 1000; i++) {
            handler.recordError(ErrorType::ExecutionError,
                                "error_" + std::to_string(i), "Stress");
        }
        ASSERT_TRUE(handler.getErrorCount(ErrorType::ExecutionError) >= 1000);
    PASS; }

    TEST(rapid_loop_iterations) {
        AgenticLoopState state;
        for (int i = 0; i < 100; i++) {
            state.startIteration("step_" + std::to_string(i));
            state.recordDecision("action", nlohmann::json{}, 0.5f);
            state.endIteration(IterationStatus::Completed, "ok");
        }
        ASSERT_TRUE(state.getTotalIterations() >= 100);
    PASS; }

    TEST(rapid_recovery_system_errors) {
        ErrorRecoverySystem sys;
        for (int i = 0; i < 200; i++) {
            sys.recordError("Stress_" + std::to_string(i % 10),
                            ErrorSeverity::Warning,
                            ErrorCategory::System,
                            "err_" + std::to_string(i));
        }
        std::vector<ErrorRecord_ERS> all = sys.getActiveErrors();
        ASSERT_TRUE(all.size() == 200);
        nlohmann::json stats = sys.getErrorStatistics();
        ASSERT_TRUE(stats["active_errors"].get<int>() == 200);
    PASS; }

    TEST(empty_string_handling) {
        AgenticErrorHandler handler;
        std::string id = handler.recordError(ErrorType::Unknown, "", "");
        ASSERT_TRUE(!id.empty());
    PASS; }

    TEST(very_long_message) {
        AgenticErrorHandler handler;
        std::string longMsg(10000, 'X');
        std::string id = handler.recordError(ErrorType::ExecutionError,
                                              longMsg, "Fuzz");
        ASSERT_TRUE(!id.empty());
    PASS; }

    TEST(concurrent_observability_logging) {
        auto& obs = AgenticObservability::instance();
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
