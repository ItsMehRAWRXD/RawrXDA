#ifndef AI_TRAINING_PIPELINE_HPP
#define AI_TRAINING_PIPELINE_HPP

#include "ai_digestion_engine.hpp"
#include <memory>

// Forward declarations
class LlamaTrainer;
class ModelQuantizer;
class TrainingValidator;

// Enumeration for training backends
enum class TrainingBackend {
    LlamaCpp,       // llama.cpp based training
    Transformers,   // HuggingFace transformers
    Custom,         // Custom implementation
    OpenAI,         // OpenAI fine-tuning
    Ollama          // Ollama training
};

// Structure for model architecture configuration
struct AITrainingArchitecture {
    std::string baseModel;          // Base model to fine-tune from
    int vocabularySize;
    int hiddenSize;
    int numLayers;
    int numAttentionHeads;
    int intermediateSize;
    int maxPositionEmbeddings;
    std::string activationFunction;
    double dropoutRate;
    double attentionDropout;
    std::string normalizationType;
    
    AITrainingArchitecture() 
        : vocabularySize(32000)
        , hiddenSize(4096)
        , numLayers(32)
        , numAttentionHeads(32)
        , intermediateSize(11008)
        , maxPositionEmbeddings(2048)
        , activationFunction("silu")
        , dropoutRate(0.0)
        , attentionDropout(0.0)
        , normalizationType("RMSNorm")
    {}
};

// Structure for training hyperparameters
struct TrainingHyperparameters {
    double learningRate;
    double weightDecay;
    double beta1;
    double beta2;
    double epsilon;
    int batchSize;
    int gradientAccumulationSteps;
    int maxGradientNorm;
    double warmupRatio;
    std::string scheduler;
    int saveSteps;
    int evaluationSteps;
    int loggingSteps;
    bool useGradientCheckpointing;
    bool useFp16;
    bool useBf16;
    
    TrainingHyperparameters()
        : learningRate(5e-5)
        , weightDecay(0.0)
        , beta1(0.9)
        , beta2(0.999)
        , epsilon(1e-8)
        , batchSize(4)
        , gradientAccumulationSteps(8)
        , maxGradientNorm(1.0)
        , warmupRatio(0.03)
        , scheduler("cosine")
        , saveSteps(500)
        , evaluationSteps(500)
        , loggingSteps(10)
        , useGradientCheckpointing(true)
        , useFp16(false)
        , useBf16(true)
    {}
};

// Main training pipeline class
class AITrainingPipeline  {public:
    explicit AITrainingPipeline( = nullptr);
    virtual ~AITrainingPipeline();

    // Configuration methods
    void setBackend(TrainingBackend backend);
    TrainingBackend getBackend() const;
    
    void setArchitecture(const AITrainingArchitecture& arch);
    AITrainingArchitecture getArchitecture() const;
    
    void setHyperparameters(const TrainingHyperparameters& params);
    TrainingHyperparameters getHyperparameters() const;

    // Training methods
    bool prepareTraining(const AIDigestionDataset& dataset, const DigestionConfig& config);
    void startTraining();
    void stopTraining();
    void pauseTraining();
    void resumeTraining();
    
    // Status methods
    bool isTraining() const;
    bool isPaused() const;
    double getTrainingProgress() const;
    std::string getTrainingStatus() const;
    nlohmann::json getTrainingMetrics() const;
    
    // Model management
    std::string getModelOutputPath() const;
    bool quantizeModel(const std::string& inputPath, const std::string& outputPath, const std::string& quantization);
    bool validateModel(const std::string& modelPath);
    bool testModel(const std::string& modelPath, const std::stringList& testPrompts);
\npublic:\n    // Training progress
    void trainingStarted();
    void trainingProgress(double progress, const nlohmann::json& metrics);
    void trainingPaused();
    void trainingResumed();
    void trainingCompleted(const std::string& modelPath);
    void trainingFailed(const std::string& error);
    
    // Epoch and step events
    void epochStarted(int epoch, int totalEpochs);
    void epochCompleted(int epoch, double loss, double accuracy);
    void stepCompleted(int step, int totalSteps, double loss);
    
    // Model events
    void modelSaved(const std::string& path);
    void modelQuantized(const std::string& path, const std::string& quantization);
    void modelValidated(const std::string& path, bool isValid);
\npublic:\n    void onTrainingTimeout();
    void onModelCheckpoint();
\nprivate:\n    void handleTrainingOutput();
    void handleTrainingError();
    void handleTrainingFinished();

private:
    // Core training methods
    bool setupTrainingEnvironment();
    bool prepareTrainingData(const AIDigestionDataset& dataset);
    bool generateTrainingScript();
    bool executeTraining();
    void cleanupTraining();
    
    // Backend-specific methods
    bool setupLlamaCppTraining();
    bool setupTransformersTraining();
    bool setupCustomTraining();
    bool setupOllamaTraining();
    
    // Data preparation methods
    bool createDatasetFiles(const AIDigestionDataset& dataset);
    bool generateTokenizerFiles();
    bool createConfigFiles();
    
    // Model creation methods
    bool initializeModelWeights();
    bool createTrainingLoop();
    bool implementValidationLoop();
    bool saveModelCheckpoints();
    
    // Utility methods
    std::string generateRequirementsFile();
    bool installDependencies();
    bool checkGPUAvailability();
    std::string detectOptimalQuantization();
    
    // Monitoring methods
    void parseTrainingLogs();
    void updateTrainingMetrics();
    void saveTrainingState();

private:
    TrainingBackend m_backend;
    AITrainingArchitecture m_architecture;
    TrainingHyperparameters m_hyperparameters;
    
    // Training state
    bool m_isTraining;
    bool m_isPaused;
    double m_progress;
    std::string m_status;
    nlohmann::json m_metrics;
    
    // Training environment
    std::unique_ptr<QTemporaryDir> m_workingDir;
    std::string m_modelOutputPath;
    std::string m_datasetPath;
    std::string m_configPath;
    std::string m_scriptPath;
    
    // Training process
    std::unique_ptr<void*> m_trainingProcess;
    // Timer m_progressTimer;
    // Timer m_checkpointTimer;
    
    // Training data
    AIDigestionDataset m_dataset;
    DigestionConfig m_config;
    
    // Network for downloading models/dependencies
    void** m_networkManager;
    
    // Training components
    std::unique_ptr<LlamaTrainer> m_llamaTrainer;
    std::unique_ptr<ModelQuantizer> m_quantizer;
    std::unique_ptr<TrainingValidator> m_validator;
    
    // Training statistics
    int m_currentEpoch;
    int m_totalEpochs;
    int m_currentStep;
    int m_totalSteps;
    double m_currentLoss;
    double m_currentAccuracy;
    double m_bestLoss;
    // DateTime m_trainingStartTime;
    
    // Model configuration
    std::string m_baseModelPath;
    std::string m_tokenizerPath;
    std::stringList m_requiredFiles;
};

// LlamaTrainer for llama.cpp backend
class LlamaTrainer  {public:
    explicit LlamaTrainer( = nullptr);
    
    bool prepareTraining(const std::string& datasetPath, const AITrainingArchitecture& arch);
    bool startTraining(const TrainingHyperparameters& params);
    bool quantizeModel(const std::string& inputPath, const std::string& outputPath, const std::string& quantization);
    \npublic:\n    void trainingProgress(double progress);
    void trainingCompleted(const std::string& modelPath);
    void trainingFailed(const std::string& error);

private:
    bool downloadLlamaCpp();
    bool buildLlamaCpp();
    bool prepareLlamaData();
    bool executeLlamaTraining();
    
private:
    std::string m_llamaCppPath;
    std::string m_workingDir;
    std::unique_ptr<void*> m_process;
};

// ModelQuantizer for model compression
class ModelQuantizer  {public:
    explicit ModelQuantizer( = nullptr);
    
    bool quantizeModel(const std::string& inputPath, const std::string& outputPath, const std::string& quantization);
    std::stringList getSupportedQuantizations() const;
    double estimateQuantizedSize(const std::string& modelPath, const std::string& quantization);
\npublic:\n    void quantizationProgress(double progress);
    void quantizationCompleted(const std::string& outputPath);
    void quantizationFailed(const std::string& error);

private:
    bool setupQuantizationEnvironment();
    bool executeQuantization(const std::string& inputPath, const std::string& outputPath, const std::string& quantization);
    bool validateQuantizedModel(const std::string& modelPath);
    
private:
    std::unique_ptr<void*> m_process;
    std::string m_quantizerPath;
};

// TrainingValidator for model validation
class TrainingValidator  {public:
    explicit TrainingValidator( = nullptr);
    
    bool validateModel(const std::string& modelPath);
    bool testModel(const std::string& modelPath, const std::stringList& testPrompts);
    nlohmann::json getValidationResults() const;
\npublic:\n    void validationProgress(double progress);
    void validationCompleted(bool isValid, const nlohmann::json& results);

private:
    bool checkModelFormat(const std::string& modelPath);
    bool validateModelWeights(const std::string& modelPath);
    bool testModelInference(const std::string& modelPath, const std::stringList& prompts);
    double calculatePerplexity(const std::string& modelPath, const std::stringList& testData);
    
private:
    nlohmann::json m_validationResults;
    std::unique_ptr<void*> m_testProcess;
};

#endif // AI_TRAINING_PIPELINE_HPP





