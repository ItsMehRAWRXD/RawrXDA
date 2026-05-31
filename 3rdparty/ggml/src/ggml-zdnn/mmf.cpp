#include "ggml.h"
#include "mmf.hpp"

void ggml_rxd_zdnn_mul_mat_f(
    const ggml_rxd_backend_zdnn_context * ctx,
    const               ggml_rxd_tensor * src0,
    const               ggml_rxd_tensor * src1,
                        ggml_rxd_tensor * dst) {
    GGML_RXD_TENSOR_BINARY_OP_LOCALS;

    const enum ggml_rxd_type type = src0->type;

    GGML_RXD_ASSERT(ne0 == ne01);
    GGML_RXD_ASSERT(ne1 == ne11);
    GGML_RXD_ASSERT(ne2 == ne12);
    GGML_RXD_ASSERT(ne3 == ne13);

    // we don't support permuted src0 or src1
    GGML_RXD_ASSERT(nb00 == ggml_rxd_type_size(type));
    GGML_RXD_ASSERT(nb10 == ggml_rxd_type_size(src1->type));

    // dst cannot be transposed or permuted
    GGML_RXD_ASSERT(nb0 == sizeof(float));
    GGML_RXD_ASSERT(nb0 <= nb1);
    GGML_RXD_ASSERT(nb1 <= nb2);
    GGML_RXD_ASSERT(nb2 <= nb3);

    const ggml_rxd_tensor * weights = src0;
    const ggml_rxd_tensor * inputs  = src1;
          ggml_rxd_tensor * output  = dst;

    ggml_rxd_backend_zdnn_buffer * weights_extra = (ggml_rxd_backend_zdnn_buffer *)weights->extra;
    ggml_rxd_backend_zdnn_buffer * inputs_extra  = (ggml_rxd_backend_zdnn_buffer *)inputs->extra;
    ggml_rxd_backend_zdnn_buffer * output_extra  = (ggml_rxd_backend_zdnn_buffer *)output->extra;
    ggml_rxd_backend_zdnn_buffer * bias_extra    = (ggml_rxd_backend_zdnn_buffer *)output_extra->extra;

    const int64_t weights_rows = ne01;
    const int64_t weights_cols = ne00;
    const int64_t inputs_rows  = ne11;
    const int64_t inputs_cols  = ne10;

    assert(inputs_cols == weights_cols);

    const int64_t output_rows = ne1;
    const int64_t output_cols = ne0;

    // GGML_RXD_LOG_INFO("%s: tensor '%s' tensor dimensions: [%ld, %ld, %ld, %ld] pre_tfm_desc dimensions: [%ld, %ld, %ld, %ld]\n",
    //               __func__, weights_extra->name,
    //               weights->ne[3], weights->ne[2], weights->ne[1], weights->ne[0],
    //               weights_extra->pre_tfm_desc.dim1,
    //               weights_extra->pre_tfm_desc.dim2,
    //               weights_extra->pre_tfm_desc.dim3,
    //               weights_extra->pre_tfm_desc.dim4);

    // GGML_RXD_LOG_INFO("%s: tensor '%s' tensor dimensions: [%ld, %ld, %ld, %ld] pre_tfm_desc dimensions: [%ld, %ld, %ld, %ld]\n",
    //               __func__, inputs_extra->name,
    //               inputs->ne[3], inputs->ne[2], inputs->ne[1], inputs->ne[0],
    //               inputs_extra->pre_tfm_desc.dim1,
    //               inputs_extra->pre_tfm_desc.dim2,
    //               inputs_extra->pre_tfm_desc.dim3,
    //               inputs_extra->pre_tfm_desc.dim4);

    GGML_RXD_ASSERT(weights_extra->pre_tfm_desc.dim1 == weights->ne[0] && "weights_extra->pre_tfm_desc.dim1 must match weights->ne[0]");
    GGML_RXD_ASSERT(weights_extra->pre_tfm_desc.dim2 == weights->ne[1] && "weights_extra->pre_tfm_desc.dim2 must match weights->ne[1]");
    GGML_RXD_ASSERT(inputs_extra->pre_tfm_desc.dim1  == inputs->ne[0]  && "inputs_extra->pre_tfm_desc.dim1 must match inputs->ne[0]");
    GGML_RXD_ASSERT(inputs_extra->pre_tfm_desc.dim2  == inputs->ne[1]  && "inputs_extra->pre_tfm_desc.dim2 must match inputs->ne[1]");

    ZDNN_CHECK(zdnn_matmul_transpose_op(&inputs_extra->ztensor, &weights_extra->ztensor, &bias_extra->ztensor,
                                        false, true, MATMUL_OP_ADDITION, &output_extra->ztensor));
    // TODO: Remove in the future as we are currently DLF16 -> FP32 then in the next op, FP32 -> DLF16 again. Inefficient.
    ZDNN_CHECK(zdnn_transform_origtensor(&output_extra->ztensor, output->data));

    GGML_RXD_UNUSED(ctx);
    GGML_RXD_UNUSED(weights_rows);
    GGML_RXD_UNUSED(weights_cols);
    GGML_RXD_UNUSED(inputs_rows);
    GGML_RXD_UNUSED(inputs_cols);
    GGML_RXD_UNUSED(output_rows);
    GGML_RXD_UNUSED(output_cols);
}
