#ifndef RAWRXD_SPECSTRINGS_STRICT_STUB
#define RAWRXD_SPECSTRINGS_STRICT_STUB
// Stub definitions for Windows SDK annotation macros when specstrings_strict.h is unavailable.
// These expand to nothing and are only to satisfy the compiler in environments missing the header.
// If the real header becomes available, remove this stub.
#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif
#ifndef _Out_opt_
#define _Out_opt_
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _Inout_opt_
#define _Inout_opt_
#endif
#ifndef _In_reads_
#define _In_reads_(x)
#endif
#ifndef _Out_writes_
#define _Out_writes_(x)
#endif
#ifndef _Outptr_result_maybenull_
#define _Outptr_result_maybenull_
#endif
#ifndef _Success_
#define _Success_(expr)
#endif
#ifndef _Check_return_
#define _Check_return_
#endif
#ifndef _Ret_notnull_
#define _Ret_notnull_
#endif
#ifndef _Use_decl_annotations_
#define _Use_decl_annotations_
#endif
#endif // RAWRXD_SPECSTRINGS_STRICT_STUB
