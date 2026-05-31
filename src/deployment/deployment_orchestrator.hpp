// ============================================================================
// deployment_orchestrator.hpp — AI-Powered Deployment Orchestrator
// Automates build, test, package, deploy, verify, and monitor pipeline.
// Uses SovereignInferenceClient for intelligent deployment decisions.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured DeploymentResult returns only.
// ============================================================================
#pragma once

#ifndef RAWRXD_DEPLOYMENT_ORCHESTRATOR_HPP
#define RAWRXD_DEPLOYMENT_ORCHESTRATOR_HPP

#include "deployment_types.hpp"
#include "../agentic/SovereignInferenceClient.h"
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <future>

namespace RawrXD {
namespace Deployment {

// ============================================================================
// Deployment Stage Handler — Interface for pipeline stages
// ============================================================================
class DeploymentStageHandler {
public:
    virtual ~DeploymentStageHandler() = default;
    virtual StageResult Execute(const DeploymentConfig& config) = 0;
    virtual DeploymentStage GetStage() const = 0;
};

// ============================================================================
// Deployment Orchestrator — Main deployment engine
// ============================================================================
class DeploymentOrchestrator {
public:
    explicit DeploymentOrchestrator(
        std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> inferenceClient = nullptr);
    ~DeploymentOrchestrator();

    // Initialize with configuration
    bool Initialize(const std::string& configPath = "");
    void Shutdown();

    // Core deployment API
    DeploymentResult Deploy(const DeploymentConfig& config);
    
    // Async deployment
    std::future<DeploymentResult> DeployAsync(const DeploymentConfig& config);
    
    // Stage-by-stage deployment (for UI progress)
    StageResult ExecuteStage(const DeploymentConfig& config, DeploymentStage stage);
    
    // Health checking
    HealthCheckResult CheckHealth(const std::string& endpoint, 
                                 const std::string& host = "localhost",
                                 uint32_t port = 80);
    
    // Rollback
    DeploymentResult Rollback(const std::string& deploymentId);
    
    // Monitoring
    void StartMonitoring(const std::string& deploymentId, 
                        const std::string& endpoint);
    void StopMonitoring(const std::string& deploymentId);
    
    // AI-powered deployment optimization
    DeploymentConfig OptimizeDeployment(const DeploymentConfig& config);
    std::string PredictDeploymentRisk(const DeploymentConfig& config);
    std::vector<std::string> SuggestDeploymentStrategy(const DeploymentConfig& config);
    
    // Metrics
    DeploymentMetrics GetMetrics() const;
    std::vector<DeploymentResult> GetDeploymentHistory() const;
    
    // Configuration
    void SetAutoRollback(bool enabled) { m_autoRollback = enabled; }
    bool GetAutoRollback() const { return m_autoRollback; }
    
    void SetParallelStages(bool enabled) { m_parallelStages = enabled; }
    bool GetParallelStages() const { return m_parallelStages; }
    
    bool IsInitialized() const { return m_initialized; }
    bool IsDeploying() const { return m_isDeploying; }

    // Callbacks
    using StageCallback = std::function<void(DeploymentStage, const StageResult&)>;
    using HealthCallback = std::function<void(const HealthCheckResult&)>;
    using CompletionCallback = std::function<void(const DeploymentResult&)>;
    
    void SetStageCallback(StageCallback callback) { m_stageCallback = callback; }
    void SetHealthCallback(HealthCallback callback) { m_healthCallback = callback; }
    void SetCompletionCallback(CompletionCallback callback) { m_completionCallback = callback; }

private:
    // Stage handlers
    StageResult ValidateConfig(const DeploymentConfig& config);
    StageResult BuildArtifacts(const DeploymentConfig& config);
    StageResult RunTests(const DeploymentConfig& config);
    StageResult PackageArtifacts(const DeploymentConfig& config);
    StageResult DeployToTarget(const DeploymentConfig& config);
    StageResult VerifyDeployment(const DeploymentConfig& config);
    StageResult MonitorHealth(const DeploymentConfig& config);
    
    // AI-powered functions
    DeploymentConfig AIOptimizeConfig(const DeploymentConfig& config);
    std::string AIPredictRisk(const DeploymentConfig& config);
    std::vector<std::string> AISuggestStrategy(const DeploymentConfig& config);
    
    // Utility functions
    bool ExecuteCommand(const std::string& command, std::vector<std::string>& output);
    bool CopyFile(const std::string& source, const std::string& destination);
    bool CreatePackage(const std::string& source, const std::string& destination, 
                      const std::string& format);
    
    // Monitoring
    void MonitorLoop(const std::string& deploymentId, const std::string& endpoint);
    
    std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> m_inferenceClient;
    std::vector<std::unique_ptr<DeploymentStageHandler>> m_stageHandlers;
    std::vector<DeploymentResult> m_deploymentHistory;
    DeploymentMetrics m_metrics;
    
    bool m_initialized = false;
    bool m_isDeploying = false;
    bool m_autoRollback = true;
    bool m_parallelStages = false;
    
    StageCallback m_stageCallback;
    HealthCallback m_healthCallback;
    CompletionCallback m_completionCallback;
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::thread> m_monitorThreads;
    std::unordered_map<std::string, bool> m_monitorStopFlags;
};

} // namespace Deployment
} // namespace RawrXD

#endif // RAWRXD_DEPLOYMENT_ORCHESTRATOR_HPP
