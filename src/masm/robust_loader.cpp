#include "robust_loader.h"
#include <windows.h>
#include <atomic>
#include <mutex>
#include <cstdio>

using FnInit = void(__cdecl*)();
using FnAlloc = void* (__cdecl*)(uint64_t, uint32_t);
using FnFree = void (__cdecl*)(void*);
using FnOpenStream = int(__cdecl*)(const wchar_t*, void*);
using FnReadSafe = uint64_t(__cdecl*)(void*, void*, uint64_t);
using FnSkipString = uint64_t(__cdecl*)(void*);
using FnCrc64 = uint64_t(__cdecl*)(uint64_t, const void*, uint64_t);
using FnAtomicInc = void(__cdecl*)(void*);
using FnLog = void(__cdecl*)(const char*);

static HMODULE g_h = nullptr;
static std::once_flag g_init_flag;
static FnInit pInit = nullptr;
static FnAlloc pAlloc = nullptr;
static FnFree pFree = nullptr;
static FnOpenStream pOpenStream = nullptr;
static FnReadSafe pReadSafe = nullptr;
static FnSkipString pSkipString = nullptr;
static FnCrc64 pCrc64 = nullptr;
static FnAtomicInc pAtomicInc = nullptr;
static FnLog pLog = nullptr;

static void EnsureInit() {
    std::call_once(g_init_flag, []() {
        // Try to load robust_tools.dll from the executable directory
        CHAR path[MAX_PATH];
        if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
            // replace executable name with robust_tools.dll
            for (int i = (int)strlen(path) - 1; i >= 0; --i) {
                if (path[i] == '\\' || path[i] == '/') { path[i+1] = '\0'; break; }
            }
            strcat_s(path, MAX_PATH, "robust_tools.dll");
            g_h = LoadLibraryA(path);
        }
        if (!g_h) {
            // Try system search
            g_h = LoadLibraryA("robust_tools.dll");
        }
        if (g_h) {
            pInit = (FnInit)GetProcAddress(g_h, "Robust_Initialize");
            pAlloc = (FnAlloc)GetProcAddress(g_h, "Robust_Allocate");
            pFree = (FnFree)GetProcAddress(g_h, "Robust_Free");
            pOpenStream = (FnOpenStream)GetProcAddress(g_h, "Robust_OpenStream");
            pReadSafe = (FnReadSafe)GetProcAddress(g_h, "Robust_ReadSafe");
            pSkipString = (FnSkipString)GetProcAddress(g_h, "Robust_SkipString");
            pCrc64 = (FnCrc64)GetProcAddress(g_h, "Robust_Crc64Update");
            pAtomicInc = (FnAtomicInc)GetProcAddress(g_h, "Robust_AtomicInc64");
            pLog = (FnLog)GetProcAddress(g_h, "Robust_Log");
        }
    });
}

extern "C" void __cdecl Robust_Initialize() {
    EnsureInit();
    if (pInit) { pInit(); return; }
    // Fallback: nothing to init
}

extern "C" void* __cdecl Robust_Allocate(uint64_t size, uint32_t flags) {
    EnsureInit();
    if (pAlloc) return pAlloc(size, flags);
    // Fallback: VirtualAlloc with alignment
    void* p = VirtualAlloc(nullptr, (SIZE_T)size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return p;
}

extern "C" void __cdecl Robust_Free(void* ptr) {
    EnsureInit();
    if (pFree) { pFree(ptr); return; }
    if (ptr) VirtualFree(ptr, 0, MEM_RELEASE);
}

extern "C" int __cdecl Robust_OpenStream(const wchar_t* filename, void* io_context) {
    EnsureInit();
    if (pOpenStream) return pOpenStream(filename, io_context);
    // Fallback: not implemented
    return 0;
}

extern "C" uint64_t __cdecl Robust_ReadSafe(void* io_context, void* dest, uint64_t bytesToRead) {
    EnsureInit();
    if (pReadSafe) return pReadSafe(io_context, dest, bytesToRead);
    // Fallback: not implemented
    return 0;
}

extern "C" uint64_t __cdecl Robust_SkipString(void* io_context) {
    EnsureInit();
    if (pSkipString) return pSkipString(io_context);
    return 0;
}

extern "C" uint64_t __cdecl Robust_Crc64Update(uint64_t prev_crc, const void* buffer, uint64_t len) {
    EnsureInit();
    if (pCrc64) return pCrc64(prev_crc, buffer, len);
    // Simple fallback CRC32-like (not CRC64)
    const uint8_t* b = static_cast<const uint8_t*>(buffer);
    uint64_t crc = prev_crc ^ 0xFFFFFFFFULL;
    for (uint64_t i = 0; i < len; ++i) {
        crc = (crc >> 8) ^ (uint64_t)(b[i] + 0x9e3779b97f4a7c15ULL);
    }
    return crc ^ 0xFFFFFFFFULL;
}

extern "C" void __cdecl Robust_AtomicInc64(void* ptr) {
    EnsureInit();
    if (pAtomicInc) { pAtomicInc(ptr); return; }
    if (!ptr) return;
    InterlockedIncrement64(reinterpret_cast<LONG64*>(ptr));
}

extern "C" void __cdecl Robust_Log(const char* msg) {
    EnsureInit();
    if (pLog) { pLog(msg); return; }
    OutputDebugStringA(msg);
}
