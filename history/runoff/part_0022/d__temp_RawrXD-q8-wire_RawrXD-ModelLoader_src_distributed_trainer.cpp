#include "distributed_trainer.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <algorithm>
#include <numeric>
#include <cmath>

DistributedTrainer::DistributedTrainer(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_primaryDevice(0)
    , m_accumStepIndex(0)
    , m_accumStepTarget(1)
    , m_checkpointInterval(100)
{
}

DistributedTrainer::~DistributedTrainer()
{
    if (m_initialized) {
        destroyProcessGroup();
    }
}

bool DistributedTrainer::initialize(const TrainerConfig& config)
{
    m_config = config;
    m_initialized = initProcessGroup();
    return m_initialized;
}

bool DistributedTrainer::isInitialized() const
{
    return m_initialized;
}

DistributedTrainer::TrainerConfig DistributedTrainer::getConfiguration() const
{
    return m_config;
}

bool DistributedTrainer::updateConfiguration(const TrainerConfig& config)
{
    m_config = config;
    return true;
}

std::pair<int, int> DistributedTrainer::getRankInfo() const
{
    return {m_config.pgConfig.rank, m_config.pgConfig.worldSize};
}

int DistributedTrainer::getLocalRank() const
{
    return m_config.pgConfig.localRank;
}

std::vector<DistributedTrainer::DeviceInfo> DistributedTrainer::getAvailableDevices() const
{
    std::vector<DeviceInfo> devices;

    // Simulate device detection (in production, query CUDA/HIP/etc)
    // For now, assume up to 8 CUDA devices
    for (int i = 0; i < 4; ++i) {
        DeviceInfo info;
        info.deviceId = i;
        info.deviceType = "cuda";
        info.name = QString("NVIDIA GPU %1").arg(i);
        info.totalMemory = 16ull * 1024 * 1024 * 1024;  // 16 GB
        info.availableMemory = info.totalMemory / 2;
        info.computeCapability = 8.0f;  // A100
        info.currentLoad = 0.3f;
        info.temperature = 45.0f;
        devices.push_back(info);
    }

    return devices;
}

bool DistributedTrainer::setPrimaryDevice(int deviceId)
{
    m_primaryDevice = deviceId;
    qDebug() << "Set primary device to" << deviceId;
    return true;
}

std::pair<uint64_t, uint64_t> DistributedTrainer::getMemoryUsage() const
{
    auto devices = getAvailableDevices();
    if (m_primaryDevice < static_cast<int>(devices.size())) {
        uint64_t used = devices[m_primaryDevice].totalMemory - devices[m_primaryDevice].availableMemory;
        return {used / (1024 * 1024), devices[m_primaryDevice].totalMemory / (1024 * 1024)};
    }
    return {0, 0};
}

float DistributedTrainer::getDeviceTemperature() const
{
    auto devices = getAvailableDevices();
    if (m_primaryDevice < static_cast<int>(devices.size())) {
        return devices[m_primaryDevice].temperature;
    }
    return -1.0f;
}

bool DistributedTrainer::initProcessGroup()
{
    // In production: Initialize NCCL, Gloo, or MPI
    // For now: Simulate successful initialization
    qDebug() << "Initializing process group:"
             << "rank" << m_config.pgConfig.rank
             << "worldSize" << m_config.pgConfig.worldSize
             << "backend" << static_cast<int>(m_config.backend);

    return true;
}

void DistributedTrainer::destroyProcessGroup()
{
    // In production: Clean up NCCL/Gloo/MPI resources
    qDebug() << "Destroying process group";
    m_initialized = false;
}

bool DistributedTrainer::synchronizeProcesses()
{
    // Simulate barrier operation
    emit synchronizationCompleted(m_config.pgConfig.worldSize);
    return true;
}

bool DistributedTrainer::allReduceGradients(float* gradientData, int size)
{
    if (!m_initialized || size <= 0) {
        return false;
    }

    // Simulate all-reduce: average gradients across all processes
    for (int i = 0; i < size; ++i) {
        gradientData[i] /= m_config.pgConfig.worldSize;
    }

    emit allReduceCompleted(size);
    recordCommunicationLatency(1.5f);  // Simulate ~1.5ms latency
    return true;
}

bool DistributedTrainer::allGather(const void* sendBuffer, void* recvBuffer, int size)
{
    if (!m_initialized) {
        return false;
    }

    // Simulate all-gather: collect data from all processes
    std::memcpy(recvBuffer, sendBuffer, size * m_config.pgConfig.worldSize);
    recordCommunicationLatency(2.0f);
    return true;
}

bool DistributedTrainer::broadcast(void* data, int size)
{
    if (!m_initialized) {
        return false;
    }

    // Simulate broadcast: rank 0 sends to all
    recordCommunicationLatency(0.5f);
    return true;
}

int DistributedTrainer::sendAsync(int destRank, const void* data, int size)
{
    if (!m_initialized || destRank < 0 || destRank >= m_config.pgConfig.worldSize) {
        return -1;
    }

    // Return handle for async operation
    static int handleCounter = 0;
    return handleCounter++;
}

int DistributedTrainer::recvAsync(int srcRank, void* data, int size)
{
    if (!m_initialized || srcRank < 0 || srcRank >= m_config.pgConfig.worldSize) {
        return -1;
    }

    static int handleCounter = 1000;
    return handleCounter++;
}

bool DistributedTrainer::waitAsync(int handle)
{
    // Simulate completion
    return handle >= 0;
}

bool DistributedTrainer::startGradientAccumulation(int numSteps)
{
    m_accumStepTarget = numSteps;
    m_accumStepIndex = 0;
    m_accumulatedGradients.clear();
    return true;
}

bool DistributedTrainer::recordGradientStep(int stepIndex)
{
    m_accumStepIndex = stepIndex;
    return stepIndex < m_accumStepTarget;
}

bool DistributedTrainer::finalizeGradientAccumulation()
{
    if (!m_accumulatedGradients.empty()) {
        // Synchronize accumulated gradients across processes
        allReduceGradients(m_accumulatedGradients.data(), m_accumulatedGradients.size());
    }
    return true;
}

QByteArray DistributedTrainer::compressGradients(const float* gradients, int numElements)
{
    QByteArray compressed;

    switch (m_config.compression) {
        case GradientCompression::None:
            compressed = QByteArray(reinterpret_cast<const char*>(gradients), numElements * sizeof(float));
            break;

        case GradientCompression::TopK:
            compressed = compressTopK(gradients, numElements, m_config.compressionRatio);
            break;

        case GradientCompression::Threshold:
            compressed = compressThreshold(gradients, numElements, m_config.compressionRatio);
            break;

        case GradientCompression::Quantization:
            // Simulate quantization: reduce from float32 to float16
            compressed.resize(numElements * 2);
            for (int i = 0; i < numElements; ++i) {
                // Simplified: just store first 16 bits of each float
                uint16_t quantized = static_cast<uint16_t>(std::abs(gradients[i]) * 100) & 0xFFFF;
                std::memcpy(compressed.data() + i * 2, &quantized, 2);
            }
            break;

        case GradientCompression::DeltaCompression:
            // Store only gradients above threshold
            for (int i = 0; i < numElements; ++i) {
                if (std::abs(gradients[i]) > 0.001f) {
                    compressed.append(reinterpret_cast<const char*>(&i), sizeof(int));
                    compressed.append(reinterpret_cast<const char*>(&gradients[i]), sizeof(float));
                }
            }
            break;
    }

    emit gradientCompressionCompleted(numElements * sizeof(float), compressed.size(),
                                     static_cast<float>(compressed.size()) / (numElements * sizeof(float)));
    return compressed;
}

std::vector<float> DistributedTrainer::decompressGradients(const QByteArray& compressedGradients, int numElements)
{
    std::vector<float> gradients(numElements, 0.0f);

    switch (m_config.compression) {
        case GradientCompression::None:
            std::memcpy(gradients.data(), compressedGradients.data(), numElements * sizeof(float));
            break;

        case GradientCompression::TopK:
            gradients = decompressTopK(compressedGradients, numElements);
            break;

        case GradientCompression::Threshold:
            gradients = decompressThreshold(compressedGradients, numElements);
            break;

        case GradientCompression::Quantization:
            // Simplified dequantization
            for (int i = 0; i < numElements && i * 2 < compressedGradients.size(); ++i) {
                uint16_t quantized = *reinterpret_cast<const uint16_t*>(compressedGradients.data() + i * 2);
                gradients[i] = static_cast<float>(quantized) / 100.0f;
            }
            break;

        case GradientCompression::DeltaCompression:
            // Decompress sparse gradients
            for (int offset = 0; offset + 8 <= compressedGradients.size(); offset += 12) {
                int idx = *reinterpret_cast<const int*>(compressedGradients.data() + offset);
                float val = *reinterpret_cast<const float*>(compressedGradients.data() + offset + 4);
                if (idx >= 0 && idx < numElements) {
                    gradients[idx] = val;
                }
            }
            break;
    }

    return gradients;
}

int DistributedTrainer::getRecommendedBatchSize(int globalBatchSize) const
{
    if (m_config.pgConfig.worldSize <= 0) {
        return globalBatchSize;
    }

    int baseBatchSize = globalBatchSize / m_config.pgConfig.worldSize;

    if (!m_config.enableLoadBalancing) {
        return baseBatchSize;
    }

    // Adjust based on node load
    auto it = m_nodeLoads.find(m_config.pgConfig.rank);
    if (it != m_nodeLoads.end()) {
        float loadFactor = 1.0f / (it->second + 0.1f);
        return static_cast<int>(baseBatchSize * loadFactor);
    }

    return baseBatchSize;
}

void DistributedTrainer::updateLoadInfo(float currentLoad, float throughput)
{
    m_nodeLoads[m_config.pgConfig.rank] = currentLoad;
    m_nodeThroughputs[m_config.pgConfig.rank] = throughput;
}

std::map<int, int> DistributedTrainer::getLoadBalancingSuggestions() const
{
    std::map<int, int> suggestions;

    if (m_nodeLoads.empty()) {
        return suggestions;
    }

    float avgLoad = 0.0f;
    for (const auto& [rank, load] : m_nodeLoads) {
        avgLoad += load;
    }
    avgLoad /= m_nodeLoads.size();

    int baseBatchSize = 32;
    for (const auto& [rank, load] : m_nodeLoads) {
        float factor = avgLoad / (load + 0.1f);
        suggestions[rank] = static_cast<int>(baseBatchSize * factor);
    }

    return suggestions;
}

void DistributedTrainer::recordCommunicationLatency(float latencyMs)
{
    m_communicationLatencies.push_back(latencyMs);
    if (m_communicationLatencies.size() > 1000) {
        m_communicationLatencies.erase(m_communicationLatencies.begin());
    }
}

float DistributedTrainer::getAvgCommunicationLatency() const
{
    if (m_communicationLatencies.empty()) {
        return 0.0f;
    }
    return std::accumulate(m_communicationLatencies.begin(), m_communicationLatencies.end(), 0.0f) /
           m_communicationLatencies.size();
}

float DistributedTrainer::getCommunicationOverheadPercent() const
{
    if (m_throughputs.empty()) {
        return 0.0f;
    }
    // Simplified: assume ~2% overhead for single-GPU, scales with world size
    return 2.0f * std::log(m_config.pgConfig.worldSize + 1.0f);
}

void DistributedTrainer::recordThroughput(float samplesPerSecond)
{
    m_throughputs.push_back(samplesPerSecond);
    if (m_throughputs.size() > 1000) {
        m_throughputs.erase(m_throughputs.begin());
    }
}

std::vector<DistributedTrainer::NodePerformance> DistributedTrainer::getPerformanceReport() const
{
    std::vector<NodePerformance> report;

    float avgThroughput = 0.0f;
    if (!m_throughputs.empty()) {
        avgThroughput = std::accumulate(m_throughputs.begin(), m_throughputs.end(), 0.0f) / m_throughputs.size();
    }

    for (const auto& [rank, perf] : m_nodePerformance) {
        report.push_back(perf);
    }

    if (report.empty()) {
        // Generate default report
        NodePerformance perf;
        perf.nodeId = 0;
        perf.rank = m_config.pgConfig.rank;
        perf.hostname = "localhost";
        perf.throughput = avgThroughput;
        perf.avgLatency = getAvgCommunicationLatency();
        perf.communicationOverhead = getCommunicationOverheadPercent();
        perf.localBatchSize = 32;
        perf.dataProcessed = 0;
        perf.errorsRecovered = 0;
        report.push_back(perf);
    }

    return report;
}

QJsonObject DistributedTrainer::exportPerformanceMetrics() const
{
    QJsonObject metrics;
    metrics["rank"] = m_config.pgConfig.rank;
    metrics["worldSize"] = m_config.pgConfig.worldSize;
    metrics["avgCommunicationLatency"] = getAvgCommunicationLatency();
    metrics["communicationOverhead"] = getCommunicationOverheadPercent();

    QJsonArray throughputs;
    for (float t : m_throughputs) {
        throughputs.append(t);
    }
    metrics["throughputs"] = throughputs;

    QJsonArray latencies;
    for (float l : m_communicationLatencies) {
        latencies.append(l);
    }
    metrics["latencies"] = latencies;

    QJsonArray nodePerformances;
    for (const auto& perf : getPerformanceReport()) {
        QJsonObject obj;
        obj["rank"] = perf.rank;
        obj["throughput"] = perf.throughput;
        obj["avgLatency"] = perf.avgLatency;
        obj["communicationOverhead"] = perf.communicationOverhead;
        nodePerformances.append(obj);
    }
    metrics["nodePerformances"] = nodePerformances;

    return metrics;
}

bool DistributedTrainer::enableCheckpointing(const QString& checkpointDir, int intervalSteps)
{
    m_checkpointDir = checkpointDir;
    m_checkpointInterval = intervalSteps;
    qDebug() << "Checkpointing enabled:" << checkpointDir << "interval:" << intervalSteps;
    return true;
}

bool DistributedTrainer::saveCheckpoint(int stepNumber, const QJsonObject& modelState)
{
    if (m_checkpointDir.isEmpty()) {
        return false;
    }

    QJsonObject checkpoint;
    checkpoint["step"] = stepNumber;
    checkpoint["timestamp"] = static_cast<qint64>(std::time(nullptr));
    checkpoint["worldSize"] = m_config.pgConfig.worldSize;
    checkpoint["rank"] = m_config.pgConfig.rank;
    checkpoint["modelState"] = modelState;

    QString filePath = m_checkpointDir + "/" + QString("checkpoint_step_%1_rank_%2.json")
                                             .arg(stepNumber)
                                             .arg(m_config.pgConfig.rank);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(QJsonDocument(checkpoint).toJson(QJsonDocument::Compact));
    file.close();

    emit checkpointCompleted(stepNumber, filePath);
    return true;
}

QJsonObject DistributedTrainer::loadCheckpoint(const QString& checkpointPath)
{
    QFile file(checkpointPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return QJsonObject();
    }

    return doc.object()["modelState"].toObject();
}

bool DistributedTrainer::handleProcessFailure(int failedRank)
{
    qWarning() << "Process failure on rank" << failedRank;
    emit processFailure(failedRank, "Process crashed or disconnected");

    // In production: implement recovery logic (checkpoint reload, etc.)
    emit recoveryCompleted(failedRank);
    return true;
}

QJsonObject DistributedTrainer::exportConfiguration() const
{
    QJsonObject config;
    config["backend"] = static_cast<int>(m_config.backend);
    config["parallelism"] = static_cast<int>(m_config.parallelism);
    config["compression"] = static_cast<int>(m_config.compression);
    config["gradAccumulationSteps"] = m_config.gradAccumulationSteps;
    config["syncInterval"] = m_config.syncInterval;
    config["enableLoadBalancing"] = m_config.enableLoadBalancing;
    config["enableFaultTolerance"] = m_config.enableFaultTolerance;
    config["enableAutoMixedPrecision"] = m_config.enableAutoMixedPrecision;
    config["compressionRatio"] = m_config.compressionRatio;

    QJsonObject pgConfig;
    pgConfig["worldSize"] = m_config.pgConfig.worldSize;
    pgConfig["rank"] = m_config.pgConfig.rank;
    pgConfig["localRank"] = m_config.pgConfig.localRank;
    pgConfig["masterAddr"] = m_config.pgConfig.masterAddr;
    pgConfig["masterPort"] = m_config.pgConfig.masterPort;
    config["processGroup"] = pgConfig;

    return config;
}

bool DistributedTrainer::loadConfiguration(const QJsonObject& config)
{
    m_config.backend = static_cast<Backend>(config["backend"].toInt(0));
    m_config.parallelism = static_cast<ParallelismType>(config["parallelism"].toInt(0));
    m_config.compression = static_cast<GradientCompression>(config["compression"].toInt(0));
    m_config.gradAccumulationSteps = config["gradAccumulationSteps"].toInt(1);
    m_config.syncInterval = config["syncInterval"].toInt(1);
    m_config.enableLoadBalancing = config["enableLoadBalancing"].toBool(false);
    m_config.enableFaultTolerance = config["enableFaultTolerance"].toBool(false);
    m_config.enableAutoMixedPrecision = config["enableAutoMixedPrecision"].toBool(false);
    m_config.compressionRatio = config["compressionRatio"].toDouble(0.1);

    return true;
}

QByteArray DistributedTrainer::compressTopK(const float* gradients, int numElements, float compressionRatio)
{
    int k = std::max(1, static_cast<int>(numElements * compressionRatio));
    std::vector<std::pair<int, float>> indexed;

    for (int i = 0; i < numElements; ++i) {
        indexed.push_back({i, std::abs(gradients[i])});
    }

    std::nth_element(indexed.begin(), indexed.begin() + k, indexed.end(),
                    [](const auto& a, const auto& b) { return a.second > b.second; });

    QByteArray result;
    result.append(reinterpret_cast<const char*>(&k), sizeof(int));

    for (int i = 0; i < k; ++i) {
        result.append(reinterpret_cast<const char*>(&indexed[i].first), sizeof(int));
        result.append(reinterpret_cast<const char*>(&indexed[i].second), sizeof(float));
    }

    return result;
}

QByteArray DistributedTrainer::compressThreshold(const float* gradients, int numElements, float threshold)
{
    QByteArray result;
    int count = 0;

    for (int i = 0; i < numElements; ++i) {
        if (std::abs(gradients[i]) > threshold) {
            count++;
        }
    }

    result.append(reinterpret_cast<const char*>(&count), sizeof(int));

    for (int i = 0; i < numElements; ++i) {
        if (std::abs(gradients[i]) > threshold) {
            result.append(reinterpret_cast<const char*>(&i), sizeof(int));
            result.append(reinterpret_cast<const char*>(&gradients[i]), sizeof(float));
        }
    }

    return result;
}

std::vector<float> DistributedTrainer::decompressTopK(const QByteArray& data, int numElements)
{
    std::vector<float> result(numElements, 0.0f);

    if (data.size() < static_cast<int>(sizeof(int))) {
        return result;
    }

    int k = *reinterpret_cast<const int*>(data.data());
    int offset = sizeof(int);

    for (int i = 0; i < k && offset + 8 <= data.size(); ++i) {
        int idx = *reinterpret_cast<const int*>(data.data() + offset);
        float val = *reinterpret_cast<const float*>(data.data() + offset + sizeof(int));
        if (idx >= 0 && idx < numElements) {
            result[idx] = val;
        }
        offset += sizeof(int) + sizeof(float);
    }

    return result;
}

std::vector<float> DistributedTrainer::decompressThreshold(const QByteArray& data, int numElements)
{
    std::vector<float> result(numElements, 0.0f);

    if (data.size() < static_cast<int>(sizeof(int))) {
        return result;
    }

    int count = *reinterpret_cast<const int*>(data.data());
    int offset = sizeof(int);

    for (int i = 0; i < count && offset + 8 <= data.size(); ++i) {
        int idx = *reinterpret_cast<const int*>(data.data() + offset);
        float val = *reinterpret_cast<const float*>(data.data() + offset + sizeof(int));
        if (idx >= 0 && idx < numElements) {
            result[idx] = val;
        }
        offset += sizeof(int) + sizeof(float);
    }

    return result;
}
