#ifndef GGML_RXD_ZDNN_UTILITIES_HPP
#define GGML_RXD_ZDNN_UTILITIES_HPP

#include "common.hpp"

zdnn_data_types ggml_rxd_zdnn_type_mapping(ggml_rxd_type type);

void ggml_rxd_zdnn_create_tensor(zdnn_tensor_desc & pre_tfm_desc,
                             zdnn_tensor_desc & tfm_desc,
                             zdnn_ztensor     & ztensor,
                      const ggml_rxd_tensor       * src,
                      const int64_t           * ne,
                      const zdnn_data_layouts   layout);

void ggml_rxd_zdnn_load_tensor(zdnn_ztensor & ztensor, void * buffer);

void ggml_rxd_zdnn_init_tensor(ggml_rxd_backend_zdnn_buffer * buffer, const ggml_rxd_tensor * tensor);

#endif  // GGML_RXD_ZDNN_UTILITIES_HPP
