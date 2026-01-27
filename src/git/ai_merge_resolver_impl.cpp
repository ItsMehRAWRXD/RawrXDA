#include "ai_merge_resolver.hpp"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QRegularExpression>

AIMergeResolver::AIMergeResolver(QObject* parent)
    : QObject(parent)
{
    logStructured("INFO", "AIMergeResolver initializing", QJsonObject{{"component", "AIMergeResolver"}});
    logStructured("INFO", "AIMergeResolver initialized successfully", QJsonObject{{"component", "AIMergeResolver"}});
}

AIMergeResolver::~AIMergeResolver()
{
    logStructured("INFO", "AIMergeResolver shutting down", QJsonObject{{"component", "AIMergeResolver"}});
}

void AIMergeResolver::setConfig(const Config& config)
{
    QMutexLocker locker(&m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", QJsonObject{
        {"enableAutoResolve", config.enableAutoResolve},
        {"minConfidenceThreshold", config.minConfidenceThreshold},
        {"maxConflictSize", config.maxConflictSize}
    });
}

AIMergeResolver::Config AIMergeResolver::getConfig() const
{
    QMutexLocker locker(&m_configMutex);
    return m_config;
}

QVector<AIMergeResolver::ConflictBlock> AIMergeResolver::detectConflicts(const QString& filePath)
{
    auto startTime = std::chrono::steady_clock::now();
    QVector<ConflictBlock> conflicts;
    
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            logStructured("ERROR", "Failed to open file for conflict detection", QJsonObject{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            emit errorOccurred(QString("Failed to open file: %1").arg(file.errorString()));
            return conflicts;
        }
        
        QTextStream in(&file);
        QString content = in.readAll();
        file.close();
        
        // Detect conflict markers
        QRegularExpression conflictStart("^<<<<<<< (.+)$", QRegularExpression::MultilineOption);
        QRegularExpression conflictMid("^=======\\s*$", QRegularExpression::MultilineOption);
        QRegularExpression conflictEnd("^>>>>>>> (.+)$", QRegularExpression::MultilineOption);
        
        QStringList lines = content.split('\n');
        int i = 0;
        
        while (i < lines.size()) {
            QRegularExpressionMatch startMatch = conflictStart.match(lines[i]);
            if (startMatch.hasMatch()) {
                ConflictBlock conflict;
                conflict.file = filePath;
                conflict.startLine = i + 1;
                
                QString currentBranch = startMatch.captured(1);
                QStringList currentLines;
                i++;
                
                // Collect current version lines
                while (i < lines.size() && !conflictMid.match(lines[i]).hasMatch()) {
                    currentLines.append(lines[i]);
                    i++;
                }
                
                conflict.currentVersion = currentLines.join("\n");
                
                if (i < lines.size()) {
                    i++; // Skip =======
                }
                
                // Collect incoming version lines
                QStringList incomingLines;
                while (i < lines.size() && !conflictEnd.match(lines[i]).hasMatch()) {
                    incomingLines.append(lines[i]);
                    i++;
                }
                
                conflict.incomingVersion = incomingLines.join("\n");
                
                if (i < lines.size()) {
                    QRegularExpressionMatch endMatch = conflictEnd.match(lines[i]);
                    if (endMatch.hasMatch()) {
                        QString incomingBranch = endMatch.captured(1);
                        conflict.endLine = i + 1;
                        
                        // Extract context (5 lines before and after)
                        int contextStart = qMax(0, conflict.startLine - 6);
                        int contextEnd = qMin(lines.size(), conflict.endLine + 5);
                        conflict.context = lines.mid(contextStart, contextEnd - contextStart).join("\n");
                        
                        conflicts.append(conflict);
                    }
                }
            }
            i++;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.conflictsDetected += conflicts.size();
        }
        
        logStructured("INFO", "Conflicts detected", QJsonObject{
            {"filePath", filePath},
            {"conflictCount", conflicts.size()},
            {"latencyMs", duration.count()}
        });
        
        emit conflictsDetected(conflicts.size());
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception during conflict detection", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Conflict detection failed: %1").arg(e.what()));
    }
    
    return conflicts;
}

AIMergeResolver::Resolution AIMergeResolver::resolveConflict(const ConflictBlock& conflict)
{
    auto startTime = std::chrono::steady_clock::now();
    Resolution resolution;
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Validate conflict size
        int conflictSize = conflict.currentVersion.length() + conflict.incomingVersion.length();
        if (conflictSize > config.maxConflictSize) {
            logStructured("WARN", "Conflict exceeds max size, requires manual resolution", QJsonObject{
                {"conflictSize", conflictSize},
                {"maxSize", config.maxConflictSize}
            });
            resolution.requiresManualReview = true;
            resolution.explanation = "Conflict too large for automated resolution";
            return resolution;
        }
        
        // Prepare AI request
        QJsonObject payload;
        payload["current_version"] = conflict.currentVersion;
        payload["incoming_version"] = conflict.incomingVersion;
        payload["base_version"] = conflict.baseVersion;
        payload["context"] = conflict.context;
        payload["file"] = conflict.file;
        
        logStructured("DEBUG", "Sending conflict resolution request to AI", QJsonObject{
            {"file", conflict.file},
            {"conflictSize", conflictSize}
        });
        
        QJsonObject response = makeAiRequest(config.aiEndpoint + "/resolve-conflict", payload);
        
        resolution.resolvedContent = response["resolved_content"].toString();
        resolution.confidence = response["confidence"].toDouble();
        resolution.strategy = response["strategy"].toString();
        resolution.explanation = response["explanation"].toString();
        resolution.requiresManualReview = resolution.confidence < config.minConfidenceThreshold;
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("conflict_resolution", duration);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.conflictsResolved++;
            if (!resolution.requiresManualReview) {
                m_metrics.autoResolved++;
            } else {
                m_metrics.manualResolved++;
            }
            
            // Update average confidence
            m_metrics.avgResolutionConfidence = 
                (m_metrics.avgResolutionConfidence * (m_metrics.conflictsResolved - 1) + resolution.confidence) 
                / m_metrics.conflictsResolved;
        }
        
        logStructured("INFO", "Conflict resolved", QJsonObject{
            {"file", conflict.file},
            {"confidence", resolution.confidence},
            {"strategy", resolution.strategy},
            {"requiresManualReview", resolution.requiresManualReview},
            {"latencyMs", duration.count()}
        });
        
        // Audit log
        if (config.enableAuditLog) {
            logAudit("conflict_resolved", QJsonObject{
                {"file", conflict.file},
                {"startLine", conflict.startLine},
                {"endLine", conflict.endLine},
                {"resolution", resolution.resolvedContent},
                {"confidence", resolution.confidence},
                {"strategy", resolution.strategy}
            });
        }
        
        emit conflictResolved(resolution);
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Conflict resolution failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Resolution failed: %1").arg(e.what()));
        resolution.requiresManualReview = true;
        resolution.explanation = QString("AI resolution failed: %1").arg(e.what());
    }
    
    return resolution;
}

bool AIMergeResolver::applyResolution(const QString& filePath, const Resolution& resolution, int lineStart, int lineEnd)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            logStructured("ERROR", "Failed to open file for applying resolution", QJsonObject{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            emit errorOccurred(QString("Failed to open file: %1").arg(file.errorString()));
            return false;
        }
        
        QTextStream in(&file);
        QStringList lines;
        while (!in.atEnd()) {
            lines.append(in.readLine());
        }
        file.close();
        
        // Replace conflict block with resolution
        if (lineStart < 1 || lineEnd > lines.size() || lineStart > lineEnd) {
            logStructured("ERROR", "Invalid line range for resolution", QJsonObject{
                {"lineStart", lineStart},
                {"lineEnd", lineEnd},
                {"fileLines", lines.size()}
            });
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            return false;
        }
        
        // Remove conflict lines
        lines.erase(lines.begin() + lineStart - 1, lines.begin() + lineEnd);
        
        // Insert resolved content
        QStringList resolvedLines = resolution.resolvedContent.split('\n');
        for (int i = resolvedLines.size() - 1; i >= 0; i--) {
            lines.insert(lineStart - 1, resolvedLines[i]);
        }
        
        // Write back to file
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            logStructured("ERROR", "Failed to open file for writing resolution", QJsonObject{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            emit errorOccurred(QString("Failed to write file: %1").arg(file.errorString()));
            return false;
        }
        
        QTextStream out(&file);
        for (const QString& line : lines) {
            out << line << "\n";
        }
        file.close();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Resolution applied successfully", QJsonObject{
            {"filePath", filePath},
            {"lineStart", lineStart},
            {"lineEnd", lineEnd},
            {"latencyMs", duration.count()}
        });
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("resolution_applied", QJsonObject{
                {"file", filePath},
                {"lineStart", lineStart},
                {"lineEnd", lineEnd},
                {"resolution", resolution.resolvedContent}
            });
        }
        
        return true;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception applying resolution", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Apply resolution failed: %1").arg(e.what()));
        return false;
    }
}

QJsonObject AIMergeResolver::analyzeSemanticMerge(const QString& base, const QString& current, const QString& incoming)
{
    auto startTime = std::chrono::steady_clock::now();
    QJsonObject analysis;
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        QJsonObject payload;
        payload["base"] = base;
        payload["current"] = current;
        payload["incoming"] = incoming;
        payload["analysis_type"] = "semantic";
        
        logStructured("DEBUG", "Requesting semantic merge analysis", QJsonObject{
            {"baseSize", base.length()},
            {"currentSize", current.length()},
            {"incomingSize", incoming.length()}
        });
        
        analysis = makeAiRequest(config.aiEndpoint + "/analyze-semantic-merge", payload);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Semantic analysis completed", QJsonObject{
            {"hasConflicts", analysis["has_conflicts"].toBool()},
            {"semanticChanges", analysis["semantic_changes"].toArray().size()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Semantic analysis failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Semantic analysis failed: %1").arg(e.what()));
    }
    
    return analysis;
}

QVector<QString> AIMergeResolver::detectBreakingChanges(const QString& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    QVector<QString> breakingChanges;
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        QJsonObject payload;
        payload["diff"] = diff;
        payload["detection_mode"] = "breaking_changes";
        
        logStructured("DEBUG", "Detecting breaking changes", QJsonObject{{"diffSize", diff.length()}});
        
        QJsonObject response = makeAiRequest(config.aiEndpoint + "/detect-breaking-changes", payload);
        
        QJsonArray changesArray = response["breaking_changes"].toArray();
        for (const QJsonValue& value : changesArray) {
            breakingChanges.append(value.toString());
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.breakingChangesDetected += breakingChanges.size();
        }
        
        logStructured("INFO", "Breaking changes detected", QJsonObject{
            {"count", breakingChanges.size()},
            {"latencyMs", duration.count()}
        });
        
        for (const QString& change : breakingChanges) {
            emit breakingChangeDetected(change);
        }
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Breaking change detection failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Breaking change detection failed: %1").arg(e.what()));
    }
    
    return breakingChanges;
}

AIMergeResolver::Metrics AIMergeResolver::getMetrics() const
{
    QMutexLocker locker(&m_metricsMutex);
    return m_metrics;
}

void AIMergeResolver::resetMetrics()
{
    QMutexLocker locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", QJsonObject{});
}

void AIMergeResolver::logStructured(const QString& level, const QString& message, const QJsonObject& context)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "AIMergeResolver";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    QJsonDocument doc(logEntry);
    qDebug().noquote() << doc.toJson(QJsonDocument::Compact);
}

void AIMergeResolver::recordLatency(const QString& operation, const std::chrono::milliseconds& duration)
{
    QMutexLocker locker(&m_metricsMutex);
    
    if (operation == "conflict_resolution") {
        m_metrics.avgResolutionLatencyMs = 
            (m_metrics.avgResolutionLatencyMs * (m_metrics.conflictsResolved - 1) + duration.count()) 
            / m_metrics.conflictsResolved;
    }
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.enableMetrics) {
        emit metricsUpdated(m_metrics);
    }
}

void AIMergeResolver::logAudit(const QString& action, const QJsonObject& details)
{
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableAuditLog || config.auditLogPath.isEmpty()) {
        return;
    }
    
    QJsonObject auditEntry;
    auditEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    auditEntry["action"] = action;
    auditEntry["details"] = details;
    
    QFile auditFile(config.auditLogPath);
    if (auditFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&auditFile);
        QJsonDocument doc(auditEntry);
        out << doc.toJson(QJsonDocument::Compact) << "\n";
        auditFile.close();
    } else {
        logStructured("ERROR", "Failed to write audit log", QJsonObject{
            {"path", config.auditLogPath},
            {"error", auditFile.errorString()}
        });
    }
}

QJsonObject AIMergeResolver::makeAiRequest(const QString& endpoint, const QJsonObject& payload)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(endpoint);
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!config.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(config.apiKey).toUtf8());
    }
    
    QJsonDocument doc(payload);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    QEventLoop loop;
    QNetworkReply* reply = manager.post(request, data);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    QJsonObject response;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        response = responseDoc.object();
    } else {
        logStructured("ERROR", "AI API request failed", QJsonObject{
            {"endpoint", endpoint},
            {"error", reply->errorString()}
        });
        throw std::runtime_error(reply->errorString().toStdString());
    }
    
    reply->deleteLater();
    return response;
}

bool AIMergeResolver::validateResolution(const Resolution& resolution, const ConflictBlock& conflict)
{
    if (resolution.resolvedContent.isEmpty()) {
        logStructured("WARN", "Empty resolution content", QJsonObject{});
        return false;
    }
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (resolution.confidence < config.minConfidenceThreshold) {
        logStructured("WARN", "Resolution confidence below threshold", QJsonObject{
            {"confidence", resolution.confidence},
            {"threshold", config.minConfidenceThreshold}
        });
        return false;
    }
    
    return true;
}
