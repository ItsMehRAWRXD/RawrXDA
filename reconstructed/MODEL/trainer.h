#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QFile>
#include <memory>
#include <vector>
#include <string>

class GGUFLoader;
class InferenceEngine;

/**
* @class ModelTrainer
* @brief Production-ready on-device GGUF model fine-tuning
*
* Features:
* - Dataset ingestion (CSV, JSON-L, plain text)
* - Tokenization using existing InferenceEngine
* - Lightweight Adam optimizer
* - Gradient clipping and learning rate scheduling
* - Real-time progress reporting
* - Model validation and perplexity calculation
* - Thread-safe training execution
*/
class ModelTrainer : public QObject {
Q_OBJECT

public:
explicit ModelTrainer(QObject* parent = nullptr);
~ModelTrainer();

// Training configuration
struct TrainingConfig {
QString datasetPath;
QString outputPath;
int epochs = 3;
float learningRate = 1e-4f;
int batchSize = 4;
int sequenceLength = 512;
float gradientClip = 1.0f;
bool validateEveryEpoch = true;
QString validationSplit = "0.1"; // 10% for validation
};

// Dataset formats supported
enum class DatasetFormat {
PlainText,
JsonLines,
Csv
};

// Initialize with model and inference engine
bool initialize(InferenceEngine* engine, const QString& modelPath);

// Start training with configuration
bool startTraining(const TrainingConfig& config);

// Stop training (graceful shutdown)
void stopTraining();

// Get training status
bool isTraining() const { return m_isTraining; }
int getCurrentEpoch() const { return m_currentEpoch; }
int getTotalEpochs() const { return m_totalEpochs; }
float getCurrentLoss() const { return m_currentLoss; }
float getValidationPerplexity() const { return m_validationPerplexity; }

// Dataset utilities
DatasetFormat detectDatasetFormat(const QString& filePath);
bool loadDataset(const QString& filePath, DatasetFormat format);
std::vector<std::vector<uint32_t>> tokenizeDataset();

signals:
void trainingStarted();
void epochStarted(int epoch, int totalEpochs);
void batchProcessed(int batch, int totalBatches, float loss);
void epochCompleted(int epoch, float loss, float perplexity);
void trainingCompleted(const QString& outputPath, float finalPerplexity);
void trainingError(const QString& error);
void trainingStopped();
void logMessage(const QString& message);

private slots:
void runTraining();

private:
// Internal training methods
bool prepareTrainingData();
bool executeEpoch(int epoch);
bool processBatch(const std::vector<std::vector<uint32_t>>& batchData);
bool updateModelWeights(const std::vector<float>& gradients);
bool validateModel();
float calculatePerplexity();

// Data processing
std::vector<std::string> readPlainTextDataset(const QString& filePath);
std::vector<QJsonObject> readJsonLinesDataset(const QString& filePath);
std::vector<QJsonObject> readCsvDataset(const QString& filePath);

// Tokenization and batching
std::vector<uint32_t> tokenizeText(const std::string& text);
std::vector<std::vector<uint32_t>> createBatches(const std::vector<std::vector<uint32_t>>& tokenizedData);

// Optimizer (Adam)
struct AdamOptimizer {
float beta1 = 0.9f;
float beta2 = 0.999f;
float epsilon = 1e-8f;
std::vector<float> m; // First moment
std::vector<float> v; // Second moment
int t = 0; // Time step
};

// Model utilities
bool saveModel(const QString& outputPath);
bool backupOriginalModel();
std::vector<float> extractModelWeights();
bool applyWeightUpdates(const std::vector<float>& newWeights);

// Internal state
InferenceEngine* m_inferenceEngine = nullptr;
std::unique_ptr<GGUFLoader> m_modelLoader;
QString m_modelPath;
QString m_originalModelPath;

TrainingConfig m_config;
std::vector<std::string> m_textData;
std::vector<QJsonObject> m_jsonData;
std::vector<std::vector<uint32_t>> m_tokenizedData;
std::vector<std::vector<uint32_t>> m_trainingBatches;
std::vector<std::vector<uint32_t>> m_validationBatches;

AdamOptimizer m_optimizer;
bool m_isTraining = false;
bool m_shouldStop = false;
int m_currentEpoch = 0;
int m_totalEpochs = 0;
float m_currentLoss = 0.0f;
float m_validationPerplexity = 0.0f;

QThread* m_trainingThread = nullptr;
};