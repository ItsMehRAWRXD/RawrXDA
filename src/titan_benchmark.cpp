// ============================================================================
// titan_benchmark.cpp — Non-Simulated TPS Benchmark for RawrXD Titan
// ============================================================================
// Loads RawrXD_Titan.dll directly, initializes with the real 38.8GB GGUF,
// fires inference, measures wall-clock latency, reports TPS.
// SUBSYSTEM:CONSOLE for stdout output.
// ============================================================================

#include <stdint.h>
#include <string.h>
#include <intrin.h>

// ============================================================================
// Win32 Forward Declarations (no windows.h)
// ============================================================================

typedef void*          HANDLE;
typedef void*          HMODULE;
typedef void*          LPVOID;
typedef unsigned long  DWORD;
typedef signed long    LONG;
typedef int            BOOL;

extern "C" {
    HMODULE __stdcall LoadLibraryA(const char* name);
    void*   __stdcall GetProcAddress(HMODULE h, const char* proc);
    BOOL    __stdcall FreeLibrary(HMODULE h);
    DWORD   __stdcall GetLastError(void);
    void    __stdcall Sleep(DWORD ms);
    HANDLE  __stdcall GetStdHandle(DWORD nStdHandle);
    BOOL    __stdcall WriteFile(HANDLE h, const void* buf, DWORD nBytes,
                                DWORD* written, void* overlapped);
    void    __stdcall ExitProcess(unsigned int code);
    BOOL    __stdcall QueryPerformanceCounter(int64_t* lpCount);
    BOOL    __stdcall QueryPerformanceFrequency(int64_t* lpFreq);
    HANDLE  __stdcall CreateFileA(const char* fn, DWORD access, DWORD share,
                                   void* sa, DWORD disp, DWORD flags, HANDLE tmpl);
    DWORD   __stdcall GetFileSize(HANDLE h, DWORD* hi);
    BOOL    __stdcall CloseHandle(HANDLE h);
    int     __cdecl   wsprintfA(char* buf, const char* fmt, ...);
}

#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE  ((DWORD)-12)
#define GENERIC_READ_     0x80000000UL
#define FILE_SHARE_READ_  0x00000001UL
#define OPEN_EXISTING_    3UL
#define INVALID_HANDLE_   ((HANDLE)(long long)-1)

// ============================================================================
// Titan DLL Interface
// ============================================================================

typedef struct {
    const char* prompt;
    int         max_tokens;
    float       temperature;
    void      (*callback)(const char* text, int len);
} TITAN_PARAMS;

typedef int  (*PFN_TITAN_INITIALIZE)(const char* modelPath);
typedef int  (*PFN_TITAN_INFER_ASYNC)(TITAN_PARAMS* params);
typedef void (*PFN_TITAN_SHUTDOWN)(void);

// ============================================================================
// Console Output Helpers
// ============================================================================

static HANDLE g_hStdOut = 0;
static HANDLE g_hStdErr = 0;

static void ConOut(const char* s) {
    DWORD written = 0;
    int len = 0;
    while (s[len]) len++;
    WriteFile(g_hStdOut, s, (DWORD)len, &written, 0);
}

static void ConOutLine(const char* s) {
    ConOut(s);
    ConOut("\r\n");
}

static void ConOutInt(const char* label, int64_t val) {
    char buf[256];
    wsprintfA(buf, "%s%I64d", label, val);
    ConOutLine(buf);
}

static void ConOutFloat(const char* label, double val) {
    // Manual float formatting (no CRT printf for floats)
    char buf[256];
    int whole = (int)val;
    int frac = (int)((val - (double)whole) * 1000.0);
    if (frac < 0) frac = -frac;
    wsprintfA(buf, "%s%d.%03d", label, whole, frac);
    ConOutLine(buf);
}

// ============================================================================
// Inference Callback State
// ============================================================================

static volatile long g_callbackFired = 0;
static int64_t       g_callbackTime  = 0;
static char          g_responseText[4096] = {0};
static int           g_responseLen = 0;
static int           g_tokenCount  = 0;

static void BenchmarkCallback(const char* text, int len) {
    // Record when callback fires
    QueryPerformanceCounter(&g_callbackTime);
    
    if (text && len > 0) {
        int copyLen = len < 4095 ? len : 4095;
        for (int i = 0; i < copyLen; i++) {
            g_responseText[i] = text[i];
        }
        g_responseText[copyLen] = 0;
        g_responseLen = copyLen;
        
        // Estimate token count (rough: ~4 chars per token for English)
        g_tokenCount = (copyLen + 3) / 4;
        if (g_tokenCount < 1) g_tokenCount = 1;
    }
    
    _InterlockedExchange(&g_callbackFired, 1);
}

// ============================================================================
// Main — Console Entry Point
// ============================================================================

extern "C" void __stdcall mainCRTStartup() {
    g_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    g_hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    
    ConOutLine("============================================================");
    ConOutLine("  RawrXD Titan — Non-Simulated TPS Benchmark");
    ConOutLine("  Phase 4A Live Inference Validation");
    ConOutLine("============================================================");
    ConOut("\r\n");
    
    // ── Step 1: Verify GGUF file exists and get size ──
    const char* ggufPath = "F:\\OllamaModels\\BigDaddyG-UNLEASHED-Q4_K_M.gguf";
    
    ConOut("[1/6] Probing GGUF weights: ");
    ConOutLine(ggufPath);
    
    HANDLE hFile = CreateFileA(ggufPath, GENERIC_READ_, FILE_SHARE_READ_,
                                0, OPEN_EXISTING_, 0, 0);
    if (hFile == INVALID_HANDLE_) {
        ConOutLine("[FAIL] Cannot open GGUF file!");
        ConOutInt("       GetLastError: ", GetLastError());
        ExitProcess(1);
    }
    
    DWORD sizeHi = 0;
    DWORD sizeLo = GetFileSize(hFile, &sizeHi);
    uint64_t fileSize = ((uint64_t)sizeHi << 32) | sizeLo;
    CloseHandle(hFile);
    
    ConOutInt("       File size: ", (int64_t)fileSize);
    ConOutFloat("       Size (GB): ", (double)fileSize / (1024.0 * 1024.0 * 1024.0));
    ConOut("\r\n");
    
    // ── Step 2: Load RawrXD_Titan.dll ──
    ConOut("[2/6] Loading RawrXD_Titan.dll... ");
    
    HMODULE hDll = LoadLibraryA("D:\\rawrxd\\bin\\RawrXD_Titan.dll");
    if (!hDll) {
        hDll = LoadLibraryA("RawrXD_Titan.dll");
    }
    if (!hDll) {
        ConOutLine("FAILED!");
        ConOutInt("       GetLastError: ", GetLastError());
        ExitProcess(2);
    }
    ConOutLine("OK");
    
    // ── Step 3: Resolve exports ──
    ConOut("[3/6] Resolving exports... ");
    
    PFN_TITAN_INITIALIZE pfnInit = 
        (PFN_TITAN_INITIALIZE)GetProcAddress(hDll, "Titan_Initialize");
    PFN_TITAN_INFER_ASYNC pfnInfer = 
        (PFN_TITAN_INFER_ASYNC)GetProcAddress(hDll, "Titan_InferAsync");
    PFN_TITAN_SHUTDOWN pfnShutdown = 
        (PFN_TITAN_SHUTDOWN)GetProcAddress(hDll, "Titan_Shutdown");
    
    if (!pfnInit || !pfnInfer || !pfnShutdown) {
        ConOutLine("FAILED!");
        if (!pfnInit)     ConOutLine("  - Titan_Initialize: NOT FOUND");
        if (!pfnInfer)    ConOutLine("  - Titan_InferAsync: NOT FOUND");
        if (!pfnShutdown) ConOutLine("  - Titan_Shutdown:   NOT FOUND");
        ExitProcess(3);
    }
    ConOutLine("OK");
    ConOutLine("       Titan_Initialize:  resolved");
    ConOutLine("       Titan_InferAsync:  resolved");
    ConOutLine("       Titan_Shutdown:    resolved");
    ConOut("\r\n");
    
    // ── Step 4: Initialize with GGUF weights ──
    ConOut("[4/6] Titan_Initialize(\"");
    ConOut(ggufPath);
    ConOutLine("\")...");
    
    int64_t tInitStart, tInitEnd, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&tInitStart);
    
    int initResult = pfnInit(ggufPath);
    
    QueryPerformanceCounter(&tInitEnd);
    
    double initMs = (double)(tInitEnd - tInitStart) * 1000.0 / (double)freq;
    
    if (initResult != 0) {
        ConOutInt("       [FAIL] Titan_Initialize returned: ", initResult);
        ConOutFloat("       Time elapsed: ", initMs);
        ConOutLine(" ms");
        // Don't exit — log the failure but continue to report
        ConOutLine("       Continuing with diagnostics...");
    } else {
        ConOutLine("       [OK] Titan_Initialize succeeded");
        ConOutFloat("       Init latency: ", initMs);
        ConOutLine("");
    }
    ConOut("\r\n");
    
    // ── Step 5: Fire Inference + Measure TPS ──
    ConOutLine("[5/6] Titan_InferAsync — Live Inference Benchmark");
    
    const char* testPrompts[] = {
        "// Complete this function:\nint fibonacci(int n) {",
        "// Explain what this code does:\nmov rax, [rcx+8]\nshl rax, 4\nadd rax, rdx",
        "Write a MASM64 procedure that reverses a null-terminated string in-place.",
    };
    const int numPrompts = 3;
    
    int64_t totalInferTime = 0;
    int totalTokens = 0;
    int successCount = 0;
    
    for (int p = 0; p < numPrompts; p++) {
        char hdr[128];
        wsprintfA(hdr, "       Prompt %d/%d: ", p + 1, numPrompts);
        ConOut(hdr);
        
        // Truncated prompt display
        char truncated[60];
        int tlen = 0;
        while (testPrompts[p][tlen] && tlen < 55) {
            if (testPrompts[p][tlen] == '\n') {
                truncated[tlen] = ' ';
            } else {
                truncated[tlen] = testPrompts[p][tlen];
            }
            tlen++;
        }
        truncated[tlen] = 0;
        ConOut(truncated);
        ConOutLine("...");
        
        // Reset callback state
        g_callbackFired = 0;
        g_callbackTime = 0;
        g_responseLen = 0;
        g_tokenCount = 0;
        g_responseText[0] = 0;
        
        TITAN_PARAMS params;
        params.prompt = testPrompts[p];
        params.max_tokens = 256;
        params.temperature = 0.4f;
        params.callback = BenchmarkCallback;
        
        int64_t tInferStart;
        QueryPerformanceCounter(&tInferStart);
        
        int inferResult = pfnInfer(&params);
        
        if (inferResult != 0) {
            ConOutInt("       [SKIP] InferAsync returned: ", inferResult);
            if (inferResult == -2) {
                ConOutLine("       (Previous request still in flight — waiting...)");
                // Wait for previous to finish
                int waits = 0;
                while (g_callbackFired == 0 && waits < 3000) {
                    Sleep(100);
                    waits++;
                }
            }
            continue;
        }
        
        // Wait for callback (max 300 seconds — 70B cold-load can take minutes)
        int waitCount = 0;
        int maxWait = 3000;  // 3000 × 100ms = 300s
        while (g_callbackFired == 0 && waitCount < maxWait) {
            Sleep(100);
            waitCount++;
        }
        
        if (g_callbackFired) {
            double inferMs = (double)(g_callbackTime - tInferStart) * 1000.0 / (double)freq;
            double tps = (g_tokenCount > 0 && inferMs > 0) 
                         ? (double)g_tokenCount / (inferMs / 1000.0) 
                         : 0.0;
            
            ConOutFloat("       Latency:    ", inferMs);
            ConOutInt("       Tokens:     ", g_tokenCount);
            ConOutInt("       Chars:      ", g_responseLen);
            ConOutFloat("       TPS:        ", tps);
            
            // Show first 500 chars of response
            ConOut("       Response:   \"");
            char preview[501];
            int plen = g_responseLen < 500 ? g_responseLen : 500;
            for (int i = 0; i < plen; i++) preview[i] = g_responseText[i];
            preview[plen] = 0;
            ConOut(preview);
            ConOutLine("\"");
            ConOut("\r\n");
            
            totalInferTime += (int64_t)inferMs;
            totalTokens += g_tokenCount;
            successCount++;
            
            // Small delay between prompts to let thread complete
            Sleep(500);
        } else {
            ConOutLine("       [TIMEOUT] No callback after 300s");
            ConOut("\r\n");
        }
    }
    
    // ── Step 6: Summary ──
    ConOutLine("============================================================");
    ConOutLine("  BENCHMARK RESULTS — Phase 4A Non-Simulated");
    ConOutLine("============================================================");
    ConOutFloat("  GGUF Size:           ", (double)fileSize / (1024.0 * 1024.0 * 1024.0));
    ConOutFloat("  Init Latency:        ", initMs);
    ConOutInt("  Prompts Tested:      ", numPrompts);
    ConOutInt("  Successful:          ", successCount);
    ConOutInt("  Total Tokens:        ", totalTokens);
    ConOutInt("  Total Infer Time:    ", totalInferTime);
    
    if (successCount > 0 && totalInferTime > 0) {
        double avgTps = (double)totalTokens / ((double)totalInferTime / 1000.0);
        double avgLatency = (double)totalInferTime / (double)successCount;
        ConOutFloat("  Avg TPS:             ", avgTps);
        ConOutFloat("  Avg Latency:         ", avgLatency);
    }
    
    if (initResult != 0) {
        ConOutLine("");
        ConOutInt("  [!] Init failed with code: ", initResult);
        ConOutLine("  [!] TPS reflects diagnostic mode, not full transformer pass");
    }
    
    ConOutLine("============================================================");
    ConOut("\r\n");
    
    // Shutdown
    ConOut("Titan_Shutdown... ");
    pfnShutdown();
    ConOutLine("OK");
    
    FreeLibrary(hDll);
    ConOutLine("Benchmark complete.");
    
    ExitProcess(0);
}
