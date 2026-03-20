// Linker fallbacks for Win32IDE when model loader symbols are unavailable.
#include <cstdint>

extern "C" bool LoadModel(const char* path) {
    (void)path;
    return false;
}

extern "C" bool ModelLoaderInit(void) {
    return false;
}

extern "C" bool HotSwapModel(const char* model_id) {
    (void)model_id;
    return false;
}

extern "C" bool ModelLoaderShutdown(void) {
    return false;
}
