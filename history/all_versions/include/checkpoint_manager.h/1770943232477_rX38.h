#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

/**
 * @class CheckpointManager
 * @brief Save, manage, and restore training checkpoints — Qt-free C++20
 *
 * Features:
 * - Save model state, optimizer state, training state
 * - Checkpoint versioning and history
 * - Automatic checkpointing (interval-based)
 * - Best model tracking (based on validation metric)
 * - Model state compression and decompression
 * - Efficient incremental snapshots
 * - Checkpoint validation and integrity checking
 * - Rollback to previous checkpoints
 * - Distributed checkpoint (multi-GPU/node)
 * - Cloud storage support (optional)
 */
class CheckpointManager
{
public:
    enum class CompressionLevel {
        None,
        Low,            // Quick compression
        Medium,
        High,           // Slow but small
        Maximum         // Very slow
    };

    struct CheckpointMetadata {
        std::string checkpointId;
        int epoch = 0;
        int step = 0;
        int64_t timestamp = 0;
        float validationLoss = 0.0f;
        float trainLoss = 0.0f;
        float accuracy = 0.0f;
        float wallclockTime = 0.0f; // Training time so far (seconds)
        int modelSize = 0;          // In bytes
        std::string modelArchitecture;
        std::string hyperparameters;    // JSON
        std::string datasetVersion;
        bool isBestModel = false;
        std::string notes;
    };

    struct CheckpointState {
        std::vector<uint8_t> modelWeights;
        std::vector<uint8_t> optimizerState;
        std::vector<uint8_t> schedulerState;
        std::vector<uint8_t> trainingState;   // Epoch, step, loss history
        std::string config;                    // JSON string
    };

    struct CheckpointIndex {
        std::string checkpointId;
        std::string filePath;
        CheckpointMetadata metadata;
        int checkpointNumber = 0;       // Sequential number
    };

    // Constructor
    CheckpointManager() = default;
    ~CheckpointManager() = default;

    // Singleton access
    static CheckpointManager& instance()
    {
        static CheckpointManager inst;
        return inst;
    }

    /**
     * @brief Initialize checkpoint manager
     * @param checkpointDir Directory to store checkpoints
     * @param maxCheckpoints Maximum number to keep (0 = unlimited)
     * @return true if successful
     */
    bool initialize(const std::string& checkpointDir, int maxCheckpoints = 10);

    /**
     * @brief Check if initialized
     * @return true if ready
     */
    bool isInitialized() const;

    // ===== Checkpoint Creation =====
    /**
     * @brief Save checkpoint during training
     * @param metadata Checkpoint metadata (epoch, step, metrics)
     * @param state Model and optimizer state
     * @param compress Compression level
     * @return Checkpoint ID or empty if failed
     */
    std::string saveCheckpoint(const CheckpointMetadata& metadata,
                          const CheckpointState& state,
                          CompressionLevel compress = CompressionLevel::Medium);

    /**
     * @brief Quick save (lightweight checkpoint)
     * @param metadata Checkpoint metadata
     * @param state State to save
     * @return Checkpoint ID or empty if failed
     */
    std::string quickSaveCheckpoint(const CheckpointMetadata& metadata,
                               const CheckpointState& state);

    /**
     * @brief Save only model weights (smaller checkpoint)
     * @param metadata Checkpoint metadata
     * @param modelWeights Model weights data
     * @param compress Compression level
     * @return Checkpoint ID or empty if failed
     */
    std::string saveModelWeights(const CheckpointMetadata& metadata,
                            const std::vector<uint8_t>& modelWeights,
                            CompressionLevel compress = CompressionLevel::Medium);

    // ===== Checkpoint Retrieval =====
    /**
     * @brief Load checkpoint
     * @param checkpointId Checkpoint to load
     * @param state Output: loaded state
     * @return true if successful
     */
    bool loadCheckpoint(const std::string& checkpointId, CheckpointState& state);

    /**
     * @brief Load latest checkpoint
     * @param state Output: loaded state
     * @return Checkpoint ID or empty if no checkpoints
     */
    std::string loadLatestCheckpoint(CheckpointState& state);

    /**
     * @brief Load best checkpoint (by validation metric)
     * @param state Output: loaded state
     * @return Checkpoint ID or empty if not found
     */
    std::string loadBestCheckpoint(CheckpointState& state);

    /**
     * @brief Load checkpoint from epoch
     * @param epoch Target epoch
     * @param state Output: loaded state
     * @return Checkpoint ID or empty if not found
     */
    std::string loadCheckpointFromEpoch(int epoch, CheckpointState& state);

    /**
     * @brief Get checkpoint metadata
     * @param checkpointId Checkpoint to query
     * @return Metadata or empty if not found
     */
    CheckpointMetadata getCheckpointMetadata(const std::string& checkpointId) const;

    // ===== Checkpoint Management =====
    /**
     * @brief List all saved checkpoints
     * @return Vector of checkpoint indices
     */
    std::vector<CheckpointIndex> listCheckpoints() const;

    /**
     * @brief Get checkpoint history (last N checkpoints)
     * @param limit Number to return
     * @return History of recent checkpoints
     */
    std::vector<CheckpointIndex> getCheckpointHistory(int limit = 10) const;

    /**
     * @brief Delete checkpoint
     * @param checkpointId Checkpoint to delete
     * @return true if successful
     */
    bool deleteCheckpoint(const std::string& checkpointId);

    /**
     * @brief Delete old checkpoints (keep only recent)
     * @param keepCount Number of recent to keep
     * @return Number of deleted checkpoints
     */
    int pruneOldCheckpoints(int keepCount);

    /**
     * @brief Get best checkpoint info
     * @return Metadata of best checkpoint or empty
     */
    CheckpointMetadata getBestCheckpointInfo() const;

    /**
     * @brief Update checkpoint metadata (e.g., mark as best)
     * @param checkpointId Checkpoint to update
     * @param metadata Updated metadata
     * @return true if successful
     */
    bool updateCheckpointMetadata(const std::string& checkpointId,
                                 const CheckpointMetadata& metadata);

    /**
     * @brief Set note for checkpoint
     * @param checkpointId Checkpoint to annotate
     * @param note Note text
     * @return true if successful
     */
    bool setCheckpointNote(const std::string& checkpointId, const std::string& note);

    // ===== Automatic Checkpointing =====
    /**
     * @brief Enable automatic checkpointing
     * @param intervalSteps Save checkpoint every N steps
     * @param saveEveryNEpochs Also save every N epochs
     * @return true if successful
     */
    bool enableAutoCheckpointing(int intervalSteps, int saveEveryNEpochs = 1);

    /**
     * @brief Disable automatic checkpointing
     */
    void disableAutoCheckpointing();

    /**
     * @brief Check if should save checkpoint (for auto mode)
     * @param step Current training step
     * @param epoch Current epoch
     * @return true if should save
     */
    bool shouldCheckpoint(int step, int epoch) const;

    // ===== Validation & Integrity =====
    /**
     * @brief Validate checkpoint integrity
     * @param checkpointId Checkpoint to validate
     * @return true if valid and uncorrupted
     */
    bool validateCheckpoint(const std::string& checkpointId) const;

    /**
     * @brief Validate all checkpoints
     * @return Map of checkpointId -> isValid
     */
    std::map<std::string, bool> validateAllCheckpoints() const;

    /**
     * @brief Repair/fix checkpoint if possible
     * @param checkpointId Checkpoint to repair
     * @return true if repair successful
     */
    bool repairCheckpoint(const std::string& checkpointId);

    // ===== Statistics & Analysis =====
    /**
     * @brief Get checkpoint storage usage (bytes)
     * @return Total size of all checkpoints
     */
    uint64_t getTotalCheckpointSize() const;

    /**
     * @brief Get checkpoint file size
     * @param checkpointId Checkpoint to query
     * @return Size in bytes or 0 if not found
     */
    uint64_t getCheckpointSize(const std::string& checkpointId) const;

    /**
     * @brief Generate checkpoint report
     * @return JSON with checkpoint statistics
     */
    std::string generateCheckpointReport() const;

    /**
     * @brief Compare two checkpoints
     * @param checkpointId1 First checkpoint
     * @param checkpointId2 Second checkpoint
     * @return JSON with comparison (validation loss, accuracy, etc.)
     */
    std::string compareCheckpoints(const std::string& checkpointId1,
                                  const std::string& checkpointId2) const;

    // ===== Distributed Checkpointing =====
    /**
     * @brief Set rank for distributed training
     * @param rank Process rank (0 for master)
     * @param worldSize Total number of processes
     */
    void setDistributedInfo(int rank, int worldSize);

    /**
     * @brief Synchronize checkpoints across distributed processes
     * @return true if successful
     */
    bool synchronizeDistributedCheckpoints();

    // ===== Configuration Export/Import =====
    /**
     * @brief Export checkpoint configuration
     * @return JSON configuration
     */
    std::string exportConfiguration() const;

    /**
     * @brief Import checkpoint configuration
     * @param config Configuration to import
     * @return true if successful
     */
    bool importConfiguration(const std::string& config);

    /**
     * @brief Save configuration to file
     * @param filePath Output file path
     * @return true if successful
     */
    bool saveConfigurationToFile(const std::string& filePath) const;

    /**
     * @brief Load configuration from file
     * @param filePath Input file path
     * @return true if successful
     */
    bool loadConfigurationFromFile(const std::string& filePath);

    // --- Callbacks (replaces Qt signals) ---
    using CheckpointSavedCb = void(*)(void* ctx, const char* checkpointId, int epoch, int step);
    using CheckpointLoadedCb = void(*)(void* ctx, const char* checkpointId);
    using BestCheckpointCb = void(*)(void* ctx, const char* checkpointId, float validationLoss);
    using CheckpointDeletedCb = void(*)(void* ctx, const char* checkpointId);
    using CheckpointErrorCb = void(*)(void* ctx, const char* errorMessage);
    using CheckpointStatsCb = void(*)(void* ctx, uint64_t totalSize, int count);

    void setCheckpointSavedCb(CheckpointSavedCb cb, void* ctx) { m_savedCb = cb; m_savedCtx = ctx; }
    void setCheckpointLoadedCb(CheckpointLoadedCb cb, void* ctx) { m_loadedCb = cb; m_loadedCtx = ctx; }
    void setBestCheckpointCb(BestCheckpointCb cb, void* ctx) { m_bestCb = cb; m_bestCtx = ctx; }
    void setCheckpointDeletedCb(CheckpointDeletedCb cb, void* ctx) { m_deletedCb = cb; m_deletedCtx = ctx; }
    void setCheckpointErrorCb(CheckpointErrorCb cb, void* ctx) { m_errorCb = cb; m_errorCtx = ctx; }
    void setCheckpointStatsCb(CheckpointStatsCb cb, void* ctx) { m_statsCb = cb; m_statsCtx = ctx; }

private:
    // Internal state
    std::string m_checkpointDir;
    int m_maxCheckpoints;
    int m_checkpointCounter;
    
    // Auto checkpointing
    bool m_autoCheckpointEnabled;
    int m_autoCheckpointInterval;
    int m_autoCheckpointEpochInterval;
    int m_lastAutoCheckpointStep;
    int m_lastAutoCheckpointEpoch;
    
    // Distributed info
    int m_rank;
    int m_worldSize;
    
    // Checkpoint tracking
    std::vector<CheckpointIndex> m_checkpointIndex;
    std::string m_bestCheckpointId;

    // Helper methods
    std::string generateCheckpointId();
    std::vector<uint8_t> compressState(const std::vector<uint8_t>& data, CompressionLevel level);
    std::vector<uint8_t> decompressState(const std::vector<uint8_t>& data);
    bool writeCheckpointToDisk(const std::string& checkpointId,
                              const CheckpointState& state,
                              CompressionLevel compress);
    bool readCheckpointFromDisk(const std::string& checkpointId,
                               CheckpointState& state);

    // Callback members
    CheckpointSavedCb m_savedCb = nullptr;
    void* m_savedCtx = nullptr;
    CheckpointLoadedCb m_loadedCb = nullptr;
    void* m_loadedCtx = nullptr;
    BestCheckpointCb m_bestCb = nullptr;
    void* m_bestCtx = nullptr;
    CheckpointDeletedCb m_deletedCb = nullptr;
    void* m_deletedCtx = nullptr;
    CheckpointErrorCb m_errorCb = nullptr;
    void* m_errorCtx = nullptr;
    CheckpointStatsCb m_statsCb = nullptr;
    void* m_statsCtx = nullptr;
};
