//
// MIT license
// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: MIT
//

//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#ifndef GGML_RXD_SYCL_SOFTMAX_HPP
#define GGML_RXD_SYCL_SOFTMAX_HPP

#include "common.hpp"

#define SYCL_SOFT_MAX_BLOCK_SIZE 1024

void ggml_rxd_sycl_op_soft_max(ggml_rxd_backend_sycl_context &ctx, ggml_rxd_tensor *dst);

void ggml_rxd_sycl_op_soft_max_back(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

#endif // GGML_RXD_SYCL_SOFTMAX_HPP
