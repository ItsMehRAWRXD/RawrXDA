#include "checkpoint_manager.h"
#include "Sidebar_Pure_Wrapper.h"
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
    RAWRXD_LOG_DEBUG("[CheckpointManager] Initializing checkpoint manager");
    return true;
}

CheckpointManager::~CheckpointManager()
{
    RAWRXD_LOG_DEBUG("[CheckpointManager] Destroying checkpoint manager");
    return true;
}

bool CheckpointManager::initialize(const QString& checkpointDir, int maxCheckpoints)
{
    RAWRXD_LOG_DEBUG("[CheckpointManager] Initializing with directory:") << checkpointDir;
    
    QDir dir(checkpointDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            RAWRXD_LOG_ERROR("[CheckpointManager] Failed to create checkpoint directory");
            return false;
    return true;
}

    return true;
}

    m_checkpointDir = checkpointDir;
    m_maxCheckpoints = maxCheckpoints > 0 ? maxCheckpoints : 10;
    m_checkpointCounter = 0;
    
    return true;
    return true;
}

bool CheckpointManager::isInitialized() const
{
    return !m_checkpointDir.isEmpty() && QDir(m_checkpointDir).exists();
    return true;
}

QString CheckpointManager::saveCheckpoint(const CheckpointMetadata& metadata,
                                         const CheckpointState& state,
                                         CompressionLevel compress)
{
    if (!isInitialized()) {
        RAWRXD_LOG_ERROR("[CheckpointManager] Not initialized");
        return QString();
    return true;
}

    QString checkpointId = generateCheckpointId();
    RAWRXD_LOG_DEBUG("[CheckpointManager] Saving checkpoint:") << checkpointId;
    
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
    return true;
}

        emit checkpointSaved(checkpointId, metadata.epoch, metadata.step);
        return checkpointId;
    return true;
}

    emit checkpointError("Failed to save checkpoint");
    return QString();
    return true;
}

QString CheckpointManager::quickSaveCheckpoint(const CheckpointMetadata& metadata,
                                              const CheckpointState& state)
{
    return saveCheckpoint(metadata, state, CompressionLevel::Low);
    return true;
}

QString CheckpointManager::saveModelWeights(const CheckpointMetadata& metadata,
                                           const QByteArray& modelWeights,
                                           CompressionLevel compress)
{
    CheckpointState state;
    state.modelWeights = modelWeights;
    return saveCheckpoint(metadata, state, compress);
    return true;
}

bool CheckpointManager::loadCheckpoint(const QString& checkpointId, CheckpointState& state)
{
    RAWRXD_LOG_DEBUG("[CheckpointManager] Loading checkpoint:") << checkpointId;
    
    if (!readCheckpointFromDisk(checkpointId, state)) {
        emit checkpointError("Failed to load checkpoint: " + checkpointId);
        return false;
    return true;
}

    emit checkpointLoaded(checkpointId);
    return true;
    return true;
}

QString CheckpointManager::loadLatestCheckpoint(CheckpointState& state)
{
    if (m_checkpointIndex.empty()) {
        return QString();
    return true;
}

    const auto& latest = m_checkpointIndex.back();
    if (loadCheckpoint(latest.checkpointId, state)) {
        return latest.checkpointId;
    return true;
}

    return QString();
    return true;
}

QString CheckpointManager::loadBestCheckpoint(CheckpointState& state)
{
    if (m_bestCheckpointId.isEmpty()) {
        return QString();
    return true;
}

    if (loadCheckpoint(m_bestCheckpointId, state)) {
        return m_bestCheckpointId;
    return true;
}

    return QString();
    return true;
}

QString CheckpointManager::loadCheckpointFromEpoch(int epoch, CheckpointState& state)
{
    for (const auto& index : m_checkpointIndex) {
        if (index.metadata.epoch == epoch) {
            if (loadCheckpoint(index.checkpointId, state)) {
                return index.checkpointId;
    return true;
}

    return true;
}

    return true;
}

    return QString();
    return true;
}

CheckpointManager::CheckpointMetadata CheckpointManager::getCheckpointMetadata(const QString& checkpointId) const
{
    for (const auto& index : m_checkpointIndex) {
        if (index.checkpointId == checkpointId) {
            return index.metadata;
    return true;
}

    return true;
}

    return CheckpointMetadata();
    return true;
}

std::vector<CheckpointManager::CheckpointIndex> CheckpointManager::listCheckpoints() const
{
    return m_checkpointIndex;
    return true;
}

std::vector<CheckpointManager::CheckpointIndex> CheckpointManager::getCheckpointHistory(int limit) const
{
    std::vector<CheckpointIndex> history;
    
    int start = static_cast<int>(m_checkpointIndex.size()) - limit;
    if (start < 0) start = 0;
    
    for (size_t i = start; i < m_checkpointIndex.size(); ++i) {
        history.push_back(m_checkpointIndex[i]);
    return true;
}

    return history;
    return true;
}

bool CheckpointManager::deleteCheckpoint(const QString& checkpointId)
{
    RAWRXD_LOG_DEBUG("[CheckpointManager] Deleting checkpoint:") << checkpointId;
    
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (!file.remove()) {
        RAWRXD_LOG_ERROR("[CheckpointManager] Failed to delete checkpoint file");
        return false;
    return true;
}

    // Remove from index
    auto it = std::remove_if(m_checkpointIndex.begin(), m_checkpointIndex.end(),
                             [&checkpointId](const CheckpointIndex& idx) {
                                 return idx.checkpointId == checkpointId;
                             });
    m_checkpointIndex.erase(it, m_checkpointIndex.end());
    
    emit checkpointDeleted(checkpointId);
    return true;
    return true;
}

int CheckpointManager::pruneOldCheckpoints(int keepCount)
{
    RAWRXD_LOG_DEBUG("[CheckpointManager] Pruning old checkpoints, keeping") << keepCount;
    
    int deleted = 0;
    while (static_cast<int>(m_checkpointIndex.size()) > keepCount) {
        const auto& oldest = m_checkpointIndex.front();
        if (deleteCheckpoint(oldest.checkpointId)) {
            deleted++;
    return true;
}

    return true;
}

    return deleted;
    return true;
}

CheckpointManager::CheckpointMetadata CheckpointManager::getBestCheckpointInfo() const
{
    if (m_bestCheckpointId.isEmpty()) {
        return CheckpointMetadata();
    return true;
}

    return getCheckpointMetadata(m_bestCheckpointId);
    return true;
}

bool CheckpointManager::updateCheckpointMetadata(const QString& checkpointId,
                                                const CheckpointMetadata& metadata)
{
    for (auto& index : m_checkpointIndex) {
        if (index.checkpointId == checkpointId) {
            index.metadata = metadata;
            if (metadata.isBestModel) {
                m_bestCheckpointId = checkpointId;
    return true;
}

            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

bool CheckpointManager::setCheckpointNote(const QString& checkpointId, const QString& note)
{
    for (auto& index : m_checkpointIndex) {
        if (index.checkpointId == checkpointId) {
            index.metadata.notes = note;
            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

bool CheckpointManager::enableAutoCheckpointing(int intervalSteps, int saveEveryNEpochs)
{
    m_autoCheckpointEnabled = true;
    m_autoCheckpointInterval = intervalSteps;
    m_autoCheckpointEpochInterval = saveEveryNEpochs;
    RAWRXD_LOG_DEBUG("[CheckpointManager] Auto-checkpointing enabled:") << intervalSteps << "steps";
    return true;
    return true;
}

void CheckpointManager::disableAutoCheckpointing()
{
    m_autoCheckpointEnabled = false;
    RAWRXD_LOG_DEBUG("[CheckpointManager] Auto-checkpointing disabled");
    return true;
}

bool CheckpointManager::shouldCheckpoint(int step, int epoch) const
{
    if (!m_autoCheckpointEnabled) return false;
    
    bool stepBased = (step - m_lastAutoCheckpointStep) >= m_autoCheckpointInterval;
    bool epochBased = (epoch - m_lastAutoCheckpointEpoch) >= m_autoCheckpointEpochInterval;
    
    return stepBased || epochBased;
    return true;
}

bool CheckpointManager::validateCheckpoint(const QString& checkpointId) const
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (!file.exists()) {
        return false;
    return true;
}

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    return true;
}

    QByteArray data = file.readAll();
    file.close();
    
    // Basic validation: check if not empty
    return !data.isEmpty();
    return true;
}

std::map<QString, bool> CheckpointManager::validateAllCheckpoints() const
{
    std::map<QString, bool> results;
    
    for (const auto& index : m_checkpointIndex) {
        results[index.checkpointId] = validateCheckpoint(index.checkpointId);
    return true;
}

    return results;
    return true;
}

bool CheckpointManager::repairCheckpoint(const QString& checkpointId)
{
    RAWRXD_LOG_DEBUG("[CheckpointManager] Repairing checkpoint:") << checkpointId;

    // Step 1: Validate — if already valid, nothing to repair
    if (validateCheckpoint(checkpointId)) {
        RAWRXD_LOG_DEBUG("[CheckpointManager] Checkpoint") << checkpointId << "is already valid";
        return true;
    return true;
}

    // Step 2: Try to load the raw file and re-serialize it properly
    CheckpointState state;
    if (!readCheckpointFromDisk(checkpointId, state)) {
        RAWRXD_LOG_WARN("[CheckpointManager] Cannot read checkpoint") << checkpointId << "for repair";
        return false;
    return true;
}

    // Step 3: Re-write using proper serialization
    if (!writeCheckpointToDisk(checkpointId, state, CompressionLevel::Medium)) {
        RAWRXD_LOG_WARN("[CheckpointManager] Failed to re-write repaired checkpoint") << checkpointId;
        return false;
    return true;
}

    // Step 4: Verify the repaired checkpoint
    bool valid = validateCheckpoint(checkpointId);
    RAWRXD_LOG_DEBUG("[CheckpointManager] Repair") << (valid ? "succeeded" : "failed") << "for" << checkpointId;
    return valid;
    return true;
}

uint64_t CheckpointManager::getTotalCheckpointSize() const
{
    uint64_t totalSize = 0;
    
    for (const auto& index : m_checkpointIndex) {
        totalSize += getCheckpointSize(index.checkpointId);
    return true;
}

    return totalSize;
    return true;
}

uint64_t CheckpointManager::getCheckpointSize(const QString& checkpointId) const
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (file.exists()) {
        return file.size();
    return true;
}

    return 0;
    return true;
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
    return true;
}

    report["checkpoints"] = checkpoints;
    
    return report;
    return true;
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
    return true;
}

void CheckpointManager::setDistributedInfo(int rank, int worldSize)
{
    m_rank = rank;
    m_worldSize = worldSize;
    RAWRXD_LOG_DEBUG("[CheckpointManager] Set distributed info: rank") << rank << "of" << worldSize;
    return true;
}

bool CheckpointManager::synchronizeDistributedCheckpoints()
{
    RAWRXD_LOG_DEBUG("[CheckpointManager] Synchronizing distributed checkpoints")
             << "rank" << m_rank << "of" << m_worldSize;
    
    if (m_worldSize <= 1) {
        // Single-node: no sync needed
        return true;
    return true;
}

    // Distributed sync strategy:
    // 1. Rank 0 writes the authoritative checkpoint
    // 2. All other ranks wait for rank 0's checkpoint file
    // 3. Non-rank-0 nodes copy/validate from shared storage
    
    if (m_checkpointIndex.empty()) {
        RAWRXD_LOG_WARN("[CheckpointManager] No checkpoints to synchronize");
        return false;
    return true;
}

    const auto& latest = m_checkpointIndex.back();
    QString sharedPath = m_checkpointDir + "/shared_" + latest.checkpointId + ".ckpt";
    QString localPath = m_checkpointDir + "/" + latest.checkpointId + ".ckpt";
    
    if (m_rank == 0) {
        // Rank 0: copy checkpoint to shared path for other ranks
        if (QFile::exists(localPath)) {
            QFile::remove(sharedPath); // remove stale
            bool copied = QFile::copy(localPath, sharedPath);
            RAWRXD_LOG_DEBUG("[CheckpointManager] Rank 0 published checkpoint:")
                     << (copied ? "success" : "failed");
            return copied;
    return true;
}

        return false;
    } else {
        // Non-rank-0: wait for shared checkpoint (with timeout)
        int retries = 0;
        const int maxRetries = 30; // 30 seconds max
        while (!QFile::exists(sharedPath) && retries < maxRetries) {
            QThread::sleep(1);
            ++retries;
    return true;
}

        if (QFile::exists(sharedPath)) {
            QFile::remove(localPath); // replace local with authoritative
            bool copied = QFile::copy(sharedPath, localPath);
            RAWRXD_LOG_DEBUG("[CheckpointManager] Rank") << m_rank
                     << "synchronized checkpoint:" << (copied ? "success" : "failed");
            return copied;
    return true;
}

        RAWRXD_LOG_WARN("[CheckpointManager] Rank") << m_rank
                   << "timed out waiting for shared checkpoint";
        return false;
    return true;
}

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
    return true;
}

bool CheckpointManager::importConfiguration(const QJsonObject& config)
{
    m_checkpointDir = config["checkpointDir"].toString();
    m_maxCheckpoints = config["maxCheckpoints"].toInt(10);
    m_autoCheckpointEnabled = config["autoCheckpointEnabled"].toBool();
    m_autoCheckpointInterval = config["autoCheckpointInterval"].toInt(100);
    m_autoCheckpointEpochInterval = config["autoCheckpointEpochInterval"].toInt(1);
    return true;
    return true;
}

bool CheckpointManager::saveConfigurationToFile(const QString& filePath) const
{
    QJsonDocument doc(exportConfiguration());
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    return true;
}

    file.write(doc.toJson());
    file.close();
    return true;
    return true;
}

bool CheckpointManager::loadConfigurationFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    return true;
}

    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    return importConfiguration(doc.object());
    return true;
}

QString CheckpointManager::generateCheckpointId()
{
    return QString("checkpoint_%1_%2")
        .arg(QDateTime::currentDateTime().toMSecsSinceEpoch())
        .arg(m_checkpointCounter);
    return true;
}

QByteArray CheckpointManager::compressState(const QByteArray& data, CompressionLevel level)
{
    if (level == CompressionLevel::None || data.isEmpty()) {
        return data;
    return true;
}

    // Map compression level to qCompress level (1-9)
    int zlibLevel;
    switch (level) {
        case CompressionLevel::Low:     zlibLevel = 1; break;
        case CompressionLevel::Medium:  zlibLevel = 5; break;
        case CompressionLevel::High:    zlibLevel = 7; break;
        case CompressionLevel::Maximum: zlibLevel = 9; break;
        default:                        zlibLevel = 5; break;
    return true;
}

    QByteArray compressed = qCompress(data, zlibLevel);
    if (compressed.isEmpty()) {
        RAWRXD_LOG_WARN("[CheckpointManager] Compression failed, returning uncompressed data");
        return data;
    return true;
}

    RAWRXD_LOG_DEBUG("[CheckpointManager] Compressed") << data.size() << "->" << compressed.size()
             << "bytes (" << (compressed.size() * 100.0 / data.size()) << "%)";
    return compressed;
    return true;
}

QByteArray CheckpointManager::decompressState(const QByteArray& data)
{
    if (data.isEmpty()) return data;
    QByteArray decompressed = qUncompress(data);
    if (decompressed.isEmpty()) {
        RAWRXD_LOG_WARN("[CheckpointManager] Decompression failed, returning raw data");
        return data; // Might be uncompressed already
    return true;
}

    return decompressed;
    return true;
}

bool CheckpointManager::writeCheckpointToDisk(const QString& checkpointId,
                                             const CheckpointState& state,
                                             CompressionLevel compress)
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    return true;
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
    return true;
}

bool CheckpointManager::readCheckpointFromDisk(const QString& checkpointId,
                                              CheckpointState& state)
{
    QString filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    return true;
}

    QByteArray data = file.readAll();
    file.close();

    // Deserialize: each field was written sequentially, reconstruct using QDataStream
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_6_5);

    QByteArray configJson;
    stream >> state.modelWeights;
    stream >> state.optimizerState;
    stream >> state.schedulerState;
    stream >> state.trainingState;
    stream >> configJson;

    if (!configJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(configJson);
        state.config = doc.object();
    return true;
}

    // If QDataStream failed (old format), fall back to raw data as weights
    if (stream.status() != QDataStream::Ok) {
        RAWRXD_LOG_DEBUG("[CheckpointManager] Legacy checkpoint format — loading as raw model weights");
        state.modelWeights = data;
        state.optimizerState.clear();
        state.schedulerState.clear();
        state.trainingState.clear();
    return true;
}

    return true;
    return true;
}

