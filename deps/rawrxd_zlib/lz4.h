/* ============================================================================
 * RawrXD from-scratch lz4.h compatibility header
 * Provides API declarations - returns errors at runtime
 * ============================================================================ */
#ifndef RAWRXD_LZ4_H
#define RAWRXD_LZ4_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LZ4_VERSION_MAJOR    1
#define LZ4_VERSION_MINOR    9
#define LZ4_VERSION_RELEASE  4
#define LZ4_VERSION_NUMBER   (LZ4_VERSION_MAJOR *100*100 + LZ4_VERSION_MINOR *100 + LZ4_VERSION_RELEASE)

static inline int LZ4_decompress_safe(const char* src, char* dst,
                                       int compressedSize, int dstCapacity) {
    (void)src; (void)dst; (void)compressedSize; (void)dstCapacity;
    return -1; /* error: not implemented */
}

static inline int LZ4_compressBound(int inputSize) {
    return inputSize + (inputSize / 255) + 16;
}

static inline int LZ4_compress_default(const char* src, char* dst,
                                        int srcSize, int dstCapacity) {
    (void)src; (void)dst; (void)srcSize; (void)dstCapacity;
    return 0; /* error */
}

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_LZ4_H */
