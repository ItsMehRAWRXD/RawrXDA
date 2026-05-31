#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QProcess>
#include <vector>
#include <map>

class DistributedTrainer : public QObject {
    Q_OBJECT

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
        PipelineParallel
    };

    explicit DistributedTrainer(QObject* parent = nullptr);
    ~DistributedTrainer();

    // Initialization
    bool initialize(Backend backend, ParallelismType parallelism, const QJsonObject& config);

    // Training control
    bool startTraining(const QJsonObject& trainingConfig);
    bool stopTraining();
    bool pauseTraining();
    bool resumeTraining();

    // Node management
    bool addNode(const QJsonObject& nodeConfig);
    bool removeNode(const QString& nodeId);
    QJsonObject getNodeStatus(const QString& nodeId) const;

    // Monitoring and metrics
    QJsonObject getTrainingStatus() const;
    QJsonObject getPerformanceMetrics() const;

    // Fault tolerance
    bool enableCheckpointing(int interval);
    bool recoverFromFailure();

signals:
    void trainingStarted();
    void trainingStopped();
    void trainingProgress(int epoch, float loss, float accuracy);
    void nodeAdded(const QString& nodeId);
    void nodeRemoved(const QString& nodeId);
    void allReduceCompleted(int size);  // Add this signal

private:
    // Private helper methods
    bool allGather(const void* sendBuffer, void* recvBuffer, int size);  // Add this method
    bool broadcast(void* data, int size);  // Add this method
    void recordCommunicationLatency(float latency);  // Add this method
    
    Backend m_backend;
    ParallelismType m_parallelism;
    QJsonObject m_config;
    std::map<QString, QJsonObject> m_nodes;
    bool m_trainingActive;
    bool m_initialized;  // Add this member variable
};
