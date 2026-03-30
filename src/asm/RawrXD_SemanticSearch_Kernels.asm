; =============================================================================
; RawrXD_SemanticSearch_Kernels.asm — Semantic similarity inner loops (MASM64)
; =============================================================================
;
; Hot-path math for zero-copy / mmap-friendly vector search. Uses the same
; Win64 ABI as quant_avx2.asm (see WINDOWS x64 REGISTER PRESERVATION CONTRACT
; in quant_avx2.asm).
;
; RawrXD_Q8_Q8_Dot256
;   Dot product of two plain int8 vectors of exactly 256 elements (no GGUF block
;   headers). Integer dot is accumulated, converted to fp32, then multiplied by
;   the caller-supplied combined scale (e.g. d_a * d_b).
;   RCX = a (int8[256], 16-byte aligned recommended)
;   RDX = b (int8[256])
;   XMM2 = scale (float) — third parameter in MSVC Win64 for (ptr, ptr, float)
;   Returns XMM0 = dot as float
;
; RawrXD_F16F32_Dot
;   Dot product: half-precision vector × fp32 query, length = multiple of 16.
;   RCX = vec_f16 (uint16*)
;   RDX = query_f32 (float*)
;   R8  = num_elements (must be multiple of 16; if 0, returns 0)
;   Returns XMM0 = dot as float
;
; Implementation note: Q8 path uses AVX2 integer widen + vpmulld (same family
; as Quant_VecDotQ8_0_F32). Optional AVX-512 VNNI widening can be layered later.
;
; Build: ml64.exe /c /Zi /Zd /I src/asm RawrXD_SemanticSearch_Kernels.asm
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
; EXPORTS
; =============================================================================
PUBLIC RawrXD_Q8_Q8_Dot256
PUBLIC RawrXD_F16F32_Dot

.code

ALIGN 16
; -----------------------------------------------------------------------------
; RawrXD_Q8_Q8_Dot256
; -----------------------------------------------------------------------------
RawrXD_Q8_Q8_Dot256 PROC FRAME
    .endprolog

    ; YMM4 = 8-lane int32 accumulator
    vxorps  ymm4, ymm4, ymm4
    xor     eax, eax

@@q8_chunk:
    vpmovsxbd ymm0, qword ptr [rcx + rax]
    vpmovsxbd ymm1, qword ptr [rdx + rax]
    vpmulld ymm2, ymm0, ymm1
    vpaddd  ymm4, ymm4, ymm2
    add     rax, 8
    cmp     rax, 256
    jl      @@q8_chunk

    ; Horizontal sum of 8 int32 lanes in ymm4 -> EAX
    vextractf128 xmm0, ymm4, 1
    vpaddd  xmm0, xmm0, xmm4
    vpshufd xmm1, xmm0, 0Eh
    vpaddd  xmm0, xmm0, xmm1
    vpshufd xmm1, xmm0, 01h
    vpaddd  xmm0, xmm0, xmm1
    vmovd   eax, xmm0

    vcvtsi2ss xmm0, xmm1, eax
    vmulss  xmm0, xmm0, xmm2

    vzeroupper
    ret
RawrXD_Q8_Q8_Dot256 ENDP

ALIGN 16
; -----------------------------------------------------------------------------
; RawrXD_F16F32_Dot
; -----------------------------------------------------------------------------
RawrXD_F16F32_Dot PROC FRAME
    .endprolog

    vxorps  ymm4, ymm4, ymm4
    cmp     r8, 0
    je      @@f16_done

@@f16_16:
    cmp     r8, 16
    jb      @@f16_done

    vmovdqu ymm0, ymmword ptr [rcx]
    vcvtph2ps ymm2, xmm0
    vextractf128 xmm1, ymm0, 1
    vcvtph2ps ymm3, xmm1

    vmovaps ymm5, ymmword ptr [rdx]
    vmovaps ymm6, ymmword ptr [rdx + 32]

    vfmadd231ps ymm4, ymm2, ymm5
    vfmadd231ps ymm4, ymm3, ymm6

    add     rcx, 32
    add     rdx, 64
    sub     r8, 16
    jmp     @@f16_16

@@f16_done:
    vextractf128 xmm0, ymm4, 1
    vaddps  xmm0, xmm0, xmm4
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    vzeroupper
    ret
RawrXD_F16F32_Dot ENDP

END
