/* ============================================================================
 * RawrXD zlib.h - From-scratch zlib-compatible API header
 * Implements DEFLATE (RFC 1951) with zlib framing (RFC 1950) and gzip (RFC 1952)
 * Written from scratch for the RawrXD IDE build system
 * ============================================================================ */
#ifndef RAWRXD_ZLIB_H
#define RAWRXD_ZLIB_H

#include "zconf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version
 * ============================================================================ */
#define ZLIB_VERSION     "1.3.0-rawrxd"
#define ZLIB_VERNUM      0x1300
#define ZLIB_VER_MAJOR   1
#define ZLIB_VER_MINOR   3
#define ZLIB_VER_REVISION 0
#define ZLIB_VER_SUBREVISION 0

/* ============================================================================
 * Return codes
 * ============================================================================ */
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)

/* ============================================================================
 * Compression levels
 * ============================================================================ */
#define Z_NO_COMPRESSION      0
#define Z_BEST_SPEED          1
#define Z_BEST_COMPRESSION    9
#define Z_DEFAULT_COMPRESSION (-1)

/* ============================================================================
 * Compression strategy
 * ============================================================================ */
#define Z_FILTERED         1
#define Z_HUFFMAN_ONLY     2
#define Z_RLE              3
#define Z_FIXED            4
#define Z_DEFAULT_STRATEGY 0

/* ============================================================================
 * Compression method
 * ============================================================================ */
#define Z_DEFLATED 8

/* ============================================================================
 * Flush values
 * ============================================================================ */
#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
#define Z_BLOCK         5
#define Z_TREES         6

/* ============================================================================
 * Data type values
 * ============================================================================ */
#define Z_BINARY  0
#define Z_TEXT    1
#define Z_ASCII   Z_TEXT
#define Z_UNKNOWN 2

/* ============================================================================
 * z_stream structure
 * ============================================================================ */
typedef voidpf (*alloc_func) OF((voidpf opaque, uInt items, uInt size));
typedef void   (*free_func)  OF((voidpf opaque, voidpf address));

typedef struct z_stream_s {
    const Bytef *next_in;     /* next input byte */
    uInt        avail_in;     /* number of bytes available at next_in */
    uLong       total_in;     /* total number of input bytes read so far */

    Bytef       *next_out;    /* next output byte */
    uInt        avail_out;    /* remaining free space at next_out */
    uLong       total_out;    /* total number of bytes output so far */

    const char  *msg;         /* last error message, NULL if no error */
    void        *state;       /* internal state (opaque) */

    alloc_func  zalloc;       /* used to allocate the internal state */
    free_func   zfree;        /* used to free the internal state */
    voidpf      opaque;       /* private data object passed to zalloc/zfree */

    int         data_type;    /* best guess about the data type */
    uLong       adler;        /* Adler-32 or CRC-32 value of uncompressed data */
    uLong       reserved;     /* reserved for future use */
} z_stream;

typedef z_stream FAR *z_streamp;

/* gz_header for gzip wrapper */
typedef struct gz_header_s {
    int     text;       
    uLong   time;       
    int     xflags;     
    int     os;         
    Bytef   *extra;     
    uInt    extra_len;  
    uInt    extra_max;  
    Bytef   *name;      
    uInt    name_max;   
    Bytef   *comment;   
    uInt    comm_max;   
    int     hcrc;       
    int     done;       
} gz_header;

typedef gz_header FAR *gz_headerp;

/* ============================================================================
 * Compression functions
 * ============================================================================ */

int deflateInit_ OF((z_streamp strm, int level,
                     const char *version, int stream_size));
int deflateInit2_ OF((z_streamp strm, int level, int method,
                      int windowBits, int memLevel, int strategy,
                      const char *version, int stream_size));
int deflate OF((z_streamp strm, int flush));
int deflateEnd OF((z_streamp strm));
int deflateBound OF((z_streamp strm, uLong sourceLen));
int deflateReset OF((z_streamp strm));

/* ============================================================================
 * Decompression functions
 * ============================================================================ */

int inflateInit_ OF((z_streamp strm,
                     const char *version, int stream_size));
int inflateInit2_ OF((z_streamp strm, int windowBits,
                      const char *version, int stream_size));
int inflate OF((z_streamp strm, int flush));
int inflateEnd OF((z_streamp strm));
int inflateReset OF((z_streamp strm));

/* ============================================================================
 * Utility functions
 * ============================================================================ */

int compress OF((Bytef *dest, uLongf *destLen,
                 const Bytef *source, uLong sourceLen));
int compress2 OF((Bytef *dest, uLongf *destLen,
                  const Bytef *source, uLong sourceLen, int level));
uLong compressBound OF((uLong sourceLen));
int uncompress OF((Bytef *dest, uLongf *destLen,
                   const Bytef *source, uLong sourceLen));
int uncompress2 OF((Bytef *dest, uLongf *destLen,
                    const Bytef *source, uLong *sourceLen));

/* ============================================================================
 * Checksum functions
 * ============================================================================ */

uLong adler32 OF((uLong adler, const Bytef *buf, uInt len));
uLong crc32 OF((uLong crc, const Bytef *buf, uInt len));

/* ============================================================================
 * Convenience macros
 * ============================================================================ */

#define deflateInit(strm, level) \
    deflateInit_((strm), (level), ZLIB_VERSION, (int)sizeof(z_stream))

#define inflateInit(strm) \
    inflateInit_((strm), ZLIB_VERSION, (int)sizeof(z_stream))

#define deflateInit2(strm, level, method, windowBits, memLevel, strategy) \
    deflateInit2_((strm), (level), (method), (windowBits), (memLevel), \
                  (strategy), ZLIB_VERSION, (int)sizeof(z_stream))

#define inflateInit2(strm, windowBits) \
    inflateInit2_((strm), (windowBits), ZLIB_VERSION, (int)sizeof(z_stream))

/* ============================================================================
 * Version info
 * ============================================================================ */
const char *zlibVersion OF((void));

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_ZLIB_H */
