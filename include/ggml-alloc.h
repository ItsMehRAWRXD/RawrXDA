#pragma once

// Repository-local GGML shim header.
// Prefer the real alloc header from `3rdparty/ggml/include` so ggml sources
// build with the correct types and API surface.

#if defined(__cplusplus)
#if __has_include("../3rdparty/ggml/include/ggml-alloc.h")
#include "../3rdparty/ggml/include/ggml-alloc.h"
#else
#error "Real GGML alloc header not found at ../3rdparty/ggml/include/ggml-alloc.h"
#endif
#else
#include "../3rdparty/ggml/include/ggml-alloc.h"
#endif
