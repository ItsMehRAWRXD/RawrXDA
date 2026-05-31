#pragma once

// Repository-local GGML shim header.
// Prefer the real GGML header from `3rdparty/ggml/include` to avoid ODR/type
// redefinition issues when GGML sources are built.
//
// The previous minimal stub definitions were only intended for tiny GGUF-only
// consumers, but they break any target that compiles GGML itself.

#if defined(__cplusplus)
#  if __has_include("../3rdparty/ggml/include/ggml.h")
#    include "../3rdparty/ggml/include/ggml.h"
#  else
#    error "Real GGML header not found at ../3rdparty/ggml/include/ggml.h"
#  endif
#else
#  include "../3rdparty/ggml/include/ggml.h"
#endif
