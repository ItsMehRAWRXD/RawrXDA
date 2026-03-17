// ============================================================================
// File: model_trainer.h
// Purpose: Production-ready GGUF model fine-tuning interface
// Features: Dataset ingestion, tokenization, Adam optimizer, validation
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once


#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "nlohmann/json.hpp"

class GGUFLoader;
namespace RawrXD { class InferenceEngine; }

/**
 * @class ModelTrainer
 * @brief Production-ready on-device GGUF model fine-tuning
 *
 * Features:
 * - Dataset ingestion (CSV, JSON-L, plain text)
 * - Real tokenization using existing InferenceEngine
 * - AdamW optimizer with learning rate scheduling
 * - Gradient clipping and weight decay
 * - Real-time progress reporting via signals
 * - Model validation and perplexity calculation
 * - Thread-safe training execution
 * - Checkpoint management and model registration
 */
class ModelTrainer {
    friend class AITrainingPipeline; // Allow pipeline to drive training step-by-step

public:
    // Callbacks
    std::function<void(const std::string&)> onLogMessage;
    std::function<void(const std::string&)> onTrainingError;
    std::function<void(int, int)> onEpochStarted;
    std::function<void(int, float, float)> onEpochCompleted;
    std::function<void()> onTrainingStarted;
    std::function<void(const std::string&, float)> onTrainingCompleted;
    std::function<void()> onTrainingStopped;

    // ===== Configuration =====

    /**
     * @struct TrainingConfig
     * @brief Training hyperparameters and options
     */
    struct TrainingConfig {
        std::string datasetPath;           ///< Path to training dataset
        std::string outputPath;            ///< Path to save trained model
        int epochs = 3;                ///< Number of training epochs
        float learningRate = 1e-4f;    ///< Base learning rate
        int batchSize = 4;             ///< Batch size for training
        int sequenceLength = 512;      ///< Max sequence length
        float gradientClip = 1.0f;     ///< Gradient clipping threshold
        float validationSplit = 0.1f;  ///< Validation data split ratio
        float warmupSteps = 0.1f;      ///< Warmup as % of total steps
        float weightDecay = 0.01f;     ///< L2 regularization coefficient
        bool validateEveryEpoch = true; ///< Run validation each epoch
    };

    /**
     * @enum DatasetFormat
     * @brief Supported dataset formats
     */
    enum class DatasetFormat {
        PlainText,  ///< Plain text (one sample per line)
        JsonLines,  ///< JSON lines (JSONL format)
        Csv         ///< CSV format with text column
    };

    // ===== Lifecycle =====

    /**
     * @brief Constructor
     * @param parent Qt parent object
     */
    explicit ModelTrainer(void* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~ModelTrainer();

    /**
     * @brief Initialize trainer with model and inference engine
     * @param engine Pointer to InferenceEngine
     * @param modelPath Path to GGUF model file
     * @return true if initialization successful
     */
    bool initialize(RawrXD::InferenceEngine* engine, const std::string& modelPath);

    /**
     * @brief Start training with configuration
     * @param config Training configuration
     * @return true if training started successfully
     */
    bool startTraining(const TrainingConfig& config);

    /**
     * @brief Stop training gracefully
     */
    void stopTraining();

    /**
     * @brief Start training synchronously (blocks until completion)
     * @param config Configuration
     * @return true if training completed successfully
     */
    bool startTrainingSync(const TrainingConfig& config);

    // ===== Status & Queries =====

    /**
     * @brief Check if training is in progress
     * @return true if training active
     */
    bool isTraining() const { return m_isTraining; }

    /**
     * @brief Get current epoch number
     * @return Current epoch (1-based)
     */
    int getCurrentEpoch() const { return m_currentEpoch; }

    /**
     * @brief Get total epochs to train
     * @return Total epochs
     */
    int getTotalEpochs() const { return m_totalEpochs; }

    /**
     * @brief Get current training loss
     * @return Loss value
     */
    float getCurrentLoss() const { return m_currentLoss; }

    /**
     * @brief Get validation perplexity
     * @return Perplexity value
     */
    float getValidationPerplexity() const { return m_validationPerplexity; }

    /**
     * @brief Get current training status message
     * @return Status string
     */
    std::string getCurrentStatus() const { return m_currentStatus; }

    // ===== Dataset Operations =====

    /**
     * @brief Auto-detect dataset format from file extension
     * @param filePath Path to dataset file
     * @return Detected format
     */
    DatasetFormat detectDatasetFormat(const std::string& filePath);

    /**
     * @brief Load dataset from file
     * @param filePath Path to dataset file
     * @param format Dataset format
     * @return true if dataset loaded successfully
     */
    bool loadDataset(const std::string& filePath, DatasetFormat format);

    /**
     * @brief Tokenize loaded dataset
     * @return Vector of token sequences
     */
    std::vector<std::vector<uint32_t>> tokenizeDataset();

    // ===== Signals =====


    /**
     * @brief Emitted when training starts
     */
    void trainingStarted();

    /**
     * @brief Emitted when an epoch begins
     * @param epoch Current epoch number (1-based)
     * @param totalEpochs Total epochs to train
     */
    void epochStarted(int epoch, int totalEpochs);

    /**
     * @brief Emitted after each batch is processed
     * @param batch Current batch number
     * @param totalBatches Total batches in epoch
     * @param loss Current batch loss
     */
    void batchProcessed(int batch, int totalBatches, float loss);

    /**
     * @brief Emitted when an epoch completes
     * @param epoch Completed epoch number
     * @param loss Final epoch loss
     * @param perplexity Validation perplexity if available
     */
    void epochCompleted(int epoch, float loss, float perplexity);

    /**
     * @brief Emitted when training completes successfully
     * @param outputPath Path to saved model
     * @param finalPerplexity Final model perplexity
     */
    void trainingCompleted(const std::string& outputPath, float finalPerplexity);

    /**
     * @brief Emitted when training is stopped by user
     */
    void trainingStopped();

    /**
     * @brief Emitted when an error occurs
     * @param error Error message
     */
    void trainingError(const std::string& error);

    /**
     * @brief Emitted for informational log messages
     * @param message Log message
     */
    void logMessage(const std::string& message);

    /**
     * @brief Emitted when validation completes
     * @param perplexity Model perplexity
     * @param details Additional validation details
     */
    void validationResults(float perplexity, const std::string& details);

    /**
     * @brief Emitted when trained model is registered in IDE
     * @param modelPath Path to registered model
     */
    void modelRegistered(const std::string& modelPath);

private:
    /**
     * @brief Main training loop (runs in worker thread)
     */
    void runTraining();

private:
    // ===== Dataset Loading =====
    std::vector<std::string> readPlainTextDataset(const std::string& filePath);
    std::vector<nlohmann::json> readJsonLinesDataset(const std::string& filePath);
    std::vector<nlohmann::json> readCsvDataset(const std::string& filePath);
    std::vector<std::string> splitString(const std::string& str, char delimiter);
    std::string trimString(const std::string& str);

    // ===== Tokenization =====
    std::vector<uint32_t> tokenizeText(const std::string& text);
    std::vector<std::vector<uint32_t>> createBatches(
        const std::vector<std::vector<uint32_t>>& tokenizedData);

    // ===== Training Data =====
    bool prepareTrainingData();
    bool executeEpoch(int epoch);
    bool processBatch(const std::vector<std::vector<uint32_t>>& batchData);
    std::vector<uint32_t> extractTargets(const std::vector<uint32_t>& sequence);
    float computeLoss(const std::vector<float>& logits,
                     const std::vector<uint32_t>& targets);

    // ===== Optimizer =====
    struct AdamOptimizer {
        float learningRate = 1e-4f;
        float beta1 = 0.9f;
        float beta2 = 0.999f;
        float epsilon = 1e-8f;
        float weightDecay = 0.01f;
        std::vector<float> m;  // First moment
        std::vector<float> v;  // Second moment
        int t = 0;             // Time step
    };

    bool updateModelWeights(const std::vector<float>& gradients);
    float getLearningRate(int step, int totalSteps);
    void clipGradients(std::vector<float>& gradients, float maxNorm);
    void applyWeightDecay(std::vector<float>& gradients,
                         const std::vector<float>& weights);

    // ===== Model Operations =====
    std::vector<float> extractModelWeights();
    bool applyWeightUpdates(const std::vector<float>& newWeights);
    bool saveModel(const std::string& outputPath);
    bool registerTrainedModel(const std::string& modelPath);

    // ===== Validation =====
    bool validateModel();
    float calculatePerplexity();

    // ===== State Management =====
    RawrXD::InferenceEngine* m_inferenceEngine = nullptr;
    std::unique_ptr<GGUFLoader> m_modelLoader;
    std::string m_modelPath;
    std::string m_originalModelPath;

    TrainingConfig m_config;
    std::vector<std::string> m_textData;
    std::vector<nlohmann::json> m_jsonData;
    std::vector<std::vector<int>> m_tokenizedData;
    std::vector<std::vector<int>> m_trainingBatches;
    std::vector<std::vector<int>> m_validationBatches;

    AdamOptimizer m_optimizer;
    bool m_isTraining = false;
    bool m_shouldStop = false;
    int m_currentEpoch = 0;
    int m_totalEpochs = 0;
    float m_currentLoss = 0.0f;
    float m_validationPerplexity = 0.0f;
    std::string m_currentStatus = "Idle";

    std::thread* m_trainingThread = nullptr;

    // Model metadata
    uint32_t m_vocabSize = 32000;
    uint32_t m_embeddingDim = 4096;
    uint32_t m_layerCount = 32;
    uint32_t m_sequenceLength = 512;
};

