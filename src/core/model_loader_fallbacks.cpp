// Win32IDE bridge implementations for legacy model-loader C exports.
// These are functional fallbacks that keep state and validate model paths.

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>

namespace {
std::mutex g_modelLoaderMutex;
std::atomic<bool> g_modelLoaderInitialized{false};
std::string g_loadedModelPath;
}

extern "C" bool LoadModel(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    if (!g_modelLoaderInitialized.load(std::memory_order_acquire)) {
        return false;
    }

    const std::filesystem::path modelPath(path);
    std::error_code ec;
    const bool exists = std::filesystem::exists(modelPath, ec);
    if (ec || !exists) {
        return false;
    }

    g_loadedModelPath = modelPath.string();
    return true;
}

extern "C" bool ModelLoaderInit(void) {
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_modelLoaderInitialized.store(true, std::memory_order_release);
    return true;
}

extern "C" bool HotSwapModel(const char* model_id) {
    return LoadModel(model_id);
}

extern "C" bool ModelLoaderShutdown(void) {
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_loadedModelPath.clear();
    g_modelLoaderInitialized.store(false, std::memory_order_release);
    return true;
}
