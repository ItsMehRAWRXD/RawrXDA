/* Fallback C implementation of the orchestrator API for testing when MASM
 * objects are not available. This keeps the test buildable and runnable.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../src/core/stream_orch.h"

typedef struct {
    void **ring;
    uint64_t capacity;
    uint64_t head;
    uint64_t tail;
} stream_ctl_impl;

int orchestrator_init(void) {
    return SR_OK;
}

stream_ctl_t* stream_attach(void* ring_base, uint64_t capacity) {
    if (!ring_base || capacity == 0) return NULL;
    stream_ctl_impl* ctl = (stream_ctl_impl*)malloc(sizeof(stream_ctl_impl));
    if (!ctl) return NULL;
    ctl->ring = (void**)ring_base;
    ctl->capacity = capacity;
    ctl->head = 0;
    ctl->tail = 0;
    return (stream_ctl_t*)ctl;
}

int stream_detach(stream_ctl_t* ctl) {
    if (!ctl) return SR_ERR_INVALID;
    free(ctl);
    return SR_OK;
}

int submit_chunk(stream_ctl_t* ctlp, void* chunk) {
    stream_ctl_impl* ctl = (stream_ctl_impl*)ctlp;
    if (!ctl) return SR_ERR_INVALID;
    uint64_t depth = ctl->tail - ctl->head;
    if (depth >= ctl->capacity) return SR_ERR_BACKPRESSURE;
    uint64_t idx = ctl->tail % ctl->capacity;
    ctl->ring[idx] = chunk;
    ctl->tail++;
    return SR_OK;
}

void* fetch_chunk(stream_ctl_t* ctlp) {
    stream_ctl_impl* ctl = (stream_ctl_impl*)ctlp;
    if (!ctl) return NULL;
    if (ctl->head == ctl->tail) return NULL;
    uint64_t idx = ctl->head % ctl->capacity;
    void* v = ctl->ring[idx];
    ctl->head++;
    return v;
}

void prefetch_stream_window(stream_ctl_t* ctlp, uint64_t start_index, uint64_t count) {
    (void)ctlp; (void)start_index; (void)count; /* no-op fallback */
}
