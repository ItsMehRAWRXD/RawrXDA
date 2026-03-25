<<<<<<< HEAD
#pragma once

// ============================================================================
// DistributedTrainer — C++20, no Qt. Multi-GPU / multi-node training support.
// ============================================================================
// JSON payloads use std::string (serialized JSON). Replacements: QString→std::string,
// QMap→std::map, QByteArray→std::vector<uint8_t>, QJsonObject→std::string.
// ============================================================================

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
 * @brief Multi-GPU and multi-node distributed training support (Qt-free)
 *
 * Features: data/model/pipeline parallelism, gradient accumulation/compression,
 * NCCL/Gloo/MPI backends, load balancing, fault tolerance, checkpointing.
 * Thread-safe for concurrent training across nodes.
 */
class DistributedTrainer {
public:
    enum class Backend {
        NCCL,
        Gloo,
        MPI,
        Custom
    };

    enum class ParallelismType {
        DataParallel,
        ModelParallel,
        PipelineParallel,
        Hybrid
    };

    enum class GradientCompression {
        None,
        TopK,
        Threshold,
        Quantization,
        DeltaCompression
    };

    struct ProcessGroupConfig {
        int worldSize = 0;
        int rank = 0;
        int localRank = 0;
        std::string masterAddr;
        int masterPort = 0;
        int timeout = 0;
        bool enableProfiling = false;
    };

    struct TrainerConfig {
        Backend backend = Backend::Gloo;
        ParallelismType parallelism = ParallelismType::DataParallel;
        GradientCompression compression = GradientCompression::None;
        ProcessGroupConfig pgConfig;
        int gradAccumulationSteps = 1;
        int syncInterval = 1;
        bool enableLoadBalancing = false;
        bool enableFaultTolerance = false;
        bool enableAutoMixedPrecision = false;
        float compressionRatio = 0.5f;
    };

    struct DeviceInfo {
        int deviceId = 0;
        std::string deviceType;
        std::string name;
        uint64_t totalMemory = 0;
        uint64_t availableMemory = 0;
        float computeCapability = 0.0f;
        float currentLoad = 0.0f;
        float temperature = 0.0f;
    };

    struct NodePerformance {
        int nodeId = 0;
        int rank = 0;
        std::string hostname;
        float throughput = 0.0f;
        float avgLatency = 0.0f;
        float communicationOverhead = 0.0f;
        int localBatchSize = 0;
        uint64_t dataProcessed = 0;
        int errorsRecovered = 0;
    };

    DistributedTrainer() = default;
    ~DistributedTrainer() = default;

    bool Initialize(const TrainerConfig& config);
    void Shutdown();
    bool initialize(const TrainerConfig& config) { return Initialize(config); }
    bool isInitialized() const;

    TrainerConfig getConfiguration() const;
    bool updateConfiguration(const TrainerConfig& config);
    std::pair<int, int> getRankInfo() const;
    int getLocalRank() const;

    std::vector<DeviceInfo> getAvailableDevices() const;
    bool setPrimaryDevice(int deviceId);
    std::pair<uint64_t, uint64_t> getMemoryUsage() const;
    float getDeviceTemperature() const;

    bool initProcessGroup();
    void destroyProcessGroup();
    bool synchronizeProcesses();
    bool allReduceGradients(float* gradientData, int size);
    bool allGather(const void* sendBuffer, void* recvBuffer, int size);
    bool broadcast(void* data, int size);
    int sendAsync(int destRank, const void* data, int size);
    int recvAsync(int srcRank, void* data, int size);
    bool waitAsync(int handle);

    bool startGradientAccumulation(int numSteps);
    bool recordGradientStep(int stepIndex);
    bool finalizeGradientAccumulation();
    /** Returns compressed gradient bytes (replaces QByteArray) */
    std::vector<uint8_t> compressGradients(const float* gradients, int numElements);
    std::vector<float> decompressGradients(const std::vector<uint8_t>& compressedGradients, int numElements);

    int getRecommendedBatchSize(int globalBatchSize) const;
    void updateLoadInfo(float currentLoad, float throughput);
    std::map<int, int> getLoadBalancingSuggestions() const;

    void recordCommunicationLatency(float latencyMs);
    float getAvgCommunicationLatency() const;
    float getCommunicationOverheadPercent() const;
    void recordThroughput(float samplesPerSecond);
    std::vector<NodePerformance> getPerformanceReport() const;
    /** Returns JSON-serialized metrics string (replaces QJsonObject) */
    std::string exportPerformanceMetrics() const;

    bool enableCheckpointing(const std::string& checkpointDir, int intervalSteps);
    /** modelState: JSON-serialized string (replaces QJsonObject) */
    bool saveCheckpoint(int stepNumber, const std::string& modelState);
    /** Returns JSON-serialized model state or empty string (replaces QJsonObject) */
    std::string loadCheckpoint(const std::string& checkpointPath);
    bool handleProcessFailure(int failedRank);

    std::string exportConfiguration() const;
    bool loadConfiguration(const std::string& config);

    /** batchData: JSON-serialized string (replaces QJsonObject) */
    bool TrainStep(const std::string& batchData, float* lossOut = nullptr);
    bool Checkpoint(const std::string& path);
    bool RestoreFromCheckpoint(const std::string& path);
    std::string GetMetrics() const;
    std::vector<DeviceInfo> GetDevices() const;
    std::vector<NodePerformance> GetNodePerformance() const;

private:
    bool validateConfig() const;
    bool initializeBackend();
    void cleanupBackend();
    bool initializeNCCL();
    bool initializeGloo();
    bool initializeMPI();
    bool detectDevices();
    void detectCUDADevices();
    bool setupProcessGroup();
    void cleanupProcessGroup();
    void initializeLoadBalancer();
    void balanceLoad();
    void updateDeviceLoads();
    void redistributeWork();
    void initializeFaultTolerance();
    void checkNodeHealth();
    bool isNodeHealthy(const NodePerformance& metrics) const;
    void handleNodeFailure(int rank);
    bool forwardPass(const std::string& batchData);
    bool backwardPass();
    bool optimizerStep();
    bool synchronizeGradients();
    bool allReduceGradients();
    void compressGradients();
    void decompressGradients();
    void updateMetrics(float stepTimeMs);
    void logError(const std::string& message, InferenceErrorCode code);

    std::vector<uint8_t> compressTopK(const float* gradients, int numElements, float compressionRatio);
    std::vector<uint8_t> compressThreshold(const float* gradients, int numElements, float threshold);
    std::vector<float> decompressTopK(const std::vector<uint8_t>& data, int numElements);
    std::vector<float> decompressThreshold(const std::vector<uint8_t>& data, int numElements);

    TrainerConfig m_config;
    bool m_initialized = false;
    int m_primaryDevice = 0;
    std::vector<DeviceInfo> m_devices;
    std::map<int, float> m_deviceWorkloads;
    std::map<int, NodePerformance> m_nodeMetrics;
    std::vector<float> m_communicationLatencies;
    std::vector<float> m_throughputs;
    std::map<int, NodePerformance> m_nodePerformance;
    uint64_t m_globalStep = 0;
    float m_currentLoss = 0.0f;
    std::vector<float> m_recentStepTimes;
    float m_averageStepTimeMs = 0.0f;
    float m_lastSyncTimeMs = 0.0f;
    int m_accumStepIndex = 0;
    int m_accumStepTarget = 1;
    int m_currentGradAccumStep = 0;
    std::vector<float> m_accumulatedGradients;
    std::string m_checkpointDir;
    int m_checkpointInterval = 0;
    std::string m_lastCheckpointPath;
    std::map<int, float> m_nodeLoads;
    std::map<int, float> m_nodeThroughputs;
    bool m_ncclSimulated = false;
    std::string m_glooTransport;
    std::string m_ncclUniqueId;
    bool m_faultDetectionEnabled = false;
    std::chrono::steady_clock::time_point m_lastHealthCheck;
    std::vector<uint8_t> m_compressedGradientData;
    std::vector<float> m_previousGradients;
    float m_quantScale = 1.0f;
    float m_lastGradientNorm = 0.0f;
};
=======
#pragma once

// ============================================================================
// DistributedTrainer — C++20, no Qt. Multi-GPU / multi-node training support.
// ============================================================================
// JSON payloads use std::string (serialized JSON). Replacements: QString→std::string,
// QMap→std::map, QByteArray→std::vector<uint8_t>, QJsonObject→std::string.
// ============================================================================

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
 * @brief Multi-GPU and multi-node distributed training support (Qt-free)
 *
 * Features: data/model/pipeline parallelism, gradient accumulation/compression,
 * NCCL/Gloo/MPI backends, load balancing, fault tolerance, checkpointing.
 * Thread-safe for concurrent training across nodes.
 */
class DistributedTrainer {
public:
    enum class Backend {
        NCCL,
        Gloo,
        MPI,
        Custom
    };

    enum class ParallelismType {
        DataParallel,
        ModelParallel,
        PipelineParallel,
        Hybrid
    };

    enum class GradientCompression {
        None,
        TopK,
        Threshold,
        Quantization,
        DeltaCompression
    };

    struct ProcessGroupConfig {
        int worldSize = 0;
        int rank = 0;
        int localRank = 0;
        std::string masterAddr;
        int masterPort = 0;
        int timeout = 0;
        bool enableProfiling = false;
    };

    struct TrainerConfig {
        Backend backend = Backend::Gloo;
        ParallelismType parallelism = ParallelismType::DataParallel;
        GradientCompression compression = GradientCompression::None;
        ProcessGroupConfig pgConfig;
        int gradAccumulationSteps = 1;
        int syncInterval = 1;
        bool enableLoadBalancing = false;
        bool enableFaultTolerance = false;
        bool enableAutoMixedPrecision = false;
        float compressionRatio = 0.5f;
    };

    struct DeviceInfo {
        int deviceId = 0;
        std::string deviceType;
        std::string name;
        uint64_t totalMemory = 0;
        uint64_t availableMemory = 0;
        float computeCapability = 0.0f;
        float currentLoad = 0.0f;
        float temperature = 0.0f;
    };

    struct NodePerformance {
        int nodeId = 0;
        int rank = 0;
        std::string hostname;
        float throughput = 0.0f;
        float avgLatency = 0.0f;
        float communicationOverhead = 0.0f;
        int localBatchSize = 0;
        uint64_t dataProcessed = 0;
        int errorsRecovered = 0;
    };

    DistributedTrainer() = default;
    ~DistributedTrainer() = default;

    bool Initialize(const TrainerConfig& config);
    void Shutdown();
    bool initialize(const TrainerConfig& config) { return Initialize(config); }
    bool isInitialized() const;

    TrainerConfig getConfiguration() const;
    bool updateConfiguration(const TrainerConfig& config);
    std::pair<int, int> getRankInfo() const;
    int getLocalRank() const;

    std::vector<DeviceInfo> getAvailableDevices() const;
    bool setPrimaryDevice(int deviceId);
    std::pair<uint64_t, uint64_t> getMemoryUsage() const;
    float getDeviceTemperature() const;

    bool initProcessGroup();
    void destroyProcessGroup();
    bool synchronizeProcesses();
    bool allReduceGradients(float* gradientData, int size);
    bool allGather(const void* sendBuffer, void* recvBuffer, int size);
    bool broadcast(void* data, int size);
    int sendAsync(int destRank, const void* data, int size);
    int recvAsync(int srcRank, void* data, int size);
    bool waitAsync(int handle);

    bool startGradientAccumulation(int numSteps);
    bool recordGradientStep(int stepIndex);
    bool finalizeGradientAccumulation();
    /** Returns compressed gradient bytes (replaces QByteArray) */
    std::vector<uint8_t> compressGradients(const float* gradients, int numElements);
    std::vector<float> decompressGradients(const std::vector<uint8_t>& compressedGradients, int numElements);

    int getRecommendedBatchSize(int globalBatchSize) const;
    void updateLoadInfo(float currentLoad, float throughput);
    std::map<int, int> getLoadBalancingSuggestions() const;

    void recordCommunicationLatency(float latencyMs);
    float getAvgCommunicationLatency() const;
    float getCommunicationOverheadPercent() const;
    void recordThroughput(float samplesPerSecond);
    std::vector<NodePerformance> getPerformanceReport() const;
    /** Returns JSON-serialized metrics string (replaces QJsonObject) */
    std::string exportPerformanceMetrics() const;

    bool enableCheckpointing(const std::string& checkpointDir, int intervalSteps);
    /** modelState: JSON-serialized string (replaces QJsonObject) */
    bool saveCheckpoint(int stepNumber, const std::string& modelState);
    /** Returns JSON-serialized model state or empty string (replaces QJsonObject) */
    std::string loadCheckpoint(const std::string& checkpointPath);
    bool handleProcessFailure(int failedRank);

    std::string exportConfiguration() const;
    bool loadConfiguration(const std::string& config);

    /** batchData: JSON-serialized string (replaces QJsonObject) */
    bool TrainStep(const std::string& batchData, float* lossOut = nullptr);
    bool Checkpoint(const std::string& path);
    bool RestoreFromCheckpoint(const std::string& path);
    std::string GetMetrics() const;
    std::vector<DeviceInfo> GetDevices() const;
    std::vector<NodePerformance> GetNodePerformance() const;

private:
    bool validateConfig() const;
    bool initializeBackend();
    void cleanupBackend();
    bool initializeNCCL();
    bool initializeGloo();
    bool initializeMPI();
    bool detectDevices();
    void detectCUDADevices();
    bool setupProcessGroup();
    void cleanupProcessGroup();
    void initializeLoadBalancer();
    void balanceLoad();
    void updateDeviceLoads();
    void redistributeWork();
    void initializeFaultTolerance();
    void checkNodeHealth();
    bool isNodeHealthy(const NodePerformance& metrics) const;
    void handleNodeFailure(int rank);
    bool forwardPass(const std::string& batchData);
    bool backwardPass();
    bool optimizerStep();
    bool synchronizeGradients();
    bool allReduceGradients();
    void compressGradients();
    void decompressGradients();
    void updateMetrics(float stepTimeMs);
    void logError(const std::string& message, InferenceErrorCode code);

    std::vector<uint8_t> compressTopK(const float* gradients, int numElements, float compressionRatio);
    std::vector<uint8_t> compressThreshold(const float* gradients, int numElements, float threshold);
    std::vector<float> decompressTopK(const std::vector<uint8_t>& data, int numElements);
    std::vector<float> decompressThreshold(const std::vector<uint8_t>& data, int numElements);

    TrainerConfig m_config;
    bool m_initialized = false;
    int m_primaryDevice = 0;
    std::vector<DeviceInfo> m_devices;
    std::map<int, float> m_deviceWorkloads;
    std::map<int, NodePerformance> m_nodeMetrics;
    std::vector<float> m_communicationLatencies;
    std::vector<float> m_throughputs;
    std::map<int, NodePerformance> m_nodePerformance;
    uint64_t m_globalStep = 0;
    float m_currentLoss = 0.0f;
    std::vector<float> m_recentStepTimes;
    float m_averageStepTimeMs = 0.0f;
    float m_lastSyncTimeMs = 0.0f;
    int m_accumStepIndex = 0;
    int m_accumStepTarget = 1;
    int m_currentGradAccumStep = 0;
    std::vector<float> m_accumulatedGradients;
    std::string m_checkpointDir;
    int m_checkpointInterval = 0;
    std::string m_lastCheckpointPath;
    std::map<int, float> m_nodeLoads;
    std::map<int, float> m_nodeThroughputs;
    bool m_ncclSimulated = false;
    std::string m_glooTransport;
    std::string m_ncclUniqueId;
    bool m_faultDetectionEnabled = false;
    std::chrono::steady_clock::time_point m_lastHealthCheck;
    std::vector<uint8_t> m_compressedGradientData;
    std::vector<float> m_previousGradients;
    float m_quantScale = 1.0f;
    float m_lastGradientNorm = 0.0f;
};
>>>>>>> origin/main
