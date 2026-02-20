// ============================================================================
// model_training_pipeline.hpp — From-Scratch Model Training + Quantization
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// End-to-end pipeline: Dataset Ingest → Architecture Build → Training Loop
//   → Checkpoint → Quantization (MASM SIMD) → GGUF Export
//
// The training engine uses PyTorch/LibTorch via subprocess bridge for gradient
// computation, while the quantization engine uses our pure MASM x64 kernels
// for maximum compression efficiency (12x+ in MASM alone).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#ifndef MODEL_TRAINING_PIPELINE_HPP
#define MODEL_TRAINING_PIPELINE_HPP

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Training {

// ============================================================================
// Result types (no exceptions)
// ============================================================================

struct TrainingResult {
    bool success;
    const char* detail;
    int errorCode;

    static TrainingResult ok(const char* msg) { return { true, msg, 0 }; }
    static TrainingResult error(const char* msg, int ec = -1) { return { false, msg, ec }; }
};

// ============================================================================
// Dataset Formats
// ============================================================================

enum class DatasetFormat : uint8_t {
    PlainText,      // Raw text files (.txt, .md)
    JSONL,          // {"text": "..."} per line
    Parquet,        // Apache Parquet (via arrow bridge)
    CSV,            // Comma-separated values
    Alpaca,         // {"instruction", "input", "output"}
    ShareGPT,       // {"conversations": [{"from":"human","value":"..."}]}
    CodeFiles,      // Source code files (.cpp, .py, .rs, etc.)
    CustomTokenized // Pre-tokenized binary (uint16_t token IDs)
};

// ============================================================================
// Model Architecture Config
// ============================================================================

enum class ModelArch : uint8_t {
    LLaMA,          // LLaMA/LLaMA2/LLaMA3 architecture
    Mistral,        // Mistral with sliding window attention
    Phi,            // Phi-2/3 architecture (dense attention)
    GPT2,           // Classic GPT-2 architecture
    RWKV,           // Linear attention (RNN-like)
    Mamba,          // State-space model
    Custom          // User-defined layer stack
};

struct ModelArchConfig {
    ModelArch arch           = ModelArch::LLaMA;
    uint32_t  vocabSize      = 32000;
    uint32_t  hiddenSize     = 4096;
    uint32_t  intermediateSize = 11008;
    uint32_t  numLayers      = 32;
    uint32_t  numHeads       = 32;
    uint32_t  numKVHeads     = 32;     // GQA: < numHeads for grouped query
    uint32_t  maxSeqLen      = 4096;
    float     rmsNormEps     = 1e-5f;
    float     ropeTheta      = 10000.0f;
    bool      tieTokEmbeddings = false;
    char      activationFn[32] = "silu";
    char      normType[32]     = "rmsnorm";
};

// ============================================================================
// Training Hyperparameters
// ============================================================================

struct TrainingConfig {
    // Core
    uint32_t batchSize       = 4;
    uint32_t microBatchSize  = 1;
    uint32_t gradientAccumSteps = 4;
    float    learningRate    = 3e-4f;
    float    weightDecay     = 0.1f;
    float    warmupRatio     = 0.03f;
    uint32_t numEpochs       = 1;
    uint64_t maxSteps        = 0;       // 0 = epoch-based
    uint32_t saveEverySteps  = 500;
    uint32_t evalEverySteps  = 100;

    // Optimizer
    char     optimizer[32]   = "adamw";
    float    beta1           = 0.9f;
    float    beta2           = 0.95f;
    float    epsilon         = 1e-8f;
    float    gradClipNorm    = 1.0f;

    // LR schedule
    char     lrScheduler[32] = "cosine";
    float    minLR           = 1e-5f;

    // Precision
    bool     useBF16         = true;
    bool     useGradientCheckpointing = true;

    // LoRA (if not from-scratch)
    bool     useLoRA         = false;
    uint32_t loraRank        = 16;
    float    loraAlpha       = 32.0f;
    float    loraDropout     = 0.05f;
    char     loraTargetModules[128] = "q_proj,k_proj,v_proj,o_proj";

    // Paths
    char     outputDir[512]  = "./training_output";
    char     tokenizerPath[512] = "";
    char     resumeFrom[512] = "";
};

// ============================================================================
// Quantization Config
// ============================================================================

enum class QuantType : uint8_t {
    Q2_K = 0,      // 2-bit K-quant (aggressive)
    Q3_K_S,        // 3-bit K-quant small
    Q3_K_M,        // 3-bit K-quant medium
    Q3_K_L,        // 3-bit K-quant large
    Q4_0,          // 4-bit legacy
    Q4_K_S,        // 4-bit K-quant small
    Q4_K_M,        // 4-bit K-quant medium (default)
    Q5_0,          // 5-bit legacy
    Q5_K_S,        // 5-bit K-quant small
    Q5_K_M,        // 5-bit K-quant medium
    Q6_K,          // 6-bit K-quant
    Q8_0,          // 8-bit
    F16,           // FP16
    F32,           // FP32 (no quantization)
    // RawrXD specials
    IQ2_XXS,       // Importance-weighted 2-bit
    IQ3_S,         // Importance-weighted 3-bit
    NanoQuant,     // Sub-1-bit ADMM (experimental)
    Adaptive       // Per-layer adaptive (max punch per size)
};

struct QuantConfig {
    QuantType       targetQuant       = QuantType::Q4_K_M;
    QuantType       embedQuant        = QuantType::Q8_0;   // Embeddings stay higher
    QuantType       outputQuant       = QuantType::Q6_K;   // Output layer stays higher
    bool            useMASMKernels    = true;
    bool            useAVX512         = false;   // Auto-detected
    bool            useImportanceMatrix = false;
    char            imatrixPath[512]  = "";
    uint32_t        numThreads        = 0;       // 0 = auto
    bool            adaptivePerLayer  = false;    // Per-layer quant selection
    float           targetBPW         = 0.0f;    // 0 = use targetQuant, else aim for this BPW

    // GGUF output
    char            outputPath[512]   = "";
    char            modelName[128]    = "rawrxd-custom";
    uint32_t        ggufAlignment     = 32;
};

// ============================================================================
// Training Metrics (real-time, lock-free)
// ============================================================================

struct TrainingMetrics {
    std::atomic<uint64_t> currentStep{0};
    std::atomic<uint64_t> totalTokensProcessed{0};
    std::atomic<uint64_t> totalSamples{0};
    std::atomic<uint64_t> epochsCompleted{0};
    std::atomic<uint64_t> checkpointsSaved{0};

    // These use relaxed ordering for perf counters
    std::atomic<uint64_t> elapsedMs{0};
    std::atomic<uint64_t> forwardMs{0};
    std::atomic<uint64_t> backwardMs{0};
    std::atomic<uint64_t> optimizerMs{0};

    // Losses stored as fixed-point (x1000 for 3 decimals)
    std::atomic<int64_t>  lastLoss{0};         // x1000
    std::atomic<int64_t>  bestLoss{INT64_MAX};  // x1000
    std::atomic<int64_t>  evalLoss{0};          // x1000

    double getLoss() const { return lastLoss.load(std::memory_order_relaxed) / 1000.0; }
    double getBestLoss() const { return bestLoss.load(std::memory_order_relaxed) / 1000.0; }
    double getEvalLoss() const { return evalLoss.load(std::memory_order_relaxed) / 1000.0; }
    double getTokensPerSec() const {
        uint64_t ms = elapsedMs.load(std::memory_order_relaxed);
        if (ms == 0) return 0.0;
        return (totalTokensProcessed.load(std::memory_order_relaxed) * 1000.0) / ms;
    }
};

// ============================================================================
// Quantization Metrics (real-time)
// ============================================================================

struct QuantMetrics {
    std::atomic<uint32_t> layersProcessed{0};
    std::atomic<uint32_t> totalLayers{0};
    std::atomic<uint64_t> inputBytes{0};
    std::atomic<uint64_t> outputBytes{0};
    std::atomic<uint64_t> elapsedMs{0};
    std::atomic<uint64_t> masmKernelCalls{0};
    std::atomic<uint64_t> cpuFallbackCalls{0};

    double getCompressionRatio() const {
        uint64_t in = inputBytes.load(std::memory_order_relaxed);
        uint64_t out = outputBytes.load(std::memory_order_relaxed);
        return (out > 0) ? (double)in / (double)out : 0.0;
    }
    double getProgress() const {
        uint32_t total = totalLayers.load(std::memory_order_relaxed);
        return (total > 0) ? (double)layersProcessed.load(std::memory_order_relaxed) / total : 0.0;
    }
};

// ============================================================================
// Dataset Entry
// ============================================================================

struct DatasetEntry {
    std::vector<uint16_t> tokenIds;
    uint32_t              seqLen;
    // For instruction-tuning
    uint32_t              promptLen;   // Tokens before response
    uint32_t              responseLen; // Tokens in response
};

// ============================================================================
// Callbacks
// ============================================================================

using TrainProgressCallback = void(*)(const TrainingMetrics& metrics, void* userData);
using QuantProgressCallback = void(*)(const QuantMetrics& metrics, void* userData);
using LogCallback           = void(*)(const char* msg, int level, void* userData);

// ============================================================================
// Dataset Ingest Pipeline
// ============================================================================

class DatasetPipeline {
public:
    TrainingResult addFile(const char* path, DatasetFormat format);
    TrainingResult addDirectory(const char* dirPath, DatasetFormat format, bool recursive = true);
    TrainingResult addText(const char* text, uint64_t length);

    // Tokenization
    TrainingResult loadTokenizer(const char* tokenizerPath);
    TrainingResult buildTokenizer(uint32_t vocabSize = 32000);
    TrainingResult tokenizeAll();

    // Access
    uint64_t         getTotalTokens() const;
    uint64_t         getNumSamples() const;
    const DatasetEntry& getSample(uint64_t idx) const;

    // Serialization
    TrainingResult saveToDisk(const char* path) const;
    TrainingResult loadFromDisk(const char* path);

    // Stats
    struct Stats {
        uint64_t totalFiles;
        uint64_t totalBytes;
        uint64_t totalTokens;
        uint64_t numSamples;
        uint32_t vocabSize;
        double   avgSeqLen;
    };
    Stats getStats() const;

private:
    std::vector<DatasetEntry>  m_samples;
    std::vector<std::string>   m_rawTexts;
    std::vector<uint16_t>      m_vocabulary;
    std::mutex                 m_mutex;
    bool                       m_tokenized = false;
    uint32_t                   m_vocabSize = 0;

    // BPE tokenizer state
    struct BPEMerge {
        uint16_t a, b;   // pair to merge
        uint16_t result; // merged token
    };
    std::vector<BPEMerge>                  m_merges;
    std::vector<std::string>               m_tokenStrings;

    TrainingResult parseJSONL(const char* path);
    TrainingResult parsePlainText(const char* path);
    TrainingResult parseCodeFiles(const char* path);
    TrainingResult parseAlpaca(const char* path);
    TrainingResult parseShareGPT(const char* path);
    TrainingResult parseCSV(const char* path);
};

// ============================================================================
// Model Architecture Builder
// ============================================================================

class ModelArchBuilder {
public:
    TrainingResult configure(const ModelArchConfig& config);

    // Generate PyTorch model definition
    TrainingResult generateModelScript(const char* outputPath) const;

    // Compute parameter count
    uint64_t estimateParamCount() const;
    uint64_t estimateVRAM_FP32() const;
    uint64_t estimateVRAM_BF16() const;

    // Layer analysis
    struct LayerInfo {
        char     name[128];
        uint64_t paramCount;
        uint64_t sizeBytes;
        QuantType recommendedQuant;
    };
    std::vector<LayerInfo> getLayerBreakdown() const;

    const ModelArchConfig& getConfig() const { return m_config; }

private:
    ModelArchConfig m_config;
};

// ============================================================================
// PyTorch Bridge (subprocess-based for GPU training)
// ============================================================================

class PyTorchBridge {
public:
    PyTorchBridge();
    ~PyTorchBridge();

    // Environment detection
    TrainingResult detectPython();
    TrainingResult detectPyTorch();
    TrainingResult detectCUDA();

    struct Environment {
        char pythonPath[512];
        char pythonVersion[32];
        char torchVersion[32];
        char cudaVersion[32];
        bool hasCUDA;
        bool hasBF16;   // Ampere+
        uint32_t gpuCount;
        uint64_t totalVRAM;
    };
    const Environment& getEnv() const { return m_env; }

    // Training execution
    TrainingResult startTraining(const ModelArchConfig& arch,
                                 const TrainingConfig& train,
                                 const DatasetPipeline& dataset);
    TrainingResult stopTraining();
    TrainingResult pauseTraining();
    TrainingResult resumeTraining();
    bool           isTraining() const { return m_training.load(std::memory_order_acquire); }

    // Metrics from subprocess
    TrainingResult pollMetrics(TrainingMetrics& out);

    // Callbacks
    void setProgressCallback(TrainProgressCallback cb, void* userData);
    void setLogCallback(LogCallback cb, void* userData);

    // Checkpoint management
    TrainingResult getLatestCheckpoint(char* pathOut, uint32_t maxLen) const;

    // Convert checkpoint → safetensors/raw weights
    TrainingResult exportWeights(const char* checkpointPath, const char* outputPath) const;

private:
    Environment m_env;
    std::atomic<bool> m_training{false};
    std::mutex m_mutex;

    // Subprocess management
    HANDLE m_processHandle   = nullptr;
    HANDLE m_stdoutRead      = nullptr;
    HANDLE m_stdinWrite      = nullptr;
    HANDLE m_stderrRead      = nullptr;
    HANDLE m_monitorThread   = nullptr;

    // Callbacks
    TrainProgressCallback m_progressCb = nullptr;
    void*                 m_progressData = nullptr;
    LogCallback           m_logCb = nullptr;
    void*                 m_logData = nullptr;

    // Training script path
    char m_scriptPath[512] = {};

    TrainingResult launchProcess(const char* cmd);
    TrainingResult killProcess();
    static DWORD WINAPI monitorThreadProc(LPVOID param);
};

// ============================================================================
// Unified Quantization Engine (MASM + C++ hybrid)
// ============================================================================

class QuantizationEngine {
public:
    static QuantizationEngine& instance();

    // Initialize: detect CPU features, load MASM kernels
    TrainingResult initialize();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // Quantize a full model
    TrainingResult quantizeModel(const char* inputPath,    // safetensors or raw weights
                                  const char* outputGGUF,   // output .gguf path
                                  const QuantConfig& config,
                                  const ModelArchConfig& arch);

    // Quantize a single tensor (for streaming/incremental)
    TrainingResult quantizeTensor(const float* data, uint64_t numElements,
                                   QuantType type, void* outBuf, uint64_t* outBytes);

    // MASM kernel availability
    bool hasMASMKernel(QuantType type) const;
    bool hasAVX512() const { return m_hasAVX512; }
    bool hasAVX2() const { return m_hasAVX2; }

    // Importance matrix
    TrainingResult loadIMatrix(const char* path);
    TrainingResult computeIMatrix(const char* modelPath, const DatasetPipeline& calibData);

    // Metrics
    const QuantMetrics& getMetrics() const { return m_metrics; }
    void resetMetrics();

    // Callbacks
    void setProgressCallback(QuantProgressCallback cb, void* userData);

private:
    QuantizationEngine();

    std::atomic<bool> m_initialized{false};
    bool m_hasAVX2    = false;
    bool m_hasAVX512  = false;
    bool m_hasF16C    = false;
    std::mutex m_mutex;

    QuantMetrics m_metrics;
    QuantProgressCallback m_progressCb = nullptr;
    void*                 m_progressData = nullptr;

    // Importance matrix data
    std::vector<float> m_imatrix;
    bool               m_hasIMatrix = false;

    // MASM kernel function pointers (resolved at init)
    // Dequantize
    using DequantFn = uint64_t(__cdecl*)(const void* src, float* dst, uint64_t numElements);
    DequantFn m_dequantQ4_0 = nullptr;
    DequantFn m_dequantQ8_0 = nullptr;
    DequantFn m_dequantQ4_K = nullptr;
    DequantFn m_dequantQ6_K = nullptr;
    DequantFn m_dequantF16  = nullptr;

    // Quantize (C++ implementations for all types)
    TrainingResult quantize_Q4_0(const float* src, void* dst, uint64_t n, uint64_t* outBytes);
    TrainingResult quantize_Q4_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes);
    TrainingResult quantize_Q5_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes);
    TrainingResult quantize_Q6_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes);
    TrainingResult quantize_Q8_0(const float* src, void* dst, uint64_t n, uint64_t* outBytes);
    TrainingResult quantize_Q2_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes);
    TrainingResult quantize_Q3_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes);
    TrainingResult quantize_F16(const float* src, void* dst, uint64_t n, uint64_t* outBytes);

    // Adaptive quantization
    QuantType selectAdaptiveQuant(const float* data, uint64_t n,
                                   float targetBPW, bool isEmbedding, bool isOutput);

    // GGUF writing
    TrainingResult writeGGUFHeader(FILE* f, const ModelArchConfig& arch, const QuantConfig& config);
    TrainingResult writeGGUFTensor(FILE* f, const char* name, const void* data,
                                     uint64_t bytes, QuantType type,
                                     const uint64_t* shape, uint32_t nDims);

    // CPU feature detection
    void detectCPUFeatures();
    void resolveMASMKernels();
};

// ============================================================================
// End-to-End Training Pipeline Orchestrator
// ============================================================================

class TrainingPipelineOrchestrator {
public:
    static TrainingPipelineOrchestrator& instance();

    // Full pipeline: ingest → train → quantize → export
    TrainingResult runFullPipeline(const char* dataDir,
                                    DatasetFormat dataFormat,
                                    const ModelArchConfig& arch,
                                    const TrainingConfig& train,
                                    const QuantConfig& quant);

    // Step-by-step control
    TrainingResult stepIngest(const char* dataDir, DatasetFormat fmt);
    TrainingResult stepTrain(const ModelArchConfig& arch, const TrainingConfig& train);
    TrainingResult stepQuantize(const QuantConfig& quant);
    TrainingResult stepExport(const char* outputPath);

    // State
    enum class PipelineStage : uint8_t {
        Idle, Ingesting, Training, Quantizing, Exporting, Complete, Failed
    };
    PipelineStage getCurrentStage() const { return m_stage.load(std::memory_order_acquire); }
    const char*   getStageName() const;

    // Component access
    DatasetPipeline&     getDataset() { return m_dataset; }
    ModelArchBuilder&    getArchBuilder() { return m_archBuilder; }
    PyTorchBridge&       getPyTorchBridge() { return m_pytorch; }
    QuantizationEngine&  getQuantEngine() { return QuantizationEngine::instance(); }

    const TrainingMetrics& getTrainMetrics() const { return m_trainMetrics; }
    const QuantMetrics&    getQuantMetrics() const { return QuantizationEngine::instance().getMetrics(); }

    // JSON status
    std::string toJson() const;

private:
    TrainingPipelineOrchestrator() = default;

    DatasetPipeline   m_dataset;
    ModelArchBuilder  m_archBuilder;
    PyTorchBridge     m_pytorch;
    TrainingMetrics   m_trainMetrics;

    std::atomic<PipelineStage> m_stage{PipelineStage::Idle};
    std::mutex m_mutex;
};

} // namespace Training
} // namespace RawrXD

#endif // MODEL_TRAINING_PIPELINE_HPP
