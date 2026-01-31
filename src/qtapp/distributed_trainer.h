#pragma once


#include <vector>
#include <map>

class DistributedTrainer : public void {

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

    explicit DistributedTrainer(void* parent = nullptr);
    ~DistributedTrainer();

    // Initialization
    bool initialize(Backend backend, ParallelismType parallelism, const void*& config);

    // Training control
    bool startTraining(const void*& trainingConfig);
    bool stopTraining();
    bool pauseTraining();
    bool resumeTraining();

    // Node management
    bool addNode(const void*& nodeConfig);
    bool removeNode(const std::string& nodeId);
    void* getNodeStatus(const std::string& nodeId) const;

    // Monitoring and metrics
    void* getTrainingStatus() const;
    void* getPerformanceMetrics() const;

    // Fault tolerance
    bool enableCheckpointing(int interval);
    bool recoverFromFailure();


    void trainingStarted();
    void trainingStopped();
    void trainingProgress(int epoch, float loss, float accuracy);
    void nodeAdded(const std::string& nodeId);
    void nodeRemoved(const std::string& nodeId);
    void allReduceCompleted(int size);  // Add this signal

private:
    // Private helper methods
    bool allGather(const void* sendBuffer, void* recvBuffer, int size);  // Add this method
    bool broadcast(void* data, int size);  // Add this method
    void recordCommunicationLatency(float latency);  // Add this method
    
    Backend m_backend;
    ParallelismType m_parallelism;
    void* m_config;
    std::map<std::string, void*> m_nodes;
    bool m_trainingActive;
    bool m_initialized;  // Add this member variable
};

