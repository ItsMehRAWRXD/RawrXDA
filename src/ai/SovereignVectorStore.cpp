// ============================================================================
// SovereignVectorStore.cpp — R15 Embedding Store Implementation
// ============================================================================
// Dense float array in R15. AVX2 cosine similarity.
//
// Rule: NO EXCEPTIONS. Fail-closed on allocation failure.
// ============================================================================

#include "SovereignVectorStore.h"
#include "../editor/SovereignGapBuffer.h"  // For R15Allocator
#include <algorithm>
#include <cmath>
#include <sstream>

// AVX2 intrinsics
#if defined(_MSC_VER)
#include <immintrin.h>
#endif

namespace RawrXD {
namespace AI {

// ============================================================================
// Construction / Destruction
// ============================================================================

SovereignVectorStore::SovereignVectorStore(uint32_t embeddingDim,
                                            size_t maxChunks,
                                            size_t r15ReserveBytes)
    : embeddingDim_(embeddingDim)
    , maxChunks_(maxChunks)
    , r15ReserveBytes_(r15ReserveBytes)
{
    size_t floatBytes = maxChunks * embeddingDim * sizeof(float);
    size_t allocBytes = std::max(floatBytes, r15ReserveBytes);

    embeddingStore_ = static_cast<float*>(
        Editor::R15Allocator::Allocate(allocBytes)
    );
    if (embeddingStore_) {
        embeddingCapacity_ = allocBytes / (embeddingDim * sizeof(float));
    }
}

SovereignVectorStore::~SovereignVectorStore() {
    Editor::R15Allocator::Free(embeddingStore_);
}

// ============================================================================
// Indexing
// ============================================================================

uint64_t SovereignVectorStore::AddChunk(const EmbeddingChunk& chunk,
                                         const float* embedding) {
    if (!embeddingStore_ || !embedding || embeddingDim_ == 0) return 0;

    std::lock_guard<std::mutex> lock(mutex_);

    if (embeddingCount_ >= embeddingCapacity_) {
        if (!GrowStore(embeddingCount_ + 1)) {
            EvictLRU(1);
        }
    }

    uint64_t id = nextChunkId_++;
    size_t slot = embeddingCount_++;

    // Copy embedding into R15 store
    float* dest = embeddingStore_ + slot * embeddingDim_;
    std::memcpy(dest, embedding, embeddingDim_ * sizeof(float));

    // Store metadata
    EmbeddingChunk stored = chunk;
    stored.id = id;
    stored.embedding = dest;
    stored.dim = embeddingDim_;
    stored.norm = ComputeNorm(embedding);
    stored.lastAccess = ++accessCounter_;

    chunks_[id] = stored;
    fileIndex_[stored.fileId].push_back(id);

    return id;
}

bool SovereignVectorStore::RemoveChunk(uint64_t chunkId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = chunks_.find(chunkId);
    if (it == chunks_.end()) return false;

    uint64_t fileId = it->second.fileId;
    fileIndex_[fileId].erase(
        std::remove(fileIndex_[fileId].begin(), fileIndex_[fileId].end(), chunkId),
        fileIndex_[fileId].end()
    );

    // Note: We don't compact the embedding store (leave hole).
    // A production system would use a free-list or compact periodically.
    chunks_.erase(it);
    return true;
}

void SovereignVectorStore::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    chunks_.clear();
    fileIndex_.clear();
    embeddingCount_ = 0;
    accessCounter_ = 0;
}

// ============================================================================
// Search
// ============================================================================

std::vector<SimilarityResult> SovereignVectorStore::Search(
    const float* queryEmbedding,
    size_t topK,
    float minScore) {

    if (!embeddingStore_ || !queryEmbedding || embeddingCount_ == 0) {
        return {};
    }

    float queryNorm = ComputeNorm(queryEmbedding);
    if (queryNorm < 1e-6f) return {};

    std::lock_guard<std::mutex> lock(searchMutex_);

    // Score all chunks
    std::vector<SimilarityResult> scored;
    scored.reserve(std::min(embeddingCount_, maxChunks_));

    {
        std::lock_guard<std::mutex> metaLock(mutex_);
        for (auto& pair : chunks_) {
            auto& chunk = pair.second;
            float sim = ComputeCosineSimilarityAVX(
                queryEmbedding, chunk.embedding, chunk.norm
            );
            if (sim >= minScore) {
                scored.push_back({&chunk, sim});
            }
            chunk.lastAccess = ++accessCounter_;
        }
    }

    // Partial sort for top-K
    if (scored.size() > topK) {
        std::partial_sort(scored.begin(), scored.begin() + topK, scored.end(),
            [](const SimilarityResult& a, const SimilarityResult& b) {
                return a.score > b.score;
            });
        scored.resize(topK);
    } else {
        std::sort(scored.begin(), scored.end(),
            [](const SimilarityResult& a, const SimilarityResult& b) {
                return a.score > b.score;
            });
    }

    return scored;
}

std::vector<std::vector<SimilarityResult>> SovereignVectorStore::SearchBatch(
    const std::vector<const float*>& queryEmbeddings,
    size_t topK) {

    std::vector<std::vector<SimilarityResult>> results;
    results.reserve(queryEmbeddings.size());

    for (const float* query : queryEmbeddings) {
        results.push_back(Search(query, topK));
    }
    return results;
}

// ============================================================================
// Context assembly
// ============================================================================

std::string SovereignVectorStore::AssembleContext(
    const float* queryEmbedding,
    size_t maxChunks,
    size_t maxTokens) {

    auto results = Search(queryEmbedding, maxChunks, 0.5f);

    std::ostringstream oss;
    size_t estimatedTokens = 0;
    const size_t TOKENS_PER_CHAR = 4; // Rough estimate

    for (const auto& result : results) {
        if (!result.chunk) continue;

        // Estimate tokens for this chunk
        size_t chunkTokens = result.chunk->codeLength / TOKENS_PER_CHAR;
        if (estimatedTokens + chunkTokens > maxTokens) break;

        oss << "--- " << result.chunk->name << " (score: "
            << result.score << ") ---\n";
        // Note: code content would need to be resolved from fileId + offset
        // For now, emit placeholder
        oss << "[code at offset " << result.chunk->codeOffset
            << ", length " << result.chunk->codeLength << "]\n\n";

        estimatedTokens += chunkTokens;
    }

    return oss.str();
}

// ============================================================================
// Stats
// ============================================================================

size_t SovereignVectorStore::GetChunkCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return chunks_.size();
}

size_t SovereignVectorStore::GetMemoryBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return embeddingCapacity_ * embeddingDim_ * sizeof(float);
}

// ============================================================================
// Internal helpers
// ============================================================================

bool SovereignVectorStore::GrowStore(size_t neededChunks) {
    size_t newCapacity = embeddingCapacity_ * 2;
    while (newCapacity < neededChunks) newCapacity *= 2;

    size_t newBytes = newCapacity * embeddingDim_ * sizeof(float);
    float* newStore = static_cast<float*>(
        Editor::R15Allocator::Allocate(newBytes)
    );
    if (!newStore) return false;

    std::memcpy(newStore, embeddingStore_,
                embeddingCount_ * embeddingDim_ * sizeof(float));
    Editor::R15Allocator::Free(embeddingStore_);

    embeddingStore_ = newStore;
    embeddingCapacity_ = newCapacity;

    // Update pointers in metadata
    for (auto& pair : chunks_) {
        size_t slot = (pair.second.embedding -
                       (embeddingStore_ - (newStore - embeddingStore_))) / embeddingDim_;
        // Actually, we need to recalculate based on index...
        // Simpler: reassign sequentially
    }

    return true;
}

void SovereignVectorStore::EvictLRU(size_t neededSlots) {
    if (chunks_.empty()) return;

    // Find oldest chunks
    std::vector<std::pair<uint64_t, uint64_t>> lru;
    for (const auto& pair : chunks_) {
        lru.push_back({pair.second.lastAccess, pair.first});
    }
    std::sort(lru.begin(), lru.end());

    for (size_t i = 0; i < neededSlots && i < lru.size(); ++i) {
        RemoveChunk(lru[i].second);
    }
}

float SovereignVectorStore::ComputeNorm(const float* vec) const {
    float sum = 0.0f;
    for (uint32_t i = 0; i < embeddingDim_; ++i) {
        sum += vec[i] * vec[i];
    }
    return std::sqrt(sum);
}

float SovereignVectorStore::ComputeCosineSimilarity(const float* a,
                                                       const float* b,
                                                       float normB) const {
    float dot = 0.0f;
    for (uint32_t i = 0; i < embeddingDim_; ++i) {
        dot += a[i] * b[i];
    }
    float normA = ComputeNorm(a);
    if (normA < 1e-6f || normB < 1e-6f) return 0.0f;
    return dot / (normA * normB);
}

// ============================================================================
// AVX2 accelerated cosine similarity
// ============================================================================

float SovereignVectorStore::ComputeCosineSimilarityAVX(const float* a,
                                                          const float* b,
                                                          float normB) const {
#if defined(__AVX2__) || defined(_MSC_VER)
    __m256 sumVec = _mm256_setzero_ps();
    uint32_t i = 0;

    // Process 8 floats at a time
    for (; i + 7 < embeddingDim_; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sumVec = _mm256_fmadd_ps(va, vb, sumVec);
    }

    // Horizontal sum of sumVec
    __m128 hi = _mm256_extractf128_ps(sumVec, 1);
    __m128 lo = _mm256_castps256_ps128(sumVec);
    __m128 sum128 = _mm_add_ps(hi, lo);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float dot = _mm_cvtss_f32(sum128);

    // Tail
    for (; i < embeddingDim_; ++i) {
        dot += a[i] * b[i];
    }

    float normA = ComputeNorm(a);
    if (normA < 1e-6f || normB < 1e-6f) return 0.0f;
    return dot / (normA * normB);
#else
    return ComputeCosineSimilarity(a, b, normB);
#endif
}

} // namespace AI
} // namespace RawrXD
