#include "system_runtime.hpp"
#include <cstring>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief AsmDeflate implementation using zlib
 * 
 * Provides DEFLATE compression using the zlib library.
 */
int __stdcall AsmDeflate(
    const unsigned char* source,
    unsigned int sourceLen,
    unsigned char* dest,
    unsigned int* destLen)
{
    if (!source || !dest || !destLen) {
        return -1;
    }
    
#ifdef HAVE_ZLIB
    // Use zlib's compress2 function
    int ret = compress2(
        dest,
        (unsigned long*)destLen,
        source,
        sourceLen,
        Z_DEFAULT_COMPRESSION);
    
    // Convert zlib return codes to simple int
    // 0 = Z_OK (success)
    // -1 = Z_ERRNO (error)
    // -2 = Z_STREAM_ERROR (stream error)
    // -3 = Z_DATA_ERROR (data error)
    // -4 = Z_MEM_ERROR (memory error)
    // -5 = Z_BUF_ERROR (buffer error)
    // -6 = Z_VERSION_ERROR (incompatible version)
    return ret;
#else
    // Stub implementation - just copy data (no compression)
    if (*destLen < sourceLen) {
        return -5; // Z_BUF_ERROR
    }
    memcpy(dest, source, sourceLen);
    *destLen = sourceLen;
    return 0; // Success (no compression)
#endif
}

/**
 * @brief AsmInflate implementation using zlib
 * 
 * Provides DEFLATE decompression using the zlib library.
 */
int __stdcall AsmInflate(
    const unsigned char* source,
    unsigned int sourceLen,
    unsigned char* dest,
    unsigned int* destLen)
{
    if (!source || !dest || !destLen) {
        return -1;
    }
    
#ifdef HAVE_ZLIB
    // Use zlib's uncompress function
    int ret = uncompress(
        dest,
        (unsigned long*)destLen,
        source,
        sourceLen);
    
    // zlib return codes are used directly
    return ret;
#else
    // Stub implementation - just copy data (no decompression)
    if (*destLen < sourceLen) {
        return -5; // Z_BUF_ERROR
    }
    memcpy(dest, source, sourceLen);
    *destLen = sourceLen;
    return 0; // Success (no decompression)
#endif
}

/**
 * @brief CreateThreadEx implementation
 * 
 * Creates a thread with extended options using Windows API or stub.
 */
int __stdcall CreateThreadEx(
    void** hThread,
    unsigned long (__stdcall* threadFunc)(void*),
    void* arg,
    unsigned int flags)
{
    if (!hThread || !threadFunc) {
        return -1;
    }
    
#ifdef _WIN32
    // Use Windows API CreateThread
    HANDLE h = ::CreateThread(
        nullptr,                       // Security attributes
        0,                             // Stack size (default)
        (LPTHREAD_START_ROUTINE)threadFunc,
        arg,
        (flags & 0x04) ? CREATE_SUSPENDED : 0,
        nullptr);
    
    if (h == nullptr) {
        *hThread = nullptr;
        return static_cast<int>(::GetLastError());
    }
    
    *hThread = h;
    return 0;
#else
    // Non-Windows stub
    return -1;
#endif
}

/**
 * @brief CreatePipeEx implementation
 * 
 * Creates an IPC pipe with extended options using Windows API or stub.
 */
int __stdcall CreatePipeEx(
    void** hReadPipe,
    void** hWritePipe,
    void* pipeAttributes,
    unsigned int size)
{
    if (!hReadPipe || !hWritePipe) {
        return -1;
    }
    
#ifdef _WIN32
    // Use Windows API CreatePipe
    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    
    // Set default size if not specified
    if (size == 0) {
        size = 65536;  // 64KB default
    }
    
    if (!::CreatePipe(
        &hRead,
        &hWrite,
        static_cast<LPSECURITY_ATTRIBUTES>(pipeAttributes),
        size))
    {
        *hReadPipe = nullptr;
        *hWritePipe = nullptr;
        return static_cast<int>(::GetLastError());
    }
    
    *hReadPipe = hRead;
    *hWritePipe = hWrite;
    return 0;
#else
    // Non-Windows stub
    return -1;
#endif
}
