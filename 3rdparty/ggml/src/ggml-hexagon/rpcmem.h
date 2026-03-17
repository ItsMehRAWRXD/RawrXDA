#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hexagon RPC Memory Stubs

static inline void* rpcmem_alloc(int heapid, uint32_t flags, int size) {
    return malloc(size);
}

static inline void rpcmem_free(void* ptr) {
    free(ptr);
}

static inline int rpcmem_to_fd(void* ptr) {
    return -1;
}

#ifdef __cplusplus
}
#endif
