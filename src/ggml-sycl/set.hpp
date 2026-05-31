#pragma once
#include "backend.hpp"
#include "ggml_rxd_internal.h"

void ggml_sycl_op_set(ggml_backend_sycl_context & ctx, ggml_tensor * dst);
