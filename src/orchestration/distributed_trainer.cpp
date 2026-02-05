#include "distributed_trainer.h"
#include <QJsonArray>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <numeric>

DistributedTrainer::DistributedTrainer(QObject* parent)
    : QObject(parent)
    , m_backend(Backend::Custom)
    , m_parallelism(ParallelismType::DataParallel)
    , m_initialized(false)
    , m_trainingActive(false)
    , m_nextNodeId(1)
{}

DistributedTrainer::~DistributedTrainer() {}

bool DistributedTrainer::initialize(Backend backend, ParallelismType parallelism, const QJsonObject& config) {
    m_backend = backend;
    m_parallelism = parallelism;
    m_config = config;
    m_initialized = true;
    m_trainingActive = false;
    m_nodes.clear();
    m_metrics.clear();
    qInfo() << "[DistributedTrainer] Initialized backend=" << static_cast<int>(backend)
            << "parallelism=" << static_cast<int>(parallelism)
            << "nodes=0";
    return true;
}

bool DistributedTrainer::startTraining(const TrainingConfig& config) {
    if (!m_initialized) return false;
    m_trainingConfig = config;
    m_trainingActive = true;
    m_trainingStartTime = QDateTime::currentDateTimeUtc();
    qInfo() << "[DistributedTrainer] Training started with" << m_nodes.size() << "nodes";
    return true;
}

bool DistributedTrainer::stopTraining() {
    if (!m_initialized) return false;
    m_trainingActive = false;
    qInfo() << "[DistributedTrainer] Training stopped";
    return true;
}

QJsonObject DistributedTrainer::getTrainingStatus() const {
    QJsonObject status;
    status["initialized"] = m_initialized;
    status["trainingActive"] = m_trainingActive;
    status["backend"] = static_cast<int>(m_backend);
    status["parallelism"] = static_cast<int>(m_parallelism);
    status["nodeCount"] = static_cast<int>(m_nodes.size());
    if (m_trainingActive && m_trainingStartTime.isValid()) {
        status["uptimeSeconds"] = m_trainingStartTime.secsTo(QDateTime::currentDateTimeUtc());
    }
    status["metrics"] = m_metrics;
    return status;
}

bool DistributedTrainer::addNode(const NodeConfig& config) {
    if (!m_initialized) return false;
    NodeInfo info;
    info.nodeId = m_nextNodeId++;
    info.rank = static_cast<int>(m_nodes.size());
    info.hostname = config.hostname;
    info.device = config.device;
    info.status = "ready";
    m_nodes.push_back(info);
    qInfo() << "[DistributedTrainer] Added node" << info.nodeId << "host=" << info.hostname;
    return true;
}

bool DistributedTrainer::removeNode(int nodeId) {
    if (!m_initialized) return false;
    auto it = std::remove_if(m_nodes.begin(), m_nodes.end(), [nodeId](const NodeInfo& n){ return n.nodeId == nodeId; });
    if (it == m_nodes.end()) return false;
    m_nodes.erase(it, m_nodes.end());
    qInfo() << "[DistributedTrainer] Removed node" << nodeId;
    return true;
}

QJsonObject DistributedTrainer::getNodeStatus(int nodeId) const {
    for (const auto& n : m_nodes) {
        if (n.nodeId == nodeId) {
            QJsonObject obj;
            obj["nodeId"] = n.nodeId;
            obj["rank"] = n.rank;
            obj["hostname"] = n.hostname;
            obj["device"] = n.device;
            obj["status"] = n.status;
            return obj;
        }
    }
    return QJsonObject();
}

QByteArray DistributedTrainer::compressTopK(const float* gradients, int numElements, float compressionRatio) {
    if (!gradients || numElements <= 0 || compressionRatio <= 0.f) return QByteArray();
    int k = std::clamp(static_cast<int>(std::ceil(numElements * compressionRatio)), 1, numElements);
    std::vector<std::pair<int, float>> indexed;
    indexed.reserve(numElements);
    for (int i = 0; i < numElements; ++i) {
        indexed.emplace_back(i, gradients[i]);
    }
    std::partial_sort(indexed.begin(), indexed.begin() + k, indexed.end(),
                      [](auto& a, auto& b){ return std::fabs(a.second) > std::fabs(b.second); });

    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    out << k;
    for (int i = 0; i < k; ++i) {
        out << indexed[i].first << indexed[i].second;
    }
    return buffer;
}

QByteArray DistributedTrainer::compressThreshold(const float* gradients, int numElements, float threshold) {
    if (!gradients || numElements <= 0) return QByteArray();
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    int count = 0;
    for (int i = 0; i < numElements; ++i) {
        if (std::fabs(gradients[i]) >= threshold) {
            ++count;
        }
    }
    out << count;
    for (int i = 0; i < numElements; ++i) {
        if (std::fabs(gradients[i]) >= threshold) {
            out << i << gradients[i];
        }
    }
    return buffer;
}

std::vector<float> DistributedTrainer::decompressTopK(const QByteArray& data, int numElements) {
    std::vector<float> result(numElements, 0.f);
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_6_5);
    int k = 0;
    in >> k;
    for (int i = 0; i < k; ++i) {
        int idx; float val;
        in >> idx >> val;
        if (idx >= 0 && idx < numElements) {
            result[idx] = val;
        }
    }
    return result;
}

std::vector<float> DistributedTrainer::decompressThreshold(const QByteArray& data, int numElements) {
    std::vector<float> result(numElements, 0.f);
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_6_5);
    int count = 0;
    in >> count;
    for (int i = 0; i < count; ++i) {
        int idx; float val;
        in >> idx >> val;
        if (idx >= 0 && idx < numElements) {
            result[idx] = val;
        }
    }
    return result;
}

// Helpers to record metrics (simple aggregation)
void DistributedTrainer::recordThroughput(float samplesPerSec) {
    m_metrics["throughput"] = samplesPerSec;
}

void DistributedTrainer::recordLatency(float ms) {
    m_metrics["latencyMs"] = ms;
}