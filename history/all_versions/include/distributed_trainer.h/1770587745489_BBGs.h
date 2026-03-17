#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QMap>
#include <QThread>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>
#include <string>

// Inference error codes used by distributed training error reporting
enum class InferenceErrorCode {
    SUCCESS = 0,
    MODEL_LOAD_FAILED = 4001,
    INVALID_MODEL_PATH = 4002,
    UNSUPPORTED_MODEL_FORMAT = 4003,
    TOKENIZER_NOT_INITIALIZED = 4101,
    TOKENIZATION_FAILED = 4102,
    EMPTY_REQUEST = 4201,
    PROMPT_TOO_LONG = 4202,
    INVALID_GENERATION_PARAMETERS = 4203,
    REQUEST_TIMEOUT = 4204,
    INSUFFICIENT_MEMORY = 4301,
    REQUEST_QUEUE_FULL = 4302,
    TRANSFORMER_ERROR = 4401,
    INFERENCE_FAILURE = 4402,
    OUTPUT_GENERATION_FAILURE = 4403,
};

/**
 * @class DistributedTrainer
 * @brief Multi-GPU and multi-node distributed training support
 *
 * Features:
 * - Data parallelism (replicate model across GPUs/nodes)
 * - Model parallelism (split model across GPUs/nodes)
 * - Pipeline parallelism (split model layers into stages)
 * - Gradient accumulation for larger effective batch sizes
 * - Gradient compression and reduction optimization
 * - NCCL backend for multi-GPU (fastest)
 * - Gloo backend for multi-node CPU/GPU
 * - MPI support for HPC clusters
 * - Automatic load balancing
 * - Fault tolerance and checkpointing
 * - Performance monitoring per node/GPU
 *
 * Thread-safe for concurrent training across nodes.
 */
class DistributedTrainer : public QObject
{
    Q_OBJECT

public:
    enum class Backend {
        NCCL,           // Multi-GPU on single node (fastest)
        Gloo,           // Multi-node CPU/GPU
        MPI,            // HPC clusters
        Custom          // User-defined backend
    };

    enum class ParallelismType {
        DataParallel,       // Replicate model, distribute data
        ModelParallel,      // Split model across devices
        PipelineParallel,   // Split layers into stages
        Hybrid              // Combination of above
    };

    enum class GradientCompression {
        None,               // No compression
        TopK,               // Keep top-K gradients
        Threshold,          // Threshold-based sparsification
        Quantization,       // Quantize to lower precision
        DeltaCompression    // Only send gradient deltas
    };

    struct ProcessGroupConfig {
        int worldSize;              // Total number of processes
        int rank;                   // Current process rank (0 to worldSize-1)
        int localRank;              // Rank within node (GPU device ID for NCCL)
        QString masterAddr;         // Master node address (for multi-node)
        int masterPort;             // Master node port
        int timeout;                // Communication timeout (seconds)
        bool enableProfiling;       // Profile communication overhead
    };

    struct TrainerConfig {
        Backend backend;
        ParallelismType parallelism;
        GradientCompression compression;
        ProcessGroupConfig pgConfig;
        int gradAccumulationSteps;  // Gradient accumulation
        int syncInterval;           // Sync interval (steps)
        bool enableLoadBalancing;
        bool enableFaultTolerance;
        bool enableAutoMixedPrecision;
        float compressionRatio;     // For TopK/Threshold compression
    };

    struct DeviceInfo {
        int deviceId;
        QString deviceType;         // "cuda", "cpu"
        QString name;               // e.g., "Tesla V100"
        uint64_t totalMemory;       // In bytes
        uint64_t availableMemory;
        float computeCapability;
        float currentLoad;          // 0.0 to 1.0
        float temperature;          // Celsius
    };

    struct NodePerformance {
        int nodeId;
        int rank;
        QString hostname;
        float throughput;           // Samples/sec
        float avgLatency;           // ms
        float communicationOverhead; // % of time spent in allreduce
        int localBatchSize;
        uint64_t dataProcessed;     // Total bytes
        int errorsRecovered;        // Fault recovery count
    };

    // Constructor & initialization
    DistributedTrainer(QObject* parent = nullptr);
    ~DistributedTrainer();

    /**
     * @brief Initialize distributed training (uppercase API)
     * @param config Trainer configuration
     * @return true if successful
     */
    bool Initialize(const TrainerConfig& config);

    /**
     * @brief Shut down distributed training and release resources
     */
    void Shutdown();

    /**
     * @brief Initialize distributed training (lowercase alias)
     * @param config Trainer configuration
     * @return true if successful
     */
    bool initialize(const TrainerConfig& config) { return Initialize(config); }

    /**
     * @brief Check if initialized and ready for training
     * @return true if ready
     */
    bool isInitialized() const;

    // ===== Configuration =====
    /**
     * @brief Get current configuration
     * @return Current trainer config
     */
    TrainerConfig getConfiguration() const;

    /**
     * @brief Update configuration (for dynamic tuning)
     * @param config New configuration
     * @return true if update successful
     */
    bool updateConfiguration(const TrainerConfig& config);

    /**
     * @brief Get this process's rank and world size
     * @return (rank, worldSize) pair
     */
    std::pair<int, int> getRankInfo() const;

    /**
     * @brief Get this process's local rank (GPU device ID for NCCL)
     * @return Local rank (0 to num_local_processes-1)
     */
    int getLocalRank() const;

    // ===== Device Management =====
    /**
     * @brief Get available devices for this process
     * @return List of device info
     */
    std::vector<DeviceInfo> getAvailableDevices() const;

    /**
     * @brief Set primary device for this process
     * @param deviceId Device ID (usually GPU index)
     * @return true if successful
     */
    bool setPrimaryDevice(int deviceId);

    /**
     * @brief Get memory usage on primary device
     * @return (usedMB, totalMB) pair
     */
    std::pair<uint64_t, uint64_t> getMemoryUsage() const;

    /**
     * @brief Get device temperature (if available)
     * @return Temperature in Celsius, or -1 if unavailable
     */
    float getDeviceTemperature() const;

    // ===== Training Operations =====
    /**
     * @brief Initialize distributed process group
     * @return true if successful
     */
    bool initProcessGroup();

    /**
     * @brief Clean up distributed process group
     */
    void destroyProcessGroup();

    /**
     * @brief Synchronize all processes (blocking barrier)
     * @return true if successful
     */
    bool synchronizeProcesses();

    /**
     * @brief All-reduce operation on gradients (average across nodes)
     * @param gradientData Gradient tensor data
     * @param size Number of elements
     * @return true if successful
     */
    bool allReduceGradients(float* gradientData, int size);

    /**
     * @brief All-gather operation (collect data from all processes)
     * @param sendBuffer Data to send
     * @param recvBuffer Buffer to receive all data
     * @param size Size per process
     * @return true if successful
     */
    bool allGather(const void* sendBuffer, void* recvBuffer, int size);

    /**
     * @brief Broadcast data from rank 0 to all other ranks
     * @param data Data buffer
     * @param size Size in bytes
     * @return true if successful
     */
    bool broadcast(void* data, int size);

    /**
     * @brief Point-to-point send (async)
     * @param destRank Destination process rank
     * @param data Data to send
     * @param size Size in bytes
     * @return Handle for later wait() call, or -1 if failed
     */
    int sendAsync(int destRank, const void* data, int size);

    /**
     * @brief Point-to-point receive (async)
     * @param srcRank Source process rank
     * @param data Buffer for receiving
     * @param size Size in bytes
     * @return Handle for later wait() call, or -1 if failed
     */
    int recvAsync(int srcRank, void* data, int size);

    /**
     * @brief Wait for async operation to complete
     * @param handle Handle from send/recv
     * @return true if completed successfully
     */
    bool waitAsync(int handle);

    // ===== Gradient Management =====
    /**
     * @brief Start gradient accumulation phase
     * @param numSteps Number of steps to accumulate
     * @return true if successful
     */
    bool startGradientAccumulation(int numSteps);

    /**
     * @brief Mark step in gradient accumulation
     * @param stepIndex Current step
     * @return true if successful
     */
    bool recordGradientStep(int stepIndex);

    /**
     * @brief Finalize gradient accumulation and synchronize
     * @return Average gradient across all accumulated steps
     */
    bool finalizeGradientAccumulation();

    /**
     * @brief Compress gradients for communication
     * @param gradients Input gradients
     * @param numElements Number of elements
     * @return Compressed gradients
     */
    QByteArray compressGradients(const float* gradients, int numElements);

    /**
     * @brief Decompress gradients after communication
     * @param compressedGradients Compressed data
     * @param numElements Expected number of elements
     * @return Decompressed gradients
     */
    std::vector<float> decompressGradients(const QByteArray& compressedGradients, int numElements);

    // ===== Load Balancing =====
    /**
     * @brief Get recommended batch size for this process
     * @param globalBatchSize Total batch size across all processes
     * @return Local batch size
     */
    int getRecommendedBatchSize(int globalBatchSize) const;

    /**
     * @brief Update load information for load balancing
     * @param currentLoad Current load (0.0 to 1.0)
     * @param throughput Throughput (samples/sec)
     */
    void updateLoadInfo(float currentLoad, float throughput);

    /**
     * @brief Get load balancing suggestions
     * @return Suggested batch size adjustment per node
     */
    std::map<int, int> getLoadBalancingSuggestions() const;

    // ===== Performance Monitoring =====
    /**
     * @brief Record communication latency
     * @param latencyMs Latency in milliseconds
     */
    void recordCommunicationLatency(float latencyMs);

    /**
     * @brief Get average communication latency
     * @return Latency in milliseconds
     */
    float getAvgCommunicationLatency() const;

    /**
     * @brief Get communication time percentage
     * @return Percentage (0.0 to 100.0) of time spent in communication
     */
    float getCommunicationOverheadPercent() const;

    /**
     * @brief Record processing throughput
     * @param samplesPerSecond Throughput in samples/sec
     */
    void recordThroughput(float samplesPerSecond);

    /**
     * @brief Get performance report for all nodes
     * @return List of node performance metrics
     */
    std::vector<NodePerformance> getPerformanceReport() const;

    /**
     * @brief Export performance metrics to JSON
     * @return Performance data as JSON object
     */
    QJsonObject exportPerformanceMetrics() const;

    // ===== Fault Tolerance =====
    /**
     * @brief Enable checkpointing
     * @param checkpointDir Directory for checkpoints
     * @param intervalSteps Checkpoint every N steps
     * @return true if successful
     */
    bool enableCheckpointing(const QString& checkpointDir, int intervalSteps);

    /**
     * @brief Save checkpoint
     * @param stepNumber Current training step
     * @param modelState Model state data
     * @return true if successful
     */
    bool saveCheckpoint(int stepNumber, const QJsonObject& modelState);

    /**
     * @brief Load checkpoint
     * @param checkpointPath Path to checkpoint file
     * @return Model state or empty object if failed
     */
    QJsonObject loadCheckpoint(const QString& checkpointPath);

    /**
     * @brief Handle process failure and recovery
     * @param failedRank Rank of failed process
     * @return true if recovery successful
     */
    bool handleProcessFailure(int failedRank);

    // ===== Configuration Export/Import =====
    /**
     * @brief Export current configuration to JSON
     * @return Configuration as JSON object
     */
    QJsonObject exportConfiguration() const;

    /**
     * @brief Load configuration from JSON
     * @param config Configuration object
     * @return true if successful
     */
    bool loadConfiguration(const QJsonObject& config);

    // ===== Training Operations =====
    /**
     * @brief Execute a single training step
     * @param batchData Batch data as JSON
     * @param lossOut Optional pointer to receive computed loss
     * @return true if successful
     */
    bool TrainStep(const QJsonObject& batchData, float* lossOut = nullptr);

    /**
     * @brief Save a training checkpoint
     * @param path Directory path for checkpoint
     * @return true if successful
     */
    bool Checkpoint(const QString& path);

    /**
     * @brief Restore training state from a checkpoint
     * @param path Path to checkpoint directory
     * @return true if successful
     */
    bool RestoreFromCheckpoint(const QString& path);

    /**
     * @brief Get current training metrics as JSON
     * @return Metrics object
     */
    QJsonObject GetMetrics() const;

    /**
     * @brief Get all detected devices
     * @return Vector of DeviceInfo
     */
    std::vector<DeviceInfo> GetDevices() const;

    /**
     * @brief Get performance data for all nodes
     * @return Vector of NodePerformance
     */
    std::vector<NodePerformance> GetNodePerformance() const;

signals:
    /// Emitted when synchronization between processes completes
    void synchronizationCompleted(int worldSize);

    /// Emitted when all-reduce operation completes
    void allReduceCompleted(int numElements);

    /// Emitted when gradient compression completes
    void gradientCompressionCompleted(int originalSize, int compressedSize, float ratio);

    /// Emitted on communication error
    void communicationError(const QString& details);

    /// Emitted when process failure detected
    void processFailure(int rank, const QString& reason);

    /// Emitted when recovery completes
    void recoveryCompleted(int rank);

    /// Emitted periodically with performance metrics
    void performanceUpdated(const QJsonObject& metrics);

    /// Emitted when checkpointing completes
    void checkpointCompleted(int stepNumber, const QString& path);

    /// Emitted when status changes (initialization, shutdown, etc.)
    void statusChanged(const QString& status);

    /// Emitted after each training step
    void trainingStepCompleted(uint64_t globalStep, float loss);

    /// Emitted after gradient synchronization
    void gradientsSynchronized(float syncTimeMs);

    /// Emitted when a checkpoint is saved
    void checkpointSaved(const QString& path);

    /// Emitted when a checkpoint is restored
    void checkpointRestored(const QString& path);

    /// Emitted when a failed node recovers
    void nodeRecovered(int rank);

    /// Emitted when per-node metrics are updated
    void metricsUpdated(const NodePerformance& metrics);

    /// Emitted on error
    void errorOccurred(const QString& message);

private:
    // ===== Private Helper Methods =====

    /// Validate configuration before initialization
    bool validateConfig() const;

    /// Initialize selected communication backend
    bool initializeBackend();
    void cleanupBackend();

    /// Backend-specific initialization
    bool initializeNCCL();
    bool initializeGloo();
    bool initializeMPI();

    /// Device detection
    bool detectDevices();
    void detectCUDADevices();

    /// Process group management
    bool setupProcessGroup();
    void cleanupProcessGroup();

    /// Load balancing internals
    void initializeLoadBalancer();
    void balanceLoad();
    void updateDeviceLoads();
    void redistributeWork();

    /// Fault tolerance internals
    void initializeFaultTolerance();
    void checkNodeHealth();
    bool isNodeHealthy(const NodePerformance& metrics) const;
    void handleNodeFailure(int rank);

    /// Training step internals
    bool forwardPass(const QJsonObject& batchData);
    bool backwardPass();
    bool optimizerStep();
    bool synchronizeGradients();
    bool allReduceGradients();
    void compressGradients();
    void decompressGradients();

    /// Metrics
    void updateMetrics(float stepTimeMs);

    /// Error logging
    void logError(const QString& message, InferenceErrorCode code);

    // ===== Member Data =====

    TrainerConfig m_config;
    bool m_initialized = false;
    int m_primaryDevice = 0;

    // Device management
    std::vector<DeviceInfo> m_devices;
    QMap<int, float> m_deviceWorkloads;

    // Node metrics (per-rank performance)
    QMap<int, NodePerformance> m_nodeMetrics;

    // Performance tracking
    std::vector<float> m_communicationLatencies;
    std::vector<float> m_throughputs;
    std::map<int, NodePerformance> m_nodePerformance;

    // Training state
    uint64_t m_globalStep = 0;
    float m_currentLoss = 0.0f;
    std::vector<float> m_recentStepTimes;
    float m_averageStepTimeMs = 0.0f;
    float m_lastSyncTimeMs = 0.0f;

    // Gradient accumulation state
    int m_accumStepIndex = 0;
    int m_accumStepTarget = 1;
    int m_currentGradAccumStep = 0;
    std::vector<float> m_accumulatedGradients;

    // Checkpointing
    QString m_checkpointDir;
    int m_checkpointInterval = 0;
    QString m_lastCheckpointPath;

    // Load balancing
    std::map<int, float> m_nodeLoads;
    std::map<int, float> m_nodeThroughputs;

    // Backend-specific state
    bool m_ncclSimulated = false;
    QString m_glooTransport;
    std::string m_ncclUniqueId;

    // Fault tolerance
    bool m_faultDetectionEnabled = false;
    std::chrono::steady_clock::time_point m_lastHealthCheck;

    // Compression utilities
    QByteArray compressTopK(const float* gradients, int numElements, float compressionRatio);
    QByteArray compressThreshold(const float* gradients, int numElements, float threshold);
    std::vector<float> decompressTopK(const QByteArray& data, int numElements);
    std::vector<float> decompressThreshold(const QByteArray& data, int numElements);
};
