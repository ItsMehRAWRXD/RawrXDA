#include "GGUFRunner.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace {
std::mutex g_signalMutex;
std::string g_lastLoadedPath;
std::atomic<uint64_t> g_tokenChunkCount{0};
std::atomic<uint64_t> g_tokenBytes{0};
std::atomic<uint64_t> g_inferenceSuccessCount{0};
std::atomic<uint64_t> g_inferenceFailureCount{0};
std::atomic<int64_t> g_lastLoadedSize{0};
}

void GGUFRunner::tokenChunkGenerated(const std::string& chunk) {
    g_tokenChunkCount.fetch_add(1, std::memory_order_relaxed);
    g_tokenBytes.fetch_add(static_cast<uint64_t>(chunk.size()), std::memory_order_relaxed);
}

void GGUFRunner::inferenceComplete(bool success) {
    if (success) {
        g_inferenceSuccessCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_inferenceFailureCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void GGUFRunner::modelLoaded(const std::string& path, int64_t sizeBytes) {
    std::lock_guard<std::mutex> lock(g_signalMutex);
    g_lastLoadedPath = path;
    g_lastLoadedSize.store(sizeBytes, std::memory_order_relaxed);
}
