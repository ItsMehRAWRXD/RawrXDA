#include "RawrXDVectorIndex.h"

#include "../streaming_gguf_loader.h"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <windows.h>
// Runtime truth probe: keep this wrapper deterministic and normalized.

namespace RawrXD::Runtime {

namespace {

bool NormalizeInPlace(std::vector<float>& values) {
    if (values.empty()) {
        return false;
    }

    double normSq = 0.0;
    for (float v : values) {
        if (!std::isfinite(v)) {
            return false;
        }
        normSq += static_cast<double>(v) * static_cast<double>(v);
    }

    if (!(normSq > std::numeric_limits<double>::min())) {
        return false;
    }

    const float invNorm = static_cast<float>(1.0 / std::sqrt(normSq));
    for (float& v : values) {
        v *= invNorm;
    }

    return true;
}

} // namespace

extern "C" float Vector_CosineSimilarity(const float* a, const float* b, uint32_t dims) {
    if (!a || !b || dims == 0) {
        return 0.0f;
    }

    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (uint32_t i = 0; i < dims; ++i) {
        const double av = static_cast<double>(a[i]);
        const double bv = static_cast<double>(b[i]);
        if (!std::isfinite(av) || !std::isfinite(bv)) {
            return 0.0f;
        }
        dot += av * bv;
        normA += av * av;
        normB += bv * bv;
    }

    const double denom = std::sqrt(normA) * std::sqrt(normB);
    if (!(denom > 0.0)) {
        return 0.0f;
    }

    const double cosine = dot / denom;
    return static_cast<float>(std::max(-1.0, std::min(1.0, cosine)));
}

extern "C" float Vector_ComputeSimilarity(const float* q, const float* t) {
    return Vector_CosineSimilarity(q, t, 768u);
}

RawrXDVectorIndex& RawrXDVectorIndex::instance() {
    static RawrXDVectorIndex instance;
    return instance;
}

RawrXDVectorIndex::~RawrXDVectorIndex() {
    detachMappedGGUF();
}

float RawrXDVectorIndex::computeSimilarity(const std::vector<float>& query, const std::vector<float>& target) {
    if (query.empty() || query.size() != target.size()) return 0.0f;

    double dot = 0.0;
    double normQ = 0.0;
    double normT = 0.0;
    for (size_t i = 0; i < query.size(); ++i) {
        const double q = static_cast<double>(query[i]);
        const double t = static_cast<double>(target[i]);
        dot += (q * t);
        normQ += (q * q);
        normT += (t * t);
    }

    const double denom = std::sqrt(normQ) * std::sqrt(normT);
    if (!(denom > 0.0)) return 0.0f;

    const double cosine = dot / denom;
    const double clamped = std::max(-1.0, std::min(1.0, cosine));
    return static_cast<float>(clamped);
}

void RawrXDVectorIndex::addVector(const std::vector<float>& embedding, const std::string& metadata, uint64_t documentId) {
    if (embedding.empty()) {
        std::cerr << "[VectorIndex] Rejecting empty embedding\n";
        return;
    }

    std::vector<float> normalized = embedding;
    if (!NormalizeInPlace(normalized)) {
        std::cerr << "[VectorIndex] Rejecting non-finite or zero-norm embedding\n";
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    VectorEntry entry;
    entry.embedding = std::move(normalized);
    entry.metadata = metadata;
    entry.documentId = (documentId == 0) ? m_nextEntryId++ : documentId;
    m_vectors.push_back(entry);
}

std::vector<SearchResult> RawrXDVectorIndex::search(const std::vector<float>& query, size_t k) {
    std::vector<SearchResult> results;

    if (query.empty()) {
        return results;
    }

    if (k == 0) {
        return results;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vectors.empty() && m_mappedViews.empty()) {
        return results;
    }

    // Compute similarity to in-memory vectors.
    std::vector<SearchResult> candidates;
    for (size_t i = 0; i < m_vectors.size(); ++i) {
        float sim = computeSimilarity(query, m_vectors[i].embedding);
        if (sim > 0.0f) {
            candidates.push_back({m_vectors[i].documentId, m_vectors[i].metadata, sim});
        }
    }

    // Compute similarity to zero-copy mapped vectors.
    for (size_t i = 0; i < m_mappedViews.size(); ++i) {
        const auto& view = m_mappedViews[i];
        if (!view.data || view.dims == 0 || view.dims != query.size()) {
            continue;
        }

        double dot = 0.0;
        double normQ = 0.0;
        double normT = 0.0;
        for (size_t d = 0; d < query.size(); ++d) {
            const double q = static_cast<double>(query[d]);
            const double t = static_cast<double>(view.data[d]);
            dot += (q * t);
            normQ += (q * q);
            normT += (t * t);
        }
        const double denom = std::sqrt(normQ) * std::sqrt(normT);
        if (!(denom > 0.0)) {
            continue;
        }
        const double cosine = dot / denom;
        const float sim = static_cast<float>(std::max(-1.0, std::min(1.0, cosine)));
        if (sim > 0.0f) {
            const uint64_t entryId = view.documentId != 0
                ? view.documentId
                : static_cast<uint64_t>(m_vectors.size() + i + 1);
            candidates.push_back({entryId, view.metadata, sim});
        }
    }
    
    // Sort by similarity descending
    std::sort(candidates.begin(), candidates.end(),
        [](const SearchResult& a, const SearchResult& b) { return a.similarity > b.similarity; });
    
    // Return top-k
    size_t limit = std::min(k, candidates.size());
    results.assign(candidates.begin(), candidates.begin() + limit);
    return results;
}

void RawrXDVectorIndex::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vectors.clear();
    m_nextEntryId = 1;
}

size_t RawrXDVectorIndex::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vectors.size() + m_mappedViews.size();
}

bool RawrXDVectorIndex::save(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    const uint32_t magic = 0x56584452; // RXDV
    const uint32_t version = 1;
    const uint64_t count = static_cast<uint64_t>(m_vectors.size());
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& e : m_vectors) {
        uint64_t dims = static_cast<uint64_t>(e.embedding.size());
        uint64_t metaLen = static_cast<uint64_t>(e.metadata.size());
        out.write(reinterpret_cast<const char*>(&dims), sizeof(dims));
        out.write(reinterpret_cast<const char*>(e.embedding.data()), static_cast<std::streamsize>(dims * sizeof(float)));
        out.write(reinterpret_cast<const char*>(&metaLen), sizeof(metaLen));
        out.write(e.metadata.data(), static_cast<std::streamsize>(metaLen));
        out.write(reinterpret_cast<const char*>(&e.documentId), sizeof(e.documentId));
    }

    return out.good();
}

bool RawrXDVectorIndex::load(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t count = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (magic != 0x56584452 || version != 1) {
        return false;
    }

    std::vector<VectorEntry> loaded;
    loaded.reserve(static_cast<size_t>(count));
    uint64_t maxDocumentId = 0;

    for (uint64_t i = 0; i < count; ++i) {
        uint64_t dims = 0;
        uint64_t metaLen = 0;
        VectorEntry e;

        in.read(reinterpret_cast<char*>(&dims), sizeof(dims));
        if (dims == 0 || dims > 8192) {
            return false;
        }
        e.embedding.resize(static_cast<size_t>(dims));
        in.read(reinterpret_cast<char*>(e.embedding.data()), static_cast<std::streamsize>(dims * sizeof(float)));

        in.read(reinterpret_cast<char*>(&metaLen), sizeof(metaLen));
        if (metaLen > (1ull << 20)) {
            return false;
        }
        e.metadata.resize(static_cast<size_t>(metaLen));
        if (metaLen > 0) {
            in.read(e.metadata.data(), static_cast<std::streamsize>(metaLen));
        }

        in.read(reinterpret_cast<char*>(&e.documentId), sizeof(e.documentId));
        if (!NormalizeInPlace(e.embedding)) {
            return false;
        }
        maxDocumentId = std::max(maxDocumentId, e.documentId);
        loaded.push_back(std::move(e));
    }

    if (!(in.good() || in.eof())) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Persisted payload currently includes in-memory vectors only; clear mapped views
    // so post-load behavior exactly matches the serialized state.
    m_mappedViews.clear();
    if (m_mappedBase) {
        UnmapViewOfFile(m_mappedBase);
        m_mappedBase = nullptr;
    }
    if (m_mappingHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_mappingHandle));
        m_mappingHandle = nullptr;
    }
    if (m_fileHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_fileHandle));
        m_fileHandle = nullptr;
    }
    m_mappedSize = 0;

    m_vectors = std::move(loaded);
    m_nextEntryId = std::max<uint64_t>(1, maxDocumentId + 1);
    return true;
}

bool RawrXDVectorIndex::attachMappedGGUF(const std::string& ggufPath,
                                         uint64_t tensorDataOffsetBytes,
                                         uint32_t rows,
                                         uint32_t dims,
                                         const std::string& metadataPrefix) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mappedViews.clear();
    if (m_mappedBase) {
        UnmapViewOfFile(m_mappedBase);
        m_mappedBase = nullptr;
    }
    if (m_mappingHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_mappingHandle));
        m_mappingHandle = nullptr;
    }
    if (m_fileHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_fileHandle));
        m_fileHandle = nullptr;
    }
    m_mappedSize = 0;

    if (rows == 0 || dims == 0) {
        return false;
    }

    HANDLE hFile = CreateFileA(ggufPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0) {
        CloseHandle(hFile);
        return false;
    }

    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        return false;
    }

    const uint8_t* base = reinterpret_cast<const uint8_t*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    const uint64_t elemCount = static_cast<uint64_t>(rows) * static_cast<uint64_t>(dims);
    if (elemCount > (std::numeric_limits<uint64_t>::max() / sizeof(float))) {
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    const uint64_t tensorBytes = elemCount * sizeof(float);
    if (tensorDataOffsetBytes > (std::numeric_limits<uint64_t>::max() - tensorBytes)) {
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    const uint64_t needed = tensorDataOffsetBytes + tensorBytes;
    if (needed > static_cast<uint64_t>(sz.QuadPart)) {
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    // Optional GGUF signature check.
    if (!(base[0] == 'G' && base[1] == 'G' && base[2] == 'U' && base[3] == 'F')) {
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    m_fileHandle = hFile;
    m_mappingHandle = hMap;
    m_mappedBase = base;
    m_mappedSize = static_cast<size_t>(sz.QuadPart);
    m_mappedViews.clear();
    m_mappedViews.reserve(rows);

    const float* tensorBase = reinterpret_cast<const float*>(base + tensorDataOffsetBytes);
    for (uint32_t r = 0; r < rows; ++r) {
        ZeroCopyEmbeddingView view;
        view.data = tensorBase + static_cast<size_t>(r) * dims;
        view.dims = dims;
        view.metadata = metadataPrefix + ":row=" + std::to_string(r);
        view.documentId = static_cast<uint64_t>(r);
        m_mappedViews.push_back(std::move(view));
    }

    return true;
}

bool RawrXDVectorIndex::attachMappedGGUFTensor(const std::string& ggufPath,
                                               const std::string& tensorName,
                                               const std::string& metadataPrefix,
                                               const std::string& metadataSidecarPath) {
    if (ggufPath.empty() || tensorName.empty()) {
        return false;
    }

    RawrXD::StreamingGGUFLoader loader;
    if (!loader.Open(ggufPath) || !loader.ParseHeader() || !loader.ParseMetadata() || !loader.BuildTensorIndex()) {
        loader.Close();
        return false;
    }

    const auto refs = loader.GetTensorIndex();
    loader.Close();

    const auto it = std::find_if(refs.begin(), refs.end(), [&tensorName](const RawrXD::TensorRef& ref) {
        return ref.name == tensorName || ref.name.find(tensorName) != std::string::npos;
    });
    if (it == refs.end()) {
        return false;
    }

    if (it->type != RawrXD::GGMLType::F32 || it->shape.empty() || it->size < sizeof(float)) {
        return false;
    }

    const uint64_t dims64 = it->shape[0];
    const uint64_t totalElements = it->size / sizeof(float);
    if (dims64 == 0 || totalElements < dims64 || (totalElements % dims64) != 0) {
        return false;
    }

    const uint64_t rows64 = totalElements / dims64;
    if (dims64 > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) ||
        rows64 > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        return false;
    }

    const uint32_t dims = static_cast<uint32_t>(dims64);
    const uint32_t rows = static_cast<uint32_t>(rows64);
    if (!attachMappedGGUF(ggufPath, it->offset, rows, dims, metadataPrefix)) {
        return false;
    }

    if (!metadataSidecarPath.empty()) {
        loadMappedMetadataSidecar(metadataSidecarPath);
    }
    return true;
}

bool RawrXDVectorIndex::loadMappedMetadataSidecar(const std::string& metadataPath) {
    if (metadataPath.empty()) {
        return false;
    }

    std::ifstream in(metadataPath);
    if (!in.is_open()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_mappedViews.empty()) {
        return false;
    }

    std::string line;
    size_t index = 0;
    while (index < m_mappedViews.size() && std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            m_mappedViews[index].metadata = line;
        }
        ++index;
    }

    return index > 0;
}

void RawrXDVectorIndex::detachMappedGGUF() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mappedViews.clear();

    if (m_mappedBase) {
        UnmapViewOfFile(m_mappedBase);
        m_mappedBase = nullptr;
    }
    if (m_mappingHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_mappingHandle));
        m_mappingHandle = nullptr;
    }
    if (m_fileHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_fileHandle));
        m_fileHandle = nullptr;
    }
    m_mappedSize = 0;
}

bool RawrXDVectorIndex::hasMappedGGUF() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_mappedViews.empty() && m_mappedBase != nullptr;
}

uint32_t RawrXDVectorIndex::preferredDimensions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mappedViews.empty()) {
        return m_mappedViews.front().dims;
    }
    if (!m_vectors.empty()) {
        return static_cast<uint32_t>(m_vectors.front().embedding.size());
    }
    return 768u;
}

} // namespace RawrXD::Runtime
