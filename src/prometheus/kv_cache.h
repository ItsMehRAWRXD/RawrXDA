#pragma once
#include "prometheus_config.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <array>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <string>

namespace Prometheus {

// =============================================================================
// KVCACHE CONFIGURATION
// =============================================================================

struct KVCacheConfig {
    uint32_t numLayers = 80;
    uint32_t numHeads = 96;
    uint32_t numKVHeads = 12;
    uint32_t headDim = 128;
    uint32_t maxSeqLen = 262144;
    uint32_t cacheQuantization = 4;
    bool usePagedCache = true;
    uint32_t pageSize = 512;
    float evictionRatio = 0.3f;
    bool enableCompression = true;
    uint32_t compressionLevel = 3;
};

// =============================================================================
// KVCACHE PAGE
// =============================================================================

struct KVCachePage {
    uint32_t pageIndex = 0;
    uint32_t layerIndex = 0;
    uint32_t startPosition = 0;
    uint32_t length = 0;
    uint32_t refCount = 0;
    uint64_t lastAccessTimeNs = 0;
    float attentionScore = 0.0f;
    bool isResident = false;
    bool isCompressed = false;
    void* dataK = nullptr;
    void* dataV = nullptr;
    uint32_t dataSize = 0;
    uint32_t compressedSize = 0;
};

// =============================================================================
// KVCACHE - PRODUCTION IMPLEMENTATION
// =============================================================================

class KVCache {
public:
    explicit KVCache(const KVCacheConfig& config);
    ~KVCache();

    KVCache(const KVCache&) = delete;
    KVCache& operator=(const KVCache&) = delete;
    KVCache(KVCache&&) noexcept;
    KVCache& operator=(KVCache&&) noexcept;

    bool allocate(uint32_t maxSeqLen);
    void deallocate();
    size_t memoryUsage() const;
    size_t compressedMemoryUsage() const;

    uint32_t createSequence();
    void destroySequence(uint32_t seqId);
    bool extendSequence(uint32_t seqId, uint32_t newLength);

    void* getK(uint32_t seqId, uint32_t layer, uint32_t head);
    void* getV(uint32_t seqId, uint32_t layer, uint32_t head);

    void setK(uint32_t seqId, uint32_t layer, uint32_t head,
               const void* data, uint32_t startPos, uint32_t length);
    void setV(uint32_t seqId, uint32_t layer, uint32_t head,
               const void* data, uint32_t startPos, uint32_t length);

    bool getK(uint32_t seqId, uint32_t layer, uint32_t head,
               void* dst, uint32_t startPos, uint32_t length) const;
    bool getV(uint32_t seqId, uint32_t layer, uint32_t head,
               void* dst, uint32_t startPos, uint32_t length) const;

    uint32_t allocatePage(uint32_t seqId, uint32_t layer);
    void releasePage(uint32_t seqId, uint32_t layer, uint32_t pageIndex);
    void touchPage(uint32_t seqId, uint32_t layer, uint32_t pageIndex);

    void evictPages(uint32_t seqId, float ratio);
    void evictAllPages(uint32_t seqId);
    uint32_t evictOldestPages(uint32_t count);

    bool compressPage(uint32_t seqId, uint32_t layer, uint32_t pageIndex);
    bool decompressPage(uint32_t seqId, uint32_t layer, uint32_t pageIndex);

    void updateAttentionScore(uint32_t seqId, uint32_t layer,
                               uint32_t pageIndex, float score);

    static void quantizeQ4(const float* src, void* dst, size_t count);
    static void dequantizeQ4(const void* src, float* dst, size_t count);
    static void quantizeQ8(const float* src, void* dst, size_t count);
    static void dequantizeQ8(const void* src, float* dst, size_t count);

    bool saveToFile(const char* path);
    bool loadFromFile(const char* path);

    struct Stats {
        uint64_t totalPages = 0;
        uint64_t activePages = 0;
        uint64_t residentPages = 0;
        uint64_t compressedPages = 0;
        uint64_t totalMemoryBytes = 0;
        uint64_t compressedMemoryBytes = 0;
        uint32_t activeSequences = 0;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        uint64_t evictions = 0;
        uint64_t compressions = 0;
        uint64_t decompressions = 0;
    };

    Stats getStats() const;
    void resetStats();

private:
    struct SequenceData {
        uint32_t id = 0;
        uint32_t currentLength = 0;
        uint32_t maxWidth = 0;
        uint64_t creationTimeNs = 0;
        bool active = false;
        std::vector<std::vector<KVCachePage>> pages;
    };

    KVCacheConfig config_;
    std::unique_ptr<float[]> bufferK_;
    std::unique_ptr<float[]> bufferV_;
    std::vector<SequenceData> sequences_;
    mutable std::shared_mutex mutex_;

    std::vector<KVCachePage> pagePool_;
    std::vector<uint32_t> freePages_;

    mutable std::atomic<uint64_t> cacheHits_{0};
    mutable std::atomic<uint64_t> cacheMisses_{0};
    mutable std::atomic<uint64_t> evictions_{0};
    mutable std::atomic<uint64_t> compressions_{0};
    mutable std::atomic<uint64_t> decompressions_{0};

    size_t getLayerOffset(uint32_t layer) const;
    size_t getHeadOffset(uint32_t layer, uint32_t head) const;
    void* getPageData(uint32_t seqId, uint32_t layer, uint32_t pageIndex, bool isK);
};

// =============================================================================
// SPECULATIVE DECODER
// =============================================================================

struct SpeculativeConfig {
    uint32_t draftModelSize = 1;
    uint32_t maxSpeculativeTokens = 8;
    float acceptThreshold = 0.9f;
    uint32_t draftBatchSize = 1;
    bool useTreeSpeculation = false;
    uint32_t maxTreeWidth = 4;
    uint32_t maxTreeDepth = 3;
};

struct SpeculativeResult {
    std::vector<uint32_t> acceptedTokens;
    std::vector<uint32_t> rejectedTokens;
    std::vector<float> acceptedLogProbs;
    uint32_t numSpeculated = 0;
    uint32_t numAccepted = 0;
    float acceptRate = 0.0f;
    double draftTimeMs = 0.0;
    double verifyTimeMs = 0.0;
    uint64_t savedTokens = 0;
};

class SpeculativeDecoder {
public:
    explicit SpeculativeDecoder(const SpeculativeConfig& config);
    ~SpeculativeDecoder();

    bool loadDraftModel(const std::string& modelPath);
    void unloadDraftModel();
    bool isDraftModelLoaded() const;

    SpeculativeResult speculate(
        const std::vector<uint32_t>& prefix,
        uint32_t maxTokens,
        const std::function<uint32_t(const std::vector<uint32_t>&)>& mainSampler
    );

    SpeculativeResult speculateTree(
        const std::vector<uint32_t>& prefix,
        uint32_t maxTokens,
        const std::function<uint32_t(const std::vector<uint32_t>&)>& mainSampler
    );

    bool verifyTokens(
        const std::vector<uint32_t>& prefix,
        const std::vector<uint32_t>& candidateTokens,
        std::vector<float>& logProbs
    );

    void setConfig(const SpeculativeConfig& config);
    SpeculativeConfig getConfig() const;

    struct Stats {
        uint64_t totalSpeculations = 0;
        uint64_t totalTokensSpeculated = 0;
        uint64_t totalTokensAccepted = 0;
        double averageAcceptRate = 0.0;
        double averageDraftTimeMs = 0.0;
        double averageVerifyTimeMs = 0.0;
        uint64_t draftsSkipped = 0;
        uint64_t draftsUsed = 0;
    };

    Stats getStats() const;
    void resetStats();

private:
    SpeculativeConfig config_;
    void* draftModelState_ = nullptr;
    bool draftModelLoaded_ = false;

    std::vector<uint32_t> draftGenerate(
        const std::vector<uint32_t>& prefix,
        uint32_t maxTokens
    );

    float computeAcceptProbability(
        uint32_t token,
        const std::vector<float>& draftProbs,
        const std::vector<float>& mainProbs
    );

    mutable std::mutex mutex_;
    Stats stats_;
};

// =============================================================================
// VISION ENCODER
// =============================================================================

struct VisionConfig {
    uint32_t imageWidth = 336;
    uint32_t imageHeight = 336;
    uint32_t patchSize = 14;
    uint32_t hiddenDim = 1024;
    uint32_t numLayers = 24;
    uint32_t numHeads = 16;
    uint32_t intermediateDim = 4096;
    float dropRate = 0.0f;
    std::string imageMean = "clip";
    std::string imageStd = "clip";
    bool useFlashAttention = true;
    bool useFP16 = true;
};

struct ImageInput {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    float aspectRatio = 1.0f;
    std::string format;
};

struct ImageEmbeddings {
    std::vector<float> patchEmbeddings;
    std::vector<uint32_t> patchIndices;
    uint32_t numPatches = 0;
    uint32_t originalWidth = 0;
    uint32_t originalHeight = 0;
};

class VisionEncoder {
public:
    explicit VisionEncoder(const VisionConfig& config);
    ~VisionEncoder();

    bool loadWeights(const std::string& modelPath);
    void unloadWeights();
    bool isLoaded() const;

    ImageInput preprocessImage(
        const std::vector<uint8_t>& rawPixels,
        uint32_t width,
        uint32_t height,
        uint32_t channels
    );

    ImageEmbeddings encode(const ImageInput& image);
    ImageEmbeddings encodeBatch(const std::vector<ImageInput>& images);

    void setConfig(const VisionConfig& config);
    VisionConfig getConfig() const;

    struct Stats {
        uint64_t imagesEncoded = 0;
        uint64_t patchesProcessed = 0;
        double averageEncodeTimeMs = 0.0;
        double peakMemoryMB = 0.0;
    };

    Stats getStats() const;
    void resetStats();

private:
    VisionConfig config_;
    std::vector<float> weights_;
    bool weightsLoaded_ = false;

    std::vector<float> resizeImage(const std::vector<uint8_t>& pixels,
                                     uint32_t srcWidth, uint32_t srcHeight,
                                     uint32_t dstWidth, uint32_t dstHeight);
    std::vector<float> normalizeImage(const std::vector<float>& pixels);
    std::vector<float> extractPatches(const std::vector<float>& normalized);
    std::vector<float> forward(const std::vector<float>& patches);

    mutable std::mutex mutex_;
    Stats stats_;
};

// =============================================================================
// CODE SANDBOX
// =============================================================================

struct SandboxConfig {
    std::string runtime = "python";
    uint64_t memoryLimitMB = 256;
    uint32_t timeoutMs = 30000;
    bool allowNetwork = false;
    bool allowFilesystem = false;
    bool allowSubprocess = false;
    std::string workDir;
    std::vector<std::string> allowedCommands;
    std::vector<std::string> allowedPaths;
};

struct ExecutionResult {
    bool success = false;
    std::string stdout_output;
    std::string stderr_output;
    int exitCode = -1;
    std::chrono::milliseconds duration{0};
    uint64_t memoryUsedKB = 0;
    std::vector<std::string> generatedFiles;
    std::string error;
};

class CodeSandbox {
public:
    explicit CodeSandbox(const SandboxConfig& config);
    ~CodeSandbox();

    bool initialize();
    void shutdown();
    bool isInitialized() const;

    ExecutionResult execute(
        const std::string& code,
        const std::string& language = "python"
    );

    ExecutionResult executeWithInput(
        const std::string& code,
        const std::string& input,
        const std::string& language = "python"
    );

    ExecutionResult executeFile(const std::string& filePath);

    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    std::vector<std::string> listFiles(const std::string& dir = "");
    bool deleteFile(const std::string& path);

    bool validate(const std::string& code);
    std::vector<std::string> scanForDangerousOperations(const std::string& code);

    void setConfig(const SandboxConfig& config);
    SandboxConfig getConfig() const;

    struct Stats {
        uint64_t totalExecutions = 0;
        uint64_t successfulExecutions = 0;
        uint64_t timeoutExecutions = 0;
        uint64_t memoryLimitHits = 0;
        uint64_t securityViolations = 0;
        double averageExecutionTimeMs = 0.0;
        double peakMemoryMB = 0.0;
    };

    Stats getStats() const;
    void resetStats();

private:
    SandboxConfig config_;
    bool initialized_ = false;
    void* runtimeState_ = nullptr;

    mutable std::mutex mutex_;
    Stats stats_;

    ExecutionResult executePython(const std::string& code, const std::string& input);
    ExecutionResult executeNode(const std::string& code, const std::string& input);
    ExecutionResult executeBash(const std::string& code, const std::string& input);

    bool setupSandbox();
    void teardownSandbox();
    bool enforceLimits();
    std::string sanitizeCode(const std::string& code);
};

} // namespace Prometheus
