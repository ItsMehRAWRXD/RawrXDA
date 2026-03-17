#ifndef RAWRXD_SOLOCOMPILER_BACKEND_H
#define RAWRXD_SOLOCOMPILER_BACKEND_H

// This header is for C/C++ consumption.
// For MASM, you would use:
// EXTERN RawrXD_GenerateExecutable:PROC

#ifdef __cplusplus
extern "C" {
#endif

#if defined(RAWRXD_BACKEND_DLL_EXPORT)
    #define RAWRXD_BACKEND_API __declspec(dllexport)
#else
    #define RAWRXD_BACKEND_API __declspec(dllimport)
#endif

RAWRXD_BACKEND_API bool RawrXD_GenerateExecutable(const char* asmSource, const char* outputPath);

#ifdef __cplusplus
}
#endif

#endif // RAWRXD_SOLOCOMPILER_BACKEND_H
