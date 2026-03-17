#include <windows.h>
#include <cstdint>

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
