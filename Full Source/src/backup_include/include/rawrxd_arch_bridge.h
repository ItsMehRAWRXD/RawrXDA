// rawrxd_arch_bridge.h - C++20 to MASM64 boundary
#pragma once
#include <cstddef>
#include <cstdint>


#ifdef __cplusplus
extern "C"
{
#endif

    void RawrXD_SGEMM_AVX2(const float* A, const float* B, float* C, int64_t M, int64_t N, int64_t K);
    void RawrXD_SGEMM_AVX512(const float* A, const float* B, float* C, int64_t M, int64_t N, int64_t K);

    void RawrXD_FlashAttention_Fwd(const float* Q, const float* K, const float* V, float* O, int64_t N, int64_t D,
                                   float scale);

    void RawrXD_Dequant_Q4_0(const void* blocks, float* output, int64_t n);
    void RawrXD_Dequant_Q4_K(const void* blocks, float* output, int64_t n);
    void RawrXD_Dequant_Q6_K(const void* blocks, float* output, int64_t n);

    bool RawrXD_MemPatch(void* target, const uint8_t* bytes, size_t len);
    void* RawrXD_MemFind(const uint8_t* hay, size_t hlen, const uint8_t* needle, size_t nlen);

    size_t RawrCodex_Disasm(const uint8_t* code, size_t len, void* out, size_t outsize, uint64_t base);

#ifdef __cplusplus
}
#endif

namespace RawrXD::Arch
{
inline bool HasAVX512()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 16)) != 0;
}
inline void SGEMM(const float* A, const float* B, float* C, int64_t M, int64_t N, int64_t K)
{
    if (HasAVX512())
        RawrXD_SGEMM_AVX512(A, B, C, M, N, K);
    else
        RawrXD_SGEMM_AVX2(A, B, C, M, N, K);
}
}  // namespace RawrXD::Arch
