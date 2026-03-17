#include "checkpoint_manager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QUuid>
#include <QCryptographicHash>
#include <zlib.h>
#include <algorithm>
#include <numeric>

CheckpointManager::CheckpointManager(QObject* parent)
    : QObject(parent)
    , m_maxCheckpoints(10)
    , m_checkpointCounter(0)
    , m_autoCheckpointEnabled(false)
    , m_autoCheckpointInterval(100)
    , m_autoCheckpointEpochInterval(1)
    , m_lastAutoCheckpointStep(0)
    , m_lastAutoCheckpointEpoch(0)
    , m_rank(0)
    , m_worldSize(1)
{
}

CheckpointManager::~CheckpointManager()
{
}

bool CheckpointManager::initialize(const QString& checkpointDir, int maxCheckpoints)
{
    m_checkpointDir = checkpointDir;
    m_maxCheckpoints = maxCheckpoints;

    QDir dir(checkpointDir);
    if (!dir.exists()) {
        if (!dir.mkpath(checkpointDir)) {
            qWarning() << "Failed to create checkpoint directory:" << checkpointDir;
            return false;
        }
    }

    qDebug() << "Checkpoint manager initialized at:" << checkpointDir;
    return true;
}

bool CheckpointManager::isInitialized() const
{
    return !m_checkpointDir.isEmpty();
}

QString CheckpointManager::saveCheckpoint(const CheckpointMetadata& metadata,
                                         const CheckpointState& state,
                                         CompressionLevel compress)
{
    if (!isInitialized()) {
        emit checkpointError("Checkpoint manager not initialized");
        return "";
    }

    QString checkpointId = generateCheckpointId();
    
    if (!writeCheckpointToDisk(checkpointId, state, compress)) {
        emit checkpointError("Failed to write checkpoint to disk");
        return "";
    }

    // Create index entry
    CheckpointIndex index;
    index.checkpointId = checkpointId;
    index.checkpointNumber = ++m_checkpointCounter;
    index.filePath = m_checkpointDir + "/" + checkpointId + ".pt";
    
    CheckpointMetadata meta = metadata;
    meta.checkpointId = checkpointId;
    meta.timestamp = QDateTime::currentSecsSinceEpoch();
    index.metadata = meta;

    m_checkpointIndex.push_back(index);

    // Update best checkpoint
    if (meta.isBestModel || m_bestCheckpointId.isEmpty()) {
        m_bestCheckpointId = checkpointId;
        emit bestCheckpointUpdated(checkpointId, meta.validationLoss);
    }

    // Prune if necessary
    if (m_maxCheckpoints > 0 && m_checkpointIndex.size() > static_cast<size_t>(m_maxCheckpoints)) {
        pruneOldCheckpoints(m_maxCheckpoints);
    }

    emit checkpointSaved(checkpointId, meta.epoch, meta.step);
    qDebug() << "Saved checkpoint:" << checkpointId << "epoch:" << meta.epoch << "step:" << meta.step;

    return checkpointId;
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
    if (!isInitialized()) {
        emit checkpointError("Checkpoint manager not initialized");
        return false;
    }

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
        return "";
    }

    const auto& latest = m_checkpointIndex.back();
    if (loadCheckpoint(latest.checkpointId, state)) {
        return latest.checkpointId;
    }

    return "";
}

QString CheckpointManager::loadBestCheckpoint(CheckpointState& state)
{
    if (m_bestCheckpointId.isEmpty()) {
        return "";
    }

    if (loadCheckpoint(m_bestCheckpointId, state)) {
        return m_bestCheckpointId;
    }

    return "";
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

    return "";
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
    std::vector<CheckpointIndex> result;

    int start = std::max(0, static_cast<int>(m_checkpointIndex.size()) - limit);
    for (size_t i = start; i < m_checkpointIndex.size(); ++i) {
        result.push_back(m_checkpointIndex[i]);
    }

    return result;
}

bool CheckpointManager::deleteCheckpoint(const QString& checkpointId)
{
    for (auto it = m_checkpointIndex.begin(); it != m_checkpointIndex.end(); ++it) {
        if (it->checkpointId == checkpointId) {
            QFile::remove(it->filePath);
            m_checkpointIndex.erase(it);
            emit checkpointDeleted(checkpointId);
            return true;
        }
    }

    return false;
}

int CheckpointManager::pruneOldCheckpoints(int keepCount)
{
    int deleted = 0;

    while (static_cast<int>(m_checkpointIndex.size()) > keepCount && !m_checkpointIndex.empty()) {
        const auto& oldest = m_checkpointIndex.front();
        deleteCheckpoint(oldest.checkpointId);
        deleted++;
    }

    qDebug() << "Pruned" << deleted << "old checkpoints";
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
    m_lastAutoCheckpointStep = 0;
    m_lastAutoCheckpointEpoch = 0;
    qDebug() << "Auto-checkpointing enabled: every" << intervalSteps << "steps and" << saveEveryNEpochs << "epochs";
    return true;
}

void CheckpointManager::disableAutoCheckpointing()
{
    m_autoCheckpointEnabled = false;
    qDebug() << "Auto-checkpointing disabled";
}

bool CheckpointManager::shouldCheckpoint(int step, int epoch) const
{
    if (!m_autoCheckpointEnabled) {
        return false;
    }

    bool stepCheckpoint = (step - m_lastAutoCheckpointStep) >= m_autoCheckpointInterval;
    bool epochCheckpoint = (epoch - m_lastAutoCheckpointEpoch) >= m_autoCheckpointEpochInterval;

    return stepCheckpoint || epochCheckpoint;
}

bool CheckpointManager::validateCheckpoint(const QString& checkpointId) const
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".pt";
    QFile file(filePath);

    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Check file integrity (simplified: just check size > 0)
    bool valid = file.size() > 0;
    file.close();

    return valid;
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
    // In production: attempt to recover corrupted checkpoint
    qDebug() << "Attempting to repair checkpoint:" << checkpointId;
    return validateCheckpoint(checkpointId);
}

uint64_t CheckpointManager::getTotalCheckpointSize() const
{
    uint64_t total = 0;

    for (const auto& index : m_checkpointIndex) {
        QFile file(index.filePath);
        if (file.exists()) {
            total += file.size();
        }
    }

    return total;
}

uint64_t CheckpointManager::getCheckpointSize(const QString& checkpointId) const
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".pt";
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

    QJsonArray checkpointsArray;
    for (const auto& index : m_checkpointIndex) {
        QJsonObject cpObj;
        cpObj["checkpointId"] = index.checkpointId;
        cpObj["epoch"] = index.metadata.epoch;
        cpObj["step"] = index.metadata.step;
        cpObj["validationLoss"] = index.metadata.validationLoss;
        cpObj["accuracy"] = index.metadata.accuracy;
        cpObj["size"] = static_cast<qint64>(getCheckpointSize(index.checkpointId));
        cpObj["isBest"] = index.checkpointId == m_bestCheckpointId;
        checkpointsArray.append(cpObj);
    }
    report["checkpoints"] = checkpointsArray;

    return report;
}

QJsonObject CheckpointManager::compareCheckpoints(const QString& checkpointId1,
                                                 const QString& checkpointId2) const
{
    QJsonObject comparison;

    CheckpointMetadata meta1 = getCheckpointMetadata(checkpointId1);
    CheckpointMetadata meta2 = getCheckpointMetadata(checkpointId2);

    comparison["checkpoint1"] = checkpointId1;
    comparison["checkpoint2"] = checkpointId2;
    comparison["loss_diff"] = meta2.validationLoss - meta1.validationLoss;
    comparison["accuracy_diff"] = meta2.accuracy - meta1.accuracy;
    comparison["epoch_diff"] = meta2.epoch - meta1.epoch;
    comparison["step_diff"] = meta2.step - meta1.step;

    return comparison;
}

void CheckpointManager::setDistributedInfo(int rank, int worldSize)
{
    m_rank = rank;
    m_worldSize = worldSize;
    qDebug() << "Set distributed info: rank" << rank << "worldSize" << worldSize;
}

bool CheckpointManager::synchronizeDistributedCheckpoints()
{
    // In production: use distributed communication (MPI, NCCL)
    qDebug() << "Synchronizing distributed checkpoints (rank" << m_rank << ")";
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
    config["totalCheckpoints"] = static_cast<int>(m_checkpointIndex.size());
    config["bestCheckpointId"] = m_bestCheckpointId;
    return config;
}

bool CheckpointManager::importConfiguration(const QJsonObject& config)
{
    QString dir = config["checkpointDir"].toString();
    if (!dir.isEmpty()) {
        return initialize(dir, config["maxCheckpoints"].toInt(10));
    }

    return false;
}

bool CheckpointManager::saveConfigurationToFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonObject config = exportConfiguration();
    file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

bool CheckpointManager::loadConfigurationFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    return importConfiguration(doc.object());
}

QString CheckpointManager::generateCheckpointId()
{
    return QString("ckpt_%1_%2")
        .arg(QDateTime::currentSecsSinceEpoch())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

QByteArray CheckpointManager::compressState(const QByteArray& data, CompressionLevel level)
{
    int compressionLevel = Z_DEFAULT_COMPRESSION;

    switch (level) {
        case CompressionLevel::None: return data;
        case CompressionLevel::Low: compressionLevel = Z_BEST_SPEED; break;
        case CompressionLevel::Medium: compressionLevel = Z_DEFAULT_COMPRESSION; break;
        case CompressionLevel::High: compressionLevel = Z_BEST_COMPRESSION; break;
        case CompressionLevel::Maximum: compressionLevel = Z_BEST_COMPRESSION; break;
    }

    unsigned long compressedSize = compressBound(data.length());
    QByteArray compressed(compressedSize, 0);

    int ret = compress2(reinterpret_cast<unsigned char*>(compressed.data()),
                       &compressedSize,
                       reinterpret_cast<const unsigned char*>(data.data()),
                       data.length(),
                       compressionLevel);

    if (ret != Z_OK) {
        return QByteArray();
    }

    compressed.resize(compressedSize);
    return compressed;
}

QByteArray CheckpointManager::decompressState(const QByteArray& data)
{
    // Maximum expected uncompressed size (100 MB)
    unsigned long decompressedSize = 100 * 1024 * 1024;
    QByteArray decompressed(decompressedSize, 0);

    int ret = uncompress(reinterpret_cast<unsigned char*>(decompressed.data()),
                        &decompressedSize,
                        reinterpret_cast<const unsigned char*>(data.data()),
                        data.length());

    if (ret != Z_OK) {
        return QByteArray();
    }

    decompressed.resize(decompressedSize);
    return decompressed;
}

bool CheckpointManager::writeCheckpointToDisk(const QString& checkpointId,
                                             const CheckpointState& state,
                                             CompressionLevel compress)
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".pt";
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    // Write state (simplified: just write model weights)
    QByteArray toWrite = state.modelWeights;
    if (compress != CompressionLevel::None) {
        toWrite = compressState(toWrite, compress);
    }

    file.write(toWrite);
    file.close();

    return true;
}

bool CheckpointManager::readCheckpointFromDisk(const QString& checkpointId,
                                              CheckpointState& state)
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".pt";
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    // Try to decompress
    QByteArray decompressed = decompressState(data);
    if (!decompressed.isEmpty()) {
        state.modelWeights = decompressed;
    } else {
        state.modelWeights = data;
    }

    return true;
}
