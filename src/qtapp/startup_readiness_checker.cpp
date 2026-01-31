/**
 * @file startup_readiness_checker.cpp
 * @brief Implementation of production startup validation system
 * @author RawrXD Team
 * @date 2026-01-08
 */

#include "startup_readiness_checker.hpp"
#include "unified_hotpatch_manager.hpp"
#include "settings_manager.h"

#include <map>

// ==================== StartupReadinessChecker Implementation ====================

StartupReadinessChecker::StartupReadinessChecker()
    
    , m_networkManager(new void*(this))
    , m_maxRetries(3)
    , m_timeoutMs(5000)
    , m_backoffMs(500)
    , m_completedChecks(0)
    , m_totalChecks(8)
{  // Signal connection removed\n}

StartupReadinessChecker::~StartupReadinessChecker()
{
    // Cancel all pending timers
    for (auto timer : m_checkTimers) {
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
    }
    
    // Wait for concurrent checks to finish
    for (auto& future : m_runningChecks) {
        if (future.isRunning()) {
            future.waitForFinished();
        }
    }
}

void StartupReadinessChecker::runChecks()
{
    
    m_lastReport = AgentReadinessReport();
    m_lastReport.startTime = // DateTime::currentDateTime();
    m_completedChecks = 0;
    m_retryCount.clear();
    m_totalTimer.start();
    
    // Get configuration from settings
    SettingsManager& settings = SettingsManager::instance();
    m_maxRetries = std::max(1, settings.getValue("AppSettings/readiness.max_retries", m_maxRetries));
    m_backoffMs = std::max(0, settings.getValue("AppSettings/readiness.backoff_ms", m_backoffMs));
    
    // 1. Check LLM Endpoints (concurrent for speed)
    std::string ollamaEndpoint = settings.getValue("llm/ollama_endpoint", "http://localhost:11434").toString();
    std::string claudeEndpoint = settings.getValue("llm/claude_endpoint", "https://api.anthropic.com").toString();
    std::string claudeKey = settings.getValue("llm/claude_api_key", "").toString();
    std::string openaiEndpoint = settings.getValue("llm/openai_endpoint", "https://api.openai.com/v1").toString();
    std::string openaiKey = settings.getValue("llm/openai_api_key", "").toString();
    
    // Launch LLM checks concurrently
    m_runningChecks["Ollama"] = [](auto f){f();}([this, ollamaEndpoint]() {
        return runWithRetry("Ollama", [this, ollamaEndpoint]() {
            return checkLLMEndpoint("Ollama", ollamaEndpoint, "");
        });
    });
    
    if (!claudeKey.empty()) {
        m_runningChecks["Claude"] = [](auto f){f();}([this, claudeEndpoint, claudeKey]() {
            return runWithRetry("Claude", [this, claudeEndpoint, claudeKey]() {
                return checkLLMEndpoint("Claude", claudeEndpoint, claudeKey);
            });
        });
    }
    
    if (!openaiKey.empty()) {
        m_runningChecks["OpenAI"] = [](auto f){f();}([this, openaiEndpoint, openaiKey]() {
            return runWithRetry("OpenAI", [this, openaiEndpoint, openaiKey]() {
                return checkLLMEndpoint("OpenAI", openaiEndpoint, openaiKey);
            });
        });
    }
    
    // 2. Check GGUF Server
    uint16_t ggufPort = settings.getValue("gguf/server_port", 11434).toUInt();
    m_runningChecks["GGUF Server"] = [](auto f){f();}([this, ggufPort]() {
        return runWithRetry("GGUF Server", [this, ggufPort]() {
            return checkGGUFServer(ggufPort);
        });
    });
    
    // 3. Check Project Root
    std::string projectRoot = settings.getValue("project/default_root", "").toString();
    if (projectRoot.empty()) {
        // Try environment variable
        projectRoot = qEnvironmentVariable("RAWRXD_PROJECT_ROOT");
    }
    if (projectRoot.empty()) {
        // Default fallback
        if (std::filesystem::exists("E:\\")) projectRoot = "E:\\";
        else if (std::filesystem::exists("D:\\RawrXD-production-lazy-init")) projectRoot = "D:\\RawrXD-production-lazy-init";
        else projectRoot = "";
    }
    
    m_runningChecks["Project Root"] = [](auto f){f();}([this, projectRoot]() {
        return runWithRetry("Project Root", [this, projectRoot]() {
            return checkProjectRoot(projectRoot);
        });
    });
    
    // 4. Check Environment Variables
    m_runningChecks["Environment"] = [](auto f){f();}([this]() {
        return runWithRetry("Environment", [this]() {
            return checkEnvironmentVariables();
        });
    });
    
    // 5. Check Network Connectivity
    m_runningChecks["Network"] = [](auto f){f();}([this]() {
        return runWithRetry("Network", [this]() {
            return checkNetworkConnectivity();
        });
    });
    
    // 6. Check Disk Space
    m_runningChecks["Disk Space"] = [](auto f){f();}([this]() {
        return runWithRetry("Disk Space", [this]() {
            return checkDiskSpace();
        });
    });
    
    // 7. Check Model Cache
    m_runningChecks["Model Cache"] = [](auto f){f();}([this]() {
        return runWithRetry("Model Cache", [this]() {
            return checkModelCache();
        });
    });
    
    // Monitor completion of all checks
    // Timer monitorTimer = new // Timer(this);
    // Connect removed {
        bool allDone = true;
        int completed = 0;
        
        for (auto it = m_runningChecks.begin(); it != m_runningChecks.end(); ++it) {
            if (it.value().isFinished()) {
                // Collect result if not already collected
                if (!m_lastReport.checks.contains(it.key())) {
                    HealthCheckResult result = it.value().result();
                    m_lastReport.checks[it.key()] = result;
                    
                    if (!result.success) {
                        m_lastReport.failures.append(it.key());
                    }
                    
                    checkProgress(it.key(), 100, result.message);
                    completed++;
                }
            } else {
                allDone = false;
            }
        }
        
        m_completedChecks = m_lastReport.checks.size();
        
        if (allDone) {
            monitorTimer->stop();
            monitorTimer->deleteLater();
            
            // Finalize report
            m_lastReport.endTime = // DateTime::currentDateTime();
            m_lastReport.totalLatency = m_totalTimer.elapsed();
            m_lastReport.overallReady = m_lastReport.failures.empty();
            logReadinessMetrics(m_lastReport);
            
                     << "Failures:" << m_lastReport.failures.size();
            
            readinessComplete(m_lastReport);
        }
    });
    monitorTimer->start(100); // Check every 100ms
}

HealthCheckResult StartupReadinessChecker::checkLLMEndpoint(const std::string& backend, 
                                                             const std::string& endpoint, 
                                                             const std::string& apiKey)
{
    HealthCheckResult result;
    result.subsystem = backend;
    result.timestamp = // DateTime::currentDateTime();
    
    std::chrono::steady_clock timer;
    timer.start();
    
    checkProgress(backend, 10, "Connecting to " + endpoint + "...");
    
    std::string url(endpoint);
    if (backend == "Ollama") {
        // Ollama health check: GET /api/version or /api/tags
        url.setPath("/api/tags");
    } else if (backend == "Claude") {
        // Claude health check: minimal request
        url.setPath("/v1/messages");
    } else if (backend == "OpenAI") {
        // OpenAI health check: GET /v1/models
        url.setPath("/v1/models");
    }
    
    void* request(url);
    request.setHeader(void*::ContentTypeHeader, "application/json");
    
    if (!apiKey.empty()) {
        if (backend == "Claude") {
            request.setRawHeader("x-api-key", apiKey.toUtf8());
            request.setRawHeader("anthropic-version", "2023-06-01");
        } else if (backend == "OpenAI") {
            request.setRawHeader("Authorization", std::string("Bearer %1").toUtf8());
        }
    }
    
    checkProgress(backend, 30, "Sending health check request...");
    
    // Synchronous request with timeout
    void** reply = m_networkManager->get(request);
    
    voidLoop loop;
    // Timer timeoutTimer;
    timeoutTimer.setSingleShot(true);  // Signal connection removed\n  // Signal connection removed\ntimeoutTimer.start(m_timeoutMs);
    loop.exec();
    
    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
        
        if (reply->error() == void*::NoError) {
            result.success = true;
            result.latencyMs = timer.elapsed();
            result.message = std::string("%1 endpoint healthy (%2ms)");
            result.technicalDetails = std::string("HTTP %1, URL: %2")
                )
                ;
            
            checkProgress(backend, 100, result.message);
        } else {
            result.success = false;
            result.message = std::string("%1 endpoint unreachable");
            result.technicalDetails = std::string("Error: %1, URL: %2")
                )
                ;
            
            checkProgress(backend, 100, result.message);
        }
    } else {
        // Timeout
        reply->abort();
        result.success = false;
        result.message = std::string("%1 endpoint timeout");
        result.technicalDetails = std::string("Timeout after %1ms, URL: %2")
            
            ;
        
        checkProgress(backend, 100, result.message);
    }
    
    reply->deleteLater();
    return result;
}

HealthCheckResult StartupReadinessChecker::checkGGUFServer(uint16_t port)
{
    HealthCheckResult result;
    result.subsystem = "GGUF Server";
    result.timestamp = // DateTime::currentDateTime();
    
    checkProgress("GGUF Server", 20, std::string("Testing port %1..."));
    
    void* socket;
    std::chrono::steady_clock timer;
    timer.start();
    
    socket.connectToHost("localhost", port);
    
    if (socket.waitForConnected(m_timeoutMs)) {
        result.latencyMs = timer.elapsed();
        
        // Try to send a minimal HTTP request
        socket.write("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        socket.flush();
        
        if (socket.waitForReadyRead(2000)) {
            std::vector<uint8_t> response = socket.readAll();
            
            result.success = true;
            result.message = std::string("GGUF Server responsive on port %1 (%2ms)");
            result.technicalDetails = std::string("TCP connection successful, HTTP response received: %1 bytes")
                );
        } else {
            result.success = false;
            result.message = std::string("GGUF Server port %1 open but not responding");
            result.technicalDetails = "TCP connected but no HTTP response";
        }
        
        socket.close();
    } else {
        result.success = false;
        result.message = std::string("GGUF Server port %1 not accessible");
        result.technicalDetails = std::string("Connection failed: %1"));
    }
    
    checkProgress("GGUF Server", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkHotpatchManager(UnifiedHotpatchManager* manager)
{
    HealthCheckResult result;
    result.subsystem = "Hotpatch Manager";
    result.timestamp = // DateTime::currentDateTime();
    
    checkProgress("Hotpatch Manager", 50, "Validating hotpatch subsystem...");
    
    if (!manager) {
        result.success = false;
        result.message = "Hotpatch Manager not initialized";
        result.technicalDetails = "Manager pointer is null";
    } else {
        // Check if manager is initialized (we'll assume it has an isInitialized method or similar)
        // For now, just check if pointer is valid
        result.success = true;
        result.message = "Hotpatch Manager initialized";
        result.technicalDetails = "Manager instance exists and is accessible";
    }
    
    checkProgress("Hotpatch Manager", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkProjectRoot(const std::string& projectPath)
{
    HealthCheckResult result;
    result.subsystem = "Project Root";
    result.timestamp = // DateTime::currentDateTime();
    
    checkProgress("Project Root", 30, std::string("Validating %1..."));
    
    // Info pathInfo(projectPath);
    
    if (projectPath.empty()) {
        result.success = false;
        result.message = "No project root configured";
        result.technicalDetails = "Project root path is empty. Set RAWRXD_PROJECT_ROOT env var or configure in settings.";
        m_lastReport.warnings.append("Configure default project root in settings");
    } else if (!pathInfo.exists()) {
        result.success = false;
        result.message = std::string("Project root does not exist: %1");
        result.technicalDetails = std::string("Path: %1 (not found)"));
    } else if (!pathInfo.isDir()) {
        result.success = false;
        result.message = std::string("Project root is not a directory: %1");
        result.technicalDetails = std::string("Path: %1 (is a file)"));
    } else if (!pathInfo.isReadable()) {
        result.success = false;
        result.message = std::string("Project root not accessible: %1");
        result.technicalDetails = std::string("Path: %1 (permission denied)"));
    } else {
        result.success = true;
        result.message = std::string("Project root ready: %1");
        
        // Get directory stats
        // dir(projectPath);
        int fileCount = dir.entryList(// Dir::Files).size();
        int dirCount = dir.entryList(// Dir::Dirs | // Dir::NoDotAndDotDot).size();
        
        result.technicalDetails = std::string("Path: %1\nFiles: %2, Subdirs: %3, Writable: %4")
            )


             ? "Yes" : "No");
    }
    
    checkProgress("Project Root", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkEnvironmentVariables()
{
    HealthCheckResult result;
    result.subsystem = "Environment";
    result.timestamp = // DateTime::currentDateTime();
    
    checkProgress("Environment", 40, "Checking environment variables...");
    
    std::stringList requiredVars = {
        // Optional but recommended
        "RAWRXD_PROJECT_ROOT",
        "RAWRXD_MODEL_CACHE",
        "RAWRXD_LOG_LEVEL"
    };
    
    std::stringList missingVars;
    std::stringList foundVars;
    
    for (const std::string& varName : requiredVars) {
        std::string value = qEnvironmentVariable(varName.toUtf8().constData());
        if (value.empty()) {
            missingVars.append(varName);
        } else {
            foundVars.append(std::string("%1=%2"));
        }
    }
    
    // Environment vars are optional, so this is always a success
    result.success = true;
    
    if (missingVars.empty()) {
        result.message = "All recommended environment variables set";
        result.technicalDetails = foundVars.join("\n");
    } else {
        result.message = std::string("%1 recommended env vars set"));
        result.technicalDetails = std::string("Set: %1\nOptional but recommended: %2")
            )
            );
        
        if (!missingVars.empty()) {
            m_lastReport.warnings.append(std::string("Optional env vars not set: %1")));
        }
    }
    
    checkProgress("Environment", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkNetworkConnectivity()
{
    HealthCheckResult result;
    result.subsystem = "Network";
    result.timestamp = // DateTime::currentDateTime();
    
    checkProgress("Network", 30, "Testing internet connectivity...");
    
    std::chrono::steady_clock timer;
    timer.start();
    
    // Ping a reliable endpoint
    void* socket;
    socket.connectToHost("8.8.8.8", 53); // Google DNS
    
    if (socket.waitForConnected(3000)) {
        result.success = true;
        result.latencyMs = timer.elapsed();
        result.message = std::string("Network connectivity OK (%1ms)");
        result.technicalDetails = "Successfully connected to 8.8.8.8:53 (Google DNS)";
        socket.close();
    } else {
        // Network might be local-only, which is OK
        result.success = true; // Don't fail on this
        result.message = "Limited network connectivity (local only)";
        result.technicalDetails = std::string("Cannot reach internet: %1"));
        m_lastReport.warnings.append("Internet connectivity limited - cloud LLM features may not work");
    }
    
    checkProgress("Network", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkDiskSpace()
{
    HealthCheckResult result;
    result.subsystem = "Disk Space";
    result.timestamp = // DateTime::currentDateTime();
    
    checkProgress("Disk Space", 50, "Checking available disk space...");
    
    std::string projectRoot = SettingsManager::instance().getValue("project/default_root", "").toString();
    QStorageInfo storage(projectRoot);
    
    if (storage.isValid() && storage.isReady()) {
        int64_t availableBytes = storage.bytesAvailable();
        int64_t totalBytes = storage.bytesTotal();
        double usedPercent = 100.0 - (100.0 * availableBytes / totalBytes);
        
        // Require at least 10GB free for model operations
        const int64_t requiredBytes = 10LL * 1024 * 1024 * 1024;
        
        if (availableBytes >= requiredBytes) {
            result.success = true;
            result.message = std::string("Sufficient disk space: %1 available"));
            result.technicalDetails = std::string("Mount: %1\nTotal: %2\nUsed: %3 (%4%)\nAvailable: %5")
                )
                )
                )
                
                );
        } else {
            result.success = false;
            result.message = std::string("Low disk space: only %1 available (need 10GB)"));
            result.technicalDetails = std::string("Mount: %1\nAvailable: %2 (need %3)")
                )
                )
                );
        }
    } else {
        result.success = false;
        result.message = "Cannot determine disk space";
        result.technicalDetails = std::string("Storage info unavailable for: %1");
    }
    
    checkProgress("Disk Space", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkModelCache()
{
    HealthCheckResult result;
    result.subsystem = "Model Cache";
    result.timestamp = // DateTime::currentDateTime();
    
    checkProgress("Model Cache", 60, "Validating model cache directory...");
    
    std::string cacheDir = qEnvironmentVariable("RAWRXD_MODEL_CACHE");
    if (cacheDir.empty()) {
        cacheDir = SettingsManager::instance().getValue("model/cache_dir", 
                    QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/models").toString();
    }
    
    // cache(cacheDir);
    
    if (!cache.exists()) {
        // Try to create it
        if (cache.mkpath(".")) {
            result.success = true;
            result.message = std::string("Model cache created: %1");
            result.technicalDetails = std::string("Directory created at: %1"));
        } else {
            result.success = false;
            result.message = std::string("Cannot create model cache: %1");
            result.technicalDetails = std::string("Failed to create directory: %1"));
        }
    } else {
        // Check if writable
        // Info cacheInfo(cacheDir);
        if (cacheInfo.isWritable()) {
            // Count cached models
            std::stringList models = cache.entryList(std::stringList() << "*.gguf" << "*.bin", // Dir::Files);
            
            result.success = true;
            result.message = std::string("Model cache ready: %1 models cached"));
            result.technicalDetails = std::string("Path: %1\nCached models: %2\nWritable: Yes")
                )
                );
        } else {
            result.success = false;
            result.message = std::string("Model cache not writable: %1");
            result.technicalDetails = std::string("Path: %1 (read-only)"));
        }
    }
    
    checkProgress("Model Cache", 100, result.message);
    return result;
}

std::string StartupReadinessChecker::formatBytes(int64_t bytes)
{
    const int64_t KB = 1024;
    const int64_t MB = KB * 1024;
    const int64_t GB = MB * 1024;
    const int64_t TB = GB * 1024;
    
    if (bytes >= TB) {
        return std::string("%1 TB")TB, 0, 'f', 2);
    } else if (bytes >= GB) {
        return std::string("%1 GB")GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return std::string("%1 MB")MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return std::string("%1 KB")KB, 0, 'f', 2);
    } else {
        return std::string("%1 bytes");
    }
}

HealthCheckResult StartupReadinessChecker::runWithRetry(const std::string& subsystem, std::function<HealthCheckResult()> checkFunc)
{
    const int attempts = std::max(1, m_maxRetries);
    HealthCheckResult lastResult;

    for (int attempt = 1; attempt <= attempts; ++attempt) {
        lastResult = checkFunc();
        lastResult.attemptCount = attempt;

        if (lastResult.success) {
            break;
        }

                   << "failed:" << lastResult.message;

        if (attempt < attempts && m_backoffMs > 0) {
            const int sleepMs = m_backoffMs * attempt; // linear backoff
            std::thread::msleep(static_cast<unsigned long>(sleepMs));
        }
    }

    return lastResult;
}

void StartupReadinessChecker::onNetworkReplyFinished(void** reply)
{
    // Handled synchronously in checkLLMEndpoint
    reply->deleteLater();
}

void StartupReadinessChecker::onCheckTimeout()
{
    // Timeout handling is done in individual check functions
}

void StartupReadinessChecker::logReadinessMetrics(const AgentReadinessReport& report)
{
    static EnterpriseMetricsCollector metricsCollector;
    std::map<std::string, std::string> baseTags{{"component", "startup_readiness"}};

    metricsCollector.recordHistogram("startup_readiness_total_latency_ms", report.totalLatency, baseTags);
    metricsCollector.recordCounter("startup_readiness_runs", 1, baseTags);
    metricsCollector.recordCounter("startup_readiness_failures", static_cast<uint64_t>(report.failures.size()), baseTags);
    metricsCollector.recordCounter("startup_readiness_success", report.overallReady ? 1ULL : 0ULL, baseTags);

    for (auto it = report.checks.constBegin(); it != report.checks.constEnd(); ++it) {
        std::map<std::string, std::string> checkTags = baseTags;
        checkTags["subsystem"] = it.key();
        checkTags["status"] = it.value().success ? "ok" : "failed";
        metricsCollector.recordHistogram("startup_readiness_check_latency_ms", it.value().latencyMs, checkTags);
        metricsCollector.recordCounter("startup_readiness_check_attempts", static_cast<uint64_t>(it.value().attemptCount), checkTags);
    }

    std::string appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.empty()) {
        return;
    }

    // dir(appData);
    if (!dir.exists()) {
        dir.mkpath(appData);
    }

    // File operation removed);
    if (!file.open(std::iostream::Append | std::iostream::Text)) {
        return;
    }

    nlohmann::json obj;
    obj["timestamp"] = // DateTime::currentDateTimeUtc().toString(ISODate);
    obj["overallReady"] = report.overallReady;
    obj["totalLatencyMs"] = report.totalLatency;

    nlohmann::json failures;
    for (const auto& f : report.failures) failures.append(f);
    obj["failures"] = failures;

    nlohmann::json warnings;
    for (const auto& w : report.warnings) warnings.append(w);
    obj["warnings"] = warnings;

    nlohmann::json checks;
    for (auto it = report.checks.constBegin(); it != report.checks.constEnd(); ++it) {
        nlohmann::json c;
        c["success"] = it.value().success;
        c["latencyMs"] = it.value().latencyMs;
        c["message"] = it.value().message;
        c["attempts"] = it.value().attemptCount;
        checks[it.key()] = c;
    }
    obj["checks"] = checks;

    file.write(nlohmann::json(obj).toJson(nlohmann::json::Compact));
    file.write("\n");
    file.close();

}

// ==================== StartupReadinessDialog Implementation ====================

StartupReadinessDialog::StartupReadinessDialog(void* parent)
    : void(parent)
    , m_checker(new StartupReadinessChecker(this))
    , m_hotpatchManager(nullptr)
    , m_checksPassed(false)
{
    setWindowTitle("RawrXD IDE - Startup Readiness Check");
    setModal(true);
    setMinimumSize(700, 600);
    
    setupUI();  // Signal connection removed\n  // Signal connection removed\n}

StartupReadinessDialog::~StartupReadinessDialog()
{
}

void StartupReadinessDialog::setupUI()
{
    m_mainLayout = new void(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // Title
    void* titleLabel = new void("<h2>Autonomous Agent Readiness Check</h2>", this);
    titleLabel->setAlignment(AlignCenter);
    m_mainLayout->addWidget(titleLabel);
    
    void* subtitleLabel = new void(
        "Validating all subsystems before enabling autonomous agent operations...", this);
    subtitleLabel->setAlignment(AlignCenter);
    subtitleLabel->setStyleSheet("color: #888; font-size: 10pt;");
    m_mainLayout->addWidget(subtitleLabel);
    
    m_mainLayout->addSpacing(10);
    
    // Overall progress
    m_overallProgress = new void(this);
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    m_overallProgress->setTextVisible(true);
    m_overallProgress->setFormat("Overall Progress: %p%");
    m_mainLayout->addWidget(m_overallProgress);
    
    m_mainLayout->addSpacing(5);
    
    // Checks group
    m_checksGroup = new void("System Checks", this);
    m_checksLayout = new void(m_checksGroup);
    m_checksLayout->setSpacing(5);
    
    // Create status labels for each subsystem
    std::stringList subsystems = {
        "Ollama", "Claude", "OpenAI", "GGUF Server", "Hotpatch Manager",
        "Project Root", "Environment", "Network", "Disk Space", "Model Cache"
    };
    
    for (const std::string& subsystem : subsystems) {
        void* checkLayout = new void();
        
        void* statusIcon = new void("⏳", this);
        statusIcon->setFixedWidth(30);
        statusIcon->setAlignment(AlignCenter);
        checkLayout->addWidget(statusIcon);
        m_statusLabels[subsystem] = statusIcon;
        
        void* nameLabel = new void(subsystem + ":", this);
        nameLabel->setFixedWidth(120);
        checkLayout->addWidget(nameLabel);
        
        void* progressBar = new void(this);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        progressBar->setMaximumHeight(15);
        progressBar->setTextVisible(false);
        checkLayout->addWidget(progressBar, 1);
        m_progressBars[subsystem] = progressBar;
        
        m_checksLayout->addLayout(checkLayout);
    }
    
    m_mainLayout->addWidget(m_checksGroup);
    
    // Diagnostics log
    void* diagLabel = new void("Diagnostic Log:", this);
    m_mainLayout->addWidget(diagLabel);
    
    m_diagnosticsLog = new void(this);
    m_diagnosticsLog->setReadOnly(true);
    m_diagnosticsLog->setMaximumHeight(150);
    m_diagnosticsLog->setStyleSheet(
        "void { "
        "  background-color: #1e1e1e; "
        "  color: #d4d4d4; "
        "  font-family: 'Consolas', 'Courier New', monospace; "
        "  font-size: 9pt; "
        "  border: 1px solid #3e3e3e; "
        "}"
    );
    m_mainLayout->addWidget(m_diagnosticsLog);
    
    // Summary label
    m_summaryLabel = new void(this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet("font-weight: bold; padding: 10px;");
    m_summaryLabel->hide();
    m_mainLayout->addWidget(m_summaryLabel);
    
    // Buttons
    void* buttonLayout = new void();
    buttonLayout->addStretch();
    
    m_retryButton = new void("Retry Failed Checks", this);
    m_retryButton->setEnabled(false);
    m_retryButton->hide();  // Signal connection removed\nbuttonLayout->addWidget(m_retryButton);
    
    m_configureButton = new void("Configure Settings", this);
    m_configureButton->setEnabled(false);
    m_configureButton->hide();  // Signal connection removed\nbuttonLayout->addWidget(m_configureButton);
    
    m_skipButton = new void("Skip && Continue", this);
    m_skipButton->setEnabled(false);
    m_skipButton->hide();  // Signal connection removed\nbuttonLayout->addWidget(m_skipButton);
    
    m_continueButton = new void("Continue", this);
    m_continueButton->setEnabled(false);
    m_continueButton->setDefault(true);  // Signal connection removed\nbuttonLayout->addWidget(m_continueButton);
    
    m_mainLayout->addLayout(buttonLayout);
    
    // Apply dark theme styling
    setStyleSheet(
        "void { background-color: #2d2d30; color: #d4d4d4; }"
        "void { "
        "  border: 1px solid #3e3e3e; "
        "  border-radius: 5px; "
        "  margin-top: 10px; "
        "  padding-top: 10px; "
        "  font-weight: bold; "
        "}"
        "void::title { "
        "  subcontrol-origin: margin; "
        "  subcontrol-position: top left; "
        "  padding: 0 5px; "
        "}"
        "void { color: #d4d4d4; }"
        "void { "
        "  background-color: #0e639c; "
        "  color: white; "
        "  border: none; "
        "  padding: 8px 16px; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}"
        "void:hover { background-color: #1177bb; }"
        "void:pressed { background-color: #0d5484; }"
        "void:disabled { background-color: #3e3e3e; color: #888; }"
        "void { "
        "  border: 1px solid #3e3e3e; "
        "  border-radius: 3px; "
        "  text-align: center; "
        "  background-color: #1e1e1e; "
        "}"
        "void::chunk { "
        "  background-color: #0e639c; "
        "  border-radius: 2px; "
        "}"
    );
}

bool StartupReadinessDialog::runChecks(UnifiedHotpatchManager* hotpatchManager, 
                                        const std::string& projectRoot)
{
    m_hotpatchManager = hotpatchManager;
    m_projectRoot = projectRoot;
    m_checksPassed = false;
    
    m_diagnosticsLog->append(std::string("[%1] Starting startup readiness checks...")
        .toString("HH:mm:ss")));
    
    // Start checks
    m_checker->runChecks();
    
    // Also run hotpatch manager check manually since it needs the pointer
    // Timer::singleShot(100, this, [this, hotpatchManager]() {
        HealthCheckResult hotpatchResult;
        hotpatchResult.subsystem = "Hotpatch Manager";
        hotpatchResult.timestamp = // DateTime::currentDateTime();
        
        if (!hotpatchManager) {
            hotpatchResult.success = false;
            hotpatchResult.message = "Hotpatch Manager not initialized";
            hotpatchResult.technicalDetails = "Manager pointer is null";
        } else {
            hotpatchResult.success = true;
            hotpatchResult.message = "Hotpatch Manager initialized";
            hotpatchResult.technicalDetails = "Manager instance exists and is accessible";
        }
        
        m_report.checks["Hotpatch Manager"] = hotpatchResult;
        updateSubsystemStatus("Hotpatch Manager", hotpatchResult.success, hotpatchResult.message);
    });

    const bool headlessMode = SettingsManager::instance()
        .getValue("AppSettings/readiness.headless_mode", false).toBool()
        || qEnvironmentVariableIsSet("RAWRXD_HEADLESS_READINESS");

    if (headlessMode) {
        AgentReadinessReport headlessReport;
        voidLoop loop;
        QMetaObject::Connection conn = // Connect removed {
                headlessReport = report;
                m_report = report;
                m_checksPassed = report.overallReady;
                if (!report.overallReady) {
                } else {
                }
                loop.quit();
            });

        loop.exec();
        dis  // Signal connection removed\nreturn true; // auto-continue with warnings if needed
    }
    
    // Show dialog and wait
    int result = exec();
    
    return result == void::Accepted && m_checksPassed;
}

void StartupReadinessDialog::onCheckProgress(const std::string& subsystem, int progress, const std::string& message)
{
    if (m_progressBars.contains(subsystem)) {
        m_progressBars[subsystem]->setValue(progress);
    }
    
    m_diagnosticsLog->append(std::string("[%1] %2: %3")
        .toString("HH:mm:ss"))
        
        );
    
    // Update overall progress
    int totalProgress = 0;
    for (auto bar : m_progressBars) {
        totalProgress += bar->value();
    }
    m_overallProgress->setValue(totalProgress / m_progressBars.size());
}

void StartupReadinessDialog::onReadinessComplete(const AgentReadinessReport& report)
{
    m_report = report;
    m_checksPassed = report.overallReady;
    
    m_diagnosticsLog->append(std::string("\n[%1] ========== CHECK SUMMARY ==========")
        .toString("HH:mm:ss")));
    
    // Update status icons
    for (auto it = report.checks.constBegin(); it != report.checks.constEnd(); ++it) {
        updateSubsystemStatus(it.key(), it.value().success, it.value().message);
        
        m_diagnosticsLog->append(std::string("[%1] %2: %3")
            .success ? "✓" : "✗")
            )
            .message));
        
        if (!it.value().technicalDetails.empty()) {
            m_diagnosticsLog->append(std::string("    Details: %1").technicalDetails));
        }
    }
    
    m_overallProgress->setValue(100);
    
    showFinalSummary();
}

void StartupReadinessDialog::updateSubsystemStatus(const std::string& subsystem, bool success, const std::string& message)
{
    if (m_statusLabels.contains(subsystem)) {
        m_statusLabels[subsystem]->setText(success ? "✓" : "✗");
        m_statusLabels[subsystem]->setStyleSheet(success ? "color: #4ec9b0;" : "color: #f48771;");
    }
    if (m_progressBars.contains(subsystem)) {
        m_progressBars[subsystem]->setValue(100);
    }
}

void StartupReadinessDialog::showFinalSummary()
{
    m_summaryLabel->show();
    
    if (m_report.overallReady) {
        m_summaryLabel->setText("✓ All systems ready! Autonomous agents can start safely.");
        m_summaryLabel->setStyleSheet(
            "background-color: #1a3d1a; color: #4ec9b0; "
            "border: 1px solid #4ec9b0; border-radius: 4px; "
            "padding: 10px; font-weight: bold;");
        m_continueButton->setEnabled(true);
        m_continueButton->setText("Start IDE");
    } else {
        int failureCount = m_report.failures.size();
        m_summaryLabel->setText(
            std::string("⚠ %1 critical check(s) failed. Some features may be unavailable.\n"
                    "Failed: %2")
            
            ));
        m_summaryLabel->setStyleSheet(
            "background-color: #3d1a1a; color: #f48771; "
            "border: 1px solid #f48771; border-radius: 4px; "
            "padding: 10px; font-weight: bold;");
        
        m_retryButton->setEnabled(true);
        m_retryButton->show();
        m_configureButton->setEnabled(true);
        m_configureButton->show();
        m_skipButton->setEnabled(true);
        m_skipButton->show();
        m_continueButton->setEnabled(true);
        m_continueButton->setText("Continue Anyway");
    }
    
    if (!m_report.warnings.empty()) {
        m_diagnosticsLog->append(std::string("\n[WARNINGS]"));
        for (const std::string& warning : m_report.warnings) {
            m_diagnosticsLog->append(std::string("  ⚠ %1"));
        }
    }
    
    m_diagnosticsLog->append(std::string("\nTotal validation time: %1ms"));
}

void StartupReadinessDialog::onRetryClicked()
{
    // Reset failed checks
    for (const std::string& failed : m_report.failures) {
        if (m_statusLabels.contains(failed)) {
            m_statusLabels[failed]->setText("⏳");
            m_statusLabels[failed]->setStyleSheet("");
        }
        if (m_progressBars.contains(failed)) {
            m_progressBars[failed]->setValue(0);
        }
    }
    
    m_diagnosticsLog->append(std::string("\n[%1] Retrying failed checks...")
        .toString("HH:mm:ss")));
    
    m_summaryLabel->hide();
    m_retryButton->setEnabled(false);
    m_configureButton->setEnabled(false);
    m_skipButton->setEnabled(false);
    m_continueButton->setEnabled(false);
    
    // Restart checks
    m_checker->runChecks();
}

void StartupReadinessDialog::onSkipClicked()
{
    m_checksPassed = false; // Mark as not fully passed
    accept(); // But allow continuation
}

void StartupReadinessDialog::onConfigureClicked()
{
    // Open settings dialog (would need to implement settings UI)
    void::information(this, "Configure Settings",
        "Settings configuration dialog would open here.\n\n"
        "For now, please configure settings via:\n"
        "1. Environment variables (RAWRXD_PROJECT_ROOT, etc.)\n"
        "2. Settings file: " + 
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/RawrXD.ini");
}

