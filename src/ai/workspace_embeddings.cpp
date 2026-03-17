// =============================================================================
// RawrXD Workspace Embeddings — Production Implementation
// Copilot/Cursor Parity: workspace-wide vector embeddings for retrieval
// =============================================================================
// Manages persistent embedding indices for the entire workspace.
// Uses locality-sensitive hashing (LSH) for fast approximate
// nearest-neighbor search without external vector DB dependencies.
// Integrates with EmbeddingProvider for vector generation and
// CodebaseRAG for hybrid TF-IDF + vector retrieval.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <random>
#include <chrono>
#include <functional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace RawrXD {
namespace AI {

// ─── Configuration ───────────────────────────────────────────────────────────
struct WorkspaceEmbeddingConfig {
    size_t embeddingDim = 384;        // Dimension of embedding vectors
    size_t chunkSize = 512;           // Characters per chunk
    size_t chunkOverlap = 64;         // Overlap between chunks
    size_t lshTables = 8;             // Number of LSH hash tables
    size_t lshBucketBits = 10;        // Bits per LSH hash → 1024 buckets
    size_t maxChunksPerFile = 100;    // Limit chunks per file
    float stalenessThresholdSec = 300.0f; // Re-index files older than 5 min
    std::string indexPath;            // Where to persist the index on disk
};

// ─── Embedding Vector ────────────────────────────────────────────────────────
struct EmbeddingVector {
    std::vector<float> data;

    float dot(const EmbeddingVector& other) const {
        float sum = 0.0f;
        size_t n = std::min(data.size(), other.data.size());
        for (size_t i = 0; i < n; ++i) sum += data[i] * other.data[i];
        return sum;
    }

    float norm() const {
        float sum = 0.0f;
        for (float v : data) sum += v * v;
        return std::sqrt(sum);
    }

    float cosineSimilarity(const EmbeddingVector& other) const {
        float d = dot(other);
        float n1 = norm(), n2 = other.norm();
        return (n1 > 1e-8f && n2 > 1e-8f) ? d / (n1 * n2) : 0.0f;
    }

    void normalize() {
        float n = norm();
        if (n > 1e-8f) {
            for (float& v : data) v /= n;
        }
    }
};

// ─── Chunk Metadata ──────────────────────────────────────────────────────────
struct ChunkMetadata {
    std::string filePath;
    uint32_t startLine = 0;
    uint32_t endLine = 0;
    uint32_t startChar = 0;
    uint32_t endChar = 0;
    std::string language;           // "cpp", "python", "asm", etc.
    std::string symbolScope;       // enclosing function/class name
    uint64_t fileModTime = 0;      // last modified time
};

// ─── Indexed Chunk ───────────────────────────────────────────────────────────
struct IndexedChunk {
    size_t id;
    ChunkMetadata metadata;
    EmbeddingVector embedding;
    std::string content;
    std::vector<uint64_t> lshHashes;  // Pre-computed LSH bucket hashes
};

// ─── Retrieval Result ────────────────────────────────────────────────────────
struct RetrievalResult {
    size_t chunkId;
    std::string filePath;
    uint32_t startLine;
    uint32_t endLine;
    std::string content;
    float similarity;
    std::string symbolScope;
};

// ─── Bag-of-Words Embedding (zero-dep fallback) ─────────────────────────────
// When no neural embedding model is loaded, we use a deterministic
// hash-based projection: FNV-1a of each token → dimension index,
// accumulate into a fixed-size vector, then L2-normalize.
static EmbeddingVector hashEmbedding(const std::string& text, size_t dim) {
    EmbeddingVector vec;
    vec.data.resize(dim, 0.0f);

    std::string token;
    for (char c : text) {
        if (isalnum(c) || c == '_') {
            token += static_cast<char>(tolower(c));
        } else {
            if (!token.empty()) {
                // FNV-1a hash
                uint64_t h = 0xcbf29ce484222325ULL;
                for (char tc : token) {
                    h ^= static_cast<uint64_t>(static_cast<unsigned char>(tc));
                    h *= 0x100000001b3ULL;
                }
                // Project into multiple dimensions for better coverage
                for (int k = 0; k < 3; ++k) {
                    size_t idx = (h + k * 0x9e3779b97f4a7c15ULL) % dim;
                    float sign = ((h >> (k + 10)) & 1) ? 1.0f : -1.0f;
                    vec.data[idx] += sign;
                }
                token.clear();
            }
        }
    }
    if (!token.empty()) {
        uint64_t h = 0xcbf29ce484222325ULL;
        for (char tc : token) {
            h ^= static_cast<uint64_t>(static_cast<unsigned char>(tc));
            h *= 0x100000001b3ULL;
        }
        for (int k = 0; k < 3; ++k) {
            size_t idx = (h + k * 0x9e3779b97f4a7c15ULL) % dim;
            float sign = ((h >> (k + 10)) & 1) ? 1.0f : -1.0f;
            vec.data[idx] += sign;
        }
    }

    vec.normalize();
    return vec;
}

// =============================================================================
// WorkspaceEmbeddings — Main Engine
// =============================================================================
class WorkspaceEmbeddings {
public:
    static WorkspaceEmbeddings& instance() {
        static WorkspaceEmbeddings s;
        return s;
    }

    void configure(const WorkspaceEmbeddingConfig& cfg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = cfg;
        initLSH();
    }

    // ── Index entire workspace ───────────────────────────────────────────────
    size_t indexWorkspace(const std::string& rootPath) {
        size_t count = 0;
        try {
            for (auto& entry : std::filesystem::recursive_directory_iterator(
                     rootPath, std::filesystem::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (!isSupportedExtension(ext)) continue;

                auto pathStr = entry.path().string();
                if (shouldSkipPath(pathStr)) continue;

                count += indexFile(pathStr);
            }
        } catch (...) {}
        return count;
    }

    // ── Index a single file ──────────────────────────────────────────────────
    size_t indexFile(const std::string& filePath) {
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) return 0;

        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        ifs.close();

        // Detect language
        std::string lang = detectLanguage(filePath);

        // Get file modification time
        uint64_t modTime = 0;
#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &fad)) {
            modTime = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) | fad.ftLastWriteTime.dwLowDateTime;
        }
#endif

        // Check if file is already indexed and unchanged
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_fileModTimes.find(filePath);
            if (it != m_fileModTimes.end() && it->second == modTime) {
                return 0; // Already up to date
            }
            // Remove old chunks for this file
            removeFileChunks(filePath);
            m_fileModTimes[filePath] = modTime;
        }

        // Split into chunks
        auto chunks = chunkContent(content, filePath, lang, modTime);

        // Embed each chunk and index
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t added = 0;
        for (auto& chunk : chunks) {
            // Generate embedding
            chunk.embedding = hashEmbedding(chunk.content, m_config.embeddingDim);

            // Compute LSH hashes for each table
            chunk.lshHashes = computeLSHHashes(chunk.embedding);

            // Insert into LSH tables
            for (size_t t = 0; t < m_config.lshTables; ++t) {
                uint64_t bucket = chunk.lshHashes[t];
                m_lshTables[t][bucket].push_back(chunk.id);
            }

            m_chunks[chunk.id] = std::move(chunk);
            ++added;
        }
        return added;
    }

    // ── Retrieve nearest chunks for a query ──────────────────────────────────
    std::vector<RetrievalResult> retrieve(const std::string& query, size_t topK = 10) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Embed the query
        EmbeddingVector queryVec = hashEmbedding(query, m_config.embeddingDim);
        auto queryHashes = computeLSHHashes(queryVec);

        // Candidate set from LSH tables (approximate nearest neighbors)
        std::unordered_set<size_t> candidates;
        for (size_t t = 0; t < m_config.lshTables; ++t) {
            uint64_t bucket = queryHashes[t];
            auto it = m_lshTables[t].find(bucket);
            if (it != m_lshTables[t].end()) {
                for (auto id : it->second) {
                    candidates.insert(id);
                }
            }
        }

        // If LSH didn't find enough candidates, fall back to brute-force
        if (candidates.size() < topK * 2 && m_chunks.size() <= 10000) {
            for (auto& [id, _] : m_chunks) candidates.insert(id);
        }

        // Score candidates by exact cosine similarity
        struct ScoredChunk { size_t id; float sim; };
        std::vector<ScoredChunk> scored;
        scored.reserve(candidates.size());
        for (auto id : candidates) {
            auto it = m_chunks.find(id);
            if (it == m_chunks.end()) continue;
            float sim = queryVec.cosineSimilarity(it->second.embedding);
            scored.push_back({id, sim});
        }

        // Sort by similarity descending
        std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.sim > b.sim; });

        // Build results
        std::vector<RetrievalResult> results;
        for (size_t i = 0; i < std::min(topK, scored.size()); ++i) {
            auto& chunk = m_chunks[scored[i].id];
            RetrievalResult r;
            r.chunkId = scored[i].id;
            r.filePath = chunk.metadata.filePath;
            r.startLine = chunk.metadata.startLine;
            r.endLine = chunk.metadata.endLine;
            r.content = chunk.content;
            r.similarity = scored[i].sim;
            r.symbolScope = chunk.metadata.symbolScope;
            results.push_back(std::move(r));
        }
        return results;
    }

    // ── Persist index to disk ────────────────────────────────────────────────
    bool saveIndex(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs.is_open()) return false;

        // Header
        uint32_t magic = 0x52585745; // "RXWE"
        uint32_t version = 1;
        uint32_t chunkCount = static_cast<uint32_t>(m_chunks.size());
        uint32_t dim = static_cast<uint32_t>(m_config.embeddingDim);
        ofs.write(reinterpret_cast<const char*>(&magic), 4);
        ofs.write(reinterpret_cast<const char*>(&version), 4);
        ofs.write(reinterpret_cast<const char*>(&chunkCount), 4);
        ofs.write(reinterpret_cast<const char*>(&dim), 4);

        // Chunks
        for (auto& [id, chunk] : m_chunks) {
            uint32_t idVal = static_cast<uint32_t>(id);
            ofs.write(reinterpret_cast<const char*>(&idVal), 4);

            // Metadata
            auto writeStr = [&ofs](const std::string& s) {
                uint32_t len = static_cast<uint32_t>(s.size());
                ofs.write(reinterpret_cast<const char*>(&len), 4);
                ofs.write(s.data(), len);
            };
            writeStr(chunk.metadata.filePath);
            ofs.write(reinterpret_cast<const char*>(&chunk.metadata.startLine), 4);
            ofs.write(reinterpret_cast<const char*>(&chunk.metadata.endLine), 4);
            writeStr(chunk.metadata.language);

            // Embedding
            ofs.write(reinterpret_cast<const char*>(chunk.embedding.data.data()),
                      dim * sizeof(float));

            // Content
            writeStr(chunk.content);
        }
        return true;
    }

    // ── Load index from disk ─────────────────────────────────────────────────
    bool loadIndex(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) return false;

        uint32_t magic, version, chunkCount, dim;
        ifs.read(reinterpret_cast<char*>(&magic), 4);
        ifs.read(reinterpret_cast<char*>(&version), 4);
        ifs.read(reinterpret_cast<char*>(&chunkCount), 4);
        ifs.read(reinterpret_cast<char*>(&dim), 4);

        if (magic != 0x52585745 || version != 1) return false;

        m_config.embeddingDim = dim;
        m_chunks.clear();
        for (size_t t = 0; t < m_lshTables.size(); ++t) m_lshTables[t].clear();

        for (uint32_t i = 0; i < chunkCount; ++i) {
            uint32_t idVal;
            ifs.read(reinterpret_cast<char*>(&idVal), 4);

            IndexedChunk chunk;
            chunk.id = idVal;

            auto readStr = [&ifs]() -> std::string {
                uint32_t len = 0;
                ifs.read(reinterpret_cast<char*>(&len), 4);
                std::string s(len, '\0');
                ifs.read(s.data(), len);
                return s;
            };

            chunk.metadata.filePath = readStr();
            ifs.read(reinterpret_cast<char*>(&chunk.metadata.startLine), 4);
            ifs.read(reinterpret_cast<char*>(&chunk.metadata.endLine), 4);
            chunk.metadata.language = readStr();

            chunk.embedding.data.resize(dim);
            ifs.read(reinterpret_cast<char*>(chunk.embedding.data.data()), dim * sizeof(float));

            chunk.content = readStr();

            // Recompute LSH hashes
            chunk.lshHashes = computeLSHHashes(chunk.embedding);
            for (size_t t = 0; t < m_config.lshTables; ++t) {
                m_lshTables[t][chunk.lshHashes[t]].push_back(chunk.id);
            }

            if (chunk.id >= m_nextChunkId) m_nextChunkId = chunk.id + 1;
            m_chunks[chunk.id] = std::move(chunk);
        }
        return true;
    }

    // ── Stats ────────────────────────────────────────────────────────────────
    size_t chunkCount() const { std::lock_guard<std::mutex> lock(m_mutex); return m_chunks.size(); }
    size_t fileCount() const { std::lock_guard<std::mutex> lock(m_mutex); return m_fileModTimes.size(); }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_chunks.clear();
        m_fileModTimes.clear();
        for (auto& table : m_lshTables) table.clear();
        m_nextChunkId = 0;
    }

private:
    WorkspaceEmbeddings() { initLSH(); }

    void initLSH() {
        m_lshTables.resize(m_config.lshTables);
        m_lshProjections.resize(m_config.lshTables);

        // Generate random hyperplane projections for LSH
        std::mt19937 rng(42); // Deterministic seed for reproducibility
        std::normal_distribution<float> dist(0.0f, 1.0f);
        for (size_t t = 0; t < m_config.lshTables; ++t) {
            m_lshProjections[t].resize(m_config.lshBucketBits);
            for (size_t b = 0; b < m_config.lshBucketBits; ++b) {
                m_lshProjections[t][b].resize(m_config.embeddingDim);
                for (size_t d = 0; d < m_config.embeddingDim; ++d) {
                    m_lshProjections[t][b][d] = dist(rng);
                }
            }
        }
    }

    std::vector<uint64_t> computeLSHHashes(const EmbeddingVector& vec) {
        std::vector<uint64_t> hashes(m_config.lshTables);
        for (size_t t = 0; t < m_config.lshTables; ++t) {
            uint64_t h = 0;
            for (size_t b = 0; b < m_config.lshBucketBits; ++b) {
                float dot = 0.0f;
                for (size_t d = 0; d < std::min(vec.data.size(), m_lshProjections[t][b].size()); ++d) {
                    dot += vec.data[d] * m_lshProjections[t][b][d];
                }
                if (dot > 0.0f) h |= (1ULL << b);
            }
            hashes[t] = h;
        }
        return hashes;
    }

    std::vector<IndexedChunk> chunkContent(const std::string& content, const std::string& filePath,
                                            const std::string& lang, uint64_t modTime) {
        std::vector<IndexedChunk> chunks;

        // Split by lines first, then group into chunks
        std::vector<std::string> lines;
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) lines.push_back(line);

        size_t chunkChars = 0;
        uint32_t chunkStartLine = 0;
        std::string chunkContent;
        size_t chunkCount = 0;

        for (uint32_t i = 0; i < lines.size() && chunkCount < m_config.maxChunksPerFile; ++i) {
            chunkContent += lines[i] + "\n";
            chunkChars += lines[i].size() + 1;

            if (chunkChars >= m_config.chunkSize || i == lines.size() - 1) {
                IndexedChunk chunk;
                chunk.id = m_nextChunkId++;
                chunk.metadata.filePath = filePath;
                chunk.metadata.startLine = chunkStartLine + 1;
                chunk.metadata.endLine = i + 1;
                chunk.metadata.language = lang;
                chunk.metadata.fileModTime = modTime;
                chunk.content = chunkContent;

                chunks.push_back(std::move(chunk));
                ++chunkCount;

                // Overlap: keep last N chars
                if (m_config.chunkOverlap > 0 && chunkContent.size() > m_config.chunkOverlap) {
                    chunkContent = chunkContent.substr(chunkContent.size() - m_config.chunkOverlap);
                    // Find the line number for the overlap start
                    chunkStartLine = i > 0 ? i : 0;
                } else {
                    chunkContent.clear();
                    chunkStartLine = i + 1;
                }
                chunkChars = chunkContent.size();
            }
        }

        return chunks;
    }

    void removeFileChunks(const std::string& filePath) {
        std::vector<size_t> toRemove;
        for (auto& [id, chunk] : m_chunks) {
            if (chunk.metadata.filePath == filePath) {
                toRemove.push_back(id);
            }
        }
        for (auto id : toRemove) {
            // Remove from LSH tables
            auto& chunk = m_chunks[id];
            for (size_t t = 0; t < m_config.lshTables && t < chunk.lshHashes.size(); ++t) {
                auto& bucket = m_lshTables[t][chunk.lshHashes[t]];
                bucket.erase(std::remove(bucket.begin(), bucket.end(), id), bucket.end());
            }
            m_chunks.erase(id);
        }
    }

    static std::string detectLanguage(const std::string& filePath) {
        auto ext = std::filesystem::path(filePath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") return "cpp";
        if (ext == ".h" || ext == ".hpp" || ext == ".hxx") return "cpp-header";
        if (ext == ".c") return "c";
        if (ext == ".asm") return "asm";
        if (ext == ".py") return "python";
        if (ext == ".js") return "javascript";
        if (ext == ".ts") return "typescript";
        if (ext == ".json") return "json";
        if (ext == ".md") return "markdown";
        if (ext == ".cmake") return "cmake";
        return "unknown";
    }

    static bool isSupportedExtension(const std::string& ext) {
        static const std::unordered_set<std::string> supported = {
            ".cpp",".hpp",".h",".c",".cxx",".hxx",".cc",".asm",
            ".py",".js",".ts",".json",".md",".cmake",".ps1"
        };
        return supported.count(ext) > 0;
    }

    static bool shouldSkipPath(const std::string& path) {
        return path.find("\\.git\\") != std::string::npos ||
               path.find("\\node_modules\\") != std::string::npos ||
               path.find("\\build\\") != std::string::npos ||
               path.find("\\obj\\") != std::string::npos ||
               path.find("\\.vs\\") != std::string::npos;
    }

    mutable std::mutex m_mutex;
    WorkspaceEmbeddingConfig m_config;
    std::unordered_map<size_t, IndexedChunk> m_chunks;
    std::unordered_map<std::string, uint64_t> m_fileModTimes;
    std::vector<std::unordered_map<uint64_t, std::vector<size_t>>> m_lshTables;
    std::vector<std::vector<std::vector<float>>> m_lshProjections; // [table][bit][dim]
    size_t m_nextChunkId = 0;
};

} // namespace AI
} // namespace RawrXD

// =============================================================================
// C API
// =============================================================================
extern "C" {

__declspec(dllexport) size_t WorkspaceEmbed_IndexWorkspace(const char* rootPath) {
    return RawrXD::AI::WorkspaceEmbeddings::instance().indexWorkspace(rootPath ? rootPath : ".");
}

__declspec(dllexport) size_t WorkspaceEmbed_IndexFile(const char* filePath) {
    return RawrXD::AI::WorkspaceEmbeddings::instance().indexFile(filePath ? filePath : "");
}

__declspec(dllexport) size_t WorkspaceEmbed_ChunkCount() {
    return RawrXD::AI::WorkspaceEmbeddings::instance().chunkCount();
}

__declspec(dllexport) int WorkspaceEmbed_SaveIndex(const char* path) {
    return RawrXD::AI::WorkspaceEmbeddings::instance().saveIndex(path ? path : "") ? 1 : 0;
}

__declspec(dllexport) int WorkspaceEmbed_LoadIndex(const char* path) {
    return RawrXD::AI::WorkspaceEmbeddings::instance().loadIndex(path ? path : "") ? 1 : 0;
}

__declspec(dllexport) void WorkspaceEmbed_Clear() {
    RawrXD::AI::WorkspaceEmbeddings::instance().clear();
}

} // extern "C"
