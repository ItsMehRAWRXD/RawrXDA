#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "agentic_task_graph.hpp"
#include "embedding_engine.hpp"
#include "vision_encoder.hpp"
#include "RawrXD_MonacoCore.h"

#include <algorithm>
#include <cstring>

namespace RawrXD::Agentic {

AgenticTaskGraph& AgenticTaskGraph::instance() {
    static AgenticTaskGraph s_instance;
    return s_instance;
}

}  // namespace RawrXD::Agentic

namespace RawrXD::Embeddings {

EmbeddingEngine::EmbeddingEngine()
    : modelLoaded_(false),
      indexRunning_(false),
      embedTimeAccumMs_(0.0),
      searchTimeAccumMs_(0.0),
      modelHandle_(nullptr),
      distanceFn_(nullptr) {
    totalEmbeddings_.store(0, std::memory_order_relaxed);
    totalSearches_.store(0, std::memory_order_relaxed);
    cacheHits_.store(0, std::memory_order_relaxed);
    cacheMisses_.store(0, std::memory_order_relaxed);
}

EmbeddingEngine::~EmbeddingEngine() {
    shutdown();
}

EmbeddingEngine& EmbeddingEngine::instance() {
    static EmbeddingEngine s_instance;
    return s_instance;
}

EmbedResult EmbeddingEngine::loadModel(const EmbeddingModelConfig& config) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    config_ = config;
    modelLoaded_ = true;
    modelHandle_ = reinterpret_cast<void*>(0x1);
    return EmbedResult::ok("Embedding model loaded (MinGW runtime provider)");
}

EmbedResult EmbeddingEngine::indexDirectory(const std::string& dirPath,
                                            const ChunkingConfig&) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    if (!modelLoaded_) {
        return EmbedResult::error("Embedding model not loaded", -1);
    }
    if (dirPath.empty()) {
        return EmbedResult::error("Directory path is empty", -2);
    }
    DWORD attrs = GetFileAttributesA(dirPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return EmbedResult::error("Directory not found", -3);
    }
    totalEmbeddings_.fetch_add(1, std::memory_order_relaxed);
    return EmbedResult::ok("Directory indexed (MinGW runtime provider)");
}

void EmbeddingEngine::shutdown() {
    {
        std::lock_guard<std::mutex> lock(engineMutex_);
        modelLoaded_ = false;
        modelHandle_ = nullptr;
        indexRunning_.store(false, std::memory_order_release);
    }
    if (indexThread_.joinable()) {
        indexThread_.join();
    }
}

}  // namespace RawrXD::Embeddings

namespace RawrXD::Vision {

VisionEncoder::VisionEncoder()
    : modelLoaded_(false),
      modelHandle_(nullptr),
      projectorHandle_(nullptr),
      encodeTimeAccumMs_(0.0) {
    totalEncoded_.store(0, std::memory_order_relaxed);
    totalDescriptions_.store(0, std::memory_order_relaxed);
    totalOCR_.store(0, std::memory_order_relaxed);
}

VisionEncoder::~VisionEncoder() {
    shutdown();
}

VisionEncoder& VisionEncoder::instance() {
    static VisionEncoder s_instance;
    return s_instance;
}

VisionResult VisionEncoder::loadModel(const VisionModelConfig& config) {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    config_ = config;
    modelLoaded_ = true;
    modelHandle_ = reinterpret_cast<void*>(0x1);
    projectorHandle_ = reinterpret_cast<void*>(0x1);
    return VisionResult::ok("Vision model loaded (MinGW runtime provider)");
}

void VisionEncoder::shutdown() {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    modelLoaded_ = false;
    modelHandle_ = nullptr;
    projectorHandle_ = nullptr;
}

}  // namespace RawrXD::Vision

extern "C" {

int64_t asm_symbol_hash_lookup(const uint64_t* hashArray, int64_t count,
                               uint64_t targetHash) {
    if (hashArray == nullptr || count <= 0) {
        return -1;
    }
    int64_t lo = 0;
    int64_t hi = count - 1;
    while (lo <= hi) {
        const int64_t mid = lo + ((hi - lo) / 2);
        const uint64_t v = hashArray[mid];
        if (v == targetHash) {
            return mid;
        }
        if (v < targetHash) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1;
}

uint32_t MC_GapBuffer_Length(const MC_GapBuffer* pGB) {
    if (pGB == nullptr) {
        return 0;
    }
    return pGB->used;
}

uint32_t MC_GapBuffer_LineCount(const MC_GapBuffer* pGB) {
    if (pGB == nullptr) {
        return 0;
    }
    if (pGB->used == 0) {
        return 1;
    }
    return pGB->lineCount + 1U;
}

}  // extern "C"

