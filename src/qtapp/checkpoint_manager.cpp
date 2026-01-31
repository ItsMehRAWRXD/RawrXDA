#include "checkpoint_manager.h"


CheckpointManager::CheckpointManager(void* parent)
    : void(parent),
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
}

CheckpointManager::~CheckpointManager()
{
}

bool CheckpointManager::initialize(const std::string& checkpointDir, int maxCheckpoints)
{
    
    std::filesystem::path dir(checkpointDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
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
    return !m_checkpointDir.empty() && std::filesystem::path(m_checkpointDir).exists();
}

std::string CheckpointManager::saveCheckpoint(const CheckpointMetadata& metadata,
                                         const CheckpointState& state,
                                         CompressionLevel compress)
{
    if (!isInitialized()) {
        return std::string();
    }
    
    std::string checkpointId = generateCheckpointId();
    
    if (writeCheckpointToDisk(checkpointId, state, compress)) {
        CheckpointIndex index;
        index.checkpointId = checkpointId;
        index.filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
        index.metadata = metadata;
        index.checkpointNumber = m_checkpointCounter++;
        
        m_checkpointIndex.push_back(index);
        
        if (metadata.isBestModel) {
            m_bestCheckpointId = checkpointId;
            bestCheckpointUpdated(checkpointId, metadata.validationLoss);
        }
        
        checkpointSaved(checkpointId, metadata.epoch, metadata.step);
        return checkpointId;
    }
    
    checkpointError("Failed to save checkpoint");
    return std::string();
}

std::string CheckpointManager::quickSaveCheckpoint(const CheckpointMetadata& metadata,
                                              const CheckpointState& state)
{
    return saveCheckpoint(metadata, state, CompressionLevel::Low);
}

std::string CheckpointManager::saveModelWeights(const CheckpointMetadata& metadata,
                                           const std::vector<uint8_t>& modelWeights,
                                           CompressionLevel compress)
{
    CheckpointState state;
    state.modelWeights = modelWeights;
    return saveCheckpoint(metadata, state, compress);
}

bool CheckpointManager::loadCheckpoint(const std::string& checkpointId, CheckpointState& state)
{
    
    if (!readCheckpointFromDisk(checkpointId, state)) {
        checkpointError("Failed to load checkpoint: " + checkpointId);
        return false;
    }
    
    checkpointLoaded(checkpointId);
    return true;
}

std::string CheckpointManager::loadLatestCheckpoint(CheckpointState& state)
{
    if (m_checkpointIndex.empty()) {
        return std::string();
    }
    
    const auto& latest = m_checkpointIndex.back();
    if (loadCheckpoint(latest.checkpointId, state)) {
        return latest.checkpointId;
    }
    return std::string();
}

std::string CheckpointManager::loadBestCheckpoint(CheckpointState& state)
{
    if (m_bestCheckpointId.empty()) {
        return std::string();
    }
    
    if (loadCheckpoint(m_bestCheckpointId, state)) {
        return m_bestCheckpointId;
    }
    return std::string();
}

std::string CheckpointManager::loadCheckpointFromEpoch(int epoch, CheckpointState& state)
{
    for (const auto& index : m_checkpointIndex) {
        if (index.metadata.epoch == epoch) {
            if (loadCheckpoint(index.checkpointId, state)) {
                return index.checkpointId;
            }
        }
    }
    return std::string();
}

CheckpointManager::CheckpointMetadata CheckpointManager::getCheckpointMetadata(const std::string& checkpointId) const
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

bool CheckpointManager::deleteCheckpoint(const std::string& checkpointId)
{
    
    std::string filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    std::fstream file(filePath);
    
    if (!file.remove()) {
        return false;
    }
    
    // Remove from index
    auto it = std::remove_if(m_checkpointIndex.begin(), m_checkpointIndex.end(),
                             [&checkpointId](const CheckpointIndex& idx) {
                                 return idx.checkpointId == checkpointId;
                             });
    m_checkpointIndex.erase(it, m_checkpointIndex.end());
    
    checkpointDeleted(checkpointId);
    return true;
}

int CheckpointManager::pruneOldCheckpoints(int keepCount)
{
    
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
    if (m_bestCheckpointId.empty()) {
        return CheckpointMetadata();
    }
    return getCheckpointMetadata(m_bestCheckpointId);
}

bool CheckpointManager::updateCheckpointMetadata(const std::string& checkpointId,
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

bool CheckpointManager::setCheckpointNote(const std::string& checkpointId, const std::string& note)
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
    return true;
}

void CheckpointManager::disableAutoCheckpointing()
{
    m_autoCheckpointEnabled = false;
}

bool CheckpointManager::shouldCheckpoint(int step, int epoch) const
{
    if (!m_autoCheckpointEnabled) return false;
    
    bool stepBased = (step - m_lastAutoCheckpointStep) >= m_autoCheckpointInterval;
    bool epochBased = (epoch - m_lastAutoCheckpointEpoch) >= m_autoCheckpointEpochInterval;
    
    return stepBased || epochBased;
}

bool CheckpointManager::validateCheckpoint(const std::string& checkpointId) const
{
    std::string filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    std::fstream file(filePath);
    
    if (!file.exists()) {
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    std::vector<uint8_t> data = file.readAll();
    file.close();
    
    // Basic validation: check if not empty
    return !data.empty();
}

std::map<std::string, bool> CheckpointManager::validateAllCheckpoints() const
{
    std::map<std::string, bool> results;
    
    for (const auto& index : m_checkpointIndex) {
        results[index.checkpointId] = validateCheckpoint(index.checkpointId);
    }
    
    return results;
}

bool CheckpointManager::repairCheckpoint(const std::string& checkpointId)
{
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

uint64_t CheckpointManager::getCheckpointSize(const std::string& checkpointId) const
{
    std::string filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    std::fstream file(filePath);
    
    if (file.exists()) {
        return file.size();
    }
    
    return 0;
}

void* CheckpointManager::generateCheckpointReport() const
{
    void* report;
    report["totalCheckpoints"] = static_cast<int>(m_checkpointIndex.size());
    report["totalSize"] = static_cast<int64_t>(getTotalCheckpointSize());
    report["bestCheckpointId"] = m_bestCheckpointId;
    
    void* checkpoints;
    for (const auto& index : m_checkpointIndex) {
        void* cp;
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

void* CheckpointManager::compareCheckpoints(const std::string& checkpointId1,
                                                 const std::string& checkpointId2) const
{
    void* comparison;
    
    auto meta1 = getCheckpointMetadata(checkpointId1);
    auto meta2 = getCheckpointMetadata(checkpointId2);
    
    void* cp1;
    cp1["validationLoss"] = meta1.validationLoss;
    cp1["accuracy"] = meta1.accuracy;
    
    void* cp2;
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
}

bool CheckpointManager::synchronizeDistributedCheckpoints()
{
    // Placeholder for distributed sync logic
    return true;
}

void* CheckpointManager::exportConfiguration() const
{
    void* config;
    config["checkpointDir"] = m_checkpointDir;
    config["maxCheckpoints"] = m_maxCheckpoints;
    config["autoCheckpointEnabled"] = m_autoCheckpointEnabled;
    config["autoCheckpointInterval"] = m_autoCheckpointInterval;
    config["autoCheckpointEpochInterval"] = m_autoCheckpointEpochInterval;
    return config;
}

bool CheckpointManager::importConfiguration(const void*& config)
{
    m_checkpointDir = config["checkpointDir"].toString();
    m_maxCheckpoints = config["maxCheckpoints"].toInt(10);
    m_autoCheckpointEnabled = config["autoCheckpointEnabled"].toBool();
    m_autoCheckpointInterval = config["autoCheckpointInterval"].toInt(100);
    m_autoCheckpointEpochInterval = config["autoCheckpointEpochInterval"].toInt(1);
    return true;
}

bool CheckpointManager::saveConfigurationToFile(const std::string& filePath) const
{
    void* doc(exportConfiguration());
    std::fstream file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    return true;
}

bool CheckpointManager::loadConfigurationFromFile(const std::string& filePath)
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    std::vector<uint8_t> data = file.readAll();
    file.close();
    
    void* doc = void*::fromJson(data);
    return importConfiguration(doc.object());
}

std::string CheckpointManager::generateCheckpointId()
{
    return std::string("checkpoint_%1_%2")
        .toMSecsSinceEpoch())
        ;
}

std::vector<uint8_t> CheckpointManager::compressState(const std::vector<uint8_t>& data, CompressionLevel level)
{
    if (level == CompressionLevel::None || data.empty()) {
        return data;
    }
    
    // Placeholder for zlib compression
    // In production, use zlib or similar
    return data;
}

std::vector<uint8_t> CheckpointManager::decompressState(const std::vector<uint8_t>& data)
{
    // Placeholder for decompression
    return data;
}

bool CheckpointManager::writeCheckpointToDisk(const std::string& checkpointId,
                                             const CheckpointState& state,
                                             CompressionLevel compress)
{
    std::string filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    std::fstream file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    // Write state - serialize config to JSON first
    void* configDoc(state.config);
    std::vector<uint8_t> configData = configDoc.toJson(void*::Compact);
    
    file.write(state.modelWeights);
    file.write(state.optimizerState);
    file.write(state.schedulerState);
    file.write(state.trainingState);
    file.write(configData);
    
    file.close();
    return true;
}

bool CheckpointManager::readCheckpointFromDisk(const std::string& checkpointId,
                                              CheckpointState& state)
{
    std::string filePath = m_checkpointDir + "/" + checkpointId + ".ckpt";
    std::fstream file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    std::vector<uint8_t> data = file.readAll();
    file.close();
    
    // Placeholder: in production, properly deserialize
    state.modelWeights = data;
    return true;
}



