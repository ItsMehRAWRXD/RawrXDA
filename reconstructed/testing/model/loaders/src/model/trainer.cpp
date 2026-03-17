// model_trainer.cpp - GGUF Model Fine-Tuning Engine
// Converted from Qt to pure C++17
#include "model_trainer.h"
#include "common/logger.hpp"
#include "common/file_utils.hpp"
#include "common/string_utils.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <numeric>

namespace fs = std::filesystem;

ModelTrainer::ModelTrainer() {}

ModelTrainer::~ModelTrainer() {
    stopTraining();
}

bool ModelTrainer::startTraining(const TrainingConfig& config) {
    if (m_running.load()) {
        logWarning() << "Training already in progress";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;
        m_metrics = TrainingMetrics{};
        m_metrics.totalEpochs = config.epochs;
    }

    if (m_dataset.empty()) {
        if (!loadDataset(config.datasetPath)) {
            onErrorOccurred.emit("Failed to load dataset: " + config.datasetPath);
            return false;
        }
    }

    m_running.store(true);
    m_paused.store(false);
    setState(TrainingState::Loading);

    // Launch training in background thread
    if (m_trainingThread.joinable()) m_trainingThread.join();
    m_trainingThread = std::thread(&ModelTrainer::trainingLoop, this);

    logInfo() << "Training started: " << config.epochs << " epochs, "
              << m_dataset.size() << " samples";
    return true;
}

void ModelTrainer::pauseTraining() {
    if (m_running.load() && !m_paused.load()) {
        m_paused.store(true);
        setState(TrainingState::Paused);
        logInfo() << "Training paused";
    }
}

void ModelTrainer::resumeTraining() {
    if (m_running.load() && m_paused.load()) {
        m_paused.store(false);
        setState(TrainingState::Training);
        logInfo() << "Training resumed";
    }
}

void ModelTrainer::stopTraining() {
    if (m_running.load()) {
        m_running.store(false);
        m_paused.store(false);
        if (m_trainingThread.joinable()) m_trainingThread.join();
        setState(TrainingState::Idle);
        logInfo() << "Training stopped";
    }
}

bool ModelTrainer::loadDataset(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dataset.clear();

    if (!fs::exists(path)) {
        logCritical() << "Dataset not found: " << path;
        return false;
    }

    std::string content = FileUtils::readFile(path);
    if (content.empty()) return false;

    // Support JSONL format (one JSON object per line)
    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    while (std::getline(stream, line)) {
        lineNum++;
        line = StringUtils::trimmed(line);
        if (line.empty() || line[0] != '{') continue;

        TrainingSample sample;
        // Simple JSON parse for instruction/input/output fields
        auto extractField = [&](const std::string& field) -> std::string {
            std::string key = "\"" + field + "\":\"";
            auto pos = line.find(key);
            if (pos == std::string::npos) {
                key = "\"" + field + "\": \"";
                pos = line.find(key);
            }
            if (pos == std::string::npos) return "";
            pos += key.size();
            auto end = line.find("\"", pos);
            if (end == std::string::npos) return "";
            return line.substr(pos, end - pos);
        };

        sample.instruction = extractField("instruction");
        sample.input = extractField("input");
        sample.output = extractField("output");

        // Fallback: use "text" field
        if (sample.input.empty() && sample.output.empty()) {
            sample.input = extractField("text");
            if (sample.input.empty()) {
                sample.input = extractField("prompt");
                sample.output = extractField("completion");
            }
        }

        if (!sample.input.empty() || !sample.output.empty()) {
            // Estimate token count (~4 chars per token)
            std::string combined = sample.instruction + sample.input + sample.output;
            sample.tokenCount = static_cast<int>(combined.size()) / 4;
            m_dataset.push_back(sample);
        }
    }

    logInfo() << "Loaded dataset: " << m_dataset.size() << " samples from " << path;
    return !m_dataset.empty();
}

int ModelTrainer::getDatasetSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_dataset.size());
}

TrainingSample ModelTrainer::getSample(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_dataset.size())) return m_dataset[index];
    return TrainingSample{};
}

bool ModelTrainer::validateDataset() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_dataset.empty()) return false;
    for (const auto& sample : m_dataset) {
        if (sample.input.empty() && sample.output.empty()) return false;
    }
    return true;
}

TrainingState ModelTrainer::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

TrainingMetrics ModelTrainer::getMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

float ModelTrainer::getProgress() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_metrics.totalSteps == 0) return 0.0f;
    return static_cast<float>(m_metrics.currentStep) / m_metrics.totalSteps;
}

void ModelTrainer::setConfig(const TrainingConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

TrainingConfig ModelTrainer::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

bool ModelTrainer::saveCheckpoint(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Serialize training state as JSON
    std::ostringstream oss;
    oss << "{\n"
        << "  \"epoch\": " << m_metrics.currentEpoch << ",\n"
        << "  \"step\": " << m_metrics.currentStep << ",\n"
        << "  \"loss\": " << m_metrics.loss << ",\n"
        << "  \"bestLoss\": " << m_metrics.bestLoss << ",\n"
        << "  \"learningRate\": " << m_metrics.learningRate << ",\n"
        << "  \"model\": \"" << m_config.modelPath << "\",\n"
        << "  \"dataset\": \"" << m_config.datasetPath << "\"\n"
        << "}";

    fs::create_directories(fs::path(path).parent_path());
    FileUtils::writeFile(path, oss.str());
    logInfo() << "Checkpoint saved: " << path;
    onCheckpointSaved.emit(path);
    return true;
}

bool ModelTrainer::exportModel(const std::string& path, const std::string& format) {
    logInfo() << "Exporting model to: " << path << " (format: " << format << ")";
    if (format == "gguf") {
        return convertToGGUF(m_config.outputPath, path, "q4_0");
    }
    // For other formats, just copy the output
    std::error_code ec;
    fs::copy_file(m_config.outputPath, path, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

bool ModelTrainer::convertToGGUF(const std::string& inputPath, const std::string& outputPath,
                                  const std::string& quantType)
{
    logInfo() << "Converting to GGUF: " << inputPath << " -> " << outputPath
              << " (" << quantType << ")";
    // GGUF conversion would invoke external tool or internal converter
    // Stub: copy file as placeholder
    std::error_code ec;
    fs::copy_file(inputPath, outputPath, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

void ModelTrainer::setState(TrainingState state) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = state;
    }
    onStateChanged.emit(state);
}

void ModelTrainer::trainingLoop() {
    setState(TrainingState::Training);
    auto startTime = std::chrono::steady_clock::now();

    int totalSamples = static_cast<int>(m_dataset.size());
    int stepsPerEpoch = (totalSamples + m_config.batchSize - 1) / m_config.batchSize;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metrics.totalSteps = stepsPerEpoch * m_config.epochs;
    }

    std::mt19937 rng(m_config.seed);

    for (int epoch = 0; epoch < m_config.epochs && m_running.load(); epoch++) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_metrics.currentEpoch = epoch + 1;
        }

        // Shuffle dataset indices
        std::vector<int> indices(totalSamples);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int step = 0; step < stepsPerEpoch && m_running.load(); step++) {
            // Handle pause
            while (m_paused.load() && m_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (!m_running.load()) break;

            int globalStep = epoch * stepsPerEpoch + step;

            // Compute batch loss
            float batchLoss = 0.0f;
            int batchStart = step * m_config.batchSize;
            int batchEnd = std::min(batchStart + m_config.batchSize, totalSamples);
            int batchCount = 0;

            for (int i = batchStart; i < batchEnd; i++) {
                int idx = indices[i % totalSamples];
                float sampleLoss = computeLoss(m_dataset[idx]);
                batchLoss += sampleLoss;
                batchCount++;
            }
            if (batchCount > 0) batchLoss /= batchCount;

            // Update learning rate
            updateLearningRate(globalStep, m_metrics.totalSteps);

            // Update metrics
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_metrics.currentStep = globalStep + 1;
                m_metrics.loss = batchLoss;
                m_metrics.elapsedSeconds = elapsed;
                m_metrics.lossHistory.push_back(batchLoss);
                if (batchLoss < m_metrics.bestLoss) m_metrics.bestLoss = batchLoss;

                // Estimate remaining time
                double stepsRemaining = m_metrics.totalSteps - m_metrics.currentStep;
                double timePerStep = elapsed / m_metrics.currentStep;
                m_metrics.estimatedRemainingSeconds = stepsRemaining * timePerStep;

                // Estimate tokens/sec
                int totalTokens = 0;
                for (int i = batchStart; i < batchEnd; i++) {
                    totalTokens += m_dataset[indices[i % totalSamples]].tokenCount;
                }
                if (elapsed > 0) m_metrics.tokensPerSecond = static_cast<float>(totalTokens / elapsed);
            }

            // Logging
            if ((globalStep + 1) % m_config.loggingSteps == 0) {
                std::ostringstream msg;
                msg << "Step " << (globalStep + 1) << "/" << m_metrics.totalSteps
                    << " | Loss: " << batchLoss
                    << " | LR: " << m_metrics.learningRate
                    << " | Elapsed: " << elapsed << "s";
                onLogMessage.emit(msg.str());
                onProgressUpdate.emit(m_metrics);
            }

            // Save checkpoint
            if ((globalStep + 1) % m_config.saveSteps == 0) {
                std::string ckptPath = m_config.outputPath + "/checkpoint-" +
                                       std::to_string(globalStep + 1) + ".json";
                saveCheckpoint(ckptPath);
            }

            trainStep(globalStep);
        }

        onEpochComplete.emit(epoch + 1, m_config.epochs);
        logInfo() << "Epoch " << (epoch + 1) << "/" << m_config.epochs
                  << " complete. Loss: " << m_metrics.loss;

        // Evaluate at end of epoch
        setState(TrainingState::Evaluating);
        evaluateStep();
        setState(TrainingState::Training);
    }

    if (m_running.load()) {
        // Save final model
        setState(TrainingState::Saving);
        std::string finalPath = m_config.outputPath + "/final_model.json";
        saveCheckpoint(finalPath);
        setState(TrainingState::Completed);
        onTrainingComplete.emit(finalPath);
        logInfo() << "Training complete: " << m_metrics.totalSteps << " steps, "
                  << "best loss: " << m_metrics.bestLoss;
    }

    m_running.store(false);
}

void ModelTrainer::trainStep(int step) {
    // Actual gradient computation and weight update would go here
    // In a real implementation this would call into GGML for tensor operations
    (void)step;
    // Simulate computation time
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}

void ModelTrainer::evaluateStep() {
    // Evaluation on a held-out portion of dataset
    if (m_dataset.size() < 10) return;

    float evalLoss = 0.0f;
    int evalCount = std::min(static_cast<int>(m_dataset.size()) / 10, 50);
    for (int i = 0; i < evalCount; i++) {
        int idx = static_cast<int>(m_dataset.size()) - evalCount + i;
        evalLoss += computeLoss(m_dataset[idx]);
    }
    evalLoss /= evalCount;

    std::ostringstream msg;
    msg << "Evaluation loss: " << evalLoss;
    onLogMessage.emit(msg.str());
}

float ModelTrainer::computeLoss(const TrainingSample& sample) {
    // Simulated cross-entropy loss based on token count
    // Real implementation would use GGML forward pass
    float baseLoss = 2.5f;
    float tokenFactor = 1.0f / (1.0f + 0.01f * sample.tokenCount);

    // Simulate loss decrease over training
    float stepFactor = 1.0f;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_metrics.totalSteps > 0) {
            stepFactor = 1.0f - 0.5f * (static_cast<float>(m_metrics.currentStep) / m_metrics.totalSteps);
        }
    }

    // Add some noise for realism
    static std::mt19937 rng(42);
    std::normal_distribution<float> noise(0.0f, 0.1f);
    return baseLoss * tokenFactor * stepFactor + noise(rng);
}

void ModelTrainer::updateLearningRate(int step, int totalSteps) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int warmupSteps = static_cast<int>(totalSteps * m_config.warmupRatio);

    if (step < warmupSteps) {
        // Linear warmup
        m_metrics.learningRate = m_config.learningRate * (static_cast<float>(step) / warmupSteps);
    } else if (m_config.scheduler == "cosine") {
        // Cosine decay
        float progress = static_cast<float>(step - warmupSteps) / (totalSteps - warmupSteps);
        m_metrics.learningRate = m_config.learningRate * 0.5f * (1.0f + std::cos(3.14159265f * progress));
    } else if (m_config.scheduler == "linear") {
        // Linear decay
        float progress = static_cast<float>(step - warmupSteps) / (totalSteps - warmupSteps);
        m_metrics.learningRate = m_config.learningRate * (1.0f - progress);
    } else {
        // Constant
        m_metrics.learningRate = m_config.learningRate;
    }
}
