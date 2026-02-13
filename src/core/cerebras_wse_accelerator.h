// ============================================================================
// cerebras_wse_accelerator.h — Cerebras WSE-2/WSE-3 Wafer-Scale Engine
// ============================================================================
// Phase 29C: Network-attached wafer-scale inference accelerator.
//
// ARCHITECTURE NOTES — Cerebras is fundamentally different from GPU:
//   - WSE-2: 850,000 AI cores, 40 GB on-chip SRAM, 220 Pb/s fabric
//   - WSE-3: 900,000 AI cores, 44 GB on-chip SRAM, 264 Pb/s fabric
//   - NO off-chip memory bottleneck — entire model fits on-wafer
//   - NO PCIe bus — accessed via network (TCP/gRPC to CS-2/CS-3 appliance)
//   - Weight streaming: model partitioned across wafer by compiler
//   - Dataflow architecture: data flows through cores, no cache hierarchy
//   - Software stack: Cerebras SDK (CSoft), Weight Streaming API
//
// Integration pattern:
//   Local machine → TCP/gRPC → CS-2/CS-3 appliance → WSE wafer
//   This accelerator is a *network client*, not a local device driver.
//
// Follows AMD/Intel/ARM64 singleton pattern for API consistency.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef CEREBRAS_WSE_ACCELERATOR_H
#define CEREBRAS_WSE_ACCELERATOR_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <functional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// ============================================================================
// Enumerations
// ============================================================================

enum class CerebrasBackend : uint8_t {
    None       = 0,
    TCP        = 1,   // Raw TCP socket to CS appliance
    gRPC       = 2,   // gRPC (via grpc_cpp_plugin / dynamic load)
    REST       = 3,   // REST/HTTP2 endpoint (Cerebras Cloud)
    WeightStream = 4, // Cerebras Weight Streaming SDK
    Auto       = 255
};

enum class CerebrasWSEGen : uint8_t {
    Unknown = 0,
    WSE_1   = 1,  // Original WSE (Cerebras CS-1)
    WSE_2   = 2,  // 850k cores, 40 GB SRAM, 2.6T transistors
    WSE_3   = 3,  // 900k cores, 44 GB SRAM, 4T transistors
};

enum class CerebrasFeatureFlag : uint32_t {
    None              = 0,
    // Datatype support
    FP32              = (1u << 0),
    FP16              = (1u << 1),
    BF16              = (1u << 2),
    INT8              = (1u << 3),
    INT16             = (1u << 4),
    csBFloat16        = (1u << 5),  // Cerebras custom bfloat format
    // Architecture features
    WeightStreaming   = (1u << 8),  // Model weights streamed from host to wafer
    PipelinedExec     = (1u << 9),  // Pipelined layer execution across cores
    SparsitySupport   = (1u << 10), // Sparse tensor acceleration
    DynamicSparsity   = (1u << 11), // Runtime sparsity pattern changes
    LazyExec          = (1u << 12), // Lazy/deferred execution mode
    // Network features
    RoCE_v2           = (1u << 16), // RDMA over Converged Ethernet
    InfiniBand        = (1u << 17), // IB fabric to appliance
    TCPDirect         = (1u << 18), // Standard TCP
    gRPCStream        = (1u << 19), // Streaming gRPC
    // Appliance features
    MultiCS           = (1u << 24), // Multiple CS-2 systems chained
    MemoryX           = (1u << 25), // MemoryX off-wafer memory extension
    SwarmX            = (1u << 26), // SwarmX multi-system coordination
};

enum class CerebrasAccelScope : uint8_t {
    ModelLoad    = (1 << 0),  // GGUF model weight streaming to wafer
    Inference    = (1 << 1),  // Forward pass on WSE
    Training     = (1 << 2),  // Fine-tuning / training loops
    Quantization = (1 << 3),  // On-wafer quantization/dequantization
    Attention    = (1 << 4),  // Attention kernel dispatch
    Embedding    = (1 << 5),  // Embedding table lookup on wafer
    All          = 0xFF
};

// Appliance connection state
enum class CerebrasConnState : uint8_t {
    Disconnected = 0,
    Connecting   = 1,
    Connected    = 2,
    Streaming    = 3,  // Actively streaming weights/data
    Error        = 4
};

// ============================================================================
// Structures
// ============================================================================

struct CerebrasAccelResult {
    bool        success;
    const char* detail;
    int         errorCode;
    double      elapsedMs;
    double      throughputTFLOPS;  // Peak TFLOPS achieved
    double      networkLatencyMs;  // Round-trip to appliance

    static CerebrasAccelResult ok(const char* msg) {
        return { true, msg, 0, 0.0, 0.0, 0.0 };
    }
    static CerebrasAccelResult error(const char* msg, int code = -1) {
        return { false, msg, code, 0.0, 0.0, 0.0 };
    }
};

// Represents the remote wafer's memory — NOT local RAM
struct CerebrasWaferMemory {
    uint64_t totalSRAM_Bytes;      // 40 GB (WSE-2) or 44 GB (WSE-3)
    uint64_t usedSRAM_Bytes;       // Reported by appliance
    uint64_t peakSRAM_Bytes;
    uint64_t memoryXBytes;         // MemoryX off-wafer extension (if present)
    uint32_t coreCount;            // 850,000 or 900,000
    uint32_t activeCores;          // Cores allocated to this session
    uint32_t allocCount;
    uint32_t freeCount;
};

// A "buffer" is actually a remote tensor handle on the wafer
struct CerebrasWaferBuffer {
    uint64_t remoteHandle;    // Opaque handle returned by appliance
    uint64_t sizeBytes;       // Logical size
    uint32_t bufferId;        // Local tracking ID
    uint16_t coresAssigned;   // How many wafer cores hold this tensor
    uint8_t  quantType;       // 0=FP32, 1=FP16, 2=BF16, 3=INT8
    bool     onWafer;         // true if currently resident on WSE SRAM
    bool     streaming;       // true if weight-streaming mode
    void*    localShadow;     // Optional local shadow copy for validation
};

// Appliance endpoint configuration
struct CerebrasEndpoint {
    char     host[256];      // IP or hostname of CS-2/CS-3
    uint16_t port;           // Default: 9000 (gRPC), 443 (REST)
    char     apiKey[256];    // For Cerebras Cloud API
    char     modelPath[512]; // Model path on appliance filesystem
    bool     useTLS;         // TLS encryption
    uint32_t timeoutMs;      // Connection timeout
    uint32_t keepAliveMs;    // Keepalive interval
};

// Statistics
struct CerebrasAccelStats {
    std::atomic<uint64_t> waferDispatches;
    std::atomic<uint64_t> cpuFallbacks;
    std::atomic<uint64_t> networkBytes_Sent;
    std::atomic<uint64_t> networkBytes_Recv;
    std::atomic<uint64_t> weightStreamChunks;
    std::atomic<uint64_t> inferenceCount;
    std::atomic<uint64_t> tokensGenerated;
    std::atomic<uint64_t> toggleOnCount;
    std::atomic<uint64_t> toggleOffCount;
    // Timing
    std::atomic<uint64_t> totalInferenceMs;
    std::atomic<uint64_t> totalNetworkMs;
    std::atomic<uint64_t> totalStreamMs;
    // Peak performance
    double peakTFLOPS;
    double avgTokensPerSec;
    double avgLatencyMs;
    // Error tracking
    std::atomic<uint64_t> networkErrors;
    std::atomic<uint64_t> waferErrors;
    std::atomic<uint64_t> timeouts;
};

// ============================================================================
// Callback Types
// ============================================================================

// Connection state change
using CerebrasConnCallback = void(*)(CerebrasConnState newState, void* userData);
// Toggle callback
using CerebrasToggleCallback = void(*)(bool enabled, CerebrasBackend backend, void* userData);
// Error callback
using CerebrasErrorCallback = void(*)(const char* msg, int code, void* userData);
// Weight streaming progress
using CerebrasStreamCallback = void(*)(uint64_t bytesSent, uint64_t totalBytes, void* userData);

// ============================================================================
// Cerebras WSE Accelerator (Singleton)
// ============================================================================

class CerebrasWSEAccelerator {
public:
    static CerebrasWSEAccelerator& instance();

    // Lifecycle
    CerebrasAccelResult initialize(CerebrasBackend preferredBackend = CerebrasBackend::Auto);
    void                shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }
    bool isConnected()   const { return m_connState.load(std::memory_order_acquire) == CerebrasConnState::Connected ||
                                        m_connState.load(std::memory_order_acquire) == CerebrasConnState::Streaming; }

    // Connection management
    CerebrasAccelResult connect(const CerebrasEndpoint& endpoint);
    CerebrasAccelResult disconnect();
    CerebrasAccelResult reconnect();
    CerebrasConnState   getConnectionState() const { return m_connState.load(std::memory_order_acquire); }

    // Master toggle
    CerebrasAccelResult enableWSE();
    CerebrasAccelResult disableWSE();
    CerebrasAccelResult toggleWSE();
    bool isWSEEnabled() const { return m_wseEnabled.load(std::memory_order_acquire); }

    // Scope toggles
    CerebrasAccelResult enableScope(CerebrasAccelScope scope);
    CerebrasAccelResult disableScope(CerebrasAccelScope scope);
    bool isScopeEnabled(CerebrasAccelScope scope) const;

    // Wafer info
    CerebrasWSEGen      getWSEGeneration() const { return m_wseGen; }
    CerebrasBackend     getActiveBackend() const { return m_activeBackend; }
    const char*         getBackendName() const;
    const char*         getApplianceName() const { return m_applianceName.c_str(); }
    uint32_t            getCoreCount() const { return m_waferMemory.coreCount; }
    uint64_t            getWaferSRAM() const { return m_waferMemory.totalSRAM_Bytes; }
    bool                hasFeature(CerebrasFeatureFlag feature) const;

    // Weight streaming (primary model loading mechanism)
    CerebrasAccelResult beginWeightStream(const char* modelPath, uint64_t modelSizeBytes);
    CerebrasAccelResult streamWeightChunk(const void* data, uint64_t bytes, uint64_t offset);
    CerebrasAccelResult endWeightStream();
    CerebrasAccelResult isModelLoaded(bool& loaded) const;
    double              getStreamProgress() const;

    // Remote buffer management (wafer SRAM handles)
    CerebrasAccelResult allocWafer(uint64_t sizeBytes, CerebrasWaferBuffer& outBuffer);
    CerebrasAccelResult freeWafer(CerebrasWaferBuffer& buffer);
    CerebrasAccelResult uploadToWafer(CerebrasWaferBuffer& dst, const void* hostSrc, uint64_t bytes);
    CerebrasAccelResult downloadFromWafer(void* hostDst, const CerebrasWaferBuffer& src, uint64_t bytes);

    // Compute dispatch (remote execution on wafer)
    CerebrasAccelResult dispatchInference(const CerebrasWaferBuffer& input,
                                           CerebrasWaferBuffer& output,
                                           uint32_t batchSize, uint32_t seqLen);
    CerebrasAccelResult dispatchMatMul(const CerebrasWaferBuffer& A,
                                        const CerebrasWaferBuffer& B,
                                        CerebrasWaferBuffer& C,
                                        uint32_t M, uint32_t N, uint32_t K,
                                        bool fp16 = false);
    CerebrasAccelResult dispatchAttention(const CerebrasWaferBuffer& Q,
                                           const CerebrasWaferBuffer& K,
                                           const CerebrasWaferBuffer& V,
                                           CerebrasWaferBuffer& output,
                                           uint32_t heads, uint32_t seqLen,
                                           uint32_t headDim);
    CerebrasAccelResult dispatchRMSNorm(const CerebrasWaferBuffer& input,
                                         const CerebrasWaferBuffer& weight,
                                         CerebrasWaferBuffer& output,
                                         uint32_t size, float eps);
    CerebrasAccelResult dispatchSoftmax(const CerebrasWaferBuffer& input,
                                         CerebrasWaferBuffer& output,
                                         uint32_t rows, uint32_t cols);
    CerebrasAccelResult dispatchRoPE(CerebrasWaferBuffer& qk, uint32_t seqLen,
                                      uint32_t headDim, uint32_t posOffset,
                                      float theta);
    CerebrasAccelResult dispatchGeneric(const char* kernelName,
                                         const CerebrasWaferBuffer* buffers,
                                         uint32_t bufferCount,
                                         uint32_t coresRequested);

    // Synchronization
    CerebrasAccelResult syncWafer();
    CerebrasAccelResult flushWafer();
    CerebrasAccelResult waitForCompletion(uint32_t timeoutMs = 30000);

    // Integration hooks
    bool shouldUseWSE(CerebrasAccelScope scope) const;
    bool shouldUseWSE(CerebrasAccelScope scope, uint64_t dataBytes) const;

    // Callbacks
    void setConnCallback(CerebrasConnCallback cb, void* userData);
    void setToggleCallback(CerebrasToggleCallback cb, void* userData);
    void setErrorCallback(CerebrasErrorCallback cb, void* userData);
    void setStreamCallback(CerebrasStreamCallback cb, void* userData);

    // Stats & serialization
    const CerebrasAccelStats& getStats() const { return m_stats; }
    void                      resetStats();
    std::string               toJson() const;
    std::string               waferMemoryToJson() const;
    std::string               featuresToJson() const;
    std::string               connectionToJson() const;

private:
    CerebrasWSEAccelerator();
    ~CerebrasWSEAccelerator();
    CerebrasWSEAccelerator(const CerebrasWSEAccelerator&) = delete;
    CerebrasWSEAccelerator& operator=(const CerebrasWSEAccelerator&) = delete;

    // Internal init
    CerebrasAccelResult initTCP(const CerebrasEndpoint& ep);
    CerebrasAccelResult initGRPC(const CerebrasEndpoint& ep);
    CerebrasAccelResult initREST(const CerebrasEndpoint& ep);
    CerebrasAccelResult initWeightStreamSDK();
    CerebrasAccelResult probeWafer();       // Query appliance for WSE generation/cores/features
    CerebrasAccelResult probeFeatures();

    // Network I/O (internal)
    CerebrasAccelResult sendRequest(const void* data, uint64_t bytes);
    CerebrasAccelResult recvResponse(void* buffer, uint64_t maxBytes, uint64_t& bytesRead);

    // State
    std::mutex                     m_mutex;
    std::atomic<bool>              m_initialized{false};
    std::atomic<bool>              m_wseEnabled{false};
    std::atomic<uint8_t>           m_enabledScopes{static_cast<uint8_t>(CerebrasAccelScope::All)};
    std::atomic<CerebrasConnState> m_connState{CerebrasConnState::Disconnected};
    CerebrasBackend                m_activeBackend;
    CerebrasWSEGen                 m_wseGen;
    uint32_t                       m_cerebrasFeatures;
    std::string                    m_applianceName;
    std::string                    m_firmwareVersion;

    // Network
    CerebrasEndpoint m_endpoint;
#ifdef _WIN32
    SOCKET           m_socket;
#else
    int              m_socket;
#endif
    HMODULE          m_grpcModule;

    // Wafer state
    CerebrasWaferMemory            m_waferMemory;
    std::vector<CerebrasWaferBuffer> m_allocatedBuffers;
    uint32_t                       m_nextBufferId;

    // Weight streaming state
    bool       m_streamActive;
    uint64_t   m_streamTotalBytes;
    uint64_t   m_streamSentBytes;
    std::string m_streamModelPath;

    // Thresholds
    uint64_t m_wseMinBytes; // Minimum data size to justify network roundtrip

    // Callbacks
    CerebrasConnCallback   m_connCb;     void* m_connData;
    CerebrasToggleCallback m_toggleCb;   void* m_toggleData;
    CerebrasErrorCallback  m_errorCb;    void* m_errorData;
    CerebrasStreamCallback m_streamCb;   void* m_streamData;

    // Stats
    CerebrasAccelStats m_stats;
};

#endif // CEREBRAS_WSE_ACCELERATOR_H
