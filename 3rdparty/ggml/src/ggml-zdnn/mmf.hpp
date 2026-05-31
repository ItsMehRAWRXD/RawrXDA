#ifndef GGML_RXD_ZDNN_MMF_HPP
#define GGML_RXD_ZDNN_MMF_HPP

#include "common.hpp"

void ggml_rxd_zdnn_mul_mat_f(
    const ggml_rxd_backend_zdnn_context * ctx,
    const               ggml_rxd_tensor * src0,
    const               ggml_rxd_tensor * src1,
                        ggml_rxd_tensor * dst);

#endif  // GGML_RXD_ZDNN_MMF_HPP
