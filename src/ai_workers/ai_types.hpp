#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class BuildMode {
    Model,
    Agent,
    Hybrid
};

enum class AgentType {
    CodeGenerator,
    DataAnalyst,
    ModelOptimizer,
    AutoML,
    Custom
};

struct DigestionConfig {
    std::string sourceRoot;
    std::vector<std::string> excludePatterns;
    int maxFileSizeBytes = 10 * 1024 * 1024;
    bool extractMetadata = true;
    bool generateEmbeddings = false;
};

struct TrainingConfig {
    std::string modelName;
    BuildMode buildMode = BuildMode::Model;
    AgentType agentType = AgentType::Custom;
    int agentIterations = 100;
    int batchSize = 4;
    double learningRate = 1e-4;
    std::string baseModelPath;
};
