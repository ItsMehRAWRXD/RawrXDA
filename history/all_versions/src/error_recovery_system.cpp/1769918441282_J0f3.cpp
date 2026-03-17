// error_recovery_system.cpp - Enterprise Error Recovery & Auto-Healing System
#include "error_recovery_system.h"


#include <iostream>
#include <algorithm>
#include <filesystem>
#include <iostream>

ErrorRecoverySystem::ErrorRecoverySystem(void* parent)
    : void(parent),
      autoRecoveryEnabled(true),
      maxRetries(3),
      retryDelayMs(5000),
      healthCheckIntervalMs(30000) {
    
    autoRecoveryTimer = new void*(this);
    autoRecoveryTimer->setInterval(retryDelayMs);
// Qt connect removed
    healthCheckTimer = new void*(this);
    healthCheckTimer->setInterval(healthCheckIntervalMs);
// Qt connect removed
    setupDefaultStrategies();
    
    autoRecoveryTimer->start();
    healthCheckTimer->start();


}

ErrorRecoverySystem::~ErrorRecoverySystem() {
    autoRecoveryTimer->stop();
    healthCheckTimer->stop();
}

void ErrorRecoverySystem::setupDefaultStrategies() {
    // Strategy 1: Retry with exponential backoff
    RecoveryStrategy retryStrategy;
    retryStrategy.strategyId = "retry_exponential";
    retryStrategy.name = "Retry with Exponential Backoff";
    retryStrategy.applicableCategories << ErrorCategory::Network << ErrorCategory::CloudProvider;
    retryStrategy.maxRetries = 5;
    retryStrategy.retryDelayMs = 1000;
    retryStrategy.successRate = 0.85;
    retryStrategy.isAutomatic = true;
    retryStrategy.recoverySteps << "Wait with exponential backoff" << "Retry operation" << "Log attempt";
    strategies["retry_exponential"] = retryStrategy;
    
    // Strategy 2: Fallback to local model
    RecoveryStrategy fallbackLocal;
    fallbackLocal.strategyId = "fallback_local";
    fallbackLocal.name = "Fallback to Local Model";
    fallbackLocal.applicableCategories << ErrorCategory::AIModel << ErrorCategory::CloudProvider;
    fallbackLocal.maxRetries = 1;
    fallbackLocal.retryDelayMs = 0;
    fallbackLocal.successRate = 0.95;
    fallbackLocal.isAutomatic = true;
    fallbackLocal.recoverySteps << "Detect cloud failure" << "Switch to local Ollama" << "Continue execution";
    strategies["fallback_local"] = fallbackLocal;
    
    // Strategy 3: Clear cache and retry
    RecoveryStrategy clearCache;
    clearCache.strategyId = "clear_cache";
    clearCache.name = "Clear Cache and Retry";
    clearCache.applicableCategories << ErrorCategory::Performance << ErrorCategory::System;
    clearCache.maxRetries = 2;
    clearCache.retryDelayMs = 2000;
    clearCache.successRate = 0.80;
    clearCache.isAutomatic = true;
    clearCache.recoverySteps << "Clear application cache" << "Free memory" << "Retry operation";
    strategies["clear_cache"] = clearCache;
    
    // Strategy 4: Restart component
    RecoveryStrategy restartComponent;
    restartComponent.strategyId = "restart_component";
    restartComponent.name = "Restart Failed Component";
    restartComponent.applicableCategories << ErrorCategory::System << ErrorCategory::Performance;
    restartComponent.maxRetries = 3;
    restartComponent.retryDelayMs = 5000;
    restartComponent.successRate = 0.90;
    restartComponent.isAutomatic = true;
    restartComponent.recoverySteps << "Stop component" << "Clean resources" << "Restart component" << "Verify health";
    strategies["restart_component"] = restartComponent;
    
    // Strategy 5: Reconnect network
    RecoveryStrategy reconnect;
    reconnect.strategyId = "reconnect_network";
    reconnect.name = "Reconnect Network Connection";
    reconnect.applicableCategories << ErrorCategory::Network;
    reconnect.maxRetries = 5;
    reconnect.retryDelayMs = 3000;
    reconnect.successRate = 0.88;
    reconnect.isAutomatic = true;
    reconnect.recoverySteps << "Close existing connections" << "Wait for network stability" << "Reestablish connections";
    strategies["reconnect_network"] = reconnect;
    
    // Strategy 6: Reset configuration
    RecoveryStrategy resetConfig;
    resetConfig.strategyId = "reset_config";
    resetConfig.name = "Reset to Default Configuration";
    resetConfig.applicableCategories << ErrorCategory::Configuration;
    resetConfig.maxRetries = 1;
    resetConfig.retryDelayMs = 1000;
    resetConfig.successRate = 0.92;
    resetConfig.isAutomatic = false; // Requires user confirmation
    resetConfig.recoverySteps << "Backup current config" << "Load default config" << "Restart application";
    strategies["reset_config"] = resetConfig;
    
    // Strategy 7: Reload data
    RecoveryStrategy reloadData;
    reloadData.strategyId = "reload_data";
    reloadData.name = "Reload Data from Source";
    reloadData.applicableCategories << ErrorCategory::Database << ErrorCategory::FileIO;
    reloadData.maxRetries = 3;
    reloadData.retryDelayMs = 2000;
    reloadData.successRate = 0.87;
    reloadData.isAutomatic = true;
    reloadData.recoverySteps << "Clear stale data" << "Reconnect to source" << "Reload fresh data";
    strategies["reload_data"] = reloadData;
    
    // Strategy 8: Reduce resource usage
    RecoveryStrategy reduceResources;
    reduceResources.strategyId = "reduce_resources";
    reduceResources.name = "Reduce Resource Consumption";
    reduceResources.applicableCategories << ErrorCategory::Performance << ErrorCategory::System;
    reduceResources.maxRetries = 1;
    reduceResources.retryDelayMs = 1000;
    reduceResources.successRate = 0.83;
    reduceResources.isAutomatic = true;
    reduceResources.recoverySteps << "Reduce thread count" << "Lower memory allocation" << "Throttle operations";
    strategies["reduce_resources"] = reduceResources;
    
    // Strategy 9: Rollback transaction
    RecoveryStrategy rollback;
    rollback.strategyId = "rollback_transaction";
    rollback.name = "Rollback Failed Transaction";
    rollback.applicableCategories << ErrorCategory::Database;
    rollback.maxRetries = 1;
    rollback.retryDelayMs = 500;
    rollback.successRate = 0.98;
    rollback.isAutomatic = true;
    rollback.recoverySteps << "Detect transaction failure" << "Rollback changes" << "Clean up resources";
    strategies["rollback_transaction"] = rollback;
    
    // Strategy 10: Switch to backup endpoint
    RecoveryStrategy switchEndpoint;
    switchEndpoint.strategyId = "switch_endpoint";
    switchEndpoint.name = "Switch to Backup Endpoint";
    switchEndpoint.applicableCategories << ErrorCategory::Network << ErrorCategory::CloudProvider;
    switchEndpoint.maxRetries = 3;
    switchEndpoint.retryDelayMs = 2000;
    switchEndpoint.successRate = 0.91;
    switchEndpoint.isAutomatic = true;
    switchEndpoint.recoverySteps << "Detect endpoint failure" << "Switch to backup" << "Update routing";
    strategies["switch_endpoint"] = switchEndpoint;
    
    // Strategy 11: Graceful degradation
    RecoveryStrategy degrade;
    degrade.strategyId = "graceful_degradation";
    degrade.name = "Graceful Degradation";
    degrade.applicableCategories << ErrorCategory::Performance << ErrorCategory::AIModel;
    degrade.maxRetries = 1;
    degrade.retryDelayMs = 0;
    degrade.successRate = 0.96;
    degrade.isAutomatic = true;
    degrade.recoverySteps << "Disable non-critical features" << "Continue with reduced functionality";
    strategies["graceful_degradation"] = degrade;
    
    // Strategy 12: Re-authenticate
    RecoveryStrategy reauth;
    reauth.strategyId = "reauthenticate";
    reauth.name = "Re-authenticate Credentials";
    reauth.applicableCategories << ErrorCategory::Security << ErrorCategory::CloudProvider;
    reauth.maxRetries = 2;
    reauth.retryDelayMs = 3000;
    reauth.successRate = 0.89;
    reauth.isAutomatic = true;
    reauth.recoverySteps << "Refresh authentication token" << "Retry with new credentials";
    strategies["reauthenticate"] = reauth;
    
    // Strategy 13: Repair file system
    RecoveryStrategy repairFS;
    repairFS.strategyId = "repair_filesystem";
    repairFS.name = "Repair File System Issues";
    repairFS.applicableCategories << ErrorCategory::FileIO;
    repairFS.maxRetries = 1;
    repairFS.retryDelayMs = 2000;
    repairFS.successRate = 0.75;
    repairFS.isAutomatic = false; // May require admin permissions
    repairFS.recoverySteps << "Check file permissions" << "Repair corrupted files" << "Recreate missing directories";
    strategies["repair_filesystem"] = repairFS;
    
    // Strategy 14: Notify and escalate
    RecoveryStrategy escalate;
    escalate.strategyId = "escalate_admin";
    escalate.name = "Escalate to Administrator";
    escalate.applicableCategories << ErrorCategory::Security << ErrorCategory::System;
    escalate.maxRetries = 1;
    escalate.retryDelayMs = 0;
    escalate.successRate = 1.0; // Always succeeds (just notifies)
    escalate.isAutomatic = true;
    escalate.recoverySteps << "Log critical error" << "Send alert to admin" << "Wait for manual intervention";
    strategies["escalate_admin"] = escalate;
    
    // Strategy 15: Kill and restart process
    RecoveryStrategy killRestart;
    killRestart.strategyId = "kill_restart";
    killRestart.name = "Kill and Restart Process";
    killRestart.applicableCategories << ErrorCategory::System << ErrorCategory::Performance;
    killRestart.maxRetries = 2;
    killRestart.retryDelayMs = 10000;
    killRestart.successRate = 0.93;
    killRestart.isAutomatic = false; // Dangerous operation
    killRestart.recoverySteps << "Save state" << "Kill hanging process" << "Restart process" << "Restore state";
    strategies["kill_restart"] = killRestart;


}

std::string ErrorRecoverySystem::recordError(const std::string& component, ErrorSeverity severity,
                                         ErrorCategory category, const std::string& message,
                                         const std::string& stackTrace, const void*& context) {
    ErrorRecord error;
    error.errorId = generateErrorId();
    error.component = component;
    error.severity = severity;
    error.category = category;
    error.message = message;
    error.stackTrace = stackTrace;
    error.context = context;
    error.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    error.retryCount = 0;
    error.wasRecovered = false;
    
    activeErrors[error.errorId] = error;
    errorHistory.append(error);
    
    // Log based on severity
    std::string severityStr = errorSeverityToString(severity);


    errorRecorded(error);
    
    // Auto-recovery for critical errors
    if (autoRecoveryEnabled && (severity == ErrorSeverity::Critical || severity == ErrorSeverity::Error)) {
        
        void*::singleShot(100, this, [this, errorId = error.errorId]() {
            attemptRecovery(errorId);
        });
    }
    
    return error.errorId;
}

bool ErrorRecoverySystem::attemptRecovery(const std::string& errorId) {
    if (!activeErrors.contains(errorId)) {
        
        return false;
    }
    
    ErrorRecord& error = activeErrors[errorId];
    
    // Check retry limit
    if (error.retryCount >= maxRetries) {
        
        error.wasRecovered = false;
        recoveryFailed(error);
        return false;
    }
    
    // Select best recovery strategy
    RecoveryStrategy strategy = selectBestStrategy(error);
    
    if (strategy.strategyId.empty()) {
        
        return false;
    }


    // Execute recovery
    bool success = executeRecoveryStrategy(error, strategy);
    
    error.retryCount++;
    
    if (success) {
        error.wasRecovered = true;
        error.recoveredAt = std::chrono::system_clock::time_point::currentDateTime();
        
        // Move to recovered errors
        recoveredErrors.append(error);
        activeErrors.remove(errorId);


        errorRecoveredRecord(error);
        
        return true;
    } else {


        // Schedule another retry if under limit
        if (error.retryCount < maxRetries && autoRecoveryEnabled) {
            int delay = retryDelayMs * (1 << error.retryCount); // Exponential backoff


            void*::singleShot(delay, this, [this, errorId]() {
                attemptRecovery(errorId);
            });
        }
        
        return false;
    }
}

RecoveryStrategy ErrorRecoverySystem::selectBestStrategy(const ErrorRecord& error) {
    RecoveryStrategy bestStrategy;
    double bestScore = -1.0;
    
    for (const RecoveryStrategy& strategy : strategies.values()) {
        // Check if strategy applies to this error category
        if (!strategy.applicableCategories.contains(error.category)) {
            continue;
        }
        
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

bool ErrorRecoverySystem::executeRecoveryStrategy(ErrorRecord& error, const RecoveryStrategy& strategy) {
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
        
    }
    
    return success;
}

bool ErrorRecoverySystem::recoverWithRetry(ErrorRecord& error) {
    // Simple retry with delay - actual retry happens in attemptRecovery
    
    return true; // Indicate retry should continue
}

bool ErrorRecoverySystem::recoverFallbackLocal(ErrorRecord& error) {


    // Signal to switch to local execution
    fallbackToLocalRequested(error.component);
    
    return true;
}

bool ErrorRecoverySystem::recoverClearCache(ErrorRecord& error) {
    // Signal cache clear
    cacheClearRequested(error.component);
    
    // Real Logic: Delete temp files/cache directory
    try {
        std::filesystem::path cachePath = "temp/cache"; // Standardize on this relative path for portable cache
        if (std::filesystem::exists(cachePath)) {
            std::filesystem::remove_all(cachePath);
            std::filesystem::create_directories(cachePath); // Recreate clean
        }
        
        // Also clear any model specific caches if component is model
        if (error.component.find("model") != std::string::npos) {
             std::filesystem::path modelCache = "temp/model_cache";
             if (std::filesystem::exists(modelCache)) {
                  std::filesystem::remove_all(modelCache);
             }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Cache clear failed: " << e.what() << std::endl;
        return false;
    }
}

bool ErrorRecoverySystem::recoverRestartComponent(ErrorRecord& error) {


    componentRestartRequested(error.component);
    
    return true;
}

bool ErrorRecoverySystem::recoverReconnectNetwork(ErrorRecord& error) {


    networkReconnectRequested();
    
    return true;
}

bool ErrorRecoverySystem::recoverReloadData(ErrorRecord& error) {


    dataReloadRequested(error.component);
    
    return true;
}

bool ErrorRecoverySystem::recoverReduceResources(ErrorRecord& error) {


    resourceReductionRequested();
    
    return true;
}

bool ErrorRecoverySystem::recoverSwitchEndpoint(ErrorRecord& error) {


    endpointSwitchRequested(error.component);
    
    return true;
}

bool ErrorRecoverySystem::recoverGracefulDegradation(ErrorRecord& error) {


    gracefulDegradationEnabled();
    
    return true;
}

bool ErrorRecoverySystem::recoverReauthenticate(ErrorRecord& error) {


    reauthenticationRequested(error.component);
    
    return true;
}

bool ErrorRecoverySystem::recoverEscalateAdmin(ErrorRecord& error) {


    adminEscalationRequired(error);
    
    return true; // Notification always succeeds
}

void ErrorRecoverySystem::resolveError(const std::string& errorId) {
    if (!activeErrors.contains(errorId)) {
        return;
    }
    
    ErrorRecord error = activeErrors[errorId];
    error.wasRecovered = true;
    error.recoveredAt = std::chrono::system_clock::time_point::currentDateTime();
    
    recoveredErrors.append(error);
    activeErrors.remove(errorId);


    errorRecoveredRecord(error);
}

ErrorRecord ErrorRecoverySystem::getError(const std::string& errorId) const {
    if (activeErrors.contains(errorId)) {
        return activeErrors[errorId];
    }
    
    for (const ErrorRecord& error : recoveredErrors) {
        if (error.errorId == errorId) {
            return error;
        }
    }
    
    return ErrorRecord();
}

std::vector<ErrorRecord> ErrorRecoverySystem::getActiveErrors() const {
    return activeErrors.values().toVector();
}

std::vector<ErrorRecord> ErrorRecoverySystem::getErrorsByComponent(const std::string& component) const {
    std::vector<ErrorRecord> componentErrors;
    
    for (const ErrorRecord& error : activeErrors.values()) {
        if (error.component == component) {
            componentErrors.append(error);
        }
    }
    
    return componentErrors;
}

std::vector<ErrorRecord> ErrorRecoverySystem::getErrorsBySeverity(ErrorSeverity severity) const {
    std::vector<ErrorRecord> severityErrors;
    
    for (const ErrorRecord& error : activeErrors.values()) {
        if (error.severity == severity) {
            severityErrors.append(error);
        }
    }
    
    return severityErrors;
}

SystemHealth ErrorRecoverySystem::getSystemHealth() const {
    return currentSystemHealth;
}

void ErrorRecoverySystem::updateSystemHealth() {
    currentSystemHealth.activeErrors = activeErrors.size();
    currentSystemHealth.criticalErrors = getErrorsBySeverity(ErrorSeverity::Critical).size();
    currentSystemHealth.errorsRecovered = recoveredErrors.size();
    
    // Count errors by component
    currentSystemHealth.errorsByComponent.clear();
    for (const ErrorRecord& error : activeErrors.values()) {
        currentSystemHealth.errorsByComponent[error.component]++;
    }
    
    // Calculate health score (0-100)
    int totalErrors = currentSystemHealth.activeErrors;
    int criticalCount = currentSystemHealth.criticalErrors;
    
    if (totalErrors == 0) {
        currentSystemHealth.healthScore = 100.0;
        currentSystemHealth.isHealthy = true;
    } else {
        // Deduct points for errors
        double score = 100.0;
        score -= criticalCount * 20.0;  // Critical errors: -20 points each
        score -= (totalErrors - criticalCount) * 5.0;  // Other errors: -5 points each
        
        currentSystemHealth.healthScore = std::max(0.0, score);
        currentSystemHealth.isHealthy = (currentSystemHealth.healthScore >= 80.0);
    }
    
    systemHealthUpdated(currentSystemHealth);
}

void ErrorRecoverySystem::processAutoRecovery() {
    if (!autoRecoveryEnabled) {
        return;
    }
    
    // Process pending recoveries
    for (const std::string& errorId : activeErrors.keys()) {
        ErrorRecord& error = activeErrors[errorId];
        
        // Only auto-recover errors that haven't exceeded retry limit
        if (error.retryCount < maxRetries) {
            std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point::currentDateTime();
            int64_t msSinceError = error.timestamp.msecsTo(now);
            
            // Wait before first retry
            if (error.retryCount == 0 && msSinceError > retryDelayMs) {
                attemptRecovery(errorId);
            }
        }
    }
}

void ErrorRecoverySystem::enableAutoRecovery(bool enable) {
    autoRecoveryEnabled = enable;
    
}

void ErrorRecoverySystem::setMaxRetries(int retries) {
    maxRetries = retries;
}

void ErrorRecoverySystem::setRetryDelay(int milliseconds) {
    retryDelayMs = milliseconds;
    autoRecoveryTimer->setInterval(milliseconds);
}

void ErrorRecoverySystem::clearErrorHistory() {
    errorHistory.clear();
    
}

void ErrorRecoverySystem::clearRecoveredErrors() {
    recoveredErrors.clear();
    
}

std::string ErrorRecoverySystem::generateErrorId() {
    return std::string("error_%1_%2"))->bounded(10000));
}

std::string ErrorRecoverySystem::errorSeverityToString(ErrorSeverity severity) const {
    switch (severity) {
        case ErrorSeverity::Info: return "INFO";
        case ErrorSeverity::Warning: return "WARNING";
        case ErrorSeverity::Error: return "ERROR";
        case ErrorSeverity::Critical: return "CRITICAL";
        case ErrorSeverity::Fatal: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string ErrorRecoverySystem::errorCategoryToString(ErrorCategory category) const {
    switch (category) {
        case ErrorCategory::System: return "System";
        case ErrorCategory::Network: return "Network";
        case ErrorCategory::FileIO: return "FileIO";
        case ErrorCategory::Database: return "Database";
        case ErrorCategory::AIModel: return "AIModel";
        case ErrorCategory::CloudProvider: return "CloudProvider";
        case ErrorCategory::Security: return "Security";
        case ErrorCategory::Performance: return "Performance";
        case ErrorCategory::UserInput: return "UserInput";
        case ErrorCategory::Configuration: return "Configuration";
        default: return "Unknown";
    }
}

void* ErrorRecoverySystem::getErrorStatistics() const {
    void* stats;
    
    stats["active_errors"] = activeErrors.size();
    stats["recovered_errors"] = recoveredErrors.size();
    stats["total_errors"] = errorHistory.size();
    stats["critical_errors"] = getErrorsBySeverity(ErrorSeverity::Critical).size();
    stats["health_score"] = currentSystemHealth.healthScore;
    stats["is_healthy"] = currentSystemHealth.isHealthy;
    
    // Recovery rate
    if (errorHistory.size() > 0) {
        double recoveryRate = static_cast<double>(recoveredErrors.size()) / errorHistory.size() * 100.0;
        stats["recovery_rate_percent"] = recoveryRate;
    }
    
    // Errors by category
    void* byCategory;
    for (const ErrorRecord& error : activeErrors.values()) {
        std::string categoryStr = errorCategoryToString(error.category);
        byCategory[categoryStr] = byCategory[categoryStr].toInt() + 1;
    }
    stats["errors_by_category"] = byCategory;
    
    return stats;
}


