#include "model_trainer.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QThread>
#include <QElapsedTimer>
#include <algorithm>
#include <math>
#include <fstream>
#include <sstream>

ModelTrainer::ModelTrainer(QObject* parent)
    : QObject(parent)
    , m_isTraining(false)
    , m_shouldStop(false)
    , m_currentEpoch(0)
    , m_totalEpochs(0)
    , m_currentLoss(0.0f)
    , m_validationPerplexity(0.0f)
    , m_trainingThread(nullptr)
{
    qDebug() << "ModelTrainer initialized";
}

ModelTrainer::~ModelTrainer()
{
    stopTraining();
    if (m_trainingThread) {
        m_trainingThread->wait();
        delete m_trainingThread;
    }
    qDebug() << "ModelTrainer destroyed";
}

bool ModelTrainer::initialize(InferenceEngine* engine, const QString& modelPath)
{
    if (!engine) {
        emit trainingError("InferenceEngine is null");
        return false;
    }
    
    m_inferenceEngine = engine;
    m_modelPath = modelPath;
    
    // Backup original model
    if (!backupOriginalModel()) {
        emit trainingError("Failed to backup original model");
        return false;
    }
    
    emit logMessage("ModelTrainer initialized successfully");
    return true;
}

bool ModelTrainer::startTraining(const TrainingConfig& config)
{
    if (m_isTraining) {
        emit trainingError("Training already in progress");
        return false;
    }
    
    m_config = config;
    m_isTraining = true;
    m_shouldStop = false;
    
    // Load dataset
    DatasetFormat format = detectDatasetFormat(config.datasetPath);
    if (!loadDataset(config.datasetPath, format)) {
        emit trainingError("Failed to load dataset");
        m_isTraining = false;
        return false;
    }
    
    // Tokenize dataset
    m_tokenizedData = tokenizeDataset();
    if (m_tokenizedData.empty()) {
        emit trainingError("Failed to tokenize dataset");
        m_isTraining = false;
        return false;
    }
    
    // Prepare training data
    if (!prepareTrainingData()) {
        emit trainingError("Failed to prepare training data");
        m_isTraining = false;
        return false;
    }
    
    // Start training in separate thread
    m_trainingThread = new QThread();
    connect(m_trainingThread, &QThread::started, this, &ModelTrainer::runTraining);
    connect(this, &ModelTrainer::trainingStopped, m_trainingThread, &QThread::quit);
    
    this->moveToThread(m_trainingThread);
    m_trainingThread->start();
    
    emit trainingStarted();
    emit logMessage("Training started with " + QString::number(m_tokenizedData.size()) + " samples");
    
    return true;
}

void ModelTrainer::stopTraining()
{
    m_shouldStop = true;
    if (m_trainingThread && m_trainingThread->isRunning()) {
        m_trainingThread->quit();
        m_trainingThread->wait();
    }
    m_isTraining = false;
    emit trainingStopped();
    emit logMessage("Training stopped");
}

void ModelTrainer::runTraining()
{
    QElapsedTimer timer;
    timer.start();
    
    m_totalEpochs = m_config.epochs;
    
    for (m_currentEpoch = 0; m_currentEpoch < m_totalEpochs && !m_shouldStop; m_currentEpoch++) {
        emit epochStarted(m_currentEpoch + 1, m_totalEpochs);
        
        if (!executeEpoch(m_currentEpoch)) {
            emit trainingError("Failed to execute epoch " + QString::number(m_currentEpoch + 1));
            break;
        }
        
        // Validate if requested
        if (m_config.validateEveryEpoch) {
            if (!validateModel()) {
                emit trainingError("Validation failed for epoch " + QString::number(m_currentEpoch + 1));
                break;
            }
        }
        
        emit epochCompleted(m_currentEpoch + 1, m_currentLoss, m_validationPerplexity);
        
        emit logMessage(QString("Epoch %1/%2 completed - Loss: %3, Perplexity: %4")
                       .arg(m_currentEpoch + 1)
                       .arg(m_totalEpochs)
                       .arg(m_currentLoss, 0, 'f', 4)
                       .arg(m_validationPerplexity, 0, 'f', 4));
    }
    
    // Save final model
    if (!m_shouldStop) {
        if (saveModel(m_config.outputPath)) {
            emit trainingCompleted(m_config.outputPath, m_validationPerplexity);
            emit logMessage("Training completed successfully in " + 
                          QString::number(timer.elapsed() / 1000.0) + " seconds");
        } else {
            emit trainingError("Failed to save trained model");
        }
    }
    
    m_isTraining = false;
    emit trainingStopped();
}

ModelTrainer::DatasetFormat ModelTrainer::detectDatasetFormat(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return DatasetFormat::PlainText;
    }
    
    QTextStream stream(&file);
    QString firstLine = stream.readLine();
    file.close();
    
    if (firstLine.startsWith("{")) {
        return DatasetFormat::JsonLines;
    } else if (firstLine.contains(",")) {
        return DatasetFormat::Csv;
    }
    
    return DatasetFormat::PlainText;
}

bool ModelTrainer::loadDataset(const QString& filePath, DatasetFormat format)
{
    switch (format) {
    case DatasetFormat::PlainText:
        m_textData = readPlainTextDataset(filePath);
        return !m_textData.empty();
    case DatasetFormat::JsonLines:
        m_jsonData = readJsonLinesDataset(filePath);
        return !m_jsonData.empty();
    case DatasetFormat::Csv:
        m_jsonData = readCsvDataset(filePath);
        return !m_jsonData.empty();
    }
    return false;
}

std::vector<std::vector<uint32_t>> ModelTrainer::tokenizeDataset()
{
    std::vector<std::vector<uint32_t>> tokenized;
    
    // Tokenize text data
    for (const auto& text : m_textData) {
        std::vector<uint32_t> tokens = tokenizeText(text);
        if (!tokens.empty()) {
            tokenized.push_back(tokens);
        }
    }
    
    // Tokenize JSON data (extract text fields)
    for (const auto& json : m_jsonData) {
        QString text;
        if (json.contains("text")) {
            text = json["text"].toString();
        } else if (json.contains("content")) {
            text = json["content"].toString();
        } else if (json.contains("prompt")) {
            text = json["prompt"].toString();
        }
        
        if (!text.isEmpty()) {
            std::vector<uint32_t> tokens = tokenizeText(text.toStdString());
            if (!tokens.empty()) {
                tokenized.push_back(tokens);
            }
        }
    }
    
    return tokenized;
}

bool ModelTrainer::prepareTrainingData()
{
    if (m_tokenizedData.empty()) {
        return false;
    }
    
    // Split into training and validation
    int totalSamples = static_cast<int>(m_tokenizedData.size());
    int validationSize = static_cast<int>(totalSamples * m_config.validationSplit.toFloat());
    int trainingSize = totalSamples - validationSize;
    
    // Shuffle data
    std::random_shuffle(m_tokenizedData.begin(), m_tokenizedData.end());
    
    // Split
    m_trainingBatches = createBatches(std::vector<std::vector<uint32_t>>(
        m_tokenizedData.begin(), m_tokenizedData.begin() + trainingSize));
    
    if (validationSize > 0) {
        m_validationBatches = createBatches(std::vector<std::vector<uint32_t>>(
            m_tokenizedData.begin() + trainingSize, m_tokenizedData.end()));
    }
    
    emit logMessage(QString("Prepared %1 training batches and %2 validation batches")
                   .arg(m_trainingBatches.size())
                   .arg(m_validationBatches.size()));
    
    return true;
}

bool ModelTrainer::executeEpoch(int epoch)
{
    Q_UNUSED(epoch)
    
    float epochLoss = 0.0f;
    int batchCount = static_cast<int>(m_trainingBatches.size());
    
    for (int batchIdx = 0; batchIdx < batchCount && !m_shouldStop; batchIdx++) {
        float batchLoss = processBatch(m_trainingBatches[batchIdx]);
        epochLoss += batchLoss;
        
        emit batchProcessed(batchIdx + 1, batchCount, batchLoss);
        
        // Check for gradient clipping
        if (m_config.gradientClip > 0) {
            // Apply gradient clipping here
        }
    }
    
    if (batchCount > 0) {
        m_currentLoss = epochLoss / batchCount;
    }
    
    return !m_shouldStop;
}

float ModelTrainer::processBatch(const std::vector<std::vector<uint32_t>>& batchData)
{
    // Calculate gradients for this batch
    std::vector<float> gradients;
    
    // Simplified gradient calculation - in real implementation, this would use backpropagation
    float batchLoss = 0.0f;
    for (const auto& sequence : batchData) {
        // Calculate loss for each sequence
        batchLoss += calculateSequenceLoss(sequence);
    }
    
    if (!batchData.empty()) {
        batchLoss /= batchData.size();
    }
    
    // Update model weights using Adam optimizer
    if (!updateModelWeights(gradients)) {
        emit trainingError("Failed to update model weights");
    }
    
    return batchLoss;
}

bool ModelTrainer::updateModelWeights(const std::vector<float>& gradients)
{
    // Update Adam optimizer state
    m_optimizer.t++;
    
    // Apply weight updates using Adam algorithm
    // Simplified implementation - real implementation would update each weight
    
    return true;
}

bool ModelTrainer::validateModel()
{
    if (m_validationBatches.empty()) {
        m_validationPerplexity = 0.0f;
        return true;
    }
    
    float totalPerplexity = 0.0f;
    for (const auto& batch : m_validationBatches) {
        totalPerplexity += calculatePerplexity();
    }
    
    m_validationPerplexity = totalPerplexity / m_validationBatches.size();
    return true;
}

float ModelTrainer::calculatePerplexity()
{
    // Simplified perplexity calculation
    // Real implementation would calculate cross-entropy loss and exponentiate
    return std::exp(m_currentLoss);
}

float ModelTrainer::calculateSequenceLoss(const std::vector<uint32_t>& sequence)
{
    // Simplified loss calculation
    // Real implementation would use cross-entropy loss
    return 1.0f;
}

std::vector<std::string> ModelTrainer::readPlainTextDataset(const QString& filePath)
{
    std::vector<std::string> lines;
    QFile file(filePath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (!line.isEmpty()) {
                lines.push_back(line.toStdString());
            }
        }
        file.close();
    }
    
    return lines;
}

std::vector<QJsonObject> ModelTrainer::readJsonLinesDataset(const QString& filePath)
{
    std::vector<QJsonObject> objects;
    QFile file(filePath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (!line.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    objects.push_back(doc.object());
                }
            }
        }
        file.close();
    }
    
    return objects;
}

std::vector<QJsonObject> ModelTrainer::readCsvDataset(const QString& filePath)
{
    std::vector<QJsonObject> objects;
    QFile file(filePath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        QString headerLine = stream.readLine();
        QStringList headers = headerLine.split(",");
        
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (!line.isEmpty()) {
                QStringList values = line.split(",");
                QJsonObject obj;
                
                for (int i = 0; i < headers.size() && i < values.size(); i++) {
                    obj[headers[i]] = values[i];
                }
                
                objects.push_back(obj);
            }
        }
        file.close();
    }
    
    return objects;
}

std::vector<uint32_t> ModelTrainer::tokenizeText(const std::string& text)
{
    // Simplified tokenization - real implementation would use InferenceEngine
    std::vector<uint32_t> tokens;
    
    // Basic whitespace tokenization
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        // Simple hash-based token ID
        uint32_t tokenId = std::hash<std::string>{}(word) % 10000;
        tokens.push_back(tokenId);
    }
    
    return tokens;
}

std::vector<std::vector<uint32_t>> ModelTrainer::createBatches(const std::vector<std::vector<uint32_t>>& tokenizedData)
{
    std::vector<std::vector<uint32_t>> batches;
    
    for (const auto& sequence : tokenizedData) {
        // Split sequences into batches of specified length
        for (size_t i = 0; i < sequence.size(); i += m_config.sequenceLength) {
            size_t end = std::min(i + m_config.sequenceLength, sequence.size());
            std::vector<uint32_t> batch(sequence.begin() + i, sequence.begin() + end);
            batches.push_back(batch);
        }
    }
    
    return batches;
}

bool ModelTrainer::saveModel(const QString& outputPath)
{
    // Simplified model saving - real implementation would save GGUF format
    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("Trained model data");
        file.close();
        return true;
    }
    return false;
}

bool ModelTrainer::backupOriginalModel()
{
    QString backupPath = m_modelPath + ".backup";
    return QFile::copy(m_modelPath, backupPath);
}

std::vector<float> ModelTrainer::extractModelWeights()
{
    // Simplified weight extraction
    return std::vector<float>();
}

bool ModelTrainer::applyWeightUpdates(const std::vector<float>& newWeights)
{
    // Simplified weight application
    return true;
}