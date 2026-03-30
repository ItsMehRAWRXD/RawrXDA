// d:\rawrxd\src\kernels\RawrXD_AVX512_Semantic_Bridge.hpp
// C++ bridge interface for AVX-512 semantic search kernels
// Maps MASM exports to typed C++ signatures for type-safe caller sites

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace RawrXD {

// ============================================================================
// MASM Kernel Exports (extern "C" with no-mangle)
// ============================================================================

extern "C" {
    // Q8·Q8 dot product batch (256-dim vectors)
    void RawrXD_Q8_CosineBatch(
        const int8_t* vectors,      // Vector array (contiguous, row-major)
        const int8_t* query,        // Query vector (256 int8 values)
        size_t vecCount,            // Number of vectors
        size_t vecDim,              // Dimension (typically 256)
        float* outScores,           // Output scores array (length >= vecCount)
        float scale                 // Scale factor for normalization
    );
    
    // Top-K selection via min-heap
    void RawrXD_TopK_Heapify(
        float* scores,              // Input scores
        uint32_t* indices,          // Input indices
        size_t count,               // Total count
        uint32_t k,                 // K for top-k selection
        float* outScores,           // Output top-k scores (preallocated)
        uint32_t* outIndices        // Output top-k indices (preallocated)
    );
    
    // Register capture for deterministic execution state
    void RawrXD_CaptureExecutionState(void* snapPtr);
    
    // Non-temporal vectorized compare
    bool RawrXD_StreamCompareNT(
        const void* src1,
        const void* src2,
        size_t length
    );
    
    // Software prefetch for index pages
    void RawrXD_PrefetchIndexPages(
        const void* base,
        size_t pageCount,
        size_t stride
    );
    
    // Non-temporal memcpy
    void RawrXD_MemcpyNT(
        void* dst,
        const void* src,
        size_t size
    );
    
    // F32·F32 dot product (bonus kernel)
    void RawrXD_F32_DotBatch(
        const float* vectors,
        const float* query,
        size_t vecCount,
        size_t vecDim,
        float* outScores,
        float scale
    );
}

// ============================================================================
// C++ Type-Safe Wrapper Class
// ============================================================================

class AVX512SemanticKernels {
public:
    /// Compute batch cosine similarity (Q8·Q8 via MASM)
    static void computeQ8CosineBatch(
        const int8_t* vectors,
        const int8_t* query,
        size_t vectorCount,
        size_t dimension,
        std::vector<float>& outScores,
        float scale = 1.0f
    ) {
        if (vectorCount == 0 || dimension == 0) {
            outScores.clear();
            return;
        }
        
        outScores.resize(vectorCount, 0.0f);
        RawrXD_Q8_CosineBatch(
            vectors, query, vectorCount, dimension,
            outScores.data(), scale
        );
    }
    
    /// Select top-K scores using min-heap
    static void selectTopK(
        const std::vector<float>& scores,
        const std::vector<uint32_t>& indices,
        uint32_t k,
        std::vector<float>& outScores,
        std::vector<uint32_t>& outIndices
    ) {
        if (k == 0 || scores.empty()) {
            outScores.clear();
            outIndices.clear();
            return;
        }
        
        uint32_t actualK = static_cast<uint32_t>(scores.size() < k ? scores.size() : k);
        outScores.resize(actualK);
        outIndices.resize(actualK);
        
        RawrXD_TopK_Heapify(
            const_cast<float*>(scores.data()),
            const_cast<uint32_t*>(indices.data()),
            scores.size(),
            actualK,
            outScores.data(),
            outIndices.data()
        );
    }
    
    /// Verify memory identity (for deterministic replay validation)
    static bool verifyMemoryIdentity(
        const void* expected,
        const void* actual,
        size_t size
    ) {
        if (size == 0) return true;
        return RawrXD_StreamCompareNT(expected, actual, size) != 0;
    }
    
    /// Prefetch index pages for zero-copy search
    static void prefetchIndexPages(
        const void* baseAddr,
        size_t pageCount,
        size_t stride = 4096  // Default: 4KB page stride
    ) {
        RawrXD_PrefetchIndexPages(baseAddr, pageCount, stride);
    }
    
    /// Non-temporal copy for large buffers
    static void copyNonTemporal(
        void* dst,
        const void* src,
        size_t size
    ) {
        RawrXD_MemcpyNT(dst, src, size);
    }
};

}  // namespace RawrXD
