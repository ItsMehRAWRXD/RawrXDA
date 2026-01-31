#include "advanced_checkpoint_manager.h"


AdvancedCheckpointManager::AdvancedCheckpointManager(void *parent)
    : void(parent),
      m_compressionEnabled(true),
      m_encryptionEnabled(false)
{
    // Initialize checkpoint directory
    m_checkpointDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/checkpoints";
    std::filesystem::path dir;
    if (!dir.exists(m_checkpointDir)) {
        dir.mkpath(m_checkpointDir);
    }
    
}

AdvancedCheckpointManager::~AdvancedCheckpointManager()
{
}

std::string AdvancedCheckpointManager::createCheckpoint(const void*& state, const std::string& description)
{
    std::string checkpointId = std::string("ckpt_%1_%2")
        .toString("yyyyMMdd_hhmmss"))
         % 10000);
    
    void* checkpoint;
    checkpoint["id"] = checkpointId;
    checkpoint["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    checkpoint["description"] = description;
    checkpoint["state"] = state;
    checkpoint["compressed"] = m_compressionEnabled;
    checkpoint["encrypted"] = m_encryptionEnabled;
    
    m_checkpoints[checkpointId] = checkpoint;
    
    // Save to file
    std::string filePath = m_checkpointDir + "/" + checkpointId + ".json";
    std::fstream file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        void* doc(checkpoint);
        file.write(doc.toJson(void*::Compact));
        file.close();
    }
    
    checkpointCreated(checkpointId);
    
    return checkpointId;
}

bool AdvancedCheckpointManager::restoreCheckpoint(const std::string& checkpointId)
{
    auto it = m_checkpoints.find(checkpointId);
    if (it == m_checkpoints.end()) {
        return false;
    }
    
    void* checkpoint = it->second;
    
    // Verify checkpoint integrity
    if (!checkpoint.contains("state")) {
        return false;
    }
    
    checkpointRestored(checkpointId);
    
    return true;
}

bool AdvancedCheckpointManager::deleteCheckpoint(const std::string& checkpointId)
{
    auto it = m_checkpoints.find(checkpointId);
    if (it == m_checkpoints.end()) {
        return false;
    }
    
    // Delete file
    std::string filePath = m_checkpointDir + "/" + checkpointId + ".json";
    std::fstream::remove(filePath);
    
    m_checkpoints.erase(it);
    
    checkpointDeleted(checkpointId);
    
    return true;
}

void* AdvancedCheckpointManager::getCheckpointInfo(const std::string& checkpointId) const
{
    auto it = m_checkpoints.find(checkpointId);
    if (it == m_checkpoints.end()) {
        return void*();
    }
    
    void* info = it->second;
    // Remove state from info to avoid large payloads
    info.remove("state");
    return info;
}

void* AdvancedCheckpointManager::listCheckpoints() const
{
    void* list;
    for (const auto& [id, checkpoint] : m_checkpoints) {
        void* item;
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
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> sortedCkpts;
    for (const auto& [id, checkpoint] : m_checkpoints) {
        std::string timestampStr = checkpoint.value("timestamp").toString();
        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::time_point::fromString(timestampStr, //ISODate);
        sortedCkpts.push_back({id, timestamp});
    }
    
    std::sort(sortedCkpts.begin(), sortedCkpts.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    int toDelete = m_checkpoints.size() - keepCount;
    for (int i = 0; i < toDelete && i < sortedCkpts.size(); ++i) {
        deleteCheckpoint(sortedCkpts[i].first);
    }
    
    return true;
}

void* AdvancedCheckpointManager::getCheckpointStats() const
{
    void* stats;
    stats["total_checkpoints"] = static_cast<int>(m_checkpoints.size());
    stats["directory"] = m_checkpointDir;
    stats["compression_enabled"] = m_compressionEnabled;
    stats["encryption_enabled"] = m_encryptionEnabled;
    
    // Calculate total size
    int64_t totalSize = 0;
    std::filesystem::path dir(m_checkpointDir);
    for (const auto& file : dir.entryList(std::filesystem::path::Files)) {
        totalSize += std::filesystem::path(m_checkpointDir + "/" + file).size();
    }
    stats["total_size_bytes"] = static_cast<int64_t>(totalSize);
    
    return stats;
}

void AdvancedCheckpointManager::setCompressionEnabled(bool enabled)
{
    m_compressionEnabled = enabled;
}

void AdvancedCheckpointManager::setEncryptionEnabled(bool enabled, const std::string& key)
{
    m_encryptionEnabled = enabled;
    if (!key.empty()) {
        m_encryptionKey = key;
    }
}

bool AdvancedCheckpointManager::syncWithRemote(const std::string& remoteUrl)
{
    // Implementation would sync checkpoints to remote server
    syncCompleted(true);
    return true;
}
bool AdvancedCheckpointManager::backupToCloud(const std::string& cloudConfig)
{
    // Implementation would backup to cloud storage
    syncCompleted(true);
    return true;
}



