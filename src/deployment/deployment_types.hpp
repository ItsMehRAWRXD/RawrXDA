// ============================================================================
// deployment_types.hpp — Deployment Orchestrator Type System
// Defines deployment configurations, strategies, and result types.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured results only.
// ============================================================================
#pragma once

#ifndef RAWRXD_DEPLOYMENT_TYPES_HPP
#define RAWRXD_DEPLOYMENT_TYPES_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>

namespace RawrXD {
namespace Deployment {

// ============================================================================
// Deployment Target — Where to deploy
// ============================================================================
enum class DeploymentTarget : uint32_t {
    Unknown = 0,
    Local = 1,           // Local machine
    RemoteSSH = 2,       // Remote via SSH
    Docker = 3,          // Docker container
    Kubernetes = 4,      // K8s cluster
    CloudVM = 5,         // Cloud VM (AWS/Azure/GCP)
    BareMetal = 6,       // Physical server
    EdgeDevice = 7,      // IoT/Edge device
    AirGapped = 8,       // Air-gapped network
};

std::string DeploymentTargetToString(DeploymentTarget target);
DeploymentTarget StringToDeploymentTarget(const std::string& str);

// ============================================================================
// Deployment Strategy — How to deploy
// ============================================================================
enum class DeploymentStrategy : uint32_t {
    Unknown = 0,
    Rolling = 1,         // Rolling update
    BlueGreen = 2,       // Blue-green deployment
    Canary = 3,          // Canary release
    Recreate = 4,        // Stop and recreate
    Shadow = 5,          // Shadow deployment
    ABB = 6,             // A/B testing
    HotSwap = 7,         // Hot swap (zero downtime)
};

std::string DeploymentStrategyToString(DeploymentStrategy strategy);

// ============================================================================
// Deployment Stage — Pipeline stages
// ============================================================================
enum class DeploymentStage : uint32_t {
    Idle = 0,
    Validating = 1,      // Validate configuration
    Building = 2,        // Build artifacts
    Testing = 3,         // Run tests
    Packaging = 4,       // Package for deployment
    Deploying = 5,       // Deploy to target
    Verifying = 6,       // Verify deployment
    Monitoring = 7,      // Monitor health
    Completed = 8,       // Deployment complete
    Failed = 9,          // Deployment failed
    RollingBack = 10,    // Rolling back
};

std::string DeploymentStageToString(DeploymentStage stage);

// ============================================================================
// Health Status — Deployment health
// ============================================================================
enum class HealthStatus : uint32_t {
    Unknown = 0,
    Healthy = 1,
    Degraded = 2,
    Unhealthy = 3,
    Critical = 4,
};

// ============================================================================
// Deployment Configuration
// ============================================================================
struct DeploymentConfig {
    std::string deploymentId;
    std::string applicationName;
    std::string version;
    DeploymentTarget target;
    DeploymentStrategy strategy;
    
    // Target configuration
    std::string targetHost;
    uint32_t targetPort = 22;
    std::string targetPath;
    std::string credentialsId;
    
    // Strategy configuration
    uint32_t canaryPercentage = 10;      // For canary strategy
    uint32_t rollingBatchSize = 1;     // For rolling strategy
    std::chrono::seconds healthCheckInterval{30};
    std::chrono::seconds rollbackTimeout{300};
    
    // Build configuration
    std::string buildCommand = "cmake --build build --target RawrXD-Win32IDE";
    std::string artifactPath = "build/bin/RawrXD-Win32IDE.exe";
    std::string packageFormat = "zip";  // zip, tar.gz, docker
    
    // Pre/post deployment hooks
    std::vector<std::string> preDeployCommands;
    std::vector<std::string> postDeployCommands;
    
    // Health check configuration
    std::string healthCheckEndpoint = "/health";
    uint32_t healthCheckRetries = 3;
    std::chrono::seconds healthCheckTimeout{10};
    
    // Notification configuration
    bool notifyOnSuccess = true;
    bool notifyOnFailure = true;
    std::vector<std::string> notificationChannels;  // slack, email, webhook
};

// ============================================================================
// Deployment Result
// ============================================================================
struct DeploymentResult {
    bool success = false;
    std::string deploymentId;
    std::string errorMessage;
    
    DeploymentStage finalStage = DeploymentStage::Idle;
    HealthStatus finalHealth = HealthStatus::Unknown;
    
    // Timing
    std::chrono::milliseconds totalDuration{0};
    std::chrono::milliseconds buildDuration{0};
    std::chrono::milliseconds deployDuration{0};
    std::chrono::milliseconds verifyDuration{0};
    
    // Metrics
    uint32_t artifactsDeployed = 0;
    uint32_t servicesRestarted = 0;
    uint32_t healthCheckFailures = 0;
    
    // Logs
    std::vector<std::string> deploymentLogs;
    std::vector<std::string> errorLogs;
    
    // Rollback info
    bool rollbackInitiated = false;
    std::string rollbackReason;
    std::chrono::milliseconds rollbackDuration{0};
};

// ============================================================================
// Deployment Stage Result
// ============================================================================
struct StageResult {
    bool success = false;
    DeploymentStage stage;
    std::string message;
    std::chrono::milliseconds duration{0};
    std::vector<std::string> logs;
};

// ============================================================================
// Health Check Result
// ============================================================================
struct HealthCheckResult {
    bool healthy = false;
    HealthStatus status = HealthStatus::Unknown;
    std::string endpoint;
    uint32_t responseTimeMs = 0;
    std::string responseBody;
    uint32_t statusCode = 0;
    std::string errorMessage;
};

// ============================================================================
// Rollback Configuration
// ============================================================================
struct RollbackConfig {
    bool autoRollback = true;
    uint32_t maxHealthCheckFailures = 3;
    std::chrono::seconds rollbackTimeout{300};
    std::string previousVersion;
    std::string backupPath;
};

// ============================================================================
// Deployment Metrics
// ============================================================================
struct DeploymentMetrics {
    uint32_t totalDeployments = 0;
    uint32_t successfulDeployments = 0;
    uint32_t failedDeployments = 0;
    uint32_t rollbackCount = 0;
    
    float successRate = 0.0f;
    float averageDeployTimeMs = 0.0f;
    float averageRollbackTimeMs = 0.0f;
    
    std::chrono::system_clock::time_point lastDeployment;
    std::chrono::system_clock::time_point lastFailure;
};

} // namespace Deployment
} // namespace RawrXD

#endif // RAWRXD_DEPLOYMENT_TYPES_HPP
