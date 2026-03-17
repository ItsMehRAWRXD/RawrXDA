#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hexagon DSP Queue Stubs

typedef void* dspqueue_t;

static inline dspqueue_t dspqueue_open(const char* name) {
    return (dspqueue_t)0xDEADBEEF;
}

static inline void dspqueue_close(dspqueue_t q) {
}

static inline int dspqueue_push(dspqueue_t q, void* msg, uint32_t size) {
    return 0;
}

static inline int dspqueue_pop(dspqueue_t q, void* msg, uint32_t size) {
    return -1;
}

#ifdef __cplusplus
}
#endif
