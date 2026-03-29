#pragma once

#include <cstdint>
#include <string>
#include <vector>

class CheckpointManager {
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
        float validationLoss = 0.0f;
        float trainLoss = 0.0f;
        float accuracy = 0.0f;
        float wallclockTime = 0.0f;
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
        std::string configJson;
    };

    struct CheckpointIndex {
        std::string checkpointId;
        std::string filePath;
        CheckpointMetadata metadata;
        int checkpointNumber = 0;
    };

    using ShowCallback = void(*)(void* ctx, const std::vector<CheckpointIndex>& checkpoints);

    explicit CheckpointManager(void* parent = nullptr);
    ~CheckpointManager();

    bool initialize(const std::string& checkpointDir, int maxCheckpoints = 10);
    std::vector<CheckpointIndex> listCheckpoints() const;

    void setShowCallback(ShowCallback cb, void* ctx);
    void show();

private:
    std::string m_checkpointDir;
    int m_maxCheckpoints = 10;
    ShowCallback m_showCb = nullptr;
    void* m_showCtx = nullptr;
};
