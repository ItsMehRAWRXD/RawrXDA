// omega_kernels.h
// C ABI bridge for RDNA3 HIP kernels used by Omega Singularity orchestration.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Launches an 11x attention pass on HIP/ROCm.
// q_half:   [11, head_dim] in fp16
// k_half:   [seq_len, head_dim] in fp16
// v_f32:    [seq_len, head_dim] in fp32
// out_f32:  [11, head_dim] output in fp32
// stream_handle: optional hip stream (pass 0 for default stream)
// Returns HIP status code (0 == success).
int RawrXD_Omega_Attention11x_HIP(
	const void* q_half,
	const void* k_half,
	const float* v_f32,
	float* out_f32,
	int seq_len,
	int head_dim,
	uintptr_t stream_handle);

#ifdef __cplusplus
}
#endif

