#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <vector>
#include <map>
#include <cstdint>

/**
 * @class CheckpointManager
 * @brief Save, manage, and restore training checkpoints
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
class CheckpointManager : public QObject
{
    Q_OBJECT

public:
    enum class CompressionLevel {
        None,
        Low,            // Quick compression
        Medium,
        High,           // Slow but small
        Maximum         // Very slow
    };

    struct CheckpointMetadata {
        QString checkpointId;
        int epoch;
        int step;
        qint64 timestamp;
        float validationLoss;
        float trainLoss;
        float accuracy;
        float wallclockTime;        // Training time so far (seconds)
        int modelSize;              // In bytes
        QString modelArchitecture;
        QString hyperparameters;    // JSON
        QString datasetVersion;
        bool isBestModel;
        QString notes;
    };

    struct CheckpointState {
        QByteArray modelWeights;
        QByteArray optimizerState;
        QByteArray schedulerState;
        QByteArray trainingState;   // Epoch, step, loss history
        QJsonObject config;
    };

    struct CheckpointIndex {
        QString checkpointId;
        QString filePath;
        CheckpointMetadata metadata;
        int checkpointNumber;       // Sequential number
    };

    // Constructor
    explicit CheckpointManager(QObject* parent = nullptr);
    ~CheckpointManager() override;

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
    bool initialize(const QString& checkpointDir, int maxCheckpoints = 10);

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
    QString saveCheckpoint(const CheckpointMetadata& metadata,
                          const CheckpointState& state,
                          CompressionLevel compress = CompressionLevel::Medium);

    /**
     * @brief Quick save (lightweight checkpoint)
     * @param metadata Checkpoint metadata
     * @param state State to save
     * @return Checkpoint ID or empty if failed
     */
    QString quickSaveCheckpoint(const CheckpointMetadata& metadata,
                               const CheckpointState& state);

    /**
     * @brief Save only model weights (smaller checkpoint)
     * @param metadata Checkpoint metadata
     * @param modelWeights Model weights data
     * @param compress Compression level
     * @return Checkpoint ID or empty if failed
     */
    QString saveModelWeights(const CheckpointMetadata& metadata,
                            const QByteArray& modelWeights,
                            CompressionLevel compress = CompressionLevel::Medium);

    // ===== Checkpoint Retrieval =====
    /**
     * @brief Load checkpoint
     * @param checkpointId Checkpoint to load
     * @param state Output: loaded state
     * @return true if successful
     */
    bool loadCheckpoint(const QString& checkpointId, CheckpointState& state);

    /**
     * @brief Load latest checkpoint
     * @param state Output: loaded state
     * @return Checkpoint ID or empty if no checkpoints
     */
    QString loadLatestCheckpoint(CheckpointState& state);

    /**
     * @brief Load best checkpoint (by validation metric)
     * @param state Output: loaded state
     * @return Checkpoint ID or empty if not found
     */
    QString loadBestCheckpoint(CheckpointState& state);

    /**
     * @brief Load checkpoint from epoch
     * @param epoch Target epoch
     * @param state Output: loaded state
     * @return Checkpoint ID or empty if not found
     */
    QString loadCheckpointFromEpoch(int epoch, CheckpointState& state);

    /**
     * @brief Get checkpoint metadata
     * @param checkpointId Checkpoint to query
     * @return Metadata or empty if not found
     */
    CheckpointMetadata getCheckpointMetadata(const QString& checkpointId) const;

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
    bool deleteCheckpoint(const QString& checkpointId);

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
    bool updateCheckpointMetadata(const QString& checkpointId,
                                 const CheckpointMetadata& metadata);

    /**
     * @brief Set note for checkpoint
     * @param checkpointId Checkpoint to annotate
     * @param note Note text
     * @return true if successful
     */
    bool setCheckpointNote(const QString& checkpointId, const QString& note);

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
    bool validateCheckpoint(const QString& checkpointId) const;

    /**
     * @brief Validate all checkpoints
     * @return Map of checkpointId -> isValid
     */
    std::map<QString, bool> validateAllCheckpoints() const;

    /**
     * @brief Repair/fix checkpoint if possible
     * @param checkpointId Checkpoint to repair
     * @return true if repair successful
     */
    bool repairCheckpoint(const QString& checkpointId);

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
    uint64_t getCheckpointSize(const QString& checkpointId) const;

    /**
     * @brief Generate checkpoint report
     * @return JSON with checkpoint statistics
     */
    QJsonObject generateCheckpointReport() const;

    /**
     * @brief Compare two checkpoints
     * @param checkpointId1 First checkpoint
     * @param checkpointId2 Second checkpoint
     * @return JSON with comparison (validation loss, accuracy, etc.)
     */
    QJsonObject compareCheckpoints(const QString& checkpointId1,
                                  const QString& checkpointId2) const;

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
    QJsonObject exportConfiguration() const;

    /**
     * @brief Import checkpoint configuration
     * @param config Configuration to import
     * @return true if successful
     */
    bool importConfiguration(const QJsonObject& config);

    /**
     * @brief Save configuration to file
     * @param filePath Output file path
     * @return true if successful
     */
    bool saveConfigurationToFile(const QString& filePath) const;

    /**
     * @brief Load configuration from file
     * @param filePath Input file path
     * @return true if successful
     */
    bool loadConfigurationFromFile(const QString& filePath);

signals:
    /// Emitted when checkpoint is saved
    void checkpointSaved(const QString& checkpointId, int epoch, int step);

    /// Emitted when checkpoint is loaded
    void checkpointLoaded(const QString& checkpointId);

    /// Emitted when new best checkpoint found
    void bestCheckpointUpdated(const QString& checkpointId, float validationLoss);

    /// Emitted when checkpoint deletion occurs
    void checkpointDeleted(const QString& checkpointId);

    /// Emitted on checkpoint error
    void checkpointError(const QString& errorMessage);

    /// Emitted periodically with checkpoint statistics
    void checkpointStatsUpdated(uint64_t totalSize, int checkpointCount);

private:
    // Internal state
    QString m_checkpointDir;
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
    QString m_bestCheckpointId;

    // Helper methods
    QString generateCheckpointId();
    QByteArray compressState(const QByteArray& data, CompressionLevel level);
    QByteArray decompressState(const QByteArray& data);
    bool writeCheckpointToDisk(const QString& checkpointId,
                              const CheckpointState& state,
                              CompressionLevel compress);
    bool readCheckpointFromDisk(const QString& checkpointId,
                               CheckpointState& state);
};
