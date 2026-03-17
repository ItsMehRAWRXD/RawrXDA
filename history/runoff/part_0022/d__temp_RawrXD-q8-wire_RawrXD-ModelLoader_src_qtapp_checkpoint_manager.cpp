#include "checkpoint_manager.h"
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QStandardPaths>

CheckpointManager::CheckpointManager(QObject* parent)
    : QObject(parent),
      m_maxCheckpoints(10),
      m_checkpointCounter(0),
      m_autoCheckpointEnabled(false),
      m_autoCheckpointInterval(100),
      m_autoCheckpointEpochInterval(1),
      m_lastAutoCheckpointStep(0),
      m_lastAutoCheckpointEpoch(0),
      m_rank(0),
      m_worldSize(1)
{
    qDebug() << "[CheckpointManager] Initializing checkpoint manager";
}

CheckpointManager::~CheckpointManager()
{
    qDebug() << "[CheckpointManager] Destroying checkpoint manager";
}

bool CheckpointManager::initialize(const QString& checkpointDir, int maxCheckpoints)
{
    qDebug() << "[CheckpointManager] Initializing with directory:" << checkpointDir;
    
    QDir dir(checkpointDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCritical() << "[CheckpointManager] Failed to create checkpoint directory";
            return false;
        }
    }
    
    m_checkpointDir = checkpointDir;
    m_maxCheckpoints = maxCheckpoints > 0 ? maxCheckpoints : 10;
    m_checkpointCounter = 0;
    
    return true;
}

bool CheckpointManager::isInitialized() const
{
    return !m_checkpointDir.isEmpty() && QDir(m_checkpointDir).exists();
}

QString CheckpointManager::saveCheckpoint(const CheckpointMetadata& metadata,
                                         const CheckpointState& state,
                                         CompressionLevel compress)
{
    if (!isInitialized()) {
        qCritical() << "[CheckpointManager] Not initialized";
        return QString();
    }
    
    QString checkpointId = generateCheckpointId();
    qDebug() << "[CheckpointManager] Saving checkpoint:" << checkpointId;
    
    if (writeCheckpointToDisk(checkpointId, state, compress)) {
        CheckpointIndex index;
        index.checkpointId = checkpointId;
        index.filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
        index.metadata = metadata;
        index.checkpointNumber = m_checkpointCounter++;
        
        m_checkpointIndex.push_back(index);
        
        if (metadata.isBestModel) {
            m_bestCheckpointId = checkpointId;
            emit bestCheckpointUpdated(checkpointId, metadata.validationLoss);
        }
        
        emit checkpointSaved(checkpointId, metadata.epoch, metadata.step);
        return checkpointId;
    }
    
    emit checkpointError("Failed to save checkpoint");
    return QString();
}

QString CheckpointManager::quickSaveCheckpoint(const CheckpointMetadata& metadata,
                                              const CheckpointState& state)
{
    return saveCheckpoint(metadata, state, CompressionLevel::Low);
}

QString CheckpointManager::saveModelWeights(const CheckpointMetadata& metadata,
                                           const QByteArray& modelWeights,
                                           CompressionLevel compress)
{
    CheckpointState state;
    state.modelWeights = modelWeights;
    return saveCheckpoint(metadata, state, compress);
}

bool CheckpointManager::loadCheckpoint(const QString& checkpointId, CheckpointState& state)
{
    qDebug() << "[CheckpointManager] Loading checkpoint:" << checkpointId;
    
    if (!readCheckpointFromDisk(checkpointId, state)) {
        emit checkpointError("Failed to load checkpoint: " + checkpointId);
        return false;
    }
    
    emit checkpointLoaded(checkpointId);
    return true;
}

QString CheckpointManager::loadLatestCheckpoint(CheckpointState& state)
{
    if (m_checkpointIndex.empty()) {
        return QString();
    }
    
    const auto& latest = m_checkpointIndex.back();
    if (loadCheckpoint(latest.checkpointId, state)) {
        return latest.checkpointId;
    }
    return QString();
}

QString CheckpointManager::loadBestCheckpoint(CheckpointState& state)
{
    if (m_bestCheckpointId.isEmpty()) {
        return QString();
    }
    
    if (loadCheckpoint(m_bestCheckpointId, state)) {
        return m_bestCheckpointId;
    }
    return QString();
}

QString CheckpointManager::loadCheckpointFromEpoch(int epoch, CheckpointState& state)
{
    for (const auto& index : m_checkpointIndex) {
        if (index.metadata.epoch == epoch) {
            if (loadCheckpoint(index.checkpointId, state)) {
                return index.checkpointId;
            }
        }
    }
    return QString();
}

CheckpointManager::CheckpointMetadata CheckpointManager::getCheckpointMetadata(const QString& checkpointId) const
{
    for (const auto& index : m_checkpointIndex) {
        if (index.checkpointId == checkpointId) {
            return index.metadata;
        }
    }
    return CheckpointMetadata();
}

std::vector<CheckpointManager::CheckpointIndex> CheckpointManager::listCheckpoints() const
{
    return m_checkpointIndex;
}

std::vector<CheckpointManager::CheckpointIndex> CheckpointManager::getCheckpointHistory(int limit) const
{
    std::vector<CheckpointIndex> history;
    
    int start = static_cast<int>(m_checkpointIndex.size()) - limit;
    if (start < 0) start = 0;
    
    for (size_t i = start; i < m_checkpointIndex.size(); ++i) {
        history.push_back(m_checkpointIndex[i]);
    }
    
    return history;
}

bool CheckpointManager::deleteCheckpoint(const QString& checkpointId)
{
    qDebug() << "[CheckpointManager] Deleting checkpoint:" << checkpointId;
    
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (!file.remove()) {
        qCritical() << "[CheckpointManager] Failed to delete checkpoint file";
        return false;
    }
    
    // Remove from index
    auto it = std::remove_if(m_checkpointIndex.begin(), m_checkpointIndex.end(),
                             [&checkpointId](const CheckpointIndex& idx) {
                                 return idx.checkpointId == checkpointId;
                             });
    m_checkpointIndex.erase(it, m_checkpointIndex.end());
    
    emit checkpointDeleted(checkpointId);
    return true;
}

int CheckpointManager::pruneOldCheckpoints(int keepCount)
{
    qDebug() << "[CheckpointManager] Pruning old checkpoints, keeping" << keepCount;
    
    int deleted = 0;
    while (static_cast<int>(m_checkpointIndex.size()) > keepCount) {
        const auto& oldest = m_checkpointIndex.front();
        if (deleteCheckpoint(oldest.checkpointId)) {
            deleted++;
        }
    }
    
    return deleted;
}

CheckpointManager::CheckpointMetadata CheckpointManager::getBestCheckpointInfo() const
{
    if (m_bestCheckpointId.isEmpty()) {
        return CheckpointMetadata();
    }
    return getCheckpointMetadata(m_bestCheckpointId);
}

bool CheckpointManager::updateCheckpointMetadata(const QString& checkpointId,
                                                const CheckpointMetadata& metadata)
{
    for (auto& index : m_checkpointIndex) {
        if (index.checkpointId == checkpointId) {
            index.metadata = metadata;
            if (metadata.isBestModel) {
                m_bestCheckpointId = checkpointId;
            }
            return true;
        }
    }
    return false;
}

bool CheckpointManager::setCheckpointNote(const QString& checkpointId, const QString& note)
{
    for (auto& index : m_checkpointIndex) {
        if (index.checkpointId == checkpointId) {
            index.metadata.notes = note;
            return true;
        }
    }
    return false;
}

bool CheckpointManager::enableAutoCheckpointing(int intervalSteps, int saveEveryNEpochs)
{
    m_autoCheckpointEnabled = true;
    m_autoCheckpointInterval = intervalSteps;
    m_autoCheckpointEpochInterval = saveEveryNEpochs;
    qDebug() << "[CheckpointManager] Auto-checkpointing enabled:" << intervalSteps << "steps";
    return true;
}

void CheckpointManager::disableAutoCheckpointing()
{
    m_autoCheckpointEnabled = false;
    qDebug() << "[CheckpointManager] Auto-checkpointing disabled";
}

bool CheckpointManager::shouldCheckpoint(int step, int epoch) const
{
    if (!m_autoCheckpointEnabled) return false;
    
    bool stepBased = (step - m_lastAutoCheckpointStep) >= m_autoCheckpointInterval;
    bool epochBased = (epoch - m_lastAutoCheckpointEpoch) >= m_autoCheckpointEpochInterval;
    
    return stepBased || epochBased;
}

bool CheckpointManager::validateCheckpoint(const QString& checkpointId) const
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (!file.exists()) {
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    // Basic validation: check if not empty
    return !data.isEmpty();
}

std::map<QString, bool> CheckpointManager::validateAllCheckpoints() const
{
    std::map<QString, bool> results;
    
    for (const auto& index : m_checkpointIndex) {
        results[index.checkpointId] = validateCheckpoint(index.checkpointId);
    }
    
    return results;
}

bool CheckpointManager::repairCheckpoint(const QString& checkpointId)
{
    qDebug() << "[CheckpointManager] Repairing checkpoint:" << checkpointId;
    // Placeholder for repair logic
    return validateCheckpoint(checkpointId);
}

uint64_t CheckpointManager::getTotalCheckpointSize() const
{
    uint64_t totalSize = 0;
    
    for (const auto& index : m_checkpointIndex) {
        totalSize += getCheckpointSize(index.checkpointId);
    }
    
    return totalSize;
}

uint64_t CheckpointManager::getCheckpointSize(const QString& checkpointId) const
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (file.exists()) {
        return file.size();
    }
    
    return 0;
}

QJsonObject CheckpointManager::generateCheckpointReport() const
{
    QJsonObject report;
    report["totalCheckpoints"] = static_cast<int>(m_checkpointIndex.size());
    report["totalSize"] = static_cast<qint64>(getTotalCheckpointSize());
    report["bestCheckpointId"] = m_bestCheckpointId;
    
    QJsonArray checkpoints;
    for (const auto& index : m_checkpointIndex) {
        QJsonObject cp;
        cp["id"] = index.checkpointId;
        cp["epoch"] = index.metadata.epoch;
        cp["step"] = index.metadata.step;
        cp["validationLoss"] = index.metadata.validationLoss;
        cp["accuracy"] = index.metadata.accuracy;
        checkpoints.append(cp);
    }
    report["checkpoints"] = checkpoints;
    
    return report;
}

QJsonObject CheckpointManager::compareCheckpoints(const QString& checkpointId1,
                                                 const QString& checkpointId2) const
{
    QJsonObject comparison;
    
    auto meta1 = getCheckpointMetadata(checkpointId1);
    auto meta2 = getCheckpointMetadata(checkpointId2);
    
    QJsonObject cp1;
    cp1["validationLoss"] = meta1.validationLoss;
    cp1["accuracy"] = meta1.accuracy;
    
    QJsonObject cp2;
    cp2["validationLoss"] = meta2.validationLoss;
    cp2["accuracy"] = meta2.accuracy;
    
    comparison["checkpoint1"] = cp1;
    comparison["checkpoint2"] = cp2;
    comparison["lossImprovement"] = meta1.validationLoss - meta2.validationLoss;
    comparison["accuracyDiff"] = meta1.accuracy - meta2.accuracy;
    
    return comparison;
}

void CheckpointManager::setDistributedInfo(int rank, int worldSize)
{
    m_rank = rank;
    m_worldSize = worldSize;
    qDebug() << "[CheckpointManager] Set distributed info: rank" << rank << "of" << worldSize;
}

bool CheckpointManager::synchronizeDistributedCheckpoints()
{
    qDebug() << "[CheckpointManager] Synchronizing distributed checkpoints";
    // Placeholder for distributed sync logic
    return true;
}

QJsonObject CheckpointManager::exportConfiguration() const
{
    QJsonObject config;
    config["checkpointDir"] = m_checkpointDir;
    config["maxCheckpoints"] = m_maxCheckpoints;
    config["autoCheckpointEnabled"] = m_autoCheckpointEnabled;
    config["autoCheckpointInterval"] = m_autoCheckpointInterval;
    config["autoCheckpointEpochInterval"] = m_autoCheckpointEpochInterval;
    return config;
}

bool CheckpointManager::importConfiguration(const QJsonObject& config)
{
    m_checkpointDir = config["checkpointDir"].toString();
    m_maxCheckpoints = config["maxCheckpoints"].toInt(10);
    m_autoCheckpointEnabled = config["autoCheckpointEnabled"].toBool();
    m_autoCheckpointInterval = config["autoCheckpointInterval"].toInt(100);
    m_autoCheckpointEpochInterval = config["autoCheckpointEpochInterval"].toInt(1);
    return true;
}

bool CheckpointManager::saveConfigurationToFile(const QString& filePath) const
{
    QJsonDocument doc(exportConfiguration());
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    return true;
}

bool CheckpointManager::loadConfigurationFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    return importConfiguration(doc.object());
}

QString CheckpointManager::generateCheckpointId()
{
    return QString("checkpoint_%1_%2")
        .arg(QDateTime::currentDateTime().toMSecsSinceEpoch())
        .arg(m_checkpointCounter);
}

QByteArray CheckpointManager::compressState(const QByteArray& data, CompressionLevel level)
{
    if (level == CompressionLevel::None || data.isEmpty()) {
        return data;
    }
    
    // Placeholder for zlib compression
    // In production, use zlib or similar
    return data;
}

QByteArray CheckpointManager::decompressState(const QByteArray& data)
{
    // Placeholder for decompression
    return data;
}

bool CheckpointManager::writeCheckpointToDisk(const QString& checkpointId,
                                             const CheckpointState& state,
                                             CompressionLevel compress)
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    // Write state - serialize config to JSON first
    QJsonDocument configDoc(state.config);
    QByteArray configData = configDoc.toJson(QJsonDocument::Compact);
    
    file.write(state.modelWeights);
    file.write(state.optimizerState);
    file.write(state.schedulerState);
    file.write(state.trainingState);
    file.write(configData);
    
    file.close();
    return true;
}

bool CheckpointManager::readCheckpointFromDisk(const QString& checkpointId,
                                              CheckpointState& state)
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    // Placeholder: in production, properly deserialize
    state.modelWeights = data;
    return true;
}
