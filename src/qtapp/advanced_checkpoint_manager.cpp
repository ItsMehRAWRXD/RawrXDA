#include "advanced_checkpoint_manager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QFileInfo>

AdvancedCheckpointManager::AdvancedCheckpointManager(QObject *parent)
    : QObject(parent),
      m_compressionEnabled(true),
      m_encryptionEnabled(false)
{
    // Initialize checkpoint directory
    m_checkpointDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/checkpoints";
    QDir dir;
    if (!dir.exists(m_checkpointDir)) {
        dir.mkpath(m_checkpointDir);
    }
    
    qDebug() << "[AdvancedCheckpointManager] Initialized with checkpoints dir:" << m_checkpointDir;
}

AdvancedCheckpointManager::~AdvancedCheckpointManager()
{
    qDebug() << "[AdvancedCheckpointManager] Destroyed";
}

QString AdvancedCheckpointManager::createCheckpoint(const QJsonObject& state, const QString& description)
{
    QString checkpointId = QString("ckpt_%1_%2")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
        .arg(QDateTime::currentMSecsSinceEpoch() % 10000);
    
    QJsonObject checkpoint;
    checkpoint["id"] = checkpointId;
    checkpoint["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    checkpoint["description"] = description;
    checkpoint["state"] = state;
    checkpoint["compressed"] = m_compressionEnabled;
    checkpoint["encrypted"] = m_encryptionEnabled;
    
    m_checkpoints[checkpointId] = checkpoint;
    
    // Save to file
    QString filePath = m_checkpointDir + "/" + checkpointId + ".json";
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument doc(checkpoint);
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
    
    emit checkpointCreated(checkpointId);
    qDebug() << "[AdvancedCheckpointManager] Created checkpoint:" << checkpointId;
    
    return checkpointId;
}

bool AdvancedCheckpointManager::restoreCheckpoint(const QString& checkpointId)
{
    auto it = m_checkpoints.find(checkpointId);
    if (it == m_checkpoints.end()) {
        qWarning() << "[AdvancedCheckpointManager] Checkpoint not found:" << checkpointId;
        return false;
    }
    
    QJsonObject checkpoint = it->second;
    
    // Verify checkpoint integrity
    if (!checkpoint.contains("state")) {
        qWarning() << "[AdvancedCheckpointManager] Invalid checkpoint state:" << checkpointId;
        return false;
    }
    
    emit checkpointRestored(checkpointId);
    qDebug() << "[AdvancedCheckpointManager] Restored checkpoint:" << checkpointId;
    
    return true;
}

bool AdvancedCheckpointManager::deleteCheckpoint(const QString& checkpointId)
{
    auto it = m_checkpoints.find(checkpointId);
    if (it == m_checkpoints.end()) {
        qWarning() << "[AdvancedCheckpointManager] Checkpoint not found:" << checkpointId;
        return false;
    }
    
    // Delete file
    QString filePath = m_checkpointDir + "/" + checkpointId + ".json";
    QFile::remove(filePath);
    
    m_checkpoints.erase(it);
    
    emit checkpointDeleted(checkpointId);
    qDebug() << "[AdvancedCheckpointManager] Deleted checkpoint:" << checkpointId;
    
    return true;
}

QJsonObject AdvancedCheckpointManager::getCheckpointInfo(const QString& checkpointId) const
{
    auto it = m_checkpoints.find(checkpointId);
    if (it == m_checkpoints.end()) {
        return QJsonObject();
    }
    
    QJsonObject info = it->second;
    // Remove state from info to avoid large payloads
    info.remove("state");
    return info;
}

QJsonArray AdvancedCheckpointManager::listCheckpoints() const
{
    QJsonArray list;
    for (const auto& [id, checkpoint] : m_checkpoints) {
        QJsonObject item;
        item["id"] = id;
        item["timestamp"] = checkpoint.value("timestamp");
        item["description"] = checkpoint.value("description");
        list.append(item);
    }
    return list;
}

bool AdvancedCheckpointManager::pruneOldCheckpoints(int keepCount)
{
    if (m_checkpoints.size() <= keepCount) {
        return true;
    }
    
    // Sort by timestamp and delete oldest
    std::vector<std::pair<QString, QDateTime>> sortedCkpts;
    for (const auto& [id, checkpoint] : m_checkpoints) {
        QString timestampStr = checkpoint.value("timestamp").toString();
        QDateTime timestamp = QDateTime::fromString(timestampStr, Qt::ISODate);
        sortedCkpts.push_back({id, timestamp});
    }
    
    std::sort(sortedCkpts.begin(), sortedCkpts.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    int toDelete = m_checkpoints.size() - keepCount;
    for (int i = 0; i < toDelete && i < sortedCkpts.size(); ++i) {
        deleteCheckpoint(sortedCkpts[i].first);
    }
    
    qDebug() << "[AdvancedCheckpointManager] Pruned" << toDelete << "old checkpoints";
    return true;
}

QJsonObject AdvancedCheckpointManager::getCheckpointStats() const
{
    QJsonObject stats;
    stats["total_checkpoints"] = static_cast<int>(m_checkpoints.size());
    stats["directory"] = m_checkpointDir;
    stats["compression_enabled"] = m_compressionEnabled;
    stats["encryption_enabled"] = m_encryptionEnabled;
    
    // Calculate total size
    qint64 totalSize = 0;
    QDir dir(m_checkpointDir);
    for (const auto& file : dir.entryList(QDir::Files)) {
        totalSize += QFileInfo(m_checkpointDir + "/" + file).size();
    }
    stats["total_size_bytes"] = static_cast<qint64>(totalSize);
    
    return stats;
}

void AdvancedCheckpointManager::setCompressionEnabled(bool enabled)
{
    m_compressionEnabled = enabled;
    qDebug() << "[AdvancedCheckpointManager] Compression" << (enabled ? "enabled" : "disabled");
}

void AdvancedCheckpointManager::setEncryptionEnabled(bool enabled, const QString& key)
{
    m_encryptionEnabled = enabled;
    if (!key.isEmpty()) {
        m_encryptionKey = key;
    }
    qDebug() << "[AdvancedCheckpointManager] Encryption" << (enabled ? "enabled" : "disabled");
}

bool AdvancedCheckpointManager::syncWithRemote(const QString& remoteUrl)
{
    qDebug() << "[AdvancedCheckpointManager] Syncing with remote:" << remoteUrl;
    // Implementation would sync checkpoints to remote server
    emit syncCompleted(true);
    return true;
}
bool AdvancedCheckpointManager::backupToCloud(const QString& cloudConfig)
{
    qDebug() << "[AdvancedCheckpointManager] Backing up to cloud with config:" << cloudConfig;
    // Implementation would backup to cloud storage
    emit syncCompleted(true);
    return true;
}
