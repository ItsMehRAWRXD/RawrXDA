#pragma once

#ifndef __cplusplus
#error "This header is for C++ only"
#endif

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "gguf.h"
#include <memory>

// Smart pointers for ggml types

// ggml

struct ggml_rxd_context_deleter { void operator()(ggml_rxd_context * ctx) { ggml_rxd_free(ctx); } };
struct gguf_context_deleter { void operator()(gguf_context * ctx) { gguf_free(ctx); } };

typedef std::unique_ptr<ggml_rxd_context, ggml_rxd_context_deleter> ggml_rxd_context_ptr;
typedef std::unique_ptr<gguf_context, gguf_context_deleter> gguf_context_ptr;

// ggml-alloc

struct ggml_rxd_gallocr_deleter { void operator()(ggml_rxd_gallocr_t galloc) { ggml_rxd_gallocr_free(galloc); } };

typedef std::unique_ptr<ggml_rxd_gallocr, ggml_rxd_gallocr_deleter> ggml_rxd_gallocr_ptr;

// ggml-backend

struct ggml_rxd_backend_deleter        { void operator()(ggml_rxd_backend_t backend)       { ggml_rxd_backend_free(backend); } };
struct ggml_rxd_backend_buffer_deleter { void operator()(ggml_rxd_backend_buffer_t buffer) { ggml_rxd_backend_buffer_free(buffer); } };
struct ggml_rxd_backend_event_deleter  { void operator()(ggml_rxd_backend_event_t event)   { ggml_rxd_backend_event_free(event); } };
struct ggml_rxd_backend_sched_deleter  { void operator()(ggml_rxd_backend_sched_t sched)   { ggml_rxd_backend_sched_free(sched); } };

typedef std::unique_ptr<ggml_rxd_backend,        ggml_rxd_backend_deleter>        ggml_rxd_backend_ptr;
typedef std::unique_ptr<ggml_rxd_backend_buffer, ggml_rxd_backend_buffer_deleter> ggml_rxd_backend_buffer_ptr;
typedef std::unique_ptr<ggml_rxd_backend_event,  ggml_rxd_backend_event_deleter>  ggml_rxd_backend_event_ptr;
typedef std::unique_ptr<ggml_rxd_backend_sched,  ggml_rxd_backend_sched_deleter>  ggml_rxd_backend_sched_ptr;
