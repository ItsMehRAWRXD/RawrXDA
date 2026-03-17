#include "semantic_diff_analyzer.hpp"
#include <QDebug>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDateTime>

SemanticDiffAnalyzer::SemanticDiffAnalyzer(QObject* parent)
    : QObject(parent)
{
    logStructured("INFO", "SemanticDiffAnalyzer initializing", QJsonObject{{"component", "SemanticDiffAnalyzer"}});
    logStructured("INFO", "SemanticDiffAnalyzer initialized successfully", QJsonObject{{"component", "SemanticDiffAnalyzer"}});
}

SemanticDiffAnalyzer::~SemanticDiffAnalyzer()
{
    logStructured("INFO", "SemanticDiffAnalyzer shutting down", QJsonObject{{"component", "SemanticDiffAnalyzer"}});
}

void SemanticDiffAnalyzer::setConfig(const Config& config)
{
    QMutexLocker locker(&m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", QJsonObject{
        {"enableBreakingChangeDetection", config.enableBreakingChangeDetection},
        {"enableImpactAnalysis", config.enableImpactAnalysis},
        {"maxDiffSize", config.maxDiffSize},
        {"enableCaching", config.enableCaching}
    });
}

SemanticDiffAnalyzer::Config SemanticDiffAnalyzer::getConfig() const
{
    QMutexLocker locker(&m_configMutex);
    return m_config;
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::analyzeDiff(const QString& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    DiffAnalysis analysis;
    
    try {
        if (!validateDiff(diff)) {
            logStructured("ERROR", "Invalid diff provided", QJsonObject{{"diffLength", diff.length()}});
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            emit errorOccurred("Invalid diff data");
            return analysis;
        }
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Check cache
        QString diffHash = calculateDiffHash(diff);
        if (config.enableCaching) {
            DiffAnalysis cached = getCachedAnalysis(diffHash);
            if (!cached.summary.isEmpty()) {
                QMutexLocker metricsLocker(&m_metricsMutex);
                m_metrics.cacheHits++;
                logStructured("INFO", "Cache hit for diff analysis", QJsonObject{{"diffHash", diffHash}});
                return cached;
            }
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.cacheMisses++;
        }
        
        // Prepare AI request
        QJsonObject payload;
        payload["diff"] = diff;
        payload["enable_breaking_change_detection"] = config.enableBreakingChangeDetection;
        payload["enable_impact_analysis"] = config.enableImpactAnalysis;
        
        logStructured("DEBUG", "Sending diff analysis request to AI", QJsonObject{
            {"diffSize", diff.length()},
            {"diffHash", diffHash}
        });
        
        QJsonObject response = makeAiRequest(config.aiEndpoint + "/analyze-diff", payload);
        
        // Parse response
        analysis.summary = response["summary"].toString();
        analysis.breakingChangeCount = response["breaking_change_count"].toInt();
        analysis.overallImpactScore = response["overall_impact_score"].toDouble();
        analysis.metadata = response["metadata"].toObject();
        
        QJsonArray changesArray = response["changes"].toArray();
        for (const QJsonValue& value : changesArray) {
            QJsonObject changeObj = value.toObject();
            SemanticChange change;
            change.type = changeObj["type"].toString();
            change.name = changeObj["name"].toString();
            change.description = changeObj["description"].toString();
            change.file = changeObj["file"].toString();
            change.startLine = changeObj["start_line"].toInt();
            change.endLine = changeObj["end_line"].toInt();
            change.isBreaking = changeObj["is_breaking"].toBool();
            change.impactScore = changeObj["impact_score"].toDouble();
            
            QJsonArray affectedArray = changeObj["affected_files"].toArray();
            for (const QJsonValue& af : affectedArray) {
                change.affectedFiles.append(af.toString());
            }
            
            analysis.changes.append(change);
            
            // Emit signals for breaking/high-impact changes
            if (change.isBreaking) {
                emit breakingChangeDetected(change);
            }
            if (change.impactScore >= 0.7) {
                emit highImpactChangeDetected(change);
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("diff_analysis", duration);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.diffsAnalyzed++;
            m_metrics.semanticChangesDetected += analysis.changes.size();
            m_metrics.breakingChangesDetected += analysis.breakingChangeCount;
            
            // Update average impact score
            if (analysis.changes.size() > 0) {
                m_metrics.avgImpactScore = 
                    (m_metrics.avgImpactScore * (m_metrics.semanticChangesDetected - analysis.changes.size()) 
                     + analysis.overallImpactScore * analysis.changes.size()) / m_metrics.semanticChangesDetected;
            }
        }
        
        logStructured("INFO", "Diff analysis completed", QJsonObject{
            {"changesDetected", analysis.changes.size()},
            {"breakingChanges", analysis.breakingChangeCount},
            {"overallImpactScore", analysis.overallImpactScore},
            {"latencyMs", duration.count()}
        });
        
        // Cache the result
        if (config.enableCaching) {
            cacheAnalysis(diffHash, analysis);
        }
        
        emit analysisCompleted(analysis);
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Diff analysis failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Analysis failed: %1").arg(e.what()));
    }
    
    return analysis;
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::compareFiles(const QString& oldContent, const QString& newContent, const QString& filePath)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Generate unified diff
        // In production, use libgit2 or similar for proper diff generation
        QString diffContent = QString("--- a/%1\n+++ b/%1\n").arg(filePath);
        
        // Simple diff simulation (in production, use proper diff algorithm)
        QStringList oldLines = oldContent.split('\n');
        QStringList newLines = newContent.split('\n');
        
        for (int i = 0; i < qMax(oldLines.size(), newLines.size()); i++) {
            if (i < oldLines.size() && i < newLines.size()) {
                if (oldLines[i] != newLines[i]) {
                    diffContent += QString("@@ -%1 +%1 @@\n").arg(i + 1);
                    diffContent += QString("-%1\n").arg(oldLines[i]);
                    diffContent += QString("+%1\n").arg(newLines[i]);
                }
            } else if (i >= oldLines.size()) {
                diffContent += QString("+%1\n").arg(newLines[i]);
            } else {
                diffContent += QString("-%1\n").arg(oldLines[i]);
            }
        }
        
        logStructured("DEBUG", "Generated diff for file comparison", QJsonObject{
            {"filePath", filePath},
            {"oldContentSize", oldContent.length()},
            {"newContentSize", newContent.length()}
        });
        
        return analyzeDiff(diffContent);
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "File comparison failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("File comparison failed: %1").arg(e.what()));
        return DiffAnalysis();
    }
}

QVector<QString> SemanticDiffAnalyzer::detectBreakingChanges(const DiffAnalysis& analysis)
{
    QVector<QString> breakingChanges;
    
    for (const SemanticChange& change : analysis.changes) {
        if (change.isBreaking) {
            QString description = QString("%1: %2 in %3")
                .arg(change.type)
                .arg(change.name)
                .arg(change.file);
            breakingChanges.append(description);
        }
    }
    
    logStructured("INFO", "Breaking changes extracted", QJsonObject{
        {"count", breakingChanges.size()}
    });
    
    return breakingChanges;
}

QJsonObject SemanticDiffAnalyzer::analyzeImpact(const SemanticChange& change)
{
    auto startTime = std::chrono::steady_clock::now();
    QJsonObject impactAnalysis;
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (!config.enableImpactAnalysis) {
            logStructured("DEBUG", "Impact analysis disabled", QJsonObject{});
            return impactAnalysis;
        }
        
        QJsonObject payload;
        payload["change_type"] = change.type;
        payload["change_name"] = change.name;
        payload["change_description"] = change.description;
        payload["file"] = change.file;
        payload["is_breaking"] = change.isBreaking;
        
        logStructured("DEBUG", "Requesting impact analysis", QJsonObject{
            {"changeType", change.type},
            {"changeName", change.name}
        });
        
        impactAnalysis = makeAiRequest(config.aiEndpoint + "/analyze-impact", payload);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.impactAnalysesPerformed++;
        }
        
        logStructured("INFO", "Impact analysis completed", QJsonObject{
            {"impactScore", impactAnalysis["impact_score"].toDouble()},
            {"affectedComponents", impactAnalysis["affected_components"].toArray().size()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Impact analysis failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Impact analysis failed: %1").arg(e.what()));
    }
    
    return impactAnalysis;
}

QString SemanticDiffAnalyzer::enrichDiffContext(const QString& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    QString enriched;
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        QJsonObject payload;
        payload["diff"] = diff;
        payload["enrichment_type"] = "context";
        
        logStructured("DEBUG", "Requesting diff context enrichment", QJsonObject{
            {"diffSize", diff.length()}
        });
        
        QJsonObject response = makeAiRequest(config.aiEndpoint + "/enrich-diff", payload);
        
        enriched = response["enriched_diff"].toString();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Diff enrichment completed", QJsonObject{
            {"originalSize", diff.length()},
            {"enrichedSize", enriched.length()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Diff enrichment failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Enrichment failed: %1").arg(e.what()));
        enriched = diff; // Return original on error
    }
    
    return enriched;
}

SemanticDiffAnalyzer::Metrics SemanticDiffAnalyzer::getMetrics() const
{
    QMutexLocker locker(&m_metricsMutex);
    return m_metrics;
}

void SemanticDiffAnalyzer::resetMetrics()
{
    QMutexLocker locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", QJsonObject{});
}

void SemanticDiffAnalyzer::clearCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_analysisCache.clear();
    logStructured("INFO", "Analysis cache cleared", QJsonObject{});
}

void SemanticDiffAnalyzer::logStructured(const QString& level, const QString& message, const QJsonObject& context)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "SemanticDiffAnalyzer";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    QJsonDocument doc(logEntry);
    qDebug().noquote() << doc.toJson(QJsonDocument::Compact);
}

void SemanticDiffAnalyzer::recordLatency(const QString& operation, const std::chrono::milliseconds& duration)
{
    QMutexLocker locker(&m_metricsMutex);
    
    if (operation == "diff_analysis") {
        m_metrics.avgAnalysisLatencyMs = 
            (m_metrics.avgAnalysisLatencyMs * (m_metrics.diffsAnalyzed - 1) + duration.count()) 
            / m_metrics.diffsAnalyzed;
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

QJsonObject SemanticDiffAnalyzer::makeAiRequest(const QString& endpoint, const QJsonObject& payload)
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

QString SemanticDiffAnalyzer::calculateDiffHash(const QString& diff)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(diff.toUtf8());
    return QString(hash.result().toHex());
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::getCachedAnalysis(const QString& diffHash)
{
    QMutexLocker locker(&m_cacheMutex);
    return m_analysisCache.value(diffHash, DiffAnalysis());
}

void SemanticDiffAnalyzer::cacheAnalysis(const QString& diffHash, const DiffAnalysis& analysis)
{
    QMutexLocker locker(&m_cacheMutex);
    m_analysisCache[diffHash] = analysis;
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    // Persist to disk if cache directory configured
    if (!config.cacheDirectory.isEmpty()) {
        QDir cacheDir(config.cacheDirectory);
        if (!cacheDir.exists()) {
            cacheDir.mkpath(".");
        }
        
        QFile cacheFile(cacheDir.filePath(diffHash + ".json"));
        if (cacheFile.open(QIODevice::WriteOnly)) {
            QJsonObject cacheObj;
            cacheObj["summary"] = analysis.summary;
            cacheObj["breaking_change_count"] = analysis.breakingChangeCount;
            cacheObj["overall_impact_score"] = analysis.overallImpactScore;
            cacheObj["metadata"] = analysis.metadata;
            
            QJsonArray changesArray;
            for (const SemanticChange& change : analysis.changes) {
                QJsonObject changeObj;
                changeObj["type"] = change.type;
                changeObj["name"] = change.name;
                changeObj["description"] = change.description;
                changeObj["file"] = change.file;
                changeObj["start_line"] = change.startLine;
                changeObj["end_line"] = change.endLine;
                changeObj["is_breaking"] = change.isBreaking;
                changeObj["impact_score"] = change.impactScore;
                changesArray.append(changeObj);
            }
            cacheObj["changes"] = changesArray;
            
            QJsonDocument doc(cacheObj);
            cacheFile.write(doc.toJson());
            cacheFile.close();
        }
    }
}

bool SemanticDiffAnalyzer::validateDiff(const QString& diff)
{
    if (diff.isEmpty()) {
        return false;
    }
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (diff.length() > config.maxDiffSize) {
        logStructured("WARN", "Diff exceeds maximum size", QJsonObject{
            {"diffSize", diff.length()},
            {"maxSize", config.maxDiffSize}
        });
        return false;
    }
    
    return true;
}
