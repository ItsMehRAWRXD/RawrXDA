#pragma once
#include "ggml.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void * ggml_backend_t;
typedef void * ggml_backend_buffer_t;
typedef void * ggml_backend_event_t;
typedef void * ggml_backend_sched_t;

struct ggml_backend_buffer_type;
typedef struct ggml_backend_buffer_type * ggml_backend_buffer_type_t;

#ifdef __cplusplus
}
#endif
