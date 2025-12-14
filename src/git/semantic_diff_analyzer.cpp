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
#include <algorithm>
#include <vector>

// Production-ready Myers diff algorithm implementation
namespace DiffAlgorithm {

struct DiffEdit {
    enum Type { KEEP, INSERT, DELETE };
    Type type;
    int oldIndex;
    int newIndex;
    QString line;
};

// Myers diff algorithm for computing minimal edit script
std::vector<DiffEdit> computeMyersDiff(const QStringList& oldLines, const QStringList& newLines) {
    const int N = oldLines.size();
    const int M = newLines.size();
    const int MAX = N + M;
    
    std::vector<int> V(2 * MAX + 1, 0);
    std::vector<std::vector<int>> traces;
    
    // Forward search
    for (int D = 0; D <= MAX; ++D) {
        traces.push_back(V);
        
        for (int k = -D; k <= D; k += 2) {
            int x;
            if (k == -D || (k != D && V[MAX + k - 1] < V[MAX + k + 1])) {
                x = V[MAX + k + 1];
            } else {
                x = V[MAX + k - 1] + 1;
            }
            
            int y = x - k;
            
            // Follow diagonal
            while (x < N && y < M && oldLines[x] == newLines[y]) {
                ++x;
                ++y;
            }
            
            V[MAX + k] = x;
            
            if (x >= N && y >= M) {
                // Found solution
                std::vector<DiffEdit> edits;
                
                // Backtrack to build edit script
                int currentX = N;
                int currentY = M;
                
                for (int d = D; d > 0; --d) {
                    const auto& prevV = traces[d - 1];
                    int currentK = currentX - currentY;
                    
                    int prevK;
                    if (currentK == -d || (currentK != d && prevV[MAX + currentK - 1] < prevV[MAX + currentK + 1])) {
                        prevK = currentK + 1;
                    } else {
                        prevK = currentK - 1;
                    }
                    
                    int prevX = prevV[MAX + prevK];
                    int prevY = prevX - prevK;
                    
                    // Add diagonal moves (equals)
                    while (currentX > prevX && currentY > prevY) {
                        --currentX;
                        --currentY;
                        edits.push_back({DiffEdit::KEEP, currentX, currentY, oldLines[currentX]});
                    }
                    
                    // Add edit
                    if (currentX > prevX) {
                        --currentX;
                        edits.push_back({DiffEdit::DELETE, currentX, -1, oldLines[currentX]});
                    } else if (currentY > prevY) {
                        --currentY;
                        edits.push_back({DiffEdit::INSERT, -1, currentY, newLines[currentY]});
                    }
                }
                
                std::reverse(edits.begin(), edits.end());
                return edits;
            }
        }
    }
    
    return {};
}

QString generateUnifiedDiff(const QString& filePath, const QStringList& oldLines, 
                           const QStringList& newLines, int contextLines = 3) {
    auto edits = computeMyersDiff(oldLines, newLines);
    
    QString result = QString("--- a/%1\n+++ b/%1\n").arg(filePath);
    
    // Group edits into hunks
    std::vector<std::pair<int, int>> hunks; // (start, end) indices in edits
    int hunkStart = 0;
    int lastEditIndex = -contextLines - 1;
    
    for (size_t i = 0; i < edits.size(); ++i) {
        if (edits[i].type != DiffEdit::KEEP) {
            if (i - lastEditIndex > 2 * contextLines) {
                if (lastEditIndex >= 0) {
                    hunks.push_back({hunkStart, lastEditIndex + contextLines});
                }
                hunkStart = std::max(0, static_cast<int>(i) - contextLines);
            }
            lastEditIndex = i;
        }
    }
    
    if (lastEditIndex >= 0) {
        hunks.push_back({hunkStart, std::min(static_cast<int>(edits.size()) - 1, 
                                              lastEditIndex + contextLines)});
    }
    
    // Generate hunk output
    for (const auto& hunk : hunks) {
        int oldStart = -1, newStart = -1;
        int oldCount = 0, newCount = 0;
        
        for (int i = hunk.first; i <= hunk.second; ++i) {
            if (edits[i].type == DiffEdit::DELETE || edits[i].type == DiffEdit::KEEP) {
                if (oldStart < 0) oldStart = edits[i].oldIndex;
                ++oldCount;
            }
            if (edits[i].type == DiffEdit::INSERT || edits[i].type == DiffEdit::KEEP) {
                if (newStart < 0) newStart = edits[i].newIndex;
                ++newCount;
            }
        }
        
        result += QString("@@ -%1,%2 +%3,%4 @@\n")
                    .arg(oldStart + 1).arg(oldCount)
                    .arg(newStart + 1).arg(newCount);
        
        for (int i = hunk.first; i <= hunk.second; ++i) {
            switch (edits[i].type) {
                case DiffEdit::KEEP:
                    result += " " + edits[i].line + "\n";
                    break;
                case DiffEdit::DELETE:
                    result += "-" + edits[i].line + "\n";
                    break;
                case DiffEdit::INSERT:
                    result += "+" + edits[i].line + "\n";
                    break;
            }
        }
    }
    
    return result;
}

} // namespace DiffAlgorithm

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
        
        // Generate unified diff using production-ready Myers diff algorithm
        QStringList oldLines = oldContent.split('\n');
        QStringList newLines = newContent.split('\n');
        
        QString diffContent = DiffAlgorithm::generateUnifiedDiff(filePath, oldLines, newLines, 3);
        
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
