#pragma once
#include "backend.hpp"
#include "ggml.h"

void ggml_rxd_sycl_op_set(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
