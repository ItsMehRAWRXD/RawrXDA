#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

/**
 * @class CheckpointManager
 * @brief Save, manage, and restore training checkpoints (Win32/Qt-free)
 *
 * Features:
 * - Save model state, optimizer state, training state
 * - Checkpoint versioning and history
 * - Automatic checkpointing (interval-based)
 * - Best model tracking (based on validation metric)
 * - Checkpoint validation and rollback
 */
class CheckpointManager
{
public:
    enum class CompressionLevel {
        None,
        Low,
        Medium,
        High,
        Maximum
    };

    struct CheckpointMetadata {
        std::string checkpointId;
        int epoch = 0;
        int step = 0;
        int64_t timestamp = 0;
        float validationLoss = 0.f;
        float trainLoss = 0.f;
        float accuracy = 0.f;
        float wallclockTime = 0.f;
        int modelSize = 0;
        std::string modelArchitecture;
        std::string hyperparameters;
        std::string datasetVersion;
        bool isBestModel = false;
        std::string notes;
    };

    struct CheckpointState {
        std::vector<uint8_t> modelWeights;
        std::vector<uint8_t> optimizerState;
        std::vector<uint8_t> schedulerState;
        std::vector<uint8_t> trainingState;
        std::string config;
    };

    struct CheckpointIndex {
        std::string checkpointId;
        std::string filePath;
        CheckpointMetadata metadata;
        int checkpointNumber = 0;
    };

    using SavedCallback = void (*)(void* ctx, const char* checkpointId, int epoch, int step);
    using LoadedCallback = void (*)(void* ctx, const char* checkpointId);
    using DeletedCallback = void (*)(void* ctx, const char* checkpointId);

    explicit CheckpointManager(void* parent = nullptr);
    ~CheckpointManager();

    static CheckpointManager& instance() {
        static CheckpointManager inst;
        return inst;
    }

    bool initialize(const std::string& checkpointDir, int maxCheckpoints = 10);
    bool isInitialized() const;

    std::string saveCheckpoint(const CheckpointMetadata& metadata,
                               const CheckpointState& state,
                               CompressionLevel compress = CompressionLevel::Medium);
    std::string quickSaveCheckpoint(const CheckpointMetadata& metadata,
                                    const CheckpointState& state);
    std::string saveModelWeights(const CheckpointMetadata& metadata,
                                 const std::vector<uint8_t>& modelWeights,
                                 CompressionLevel compress = CompressionLevel::Medium);

    bool loadCheckpoint(const std::string& checkpointId, CheckpointState& state);
    std::string loadLatestCheckpoint(CheckpointState& state);
    std::string loadBestCheckpoint(CheckpointState& state);
    std::string loadCheckpointFromEpoch(int epoch, CheckpointState& state);
    CheckpointMetadata getCheckpointMetadata(const std::string& checkpointId) const;

    std::vector<CheckpointIndex> listCheckpoints() const;
    std::vector<CheckpointIndex> getCheckpointHistory(int limit = 10) const;
    bool deleteCheckpoint(const std::string& checkpointId);
    int pruneOldCheckpoints(int keepCount);
    CheckpointMetadata getBestCheckpointInfo() const;
    bool updateCheckpointMetadata(const std::string& checkpointId,
                                  const CheckpointMetadata& metadata);
    bool setCheckpointNote(const std::string& checkpointId, const std::string& note);

    bool enableAutoCheckpointing(int intervalSteps, int saveEveryNEpochs = 1);
    void disableAutoCheckpointing();
    bool shouldCheckpoint(int step, int epoch) const;

    bool validateCheckpoint(const std::string& checkpointId) const;
    std::map<std::string, bool> validateAllCheckpoints() const;
    bool repairCheckpoint(const std::string& checkpointId);

    uint64_t getTotalCheckpointSize() const;
    uint64_t getCheckpointSize(const std::string& checkpointId) const;
    std::string generateCheckpointReport() const;
    std::string compareCheckpoints(const std::string& checkpointId1,
                                   const std::string& checkpointId2) const;

    void setDistributedInfo(int rank, int worldSize);
    bool synchronizeDistributedCheckpoints();

    std::string exportConfiguration() const;
    bool importConfiguration(const std::string& config);
    bool saveConfigurationToFile(const std::string& filePath) const;
    bool loadConfigurationFromFile(const std::string& filePath);

    using ShowCallback = std::function<void(void*, const std::vector<CheckpointIndex>&)>;
    void setShowCallback(ShowCallback cb, void* ctx);
    void show();

    void setSavedCallback(void* ctx, SavedCallback cb) { m_savedCtx = ctx; m_savedCb = cb; }
    void setLoadedCallback(void* ctx, LoadedCallback cb) { m_loadedCtx = ctx; m_loadedCb = cb; }
    void setDeletedCallback(void* ctx, DeletedCallback cb) { m_deletedCtx = ctx; m_deletedCb = cb; }

private:
    std::string m_checkpointDir;
    int m_maxCheckpoints = 10;
    int m_checkpointCounter = 0;

    bool m_autoCheckpointEnabled = false;
    int m_autoCheckpointInterval = 0;
    int m_autoCheckpointEpochInterval = 1;
    int m_lastAutoCheckpointStep = -1;
    int m_lastAutoCheckpointEpoch = -1;

    int m_rank = 0;
    int m_worldSize = 1;

    std::vector<CheckpointIndex> m_checkpointIndex;
    std::string m_bestCheckpointId;

    void* m_savedCtx = nullptr;
    SavedCallback m_savedCb = nullptr;
    void* m_loadedCtx = nullptr;
    LoadedCallback m_loadedCb = nullptr;
    void* m_deletedCtx = nullptr;
    DeletedCallback m_deletedCb = nullptr;

    ShowCallback m_showCb;
    void* m_showCtx = nullptr;

    std::string generateCheckpointId();
    std::vector<uint8_t> compressState(const std::vector<uint8_t>& data, CompressionLevel level);
    std::vector<uint8_t> decompressState(const std::vector<uint8_t>& data);
    bool writeCheckpointToDisk(const std::string& checkpointId,
                               const CheckpointState& state,
                               CompressionLevel compress);
    bool readCheckpointFromDisk(const std::string& checkpointId, CheckpointState& state);
};
