// ============================================================================
// test_deployment_orchestrator.cpp — Smoke test for Deployment Orchestrator
// Validates deployment pipeline, health checks, and rollback.
// ============================================================================
#include "../src/deployment/deployment_orchestrator.hpp"
#include <iostream>
#include <cassert>
#include <chrono>

using namespace RawrXD::Deployment;

// ============================================================================
// Test Helpers
// ============================================================================
static bool g_testsPassed = true;
static int g_testCount = 0;
static int g_passCount = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running: " #name "... "; \
    g_testCount++; \
    try { \
        test_##name(); \
        std::cout << "PASSED\n"; \
        g_passCount++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << "\n"; \
        g_testsPassed = false; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        throw std::runtime_error("Assertion failed: " #expr); \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))

// ============================================================================
// Tests
// ============================================================================

TEST(orchestrator_initialization) {
    DeploymentOrchestrator orchestrator(nullptr);
    ASSERT_TRUE(orchestrator.Initialize());
    ASSERT_TRUE(orchestrator.IsInitialized());
    orchestrator.Shutdown();
}

TEST(deployment_config_validation) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    DeploymentConfig config;
    config.deploymentId = "test-001";
    config.applicationName = "RawrXD";
    config.version = "1.0.0";
    config.target = DeploymentTarget::Local;
    config.strategy = DeploymentStrategy::Rolling;
    config.artifactPath = "build/bin/RawrXD-Win32IDE.exe";
    
    auto result = orchestrator.Deploy(config);
    
    // Should fail at build stage (no actual build)
    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.finalStage, DeploymentStage::Building);
    orchestrator.Shutdown();
}

TEST(deployment_target_types) {
    ASSERT_EQ(DeploymentTargetToString(DeploymentTarget::Local), "local");
    ASSERT_EQ(DeploymentTargetToString(DeploymentTarget::RemoteSSH), "remote_ssh");
    ASSERT_EQ(DeploymentTargetToString(DeploymentTarget::Docker), "docker");
    ASSERT_EQ(DeploymentTargetToString(DeploymentTarget::Kubernetes), "kubernetes");
    
    ASSERT_EQ(StringToDeploymentTarget("local"), DeploymentTarget::Local);
    ASSERT_EQ(StringToDeploymentTarget("docker"), DeploymentTarget::Docker);
    ASSERT_EQ(StringToDeploymentTarget("unknown"), DeploymentTarget::Unknown);
}

TEST(deployment_strategy_types) {
    ASSERT_EQ(DeploymentStrategyToString(DeploymentStrategy::Rolling), "rolling");
    ASSERT_EQ(DeploymentStrategyToString(DeploymentStrategy::BlueGreen), "blue_green");
    ASSERT_EQ(DeploymentStrategyToString(DeploymentStrategy::Canary), "canary");
}

TEST(deployment_stage_types) {
    ASSERT_EQ(DeploymentStageToString(DeploymentStage::Validating), "validating");
    ASSERT_EQ(DeploymentStageToString(DeploymentStage::Building), "building");
    ASSERT_EQ(DeploymentStageToString(DeploymentStage::Deploying), "deploying");
}

TEST(health_check) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    auto result = orchestrator.CheckHealth("/health", "localhost", 80);
    
    // Should be healthy with endpoint
    ASSERT_TRUE(result.healthy);
    ASSERT_EQ(result.status, HealthStatus::Healthy);
    ASSERT_EQ(result.statusCode, 200);
    orchestrator.Shutdown();
}

TEST(health_check_empty_endpoint) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    auto result = orchestrator.CheckHealth("", "localhost", 80);
    
    // Should be unhealthy without endpoint
    ASSERT_FALSE(result.healthy);
    ASSERT_EQ(result.status, HealthStatus::Unknown);
    orchestrator.Shutdown();
}

TEST(rollback) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    auto result = orchestrator.Rollback("test-001");
    
    ASSERT_TRUE(result.rollbackInitiated);
    ASSERT_EQ(result.finalStage, DeploymentStage::RollingBack);
    ASSERT_GT(result.rollbackDuration.count(), 0);
    orchestrator.Shutdown();
}

TEST(metrics_tracking) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    // Initial metrics should be zero
    auto metrics = orchestrator.GetMetrics();
    ASSERT_EQ(metrics.totalDeployments, 0);
    ASSERT_EQ(metrics.successfulDeployments, 0);
    ASSERT_EQ(metrics.failedDeployments, 0);
    
    // Deploy (will fail at build stage)
    DeploymentConfig config;
    config.deploymentId = "test-metrics";
    config.applicationName = "TestApp";
    config.version = "1.0.0";
    config.target = DeploymentTarget::Local;
    config.strategy = DeploymentStrategy::Rolling;
    config.artifactPath = "test.exe";
    
    orchestrator.Deploy(config);
    
    // Metrics should be updated
    metrics = orchestrator.GetMetrics();
    ASSERT_EQ(metrics.totalDeployments, 1);
    ASSERT_EQ(metrics.failedDeployments, 1);
    ASSERT_EQ(metrics.successRate, 0.0f);
    orchestrator.Shutdown();
}

TEST(deployment_history) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    // Initial history should be empty
    auto history = orchestrator.GetDeploymentHistory();
    ASSERT_EQ(history.size(), 0);
    
    // Deploy (will fail)
    DeploymentConfig config;
    config.deploymentId = "test-history";
    config.applicationName = "TestApp";
    config.version = "1.0.0";
    config.target = DeploymentTarget::Local;
    config.strategy = DeploymentStrategy::Rolling;
    config.artifactPath = "test.exe";
    
    orchestrator.Deploy(config);
    
    // History should have one entry
    history = orchestrator.GetDeploymentHistory();
    ASSERT_EQ(history.size(), 1);
    ASSERT_EQ(history[0].deploymentId, "test-history");
    orchestrator.Shutdown();
}

TEST(auto_rollback_enabled) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    ASSERT_TRUE(orchestrator.GetAutoRollback());
    
    orchestrator.SetAutoRollback(false);
    ASSERT_FALSE(orchestrator.GetAutoRollback());
    orchestrator.Shutdown();
}

TEST(parallel_stages) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    ASSERT_FALSE(orchestrator.GetParallelStages());
    
    orchestrator.SetParallelStages(true);
    ASSERT_TRUE(orchestrator.GetParallelStages());
    orchestrator.Shutdown();
}

TEST(monitoring_start_stop) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    // Start monitoring
    orchestrator.StartMonitoring("test-deploy", "/health");
    
    // Stop monitoring
    orchestrator.StopMonitoring("test-deploy");
    
    // Should not crash
    ASSERT_TRUE(true);
    orchestrator.Shutdown();
}

TEST(deployment_async) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    DeploymentConfig config;
    config.deploymentId = "test-async";
    config.applicationName = "TestApp";
    config.version = "1.0.0";
    config.target = DeploymentTarget::Local;
    config.strategy = DeploymentStrategy::Rolling;
    config.artifactPath = "test.exe";
    
    auto future = orchestrator.DeployAsync(config);
    auto result = future.get();
    
    ASSERT_FALSE(result.success);  // Will fail at build stage
    ASSERT_EQ(result.deploymentId, "test-async");
    orchestrator.Shutdown();
}

TEST(ai_optimization_without_client) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    DeploymentConfig config;
    config.applicationName = "TestApp";
    config.target = DeploymentTarget::Local;
    config.strategy = DeploymentStrategy::Rolling;
    
    // Without inference client, should return original config
    auto optimized = orchestrator.OptimizeDeployment(config);
    ASSERT_EQ(optimized.applicationName, config.applicationName);
    ASSERT_EQ(optimized.target, config.target);
    orchestrator.Shutdown();
}

TEST(ai_risk_prediction_without_client) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    DeploymentConfig config;
    config.applicationName = "TestApp";
    config.target = DeploymentTarget::Local;
    config.strategy = DeploymentStrategy::Rolling;
    
    // Without inference client, should return fallback message
    auto risk = orchestrator.PredictDeploymentRisk(config);
    ASSERT_TRUE(risk.find("Unable to predict risk") != std::string::npos);
    orchestrator.Shutdown();
}

TEST(ai_strategy_suggestion_without_client) {
    DeploymentOrchestrator orchestrator(nullptr);
    orchestrator.Initialize();
    
    DeploymentConfig config;
    config.applicationName = "TestApp";
    config.target = DeploymentTarget::Local;
    config.strategy = DeploymentStrategy::Rolling;
    
    // Without inference client, should return default strategies
    auto strategies = orchestrator.SuggestDeploymentStrategy(config);
    ASSERT_GT(strategies.size(), 0);
    orchestrator.Shutdown();
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "Deployment Orchestrator Tests\n";
    std::cout << "========================================\n\n";
    
    RUN_TEST(orchestrator_initialization);
    RUN_TEST(deployment_config_validation);
    RUN_TEST(deployment_target_types);
    RUN_TEST(deployment_strategy_types);
    RUN_TEST(deployment_stage_types);
    RUN_TEST(health_check);
    RUN_TEST(health_check_empty_endpoint);
    RUN_TEST(rollback);
    RUN_TEST(metrics_tracking);
    RUN_TEST(deployment_history);
    RUN_TEST(auto_rollback_enabled);
    RUN_TEST(parallel_stages);
    RUN_TEST(monitoring_start_stop);
    RUN_TEST(deployment_async);
    RUN_TEST(ai_optimization_without_client);
    RUN_TEST(ai_risk_prediction_without_client);
    RUN_TEST(ai_strategy_suggestion_without_client);
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << g_passCount << "/" << g_testCount << " passed\n";
    std::cout << "========================================\n";
    
    return g_testsPassed ? 0 : 1;
}
