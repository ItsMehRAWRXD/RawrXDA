#ifndef AI_DIGESTION_ENGINE_HPP
#define AI_DIGESTION_ENGINE_HPP

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QRandomGenerator>
#include <QtConcurrent/QtConcurrent>
#include <memory>
#include <vector>

// Forward declarations
struct DigestionConfig;
struct KnowledgeRepresentation;
struct TrainingDataset;

// Enumeration for supported file types
enum class FileType {
    SourceCode,
    Documentation,
    PlainText,
    Markdown,
    Assembly,
    CPlusPlus,
    C,
    Python,
    JavaScript,
    HTML,
    XML,
    JSON,
    Binary,
    Configuration,
    Unknown
};

// Enumeration for knowledge extraction modes
enum class ExtractionMode {
    Syntactic,      // Focus on syntax and structure
    Semantic,       // Focus on meaning and context
    Functional,     // Focus on functionality and behavior
    Comprehensive   // All of the above
};

// Structure for digestion configuration
struct DigestionConfig {
    QStringList inputPaths;
    QString outputDirectory;
    QString modelName;
    ExtractionMode mode;
    int maxTokens;
    int chunkSize;
    int overlapSize;
    double learningRate;
    int epochs;
    QString quantization;  // Q4_0, Q4_1, Q5_0, Q5_1, Q8_0, F16, F32
    int modelSizeGB;
    bool enableSpecialization;
    QStringList focusDomains;
    bool preserveStructure;
    bool extractComments;
    bool extractFunctions;
    bool extractClasses;
    bool extractVariables;
    int minContentLength;
    QStringList fileExtensions;
    QStringList includePatterns;
    QStringList excludePatterns;
    long long maxFileSize;
    
    DigestionConfig() 
        : mode(ExtractionMode::Comprehensive)
        , maxTokens(2048)
        , chunkSize(512)
        , overlapSize(64)
        , learningRate(0.0001)
        , epochs(10)
        , quantization("Q4_0")
        , modelSizeGB(7)
        , enableSpecialization(true)
        , preserveStructure(true)
        , extractComments(true)
        , extractFunctions(true)
        , extractClasses(true)
        , extractVariables(true)
        , minContentLength(50)
        , maxFileSize(10 * 1024 * 1024)
    {}
};

// Structure for knowledge representation
struct KnowledgeRepresentation {
    QString id;
    QString content;
    QString originalFile;
    FileType fileType;
    QJsonObject metadata;
    QStringList tokens;
    QStringList keywords;
    QStringList functions;
    QStringList classes;
    QStringList variables;
    QStringList comments;
    QHash<QString, double> semanticWeights;
    QDateTime timestamp;
    int contextWindow;
    
    // Serialization methods
    QJsonObject toJson() const;
    static KnowledgeRepresentation fromJson(const QJsonObject& obj);
};

// Structure for training dataset
struct TrainingDataset {
    QString name;
    QVector<KnowledgeRepresentation> samples;
    QJsonObject statistics;
    QDateTime created;
    QString description;
    int totalTokens;
    int totalSamples;
    QHash<FileType, int> fileTypeCounts;
    QStringList domains;
    
    // Serialization methods
    QJsonObject toJson() const;
    static TrainingDataset fromJson(const QJsonObject& obj);
};

// Main AI Digestion Engine class
class AIDigestionEngine : public QObject {
    Q_OBJECT

public:
    explicit AIDigestionEngine(QObject* parent = nullptr);
    virtual ~AIDigestionEngine();

    // Configuration methods
    void setConfig(const DigestionConfig& config);
    DigestionConfig getConfig() const;
    
    // Main digestion methods
    void startDigestion(const QStringList& inputPaths);
    void stopDigestion();
    bool isDigesting() const;
    
    // Progress and status
    double getProgress() const;
    QString getStatusMessage() const;
    int getProcessedFiles() const;
    int getTotalFiles() const;
    
    // Knowledge base management
    TrainingDataset getTrainingDataset() const;
    bool saveDataset(const QString& filePath) const;
    bool loadDataset(const QString& filePath);
    
    // Content processing methods
    static FileType detectFileType(const QString& filePath);
    static QString extractFileContent(const QString& filePath);
    
    // Training methods
    void startTraining();
    void stopTraining();
    bool isTraining() const;
    
signals:
    // Progress signals
    void progressChanged(double progress);
    void statusChanged(const QString& status);
    void fileProcessed(const QString& filePath, int processedCount, int totalCount);
    
    // Completion signals
    void digestionCompleted(const TrainingDataset& dataset);
    void digestionFailed(const QString& error);
    void trainingCompleted(const QString& modelPath);
    void trainingFailed(const QString& error);
    
    // Knowledge extraction signals
    void knowledgeExtracted(const KnowledgeRepresentation& knowledge);
    void datasetUpdated(int totalSamples, int totalTokens);

public slots:
    void pauseDigestion();
    void resumeDigestion();
    void clearDataset();
    void processFile(const QString& filePath);

private slots:
    void processFileInternal(const QString& filePath);
    void onDigestionThreadFinished();
    void onTrainingThreadFinished();

private:
    // Core processing methods
    void initializeEngine();
    void processDirectory(const QString& dirPath);
    
    // Content analysis methods
    KnowledgeRepresentation analyzeContent(const QString& content, const QString& filePath);
    QStringList tokenizeContent(const QString& content, FileType type);
    QStringList extractKeywords(const QString& content, FileType type);
    QStringList extractFunctions(const QString& content, FileType type);
    QStringList extractClasses(const QString& content, FileType type);
    QStringList extractVariables(const QString& content, FileType type);
    QStringList extractComments(const QString& content, FileType type);
    QHash<QString, double> calculateSemanticWeights(const QStringList& tokens);
    
    // Specialized extractors for different file types
    KnowledgeRepresentation extractFromSourceCode(const QString& content, const QString& filePath);
    KnowledgeRepresentation extractFromDocumentation(const QString& content, const QString& filePath);
    KnowledgeRepresentation extractFromAssembly(const QString& content, const QString& filePath);
    KnowledgeRepresentation extractFromCPlusPlus(const QString& content, const QString& filePath);
    KnowledgeRepresentation extractFromPython(const QString& content, const QString& filePath);
    
    // Chunking and preprocessing
    QStringList chunkContent(const QString& content, int chunkSize, int overlapSize);
    QString preprocessContent(const QString& content, FileType type);
    double calculateComplexityScore(const QString& content);
    int calculateIndentationComplexity(const QString& content);
    void initialize();
    
    // Training data preparation
    void prepareTrainingData();
    QJsonArray generateTrainingPrompts(const KnowledgeRepresentation& knowledge);
    
    // Model training methods
    void executeTraining();
    bool createModelArchitecture();
    bool trainModelFromDataset();
    bool quantizeModel(const QString& inputPath, const QString& outputPath);
    
    // Utility methods
    QString generateUniqueId();
    bool shouldProcessFile(const QString& filePath);
    QString sanitizeContent(const QString& content);
    QJsonObject generateStatistics();

private:
    DigestionConfig m_config;
    TrainingDataset m_dataset;
    
    // Threading and control
    std::unique_ptr<QThread> m_digestionThread;
    std::unique_ptr<QThread> m_trainingThread;
    mutable QMutex m_mutex;
    QWaitCondition m_pauseCondition;
    
    // State management
    bool m_isDigesting;
    bool m_isTraining;
    bool m_isPaused;
    bool m_shouldStop;
    
    // Progress tracking
    double m_progress;
    QString m_statusMessage;
    int m_processedFiles;
    int m_totalFiles;
    
    // File processing
    QStringList m_filesToProcess;
    QHash<QString, FileType> m_fileTypeCache;
    QTimer* m_progressTimer;
    
    // Knowledge storage
    QVector<KnowledgeRepresentation> m_knowledgeBase;
    QHash<QString, int> m_vocabularyMap;
    QStringList m_vocabulary;
    
    // Statistics
    QHash<FileType, int> m_fileTypeStats;
    QHash<QString, int> m_domainStats;
    QDateTime m_startTime;
    
    // Model training data
    QString m_modelOutputPath;
    QJsonObject m_trainingConfig;
    QRandomGenerator m_randomGenerator;
};

// Helper class for background digestion
class DigestionWorker : public QObject {
    Q_OBJECT

public:
    explicit DigestionWorker(AIDigestionEngine* engine, QObject* parent = nullptr);
    
public slots:
    void processFiles(const QStringList& files, const DigestionConfig& config);

signals:
    void fileProcessed(const QString& filePath);
    void finished();
    void error(const QString& message);

private:
    AIDigestionEngine* m_engine;
};

// Helper class for background training
class TrainingWorker : public QObject {
    Q_OBJECT

public:
    explicit TrainingWorker(AIDigestionEngine* engine, QObject* parent = nullptr);
    
public slots:
    void startTraining(const TrainingDataset& dataset, const DigestionConfig& config);

signals:
    void trainingProgress(double progress);
    void finished(const QString& modelPath);
    void error(const QString& message);

private:
    AIDigestionEngine* m_engine;
    
    // Training methods
    bool prepareTrainingEnvironment();
    bool executeTrainingPipeline();
    bool validateTrainingResults();
};

#endif // AI_DIGESTION_ENGINE_HPP