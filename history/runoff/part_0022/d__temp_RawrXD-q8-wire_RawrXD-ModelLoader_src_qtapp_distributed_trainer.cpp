#include "distributed_trainer.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <algorithm>
#include <numeric>
#include <cmath>

DistributedTrainer::DistributedTrainer(QObject* parent)
    : QObject(parent),
      m_initialized(false),
      m_primaryDevice(0),
      m_accumStepIndex(0),
      m_accumStepTarget(1),
      m_checkpointInterval(100)
{
    qDebug() << "[DistributedTrainer] Initializing distributed trainer";
    
    // Initialize default config
    m_config.backend = Backend::NCCL;
    m_config.parallelism = ParallelismType::DataParallel;
    m_config.compression = GradientCompression::None;
    m_config.pgConfig.worldSize = 1;
    m_config.pgConfig.rank = 0;
    m_config.pgConfig.localRank = 0;
    m_config.pgConfig.masterAddr = "127.0.0.1";
    m_config.pgConfig.masterPort = 29500;
    m_config.pgConfig.timeout = 30;
    m_config.pgConfig.enableProfiling = false;
    m_config.gradAccumulationSteps = 1;
    m_config.syncInterval = 1;
    m_config.enableLoadBalancing = false;
    m_config.enableFaultTolerance = false;
    m_config.enableAutoMixedPrecision = false;
    m_config.compressionRatio = 0.1f;
}

DistributedTrainer::~DistributedTrainer()
{
    qDebug() << "[DistributedTrainer] Destroying distributed trainer";
}

// ===== Configuration =====
DistributedTrainer::TrainerConfig DistributedTrainer::getConfiguration() const
{
    return m_config;
}

bool DistributedTrainer::updateConfiguration(const TrainerConfig& config)
{
    m_config = config;
    m_accumStepTarget = config.gradAccumulationSteps;
    qDebug() << "[DistributedTrainer] Configuration updated";
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

// ===== Device Management =====
std::vector<DistributedTrainer::DeviceInfo> DistributedTrainer::getAvailableDevices() const
{
    std::vector<DeviceInfo> devices;
    
    // Simulated device info (in production, use cudaGetDeviceProperties or similar)
    DeviceInfo dev;
    dev.deviceId = 0;
    dev.deviceType = "cuda";
    dev.name = "Simulated GPU";
    dev.totalMemory = 16ULL * 1024 * 1024 * 1024; // 16GB
    dev.availableMemory = 12ULL * 1024 * 1024 * 1024; // 12GB available
    dev.computeCapability = 8.0f;
    dev.currentLoad = 0.3f;
    dev.temperature = 65.0f;
    devices.push_back(dev);
    
    return devices;
}

bool DistributedTrainer::setPrimaryDevice(int deviceId)
{
    m_primaryDevice = deviceId;
    qDebug() << "[DistributedTrainer] Primary device set to" << deviceId;
    return true;
}

std::pair<uint64_t, uint64_t> DistributedTrainer::getMemoryUsage() const
{
    // Simulated memory usage
    return {4096, 16384}; // 4GB used, 16GB total (in MB)
}

float DistributedTrainer::getDeviceTemperature() const
{
    return 65.0f; // Simulated temperature
}

// ===== Training Operations =====
bool DistributedTrainer::initProcessGroup()
{
    qDebug() << "[DistributedTrainer] Initializing process group";
    m_initialized = true;
    emit synchronizationCompleted(m_config.pgConfig.worldSize);
    return true;
}

void DistributedTrainer::destroyProcessGroup()
{
    qDebug() << "[DistributedTrainer] Destroying process group";
    m_initialized = false;
}

bool DistributedTrainer::synchronizeProcesses()
{
    if (!m_initialized) {
        qCritical() << "[DistributedTrainer] Not initialized";
        return false;
    }
    
    qDebug() << "[DistributedTrainer] Synchronizing processes";
    emit synchronizationCompleted(m_config.pgConfig.worldSize);
    return true;
}

bool DistributedTrainer::allReduceGradients(float* gradientData, int size)
{
    if (!m_initialized) {
        qCritical() << "[DistributedTrainer] Not initialized";
        return false;
    }
    
    qDebug() << "[DistributedTrainer] All-reduce on" << size << "elements";
    
    // Simulate averaging (in production, use NCCL/MPI allreduce)
    for (int i = 0; i < size; ++i) {
        gradientData[i] /= m_config.pgConfig.worldSize;
    }
    
    recordCommunicationLatency(1.5f); // Simulated latency
    emit allReduceCompleted(size);
    return true;
}

bool DistributedTrainer::allGather(const void* sendBuffer, void* recvBuffer, int size)
{
    if (!m_initialized) {
        qCritical() << "[DistributedTrainer] Not initialized";
        return false;
    }
    
    qDebug() << "[DistributedTrainer] All-gather" << size << "bytes";
    
    // Simulate copying (in production, use NCCL/MPI allgather)
    std::memcpy(recvBuffer, sendBuffer, size);
    
    recordCommunicationLatency(2.0f);
    return true;
}

bool DistributedTrainer::broadcast(void* data, int size)
{
    if (!m_initialized) {
        qCritical() << "[DistributedTrainer] Not initialized";
        return false;
    }
    
    qDebug() << "[DistributedTrainer] Broadcasting" << size << "bytes from rank 0";
    recordCommunicationLatency(0.5f);
    return true;
}

void DistributedTrainer::recordCommunicationLatency(float latency)
{
    // In a real implementation, this would record metrics
    qDebug() << "[DistributedTrainer] Communication latency:" << latency << "ms";
}

int DistributedTrainer::sendAsync(int destRank, const void* data, int size)
{
    qDebug() << "[DistributedTrainer] Async send to rank" << destRank << size << "bytes";
    return 1; // Simulated handle
}

int DistributedTrainer::recvAsync(int srcRank, void* data, int size)
{
    qDebug() << "[DistributedTrainer] Async recv from rank" << srcRank << size << "bytes";
    return 2; // Simulated handle
}

bool DistributedTrainer::waitAsync(int handle)
{
    qDebug() << "[DistributedTrainer] Waiting for handle" << handle;
    return true;
}

// ===== Gradient Management =====
bool DistributedTrainer::startGradientAccumulation(int numSteps)
{
    qDebug() << "[DistributedTrainer] Starting gradient accumulation for" << numSteps << "steps";
    m_accumStepTarget = numSteps;
    m_accumStepIndex = 0;
    return true;
}

bool DistributedTrainer::recordGradientStep(int stepIndex)
{
    qDebug() << "[DistributedTrainer] Recording gradient step" << stepIndex;
    m_accumStepIndex = stepIndex;
    return true;
}

bool DistributedTrainer::finalizeGradientAccumulation()
{
    qDebug() << "[DistributedTrainer] Finalizing gradient accumulation";
    
    // Average accumulated gradients
    if (!m_accumulatedGradients.empty() && m_accumStepTarget > 0) {
        for (float& grad : m_accumulatedGradients) {
            grad /= m_accumStepTarget;
        }
    }
    
    m_accumStepIndex = 0;
    return true;
}

QByteArray DistributedTrainer::compressGradients(const float* gradients, int numElements)
{
    qDebug() << "[DistributedTrainer] Compressing" << numElements << "gradients";
    
    QByteArray compressed;
    
    switch (m_config.compression) {
        case GradientCompression::TopK:
            compressed = compressTopK(gradients, numElements, m_config.compressionRatio);
            break;
        case GradientCompression::Threshold:
            compressed = compressThreshold(gradients, numElements, 0.01f);
            break;
        case GradientCompression::Quantization:
            // 8-bit quantization
            compressed.resize(numElements);
            for (int i = 0; i < numElements; ++i) {
                compressed[i] = static_cast<char>(std::clamp(gradients[i] * 127.0f, -128.0f, 127.0f));
            }
            break;
        case GradientCompression::DeltaCompression:
            // Delta encoding (placeholder)
            compressed.resize(numElements * sizeof(float));
            std::memcpy(compressed.data(), gradients, numElements * sizeof(float));
            break;
        default:
            // No compression
            compressed.resize(numElements * sizeof(float));
            std::memcpy(compressed.data(), gradients, numElements * sizeof(float));
            break;
    }
    
    if (m_config.compression != GradientCompression::None) {
        int originalSize = numElements * sizeof(float);
        float ratio = static_cast<float>(compressed.size()) / originalSize;
        emit gradientCompressionCompleted(originalSize, compressed.size(), ratio);
    }
    
    return compressed;
}

std::vector<float> DistributedTrainer::decompressGradients(const QByteArray& compressedGradients, int numElements)
{
    qDebug() << "[DistributedTrainer] Decompressing gradients";
    
    std::vector<float> decompressed(numElements, 0.0f);
    
    switch (m_config.compression) {
        case GradientCompression::TopK:
            decompressed = decompressTopK(compressedGradients, numElements);
            break;
        case GradientCompression::Threshold:
            decompressed = decompressThreshold(compressedGradients, numElements);
            break;
        case GradientCompression::Quantization:
            // Dequantize from 8-bit
            for (int i = 0; i < numElements && i < compressedGradients.size(); ++i) {
                decompressed[i] = static_cast<float>(compressedGradients[i]) / 127.0f;
            }
            break;
        default:
            // No compression
            if (compressedGradients.size() >= numElements * sizeof(float)) {
                std::memcpy(decompressed.data(), compressedGradients.data(), numElements * sizeof(float));
            }
            break;
    }
    
    return decompressed;
}

// ===== Load Balancing =====
int DistributedTrainer::getRecommendedBatchSize(int globalBatchSize) const
{
    if (!m_config.enableLoadBalancing) {
        return globalBatchSize / m_config.pgConfig.worldSize;
    }
    
    // Adjust based on node performance (simplified)
    float avgLoad = 0.5f;
    return static_cast<int>(globalBatchSize / m_config.pgConfig.worldSize / avgLoad);
}

void DistributedTrainer::updateLoadInfo(float currentLoad, float throughput)
{
    int rank = m_config.pgConfig.rank;
    m_nodeLoads[rank] = currentLoad;
    m_nodeThroughputs[rank] = throughput;
    qDebug() << "[DistributedTrainer] Updated load info: rank" << rank << "load" << currentLoad << "throughput" << throughput;
}

std::map<int, int> DistributedTrainer::getLoadBalancingSuggestions() const
{
    std::map<int, int> suggestions;
    
    // Suggest adjustments based on relative loads
    for (const auto& pair : m_nodeLoads) {
        int rank = pair.first;
        float load = pair.second;
        
        if (load > 0.8f) {
            suggestions[rank] = -10; // Reduce batch size by 10%
        } else if (load < 0.3f) {
            suggestions[rank] = 10; // Increase batch size by 10%
        }
    }
    
    return suggestions;
}

// ===== Performance Monitoring =====
void DistributedTrainer::recordCommunicationLatency(float latencyMs)
{
    m_communicationLatencies.push_back(latencyMs);
    
    // Keep only recent 100 samples
    if (m_communicationLatencies.size() > 100) {
        m_communicationLatencies.erase(m_communicationLatencies.begin());
    }
}

float DistributedTrainer::getAvgCommunicationLatency() const
{
    if (m_communicationLatencies.empty()) {
        return 0.0f;
    }
    
    float sum = std::accumulate(m_communicationLatencies.begin(), m_communicationLatencies.end(), 0.0f);
    return sum / m_communicationLatencies.size();
}

float DistributedTrainer::getCommunicationOverheadPercent() const
{
    // Simulated: assume 20% of time in communication
    return 20.0f;
}

void DistributedTrainer::recordThroughput(float samplesPerSecond)
{
    m_throughputs.push_back(samplesPerSecond);
    
    if (m_throughputs.size() > 100) {
        m_throughputs.erase(m_throughputs.begin());
    }
}

std::vector<DistributedTrainer::NodePerformance> DistributedTrainer::getPerformanceReport() const
{
    std::vector<NodePerformance> report;
    
    for (const auto& pair : m_nodePerformance) {
        report.push_back(pair.second);
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
    
    if (!m_throughputs.empty()) {
        float avgThroughput = std::accumulate(m_throughputs.begin(), m_throughputs.end(), 0.0f) / m_throughputs.size();
        metrics["avgThroughput"] = avgThroughput;
    }
    
    // Cannot emit from const context - removed signal
    
    return metrics;
}

// ===== Fault Tolerance =====
bool DistributedTrainer::enableCheckpointing(const QString& checkpointDir, int intervalSteps)
{
    qDebug() << "[DistributedTrainer] Enabling checkpointing:" << checkpointDir;
    m_checkpointDir = checkpointDir;
    m_checkpointInterval = intervalSteps;
    
    QDir dir(m_checkpointDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    return true;
}

bool DistributedTrainer::saveCheckpoint(int stepNumber, const QJsonObject& modelState)
{
    QString checkpointPath = m_checkpointDir + QString("/checkpoint_step_%1_rank_%2.json")
                                .arg(stepNumber)
                                .arg(m_config.pgConfig.rank);
    
    qDebug() << "[DistributedTrainer] Saving checkpoint:" << checkpointPath;
    
    QFile file(checkpointPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << "[DistributedTrainer] Failed to open checkpoint file";
        return false;
    }
    
    QJsonDocument doc(modelState);
    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();
    
    emit checkpointCompleted(stepNumber, checkpointPath);
    return true;
}

QJsonObject DistributedTrainer::loadCheckpoint(const QString& checkpointPath)
{
    qDebug() << "[DistributedTrainer] Loading checkpoint:" << checkpointPath;
    
    QFile file(checkpointPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "[DistributedTrainer] Failed to open checkpoint file";
        return QJsonObject();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    return doc.object();
}

bool DistributedTrainer::handleProcessFailure(int failedRank)
{
    qDebug() << "[DistributedTrainer] Handling failure of rank" << failedRank;
    
    emit processFailure(failedRank, "Process stopped responding");
    
    // Simulate recovery
    emit recoveryCompleted(failedRank);
    
    return true;
}

// ===== Configuration Export/Import =====
QJsonObject DistributedTrainer::exportConfiguration() const
{
    QJsonObject config;
    
    config["backend"] = static_cast<int>(m_config.backend);
    config["parallelism"] = static_cast<int>(m_config.parallelism);
    config["compression"] = static_cast<int>(m_config.compression);
    config["worldSize"] = m_config.pgConfig.worldSize;
    config["rank"] = m_config.pgConfig.rank;
    config["localRank"] = m_config.pgConfig.localRank;
    config["gradAccumulationSteps"] = m_config.gradAccumulationSteps;
    config["enableLoadBalancing"] = m_config.enableLoadBalancing;
    config["enableFaultTolerance"] = m_config.enableFaultTolerance;
    config["compressionRatio"] = m_config.compressionRatio;
    
    return config;
}

bool DistributedTrainer::loadConfiguration(const QJsonObject& config)
{
    qDebug() << "[DistributedTrainer] Loading configuration from JSON";
    
    if (config.contains("backend")) {
        m_config.backend = static_cast<Backend>(config["backend"].toInt());
    }
    
    if (config.contains("parallelism")) {
        m_config.parallelism = static_cast<ParallelismType>(config["parallelism"].toInt());
    }
    
    if (config.contains("compression")) {
        m_config.compression = static_cast<GradientCompression>(config["compression"].toInt());
    }
    
    if (config.contains("worldSize")) {
        m_config.pgConfig.worldSize = config["worldSize"].toInt();
    }
    
    if (config.contains("rank")) {
        m_config.pgConfig.rank = config["rank"].toInt();
    }
    
    if (config.contains("gradAccumulationSteps")) {
        m_config.gradAccumulationSteps = config["gradAccumulationSteps"].toInt();
        m_accumStepTarget = m_config.gradAccumulationSteps;
    }
    
    if (config.contains("compressionRatio")) {
        m_config.compressionRatio = config["compressionRatio"].toDouble();
    }
    
    return true;
}

// ===== Private Helper Methods =====
QByteArray DistributedTrainer::compressTopK(const float* gradients, int numElements, float compressionRatio)
{
    int k = static_cast<int>(numElements * compressionRatio);
    
    // Create pairs of (abs value, index)
    std::vector<std::pair<float, int>> valueIndexPairs;
    for (int i = 0; i < numElements; ++i) {
        valueIndexPairs.push_back({std::abs(gradients[i]), i});
    }
    
    // Partial sort to get top-K
    std::partial_sort(valueIndexPairs.begin(), valueIndexPairs.begin() + k, valueIndexPairs.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Serialize
    QByteArray compressed;
    QDataStream stream(&compressed, QIODevice::WriteOnly);
    stream << k;
    
    for (int i = 0; i < k; ++i) {
        int idx = valueIndexPairs[i].second;
        stream << idx << gradients[idx];
    }
    
    return compressed;
}

QByteArray DistributedTrainer::compressThreshold(const float* gradients, int numElements, float threshold)
{
    QByteArray compressed;
    QDataStream stream(&compressed, QIODevice::WriteOnly);
    
    int count = 0;
    for (int i = 0; i < numElements; ++i) {
        if (std::abs(gradients[i]) > threshold) {
            count++;
        }
    }
    
    stream << count;
    
    for (int i = 0; i < numElements; ++i) {
        if (std::abs(gradients[i]) > threshold) {
            stream << i << gradients[i];
        }
    }
    
    return compressed;
}

std::vector<float> DistributedTrainer::decompressTopK(const QByteArray& data, int numElements)
{
    std::vector<float> decompressed(numElements, 0.0f);
    
    QDataStream stream(data);
    int k;
    stream >> k;
    
    for (int i = 0; i < k; ++i) {
        int idx;
        float value;
        stream >> idx >> value;
        if (idx >= 0 && idx < numElements) {
            decompressed[idx] = value;
        }
    }
    
    return decompressed;
}

std::vector<float> DistributedTrainer::decompressThreshold(const QByteArray& data, int numElements)
{
    std::vector<float> decompressed(numElements, 0.0f);
    
    QDataStream stream(data);
    int count;
    stream >> count;
    
    for (int i = 0; i < count; ++i) {
        int idx;
        float value;
        stream >> idx >> value;
        if (idx >= 0 && idx < numElements) {
            decompressed[idx] = value;
        }
    }
    
    return decompressed;
}
