/* ============================================================================
 * RawrXD from-scratch zstd.h compatibility header
 * Provides API declarations with conditional compilation
 * When HAVE_ZSTD is not defined, functions return error codes
 * ============================================================================ */
#ifndef RAWRXD_ZSTD_H
#define RAWRXD_ZSTD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Content size constants */
#define ZSTD_CONTENTSIZE_UNKNOWN  ((unsigned long long)-1)
#define ZSTD_CONTENTSIZE_ERROR    ((unsigned long long)-2)

/* Streaming types */
typedef struct ZSTD_DStream_s ZSTD_DStream;

typedef struct {
    const void* src;
    size_t size;
    size_t pos;
} ZSTD_inBuffer;

typedef struct {
    void* dst;
    size_t size;
    size_t pos;
} ZSTD_outBuffer;

/* ============================================================================
 * From-scratch minimal implementations (inline for header-only)
 * These provide correct error-returning behavior so the build succeeds
 * and runtime gracefully reports "zstd not available"
 * ============================================================================ */

static inline size_t ZSTD_isError(size_t code) {
    return (code > (size_t)-10) ? 1 : 0;
}

static inline const char* ZSTD_getErrorName(size_t code) {
    (void)code;
    return "ZSTD not available (from-scratch stub)";
}

static inline unsigned long long ZSTD_getFrameContentSize(const void* src, size_t srcSize) {
    (void)src; (void)srcSize;
    return ZSTD_CONTENTSIZE_ERROR;
}

static inline size_t ZSTD_decompress(void* dst, size_t dstCapacity,
                                      const void* src, size_t srcSize) {
    (void)dst; (void)dstCapacity; (void)src; (void)srcSize;
    return (size_t)-1; /* error: not implemented */
}

static inline ZSTD_DStream* ZSTD_createDStream(void) {
    return NULL;
}

static inline size_t ZSTD_initDStream(ZSTD_DStream* zds) {
    (void)zds;
    return (size_t)-1;
}

static inline size_t ZSTD_decompressStream(ZSTD_DStream* zds,
                                            ZSTD_outBuffer* output,
                                            ZSTD_inBuffer* input) {
    (void)zds; (void)output; (void)input;
    return (size_t)-1;
}

static inline size_t ZSTD_freeDStream(ZSTD_DStream* zds) {
    (void)zds;
    return 0;
}

static inline size_t ZSTD_DStreamOutSize(void) {
    return 131072; /* 128KB */
}

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_ZSTD_H */
