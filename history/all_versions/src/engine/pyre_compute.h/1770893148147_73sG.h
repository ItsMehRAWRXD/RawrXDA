// pyre_compute.h — Pyre Compute Engine: Dependency-Free Tensor Runtime
//
// A from-scratch replacement for PyTorch's tensor runtime, built for
// the RawrXD inference pipeline. Loads .pyre weight files via memory-mapped
// I/O, orchestrates forward passes through fused MASM AVX2/AVX-512 kernels.
//
// Architecture: C++20 graph runner + MASM64 compute kernels
// Threading: tiled GEMM, vectorized elementwise ops
// Error model: PatchResult (no exceptions)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>

// Forward declarations
struct PatchResult;

// ---------------------------------------------------------------------------
//                     Pyre Data Type Enumeration
// ---------------------------------------------------------------------------
enum class PyreDataType : uint32_t {
    FP32    = 0,    // IEEE 754 single precision (4 bytes)
    FP16    = 1,    // IEEE 754 half precision (2 bytes)
    BF16    = 2,    // Brain float 16 (2 bytes)
    INT8    = 3,    // Signed 8-bit integer
    INT32   = 4,    // Signed 32-bit integer
    Q4_0    = 10,   // 4-bit quantized (GGML-compatible)
    Q8_0    = 11,   // 8-bit quantized (GGML-compatible)
    Q2_K    = 14,   // K-quant 2-bit
    Q4_K    = 16,   // K-quant 4-bit
    Q6_K    = 18,   // K-quant 6-bit
};

// ---------------------------------------------------------------------------
//                     Pyre Operation Types
// ---------------------------------------------------------------------------
enum class PyreOpType : uint32_t {
    // Elementwise
    Add         = 0,
    Mul         = 1,
    SiLU        = 2,    // x * sigmoid(x) — SwiGLU activation
    GeLU        = 3,    // Gaussian error linear unit
    Softmax     = 4,    // Stable softmax with max-subtraction

    // Normalization
    RMSNorm     = 10,   // Root mean square layer normalization
    LayerNorm   = 11,   // Standard layer normalization

    // Linear algebra
    MatMul      = 20,   // General matrix multiplication (GEMM)
    MatVecMul   = 21,   // Matrix-vector multiplication (GEMV)

    // Positional
    RoPE        = 30,   // Rotary position embedding

    // Memory
    Embedding   = 40,   // Token embedding lookup
    Reshape     = 41,   // Tensor reshape (no data copy)
    Transpose   = 42,   // Matrix transpose
    Concat      = 43,   // Tensor concatenation

    // Attention
    ScaledDotProductAttention = 50,

    // Custom
    FusedMLP    = 60,   // Linear → SiLU → Linear (fused)
    Identity    = 99,   // Pass-through (no-op)
};

// ---------------------------------------------------------------------------
//                     PyreTensor — Core tensor structure
// ---------------------------------------------------------------------------
struct PyreTensor {
    void*           data;           // Pointer to tensor data (owned or mmap'd)
    uint64_t        dims[4];        // Shape: [batch, channels/seq, height/heads, width/dim]
    uint64_t        strides[4];     // Element strides per dimension
    PyreDataType    dtype;          // Data type
    uint32_t        ndim;           // Number of active dimensions (1-4)
    uint64_t        numElements;    // Total element count
    uint64_t        byteSize;       // Total size in bytes
    bool            ownsData;       // If true, data is heap-allocated and must be freed
    const char*     name;           // Optional debug name (not owned)

    // Convenience constructors
    static PyreTensor empty();
    static PyreTensor allocate(PyreDataType dtype, uint64_t d0, uint64_t d1 = 1,
                               uint64_t d2 = 1, uint64_t d3 = 1);
    static PyreTensor wrap(void* data, PyreDataType dtype, uint64_t d0, uint64_t d1 = 1,
                           uint64_t d2 = 1, uint64_t d3 = 1);
    void release();

    // Accessors
    float*   fp32()       { return static_cast<float*>(data); }
    const float* fp32() const { return static_cast<const float*>(data); }
    uint16_t* fp16()     { return static_cast<uint16_t*>(data); }
    int8_t*  i8()        { return static_cast<int8_t*>(data); }

    uint64_t dim(uint32_t i) const { return (i < 4) ? dims[i] : 0; }
    bool isValid() const { return data != nullptr && numElements > 0; }
};

// ---------------------------------------------------------------------------
//                     PyreOp — Single operation node in the graph
// ---------------------------------------------------------------------------
struct PyreOp {
    PyreOpType  type;
    uint32_t    inputIndices[4];    // Indices into PyreGraph::m_tensors
    uint32_t    numInputs;
    uint32_t    outputIndex;        // Index into PyreGraph::m_tensors
    float       params[8];         // Op-specific parameters (eps for RMSNorm, scale for attention, etc.)
    const char* debugName;
};

// ---------------------------------------------------------------------------
//                     PyreLayerConfig — Transformer layer descriptor
// ---------------------------------------------------------------------------
struct PyreLayerConfig {
    uint32_t    hiddenDim;          // Model hidden dimension (e.g., 4096)
    uint32_t    numHeads;           // Number of attention heads
    uint32_t    numKVHeads;         // Number of KV heads (GQA support)
    uint32_t    headDim;            // Per-head dimension (hiddenDim / numHeads)
    uint32_t    intermediateSize;   // FFN intermediate size (e.g., 11008)
    uint32_t    vocabSize;          // Vocabulary size
    uint32_t    maxSeqLen;          // Maximum sequence length
    uint32_t    numLayers;          // Number of transformer layers
    float       rmsNormEps;         // RMSNorm epsilon (typically 1e-5 or 1e-6)
    float       ropeTheta;          // RoPE base frequency (typically 10000.0)
    float       ropeScalingFactor;  // RoPE scaling factor (1.0 = no scaling)
};

// ---------------------------------------------------------------------------
//                     PyreModelHeader — .pyre file header
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct PyreModelHeader {
    uint32_t    magic;              // 'PYRE' = 0x45525950
    uint32_t    version;            // File format version
    uint32_t    numTensors;         // Total number of weight tensors
    uint32_t    numLayers;          // Number of transformer layers
    uint64_t    totalBytes;         // Total file size
    uint64_t    dataOffset;         // Byte offset to tensor data section
    PyreLayerConfig config;         // Model configuration
    uint8_t     reserved[64];       // Future use
};
#pragma pack(pop)

// ---------------------------------------------------------------------------
//                     PyreWeightEntry — Tensor directory entry
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct PyreWeightEntry {
    char            name[128];      // Tensor name (null-terminated)
    PyreDataType    dtype;          // Data type
    uint32_t        ndim;           // Number of dimensions
    uint64_t        dims[4];        // Shape
    uint64_t        offset;         // Byte offset from dataOffset
    uint64_t        byteSize;       // Size in bytes
};
#pragma pack(pop)

// ---------------------------------------------------------------------------
//                     PyreGraph — Compute graph runner
// ---------------------------------------------------------------------------
// Statistics for observability
struct PyreStats {
    uint64_t    totalOpsExecuted;
    uint64_t    totalGemmCycles;
    uint64_t    totalNormCycles;
    uint64_t    totalActivationCycles;
    uint64_t    totalSoftmaxCycles;
    uint64_t    totalForwardPasses;
    uint64_t    peakMemoryBytes;
    uint64_t    currentMemoryBytes;
    double      lastForwardMs;
    double      avgTokensPerSec;
};

class PyreGraph {
public:
    static PyreGraph& instance();

    // ---- Lifecycle ----
    PatchResult initialize();
    bool isInitialized() const;
    PatchResult shutdown();

    // ---- Model Loading (.pyre format) ----
    PatchResult loadModel(const char* filepath);
    PatchResult loadFromGGUF(const char* ggufPath);
    bool isModelLoaded() const;
    const PyreLayerConfig& getConfig() const;

    // ---- Tensor Management ----
    uint32_t addTensor(PyreTensor tensor);
    PyreTensor* getTensor(uint32_t index);
    const PyreTensor* getTensor(uint32_t index) const;
    uint32_t getTensorCount() const;
    PyreTensor* getWeightTensor(const char* name);

    // ---- Graph Construction ----
    uint32_t addOp(PyreOp op);
    PatchResult buildGraph();

    // ---- Forward Pass ----
    PatchResult forward(const uint32_t* tokenIds, uint32_t seqLen, float* logitsOut);

    // ---- Individual Kernel Dispatch ----
    PatchResult execMatMul(PyreTensor& out, const PyreTensor& A, const PyreTensor& B);
    PatchResult execRMSNorm(PyreTensor& out, const PyreTensor& input, const PyreTensor& weight, float eps);
    PatchResult execSiLU(PyreTensor& inout);
    PatchResult execSoftmax(PyreTensor& inout, uint32_t axis);
    PatchResult execRoPE(PyreTensor& inout, uint32_t seqOffset, float theta);
    PatchResult execEmbeddingLookup(PyreTensor& out, const PyreTensor& table, const uint32_t* ids, uint32_t count);
    PatchResult execAdd(PyreTensor& out, const PyreTensor& a, const PyreTensor& b);
    PatchResult execMul(PyreTensor& out, const PyreTensor& a, const PyreTensor& b);

    // ---- Diagnostics ----
    PyreStats getStats() const;
    size_t dumpDiagnostics(char* buffer, size_t bufferSize) const;

private:
    PyreGraph();
    ~PyreGraph();
    PyreGraph(const PyreGraph&) = delete;
    PyreGraph& operator=(const PyreGraph&) = delete;

    // ---- Internal forward pass stages ----
    PatchResult forwardEmbedding(const uint32_t* tokenIds, uint32_t seqLen);
    PatchResult forwardTransformerLayer(uint32_t layerIdx);
    PatchResult forwardFinalNorm();
    PatchResult forwardLogits(float* logitsOut, uint32_t seqLen);

    // ---- Memory-mapped model file ----
    HANDLE                          m_hFile;
    HANDLE                          m_hMapping;
    void*                           m_pMappedView;
    uint64_t                        m_mappedSize;

    // ---- Model state ----
    PyreModelHeader                 m_header;
    PyreLayerConfig                 m_config;
    bool                            m_initialized;
    bool                            m_modelLoaded;

    // ---- Tensor storage ----
    std::vector<PyreTensor>         m_tensors;
    std::unordered_map<std::string, uint32_t> m_weightNameMap; // name → tensor index

    // ---- Graph ops ----
    std::vector<PyreOp>             m_ops;
    bool                            m_graphBuilt;

    // ---- Working buffers (pre-allocated for forward pass) ----
    PyreTensor                      m_hiddenState;      // [seqLen, hiddenDim]
    PyreTensor                      m_residual;         // [seqLen, hiddenDim]
    PyreTensor                      m_normOut;          // [seqLen, hiddenDim]
    PyreTensor                      m_attnOut;          // [seqLen, hiddenDim]
    PyreTensor                      m_ffnGate;          // [seqLen, intermediateSize]
    PyreTensor                      m_ffnUp;            // [seqLen, intermediateSize]
    PyreTensor                      m_ffnDown;          // [seqLen, hiddenDim]
    PyreTensor                      m_qBuf;             // [seqLen, hiddenDim]
    PyreTensor                      m_kBuf;             // [seqLen, hiddenDim]
    PyreTensor                      m_vBuf;             // [seqLen, hiddenDim]

    // ---- Statistics ----
    mutable std::mutex              m_mutex;
    PyreStats                       m_stats;
};

// ---------------------------------------------------------------------------
//              MASM64 Kernel Exports (extern "C")
// ---------------------------------------------------------------------------
#ifdef RAWR_HAS_MASM
extern "C" {
    // ---- GEMM (General Matrix Multiply) ----
    // C[M×N] = A[M×K] × B[K×N], row-major, tiled AVX2/AVX-512
    // Returns 0 on success, nonzero on error
    int asm_pyre_gemm_fp32(const float* A, const float* B, float* C,
                            uint32_t M, uint32_t N, uint32_t K);

    // ---- GEMV (Matrix-Vector Multiply) ----
    // y[M] = A[M×K] × x[K], row-major
    int asm_pyre_gemv_fp32(const float* A, const float* x, float* y,
                            uint32_t M, uint32_t K);

    // ---- RMSNorm ----
    // out[i] = (x[i] / sqrt(mean(x^2) + eps)) * weight[i]
    int asm_pyre_rmsnorm(const float* input, const float* weight, float* output,
                          uint32_t dim, float eps);

    // ---- SiLU (Sigmoid Linear Unit) ----
    // inout[i] = x[i] * sigmoid(x[i])   (SwiGLU activation)
    int asm_pyre_silu(float* inout, uint32_t count);

    // ---- Softmax ----
    // Stable softmax with max-subtraction: exp(x[i] - max) / sum(exp(x[j] - max))
    int asm_pyre_softmax(float* inout, uint32_t count);

    // ---- RoPE (Rotary Positional Embedding) ----
    // Applies rotary embedding to [seqLen × headDim] in-place
    int asm_pyre_rope(float* data, uint32_t seqLen, uint32_t headDim,
                       uint32_t seqOffset, float theta);

    // ---- Element-wise Add ----
    // out[i] = a[i] + b[i]
    int asm_pyre_add_fp32(const float* a, const float* b, float* out, uint32_t count);

    // ---- Element-wise Multiply ----
    // out[i] = a[i] * b[i]
    int asm_pyre_mul_fp32(const float* a, const float* b, float* out, uint32_t count);

    // ---- Embedding Lookup ----
    // For each token id, copies embedding row → output
    int asm_pyre_embedding_lookup(const float* table, const uint32_t* ids,
                                    float* output, uint32_t count, uint32_t dim);
}
#endif // RAWR_HAS_MASM

// ---------------------------------------------------------------------------
//                     Pyre Magic Number
// ---------------------------------------------------------------------------
#define PYRE_MAGIC      0x45525950  // 'PYRE' in little-endian
#define PYRE_VERSION    1

// ---------------------------------------------------------------------------
//                     Utility: bytes per element for a PyreDataType
// ---------------------------------------------------------------------------
inline uint64_t pyreDataTypeSize(PyreDataType dtype) {
    switch (dtype) {
        case PyreDataType::FP32:    return 4;
        case PyreDataType::FP16:    return 2;
        case PyreDataType::BF16:    return 2;
        case PyreDataType::INT8:    return 1;
        case PyreDataType::INT32:   return 4;
        case PyreDataType::Q4_0:    return 18; // block_q4_0 = 2 + 16 (for 32 elements)
        case PyreDataType::Q8_0:    return 34; // block_q8_0 = 2 + 32 (for 32 elements)
        case PyreDataType::Q2_K:    return 84; // block_q2_k = 4 + 16 + 64 (for 256 elements)
        case PyreDataType::Q4_K:    return 144; // K-quant 4-bit (for 256 elements)
        case PyreDataType::Q6_K:    return 210; // K-quant 6-bit (for 256 elements)
        default:                    return 4;
    }
}

#endif // Include guard is via #pragma once
