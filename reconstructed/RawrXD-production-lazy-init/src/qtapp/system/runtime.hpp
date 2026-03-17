#ifndef SYSTEM_RUNTIME_HPP
#define SYSTEM_RUNTIME_HPP

/**
 * @brief System Runtime Function Stubs
 * 
 * Provides C-style implementations of MASM and Windows threading functions
 * that may not be available in all build configurations.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Deflate compression routine (stub)
 * 
 * Compresses data using DEFLATE algorithm. This is a stub that can be
 * replaced with actual MASM or external library implementations.
 * 
 * @param[in] source Source buffer
 * @param[in] sourceLen Source buffer length
 * @param[out] dest Destination buffer
 * @param[in,out] destLen Destination buffer size (updated with output size)
 * @return 0 on success, non-zero on error
 */
int __stdcall AsmDeflate(
    const unsigned char* source,
    unsigned int sourceLen,
    unsigned char* dest,
    unsigned int* destLen
);

/**
 * @brief Inflate decompression routine (stub)
 * 
 * Decompresses DEFLATE compressed data. This is a stub that can be
 * replaced with actual MASM or external library implementations.
 * 
 * @param[in] source Source buffer (DEFLATE compressed)
 * @param[in] sourceLen Source buffer length
 * @param[out] dest Destination buffer
 * @param[in,out] destLen Destination buffer size (updated with output size)
 * @return 0 on success, non-zero on error
 */
int __stdcall AsmInflate(
    const unsigned char* source,
    unsigned int sourceLen,
    unsigned char* dest,
    unsigned int* destLen
);

/**
 * @brief Extended create thread function (stub)
 * 
 * Creates a thread with extended options. This is a stub that can be
 * replaced with actual Windows API or optimized implementations.
 * 
 * @param[out] hThread Output thread handle
 * @param[in] threadFunc Thread entry point
 * @param[in] arg Thread argument
 * @param[in] flags Creation flags
 * @return 0 on success, non-zero on error
 */
int __stdcall CreateThreadEx(
    void** hThread,
    unsigned long (__stdcall* threadFunc)(void*),
    void* arg,
    unsigned int flags
);

/**
 * @brief Extended create pipe function (stub)
 * 
 * Creates an inter-process communication pipe with extended options.
 * This is a stub that can be replaced with actual Windows API implementations.
 * 
 * @param[out] hReadPipe Output read pipe handle
 * @param[out] hWritePipe Output write pipe handle
 * @param[in] pipeAttributes Security attributes
 * @param[in] size Buffer size
 * @return 0 on success, non-zero on error
 */
int __stdcall CreatePipeEx(
    void** hReadPipe,
    void** hWritePipe,
    void* pipeAttributes,
    unsigned int size
);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_RUNTIME_HPP
