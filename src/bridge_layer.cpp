// ============================================================================
// bridge_layer.cpp - Titan 70B Inference Bridge
// ============================================================================
// Routes code context from ui.asm -> RawrXD_Titan.dll -> ghost text callback
// Phase 4A: Non-Simulated Live Inference via Ollama HTTP (70B Q4_K_M)
// ============================================================================

// C-compatible standard headers (MUST be outside extern "C")
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <intrin.h>

// Windows API forward declarations (no windows.h dependency)
typedef void* HMODULE;
typedef void* HWND;
typedef void* HANDLE;
typedef void* LPVOID;
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef unsigned long DWORD;
typedef signed long LONG;
typedef unsigned short WORD;
typedef DWORD (__stdcall *LPTHREAD_START_ROUTINE)(LPVOID);

extern "C" {
    HMODULE __stdcall LoadLibraryA(const char* lpLibFileName);
    HMODULE __stdcall GetModuleHandleW(const wchar_t* lpModuleName);
    void* __stdcall GetProcAddress(HMODULE hModule, const char* lpProcName);
    HANDLE __stdcall CreateThread(void* sa, size_t stack, LPTHREAD_START_ROUTINE fn, LPVOID param, DWORD flags, DWORD* tid);
    BOOL __stdcall CloseHandle(HANDLE h);
}

// Global required by ui.asm (EXTERN g_hInstance:QWORD)
extern "C" void* g_hInstance = nullptr;

// Scorer globals (EXTERN'd by ui.asm ScoreCandidate/PruneCandidates)
extern "C" unsigned long g_swarmDeviceCount = 1;
extern "C" unsigned long g_remoteCount     = 0;
extern "C" unsigned long g_accumulatedSteps = 0;

// ============================================================================
// Constants
// ============================================================================

static const int MAX_GHOST_LEN_CPP   = 1024;

// ============================================================================
// Titan 70B Live Inference Protocol - Phase 4A
// ============================================================================

typedef struct {
    const char* prompt;
    int max_tokens;
    float temperature;
    void (*callback)(const char* text, int len);
} TITAN_PARAMS;

typedef int (*TITAN_PROC)(TITAN_PARAMS*);
typedef int (*TITAN_INIT_PROC)(const char*);

typedef void (*TITAN_ABORT_PROC)(void);

static HMODULE g_hTitan = NULL;
static TITAN_PROC g_TitanInfer = NULL;
static TITAN_INIT_PROC g_TitanInit = NULL;
static TITAN_ABORT_PROC g_TitanAbort = NULL;
static int g_TitanReady = 0;

// Forward declaration: ASM-callable ghost text callback
extern "C" void Bridge_OnSuggestionReady(const wchar_t* text, int len);

// ============================================================================
// ============================================================================\n// Titan_Live_Callback - C-String to WCHAR bridge for 70B inference\n// ============================================================================

static void Titan_Live_Callback(const char* text, int len) {
    if (!text || len <= 0) return;

    static wchar_t wbuf[MAX_GHOST_LEN_CPP];
    int i = 0;
    while (text[i] && i < MAX_GHOST_LEN_CPP - 1) {
        wbuf[i] = (wchar_t)(unsigned char)text[i];
        i++;
    }
    wbuf[i] = 0;
    Bridge_OnSuggestionReady(wbuf, i);
}

// ============================================================================
// Bridge_OnSuggestionComplete - Callback when model returns tokens
// ============================================================================

// ============================================================================
// ASM-Callable Functions (extern "C" linkage)
// ============================================================================

extern "C" int Bridge_RequestSuggestion(const wchar_t* text, int row, int col) {
    if (!text) return -1;

    // Phase 4A: Dynamic Titan Activation
    if (!g_hTitan) {
        g_hTitan = LoadLibraryA("RawrXD_Titan.dll");
        if (g_hTitan) {
            g_TitanInfer = (TITAN_PROC)GetProcAddress(g_hTitan, "Titan_InferAsync");
            g_TitanInit  = (TITAN_INIT_PROC)GetProcAddress(g_hTitan, "Titan_Initialize");
            g_TitanAbort = (TITAN_ABORT_PROC)GetProcAddress(g_hTitan, "Titan_Abort");
        }
    }

    // Initialize with GGUF weights on first call
    if (g_TitanInit && !g_TitanReady) {
        // Primary: BigDaddyG 70B Q4_K_M weights
        int rc = g_TitanInit("F:\\OllamaModels\\BigDaddyG-UNLEASHED-Q4_K_M.gguf");
        if (rc != 0) {
            // Fallback: local models directory
            rc = g_TitanInit("models\\fallback.gguf");
        }
        g_TitanReady = (rc == 0) ? 1 : -1;
    }

    // Convert WCHAR context to narrow for 70B consumption
    char context[4096];
    int len = 0;
    while (text[len] && len < 4095) {
        context[len] = (char)text[len];
        len++;
    }
    context[len] = 0;

    if (g_TitanInfer && g_TitanReady == 1) {
        // Live 70B Path: F:\OllamaModels\BigDaddyG-UNLEASHED-Q4_K_M.gguf
        static TITAN_PARAMS p;
        p.prompt = context;
        p.max_tokens = 256;
        p.temperature = 0.4f;
        p.callback = Titan_Live_Callback;
        return g_TitanInfer(&p);
    }

    // No inference available — notify ASM side to clear pending state
    // by sending an empty callback (prevents g_inferencePending from being stuck)
    static const wchar_t emptyMsg[] = L"";
    Bridge_OnSuggestionReady(emptyMsg, 0);
    return -1;
}

// RLHF Tracking Counters (non-simulated)
static volatile long g_rlhfAcceptCount = 0;
static volatile long g_rlhfRejectCount = 0;
static volatile long g_rlhfTotalSuggestions = 0;

extern "C" int Bridge_SubmitCompletion(int accepted) {
    _InterlockedIncrement(&g_rlhfTotalSuggestions);
    if (accepted) {
        _InterlockedIncrement(&g_rlhfAcceptCount);
    } else {
        _InterlockedIncrement(&g_rlhfRejectCount);
    }
    return 0;
}

extern "C" void Bridge_ClearSuggestion() {
    // Ghost text state is managed directly by ui.asm (g_ghostActive).
    // This function exists for forward compatibility.
}

extern "C" void Bridge_AbortInference() {
    if (g_TitanAbort) {
        g_TitanAbort();  // Sets g_cancelRequest = 1 in DLL
    }
}

// ============================================================================
// End of bridge_layer.cpp - Phase 4A Non-Simulated
// ============================================================================
