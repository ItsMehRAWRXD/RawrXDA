#include "memory_persistence_system.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QApplication>

MemoryPersistenceSystem::MemoryPersistenceSystem(QObject* parent)
    : QObject(parent)
    , m_optimizationTimer(new QTimer(this))
    , m_snapshotTimer(new QTimer(this))
{
    // Initialize storage paths
    m_baseStoragePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/RawrXD/Memory";
    m_snapshotsPath = m_baseStoragePath + "/snapshots";
    m_sessionsPath = m_baseStoragePath + "/sessions";
    m_knowledgePath = m_baseStoragePath + "/knowledge";
    
    // Create directories
    QDir().mkpath(m_snapshotsPath);
    QDir().mkpath(m_sessionsPath);
    QDir().mkpath(m_knowledgePath);
    
    // Initialize knowledge graph
    m_knowledgeGraph["nodes"] = QJsonArray();
    m_knowledgeGraph["edges"] = QJsonArray();
    
    // Connect timers
    connect(m_optimizationTimer, &QTimer::timeout, this, &MemoryPersistenceSystem::optimizeMemoryUsage);
    connect(m_snapshotTimer, &QTimer::timeout, this, [this]() {
        if (m_autoSnapshot) {
            QString sessionId = generateSnapshotId();
            QJsonObject context = createCurrentContext();
            saveContextSnapshot(sessionId, context);
        }
    });
    
    // Start timers
    m_optimizationTimer->start(300000); // 5 minutes
    m_snapshotTimer->start(m_snapshotIntervalMinutes * 60000); // Convert minutes to ms
    
    qDebug() << "[MemoryPersistenceSystem] Initialized with storage path:" << m_baseStoragePath;
}

MemoryPersistenceSystem::~MemoryPersistenceSystem() = default;

void MemoryPersistenceSystem::saveContextSnapshot(const QString& sessionId, const QJsonObject& context) {
    qDebug() << "[MemoryPersistenceSystem] Saving context snapshot:" << sessionId;
    
    SessionData* session = new SessionData();
    session->sessionId = sessionId;
    session->timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    session->context = context;
    session->projectPath = context["project_path"].toString();
    
    // Convert QJsonArray to QStringList
    QJsonArray openFilesArray = context["open_files"].toArray();
    QStringList openFilesList;
    for (const QJsonValue& val : openFilesArray) {
        openFilesList.append(val.toString());
    }
    session->openFiles = openFilesList;
    
    // Store in active sessions
    m_activeSessions[sessionId] = std::shared_ptr<SessionData>(session);
    
    // Save to disk
    QString filePath = m_snapshotsPath + "/" + sessionId + ".json";
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc;
        doc.setObject(serializeContext(context));
        file.write(doc.toJson());
        file.close();
        
        emit snapshotSaved(sessionId);
        qDebug() << "[MemoryPersistenceSystem] Snapshot saved:" << filePath;
    }
}

QJsonObject MemoryPersistenceSystem::loadContextSnapshot(const QString& sessionId) {
    qDebug() << "[MemoryPersistenceSystem] Loading context snapshot:" << sessionId;
    
    // Check active sessions first
    if (m_activeSessions.contains(sessionId)) {
        return m_activeSessions[sessionId]->context;
    }
    
    // Load from disk
    QString filePath = m_snapshotsPath + "/" + sessionId + ".json";
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        
        QJsonObject context = deserializeContext(doc.object());
        
        // Store in active sessions for future use
        SessionData* session = new SessionData();
        session->sessionId = sessionId;
        session->timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        session->context = context;
        m_activeSessions[sessionId] = std::shared_ptr<SessionData>(session);
        
        return context;
    }
    
    return QJsonObject();
}

QJsonArray MemoryPersistenceSystem::listSnapshots() {
    QJsonArray snapshots;
    QDir snapshotDir(m_snapshotsPath);
    
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = snapshotDir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    for (const QFileInfo& fileInfo : files) {
        QJsonObject snapshot;
        snapshot["id"] = fileInfo.baseName();
        snapshot["filename"] = fileInfo.fileName();
        snapshot["created"] = fileInfo.birthTime().toString(Qt::ISODate);
        snapshot["size"] = fileInfo.size();
        snapshots.append(snapshot);
    }
    
    return snapshots;
}

void MemoryPersistenceSystem::deleteSnapshot(const QString& sessionId) {
    qDebug() << "[MemoryPersistenceSystem] Deleting snapshot:" << sessionId;
    
    // Remove from active sessions
    m_activeSessions.remove(sessionId);
    
    // Remove from disk
    QString filePath = m_snapshotsPath + "/" + sessionId + ".json";
    QFile::remove(filePath);
}

void MemoryPersistenceSystem::saveSessionState(const QString& sessionName, const QJsonObject& state) {
    qDebug() << "[MemoryPersistenceSystem] Saving session state:" << sessionName;
    
    SessionData* session = new SessionData();
    session->sessionId = sessionName;
    session->timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    session->context = state;
    
    // Store in persistent sessions
    m_persistentSessions[sessionName] = std::shared_ptr<SessionData>(session);
    
    // Save to disk
    QString filePath = m_sessionsPath + "/" + sessionName + ".json";
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc;
        doc.setObject(serializeContext(state));
        file.write(doc.toJson());
        file.close();
        qDebug() << "[MemoryPersistenceSystem] Session state saved:" << filePath;
    }
}

QJsonObject MemoryPersistenceSystem::loadSessionState(const QString& sessionName) {
    qDebug() << "[MemoryPersistenceSystem] Loading session state:" << sessionName;
    
    // Check persistent sessions
    if (m_persistentSessions.contains(sessionName)) {
        return m_persistentSessions[sessionName]->context;
    }
    
    // Load from disk
    QString filePath = m_sessionsPath + "/" + sessionName + ".json";
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        
        QJsonObject state = deserializeContext(doc.object());
        
        // Store in persistent sessions
        SessionData* session = new SessionData();
        session->sessionId = sessionName;
        session->timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        session->context = state;
        m_persistentSessions[sessionName] = std::shared_ptr<SessionData>(session);
        
        return state;
    }
    
    return QJsonObject();
}

void MemoryPersistenceSystem::saveCurrentSession() {
    QJsonObject state = createCurrentContext();
    QString sessionName = "current_session_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    saveSessionState(sessionName, state);
}

void MemoryPersistenceSystem::restoreLastSession() {
    // Find the most recent session
    QDir sessionDir(m_sessionsPath);
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = sessionDir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    if (!files.isEmpty()) {
        QFileInfo mostRecent = files.first();
        QString sessionName = mostRecent.baseName();
        QJsonObject state = loadSessionState(sessionName);
        applyContext(state);
        emit sessionRestored(sessionName);
    }
}

void MemoryPersistenceSystem::addCodeRelationship(const QString& filePath, const QString& symbol, const QJsonObject& metadata) {
    qDebug() << "[MemoryPersistenceSystem] Adding code relationship:" << filePath << symbol;
    
    // Add node to knowledge graph
    QJsonObject node;
    node["id"] = symbol;
    node["type"] = "symbol";
    node["file_path"] = filePath;
    node["metadata"] = metadata;
    node["last_accessed"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Add or update node in graph
    QJsonArray nodes = m_knowledgeGraph["nodes"].toArray();
    bool found = false;
    for (int i = 0; i < nodes.size(); ++i) {
        if (nodes[i].toObject()["id"] == symbol) {
            nodes[i] = node;
            found = true;
            break;
        }
    }
    if (!found) {
        nodes.append(node);
    }
    m_knowledgeGraph["nodes"] = nodes;
    
    // Save knowledge graph
    saveKnowledgeGraph();
}

QJsonArray MemoryPersistenceSystem::findRelatedCode(const QString& symbol) {
    qDebug() << "[MemoryPersistenceSystem] Finding related code for:" << symbol;
    
    QJsonArray related;
    QJsonArray nodes = m_knowledgeGraph["nodes"].toArray();
    
    // Find related symbols based on file path or common patterns
    for (const QJsonValue& nodeVal : nodes) {
        QJsonObject node = nodeVal.toObject();
        QString nodeSymbol = node["id"].toString();
        QString nodeFile = node["file_path"].toString();
        
        if (nodeSymbol != symbol && 
            (nodeSymbol.contains(symbol, Qt::CaseInsensitive) ||
             symbol.contains(nodeSymbol, Qt::CaseInsensitive) ||
             nodeFile == findFileForSymbol(symbol))) {
            related.append(node);
        }
    }
    
    return related;
}

QJsonObject MemoryPersistenceSystem::buildKnowledgeGraph(const QString& projectPath) {
    qDebug() << "[MemoryPersistenceSystem] Building knowledge graph for:" << projectPath;
    
    QJsonObject graph = m_knowledgeGraph;
    graph["project_path"] = projectPath;
    graph["built_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Add project-level metadata
    QJsonObject projectNode;
    projectNode["id"] = "project:" + projectPath;
    projectNode["type"] = "project";
    projectNode["path"] = projectPath;
    
    QJsonArray nodes = graph["nodes"].toArray();
    nodes.append(projectNode);
    graph["nodes"] = nodes;
    
    return graph;
}

void MemoryPersistenceSystem::optimizeMemoryUsage() {
    qDebug() << "[MemoryPersistenceSystem] Optimizing memory usage";
    
    // Clean up expired snapshots
    cleanupExpiredData();
    
    // Calculate current usage
    MemoryStats stats = calculateMemoryStats();
    
    // Remove oldest snapshots if over limit
    if (stats.totalSize > m_maxStorageMB * 1024 * 1024) {
        QDir snapshotDir(m_snapshotsPath);
        QStringList filters;
        filters << "*.json";
        QFileInfoList files = snapshotDir.entryInfoList(filters, QDir::Files, QDir::Time);
        
        int removed = 0;
        for (int i = files.size() - 1; i >= 0 && stats.totalSize > m_maxStorageMB * 1024 * 1024 * 0.8; --i) {
            QString filePath = files[i].absoluteFilePath();
            stats.totalSize -= files[i].size();
            QFile::remove(filePath);
            removed++;
        }
        
        qDebug() << "[MemoryPersistenceSystem] Removed" << removed << "old snapshots during optimization";
    }
    
    emit memoryOptimized(serializeMemoryStats(stats));
}

QJsonObject MemoryPersistenceSystem::getMemoryUsageStats() {
    MemoryStats stats = calculateMemoryStats();
    return serializeMemoryStats(stats);
}

void MemoryPersistenceSystem::cleanupExpiredData() {
    qDebug() << "[MemoryPersistenceSystem] Cleaning up expired data";
    
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-30); // 30 days
    
    // Clean up old snapshots
    QDir snapshotDir(m_snapshotsPath);
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = snapshotDir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    int removed = 0;
    for (const QFileInfo& fileInfo : files) {
        if (fileInfo.birthTime() < cutoff) {
            QFile::remove(fileInfo.absoluteFilePath());
            removed++;
        }
    }
    
    qDebug() << "[MemoryPersistenceSystem] Removed" << removed << "expired snapshots";
}

QJsonArray MemoryPersistenceSystem::suggestContextBasedOnHistory(const QString& currentContext) {
    qDebug() << "[MemoryPersistenceSystem] Suggesting context based on history";
    
    QJsonArray suggestions;
    
    // Analyze current context and find similar historical contexts
    QStringList currentKeywords = currentContext.toLower().split(' ', Qt::SkipEmptyParts);
    
    // Get recent sessions
    QDir sessionDir(m_sessionsPath);
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = sessionDir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    for (const QFileInfo& fileInfo : files.mid(0, 10)) { // Check last 10 sessions
        QFile file(fileInfo.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            
            QJsonObject session = doc.object();
            QString sessionContext = session["description"].toString().toLower();
            
            // Calculate similarity
            int matchCount = 0;
            for (const QString& keyword : currentKeywords) {
                if (sessionContext.contains(keyword)) {
                    matchCount++;
                }
            }
            
            if (matchCount > 0) {
                QJsonObject suggestion;
                suggestion["session"] = fileInfo.baseName();
                suggestion["similarity"] = double(matchCount) / currentKeywords.size();
                suggestion["description"] = session["description"];
                suggestions.append(suggestion);
            }
        }
    }
    
    // Sort by similarity (convert to sortable container first)
    QList<QPair<double, QJsonObject>> sortableSuggestions;
    for (const QJsonValue& val : suggestions) {
        QJsonObject obj = val.toObject();
        double similarity = obj["similarity"].toDouble();
        sortableSuggestions.append(qMakePair(similarity, obj));
    }
    
    std::sort(sortableSuggestions.begin(), sortableSuggestions.end(),
              [](const QPair<double, QJsonObject>& a, const QPair<double, QJsonObject>& b) {
        return a.first > b.first;
    });
    
    // Convert back and return top 5
    QJsonArray topSuggestions;
    int maxItems = qMin(5, sortableSuggestions.size());
    for (int i = 0; i < maxItems; ++i) {
        topSuggestions.append(sortableSuggestions[i].second);
    }
    return topSuggestions;
}

QJsonArray MemoryPersistenceSystem::suggestRelevantFiles(const QString& currentFile) {
    qDebug() << "[MemoryPersistenceSystem] Suggesting relevant files for:" << currentFile;
    
    QJsonArray suggestions;
    
    // Find files that were opened together in the past
    for (auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it) {
        std::shared_ptr<SessionData> session = it.value();
        if (session && session->openFiles.contains(currentFile)) {
            for (const QString& relatedFile : session->openFiles) {
                if (relatedFile != currentFile) {
                    QJsonObject suggestion;
                    suggestion["file"] = relatedFile;
                    suggestion["reason"] = "opened together";
                    suggestion["session"] = session->sessionId;
                    suggestions.append(suggestion);
                }
            }
        }
    }
    
    return suggestions;
}

QString MemoryPersistenceSystem::suggestNextAction(const QJsonObject& currentState) {
    qDebug() << "[MemoryPersistenceSystem] Suggesting next action";
    
    QString currentActivity = currentState["current_activity"].toString();
    QString currentFile = currentState["current_file"].toString();
    
    // Simple rule-based suggestions
    if (currentActivity == "coding" && currentFile.endsWith(".cpp")) {
        return "Consider running tests or building the project";
    } else if (currentActivity == "debugging") {
        return "Try using the error analysis tool to diagnose issues";
    } else if (currentActivity == "design") {
        return "Create a master plan for the implementation";
    }
    
    return "Continue with your current task";
}

// Slots implementation
void MemoryPersistenceSystem::onSessionStarted(const QString& sessionId) {
    qDebug() << "[MemoryPersistenceSystem] Session started:" << sessionId;
}

void MemoryPersistenceSystem::onSessionEnded(const QString& sessionId) {
    qDebug() << "[MemoryPersistenceSystem] Session ended:" << sessionId;
}

void MemoryPersistenceSystem::onCodeFileOpened(const QString& filePath) {
    addCodeRelationship(filePath, "file_opened", QJsonObject{{"action", "opened"}});
}

void MemoryPersistenceSystem::onCodeFileModified(const QString& filePath) {
    addCodeRelationship(filePath, "file_modified", QJsonObject{{"action", "modified"}});
}

void MemoryPersistenceSystem::onBuildCompleted(const QString& buildId, bool success) {
    QJsonObject metadata;
    metadata["build_id"] = buildId;
    metadata["success"] = success;
    metadata["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    addCodeRelationship("build_system", "build_" + buildId, metadata);
}

void MemoryPersistenceSystem::enableAutoSnapshot(bool enable) {
    m_autoSnapshot = enable;
    qDebug() << "[MemoryPersistenceSystem] Auto snapshot" << (enable ? "enabled" : "disabled");
}

void MemoryPersistenceSystem::setSnapshotInterval(int minutes) {
    m_snapshotIntervalMinutes = minutes;
    m_snapshotTimer->setInterval(minutes * 60000);
    qDebug() << "[MemoryPersistenceSystem] Snapshot interval set to" << minutes << "minutes";
}

// Private helper methods
QString MemoryPersistenceSystem::generateSnapshotId() {
    return "snapshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
}

QString MemoryPersistenceSystem::getStoragePath() {
    return m_baseStoragePath;
}

QJsonObject MemoryPersistenceSystem::serializeContext(const QJsonObject& context) {
    QJsonObject serialized = context;
    serialized["serialized_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    serialized["version"] = "1.0";
    return serialized;
}

QJsonObject MemoryPersistenceSystem::deserializeContext(const QJsonObject& data) {
    QJsonObject context = data;
    context["deserialized_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return context;
}

QJsonObject MemoryPersistenceSystem::createCurrentContext() {
    QJsonObject context;
    context["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    context["application"] = QApplication::applicationName();
    context["version"] = QApplication::applicationVersion();
    
    // Add current IDE state
    context["active_files"] = QJsonArray(); // Would be populated from MainWindow
    context["current_activity"] = "unknown";
    
    return context;
}

void MemoryPersistenceSystem::applyContext(const QJsonObject& context) {
    qDebug() << "[MemoryPersistenceSystem] Applying context from snapshot";
    // Implementation would restore IDE state from context
}

void MemoryPersistenceSystem::saveKnowledgeGraph() {
    QString filePath = m_knowledgePath + "/knowledge_graph.json";
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(m_knowledgeGraph);
        file.write(doc.toJson());
        file.close();
    }
}

QString MemoryPersistenceSystem::findFileForSymbol(const QString& symbol) {
    // Simple implementation - would be enhanced with actual symbol indexing
    return QString();
}

MemoryPersistenceSystem::MemoryStats MemoryPersistenceSystem::calculateMemoryStats() {
    MemoryStats stats;
    stats.totalSize = 0;
    stats.activeSnapshots = m_activeSessions.size();
    stats.totalSessions = m_persistentSessions.size();
    
    // Calculate total size of snapshot files
    QDir snapshotDir(m_snapshotsPath);
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = snapshotDir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : files) {
        stats.totalSize += fileInfo.size();
    }
    
    stats.compressionRatio = 1.0; // Simplified
    
    return stats;
}

QJsonObject MemoryPersistenceSystem::serializeMemoryStats(const MemoryStats& stats) {
    QJsonObject serialized;
    serialized["total_size"] = stats.totalSize;
    serialized["active_snapshots"] = stats.activeSnapshots;
    serialized["total_sessions"] = stats.totalSessions;
    serialized["compression_ratio"] = stats.compressionRatio;
    return serialized;
}
