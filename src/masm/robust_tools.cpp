// robust_tools.cpp — Production robust tools implementation

#include "robust_tools.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <mutex>

static std::vector<void*> g_allocations;
static std::mutex g_allocMutex;
static uint64_t g_crc64Table[256];
static bool g_crc64Initialized = false;

static void initCRC64() {
    if (g_crc64Initialized) return;
    
    for (int i = 0; i < 256; i++) {
        uint64_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xC96C5795D7870F42ULL : 0);
        }
        g_crc64Table[i] = crc;
    }
    g_crc64Initialized = true;
}

extern "C" void __cdecl Robust_Initialize() {
    initCRC64();
}

extern "C" void* __cdecl Robust_Allocate(uint64_t size, uint32_t flags) {
    (void)flags;
    if (size == 0) return nullptr;
    
    void* ptr = HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(size));
    if (ptr) {
        std::lock_guard<std::mutex> lock(g_allocMutex);
        g_allocations.push_back(ptr);
    }
    return ptr;
}

extern "C" void __cdecl Robust_Free(void* ptr) {
    if (!ptr) return;
    
    {
        std::lock_guard<std::mutex> lock(g_allocMutex);
        auto it = std::find(g_allocations.begin(), g_allocations.end(), ptr);
        if (it != g_allocations.end()) {
            g_allocations.erase(it);
        }
    }
    
    HeapFree(GetProcessHeap(), 0, ptr);
}

extern "C" int __cdecl Robust_OpenStream(const wchar_t* filename, void* io_context) {
    if (!filename || !io_context) return -1;
    
    HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, 
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    *(static_cast<HANDLE*>(io_context)) = hFile;
    return 0;
}

extern "C" uint64_t __cdecl Robust_ReadSafe(void* io_context, void* dest, uint64_t bytesToRead) {
    if (!io_context || !dest || bytesToRead == 0) return 0;
    
    HANDLE hFile = *(static_cast<HANDLE*>(io_context));
    DWORD bytesRead = 0;
    
    ReadFile(hFile, dest, static_cast<DWORD>(bytesToRead), &bytesRead, nullptr);
    return bytesRead;
}

extern "C" uint64_t __cdecl Robust_SkipString(void* io_context) {
    if (!io_context) return 0;
    
    HANDLE hFile = *(static_cast<HANDLE*>(io_context));
    uint64_t skipped = 0;
    char ch;
    DWORD bytesRead = 0;
    
    while (ReadFile(hFile, &ch, 1, &bytesRead, nullptr) && bytesRead == 1 && ch != '\0') {
        skipped++;
    }
    
    return skipped;
}

extern "C" uint64_t __cdecl Robust_Crc64Update(uint64_t prev_crc, const void* buffer, uint64_t len) {
    if (!buffer || len == 0) return prev_crc;
    
    initCRC64();
    
    uint64_t crc = ~prev_crc;
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
    
    for (uint64_t i = 0; i < len; i++) {
        crc = g_crc64Table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return ~crc;
}

extern "C" void __cdecl Robust_AtomicInc64(void* ptr) {
    if (!ptr) return;
    InterlockedIncrement64(static_cast<volatile LONG64*>(ptr));
}

extern "C" void __cdecl Robust_Log(const char* msg) {
    if (!msg) return;
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}
