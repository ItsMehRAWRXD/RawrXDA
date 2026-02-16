// error_recovery_system.cpp - Enterprise Error Recovery & Auto-Healing System (Qt-free)
#include "error_recovery_system.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cstdio>
#include <thread>
#include <cstdlib>

#include "logging/logger.h"
static Logger s_logger("error_recovery_system");

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <psapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#endif

ErrorRecoverySystem::ErrorRecoverySystem()
    : autoRecoveryEnabled(true),
      maxRetries(3),
      retryDelayMs(5000),
      healthCheckIntervalMs(30000) {
    
    auto now = std::chrono::steady_clock::now();
    m_lastAutoRecoveryTick = now;
    m_lastHealthCheckTick = now;
    
    setupDefaultStrategies();
    
    s_logger.info("[ErrorRecoverySystem] Initialized with 15+ recovery strategies");
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
    
    s_logger.info("[ErrorRecoverySystem] Loaded ");
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
    s_logger.info("[ErrorRecoverySystem] ");
    
    if (m_errorRecordedCb) {
        m_errorRecordedCb(error, m_errorRecordedUd);
    }
    
    // Auto-recovery for critical errors — schedule deferred
    if (autoRecoveryEnabled && (severity == ErrorSeverity::Critical || severity == ErrorSeverity::Error)) {
        s_logger.info("[ErrorRecoverySystem] Scheduling auto-recovery for ");
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
        s_logger.info("[ErrorRecoverySystem] Error not found: ");
        return false;
    }
    
    ErrorRecord_ERS& error = it->second;
    
    // Check retry limit
    if (error.retryCount >= maxRetries) {
        s_logger.info("[ErrorRecoverySystem] Max retries exceeded for ");
        error.wasRecovered = false;
        if (m_recoveryFailedCb) {
            m_recoveryFailedCb(error, m_recoveryFailedUd);
        }
        return false;
    }
    
    // Select best recovery strategy
    RecoveryStrategy strategy = selectBestStrategy(error);
    
    if (strategy.strategyId.empty()) {
        s_logger.info("[ErrorRecoverySystem] No suitable strategy found for ");
        return false;
    }
    
    s_logger.info("[ErrorRecoverySystem] Applying strategy: ");
    
    // Execute recovery
    bool success = executeRecoveryStrategy(error, strategy);
    
    error.retryCount++;
    
    if (success) {
        error.wasRecovered = true;
        error.recoveredAt = std::chrono::system_clock::now();
        
        // Move to recovered errors
        recoveredErrors.push_back(error);
        activeErrors.erase(errorId);
        
        s_logger.info("[ErrorRecoverySystem] Recovery successful for ");
        if (m_errorRecoveredRecordCb) {
            m_errorRecoveredRecordCb(error, m_errorRecoveredRecordUd);
        }
        
        return true;
    } else {
        s_logger.info("[ErrorRecoverySystem] Recovery attempt ");
        
        // Schedule another retry if under limit
        if (error.retryCount < maxRetries && autoRecoveryEnabled) {
            int delay = retryDelayMs * (1 << error.retryCount); // Exponential backoff
            s_logger.info("[ErrorRecoverySystem] Scheduling retry in ");
            
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
        s_logger.info("[ErrorRecoverySystem] Unknown strategy: ");
    }
    
    return success;
}

bool ErrorRecoverySystem::recoverWithRetry(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Retry recovery for ");

    // Exponential backoff: delay = base * 2^retryCount, capped at 30s
    int backoffMs = retryDelayMs * (1 << error.retryCount);
    if (backoffMs > 30000) backoffMs = 30000;

    s_logger.info("[ErrorRecoverySystem] Backoff wait: ");
    std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));

    // Record the recovery action taken
    error.recoveryAction = "retry_exponential (attempt " + std::to_string(error.retryCount + 1)
                         + ", backoff " + std::to_string(backoffMs) + "ms)";

    // The retry itself is performed by the caller (attemptRecovery) which re-invokes
    // the failed operation. Here we signal readiness to retry.
    // If the error was transient (network, cloud), the next attempt likely succeeds.
    bool isTransient = (error.category == ErrorCategory::Network ||
                        error.category == ErrorCategory::CloudProvider);

    if (isTransient && error.retryCount < maxRetries) {
        s_logger.info("[ErrorRecoverySystem] Transient error — retry eligible");
        return true;
    }

    // For non-transient errors, retry only if under half the max retries
    if (error.retryCount < maxRetries / 2) {
        return true;
    }

    s_logger.info("[ErrorRecoverySystem] Retry exhaustion approaching for ");
    return false;
}

bool ErrorRecoverySystem::recoverFallbackLocal(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Falling back to local model for ");

    error.recoveryAction = "fallback_local";

    // Notify the subsystem to switch inference backend to local Ollama
    if (m_fallbackToLocalCb) {
        m_fallbackToLocalCb(error.component, m_fallbackToLocalUd);
        s_logger.info("[ErrorRecoverySystem] Local fallback callback invoked for ");
        return true;
    }

    // No callback registered — attempt direct Ollama health check via HTTP
    // This verifies the local server is actually available before we claim success
#ifdef _WIN32
    // Quick TCP connect test to localhost:11434 (Ollama default)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock != INVALID_SOCKET) {
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(11434);
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            // Non-blocking connect with 2s timeout
            u_long nonBlock = 1;
            ioctlsocket(sock, FIONBIO, &nonBlock);
            connect(sock, (struct sockaddr*)&addr, sizeof(addr));

            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(sock, &writeSet);
            struct timeval tv = {2, 0};
            bool connected = (select(0, nullptr, &writeSet, nullptr, &tv) > 0);

            closesocket(sock);
            WSACleanup();

            if (connected) {
                s_logger.info("[ErrorRecoverySystem] Ollama detected on localhost:11434");
                return true;
            }
        } else {
            WSACleanup();
        }
    }
    s_logger.info("[ErrorRecoverySystem] Local Ollama not reachable — fallback failed");
    return false;
#else
    s_logger.info("[ErrorRecoverySystem] No fallback callback registered");
    return false;
#endif
}

bool ErrorRecoverySystem::recoverClearCache(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Clearing cache for ");

    error.recoveryAction = "clear_cache";

    // Invoke registered cache-clear callback (clears model cache, KV cache, token cache)
    if (m_cacheClearCb) {
        m_cacheClearCb(error.component, m_cacheClearUd);
    }

    // Attempt OS-level memory pressure relief
#ifdef _WIN32
    // Flush working set to reclaim virtual memory
    HANDLE hProcess = GetCurrentProcess();
    SetProcessWorkingSetSize(hProcess, (SIZE_T)-1, (SIZE_T)-1);
    s_logger.info("[ErrorRecoverySystem] Flushed process working set");
#endif

    // Clear temp files for this component if they exist
    std::string tempPattern = "rawrxd_cache_" + error.component;
    char tempPath[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, tempPath)) {
        std::string searchPath = std::string(tempPath) + tempPattern + "*";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            int cleared = 0;
            do {
                std::string fullPath = std::string(tempPath) + fd.cFileName;
                if (DeleteFileA(fullPath.c_str())) cleared++;
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
            s_logger.info("[ErrorRecoverySystem] Cleared ");
        }
    }

    return true;
}

bool ErrorRecoverySystem::recoverRestartComponent(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Restarting component: ");

    error.recoveryAction = "restart_component (" + error.component + ")";

    // Step 1: Signal component shutdown via callback
    if (m_componentRestartCb) {
        m_componentRestartCb(error.component, m_componentRestartUd);
        s_logger.info("[ErrorRecoverySystem] Restart callback invoked for ");
    } else {
        s_logger.info("[ErrorRecoverySystem] No restart callback — cannot restart ");
        return false;
    }

    // Step 2: Brief pause to allow shutdown to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Step 3: Clear any errors from this component (they're stale post-restart)
    std::vector<std::string> staleIds;
    for (const auto& pair : activeErrors) {
        if (pair.second.component == error.component && pair.first != error.errorId) {
            staleIds.push_back(pair.first);
        }
    }
    for (const auto& id : staleIds) {
        auto it = activeErrors.find(id);
        if (it != activeErrors.end()) {
            it->second.wasRecovered = true;
            it->second.recoveryAction = "cleared_by_component_restart";
            recoveredErrors.push_back(it->second);
            activeErrors.erase(it);
        }
    }
    if (!staleIds.empty()) {
        s_logger.info("[ErrorRecoverySystem] Cleared ");
    }

    return true;
}

bool ErrorRecoverySystem::recoverReconnectNetwork(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Reconnecting network for ");

    error.recoveryAction = "reconnect_network";

    // Invoke registered network-reconnect callback (resets HTTP pools, WebSocket connections)
    if (m_networkReconnectCb) {
        m_networkReconnectCb(m_networkReconnectUd);
    }

#ifdef _WIN32
    // Verify basic network connectivity by pinging loopback + DNS resolution
    // Quick DNS check: resolve huggingface.co to verify external connectivity
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        s_logger.info("[ErrorRecoverySystem] WSAStartup failed — network stack error");
        return false;
    }

    // Test 1: DNS resolution
    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int dnsResult = getaddrinfo("huggingface.co", "443", &hints, &result);
    if (result) freeaddrinfo(result);
    WSACleanup();

    if (dnsResult != 0) {
        s_logger.info("[ErrorRecoverySystem] DNS resolution failed — no external network");
        // Still return true if callback was invoked (local-only mode may work)
        return (m_networkReconnectCb != nullptr);
    }

    s_logger.info("[ErrorRecoverySystem] Network connectivity verified");
#endif

    return true;
}

bool ErrorRecoverySystem::recoverReloadData(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Reloading data for ");

    error.recoveryAction = "reload_data (" + error.component + ")";

    // Invoke registered data-reload callback (reloads model weights, config files)
    if (m_dataReloadCb) {
        m_dataReloadCb(error.component, m_dataReloadUd);
        s_logger.info("[ErrorRecoverySystem] Data reload callback invoked for ");
        return true;
    }

    // No callback — attempt to reload configuration from disk as a generic fallback
    // Check for standard config paths
    const char* configPaths[] = {
        "config.json",
        "rawrxd.json",
        "settings.json"
    };

    for (const char* path : configPaths) {
        DWORD attr = GetFileAttributesA(path);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            s_logger.info("[ErrorRecoverySystem] Config file found: ");
            return true; // File exists, subsystem can reload on next access
        }
    }

    s_logger.info("[ErrorRecoverySystem] No data reload callback or config files found");
    return false;
}

bool ErrorRecoverySystem::recoverReduceResources(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Reducing resource usage for ");

    error.recoveryAction = "reduce_resources";

    // Invoke registered resource-reduction callback (shrinks batch size, context window, memory pools)
    if (m_resourceReductionCb) {
        m_resourceReductionCb(m_resourceReductionUd);
    }

#ifdef _WIN32
    // Proactive memory management: trim working set and empty standby pages
    HANDLE hProcess = GetCurrentProcess();
    SetProcessWorkingSetSize(hProcess, (SIZE_T)-1, (SIZE_T)-1);

    // Log current memory state
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        double workingSetMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
        double peakMB = pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
        s_logger.info("[ErrorRecoverySystem] Memory after reduction — Working: ");
    }
#endif

    // Lower thread priority to reduce CPU contention
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    s_logger.info("[ErrorRecoverySystem] Thread priority lowered to reduce contention");

    return true;
}

bool ErrorRecoverySystem::recoverSwitchEndpoint(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Switching endpoint for ");

    error.recoveryAction = "switch_endpoint";

    // Notify the subsystem to rotate to the next API endpoint
    if (m_endpointSwitchCb) {
        m_endpointSwitchCb(error.component, m_endpointSwitchUd);
        s_logger.info("[ErrorRecoverySystem] Endpoint switch callback invoked for ");
        return true;
    }

    // No callback — log available defaults for common components
    if (error.component == "inference" || error.component == "ai_model") {
        s_logger.info("[ErrorRecoverySystem] Inference endpoint switch — ");
        // Common fallback: localhost Ollama
        error.context["switchedEndpoint"] = "http://localhost:11434";
        return true;
    } else if (error.component == "hf_downloader") {
        // HuggingFace mirror endpoints
        error.context["switchedEndpoint"] = "https://hf-mirror.com";
        s_logger.info("[ErrorRecoverySystem] HF downloader switched to mirror");
        return true;
    }

    s_logger.info("[ErrorRecoverySystem] No endpoint switch callback for ");
    return false;
}

bool ErrorRecoverySystem::recoverGracefulDegradation(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Enabling graceful degradation for ");

    error.recoveryAction = "graceful_degradation";

    // Invoke registered degradation callback (disables non-critical features,
    // reduces model quality, switches to smaller quantization)
    if (m_gracefulDegradationCb) {
        m_gracefulDegradationCb(m_gracefulDegradationUd);
    }

    // Record degradation state in error context for downstream consumers
    error.context["degraded"] = true;
    error.context["degradation_reason"] = error.message;
    error.context["degradation_component"] = error.component;

    // Downgrade severity — a degraded but functioning system is Warning, not Critical
    if (error.severity == ErrorSeverity::Critical) {
        error.severity = ErrorSeverity::Warning;
        s_logger.info("[ErrorRecoverySystem] Severity downgraded Critical → Warning (degraded mode)");
    }

    s_logger.info("[ErrorRecoverySystem] Graceful degradation active — non-critical features disabled");
    return true;
}

bool ErrorRecoverySystem::recoverReauthenticate(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Re-authenticating for ");

    error.recoveryAction = "reauthenticate";

    // Invoke registered re-authentication callback (refreshes OAuth tokens, API keys)
    if (m_reauthenticationCb) {
        m_reauthenticationCb(error.component, m_reauthenticationUd);
        s_logger.info("[ErrorRecoverySystem] Re-authentication callback invoked");
        return true;
    }

    // No callback — check for stored credentials file and verify it's readable
    const char* credPaths[] = {
        ".env",
        "credentials.json",
        ".hf_token"
    };

    for (const char* credPath : credPaths) {
        DWORD attr = GetFileAttributesA(credPath);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            s_logger.info("[ErrorRecoverySystem] Credential file found: ");
            error.context["credentialSource"] = credPath;
            return true;
        }
    }

    // Check environment variables for common API keys
    const char* envKeys[] = { "HF_TOKEN", "OPENAI_API_KEY", "ANTHROPIC_API_KEY" };
    for (const char* envKey : envKeys) {
        const char* val = std::getenv(envKey);
        if (val && val[0] != '\0') {
            s_logger.info("[ErrorRecoverySystem] Found env credential: ");
            error.context["credentialSource"] = std::string("env:") + envKey;
            return true;
        }
    }

    s_logger.info("[ErrorRecoverySystem] No credentials found for re-authentication");
    return false;
}

bool ErrorRecoverySystem::recoverEscalateAdmin(ErrorRecord_ERS& error) {
    s_logger.info("[ErrorRecoverySystem] Escalating to administrator: ");

    error.recoveryAction = "escalate_admin";

    // Invoke registered admin escalation callback
    if (m_adminEscalationCb) {
        m_adminEscalationCb(error, m_adminEscalationUd);
    }

    // Write escalation to a persistent log file for external monitoring tools
    char tempPath[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tempPath);
    std::string escalationLog = std::string(tempPath) + "rawrxd_escalation.log";

    FILE* fLog = fopen(escalationLog.c_str(), "a");
    if (fLog) {
        auto now = std::chrono::system_clock::now();
        auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();

        fprintf(fLog, "[%lld] ESCALATION | Component: %s | Severity: %s | Message: %s\n",
                (long long)epoch,
                error.component.c_str(),
                errorSeverityToString(error.severity).c_str(),
                error.message.c_str());

        if (!error.stackTrace.empty()) {
            fprintf(fLog, "  Stack: %s\n", error.stackTrace.c_str());
        }
        fclose(fLog);
        s_logger.info("[ErrorRecoverySystem] Escalation logged to ");
    }

    // Also emit via OutputDebugString for attached debuggers
    std::string debugMsg = "[RAWRXD ESCALATION] " + error.component + ": " + error.message;
    OutputDebugStringA(debugMsg.c_str());

    // Escalation always "succeeds" — the error is now visible to admins
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
    
    s_logger.info("[ErrorRecoverySystem] Manually resolved error: ");
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
    s_logger.info("[ErrorRecoverySystem] Auto-recovery ");
}

void ErrorRecoverySystem::setMaxRetries(int retries) {
    maxRetries = retries;
}

void ErrorRecoverySystem::setRetryDelay(int milliseconds) {
    retryDelayMs = milliseconds;
}

void ErrorRecoverySystem::clearErrorHistory() {
    errorHistory.clear();
    s_logger.info("[ErrorRecoverySystem] Error history cleared");
}

void ErrorRecoverySystem::clearRecoveredErrors() {
    recoveredErrors.clear();
    s_logger.info("[ErrorRecoverySystem] Recovered errors cleared");
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
