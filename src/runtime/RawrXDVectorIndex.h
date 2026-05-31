#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD::Runtime {

extern "C" float Vector_ComputeSimilarity(const float* q, const float* t);
extern "C" float Vector_CosineSimilarity(const float* a, const float* b, uint32_t dims);

struct VectorEntry {
    std::vector<float> embedding;
    std::string metadata;
    uint64_t documentId;
};

struct SearchResult {
    uint64_t entryId;
    std::string metadata;
    float similarity;
};

// Zero-copy row view into mapped GGUF-backed embedding memory.
struct ZeroCopyEmbeddingView {
    const float* data = nullptr;
    uint32_t dims = 0;
    std::string metadata;
    uint64_t documentId = 0;
};

class RawrXDVectorIndex {
public:
    static RawrXDVectorIndex& instance();

    float computeSimilarity(const std::vector<float>& query, const std::vector<float>& target);

    void addVector(const std::vector<float>& embedding, const std::string& metadata, uint64_t documentId);
    std::vector<SearchResult> search(const std::vector<float>& query, size_t k = 10);
    void clear();
    size_t size() const;

    // In-memory persistence for local vectors.
    bool save(const std::string& filepath);
    bool load(const std::string& filepath);

    // Memory-mapped GGUF-backed embeddings.
    // The caller provides tensor offset and shape inside GGUF payload to avoid new parser deps.
    bool attachMappedGGUF(const std::string& ggufPath,
                          uint64_t tensorDataOffsetBytes,
                          uint32_t rows,
                          uint32_t dims,
                          const std::string& metadataPrefix = "gguf");
    bool attachMappedGGUFTensor(const std::string& ggufPath,
                                const std::string& tensorName,
                                const std::string& metadataPrefix = "gguf",
                                const std::string& metadataSidecarPath = "");
    bool loadMappedMetadataSidecar(const std::string& metadataPath);
    void detachMappedGGUF();
    bool hasMappedGGUF() const;
    uint32_t preferredDimensions() const;

private:
    RawrXDVectorIndex() = default;
    ~RawrXDVectorIndex();

    // Windows mapping handles (zero dependency: Win32 API only)
    void* m_fileHandle = nullptr;
    void* m_mappingHandle = nullptr;
    const uint8_t* m_mappedBase = nullptr;
    size_t m_mappedSize = 0;

    std::vector<VectorEntry> m_vectors;
    std::vector<ZeroCopyEmbeddingView> m_mappedViews;
    uint64_t m_nextEntryId = 1;
    mutable std::mutex m_mutex;
};

} // namespace RawrXD::Runtime
