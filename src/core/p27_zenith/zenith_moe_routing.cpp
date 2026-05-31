#include "zenith_moe_routing.hpp"

// MASM Kernel Hooks for OS-bypass functions
extern "C" void* MASM_AsyncRead_NoBuffer(HANDLE hFile, void* Dest, UINT64 Size, UINT64 Offset);
extern "C" BOOL MASM_SIMD_CompressKV(void* Src, void* Dest, UINT64 RawSize, UINT64 CompressedSize);
extern "C" void MASM_Trigger_GPU_Dispatch(void* VramPtr, UINT64 Size);

ZenithMoERouter::ZenithMoERouter() {
    m_TotalRoutedTokens = 0;
    m_DroppedTokens     = 0;
    m_OverlappedPool    = NULL_PTR;
    m_RouterWeights     = NULL_PTR;
    m_HiddenDim         = 0;
    m_NumExperts        = 128; // default: 800B Mixtral-style 128-expert layout
    m_CapacityFactor    = 2.0f; // inference-mode capacity (looser than training)
    m_BatchWindow       = 512;
    m_BatchTokenCount   = 0;
    std::memset(m_ExpertTokenCount, 0, sizeof(m_ExpertTokenCount));
}

ZenithMoERouter::~ZenithMoERouter() {
    // Router weight memory is externally owned (model shard mapping).
    // Cleanup of m_OverlappedPool delegated to Sovereign Shutdown.
}

BOOL ZenithMoERouter::InitializeRouterWeights(float* Weights, UINT32 HiddenDim, UINT32 NumExperts) {
    if (!Weights || HiddenDim == 0 || NumExperts == 0 || NumExperts > 256) return FALSE;
    m_RouterWeights = Weights;
    m_HiddenDim     = HiddenDim;
    m_NumExperts    = NumExperts;
    m_DroppedTokens = 0;
    m_BatchTokenCount = 0;
    std::memset(m_ExpertTokenCount, 0, sizeof(m_ExpertTokenCount));
    return TRUE;
}

// ============================================================================
// RouteFromLogits — shared routing kernel.
//
// Inputs:  Logits[0..m_NumExperts-1] — raw router scores (pre-softmax)
// Outputs: OutExperts[0..ExpertCount-1] filled with top-K experts by probability
//
// Algorithm:
//   1. Stable numerics: subtract max before exp (log-sum-exp trick)
//   2. Compute softmax probabilities over all experts
//   3. Partial-sort O(num_experts × log(K)) to identify top-K indices
//   4. Capacity enforcement: skip experts already at token capacity
//   5. Fill OutExperts with selected ExpertIDs and their routing probabilities
// ============================================================================
BOOL ZenithMoERouter::RouteFromLogits(float* Logits, ExpertDescriptor* OutExperts, UINT32& ExpertCount) {
    const UINT32 n = m_NumExperts;

    // --- Softmax with numerically stable max subtraction ---
    float maxLogit = Logits[0];
    for (UINT32 e = 1; e < n; ++e) {
        if (Logits[e] > maxLogit) maxLogit = Logits[e];
    }

    float sumExp = 0.0f;
    for (UINT32 e = 0; e < n; ++e) {
        Logits[e] = expf(Logits[e] - maxLogit); // reuse array for softmax probs
        sumExp += Logits[e];
    }
    const float invSum = 1.0f / (sumExp + 1e-10f);
    for (UINT32 e = 0; e < n; ++e) Logits[e] *= invSum;

    // --- Capacity limit per expert ---
    // Capacity = floor(capacity_factor × tokens_per_expert)
    // where tokens_per_expert = m_BatchWindow / m_NumExperts
    const UINT32 tokenCapacity = static_cast<UINT32>(
        m_CapacityFactor * (static_cast<float>(m_BatchWindow) / static_cast<float>(n)) + 0.5f);
    const UINT32 maxCap = (tokenCapacity < 1) ? 1 : tokenCapacity;

    // Reset per-expert counters every batch window
    m_BatchTokenCount++;
    if (m_BatchTokenCount >= m_BatchWindow) {
        m_BatchTokenCount = 0;
        std::memset(m_ExpertTokenCount, 0, sizeof(UINT32) * n);
    }

    // --- Top-K selection via linear scan (K ≤ MAX_ACTIVE_EXPERTS = 4) ---
    // For K=2–4 experts, a linear scan outperforms partial_sort on typical
    // cache-resident arrays of 128 floats (no heap allocation, fully predictable).
    UINT32 selected = 0;
    UINT32 remaining = n;

    while (selected < MAX_ACTIVE_EXPERTS && remaining > 0) {
        // Find expert with highest probability not yet selected
        float   bestProb  = -1.0f;
        UINT32  bestIndex = 0;
        for (UINT32 e = 0; e < n; ++e) {
            if (Logits[e] > bestProb) {
                bestProb  = Logits[e];
                bestIndex = e;
            }
        }
        Logits[bestIndex] = -1.0f; // mark consumed
        --remaining;

        // Capacity gate: skip overflowed experts rather than dropping the token
        if (m_ExpertTokenCount[bestIndex] >= maxCap) {
            ++m_DroppedTokens;
            continue; // try next-best expert
        }

        m_ExpertTokenCount[bestIndex]++;
        OutExperts[selected].ExpertID     = bestIndex;
        // Expert shard layout: each expert is a contiguous slab on NVMe / mapped VRAM.
        // ByteSize = ~8GB at 2-bit quantization for a 7B sub-expert within 800B MoE.
        OutExperts[selected].ByteSize     = static_cast<UINT64>(0x200000000ULL);
        OutExperts[selected].DiskOffset   = static_cast<UINT64>(bestIndex)
                                            * OutExperts[selected].ByteSize;
        OutExperts[selected].FileHandle   = NULL_PTR; // set by caller before Dispatch
        OutExperts[selected].VramDestPtr  = reinterpret_cast<void*>(
            static_cast<uintptr_t>(selected) * OutExperts[selected].ByteSize);
        ++selected;
    }

    ExpertCount = selected;
    ++m_TotalRoutedTokens;
    return (ExpertCount > 0) ? TRUE : FALSE;
}

BOOL ZenithMoERouter::RouteWithHiddenState(float* HiddenState,
                                            ExpertDescriptor* OutExperts, UINT32& ExpertCount) {
    if (!HiddenState || !OutExperts || m_HiddenDim == 0 || !m_RouterWeights) {
        ExpertCount = 0;
        return FALSE;
    }

    // Compute router logits: logits[e] = dot(W_r[e], hidden_state)
    // W_r layout: row e starts at m_RouterWeights + e * m_HiddenDim
    float logits[256];
    for (UINT32 e = 0; e < m_NumExperts; ++e) {
        const float* row = m_RouterWeights + e * m_HiddenDim;
        float acc = 0.0f;
        for (UINT32 d = 0; d < m_HiddenDim; ++d) {
            acc += row[d] * HiddenState[d];
        }
        logits[e] = acc;
    }

    return RouteFromLogits(logits, OutExperts, ExpertCount);
}

BOOL ZenithMoERouter::RouteTokenToExperts(UINT64 TokenID,
                                           ExpertDescriptor* OutExperts, UINT32& ExpertCount) {
    if (!OutExperts) { ExpertCount = 0; return FALSE; }

    // Synthetic logits for benchmarking / warm-up paths:
    // Use a high-quality 64-bit hash (Murmur3-finalizer mix) so different token
    // IDs produce statistically independent expert distributions.
    float logits[256];
    for (UINT32 e = 0; e < m_NumExperts; ++e) {
        UINT64 h = (TokenID ^ (static_cast<UINT64>(e) * 0x9E3779B97F4A7C15ULL))
                   * 0xC4CEB9FE1A85EC53ULL;
        h ^= h >> 33;
        // Map to [-3, +3] range — typical router logit magnitude
        logits[e] = (static_cast<float>(static_cast<UINT32>(h >> 32)) / 1073741824.0f) - 2.0f;
    }

    return RouteFromLogits(logits, OutExperts, ExpertCount);
}

BOOL ZenithMoERouter::DispatchExpertStreaming(ExpertDescriptor* Experts, UINT32 Count) {
    if (Count > MAX_ACTIVE_EXPERTS) return FALSE;

    for (UINT32 i = 0; i < Count; ++i) {
        // Zero-copy, non-buffered NVMe streaming. Peak PCIe 4.0/5.0 utilization.
        // Bypasses CPU cache completely, writing straight to DirectStorage / VRAM.
        MASM_AsyncRead_NoBuffer(
            Experts[i].FileHandle,
            Experts[i].VramDestPtr,
            Experts[i].ByteSize,
            Experts[i].DiskOffset
        );

        // Notify Clock-Edge Dispatch
        MASM_Trigger_GPU_Dispatch(Experts[i].VramDestPtr, Experts[i].ByteSize);
    }
    return TRUE;
}

BOOL ZenithMoERouter::FoldKVCache(KVCacheHolo* CacheSegment) {
    // Deep-History Folding (Semantic Compression).
    // Compresses older, low-attention KV blocks into dense representations.
    // Simulates an infinite context window without blowing up the 64GB DDR5 limit.
    UINT64 targetSize = CacheSegment->RawTokenCount / KV_COMPRESSION_RATIO;
    
    BOOL success = MASM_SIMD_CompressKV(
        CacheSegment->L2Ddr5Buffer, 
        CacheSegment->L2Ddr5Buffer, // In-place compression
        CacheSegment->RawTokenCount * 2, // Assuming FP16 embeddings initially
        targetSize
    );
    
    if (success) {
        CacheSegment->CompressedByteSize = targetSize;
    }
    
    return success;
}

