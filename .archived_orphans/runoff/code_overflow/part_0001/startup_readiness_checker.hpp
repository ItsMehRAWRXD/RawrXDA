#pragma once

/**
 * @file startup_readiness_checker.hpp
 * @brief Stub: startup readiness checker types (full impl in startup_readiness_checker.cpp).
 */

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <cstdint>

class UnifiedHotpatchManager;

struct HealthCheckResult {
    bool success = false;
    std::string message;
    int64_t latencyMs = 0;
};

struct CheckEntry {
    bool success = false;
    int64_t latencyMs = 0;
    std::string message;
    int attemptCount = 0;
};

struct AgentReadinessReport {
    int64_t totalLatency = 0;
    bool overallReady = false;
    std::vector<std::string> failures;
    std::vector<std::string> warnings;
    std::map<std::string, CheckEntry> checks;
    // Qt compatibility: constBegin/constEnd for iteration
    auto constBegin() const { return checks.begin(); }
    auto constEnd() const { return checks.end(); }
};

class StartupReadinessChecker {
public:
    StartupReadinessChecker();
    ~StartupReadinessChecker();
    void runChecks();
    HealthCheckResult checkLLMEndpoint(const std::string& backend, const std::string& endpoint, const std::string& apiKey);
    HealthCheckResult checkGGUFServer(uint16_t port);
    HealthCheckResult checkHotpatchManager(UnifiedHotpatchManager* manager);
    HealthCheckResult checkProjectRoot(const std::string& projectPath);
    HealthCheckResult checkEnvironmentVariables();
    HealthCheckResult checkNetworkConnectivity();
    HealthCheckResult checkDiskSpace();
    HealthCheckResult checkModelCache();
    static std::string formatBytes(int64_t bytes);
    HealthCheckResult runWithRetry(const std::string& subsystem, std::function<HealthCheckResult()> checkFunc);
    void onNetworkReplyFinished(void* reply);
    void onCheckTimeout();
    void logReadinessMetrics(const AgentReadinessReport& report);
private:
    void* m_networkManager = nullptr;
    int m_maxRetries = 3;
    int m_timeoutMs = 5000;
    int m_backoffMs = 500;
    int m_completedChecks = 0;
    int m_totalChecks = 8;
    std::map<std::string, int> m_retryCount;
    AgentReadinessReport m_lastReport;
    std::vector<void*> m_checkTimers;
    std::map<std::string, void*> m_runningChecks;  // QFuture or similar
    void* m_totalTimer = nullptr;
};

class StartupReadinessDialog {
public:
    explicit StartupReadinessDialog(void* parent = nullptr);
    ~StartupReadinessDialog();
    void setupUI();
    void onReadinessComplete(const AgentReadinessReport& report);
private:
    void* m_mainLayout = nullptr;
    StartupReadinessChecker* m_checker = nullptr;
    UnifiedHotpatchManager* m_hotpatchManager = nullptr;
    bool m_checksPassed = false;
};
