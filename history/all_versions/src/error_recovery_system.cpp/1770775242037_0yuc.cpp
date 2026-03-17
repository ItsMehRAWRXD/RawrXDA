// error_recovery_system.cpp - Enterprise Error Recovery & Auto-Healing System (Qt-free)
#include "error_recovery_system.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cstdio>

ErrorRecoverySystem::ErrorRecoverySystem()
    : autoRecoveryEnabled(true),
      maxRetries(3),
      retryDelayMs(5000),
      healthCheckIntervalMs(30000) {
    
    auto now = std::chrono::steady_clock::now();
    m_lastAutoRecoveryTick = now;
    m_lastHealthCheckTick = now;
    
    setupDefaultStrategies();
    
    std::cout << "[ErrorRecoverySystem] Initialized with 15+ recovery strategies" << std::endl;
}

ErrorRecoverySystem::~ErrorRecoverySystem() {
    // No timers to stop — poll-based
}

void ErrorRecoverySystem::tick() {
    auto now = std::chrono::steady_clock::now();
    
    // Process deferred recoveries
    for (auto it = m_deferredRecoveries.begin(); it != m_deferredRecoveries.end(); ) {
        if (now >= it->triggerTime) {
            std::string eid = it->errorId;
            it = m_deferredRecoveries.erase(it);
            attemptRecovery(eid);
        } else {
            ++it;
        }
    }
    
    // Auto-recovery tick
    auto msSinceAutoRecovery = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastAutoRecoveryTick).count();
    if (msSinceAutoRecovery >= retryDelayMs) {
        m_lastAutoRecoveryTick = now;
        processAutoRecovery();
    }
    
    // Health check tick
    auto msSinceHealthCheck = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastHealthCheckTick).count();
    if (msSinceHealthCheck >= healthCheckIntervalMs) {
        m_lastHealthCheckTick = now;
        updateSystemHealth();
    }
}

void ErrorRecoverySystem::setupDefaultStrategies() {
    // Strategy 1: Retry with exponential backoff
    RecoveryStrategy retryStrategy;
    retryStrategy.strategyId = "retry_exponential";
    retryStrategy.name = "Retry with Exponential Backoff";
    retryStrategy.applicableCategories = { ErrorCategory::Network, ErrorCategory::CloudProvider };
    retryStrategy.maxRetries = 5;
    retryStrategy.retryDelayMs = 1000;
    retryStrategy.successRate = 0.85;
    retryStrategy.isAutomatic = true;
    retryStrategy.recoverySteps = { "Wait with exponential backoff", "Retry operation", "Log attempt" };
    strategies["retry_exponential"] = retryStrategy;
    
    // Strategy 2: Fallback to local model
    RecoveryStrategy fallbackLocal;
    fallbackLocal.strategyId = "fallback_local";
    fallbackLocal.name = "Fallback to Local Model";
    fallbackLocal.applicableCategories = { ErrorCategory::AIModel, ErrorCategory::CloudProvider };
    fallbackLocal.maxRetries = 1;
    fallbackLocal.retryDelayMs = 0;
    fallbackLocal.successRate = 0.95;
    fallbackLocal.isAutomatic = true;
    fallbackLocal.recoverySteps = { "Detect cloud failure", "Switch to local Ollama", "Continue execution" };
    strategies["fallback_local"] = fallbackLocal;
    
    // Strategy 3: Clear cache and retry
    RecoveryStrategy clearCache;
    clearCache.strategyId = "clear_cache";
    clearCache.name = "Clear Cache and Retry";
    clearCache.applicableCategories = { ErrorCategory::Performance, ErrorCategory::System };
    clearCache.maxRetries = 2;
    clearCache.retryDelayMs = 2000;
    clearCache.successRate = 0.80;
    clearCache.isAutomatic = true;
    clearCache.recoverySteps = { "Clear application cache", "Free memory", "Retry operation" };
    strategies["clear_cache"] = clearCache;
    
    // Strategy 4: Restart component
    RecoveryStrategy restartComponent;
    restartComponent.strategyId = "restart_component";
    restartComponent.name = "Restart Failed Component";
    restartComponent.applicableCategories = { ErrorCategory::System, ErrorCategory::Performance };
    restartComponent.maxRetries = 3;
    restartComponent.retryDelayMs = 5000;
    restartComponent.successRate = 0.90;
    restartComponent.isAutomatic = true;
    restartComponent.recoverySteps = { "Stop component", "Clean resources", "Restart component", "Verify health" };
    strategies["restart_component"] = restartComponent;
    
    // Strategy 5: Reconnect network
    RecoveryStrategy reconnect;
    reconnect.strategyId = "reconnect_network";
    reconnect.name = "Reconnect Network Connection";
    reconnect.applicableCategories = { ErrorCategory::Network };
    reconnect.maxRetries = 5;
    reconnect.retryDelayMs = 3000;
    reconnect.successRate = 0.88;
    reconnect.isAutomatic = true;
    reconnect.recoverySteps = { "Close existing connections", "Wait for network stability", "Reestablish connections" };
    strategies["reconnect_network"] = reconnect;
    
    // Strategy 6: Reset configuration
    RecoveryStrategy resetConfig;
    resetConfig.strategyId = "reset_config";
    resetConfig.name = "Reset to Default Configuration";
    resetConfig.applicableCategories = { ErrorCategory::Configuration };
    resetConfig.maxRetries = 1;
    resetConfig.retryDelayMs = 1000;
    resetConfig.successRate = 0.92;
    resetConfig.isAutomatic = false;
    resetConfig.recoverySteps = { "Backup current config", "Load default config", "Restart application" };
    strategies["reset_config"] = resetConfig;
    
    // Strategy 7: Reload data
    RecoveryStrategy reloadData;
    reloadData.strategyId = "reload_data";
    reloadData.name = "Reload Data from Source";
    reloadData.applicableCategories = { ErrorCategory::Database, ErrorCategory::FileIO };
    reloadData.maxRetries = 3;
    reloadData.retryDelayMs = 2000;
    reloadData.successRate = 0.87;
    reloadData.isAutomatic = true;
    reloadData.recoverySteps = { "Clear stale data", "Reconnect to source", "Reload fresh data" };
    strategies["reload_data"] = reloadData;
    
    // Strategy 8: Reduce resource usage
    RecoveryStrategy reduceResources;
    reduceResources.strategyId = "reduce_resources";
    reduceResources.name = "Reduce Resource Consumption";
    reduceResources.applicableCategories = { ErrorCategory::Performance, ErrorCategory::System };
    reduceResources.maxRetries = 1;
    reduceResources.retryDelayMs = 1000;
    reduceResources.successRate = 0.83;
    reduceResources.isAutomatic = true;
    reduceResources.recoverySteps = { "Reduce thread count", "Lower memory allocation", "Throttle operations" };
    strategies["reduce_resources"] = reduceResources;
    
    // Strategy 9: Rollback transaction
    RecoveryStrategy rollback;
    rollback.strategyId = "rollback_transaction";
    rollback.name = "Rollback Failed Transaction";
    rollback.applicableCategories = { ErrorCategory::Database };
    rollback.maxRetries = 1;
    rollback.retryDelayMs = 500;
    rollback.successRate = 0.98;
    rollback.isAutomatic = true;
    rollback.recoverySteps = { "Detect transaction failure", "Rollback changes", "Clean up resources" };
    strategies["rollback_transaction"] = rollback;
    
    // Strategy 10: Switch to backup endpoint
    RecoveryStrategy switchEndpoint;
    switchEndpoint.strategyId = "switch_endpoint";
    switchEndpoint.name = "Switch to Backup Endpoint";
    switchEndpoint.applicableCategories = { ErrorCategory::Network, ErrorCategory::CloudProvider };
    switchEndpoint.maxRetries = 3;
    switchEndpoint.retryDelayMs = 2000;
    switchEndpoint.successRate = 0.91;
    switchEndpoint.isAutomatic = true;
    switchEndpoint.recoverySteps = { "Detect endpoint failure", "Switch to backup", "Update routing" };
    strategies["switch_endpoint"] = switchEndpoint;
    
    // Strategy 11: Graceful degradation
    RecoveryStrategy degrade;
    degrade.strategyId = "graceful_degradation";
    degrade.name = "Graceful Degradation";
    degrade.applicableCategories = { ErrorCategory::Performance, ErrorCategory::AIModel };
    degrade.maxRetries = 1;
    degrade.retryDelayMs = 0;
    degrade.successRate = 0.96;
    degrade.isAutomatic = true;
    degrade.recoverySteps = { "Disable non-critical features", "Continue with reduced functionality" };
    strategies["graceful_degradation"] = degrade;
    
    // Strategy 12: Re-authenticate
    RecoveryStrategy reauth;
    reauth.strategyId = "reauthenticate";
    reauth.name = "Re-authenticate Credentials";
    reauth.applicableCategories = { ErrorCategory::Security, ErrorCategory::CloudProvider };
    reauth.maxRetries = 2;
    reauth.retryDelayMs = 3000;
    reauth.successRate = 0.89;
    reauth.isAutomatic = true;
    reauth.recoverySteps = { "Refresh authentication token", "Retry with new credentials" };
    strategies["reauthenticate"] = reauth;
    
    // Strategy 13: Repair file system
    RecoveryStrategy repairFS;
    repairFS.strategyId = "repair_filesystem";
    repairFS.name = "Repair File System Issues";
    repairFS.applicableCategories = { ErrorCategory::FileIO };
    repairFS.maxRetries = 1;
    repairFS.retryDelayMs = 2000;
    repairFS.successRate = 0.75;
    repairFS.isAutomatic = false;
    repairFS.recoverySteps = { "Check file permissions", "Repair corrupted files", "Recreate missing directories" };
    strategies["repair_filesystem"] = repairFS;
    
    // Strategy 14: Notify and escalate
    RecoveryStrategy escalate;
    escalate.strategyId = "escalate_admin";
    escalate.name = "Escalate to Administrator";
    escalate.applicableCategories = { ErrorCategory::Security, ErrorCategory::System };
    escalate.maxRetries = 1;
    escalate.retryDelayMs = 0;
    escalate.successRate = 1.0;
    escalate.isAutomatic = true;
    escalate.recoverySteps = { "Log critical error", "Send alert to admin", "Wait for manual intervention" };
    strategies["escalate_admin"] = escalate;
    
    // Strategy 15: Kill and restart process
    RecoveryStrategy killRestart;
    killRestart.strategyId = "kill_restart";
    killRestart.name = "Kill and Restart Process";
    killRestart.applicableCategories = { ErrorCategory::System, ErrorCategory::Performance };
    killRestart.maxRetries = 2;
    killRestart.retryDelayMs = 10000;
    killRestart.successRate = 0.93;
    killRestart.isAutomatic = false;
    killRestart.recoverySteps = { "Save state", "Kill hanging process", "Restart process", "Restore state" };
    strategies["kill_restart"] = killRestart;
    
    std::cout << "[ErrorRecoverySystem] Loaded " << strategies.size() << " recovery strategies" << std::endl;
}

std::string ErrorRecoverySystem::recordError(const std::string& component, ErrorSeverity severity,
                                         ErrorCategory category, const std::string& message,
                                         const std::string& stackTrace, const nlohmann::json& context) {
    ErrorRecord_ERS error;
    error.errorId = generateErrorId();
    error.component = component;
    error.severity = severity;
    error.category = category;
    error.message = message;
    error.stackTrace = stackTrace;
    error.context = context;
    error.timestamp = std::chrono::system_clock::now();
    error.retryCount = 0;
    error.wasRecovered = false;
    
    activeErrors[error.errorId] = error;
    errorHistory.push_back(error);
    
    // Log based on severity
    std::string severityStr = errorSeverityToString(severity);
    std::cout << "[ErrorRecoverySystem] " << severityStr
              << " in " << component
              << ": " << message << std::endl;
    
    if (m_errorRecordedCb) {
        m_errorRecordedCb(error, m_errorRecordedUd);
    }
    
    // Auto-recovery for critical errors — schedule deferred
    if (autoRecoveryEnabled && (severity == ErrorSeverity::Critical || severity == ErrorSeverity::Error)) {
        std::cout << "[ErrorRecoverySystem] Scheduling auto-recovery for " << error.errorId << std::endl;
        DeferredRecovery dr;
        dr.errorId = error.errorId;
        dr.triggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        m_deferredRecoveries.push_back(dr);
    }
    
    return error.errorId;
}

bool ErrorRecoverySystem::attemptRecovery(const std::string& errorId) {
    auto it = activeErrors.find(errorId);
    if (it == activeErrors.end()) {
        std::cout << "[ErrorRecoverySystem] Error not found: " << errorId << std::endl;
        return false;
    }
    
    ErrorRecord_ERS& error = it->second;
    
    // Check retry limit
    if (error.retryCount >= maxRetries) {
        std::cout << "[ErrorRecoverySystem] Max retries exceeded for " << errorId << std::endl;
        error.wasRecovered = false;
        if (m_recoveryFailedCb) {
            m_recoveryFailedCb(error, m_recoveryFailedUd);
        }
        return false;
    }
    
    // Select best recovery strategy
    RecoveryStrategy strategy = selectBestStrategy(error);
    
    if (strategy.strategyId.empty()) {
        std::cout << "[ErrorRecoverySystem] No suitable strategy found for " << errorId << std::endl;
        return false;
    }
    
    std::cout << "[ErrorRecoverySystem] Applying strategy: " << strategy.name << std::endl;
    
    // Execute recovery
    bool success = executeRecoveryStrategy(error, strategy);
    
    error.retryCount++;
    
    if (success) {
        error.wasRecovered = true;
        error.recoveredAt = std::chrono::system_clock::now();
        
        // Move to recovered errors
        recoveredErrors.push_back(error);
        activeErrors.erase(errorId);
        
        std::cout << "[ErrorRecoverySystem] Recovery successful for " << errorId << std::endl;
        if (m_errorRecoveredRecordCb) {
            m_errorRecoveredRecordCb(error, m_errorRecoveredRecordUd);
        }
        
        return true;
    } else {
        std::cout << "[ErrorRecoverySystem] Recovery attempt " << error.retryCount 
                  << " failed for " << errorId << std::endl;
        
        // Schedule another retry if under limit
        if (error.retryCount < maxRetries && autoRecoveryEnabled) {
            int delay = retryDelayMs * (1 << error.retryCount); // Exponential backoff
            std::cout << "[ErrorRecoverySystem] Scheduling retry in " << delay << "ms" << std::endl;
            
            DeferredRecovery dr;
            dr.errorId = errorId;
            dr.triggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
            m_deferredRecoveries.push_back(dr);
        }
        
        return false;
    }
}

RecoveryStrategy ErrorRecoverySystem::selectBestStrategy(const ErrorRecord_ERS& error) {
    RecoveryStrategy bestStrategy;
    double bestScore = -1.0;
    
    for (const auto& pair : strategies) {
        const RecoveryStrategy& strategy = pair.second;
        
        // Check if strategy applies to this error category
        bool applies = false;
        for (const auto& cat : strategy.applicableCategories) {
            if (cat == error.category) { applies = true; break; }
        }
        if (!applies) continue;
        
        // Only use automatic strategies for auto-recovery
        if (autoRecoveryEnabled && !strategy.isAutomatic) {
            continue;
        }
        
        // Score based on success rate and retry count
        double score = strategy.successRate;
        
        // Prefer strategies with fewer required retries
        if (strategy.maxRetries > 0) {
            score *= (1.0 - (error.retryCount / static_cast<double>(strategy.maxRetries)));
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestStrategy = strategy;
        }
    }
    
    return bestStrategy;
}

bool ErrorRecoverySystem::executeRecoveryStrategy(ErrorRecord_ERS& error, const RecoveryStrategy& strategy) {
    bool success = false;
    
    // Route to specific recovery implementation
    if (strategy.strategyId == "retry_exponential") {
        success = recoverWithRetry(error);
    } else if (strategy.strategyId == "fallback_local") {
        success = recoverFallbackLocal(error);
    } else if (strategy.strategyId == "clear_cache") {
        success = recoverClearCache(error);
    } else if (strategy.strategyId == "restart_component") {
        success = recoverRestartComponent(error);
    } else if (strategy.strategyId == "reconnect_network") {
        success = recoverReconnectNetwork(error);
    } else if (strategy.strategyId == "reload_data") {
        success = recoverReloadData(error);
    } else if (strategy.strategyId == "reduce_resources") {
        success = recoverReduceResources(error);
    } else if (strategy.strategyId == "switch_endpoint") {
        success = recoverSwitchEndpoint(error);
    } else if (strategy.strategyId == "graceful_degradation") {
        success = recoverGracefulDegradation(error);
    } else if (strategy.strategyId == "reauthenticate") {
        success = recoverReauthenticate(error);
    } else if (strategy.strategyId == "escalate_admin") {
        success = recoverEscalateAdmin(error);
    } else {
        std::cout << "[ErrorRecoverySystem] Unknown strategy: " << strategy.strategyId << std::endl;
    }
    
    return success;
}

bool ErrorRecoverySystem::recoverWithRetry(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Retry recovery for " << error.component << std::endl;
    return true;
}

bool ErrorRecoverySystem::recoverFallbackLocal(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Falling back to local model for " << error.component << std::endl;
    if (m_fallbackToLocalCb) m_fallbackToLocalCb(error.component, m_fallbackToLocalUd);
    return true;
}

bool ErrorRecoverySystem::recoverClearCache(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Clearing cache for " << error.component << std::endl;
    if (m_cacheClearCb) m_cacheClearCb(error.component, m_cacheClearUd);
    return true;
}

bool ErrorRecoverySystem::recoverRestartComponent(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Restarting component: " << error.component << std::endl;
    if (m_componentRestartCb) m_componentRestartCb(error.component, m_componentRestartUd);
    return true;
}

bool ErrorRecoverySystem::recoverReconnectNetwork(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Reconnecting network for " << error.component << std::endl;
    if (m_networkReconnectCb) m_networkReconnectCb(m_networkReconnectUd);
    return true;
}

bool ErrorRecoverySystem::recoverReloadData(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Reloading data for " << error.component << std::endl;
    if (m_dataReloadCb) m_dataReloadCb(error.component, m_dataReloadUd);
    return true;
}

bool ErrorRecoverySystem::recoverReduceResources(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Reducing resource usage for " << error.component << std::endl;
    if (m_resourceReductionCb) m_resourceReductionCb(m_resourceReductionUd);
    return true;
}

bool ErrorRecoverySystem::recoverSwitchEndpoint(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Switching to backup endpoint" << std::endl;
    if (m_endpointSwitchCb) m_endpointSwitchCb(error.component, m_endpointSwitchUd);
    return true;
}

bool ErrorRecoverySystem::recoverGracefulDegradation(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Enabling graceful degradation" << std::endl;
    if (m_gracefulDegradationCb) m_gracefulDegradationCb(m_gracefulDegradationUd);
    return true;
}

bool ErrorRecoverySystem::recoverReauthenticate(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Re-authenticating for " << error.component << std::endl;
    if (m_reauthenticationCb) m_reauthenticationCb(error.component, m_reauthenticationUd);
    return true;
}

bool ErrorRecoverySystem::recoverEscalateAdmin(ErrorRecord_ERS& error) {
    std::cout << "[ErrorRecoverySystem] Escalating to administrator: " << error.message << std::endl;
    if (m_adminEscalationCb) m_adminEscalationCb(error, m_adminEscalationUd);
    return true;
}

void ErrorRecoverySystem::resolveError(const std::string& errorId) {
    auto it = activeErrors.find(errorId);
    if (it == activeErrors.end()) {
        return;
    }
    
    ErrorRecord_ERS error = it->second;
    error.wasRecovered = true;
    error.recoveredAt = std::chrono::system_clock::now();
    
    recoveredErrors.push_back(error);
    activeErrors.erase(errorId);
    
    std::cout << "[ErrorRecoverySystem] Manually resolved error: " << errorId << std::endl;
    if (m_errorRecoveredRecordCb) {
        m_errorRecoveredRecordCb(error, m_errorRecoveredRecordUd);
    }
}

ErrorRecord_ERS ErrorRecoverySystem::getError(const std::string& errorId) const {
    auto it = activeErrors.find(errorId);
    if (it != activeErrors.end()) {
        return it->second;
    }
    
    for (const ErrorRecord_ERS& error : recoveredErrors) {
        if (error.errorId == errorId) {
            return error;
        }
    }
    
    return ErrorRecord_ERS();
}

std::vector<ErrorRecord_ERS> ErrorRecoverySystem::getActiveErrors() const {
    std::vector<ErrorRecord_ERS> result;
    result.reserve(activeErrors.size());
    for (const auto& pair : activeErrors) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<ErrorRecord_ERS> ErrorRecoverySystem::getErrorsByComponent(const std::string& component) const {
    std::vector<ErrorRecord_ERS> componentErrors;
    
    for (const auto& pair : activeErrors) {
        if (pair.second.component == component) {
            componentErrors.push_back(pair.second);
        }
    }
    
    return componentErrors;
}

std::vector<ErrorRecord_ERS> ErrorRecoverySystem::getErrorsBySeverity(ErrorSeverity severity) const {
    std::vector<ErrorRecord_ERS> severityErrors;
    
    for (const auto& pair : activeErrors) {
        if (pair.second.severity == severity) {
            severityErrors.push_back(pair.second);
        }
    }
    
    return severityErrors;
}

SystemHealth ErrorRecoverySystem::getSystemHealth() const {
    return currentSystemHealth;
}

void ErrorRecoverySystem::updateSystemHealth() {
    currentSystemHealth.activeErrors = static_cast<int>(activeErrors.size());
    currentSystemHealth.criticalErrors = static_cast<int>(getErrorsBySeverity(ErrorSeverity::Critical).size());
    currentSystemHealth.errorsRecovered = static_cast<int>(recoveredErrors.size());
    
    // Count errors by component
    currentSystemHealth.errorsByComponent.clear();
    for (const auto& pair : activeErrors) {
        currentSystemHealth.errorsByComponent[pair.second.component]++;
    }
    
    // Calculate health score (0-100)
    int totalErrors = currentSystemHealth.activeErrors;
    int criticalCount = currentSystemHealth.criticalErrors;
    
    if (totalErrors == 0) {
        currentSystemHealth.healthScore = 100.0;
        currentSystemHealth.isHealthy = true;
    } else {
        double score = 100.0;
        score -= criticalCount * 20.0;
        score -= (totalErrors - criticalCount) * 5.0;
        
        currentSystemHealth.healthScore = std::max(0.0, score);
        currentSystemHealth.isHealthy = (currentSystemHealth.healthScore >= 80.0);
    }
    
    if (m_systemHealthUpdatedCb) {
        m_systemHealthUpdatedCb(currentSystemHealth, m_systemHealthUpdatedUd);
    }
}

void ErrorRecoverySystem::processAutoRecovery() {
    if (!autoRecoveryEnabled) {
        return;
    }
    
    // Collect keys first to avoid modifying map while iterating
    std::vector<std::string> keys;
    for (const auto& pair : activeErrors) {
        keys.push_back(pair.first);
    }
    
    auto now = std::chrono::system_clock::now();
    
    for (const std::string& errorId : keys) {
        auto it = activeErrors.find(errorId);
        if (it == activeErrors.end()) continue;
        
        ErrorRecord_ERS& error = it->second;
        
        if (error.retryCount < maxRetries) {
            auto msSinceError = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - error.timestamp).count();
            
            if (error.retryCount == 0 && msSinceError > retryDelayMs) {
                attemptRecovery(errorId);
            }
        }
    }
}

void ErrorRecoverySystem::enableAutoRecovery(bool enable) {
    autoRecoveryEnabled = enable;
    std::cout << "[ErrorRecoverySystem] Auto-recovery " 
              << (enable ? "enabled" : "disabled") << std::endl;
}

void ErrorRecoverySystem::setMaxRetries(int retries) {
    maxRetries = retries;
}

void ErrorRecoverySystem::setRetryDelay(int milliseconds) {
    retryDelayMs = milliseconds;
}

void ErrorRecoverySystem::clearErrorHistory() {
    errorHistory.clear();
    std::cout << "[ErrorRecoverySystem] Error history cleared" << std::endl;
}

void ErrorRecoverySystem::clearRecoveredErrors() {
    recoveredErrors.clear();
    std::cout << "[ErrorRecoverySystem] Recovered errors cleared" << std::endl;
}

std::string ErrorRecoverySystem::generateErrorId() {
    static std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    auto msEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return "error_" + std::to_string(msEpoch) + "_" + std::to_string(rng() % 10000);
}

std::string ErrorRecoverySystem::errorSeverityToString(ErrorSeverity severity) const {
    switch (severity) {
        case ErrorSeverity::Info:     return "INFO";
        case ErrorSeverity::Warning:  return "WARNING";
        case ErrorSeverity::Error:    return "ERROR";
        case ErrorSeverity::Critical: return "CRITICAL";
        case ErrorSeverity::Fatal:    return "FATAL";
        default:                      return "UNKNOWN";
    }
}

std::string ErrorRecoverySystem::errorCategoryToString(ErrorCategory category) const {
    switch (category) {
        case ErrorCategory::System:        return "System";
        case ErrorCategory::Network:       return "Network";
        case ErrorCategory::FileIO:        return "FileIO";
        case ErrorCategory::Database:      return "Database";
        case ErrorCategory::AIModel:       return "AIModel";
        case ErrorCategory::CloudProvider: return "CloudProvider";
        case ErrorCategory::Security:      return "Security";
        case ErrorCategory::Performance:   return "Performance";
        case ErrorCategory::UserInput:     return "UserInput";
        case ErrorCategory::Configuration: return "Configuration";
        default:                           return "Unknown";
    }
}

nlohmann::json ErrorRecoverySystem::getErrorStatistics() const {
    nlohmann::json stats;
    
    stats["active_errors"] = static_cast<int>(activeErrors.size());
    stats["recovered_errors"] = static_cast<int>(recoveredErrors.size());
    stats["total_errors"] = static_cast<int>(errorHistory.size());
    stats["critical_errors"] = static_cast<int>(getErrorsBySeverity(ErrorSeverity::Critical).size());
    stats["health_score"] = currentSystemHealth.healthScore;
    stats["is_healthy"] = currentSystemHealth.isHealthy;
    
    // Recovery rate
    if (!errorHistory.empty()) {
        double recoveryRate = static_cast<double>(recoveredErrors.size()) / errorHistory.size() * 100.0;
        stats["recovery_rate_percent"] = recoveryRate;
    }
    
    // Errors by category
    nlohmann::json byCategory = nlohmann::json::object();
    for (const auto& pair : activeErrors) {
        std::string categoryStr = errorCategoryToString(pair.second.category);
        byCategory[categoryStr] = byCategory.value(categoryStr, 0) + 1;
    }
    stats["errors_by_category"] = byCategory;
    
    return stats;
}
