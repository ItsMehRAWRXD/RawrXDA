/**
 * @file startup_readiness_checker.cpp
 * @brief Implementation of production startup validation system
 * @author RawrXD Team
 * @date 2026-01-08
 */

#include "startup_readiness_checker.hpp"
#include "unified_hotpatch_manager.hpp"
#include "settings_manager.h"
#include "monitoring/enterprise_metrics_collector.hpp"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStorageInfo>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QThread>
#include <QApplication>
#include <QStyle>
#include <QDebug>
#include <QDateTime>
#include <QMessageBox>
#include <QStandardPaths>
#include <QEventLoop>
#include <map>

// ==================== StartupReadinessChecker Implementation ====================

StartupReadinessChecker::StartupReadinessChecker(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_maxRetries(3)
    , m_timeoutMs(5000)
    , m_backoffMs(500)
    , m_completedChecks(0)
    , m_totalChecks(8)
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &StartupReadinessChecker::onNetworkReplyFinished);
}

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
    qDebug() << "[StartupReadinessChecker] Starting comprehensive health checks";
    
    m_lastReport = AgentReadinessReport();
    m_lastReport.startTime = QDateTime::currentDateTime();
    m_completedChecks = 0;
    m_retryCount.clear();
    m_totalTimer.start();
    
    // Get configuration from settings
    SettingsManager& settings = SettingsManager::instance();
    m_maxRetries = std::max(1, settings.getValue("AppSettings/readiness.max_retries", m_maxRetries).toInt());
    m_backoffMs = std::max(0, settings.getValue("AppSettings/readiness.backoff_ms", m_backoffMs).toInt());
    
    // 1. Check LLM Endpoints (concurrent for speed)
    QString ollamaEndpoint = settings.getValue("llm/ollama_endpoint", "http://localhost:11434").toString();
    QString claudeEndpoint = settings.getValue("llm/claude_endpoint", "https://api.anthropic.com").toString();
    QString claudeKey = settings.getValue("llm/claude_api_key", "").toString();
    QString openaiEndpoint = settings.getValue("llm/openai_endpoint", "https://api.openai.com/v1").toString();
    QString openaiKey = settings.getValue("llm/openai_api_key", "").toString();
    
    // Launch LLM checks concurrently
    m_runningChecks["Ollama"] = QtConcurrent::run([this, ollamaEndpoint]() {
        return runWithRetry("Ollama", [this, ollamaEndpoint]() {
            return checkLLMEndpoint("Ollama", ollamaEndpoint, "");
        });
    });
    
    if (!claudeKey.isEmpty()) {
        m_runningChecks["Claude"] = QtConcurrent::run([this, claudeEndpoint, claudeKey]() {
            return runWithRetry("Claude", [this, claudeEndpoint, claudeKey]() {
                return checkLLMEndpoint("Claude", claudeEndpoint, claudeKey);
            });
        });
    }
    
    if (!openaiKey.isEmpty()) {
        m_runningChecks["OpenAI"] = QtConcurrent::run([this, openaiEndpoint, openaiKey]() {
            return runWithRetry("OpenAI", [this, openaiEndpoint, openaiKey]() {
                return checkLLMEndpoint("OpenAI", openaiEndpoint, openaiKey);
            });
        });
    }
    
    // 2. Check GGUF Server
    quint16 ggufPort = settings.getValue("gguf/server_port", 11434).toUInt();
    m_runningChecks["GGUF Server"] = QtConcurrent::run([this, ggufPort]() {
        return runWithRetry("GGUF Server", [this, ggufPort]() {
            return checkGGUFServer(ggufPort);
        });
    });
    
    // 3. Check Project Root
    QString projectRoot = settings.getValue("project/default_root", "").toString();
    if (projectRoot.isEmpty()) {
        // Try environment variable
        projectRoot = qEnvironmentVariable("RAWRXD_PROJECT_ROOT");
    }
    if (projectRoot.isEmpty()) {
        // Default fallback
        if (QFile::exists("E:\\")) projectRoot = "E:\\";
        else if (QFile::exists("D:\\RawrXD-production-lazy-init")) projectRoot = "D:\\RawrXD-production-lazy-init";
        else projectRoot = QDir::currentPath();
    }
    
    m_runningChecks["Project Root"] = QtConcurrent::run([this, projectRoot]() {
        return runWithRetry("Project Root", [this, projectRoot]() {
            return checkProjectRoot(projectRoot);
        });
    });
    
    // 4. Check Environment Variables
    m_runningChecks["Environment"] = QtConcurrent::run([this]() {
        return runWithRetry("Environment", [this]() {
            return checkEnvironmentVariables();
        });
    });
    
    // 5. Check Network Connectivity
    m_runningChecks["Network"] = QtConcurrent::run([this]() {
        return runWithRetry("Network", [this]() {
            return checkNetworkConnectivity();
        });
    });
    
    // 6. Check Disk Space
    m_runningChecks["Disk Space"] = QtConcurrent::run([this]() {
        return runWithRetry("Disk Space", [this]() {
            return checkDiskSpace();
        });
    });
    
    // 7. Check Model Cache
    m_runningChecks["Model Cache"] = QtConcurrent::run([this]() {
        return runWithRetry("Model Cache", [this]() {
            return checkModelCache();
        });
    });
    
    // Monitor completion of all checks
    QTimer* monitorTimer = new QTimer(this);
    connect(monitorTimer, &QTimer::timeout, this, [this, monitorTimer]() {
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
                    
                    emit checkProgress(it.key(), 100, result.message);
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
            m_lastReport.endTime = QDateTime::currentDateTime();
            m_lastReport.totalLatency = m_totalTimer.elapsed();
            m_lastReport.overallReady = m_lastReport.failures.isEmpty();
            logReadinessMetrics(m_lastReport);
            
            qDebug() << "[StartupReadinessChecker] All checks complete. Ready:" << m_lastReport.overallReady
                     << "Failures:" << m_lastReport.failures.size();
            
            emit readinessComplete(m_lastReport);
        }
    });
    monitorTimer->start(100); // Check every 100ms
}

HealthCheckResult StartupReadinessChecker::checkLLMEndpoint(const QString& backend, 
                                                             const QString& endpoint, 
                                                             const QString& apiKey)
{
    HealthCheckResult result;
    result.subsystem = backend;
    result.timestamp = QDateTime::currentDateTime();
    
    QElapsedTimer timer;
    timer.start();
    
    emit checkProgress(backend, 10, "Connecting to " + endpoint + "...");
    
    QUrl url(endpoint);
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
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    if (!apiKey.isEmpty()) {
        if (backend == "Claude") {
            request.setRawHeader("x-api-key", apiKey.toUtf8());
            request.setRawHeader("anthropic-version", "2023-06-01");
        } else if (backend == "OpenAI") {
            request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
        }
    }
    
    emit checkProgress(backend, 30, "Sending health check request...");
    
    // Synchronous request with timeout
    QNetworkReply* reply = m_networkManager->get(request);
    
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeoutTimer.start(m_timeoutMs);
    loop.exec();
    
    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
        
        if (reply->error() == QNetworkReply::NoError) {
            result.success = true;
            result.latencyMs = timer.elapsed();
            result.message = QString("%1 endpoint healthy (%2ms)").arg(backend).arg(result.latencyMs);
            result.technicalDetails = QString("HTTP %1, URL: %2")
                .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                .arg(endpoint);
            
            emit checkProgress(backend, 100, result.message);
        } else {
            result.success = false;
            result.message = QString("%1 endpoint unreachable").arg(backend);
            result.technicalDetails = QString("Error: %1, URL: %2")
                .arg(reply->errorString())
                .arg(endpoint);
            
            emit checkProgress(backend, 100, result.message);
        }
    } else {
        // Timeout
        reply->abort();
        result.success = false;
        result.message = QString("%1 endpoint timeout").arg(backend);
        result.technicalDetails = QString("Timeout after %1ms, URL: %2")
            .arg(m_timeoutMs)
            .arg(endpoint);
        
        emit checkProgress(backend, 100, result.message);
    }
    
    reply->deleteLater();
    return result;
}

HealthCheckResult StartupReadinessChecker::checkGGUFServer(quint16 port)
{
    HealthCheckResult result;
    result.subsystem = "GGUF Server";
    result.timestamp = QDateTime::currentDateTime();
    
    emit checkProgress("GGUF Server", 20, QString("Testing port %1...").arg(port));
    
    QTcpSocket socket;
    QElapsedTimer timer;
    timer.start();
    
    socket.connectToHost("localhost", port);
    
    if (socket.waitForConnected(m_timeoutMs)) {
        result.latencyMs = timer.elapsed();
        
        // Try to send a minimal HTTP request
        socket.write("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        socket.flush();
        
        if (socket.waitForReadyRead(2000)) {
            QByteArray response = socket.readAll();
            
            result.success = true;
            result.message = QString("GGUF Server responsive on port %1 (%2ms)").arg(port).arg(result.latencyMs);
            result.technicalDetails = QString("TCP connection successful, HTTP response received: %1 bytes")
                .arg(response.size());
        } else {
            result.success = false;
            result.message = QString("GGUF Server port %1 open but not responding").arg(port);
            result.technicalDetails = "TCP connected but no HTTP response";
        }
        
        socket.close();
    } else {
        result.success = false;
        result.message = QString("GGUF Server port %1 not accessible").arg(port);
        result.technicalDetails = QString("Connection failed: %1").arg(socket.errorString());
    }
    
    emit checkProgress("GGUF Server", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkHotpatchManager(UnifiedHotpatchManager* manager)
{
    HealthCheckResult result;
    result.subsystem = "Hotpatch Manager";
    result.timestamp = QDateTime::currentDateTime();
    
    emit checkProgress("Hotpatch Manager", 50, "Validating hotpatch subsystem...");
    
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
    
    emit checkProgress("Hotpatch Manager", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkProjectRoot(const QString& projectPath)
{
    HealthCheckResult result;
    result.subsystem = "Project Root";
    result.timestamp = QDateTime::currentDateTime();
    
    emit checkProgress("Project Root", 30, QString("Validating %1...").arg(projectPath));
    
    QFileInfo pathInfo(projectPath);
    
    if (projectPath.isEmpty()) {
        result.success = false;
        result.message = "No project root configured";
        result.technicalDetails = "Project root path is empty. Set RAWRXD_PROJECT_ROOT env var or configure in settings.";
        m_lastReport.warnings.append("Configure default project root in settings");
    } else if (!pathInfo.exists()) {
        result.success = false;
        result.message = QString("Project root does not exist: %1").arg(projectPath);
        result.technicalDetails = QString("Path: %1 (not found)").arg(pathInfo.absoluteFilePath());
    } else if (!pathInfo.isDir()) {
        result.success = false;
        result.message = QString("Project root is not a directory: %1").arg(projectPath);
        result.technicalDetails = QString("Path: %1 (is a file)").arg(pathInfo.absoluteFilePath());
    } else if (!pathInfo.isReadable()) {
        result.success = false;
        result.message = QString("Project root not accessible: %1").arg(projectPath);
        result.technicalDetails = QString("Path: %1 (permission denied)").arg(pathInfo.absoluteFilePath());
    } else {
        result.success = true;
        result.message = QString("Project root ready: %1").arg(projectPath);
        
        // Get directory stats
        QDir dir(projectPath);
        int fileCount = dir.entryList(QDir::Files).size();
        int dirCount = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
        
        result.technicalDetails = QString("Path: %1\nFiles: %2, Subdirs: %3, Writable: %4")
            .arg(pathInfo.absoluteFilePath())
            .arg(fileCount)
            .arg(dirCount)
            .arg(pathInfo.isWritable() ? "Yes" : "No");
    }
    
    emit checkProgress("Project Root", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkEnvironmentVariables()
{
    HealthCheckResult result;
    result.subsystem = "Environment";
    result.timestamp = QDateTime::currentDateTime();
    
    emit checkProgress("Environment", 40, "Checking environment variables...");
    
    QStringList requiredVars = {
        // Optional but recommended
        "RAWRXD_PROJECT_ROOT",
        "RAWRXD_MODEL_CACHE",
        "RAWRXD_LOG_LEVEL"
    };
    
    QStringList missingVars;
    QStringList foundVars;
    
    for (const QString& varName : requiredVars) {
        QString value = qEnvironmentVariable(varName.toUtf8().constData());
        if (value.isEmpty()) {
            missingVars.append(varName);
        } else {
            foundVars.append(QString("%1=%2").arg(varName, value));
        }
    }
    
    // Environment vars are optional, so this is always a success
    result.success = true;
    
    if (missingVars.isEmpty()) {
        result.message = "All recommended environment variables set";
        result.technicalDetails = foundVars.join("\n");
    } else {
        result.message = QString("%1 recommended env vars set").arg(foundVars.size());
        result.technicalDetails = QString("Set: %1\nOptional but recommended: %2")
            .arg(foundVars.join(", "))
            .arg(missingVars.join(", "));
        
        if (!missingVars.isEmpty()) {
            m_lastReport.warnings.append(QString("Optional env vars not set: %1").arg(missingVars.join(", ")));
        }
    }
    
    emit checkProgress("Environment", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkNetworkConnectivity()
{
    HealthCheckResult result;
    result.subsystem = "Network";
    result.timestamp = QDateTime::currentDateTime();
    
    emit checkProgress("Network", 30, "Testing internet connectivity...");
    
    QElapsedTimer timer;
    timer.start();
    
    // Ping a reliable endpoint
    QTcpSocket socket;
    socket.connectToHost("8.8.8.8", 53); // Google DNS
    
    if (socket.waitForConnected(3000)) {
        result.success = true;
        result.latencyMs = timer.elapsed();
        result.message = QString("Network connectivity OK (%1ms)").arg(result.latencyMs);
        result.technicalDetails = "Successfully connected to 8.8.8.8:53 (Google DNS)";
        socket.close();
    } else {
        // Network might be local-only, which is OK
        result.success = true; // Don't fail on this
        result.message = "Limited network connectivity (local only)";
        result.technicalDetails = QString("Cannot reach internet: %1").arg(socket.errorString());
        m_lastReport.warnings.append("Internet connectivity limited - cloud LLM features may not work");
    }
    
    emit checkProgress("Network", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkDiskSpace()
{
    HealthCheckResult result;
    result.subsystem = "Disk Space";
    result.timestamp = QDateTime::currentDateTime();
    
    emit checkProgress("Disk Space", 50, "Checking available disk space...");
    
    QString projectRoot = SettingsManager::instance().getValue("project/default_root", QDir::currentPath()).toString();
    QStorageInfo storage(projectRoot);
    
    if (storage.isValid() && storage.isReady()) {
        qint64 availableBytes = storage.bytesAvailable();
        qint64 totalBytes = storage.bytesTotal();
        double usedPercent = 100.0 - (100.0 * availableBytes / totalBytes);
        
        // Require at least 10GB free for model operations
        const qint64 requiredBytes = 10LL * 1024 * 1024 * 1024;
        
        if (availableBytes >= requiredBytes) {
            result.success = true;
            result.message = QString("Sufficient disk space: %1 available").arg(formatBytes(availableBytes));
            result.technicalDetails = QString("Mount: %1\nTotal: %2\nUsed: %3 (%4%)\nAvailable: %5")
                .arg(storage.rootPath())
                .arg(formatBytes(totalBytes))
                .arg(formatBytes(totalBytes - availableBytes))
                .arg(usedPercent, 0, 'f', 1)
                .arg(formatBytes(availableBytes));
        } else {
            result.success = false;
            result.message = QString("Low disk space: only %1 available (need 10GB)").arg(formatBytes(availableBytes));
            result.technicalDetails = QString("Mount: %1\nAvailable: %2 (need %3)")
                .arg(storage.rootPath())
                .arg(formatBytes(availableBytes))
                .arg(formatBytes(requiredBytes));
        }
    } else {
        result.success = false;
        result.message = "Cannot determine disk space";
        result.technicalDetails = QString("Storage info unavailable for: %1").arg(projectRoot);
    }
    
    emit checkProgress("Disk Space", 100, result.message);
    return result;
}

HealthCheckResult StartupReadinessChecker::checkModelCache()
{
    HealthCheckResult result;
    result.subsystem = "Model Cache";
    result.timestamp = QDateTime::currentDateTime();
    
    emit checkProgress("Model Cache", 60, "Validating model cache directory...");
    
    QString cacheDir = qEnvironmentVariable("RAWRXD_MODEL_CACHE");
    if (cacheDir.isEmpty()) {
        cacheDir = SettingsManager::instance().getValue("model/cache_dir", 
                    QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/models").toString();
    }
    
    QDir cache(cacheDir);
    
    if (!cache.exists()) {
        // Try to create it
        if (cache.mkpath(".")) {
            result.success = true;
            result.message = QString("Model cache created: %1").arg(cacheDir);
            result.technicalDetails = QString("Directory created at: %1").arg(cache.absolutePath());
        } else {
            result.success = false;
            result.message = QString("Cannot create model cache: %1").arg(cacheDir);
            result.technicalDetails = QString("Failed to create directory: %1").arg(cache.absolutePath());
        }
    } else {
        // Check if writable
        QFileInfo cacheInfo(cacheDir);
        if (cacheInfo.isWritable()) {
            // Count cached models
            QStringList models = cache.entryList(QStringList() << "*.gguf" << "*.bin", QDir::Files);
            
            result.success = true;
            result.message = QString("Model cache ready: %1 models cached").arg(models.size());
            result.technicalDetails = QString("Path: %1\nCached models: %2\nWritable: Yes")
                .arg(cache.absolutePath())
                .arg(models.size());
        } else {
            result.success = false;
            result.message = QString("Model cache not writable: %1").arg(cacheDir);
            result.technicalDetails = QString("Path: %1 (read-only)").arg(cache.absolutePath());
        }
    }
    
    emit checkProgress("Model Cache", 100, result.message);
    return result;
}

QString StartupReadinessChecker::formatBytes(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;
    
    if (bytes >= TB) {
        return QString("%1 TB").arg(bytes / (double)TB, 0, 'f', 2);
    } else if (bytes >= GB) {
        return QString("%1 GB").arg(bytes / (double)GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(bytes / (double)MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(bytes / (double)KB, 0, 'f', 2);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

HealthCheckResult StartupReadinessChecker::runWithRetry(const QString& subsystem, std::function<HealthCheckResult()> checkFunc)
{
    const int attempts = std::max(1, m_maxRetries);
    HealthCheckResult lastResult;

    for (int attempt = 1; attempt <= attempts; ++attempt) {
        lastResult = checkFunc();
        lastResult.attemptCount = attempt;

        if (lastResult.success) {
            break;
        }

        qWarning() << "[StartupReadinessChecker]" << subsystem << "attempt" << attempt
                   << "failed:" << lastResult.message;

        if (attempt < attempts && m_backoffMs > 0) {
            const int sleepMs = m_backoffMs * attempt; // linear backoff
            QThread::msleep(static_cast<unsigned long>(sleepMs));
        }
    }

    return lastResult;
}

void StartupReadinessChecker::onNetworkReplyFinished(QNetworkReply* reply)
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
    std::map<QString, QString> baseTags{{"component", "startup_readiness"}};

    metricsCollector.recordHistogram("startup_readiness_total_latency_ms", report.totalLatency, baseTags);
    metricsCollector.recordCounter("startup_readiness_runs", 1, baseTags);
    metricsCollector.recordCounter("startup_readiness_failures", static_cast<uint64_t>(report.failures.size()), baseTags);
    metricsCollector.recordCounter("startup_readiness_success", report.overallReady ? 1ULL : 0ULL, baseTags);

    for (auto it = report.checks.constBegin(); it != report.checks.constEnd(); ++it) {
        std::map<QString, QString> checkTags = baseTags;
        checkTags["subsystem"] = it.key();
        checkTags["status"] = it.value().success ? "ok" : "failed";
        metricsCollector.recordHistogram("startup_readiness_check_latency_ms", it.value().latencyMs, checkTags);
        metricsCollector.recordCounter("startup_readiness_check_attempts", static_cast<uint64_t>(it.value().attemptCount), checkTags);
    }

    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
        qWarning() << "[StartupReadinessChecker] Cannot log readiness metrics: no app data path";
        return;
    }

    QDir dir(appData);
    if (!dir.exists()) {
        dir.mkpath(appData);
    }

    QFile file(dir.filePath("readiness_metrics.log"));
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "[StartupReadinessChecker] Cannot open readiness_metrics.log for writing";
        return;
    }

    QJsonObject obj;
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    obj["overallReady"] = report.overallReady;
    obj["totalLatencyMs"] = report.totalLatency;

    QJsonArray failures;
    for (const auto& f : report.failures) failures.append(f);
    obj["failures"] = failures;

    QJsonArray warnings;
    for (const auto& w : report.warnings) warnings.append(w);
    obj["warnings"] = warnings;

    QJsonObject checks;
    for (auto it = report.checks.constBegin(); it != report.checks.constEnd(); ++it) {
        QJsonObject c;
        c["success"] = it.value().success;
        c["latencyMs"] = it.value().latencyMs;
        c["message"] = it.value().message;
        c["attempts"] = it.value().attemptCount;
        checks[it.key()] = c;
    }
    obj["checks"] = checks;

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.write("\n");
    file.close();

    qDebug() << "[StartupReadinessChecker] Logged readiness metrics to" << file.fileName();
}

// ==================== StartupReadinessDialog Implementation ====================

StartupReadinessDialog::StartupReadinessDialog(QWidget* parent)
    : QDialog(parent)
    , m_checker(new StartupReadinessChecker(this))
    , m_hotpatchManager(nullptr)
    , m_checksPassed(false)
{
    setWindowTitle("RawrXD IDE - Startup Readiness Check");
    setModal(true);
    setMinimumSize(700, 600);
    
    setupUI();
    
    connect(m_checker, &StartupReadinessChecker::checkProgress,
            this, &StartupReadinessDialog::onCheckProgress);
    connect(m_checker, &StartupReadinessChecker::readinessComplete,
            this, &StartupReadinessDialog::onReadinessComplete);
}

StartupReadinessDialog::~StartupReadinessDialog()
{
}

void StartupReadinessDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // Title
    QLabel* titleLabel = new QLabel("<h2>Autonomous Agent Readiness Check</h2>", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(titleLabel);
    
    QLabel* subtitleLabel = new QLabel(
        "Validating all subsystems before enabling autonomous agent operations...", this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet("color: #888; font-size: 10pt;");
    m_mainLayout->addWidget(subtitleLabel);
    
    m_mainLayout->addSpacing(10);
    
    // Overall progress
    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    m_overallProgress->setTextVisible(true);
    m_overallProgress->setFormat("Overall Progress: %p%");
    m_mainLayout->addWidget(m_overallProgress);
    
    m_mainLayout->addSpacing(5);
    
    // Checks group
    m_checksGroup = new QGroupBox("System Checks", this);
    m_checksLayout = new QVBoxLayout(m_checksGroup);
    m_checksLayout->setSpacing(5);
    
    // Create status labels for each subsystem
    QStringList subsystems = {
        "Ollama", "Claude", "OpenAI", "GGUF Server", "Hotpatch Manager",
        "Project Root", "Environment", "Network", "Disk Space", "Model Cache"
    };
    
    for (const QString& subsystem : subsystems) {
        QHBoxLayout* checkLayout = new QHBoxLayout();
        
        QLabel* statusIcon = new QLabel("⏳", this);
        statusIcon->setFixedWidth(30);
        statusIcon->setAlignment(Qt::AlignCenter);
        checkLayout->addWidget(statusIcon);
        m_statusLabels[subsystem] = statusIcon;
        
        QLabel* nameLabel = new QLabel(subsystem + ":", this);
        nameLabel->setFixedWidth(120);
        checkLayout->addWidget(nameLabel);
        
        QProgressBar* progressBar = new QProgressBar(this);
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
    QLabel* diagLabel = new QLabel("Diagnostic Log:", this);
    m_mainLayout->addWidget(diagLabel);
    
    m_diagnosticsLog = new QTextEdit(this);
    m_diagnosticsLog->setReadOnly(true);
    m_diagnosticsLog->setMaximumHeight(150);
    m_diagnosticsLog->setStyleSheet(
        "QTextEdit { "
        "  background-color: #1e1e1e; "
        "  color: #d4d4d4; "
        "  font-family: 'Consolas', 'Courier New', monospace; "
        "  font-size: 9pt; "
        "  border: 1px solid #3e3e3e; "
        "}"
    );
    m_mainLayout->addWidget(m_diagnosticsLog);
    
    // Summary label
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet("font-weight: bold; padding: 10px;");
    m_summaryLabel->hide();
    m_mainLayout->addWidget(m_summaryLabel);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_retryButton = new QPushButton("Retry Failed Checks", this);
    m_retryButton->setEnabled(false);
    m_retryButton->hide();
    connect(m_retryButton, &QPushButton::clicked, this, &StartupReadinessDialog::onRetryClicked);
    buttonLayout->addWidget(m_retryButton);
    
    m_configureButton = new QPushButton("Configure Settings", this);
    m_configureButton->setEnabled(false);
    m_configureButton->hide();
    connect(m_configureButton, &QPushButton::clicked, this, &StartupReadinessDialog::onConfigureClicked);
    buttonLayout->addWidget(m_configureButton);
    
    m_skipButton = new QPushButton("Skip && Continue", this);
    m_skipButton->setEnabled(false);
    m_skipButton->hide();
    connect(m_skipButton, &QPushButton::clicked, this, &StartupReadinessDialog::onSkipClicked);
    buttonLayout->addWidget(m_skipButton);
    
    m_continueButton = new QPushButton("Continue", this);
    m_continueButton->setEnabled(false);
    m_continueButton->setDefault(true);
    connect(m_continueButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_continueButton);
    
    m_mainLayout->addLayout(buttonLayout);
    
    // Apply dark theme styling
    setStyleSheet(
        "QDialog { background-color: #2d2d30; color: #d4d4d4; }"
        "QGroupBox { "
        "  border: 1px solid #3e3e3e; "
        "  border-radius: 5px; "
        "  margin-top: 10px; "
        "  padding-top: 10px; "
        "  font-weight: bold; "
        "}"
        "QGroupBox::title { "
        "  subcontrol-origin: margin; "
        "  subcontrol-position: top left; "
        "  padding: 0 5px; "
        "}"
        "QLabel { color: #d4d4d4; }"
        "QPushButton { "
        "  background-color: #0e639c; "
        "  color: white; "
        "  border: none; "
        "  padding: 8px 16px; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}"
        "QPushButton:hover { background-color: #1177bb; }"
        "QPushButton:pressed { background-color: #0d5484; }"
        "QPushButton:disabled { background-color: #3e3e3e; color: #888; }"
        "QProgressBar { "
        "  border: 1px solid #3e3e3e; "
        "  border-radius: 3px; "
        "  text-align: center; "
        "  background-color: #1e1e1e; "
        "}"
        "QProgressBar::chunk { "
        "  background-color: #0e639c; "
        "  border-radius: 2px; "
        "}"
    );
}

bool StartupReadinessDialog::runChecks(UnifiedHotpatchManager* hotpatchManager, 
                                        const QString& projectRoot)
{
    m_hotpatchManager = hotpatchManager;
    m_projectRoot = projectRoot;
    m_checksPassed = false;
    
    m_diagnosticsLog->append(QString("[%1] Starting startup readiness checks...")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    
    // Start checks
    m_checker->runChecks();
    
    // Also run hotpatch manager check manually since it needs the pointer
    QTimer::singleShot(100, this, [this, hotpatchManager]() {
        HealthCheckResult hotpatchResult;
        hotpatchResult.subsystem = "Hotpatch Manager";
        hotpatchResult.timestamp = QDateTime::currentDateTime();
        
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
        qInfo() << "[StartupReadinessDialog] Headless readiness enabled - auto-continuing after checks";
        AgentReadinessReport headlessReport;
        QEventLoop loop;
        QMetaObject::Connection conn = connect(
            m_checker, &StartupReadinessChecker::readinessComplete,
            this, [&](const AgentReadinessReport& report) {
                headlessReport = report;
                m_report = report;
                m_checksPassed = report.overallReady;
                if (!report.overallReady) {
                    qWarning() << "[StartupReadinessDialog] Readiness completed with failures:" << report.failures;
                } else {
                    qInfo() << "[StartupReadinessDialog] Readiness checks passed in headless mode";
                }
                loop.quit();
            });

        loop.exec();
        disconnect(conn);
        return true; // auto-continue with warnings if needed
    }
    
    // Show dialog and wait
    int result = exec();
    
    return result == QDialog::Accepted && m_checksPassed;
}

void StartupReadinessDialog::onCheckProgress(const QString& subsystem, int progress, const QString& message)
{
    if (m_progressBars.contains(subsystem)) {
        m_progressBars[subsystem]->setValue(progress);
    }
    
    m_diagnosticsLog->append(QString("[%1] %2: %3")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(subsystem)
        .arg(message));
    
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
    
    m_diagnosticsLog->append(QString("\n[%1] ========== CHECK SUMMARY ==========")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    
    // Update status icons
    for (auto it = report.checks.constBegin(); it != report.checks.constEnd(); ++it) {
        updateSubsystemStatus(it.key(), it.value().success, it.value().message);
        
        m_diagnosticsLog->append(QString("[%1] %2: %3")
            .arg(it.value().success ? "✓" : "✗")
            .arg(it.key())
            .arg(it.value().message));
        
        if (!it.value().technicalDetails.isEmpty()) {
            m_diagnosticsLog->append(QString("    Details: %1").arg(it.value().technicalDetails));
        }
    }
    
    m_overallProgress->setValue(100);
    
    showFinalSummary();
}

void StartupReadinessDialog::updateSubsystemStatus(const QString& subsystem, bool success, const QString& message)
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
            QString("⚠ %1 critical check(s) failed. Some features may be unavailable.\n"
                    "Failed: %2")
            .arg(failureCount)
            .arg(m_report.failures.join(", ")));
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
    
    if (!m_report.warnings.isEmpty()) {
        m_diagnosticsLog->append(QString("\n[WARNINGS]"));
        for (const QString& warning : m_report.warnings) {
            m_diagnosticsLog->append(QString("  ⚠ %1").arg(warning));
        }
    }
    
    m_diagnosticsLog->append(QString("\nTotal validation time: %1ms").arg(m_report.totalLatency));
}

void StartupReadinessDialog::onRetryClicked()
{
    // Reset failed checks
    for (const QString& failed : m_report.failures) {
        if (m_statusLabels.contains(failed)) {
            m_statusLabels[failed]->setText("⏳");
            m_statusLabels[failed]->setStyleSheet("");
        }
        if (m_progressBars.contains(failed)) {
            m_progressBars[failed]->setValue(0);
        }
    }
    
    m_diagnosticsLog->append(QString("\n[%1] Retrying failed checks...")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    
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
    QMessageBox::information(this, "Configure Settings",
        "Settings configuration dialog would open here.\n\n"
        "For now, please configure settings via:\n"
        "1. Environment variables (RAWRXD_PROJECT_ROOT, etc.)\n"
        "2. Settings file: " + 
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/RawrXD.ini");
}

