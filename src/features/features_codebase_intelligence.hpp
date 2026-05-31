#pragma once
/**
 * features_codebase_intelligence.hpp - Features 11-18: Codebase Intelligence
 *
 * 11. Full Codebase Indexing (RAG)
 * 12. @codebase References
 * 13. Semantic Code Search
 * 14. Cross-File Context Awareness
 * 15. Project-Wide Pattern Detection
 * 16. Import Relationship Mapping
 * 17. Git History Analysis
 * 18. Dependency Graph Understanding
 */

#include "ai_ide_features.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace rawrxd {

//=============================================================================
// FEATURE 11: Full Codebase Indexing (RAG)
//=============================================================================

class Feature_CodebaseIndex {
public:
    struct Config {
        uint32_t maxFileSize = 100000;
        std::vector<std::string> includePatterns = {"**/*.cpp", "**/*.hpp", "**/*.h"};
        std::vector<std::string> excludePatterns = {"**/node_modules/**", "**/.git/**"};
        uint32_t embeddingDimensions = 768;
        uint32_t chunkSize = 512;
    };

    explicit Feature_CodebaseIndex(const Config& config = {});

    void indexDirectory(const std::string& path);
    void indexFile(const std::string& path);
    void reindex();
    void clearIndex();

    std::vector<SearchResult> search(const std::string& query, uint32_t k = 10);
    std::vector<SearchResult> semanticSearch(const std::string& query, uint32_t k = 10);

    size_t getIndexSize() const;
    bool isIndexed(const std::string& path) const;
    float getIndexProgress() const;

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    Config config_;
    std::shared_ptr<AIProvider> provider_;

    struct Chunk {
        std::string filePath;
        uint32_t line = 0;
        uint32_t startLine = 0;
        uint32_t endLine = 0;
        std::string content;
        std::vector<float> embedding;
    };

    std::vector<Chunk> chunks_;
    std::unordered_map<std::string, std::vector<size_t>> fileToChunks_;
    mutable std::mutex mutex_;

    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
};

inline Feature_CodebaseIndex::Feature_CodebaseIndex(const Config& config)
    : config_(config) {}

inline void Feature_CodebaseIndex::indexDirectory(const std::string& path) {
    namespace fs = std::filesystem;
    if (!fs::exists(path)) return;

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (!entry.is_regular_file()) continue;
        std::string filePath = entry.path().string();

        bool included = false;
        for (const auto& pattern : config_.includePatterns) {
            if (filePath.find(pattern.substr(2)) != std::string::npos) {
                included = true;
                break;
            }
        }
        if (!included) continue;

        bool excluded = false;
        for (const auto& pattern : config_.excludePatterns) {
            if (filePath.find(pattern.substr(2)) != std::string::npos) {
                excluded = true;
                break;
            }
        }
        if (!excluded) indexFile(filePath);
    }
}

inline void Feature_CodebaseIndex::indexFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    uint32_t chunkId = 0;
    for (size_t i = 0; i < content.size(); i += config_.chunkSize) {
        Chunk chunk;
        chunk.filePath = path;
        chunk.content = content.substr(i, config_.chunkSize);
        chunk.startLine = static_cast<uint32_t>(
            std::count(content.begin(), content.begin() + static_cast<std::ptrdiff_t>(i), '\n'));

        if (provider_) {
            chunk.embedding = provider_->embed(chunk.content);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        chunks_.push_back(std::move(chunk));
        fileToChunks_[path].push_back(chunks_.size() - 1);
    }
}

inline std::vector<SearchResult> Feature_CodebaseIndex::search(
    const std::string& query, uint32_t k) {

    std::vector<SearchResult> results;
    if (!provider_) return results;

    auto queryEmbedding = provider_->embed(query);

    std::vector<std::pair<float, size_t>> scores;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < chunks_.size(); ++i) {
            float score = cosineSimilarity(queryEmbedding, chunks_[i].embedding);
            scores.push_back({score, i});
        }
    }

    std::partial_sort(scores.begin(),
                      scores.begin() + static_cast<std::ptrdiff_t>(std::min(k, static_cast<uint32_t>(scores.size()))),
                      scores.end(),
                      std::greater<>());

    for (size_t i = 0; i < std::min(static_cast<size_t>(k), scores.size()); ++i) {
        SearchResult result;
        result.filePath = chunks_[scores[i].second].filePath;
        result.line = chunks_[scores[i].second].startLine;
        result.snippet = chunks_[scores[i].second].content.substr(0, 200);
        result.score = scores[i].first;
        results.push_back(result);
    }

    return results;
}

inline float Feature_CodebaseIndex::cosineSimilarity(
    const std::vector<float>& a, const std::vector<float>& b) {

    if (a.empty() || b.empty() || a.size() != b.size()) return 0.0f;

    float dot = 0, normA = 0, normB = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    return dot / (std::sqrt(normA) * std::sqrt(normB) + 1e-10f);
}

inline void Feature_CodebaseIndex::clearIndex() {
    std::lock_guard<std::mutex> lock(mutex_);
    chunks_.clear();
    fileToChunks_.clear();
}

inline size_t Feature_CodebaseIndex::getIndexSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return chunks_.size();
}

inline void Feature_CodebaseIndex::setProvider(std::shared_ptr<AIProvider> p) {
    provider_ = p;
}

//=============================================================================
// FEATURE 12: @codebase References
//=============================================================================

class Feature_CodebaseReferences {
public:
    struct Reference {
        std::string symbol;
        std::string filePath;
        uint32_t line = 0;
        std::string context;
    };

    std::vector<Reference> findReferences(const std::string& symbol);
    std::vector<Reference> findDefinitions(const std::string& symbol);
    std::vector<Reference> query(const std::string& naturalLanguageQuery);

    void setIndex(std::shared_ptr<Feature_CodebaseIndex> index);
    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<Feature_CodebaseIndex> index_;
    std::shared_ptr<AIProvider> provider_;
};

//=============================================================================
// FEATURE 13: Semantic Code Search
//=============================================================================

class Feature_SemanticSearch {
public:
    std::vector<SearchResult> search(const std::string& query, uint32_t k = 10);
    std::vector<SearchResult> searchSimilar(const std::string& code, uint32_t k = 10);

    void setIndex(std::shared_ptr<Feature_CodebaseIndex> index);

private:
    std::shared_ptr<Feature_CodebaseIndex> index_;
};

//=============================================================================
// FEATURE 14: Cross-File Context Awareness
//=============================================================================

class Feature_CrossFileContext {
public:
    struct RelatedFile {
        std::string path;
        std::string reason;
        float relevance = 0.0f;
    };

    void analyze(const std::string& filePath);
    std::vector<RelatedFile> getRelatedFiles(const std::string& filePath);
    std::string getContext(const std::string& filePath, uint32_t maxTokens = 4000);

    void setIndex(std::shared_ptr<Feature_CodebaseIndex> index);

private:
    std::shared_ptr<Feature_CodebaseIndex> index_;
    std::unordered_map<std::string, std::vector<RelatedFile>> relations_;
};

//=============================================================================
// FEATURE 15: Project-Wide Pattern Detection
//=============================================================================

class Feature_PatternDetection {
public:
    struct Pattern {
        std::string name;
        std::string description;
        std::string example;
        uint32_t occurrences = 0;
    };

    std::vector<Pattern> detectPatterns(const std::string& filePath);
    std::vector<Pattern> detectAntiPatterns(const std::string& filePath);
    std::string suggestConsistentStyle(const std::string& code);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
};

//=============================================================================
// FEATURE 16: Import Relationship Mapping
//=============================================================================

class Feature_ImportMapping {
public:
    void buildGraph(const std::string& projectPath);
    std::vector<std::string> getDependencies(const std::string& filePath);
    std::vector<std::string> getDependents(const std::string& filePath);
    bool hasCircularDependency();

private:
    std::unordered_map<std::string, std::vector<std::string>> deps_;
    std::unordered_map<std::string, std::vector<std::string>> reverseDeps_;
};

//=============================================================================
// FEATURE 17: Git History Analysis
//=============================================================================

class Feature_GitHistory {
public:
    struct CommitInfo {
        std::string hash;
        std::string message;
        std::string author;
        std::chrono::system_clock::time_point date;
        std::vector<std::string> files;
    };

    std::vector<CommitInfo> getHistory(const std::string& filePath, uint32_t limit = 10);
    std::string getBlame(const std::string& filePath, uint32_t line);
    std::string explainChanges(const std::string& filePath);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
};

//=============================================================================
// FEATURE 18: Dependency Graph Understanding
//=============================================================================

class Feature_DependencyGraph {
public:
    struct Dependency {
        std::string name;
        std::string version;
        std::vector<std::string> dependencies;
        bool outdated = false;
        std::string latestVersion;
    };

    void analyze(const std::string& manifestPath);
    std::vector<Dependency> getDependencies();
    std::vector<Dependency> checkOutdated();
    std::string suggestUpdates();

private:
    std::vector<Dependency> dependencies_;
};

} // namespace rawrxd
