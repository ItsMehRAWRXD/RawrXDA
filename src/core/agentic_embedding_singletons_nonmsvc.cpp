#if !defined(_MSC_VER)

#include "agentic_task_graph.hpp"
#include "embedding_engine.hpp"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>

namespace {

struct EmbeddingFallbackState {
    std::mutex mutex;
    std::atomic<bool> loaded{false};
    std::string modelPath;
    uint32_t dimensions{384};
    std::atomic<uint64_t> indexedFiles{0};
};

EmbeddingFallbackState& embeddingFallbackState() {
    static EmbeddingFallbackState state;
    return state;
}

}  // namespace

namespace RawrXD::Agentic {

AgenticTaskGraph& AgenticTaskGraph::instance() {
    alignas(AgenticTaskGraph) static unsigned char storage[sizeof(AgenticTaskGraph)] = {};
    return *reinterpret_cast<AgenticTaskGraph*>(storage);
}

}  // namespace RawrXD::Agentic

namespace RawrXD::Embeddings {

EmbeddingEngine& EmbeddingEngine::instance() {
    alignas(EmbeddingEngine) static unsigned char storage[sizeof(EmbeddingEngine)] = {};
    return *reinterpret_cast<EmbeddingEngine*>(storage);
}

EmbedResult EmbeddingEngine::loadModel(const EmbeddingModelConfig& config) {
    if (config.dimensions == 0 || config.dimensions > 8192) {
        return EmbedResult::error("Invalid embedding dimensions", -1);
    }
    auto& state = embeddingFallbackState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.modelPath = config.modelPath;
    state.dimensions = config.dimensions;
    state.loaded.store(true, std::memory_order_relaxed);
    return EmbedResult::ok("Embedding model initialized (non-MSVC fallback)");
}

void EmbeddingEngine::shutdown() {
    auto& state = embeddingFallbackState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.loaded.store(false, std::memory_order_relaxed);
    state.indexedFiles.store(0, std::memory_order_relaxed);
}

EmbedResult EmbeddingEngine::indexDirectory(const std::string& dirPath,
                                            const ChunkingConfig&) {
    auto& state = embeddingFallbackState();
    if (!state.loaded.load(std::memory_order_relaxed)) {
        return EmbedResult::error("Embedding model is not loaded", -2);
    }

    std::error_code ec;
    if (!std::filesystem::exists(dirPath, ec) || ec) {
        return EmbedResult::error("Index path not found", -3);
    }

    uint64_t fileCount = 0;
    for (std::filesystem::recursive_directory_iterator it(dirPath, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec) || ec) {
            continue;
        }
        ++fileCount;
    }
    if (ec) {
        return EmbedResult::error("Failed to scan index directory", -4);
    }

    state.indexedFiles.store(fileCount, std::memory_order_relaxed);
    return EmbedResult::ok("Directory indexed (non-MSVC fallback)");
}

}  // namespace RawrXD::Embeddings

#endif  // !defined(_MSC_VER)
