#pragma once

// Phase 27 - ZENITH: Omniscient-Sovereign MoE Engine
// 800B Out-of-Core Execution via Aggressive Mixture-of-Experts Router
// with real softmax top-K routing, capacity enforcement, and NVMe streaming.

#include <algorithm>
#include <cmath>
#include <cstring>

typedef unsigned long long UINT64;
typedef unsigned int       UINT32;
typedef unsigned short     UINT16;
typedef unsigned char      UINT8;
typedef int                BOOL;
typedef void*              HANDLE;

#define NULL_PTR ((void*)0)
#define TRUE 1
#define FALSE 0

#define SHARD_2BIT_QUANT (0x02)
#define MAX_ACTIVE_EXPERTS (4) // 16GB VRAM limit allows ~4 8B experts at 2-bit
#define KV_COMPRESSION_RATIO (0x08) // 8x Spatial Holographic Compression

struct ExpertDescriptor {
    UINT32 ExpertID;
    UINT64 DiskOffset;
    UINT64 ByteSize;
    HANDLE FileHandle;
    void*  VramDestPtr;
};

struct KVCacheHolo {
    UINT64 SequenceID;
    UINT64 RawTokenCount;
    UINT64 CompressedByteSize;
    void*  L2Ddr5Buffer;
};

class ZenithMoERouter {
public:
    ZenithMoERouter();
    ~ZenithMoERouter();

    // ---- Router weight initialisation ----------------------------------------
    // Call once after loading the model shard.  Weights layout:
    //   weights[expert_id * hidden_dim + dim] = W_r[expert_id][dim]
    // This mirrors the standard linear projection: logits = W_r @ hidden_state
    BOOL InitializeRouterWeights(float* Weights, UINT32 HiddenDim, UINT32 NumExperts);

    // ---- Primary routing entry-points ----------------------------------------

    // Full-fidelity: route using the actual hidden state from the transformer.
    // Performs W_r @ hidden_state → softmax → top-K with capacity enforcement.
    BOOL RouteWithHiddenState(float* HiddenState,
                              ExpertDescriptor* OutExperts, UINT32& ExpertCount);

    // Convenience: token-hash synthetic logits (benchmarking / warm-up only).
    BOOL RouteTokenToExperts(UINT64 TokenID,
                             ExpertDescriptor* OutExperts, UINT32& ExpertCount);

    // ---- Streaming & KV ops --------------------------------------------------

    // Asynchronous NVMe -> VRAM using FILE_FLAG_NO_BUFFERING & Overlapped I/O
    BOOL DispatchExpertStreaming(ExpertDescriptor* Experts, UINT32 Count);

    // KV Cache Semantic folding for the "Infinite" Context illusion
    BOOL FoldKVCache(KVCacheHolo* CacheSegment);

    // ---- Observability -------------------------------------------------------
    UINT64 GetTotalRoutedTokens() const { return m_TotalRoutedTokens; }
    UINT64 GetDroppedTokens()     const { return m_DroppedTokens; }
    void   SetCapacityFactor(float cf)  { m_CapacityFactor = cf; }

private:
    // Internal shared routing: both public entry-points call this after
    // producing logits[0..m_NumExperts-1].
    BOOL RouteFromLogits(float* Logits, ExpertDescriptor* OutExperts, UINT32& ExpertCount);

    void* m_OverlappedPool;    // Pinned memory pool for IOCP
    UINT64 m_TotalRoutedTokens;
    UINT64 m_DroppedTokens;    // Tokens dropped due to expert capacity overflow

    float* m_RouterWeights;    // [m_NumExperts × m_HiddenDim] row-major
    UINT32 m_HiddenDim;        // Size of hidden state vector
    UINT32 m_NumExperts;       // Total number of experts (e.g. 128 for 800B)
    float  m_CapacityFactor;   // Expert capacity = factor × (tokens / num_experts)
                               // Typical values: 1.25 (training), 2.0 (inference)

    // Per-expert token-count accumulator for capacity enforcement within a batch.
    // Reset each RouteFromLogits call when m_BatchTokenCount reaches the window.
    UINT32 m_ExpertTokenCount[256]; // max num_experts assumed <= 256 for stack alloc
    UINT32 m_BatchWindow;           // Batch size for capacity window (default 512)
    UINT32 m_BatchTokenCount;       // Tokens processed since last capacity reset
};
