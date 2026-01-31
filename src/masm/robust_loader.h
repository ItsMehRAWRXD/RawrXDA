#pragma once
#include <cstdint>

extern "C" {
    void __cdecl Robust_Initialize();
    void* __cdecl Robust_Allocate(uint64_t size, uint32_t flags);
    void  __cdecl Robust_Free(void* ptr);

    int   __cdecl Robust_OpenStream(const wchar_t* filename, void* io_context);
    uint64_t __cdecl Robust_ReadSafe(void* io_context, void* dest, uint64_t bytesToRead);
    uint64_t __cdecl Robust_SkipString(void* io_context);

    uint64_t __cdecl Robust_Crc64Update(uint64_t prev_crc, const void* buffer, uint64_t len);
    void   __cdecl Robust_AtomicInc64(void* ptr);
    void   __cdecl Robust_Log(const char* msg);
}
