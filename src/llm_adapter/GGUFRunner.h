#pragma once

#include "BinaryStream.hpp"
#include "QuantBackend.h"
#include "core/unified_memory_executor.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

// GGML tensor types (numeric values match llama.cpp / ggml_type)
enum class GgmlType : uint32_t
{
    F32 = 0,
    F16 = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    Q2_K = 10,
    Q3_K = 11,
    Q4_K = 12,
    Q5_K = 13,
    Q6_K = 14,
    Q8_K = 15
};

// Q4_0 block: 32 weights in 16 bytes + 1 float16 delta
struct BlockQ4_0
{
    uint16_t d;      // delta (float16)
    uint8_t qs[16];  // 32 nibbles (2 per byte)
};

// Q8_0 block: 32 weights in 32 bytes + 1 float16 delta
struct BlockQ8_0
{
    uint16_t d;     // delta (float16)
    int8_t qs[32];  // 32 signed bytes
};

/** GGUFRunner — GGUF model load and inference. Win32 + STL only. */
class GGUFRunner
{
  public:
    explicit GGUFRunner(void* parent = nullptr);
    ~GGUFRunner();

    bool runInference(const std::string& prompt, float* outputBuffer);
    bool loadModel(const std::string& filePath);

    void setMaxTokens(int max) { context_.maxTokens = max; }
    void setTemperature(float temp) { context_.temperature = std::max(0.0f, temp); }
    void setTopP(float p) { context_.topP = std::clamp(p, 0.0f, 1.0f); }
    void setRepeatPenalty(float penalty) { context_.repeatPenalty = std::max(1.0f, penalty); }

    bool setQuantizationMode(QuantMode mode);
    QuantMode currentQuantMode() const;
    float getCompressionRatio() const;

    std::string modelPath() const { return context_.modelPath; }
    std::string modelName() const { return context_.modelName; }
    std::string architecture() const { return context_.architecture; }
    size_t vocabularySize() const { return context_.vocabSize; }
    size_t embeddingDim() const { return context_.embedDim; }
    /// Transformer block count from GGUF metadata (for diagnostics / harnesses).
    size_t layerCount() const { return context_.nLayers; }
    bool isLoaded() const { return hasModelFileBacking(); }

    /// True only when per-layer transformer weights are present (blk.* tensors loaded). Without this,
    /// runInference would access empty layer matrices → access violation on Windows (exit 0xC0000005).
    bool inferenceWeightsReady() const;

    /// Decoder steps completed in the last successful `runInference` (each forward/token; EOS early exit counts).
    int lastDecodeSteps() const { return context_.lastDecodeSteps; }

    /**
     * @brief Compresses a raw buffer using the "Brutal" stored-block algorithm.
     *        This is extremely fast (0.2ms/MB) but offers no compression ratio.
     *        Useful for wrapping data in gzip format for compatibility without CPU cost.
     * @param data Pointer to raw data
     * @param len Length of data
     * @return std::vector<uint8_t> containing the gzip stream
     */
    static std::vector<uint8_t> compressBrutal(const void* data, size_t len);


    void tokenChunkGenerated(const std::string& chunk);
    void inferenceComplete(bool success);
    void modelLoaded(const std::string& path, int64_t sizeBytes);
    void loadingProgress(int percent);

  private:
    enum class QuantType
    {
        F32,   // Full precision (13 GB for 7B)
        F16,   // Half precision (6.5 GB)
        Q4_0,  // 4-bit quantization (3.5 GB) - llama.cpp standard
        Q4_1,  // 4-bit with min/max (slightly better quality)
        Q5_0,  // 5-bit quantization (4.3 GB)
        Q5_1,  // 5-bit with min/max
        Q8_0   // 8-bit quantization (6.7 GB)
    };

    struct ModelContext
    {
        // Hardware features
        bool hasAVX2{false};
        bool hasAVX512{false};
        bool hasFMA{false};

        // Memory management
        float* mappedData{nullptr};
        bool usesMmap{false};
#if defined(_WIN32)
        /// Win32 file map (not heap); UnmapViewOfFile + CloseHandle — never delete[] mappedData when set.
        void* win32MapView{nullptr};
        void* win32MapHandle{nullptr};
        void* win32FileHandle{nullptr};
#endif
        /// Optional: full GGUF bytes copied into `UnifiedMemoryExecutor` (SAM / host arena). Tensor reads use
        /// `unifiedFileBase + TensorDesc.offset` instead of `NativeFile` seeks (see `RAWRXD_GGUF_USE_UNIFIED_MEMORY`).
        bool usesUnifiedFileBacking{false};
        void* unifiedFileBase{nullptr};
        uint64_t unifiedFileBytes{0};
        RawrXD::UnifiedMemory::UnifiedBuffer unifiedModelBuffer{};
        size_t embedDim{0};
        size_t vocabSize{0};
        size_t nLayers{0};
        size_t nHeads{0};
        size_t nKVHeads{0};
        /// Feed-forward hidden (`ffn_gate` / `ffn_up` column dim). From KV `llama.feed_forward_length` or tensor shape.
        size_t ffnDim{0};
        size_t headDim{0};
        float ropeBase{10000.0f};    // RoPE frequency base
        std::vector<float> invFreq;  // Precomputed inverse frequencies for RoPE [headDim/2]
        int64_t modelFileSize{0};

        // Inference state
        std::vector<float> logits;
        std::vector<std::string> vocabulary;
        std::string modelPath;

        // Generation parameters
        int maxTokens{64};
        int lastDecodeSteps{0};
        int eosTokenId{-1};
        float temperature{0.8f};    // 0.0 = greedy, 1.0 = creative, 2.0 = chaos
        float topP{0.95f};          // nucleus sampling threshold
        float repeatPenalty{1.1f};  // penalize token repetition

        // Quantization
        QuantType quantType{QuantType::F32};

        // GGUF metadata
        uint32_t ggufVersion{0};
        std::string modelName;
        std::string architecture;

        // GGUF tensors (essential weights)
        std::vector<float> tok_embeddings;   // [vocabSize, embedDim]
        std::vector<float> output_norm_w;    // output norm weight
        std::vector<float> output_w;         // output projection weight
        std::vector<uint8_t> raw_q4_output;  // raw Q4_0 bytes for output.weight (optional)
        std::vector<int8_t> raw_q8_output;   // raw Q8_0 bytes for output.weight (optional)
        std::vector<float> ln_f_g;           // final layernorm gamma [embedDim]
        std::vector<float> ln_f_b;           // final layernorm beta [embedDim]

        struct Layer
        {
            // Attention projections: [embedDim, embedDim]
            std::vector<float> attn_q_w;
            std::vector<float> attn_k_w;
            std::vector<float> attn_v_w;
            std::vector<float> attn_o_w;
            // LayerNorm params
            std::vector<float> ln_1_g;
            std::vector<float> ln_1_b;
            std::vector<float> ln_2_g;
            std::vector<float> ln_2_b;
            // MLP (SwiGLU): up, gate, down: [embedDim, 4*embedDim] and [4*embedDim, embedDim]
            std::vector<float> mlp_up_w;
            std::vector<float> mlp_gate_w;
            std::vector<float> mlp_down_w;
        };
        std::vector<Layer> layers;

        // KV-cache: per layer K/V for past tokens (multi-head GQA)
        std::vector<float> keyCache;    // [nLayers, nKVHeads, maxTokens, headDim]
        std::vector<float> valueCache;  // [nLayers, nKVHeads, maxTokens, headDim]
        size_t kvLen{0};

        /// After `parseGgufTensorTable`: byte offset where tensor **payload** region starts (aligned per GGUF).
        uint64_t tensorDataBaseOffset{0};
        /// From KV `general.alignment` when present; default 32. Pads end-of-metadata → tensor data.
        uint32_t ggufTensorAlignment{32};

        // Tensor directory
        struct TensorDesc
        {
            std::string name;
            std::vector<uint32_t> dims;
            GgmlType type;
            /// After `parseGgufTensorTable`: **absolute** file offset for `seek` / IO.
            uint64_t offset{};
        };
        std::unordered_map<std::string, TensorDesc> tensorTable;
    };

    void checkCpuFeatures();
    void loadGGUFModel(const std::string& filePath);
    void loadVocabulary(const std::string& vocabPath);
    float* getLayerWeights();
    bool prepareLLMInput(const std::string& prompt, std::vector<float>& embeddings);
    void applySoftmax(float* buffer);
    void applyTemperature(float* buffer, float temperature);
    size_t sampleNextToken(float* buffer);
    size_t sampleTopP(float* buffer, float topP);
    size_t sampleGreedy(float* buffer);
    std::string decodeToken(size_t tokenId) const;
    void fallback_matrix_multiply(float* A, float* B, float* C, int N, int M, int K);
    void detectExtendedCpuFeatures();

    bool parseGgufTensors(RawrXD::NativeFile& file);
    /// Load `blk.N.*` llama-architecture tensors into `context_.layers` (float32 mirror). Respects
    /// `RAWRXD_GGUF_MAX_LAYER_FLOAT_RAM_GB` to avoid OOM on multi-hundred-GB dequant.
    bool parseGgufLayerWeights(RawrXD::NativeFile& file);
    bool parseGgufTensorTable(RawrXD::NativeFile& file);
    bool readTensorFloat32(RawrXD::NativeFile& file, int64_t offset, int64_t count, std::vector<float>& out);
    bool loadTensor(RawrXD::NativeFile& file, const std::string& name, std::vector<float>& weights);
    size_t ggmlTypeSize(GgmlType type);
    std::vector<uint8_t> readTensorData(RawrXD::NativeFile& file, uint64_t offset, uint64_t numBytes);

    // Transformer forward (scalar)
  private:
    void layerNorm(const float* x, float* y, const std::vector<float>& gamma, const std::vector<float>& beta,
                   size_t dim);
    void matmul(const float* A, const float* B, float* C, int N, int M, int K);
    void attentionForward(int layerIdx, const float* x, float* y);
    void mlpForward(int layerIdx, const float* x, float* y);
    void releaseWeightFileBacking();
    bool hasModelFileBacking() const;

    /// Read standard llama.cpp-style GGUF KV fields (authoritative vs. ASCII substring scan).
    void ingestGgufKvMetadata(RawrXD::BinaryStream& ds, uint64_t kvCount);
    /// Clamp insane metadata that can overflow KV cache sizing or enable bogus n_layers==0 "ready" paths.
    void sanitizeArchMetadata();

    ModelContext context_;
};
