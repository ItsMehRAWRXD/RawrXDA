#include <windows.h>
#include <stdio.h>
#include <string>

typedef struct {
    const char*  prompt;
    int          max_tokens;
    float        temperature;
    void       (*callback)(const char* text, int len);
} TITAN_INFERENCE_PARAMS;

typedef int  (*PFN_TITAN_INITIALIZE)(const char* model_path);
typedef int  (*PFN_TITAN_INFER_ASYNC)(TITAN_INFERENCE_PARAMS* params);
typedef void (*PFN_TITAN_SHUTDOWN)(void);

static void on_result(const char* text, int len) {
    printf("[CALLBACK] Result (%d chars):\n%.*s\n", len, len, text);
}

int main() {
    HMODULE hDll = LoadLibraryA("RawrXD_Titan.dll");
    if (!hDll) {
        printf("FAIL: LoadLibraryA(RawrXD_Titan.dll) failed: %lu\n", GetLastError());
        return 1;
    }
    printf("OK: Loaded RawrXD_Titan.dll\n");

    PFN_TITAN_INITIALIZE pfnInit = (PFN_TITAN_INITIALIZE)GetProcAddress(hDll, "Titan_Initialize");
    PFN_TITAN_INFER_ASYNC pfnInfer = (PFN_TITAN_INFER_ASYNC)GetProcAddress(hDll, "Titan_InferAsync");
    PFN_TITAN_SHUTDOWN pfnShutdown = (PFN_TITAN_SHUTDOWN)GetProcAddress(hDll, "Titan_Shutdown");

    if (!pfnInit || !pfnInfer || !pfnShutdown) {
        printf("FAIL: Missing exports\n");
        return 1;
    }
    printf("OK: All exports resolved\n");

    int rc = pfnInit("models\\70b_simulation.gguf");
    printf("Titan_Initialize returned: %d\n", rc);

    TITAN_INFERENCE_PARAMS params = {};
    params.prompt = "def fibonacci(n):\n    ";
    params.max_tokens = 50;
    params.temperature = 0.7f;
    params.callback = on_result;

    printf("Calling Titan_InferAsync...\n");
    rc = pfnInfer(&params);
    printf("Titan_InferAsync returned: %d\n", rc);

    pfnShutdown();
    FreeLibrary(hDll);
    return 0;
}
