// ============================================================================
// semantic_index.cpp — Vector-based Semantic Indexer for RawrXD IDE
// Provides semantic search capabilities across codebase using embeddings
// ============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Indexing {

class SemanticIndex::Impl {
public:
    // Simple vector database using cosine similarity
    struct VectorEntry {
        std::string file_path;
        std::string content;
        std::vector<float> embedding;
        size_t line_number;
    };

    std::vector<VectorEntry> entries;
    std::unique_ptr<RawrXDInference> embedding_model;

    Impl() {
        // Initialize with a small embedding model (could be Phi-3 or similar)
        embedding_model = std::make_unique<RawrXDInference>();

        // For now, use a simple TF-IDF style approach
        // In production, this would load a proper embedding model
        std::cout << "[SemanticIndex] Initialized (stub implementation)" << std::endl;
    }

    // Generate simple embeddings (placeholder - replace with real model)
    std::vector<float> generate_embedding(const std::string& text) {
        // Simple hash-based embedding for demonstration
        // Replace with actual transformer-based embeddings
        std::vector<float> embedding(384, 0.0f); // 384-dim embedding

        std::hash<std::string> hasher;
        size_t hash = hasher(text);

        // Distribute hash across embedding dimensions
        for (size_t i = 0; i < embedding.size(); ++i) {
            embedding[i] = static_cast<float>((hash >> (i % 32)) & 0xFF) / 255.0f - 0.5f;
        }

        // Normalize
        float norm = 0.0f;
        for (float v : embedding) norm += v * v;
        norm = std::sqrt(norm);
        for (float& v : embedding) v /= norm;

        return embedding;
    }

    // Cosine similarity
    float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) return 0.0f;

        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (size_t i = 0; i < a.size(); ++i) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }

        if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
        return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
    }
};

SemanticIndex::SemanticIndex() : pimpl(std::make_unique<Impl>()) {}

SemanticIndex::~SemanticIndex() = default;

SemanticIndex& SemanticIndex::instance() {
    static SemanticIndex inst;
    return inst;
}

void SemanticIndex::indexFile(const std::string& filePath, const std::string& content) {
    // Split content into chunks (by lines or paragraphs)
    std::vector<std::string> chunks;
    std::istringstream iss(content);
    std::string line;
    size_t line_num = 1;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.length() > 10) { // Skip empty/short lines
            chunks.push_back(line);
            Impl::VectorEntry entry;
            entry.file_path = filePath;
            entry.content = line;
            entry.line_number = line_num;
            entry.embedding = pimpl->generate_embedding(line);
            pimpl->entries.push_back(entry);
        }
        line_num++;
    }

    std::cout << "[SemanticIndex] Indexed " << chunks.size()
              << " chunks from " << filePath << std::endl;
}

std::vector<std::string> SemanticIndex::search(const std::string& query, size_t max_results) {
    if (query.empty() || pimpl->entries.empty()) {
        return {};
    }

    auto query_embedding = pimpl->generate_embedding(query);

    // Calculate similarities
    std::vector<std::pair<float, size_t>> similarities;
    for (size_t i = 0; i < pimpl->entries.size(); ++i) {
        float sim = pimpl->cosine_similarity(query_embedding, pimpl->entries[i].embedding);
        similarities.emplace_back(sim, i);
    }

    // Sort by similarity (descending)
    std::sort(similarities.rbegin(), similarities.rend());

    // Return top results
    std::vector<std::string> results;
    size_t count = std::min(max_results, similarities.size());

    for (size_t i = 0; i < count; ++i) {
        const auto& entry = pimpl->entries[similarities[i].second];
        std::string result = entry.file_path + ":" + std::to_string(entry.line_number) +
                           " (" + std::to_string(similarities[i].first) + "): " + entry.content;
        results.push_back(result);
    }

    return results;
}

void SemanticIndex::clear() {
    pimpl->entries.clear();
    std::cout << "[SemanticIndex] Index cleared" << std::endl;
}

size_t SemanticIndex::size() const {
    return pimpl->entries.size();
}

}} // namespace RawrXD::Indexing