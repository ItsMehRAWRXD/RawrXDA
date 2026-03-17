#pragma once

/**
 * @file custom_model_builder.h
 * @brief Complete custom model building system from scratch
 * 
 * Features:
 * - File digestion from any source (code, docs, text, topics)
 * - Tokenization and vocabulary building
 * - Model training from ground up
 * - GGUF format export (Ollama-compatible)
 * - Custom inference engine
 * - Full CLI and GUI integration
 * - Works exactly like Ollama API but fully custom
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <filesystem>
#include <mutex>
#include <future>
#include <chrono>

namespace CustomModelBuilder {

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

/**
 * @brief Source types for model building
 */
enum class SourceType {
    CODE_FILE,          // .cpp, .py, .js, etc.
    DOCUMENTATION,      // .md, .txt, .doc
    CONVERSATION,       // chat logs, Q&A pairs
    TOPIC_DATA,         // domain-specific knowledge
    MIXED               // combined sources
};

/**
 * @brief Training data sample
 */
struct TrainingSample {
    std::string text;
    std::string context;
    SourceType sourceType;
    std::map<std::string, std::string> metadata;
    float weight = 1.0f;  // Importance weight
};

/**
 * @brief Tokenizer vocabulary
 */
struct Vocabulary {
    std::map<std::string, uint32_t> tokenToId;
    std::map<uint32_t, std::string> idToToken;
    uint32_t vocabSize = 0;
    uint32_t padToken = 0;
    uint32_t bosToken = 1;
    uint32_t eosToken = 2;
    uint32_t unkToken = 3;
};

/**
 * @brief Model architecture configuration
 */
struct ModelArchitecture {
    uint32_t vocabSize = 32000;
    uint32_t embeddingDim = 512;
    uint32_t numLayers = 6;
    uint32_t numHeads = 8;
    uint32_t hiddenDim = 2048;
    uint32_t maxSeqLen = 2048;
    float dropoutRate = 0.1f;
    std::string activationType = "gelu";
    bool useRoPE = true;  // Rotary Position Embeddings
    bool useGQA = false;  // Grouped Query Attention
};

/**
 * @brief Training hyperparameters
 */
struct TrainingConfig {
    int epochs = 10;
    int batchSize = 8;
    float learningRate = 3e-4f;
    float weightDecay = 0.01f;
    float gradientClipNorm = 1.0f;
    int warmupSteps = 1000;
    int saveEverySteps = 1000;
    int evalEverySteps = 500;
    bool useAdamW = true;
    bool useMixedPrecision = false;
    std::string checkpointDir = "./checkpoints";
};

/**
 * @brief Model metadata for registry
 */
struct CustomModelMetadata {
    std::string modelName;
    std::string modelId;
    std::string version;
    std::string description;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastModified;
    size_t modelSizeBytes;
    size_t parametersCount;
    std::vector<std::string> sourceFiles;
    std::map<std::string, std::string> trainingInfo;
    ModelArchitecture architecture;
    float trainingLoss;
    float validationPerplexity;
    std::string ggufPath;
    bool isOllamaCompatible = true;
};

// ============================================================================
// FILE DIGESTION ENGINE
// ============================================================================

/**
 * @brief Processes files and extracts training data
 */
class FileDigestionEngine {
public:
    FileDigestionEngine();
    ~FileDigestionEngine();
    
    // Add source files or directories
    void addSource(const std::string& path, SourceType type);
    void addSourceDirectory(const std::string& dirPath, bool recursive = true);
    
    // Configure digestion
    void setChunkSize(size_t size);  // Split long files into chunks
    void setOverlap(size_t overlap);  // Overlap between chunks
    void enableContextExtraction(bool enable);
    void setLanguageHints(const std::vector<std::string>& languages);
    
    // Process sources
    std::vector<TrainingSample> digestAll();
    std::vector<TrainingSample> digestFile(const std::string& path, SourceType type);
    
    // Specialized processors
    std::vector<TrainingSample> processCodeFile(const std::string& path);
    std::vector<TrainingSample> processDocumentation(const std::string& path);
    std::vector<TrainingSample> processConversation(const std::string& path);
    std::vector<TrainingSample> processTopic(const std::string& topic, const std::string& data);
    
    // Statistics
    size_t getTotalSamples() const { return totalSamples_; }
    size_t getTotalTokens() const { return totalTokens_; }
    
private:
    struct SourceEntry {
        std::string path;
        SourceType type;
    };
    
    std::vector<SourceEntry> sources_;
    size_t chunkSize_ = 512;
    size_t overlap_ = 64;
    bool extractContext_ = true;
    std::vector<std::string> languageHints_;
    size_t totalSamples_ = 0;
    size_t totalTokens_ = 0;
    
    // Helper methods
    std::string detectLanguage(const std::string& path);
    std::vector<std::string> splitIntoChunks(const std::string& text);
    std::string extractContextInfo(const std::string& filePath, const std::string& chunk);
};

// ============================================================================
// CUSTOM TOKENIZER
// ============================================================================

/**
 * @brief Builds and uses custom vocabulary
 */
class CustomTokenizer {
public:
    CustomTokenizer();
    ~CustomTokenizer();
    
    // Build vocabulary from samples
    void buildVocabulary(const std::vector<TrainingSample>& samples, uint32_t maxVocabSize = 32000);
    void buildVocabularyBPE(const std::vector<TrainingSample>& samples, uint32_t merges = 10000);
    
    // Save/load vocabulary
    bool saveVocabulary(const std::string& path);
    bool loadVocabulary(const std::string& path);
    
    // Tokenization
    std::vector<uint32_t> encode(const std::string& text);
    std::string decode(const std::vector<uint32_t>& tokens);
    
    // Vocabulary access
    const Vocabulary& getVocabulary() const { return vocab_; }
    uint32_t getVocabSize() const { return vocab_.vocabSize; }
    
private:
    Vocabulary vocab_;
    std::map<std::string, uint32_t> bpeMerges_;
    
    // BPE helpers
    std::vector<std::string> splitIntoBPETokens(const std::string& text);
    void applyBPEMerges(std::vector<std::string>& tokens);
};

// ============================================================================
// MODEL TRAINER
// ============================================================================

/**
 * @brief Trains transformer model from scratch
 */
class CustomModelTrainer {
public:
    CustomModelTrainer();
    ~CustomModelTrainer();
    
    // Configuration
    void setArchitecture(const ModelArchitecture& arch);
    void setTrainingConfig(const TrainingConfig& config);
    void setVocabulary(const Vocabulary& vocab);
    
    // Training data
    void setTrainingData(const std::vector<TrainingSample>& samples);
    void setValidationData(const std::vector<TrainingSample>& samples);
    
    // Training control
    bool startTraining();
    void pauseTraining();
    void resumeTraining();
    void stopTraining();
    
    // Progress monitoring
    struct TrainingProgress {
        int currentEpoch;
        int currentStep;
        int totalSteps;
        float currentLoss;
        float validationLoss;
        float validationPerplexity;
        float learningRate;
        std::chrono::milliseconds elapsedTime;
        std::chrono::milliseconds estimatedTimeRemaining;
    };
    
    TrainingProgress getProgress() const;
    void setProgressCallback(std::function<void(const TrainingProgress&)> callback);
    
    // Model export
    bool saveCheckpoint(const std::string& path);
    bool loadCheckpoint(const std::string& path);
    bool exportToGGUF(const std::string& path, int quantBits = 4);
    
private:
    ModelArchitecture arch_;
    TrainingConfig config_;
    Vocabulary vocab_;
    std::vector<TrainingSample> trainingSamples_;
    std::vector<TrainingSample> validationSamples_;
    std::function<void(const TrainingProgress&)> progressCallback_;
    
    bool isTraining_ = false;
    bool shouldStop_ = false;
    TrainingProgress progress_;
    mutable std::mutex progressMutex_;
    
    // Model parameters (simplified representation)
    struct ModelWeights {
        std::vector<float> embeddings;
        std::vector<std::vector<float>> layerWeights;
        std::vector<float> outputWeights;
    };
    ModelWeights weights_;
    
    // Training helpers
    void initializeWeights();
    float forwardPass(const std::vector<uint32_t>& input, std::vector<float>& logits);
    void backwardPass(const std::vector<uint32_t>& input, const std::vector<uint32_t>& target);
    void updateWeights(float lr);
    float computeLoss(const std::vector<float>& logits, const std::vector<uint32_t>& targets);
    float evaluatePerplexity();
};

// ============================================================================
// GGUF EXPORTER
// ============================================================================

/**
 * @brief Exports models to GGUF format (Ollama-compatible)
 */
class GGUFExporter {
public:
    GGUFExporter();
    ~GGUFExporter();
    
    // Export configuration
    void setQuantization(int bits);  // 4, 5, 8, or 16 bits
    void setMetadata(const CustomModelMetadata& metadata);
    
    // Export model
    bool exportModel(
        const ModelArchitecture& arch,
        const std::vector<float>& weights,
        const Vocabulary& vocab,
        const std::string& outputPath
    );
    
    // Quantization options
    enum class QuantType {
        Q4_0,  // 4-bit quantization
        Q4_1,  // 4-bit with better quality
        Q5_0,  // 5-bit quantization
        Q5_1,  // 5-bit with better quality
        Q8_0,  // 8-bit quantization
        F16,   // 16-bit float
        F32    // 32-bit float (no quantization)
    };
    
    bool exportWithQuantization(
        const std::string& modelPath,
        const std::string& outputPath,
        QuantType quantType
    );
    
private:
    int quantBits_ = 4;
    CustomModelMetadata metadata_;
    
    // GGUF format helpers
    void writeGGUFHeader(std::ofstream& file);
    void writeGGUFMetadata(std::ofstream& file, const CustomModelMetadata& metadata);
    void writeGGUFTensors(std::ofstream& file, const std::vector<float>& weights, QuantType quantType);
    void writeGGUFVocabulary(std::ofstream& file, const Vocabulary& vocab);
    
    std::vector<uint8_t> quantizeWeights(const std::vector<float>& weights, QuantType quantType);
};

// ============================================================================
// CUSTOM INFERENCE ENGINE
// ============================================================================

/**
 * @brief Inference engine for custom-built models (Ollama API compatible)
 */
class CustomInferenceEngine {
public:
    CustomInferenceEngine();
    ~CustomInferenceEngine();
    
    // Model loading
    bool loadModel(const std::string& modelPath);
    bool unloadModel();
    bool isModelLoaded() const { return modelLoaded_; }
    
    // Ollama-compatible API
    std::string generate(const std::string& prompt, const std::map<std::string, float>& params = {});
    void generateStreaming(
        const std::string& prompt,
        std::function<void(const std::string&)> callback,
        const std::map<std::string, float>& params = {}
    );
    
    // Chat API
    std::string chat(const std::vector<std::map<std::string, std::string>>& messages);
    void chatStreaming(
        const std::vector<std::map<std::string, std::string>>& messages,
        std::function<void(const std::string&)> callback
    );
    
    // Embeddings
    std::vector<float> getEmbeddings(const std::string& text);
    
    // Model info
    CustomModelMetadata getModelInfo() const { return metadata_; }
    size_t getMaxContextLength() const { return arch_.maxSeqLen; }
    
private:
    bool modelLoaded_ = false;
    CustomModelMetadata metadata_;
    ModelArchitecture arch_;
    Vocabulary vocab_;
    std::vector<float> modelWeights_;
    std::unique_ptr<CustomTokenizer> tokenizer_;
    
    // Inference helpers
    std::vector<float> forward(const std::vector<uint32_t>& tokens);
    uint32_t sampleToken(const std::vector<float>& logits, float temperature, float topP);
    std::vector<uint32_t> generateTokens(
        const std::vector<uint32_t>& promptTokens,
        int maxTokens,
        float temperature,
        float topP,
        std::function<void(uint32_t)> tokenCallback = nullptr
    );
};

// ============================================================================
// MODEL BUILDER ORCHESTRATOR
// ============================================================================

/**
 * @brief High-level interface for building custom models
 */
class ModelBuilder {
public:
    static ModelBuilder& getInstance();
    
    // Builder configuration
    struct BuildConfig {
        std::string modelName;
        std::string modelDescription;
        std::vector<std::string> sourceFiles;
        std::vector<std::string> sourceDirectories;
        ModelArchitecture architecture;
        TrainingConfig trainingConfig;
        std::string outputDirectory = "./custom_models";
        bool autoQuantize = true;
        int quantBits = 4;
    };
    
    // Build pipeline
    std::future<CustomModelMetadata> buildModelAsync(const BuildConfig& config);
    CustomModelMetadata buildModel(const BuildConfig& config);
    
    // Step-by-step building
    bool digestSources(const BuildConfig& config);
    bool buildVocabulary();
    bool trainModel();
    bool exportModel(const std::string& outputPath);
    
    // Model management
    std::vector<CustomModelMetadata> listCustomModels() const;
    CustomModelMetadata getModelMetadata(const std::string& modelName) const;
    bool deleteModel(const std::string& modelName);
    bool renameModel(const std::string& oldName, const std::string& newName);
    
    // Registry
    bool registerModel(const CustomModelMetadata& metadata);
    bool unregisterModel(const std::string& modelName);
    bool saveRegistry(const std::string& path = "./custom_models_registry.json");
    bool loadRegistry(const std::string& path = "./custom_models_registry.json");
    
    // Get components
    FileDigestionEngine& getDigestionEngine() { return *digestionEngine_; }
    CustomTokenizer& getTokenizer() { return *tokenizer_; }
    CustomModelTrainer& getTrainer() { return *trainer_; }
    CustomInferenceEngine& getInferenceEngine() { return *inferenceEngine_; }
    
private:
    ModelBuilder();
    ~ModelBuilder();
    ModelBuilder(const ModelBuilder&) = delete;
    ModelBuilder& operator=(const ModelBuilder&) = delete;
    
    std::unique_ptr<FileDigestionEngine> digestionEngine_;
    std::unique_ptr<CustomTokenizer> tokenizer_;
    std::unique_ptr<CustomModelTrainer> trainer_;
    std::unique_ptr<GGUFExporter> ggufExporter_;
    std::unique_ptr<CustomInferenceEngine> inferenceEngine_;
    
    std::map<std::string, CustomModelMetadata> registry_;
    std::mutex registryMutex_;
    
    // Current build state
    BuildConfig currentConfig_;
    std::vector<TrainingSample> currentSamples_;
    Vocabulary currentVocab_;
};

} // namespace CustomModelBuilder
