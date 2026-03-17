#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hexagon HTP Interface Stubs for Windows Build Parity
// These are normally generated from IDL or provided by Hexagon SDK

typedef void* htp_ctx_t;

typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} htp_version_t;

typedef enum {
    HTP_SUCCESS = 0,
    HTP_ERROR = 1,
    HTP_INVALID_ARGS = 2,
    HTP_OUT_OF_RESOURCES = 3
} htp_status_t;

// Capability check stubs
static inline htp_status_t htp_get_version(htp_version_t* ver) {
    if (!ver) return HTP_INVALID_ARGS;
    ver->major = 1; ver->minor = 0; ver->patch = 0;
    return HTP_SUCCESS;
}

#ifdef __cplusplus
}
#endif
