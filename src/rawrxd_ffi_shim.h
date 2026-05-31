/**
 * rawrxd_ffi_shim.h
 * C++17 FFI exports for Node.js bridge
 * Provides safe opaque handles to MASM internals
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef void* RawrXD_Context;
typedef void* RawrXD_ComposerSession;
typedef void* RawrXD_RingBuffer;

// Core initialization
__declspec(dllexport) RawrXD_Context __stdcall rawrxd_init(const char* gguf_path);
__declspec(dllexport) void __stdcall rawrxd_free(RawrXD_Context ctx);

// Multi-File Composer (Phase 16)
__declspec(dllexport) RawrXD_ComposerSession __stdcall rawrxd_composer_init(
    RawrXD_Context ctx,
    const char* json_payload,
    void (__stdcall *status_callback)(const char* status_json, void* user_data),
    void* user_data
);

__declspec(dllexport) int __stdcall rawrxd_composer_apply(
    RawrXD_ComposerSession session,
    const char* json_payload,
    void (__stdcall *result_callback)(const char* result_json, void* user_data),
    void* user_data
);

__declspec(dllexport) int __stdcall rawrxd_composer_rollback(
    RawrXD_ComposerSession session,
    const char* json_payload
);

__declspec(dllexport) const char* __stdcall rawrxd_composer_poll_status(
    RawrXD_ComposerSession session,
    char* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif
