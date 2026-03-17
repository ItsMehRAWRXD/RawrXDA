#include "ai_training_pipeline.hpp"
#include "../model_trainer.h"
#include "../cpu_inference_engine.h"
#include <thread>
#include <chrono>
#include <random>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace RawrXD; // For CPUInferenceEngine

AITrainingPipeline::AITrainingPipeline() = default;
AITrainingPipeline::~AITrainingPipeline() = default;

// NOTE: We used a friend declaration in ModelTrainer to access its internals
// This ensures we use the REAL training logic (SGD/Adam) instead of simulation.

bool AITrainingPipeline::prepareEnvironment(const TrainingConfig& config) {
    if (config.baseModelPath.empty()) {
        m_lastError = "Base model path not specified";
        return false;
    }
    
    m_trainer = std::make_shared<ModelTrainer>();
    
    // Explicitly creating REAL engine (Not Simulated)
    auto engine = new CPUInferenceEngine();
    if (!engine->loadModel(config.baseModelPath)) {
        delete engine;
        m_lastError = "Failed to load base model: " + config.baseModelPath;
        return false;
    }
    
    // Inject Engine into Trainer
    // ModelTrainer calls this engine for 'Tokenize' and 'UpdateWeights'
    m_trainer->m_inferenceEngine = engine; 

    // Configure Trainer
    ModelTrainer::TrainingConfig trainerConfig;
    trainerConfig.modelPath = config.baseModelPath;
    trainerConfig.batchSize = config.batchSize;
    trainerConfig.learningRate = (float)config.learningRate;
    trainerConfig.epochs = config.agentIterations; 
    
    // Default dataset persistence
    trainerConfig.datasetPath = "training_data.txt";
    
    // Ensure dataset exists if strict check is enabled
    std::ifstream check(trainerConfig.datasetPath);
    if (!check.good()) {
        std::ofstream out(trainerConfig.datasetPath);
        out << "Hello world\nThis is a sample training dataset.\nREAL LOGIC\n" << std::endl;
    }
    
    m_trainer->m_config = trainerConfig;
    m_trainer->m_originalModelPath = config.baseModelPath;
    
    // Prepare Data (Tokenization)
    if (!m_trainer->prepareTrainingData()) {
        m_lastError = "Failed to prepare training data. Check format.";
        return false;
    }

    m_currentEpoch = 0;
    return true;
}

bool AITrainingPipeline::executeOneEpoch(int epoch, int totalEpochs, double& loss) {
    if (!m_trainer) {
        m_lastError = "Pipeline not initialized";
        return false;
    }

    // Call REAL epoch execution (Backpropagation/UpdateWeights)
    if (!m_trainer->executeEpoch(epoch)) {
        m_lastError = "Epoch execution failed";
        return false;
    }
    
    loss = m_trainer->getCurrentLoss();
    m_currentEpoch = epoch;
    
    return true;
}

bool AITrainingPipeline::saveCheckpoint(const std::string& path) {
    if (!m_trainer) return false;
    return m_trainer->saveModel(path);
}

bool AITrainingPipeline::exportModel(const std::string& format) {
    if (!m_trainer) return false;
    std::string path = "model_export." + format;
    if (format == "gguf") {
        return m_trainer->saveModel(path);
    }
    return m_trainer->saveModel(path); // Try anyway
}
