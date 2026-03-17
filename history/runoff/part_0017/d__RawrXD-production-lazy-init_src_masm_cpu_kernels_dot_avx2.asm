.intel_syntax noprefix

; MASM Intel-style syntax for ml64
; float __cdecl dot_avx2(const float* a, const float* b, size_t len)
; Windows x64: rcx = a, rdx = b, r8 = len

PUBLIC dot_avx2
dot_avx2 PROC
    push rbp
    push rbx
    vzeroupper

    mov rax, r8          ; rax = len

    vxorps ymm0, ymm0, ymm0

    cmp rax, 8
    jl scalar_loop

loop8:
    vmovaps ymm1, ymmword ptr [rcx]
    vmovaps ymm2, ymmword ptr [rdx]
    vmulps ymm1, ymm1, ymm2
    vaddps ymm0, ymm0, ymm1
    add rcx, 32
    add rdx, 32
    sub rax, 8
    cmp rax, 7
    jg loop8

    ; horizontal add of ymm0 -> xmm0
    vextractf128 xmm1, ymm0, 1
    vaddps xmm0, xmm1, xmm0
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    cmp rax, 0
    je done

scalar_loop:
    ; scalar tail handling will be handled by caller after partial sum in xmm0
    vextractf128 xmm1, ymm0, 1
    vaddps xmm0, xmm1, xmm0
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

done:
    vzeroupper
    pop rbx
    pop rbp
    ret
dot_avx2 ENDP

END