/* ============================================================================
 * RawrXD zconf.h - From-scratch zlib configuration
 * Compatible with zlib 1.3 API
 * ============================================================================ */
#ifndef RAWRXD_ZCONF_H
#define RAWRXD_ZCONF_H

/* Type definitions compatible with zlib */
#include <stddef.h>

#ifdef _WIN32
#  include <stdint.h>
#endif

#ifndef OF
#  define OF(args)  args
#endif

#ifndef FAR
#  define FAR
#endif

typedef unsigned char  Byte;
typedef Byte  FAR      Bytef;
typedef unsigned int   uInt;
typedef unsigned long  uLong;
typedef char  FAR      charf;
typedef int   FAR      intf;
typedef uInt  FAR      uIntf;
typedef uLong FAR      uLongf;
typedef void const    *voidpc;
typedef void  FAR     *voidpf;
typedef void          *voidp;
typedef unsigned long  z_crc_t;

/* Size type */
#ifdef _WIN64
typedef unsigned __int64 z_size_t;
#else
typedef unsigned long z_size_t;
#endif

#ifndef MAX_MEM_LEVEL
#  define MAX_MEM_LEVEL 9
#endif

#ifndef MAX_WBITS
#  define MAX_WBITS   15
#endif

#endif /* RAWRXD_ZCONF_H */
