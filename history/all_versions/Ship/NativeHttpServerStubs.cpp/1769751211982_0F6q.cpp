#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" ULONG __stdcall HttpCloseRequestQueue(HANDLE hRequestQueue);

extern "C" ULONG __stdcall HttpCloseRequestHandle(HANDLE hRequestQueue) {
    return HttpCloseRequestQueue(hRequestQueue);
}

extern "C" uint32_t __stdcall InferenceEngine_Initialize() {
    return 0;
}

extern "C" uint32_t __stdcall InferenceEngine_LoadModel(const char* /*modelPath*/) {
    return 0;
}

extern "C" uint32_t __stdcall InferenceEngine_Generate(const char* /*prompt*/, char* /*outBuffer*/, uint32_t /*outBufferSize*/) {
    return 1;
}

static void EscapeJsonString(const char* src, char* dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) {
        return;
    }
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 2 < dest_size; ++i) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            dest[out++] = '\\';
            dest[out++] = c;
        } else if (c == '\n') {
            dest[out++] = '\\';
            dest[out++] = 'n';
        } else if (c == '\r') {
            dest[out++] = '\\';
            dest[out++] = 'r';
        } else if (c == '\t') {
            dest[out++] = '\\';
            dest[out++] = 't';
        } else {
            dest[out++] = c;
        }
    }
    dest[out] = '\0';
}

extern "C" uint32_t __stdcall ToolExecuteJson(const char* json, char* outBuffer, uint32_t outBufferSize) {
    if (!outBuffer || outBufferSize == 0) {
        return 1;
    }

    char escaped[2048];
    escaped[0] = '\0';
    if (json) {
        EscapeJsonString(json, escaped, sizeof(escaped));
    }

    std::snprintf(outBuffer, outBufferSize,
                  "{\"status\":\"ok\",\"tool\":\"stub\",\"result\":\"%s\"}",
                  escaped);
    return 0;
}
