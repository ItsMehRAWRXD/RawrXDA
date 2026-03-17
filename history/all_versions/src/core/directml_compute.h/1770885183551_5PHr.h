// ============================================================================
// directml_compute.h — DirectML Standalone Inference Engine
// ============================================================================
// Native Windows DirectML-based GPU compute backend for RawrXD IDE.
// Replaces PyTorch/Ollama dependency with direct DML operator dispatch.
//
// Architecture:
//   1. Runtime-loads DirectML.dll (graceful fallback if unavailable)
//   2. Creates IDMLDevice from existing GPUBackendBridge's ID3D12Device
//   3. Provides compiled DML operators for full transformer inference:
//      - GEMM (MatMul)
//      - MultiHeadAttention (native DML 6.1+)
//      - RMSNorm (via reduce + element-wise ops)
//      - Softmax
//      - GELU / SiLU activation
//      - RoPE (rotary positional encoding)
//      - Dequantize (GGUF Q4/Q8 → FP16)
//      - Element-wise Add/Multiply
//   4. Integrates with GGUF loader chain for tensor upload
//   5. Supports dual-model inference sessions with VRAM partitioning
//   6. Registers as "DirectML-Inference" engine in StreamingEngineRegistry
//
// Hardware Target: AMD RX 7800 XT (16GB VRAM), RDNA3, Wave64
// DML Feature Level: 6.2 (DML_TARGET_VERSION 0x6200)
// DML Runtime: DirectML.dll v1.15.5
//
// Design:
//   - No exceptions (all methods return DMLResult)
//   - No STL allocators in hot paths
//   - Thread-safe via std::mutex on state mutations
//   - Runtime loading via LoadLibrary("DirectML.dll")
//   - Vtable-based COM calls (matches gpu_backend_bridge.cpp pattern)
//
// Build: MSVC 2022, links against d3d12.lib, dxgi.lib (runtime loaded)
//        DirectML.dll loaded at runtime — no directml.lib link required
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <array>

#ifdef _WIN32
#include <windows.h>    // HMODULE, HRESULT
#endif

// Forward declarations — avoid including d3d12.h and DirectML.h in consumer headers
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12Fence;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;

// DML forward declarations (opaque COM pointers)
struct IDMLDevice;
struct IDMLCommandRecorder;
struct IDMLCompiledOperator;
struct IDMLOperatorInitializer;
struct IDMLBindingTable;
struct IDMLOperator;

namespace RawrXD {
namespace DML {

// ============================================================================
// Result type (no exceptions)
// ============================================================================
struct DMLResult {
    bool        success     = false;
    int32_t     errorCode   = 0;        // HRESULT or custom
    const char* detail      = "";

    static DMLResult ok(const char* msg = "OK") {
        DMLResult r; r.success = true; r.errorCode = 0; r.detail = msg; return r;
    }
    static DMLResult error(const char* msg, int32_t code = -1) {
        DMLResult r; r.success = false; r.errorCode = code; r.detail = msg; return r;
    }
};

// ============================================================================
// Tensor data types (mirror DML_TENSOR_DATA_TYPE)
// ============================================================================
enum class TensorDataType : uint32_t {
    Unknown     = 0,
    Float32     = 1,
    Float16     = 2,
    UInt32      = 3,
    UInt16      = 4,
    UInt8       = 5,
    Int32       = 6,
    Int16       = 7,
    Int8        = 8,
    Float64     = 9,
    UInt64      = 10,
    Int64       = 11,
};

// ============================================================================
// GGUF quantization types (for dequantize dispatch)
// ============================================================================
enum class GGUFQuantType : uint32_t {
    F32         = 0,
    F16         = 1,
    Q4_0        = 2,
    Q4_1        = 3,
    Q5_0        = 6,
    Q5_1        = 7,
    Q8_0        = 8,
    Q8_1        = 9,
    Q2_K        = 10,
    Q3_K_S      = 11,
    Q3_K_M      = 12,
    Q3_K_L      = 13,
    Q4_K_S      = 14,
    Q4_K_M      = 15,
    Q5_K_S      = 16,
    Q5_K_M      = 17,
    Q6_K        = 18,
};

// ============================================================================
// DML Tensor Buffer — GPU-resident tensor with metadata
// ============================================================================
struct DMLTensorBuffer {
    ID3D12Resource* resource        = nullptr;  // GPU DEFAULT heap resource
    uint64_t        gpuVA           = 0;        // GPU virtual address
    uint64_t        sizeBytes       = 0;        // Total buffer size
    TensorDataType  dataType        = TensorDataType::Float16;
    uint32_t        dimCount        = 0;
    uint32_t        dims[8]         = {};       // Max 8 dimensions
    uint32_t        strides[8]      = {};       // Packed strides
    bool            valid           = false;
    uint64_t        nameHash        = 0;        // GGUF tensor name hash

    uint64_t elementCount() const {
        if (dimCount == 0) return 0;
        uint64_t count = 1;
        for (uint32_t i = 0; i < dimCount; ++i) count *= dims[i];
        return count;
    }
};

// ============================================================================
// Compiled Operator Cache Entry
// ============================================================================
struct CompiledOpEntry {
    IDMLCompiledOperator*   compiledOp      = nullptr;
    ID3D12Resource*         persistentBuf   = nullptr;
    ID3D12Resource*         tempBuf         = nullptr;
    uint64_t                persistentSize  = 0;
    uint64_t                tempSize        = 0;
    uint32_t                descriptorCount = 0;
    uint64_t                hitCount        = 0;
    const char*             opName          = "";
};

// ============================================================================
// Inference Session — represents one loaded model's DML state
// ============================================================================
struct InferenceSession {
    uint32_t                sessionId       = 0;
    const char*             modelName       = "";
    uint64_t                vramBudget      = 0;        // Max VRAM for this session
    uint64_t                vramUsed        = 0;        // Current VRAM usage
    bool                    active          = false;

    // Tensor storage (model weights uploaded to GPU)
    std::unordered_map<uint64_t, DMLTensorBuffer> tensors;  // nameHash → tensor

    // KV cache (per-layer)
    std::vector<DMLTensorBuffer> kvCacheK;  // [layer] → key cache
    std::vector<DMLTensorBuffer> kvCacheV;  // [layer] → value cache
    uint32_t                     kvSeqLen = 0;  // Current sequence length in cache

    // Model config (populated from GGUF metadata)
    uint32_t    numLayers       = 0;
    uint32_t    numHeads        = 0;
    uint32_t    numKVHeads      = 0;    // GQA: may differ from numHeads
    uint32_t    headDim         = 0;
    uint32_t    hiddenSize      = 0;
    uint32_t    intermediateSize = 0;
    uint32_t    vocabSize       = 0;
    uint32_t    maxSeqLen       = 4096;
    float       rmsNormEps      = 1e-6f;
    float       ropeTheta       = 10000.0f;
    GGUFQuantType weightQuantType = GGUFQuantType::Q4_K_M;
};

// ============================================================================
// Statistics
// ============================================================================
struct DMLStats {
    std::atomic<uint64_t> totalDispatches{0};
    std::atomic<uint64_t> gemmDispatches{0};
    std::atomic<uint64_t> attentionDispatches{0};
    std::atomic<uint64_t> softmaxDispatches{0};
    std::atomic<uint64_t> normDispatches{0};
    std::atomic<uint64_t> activationDispatches{0};
    std::atomic<uint64_t> dequantDispatches{0};
    std::atomic<uint64_t> totalBytesUploaded{0};
    std::atomic<uint64_t> totalBytesDownloaded{0};
    std::atomic<uint64_t> vramAllocated{0};
    std::atomic<uint64_t> vramFreed{0};
    std::atomic<uint64_t> fenceWaits{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    double peakTFLOPS       = 0.0;
    double avgLatencyMs     = 0.0;
    double tokensPerSec     = 0.0;
};

// ============================================================================
// Callback types
// ============================================================================
typedef void (*DMLLogCallback)(int level, const char* msg, void* userData);
typedef void (*DMLProgressCallback)(uint32_t current, uint32_t total, const char* stage, void* userData);
typedef void (*DMLTokenCallback)(int32_t tokenId, const char* tokenStr, void* userData);

// ============================================================================
// DirectMLCompute — Master DirectML Inference Engine
// ============================================================================
class DirectMLCompute {
public:
    DirectMLCompute();
    ~DirectMLCompute();

    // No copy/move (singleton-style)
    DirectMLCompute(const DirectMLCompute&) = delete;
    DirectMLCompute& operator=(const DirectMLCompute&) = delete;

    // ===== Lifecycle =====
    DMLResult initialize(ID3D12Device* existingDevice = nullptr,
                         ID3D12CommandQueue* existingQueue = nullptr);
    DMLResult shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ===== Session Management (dual-model support) =====
    DMLResult createSession(uint32_t sessionId, const char* modelName, uint64_t vramBudget = 0);
    DMLResult destroySession(uint32_t sessionId);
    DMLResult setActiveSession(uint32_t sessionId);
    InferenceSession* getSession(uint32_t sessionId);
    InferenceSession* getActiveSession() { return getSession(m_activeSessionId); }
    uint32_t getActiveSessionId() const { return m_activeSessionId; }
    uint32_t getSessionCount() const;

    // ===== Tensor Management =====
    DMLResult createTensor(DMLTensorBuffer& outTensor, TensorDataType dtype,
                           const uint32_t* dims, uint32_t dimCount);
    DMLResult uploadTensor(DMLTensorBuffer& tensor, const void* hostData, uint64_t sizeBytes);
    DMLResult downloadTensor(void* hostDst, const DMLTensorBuffer& tensor, uint64_t sizeBytes);
    DMLResult freeTensor(DMLTensorBuffer& tensor);

    // ===== Upload GGUF tensors to session =====
    DMLResult uploadGGUFTensor(uint32_t sessionId, uint64_t nameHash,
                               const void* quantizedData, uint64_t dataSize,
                               GGUFQuantType quantType,
                               const uint32_t* shape, uint32_t shapeDims);

    // ===== Core DML Operators =====

    // GEMM: C = alpha * A * B + beta * C  (with optional transposition)
    DMLResult dispatchGEMM(const DMLTensorBuffer& A, const DMLTensorBuffer& B,
                           DMLTensorBuffer& C,
                           uint32_t M, uint32_t N, uint32_t K,
                           bool transA = false, bool transB = false,
                           float alpha = 1.0f, float beta = 0.0f);

    // Multi-Head Attention (native DML 6.1+ operator)
    DMLResult dispatchMultiHeadAttention(
        const DMLTensorBuffer& Q, const DMLTensorBuffer& K,
        const DMLTensorBuffer& V, DMLTensorBuffer& output,
        uint32_t batchSize, uint32_t seqLen, uint32_t numHeads,
        uint32_t headDim, float scale = 0.0f,
        const DMLTensorBuffer* mask = nullptr,
        const DMLTensorBuffer* pastK = nullptr,
        const DMLTensorBuffer* pastV = nullptr,
        DMLTensorBuffer* presentK = nullptr,
        DMLTensorBuffer* presentV = nullptr);

    // Softmax
    DMLResult dispatchSoftmax(const DMLTensorBuffer& input, DMLTensorBuffer& output,
                              uint32_t axis = -1);

    // RMSNorm: output = (input / rms(input)) * weight
    DMLResult dispatchRMSNorm(const DMLTensorBuffer& input, const DMLTensorBuffer& weight,
                              DMLTensorBuffer& output, float eps = 1e-6f);

    // GELU activation
    DMLResult dispatchGELU(const DMLTensorBuffer& input, DMLTensorBuffer& output);

    // SiLU activation (sigmoid(x) * x)
    DMLResult dispatchSiLU(const DMLTensorBuffer& input, DMLTensorBuffer& output);

    // Element-wise Add: C = A + B
    DMLResult dispatchAdd(const DMLTensorBuffer& A, const DMLTensorBuffer& B,
                          DMLTensorBuffer& C);

    // Element-wise Multiply: C = A * B
    DMLResult dispatchMul(const DMLTensorBuffer& A, const DMLTensorBuffer& B,
                          DMLTensorBuffer& C);

    // RoPE (Rotary Positional Encoding)
    DMLResult dispatchRoPE(DMLTensorBuffer& qk, uint32_t seqLen,
                           uint32_t headDim, uint32_t posOffset,
                           float theta = 10000.0f);

    // Dequantize: GGUF quantized → FP16
    DMLResult dispatchDequantize(const DMLTensorBuffer& quantized,
                                 DMLTensorBuffer& output,
                                 GGUFQuantType quantType,
                                 uint32_t elementCount);

    // ===== High-Level Transformer Ops =====

    // Full transformer layer forward pass
    DMLResult runTransformerLayer(uint32_t sessionId, uint32_t layerIdx,
                                 DMLTensorBuffer& hiddenState,
                                 uint32_t seqLen, uint32_t posOffset);

    // Full model forward pass (all layers + final norm + LM head)
    DMLResult runModelForward(uint32_t sessionId,
                              const int32_t* inputTokens, uint32_t numTokens,
                              float* outputLogits, uint32_t vocabSize);

    // ===== Synchronization =====
    DMLResult syncGPU(uint32_t timeoutMs = 5000);
    DMLResult flushAndWait();

    // ===== Descriptor Heap Management =====
    DMLResult createDescriptorHeap(uint32_t numDescriptors);
    void resetDescriptorHeap();

    // ===== Registry Integration =====
    void registerWithStreamingRegistry();

    // ===== Diagnostics =====
    std::string getDiagnosticsString() const;
    std::string getSessionsString() const;
    const DMLStats& getStats() const { return m_stats; }
    void resetStats();

    // ===== Callbacks =====
    void setLogCallback(DMLLogCallback cb, void* userData) {
        m_logCb = cb; m_logUserData = userData;
    }
    void setProgressCallback(DMLProgressCallback cb, void* userData) {
        m_progressCb = cb; m_progressUserData = userData;
    }
    void setTokenCallback(DMLTokenCallback cb, void* userData) {
        m_tokenCb = cb; m_tokenUserData = userData;
    }

    // ===== Raw Access =====
    ID3D12Device* getDevice() const { return m_d3d12Device; }
    IDMLDevice* getDMLDevice() const { return m_dmlDevice; }
    IDMLCommandRecorder* getCommandRecorder() const { return m_cmdRecorder; }

private:
    // ===== D3D12 Infrastructure =====
    ID3D12Device*               m_d3d12Device       = nullptr;
    ID3D12CommandQueue*         m_cmdQueue           = nullptr;
    ID3D12CommandAllocator*     m_cmdAlloc           = nullptr;
    ID3D12GraphicsCommandList*  m_cmdList            = nullptr;
    ID3D12Fence*                m_fence              = nullptr;
    void*                       m_fenceEvent         = nullptr; // HANDLE
    uint64_t                    m_fenceValue         = 0;
    bool                        m_ownsD3D12          = false;  // true if we created device

    // ===== DirectML Objects =====
    IDMLDevice*                 m_dmlDevice          = nullptr;
    IDMLCommandRecorder*        m_cmdRecorder        = nullptr;

    // ===== Descriptor Heap =====
    ID3D12DescriptorHeap*       m_descriptorHeap     = nullptr;
    uint32_t                    m_descriptorSize     = 0;
    uint32_t                    m_descriptorCount    = 0;
    uint32_t                    m_descriptorOffset   = 0;    // Next free slot

    // ===== Compiled Operator Cache =====
    std::unordered_map<uint64_t, CompiledOpEntry> m_opCache;    // hash → compiled op

    // ===== Sessions (dual-model) =====
    std::unordered_map<uint32_t, std::unique_ptr<InferenceSession>> m_sessions;
    uint32_t                    m_activeSessionId    = 0;

    // ===== State =====
    std::atomic<bool>           m_initialized{false};
    mutable std::mutex          m_mutex;
    void*                       m_dmlModule          = nullptr; // HMODULE — DirectML.dll handle

    // ===== Stats =====
    DMLStats                    m_stats;

    // ===== Callbacks =====
    DMLLogCallback              m_logCb              = nullptr;
    void*                       m_logUserData        = nullptr;
    DMLProgressCallback         m_progressCb         = nullptr;
    void*                       m_progressUserData   = nullptr;
    DMLTokenCallback            m_tokenCb            = nullptr;
    void*                       m_tokenUserData      = nullptr;

    // ===== Logging (public for engine function pointers) =====
    void log(int level, const char* msg);

    // ===== Internal Helpers =====
    DMLResult initDML();
    DMLResult createD3D12Device();
    DMLResult createCommandInfra();
    DMLResult resetCommandList();
    DMLResult executeAndWait(uint32_t timeoutMs = 5000);

    // Descriptor heap helpers
    void getCpuGpuDescriptorHandles(uint32_t offset,
                                     uint64_t& cpuHandle, uint64_t& gpuHandle);
    uint32_t allocateDescriptors(uint32_t count);

    // Resource creation helpers
    DMLResult createDefaultBuffer(ID3D12Resource** ppResource, uint64_t sizeBytes);
    DMLResult createUploadBuffer(ID3D12Resource** ppResource, uint64_t sizeBytes);
    DMLResult createReadbackBuffer(ID3D12Resource** ppResource, uint64_t sizeBytes);
    DMLResult createUAVBuffer(ID3D12Resource** ppResource, uint64_t sizeBytes);

    // Operator compilation helpers
    uint64_t computeOpHash(const char* opName, const uint32_t* params, uint32_t paramCount);
    DMLResult getOrCompileOp(const char* opName, uint64_t hash,
                             void* opDesc, uint32_t opType,
                             CompiledOpEntry& outEntry);
    DMLResult initializeOperator(CompiledOpEntry& entry);
    DMLResult bindAndDispatch(CompiledOpEntry& entry,
                              const DMLTensorBuffer* inputs, uint32_t inputCount,
                              DMLTensorBuffer* outputs, uint32_t outputCount);

    // VRAM partitioning for dual-model
    uint64_t calculateSessionVRAMBudget(uint32_t sessionCount);
};

// ============================================================================
// Global accessor (lazy-initialized)
// ============================================================================
DirectMLCompute& getDirectMLCompute();

} // namespace DML
} // namespace RawrXD
