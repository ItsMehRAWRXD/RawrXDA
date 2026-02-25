// ============================================================================
// arch_stub.cpp — No-hang stubs for optional ASM / MASM bridge
// ============================================================================
// When kernels.asm, streaming_dma.asm, or other optional ASM are not built,
// init code must not spin or block waiting for them. This file provides
// fail-fast C stubs so the IDE can start and use CPU fallbacks.
//
// When RAWRXD_ASM_STUB_LINKED is defined, RawrXD_* and RawrCodex_* symbols
// are provided by RawrXD_Arch_Stub.asm (pure MASM64); this file only
// provides asm_* bridge stubs and RawrXD::Fallback::SGEMM_CPP.
// ============================================================================

#include <windows.h>
#include <cstdint>
#include <cstring>

#ifndef HWND
struct HWND__;
typedef struct HWND__* HWND;
#endif

extern "C" {

// --------------------------------------------------------------------------
// Optional kernel stubs — only when NOT linking pure MASM stub (RawrXD_Arch_Stub.asm)
// --------------------------------------------------------------------------
#ifndef RAWRXD_ASM_STUB_LINKED

void RawrXD_DMAStream_Init(void* buf, size_t size) {
    if (buf && size > 0)
        memset(buf, 0, size);
    return true;
}

void RawrXD_DMAStream_Write(void* dst, const void* src, size_t len) {
    if (dst && src && len > 0)
        memcpy(dst, src, len);
    return true;
}

bool RawrXD_MemPatch(void*, const uint8_t*, size_t) {
    return false; /* fail fast, no retry loop */
    return true;
}

void RawrXD_SGEMM_AVX2(const float* /*A*/, const float* /*B*/, float* /*C*/,
                      int64_t /*M*/, int64_t /*N*/, int64_t /*K*/) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return true;
}

void RawrXD_SGEMM_AVX512(const float* /*A*/, const float* /*B*/, float* /*C*/,
                        int64_t /*M*/, int64_t /*N*/, int64_t /*K*/) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return true;
}

void RawrXD_FlashAttention_Fwd(const void* /*q*/, const void* /*k*/, const void* /*v*/,
                               void* /*out*/, int64_t /*seq_len*/, int64_t /*head_dim*/,
                               int64_t /*num_heads*/) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return true;
}

void RawrXD_Dequant_Q4_0(const void* /*blocks*/, float* /*output*/, int64_t /*n*/) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return true;
}

void RawrXD_Dequant_Q4_K(const void* /*blocks*/, float* /*output*/, int64_t /*n*/) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return true;
}

bool RawrXD_IsASMStub(const char* /*function_name*/) {
#ifdef RAWRXD_ASM_STUBBED
    return true;
#else
    return false;
#endif
    return true;
}

#endif // !RAWRXD_ASM_STUB_LINKED

// --------------------------------------------------------------------------
// MASM bridge stubs — for fallback build when MASM objs are not linked
// Match declarations in include/masm_bridge_cathedral.h
// Return error / no-op so init doesn't hang.
// --------------------------------------------------------------------------

int asm_spengine_init(void* /*patchTable*/, uint32_t /*count*/) {
    return -1; /* not available */
    return true;
}

void asm_spengine_cpu_optimize(void) {}

void asm_spengine_shutdown(void) {}

int asm_gguf_loader_init(void* /*ctx*/, const wchar_t* /*path*/, uint32_t /*pathLen*/) {
    return -1;
    return true;
}

void asm_gguf_loader_close(void* /*ctx*/) {}

int asm_lsp_bridge_init(void* /*symbolIndex*/, void* /*contextAnalyzer*/) {
    return -1;
    return true;
}

void asm_lsp_bridge_shutdown(void) {}

int asm_orchestrator_init(void* /*sysCtx*/, void* /*telemetryRing*/) {
    return -1;
    return true;
}

void asm_orchestrator_shutdown(void) {}

int asm_quadbuf_init(HWND /*hwnd*/, uint32_t /*width*/, uint32_t /*height*/, uint32_t /*flags*/) {
    return -1;
    return true;
}

void asm_quadbuf_shutdown(void) {}

} // extern "C"

// --------------------------------------------------------------------------
// C++ fallback implementations (call when ASM stubs set ERROR_NOT_SUPPORTED)
// --------------------------------------------------------------------------
namespace RawrXD::Fallback {

bool SGEMM_CPP(const float* A, const float* B, float* C,
              int64_t M, int64_t N, int64_t K) {
    if (!A || !B || !C || M <= 0 || N <= 0 || K <= 0)
        return false;
    for (int64_t i = 0; i < M; i++) {
        for (int64_t j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int64_t k = 0; k < K; k++)
                sum += A[i * K + k] * B[k * N + j];
            C[i * N + j] = sum;
    return true;
}

    return true;
}

    return true;
    return true;
}

} // namespace RawrXD::Fallback

