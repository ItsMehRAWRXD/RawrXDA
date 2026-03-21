// Linker fallbacks for Win32IDE when model loader symbols are unavailable.
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <cstdlib>

namespace {
std::mutex g_modelLoaderMutex;
std::string g_loadedModelPath;
std::atomic<bool> g_modelLoaderInitialized{false};
std::atomic<uint64_t> g_hotSwapCount{0};
}

extern "C" bool LoadModel(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return false;
    }
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec || !exists) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_loadedModelPath = path;
    return true;
}

extern "C" bool ModelLoaderInit(void) {
    g_modelLoaderInitialized.store(true, std::memory_order_relaxed);
    return true;
}

extern "C" bool HotSwapModel(const char* model_id) {
    if (model_id == nullptr || model_id[0] == '\0' || !g_modelLoaderInitialized.load(std::memory_order_relaxed)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_loadedModelPath = model_id;
    g_hotSwapCount.fetch_add(1, std::memory_order_relaxed);
    return true;
}

extern "C" bool ModelLoaderShutdown(void) {
    g_modelLoaderInitialized.store(false, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_loadedModelPath.clear();
    return true;
}

extern "C" bool Enterprise_DevUnlock(void) {
    const char* unlock = std::getenv("RAWRXD_ENTERPRISE_UNLOCK");
    return unlock != nullptr && unlock[0] == '1';
}
