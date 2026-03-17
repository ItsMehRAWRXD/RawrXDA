// ============================================================================
// vector_index.cpp — HNSW Vector Index Implementation
// ============================================================================
// Full HNSW graph construction, search, chunking strategies, LRU cache,
// and incremental indexing with diff-based updates.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "core/vector_index.h"

#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <numeric>
#include <cstring>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Embeddings {

// ============================================================================
// Chunking Strategies
// ============================================================================

static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

static bool isFunctionStart(const std::string& line) {
    // Heuristic: line contains return type + name + '(' and is not a declaration
    static const std::regex funcRegex(
        R"(^\s*(?:[\w:*&<>]+\s+)+(\w+)\s*\([^;]*$)",
        std::regex::optimize
    );
    return std::regex_search(line, funcRegex);
}

static bool isClassStart(const std::string& line) {
    static const std::regex classRegex(
        R"(^\s*(?:class|struct)\s+\w+)",
        std::regex::optimize
    );
    return std::regex_search(line, classRegex);
}

std::vector<CodeChunk> chunkFile(const fs::path& file,
                                  const std::string& content,
                                  const ChunkingConfig& config) {
    std::vector<CodeChunk> chunks;
    auto lines = splitLines(content);

    auto modTime = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    // 1. Function-level chunks
    if (config.enableFunctionLevel) {
        int braceDepth = 0;
        int funcStart = -1;

        for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
            if (funcStart < 0 && isFunctionStart(lines[i])) {
                funcStart = i;
                braceDepth = 0;
            }

            for (char c : lines[i]) {
                if (c == '{') ++braceDepth;
                if (c == '}') --braceDepth;
            }

            if (funcStart >= 0 && braceDepth == 0 &&
                lines[i].find('}') != std::string::npos) {
                // End of function
                if (i - funcStart + 1 >= static_cast<int>(config.minChunkLines)) {
                    CodeChunk chunk;
                    chunk.file = file;
                    chunk.startLine = funcStart + 1;
                    chunk.endLine = i + 1;
                    chunk.lastModified = modTime;
                    chunk.type = CodeChunk::ChunkType::FUNCTION;

                    std::ostringstream oss;
                    for (int j = funcStart; j <= i && j < static_cast<int>(lines.size()); ++j) {
                        oss << lines[j] << "\n";
                    }
                    chunk.content = oss.str();
                    chunks.push_back(std::move(chunk));
                }
                funcStart = -1;
            }
        }
    }

    // 2. Class-level chunks
    if (config.enableClassLevel) {
        int braceDepth = 0;
        int classStart = -1;

        for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
            if (classStart < 0 && isClassStart(lines[i])) {
                classStart = i;
                braceDepth = 0;
            }

            if (classStart >= 0) {
                for (char c : lines[i]) {
                    if (c == '{') ++braceDepth;
                    if (c == '}') --braceDepth;
                }

                if (braceDepth == 0 && lines[i].find('}') != std::string::npos) {
                    CodeChunk chunk;
                    chunk.file = file;
                    chunk.startLine = classStart + 1;
                    chunk.endLine = i + 1;
                    chunk.lastModified = modTime;
                    chunk.type = CodeChunk::ChunkType::CLASS;

                    std::ostringstream oss;
                    for (int j = classStart; j <= i; ++j) {
                        oss << lines[j] << "\n";
                    }
                    chunk.content = oss.str();
                    chunks.push_back(std::move(chunk));
                    classStart = -1;
                }
            }
        }
    }

    // 3. Sliding-window chunks (fallback for ungrouped code)
    if (config.enableSlidingWindow) {
        // Simple line-based sliding window (token estimation: ~4 chars/token)
        uint32_t windowLines = config.slidingWindowTokens / 10;  // ~10 tokens per line
        uint32_t overlapLines = config.overlapTokens / 10;
        uint32_t step = (windowLines > overlapLines) ? windowLines - overlapLines : 1;

        for (uint32_t i = 0; i < lines.size(); i += step) {
            uint32_t end = std::min(i + windowLines, static_cast<uint32_t>(lines.size()));

            CodeChunk chunk;
            chunk.file = file;
            chunk.startLine = i + 1;
            chunk.endLine = end;
            chunk.lastModified = modTime;
            chunk.type = CodeChunk::ChunkType::SLIDING_WINDOW;

            std::ostringstream oss;
            for (uint32_t j = i; j < end; ++j) {
                oss << lines[j] << "\n";
            }
            chunk.content = oss.str();
            chunks.push_back(std::move(chunk));

            if (end >= lines.size()) break;
        }
    }

    return chunks;
}

std::vector<CodeChunk> chunkFunction(const fs::path& file,
                                      const std::string& content,
                                      int startLine, int endLine) {
    std::vector<CodeChunk> chunks;
    auto lines = splitLines(content);

    if (startLine < 1 || endLine > static_cast<int>(lines.size())) return chunks;

    CodeChunk chunk;
    chunk.file = file;
    chunk.startLine = startLine;
    chunk.endLine = endLine;
    chunk.type = CodeChunk::ChunkType::FUNCTION;
    chunk.lastModified = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    std::ostringstream oss;
    for (int i = startLine - 1; i < endLine; ++i) {
        oss << lines[i] << "\n";
    }
    chunk.content = oss.str();
    chunks.push_back(std::move(chunk));

    return chunks;
}

// ============================================================================
// HNSW Index
// ============================================================================

HNSWIndex::HNSWIndex(const Config& config)
    : m_config(config)
    , m_rng(std::random_device{}())
    , m_levelMultiplier(1.0f / std::log(static_cast<float>(config.M))) {}

HNSWIndex::~HNSWIndex() = default;

float HNSWIndex::distance(const float* a, const float* b) const {
    if (m_config.useCosineDistance) {
        // Cosine distance = 1 - cosine_similarity
        float dot = 0.0f, normA = 0.0f, normB = 0.0f;
        for (uint32_t i = 0; i < m_config.dim; ++i) {
            dot += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }
        float denom = std::sqrt(normA) * std::sqrt(normB);
        if (denom < 1e-8f) return 1.0f;
        return 1.0f - (dot / denom);
    } else {
        // L2 distance
        float sum = 0.0f;
        for (uint32_t i = 0; i < m_config.dim; ++i) {
            float diff = a[i] - b[i];
            sum += diff * diff;
        }
        return sum;
    }
}

int HNSWIndex::randomLevel() const {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(const_cast<std::mt19937&>(m_rng));
    return static_cast<int>(-std::log(r) * m_levelMultiplier);
}

HNSWIndex::MinHeap HNSWIndex::searchLayer(const float* query, uint64_t entryPoint,
                                            uint32_t ef, int layer) const {
    MinHeap candidates;
    std::unordered_set<uint64_t> visited;

    auto entryIt = m_nodes.find(entryPoint);
    if (entryIt == m_nodes.end()) return candidates;

    float dist = distance(query, entryIt->second->embedding.data());
    candidates.push({dist, entryPoint});
    visited.insert(entryPoint);

    MinHeap result;
    result.push({dist, entryPoint});

    while (!candidates.empty()) {
        auto [cDist, cId] = candidates.top();
        candidates.pop();

        // If this candidate is farther than the worst result, stop
        if (!result.empty() && cDist > result.top().first && result.size() >= ef) break;

        auto nodeIt = m_nodes.find(cId);
        if (nodeIt == m_nodes.end()) continue;

        const auto& node = nodeIt->second;
        if (layer >= static_cast<int>(node->neighbors.size())) continue;

        for (uint64_t neighborId : node->neighbors[layer]) {
            if (visited.count(neighborId)) continue;
            visited.insert(neighborId);

            auto nIt = m_nodes.find(neighborId);
            if (nIt == m_nodes.end()) continue;

            float nDist = distance(query, nIt->second->embedding.data());

            if (result.size() < ef || nDist < result.top().first) {
                candidates.push({nDist, neighborId});
                result.push({nDist, neighborId});
                while (result.size() > ef) result.pop();
            }
        }
    }

    return result;
}

void HNSWIndex::selectNeighbors(const float* query,
                                 std::vector<std::pair<float, uint64_t>>& candidates,
                                 uint32_t M) const {
    // Simple heuristic: keep M nearest
    std::sort(candidates.begin(), candidates.end());
    if (candidates.size() > M) {
        candidates.resize(M);
    }
}

IndexResult HNSWIndex::insert(uint64_t id, const float* embedding) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_nodes.count(id)) {
        return IndexResult::error("ID already exists");
    }

    auto node = std::make_unique<Node>();
    node->id = id;
    node->embedding.assign(embedding, embedding + m_config.dim);
    node->maxLevel = randomLevel();
    node->neighbors.resize(node->maxLevel + 1);

    if (m_nodes.empty()) {
        m_entryPoint = id;
        m_maxLevel = node->maxLevel;
        m_nodes[id] = std::move(node);
        m_nodeCount.fetch_add(1, std::memory_order_relaxed);
        return IndexResult::ok("First element inserted");
    }

    uint64_t ep = m_entryPoint;

    // Traverse from top layers to node's maxLevel
    for (int level = m_maxLevel; level > node->maxLevel; --level) {
        auto result = searchLayer(embedding, ep, 1, level);
        if (!result.empty()) {
            ep = result.top().second;
        }
    }

    // Insert at each level
    for (int level = std::min(node->maxLevel, m_maxLevel); level >= 0; --level) {
        auto result = searchLayer(embedding, ep, m_config.efConstruction, level);

        std::vector<std::pair<float, uint64_t>> neighbors;
        while (!result.empty()) {
            neighbors.push_back(result.top());
            result.pop();
        }

        selectNeighbors(embedding, neighbors, m_config.M);

        // Set this node's neighbors
        for (const auto& [dist, nId] : neighbors) {
            node->neighbors[level].push_back(nId);

            // Add reverse edge
            auto nIt = m_nodes.find(nId);
            if (nIt != m_nodes.end()) {
                if (level < static_cast<int>(nIt->second->neighbors.size())) {
                    nIt->second->neighbors[level].push_back(id);

                    // Prune if over M
                    if (nIt->second->neighbors[level].size() > m_config.M) {
                        // Keep nearest M
                        auto& nNeighbors = nIt->second->neighbors[level];
                        std::vector<std::pair<float, uint64_t>> scored;
                        for (uint64_t nn : nNeighbors) {
                            auto nnIt = m_nodes.find(nn);
                            if (nnIt != m_nodes.end()) {
                                float d = distance(nIt->second->embedding.data(),
                                                   nnIt->second->embedding.data());
                                scored.push_back({d, nn});
                            }
                        }
                        std::sort(scored.begin(), scored.end());
                        nNeighbors.clear();
                        for (size_t i = 0; i < m_config.M && i < scored.size(); ++i) {
                            nNeighbors.push_back(scored[i].second);
                        }
                    }
                }
            }
        }

        if (!neighbors.empty()) {
            ep = neighbors[0].second;
        }
    }

    if (node->maxLevel > m_maxLevel) {
        m_maxLevel = node->maxLevel;
        m_entryPoint = id;
    }

    m_nodes[id] = std::move(node);
    m_nodeCount.fetch_add(1, std::memory_order_relaxed);

    return IndexResult::ok("Inserted");
}

IndexResult HNSWIndex::insertBatch(const std::vector<uint64_t>& ids,
                                    const std::vector<const float*>& embeddings) {
    for (size_t i = 0; i < ids.size(); ++i) {
        auto r = insert(ids[i], embeddings[i]);
        if (!r.success) return r;
    }
    return IndexResult::ok("Batch inserted");
}

std::vector<std::pair<uint64_t, float>> HNSWIndex::search(const float* query, uint32_t k) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_nodes.empty()) return {};

    uint64_t ep = m_entryPoint;

    // Greedy search from top layers
    for (int level = m_maxLevel; level > 0; --level) {
        auto result = searchLayer(query, ep, 1, level);
        if (!result.empty()) {
            ep = result.top().second;
        }
    }

    // Full search on layer 0
    auto result = searchLayer(query, ep, std::max(m_config.efSearch, k), 0);

    std::vector<std::pair<uint64_t, float>> topK;
    while (!result.empty()) {
        topK.push_back({result.top().second, result.top().first});
        result.pop();
    }

    // Sort by distance (ascending)
    std::sort(topK.begin(), topK.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    if (topK.size() > k) topK.resize(k);

    return topK;
}

IndexResult HNSWIndex::remove(uint64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return IndexResult::error("ID not found");

    // Remove all edges pointing to this node
    for (auto& [nId, node] : m_nodes) {
        for (auto& level : node->neighbors) {
            level.erase(std::remove(level.begin(), level.end(), id), level.end());
        }
    }

    m_nodes.erase(it);
    m_nodeCount.fetch_sub(1, std::memory_order_relaxed);

    // Update entry point if needed
    if (id == m_entryPoint && !m_nodes.empty()) {
        m_entryPoint = m_nodes.begin()->first;
    }

    return IndexResult::ok("Removed");
}

IndexResult HNSWIndex::saveToFile(const fs::path& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return IndexResult::error("Failed to open file for writing");

    // Header
    uint32_t magic = 0x48534E57;  // "HSNW"
    f.write(reinterpret_cast<const char*>(&magic), 4);
    f.write(reinterpret_cast<const char*>(&m_config), sizeof(m_config));

    uint64_t nodeCount = m_nodes.size();
    f.write(reinterpret_cast<const char*>(&nodeCount), 8);
    f.write(reinterpret_cast<const char*>(&m_entryPoint), 8);
    f.write(reinterpret_cast<const char*>(&m_maxLevel), 4);

    for (const auto& [id, node] : m_nodes) {
        f.write(reinterpret_cast<const char*>(&node->id), 8);
        f.write(reinterpret_cast<const char*>(&node->maxLevel), 4);
        f.write(reinterpret_cast<const char*>(node->embedding.data()),
                m_config.dim * sizeof(float));

        uint32_t numLevels = static_cast<uint32_t>(node->neighbors.size());
        f.write(reinterpret_cast<const char*>(&numLevels), 4);
        for (const auto& level : node->neighbors) {
            uint32_t numNeighbors = static_cast<uint32_t>(level.size());
            f.write(reinterpret_cast<const char*>(&numNeighbors), 4);
            for (uint64_t nId : level) {
                f.write(reinterpret_cast<const char*>(&nId), 8);
            }
        }
    }

    f.flush();
    return IndexResult::ok("Saved");
}

IndexResult HNSWIndex::loadFromFile(const fs::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return IndexResult::error("Failed to open file for reading");

    uint32_t magic;
    f.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x48534E57) return IndexResult::error("Invalid HNSW file magic");

    f.read(reinterpret_cast<char*>(&m_config), sizeof(m_config));

    uint64_t nodeCount;
    f.read(reinterpret_cast<char*>(&nodeCount), 8);
    f.read(reinterpret_cast<char*>(&m_entryPoint), 8);
    f.read(reinterpret_cast<char*>(&m_maxLevel), 4);

    m_nodes.clear();
    for (uint64_t n = 0; n < nodeCount; ++n) {
        auto node = std::make_unique<Node>();
        f.read(reinterpret_cast<char*>(&node->id), 8);
        f.read(reinterpret_cast<char*>(&node->maxLevel), 4);

        node->embedding.resize(m_config.dim);
        f.read(reinterpret_cast<char*>(node->embedding.data()),
               m_config.dim * sizeof(float));

        uint32_t numLevels;
        f.read(reinterpret_cast<char*>(&numLevels), 4);
        node->neighbors.resize(numLevels);
        for (uint32_t lvl = 0; lvl < numLevels; ++lvl) {
            uint32_t numNeighbors;
            f.read(reinterpret_cast<char*>(&numNeighbors), 4);
            node->neighbors[lvl].resize(numNeighbors);
            for (uint32_t j = 0; j < numNeighbors; ++j) {
                f.read(reinterpret_cast<char*>(&node->neighbors[lvl][j]), 8);
            }
        }

        m_nodes[node->id] = std::move(node);
    }

    m_nodeCount.store(m_nodes.size(), std::memory_order_relaxed);
    return IndexResult::ok("Loaded");
}

size_t HNSWIndex::memoryUsageBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = sizeof(*this);
    for (const auto& [id, node] : m_nodes) {
        total += sizeof(Node);
        total += node->embedding.size() * sizeof(float);
        for (const auto& level : node->neighbors) {
            total += level.size() * sizeof(uint64_t);
        }
    }
    return total;
}

// ============================================================================
// LRU Embedding Cache
// ============================================================================

EmbeddingCache::EmbeddingCache(size_t maxEntries)
    : m_maxEntries(maxEntries) {}

void EmbeddingCache::put(const std::string& key, const std::vector<float>& embedding) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_keyToIndex.find(key);
    if (it != m_keyToIndex.end()) {
        // Update existing
        m_entries[it->second].embedding = embedding;
        m_entries[it->second].lastAccess = ++m_accessCounter;
        return;
    }

    // Evict if full
    while (m_entries.size() >= m_maxEntries) {
        // Find LRU entry
        size_t lruIdx = 0;
        uint64_t lruAccess = UINT64_MAX;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].lastAccess < lruAccess) {
                lruAccess = m_entries[i].lastAccess;
                lruIdx = i;
            }
        }
        m_keyToIndex.erase(m_entries[lruIdx].key);
        m_entries.erase(m_entries.begin() + lruIdx);

        // Rebuild index
        m_keyToIndex.clear();
        for (size_t i = 0; i < m_entries.size(); ++i) {
            m_keyToIndex[m_entries[i].key] = i;
        }
    }

    CacheEntry entry;
    entry.key = key;
    entry.embedding = embedding;
    entry.lastAccess = ++m_accessCounter;

    m_keyToIndex[key] = m_entries.size();
    m_entries.push_back(std::move(entry));
}

const std::vector<float>* EmbeddingCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_keyToIndex.find(key);
    if (it == m_keyToIndex.end()) {
        ++m_misses;
        return nullptr;
    }

    ++m_hits;
    m_entries[it->second].lastAccess = ++m_accessCounter;
    return &m_entries[it->second].embedding;
}

void EmbeddingCache::evict(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_keyToIndex.find(key);
    if (it != m_keyToIndex.end()) {
        m_entries.erase(m_entries.begin() + it->second);
        m_keyToIndex.erase(it);

        // Rebuild index
        m_keyToIndex.clear();
        for (size_t i = 0; i < m_entries.size(); ++i) {
            m_keyToIndex[m_entries[i].key] = i;
        }
    }
}

void EmbeddingCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
    m_keyToIndex.clear();
}

size_t EmbeddingCache::size() const {
    return m_entries.size();
}

float EmbeddingCache::hitRate() const {
    size_t total = m_hits + m_misses;
    if (total == 0) return 0.0f;
    return static_cast<float>(m_hits) / static_cast<float>(total);
}

// ============================================================================
// Incremental Indexer
// ============================================================================

IncrementalIndexer::IncrementalIndexer(HNSWIndex& index, EmbeddingCache& cache)
    : m_index(index), m_cache(cache) {}

IndexResult IncrementalIndexer::indexFile(const fs::path& file) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!fs::exists(file)) return IndexResult::error("File not found");

    // Read file
    std::ifstream f(file, std::ios::binary);
    std::ostringstream oss;
    oss << f.rdbuf();
    std::string content = oss.str();

    // Chunk the file
    auto chunks = chunkFile(file, content, m_chunkConfig);

    std::vector<uint64_t> chunkIds;

    for (auto& chunk : chunks) {
        chunk.chunkId = m_nextChunkId++;

        // Get or compute embedding
        std::string cacheKey = file.string() + ":" +
                                std::to_string(chunk.startLine) + "-" +
                                std::to_string(chunk.endLine);

        const auto* cached = m_cache.get(cacheKey);
        if (cached) {
            chunk.embedding = *cached;
        } else if (m_embedFn) {
            chunk.embedding = m_embedFn(chunk.content);
            m_cache.put(cacheKey, chunk.embedding);
        } else {
            continue;  // No embedding function available
        }

        // Insert into HNSW index
        auto r = m_index.insert(chunk.chunkId, chunk.embedding.data());
        if (r.success) {
            chunkIds.push_back(chunk.chunkId);
            m_chunkStore[chunk.chunkId] = std::move(chunk);
        }
    }

    m_fileChunks[file.string()] = std::move(chunkIds);

    // Track modification time
    std::error_code ec;
    auto lwt = fs::last_write_time(file, ec);
    if (!ec) {
        m_fileModTimes[file.string()] =
            static_cast<uint64_t>(lwt.time_since_epoch().count());
    }

    return IndexResult::ok("File indexed");
}

IndexResult IncrementalIndexer::indexDirectory(const fs::path& dir,
                                                const std::vector<std::string>& extensions) {
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        bool matchExt = extensions.empty();
        for (const auto& e : extensions) {
            if (ext == e) { matchExt = true; break; }
        }
        if (!matchExt) continue;

        indexFile(entry.path());
    }
    return IndexResult::ok("Directory indexed");
}

IndexResult IncrementalIndexer::updateFile(const fs::path& file) {
    // Check if file actually changed
    std::error_code ec;
    auto lwt = fs::last_write_time(file, ec);
    if (!ec) {
        uint64_t newModTime = static_cast<uint64_t>(lwt.time_since_epoch().count());
        auto it = m_fileModTimes.find(file.string());
        if (it != m_fileModTimes.end() && it->second == newModTime) {
            return IndexResult::ok("File unchanged, skipping");
        }
    }

    // Remove old chunks
    removeFile(file);

    // Re-index
    return indexFile(file);
}

IndexResult IncrementalIndexer::removeFile(const fs::path& file) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_fileChunks.find(file.string());
    if (it == m_fileChunks.end()) return IndexResult::ok("File not indexed");

    for (uint64_t chunkId : it->second) {
        m_index.remove(chunkId);
        m_chunkStore.erase(chunkId);
    }

    m_fileChunks.erase(it);
    m_fileModTimes.erase(file.string());

    return IndexResult::ok("File removed from index");
}

std::vector<SearchResult> IncrementalIndexer::search(const std::string& query, uint32_t topK) {
    std::vector<SearchResult> results;

    if (!m_embedFn) return results;

    auto queryEmbedding = m_embedFn(query);
    auto neighbors = m_index.search(queryEmbedding.data(), topK);

    for (const auto& [id, dist] : neighbors) {
        auto it = m_chunkStore.find(id);
        if (it != m_chunkStore.end()) {
            SearchResult sr;
            sr.chunkId = id;
            sr.distance = dist;
            sr.chunk = &it->second;
            results.push_back(sr);
        }
    }

    return results;
}

std::vector<std::string> IncrementalIndexer::extractKeywords(const std::string& query,
                                                              int maxKeywords) {
    // Simple TF-IDF-like keyword extraction
    static const std::unordered_set<std::string> stopWords = {
        "the", "a", "an", "is", "are", "was", "were", "be", "been",
        "being", "have", "has", "had", "do", "does", "did", "will",
        "would", "could", "should", "may", "might", "can", "shall",
        "to", "of", "in", "for", "on", "with", "at", "by", "from",
        "it", "this", "that", "which", "what", "how", "where", "when",
        "and", "or", "not", "no", "but", "if", "then", "else"
    };

    std::unordered_map<std::string, int> wordFreq;
    std::istringstream stream(query);
    std::string word;

    while (stream >> word) {
        // Lowercase
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        // Remove punctuation
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());

        if (word.size() > 2 && stopWords.find(word) == stopWords.end()) {
            wordFreq[word]++;
        }
    }

    // Sort by frequency
    std::vector<std::pair<std::string, int>> sorted(wordFreq.begin(), wordFreq.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> keywords;
    for (int i = 0; i < maxKeywords && i < static_cast<int>(sorted.size()); ++i) {
        keywords.push_back(sorted[i].first);
    }

    return keywords;
}

std::string IncrementalIndexer::hydeExpand(const std::string& query) {
    // HyDE: Hypothetical Document Expansion
    // Generate a hypothetical code answer to improve embedding similarity
    // This is a template-based approach; real HyDE would use the LLM
    return "// Code implementation for: " + query + "\n"
           "// This function handles " + query + "\n";
}

size_t IncrementalIndexer::indexedFileCount() const {
    return m_fileChunks.size();
}

size_t IncrementalIndexer::totalChunks() const {
    return m_chunkStore.size();
}

} // namespace Embeddings
} // namespace RawrXD
