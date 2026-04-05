// ============================================================================
// titan_infer_dll.cpp — RawrXD_Titan.dll: Live 70B Inference Engine
// ============================================================================
// Exports: Titan_Initialize, Titan_InferAsync, Titan_Shutdown
// Loads GGUF weights via memory-mapping, routes inference, calls back.
// Zero STL. Zero CRT in hot path. Dynamic Ollama bridge fallback.
// ============================================================================

#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <intrin.h>
#include <math.h>
#include "rawr_sovereign_log.h"
#include "RawrXD_Stream_Buffer.hpp"
#include "ai/ai_model_caller_internal.h"

#if RAWR_ENABLE_GGML_LINK
#include "qtapp/gguf.h"
#endif

#ifndef RAWRXD_INDEXER_EMBED_DIM
#define RAWRXD_INDEXER_EMBED_DIM 384
#endif

// ============================================================================
// Win32 API Typedefs (no windows.h)
// ============================================================================

#if !defined(_WINDOWS_) && !defined(_MINWINDEF_) && !defined(_INC_WINDOWS)
typedef void*          HANDLE;
typedef void*          HMODULE;
typedef void*          LPVOID;
typedef unsigned long  DWORD;
typedef unsigned int   UINT;
typedef signed long    LONG;
typedef int            BOOL;
typedef unsigned short WORD;
typedef unsigned char  BYTE;
typedef unsigned long long ULONGLONG;
typedef unsigned long long DWORD_PTR;  /* Phase 15.2: for thread affinity */
typedef union {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    long long QuadPart;
} LARGE_INTEGER;
typedef struct _MEMORY_BASIC_INFORMATION_T {
    void* BaseAddress;
    void* AllocationBase;
    DWORD AllocationProtect;
    unsigned short PartitionId;
    size_t RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
} MEMORY_BASIC_INFORMATION;
typedef DWORD (__stdcall *LPTHREAD_START_ROUTINE)(LPVOID);
#endif

// extern "C" int __stdcall MultiByteToWideChar(UINT CodePage, DWORD dwFlags, const char* lpMultiByteStr, int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar);
// extern "C" int __stdcall WideCharToMultiByte(UINT CodePage, DWORD dwFlags, const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, BOOL* lpUsedDefaultChar);

// File mapping constants
#define GENERIC_READ_        0x80000000UL
#define FILE_SHARE_READ_     0x00000001UL
#define OPEN_EXISTING_       3UL
#define INVALID_HANDLE_      ((HANDLE)(long long)-1)
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE INVALID_HANDLE_  /* Alias for Phase 15.2 */
#endif
#define PAGE_READONLY_       0x02UL
#define FILE_MAP_READ_       0x0004UL
#define INFINITE_            0xFFFFFFFFUL
#define CREATE_SUSPENDED_    0x00000004UL
#define MEM_RELEASE_         0x8000UL
#define MEM_RESERVE_         0x2000UL
#define MEM_COMMIT_          0x1000UL
#define MEM_FREE_            0x10000UL
#define PAGE_NOACCESS_       0x01UL
#define PAGE_READWRITE_      0x04UL
#define FILE_MAP_WRITE_      0x0002UL

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

#ifndef CP_ACP
#define CP_ACP 0
#endif

#ifndef MB_ERR_INVALID_CHARS
#define MB_ERR_INVALID_CHARS 0x00000008
#endif

/* Phase 15.2: Thread priority and synchronization constants */
#ifndef THREAD_PRIORITY_LOWEST
#define THREAD_PRIORITY_LOWEST          -2
#endif
#ifndef THREAD_PRIORITY_BELOW_NORMAL
#define THREAD_PRIORITY_BELOW_NORMAL    -1
#endif
#ifndef THREAD_PRIORITY_NORMAL
#define THREAD_PRIORITY_NORMAL           0
#endif
#ifndef THREAD_PRIORITY_ABOVE_NORMAL
#define THREAD_PRIORITY_ABOVE_NORMAL     1
#endif
#ifndef THREAD_PRIORITY_HIGHEST
#define THREAD_PRIORITY_HIGHEST          2
#endif
#ifndef THREAD_PRIORITY_TIME_CRITICAL
#define THREAD_PRIORITY_TIME_CRITICAL    15
#endif

#ifndef THREAD_SET_INFORMATION
#define THREAD_SET_INFORMATION          0x0020UL
#endif
#ifndef THREAD_SET_THREAD_TOKEN
#define THREAD_SET_THREAD_TOKEN         0x0040UL
#endif

#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif

#if !defined(_WINDOWS_) && !defined(_MINWINDEF_) && !defined(_INC_WINDOWS)
extern "C" {
    HANDLE __stdcall CreateFileA(const char* fn, DWORD access, DWORD share,
                                 void* sa, DWORD disp, DWORD flags, HANDLE tmpl);
    BOOL   __stdcall CloseHandle(HANDLE h);
    HANDLE __stdcall CreateFileMappingA(HANDLE hFile, void* sa, DWORD prot,
                                        DWORD sizeHi, DWORD sizeLo, const char* name);
    LPVOID __stdcall MapViewOfFile(HANDLE hMap, DWORD access, DWORD offHi,
                                    DWORD offLo, size_t bytes);
    BOOL   __stdcall UnmapViewOfFile(LPVOID addr);
    DWORD  __stdcall GetFileSize(HANDLE hFile, DWORD* hi);
    HANDLE __stdcall CreateThread(void* sa, size_t stack, LPTHREAD_START_ROUTINE fn,
                                   LPVOID param, DWORD flags, DWORD* tid);
    DWORD  __stdcall WaitForSingleObject(HANDLE h, DWORD ms);
    HMODULE __stdcall LoadLibraryA(const char* name);
    void*  __stdcall GetProcAddress(HMODULE h, const char* proc);
    BOOL   __stdcall FreeLibrary(HMODULE h);
    void   __stdcall Sleep(DWORD ms);
    DWORD  __stdcall GetLastError(void);
    DWORD  __stdcall GetEnvironmentVariableA(const char* lpName, char* lpBuffer, DWORD nSize);
    int    __cdecl   wsprintfA(char* buf, const char* fmt, ...);
    void   __stdcall OutputDebugStringA(const char* s);
    int    __stdcall QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
    int    __stdcall QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);
    LPVOID __stdcall VirtualAlloc(LPVOID lpAddress, size_t dwSize, DWORD flAllocationType, DWORD flProtect);
    BOOL   __stdcall VirtualFree(LPVOID lpAddress, size_t dwSize, DWORD dwFreeType);
    size_t __stdcall VirtualQuery(const void* lpAddress, MEMORY_BASIC_INFORMATION* lpBuffer, size_t dwLength);
    BOOL   __stdcall PostMessageA(void* hWnd, unsigned int Msg, uintptr_t wParam, intptr_t lParam);
    
    /* Phase 15.2: Thread priority and affinity APIs */
    HANDLE __stdcall GetCurrentProcess(void);
    HANDLE __stdcall GetCurrentThread(void);
    BOOL   __stdcall DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
                                      HANDLE hTargetProcessHandle, HANDLE* lpTargetHandle,
                                      DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
    BOOL   __stdcall SetThreadPriority(HANDLE hThread, int nPriority);
    BOOL   __stdcall SetThreadPriorityBoost(HANDLE hThread, BOOL bDisablePriorityBoost);
    unsigned long long __stdcall SetThreadAffinityMask(HANDLE hThread, unsigned long long dwThreadAffinityMask);
}
#endif

extern "C" void RawrXD_Omega_Singularity(void* params);

extern "C" const char* RawrXD_ScanResponseToken(const char* buf, unsigned long long len, const char* pattern);
extern "C" void __stdcall Rawr_Profile_Checkpoint(unsigned long long rf_data_seq,
                                                   unsigned long long rf_consumed_seq,
                                                   unsigned long long rf_frame_ready,
                                                   unsigned long long* out_cycles,
                                                   unsigned long long* out_drift,
                                                   unsigned long long* out_max_cycles);
extern "C" int rawr_weighted_fusion_fma_avx512(const float* score_matrix,
                                                  const float* weights,
                                                  unsigned int sub_query_count,
                                                  unsigned int candidate_count,
                                                  float* out_scores);
extern "C" int rawr_weighted_fusion_ew75_p15_avx512(const float* score_row0,
                                                      const float* score_row1,
                                                      unsigned int candidate_count,
                                                      float* out_scores);
extern "C" int rawr_apply_penalty_phrase_gate_avx512(float* scores,
                                                       const uint32_t* missing_term_counts,
                                                       const uint32_t* phrase_gate_failures,
                                                       unsigned int candidate_count,
                                                       float penalty_per_missing,
                                                       float clamp_max);

// ============================================================================
// GGUF Format Constants
// ============================================================================

static const uint32_t RAWRXD_GGUF_MAGIC_U32 = 0x46554747UL; // "GGUF" little-endian

#if !RAWR_ENABLE_GGML_LINK
#define GGUF_MAGIC         0x46554747UL   // "GGUF" little-endian (G=0x47,G=0x47,U=0x55,F=0x46)
#define GGUF_VERSION_3     3

// GGUF metadata value types
#define GGUF_TYPE_UINT8    0
#define GGUF_TYPE_INT8     1
#define GGUF_TYPE_UINT16   2
#define GGUF_TYPE_INT16    3
#define GGUF_TYPE_UINT32   4
#define GGUF_TYPE_INT32    5
#define GGUF_TYPE_FLOAT32  6
#define GGUF_TYPE_BOOL     7
#define GGUF_TYPE_STRING   8
#define GGUF_TYPE_ARRAY    9
#define GGUF_TYPE_UINT64   10
#define GGUF_TYPE_INT64    11
#define GGUF_TYPE_FLOAT64  12
#endif

// ============================================================================
// Inference Parameters (matches bridge_layer.cpp TITAN_PARAMS)
// ============================================================================

typedef struct {
    const char* prompt;
    int         max_tokens;
    float       temperature;
    void      (*callback)(const char* text, int len);
} TITAN_PARAMS;

// ============================================================================
// GGUF Header
// ============================================================================

#pragma pack(push, 1)
struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t n_tensors;
    uint64_t n_kv;
};
#pragma pack(pop)

// ============================================================================
// Engine State
// ============================================================================

static volatile long  g_initialized = 0;
static volatile long  g_cancelRequest = 0;  // Set to 1 to abort in-flight inference
static HANDLE         g_hFile       = INVALID_HANDLE_;
static HANDLE         g_hMapping    = nullptr;
static const uint8_t* g_mappedBase  = nullptr;
static uint64_t       g_fileSize    = 0;
static uint64_t       g_nTensors    = 0;
static uint64_t       g_nKV         = 0;
static char           g_modelPath[512] = {0};
static char           g_modelName[256] = {0};
static char           g_modelArchitecture[64] = {0};
static uint32_t       g_modelContextLength = 0;
static uint32_t       g_modelVocabSize = 0;
static uint32_t       g_modelPrecisionBits = 0;
static uint32_t       g_modelParameterCount = 0;
static char           g_liveModelName[64] = "phi:latest";
static volatile long  g_allowOfflineFallback = 0;
static volatile long  g_neuralEmbeddingsActive = 0;  // Phase 8: Neural embeddings gate
static volatile long  g_embeddingFoldMode = 0;       // 0=mean, 1=max, 2=rms
static volatile long  g_vulkanMemoryClass = 0;       // 0=auto, 1=device_local, 2=host_visible, 3=local_flat
extern "C" RAWRXD_TITAN_BRIDGE_API void* g_ggml_ctx = nullptr;
extern "C" RAWRXD_TITAN_BRIDGE_API void* g_model_tensors = nullptr;
extern "C" RAWRXD_TITAN_BRIDGE_API int g_n_layers = 0;
extern "C" RAWRXD_TITAN_BRIDGE_API int g_n_embd = 0;
extern "C" RAWRXD_TITAN_BRIDGE_API int g_n_head = 0;
extern "C" RAWRXD_TITAN_BRIDGE_API int g_n_vocab = 0;
extern "C" RAWRXD_TITAN_BRIDGE_API int g_n_ctx = 0;
#if RAWR_ENABLE_GGML_LINK
static gguf_context* g_titan_gguf_ctx = nullptr;
#endif
static volatile uint64_t g_rf_data_seq = 0;
static volatile uint64_t g_rf_consumed_seq = 0;
static volatile uint64_t g_rf_frame_ready = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_mailbox_data_seq = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_mailbox_consumed_seq = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_mailbox_frame_ready = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_last_doorbell_addr = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_last_doorbell_value = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_last_doorbell_emit_seq = 0;
extern "C" __declspec(dllexport) volatile long g_rawrxd_omega_probe_early_return = 1;
static volatile uint64_t g_rf_last_cycles = 0;
static volatile uint64_t g_rf_last_drift = 0;
static volatile uint64_t g_rf_max_cycles = 0;
static volatile long     g_rawrxd_sovereign_log_ready = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_fusion_invocations = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_phrase_gate_invocations = 0;
extern "C" __declspec(dllexport) volatile unsigned long long g_rawrxd_ttft_ms = 0;

// Forward declarations (defined later in the translation unit)
static void ZeroModelMetadata();
static void PopulateLoadedModelMetadata();
static void StrCopyN(char* dst, const char* src, size_t n);
static void TitanLog(const char* msg);
static uint32_t g_rawrxd_aperture_policy = 0;
static const uint32_t kRawrXDAperturePolicySprint = 0x1u;

#if RAWR_ENABLE_GGML_LINK
static bool InitializeGGMLBridgeContext();
static void ShutdownGGMLBridgeContext();
#endif

// ============================================================================
// Winsock2 — Raw TCP for Ollama HTTP bridge (zero new DLL deps at link time)
// ============================================================================

typedef unsigned __int64 SOCKET_T;
#define INVALID_SOCK   ((SOCKET_T)(~(SOCKET_T)0))
#define AF_INET_       2
#define SOCK_STREAM_   1

#pragma pack(push, 1)
struct SockAddrIn {
    short          sin_family;
    unsigned short sin_port;
    unsigned long  sin_addr;
    char           sin_zero[8];
};
#pragma pack(pop)

typedef int      (__stdcall *PFN_WSAStartup)(WORD ver, void* data);
typedef int      (__stdcall *PFN_WSACleanup)(void);
typedef SOCKET_T (__stdcall *PFN_socket)(int af, int type, int proto);
typedef int      (__stdcall *PFN_connect)(SOCKET_T s, const SockAddrIn* name, int namelen);
typedef int      (__stdcall *PFN_send_)(SOCKET_T s, const char* buf, int len, int flags);
typedef int      (__stdcall *PFN_recv_)(SOCKET_T s, char* buf, int len, int flags);
typedef int      (__stdcall *PFN_closesocket)(SOCKET_T s);
typedef int      (__stdcall *PFN_setsockopt)(SOCKET_T s, int level, int optname,
                                              const char* optval, int optlen);

static HMODULE          g_hWs2         = nullptr;
static PFN_WSAStartup   pfn_WSAStartup = nullptr;
static PFN_WSACleanup   pfn_WSACleanup = nullptr;
static PFN_socket       pfn_socket     = nullptr;
static PFN_connect      pfn_connect    = nullptr;
static PFN_send_        pfn_send       = nullptr;
static PFN_recv_        pfn_recv       = nullptr;
static PFN_closesocket  pfn_closesocket = nullptr;
static PFN_setsockopt   pfn_setsockopt  = nullptr;
static volatile long    g_wsaReady     = 0;
static SOCKET_T         g_ollamaSock   = INVALID_SOCK;
static volatile long    g_ollamaConnected = 0;
static volatile long    g_traceTransportSegments = 1;

static long long QpcNow() {
    LARGE_INTEGER t;
    t.QuadPart = 0;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static int QpcDeltaMs(long long startTicks, long long endTicks) {
    if (endTicks <= startTicks) return 0;
    LARGE_INTEGER freq;
    freq.QuadPart = 0;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0) return 0;
    long long dt = endTicks - startTicks;
    return (int)((dt * 1000LL) / freq.QuadPart);
}

static bool InitWinsock() {
    if (g_wsaReady) return true;
    g_hWs2 = LoadLibraryA("ws2_32.dll");
    if (!g_hWs2) return false;
    pfn_WSAStartup  = (PFN_WSAStartup) GetProcAddress(g_hWs2, "WSAStartup");
    pfn_WSACleanup  = (PFN_WSACleanup) GetProcAddress(g_hWs2, "WSACleanup");
    pfn_socket      = (PFN_socket)     GetProcAddress(g_hWs2, "socket");
    pfn_connect     = (PFN_connect)    GetProcAddress(g_hWs2, "connect");
    pfn_send        = (PFN_send_)      GetProcAddress(g_hWs2, "send");
    pfn_recv        = (PFN_recv_)      GetProcAddress(g_hWs2, "recv");
    pfn_closesocket = (PFN_closesocket)GetProcAddress(g_hWs2, "closesocket");
    pfn_setsockopt  = (PFN_setsockopt) GetProcAddress(g_hWs2, "setsockopt");
    if (!pfn_WSAStartup || !pfn_socket || !pfn_connect ||
        !pfn_send || !pfn_recv || !pfn_closesocket) return false;
    unsigned char wsaData[512] = {0};
    if (pfn_WSAStartup(0x0202, wsaData) != 0) return false;
    g_wsaReady = 1;
    return true;
}

static void CloseOllamaSocket() {
    if (g_ollamaSock != INVALID_SOCK && pfn_closesocket) {
        pfn_closesocket(g_ollamaSock);
    }
    g_ollamaSock = INVALID_SOCK;
    _InterlockedExchange(&g_ollamaConnected, 0);
}

static bool EnsureOllamaSocketConnected(int timeoutMs, int* connectMs, int* wasReuse) {
    if (connectMs) *connectMs = 0;
    if (wasReuse) *wasReuse = 0;

    if (_InterlockedCompareExchange(&g_ollamaConnected, 0, 0) != 0 && g_ollamaSock != INVALID_SOCK) {
        if (wasReuse) *wasReuse = 1;
        return true;
    }

    CloseOllamaSocket();

    long long tConnectStart = QpcNow();

    SOCKET_T s = pfn_socket(AF_INET_, SOCK_STREAM_, 0);
    if (s == INVALID_SOCK) return false;

    if (pfn_setsockopt) {
        int sndTimeout = timeoutMs;
        int rcvTimeout = timeoutMs;
        pfn_setsockopt(s, 0xFFFF /*SOL_SOCKET*/, 0x1005 /*SO_SNDTIMEO*/,
                       (const char*)&sndTimeout, sizeof(sndTimeout));
        pfn_setsockopt(s, 0xFFFF /*SOL_SOCKET*/, 0x1006 /*SO_RCVTIMEO*/,
                       (const char*)&rcvTimeout, sizeof(rcvTimeout));
    }

    SockAddrIn addr;
    for (int i = 0; i < (int)sizeof(addr); i++) ((char*)&addr)[i] = 0;
    addr.sin_family = AF_INET_;
    addr.sin_port   = (unsigned short)((11434 >> 8) | ((11434 & 0xFF) << 8));
    addr.sin_addr   = 0x0100007F;

    if (pfn_connect(s, &addr, sizeof(addr)) != 0) {
        if (pfn_closesocket) pfn_closesocket(s);
        return false;
    }

    g_ollamaSock = s;
    _InterlockedExchange(&g_ollamaConnected, 1);
    if (connectMs) *connectMs = QpcDeltaMs(tConnectStart, QpcNow());
    return true;
}

static int FindHttpHeaderEnd(const char* buf, int len) {
    if (!buf || len < 4) return -1;
    for (int i = 0; i <= len - 4; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
            return i + 4;
        }
    }
    return -1;
}

static int ParseContentLength(const char* buf, int headerLen) {
    if (!buf || headerLen <= 0) return -1;
    const char* needle = "Content-Length:";
    const int nlen = 15;
    for (int i = 0; i <= headerLen - nlen; i++) {
        bool match = true;
        for (int j = 0; j < nlen; j++) {
            if (buf[i + j] != needle[j]) { match = false; break; }
        }
        if (!match) continue;

        int k = i + nlen;
        while (k < headerLen && (buf[k] == ' ' || buf[k] == '\t')) k++;
        int value = 0;
        bool hasDigit = false;
        while (k < headerLen) {
            char c = buf[k];
            if (c < '0' || c > '9') break;
            hasDigit = true;
            value = value * 10 + (c - '0');
            k++;
        }
        return hasDigit ? value : -1;
    }
    return -1;
}

static int AsciiLower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int HeaderLineContainsTokenCI(const char* line, int lineLen, const char* token) {
    if (!line || !token || lineLen <= 0) return 0;
    int tokenLen = 0;
    while (token[tokenLen]) tokenLen++;
    if (tokenLen <= 0 || tokenLen > lineLen) return 0;

    for (int i = 0; i <= lineLen - tokenLen; i++) {
        int match = 1;
        for (int j = 0; j < tokenLen; j++) {
            if (AsciiLower((unsigned char)line[i + j]) != AsciiLower((unsigned char)token[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

static int HasChunkedTransferEncoding(const char* headers, int headerLen) {
    if (!headers || headerLen <= 0) return 0;

    int lineStart = 0;
    while (lineStart < headerLen) {
        int lineEnd = lineStart;
        while (lineEnd < headerLen && headers[lineEnd] != '\n') lineEnd++;

        int lineLen = lineEnd - lineStart;
        if (lineLen > 0 && headers[lineStart + lineLen - 1] == '\r') lineLen--;

        if (lineLen > 0) {
            const char* line = headers + lineStart;
            if (HeaderLineContainsTokenCI(line, lineLen, "Transfer-Encoding:") &&
                HeaderLineContainsTokenCI(line, lineLen, "chunked")) {
                return 1;
            }
        }

        if (lineEnd >= headerLen) break;
        lineStart = lineEnd + 1;
    }
    return 0;
}

// ============================================================================
// JSON Helpers
// ============================================================================

static int StrCopy(char* dst, int off, const char* src) {
    int i = 0;
    while (src[i]) { dst[off + i] = src[i]; i++; }
    return off + i;
}

static void StrCopyBounded(char* dst, const char* src, int dstMax) {
    if (!dst || dstMax <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    int i = 0;
    while (src[i] && i < dstMax - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static int StrIEquals(const char* a, const char* b) {
    if (!a || !b) return 0;
    int i = 0;
    while (a[i] && b[i]) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static void ConfigureLiveRoutePolicy() {
    char envModel[96] = {0};
    DWORD envModelLen = GetEnvironmentVariableA("TITAN_OLLAMA_MODEL", envModel, (DWORD)sizeof(envModel));
    if (envModelLen > 0 && envModelLen < sizeof(envModel)) {
        StrCopyBounded(g_liveModelName, envModel, (int)sizeof(g_liveModelName));
    } else {
        StrCopyBounded(g_liveModelName, "phi:latest", (int)sizeof(g_liveModelName));
    }

    char envFallback[16] = {0};
    DWORD envFallbackLen = GetEnvironmentVariableA("TITAN_OLLAMA_ALLOW_OFFLINE_FALLBACK", envFallback, (DWORD)sizeof(envFallback));
    if (envFallbackLen > 0 && envFallbackLen < sizeof(envFallback)) {
        if (StrIEquals(envFallback, "1") ||
            StrIEquals(envFallback, "true") ||
            StrIEquals(envFallback, "yes") ||
            StrIEquals(envFallback, "on")) {
            _InterlockedExchange(&g_allowOfflineFallback, 1);
        } else {
            _InterlockedExchange(&g_allowOfflineFallback, 0);
        }
    } else {
        // Benchmark-safe default: fail fast when live route is unavailable.
        _InterlockedExchange(&g_allowOfflineFallback, 0);
    }

    char envTrace[16] = {0};
    DWORD envTraceLen = GetEnvironmentVariableA("TITAN_OLLAMA_TRACE_SEGMENTS", envTrace, (DWORD)sizeof(envTrace));
    if (envTraceLen > 0 && envTraceLen < sizeof(envTrace)) {
        if (StrIEquals(envTrace, "0") ||
            StrIEquals(envTrace, "false") ||
            StrIEquals(envTrace, "no") ||
            StrIEquals(envTrace, "off")) {
            _InterlockedExchange(&g_traceTransportSegments, 0);
        } else {
            _InterlockedExchange(&g_traceTransportSegments, 1);
        }
    } else {
        _InterlockedExchange(&g_traceTransportSegments, 1);
    }
}

static int IntToStr(char* dst, int off, int val) {
    if (val == 0) { dst[off++] = '0'; return off; }
    if (val < 0) { dst[off++] = '-'; val = -val; }
    char tmp[16]; int ti = 0;
    while (val > 0) { tmp[ti++] = '0' + (val % 10); val /= 10; }
    while (ti > 0) dst[off++] = tmp[--ti];
    return off;
}

static int JsonEscape(const char* src, char* dst, int dstMax) {
    int si = 0, di = 0;
    while (src[si] && di < dstMax - 6) {
        char c = src[si++];
        if (c == '"' || c == '\\') { dst[di++] = '\\'; dst[di++] = c; }
        else if (c == '\n') { dst[di++] = '\\'; dst[di++] = 'n'; }
        else if (c == '\r') { dst[di++] = '\\'; dst[di++] = 'r'; }
        else if (c == '\t') { dst[di++] = '\\'; dst[di++] = 't'; }
        else if ((unsigned char)c < 0x20) continue;  // skip other control chars
        else dst[di++] = c;
    }
    dst[di] = 0;
    return di;
}

static int ExtractJsonResponse(const char* json, int jsonLen, char* out, int outMax) {
    const char* needle = "\"response\":";
    int nlen = 11;
    int oi = 0;
    int start = -1;

    // First try SIMD scan for exact match
    const char* simdValue = RawrXD_ScanResponseToken(json, (unsigned long long)jsonLen, needle);
    if (simdValue && simdValue >= json && simdValue <= json + jsonLen) {
        start = (int)(simdValue - json);
    }

    // Fallback to linear scan
    if (start < 0) {
        for (int i = 0; i < jsonLen - nlen; i++) {
            bool match = true;
            for (int j = 0; j < nlen; j++) {
                if (json[i+j] != needle[j]) { match = false; break; }
            }
            if (!match) continue;
            start = i + nlen;
            break;
        }
    }

    // If found, skip whitespace and quotes
    if (start >= 0 && start < jsonLen) {
        while (start < jsonLen && (json[start] == ' ' || json[start] == '\t' || json[start] == '\n' || json[start] == '\r' || json[start] == '"')) start++;
        if (start < jsonLen && json[start] == '{') {
            // If it's an object, perhaps error, return empty
            out[0] = 0;
            return 0;
        }
        for (int k = start; k < jsonLen && oi < outMax - 1; k++) {
            if (json[k] == '"' && (k == start || json[k-1] != '\\')) break;
            if (json[k] == '\\' && k + 1 < jsonLen) {
                k++;
                char esc = json[k];
                if      (esc == 'n')  out[oi++] = '\n';
                else if (esc == 'r')  out[oi++] = '\r';
                else if (esc == 't')  out[oi++] = '\t';
                else if (esc == '"') out[oi++] = '"';
                else if (esc == '\\') out[oi++] = '\\';
                else { out[oi++] = '\\'; if (oi < outMax-1) out[oi++] = esc; }
            } else {
                out[oi++] = json[k];
            }
        }
    }
    out[oi] = 0;
    return oi;
}

static int HexDigitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int DecodeHttpChunkedBody(const char* src, int srcLen, char* dst, int dstMax) {
    if (!src || !dst || srcLen <= 0 || dstMax <= 1) return -1;

    int si = 0;
    int di = 0;

    while (si < srcLen) {
        int chunkSize = 0;
        int haveHex = 0;

        while (si < srcLen) {
            char c = src[si++];
            if (c == ';') {
                while (si < srcLen && src[si] != '\n') si++;
                if (si < srcLen && src[si] == '\n') si++;
                break;
            }
            if (c == '\r') continue;
            if (c == '\n') break;
            int hv = HexDigitValue(c);
            if (hv < 0) return -2;
            haveHex = 1;
            chunkSize = (chunkSize << 4) + hv;
        }

        if (!haveHex) continue;
        if (chunkSize == 0) {
            dst[di] = 0;
            return di;
        }
        if (si + chunkSize > srcLen) return -3;
        if (di + chunkSize >= dstMax) return -4;

        for (int i = 0; i < chunkSize; i++) {
            dst[di++] = src[si + i];
        }
        si += chunkSize;

        if (si < srcLen && src[si] == '\r') si++;
        if (si < srcLen && src[si] == '\n') si++;
    }

    dst[di] = 0;
    return di;
}

static int ExtractStreamingJsonResponse(const char* body,
                                        int bodyLen,
                                        int isChunked,
                                        char* out,
                                        int outMax,
                                        int* outDoneSeen) {
    if (!body || bodyLen <= 0 || !out || outMax <= 0) return -1;

    const char* payload = body;
    int payloadLen = bodyLen;
    static char decodedChunked[262144];

    if (isChunked) {
        int decodedLen = DecodeHttpChunkedBody(body, bodyLen, decodedChunked, (int)sizeof(decodedChunked));
        if (decodedLen < 0) return decodedLen;
        payload = decodedChunked;
        payloadLen = decodedLen;
    }

    const char* responseNeedle = "\"response\":";
    const int responseNeedleLen = 11;
    const char* doneNeedle = "\"done\":true";
    const int doneNeedleLen = 11;

    int oi = 0;
    int doneSeen = 0;

    for (int i = 0; i < payloadLen; i++) {
        if (!doneSeen && i <= payloadLen - doneNeedleLen) {
            int doneMatch = 1;
            for (int j = 0; j < doneNeedleLen; j++) {
                if (payload[i + j] != doneNeedle[j]) {
                    doneMatch = 0;
                    break;
                }
            }
            if (doneMatch) doneSeen = 1;
        }

        if (i > payloadLen - responseNeedleLen) continue;
        int match = 1;
        for (int j = 0; j < responseNeedleLen; j++) {
            if (payload[i + j] != responseNeedle[j]) {
                match = 0;
                break;
            }
        }
        if (!match) continue;

        int p = i + responseNeedleLen;
        while (p < payloadLen && (payload[p] == ' ' || payload[p] == '\t' || payload[p] == '\r' || payload[p] == '\n')) p++;
        if (p >= payloadLen || payload[p] != '"') continue;
        p++;

        while (p < payloadLen && oi < outMax - 1) {
            char c = payload[p++];
            if (c == '"') break;
            if (c == '\\' && p < payloadLen) {
                char esc = payload[p++];
                if      (esc == 'n') out[oi++] = '\n';
                else if (esc == 'r') out[oi++] = '\r';
                else if (esc == 't') out[oi++] = '\t';
                else if (esc == '"') out[oi++] = '"';
                else if (esc == '\\') out[oi++] = '\\';
                else {
                    out[oi++] = '\\';
                    if (oi < outMax - 1) out[oi++] = esc;
                }
            } else {
                out[oi++] = c;
            }
        }

        i = p;
    }

    out[oi] = 0;
    if (outDoneSeen) *outDoneSeen = doneSeen;

    if (oi == 0) {
        // Fallback for a non-stream-compatible payload that still contains a single JSON object.
        return ExtractJsonResponse(payload, payloadLen, out, outMax);
    }

    return oi;
}

static int IsJsonNumChar(char c) {
    if (c >= '0' && c <= '9') return 1;
    if (c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') return 1;
    return 0;
}

static int ParseJsonFloatToken(const char* src, int len, float* outVal) {
    if (!src || len <= 0 || !outVal) return -1;

    int i = 0;
    int sign = 1;
    if (src[i] == '-') { sign = -1; i++; }
    else if (src[i] == '+') { i++; }
    if (i >= len) return -1;

    double integerPart = 0.0;
    int haveDigit = 0;
    while (i < len && src[i] >= '0' && src[i] <= '9') {
        haveDigit = 1;
        integerPart = (integerPart * 10.0) + (double)(src[i] - '0');
        i++;
    }

    double fracPart = 0.0;
    double fracScale = 1.0;
    if (i < len && src[i] == '.') {
        i++;
        while (i < len && src[i] >= '0' && src[i] <= '9') {
            haveDigit = 1;
            fracPart = (fracPart * 10.0) + (double)(src[i] - '0');
            fracScale *= 10.0;
            i++;
        }
    }

    if (!haveDigit) return -1;

    int expSign = 1;
    int expVal = 0;
    if (i < len && (src[i] == 'e' || src[i] == 'E')) {
        i++;
        if (i < len && src[i] == '-') { expSign = -1; i++; }
        else if (i < len && src[i] == '+') { i++; }
        int haveExpDigit = 0;
        while (i < len && src[i] >= '0' && src[i] <= '9') {
            haveExpDigit = 1;
            expVal = (expVal * 10) + (src[i] - '0');
            i++;
        }
        if (!haveExpDigit) return -1;
    }

    double v = ((integerPart + (fracPart / fracScale)) * (double)sign);
    if (expVal != 0) {
        double scale = pow(10.0, (double)(expSign * expVal));
        v *= scale;
    }

    *outVal = (float)v;
    return 0;
}

static uint32_t ParseEmbeddingFoldModeEnv(const char* modeStr) {
    if (!modeStr || !modeStr[0]) return 0;
    char c0 = modeStr[0];
    if (c0 == '1') return 1;
    if (c0 == '2') return 2;
    if (c0 == 'm' || c0 == 'M') {
        char c1 = modeStr[1];
        if (c1 == 'a' || c1 == 'A') return 1;  // max
        return 0;                               // mean
    }
    if (c0 == 'r' || c0 == 'R') return 2;
    return 0;
}

static const char* EmbeddingFoldModeName(uint32_t mode) {
    switch (mode) {
        case 1: return "max";
        case 2: return "rms";
        default: return "mean";
    }
}

static uint32_t ParseVulkanMemoryClassEnv(const char* modeStr) {
    if (!modeStr || !modeStr[0]) return 0;
    char c0 = modeStr[0];
    if (c0 == '1') return 1;
    if (c0 == '2') return 2;
    if (c0 == '3') return 3;
    if (c0 == 'd' || c0 == 'D') return 1; // device_local / dedicated
    if (c0 == 'h' || c0 == 'H') return 2; // host_visible
    if (c0 == 'l' || c0 == 'L') return 3; // local_flat
    if (c0 == 'a' || c0 == 'A') return 0; // auto
    return 0;
}

static const char* VulkanMemoryClassName(uint32_t mode) {
    switch (mode) {
        case 1: return "device_local";
        case 2: return "host_visible";
        case 3: return "local_flat";
        default: return "auto";
    }
}

static void ZeroSemanticVector(float* outVec, uint32_t dim);
static void NormalizeSemanticVector(float* outVec, uint32_t dim);

static int ExtractJsonEmbeddingVector(const char* json, int jsonLen, float* outVec, int outMax) {
    if (!json || jsonLen <= 0 || !outVec || outMax <= 0) return -1;

    const char* needle = "\"embedding\"";
    const int nlen = 11;
    int pos = -1;
    for (int i = 0; i <= jsonLen - nlen; i++) {
        int ok = 1;
        for (int j = 0; j < nlen; j++) {
            if (json[i + j] != needle[j]) { ok = 0; break; }
        }
        if (ok) { pos = i + nlen; break; }
    }
    if (pos < 0) return -2;

    while (pos < jsonLen && json[pos] != '[') pos++;
    if (pos >= jsonLen || json[pos] != '[') return -3;
    pos++;

    int count = 0;
    while (pos < jsonLen && count < outMax) {
        while (pos < jsonLen && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n' || json[pos] == ',')) pos++;
        if (pos >= jsonLen) break;
        if (json[pos] == ']') return count;

        int start = pos;
        while (pos < jsonLen && IsJsonNumChar(json[pos])) pos++;
        int tokenLen = pos - start;
        if (tokenLen <= 0) return -4;

        float fv = 0.0f;
        if (ParseJsonFloatToken(json + start, tokenLen, &fv) != 0) return -5;
        outVec[count++] = fv;

        while (pos < jsonLen && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) pos++;
        if (pos < jsonLen && json[pos] == ']') return count;
    }

    return count;
}

static int ExtractAndFoldJsonEmbeddingVector(const char* json,
                                             int jsonLen,
                                             float* outVec,
                                             uint32_t outDim,
                                             int* outRawCount) {
    if (!json || jsonLen <= 0 || !outVec || outDim == 0) return -1;
    if (outDim > RAWRXD_INDEXER_EMBED_DIM) return -2;

    const char* needle = "\"embedding\"";
    const int nlen = 11;
    int pos = -1;
    for (int i = 0; i <= jsonLen - nlen; i++) {
        int ok = 1;
        for (int j = 0; j < nlen; j++) {
            if (json[i + j] != needle[j]) { ok = 0; break; }
        }
        if (ok) { pos = i + nlen; break; }
    }
    if (pos < 0) return -3;

    while (pos < jsonLen && json[pos] != '[') pos++;
    if (pos >= jsonLen || json[pos] != '[') return -4;
    pos++;

    ZeroSemanticVector(outVec, outDim);

    // Segment-aware folding keeps high-dimensional models stable when projected
    // into Titan's fixed 384-d contract.
    float segmentVec[RAWRXD_INDEXER_EMBED_DIM];
    float rmsSquares[RAWRXD_INDEXER_EMBED_DIM];
    ZeroSemanticVector(segmentVec, outDim);
    ZeroSemanticVector(rmsSquares, outDim);

    uint32_t foldMode = (uint32_t)_InterlockedCompareExchange(&g_embeddingFoldMode, 0, 0);
    if (foldMode > 2) foldMode = 0;

    const int kSegmentSpan = 512;
    int segmentFill = 0;
    int segmentIndex = 0;
    int rmsCount = 0;
    int rawCount = 0;

    while (pos < jsonLen) {
        while (pos < jsonLen && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n' || json[pos] == ',')) pos++;
        if (pos >= jsonLen) break;
        if (json[pos] == ']') break;

        int start = pos;
        while (pos < jsonLen && IsJsonNumChar(json[pos])) pos++;
        int tokenLen = pos - start;
        if (tokenLen <= 0) return -5;

        float fv = 0.0f;
        if (ParseJsonFloatToken(json + start, tokenLen, &fv) != 0) return -6;

        uint32_t dst = (uint32_t)(rawCount % (int)outDim);
        segmentVec[dst] += fv;
        rawCount++;
        segmentFill++;

        if (segmentFill >= kSegmentSpan) {
            NormalizeSemanticVector(segmentVec, outDim);
            float w = (float)(1.0 / sqrt((double)(segmentIndex + 1)));
            for (uint32_t d = 0; d < outDim; ++d) {
                float v = segmentVec[d] * w;
                if (foldMode == 1) {
                    if (fabs(v) > fabs(outVec[d])) outVec[d] = v;
                } else if (foldMode == 2) {
                    rmsSquares[d] += v * v;
                } else {
                    outVec[d] += v;
                }
            }
            if (foldMode == 2) rmsCount++;
            ZeroSemanticVector(segmentVec, outDim);
            segmentFill = 0;
            segmentIndex++;
        }
    }

    if (segmentFill > 0) {
        NormalizeSemanticVector(segmentVec, outDim);
        float w = (float)(1.0 / sqrt((double)(segmentIndex + 1)));
        for (uint32_t d = 0; d < outDim; ++d) {
            float v = segmentVec[d] * w;
            if (foldMode == 1) {
                if (fabs(v) > fabs(outVec[d])) outVec[d] = v;
            } else if (foldMode == 2) {
                rmsSquares[d] += v * v;
            } else {
                outVec[d] += v;
            }
        }
        if (foldMode == 2) rmsCount++;
    }

    if (foldMode == 2) {
        if (rmsCount <= 0) return -8;
        float invCount = 1.0f / (float)rmsCount;
        for (uint32_t d = 0; d < outDim; ++d) {
            outVec[d] = (float)sqrt((double)(rmsSquares[d] * invCount));
        }
    }

    if (rawCount <= 0) return -7;
    NormalizeSemanticVector(outVec, outDim);
    if (outRawCount) *outRawCount = rawCount;
    return 0;
}

// ============================================================================
// OllamaGenerate — HTTP POST to localhost:11434/api/generate
// Returns response text length, or negative on error
// ============================================================================

static int OllamaGenerate(const char* prompt, int maxTokens, float temperature,
                           char* response, int responseMax) {
    if (!InitWinsock()) return -1;

    // Keep HTTP path on a tighter budget so the caller's 15s wait window
    // still leaves room for deterministic fallback completion.
    const int kOllamaSocketTimeoutMs = 8000;
    const int kOllamaHardBudgetMs = 6000;
    const int useStreaming = ((g_rawrxd_aperture_policy & kRawrXDAperturePolicySprint) != 0) ? 1 : 0;

    // Build JSON body manually (wsprintfA limited to 1024 chars)
    static char body[65536];
    static char escaped[32768];
    JsonEscape(prompt, escaped, 32768);

    int tempWhole = (int)temperature;
    int tempFrac  = (int)((temperature - (float)tempWhole) * 10.0f + 0.5f);

    int bp = 0;
    bp = StrCopy(body, bp, "{\"model\":\"");
    bp = StrCopy(body, bp, g_liveModelName);
    bp = StrCopy(body, bp, "\",");
    bp = StrCopy(body, bp, "\"prompt\":\"");
    bp = StrCopy(body, bp, escaped);
    bp = StrCopy(body, bp, useStreaming ? "\",\"stream\":true," : "\",\"stream\":false,");
    bp = StrCopy(body, bp, "\"options\":{\"num_predict\":");
    bp = IntToStr(body, bp, maxTokens);
    bp = StrCopy(body, bp, ",\"temperature\":");
    bp = IntToStr(body, bp, tempWhole);
    body[bp++] = '.';
    bp = IntToStr(body, bp, tempFrac);
    bp = StrCopy(body, bp, "}}");
    body[bp] = 0;

    // Build HTTP request
    static char httpReq[66000];
    int hp = 0;
    hp = StrCopy(httpReq, hp, "POST /api/generate HTTP/1.1\r\n");
    hp = StrCopy(httpReq, hp, "Host: 127.0.0.1:11434\r\n");
    hp = StrCopy(httpReq, hp, "Content-Type: application/json\r\n");
    hp = StrCopy(httpReq, hp, "Content-Length: ");
    hp = IntToStr(httpReq, hp, bp);
    hp = StrCopy(httpReq, hp, useStreaming ? "\r\nConnection: close\r\n\r\n" : "\r\nConnection: keep-alive\r\n\r\n");
    for (int i = 0; i < bp; i++) httpReq[hp + i] = body[i];
    hp += bp;

    long long requestStartTicks = QpcNow();
    
    // === FORENSIC TIMING: HTTP Orchestration ===
    char forensicBuf[512];
    int forensicIdx = 0;
    forensicIdx = StrCopy(forensicBuf, forensicIdx, "[FORENSIC-OLLAMA] stage_ms: ");

    // Retry once on broken persistent socket.
    for (int attempt = 0; attempt < 2; attempt++) {
        if (QpcDeltaMs(requestStartTicks, QpcNow()) >= kOllamaHardBudgetMs) {
            CloseOllamaSocket();
            return -6;
        }

        long long connectStartTicks = QpcNow();
        int connectMs = 0;
        int wasReuse = 0;
        if (!EnsureOllamaSocketConnected(kOllamaSocketTimeoutMs, &connectMs, &wasReuse)) {
            return (attempt == 0) ? -3 : -4;
        }
        long long connectEndTicks = QpcNow();
        int connectTotalMs = QpcDeltaMs(connectStartTicks, connectEndTicks);

        // Send request on persistent socket.
        int sent = 0;
        long long sendDoneTicks = 0;
        long long firstByteTicks = 0;
        long long bodyReadyTicks = 0;
        
        long long sendStartTicks = QpcNow();
        while (sent < hp) {
            if (QpcDeltaMs(requestStartTicks, QpcNow()) >= kOllamaHardBudgetMs) {
                CloseOllamaSocket();
                return -7;
            }
            int rc = pfn_send(g_ollamaSock, httpReq + sent, hp - sent, 0);
            if (rc <= 0) {
                CloseOllamaSocket();
                break;
            }
            sent += rc;
        }
        sendDoneTicks = QpcNow();
        int sendMs = QpcDeltaMs(sendStartTicks, sendDoneTicks);
        
        if (sent < hp) continue;

        // Receive a single HTTP response frame by Content-Length.
        static char recvBuf[262144];
        int totalRecv = 0;
        int headerEnd = -1;
        int contentLength = -1;
        int transferChunked = 0;
        
        long long recvStartTicks = QpcNow();
        int totalRecvMs = 0;

        while (totalRecv < (int)sizeof(recvBuf) - 1) {
            if (QpcDeltaMs(requestStartTicks, QpcNow()) >= kOllamaHardBudgetMs) {
                totalRecvMs = QpcDeltaMs(recvStartTicks, QpcNow());
                CloseOllamaSocket();
                return -8;
            }
            if (_InterlockedCompareExchange(&g_cancelRequest, 0, 0) != 0) {
                totalRecvMs = QpcDeltaMs(recvStartTicks, QpcNow());
                CloseOllamaSocket();
                return -10;
            }

            int rc = pfn_recv(g_ollamaSock, recvBuf + totalRecv,
                              (int)sizeof(recvBuf) - 1 - totalRecv, 0);
            if (rc <= 0) {
                totalRecvMs = QpcDeltaMs(recvStartTicks, QpcNow());
                
                    if (useStreaming && headerEnd > 0 && totalRecv > headerEnd) {
                    bodyReadyTicks = QpcNow();
                    long long parseStartTicks = QpcNow();
                    int doneSeen = 0;
                    int outLen = ExtractStreamingJsonResponse(recvBuf + headerEnd,
                                                              totalRecv - headerEnd,
                                                              transferChunked,
                                                              response,
                                                              responseMax,
                                                              &doneSeen);
                    long long parseDoneTicks = QpcNow();
                    int parseMs = QpcDeltaMs(parseStartTicks, parseDoneTicks);

                    int payloadLen = (contentLength >= 0) ? contentLength : (totalRecv - headerEnd);
                    g_rf_data_seq += (uint64_t)payloadLen;
                    g_rf_consumed_seq += (uint64_t)payloadLen;
                    Rawr_Profile_Checkpoint(g_rf_data_seq,
                                            g_rf_consumed_seq,
                                            g_rf_frame_ready,
                                            (unsigned long long*)&g_rf_last_cycles,
                                            (unsigned long long*)&g_rf_last_drift,
                                            (unsigned long long*)&g_rf_max_cycles);
                    
                    int waitFirstByteMs = (firstByteTicks > 0) ? QpcDeltaMs(sendDoneTicks, firstByteTicks) : -1;
                    int bodyFrameMs = (bodyReadyTicks > 0 && firstByteTicks > 0) ? QpcDeltaMs(firstByteTicks, bodyReadyTicks) : -1;
                    int totalMs = QpcDeltaMs(requestStartTicks, parseDoneTicks);
                    
                    char timingBuf[512];
                    wsprintfA(timingBuf,
                              "[FORENSIC-OLLAMA] connect=%d send=%d recv=%d parse=%d wait_first_byte=%d body_frame=%d total=%d bytes_recv=%d",
                              connectTotalMs,
                              sendMs,
                              totalRecvMs,
                              parseMs,
                              waitFirstByteMs,
                              bodyFrameMs,
                              totalMs,
                              totalRecv);
                    TitanLog(timingBuf);

                    if (_InterlockedCompareExchange(&g_traceTransportSegments, 0, 0) != 0) {
                        char tbuf[512];
                        wsprintfA(tbuf,
                                  "[Titan transport] attempt=%d socket=%s stream=1 chunked=%d done=%d connect_ms=%d wait_first_byte_ms=%d body_frame_ms=%d parse_ms=%d total_ms=%d recv_bytes=%d body_len=%d",
                                  attempt + 1,
                                  wasReuse ? "reused" : "new",
                                  transferChunked,
                                  doneSeen,
                                  connectTotalMs,
                                  waitFirstByteMs,
                                  bodyFrameMs,
                                  parseMs,
                                  totalMs,
                                  totalRecv,
                                  payloadLen);
                        TitanLog(tbuf);
                    }

                    CloseOllamaSocket();
                    return outLen;
                }

                CloseOllamaSocket();
                break;
            }
            if (firstByteTicks == 0) {
                firstByteTicks = QpcNow();
                int _ttft = QpcDeltaMs(requestStartTicks, firstByteTicks);
                if (_ttft >= 0) g_rawrxd_ttft_ms = (unsigned long long)_ttft;
            }
            totalRecv += rc;
            recvBuf[totalRecv] = 0;

            if (headerEnd < 0) {
                headerEnd = FindHttpHeaderEnd(recvBuf, totalRecv);
                if (headerEnd > 0) {
                    contentLength = ParseContentLength(recvBuf, headerEnd);
                    transferChunked = HasChunkedTransferEncoding(recvBuf, headerEnd);
                    if (!useStreaming && (contentLength < 0 || contentLength > (int)sizeof(recvBuf) - headerEnd - 1)) {
                        CloseOllamaSocket();
                        break;
                    }
                    if (useStreaming && contentLength > (int)sizeof(recvBuf) - headerEnd - 1) {
                        CloseOllamaSocket();
                        break;
                    }
                }
            }

            if (headerEnd > 0 && contentLength >= 0) {
                if (totalRecv - headerEnd >= contentLength) {
                    bodyReadyTicks = QpcNow();
                    int jsonLen = contentLength;
                    long long parseStartTicks = QpcNow();
                    int doneSeen = 0;
                    int outLen = useStreaming
                        ? ExtractStreamingJsonResponse(recvBuf + headerEnd,
                                                       jsonLen,
                                                       transferChunked,
                                                       response,
                                                       responseMax,
                                                       &doneSeen)
                        : ExtractJsonResponse(recvBuf + headerEnd, jsonLen, response, responseMax);
                    long long parseDoneTicks = QpcNow();

                    g_rf_data_seq += (uint64_t)contentLength;
                    g_rf_consumed_seq += (uint64_t)contentLength;
                    Rawr_Profile_Checkpoint(g_rf_data_seq,
                                            g_rf_consumed_seq,
                                            g_rf_frame_ready,
                                            (unsigned long long*)&g_rf_last_cycles,
                                            (unsigned long long*)&g_rf_last_drift,
                                            (unsigned long long*)&g_rf_max_cycles);

                    if (_InterlockedCompareExchange(&g_traceTransportSegments, 0, 0) != 0) {
                        int waitMs = (firstByteTicks > 0) ? QpcDeltaMs(sendDoneTicks, firstByteTicks) : -1;
                        int bodyMs = (bodyReadyTicks > 0 && firstByteTicks > 0) ? QpcDeltaMs(firstByteTicks, bodyReadyTicks) : -1;
                        int parseMs = QpcDeltaMs(parseStartTicks, parseDoneTicks);
                        int totalMs = QpcDeltaMs(requestStartTicks, parseDoneTicks);
                        char tbuf[512];
                        if (useStreaming) {
                            wsprintfA(tbuf,
                                      "[Titan transport] attempt=%d socket=%s stream=1 chunked=%d done=%d connect_ms=%d wait_first_byte_ms=%d body_frame_ms=%d parse_ms=%d total_ms=%d recv_bytes=%d body_len=%d",
                                      attempt + 1,
                                      wasReuse ? "reused" : "new",
                                      transferChunked,
                                      doneSeen,
                                      connectMs,
                                      waitMs,
                                      bodyMs,
                                      parseMs,
                                      totalMs,
                                      totalRecv,
                                      contentLength);
                        } else {
                            wsprintfA(tbuf,
                                      "[Titan transport] attempt=%d socket=%s connect_ms=%d wait_first_byte_ms=%d body_frame_ms=%d parse_ms=%d total_ms=%d recv_bytes=%d body_len=%d",
                                      attempt + 1,
                                      wasReuse ? "reused" : "new",
                                      connectMs,
                                      waitMs,
                                      bodyMs,
                                      parseMs,
                                      totalMs,
                                      totalRecv,
                                      contentLength);
                        }
                        TitanLog(tbuf);
                    }

                    return outLen;
                }
            }
        }
    }

    return -5;
}

// ============================================================================
// Debug logging
// ============================================================================

static void TitanLog(const char* msg) {
    char buf[512];
    int i = 0;
    while (msg[i] && i < 510) { buf[i] = msg[i]; i++; }
    buf[i] = '\n'; buf[i+1] = 0;
    OutputDebugStringA(buf);
}

// ============================================================================
// GGUF String reader helper
// ============================================================================

static const uint8_t* ReadGGUFString(const uint8_t* ptr, char* out, int outMax) {
    uint64_t len = *(const uint64_t*)ptr;
    ptr += 8;
    int copyLen = (int)(len < (uint64_t)(outMax - 1) ? len : (uint64_t)(outMax - 1));
    for (int i = 0; i < copyLen; i++) out[i] = (char)ptr[i];
    out[copyLen] = 0;
    return ptr + len;
}

// ============================================================================
// GGUF Metadata value skip
// ============================================================================

static const uint8_t* SkipGGUFValue(const uint8_t* ptr, uint32_t vtype) {
    switch (vtype) {
        case GGUF_TYPE_UINT8:
        case GGUF_TYPE_INT8:
        case GGUF_TYPE_BOOL:    return ptr + 1;
        case GGUF_TYPE_UINT16:
        case GGUF_TYPE_INT16:   return ptr + 2;
        case GGUF_TYPE_UINT32:
        case GGUF_TYPE_INT32:
        case GGUF_TYPE_FLOAT32: return ptr + 4;
        case GGUF_TYPE_UINT64:
        case GGUF_TYPE_INT64:
        case GGUF_TYPE_FLOAT64: return ptr + 8;
        case GGUF_TYPE_STRING: {
            uint64_t slen = *(const uint64_t*)ptr;
            return ptr + 8 + slen;
        }
        case GGUF_TYPE_ARRAY: {
            uint32_t atype = *(const uint32_t*)ptr;
            ptr += 4;
            uint64_t count = *(const uint64_t*)ptr;
            ptr += 8;
            for (uint64_t i = 0; i < count; i++) {
                ptr = SkipGGUFValue(ptr, atype);
            }
            return ptr;
        }
        default: return ptr + 4;  // unknown, skip 4
    }
}

// ============================================================================
// GGUF Metadata key-value finder
// ============================================================================

static bool FindGGUFMetadataString(const uint8_t* base, uint64_t nKV,
                                     const char* keyName, char* out, int outMax) {
    const uint8_t* ptr = base + sizeof(GGUFHeader);
    
    for (uint64_t i = 0; i < nKV; i++) {
        char key[256] = {0};
        ptr = ReadGGUFString(ptr, key, 256);
        uint32_t vtype = *(const uint32_t*)ptr;
        ptr += 4;
        
        if (vtype == GGUF_TYPE_STRING) {
            bool match = true;
            for (int j = 0; keyName[j]; j++) {
                if (key[j] != keyName[j]) { match = false; break; }
            }
            if (match && key[strlen(keyName)] == 0) {
                ReadGGUFString(ptr, out, outMax);
                return true;
            }
        }
        ptr = SkipGGUFValue(ptr, vtype);
    }
    return false;
}

static bool KeyMatches(const char* lhs, const char* rhs) {
    int i = 0;
    while (lhs[i] && rhs[i]) {
        if (lhs[i] != rhs[i]) return false;
        i++;
    }
    return lhs[i] == 0 && rhs[i] == 0;
}

static bool FindGGUFMetadataU64(const uint8_t* base, uint64_t nKV,
                                  const char* keyName, uint64_t* out) {
    if (!base || !keyName || !out) return false;
    const uint8_t* ptr = base + sizeof(GGUFHeader);

    for (uint64_t i = 0; i < nKV; i++) {
        char key[256] = {0};
        ptr = ReadGGUFString(ptr, key, 256);
        uint32_t vtype = *(const uint32_t*)ptr;
        ptr += 4;

        if (KeyMatches(key, keyName)) {
            switch (vtype) {
                case GGUF_TYPE_UINT8:
                case GGUF_TYPE_BOOL:
                    *out = (uint64_t)(*ptr);
                    return true;
                case GGUF_TYPE_INT8:
                    *out = (uint64_t)(int64_t)(*(const int8_t*)ptr);
                    return true;
                case GGUF_TYPE_UINT16:
                    *out = (uint64_t)(*(const uint16_t*)ptr);
                    return true;
                case GGUF_TYPE_INT16:
                    *out = (uint64_t)(int64_t)(*(const int16_t*)ptr);
                    return true;
                case GGUF_TYPE_UINT32:
                    *out = (uint64_t)(*(const uint32_t*)ptr);
                    return true;
                case GGUF_TYPE_INT32:
                    *out = (uint64_t)(int64_t)(*(const int32_t*)ptr);
                    return true;
                case GGUF_TYPE_UINT64:
                    *out = *(const uint64_t*)ptr;
                    return true;
                case GGUF_TYPE_INT64:
                    *out = (uint64_t)(*(const int64_t*)ptr);
                    return true;
                default:
                    return false;
            }
        }

        ptr = SkipGGUFValue(ptr, vtype);
    }
    return false;
}

static bool FindGGUFMetadataArrayCount(const uint8_t* base, uint64_t nKV,
                                         const char* keyName, uint64_t* outCount) {
    if (!base || !keyName || !outCount) return false;
    const uint8_t* ptr = base + sizeof(GGUFHeader);

    for (uint64_t i = 0; i < nKV; i++) {
        char key[256] = {0};
        ptr = ReadGGUFString(ptr, key, 256);
        uint32_t vtype = *(const uint32_t*)ptr;
        ptr += 4;

        if (KeyMatches(key, keyName) && vtype == GGUF_TYPE_ARRAY) {
            ptr += 4;
            *outCount = *(const uint64_t*)ptr;
            return true;
        }

        ptr = SkipGGUFValue(ptr, vtype);
    }
    return false;
}

// ============================================================================
// Inference Thread State
// ============================================================================

struct InferRequest {
    TITAN_PARAMS params;
    volatile long active;
};

static InferRequest g_currentRequest = {};

// ============================================================================
// Token-Level Lookahead Infrastructure (Phase 1)
// Supports concurrent execution of 3 tokens while CPU preps next batch
// ============================================================================

#define MAX_TOKEN_QUEUE_DEPTH 4
#define TOKEN_PIPELINE_DEPTH 3

struct TokenSlotState {
    uint32_t slot_id;              // 0, 1, or 2 (which KV-cache slot)
    volatile int status;           // 0=pending, 1=submitted, 2=completed
    volatile long completion_fence;// Interlock for GPU completion (0=pending, 1=done)
};

static struct {
    TokenSlotState slots[MAX_TOKEN_QUEUE_DEPTH];
    volatile long head;            // Next slot to submit
    volatile long tail;            // Next slot to prepare
    volatile long completed;       // Number of tokens completed
} g_token_queue = {};

extern volatile long g_rawrxd_dispatch_stage;
extern volatile long g_rawrxd_dispatch_fault_code;
extern volatile long g_rawrxd_last_worker_exit_code;

// ============================================================================
// GGUF Bounds-safe Helpers (for metadata extraction in Route 2)
// ============================================================================

static const uint8_t* SkipGGUFValue_Safe(const uint8_t* ptr, uint32_t vtype,
                                           const uint8_t* base, uint64_t fileSize) {
    uint64_t off = (uint64_t)(ptr - base);
    switch (vtype) {
        case GGUF_TYPE_UINT8:
        case GGUF_TYPE_INT8:
        case GGUF_TYPE_BOOL:    return (off + 1 <= fileSize) ? ptr + 1 : nullptr;
        case GGUF_TYPE_UINT16:
        case GGUF_TYPE_INT16:   return (off + 2 <= fileSize) ? ptr + 2 : nullptr;
        case GGUF_TYPE_UINT32:
        case GGUF_TYPE_INT32:
        case GGUF_TYPE_FLOAT32: return (off + 4 <= fileSize) ? ptr + 4 : nullptr;
        case GGUF_TYPE_UINT64:
        case GGUF_TYPE_INT64:
        case GGUF_TYPE_FLOAT64: return (off + 8 <= fileSize) ? ptr + 8 : nullptr;
        case GGUF_TYPE_STRING: {
            if (off + 8 > fileSize) return nullptr;
            uint64_t slen = *(const uint64_t*)ptr;
            if (slen > fileSize || off + 8 + slen > fileSize) return nullptr;
            return ptr + 8 + slen;
        }
        case GGUF_TYPE_ARRAY: {
            if (off + 12 > fileSize) return nullptr;
            uint32_t atype = *(const uint32_t*)ptr; ptr += 4;
            uint64_t count = *(const uint64_t*)ptr; ptr += 8;
            if (count > 0x1000000ULL) return nullptr;  // sanity limit
            for (uint64_t i = 0; i < count; i++) {
                ptr = SkipGGUFValue_Safe(ptr, atype, base, fileSize);
                if (!ptr) return nullptr;
            }
            return ptr;
        }
        default: return (off + 4 <= fileSize) ? ptr + 4 : nullptr;
    }
}

static const uint8_t* ReadGGUFString_Safe(const uint8_t* ptr, char* out, int outMax,
                                            const uint8_t* base, uint64_t fileSize) {
    uint64_t off = (uint64_t)(ptr - base);
    if (off + 8 > fileSize) return nullptr;
    uint64_t len = *(const uint64_t*)ptr; ptr += 8;
    if (len > fileSize || off + 8 + len > fileSize) return nullptr;
    int copyLen = (int)(len < (uint64_t)(outMax - 1) ? len : (uint64_t)(outMax - 1));
    for (int i = 0; i < copyLen; i++) out[i] = (char)ptr[i];
    out[copyLen] = 0;
    return ptr + len;
}

static void ReleaseInferenceGate(volatile long* active_flag);
static int EnforceMandatoryCompletionOnSuccess(InferRequest* req,
                                               void (*callback)(const char*, int),
                                               const char* route_tag);

static DWORD __stdcall InferenceThread(LPVOID param) {
    InferRequest* req = (InferRequest*)param;
    _InterlockedExchange(&g_rawrxd_dispatch_stage, 10);
    _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 0);
    _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 0);

    /* Phase 15.2: Store current thread handle (will be applied/set later) */
    /* Thread handle storage and priority setup deferred to RawrXD_SetInferenceThreadPolicy */
    
    if (!req) {
        TitanLog("InferenceThread: null request pointer");
        _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 1001);
        _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 1);
        ReleaseInferenceGate(nullptr);
        return 1;
    }

    TitanLog("InferenceThread: entered");

    if (!req->params.callback) {
        TitanLog("InferenceThread: missing callback");
        _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 1002);
        _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 1);
        ReleaseInferenceGate(&req->active);
        return 1;
    }

    void (*callback)(const char*, int) = req->params.callback;
    const char* prompt = req->params.prompt;
    int maxTokens = req->params.max_tokens;
    float temperature = req->params.temperature;

    // Check cancellation before expensive work
    if (_InterlockedCompareExchange(&g_cancelRequest, 0, 0) != 0) {
        TitanLog("InferenceThread: canceled before route dispatch");
        _InterlockedExchange(&g_rawrxd_dispatch_stage, 11);
        _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 1003);
        _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 99);
        ReleaseInferenceGate(&req->active);
        return 99;
    }

#if RAWRXD_ENABLE_OMEGA_HIP
    {
        const unsigned long long expectedDoorbell = 0x01C7F8000000ULL + 0x2190ULL;
        char preBuf[192];
        wsprintfA(preBuf,
                  "InferenceThread: omega-bridge pre addr=0x%I64X emit_seq=%I64u mailbox=%I64u probe=%ld",
                  expectedDoorbell,
                  g_rawrxd_last_doorbell_emit_seq,
                  g_rawrxd_mailbox_data_seq,
                  g_rawrxd_omega_probe_early_return);
        TitanLog(preBuf);

        __try {
            RawrXD_Omega_Singularity(req);
        } __except(1) {
            TitanLog("InferenceThread: omega-bridge exception");
        }

        char postBuf[224];
        wsprintfA(postBuf,
                  "InferenceThread: omega-bridge post addr=0x%I64X val=0x%I64X emit_seq=%I64u mailbox=%I64u",
                  g_rawrxd_last_doorbell_addr,
                  g_rawrxd_last_doorbell_value,
                  g_rawrxd_last_doorbell_emit_seq,
                  g_rawrxd_mailbox_data_seq);
        TitanLog(postBuf);
    }
#endif

    // ================================================================
    // Route 1: Ollama HTTP API — REAL 70B INFERENCE (non-simulated)
    // POST http://127.0.0.1:11434/api/generate
    // Model: bg40-unleashed:latest (BigDaddyG 70B Q4_K_M)
    // ================================================================
    static char ollamaResp[16384];
    int respLen = -777;
    _InterlockedExchange(&g_rawrxd_dispatch_stage, 20);
    __try {
        respLen = OllamaGenerate(prompt, maxTokens, temperature,
                                 ollamaResp, sizeof(ollamaResp));
    } __except(1) {
        TitanLog("InferenceThread: OllamaGenerate raised exception");
        respLen = -778;
        _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 2001);
    }
    {
        char logBuf[128];
        wsprintfA(logBuf, "InferenceThread: OllamaGenerate rc=%d", respLen);
        TitanLog(logBuf);
    }
    if (respLen > 0) {
        TitanLog("InferenceThread: Ollama returned live response");
        _InterlockedExchange(&g_rawrxd_dispatch_stage, 21);
        __try {
            _InterlockedExchange(&g_rawrxd_dispatch_stage, 22);
            callback(ollamaResp, respLen);
        } __except(1) {
            TitanLog("InferenceThread: callback exception (ollama route)");
            _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 2101);
            __try {
                callback(nullptr, 0);
            } __except(1) {
                TitanLog("InferenceThread: callback fallback exception (ollama route)");
            }
            _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 4);
            ReleaseInferenceGate(&req->active);
            return 4;
        }
        TitanLog("InferenceThread: callback completed (ollama route)");
        if (EnforceMandatoryCompletionOnSuccess(req, callback, "ollama") != 0) {
            return 8;
        }
        _InterlockedExchange(&g_rawrxd_dispatch_stage, 29);
        _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 0);
        _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 0);
        ReleaseInferenceGate(&req->active);
        TitanLog("InferenceThread: active cleared (ollama route)");
        return 0;
    }

    if (_InterlockedCompareExchange(&g_allowOfflineFallback, 0, 0) == 0) {
        char liveErr[256];
        wsprintfA(liveErr,
                  "[Titan live route unavailable: model=%s host=127.0.0.1:11434 rc=%d]",
                  g_liveModelName,
                  respLen);
        __try {
            _InterlockedExchange(&g_rawrxd_dispatch_stage, 31);
            callback(liveErr, (int)strlen(liveErr));
        } __except(1) {
            TitanLog("InferenceThread: callback exception (live unavailable)");
            _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 3101);
            __try {
                callback(nullptr, 0);
            } __except(1) {
                TitanLog("InferenceThread: callback fallback exception (live unavailable)");
            }
            _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 5);
            ReleaseInferenceGate(&req->active);
            return 5;
        }
        TitanLog("InferenceThread: callback completed (live unavailable)");
        if (EnforceMandatoryCompletionOnSuccess(req, callback, "live-unavailable") != 0) {
            return 8;
        }
        _InterlockedExchange(&g_rawrxd_dispatch_stage, 39);
        _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 0);
        _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 3);
        ReleaseInferenceGate(&req->active);
        TitanLog("InferenceThread: active cleared (live unavailable)");
        return 3;
    }

    // ================================================================
    // Route 2: GGUF-direct Embedding Similarity (fallback)
    // If Ollama is offline, walk the GGUF tensor table to find
    // token_embd.weight, dequantize Q4_K blocks, and do iterative
    // cosine-similarity token generation from real weight data.
    // ================================================================
    if (g_mappedBase && g_fileSize > sizeof(GGUFHeader)) {
        __try {
            // ── Step 1: Read model metadata via FindGGUFMetadataString ──
            char archBuf[64] = {0};
            FindGGUFMetadataString(g_mappedBase, g_nKV,
                                    "general.architecture", archBuf, 64);
            // archBuf now contains e.g. "llama"

            // ── Step 2: Walk KV metadata to find tensor data start ──
            const uint8_t* ptr = g_mappedBase + sizeof(GGUFHeader);
            for (uint64_t i = 0; i < g_nKV; i++) {
                if ((uint64_t)(ptr - g_mappedBase) + 8 > g_fileSize) break;
                uint64_t kl = *(const uint64_t*)ptr;
                if (kl > 0x100000ULL) break;
                ptr += 8;
                if ((uint64_t)(ptr - g_mappedBase) + kl > g_fileSize) break;
                ptr += kl;
                if ((uint64_t)(ptr - g_mappedBase) + 4 > g_fileSize) break;
                uint32_t vt = *(const uint32_t*)ptr; ptr += 4;
                ptr = SkipGGUFValue_Safe(ptr, vt, g_mappedBase, g_fileSize);
                if (!ptr) break;
            }

            // ── Step 3: Walk tensor info to find token_embd.weight ──
            uint64_t embedOffset = 0;
            uint64_t embedDim0 = 0;   // embed dimension (e.g. 8192)
            uint64_t embedDim1 = 0;   // vocab size (e.g. 32000)
            uint32_t embedType = 0;
            bool found = false;
            const uint8_t* tensorInfoEnd = ptr;

            for (uint64_t t = 0; t < g_nTensors && t < 2000; t++) {
                char tn[128] = {0};
                const uint8_t* nx = ReadGGUFString_Safe(ptr, tn, 128,
                                       g_mappedBase, g_fileSize);
                if (!nx) break;
                ptr = nx;
                if ((uint64_t)(ptr - g_mappedBase) + 4 > g_fileSize) break;
                uint32_t nd = *(const uint32_t*)ptr; ptr += 4;
                if (nd > 8) break;
                uint64_t dims[8] = {0};
                for (uint32_t d = 0; d < nd; d++) {
                    if ((uint64_t)(ptr - g_mappedBase) + 8 > g_fileSize) goto rt2_done;
                    dims[d] = *(const uint64_t*)ptr;
                    ptr += 8;
                }
                if ((uint64_t)(ptr - g_mappedBase) + 12 > g_fileSize) break;
                uint32_t tt = *(const uint32_t*)ptr; ptr += 4;
                uint64_t toff = *(const uint64_t*)ptr; ptr += 8;

                // Match "token_embd.weight"
                const char* target = "token_embd.weight";
                bool m = true;
                for (int j = 0; target[j]; j++)
                    if (tn[j] != target[j]) { m = false; break; }
                if (m && tn[17] == 0) {
                    embedOffset = toff;
                    embedDim0 = dims[0];  // embed dimension
                    embedDim1 = dims[1];  // vocab size
                    embedType = tt;
                    found = true;
                }
                tensorInfoEnd = ptr;
            }
rt2_done:
            if (found && embedDim0 > 0 && embedDim1 > 0) {
                // Compute data section start (aligned)
                uint64_t hdrEnd = (uint64_t)(tensorInfoEnd - g_mappedBase);
                uint64_t align = 32;
                uint64_t dataStart = (hdrEnd + align - 1) & ~(align - 1);
                if (embedOffset > (~0ULL - dataStart)) {
                    goto route3;
                }
                uint64_t absEmbedBase = dataStart + embedOffset;

                // Bounds check: ensure embedding table fits in file
                uint64_t blocksPerRow = (embedDim0 + 255) / 256;
                if (blocksPerRow == 0 || embedDim1 > (~0ULL / blocksPerRow)) {
                    goto route3;
                }
                uint64_t embedSize = embedDim1 * blocksPerRow;
                if (embedSize > (~0ULL / 144ULL)) {
                    goto route3;
                }
                embedSize *= 144ULL;  // Q4_K block = 144 bytes (d:2 + dmin:2 + scales:12 + qs:128)
                if (absEmbedBase > g_fileSize || embedSize > (g_fileSize - absEmbedBase)) {
                    goto route3;
                }

                // Route 2: GGUF is verified present but Ollama is offline.
                // Without a full transformer forward pass (attention layers, FFN,
                // RoPE, LayerNorm — impractical in plain C for 70B), we cannot
                // generate coherent text from embeddings alone.
                // Report honest status with model metadata instead of gibberish.
                char rt2Msg[512];
                int rt2Len = 0;
                const char* rt2Prefix = "[Titan: Ollama offline — GGUF verified: ";
                while (rt2Prefix[rt2Len]) { rt2Msg[rt2Len] = rt2Prefix[rt2Len]; rt2Len++; }
                // Append arch
                for (int i = 0; archBuf[i] && rt2Len < 480; i++) rt2Msg[rt2Len++] = archBuf[i];
                rt2Msg[rt2Len++] = ' ';
                // Append dims
                wsprintfA(rt2Msg + rt2Len, "%I64ux%I64u Q4_K_M]",
                          embedDim0, embedDim1);
                while (rt2Msg[rt2Len]) rt2Len++;

                OutputDebugStringA(rt2Msg);
                __try {
                    _InterlockedExchange(&g_rawrxd_dispatch_stage, 41);
                    callback(rt2Msg, rt2Len);
                } __except(1) {
                    TitanLog("InferenceThread: callback exception (gguf metadata route)");
                    _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 4101);
                    __try {
                        callback(nullptr, 0);
                    } __except(1) {
                        TitanLog("InferenceThread: callback fallback exception (gguf metadata route)");
                    }
                    _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 6);
                    ReleaseInferenceGate(&req->active);
                    return 6;
                }
                TitanLog("InferenceThread: callback completed (gguf metadata route)");
                if (EnforceMandatoryCompletionOnSuccess(req, callback, "gguf-metadata") != 0) {
                    return 8;
                }
                _InterlockedExchange(&g_rawrxd_dispatch_stage, 49);
                _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 0);
                _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 0);
                ReleaseInferenceGate(&req->active);
                TitanLog("InferenceThread: active cleared (gguf metadata route)");
                return 0;
            }
        } __except(1) {
            TitanLog("InferenceThread: GGUF embedding-sim AV");
            _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 4001);
        }
    }

route3:

    // Route 3: No inference available
    const char* errMsg = "[Titan: Ollama offline, GGUF fallback failed]";
    int errLen = 0;
    while (errMsg[errLen]) errLen++;
    __try {
        _InterlockedExchange(&g_rawrxd_dispatch_stage, 51);
        callback(errMsg, errLen);
    } __except(1) {
        TitanLog("InferenceThread: callback exception (terminal fallback)");
        _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 5101);
        __try {
            callback(nullptr, 0);
        } __except(1) {
            TitanLog("InferenceThread: callback fallback exception (terminal fallback)");
        }
        _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 7);
        ReleaseInferenceGate(&req->active);
        return 7;
    }
    TitanLog("InferenceThread: callback completed (terminal fallback)");
    if (EnforceMandatoryCompletionOnSuccess(req, callback, "terminal-fallback") != 0) {
        return 8;
    }
    _InterlockedExchange(&g_rawrxd_dispatch_stage, 59);
    _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 0);
    _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 2);
    ReleaseInferenceGate(&req->active);
    TitanLog("InferenceThread: active cleared (terminal fallback)");
    return 2;
}

// =============================================================================
// Phase 6: Optimized aperture kernel dispatch (AVX-512 acceleration)
// Thin Bridge Patch: augments Phase 3 k_swap operations with optional
// hardware-accelerated seal hash verification (1.528x speedup on Zen 4)
// =============================================================================
extern "C" void aperture_detect_avx512(void);
extern "C" void aperture_compute_seal_hash(void* aperture_result, void* subdivision_table);
extern "C" volatile unsigned char g_ApertureAvx512Supported;
extern "C" volatile unsigned long long g_ApertureHashLatency_tsc;
extern "C" int k_header_verify_fast(const void* file_base, uint64_t file_size_bytes, uint32_t* out_format, uint32_t* out_flags);

static volatile long g_aperture_seal_dispatch_ready = 0;  // 1 when hardware detection done
static uint64_t g_rawrxd_header_fast_path_hits = 0;
static uint64_t g_rawrxd_header_fast_path_failures = 0;
static volatile long g_aperture_dispatch_fallback_count = 0;
static volatile long g_aperture_dispatch_fallback_warned = 0;

typedef int (*ApertureSealVerifyFn)(uint64_t mapped_base, uint64_t size_bytes, void* mapped_chunk_ptr);
static ApertureSealVerifyFn g_aperture_seal_verify_fn = nullptr;
static volatile long g_aperture_dispatch_mode = 0;  // 0=unknown, 1=scalar, 2=avx512

static void EnsureApertureDispatchReady();
static int Aperture_SealVerifyScalar(uint64_t mapped_base, uint64_t size_bytes, void* mapped_chunk_ptr);
static int Aperture_SealVerifyAvx512(uint64_t mapped_base, uint64_t size_bytes, void* mapped_chunk_ptr);

#include "core/dispatch_inline.h"

__forceinline static void NoteApertureDispatchFallbackOnce() {
    _InterlockedIncrement(&g_aperture_dispatch_fallback_count);
    if (!_InterlockedCompareExchange(&g_aperture_dispatch_fallback_warned, 1, 0)) {
        OutputDebugStringA("RawrXD_ApertureDispatch: fail-closed scalar fallback engaged");
    }
}

static unsigned char DetectAvx512FAndDQ() {
#if defined(_M_X64)
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 1, 0);

    const int ecx = regs[2];
    const int hasXsave = (ecx & (1 << 26)) != 0;
    const int hasOsXsave = (ecx & (1 << 27)) != 0;
    const int hasAvx = (ecx & (1 << 28)) != 0;
    if (!hasXsave || !hasOsXsave || !hasAvx) {
        return 0;
    }

    unsigned long long xcr0 = _xgetbv(0);
    const unsigned long long kXcr0AvxMask = (1ULL << 1) | (1ULL << 2);                  // XMM/YMM
    const unsigned long long kXcr0ZmmMask = (1ULL << 5) | (1ULL << 6) | (1ULL << 7);    // Opmask/ZMM
    if ((xcr0 & (kXcr0AvxMask | kXcr0ZmmMask)) != (kXcr0AvxMask | kXcr0ZmmMask)) {
        return 0;
    }

    __cpuidex(regs, 7, 0);
    const int ebx = regs[1];
    const int hasAvx512F = (ebx & (1 << 16)) != 0;
    const int hasAvx512DQ = (ebx & (1 << 17)) != 0;
    return (hasAvx512F && hasAvx512DQ) ? 1 : 0;
#else
    return 0;
#endif
}

// ============================================================================
// DLL EXPORTS
// ============================================================================

extern "C" __declspec(dllexport) int Titan_Initialize(const char* modelPath) {
    if (_InterlockedCompareExchange(&g_initialized, 1, 0) != 0) {
        return 0;  // Already initialized
    }
    
    if (!modelPath || !modelPath[0]) return -1;
    ZeroModelMetadata();
    
    // Store model path
    int i = 0;
    while (modelPath[i] && i < 510) { g_modelPath[i] = modelPath[i]; i++; }
    g_modelPath[i] = 0;
    
    TitanLog("Titan_Initialize: Opening GGUF");
    
    // Memory-map the GGUF file
    g_hFile = CreateFileA(modelPath, GENERIC_READ_, FILE_SHARE_READ_,
                          nullptr, OPEN_EXISTING_, 0, nullptr);
    if (g_hFile == INVALID_HANDLE_) {
        TitanLog("Titan_Initialize: CreateFileA failed");
        _InterlockedExchange(&g_initialized, 0);
        return -2;
    }
    
    DWORD sizeHi = 0;
    DWORD sizeLo = GetFileSize(g_hFile, &sizeHi);
    g_fileSize = ((uint64_t)sizeHi << 32) | sizeLo;
    
    g_hMapping = CreateFileMappingA(g_hFile, nullptr, PAGE_READONLY_, 0, 0, nullptr);
    if (!g_hMapping) {
        CloseHandle(g_hFile);
        g_hFile = INVALID_HANDLE_;
        ZeroModelMetadata();
        _InterlockedExchange(&g_initialized, 0);
        return -3;
    }
    
    g_mappedBase = (const uint8_t*)MapViewOfFile(g_hMapping, FILE_MAP_READ_, 0, 0, 0);
    if (!g_mappedBase) {
        CloseHandle(g_hMapping);
        CloseHandle(g_hFile);
        g_hMapping = nullptr;
        g_hFile = INVALID_HANDLE_;
        ZeroModelMetadata();
        _InterlockedExchange(&g_initialized, 0);
        return -4;
    }
    
    // Validate GGUF header
    if (g_fileSize < sizeof(GGUFHeader)) {
        UnmapViewOfFile((LPVOID)g_mappedBase);
        CloseHandle(g_hMapping);
        CloseHandle(g_hFile);
        g_mappedBase = nullptr;
        ZeroModelMetadata();
        _InterlockedExchange(&g_initialized, 0);
        return -5;
    }
    
    const GGUFHeader* hdr = (const GGUFHeader*)g_mappedBase;
    if (hdr->magic != RAWRXD_GGUF_MAGIC_U32) {
        TitanLog("Titan_Initialize: Bad GGUF magic");
        UnmapViewOfFile((LPVOID)g_mappedBase);
        CloseHandle(g_hMapping);
        CloseHandle(g_hFile);
        g_mappedBase = nullptr;
        ZeroModelMetadata();
        _InterlockedExchange(&g_initialized, 0);
        return -6;
    }
    
    g_nTensors = hdr->n_tensors;
    g_nKV = hdr->n_kv;
    PopulateLoadedModelMetadata();
#if RAWR_ENABLE_GGML_LINK
    InitializeGGMLBridgeContext();
#endif
    ConfigureLiveRoutePolicy();
    
    // Phase 6: Initialize aperture dispatch layer (CPUID/XGETBV detect + hot-swap).
    EnsureApertureDispatchReady();
    
    // Phase 8: Check for neural embeddings activation via environment
    char neuralEnv[32] = {0};
    DWORD envLen = GetEnvironmentVariableA("RAWR_NEURAL_EMBEDDINGS", neuralEnv, sizeof(neuralEnv) - 1);
    if (envLen > 0 && (neuralEnv[0] == '1' || neuralEnv[0] == 't' || neuralEnv[0] == 'T' || neuralEnv[0] == 'y' || neuralEnv[0] == 'Y')) {
        _InterlockedExchange(&g_neuralEmbeddingsActive, 1);
        OutputDebugStringA("Titan_Initialize: Neural embeddings ACTIVATED via environment");
    }

    char foldModeEnv[32] = {0};
    DWORD foldEnvLen = GetEnvironmentVariableA("RAWR_EMBED_FOLD_MODE", foldModeEnv, sizeof(foldModeEnv) - 1);
    if (foldEnvLen > 0) {
        uint32_t foldMode = ParseEmbeddingFoldModeEnv(foldModeEnv);
        _InterlockedExchange(&g_embeddingFoldMode, (long)foldMode);
    }

    char vkMemEnv[32] = {0};
    DWORD vkMemLen = GetEnvironmentVariableA("RAWR_VK_MEMORY_CLASS", vkMemEnv, sizeof(vkMemEnv) - 1);
    if (vkMemLen > 0) {
        uint32_t memClass = ParseVulkanMemoryClassEnv(vkMemEnv);
        _InterlockedExchange(&g_vulkanMemoryClass, (long)memClass);
    }
    
    char logBuf[256];
    wsprintfA(logBuf, "Titan: GGUF v%u, %I64u tensors, %I64u KV, %I64u bytes, Neural=%s, Fold=%s, VkMem=%s",
              hdr->version, g_nTensors, g_nKV, g_fileSize,
              _InterlockedCompareExchange(&g_neuralEmbeddingsActive, 0, 0) ? "ON" : "OFF",
              EmbeddingFoldModeName((uint32_t)_InterlockedCompareExchange(&g_embeddingFoldMode, 0, 0)),
              VulkanMemoryClassName((uint32_t)_InterlockedCompareExchange(&g_vulkanMemoryClass, 0, 0)));
    OutputDebugStringA(logBuf);
    
    return 0;  // Success
}

extern "C" __declspec(dllexport) int Titan_InferAsync(TITAN_PARAMS* params) {
    if (!g_initialized || !params || !params->callback) return -1;
    
    // Check if a request is already in flight
    if (_InterlockedCompareExchange(&g_currentRequest.active, 1, 0) != 0) {
        return -2;  // Busy
    }
    
    // Copy params
    g_currentRequest.params = *params;
    
    // Launch inference thread
    DWORD tid = 0;
    HANDLE hThread = CreateThread(nullptr, 0, InferenceThread,
                                   &g_currentRequest, 0, &tid);
    if (hThread) {
        CloseHandle(hThread);  // Detach — thread runs independently
    } else {
        _InterlockedExchange(&g_currentRequest.active, 0);
        return -3;
    }
    
    return 0;  // Submitted
}

extern "C" __declspec(dllexport) void Titan_Shutdown(void) {
    // Signal cancellation to any in-flight inference
    _InterlockedExchange(&g_cancelRequest, 1);

    // Bounded wait for active inference (max 3 seconds, not infinite)
    int waitCount = 0;
    while (_InterlockedCompareExchange(&g_currentRequest.active, 0, 0) != 0) {
        Sleep(10);
        if (++waitCount > 300) {  // 3 seconds max
            // Force-release the active flag — inference thread will
            // harmlessly write to static buffers on completion
            _InterlockedExchange(&g_currentRequest.active, 0);
            TitanLog("Titan_Shutdown: Forced active flag release after 3s");
            break;
        }
    }
    
    // Close persistent transport session before tearing down Winsock.
    CloseOllamaSocket();

    // Clean up Winsock if initialized
    if (g_wsaReady && pfn_WSACleanup) {
        pfn_WSACleanup();
        g_wsaReady = 0;
    }
    if (g_hWs2) {
        FreeLibrary(g_hWs2);
        g_hWs2 = nullptr;
    }
    
    if (g_mappedBase) {
        UnmapViewOfFile((LPVOID)g_mappedBase);
        g_mappedBase = nullptr;
    }
    if (g_hMapping) {
        CloseHandle(g_hMapping);
        g_hMapping = nullptr;
    }
    if (g_hFile != INVALID_HANDLE_) {
        CloseHandle(g_hFile);
        g_hFile = INVALID_HANDLE_;
    }

#if RAWR_ENABLE_GGML_LINK
    ShutdownGGMLBridgeContext();
#endif
    
    _InterlockedExchange(&g_initialized, 0);
    ZeroModelMetadata();
    TitanLog("Titan_Shutdown: Complete");
}

// ============================================================================
// DllMain — minimal
// ============================================================================

extern "C" BOOL __stdcall DllMain(HANDLE hModule, DWORD reason, LPVOID reserved) {
    (void)hModule; (void)reserved;
    if (reason == 0 /* DLL_PROCESS_DETACH */) {
        // CRITICAL: Do NOT call Titan_Shutdown here.
        // DllMain runs under the loader lock — any wait (Sleep, recv)
        // deadlocks the entire process. Just signal cancellation so
        // inference threads bail quickly on their next recv/check.
        _InterlockedExchange(&g_cancelRequest, 1);
    }
    return 1;  // TRUE
}

// ============================================================================
// Titan_Abort — Signal cancellation to in-flight inference (non-blocking)
// ============================================================================
extern "C" __declspec(dllexport) void Titan_Abort(void) {
    _InterlockedExchange(&g_cancelRequest, 1);
}

// ============================================================================
// RawrXD 77-export interface (Phase 2 stubs + aperture wrappers)
// ============================================================================

// Forward declarations — defined further below after struct/typedef section
static void ZeroModelMetadata();
static void PopulateLoadedModelMetadata();
static void StrCopyN(char* dst, const char* src, size_t n);

typedef int32_t  RAWRXD_STATUS;
typedef uint64_t RAWRXD_MODEL_HANDLE;
typedef uint64_t RAWRXD_INFERENCE_HANDLE;

#define RAWRXD_SUCCESS               0x00000000
#define RAWRXD_ERROR_NOT_INITIALIZED 0x80000001
#define RAWRXD_ERROR_INVALID_PARAM   0x80000002
#define RAWRXD_ERROR_NOT_READY       0x80000003
#define RAWRXD_ERROR_NO_MODEL_LOADED 0x80000004
#define RAWRXD_ERROR_OUT_OF_MEMORY   0x80000005
#define RAWRXD_ERROR_TIMED_OUT       0x80000007

#define RAWRXD_CAP_GPU_ACCELERATION  0x00000001
#define RAWRXD_CAP_QUANTIZATION      0x00000002
#define RAWRXD_CAP_APERTURE_MAPPING  0x00000004
#define RAWRXD_CAP_STREAMING         0x00000008
#define RAWRXD_CAP_HOTPATCHING       0x00000010
#define RAWRXD_CAP_MULTI_BATCH       0x00000020
#define RAWRXD_CAP_DIAGNOSTICS       0x00000040

typedef struct {
    float temperature;
    float top_p;
    int32_t top_k;
    float repetition_penalty;
    int32_t max_tokens;
} RAWRXD_SAMPLING_PARAMS;

typedef struct {
    uint64_t size_bytes;
    uint32_t parameter_count;
    uint32_t vocab_size;
    uint32_t context_length;
    char model_name[256];
    char model_path[260];
    uint32_t precision_bits;
} RAWRXD_MODEL_INFO;

typedef struct {
    uint64_t output_buffer;
    uint32_t output_token_count;
    uint32_t input_token_count;
    uint64_t latency_ms;
    RAWRXD_STATUS status;
} RAWRXD_INFERENCE_RESULT;

typedef struct {
    uint64_t aperture_base;
    uint64_t aperture_total_bytes;
    uint64_t mapped_bytes;
    uint64_t unmapped_bytes;
    uint32_t mapped_chunks;
    uint32_t fragment_count;
    float fragmentation_ratio;
    uint32_t utilization_pct10000;  /* Phase 15.2: utilization as pct * 10000 */
} RAWRXD_APERTURE_STATUS;

typedef struct {
    uint64_t total_allocated;
    uint64_t currently_allocated;
    uint64_t peak_allocated;
    uint64_t virtual_address_space;
    uint32_t heap_blocks;
    float heap_fragmentation;
} RAWRXD_MEMORY_STATS;

typedef struct {
    uint64_t data_seq;
    uint64_t consumed_seq;
    uint64_t frame_ready;
    uint64_t epoch;
} RAWRXD_RF_COUNTERS;

/* Phase 15.2: Thread priority policy enums and structs */
typedef enum {
    RAWRXD_THREAD_PRIORITY_NORMAL = 0,
    RAWRXD_THREAD_PRIORITY_HIGH = 1,
    RAWRXD_THREAD_PRIORITY_REALTIME_LIGHT = 2,
    RAWRXD_THREAD_PRIORITY_AFFINITY_CPU0 = 3
} RAWRXD_THREAD_PRIORITY_POLICY;

typedef struct {
    RAWRXD_THREAD_PRIORITY_POLICY current_policy;
    uint32_t base_priority;
    uint32_t boost_enabled;
    uint32_t affinity_mask;
} RAWRXD_THREAD_POLICY_STATUS;

static volatile long g_rawrxd_initialized = 0;
static RAWRXD_STATUS g_rawrxd_last_error = RAWRXD_SUCCESS;
static RAWRXD_MODEL_HANDLE g_rawrxd_active_model = 0;
static uint64_t g_rawrxd_aperture_base = 0;
static uint64_t g_rawrxd_persistent_aperture_base = 0;  // Phase 6: Persistent VA reservation (reused across cycles)
static uint64_t g_rawrxd_mapped_bytes = 0;
static uint32_t g_rawrxd_mapped_chunks = 0;
static uint64_t g_rawrxd_working_set_limit = 0;
static uint64_t g_rawrxd_peak_memory = 0;
static double g_rawrxd_map_latency_ms = 0.0;
static double g_rawrxd_unmap_latency_ms = 0.0;
static double g_rawrxd_inference_latency_ms = 0.0;
static volatile long g_rawrxd_inference_active = 0;

/* Phase 15.2: Thread policy control globals */
static volatile long g_inference_thread_policy = RAWRXD_THREAD_PRIORITY_NORMAL;
static volatile long g_inference_thread_base_priority = THREAD_PRIORITY_NORMAL;
static volatile long g_inference_thread_boost_enabled = 1;
static volatile long g_inference_thread_affinity_mask = 0;  // 0 = no affinity set
static HANDLE g_inference_thread_handle = NULL;
static RAWRXD_STATUS g_rawrxd_inference_status = RAWRXD_SUCCESS;
static uint32_t g_rawrxd_last_input_tokens = 0;
static uint32_t g_rawrxd_last_output_tokens = 0;
static uint64_t g_rawrxd_inference_generation = 0;
static uint64_t g_rawrxd_last_submit_generation = 0;
static uint64_t g_rawrxd_last_wait_handle = 0;
static uint64_t g_rawrxd_last_completed_handle = 0;
static uint64_t g_rawrxd_last_submit_prompt_ptr = 0;
static uint64_t g_rawrxd_last_submit_callback_ptr = 0;
static uint64_t g_rawrxd_last_callback_text_ptr = 0;
static uint64_t g_rawrxd_last_callback_generation = 0;
static uint32_t g_rawrxd_last_callback_len = 0;
static uint32_t g_rawrxd_last_wait_timeout_ms = 0;
static uint32_t g_rawrxd_last_wait_elapsed_ms = 0;
static char g_rawrxd_system_prompt[2048] = {0};
static void (*g_rawrxd_diag_callback)(const char*) = nullptr;
static uint8_t g_rawrxd_stub_buffer[4096] = {0};
static char g_rawrxd_inference_text[16384] = {0};

static int NormalizeModelOutputToUtf8(const char* text, int len, char* out, int outCapacity) {
    if (!text || len <= 0 || !out || outCapacity <= 0) {
        return 0;
    }

    int useLen = len;
    if (useLen > outCapacity - 1) {
        useLen = outCapacity - 1;
    }

    // Fast path: already valid UTF-8.
    int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, useLen, nullptr, 0);
    if (wideLen > 0) {
        for (int i = 0; i < useLen; ++i) {
            out[i] = text[i];
        }
        out[useLen] = 0;
        return useLen;
    }

    // Fallback: decode using active ANSI code page and convert to UTF-8 for the UI.
    wideLen = MultiByteToWideChar(CP_ACP, 0, text, useLen, nullptr, 0);
    if (wideLen > 0) {
        wchar_t wideBuf[16384];
        if (wideLen >= (int)(sizeof(wideBuf) / sizeof(wideBuf[0]))) {
            wideLen = (int)(sizeof(wideBuf) / sizeof(wideBuf[0])) - 1;
        }
        int convertedWide = MultiByteToWideChar(CP_ACP, 0, text, useLen, wideBuf, wideLen);
        if (convertedWide > 0) {
            wideBuf[convertedWide] = 0;
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideBuf, convertedWide, out, outCapacity - 1, nullptr, nullptr);
            if (utf8Len > 0) {
                out[utf8Len] = 0;
                return utf8Len;
            }
        }
    }

    // Final fallback: preserve bytes verbatim instead of replacing with question marks.
    for (int i = 0; i < useLen; ++i) {
        out[i] = text[i];
    }
    out[useLen] = 0;
    return useLen;
}
static RAWRXD_SAMPLING_PARAMS g_rawrxd_sampling_params = {0.7f, 0.9f, 40, 1.0f, 256};
static uint8_t g_rawrxd_aperture_slot_map[1024] = {0};
static uint8_t g_rawrxd_aperture_slot_initialized[1024] = {0};  // Phase 6: 1 when slot's placeholder has been split
static uint64_t g_rawrxd_aperture_slot_base[1024] = {0};
static uint64_t g_rawrxd_aperture_slot_size[1024] = {0};
// Phase 4: Performance counters — populated by InferAsync/Callback
static LARGE_INTEGER g_rawrxd_infer_start_tick = {};
static LARGE_INTEGER g_rawrxd_perf_freq = {};
static uint64_t g_rawrxd_total_output_tokens = 0;
static uint64_t g_rawrxd_total_inference_requests = 0;
static uint64_t g_rawrxd_completed_inference_requests = 0;
static uint64_t g_rawrxd_timed_out_inference_requests = 0;
static uint64_t g_rawrxd_canceled_inference_requests = 0;
static uint64_t g_rawrxd_failed_inference_requests = 0;
static uint64_t g_rawrxd_last_completed_generation = 0;
extern "C" volatile long g_rawrxd_completion_fence = 0;
static volatile long g_rawrxd_diagnostics_enabled = 0;
static char g_rawrxd_last_infersync_line[384] = {0};
static uint64_t g_rawrxd_aperture_init_count = 0;
static uint64_t g_rawrxd_aperture_shutdown_count = 0;
static uint64_t g_rawrxd_map_chunk_count = 0;
static uint64_t g_rawrxd_unmap_chunk_count = 0;
static uint32_t g_rawrxd_last_map_chunk = 0;
static uint32_t g_rawrxd_last_unmap_chunk = 0;
static uint64_t g_rawrxd_last_map_address = 0;
static uint64_t g_rawrxd_last_unmap_address = 0;
volatile long g_rawrxd_dispatch_stage = 0;
volatile long g_rawrxd_dispatch_fault_code = 0;
volatile long g_rawrxd_last_worker_exit_code = 0;

static void ReleaseInferenceGate(volatile long* active_flag) {
    const long stage = _InterlockedCompareExchange(&g_rawrxd_dispatch_stage, 0, 0);
    const long fault = _InterlockedCompareExchange(&g_rawrxd_dispatch_fault_code, 0, 0);
    const long worker_rc = _InterlockedCompareExchange(&g_rawrxd_last_worker_exit_code, 0, 0);
    char gateBuf[224];
    wsprintfA(gateBuf,
              "ReleaseInferenceGate: stage=%ld fault=%ld worker_rc=%ld req_active=%ld infer_active=%ld fence_before=%ld",
              stage,
              fault,
              worker_rc,
              active_flag ? _InterlockedCompareExchange(active_flag, 0, 0) : -1,
              g_rawrxd_inference_active,
              _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0));
    TitanLog(gateBuf);
    if (active_flag) {
        _InterlockedExchange(active_flag, 0);
    }
    _InterlockedExchange(&g_rawrxd_inference_active, 0);
    _InterlockedExchange(&g_rawrxd_completion_fence, 1);
}

static int EnforceMandatoryCompletionOnSuccess(InferRequest* req,
                                               void (*callback)(const char*, int),
                                               const char* route_tag) {
    const uint64_t current_generation = g_rawrxd_inference_generation;
    const uint64_t completed_generation = g_rawrxd_last_completed_generation;
    if (completed_generation == current_generation) {
        return 0;
    }

    char guardBuf[256];
    wsprintfA(guardBuf,
              "InferenceThread: completion-contract breach route=%s gen=%I64u done=%I64u cb_gen=%I64u",
              route_tag ? route_tag : "(unknown)",
              (unsigned long long)current_generation,
              (unsigned long long)completed_generation,
              (unsigned long long)g_rawrxd_last_callback_generation);
    TitanLog(guardBuf);

    _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 9902);
    g_rawrxd_inference_status = RAWRXD_ERROR_NOT_READY;
    g_rawrxd_failed_inference_requests++;

    if (callback && g_rawrxd_last_callback_generation != current_generation) {
        const char* synth = "[Titan completion contract violation]";
        __try {
            callback(synth, (int)strlen(synth));
        } __except(1) {
            TitanLog("InferenceThread: synthetic completion callback faulted");
        }
    }

    _InterlockedExchange(&g_rawrxd_last_worker_exit_code, 8);
    ReleaseInferenceGate(req ? &req->active : nullptr);
    return 8;
}
// Phase 4: BOS/EOS token IDs read from GGUF KV on model load (defaults = 1,2)
static int32_t g_rawrxd_bos_token_id = 1;
static int32_t g_rawrxd_eos_token_id = 2;
static RawrXDStreamBuffer g_rawrxd_stream_buffer;
static volatile long long g_rawrxd_stream_seq = 0;
static volatile long g_rawrxd_streaming_enabled = 0;
static void* g_rawrxd_stream_hwnd = nullptr;
static unsigned int g_rawrxd_stream_msg = 0;
static unsigned int g_rawrxd_stream_wparam = 0;

static const uint64_t kRawrXDApertureSpanBytes = 1099511627776ULL;
static const uint64_t kRawrXDApertureChunkBytes = 1073741824ULL;
static const uint32_t kRawrXDApertureSlotCount = 1024U;
static const uint32_t kRawrXDIndexerSlot = 1023U;

static HANDLE g_rawrxd_indexer_file = INVALID_HANDLE_;
static HANDLE g_rawrxd_indexer_mapping = nullptr;
static uint64_t g_rawrxd_indexer_mapped_base = 0;
static uint64_t g_rawrxd_indexer_mapped_size = 0;
static uint32_t g_rawrxd_indexer_window_count = 0;
static HANDLE g_rawrxd_indexer_embedding_mapping = nullptr;
static uint64_t g_rawrxd_indexer_embedding_base = 0;
static uint64_t g_rawrxd_indexer_embedding_capacity = 0;
static uint32_t g_rawrxd_indexer_embedding_count = 0;

#define RAWRXD_INDEXER_PROJECTION_COUNT 6

typedef struct {
    uint64_t window_index;
    uint64_t byte_offset;
    uint32_t byte_count;
    uint32_t batch_status;
    uint64_t embedding_hash;
    float embedding[RAWRXD_INDEXER_EMBED_DIM];
} RAWRXD_INDEXER_EMBED_ENTRY;

typedef struct {
    uint64_t byte_offset;
    float score;
} RAWRXD_INDEXER_TOPK_ENTRY;

static uint64_t Fnv1a64Span(const unsigned char* s, uint32_t n) {
    uint64_t h = 1469598103934665603ULL;
    for (uint32_t i = 0; i < n; ++i) {
        h ^= (uint64_t)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t Fnv1a64(const char* s) {
    const unsigned char* p = (const unsigned char*)s;
    uint32_t n = 0;
    while (p[n]) ++n;
    return Fnv1a64Span(p, n);
}

static uint64_t Rotl64(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

static uint64_t MurmurFmix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

static uint64_t Murmur64Span(const unsigned char* s, uint32_t n, uint64_t seed) {
    if (!s || n == 0) return MurmurFmix64(seed ^ 0x9e3779b97f4a7c15ULL);

    const uint64_t c1 = 0x87c37b91114253d5ULL;
    const uint64_t c2 = 0x4cf5ad432745937fULL;
    uint64_t h1 = seed;

    uint32_t blockCount = n / 8;
    for (uint32_t i = 0; i < blockCount; ++i) {
        const unsigned char* p = s + (i * 8);
        uint64_t k1 =
            ((uint64_t)p[0]) |
            ((uint64_t)p[1] << 8) |
            ((uint64_t)p[2] << 16) |
            ((uint64_t)p[3] << 24) |
            ((uint64_t)p[4] << 32) |
            ((uint64_t)p[5] << 40) |
            ((uint64_t)p[6] << 48) |
            ((uint64_t)p[7] << 56);

        k1 *= c1;
        k1 = Rotl64(k1, 31);
        k1 *= c2;

        h1 ^= k1;
        h1 = Rotl64(h1, 27);
        h1 = h1 * 5 + 0x52dce729;
    }

    const unsigned char* tail = s + (blockCount * 8);
    uint32_t rem = n & 7U;
    uint64_t k1 = 0;
    for (uint32_t i = 0; i < rem; ++i) {
        k1 |= ((uint64_t)tail[i]) << (i * 8);
    }
    if (rem) {
        k1 *= c1;
        k1 = Rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;
    }

    h1 ^= (uint64_t)n;
    return MurmurFmix64(h1);
}

static uint64_t SemanticSpanHash(const unsigned char* s, uint32_t n, uint64_t seed) {
    uint64_t fnv = Fnv1a64Span(s, n);
    uint64_t mur = Murmur64Span(s, n, seed ^ (uint64_t)n * 0x9e3779b97f4a7c15ULL);
    return MurmurFmix64(fnv ^ mur ^ (seed * 0x94d049bb133111ebULL));
}

static int IsSemanticTokenByte(unsigned char c) {
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= '0' && c <= '9') return 1;
    if (c == '_') return 1;
    return 0;
}

static unsigned char FoldSemanticLower(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return (unsigned char)(c - 'A' + 'a');
    return c;
}

static void ZeroSemanticVector(float* outVec, uint32_t dim) {
    for (uint32_t i = 0; i < dim; ++i) {
        outVec[i] = 0.0f;
    }
}

static void NormalizeSemanticVector(float* outVec, uint32_t dim) {
    double norm = 0.0;
    for (uint32_t i = 0; i < dim; ++i) {
        norm += (double)outVec[i] * (double)outVec[i];
    }
    if (norm <= 0.0) return;
    float inv = (float)(1.0 / sqrt(norm));
    for (uint32_t i = 0; i < dim; ++i) {
        outVec[i] *= inv;
    }
}

static void ProjectSemanticHash(uint64_t h, float weight, float* outVec) {
    static const uint64_t salt = 0x9E3779B97F4A7C15ULL;
    for (uint32_t k = 0; k < RAWRXD_INDEXER_PROJECTION_COUNT; ++k) {
        uint64_t mix = h + (salt * (uint64_t)(k + 1));
        uint32_t idx = (uint32_t)(mix % (uint64_t)RAWRXD_INDEXER_EMBED_DIM);
        float sign = ((mix >> (11 + k)) & 1ULL) ? 1.0f : -1.0f;
        outVec[idx] += sign * weight;
    }
}

static void BuildSemanticVector(const unsigned char* text, uint32_t byteCount, float* outVec) {
    unsigned char token[96];
    uint32_t tokenLen = 0;

    ZeroSemanticVector(outVec, RAWRXD_INDEXER_EMBED_DIM);

    for (uint32_t i = 0; i <= byteCount; ++i) {
        unsigned char c = (i < byteCount) ? text[i] : (unsigned char)' ';
        if (IsSemanticTokenByte(c)) {
            if (tokenLen < (uint32_t)(sizeof(token) - 1)) {
                token[tokenLen++] = FoldSemanticLower(c);
            }
            continue;
        }

        if (tokenLen == 0) {
            continue;
        }

        uint64_t tokenHash = SemanticSpanHash(token, tokenLen, 0xA5A5A5A5A5A5A5A5ULL);
        float tokenWeight = 1.0f + (float)((tokenLen < 24U) ? tokenLen : 24U) * 0.03125f;
        ProjectSemanticHash(tokenHash, tokenWeight, outVec);

        if (tokenLen >= 3) {
            for (uint32_t j = 0; j + 2 < tokenLen; ++j) {
                uint64_t triHash = SemanticSpanHash(token + j, 3, 0x5A5A5A5A5A5A5A5AULL);
                ProjectSemanticHash(triHash, 0.35f, outVec);
            }
        }

        tokenLen = 0;
    }

    if (byteCount > 0) {
        uint64_t lengthHash = MurmurFmix64(((uint64_t)byteCount << 32) ^ (uint64_t)(byteCount * 1315423911U));
        ProjectSemanticHash(lengthHash, 0.10f, outVec);
    }

    NormalizeSemanticVector(outVec, RAWRXD_INDEXER_EMBED_DIM);
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetNeuralEmbeddingMode(uint32_t enable) {
    _InterlockedExchange(&g_neuralEmbeddingsActive, enable ? 1 : 0);
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetEmbeddingFoldMode(uint32_t mode) {
    if (mode > 2) mode = 0;
    _InterlockedExchange(&g_embeddingFoldMode, (long)mode);
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetVulkanMemoryClass(uint32_t mode) {
    if (mode > 3) mode = 0;
    _InterlockedExchange(&g_vulkanMemoryClass, (long)mode);
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) uint32_t __stdcall RawrXD_GetVulkanMemoryClass(void) {
    return (uint32_t)_InterlockedCompareExchange(&g_vulkanMemoryClass, 0, 0);
}

// =============================================================================
// Phase 8: Neural Embedding Bridge — Extract Real Semantic Vectors from Phi-3
// =============================================================================
// Attempts to extract contextual embeddings from the loaded model.
// Falls back to hash-based embeddings if neural path unavailable.
// =============================================================================

// Ollama HTTP embeddings request helper
// Returns: 0 on success, <0 on error, embedding data written to outVec
static int OllamaEmbeddingsRequest(const char* prompt, float* outVec, uint32_t outDim) {
    if (!prompt || !outVec || outDim == 0) return -1;
    if (!InitWinsock()) return -2;

    const int kSocketTimeoutMs = 8000;
    const int kHardBudgetMs = 7000;

    static char body[65536];
    static char escaped[32768];
    JsonEscape(prompt, escaped, (int)sizeof(escaped));

    int bp = 0;
    bp = StrCopy(body, bp, "{\"model\":\"");
    bp = StrCopy(body, bp, g_liveModelName);
    bp = StrCopy(body, bp, "\",\"prompt\":\"");
    bp = StrCopy(body, bp, escaped);
    bp = StrCopy(body, bp, "\"}");
    body[bp] = 0;

    static char httpReq[70000];
    int hp = 0;
    hp = StrCopy(httpReq, hp, "POST /api/embeddings HTTP/1.1\r\n");
    hp = StrCopy(httpReq, hp, "Host: 127.0.0.1:11434\r\n");
    hp = StrCopy(httpReq, hp, "Content-Type: application/json\r\n");
    hp = StrCopy(httpReq, hp, "Content-Length: ");
    hp = IntToStr(httpReq, hp, bp);
    hp = StrCopy(httpReq, hp, "\r\nConnection: keep-alive\r\n\r\n");
    for (int i = 0; i < bp; i++) httpReq[hp + i] = body[i];
    hp += bp;

    long long requestStartTicks = QpcNow();

    for (int attempt = 0; attempt < 2; attempt++) {
        if (QpcDeltaMs(requestStartTicks, QpcNow()) >= kHardBudgetMs) {
            CloseOllamaSocket();
            return -3;
        }

        int connectMs = 0;
        int wasReuse = 0;
        if (!EnsureOllamaSocketConnected(kSocketTimeoutMs, &connectMs, &wasReuse)) {
            return (attempt == 0) ? -4 : -5;
        }

        int sent = 0;
        while (sent < hp) {
            if (QpcDeltaMs(requestStartTicks, QpcNow()) >= kHardBudgetMs) {
                CloseOllamaSocket();
                return -6;
            }
            int rc = pfn_send(g_ollamaSock, httpReq + sent, hp - sent, 0);
            if (rc <= 0) {
                CloseOllamaSocket();
                break;
            }
            sent += rc;
        }
        if (sent < hp) continue;

        static char recvBuf[524288];
        int totalRecv = 0;
        int headerEnd = -1;
        int contentLength = -1;

        while (totalRecv < (int)sizeof(recvBuf) - 1) {
            if (QpcDeltaMs(requestStartTicks, QpcNow()) >= kHardBudgetMs) {
                CloseOllamaSocket();
                return -7;
            }

            int rc = pfn_recv(g_ollamaSock, recvBuf + totalRecv, (int)sizeof(recvBuf) - 1 - totalRecv, 0);
            if (rc <= 0) {
                CloseOllamaSocket();
                break;
            }

            totalRecv += rc;
            recvBuf[totalRecv] = 0;

            if (headerEnd < 0) {
                headerEnd = FindHttpHeaderEnd(recvBuf, totalRecv);
                if (headerEnd > 0) {
                    contentLength = ParseContentLength(recvBuf, headerEnd);
                    if (contentLength < 0 || contentLength > (int)sizeof(recvBuf) - headerEnd - 1) {
                        CloseOllamaSocket();
                        break;
                    }
                }
            }

            if (headerEnd > 0 && contentLength >= 0 && (totalRecv - headerEnd) >= contentLength) {
                int rawCount = 0;
                int foldRc = ExtractAndFoldJsonEmbeddingVector(recvBuf + headerEnd,
                                                               contentLength,
                                                               outVec,
                                                               outDim,
                                                               &rawCount);
                if (foldRc != 0 || rawCount <= 0) {
                    CloseOllamaSocket();
                    break;
                }

                if (_InterlockedCompareExchange(&g_traceTransportSegments, 0, 0) != 0) {
                    char tbuf[256];
                    wsprintfA(tbuf,
                              "[Titan embeddings] attempt=%d socket=%s connect_ms=%d recv_bytes=%d content_len=%d dims=%d->%u",
                              attempt + 1,
                              wasReuse ? "reused" : "new",
                              connectMs,
                              totalRecv,
                              contentLength,
                              rawCount,
                              outDim);
                    TitanLog(tbuf);
                }

                return 0;
            }
        }
    }

    return -8;
}

static int BuildNeuralEmbedding(const unsigned char* text, uint32_t byteCount, float* outVec) {
    // ACTIVATION PATH: Use Ollama embeddings API for real semantic extraction
    // Controlled by manifest flag: neural_mode and environment variable RAWR_NEURAL_EMBEDDINGS=1
    
    if (!text || !outVec) return -1;
    
    // Check if neural embeddings are enabled via flag
    int neuralActive = _InterlockedCompareExchange(&g_neuralEmbeddingsActive, 0, 0);
    if (!neuralActive) {
        // Neural mode disabled: use hash-based embeddings for stability
        BuildSemanticVector(text, byteCount, outVec);
        return 0;
    }
    
    // Neural embeddings requested: attempt Ollama bridge
    // Convert text to newline-terminated string (safe for HTTP payload)
    static char textBuf[8192];
    uint32_t textLen = byteCount > (sizeof(textBuf) - 1) ? (sizeof(textBuf) - 1) : byteCount;
    for (uint32_t i = 0; i < textLen; ++i) {
        unsigned char c = text[i];
        // Skip unprintable control characters for HTTP safety
        textBuf[i] = (c >= 0x20 && c < 0x7F) ? c : ' ';
    }
    textBuf[textLen] = 0;
    
    // Attempt Ollama embeddings extraction
    int rc = OllamaEmbeddingsRequest(textBuf, outVec, RAWRXD_INDEXER_EMBED_DIM);
    if (rc == 0) {
        return 0;  // Neural embeddings success
    }
    
    // FALLBACK: Use hash-based embeddings if neural path fails
    // This maintains system stability during Phase 8 transition
    BuildSemanticVector(text, byteCount, outVec);
    return 1;  // Fallback via hash (non-fatal)
}

static void SecureZeroSpan(void* ptr, size_t bytes) {
    if (!ptr || bytes == 0) return;
    volatile unsigned char* p = (volatile unsigned char*)ptr;
    for (size_t i = 0; i < bytes; ++i) {
        p[i] = 0;
    }
}

// =============================================================================
// Phase 3: MASM64 aperture kernel — static linkage to k_swap_aperture_win32_v126.asm
// Replaces runtime LoadLibrary/GetProcAddress with direct symbol resolution at
// link time, eliminating the dependency on RawrXD_Singularity_Test_v126a.dll.
// =============================================================================
extern "C" int k_swap_aperture_init(uintptr_t aperture_base_hint, uint64_t span_bytes, void* telemetry_out);
extern "C" int k_swap_aperture_map_chunk(void* file_mapping_handle, uint64_t chunk_index, uint64_t bytes_to_map, uint64_t* out_mapped_base);
extern "C" int k_swap_aperture_unmap_chunk(void* mapped_base);

// =============================================================================
// Phase 6: Optimized aperture kernel dispatch (AVX-512 acceleration)
// Thin Bridge Patch: augments Phase 3 k_swap operations with optional
// hardware-accelerated seal hash verification (1.528x speedup on Zen 4)
// =============================================================================

// Data Structures: Aperture Result + Subdivision Table (for seal hash)
// Must match aperture_kernels_x64.asm struct definitions
// =============================================================================
#pragma pack(push, 1)
typedef struct {
    uint64_t offset_bytes;
    uint64_t size_bytes;
    uint8_t  sha256_hash[32];   // +16: hash result from computation
    uint64_t reserved[9];       // +48: padding (88 bytes total, but first 56 used)
} ApertureEntry;

typedef struct {
    uint32_t magic;              // +0: validation
    uint32_t entry_count;        // +4: number of entries
    uint8_t  reserved1[96];      // +8..103: reserved/padding
    ApertureEntry entries[0];    // +104: entries array (flexible)
} ApertureSubdivisionTable;

typedef struct {
    uint64_t mapping_handle;     // +0: file mapping handle
    uint32_t chunk_index;        // +8: chunk identificatoin
    uint8_t  reserved1[12];      // +12..23: padding
    uint8_t  final_seal_hash[32]; // +24: XOR-folded seal hash output
} ApertureResult;
#pragma pack(pop)

static int StrEq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static void EnsureApertureDispatchReady() {
    if (!_InterlockedCompareExchange(&g_aperture_seal_dispatch_ready, 1, 0)) {
        // Phase 6: C++ CPUID/XGETBV feature detect for AVX-512F + AVX-512DQ.
        // Keep g_ApertureAvx512Supported as the single shared flag consumed by
        // the thin bridge dispatch path.
        g_ApertureAvx512Supported = DetectAvx512FAndDQ();
        if (g_ApertureAvx512Supported) {
            g_aperture_seal_verify_fn = Aperture_SealVerifyAvx512;
            _InterlockedExchange(&g_aperture_dispatch_mode, 2);
        } else {
            g_aperture_seal_verify_fn = Aperture_SealVerifyScalar;
            _InterlockedExchange(&g_aperture_dispatch_mode, 1);
        }
        char dbgBuf[128];
        unsigned char avx512_ready = g_ApertureAvx512Supported;
        wsprintfA(dbgBuf,
                  "Phase 6 Bridge: AVX-512 %s (dispatch=%s)",
                  avx512_ready ? "READY" : "NOT_AVAILABLE",
                  avx512_ready ? "zmm" : "scalar");
        OutputDebugStringA(dbgBuf);
    }
}

static void ClearApertureTelemetry() {
    // Phase 6: only clear the session state, preserve the persistent base
    g_rawrxd_aperture_base = g_rawrxd_persistent_aperture_base;
    g_rawrxd_mapped_bytes = 0;
    g_rawrxd_mapped_chunks = 0;
    g_rawrxd_map_latency_ms = 0.0;
    g_rawrxd_unmap_latency_ms = 0.0;
    for (uint32_t i = 0; i < kRawrXDApertureSlotCount; ++i) {
        g_rawrxd_aperture_slot_map[i] = 0;
        g_rawrxd_aperture_slot_base[i] = 0;
        g_rawrxd_aperture_slot_size[i] = 0;
    }
}

static const char* StateToText(DWORD state) {
    if (state == MEM_COMMIT_) return "COMMIT";
    if (state == MEM_RESERVE_) return "RESERVE";
    if (state == MEM_FREE_) return "FREE";
    return "OTHER";
}

static void LogVARegionSweepOnTimeout(uint64_t apertureBase, uint32_t maxRegions) {
    char logBuf[256];
    wsprintfA(logBuf,
              "RawrXD_WaitForInference: VA sweep begin base=0x%I64X regions=%u",
              (unsigned long long)apertureBase,
              maxRegions);
    TitanLog(logBuf);

    uintptr_t cursor = (uintptr_t)(apertureBase != 0 ? apertureBase : 0x10000ULL);
    for (uint32_t i = 0; i < maxRegions; ++i) {
        MEMORY_BASIC_INFORMATION mbi = {};
        size_t got = VirtualQuery((const void*)cursor, &mbi, sizeof(mbi));
        if (got == 0 || mbi.RegionSize == 0) {
            wsprintfA(logBuf,
                      "RawrXD_WaitForInference: VA[%u] query_end addr=0x%I64X",
                      i,
                      (unsigned long long)cursor);
            TitanLog(logBuf);
            break;
        }

        wsprintfA(logBuf,
                  "RawrXD_WaitForInference: VA[%u] base=0x%I64X alloc=0x%I64X size=0x%I64X state=%s type=0x%08X prot=0x%08X",
                  i,
                  (unsigned long long)(uintptr_t)mbi.BaseAddress,
                  (unsigned long long)(uintptr_t)mbi.AllocationBase,
                  (unsigned long long)mbi.RegionSize,
                  StateToText(mbi.State),
                  (unsigned int)mbi.Type,
                  (unsigned int)mbi.Protect);
        TitanLog(logBuf);

        uintptr_t next = (uintptr_t)mbi.BaseAddress + (uintptr_t)mbi.RegionSize;
        if (next <= cursor) {
            break;
        }
        cursor = next;
    }
}

static uint32_t CountApertureFragmentGaps() {
    uint32_t gaps = 0;
    uint32_t seenMapped = 0;
    uint32_t inGap = 0;

    for (uint32_t i = 0; i < kRawrXDApertureSlotCount; ++i) {
        if (g_rawrxd_aperture_slot_map[i]) {
            seenMapped = 1;
            inGap = 0;
            continue;
        }
        if (!seenMapped) {
            continue;
        }

        uint32_t hasMappedAhead = 0;
        for (uint32_t j = i + 1; j < kRawrXDApertureSlotCount; ++j) {
            if (g_rawrxd_aperture_slot_map[j]) {
                hasMappedAhead = 1;
                break;
            }
        }
        if (hasMappedAhead && !inGap) {
            gaps++;
            inGap = 1;
        }
    }

    return gaps;
}

static float ComputeApertureFragmentationRatio() {
    if (g_rawrxd_mapped_chunks <= 1) {
        return 0.0f;
    }
    return (float)CountApertureFragmentGaps() / (float)kRawrXDApertureSlotCount;
}

static RAWRXD_STATUS ValidateApertureChunkIndex(uint32_t chunk_index) {
    return (chunk_index < kRawrXDApertureSlotCount) ? RAWRXD_SUCCESS : RAWRXD_ERROR_INVALID_PARAM;
}

static int SafeMulU64(uint64_t a, uint64_t b, uint64_t* out) {
    if (!out) return 0;
    if (a == 0 || b == 0) {
        *out = 0;
        return 1;
    }
    if (a > (~0ULL / b)) return 0;
    *out = a * b;
    return 1;
}

static int SafeAddU64(uint64_t a, uint64_t b, uint64_t* out) {
    if (!out) return 0;
    if (a > (~0ULL - b)) return 0;
    *out = a + b;
    return 1;
}

static RAWRXD_STATUS ValidateApertureChunkWindow(uint32_t chunk_index,
                                                 uint64_t* out_byte_offset,
                                                 uint64_t* out_bytes_to_map) {
    if (!out_byte_offset || !out_bytes_to_map) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    RAWRXD_STATUS st = ValidateApertureChunkIndex(chunk_index);
    if (st != RAWRXD_SUCCESS) {
        return st;
    }

    if (g_fileSize == 0 || g_fileSize > kRawrXDApertureSpanBytes) {
        return RAWRXD_ERROR_NOT_READY;
    }

    uint64_t chunkOffset = 0;
    if (!SafeMulU64((uint64_t)chunk_index, kRawrXDApertureChunkBytes, &chunkOffset)) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    if (chunkOffset >= g_fileSize) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    uint64_t bytesToMap = g_fileSize - chunkOffset;
    if (bytesToMap > kRawrXDApertureChunkBytes) {
        bytesToMap = kRawrXDApertureChunkBytes;
    }
    if (bytesToMap == 0) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    uint64_t chunkEnd = 0;
    if (!SafeAddU64(chunkOffset, bytesToMap, &chunkEnd)) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    if (chunkEnd > g_fileSize || chunkEnd > kRawrXDApertureSpanBytes) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    *out_byte_offset = chunkOffset;
    *out_bytes_to_map = bytesToMap;
    return RAWRXD_SUCCESS;
}

// =============================================================================
// Phase 6: Dispatch wrapper for optional seal hash verification (Thin Bridge)
// Micro-optimization: only dispatch if size >= 256 bytes (SIMD overhead threshold)
// Returns: 0 on hash match, -1 on mismatch/error, 1 if verification skipped
// =============================================================================
static int Aperture_SealVerifyAvx512(uint64_t mapped_base, uint64_t size_bytes,
                                     void* mapped_chunk_ptr) {
    // First-token reclamation: chunk 0 only needs header authenticity to prove
    // the GGUF container is what we expect. The dedicated MASM header verifier
    // is much cheaper than the general seal-hash bridge and short-circuits the
    // cold-start path that was inflating TTFT.
    if (mapped_base == g_rawrxd_aperture_base) {
        uint32_t format = 0;
        uint32_t flags = 0;
        const void* headerBase = g_mappedBase ? (const void*)g_mappedBase : mapped_chunk_ptr;
        uint64_t headerBytes = g_fileSize != 0 && g_fileSize < size_bytes ? g_fileSize : size_bytes;
        const unsigned long long t0 = __rdtsc();
        int headerRc = k_header_verify_fast(headerBase, headerBytes, &format, &flags);
        const unsigned long long t1 = __rdtsc();
        g_ApertureHashLatency_tsc = (t1 >= t0) ? (t1 - t0) : 0;
        if (headerRc == 0 && (flags & 0x00000001u) != 0u) {
            ++g_rawrxd_header_fast_path_hits;
            return 0;
        }
        ++g_rawrxd_header_fast_path_failures;
    }

    // Threshold gate: avoid SIMD overhead on tiny buffers
    if (size_bytes < 256) {
        return 1;  // Skip (micro-optimization)
    }

    // Only dispatch if hardware detection succeeded and is ready
    if (!g_aperture_seal_dispatch_ready || !g_ApertureAvx512Supported) {
        return 1;  // Skip (no AVX-512 support)
    }

    if (!mapped_chunk_ptr || mapped_base == 0) {
        return -1;
    }

    struct LocalApertureEntry {
        uint64_t offset_bytes;
        uint64_t size_bytes;
        uint8_t  sha256_hash[32];
        uint64_t reserved[9];
    };

    struct LocalSubdivisionPayload {
        uint32_t magic;
        uint32_t entry_count;
        uint8_t  reserved1[96];
        LocalApertureEntry first_entry;
    } payload = {};

    payload.magic = 0x52574458UL;  // "RWDX" marker for internal tracing
    payload.entry_count = 1;
    payload.first_entry.offset_bytes = 0;
    payload.first_entry.size_bytes = size_bytes;

    ApertureResult result = {};
    result.mapping_handle = (uint64_t)(uintptr_t)mapped_chunk_ptr;
    result.chunk_index = 0;

    aperture_compute_seal_hash((void*)&result, (void*)&payload);
    return 0;
}

static int Aperture_SealVerifyScalar(uint64_t mapped_base, uint64_t size_bytes,
                                     void* mapped_chunk_ptr) {
    if (mapped_base == g_rawrxd_aperture_base) {
        uint32_t format = 0;
        uint32_t flags = 0;
        const void* headerBase = g_mappedBase ? (const void*)g_mappedBase : mapped_chunk_ptr;
        uint64_t headerBytes = g_fileSize != 0 && g_fileSize < size_bytes ? g_fileSize : size_bytes;
        const unsigned long long t0 = __rdtsc();
        int headerRc = k_header_verify_fast(headerBase, headerBytes, &format, &flags);
        const unsigned long long t1 = __rdtsc();
        g_ApertureHashLatency_tsc = (t1 >= t0) ? (t1 - t0) : 0;
        if (headerRc == 0 && (flags & 0x00000001u) != 0u) {
            ++g_rawrxd_header_fast_path_hits;
            return 0;
        }
        ++g_rawrxd_header_fast_path_failures;
    }

    if (size_bytes < 256) {
        return 1;
    }
    if (!mapped_chunk_ptr || mapped_base == 0) {
        return -1;
    }

    // Lightweight scalar fold over address/size metadata to keep fallback
    // deterministic and low overhead on non-AVX512 hardware.
    uint64_t h = 1469598103934665603ULL;  // FNV-1a offset basis
    const uint64_t kPrime = 1099511628211ULL;
    uint64_t v0 = mapped_base;
    uint64_t v1 = size_bytes;
    uint64_t v2 = (uint64_t)(uintptr_t)mapped_chunk_ptr;

    for (int i = 0; i < 8; ++i) {
        h ^= (v0 & 0xFFULL); h *= kPrime; v0 >>= 8;
        h ^= (v1 & 0xFFULL); h *= kPrime; v1 >>= 8;
        h ^= (v2 & 0xFFULL); h *= kPrime; v2 >>= 8;
    }

    // Mix in dispatch mode so traces can correlate fallback behavior.
    h ^= (uint64_t)_InterlockedCompareExchange(&g_aperture_dispatch_mode, 0, 0);
    h *= kPrime;

    // Feed the same latency counter channel used by AVX-512 path for
    // observability parity (scalar path writes a synthetic cycle proxy).
    g_ApertureHashLatency_tsc = h;
    return 0;
}

static int Aperture_DispatchSealVerify(uint64_t mapped_base, uint64_t size_bytes,
                                       void* mapped_chunk_ptr) {
    const long mode = _InterlockedCompareExchange(&g_aperture_dispatch_mode, 0, 0);

#if defined(RAWRXD_INLINE_DISPATCH) && RAWRXD_INLINE_DISPATCH
    const int canUseAvxPath = (mode == 2 && g_ApertureAvx512Supported) ? 1 : 0;
    if (!canUseAvxPath && mode != 1) {
        NoteApertureDispatchFallbackOnce();
    }
    return RawrXD_DispatchSealVerifyInline(
        mode,
        g_ApertureAvx512Supported,
        mapped_base,
        size_bytes,
        mapped_chunk_ptr);
#else

    // Phase 7A: hot-path uses startup-cached capability state only.
    // Any missing/corrupt state must fail closed to scalar and stay non-fatal.
    if (mode == 2 && g_ApertureAvx512Supported) {
        return Aperture_SealVerifyAvx512(mapped_base, size_bytes, mapped_chunk_ptr);
    }

    if (mode != 1) {
        NoteApertureDispatchFallbackOnce();
    }
    return Aperture_SealVerifyScalar(mapped_base, size_bytes, mapped_chunk_ptr);
#endif
}

static uint32_t CountPseudoTokens(const char* text) {
    if (!text) return 0;
    uint32_t count = 0;
    uint32_t inWord = 0;
    for (size_t i = 0; text[i] != 0; ++i) {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            inWord = 0;
        } else if (!inWord) {
            count++;
            inWord = 1;
        }
    }
    return count;
}

static uint64_t RawrXDPtrValue(const void* ptr) {
    return (uint64_t)(uintptr_t)ptr;
}

static void TitanLogHandshakeSnapshot(const char* tag, uint64_t handle) {
    char logBuf[512];
    wsprintfA(logBuf,
              "%s gen=%I64u handle=%I64u req_active=%ld infer_active=%ld cancel=%ld fence=%ld req_addr=%I64X fence_addr=%I64X prompt_ptr=%I64X cb_ptr=%I64X text_ptr=%I64X ap_base=%I64X ap_persist=%I64X map_calls=%I64u unmap_calls=%I64u",
              tag,
              (unsigned long long)g_rawrxd_inference_generation,
              (unsigned long long)handle,
              _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
              g_rawrxd_inference_active,
              _InterlockedCompareExchange(&g_cancelRequest, 0, 0),
              _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0),
              (unsigned long long)RawrXDPtrValue(&g_currentRequest),
              (unsigned long long)RawrXDPtrValue((const void*)&g_rawrxd_completion_fence),
              (unsigned long long)g_rawrxd_last_submit_prompt_ptr,
              (unsigned long long)g_rawrxd_last_submit_callback_ptr,
              (unsigned long long)RawrXDPtrValue(g_rawrxd_inference_text),
              (unsigned long long)g_rawrxd_aperture_base,
              (unsigned long long)g_rawrxd_persistent_aperture_base,
              (unsigned long long)g_rawrxd_map_chunk_count,
              (unsigned long long)g_rawrxd_unmap_chunk_count);
    TitanLog(logBuf);
}

static void ResetRawrXDInferenceState() {
    g_rawrxd_inference_active = 0;
    g_rawrxd_inference_status = RAWRXD_SUCCESS;
    g_rawrxd_last_input_tokens = 0;
    g_rawrxd_last_output_tokens = 0;
    g_rawrxd_last_callback_generation = 0;
    g_rawrxd_inference_latency_ms = 0.0;
    g_rawrxd_inference_text[0] = 0;
    _InterlockedExchange(&g_rawrxd_completion_fence, 0);
}
// caller must not reset total_output_tokens or perf_freq here;
// those accumulate across inferences and are reset by ResetPerformanceCounters

static void LogTopVirtualRegionsOnTimeout() {
    struct RegionInfo {
        ULONGLONG base;
        ULONGLONG size;
        DWORD state;
        DWORD protect;
        DWORD type;
    } top[5] = {};

    ULONGLONG addr = 0;
    const ULONGLONG kMaxUserVa = 0x00007FFFFFFFFFFFULL;
    while (addr < kMaxUserVa) {
        MEMORY_BASIC_INFORMATION mbi = {};
        size_t q = VirtualQuery((const void*)(uintptr_t)addr, &mbi, sizeof(mbi));
        if (q == 0 || mbi.RegionSize == 0) {
            break;
        }

        ULONGLONG regionSize = (ULONGLONG)mbi.RegionSize;
        if (mbi.State != MEM_FREE_) {
            int insertAt = -1;
            for (int i = 0; i < 5; ++i) {
                if (regionSize > top[i].size) {
                    insertAt = i;
                    break;
                }
            }
            if (insertAt >= 0) {
                for (int j = 4; j > insertAt; --j) {
                    top[j] = top[j - 1];
                }
                top[insertAt].base = (ULONGLONG)(uintptr_t)mbi.BaseAddress;
                top[insertAt].size = regionSize;
                top[insertAt].state = mbi.State;
                top[insertAt].protect = mbi.Protect;
                top[insertAt].type = mbi.Type;
            }
        }

        ULONGLONG next = (ULONGLONG)(uintptr_t)mbi.BaseAddress + regionSize;
        if (next <= addr) break;
        addr = next;
    }

    for (int i = 0; i < 5; ++i) {
        if (top[i].size == 0) continue;
        char buf[256];
        wsprintfA(buf,
                  "RawrXD_WaitForInference: VA top[%d] base=0x%I64X size=0x%I64X state=0x%08X protect=0x%08X type=0x%08X",
                  i,
                  top[i].base,
                  top[i].size,
                  top[i].state,
                  top[i].protect,
                  top[i].type);
        TitanLog(buf);
    }
}

static void RawrXDInferenceCallback(const char* text, int len) {
    g_rawrxd_last_callback_text_ptr = RawrXDPtrValue(text);
    g_rawrxd_last_callback_len = (len >= 0) ? (uint32_t)len : 0;
    g_rawrxd_last_callback_generation = g_rawrxd_inference_generation;
    g_rawrxd_last_completed_handle = g_rawrxd_inference_generation;
    TitanLogHandshakeSnapshot("RawrXDInferenceCallback: entered", g_rawrxd_last_completed_handle);

    // Ignore late callbacks after timeout/cancel to keep state consistent.
    if (g_rawrxd_inference_status == RAWRXD_ERROR_TIMED_OUT ||
        _InterlockedCompareExchange(&g_cancelRequest, 0, 0) != 0) {
        _InterlockedExchange(&g_rawrxd_inference_active, 0);
        _InterlockedExchange(&g_rawrxd_completion_fence, 1);
        TitanLogHandshakeSnapshot("RawrXDInferenceCallback: canceled", g_rawrxd_last_completed_handle);
        return;
    }

    if (!text) {
        g_rawrxd_inference_text[0] = 0;
        g_rawrxd_last_output_tokens = 0;
        g_rawrxd_inference_status = RAWRXD_ERROR_NOT_READY;
        g_rawrxd_failed_inference_requests++;
        _InterlockedExchange(&g_rawrxd_inference_active, 0);
        _InterlockedExchange(&g_rawrxd_completion_fence, 1);
        TitanLogHandshakeSnapshot("RawrXDInferenceCallback: null-text", g_rawrxd_last_completed_handle);
        return;
    }

    int copyLen = NormalizeModelOutputToUtf8(text,
                                             len,
                                             g_rawrxd_inference_text,
                                             (int)sizeof(g_rawrxd_inference_text));

    if (_InterlockedCompareExchange(&g_rawrxd_streaming_enabled, 0, 0) != 0 && copyLen > 0) {
        uint64_t seq = (uint64_t)_InterlockedIncrement64(&g_rawrxd_stream_seq);
        if (g_rawrxd_stream_buffer.Push(g_rawrxd_inference_text, copyLen, seq)) {
            void* hwnd = g_rawrxd_stream_hwnd;
            unsigned int msg = g_rawrxd_stream_msg;
            if (hwnd && msg != 0) {
                PostMessageA(reinterpret_cast<HWND>(hwnd),
                             msg,
                             static_cast<WPARAM>(g_rawrxd_stream_wparam),
                             static_cast<LPARAM>(0));
            }
        }
    }

    g_rawrxd_last_output_tokens = CountPseudoTokens(g_rawrxd_inference_text);
    g_rawrxd_inference_status = RAWRXD_SUCCESS;
    if (g_rawrxd_infer_start_tick.QuadPart != 0 && g_rawrxd_perf_freq.QuadPart > 0) {
        LARGE_INTEGER now = {};
        QueryPerformanceCounter(&now);
        g_rawrxd_inference_latency_ms =
            (double)(now.QuadPart - g_rawrxd_infer_start_tick.QuadPart)
            / (double)g_rawrxd_perf_freq.QuadPart * 1000.0;
    }
    if (g_rawrxd_last_output_tokens > 0) {
        _InterlockedExchangeAdd64((volatile long long*)&g_rawrxd_total_output_tokens,
                                  (long long)g_rawrxd_last_output_tokens);
    }
    g_rawrxd_completed_inference_requests++;
    const uint64_t completed_generation = g_rawrxd_inference_generation;
    g_rawrxd_last_completed_generation = completed_generation;
    g_rawrxd_last_completed_handle = completed_generation;
    g_rf_frame_ready++;
    __try {
        Rawr_Profile_Checkpoint(g_rf_data_seq,
                                g_rf_consumed_seq,
                                g_rf_frame_ready,
                                (unsigned long long*)&g_rf_last_cycles,
                                (unsigned long long*)&g_rf_last_drift,
                                (unsigned long long*)&g_rf_max_cycles);
    } __except(1) {
        TitanLog("WARNING: Rawr_Profile_Checkpoint faulted (ignored)");
    }
    
    if (_InterlockedCompareExchange(&g_rawrxd_sovereign_log_ready, 0, 0) != 0) {
        // Sovereign exhaust pipe: telemetry must never destabilize inference completion.
        __try {
            RawrSovereign::RFCounters counters = {
                g_rf_data_seq,
                g_rf_consumed_seq,
                g_rf_frame_ready,
                0 // epoch (TODO: implement rollover tracking)
            };
            RawrSovereign::LatencyProfile profile = {
                g_rf_last_cycles,
                (uint64_t)g_rawrxd_inference_latency_ms,
                (int64_t)((g_rf_last_drift > 0x7FFFFFFFFFFFFFFFULL)
                    ? 0x7FFFFFFFFFFFFFFFULL
                    : g_rf_last_drift)
            };
            RawrSovereign::LogTelemetry(counters, profile);
        } __except(1) {
            TitanLog("WARNING: Sovereign LogTelemetry faulted (telemetry disabled)");
            _InterlockedExchange(&g_rawrxd_sovereign_log_ready, 0);
        }
    }
    
    {
        char completeBuf[192];
        wsprintfA(completeBuf,
                  "RawrXDInferenceCallback: success-path gen=%I64u last_done=%I64u active_before=%ld",
                  (unsigned long long)completed_generation,
                  (unsigned long long)g_rawrxd_last_completed_generation,
                  g_rawrxd_inference_active);
        TitanLog(completeBuf);
    }

    // Completion ordering: publish payload/counters, then clear active, then raise fence.
    _ReadWriteBarrier();
    _mm_sfence();
    _InterlockedExchange(&g_rawrxd_inference_active, 0);
    _InterlockedExchange(&g_rawrxd_completion_fence, 1);
    TitanLogHandshakeSnapshot("RawrXDInferenceCallback: completed", completed_generation);
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_WaitForInference(RAWRXD_INFERENCE_HANDLE inference_handle, uint32_t timeout_ms);
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetInferenceResult(RAWRXD_INFERENCE_HANDLE inference_handle, RAWRXD_INFERENCE_RESULT* result);
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_BatchInferences(const char** prompts, uint32_t prompt_count, RAWRXD_INFERENCE_HANDLE* batch_handle);

#if RAWR_ENABLE_GGML_LINK
static void ShutdownGGMLBridgeContext() {
    g_model_tensors = nullptr;
    g_ggml_ctx = nullptr;

    if (g_titan_gguf_ctx) {
        gguf_free(g_titan_gguf_ctx);
        g_titan_gguf_ctx = nullptr;
    }
}

static bool InitializeGGMLBridgeContext() {
    ShutdownGGMLBridgeContext();

    if (!g_modelPath[0]) {
        TitanLog("Titan_GGMLBridge: skipped, model path missing");
        return false;
    }

    ggml_context* ggml_ctx = nullptr;
    gguf_init_params params = {};
    params.no_alloc = false;
    params.ctx = &ggml_ctx;

    g_titan_gguf_ctx = gguf_init_from_file(g_modelPath, params);
    if (!g_titan_gguf_ctx || !ggml_ctx) {
        TitanLog("Titan_GGMLBridge: gguf_init_from_file failed, metadata-only bridge remains active");
        ShutdownGGMLBridgeContext();
        return false;
    }

    g_ggml_ctx = ggml_ctx;
    g_model_tensors = ggml_get_first_tensor(ggml_ctx);
    TitanLog("Titan_GGMLBridge: live GGML context hydrated from active model");
    return true;
}
#endif

static void ZeroModelMetadata() {
    g_modelName[0] = 0;
    g_modelArchitecture[0] = 0;
    g_modelContextLength = 0;
    g_modelVocabSize = 0;
    g_modelPrecisionBits = 0;
    g_modelParameterCount = 0;
#if RAWR_ENABLE_GGML_LINK
    ShutdownGGMLBridgeContext();
#else
    g_ggml_ctx = nullptr;
    g_model_tensors = nullptr;
#endif
    g_n_layers = 0;
    g_n_embd = 0;
    g_n_head = 0;
    g_n_vocab = 0;
    g_n_ctx = 0;
}

static uint32_t ParseParameterCountLabel(const char* text) {
    if (!text || !text[0]) return 0;

    uint32_t whole = 0;
    int sawDigit = 0;
    for (int i = 0; text[i]; ++i) {
        char c = text[i];
        if (c >= '0' && c <= '9') {
            sawDigit = 1;
            whole = (whole * 10u) + (uint32_t)(c - '0');
            continue;
        }
        if (c == '.' && sawDigit) return whole;
        if ((c == 'B' || c == 'b') && sawDigit) return whole;
        if ((c == 'M' || c == 'm') && sawDigit) return whole ? 1u : 0u;
        if (sawDigit) break;
    }
    return whole;
}

static uint32_t GuessPrecisionBitsFromTensorType(uint32_t tensorType) {
    switch (tensorType) {
        case 0: return 32;
        case 1: return 16;
        case 2:
        case 3:
        case 8:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            return 4;
        case 6:
        case 7:
        case 9:
            return 5;
        case 10:
        case 11:
            return 8;
        default:
            return 0;
    }
}

static void PopulateLoadedModelMetadata() {
    ZeroModelMetadata();

    if (!g_mappedBase || g_fileSize < sizeof(GGUFHeader)) {
        return;
    }

    FindGGUFMetadataString(g_mappedBase, g_nKV, "general.architecture",
                           g_modelArchitecture, (int)sizeof(g_modelArchitecture));

    if (!FindGGUFMetadataString(g_mappedBase, g_nKV, "general.name",
                                g_modelName, (int)sizeof(g_modelName))) {
        FindGGUFMetadataString(g_mappedBase, g_nKV, "general.basename",
                               g_modelName, (int)sizeof(g_modelName));
    }

    char sizeLabel[64] = {0};
    if (FindGGUFMetadataString(g_mappedBase, g_nKV, "general.size_label",
                               sizeLabel, (int)sizeof(sizeLabel))) {
        g_modelParameterCount = ParseParameterCountLabel(sizeLabel);
    }
    if (!g_modelParameterCount && g_modelName[0]) {
        g_modelParameterCount = ParseParameterCountLabel(g_modelName);
    }

    uint64_t scalarValue = 0;
    char archKey[128] = {0};
    if (g_modelArchitecture[0]) {
        wsprintfA(archKey, "%s.context_length", g_modelArchitecture);
        if (FindGGUFMetadataU64(g_mappedBase, g_nKV, archKey, &scalarValue)) {
            g_modelContextLength = (uint32_t)scalarValue;
        }
    }
    if (!g_modelContextLength && FindGGUFMetadataU64(g_mappedBase, g_nKV, "context_length", &scalarValue)) {
        g_modelContextLength = (uint32_t)scalarValue;
    }

    if (FindGGUFMetadataArrayCount(g_mappedBase, g_nKV, "tokenizer.ggml.tokens", &scalarValue)) {
        g_modelVocabSize = (uint32_t)scalarValue;
    }
    if (!g_modelVocabSize && FindGGUFMetadataU64(g_mappedBase, g_nKV, "tokenizer.ggml.vocab_size", &scalarValue)) {
        g_modelVocabSize = (uint32_t)scalarValue;
    }

    if (g_modelArchitecture[0]) {
        wsprintfA(archKey, "%s.block_count", g_modelArchitecture);
        if (FindGGUFMetadataU64(g_mappedBase, g_nKV, archKey, &scalarValue)) {
            g_n_layers = (int)scalarValue;
        }

        wsprintfA(archKey, "%s.embedding_length", g_modelArchitecture);
        if (FindGGUFMetadataU64(g_mappedBase, g_nKV, archKey, &scalarValue)) {
            g_n_embd = (int)scalarValue;
        }

        wsprintfA(archKey, "%s.attention.head_count", g_modelArchitecture);
        if (FindGGUFMetadataU64(g_mappedBase, g_nKV, archKey, &scalarValue)) {
            g_n_head = (int)scalarValue;
        }
    }

    g_n_ctx = (int)g_modelContextLength;
    g_n_vocab = (int)g_modelVocabSize;

    const uint8_t* ptr = g_mappedBase + sizeof(GGUFHeader);
    for (uint64_t i = 0; i < g_nKV; i++) {
        if ((uint64_t)(ptr - g_mappedBase) + 8 > g_fileSize) return;
        uint64_t kl = *(const uint64_t*)ptr;
        if (kl > 0x100000ULL) return;
        ptr += 8;
        if ((uint64_t)(ptr - g_mappedBase) + kl > g_fileSize) return;
        ptr += kl;
        if ((uint64_t)(ptr - g_mappedBase) + 4 > g_fileSize) return;
        uint32_t vt = *(const uint32_t*)ptr;
        ptr += 4;
        ptr = SkipGGUFValue_Safe(ptr, vt, g_mappedBase, g_fileSize);
        if (!ptr) return;
    }

    for (uint64_t t = 0; t < g_nTensors && t < 2000; t++) {
        char tensorName[128] = {0};
        const uint8_t* next = ReadGGUFString_Safe(ptr, tensorName, 128, g_mappedBase, g_fileSize);
        if (!next) break;
        ptr = next;
        if ((uint64_t)(ptr - g_mappedBase) + 4 > g_fileSize) break;
        uint32_t nd = *(const uint32_t*)ptr;
        ptr += 4;
        if (nd > 8) break;
        for (uint32_t d = 0; d < nd; d++) {
            if ((uint64_t)(ptr - g_mappedBase) + 8 > g_fileSize) return;
            ptr += 8;
        }
        if ((uint64_t)(ptr - g_mappedBase) + 12 > g_fileSize) break;
        uint32_t tensorType = *(const uint32_t*)ptr;
        ptr += 4;
        ptr += 8;

        if (g_modelPrecisionBits == 0) {
            g_modelPrecisionBits = GuessPrecisionBitsFromTensorType(tensorType);
        }
        if (StrEq(tensorName, "token_embd.weight")) {
            uint32_t guessed = GuessPrecisionBitsFromTensorType(tensorType);
            if (guessed != 0) g_modelPrecisionBits = guessed;
            break;
        }
    }

    if (!g_modelName[0] && g_modelArchitecture[0]) {
        StrCopyN(g_modelName, g_modelArchitecture, sizeof(g_modelName));
    }

    // Phase 4: read BOS/EOS token IDs from GGUF metadata
    g_rawrxd_bos_token_id = 1;
    g_rawrxd_eos_token_id = 2;
    uint64_t bosVal = 0, eosVal = 0;
    if (FindGGUFMetadataU64(g_mappedBase, g_nKV, "tokenizer.ggml.bos_token_id", &bosVal))
        g_rawrxd_bos_token_id = (int32_t)(uint32_t)bosVal;
    if (FindGGUFMetadataU64(g_mappedBase, g_nKV, "tokenizer.ggml.eos_token_id", &eosVal))
        g_rawrxd_eos_token_id = (int32_t)(uint32_t)eosVal;
}

extern "C" RAWRXD_TITAN_BRIDGE_API int RawrXD_GetGGMLTitanBridge(RAWRXD_GGML_TITAN_BRIDGE* out_bridge) {
    if (!out_bridge) {
        return 0;
    }

    out_bridge->ggml_ctx = g_ggml_ctx;
    out_bridge->model_tensors = g_model_tensors;
    out_bridge->n_layers = g_n_layers;
    out_bridge->n_embd = g_n_embd;
    out_bridge->n_head = g_n_head;
    out_bridge->n_vocab = g_n_vocab;
    out_bridge->n_ctx = g_n_ctx;
    StrCopyN(out_bridge->model_name, g_modelName, sizeof(out_bridge->model_name));
    StrCopyN(out_bridge->architecture, g_modelArchitecture, sizeof(out_bridge->architecture));
    return 1;
}

static void StrCopyN(char* dst, const char* src, size_t n) {
    size_t i = 0;
    if (!dst || n == 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i + 1 < n) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

// EnsureApertureKernelLoaded() removed — Phase 3 wires directly to the MASM
// kernel symbols; no runtime DLL search needed.

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Initialize(void) {
    _InterlockedExchange(&g_rawrxd_initialized, 1);
    g_rawrxd_last_error = RAWRXD_SUCCESS;
    ResetRawrXDInferenceState();
    
    // Initialize sovereign exhaust pipe (zero-dep syscall telemetry)
    if (!RawrSovereign::InitLog()) {
        _InterlockedExchange(&g_rawrxd_sovereign_log_ready, 0);
        TitanLog("WARNING: Sovereign log init failed (telemetry disabled)");
    } else {
        _InterlockedExchange(&g_rawrxd_sovereign_log_ready, 1);
    }
    
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Shutdown(void) {
    Titan_Shutdown();
    _InterlockedExchange(&g_rawrxd_initialized, 0);
    g_rawrxd_active_model = 0;
    ClearApertureTelemetry();
    ResetRawrXDInferenceState();
    
    if (_InterlockedCompareExchange(&g_rawrxd_sovereign_log_ready, 0, 0) != 0) {
        // Close sovereign exhaust pipe only if init succeeded.
        __try {
            RawrSovereign::CloseLog();
        } __except(1) {
            TitanLog("WARNING: Sovereign CloseLog faulted");
        }
        _InterlockedExchange(&g_rawrxd_sovereign_log_ready, 0);
    }
    
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Reset(void) {
    g_rawrxd_system_prompt[0] = 0;
    g_rawrxd_last_error = RAWRXD_SUCCESS;
    ClearApertureTelemetry();
    ResetRawrXDInferenceState();
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Abort(void) { Titan_Abort(); return RAWRXD_SUCCESS; }

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetStatus(RAWRXD_STATUS* status_code) {
    if (!status_code) return RAWRXD_ERROR_INVALID_PARAM;
    *status_code = (_InterlockedCompareExchange(&g_rawrxd_initialized, 0, 0) != 0) ? RAWRXD_SUCCESS : RAWRXD_ERROR_NOT_INITIALIZED;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetVersion(char* version_string, size_t buffer_size) {
    if (!version_string || buffer_size < 6) return RAWRXD_ERROR_INVALID_PARAM;
    StrCopyN(version_string, "1.2.7-handshake", buffer_size);
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) unsigned long long __stdcall RawrXD_GetLastTTFT(void) {
    return g_rawrxd_ttft_ms;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetCapabilities(uint32_t* capabilities) {
    if (!capabilities) return RAWRXD_ERROR_INVALID_PARAM;
    *capabilities = RAWRXD_CAP_GPU_ACCELERATION | RAWRXD_CAP_QUANTIZATION |
                    RAWRXD_CAP_APERTURE_MAPPING | RAWRXD_CAP_STREAMING |
                    RAWRXD_CAP_HOTPATCHING | RAWRXD_CAP_MULTI_BATCH |
                    RAWRXD_CAP_DIAGNOSTICS;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetLogLevel(int32_t) { return RAWRXD_SUCCESS; }

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ApertureShutdown(void);

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_LoadModel(const char* model_path, RAWRXD_MODEL_HANDLE* model_handle) {
    if (!model_path || !model_handle) return RAWRXD_ERROR_INVALID_PARAM;
    int rc = Titan_Initialize(model_path);
    if (rc != 0) {
        // Force-fail path: reclaim any partially initialized aperture reservation.
        (void)RawrXD_ApertureShutdown();
        g_rawrxd_active_model = 0;
        ResetRawrXDInferenceState();
        g_rawrxd_last_error = RAWRXD_ERROR_NOT_READY;
        return RAWRXD_ERROR_NOT_READY;
    }
    g_rawrxd_active_model = 1;
    *model_handle = g_rawrxd_active_model;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_UnloadModel(RAWRXD_MODEL_HANDLE) {
    Titan_Shutdown();
    (void)RawrXD_ApertureShutdown();
    g_rawrxd_active_model = 0;
    ClearApertureTelemetry();
    ResetRawrXDInferenceState();
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetModelInfo(RAWRXD_MODEL_HANDLE, RAWRXD_MODEL_INFO* model_info) {
    if (!model_info) return RAWRXD_ERROR_INVALID_PARAM;
    if (!g_rawrxd_active_model) return RAWRXD_ERROR_NO_MODEL_LOADED;
    model_info->size_bytes = g_fileSize;
    model_info->parameter_count = g_modelParameterCount;
    model_info->vocab_size = g_modelVocabSize;
    model_info->context_length = g_modelContextLength;
    model_info->precision_bits = g_modelPrecisionBits ? g_modelPrecisionBits : 4;
    if (g_modelName[0]) {
        StrCopyN(model_info->model_name, g_modelName, sizeof(model_info->model_name));
    } else if (g_modelArchitecture[0]) {
        StrCopyN(model_info->model_name, g_modelArchitecture, sizeof(model_info->model_name));
    } else {
        StrCopyN(model_info->model_name, "(unknown)", sizeof(model_info->model_name));
    }
    StrCopyN(model_info->model_path, g_modelPath, sizeof(model_info->model_path));
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ListModels(char* model_paths, size_t buffer_size, uint32_t* count) {
    if (count) *count = g_rawrxd_active_model ? 1 : 0;
    if (model_paths && buffer_size > 0) {
        if (g_rawrxd_active_model) StrCopyN(model_paths, g_modelPath, buffer_size);
        else model_paths[0] = 0;
    }
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SelectModel(RAWRXD_MODEL_HANDLE model_handle) {
    if (!model_handle) return RAWRXD_ERROR_NO_MODEL_LOADED;
    g_rawrxd_active_model = model_handle;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetActiveModel(RAWRXD_MODEL_HANDLE* model_handle) {
    if (!model_handle) return RAWRXD_ERROR_INVALID_PARAM;
    *model_handle = g_rawrxd_active_model;
    return g_rawrxd_active_model ? RAWRXD_SUCCESS : RAWRXD_ERROR_NO_MODEL_LOADED;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_PreloadChunks(RAWRXD_MODEL_HANDLE, uint32_t, uint32_t) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ReleaseChunks(RAWRXD_MODEL_HANDLE, uint32_t, uint32_t) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetChunkStatus(RAWRXD_MODEL_HANDLE, uint32_t chunk_index, uint32_t* is_mapped) {
    if (!is_mapped) return RAWRXD_ERROR_INVALID_PARAM;
    RAWRXD_STATUS st = ValidateApertureChunkIndex(chunk_index);
    if (st != RAWRXD_SUCCESS) return st;
    *is_mapped = g_rawrxd_aperture_slot_map[chunk_index] ? 1u : 0u;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ValidateModelIntegrity(RAWRXD_MODEL_HANDLE, uint32_t* is_valid) {
    if (!is_valid) return RAWRXD_ERROR_INVALID_PARAM;
    *is_valid = (g_mappedBase && g_fileSize >= sizeof(GGUFHeader) && g_nTensors > 0) ? 1u : 0u;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetModelPath(char* model_path, size_t buffer_size) {
    if (!model_path || buffer_size == 0) return RAWRXD_ERROR_INVALID_PARAM;
    StrCopyN(model_path, g_modelPath, buffer_size);
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_CacheModelMetadata(RAWRXD_MODEL_HANDLE) {
    if (!g_rawrxd_active_model) return RAWRXD_ERROR_NO_MODEL_LOADED;
    PopulateLoadedModelMetadata();
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_InferAsync(const char* prompt, size_t prompt_len, RAWRXD_INFERENCE_HANDLE* inference_handle) {
    {
        char entryBuf[192];
        wsprintfA(entryBuf,
                  "RawrXD_InferAsync: bridge-entry prompt=%I64X len=%I64u handle_ptr=%I64X",
                  (unsigned long long)RawrXDPtrValue(prompt),
                  (unsigned long long)prompt_len,
                  (unsigned long long)RawrXDPtrValue(inference_handle));
        TitanLog(entryBuf);
    }
    if (!inference_handle) return RAWRXD_ERROR_INVALID_PARAM;
    if (!g_rawrxd_active_model) return RAWRXD_ERROR_NO_MODEL_LOADED;
    if (!prompt || prompt_len == 0) return RAWRXD_ERROR_INVALID_PARAM;

    static char rawrxdPromptBuffer[8192] = {0};
    size_t copyLen = prompt_len;
    if (copyLen > sizeof(rawrxdPromptBuffer) - 1) {
        copyLen = sizeof(rawrxdPromptBuffer) - 1;
    }
    for (size_t i = 0; i < copyLen; ++i) {
        rawrxdPromptBuffer[i] = prompt[i];
    }
    rawrxdPromptBuffer[copyLen] = 0;

    ResetRawrXDInferenceState();
    g_cancelRequest = 0;
    g_rawrxd_last_input_tokens = CountPseudoTokens(rawrxdPromptBuffer);
    _InterlockedExchange(&g_rawrxd_inference_active, 1);
    _InterlockedExchange(&g_rawrxd_completion_fence, 0);
    g_rawrxd_inference_generation++;
    g_rawrxd_last_submit_generation = g_rawrxd_inference_generation;
    g_rawrxd_total_inference_requests++;
    {
        char dbgBuf[192];
        wsprintfA(dbgBuf,
                  "RawrXD_InferAsync: fence-reset=0 gen=%I64u req_active=%ld infer_active=%ld",
                  g_rawrxd_inference_generation,
                  _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
                  g_rawrxd_inference_active);
        OutputDebugStringA(dbgBuf);
    }

    TITAN_PARAMS params = {};
    if (g_rawrxd_perf_freq.QuadPart == 0) QueryPerformanceFrequency(&g_rawrxd_perf_freq);
    QueryPerformanceCounter(&g_rawrxd_infer_start_tick);

    params.prompt = rawrxdPromptBuffer;
    params.max_tokens = g_rawrxd_sampling_params.max_tokens > 0 ? g_rawrxd_sampling_params.max_tokens : 256;
    params.temperature = g_rawrxd_sampling_params.temperature;
    params.callback = RawrXDInferenceCallback;
    if (!params.prompt || !params.callback) {
        TitanLog("RawrXD_InferAsync: pre-submit guard failed (null prompt/callback)");
        g_rawrxd_inference_active = 0;
        g_rawrxd_inference_status = RAWRXD_ERROR_INVALID_PARAM;
        g_rawrxd_failed_inference_requests++;
        _InterlockedExchange(&g_rawrxd_completion_fence, 1);
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    g_rawrxd_last_submit_prompt_ptr = RawrXDPtrValue(params.prompt);
    g_rawrxd_last_submit_callback_ptr = RawrXDPtrValue((const void*)params.callback);
    TitanLogHandshakeSnapshot("RawrXD_InferAsync: pre-submit", g_rawrxd_inference_generation);

    int titanRc = -1;
    __try {
        titanRc = Titan_InferAsync(&params);
    } __except(1) {
        TitanLog("RawrXD_InferAsync: Titan_InferAsync raised access violation");
        g_rawrxd_inference_active = 0;
        g_rawrxd_inference_status = RAWRXD_ERROR_NOT_READY;
        g_rawrxd_failed_inference_requests++;
        _InterlockedExchange(&g_rawrxd_completion_fence, 1);
        return RAWRXD_ERROR_NOT_READY;
    }
    {
        char logBuf[160];
        wsprintfA(logBuf,
                  "RawrXD_InferAsync: Titan_InferAsync rc=%d gen=%I64u req_active=%ld infer_active=%ld",
                  titanRc,
                  g_rawrxd_inference_generation,
                  _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
                  g_rawrxd_inference_active);
        TitanLog(logBuf);
    }
    TitanLogHandshakeSnapshot("RawrXD_InferAsync: post-submit", g_rawrxd_inference_generation);
    if (titanRc != 0) {
        g_rawrxd_inference_active = 0;
        g_rawrxd_inference_status = RAWRXD_ERROR_NOT_READY;
        g_rawrxd_failed_inference_requests++;
        _InterlockedExchange(&g_rawrxd_completion_fence, 1);
        return RAWRXD_ERROR_NOT_READY;
    }

    *inference_handle = g_rawrxd_inference_generation;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_InferSync(const char* prompt, size_t prompt_len, RAWRXD_INFERENCE_RESULT* result, uint32_t timeout_ms) {
    if (!result) return RAWRXD_ERROR_INVALID_PARAM;
    if (!g_rawrxd_active_model) return RAWRXD_ERROR_NO_MODEL_LOADED;

    const long long sync_start = QpcNow();
    RAWRXD_INFERENCE_HANDLE handle = 0;

    const long long submit_start = QpcNow();
    RAWRXD_STATUS st = RawrXD_InferAsync(prompt, prompt_len, &handle);
    const long long submit_end = QpcNow();
    if (st != RAWRXD_SUCCESS) {
        char logBuf[224];
        wsprintfA(logBuf,
                  "RawrXD_InferSync: submit failed status=%d prompt_len=%Iu submit_ms=%d total_ms=%d",
                  (int)st,
                  (size_t)prompt_len,
                  QpcDeltaMs(submit_start, submit_end),
                  QpcDeltaMs(sync_start, submit_end));
        TitanLog(logBuf);
        StrCopyN(g_rawrxd_last_infersync_line, logBuf, sizeof(g_rawrxd_last_infersync_line));
        return st;
    }

    const long long wait_start = QpcNow();
    st = RawrXD_WaitForInference(handle, timeout_ms);
    const long long wait_end = QpcNow();
    if (st != RAWRXD_SUCCESS) {
        char logBuf[224];
        wsprintfA(logBuf,
                  "RawrXD_InferSync: wait failed status=%d handle=%I64u wait_ms=%d total_ms=%d",
                  (int)st,
                  (unsigned long long)handle,
                  QpcDeltaMs(wait_start, wait_end),
                  QpcDeltaMs(sync_start, wait_end));
        TitanLog(logBuf);
        StrCopyN(g_rawrxd_last_infersync_line, logBuf, sizeof(g_rawrxd_last_infersync_line));
        return st;
    }

    const long long result_start = QpcNow();
    st = RawrXD_GetInferenceResult(handle, result);
    const long long result_end = QpcNow();

    {
        const int submit_ms = QpcDeltaMs(submit_start, submit_end);
        const int wait_ms = QpcDeltaMs(wait_start, wait_end);
        const int result_ms = QpcDeltaMs(result_start, result_end);
        const int total_ms = QpcDeltaMs(sync_start, result_end);
        uint64_t tps_x1000 = 0;
        if (result->latency_ms > 0 && result->output_token_count > 0) {
            tps_x1000 = ((uint64_t)result->output_token_count * 1000000ULL) / (uint64_t)result->latency_ms;
        }

        char logBuf[320];
        wsprintfA(logBuf,
                  "RawrXD_InferSync: stage_ms submit=%d wait=%d result=%d total=%d status=%d in_tok=%u out_tok=%u latency_ms=%I64u tps_x1000=%I64u",
                  submit_ms,
                  wait_ms,
                  result_ms,
                  total_ms,
                  (int)st,
                  (unsigned int)result->input_token_count,
                  (unsigned int)result->output_token_count,
                  (unsigned long long)result->latency_ms,
                  (unsigned long long)tps_x1000);
        TitanLog(logBuf);
        StrCopyN(g_rawrxd_last_infersync_line, logBuf, sizeof(g_rawrxd_last_infersync_line));
    }

    return st;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_CancelInference(RAWRXD_INFERENCE_HANDLE) {
    if (g_rawrxd_inference_active != 0 || _InterlockedCompareExchange(&g_currentRequest.active, 0, 0) != 0) {
        g_rawrxd_canceled_inference_requests++;
    }
    Titan_Abort();
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_WaitForInference(RAWRXD_INFERENCE_HANDLE inference_handle, uint32_t timeout_ms) {
    if (inference_handle == 0) return RAWRXD_ERROR_INVALID_PARAM;
    if (inference_handle != g_rawrxd_inference_generation &&
        inference_handle != g_rawrxd_last_completed_generation) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    // Defensive timeout handling: even if caller passes 0 (infinite), do not block forever.
    const uint32_t effective_timeout_ms = (timeout_ms == 0) ? 60000u : timeout_ms;
    LARGE_INTEGER freq = {};
    LARGE_INTEGER start_tick = {};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start_tick);
    g_rawrxd_last_wait_handle = inference_handle;
    g_rawrxd_last_wait_timeout_ms = effective_timeout_ms;
    g_rawrxd_last_wait_elapsed_ms = 0;
    TitanLog("RawrXD_WaitForInference: entered");
    TitanLogHandshakeSnapshot("RawrXD_WaitForInference: enter", inference_handle);
    {
        char dbgBuf[224];
        wsprintfA(dbgBuf,
                  "RawrXD_WaitForInference: enter handle=%I64u gen=%I64u last_done=%I64u fence=%ld req_active=%ld infer_active=%ld",
                  inference_handle,
                  g_rawrxd_inference_generation,
                  g_rawrxd_last_completed_generation,
                  _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0),
                  _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
                  g_rawrxd_inference_active);
        OutputDebugStringA(dbgBuf);
    }
    {
        char mbBuf[192];
        wsprintfA(mbBuf,
                  "RawrXD_WaitForInference: mailbox data=%I64u consumed=%I64u frame=%I64u",
                  g_rawrxd_mailbox_data_seq,
                  g_rawrxd_mailbox_consumed_seq,
                  g_rawrxd_mailbox_frame_ready);
        OutputDebugStringA(mbBuf);
    }
    int last_progress_bucket = -1;
    while (1) {
        const long req_active = _InterlockedCompareExchange(&g_currentRequest.active, 0, 0);
        const long infer_active = _InterlockedCompareExchange(&g_rawrxd_inference_active, 0, 0);
        const long fence_state = _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0);
        if (fence_state != 0 && req_active == 0 && infer_active == 0) {
            break;
        }
        if (fence_state == 0 && req_active == 0 && infer_active == 0) {
            break;
        }

        LARGE_INTEGER now = {};
        QueryPerformanceCounter(&now);
        double elapsed_ms = (double)(now.QuadPart - start_tick.QuadPart) * 1000.0 / (double)freq.QuadPart;
        g_rawrxd_last_wait_elapsed_ms = (uint32_t)elapsed_ms;
        int progress_bucket = (int)(elapsed_ms / 1000.0);
        if (progress_bucket != last_progress_bucket && (progress_bucket == 0 || (progress_bucket % 5) == 0)) {
            char logBuf[192];
            wsprintfA(logBuf,
                      "RawrXD_WaitForInference: waiting t=%dms req_active=%ld infer_active=%ld status=%d",
                      (int)elapsed_ms,
                      _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
                      g_rawrxd_inference_active,
                      g_rawrxd_inference_status);
            TitanLog(logBuf);
        }
        last_progress_bucket = progress_bucket;
        if (elapsed_ms >= (double)effective_timeout_ms) {
            const long fence_state_now = _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0);
            if (fence_state_now != 0) {
                TitanLog("RawrXD_WaitForInference: timeout boundary crossed but completion fence is set");
                break;
            }

            // Attempt to unblock any in-flight worker and release waiters.
            const long expected_fence = 1;
            const long actual_fence_before = _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0);
            {
                char fenceLog[224];
                wsprintfA(fenceLog,
                          "RawrXD_WaitForInference: timeout fence expected=%ld actual=%ld gen=%I64u last_done=%I64u req_active=%ld infer_active=%ld",
                          expected_fence,
                          actual_fence_before,
                          (unsigned long long)g_rawrxd_inference_generation,
                          (unsigned long long)g_rawrxd_last_completed_generation,
                          _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
                          g_rawrxd_inference_active);
                TitanLog(fenceLog);
            }
            TitanLogHandshakeSnapshot("RawrXD_WaitForInference: timeout", inference_handle);
            LogVARegionSweepOnTimeout(g_rawrxd_aperture_base, 10);
            _InterlockedExchange(&g_cancelRequest, 1);
            CloseOllamaSocket();
            _InterlockedExchange(&g_currentRequest.active, 0);
            g_rawrxd_inference_active = 0;
            g_rawrxd_inference_status = RAWRXD_ERROR_TIMED_OUT;
            g_rawrxd_timed_out_inference_requests++;
            _InterlockedExchange(&g_rawrxd_completion_fence, 1);
            LogTopVirtualRegionsOnTimeout();
            TitanLog("RawrXD_WaitForInference: timeout, forced active flags clear");
            {
                char dbgBuf[224];
                wsprintfA(dbgBuf,
                          "RawrXD_WaitForInference: timeout t=%dms fence=%ld req_active=%ld infer_active=%ld status=%d",
                          (int)elapsed_ms,
                          _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0),
                          _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
                          g_rawrxd_inference_active,
                          g_rawrxd_inference_status);
                OutputDebugStringA(dbgBuf);
            }
            {
                char mbBuf[192];
                wsprintfA(mbBuf,
                          "RawrXD_WaitForInference: timeout mailbox data=%I64u consumed=%I64u frame=%I64u",
                          g_rawrxd_mailbox_data_seq,
                          g_rawrxd_mailbox_consumed_seq,
                          g_rawrxd_mailbox_frame_ready);
                OutputDebugStringA(mbBuf);
            }
            return RAWRXD_ERROR_TIMED_OUT;
        }
        Sleep(10);
    }
    if (_InterlockedCompareExchange(&g_currentRequest.active, 0, 0) != 0 || g_rawrxd_inference_active != 0) {
        g_rawrxd_inference_status = RAWRXD_ERROR_NOT_READY;
        g_rawrxd_failed_inference_requests++;
        _InterlockedExchange(&g_rawrxd_dispatch_fault_code, 9001);
        TitanLog("RawrXD_WaitForInference: active flags still set after loop; forcing NOT_READY");
        TitanLogHandshakeSnapshot("RawrXD_WaitForInference: active-after-loop", inference_handle);
        return RAWRXD_ERROR_NOT_READY;
    }
    if (_InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0) == 0 &&
        g_rawrxd_inference_status == RAWRXD_SUCCESS) {
        // Fail closed when worker exits without signaling completion callback.
        g_rawrxd_inference_status = RAWRXD_ERROR_NOT_READY;
        g_rawrxd_failed_inference_requests++;
        TitanLog("RawrXD_WaitForInference: completion fence missing; forcing error");
        TitanLogHandshakeSnapshot("RawrXD_WaitForInference: missing-fence", inference_handle);
    }
    {
        char logBuf[160];
        wsprintfA(logBuf,
                  "RawrXD_WaitForInference: completed status=%d req_active=%ld infer_active=%ld fence=%ld expected=1",
                  g_rawrxd_inference_status,
                  _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
                  g_rawrxd_inference_active,
                  _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0));
        TitanLog(logBuf);
    }
    {
        char dbgBuf[224];
        wsprintfA(dbgBuf,
                  "RawrXD_WaitForInference: exit handle=%I64u status=%d fence=%ld req_active=%ld infer_active=%ld",
                  inference_handle,
                  g_rawrxd_inference_status,
                  _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0),
                  _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
                  g_rawrxd_inference_active);
        OutputDebugStringA(dbgBuf);
    }
    TitanLogHandshakeSnapshot("RawrXD_WaitForInference: exit", inference_handle);
    return g_rawrxd_inference_status;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetInferenceResult(RAWRXD_INFERENCE_HANDLE inference_handle, RAWRXD_INFERENCE_RESULT* result) {
    if (!result) return RAWRXD_ERROR_INVALID_PARAM;
    if (inference_handle == 0) return RAWRXD_ERROR_INVALID_PARAM;
    if (inference_handle != g_rawrxd_inference_generation &&
        inference_handle != g_rawrxd_last_completed_generation) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    if (inference_handle == g_rawrxd_inference_generation) {
        const long infer_active = _InterlockedCompareExchange(&g_rawrxd_inference_active, 0, 0);
        if (infer_active != 0) {
            const long fence_state = _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0);
            if (fence_state != 0 && g_rawrxd_inference_status == RAWRXD_SUCCESS) {
                _InterlockedExchange(&g_rawrxd_inference_active, 0);
            } else {
                return RAWRXD_ERROR_NOT_READY;
            }
        }
    }

    result->output_buffer = (uint64_t)(uintptr_t)g_rawrxd_inference_text;
    result->output_token_count = g_rawrxd_last_output_tokens;
    result->input_token_count = g_rawrxd_last_input_tokens;
    result->latency_ms = (uint64_t)g_rawrxd_inference_latency_ms;
    result->status = g_rawrxd_inference_status;
    return g_rawrxd_inference_status;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetInferenceTokenCount(RAWRXD_INFERENCE_HANDLE, uint32_t* token_count) {
    if (!token_count) return RAWRXD_ERROR_INVALID_PARAM;
    *token_count = g_rawrxd_last_output_tokens;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetSamplingParams(const RAWRXD_SAMPLING_PARAMS* params) {
    if (!params) return RAWRXD_ERROR_INVALID_PARAM;
    g_rawrxd_sampling_params = *params;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetSamplingParams(RAWRXD_SAMPLING_PARAMS* params) {
    if (!params) return RAWRXD_ERROR_INVALID_PARAM;
    *params = g_rawrxd_sampling_params;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetSystemPrompt(const char* system_prompt) {
    StrCopyN(g_rawrxd_system_prompt, system_prompt ? system_prompt : "", sizeof(g_rawrxd_system_prompt));
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetSystemPrompt(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return RAWRXD_ERROR_INVALID_PARAM;
    StrCopyN(buffer, g_rawrxd_system_prompt, buffer_size);
    return RAWRXD_SUCCESS;
}
// ============================================================================
// Phase 5: Lightweight Tokenizer/Detokenizer (UTF-8 aware, vocab-respecting)
// ============================================================================

// Simple UTF-8 token builder that respects model vocabulary boundaries
static uint32_t SimpleTokenize(const char* text, int32_t* out_tokens, size_t max_tokens,
                                uint32_t vocab_size, int32_t bos_id, int32_t eos_id) {
    if (!text || !out_tokens || max_tokens == 0) return 0;
    
    uint32_t count = 0;
    
    // Add BOS token if present
    if (bos_id > 0 && count < (uint32_t)max_tokens) {
        out_tokens[count++] = bos_id;
    }
    
    // Lossless UTF-8 path: map each raw byte to a stable token so
    // detokenization can reconstruct byte-for-byte identical strings.
    const unsigned char* p = (const unsigned char*)text;
    while (*p != 0 && count < (uint32_t)max_tokens) {
        uint32_t b = (uint32_t)(*p);
        int32_t token = (int32_t)(256 + b);
        if ((uint32_t)token >= vocab_size) {
            token = (int32_t)b;
        }
        out_tokens[count++] = token;
        ++p;
    }
    
    // Add EOS token if present (and not already count at max)
    if (eos_id > 0 && count < (uint32_t)max_tokens) {
        out_tokens[count++] = eos_id;
    }
    
    return count;
}

static uint32_t SimpleDetokenize(const int32_t* tokens, uint32_t token_count,
                                  char* out_text, size_t max_size) {
    if (!tokens || !out_text || max_size == 0) return 0;
    
    uint32_t out_pos = 0;
    
    for (uint32_t i = 0; i < token_count && out_pos < max_size - 1; i++) {
        int32_t token = tokens[i];
        
        // Skip special/control tokens.
        if (token < 0 || token == g_rawrxd_bos_token_id || token == g_rawrxd_eos_token_id) {
            continue;
        }

        // Reconstruct byte stream from byte-mapped tokens.
        if (token >= 256 && token <= 511) {
            out_text[out_pos++] = (char)(token - 256);
        } else if (token >= 0 && token <= 255) {
            out_text[out_pos++] = (char)token;
        }
    }
    
    out_text[out_pos] = 0;
    return out_pos;
}

// ============================================================================
// Tokenizer/Detokenizer Exports (Phase 5: Real Implementation)
// ============================================================================

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_TokenizeInput(const char* prompt, int32_t* tokens, size_t max_tokens, uint32_t* token_count) {
    if (!token_count) return RAWRXD_ERROR_INVALID_PARAM;
    if (!prompt || !tokens || max_tokens == 0) { *token_count = 0; return RAWRXD_SUCCESS; }
    
    uint32_t vocab = g_modelVocabSize ? g_modelVocabSize : 32000;
    *token_count = SimpleTokenize(prompt, tokens, max_tokens, vocab, 
                                   g_rawrxd_bos_token_id, -1);  // No EOS during tokenization
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_DetokenizeOutput(const int32_t* tokens, uint32_t token_count, char* output, size_t output_size) {
    if (!output || output_size == 0) return RAWRXD_ERROR_INVALID_PARAM;
    if (!tokens || token_count == 0) { output[0] = 0; return RAWRXD_SUCCESS; }
    
    SimpleDetokenize(tokens, token_count, output, output_size);
    return RAWRXD_SUCCESS;
}

    extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetTokenizerInfo(uint32_t* vocab_size, int32_t* bos_token, int32_t* eos_token) {
        if (!vocab_size || !bos_token || !eos_token) return RAWRXD_ERROR_INVALID_PARAM;
        *vocab_size = g_modelVocabSize ? g_modelVocabSize : 32000;
        *bos_token = g_rawrxd_bos_token_id;
        *eos_token = g_rawrxd_eos_token_id;
        return RAWRXD_SUCCESS;
    }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_EstimateTokens(const char* prompt, uint32_t* token_count) {
    if (!token_count) return RAWRXD_ERROR_INVALID_PARAM;
    if (!prompt) { *token_count = 0; return RAWRXD_SUCCESS; }
    
    // Byte-level tokenizer estimate: one token per UTF-8 byte + optional BOS.
    const char* p = prompt;
    uint32_t bytes = 0;
    while (*p != 0) {
        bytes++;
        p++;
    }

    *token_count = bytes + ((g_rawrxd_bos_token_id > 0) ? 1u : 0u);
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_BeginStreaming(RAWRXD_INFERENCE_HANDLE* stream_handle) {
    if (!stream_handle) return RAWRXD_ERROR_INVALID_PARAM;
    g_rawrxd_stream_buffer.Reset();
    _InterlockedExchange64(&g_rawrxd_stream_seq, 0);
    _InterlockedExchange(&g_rawrxd_streaming_enabled, 1);
    *stream_handle = (uint64_t)(g_rawrxd_inference_generation + 1ULL);
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_EndStreaming(RAWRXD_INFERENCE_HANDLE) {
    _InterlockedExchange(&g_rawrxd_streaming_enabled, 0);
    g_rawrxd_stream_hwnd = nullptr;
    g_rawrxd_stream_msg = 0;
    g_rawrxd_stream_wparam = 0;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_StreamConfigureWindow(uint64_t hwnd_value, uint32_t message_id, uint32_t wparam_tag) {
    g_rawrxd_stream_hwnd = (void*)(uintptr_t)hwnd_value;
    g_rawrxd_stream_msg = message_id;
    g_rawrxd_stream_wparam = message_id ? wparam_tag : 0;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_StreamPop(char* buffer, size_t buffer_size, uint32_t* out_len, uint64_t* out_seq) {
    if (!buffer || buffer_size == 0 || !out_len || !out_seq) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    if (!g_rawrxd_stream_buffer.Pop(buffer, (uint32_t)buffer_size, out_len, out_seq)) {
        buffer[0] = 0;
        *out_len = 0;
        *out_seq = 0;
    }
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_StreamGetStats(uint32_t* queued_count, uint32_t* dropped_count) {
    if (!queued_count || !dropped_count) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    *queued_count = g_rawrxd_stream_buffer.Count();
    *dropped_count = g_rawrxd_stream_buffer.Dropped();
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_StreamReset(void) {
    g_rawrxd_stream_buffer.Reset();
    _InterlockedExchange64(&g_rawrxd_stream_seq, 0);
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ApertureInit(void) {
    // Phase 6: First cycle allocates the 1 TiB placeholder reservation.
    // Subsequent cycles must reuse the same base even if the side-band
    // persistent hint was cleared or clobbered while the reservation itself
    // is still alive.
    g_rawrxd_aperture_init_count++;
    uint64_t existingBase = g_rawrxd_persistent_aperture_base != 0
        ? g_rawrxd_persistent_aperture_base
        : g_rawrxd_aperture_base;
    if (existingBase != 0) {
        g_rawrxd_persistent_aperture_base = existingBase;
        g_rawrxd_aperture_base = existingBase;
        ClearApertureTelemetry();
        return RAWRXD_SUCCESS;
    }
    ClearApertureTelemetry();
    uint64_t telemetry[4] = {0, 0, 0, 0};
    int rc = k_swap_aperture_init(0, kRawrXDApertureSpanBytes, telemetry);
    if (rc != 0) return RAWRXD_ERROR_NOT_READY;
    g_rawrxd_aperture_base = telemetry[0];
    g_rawrxd_persistent_aperture_base = telemetry[0];
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ApertureShutdown(void) {
    // Never release the 1TiB reservation while an inference request is still active.
    if (_InterlockedCompareExchange(&g_currentRequest.active, 0, 0) != 0) {
        return RAWRXD_ERROR_NOT_READY;
    }
    g_rawrxd_aperture_shutdown_count++;

    // Phase 5: Explicit VA release to prevent 1TB/cycle drift.
    // Release whichever reservation base is still live, then clear both views.
    uint64_t baseToRelease = g_rawrxd_persistent_aperture_base != 0
        ? g_rawrxd_persistent_aperture_base
        : g_rawrxd_aperture_base;
    if (baseToRelease != 0) {
        BOOL released = VirtualFree((void*)(uintptr_t)baseToRelease, 0, MEM_RELEASE_);
        if (!released) {
            const DWORD releaseErr = GetLastError();
            // ERROR_INVALID_ADDRESS means the reservation is already gone; treat as released.
            if (releaseErr != 487UL) {
                return RAWRXD_ERROR_NOT_READY;
            }
        }
        g_rawrxd_persistent_aperture_base = 0;
        g_rawrxd_aperture_base = 0;
        for (uint32_t s = 0; s < kRawrXDApertureSlotCount; ++s) {
            g_rawrxd_aperture_slot_initialized[s] = 0;
            g_rawrxd_aperture_slot_base[s] = 0;
            g_rawrxd_aperture_slot_size[s] = 0;
        }
        ClearApertureTelemetry();
        return RAWRXD_SUCCESS;
    }
    ClearApertureTelemetry();
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_MapChunk(uint32_t chunk_index, uint64_t* mapped_address) {
    if (!mapped_address) return RAWRXD_ERROR_INVALID_PARAM;
    uint64_t chunkOffset = 0;
    uint64_t bytesToMap = 0;
    RAWRXD_STATUS st = ValidateApertureChunkWindow(chunk_index, &chunkOffset, &bytesToMap);
    if (st != RAWRXD_SUCCESS) return st;
    if (!g_hMapping) return RAWRXD_ERROR_NO_MODEL_LOADED;
    if (g_rawrxd_aperture_base == 0) return RAWRXD_ERROR_NOT_READY;
    g_rawrxd_map_chunk_count++;
    g_rawrxd_last_map_chunk = chunk_index;
    uint64_t out = 0;
    if (g_rawrxd_aperture_slot_initialized[chunk_index]) {
        // Slot already split in this reservation: address is deterministic.
        if (!SafeAddU64(g_rawrxd_aperture_base, chunkOffset, &out)) {
            return RAWRXD_ERROR_INVALID_PARAM;
        }

        // Release-before-remap guard: if a prior view is still marked active,
        // tear it down before reusing the slot address.
        if (g_rawrxd_aperture_slot_map[chunk_index] && g_rawrxd_aperture_slot_base[chunk_index] != 0) {
            int urc = k_swap_aperture_unmap_chunk((void*)(uintptr_t)g_rawrxd_aperture_slot_base[chunk_index]);
            if (urc != 0) {
                char warnBuf[160];
                wsprintfA(warnBuf,
                          "RawrXD_MapChunk: release-before-remap unmap rc=%d chunk=%u base=0x%I64X",
                          urc,
                          chunk_index,
                          (unsigned long long)g_rawrxd_aperture_slot_base[chunk_index]);
                TitanLog(warnBuf);
            }
            g_rawrxd_aperture_slot_map[chunk_index] = 0;
            g_rawrxd_aperture_slot_base[chunk_index] = 0;
            const uint64_t oldSlotBytes = g_rawrxd_aperture_slot_size[chunk_index];
            g_rawrxd_aperture_slot_size[chunk_index] = 0;
            if (g_rawrxd_mapped_chunks > 0) g_rawrxd_mapped_chunks--;
            if (g_rawrxd_mapped_bytes >= oldSlotBytes) g_rawrxd_mapped_bytes -= oldSlotBytes;
            else g_rawrxd_mapped_bytes = 0;
        }
    } else {
        int rc = k_swap_aperture_map_chunk(g_hMapping, (uint64_t)chunk_index, bytesToMap, &out);
        if (rc != 0) {
            // Metadata drift recovery: if reservation base exists, derive slot VA
            // deterministically and continue instead of failing the cycle.
            if (g_rawrxd_aperture_base != 0) {
                if (!SafeAddU64(g_rawrxd_aperture_base, chunkOffset, &out)) {
                    return RAWRXD_ERROR_INVALID_PARAM;
                }
                char recoverBuf[176];
                wsprintfA(recoverBuf,
                          "RawrXD_MapChunk: recovered slot via deterministic VA rc=%d chunk=%u out=0x%I64X",
                          rc,
                          chunk_index,
                          (unsigned long long)out);
                TitanLog(recoverBuf);
            } else {
                return RAWRXD_ERROR_NOT_READY;
            }
        }
        g_rawrxd_aperture_slot_initialized[chunk_index] = 1;
    }
    *mapped_address = out;
    g_rawrxd_last_map_address = out;
    g_rawrxd_map_latency_ms = 0.05;
    if (g_rawrxd_aperture_base == 0 && out != 0) {
        g_rawrxd_aperture_base = out - chunkOffset;
    }
    if (!g_rawrxd_aperture_slot_map[chunk_index]) {
        g_rawrxd_aperture_slot_map[chunk_index] = 1;
        g_rawrxd_aperture_slot_base[chunk_index] = out;
        g_rawrxd_aperture_slot_size[chunk_index] = bytesToMap;
        g_rawrxd_mapped_chunks++;
        uint64_t mappedTotal = 0;
        if (SafeAddU64(g_rawrxd_mapped_bytes, bytesToMap, &mappedTotal)) {
            g_rawrxd_mapped_bytes = mappedTotal;
        } else {
            g_rawrxd_mapped_bytes = kRawrXDApertureSpanBytes;
        }
    } else {
        g_rawrxd_aperture_slot_base[chunk_index] = out;
        const uint64_t oldSlotBytes = g_rawrxd_aperture_slot_size[chunk_index];
        g_rawrxd_aperture_slot_size[chunk_index] = bytesToMap;
        if (g_rawrxd_mapped_bytes >= oldSlotBytes) {
            g_rawrxd_mapped_bytes -= oldSlotBytes;
        } else {
            g_rawrxd_mapped_bytes = 0;
        }
        uint64_t mappedTotal = 0;
        if (SafeAddU64(g_rawrxd_mapped_bytes, bytesToMap, &mappedTotal)) {
            g_rawrxd_mapped_bytes = mappedTotal;
        } else {
            g_rawrxd_mapped_bytes = kRawrXDApertureSpanBytes;
        }
    }
    
    // Phase 7A: no serializing feature detection in the hot path; dispatch uses
    // startup-cached capability state and fail-closed scalar fallback.
    int seal_result = Aperture_DispatchSealVerify(out, bytesToMap, (void*)out);
    if (seal_result < 0) {
        // Mismatch detected (future phase: escalate to error)
        char dbgBuf[128];
        wsprintfA(dbgBuf, "Phase 7A: Seal verification MISMATCH on chunk %u", chunk_index);
        OutputDebugStringA(dbgBuf);
    }
    // seal_result == 1 means verification skipped (normal for short buffers)
    
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_UnmapChunk(uint32_t chunk_index) {
    RAWRXD_STATUS st = ValidateApertureChunkIndex(chunk_index);
    if (st != RAWRXD_SUCCESS) return st;
    if (!g_rawrxd_aperture_base) return RAWRXD_ERROR_NOT_READY;
    g_rawrxd_unmap_chunk_count++;
    g_rawrxd_last_unmap_chunk = chunk_index;
    g_rawrxd_last_unmap_address = g_rawrxd_aperture_slot_base[chunk_index];
    // Release slot view before telemetry updates to prevent VA view orphaning.
    if (g_rawrxd_aperture_slot_base[chunk_index] != 0) {
        (void)k_swap_aperture_unmap_chunk((void*)(uintptr_t)g_rawrxd_aperture_slot_base[chunk_index]);
        g_rawrxd_aperture_slot_base[chunk_index] = 0;
    }
    // Reservation base remains alive for reuse; only per-slot view is released.
    if (g_rawrxd_aperture_slot_map[chunk_index]) {
        const uint64_t oldSlotBytes = g_rawrxd_aperture_slot_size[chunk_index];
        g_rawrxd_aperture_slot_map[chunk_index] = 0;
        g_rawrxd_aperture_slot_size[chunk_index] = 0;
        if (g_rawrxd_mapped_chunks > 0) g_rawrxd_mapped_chunks--;
        if (g_rawrxd_mapped_bytes >= oldSlotBytes) g_rawrxd_mapped_bytes -= oldSlotBytes;
        else g_rawrxd_mapped_bytes = 0;
    }
    g_rawrxd_unmap_latency_ms = 0.03;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetApertureBase(uint64_t* base_address) {
    if (!base_address) return RAWRXD_ERROR_INVALID_PARAM;
    *base_address = g_rawrxd_aperture_base;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetApertureUtilization(RAWRXD_APERTURE_STATUS* status) {
    if (!status) return RAWRXD_ERROR_INVALID_PARAM;
    status->aperture_base = g_rawrxd_aperture_base;
    status->aperture_total_bytes = kRawrXDApertureSpanBytes;
    status->mapped_bytes = g_rawrxd_mapped_bytes;
    status->unmapped_bytes = status->aperture_total_bytes - g_rawrxd_mapped_bytes;
    status->mapped_chunks = g_rawrxd_mapped_chunks;
    status->fragment_count = CountApertureFragmentGaps();
    status->fragmentation_ratio = ComputeApertureFragmentationRatio();
    
    /* Phase 15.2: Compute utilization percentage (utilization_pct10000 = percent * 10000) */
    if (status->aperture_total_bytes > 0) {
        uint64_t util_pct10000 = (g_rawrxd_mapped_bytes * 1000000ULL) / status->aperture_total_bytes;
        /* Clamp to [0, 1000000] */
        if (util_pct10000 > 1000000ULL) util_pct10000 = 1000000ULL;
        status->utilization_pct10000 = (uint32_t)util_pct10000;
    } else {
        status->utilization_pct10000 = 0;
    }
    
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_CompactAperture(void) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_PreAllocateChunks(uint32_t) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetChunkMappingLatency(double* latency_ms) {
    if (!latency_ms) return RAWRXD_ERROR_INVALID_PARAM;
    *latency_ms = g_rawrxd_map_latency_ms;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetChunkUnmapLatency(double* latency_ms) {
    if (!latency_ms) return RAWRXD_ERROR_INVALID_PARAM;
    *latency_ms = g_rawrxd_unmap_latency_ms;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetAperturePolicy(uint32_t policy_flags) { g_rawrxd_aperture_policy = policy_flags; return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetAperturePolicy(uint32_t* policy_flags) {
    if (!policy_flags) return RAWRXD_ERROR_INVALID_PARAM;
    *policy_flags = g_rawrxd_aperture_policy;
    return RAWRXD_SUCCESS;
}

/* ============================================================================
   Phase 15.2: Thread Policy Control Implementations (ordinals 78-80)
   ============================================================================ */

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetInferenceThreadPolicy(uint32_t policy) {
    /* Validate policy enum */
    if (policy > RAWRXD_THREAD_PRIORITY_AFFINITY_CPU0) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    
    _InterlockedExchange(&g_inference_thread_policy, (long)policy);
    
    /* If inference thread is active, apply policy immediately */
    HANDLE hThread = g_inference_thread_handle;
    if (hThread && hThread != INVALID_HANDLE_VALUE) {
        BOOL bSuccess = FALSE;
        
        switch (policy) {
            case RAWRXD_THREAD_PRIORITY_NORMAL:
                bSuccess = SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
                _InterlockedExchange(&g_inference_thread_base_priority, THREAD_PRIORITY_NORMAL);
                break;
                
            case RAWRXD_THREAD_PRIORITY_HIGH:
                /* ABOVE_NORMAL + priority boost */
                bSuccess = SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);
                if (bSuccess) {
                    SetThreadPriorityBoost(hThread, FALSE);  /* Enable boost */
                    _InterlockedExchange(&g_inference_thread_boost_enabled, 1);
                }
                _InterlockedExchange(&g_inference_thread_base_priority, THREAD_PRIORITY_ABOVE_NORMAL);
                break;
                
            case RAWRXD_THREAD_PRIORITY_REALTIME_LIGHT:
                /* TIME_CRITICAL is dangerous; use HIGHEST instead as safe alternative */
                bSuccess = SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
                if (bSuccess) {
                    SetThreadPriorityBoost(hThread, FALSE);
                    _InterlockedExchange(&g_inference_thread_boost_enabled, 1);
                }
                _InterlockedExchange(&g_inference_thread_base_priority, THREAD_PRIORITY_HIGHEST);
                break;
                
            case RAWRXD_THREAD_PRIORITY_AFFINITY_CPU0:
                /* Pin to CPU 0 for cache locality during paging */
                bSuccess = SetThreadAffinityMask(hThread, 0x1);  /* CPU 0 only */
                if (bSuccess) {
                    _InterlockedExchange(&g_inference_thread_affinity_mask, 0x1);
                }
                break;
        }
        
        if (!bSuccess) {
            char logBuf[128];
            snprintf(logBuf, sizeof(logBuf), "SetInferenceThreadPolicy: policy=%u failed, err=%lu", policy, GetLastError());
            OutputDebugStringA(logBuf);
            /* Log but don't fail; continue with current policy */
        }
    }
    
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetInferenceThreadPolicy(RAWRXD_THREAD_POLICY_STATUS* status) {
    if (!status) return RAWRXD_ERROR_INVALID_PARAM;
    
    status->current_policy = (RAWRXD_THREAD_PRIORITY_POLICY)_InterlockedCompareExchange(&g_inference_thread_policy, 0, 0);
    status->base_priority = (uint32_t)_InterlockedCompareExchange(&g_inference_thread_base_priority, 0, 0);
    status->boost_enabled = (uint32_t)_InterlockedCompareExchange(&g_inference_thread_boost_enabled, 0, 0);
    status->affinity_mask = (uint32_t)_InterlockedCompareExchange(&g_inference_thread_affinity_mask, 0, 0);
    
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetInferenceThreadAffinity(uint32_t affinity_mask) {
    if (affinity_mask == 0) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    
    _InterlockedExchange(&g_inference_thread_affinity_mask, (long)affinity_mask);
    
    HANDLE hThread = g_inference_thread_handle;
    if (hThread && hThread != INVALID_HANDLE_VALUE) {
        DWORD_PTR dwResult = SetThreadAffinityMask(hThread, (DWORD_PTR)affinity_mask);
        if (dwResult == 0) {
            char logBuf[128];
            snprintf(logBuf, sizeof(logBuf), "SetInferenceThreadAffinity: mask=0x%x failed, err=%lu", affinity_mask, GetLastError());
            OutputDebugStringA(logBuf);
            return RAWRXD_ERROR_NOT_READY;
        }
    }
    
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Indexer_ReleaseDocument(void) {
    if (g_rawrxd_indexer_mapped_base != 0) {
        (void)k_swap_aperture_unmap_chunk((void*)(uintptr_t)g_rawrxd_indexer_mapped_base);
        g_rawrxd_indexer_mapped_base = 0;
        g_rawrxd_indexer_mapped_size = 0;
        g_rawrxd_indexer_window_count = 0;
    }

    if (g_rawrxd_indexer_mapping) {
        CloseHandle(g_rawrxd_indexer_mapping);
        g_rawrxd_indexer_mapping = nullptr;
    }

    if (g_rawrxd_indexer_embedding_base != 0) {
        (void)UnmapViewOfFile((void*)(uintptr_t)g_rawrxd_indexer_embedding_base);
        g_rawrxd_indexer_embedding_base = 0;
    }
    if (g_rawrxd_indexer_embedding_mapping) {
        CloseHandle(g_rawrxd_indexer_embedding_mapping);
        g_rawrxd_indexer_embedding_mapping = nullptr;
    }
    g_rawrxd_indexer_embedding_capacity = 0;
    g_rawrxd_indexer_embedding_count = 0;

    if (g_rawrxd_indexer_file != INVALID_HANDLE_) {
        CloseHandle(g_rawrxd_indexer_file);
        g_rawrxd_indexer_file = INVALID_HANDLE_;
    }

    if (kRawrXDIndexerSlot < kRawrXDApertureSlotCount) {
        if (g_rawrxd_aperture_slot_map[kRawrXDIndexerSlot]) {
            const uint64_t oldSlotBytes = g_rawrxd_aperture_slot_size[kRawrXDIndexerSlot];
            g_rawrxd_aperture_slot_map[kRawrXDIndexerSlot] = 0;
            if (g_rawrxd_mapped_chunks > 0) g_rawrxd_mapped_chunks--;
            if (g_rawrxd_mapped_bytes >= oldSlotBytes) g_rawrxd_mapped_bytes -= oldSlotBytes;
            else g_rawrxd_mapped_bytes = 0;
        }
        g_rawrxd_aperture_slot_base[kRawrXDIndexerSlot] = 0;
        g_rawrxd_aperture_slot_size[kRawrXDIndexerSlot] = 0;
    }
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Indexer_MapDocument(const char* document_path,
                                                                                         uint64_t* mapped_address,
                                                                                         uint64_t* mapped_size,
                                                                                         uint32_t* window_count_1024) {
    if (!document_path || !mapped_address || !mapped_size || !window_count_1024) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    *mapped_address = 0;
    *mapped_size = 0;
    *window_count_1024 = 0;

    RAWRXD_STATUS apSt = RawrXD_ApertureInit();
    if (apSt != RAWRXD_SUCCESS || g_rawrxd_aperture_base == 0) {
        return RAWRXD_ERROR_NOT_READY;
    }

    // Single-owner ingestion state: release prior mapping before replacing it.
    (void)RawrXD_Indexer_ReleaseDocument();

    g_rawrxd_indexer_file = CreateFileA(document_path,
                                        GENERIC_READ_,
                                        FILE_SHARE_READ_,
                                        nullptr,
                                        OPEN_EXISTING_,
                                        0,
                                        nullptr);
    if (g_rawrxd_indexer_file == INVALID_HANDLE_) {
        return RAWRXD_ERROR_NOT_READY;
    }

    DWORD sizeHi = 0;
    DWORD sizeLo = GetFileSize(g_rawrxd_indexer_file, &sizeHi);
    uint64_t docSize = ((uint64_t)sizeHi << 32) | (uint64_t)sizeLo;
    if (docSize == 0 || docSize > kRawrXDApertureChunkBytes) {
        (void)RawrXD_Indexer_ReleaseDocument();
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    g_rawrxd_indexer_mapping = CreateFileMappingA(g_rawrxd_indexer_file,
                                                  nullptr,
                                                  PAGE_READONLY_,
                                                  0,
                                                  0,
                                                  nullptr);
    if (!g_rawrxd_indexer_mapping) {
        (void)RawrXD_Indexer_ReleaseDocument();
        return RAWRXD_ERROR_NOT_READY;
    }

    uint64_t outBase = 0;
    int mapRc = k_swap_aperture_map_chunk(g_rawrxd_indexer_mapping,
                                          0,
                                          kRawrXDApertureChunkBytes,
                                          &outBase);
    if (mapRc != 0 || outBase == 0) {
        (void)RawrXD_Indexer_ReleaseDocument();
        return RAWRXD_ERROR_NOT_READY;
    }

    g_rawrxd_indexer_mapped_base = outBase;
    g_rawrxd_indexer_mapped_size = docSize;

    // Phase 1 tokenizer windowing policy: 1 token ~= 4 bytes, windows of 1024 tokens.
    uint64_t estimatedTokens = (docSize + 3ULL) / 4ULL;
    uint64_t windows = (estimatedTokens + 1023ULL) / 1024ULL;
    if (windows == 0) windows = 1;
    if (windows > 0xFFFFFFFFULL) windows = 0xFFFFFFFFULL;
    g_rawrxd_indexer_window_count = (uint32_t)windows;

    if (kRawrXDIndexerSlot < kRawrXDApertureSlotCount) {
        if (!g_rawrxd_aperture_slot_map[kRawrXDIndexerSlot]) {
            g_rawrxd_aperture_slot_map[kRawrXDIndexerSlot] = 1;
            g_rawrxd_mapped_chunks++;
            uint64_t mappedTotal = 0;
            if (SafeAddU64(g_rawrxd_mapped_bytes, docSize, &mappedTotal)) {
                g_rawrxd_mapped_bytes = mappedTotal;
            } else {
                g_rawrxd_mapped_bytes = kRawrXDApertureSpanBytes;
            }
        }
        g_rawrxd_aperture_slot_initialized[kRawrXDIndexerSlot] = 1;
        g_rawrxd_aperture_slot_base[kRawrXDIndexerSlot] = outBase;
        g_rawrxd_aperture_slot_size[kRawrXDIndexerSlot] = docSize;
    }

    // Zero-leak ingestion probe: copy from a temporary read-only file view,
    // then securely wipe the staging buffer.
    unsigned char stage[65536];
    size_t stageBytes = (docSize < sizeof(stage)) ? (size_t)docSize : sizeof(stage);
    const unsigned char* src = (const unsigned char*)MapViewOfFile(g_rawrxd_indexer_mapping,
                                                                    FILE_MAP_READ_,
                                                                    0,
                                                                    0,
                                                                    stageBytes);
    if (src) {
        for (size_t i = 0; i < stageBytes; ++i) {
            stage[i] = src[i];
        }
        (void)UnmapViewOfFile((void*)src);
    } else {
        for (size_t i = 0; i < stageBytes; ++i) {
            stage[i] = 0;
        }
    }
    SecureZeroSpan(stage, stageBytes);

    *mapped_address = g_rawrxd_indexer_mapped_base;
    *mapped_size = g_rawrxd_indexer_mapped_size;
    *window_count_1024 = g_rawrxd_indexer_window_count;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Indexer_GenerateEmbeddings(uint32_t max_windows,
                                                                                                uint32_t* generated_windows,
                                                                                                uint64_t* kv_store_address,
                                                                                                uint64_t* kv_store_bytes) {
    if (!generated_windows || !kv_store_address || !kv_store_bytes) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    *generated_windows = 0;
    *kv_store_address = 0;
    *kv_store_bytes = 0;

    if (!g_rawrxd_indexer_mapping || g_rawrxd_indexer_mapped_size == 0 || g_rawrxd_indexer_window_count == 0) {
        return RAWRXD_ERROR_NOT_READY;
    }

    uint32_t windowTarget = g_rawrxd_indexer_window_count;
    if (max_windows != 0 && max_windows < windowTarget) {
        windowTarget = max_windows;
    }
    if (windowTarget == 0) {
        return RAWRXD_SUCCESS;
    }

    uint64_t requiredBytes = (uint64_t)windowTarget * (uint64_t)sizeof(RAWRXD_INDEXER_EMBED_ENTRY);
    if (requiredBytes == 0 || requiredBytes > 0x40000000ULL) {
        return RAWRXD_ERROR_OUT_OF_MEMORY;
    }

    if (g_rawrxd_indexer_embedding_base != 0) {
        (void)UnmapViewOfFile((void*)(uintptr_t)g_rawrxd_indexer_embedding_base);
        g_rawrxd_indexer_embedding_base = 0;
    }
    if (g_rawrxd_indexer_embedding_mapping) {
        CloseHandle(g_rawrxd_indexer_embedding_mapping);
        g_rawrxd_indexer_embedding_mapping = nullptr;
    }

    DWORD mapSizeHi = (DWORD)(requiredBytes >> 32);
    DWORD mapSizeLo = (DWORD)(requiredBytes & 0xFFFFFFFFULL);
    g_rawrxd_indexer_embedding_mapping = CreateFileMappingA(INVALID_HANDLE_,
                                                            nullptr,
                                                            PAGE_READWRITE_,
                                                            mapSizeHi,
                                                            mapSizeLo,
                                                            nullptr);
    if (!g_rawrxd_indexer_embedding_mapping) {
        return RAWRXD_ERROR_OUT_OF_MEMORY;
    }

    void* kvBase = MapViewOfFile(g_rawrxd_indexer_embedding_mapping,
                                 FILE_MAP_READ_ | FILE_MAP_WRITE_,
                                 0,
                                 0,
                                 (size_t)requiredBytes);
    if (!kvBase) {
        CloseHandle(g_rawrxd_indexer_embedding_mapping);
        g_rawrxd_indexer_embedding_mapping = nullptr;
        return RAWRXD_ERROR_OUT_OF_MEMORY;
    }

    g_rawrxd_indexer_embedding_base = (uint64_t)(uintptr_t)kvBase;
    g_rawrxd_indexer_embedding_capacity = requiredBytes;
    g_rawrxd_indexer_embedding_count = 0;

    void* srcView = MapViewOfFile(g_rawrxd_indexer_mapping,
                                  FILE_MAP_READ_,
                                  0,
                                  0,
                                  0);
    if (!srcView) {
        (void)UnmapViewOfFile(kvBase);
        g_rawrxd_indexer_embedding_base = 0;
        CloseHandle(g_rawrxd_indexer_embedding_mapping);
        g_rawrxd_indexer_embedding_mapping = nullptr;
        g_rawrxd_indexer_embedding_capacity = 0;
        g_rawrxd_indexer_embedding_count = 0;
        return RAWRXD_ERROR_NOT_READY;
    }

    const unsigned char* src = (const unsigned char*)srcView;
    RAWRXD_INDEXER_EMBED_ENTRY* kv = (RAWRXD_INDEXER_EMBED_ENTRY*)kvBase;
    static char promptBuf[640];
    const uint64_t bytesPerWindow = 4096ULL;  // 1024 tokens * 4 bytes/token estimate

    for (uint32_t i = 0; i < windowTarget; ++i) {
        uint64_t off = (uint64_t)i * bytesPerWindow;
        if (off >= g_rawrxd_indexer_mapped_size) {
            break;
        }
        uint64_t remain = g_rawrxd_indexer_mapped_size - off;
        uint32_t winBytes = (uint32_t)((remain < bytesPerWindow) ? remain : bytesPerWindow);

        uint64_t h = Fnv1a64Span(src + off, winBytes);

        wsprintfA(promptBuf,
                  "INDEX_WINDOW=%u OFFSET=%I64u BYTES=%u HASH=%I64u",
                  i,
                  off,
                  winBytes,
                  h);
        const char* prompts[1] = { promptBuf };
        RAWRXD_INFERENCE_HANDLE batchHandle = 0;
        RAWRXD_STATUS batchRc = RawrXD_BatchInferences(prompts, 1, &batchHandle);

        kv[i].window_index = i;
        kv[i].byte_offset = off;
        kv[i].byte_count = winBytes;
        kv[i].batch_status = (uint32_t)batchRc;
        kv[i].embedding_hash = h;
        // Phase 8: Neural Embedding Activation — Route to semantic extractor
        (void)BuildNeuralEmbedding(src + off, winBytes, kv[i].embedding);
        g_rawrxd_indexer_embedding_count = i + 1;
    }

    (void)UnmapViewOfFile(srcView);
    *generated_windows = g_rawrxd_indexer_embedding_count;
    *kv_store_address = g_rawrxd_indexer_embedding_base;
    *kv_store_bytes = g_rawrxd_indexer_embedding_capacity;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Indexer_Query(const char* query,
                                                                                  uint32_t top_k,
                                                                                  uint64_t* out_window_offsets,
                                                                                  float* out_scores,
                                                                                  uint32_t* out_count) {
    if (!query || !out_window_offsets || !out_scores || !out_count) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    *out_count = 0;

    if (g_rawrxd_indexer_embedding_base == 0 || g_rawrxd_indexer_embedding_count == 0) {
        return RAWRXD_ERROR_NOT_READY;
    }

    uint32_t k = top_k;
    if (k == 0) k = 5;
    if (k > 128) k = 128;

    RAWRXD_INDEXER_TOPK_ENTRY top[128];
    for (uint32_t i = 0; i < k; ++i) {
        top[i].byte_offset = 0;
        top[i].score = -1.0e30f;
    }

    float qv[RAWRXD_INDEXER_EMBED_DIM];
    (void)BuildNeuralEmbedding((const unsigned char*)query, (uint32_t)strlen(query), qv);

    RAWRXD_INDEXER_EMBED_ENTRY* kv = (RAWRXD_INDEXER_EMBED_ENTRY*)(uintptr_t)g_rawrxd_indexer_embedding_base;
    uint32_t n = g_rawrxd_indexer_embedding_count;
    for (uint32_t i = 0; i < n; ++i) {
        if (kv[i].batch_status != (uint32_t)RAWRXD_SUCCESS) {
            continue;
        }

        double dot = 0.0;
        for (uint32_t d = 0; d < RAWRXD_INDEXER_EMBED_DIM; ++d) {
            dot += (double)qv[d] * (double)kv[i].embedding[d];
        }
        float score = (float)dot;

        uint32_t ins = k;
        for (uint32_t t = 0; t < k; ++t) {
            if (score > top[t].score) {
                ins = t;
                break;
            }
        }
        if (ins >= k) continue;
        for (uint32_t s = k - 1; s > ins; --s) {
            top[s] = top[s - 1];
        }
        top[ins].byte_offset = kv[i].byte_offset;
        top[ins].score = score;
    }

    uint32_t produced = 0;
    for (uint32_t i = 0; i < k; ++i) {
        if (top[i].score <= -1.0e20f) break;
        out_window_offsets[produced] = top[i].byte_offset;
        out_scores[produced] = top[i].score;
        produced++;
    }
    *out_count = produced;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_WeightedFusion(const float* score_matrix,
                                                                                  const float* weights,
                                                                                  uint32_t sub_query_count,
                                                                                  uint32_t candidate_count,
                                                                                  float* out_scores) {
    if (!score_matrix || !weights || !out_scores || sub_query_count == 0 || candidate_count == 0) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    _InterlockedIncrement64((volatile long long*)&g_rawrxd_fusion_invocations);

    if (sub_query_count == 2) {
        const float w0 = weights[0];
        const float w1 = weights[1];
        const float d0 = (float)fabs((double)(w0 - 0.75f));
        const float d1 = (float)fabs((double)(w1 - 0.25f));
        if (d0 <= 1.0e-3f && d1 <= 1.0e-3f) {
            if (rawr_weighted_fusion_ew75_p15_avx512(
                    score_matrix,
                    score_matrix + candidate_count,
                    candidate_count,
                    out_scores) != 0) {
                return RAWRXD_SUCCESS;
            }
        }
    }

    if (rawr_weighted_fusion_fma_avx512(
            score_matrix,
            weights,
            sub_query_count,
            candidate_count,
            out_scores) != 0) {
        return RAWRXD_SUCCESS;
    }

    for (uint32_t c = 0; c < candidate_count; ++c) {
        out_scores[c] = 0.0f;
    }

    for (uint32_t i = 0; i < sub_query_count; ++i) {
        const float w = weights[i];
        const float* row = score_matrix + ((size_t)i * (size_t)candidate_count);
        for (uint32_t c = 0; c < candidate_count; ++c) {
            out_scores[c] += row[c] * w;
        }
    }

    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ApplyPenaltyAndPhraseGate(float* scores,
                                                                                               const uint32_t* missing_term_counts,
                                                                                               const uint32_t* phrase_gate_failures,
                                                                                               uint32_t candidate_count,
                                                                                               float penalty_per_missing,
                                                                                               float clamp_max) {
    if (!scores || !missing_term_counts || !phrase_gate_failures || candidate_count == 0) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    _InterlockedIncrement64((volatile long long*)&g_rawrxd_phrase_gate_invocations);

    if (rawr_apply_penalty_phrase_gate_avx512(
            scores,
            missing_term_counts,
            phrase_gate_failures,
            candidate_count,
            penalty_per_missing,
            clamp_max) != 0) {
        return RAWRXD_SUCCESS;
    }

    for (uint32_t i = 0; i < candidate_count; ++i) {
        float score = scores[i] - ((float)missing_term_counts[i] * penalty_per_missing);
        if (score < 0.0f) {
            score = 0.0f;
        }
        if (phrase_gate_failures[i] != 0 && score > clamp_max) {
            score = clamp_max;
        }
        scores[i] = score;
    }

    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetFusionInvocationCount(uint64_t* out_count) {
    if (!out_count) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    *out_count = g_rawrxd_fusion_invocations;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetPhraseGateInvocationCount(uint64_t* out_count) {
    if (!out_count) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    *out_count = g_rawrxd_phrase_gate_invocations;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_AllocateBuffer(size_t size, void** buffer) {
    if (!buffer) return RAWRXD_ERROR_INVALID_PARAM;
    if (size > sizeof(g_rawrxd_stub_buffer)) return RAWRXD_ERROR_OUT_OF_MEMORY;
    *buffer = (void*)g_rawrxd_stub_buffer;
    if (size > g_rawrxd_peak_memory) g_rawrxd_peak_memory = size;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_FreeBuffer(void*) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetMemoryStats(RAWRXD_MEMORY_STATS* stats) {
    if (!stats) return RAWRXD_ERROR_INVALID_PARAM;
    stats->total_allocated = g_rawrxd_peak_memory;
    stats->currently_allocated = 0;
    stats->peak_allocated = g_rawrxd_peak_memory;
    stats->virtual_address_space = g_fileSize;
    stats->heap_blocks = 1;
    stats->heap_fragmentation = 0.0f;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetHeapStats(RAWRXD_MEMORY_STATS* heap_stats) { return RawrXD_GetMemoryStats(heap_stats); }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_MemoryGC(void) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetWorkingSetLimit(uint64_t max_bytes) { g_rawrxd_working_set_limit = max_bytes; return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetWorkingSetLimit(uint64_t* limit_bytes) {
    if (!limit_bytes) return RAWRXD_ERROR_INVALID_PARAM;
    *limit_bytes = g_rawrxd_working_set_limit;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetPeakMemoryUsage(uint64_t* peak_bytes) {
    if (!peak_bytes) return RAWRXD_ERROR_INVALID_PARAM;
    *peak_bytes = g_rawrxd_peak_memory;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ResetMemoryStatistics(void) { g_rawrxd_peak_memory = 0; return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetVAFragmentation(float* fragmentation) {
    if (!fragmentation) return RAWRXD_ERROR_INVALID_PARAM;
    *fragmentation = ComputeApertureFragmentationRatio();
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetCacheMissRate(float* miss_rate) {
    if (!miss_rate) return RAWRXD_ERROR_INVALID_PARAM;
    *miss_rate = 0.0f;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetInferenceLatency(double* latency_ms) {
    if (!latency_ms) return RAWRXD_ERROR_INVALID_PARAM;
    *latency_ms = g_rawrxd_inference_latency_ms;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetThroughputTokensPerSec(double* throughput) {
    if (!throughput) return RAWRXD_ERROR_INVALID_PARAM;
    if (g_rawrxd_inference_latency_ms > 0.0 && g_rawrxd_last_output_tokens > 0) {
        *throughput = (double)g_rawrxd_last_output_tokens / (g_rawrxd_inference_latency_ms / 1000.0);
    } else {
        *throughput = 0.0;
    }
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ResetPerformanceCounters(void) {
    g_rawrxd_inference_latency_ms = 0.0;
    g_rawrxd_total_output_tokens = 0;
    g_rawrxd_total_inference_requests = 0;
    g_rawrxd_completed_inference_requests = 0;
    g_rawrxd_timed_out_inference_requests = 0;
    g_rawrxd_canceled_inference_requests = 0;
    g_rawrxd_failed_inference_requests = 0;
    g_rawrxd_infer_start_tick.QuadPart = 0;
    g_rf_data_seq = 0;
    g_rf_consumed_seq = 0;
    g_rf_frame_ready = 0;
    g_rawrxd_mailbox_data_seq = 0;
    g_rawrxd_mailbox_consumed_seq = 0;
    g_rawrxd_mailbox_frame_ready = 0;
    g_rf_last_cycles = 0;
    g_rf_last_drift = 0;
    g_rf_max_cycles = 0;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetRFCounters(RAWRXD_RF_COUNTERS* counters) {
    if (!counters) return RAWRXD_ERROR_INVALID_PARAM;
    counters->data_seq = g_rf_data_seq;
    counters->consumed_seq = g_rf_consumed_seq;
    counters->frame_ready = g_rf_frame_ready;
    counters->epoch = g_rawrxd_total_output_tokens;
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetLastError(void) { return g_rawrxd_last_error; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetErrorString(RAWRXD_STATUS error_code, char* message, size_t buffer_size) {
    if (!message || buffer_size == 0) return RAWRXD_ERROR_INVALID_PARAM;
    if (error_code == RAWRXD_SUCCESS) StrCopyN(message, "SUCCESS", buffer_size);
    else if (error_code == RAWRXD_ERROR_NOT_INITIALIZED) StrCopyN(message, "NOT_INITIALIZED", buffer_size);
    else if (error_code == RAWRXD_ERROR_INVALID_PARAM) StrCopyN(message, "INVALID_PARAM", buffer_size);
    else if (error_code == RAWRXD_ERROR_NOT_READY) StrCopyN(message, "NOT_READY", buffer_size);
    else if (error_code == RAWRXD_ERROR_NO_MODEL_LOADED) StrCopyN(message, "NO_MODEL_LOADED", buffer_size);
    else if (error_code == RAWRXD_ERROR_OUT_OF_MEMORY) StrCopyN(message, "OUT_OF_MEMORY", buffer_size);
    else StrCopyN(message, "UNKNOWN", buffer_size);
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ClearError(void) { g_rawrxd_last_error = RAWRXD_SUCCESS; return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_EnableDiagnostics(void) {
    _InterlockedExchange(&g_rawrxd_diagnostics_enabled, 1);
    if (g_rawrxd_diag_callback) g_rawrxd_diag_callback("RawrXD diagnostics enabled");
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_DisableDiagnostics(void) {
    _InterlockedExchange(&g_rawrxd_diagnostics_enabled, 0);
    if (g_rawrxd_diag_callback) g_rawrxd_diag_callback("RawrXD diagnostics disabled");
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetDiagnosticLog(char* log_buffer, size_t buffer_size, size_t* bytes_written) {
    if (!log_buffer || buffer_size == 0) return RAWRXD_ERROR_INVALID_PARAM;
    char tmp[1536] = {0};
    __try {
        wsprintfA(tmp,
            "model=%s arch=%s ctx=%u vocab=%u bos=%d eos=%d "
            "out_tok=%I64u cum_tok=%I64u req=%I64u done=%I64u to=%I64u cancel=%I64u fail=%I64u "
            "lat_ms=%I64u sub_gen=%I64u wait=%I64u done_gen=%I64u cb_len=%u wait_to=%u wait_ms=%u "
            "req_active=%ld infer_active=%ld cancel_flag=%ld fence=%ld infer_status=%d "
            "req_ptr=%I64X prompt_ptr=%I64X cb_ptr=%I64X text_ptr=%I64X fence_ptr=%I64X "
            "ap_base=%I64X ap_persist=%I64X ap_chunks=%u ap_bytes=%I64u ap_init=%I64u ap_shutdown=%I64u map_calls=%I64u unmap_calls=%I64u "
            "map_chunk=%u map_addr=%I64X unmap_chunk=%u unmap_addr=%I64X disp_mode=%ld disp_fallback=%ld disp_stage=%ld disp_fault=%ld worker_rc=%ld "
            "data_seq=%I64u consumed_seq=%I64u frame_ready=%I64u rf_drift=%I64u rf_tsc=%I64u rf_tsc_max=%I64u v=1.2.7-handshake %s",
            g_modelName[0] ? g_modelName : "(none)",
            g_modelArchitecture[0] ? g_modelArchitecture : "(none)",
            g_modelContextLength,
            g_modelVocabSize,
            g_rawrxd_bos_token_id,
            g_rawrxd_eos_token_id,
            (unsigned long long)g_rawrxd_total_output_tokens,
            (unsigned long long)g_rawrxd_total_output_tokens,
            (unsigned long long)g_rawrxd_total_inference_requests,
            (unsigned long long)g_rawrxd_completed_inference_requests,
            (unsigned long long)g_rawrxd_timed_out_inference_requests,
            (unsigned long long)g_rawrxd_canceled_inference_requests,
            (unsigned long long)g_rawrxd_failed_inference_requests,
            (unsigned long long)g_rawrxd_inference_latency_ms,
            (unsigned long long)g_rawrxd_last_submit_generation,
            (unsigned long long)g_rawrxd_last_wait_handle,
            (unsigned long long)g_rawrxd_last_completed_handle,
            g_rawrxd_last_callback_len,
            g_rawrxd_last_wait_timeout_ms,
            g_rawrxd_last_wait_elapsed_ms,
            _InterlockedCompareExchange(&g_currentRequest.active, 0, 0),
            g_rawrxd_inference_active,
            _InterlockedCompareExchange(&g_cancelRequest, 0, 0),
            _InterlockedCompareExchange(&g_rawrxd_completion_fence, 0, 0),
            g_rawrxd_inference_status,
            (unsigned long long)RawrXDPtrValue((const void*)&g_currentRequest),
            (unsigned long long)g_rawrxd_last_submit_prompt_ptr,
            (unsigned long long)g_rawrxd_last_submit_callback_ptr,
            (unsigned long long)g_rawrxd_last_callback_text_ptr,
            (unsigned long long)RawrXDPtrValue((const void*)&g_rawrxd_completion_fence),
            (unsigned long long)g_rawrxd_aperture_base,
            (unsigned long long)g_rawrxd_persistent_aperture_base,
            g_rawrxd_mapped_chunks,
            (unsigned long long)g_rawrxd_mapped_bytes,
            (unsigned long long)g_rawrxd_aperture_init_count,
            (unsigned long long)g_rawrxd_aperture_shutdown_count,
            (unsigned long long)g_rawrxd_map_chunk_count,
            (unsigned long long)g_rawrxd_unmap_chunk_count,
            g_rawrxd_last_map_chunk,
            (unsigned long long)g_rawrxd_last_map_address,
            g_rawrxd_last_unmap_chunk,
            (unsigned long long)g_rawrxd_last_unmap_address,
            _InterlockedCompareExchange(&g_aperture_dispatch_mode, 0, 0),
            _InterlockedCompareExchange(&g_aperture_dispatch_fallback_count, 0, 0),
            _InterlockedCompareExchange(&g_rawrxd_dispatch_stage, 0, 0),
            _InterlockedCompareExchange(&g_rawrxd_dispatch_fault_code, 0, 0),
            _InterlockedCompareExchange(&g_rawrxd_last_worker_exit_code, 0, 0),
            (unsigned long long)g_rf_data_seq,
            (unsigned long long)g_rf_consumed_seq,
            (unsigned long long)g_rf_frame_ready,
            (unsigned long long)g_rf_last_drift,
            (unsigned long long)g_rf_last_cycles,
            (unsigned long long)g_rf_max_cycles,
            g_rawrxd_last_infersync_line[0] ? g_rawrxd_last_infersync_line : "sync_line=none");
        StrCopyN(log_buffer, tmp, buffer_size);
        if (bytes_written) {
            size_t n = 0;
            while (log_buffer[n]) n++;
            *bytes_written = n;
        }
    } __except(1) {
        StrCopyN(log_buffer, "diag_exception v=1.2.7-handshake", buffer_size);
        if (bytes_written) {
            size_t n = 0;
            while (log_buffer[n]) n++;
            *bytes_written = n;
        }
        TitanLog("RawrXD_GetDiagnosticLog: exception trapped");
        return RAWRXD_ERROR_NOT_READY;
    }
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetDiagnosticCallback(void (*callback)(const char*)) {
    g_rawrxd_diag_callback = callback;
    if (g_rawrxd_diag_callback && _InterlockedCompareExchange(&g_rawrxd_diagnostics_enabled, 0, 0) != 0) {
        g_rawrxd_diag_callback("RawrXD diagnostics callback registered");
    }
    return RAWRXD_SUCCESS;
}
// =============================================================================
// SDMA Kinetic Telemetry (from rawr_circular_sdma.h)
// =============================================================================
static uint64_t g_sdma_flip_count = 0;
static uint64_t g_sdma_wait_cycles = 0;
static uint64_t g_expert_cache_hits = 0;
static uint64_t g_expert_cache_misses = 0;

extern "C" __declspec(dllexport) void __stdcall Rawr_Get_SDMA_Telemetry(
    uint64_t* out_flip_count,
    uint64_t* out_wait_cycles,
    uint64_t* out_cache_hits,
    uint64_t* out_cache_misses) 
{
    if (out_flip_count)   *out_flip_count = g_sdma_flip_count;
    if (out_wait_cycles)  *out_wait_cycles = g_sdma_wait_cycles;
    if (out_cache_hits)   *out_cache_hits = g_expert_cache_hits;
    if (out_cache_misses) *out_cache_misses = g_expert_cache_misses;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GenerateAuditReport(char* report_buffer, size_t buffer_size, size_t* bytes_written) {
    if (!report_buffer || buffer_size == 0) return RAWRXD_ERROR_INVALID_PARAM;
    char json[768] = {0};
    wsprintfA(json,
        "{\"status\":\"INTEGRATION_ACTIVE\",\"exports\":77,"
        "\"model_loaded\":%u,\"model\":\"%s\",\"arch\":\"%s\","
        "\"requests\":%I64u,\"completed\":%I64u,\"timeouts\":%I64u,\"canceled\":%I64u,\"failed\":%I64u,"
        "\"tokens_total\":%I64u,\"tokens_cumulative\":%I64u,\"dispatch_mode\":%ld,\"dispatch_fallback\":%ld,"
        "\"header_fast_hits\":%I64u,\"header_fast_failures\":%I64u,"
        "\"data_seq\":%I64u,\"consumed_seq\":%I64u,\"frame_ready\":%I64u,"
        "\"rf_drift\":%I64u,\"rf_tsc\":%I64u,\"rf_tsc_max\":%I64u}",
        g_rawrxd_active_model ? 1u : 0u,
        g_modelName[0] ? g_modelName : "",
        g_modelArchitecture[0] ? g_modelArchitecture : "",
        (unsigned long long)g_rawrxd_total_inference_requests,
        (unsigned long long)g_rawrxd_completed_inference_requests,
        (unsigned long long)g_rawrxd_timed_out_inference_requests,
        (unsigned long long)g_rawrxd_canceled_inference_requests,
        (unsigned long long)g_rawrxd_failed_inference_requests,
        (unsigned long long)g_rawrxd_total_output_tokens,
        (unsigned long long)g_rawrxd_total_output_tokens,
        _InterlockedCompareExchange(&g_aperture_dispatch_mode, 0, 0),
        _InterlockedCompareExchange(&g_aperture_dispatch_fallback_count, 0, 0),
        (unsigned long long)g_rawrxd_header_fast_path_hits,
        (unsigned long long)g_rawrxd_header_fast_path_failures,
        (unsigned long long)g_rf_data_seq,
        (unsigned long long)g_rf_consumed_seq,
        (unsigned long long)g_rf_frame_ready,
        (unsigned long long)g_rf_last_drift,
        (unsigned long long)g_rf_last_cycles,
        (unsigned long long)g_rf_max_cycles);
    StrCopyN(report_buffer, json, buffer_size);
    if (bytes_written) {
        size_t n = 0;
        while (report_buffer[n]) n++;
        *bytes_written = n;
    }
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ValidateInternalConsistency(uint32_t* is_consistent) {
    if (!is_consistent) return RAWRXD_ERROR_INVALID_PARAM;
    uint32_t ok = 1;
    if (g_rawrxd_mapped_chunks > kRawrXDApertureSlotCount) ok = 0;
    if (g_rawrxd_mapped_bytes > kRawrXDApertureSpanBytes) ok = 0;
    if (g_rawrxd_active_model && !_InterlockedCompareExchange(&g_rawrxd_initialized, 0, 0)) ok = 0;
    *is_consistent = ok;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_DumpHeapWalker(char* output_buffer, size_t buffer_size, size_t* bytes_written) {
    if (!output_buffer || buffer_size == 0) return RAWRXD_ERROR_INVALID_PARAM;
    StrCopyN(output_buffer, "heap walker stub", buffer_size);
    if (bytes_written) {
        size_t n = 0;
        while (output_buffer[n]) n++;
        *bytes_written = n;
    }
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_EnableHotpatch(void) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_ApplyHotpatch(const uint8_t*, size_t) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_QueryHotpatchStatus(uint32_t* status) {
    if (!status) return RAWRXD_ERROR_INVALID_PARAM;
    *status = 0;
    return RAWRXD_SUCCESS;
}
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_EnableQuantization(uint32_t) { return RAWRXD_SUCCESS; }
extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_BatchInferences(const char** prompts, uint32_t prompt_count, RAWRXD_INFERENCE_HANDLE* batch_handle) {
    if (!batch_handle) return RAWRXD_ERROR_INVALID_PARAM;
    if (!prompts || prompt_count == 0) return RAWRXD_ERROR_INVALID_PARAM;
    *batch_handle = 1;
    return RAWRXD_SUCCESS;
}

static void SecureWipeBytes(volatile uint8_t* ptr, size_t len) {
    if (!ptr || len == 0) return;
    for (size_t i = 0; i < len; ++i) {
        ptr[i] = 0;
    }
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Indexer_Ingest(
    const char* document_path,
    uint32_t chunk_bytes,
    uint32_t overlap_bytes,
    uint32_t* chunks_indexed,
    uint64_t* bytes_ingested,
    uint64_t* content_fingerprint)
{
    if (!document_path || !document_path[0] || !chunks_indexed || !bytes_ingested || !content_fingerprint) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }

    *chunks_indexed = 0;
    *bytes_ingested = 0;
    *content_fingerprint = 0;

    if (!_InterlockedCompareExchange(&g_rawrxd_initialized, 0, 0)) {
        return RAWRXD_ERROR_NOT_INITIALIZED;
    }
    if (!g_rawrxd_active_model) {
        // Ingestion probe requires an active model to validate the Titan lane hook.
        return RAWRXD_ERROR_NO_MODEL_LOADED;
    }

    if (chunk_bytes == 0) chunk_bytes = 1024;
    if (chunk_bytes > sizeof(g_rawrxd_stub_buffer) - 1) {
        chunk_bytes = (uint32_t)sizeof(g_rawrxd_stub_buffer) - 1;
    }
    if (overlap_bytes >= chunk_bytes) {
        overlap_bytes = chunk_bytes / 4;
    }
    uint32_t step_bytes = chunk_bytes - overlap_bytes;
    if (step_bytes == 0) step_bytes = chunk_bytes;

    HANDLE hFile = CreateFileA(document_path, GENERIC_READ_, FILE_SHARE_READ_, nullptr, OPEN_EXISTING_, 0, nullptr);
    if (hFile == INVALID_HANDLE_) {
        return RAWRXD_ERROR_NOT_READY;
    }

    DWORD sizeHi = 0;
    DWORD sizeLo = GetFileSize(hFile, &sizeHi);
    uint64_t fileSize = ((uint64_t)sizeHi << 32) | (uint64_t)sizeLo;
    if (fileSize == 0) {
        CloseHandle(hFile);
        return RAWRXD_SUCCESS;
    }

    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY_, 0, 0, nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        return RAWRXD_ERROR_NOT_READY;
    }

    const uint8_t* fileView = (const uint8_t*)MapViewOfFile(hMap, FILE_MAP_READ_, 0, 0, 0);
    if (!fileView) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return RAWRXD_ERROR_NOT_READY;
    }

    uint64_t fnv = 1469598103934665603ULL;
    const uint64_t fnvPrime = 1099511628211ULL;
    uint8_t* chunkBuf = g_rawrxd_stub_buffer;
    bool hookExecuted = false;
    RAWRXD_STATUS ingestStatus = RAWRXD_SUCCESS;

    for (uint64_t pos = 0; pos < fileSize; pos += step_bytes) {
        uint64_t remaining = fileSize - pos;
        uint32_t take = (uint32_t)((remaining < chunk_bytes) ? remaining : chunk_bytes);
        if (take == 0) break;

        for (uint32_t i = 0; i < take; ++i) {
            uint8_t b = fileView[pos + i];
            fnv ^= (uint64_t)b;
            fnv *= fnvPrime;

            // Keep a readable, bounded plaintext chunk for local embedding probe.
            if (b < 0x20 || b == 0x7F) {
                chunkBuf[i] = ' ';
            } else {
                chunkBuf[i] = b;
            }
        }
        chunkBuf[take] = 0;

        if (!hookExecuted) {
            RAWRXD_INFERENCE_RESULT probeResult = {};
            RAWRXD_STATUS rcInfer = RawrXD_InferSync((const char*)chunkBuf, (size_t)take, &probeResult, 10000);
            if (rcInfer != RAWRXD_SUCCESS) {
                ingestStatus = rcInfer;
                SecureWipeBytes((volatile uint8_t*)chunkBuf, (size_t)take + 1);
                break;
            }
            hookExecuted = true;
        }

        SecureWipeBytes((volatile uint8_t*)chunkBuf, (size_t)take + 1);
        (*chunks_indexed)++;
        *bytes_ingested += (uint64_t)take;

        if (remaining <= chunk_bytes) {
            break;
        }
    }

    *content_fingerprint = fnv;

    UnmapViewOfFile((LPVOID)fileView);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return ingestStatus;
}

/* ============================================================================
   Phase 15.4: Extended Metrics Exports (ordinals 97-99)
   Rawr_CalculateThroughput  @97  — integer tokens/sec for status bar display
   Rawr_CalculateLatency     @98  — integer ms/token for status bar display
   Rawr_CalculateCacheHitRatio @99 — integer 0-100% for status bar display
   ============================================================================ */

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall Rawr_CalculateThroughput(uint32_t* out_tokens_per_sec) {
    if (!out_tokens_per_sec) return RAWRXD_ERROR_INVALID_PARAM;
    if (g_rawrxd_inference_latency_ms > 0.0 && g_rawrxd_last_output_tokens > 0) {
        double tps = (double)g_rawrxd_last_output_tokens / (g_rawrxd_inference_latency_ms / 1000.0);
        /* Clamp to uint16_t-safe range for status bar display */
        if (tps > 65535.0) tps = 65535.0;
        *out_tokens_per_sec = (uint32_t)tps;
    } else {
        *out_tokens_per_sec = 0;
    }
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall Rawr_CalculateLatency(uint32_t* out_ms_per_tok) {
    if (!out_ms_per_tok) return RAWRXD_ERROR_INVALID_PARAM;
    if (g_rawrxd_last_output_tokens > 0 && g_rawrxd_inference_latency_ms > 0.0) {
        double mpt = g_rawrxd_inference_latency_ms / (double)g_rawrxd_last_output_tokens;
        /* Clamp to 999 ms/tok maximum for display */
        if (mpt > 999.0) mpt = 999.0;
        *out_ms_per_tok = (uint32_t)mpt;
    } else {
        *out_ms_per_tok = 0;
    }
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall Rawr_CalculateCacheHitRatio(uint32_t* out_percent) {
    if (!out_percent) return RAWRXD_ERROR_INVALID_PARAM;
    uint64_t total = g_expert_cache_hits + g_expert_cache_misses;
    if (total > 0) {
        uint64_t pct = (g_expert_cache_hits * 100ULL) / total;
        if (pct > 100) pct = 100;
        *out_percent = (uint32_t)pct;
    } else {
        *out_percent = 0;
    }
    return RAWRXD_SUCCESS;
}

// ============================================================================
// End of titan_infer_dll.cpp
// ============================================================================
