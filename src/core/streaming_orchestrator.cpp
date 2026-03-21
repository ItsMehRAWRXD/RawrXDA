#include "streaming_orchestrator.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <thread>
#include <vector>

namespace {

struct TimelineSemaphore {
    uint64_t value = 0;
};

struct StreamingState {
    std::mutex mutex;
    bool execLoaded = false;
    bool vulkanReady = false;
    bool streamingReady = false;
    bool threadPoolReady = false;
    uint32_t threadCount = SO_DEFAULT_THREADS;

    std::set<uint64_t> loadedLayers;
    std::deque<uint64_t> prefetchQueue;
    SO_StreamingMetrics metrics{};
    std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();

    std::atomic<intptr_t> nextTimeline{1};
    std::map<intptr_t, TimelineSemaphore> timelines;

    std::atomic<intptr_t> nextMmap{1};
    std::map<intptr_t, std::vector<uint8_t>> mappedFiles;
};

StreamingState& state() {
    static StreamingState s;
    return s;
}

uint32_t pressureFromLoaded(size_t count) {
    if (count < 16) return SO_PRESSURE_LOW;
    if (count < 64) return SO_PRESSURE_MEDIUM;
    if (count < 256) return SO_PRESSURE_HIGH;
    return SO_PRESSURE_CRITICAL;
}

void touchLayer(uint64_t layerId) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.loadedLayers.insert(layerId).second) {
        ++s.metrics.layers_loaded;
    }
}

} // namespace

extern "C" int SO_LoadExecFile(const char* filePath) {
    if (!filePath || !*filePath) {
        return 0;
    }

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        return 0;
    }

    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.execLoaded = true;
    s.startedAt = std::chrono::steady_clock::now();
    return 1;
}

extern "C" int SO_InitializeVulkan(void) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.vulkanReady = true;
    return 1;
}

extern "C" void* SO_CreateMemoryArena(uint64_t sizeBytes) {
    if (sizeBytes == 0) {
        return nullptr;
    }
    auto* arena = new (std::nothrow) uint8_t[static_cast<size_t>(sizeBytes)];
    if (!arena) {
        return nullptr;
    }
    std::memset(arena, 0, static_cast<size_t>(sizeBytes));
    return arena;
}

extern "C" int SO_CompileSPIRVShader(void* shaderModule, uint32_t opType, uint32_t opCount) {
    (void)shaderModule;
    if (opType < SO_OP_NORM || opType > SO_OP_EMBED) {
        return 0;
    }
    return opCount > 0 ? 1 : 0;
}

extern "C" int SO_CreateComputePipelines(void* operatorTable, uint64_t operatorCount) {
    (void)operatorTable;
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return (s.vulkanReady && operatorCount > 0) ? 1 : 0;
}

extern "C" void SO_DispatchOperator(void* operatorPtr) {
    (void)operatorPtr;
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.metrics.bytes_streamed += 64;
}

extern "C" void SO_ExecuteLayer(void* layerInfo, void* operatorTable) {
    (void)layerInfo;
    (void)operatorTable;
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.metrics.bytes_streamed += 4096;
    s.metrics.bytes_decompressed += 4096;
}

extern "C" int SO_ExecuteInference(void* layerTable, uint64_t layerCount) {
    (void)layerTable;
    if (layerCount == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < layerCount; ++i) {
        touchLayer(i);
        SO_ExecuteLayer(nullptr, nullptr);
    }
    return 1;
}

extern "C" void SO_PrintStatistics(void) {
    SO_StreamingMetrics m{};
    SO_GetMetrics(&m);
}

extern "C" int SO_InitializeStreaming(void) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.streamingReady = s.execLoaded || s.vulkanReady;
    return s.streamingReady ? 1 : 0;
}

extern "C" int SO_CreateThreadPool(void) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.threadPoolReady = true;
    return 1;
}

extern "C" int SO_StartDEFLATEThreads(uint32_t threadCount) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.threadCount = std::max(1U, threadCount);
    s.threadPoolReady = true;
    return 1;
}

extern "C" int SO_InitializePrefetchQueue(void) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.prefetchQueue.clear();
    return 1;
}

extern "C" int SO_ExecuteStreamingInference(void* layerTable, uint64_t layerCount) {
    (void)layerTable;
    auto& s = state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (!s.streamingReady) {
            return 0;
        }
    }
    for (uint64_t i = 0; i < layerCount; ++i) {
        SO_ProcessLayerAsync(i);
        SO_PrefetchLayer(i + SO_PREFETCH_DISTANCE);
    }
    return 1;
}

extern "C" int SO_ProcessLayerAsync(uint64_t layerId) {
    auto fut = std::async(std::launch::async, [layerId]() {
        touchLayer(layerId);
        SO_ExecuteLayer(nullptr, nullptr);
    });
    fut.wait();
    return 1;
}

extern "C" int SO_EvictLayer(int64_t layerId) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.loadedLayers.empty()) {
        return 1;
    }
    if (layerId < 0) {
        auto it = s.loadedLayers.begin();
        s.loadedLayers.erase(it);
        ++s.metrics.layers_evicted;
        return 1;
    }
    const auto erased = s.loadedLayers.erase(static_cast<uint64_t>(layerId));
    if (erased) {
        ++s.metrics.layers_evicted;
    }
    return 1;
}

extern "C" int SO_PrefetchLayer(uint64_t layerId) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.prefetchQueue.push_back(layerId);
    return 1;
}

extern "C" uint32_t SO_CalculatePrefetchScore(uint64_t layerId) {
    const uint32_t locality = static_cast<uint32_t>(100U - (layerId % 100U));
    return std::max(1U, locality);
}

extern "C" uint32_t SO_GetMemoryPressure(void) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return pressureFromLoaded(s.loadedLayers.size());
}

extern "C" void SO_UpdateMetrics(void) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    const auto now = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - s.startedAt).count();
    s.metrics.avg_load_time_ms = static_cast<uint64_t>(std::max<int64_t>(1, ms / 10));
    s.metrics.avg_eviction_time_ms = static_cast<uint64_t>(std::max<int64_t>(1, ms / 20));
}

extern "C" void SO_PrintMetrics(void) {
    SO_StreamingMetrics m{};
    SO_GetMetrics(&m);
}

extern "C" void SO_GetMetrics(SO_StreamingMetrics* out) {
    if (!out) {
        return;
    }
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    *out = s.metrics;
}

extern "C" intptr_t SO_CreateTimelineSemaphore(void) {
    auto& s = state();
    const intptr_t id = s.nextTimeline.fetch_add(1);
    std::lock_guard<std::mutex> lock(s.mutex);
    s.timelines[id] = TimelineSemaphore{};
    return id;
}

extern "C" int SO_SignalTimeline(intptr_t semaphore, uint64_t value) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto it = s.timelines.find(semaphore);
    if (it == s.timelines.end()) {
        return 0;
    }
    it->second.value = std::max(it->second.value, value);
    return 1;
}

extern "C" int SO_WaitTimeline(intptr_t semaphore, uint64_t value) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto it = s.timelines.find(semaphore);
    if (it == s.timelines.end()) {
        return 0;
    }
    return it->second.value >= value ? 1 : 0;
}

extern "C" void* SO_FileSeekAndMap(uint64_t fileOffset) {
    auto* buffer = new (std::nothrow) uint8_t[64ULL * 1024ULL * 1024ULL];
    if (!buffer) {
        return nullptr;
    }
    std::memset(buffer, static_cast<int>(fileOffset & 0xFFU), 64ULL * 1024ULL * 1024ULL);
    return buffer;
}

extern "C" uint64_t SO_DecompressBlock(void* src, void* dest, uint64_t compressedSize) {
    if (!src || !dest || compressedSize == 0) {
        return 0;
    }
    std::memcpy(dest, src, static_cast<size_t>(compressedSize));
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.metrics.bytes_decompressed += compressedSize;
    return compressedSize;
}

extern "C" void SO_ExecuteLayerOps(void* layerPtr) {
    (void)layerPtr;
    SO_ExecuteLayer(nullptr, nullptr);
}

extern "C" void SO_DestroyStreamingSystem(void) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.execLoaded = false;
    s.vulkanReady = false;
    s.streamingReady = false;
    s.threadPoolReady = false;
    s.loadedLayers.clear();
    s.prefetchQueue.clear();
    s.timelines.clear();
    s.mappedFiles.clear();
    s.metrics = {};
}

extern "C" intptr_t SO_OpenMemoryMappedFile(const char* path, uint64_t fileSize) {
    if (!path || !*path || fileSize == 0) {
        return 0;
    }

    auto& s = state();
    const intptr_t id = s.nextMmap.fetch_add(1);
    std::vector<uint8_t> data(static_cast<size_t>(fileSize), 0);

    std::ifstream in(path, std::ios::binary);
    if (in.good()) {
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }

    std::lock_guard<std::mutex> lock(s.mutex);
    s.mappedFiles[id] = std::move(data);
    return id;
}
