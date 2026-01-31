#pragma once


#include <vector>
#include <map>

class AdvancedCheckpointManager : public void {

public:
    explicit AdvancedCheckpointManager(void* parent = nullptr);
    ~AdvancedCheckpointManager();

    // Checkpoint operations
    std::string createCheckpoint(const void*& state, const std::string& description = "");
    bool restoreCheckpoint(const std::string& checkpointId);
    bool deleteCheckpoint(const std::string& checkpointId);
    void* getCheckpointInfo(const std::string& checkpointId) const;

    // Checkpoint management
    void* listCheckpoints() const;
    bool pruneOldCheckpoints(int keepCount = 10);
    void* getCheckpointStats() const;

    // Compression and encryption
    void setCompressionEnabled(bool enabled);
    void setEncryptionEnabled(bool enabled, const std::string& key = "");

    // Distributed checkpointing
    bool syncWithRemote(const std::string& remoteUrl);
    bool backupToCloud(const std::string& cloudConfig);


    void checkpointCreated(const std::string& checkpointId);
    void checkpointRestored(const std::string& checkpointId);
    void checkpointDeleted(const std::string& checkpointId);
    void syncCompleted(bool success);

private:
    std::string m_checkpointDir;
    bool m_compressionEnabled;
    bool m_encryptionEnabled;
    std::string m_encryptionKey;
    std::map<std::string, void*> m_checkpoints;
};

