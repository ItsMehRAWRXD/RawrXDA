// Linker fallbacks for Win32IDE when model loader symbols are unavailable.
#include <cstdint>
#include <string>

namespace {
struct ModelLoaderFallbackState {
    bool initialized = false;
    bool devUnlocked = false;
    std::string activeModel;
    uint64_t loadCount = 0;
    uint64_t hotSwapCount = 0;
};

ModelLoaderFallbackState g_state{};
}

extern "C" bool LoadModel(const char* path) {
    if (!g_state.initialized || path == nullptr || path[0] == '\0') {
        return false;
    }
    g_state.activeModel = path;
    g_state.loadCount += 1;
    return true;
}

extern "C" bool ModelLoaderInit(void) {
    g_state.initialized = true;
    if (g_state.activeModel.empty()) {
        g_state.activeModel = "fallback/default.gguf";
    }
    return true;
}

extern "C" bool HotSwapModel(const char* model_id) {
    if (!g_state.initialized || model_id == nullptr || model_id[0] == '\0') {
        return false;
    }
    g_state.activeModel = model_id;
    g_state.hotSwapCount += 1;
    return true;
}

extern "C" bool ModelLoaderShutdown(void) {
    if (!g_state.initialized) {
        return true;
    }
    g_state.initialized = false;
    g_state.activeModel.clear();
    return true;
}

extern "C" int64_t Enterprise_DevUnlock(void) {
    return 0;
}
