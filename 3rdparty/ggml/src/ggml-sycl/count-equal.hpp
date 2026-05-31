#ifndef GGML_RXD_SYCL_COUNT_EQUAL_HPP
#define GGML_RXD_SYCL_COUNT_EQUAL_HPP
#include "common.hpp"

#define SYCL_COUNT_EQUAL_CHUNK_SIZE 128

void ggml_rxd_sycl_count_equal(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

#endif //GGML_RXD_SYCL_COUNT_EQUAL_HPP
