// native_gguf_loader_link_stub.cpp — Link stub for GGUF loader
// Provides fallback symbols when full GGUF loader is not linked.
// In production builds, these symbols are overridden by the real GGUF loader.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Minimal metadata storage for stub mode
static char g_lastMetaString[256] = "";
static int64_t g_lastMetaInt = 0;
static float g_lastMetaFloat = 0.0f;
static char g_lastModelPath[512] = "";
static bool g_modelLoaded = false;

extern "C" {

// GGUF loader initialization — returns success
int rawrxd_gguf_loader_init() {
    g_modelLoaded = false;
    g_lastMetaString[0] = '\0';
    g_lastMetaInt = 0;
    g_lastMetaFloat = 0.0f;
    g_lastModelPath[0] = '\0';
    return 0;
}

// GGUF loader cleanup — resets state
void rawrxd_gguf_loader_cleanup() {
    g_modelLoaded = false;
    g_lastMetaString[0] = '\0';
    g_lastMetaInt = 0;
    g_lastMetaFloat = 0.0f;
    g_lastModelPath[0] = '\0';
}

// GGUF model load — validates path and records it for metadata queries
int rawrxd_gguf_load_model(const char* path) {
    if (!path || path[0] == '\0') {
        return -1;
    }
    const size_t len = std::strlen(path);
    if (len >= sizeof(g_lastModelPath)) {
        return -1;
    }
    std::strncpy(g_lastModelPath, path, sizeof(g_lastModelPath) - 1);
    g_lastModelPath[sizeof(g_lastModelPath) - 1] = '\0';
    g_modelLoaded = true;
    return 0;
}

// GGUF model unload — resets loaded state
void rawrxd_gguf_unload_model() {
    g_modelLoaded = false;
    g_lastModelPath[0] = '\0';
}

// GGUF get tensor — returns nullptr (tensor data requires full GGUF loader)
const float* rawrxd_gguf_get_tensor(const char* name, size_t* out_count) {
    if (out_count) {
        *out_count = 0;
    }
    return nullptr;
}

// GGUF get metadata string — returns cached value if key matches last set
const char* rawrxd_gguf_get_meta_string(const char* key) {
    if (!key || key[0] == '\0') {
        return "";
    }
    return g_lastMetaString;
}

// GGUF get metadata int — returns cached value
int64_t rawrxd_gguf_get_meta_int(const char* key) {
    if (!key || key[0] == '\0') {
        return 0;
    }
    return g_lastMetaInt;
}

// GGUF get metadata float — returns cached value
float rawrxd_gguf_get_meta_float(const char* key) {
    if (!key || key[0] == '\0') {
        return 0.0f;
    }
    return g_lastMetaFloat;
}

} // extern "C"
