// ============================================================================
// File: model_trainer.h
// Purpose: Production-ready GGUF model fine-tuning interface
// Features: Dataset ingestion, tokenization, Adam optimizer, validation
// License: Production Grade - Enterprise Ready
// C++20-only. No Qt. No exceptions. No std::function.
// ============================================================================

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <cstdint>

class GGUFLoader;
class InferenceEngine;

/**
 * @class ModelTrainer
 * @brief Production-ready on-device GGUF model fine-tuning
 *
 * Features:
 * - Dataset ingestion (CSV, JSON-L, plain text)
 * - Real tokenization using existing InferenceEngine
 * - AdamW optimizer with learning rate scheduling
 * - Gradient clipping and weight decay
 * - Real-time progress reporting via function-pointer callbacks
 * - Model validation and perplexity calculation
 * - Thread-safe training execution
 * - Checkpoint management and model registration
 */
class ModelTrainer {
public:
    // ===== Configuration =====

    /**
     * @struct TrainingConfig
     * @brief Training hyperparameters and options
     */
    struct TrainingConfig {
        std::string datasetPath;           ///< Path to training dataset
        std::string outputPath;            ///< Path to save trained model
        int epochs = 3;                    ///< Number of training epochs
        float learningRate = 1e-4f;        ///< Base learning rate
        int batchSize = 4;                 ///< Batch size for training
        int sequenceLength = 512;          ///< Max sequence length
        float gradientClip = 1.0f;         ///< Gradient clipping threshold
        float validationSplit = 0.1f;      ///< Validation data split ratio
        float warmupSteps = 0.1f;          ///< Warmup as % of total steps
        float weightDecay = 0.01f;         ///< L2 regularization coefficient
        bool validateEveryEpoch = true;    ///< Run validation each epoch
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

    // ===== Callback types — raw function pointers, void* ctx for user data =====
    using TrainingStartedFn     = void(*)(void* ctx);
    using EpochStartedFn        = void(*)(int epoch, int totalEpochs, void* ctx);
    using BatchProcessedFn      = void(*)(int batch, int totalBatches, float loss, void* ctx);
    using EpochCompletedFn      = void(*)(int epoch, float loss, float perplexity, void* ctx);
    using TrainingCompletedFn   = void(*)(const char* outputPath, float finalPerplexity, void* ctx);
    using TrainingStoppedFn     = void(*)(void* ctx);
    using TrainingErrorFn       = void(*)(const char* error, void* ctx);
    using LogMessageFn          = void(*)(const char* message, void* ctx);
    using ValidationResultsFn   = void(*)(float perplexity, const char* details, void* ctx);
    using ModelRegisteredFn     = void(*)(const char* modelPath, void* ctx);

    // ===== Lifecycle =====

    /**
     * @brief Constructor
     */
    ModelTrainer();

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
    bool initialize(InferenceEngine* engine, const std::string& modelPath);

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

    // ===== Status & Queries =====

    /** @brief Check if training is in progress */
    bool isTraining() const { return m_isTraining.load(std::memory_order_acquire); }

    /** @brief Get current epoch number (1-based) */
    int getCurrentEpoch() const { return m_currentEpoch; }

    /** @brief Get total epochs to train */
    int getTotalEpochs() const { return m_totalEpochs; }

    /** @brief Get current training loss */
    float getCurrentLoss() const { return m_currentLoss; }

    /** @brief Get validation perplexity */
    float getValidationPerplexity() const { return m_validationPerplexity; }

    /** @brief Get current training status message */
    const char* getCurrentStatus() const { return m_currentStatus.c_str(); }

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

    // ===== Callback setters =====
    void setCallbackContext(void* ctx)                     { m_callbackCtx = ctx; }
    void setOnTrainingStarted(TrainingStartedFn f)         { m_onTrainingStarted = f; }
    void setOnEpochStarted(EpochStartedFn f)               { m_onEpochStarted = f; }
    void setOnBatchProcessed(BatchProcessedFn f)           { m_onBatchProcessed = f; }
    void setOnEpochCompleted(EpochCompletedFn f)           { m_onEpochCompleted = f; }
    void setOnTrainingCompleted(TrainingCompletedFn f)     { m_onTrainingCompleted = f; }
    void setOnTrainingStopped(TrainingStoppedFn f)         { m_onTrainingStopped = f; }
    void setOnTrainingError(TrainingErrorFn f)             { m_onTrainingError = f; }
    void setOnLogMessage(LogMessageFn f)                   { m_onLogMessage = f; }
    void setOnValidationResults(ValidationResultsFn f)     { m_onValidationResults = f; }
    void setOnModelRegistered(ModelRegisteredFn f)         { m_onModelRegistered = f; }

private:
    /**
     * @brief Main training loop (runs in worker thread)
     */
    void runTraining();

    // ===== Dataset Loading =====
    std::vector<std::string> readPlainTextDataset(const std::string& filePath);
    std::vector<std::string> readJsonLinesDataset(const std::string& filePath);
    std::vector<std::string> readCsvDataset(const std::string& filePath);

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
    InferenceEngine* m_inferenceEngine = nullptr;
    std::unique_ptr<GGUFLoader> m_modelLoader;
    std::string m_modelPath;
    std::string m_originalModelPath;

    TrainingConfig m_config;
    std::vector<std::string> m_textData;
    std::vector<std::string> m_jsonData;   // JSON strings (was QJsonObject)
    std::vector<std::vector<uint32_t>> m_tokenizedData;
    std::vector<std::vector<uint32_t>> m_trainingBatches;
    std::vector<std::vector<uint32_t>> m_validationBatches;

    AdamOptimizer m_optimizer;
    std::atomic<bool> m_isTraining{false};
    std::atomic<bool> m_shouldStop{false};
    int m_currentEpoch = 0;
    int m_totalEpochs = 0;
    float m_currentLoss = 0.0f;
    float m_validationPerplexity = 0.0f;
    std::string m_currentStatus = "Idle";

    std::thread m_trainingThread;  // OS thread, not QThread

    // Model metadata
    uint32_t m_vocabSize = 32000;
    uint32_t m_embeddingDim = 4096;
    uint32_t m_layerCount = 32;
    uint32_t m_sequenceLength = 512;

    // ===== Callback state =====
    void*                m_callbackCtx          = nullptr;
    TrainingStartedFn    m_onTrainingStarted    = nullptr;
    EpochStartedFn       m_onEpochStarted       = nullptr;
    BatchProcessedFn     m_onBatchProcessed     = nullptr;
    EpochCompletedFn     m_onEpochCompleted     = nullptr;
    TrainingCompletedFn  m_onTrainingCompleted  = nullptr;
    TrainingStoppedFn    m_onTrainingStopped    = nullptr;
    TrainingErrorFn      m_onTrainingError      = nullptr;
    LogMessageFn         m_onLogMessage         = nullptr;
    ValidationResultsFn  m_onValidationResults  = nullptr;
    ModelRegisteredFn    m_onModelRegistered    = nullptr;
};
