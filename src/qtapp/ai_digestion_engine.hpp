#ifndef AI_DIGESTION_ENGINE_HPP
#define AI_DIGESTION_ENGINE_HPP

#include <memory>
#include <vector>

// Forward declarations
struct DigestionConfig;
struct KnowledgeRepresentation;
struct AIDigestionDataset;

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
    std::stringList inputPaths;
    std::string outputDirectory;
    std::string modelName;
    ExtractionMode mode;
    int maxTokens;
    int chunkSize;
    int overlapSize;
    double learningRate;
    int epochs;
    std::string quantization;  // Q4_0, Q4_1, Q5_0, Q5_1, Q8_0, F16, F32
    int modelSizeGB;
    bool enableSpecialization;
    std::stringList focusDomains;
    bool preserveStructure;
    bool extractComments;
    bool extractFunctions;
    bool extractClasses;
    bool extractVariables;
    int minContentLength;
    std::stringList fileExtensions;
    std::stringList includePatterns;
    std::stringList excludePatterns;
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
    std::string id;
    std::string content;
    std::string originalFile;
    FileType fileType;
    nlohmann::json metadata;
    std::stringList tokens;
    std::stringList keywords;
    std::stringList functions;
    std::stringList classes;
    std::stringList variables;
    std::stringList comments;
    std::map<std::string, double> semanticWeights;
    // DateTime timestamp;
    int contextWindow;
    
    // Serialization methods
    nlohmann::json toJson() const;
    static KnowledgeRepresentation fromJson(const nlohmann::json& obj);
};

// Structure for training dataset
struct AIDigestionDataset {
    std::string name;
    std::vector<KnowledgeRepresentation> samples;
    nlohmann::json statistics;
    // DateTime created;
    std::string description;
    int totalTokens;
    int totalSamples;
    std::map<FileType, int> fileTypeCounts;
    std::stringList domains;
    
    // Serialization methods
    nlohmann::json toJson() const;
    static AIDigestionDataset fromJson(const nlohmann::json& obj);
};

// Main AI Digestion Engine class
class AIDigestionEngine  {public:
    explicit AIDigestionEngine( = nullptr);
    virtual ~AIDigestionEngine();

    // Configuration methods
    void setConfig(const DigestionConfig& config);
    DigestionConfig getConfig() const;
    
    // Main digestion methods
    void startDigestion(const std::stringList& inputPaths);
    void stopDigestion();
    bool isDigesting() const;
    
    // Progress and status
    double getProgress() const;
    std::string getStatusMessage() const;
    int getProcessedFiles() const;
    int getTotalFiles() const;
    
    // Knowledge base management
    AIDigestionDataset getAIDigestionDataset() const;
    bool saveDataset(const std::string& filePath) const;
    bool loadDataset(const std::string& filePath);
    
    // Content processing methods
    static FileType detectFileType(const std::string& filePath);
    static std::string extractFileContent(const std::string& filePath);
    
    // Training methods
    void startTraining();
    void stopTraining();
    bool isTraining() const;
    \npublic:\n    // Progress signals
    void progressChanged(double progress);
    void statusChanged(const std::string& status);
    void fileProcessed(const std::string& filePath, int processedCount, int totalCount);
    
    // Completion signals
    void digestionCompleted(const AIDigestionDataset& dataset);
    void digestionFailed(const std::string& error);
    void trainingCompleted(const std::string& modelPath);
    void trainingFailed(const std::string& error);
    
    // Knowledge extraction signals
    void knowledgeExtracted(const KnowledgeRepresentation& knowledge);
    void datasetUpdated(int totalSamples, int totalTokens);
\npublic:\n    void pauseDigestion();
    void resumeDigestion();
    void clearDataset();
    void processFile(const std::string& filePath);
\nprivate:\n    void processFileInternal(const std::string& filePath);
    void onDigestionThreadFinished();
    void onTrainingThreadFinished();

private:
    // Core processing methods
    void initializeEngine();
    void processDirectory(const std::string& dirPath);
    
    // Content analysis methods
    KnowledgeRepresentation analyzeContent(const std::string& content, const std::string& filePath);
    std::stringList tokenizeContent(const std::string& content, FileType type);
    std::stringList extractKeywords(const std::string& content, FileType type);
    std::stringList extractFunctions(const std::string& content, FileType type);
    std::stringList extractClasses(const std::string& content, FileType type);
    std::stringList extractVariables(const std::string& content, FileType type);
    std::stringList extractComments(const std::string& content, FileType type);
    std::map<std::string, double> calculateSemanticWeights(const std::stringList& tokens);
    
    // Specialized extractors for different file types
    KnowledgeRepresentation extractFromSourceCode(const std::string& content, const std::string& filePath);
    KnowledgeRepresentation extractFromDocumentation(const std::string& content, const std::string& filePath);
    KnowledgeRepresentation extractFromAssembly(const std::string& content, const std::string& filePath);
    KnowledgeRepresentation extractFromCPlusPlus(const std::string& content, const std::string& filePath);
    KnowledgeRepresentation extractFromPython(const std::string& content, const std::string& filePath);
    
    // Chunking and preprocessing
    std::stringList chunkContent(const std::string& content, int chunkSize, int overlapSize);
    std::string preprocessContent(const std::string& content, FileType type);
    double calculateComplexityScore(const std::string& content);
    int calculateIndentationComplexity(const std::string& content);
    void initialize();
    
    // Training data preparation
    void prepareTrainingData();
    nlohmann::json generateTrainingPrompts(const KnowledgeRepresentation& knowledge);
    
    // Model training methods
    void executeTraining();
    bool createAITrainingArchitecture();
    bool trainModelFromDataset();
    bool quantizeModel(const std::string& inputPath, const std::string& outputPath);
    
    // Utility methods
    std::string generateUniqueId();
    bool shouldProcessFile(const std::string& filePath);
    std::string sanitizeContent(const std::string& content);
    nlohmann::json generateStatistics();

private:
    DigestionConfig m_config;
    AIDigestionDataset m_dataset;
    
    // Threading and control
    std::unique_ptr<std::thread> m_digestionThread;
    std::unique_ptr<std::thread> m_trainingThread;
    mutable std::mutex m_mutex;
    std::condition_variable m_pauseCondition;
    
    // State management
    bool m_isDigesting;
    bool m_isTraining;
    bool m_isPaused;
    bool m_shouldStop;
    
    // Progress tracking
    double m_progress;
    std::string m_statusMessage;
    int m_processedFiles;
    int m_totalFiles;
    
    // File processing
    std::stringList m_filesToProcess;
    std::map<std::string, FileType> m_fileTypeCache;
    // Timer m_progressTimer;
    
    // Knowledge storage
    std::vector<KnowledgeRepresentation> m_knowledgeBase;
    std::map<std::string, int> m_vocabularyMap;
    std::stringList m_vocabulary;
    
    // Statistics
    std::map<FileType, int> m_fileTypeStats;
    std::map<std::string, int> m_domainStats;
    // DateTime m_startTime;
    
    // Model training data
    std::string m_modelOutputPath;
    nlohmann::json m_trainingConfig;
    QRandomGenerator m_randomGenerator;
};

// Helper class for background digestion
class DigestionWorker  {public:
    explicit DigestionWorker(AIDigestionEngine* engine);
    \npublic:\n    void processFiles(const std::stringList& files, const DigestionConfig& config);
\npublic:\n    void fileProcessed(const std::string& filePath);
    void finished();
    void error(const std::string& message);

private:
    AIDigestionEngine* m_engine;
};

// Helper class for background training
class TrainingWorker  {public:
    explicit TrainingWorker(AIDigestionEngine* engine);
    \npublic:\n    void startTraining(const AIDigestionDataset& dataset, const DigestionConfig& config);
\npublic:\n    void trainingProgress(double progress);
    void finished(const std::string& modelPath);
    void error(const std::string& message);

private:
    AIDigestionEngine* m_engine;
    
    // Training methods
    bool prepareTrainingEnvironment();
    bool executeTrainingPipeline();
    bool validateTrainingResults();
};

#endif // AI_DIGESTION_ENGINE_HPP



