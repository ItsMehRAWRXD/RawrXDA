#pragma once

// Repository-local GGML shim header.
// Prefer the real backend header from `3rdparty/ggml/include` so ggml sources
// build with the correct types and API surface.

#if defined(__cplusplus)
#if __has_include("../3rdparty/ggml/include/ggml-backend.h")
#include "../3rdparty/ggml/include/ggml-backend.h"
#else
#error "Real GGML backend header not found at ../3rdparty/ggml/include/ggml-backend.h"
#endif
#else
#include "../3rdparty/ggml/include/ggml-backend.h"
#endif
