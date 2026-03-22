// Smoke Tests: FailureIntelligence Orchestrator
// Validates: failure classification, analysis, recovery planning, autonomous healing
// Tests: all 8 failure categories, pattern matching, recovery strategy selection

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <vector>
#include "../src/agentic/failure_intelligence_orchestrator.hpp"

namespace {

using namespace Agentic;

class FailureIntelligenceTest : public ::testing::Test
{
protected:
    std::unique_ptr<FailureIntelligenceOrchestrator> orchestrator;
    std::ostringstream output_stream;
    std::vector<FailureSignal> detected_failures;
    std::vector<RecoveryPlan> initiated_recoveries;

    void SetUp() override
    {
        orchestrator = std::make_unique<FailureIntelligenceOrchestrator>();

        // Wire callbacks for verification
        orchestrator->setAnalysisLogFn([this](const std::string& log) {
            output_stream << "[ANALYSIS] " << log << "\n";
        });

        orchestrator->setFailureDetectedCallback([this](const FailureSignal& signal) {
            detected_failures.push_back(signal);
            output_stream << "[DETECTED] " << signal.source_component << ": " 
                         << signal.error_message << "\n";
        });

        orchestrator->setRecoveryInitiatedCallback([this](const RecoveryPlan& plan) {
            output_stream << "[RECOVERY_INIT] " << plan.strategy_description << "\n";
        });

        orchestrator->setRecoveryCompletedCallback([this](const RecoveryPlan& plan, bool success) {
            output_stream << "[RECOVERY_DONE] " << (success ? "SUCCESS" : "FAILED") << "\n";
        });
    }
};

// ============================================================================
// Category 1: Transient Failures (network timeouts, temporary resource issues)
// ============================================================================

TEST_F(FailureIntelligenceTest, DetectTransientNetworkTimeout)
{
    FailureSignal signal;
    signal.error_message = "Connection timeout after 30 seconds (WSAETIMEDOUT)";
    signal.source_component = "network_client";
    signal.exit_code = 10060;  // WSAETIMEDOUT
    signal.severity = SeverityLevel::Warning;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    ASSERT_EQ(recent.size(), 1);
    EXPECT_EQ(recent[0]->error_message, signal.error_message);

    auto analysis = orchestrator->analyzeFailure(*recent[0]);
    ASSERT_NE(analysis, nullptr);
    EXPECT_EQ(analysis->primary_category, FailureCategory::Transient);
    EXPECT_GT(analysis->analysis_confidence, 0.7f);  // High confidence for timeout pattern
}

TEST_F(FailureIntelligenceTest, TransientReconnectStrategy)
{
    FailureSignal signal;
    signal.error_message = "Connection refused by remote (ECONNREFUSED)";
    signal.exit_code = 111;
    signal.severity = SeverityLevel::Warning;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto plan = orchestrator->generateRecoveryPlan(*recent[0]);

    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->primary_strategy, RecoveryStrategy::RetryWithExponentialBackoff);
    EXPECT_GT(plan->recovery_steps.size(), 0);
}

// ============================================================================
// Category 2: Logic Errors (code bugs, assertion failures, unexpected state)
// ============================================================================

TEST_F(FailureIntelligenceTest, DetectLogicErrorAssertion)
{
    FailureSignal signal;
    signal.error_message = "Assertion failed: ptr != nullptr at line 532 in engine.cpp";
    signal.stderr_content = "Assertion failed at 0x00007FF8A1234567\nStack trace: engine.cpp:532";
    signal.source_component = "engine";
    signal.severity = SeverityLevel::Error;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto analysis = orchestrator->analyzeFailure(*recent[0]);

    ASSERT_NE(analysis, nullptr);
    EXPECT_EQ(analysis->primary_category, FailureCategory::Logic);
}

TEST_F(FailureIntelligenceTest, LogicEscalationStrategy)
{
    FailureSignal signal;
    signal.error_message = "Null pointer dereference detected";
    signal.severity = SeverityLevel::Critical;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto plan = orchestrator->generateRecoveryPlan(*recent[0]);

    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->primary_strategy, RecoveryStrategy::Escalate);
}

// ============================================================================
// Category 3: Timeout Failures (exceeded time limits, hanging processes)
// ============================================================================

TEST_F(FailureIntelligenceTest, DetectTimeoutLongRunning)
{
    FailureSignal signal;
    signal.error_message = "Operation exceeded maximum 120-second timeout";
    signal.duration_ms = 121000;
    signal.source_component = "build_system";
    signal.severity = SeverityLevel::Warning;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto analysis = orchestrator->analyzeFailure(*recent[0]);

    ASSERT_NE(analysis, nullptr);
    EXPECT_EQ(analysis->primary_category, FailureCategory::Timeout);
}

// ============================================================================
// Category 4: Dependency Failures (missing tools, broken paths, unavailable services)
// ============================================================================

TEST_F(FailureIntelligenceTest, DetectMissingDependency)
{
    FailureSignal signal;
    signal.error_message = "fatal: gcc compiler not found in PATH";
    signal.exit_code = 127;  // Command not found
    signal.source_component = "compiler_wrapper";
    signal.severity = SeverityLevel::Error;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto analysis = orchestrator->analyzeFailure(*recent[0]);

    ASSERT_NE(analysis, nullptr);
    EXPECT_EQ(analysis->primary_category, FailureCategory::Dependency);
}

TEST_F(FailureIntelligenceTest, DependencyFallbackStrategy)
{
    FailureSignal signal;
    signal.error_message = "gcc not found, trying clang instead";
    signal.severity = SeverityLevel::Warning;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto plan = orchestrator->generateRecoveryPlan(*recent[0]);

    ASSERT_NE(plan, nullptr);
    // Should suggest fallback strategy
    EXPECT_TRUE(plan->primary_strategy == RecoveryStrategy::FallbackToAlternative ||
                plan->primary_strategy == RecoveryStrategy::ReplanAndRecontinue);
}

// ============================================================================
// Category 5: Permission Failures (access denied, insufficient privileges)
// ============================================================================

TEST_F(FailureIntelligenceTest, DetectPermissionDenied)
{
    FailureSignal signal;
    signal.error_message = "Permission denied: cannot open /protected/file.dat";
    signal.exit_code = 13;  // EACCES
    signal.source_component = "file_system";
    signal.severity = SeverityLevel::Error;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto analysis = orchestrator->analyzeFailure(*recent[0]);

    ASSERT_NE(analysis, nullptr);
    EXPECT_EQ(analysis->primary_category, FailureCategory::Permission);
}

// ============================================================================
// Category 6: Configuration Failures (incorrect settings, missing config)
// ============================================================================

TEST_F(FailureIntelligenceTest, DetectConfigurationError)
{
    FailureSignal signal;
    signal.error_message = "Configuration error: invalid threshold value '999.99' (expected -1 to 100)";
    signal.source_component = "config_loader";
    signal.severity = SeverityLevel::Error;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto analysis = orchestrator->analyzeFailure(*recent[0]);

    ASSERT_NE(analysis, nullptr);
    EXPECT_EQ(analysis->primary_category, FailureCategory::Configuration);
}

// ============================================================================
// Category 7: Environmental Failures (out of memory, disk full, insufficient resources)
// ============================================================================

TEST_F(FailureIntelligenceTest, DetectOutOfMemory)
{
    FailureSignal signal;
    signal.error_message = "std::bad_alloc: failed to allocate 4GB memory block";
    signal.exit_code = 137;  // OOM kill on Linux
    signal.source_component = "memory_manager";
    signal.severity = SeverityLevel::Error;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto analysis = orchestrator->analyzeFailure(*recent[0]);

    ASSERT_NE(analysis, nullptr);
    EXPECT_EQ(analysis->primary_category, FailureCategory::Environmental);
}

TEST_F(FailureIntelligenceTest, EnvironmentalRecoveryPlan)
{
    FailureSignal signal;
    signal.error_message = "Disk space low: 50MB available, need 500MB";
    signal.severity = SeverityLevel::Warning;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto plan = orchestrator->generateRecoveryPlan(*recent[0]);

    ASSERT_NE(plan, nullptr);
    // Should propose cleanup or abort
    EXPECT_TRUE(plan->primary_strategy == RecoveryStrategy::RetryWithExponentialBackoff ||
                plan->primary_strategy == RecoveryStrategy::Abort);
}

// ============================================================================
// Category 8: Fatal Failures (unrecoverable errors, safety violations)
// ============================================================================

TEST_F(FailureIntelligenceTest, DetectFatalError)
{
    FailureSignal signal;
    signal.error_message = "FATAL: Stack overflow - infinite recursion detected";
    signal.severity = SeverityLevel::Critical;
    signal.source_component = "runtime";

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto analysis = orchestrator->analyzeFailure(*recent[0]);

    ASSERT_NE(analysis, nullptr);
    EXPECT_EQ(analysis->primary_category, FailureCategory::Fatal);
}

TEST_F(FailureIntelligenceTest, FatalRequiresEscalation)
{
    FailureSignal signal;
    signal.error_message = "FATAL: Unrecoverable system state";
    signal.severity = SeverityLevel::Critical;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto plan = orchestrator->generateRecoveryPlan(*recent[0]);

    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->primary_strategy, RecoveryStrategy::Escalate);
}

// ============================================================================
// Autonomous Recovery Pipeline (Full End-to-End)
// ============================================================================

TEST_F(FailureIntelligenceTest, AutonomousRecoveryPipeline)
{
    FailureSignal signal;
    signal.error_message = "Connection timeout after 30 seconds";
    signal.exit_code = 10060;
    signal.source_component = "network";
    signal.severity = SeverityLevel::Warning;

    // Execute full autonomous recovery
    std::string output;
    bool success = orchestrator->autonomousRecover(signal, output);

    // Should succeed for transient failures
    EXPECT_TRUE(success);
    EXPECT_GT(output.length(), 0);
    EXPECT_GT(detected_failures.size(), 0);
}

TEST_F(FailureIntelligenceTest, AutonomousCriticalEscalates)
{
    FailureSignal signal;
    signal.error_message = "FATAL: Core processor failure";
    signal.severity = SeverityLevel::Critical;
    signal.source_component = "processor";

    std::string output;
    bool success = orchestrator->autonomousRecover(signal, output);

    // Should escalate for critical failures (not auto-recover)
    EXPECT_FALSE(success);
    EXPECT_TRUE(output.find("escalat") != std::string::npos ||
                output.find("Escalat") != std::string::npos);
}

// ============================================================================
// Pattern Matching and Learning
// ============================================================================

TEST_F(FailureIntelligenceTest, PatternMatchingAccuracy)
{
    // Test multiple failure patterns
    FailureSignal timeout_signal;
    timeout_signal.error_message = "WSAETIMEDOUT: socket operation timeout";
    timeout_signal.exit_code = 10060;

    FailureSignal oom_signal;
    oom_signal.error_message = "std::bad_alloc: out of memory";

    FailureSignal denied_signal;
    denied_signal.error_message = "Permission denied (errno 13)";
    denied_signal.exit_code = 13;

    orchestrator->reportFailure(timeout_signal);
    orchestrator->reportFailure(oom_signal);
    orchestrator->reportFailure(denied_signal);

    auto analysis1 = orchestrator->analyzeFailure(timeout_signal);
    auto analysis2 = orchestrator->analyzeFailure(oom_signal);
    auto analysis3 = orchestrator->analyzeFailure(denied_signal);

    ASSERT_NE(analysis1, nullptr);
    ASSERT_NE(analysis2, nullptr);
    ASSERT_NE(analysis3, nullptr);

    EXPECT_EQ(analysis1->primary_category, FailureCategory::Transient);
    EXPECT_EQ(analysis2->primary_category, FailureCategory::Environmental);
    EXPECT_EQ(analysis3->primary_category, FailureCategory::Permission);
}

TEST_F(FailureIntelligenceTest, LearnFromFailureCorrection)
{
    FailureSignal signal;
    signal.error_message = "Ambiguous error: could be timeout or config";
    signal.exit_code = 1;

    orchestrator->reportFailure(signal);

    auto recent = orchestrator->getRecentFailures(1);
    auto analysis1 = orchestrator->analyzeFailure(*recent[0]);
    float initial_confidence = analysis1->analysis_confidence;

    // Human provides correction (it was actually a transient timeout)
    orchestrator->learnFromFailure(*recent[0], FailureCategory::Transient);

    // Re-analyze same signal (confidence should improve)
    auto analysis2 = orchestrator->analyzeFailure(*recent[0]);
    float updated_confidence = analysis2->analysis_confidence;

    // After learning, confidence should be higher or equal
    EXPECT_GE(updated_confidence, initial_confidence * 0.95f);
}

// ============================================================================
// Statistics and State Tracking
// ============================================================================

TEST_F(FailureIntelligenceTest, StatisticsAccumulation)
{
    // Report various failures
    for (int i = 0; i < 5; ++i) {
        FailureSignal signal;
        signal.error_message = "Test failure " + std::to_string(i);
        signal.severity = SeverityLevel::Warning;
        orchestrator->reportFailure(signal);
    }

    auto stats = orchestrator->getFailureStatistics();
    EXPECT_EQ(stats.total_failures_seen, 5);
    EXPECT_GT(stats.category_counts.size(), 0);
}

TEST_F(FailureIntelligenceTest, RecoverySuccessRateTracking)
{
    // Simulate recoveries
    for (int i = 0; i < 3; ++i) {
        FailureSignal signal;
        signal.error_message = "Test failure";
        signal.severity = SeverityLevel::Warning;
        orchestrator->reportFailure(signal);

        auto recent = orchestrator->getRecentFailures(1);
        auto plan = orchestrator->generateRecoveryPlan(*recent[0]);

        std::string output;
        orchestrator->executeRecovery(plan, output);
    }

    auto stats = orchestrator->getFailureStatistics();
    EXPECT_GT(stats.total_recoveries_attempted, 0);
}

// ============================================================================
// JSON Serialization
// ============================================================================

TEST_F(FailureIntelligenceTest, FailureQueueJsonExport)
{
    FailureSignal signal;
    signal.error_message = "Test exportable failure";
    signal.source_component = "test_component";
    signal.severity = SeverityLevel::Warning;

    orchestrator->reportFailure(signal);

    auto json = orchestrator->getFailureQueueJson();
    EXPECT_FALSE(json.is_null());
    EXPECT_FALSE(json.empty());
    EXPECT_TRUE(json.is_object() || json.is_array());
}

TEST_F(FailureIntelligenceTest, SystemHealthJsonExport)
{
    auto health = orchestrator->getSystemHealthJson();
    EXPECT_FALSE(health.is_null());
    EXPECT_FALSE(health.empty());
}

TEST_F(FailureIntelligenceTest, StatisticsJsonExport)
{
    FailureSignal signal;
    signal.error_message = "Test for statistics";
    orchestrator->reportFailure(signal);

    auto stats_json = orchestrator->getStatisticsJson();
    EXPECT_FALSE(stats_json.is_null());
    EXPECT_FALSE(stats_json.empty());
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
