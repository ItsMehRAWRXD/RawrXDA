/* ============================================================================
 * RawrXD from-scratch lz4frame.h compatibility header
 * Provides LZ4 Frame API declarations - returns errors at runtime
 * ============================================================================ */
#ifndef RAWRXD_LZ4FRAME_H
#define RAWRXD_LZ4FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LZ4F_VERSION 100

typedef size_t LZ4F_errorCode_t;
typedef struct LZ4F_dctx_s LZ4F_dctx;

typedef struct {
    unsigned blockSizeID;
    unsigned blockMode;
    unsigned contentChecksumFlag;
    unsigned frameType;
    unsigned long long contentSize;
    unsigned dictID;
    unsigned blockChecksumFlag;
} LZ4F_frameInfo_t;

typedef struct {
    LZ4F_frameInfo_t frameInfo;
    unsigned compressionLevel;
    unsigned autoFlush;
    unsigned favorDecSpeed;
} LZ4F_preferences_t;

static inline unsigned LZ4F_isError(LZ4F_errorCode_t code) {
    return (code > (size_t)-16) ? 1 : 0;
}

static inline const char* LZ4F_getErrorName(LZ4F_errorCode_t code) {
    (void)code;
    return "LZ4 not available (from-scratch stub)";
}

static inline LZ4F_errorCode_t LZ4F_createDecompressionContext(
    LZ4F_dctx** dctxPtr, unsigned version) {
    (void)version;
    *dctxPtr = NULL;
    return (size_t)-1;
}

static inline LZ4F_errorCode_t LZ4F_freeDecompressionContext(LZ4F_dctx* dctx) {
    (void)dctx;
    return 0;
}

static inline size_t LZ4F_getFrameInfo(LZ4F_dctx* dctx,
                                        LZ4F_frameInfo_t* frameInfoPtr,
                                        const void* srcBuffer,
                                        size_t* srcSizePtr) {
    (void)dctx; (void)frameInfoPtr; (void)srcBuffer; (void)srcSizePtr;
    return (size_t)-1;
}

static inline size_t LZ4F_decompress(LZ4F_dctx* dctx,
                                      void* dstBuffer, size_t* dstSizePtr,
                                      const void* srcBuffer, size_t* srcSizePtr,
                                      const void* decompressOptionsPtr) {
    (void)dctx; (void)dstBuffer; (void)dstSizePtr;
    (void)srcBuffer; (void)srcSizePtr; (void)decompressOptionsPtr;
    return (size_t)-1;
}

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_LZ4FRAME_H */
