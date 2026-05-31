#pragma once

#include "RawrXDVectorIndex.h"
#include "../../include/context/semantic_index.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace RawrXD::Runtime::SemanticRetrieval {

inline std::string Trim(std::string value) {
    const auto first = std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    value.erase(value.begin(), first);

    const auto last = std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();
    value.erase(last, value.end());
    return value;
}

inline const char* GetEnvOrNull(const char* name) {
    const char* value = std::getenv(name);
    return (value && value[0] != '\0') ? value : nullptr;
}

inline uint32_t GetPreferredEmbeddingDimensions() {
    return RawrXD::Runtime::RawrXDVectorIndex::instance().preferredDimensions();
}

inline std::vector<float> BuildDeterministicTextEmbedding(const std::string& text, uint32_t dims = 0) {
    if (dims == 0) {
        dims = GetPreferredEmbeddingDimensions();
    }
    if (dims == 0) {
        dims = 768u;
    }

    std::vector<float> embedding(dims, 0.0f);
    std::string token;

    const auto flushToken = [&embedding, dims](const std::string& value) {
        if (value.empty()) {
            return;
        }

        uint64_t hash = 1469598103934665603ull;
        for (unsigned char ch : value) {
            hash ^= static_cast<uint64_t>(ch);
            hash *= 1099511628211ull;
        }

        for (uint32_t pass = 0; pass < 4; ++pass) {
            const size_t index = static_cast<size_t>((hash + (0x9e3779b97f4a7c15ull * (pass + 1))) % dims);
            const float sign = ((hash >> (pass * 11)) & 1ull) ? 1.0f : -1.0f;
            embedding[index] += sign * (1.0f - (0.15f * static_cast<float>(pass)));
        }
    };

    for (unsigned char ch : text) {
        if (std::isalnum(ch) || ch == '_') {
            token.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!token.empty()) {
            flushToken(token);
            token.clear();
        }
    }
    flushToken(token);

    float norm = 0.0f;
    for (float value : embedding) {
        norm += value * value;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (float& value : embedding) {
            value /= norm;
        }
    }
    return embedding;
}

inline bool EnsureMappedGGUFEmbeddingsAttached() {
    auto& index = RawrXD::Runtime::RawrXDVectorIndex::instance();
    if (index.hasMappedGGUF()) {
        return true;
    }

    const char* ggufPath = GetEnvOrNull("RAWRXD_GGUF_EMBED_PATH");
    if (!ggufPath) {
        return false;
    }

    static std::atomic<bool> attempted{false};
    if (attempted.exchange(true)) {
        return index.hasMappedGGUF();
    }

    const char* metadataPrefix = GetEnvOrNull("RAWRXD_GGUF_EMBED_PREFIX");
    const char* metadataPath = GetEnvOrNull("RAWRXD_GGUF_EMBED_META");
    const char* tensorName = GetEnvOrNull("RAWRXD_GGUF_EMBED_TENSOR");
    const char* offsetValue = GetEnvOrNull("RAWRXD_GGUF_EMBED_OFFSET");
    const char* rowsValue = GetEnvOrNull("RAWRXD_GGUF_EMBED_ROWS");
    const char* dimsValue = GetEnvOrNull("RAWRXD_GGUF_EMBED_DIMS");

    bool attached = false;
    if (offsetValue && rowsValue) {
        const uint64_t offset = static_cast<uint64_t>(_strtoui64(offsetValue, nullptr, 10));
        const uint32_t rows = static_cast<uint32_t>(std::strtoul(rowsValue, nullptr, 10));
        const uint32_t dims = dimsValue
            ? static_cast<uint32_t>(std::strtoul(dimsValue, nullptr, 10))
            : 768u;
        attached = index.attachMappedGGUF(
            ggufPath,
            offset,
            rows,
            dims,
            metadataPrefix ? metadataPrefix : "gguf-embed");
        if (attached && metadataPath) {
            index.loadMappedMetadataSidecar(metadataPath);
        }
    } else {
        attached = index.attachMappedGGUFTensor(
            ggufPath,
            tensorName ? tensorName : "embeddings",
            metadataPrefix ? metadataPrefix : "gguf-embed",
            metadataPath ? metadataPath : "");
    }

    return attached;
}

inline void InstallSemanticIndexEmbeddingCallback() {
    RawrXD::SemanticIndex::SemanticIndexEngine::Instance().SetEmbeddingCallback(
        [](const std::string& text) {
            EnsureMappedGGUFEmbeddingsAttached();
            return BuildDeterministicTextEmbedding(text);
        });
}

inline std::vector<RawrXD::Runtime::SearchResult> SearchSemanticContext(const std::string& text, size_t topK = 4) {
    EnsureMappedGGUFEmbeddingsAttached();

    auto& index = RawrXD::Runtime::RawrXDVectorIndex::instance();
    const auto query = BuildDeterministicTextEmbedding(text, index.preferredDimensions());
    auto hits = index.search(query, topK);
    if (!hits.empty()) {
        return hits;
    }

    auto& semanticIndex = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    if (!semanticIndex.IsInitialized()) {
        return {};
    }

    const auto semanticHits = semanticIndex.SemanticSearch(text, static_cast<int>(topK));
    if (!semanticHits.ok()) {
        return {};
    }

    std::vector<RawrXD::Runtime::SearchResult> fallback;
    fallback.reserve(semanticHits.symbols.size());
    for (const auto& symbol : semanticHits.symbols) {
        std::ostringstream metadata;
        metadata << symbol.qualifiedName;
        if (!symbol.definition.filePath.empty()) {
            metadata << " @ " << symbol.definition.filePath << ":" << symbol.definition.startLine;
        }
        fallback.push_back({0, metadata.str(), symbol.relevanceScore});
    }
    return fallback;
}

inline std::string BuildPromptSemanticContextBlock(const std::string& text,
                                                   size_t topK = 4,
                                                   const char* sectionName = "SEMANTIC_CONTEXT") {
    const auto hits = SearchSemanticContext(text, topK);
    if (hits.empty()) {
        return {};
    }

    std::ostringstream stream;
    stream << "[" << sectionName << "]\n";
    size_t emitted = 0;
    for (const auto& hit : hits) {
        if (hit.similarity <= 0.05f) {
            continue;
        }

        std::string metadata = Trim(hit.metadata);
        if (metadata.empty()) {
            continue;
        }
        if (metadata.size() > 320) {
            metadata.resize(320);
            metadata += "...";
        }

        stream << "- " << metadata << " (score=" << hit.similarity << ")\n";
        ++emitted;
    }
    stream << "[/" << sectionName << "]\n";
    return emitted > 0 ? stream.str() : std::string();
}

} // namespace RawrXD::Runtime::SemanticRetrieval
