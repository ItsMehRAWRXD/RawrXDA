; ============================================================================
; AVX-512 KV Accumulation Kernel
; dst[i] += src[i] for i in [0, count)
; Win64 ABI:
;   rcx = src
;   rdx = dst
;   r8d = count (float elements)
; Return:
;   eax = 1 success, 0 invalid args
; ============================================================================

PUBLIC kv_accumulate_avx512
PUBLIC kv_accumulate_scaled_avx512

.code

kv_accumulate_avx512 PROC
    test rcx, rcx
    jz kv_fail
    test rdx, rdx
    jz kv_fail
    test r8d, r8d
    jle kv_fail

    sub rsp, 640
    vmovups ZMMWORD PTR [rsp + 000h], zmm6
    vmovups ZMMWORD PTR [rsp + 040h], zmm7
    vmovups ZMMWORD PTR [rsp + 080h], zmm8
    vmovups ZMMWORD PTR [rsp + 0C0h], zmm9
    vmovups ZMMWORD PTR [rsp + 100h], zmm10
    vmovups ZMMWORD PTR [rsp + 140h], zmm11
    vmovups ZMMWORD PTR [rsp + 180h], zmm12
    vmovups ZMMWORD PTR [rsp + 1C0h], zmm13
    vmovups ZMMWORD PTR [rsp + 200h], zmm14
    vmovups ZMMWORD PTR [rsp + 240h], zmm15

    mov r9d, r8d
    shr r9d, 7                  ; 8 zmm blocks = 128 floats
    jz kv_loop16_setup

kv_loop128:
    vmovups zmm0, ZMMWORD PTR [rdx + 000h]
    vmovups zmm1, ZMMWORD PTR [rdx + 040h]
    vmovups zmm2, ZMMWORD PTR [rdx + 080h]
    vmovups zmm3, ZMMWORD PTR [rdx + 0C0h]
    vmovups zmm4, ZMMWORD PTR [rdx + 100h]
    vmovups zmm5, ZMMWORD PTR [rdx + 140h]
    vmovups zmm6, ZMMWORD PTR [rdx + 180h]
    vmovups zmm7, ZMMWORD PTR [rdx + 1C0h]

    vaddps  zmm0, zmm0, ZMMWORD PTR [rcx + 000h]
    vaddps  zmm1, zmm1, ZMMWORD PTR [rcx + 040h]
    vaddps  zmm2, zmm2, ZMMWORD PTR [rcx + 080h]
    vaddps  zmm3, zmm3, ZMMWORD PTR [rcx + 0C0h]
    vaddps  zmm4, zmm4, ZMMWORD PTR [rcx + 100h]
    vaddps  zmm5, zmm5, ZMMWORD PTR [rcx + 140h]
    vaddps  zmm6, zmm6, ZMMWORD PTR [rcx + 180h]
    vaddps  zmm7, zmm7, ZMMWORD PTR [rcx + 1C0h]

    vmovups ZMMWORD PTR [rdx + 000h], zmm0
    vmovups ZMMWORD PTR [rdx + 040h], zmm1
    vmovups ZMMWORD PTR [rdx + 080h], zmm2
    vmovups ZMMWORD PTR [rdx + 0C0h], zmm3
    vmovups ZMMWORD PTR [rdx + 100h], zmm4
    vmovups ZMMWORD PTR [rdx + 140h], zmm5
    vmovups ZMMWORD PTR [rdx + 180h], zmm6
    vmovups ZMMWORD PTR [rdx + 1C0h], zmm7

    add rcx, 512
    add rdx, 512
    dec r9d
    jnz kv_loop128

kv_loop16_setup:
    and r8d, 127
    mov r9d, r8d
    shr r9d, 4                  ; remaining zmm blocks of 16 floats
    jz kv_tail

kv_loop16:
    vmovups zmm0, ZMMWORD PTR [rdx]
    vaddps  zmm0, zmm0, ZMMWORD PTR [rcx]
    vmovups ZMMWORD PTR [rdx], zmm0
    add rcx, 64
    add rdx, 64
    dec r9d
    jnz kv_loop16

kv_tail:
    and r8d, 15
    jz kv_ok

kv_tail_loop:
    vmovss xmm0, DWORD PTR [rdx]
    vaddss xmm0, xmm0, DWORD PTR [rcx]
    vmovss DWORD PTR [rdx], xmm0
    add rcx, 4
    add rdx, 4
    dec r8d
    jnz kv_tail_loop

kv_ok:
    vmovups zmm6,  ZMMWORD PTR [rsp + 000h]
    vmovups zmm7,  ZMMWORD PTR [rsp + 040h]
    vmovups zmm8,  ZMMWORD PTR [rsp + 080h]
    vmovups zmm9,  ZMMWORD PTR [rsp + 0C0h]
    vmovups zmm10, ZMMWORD PTR [rsp + 100h]
    vmovups zmm11, ZMMWORD PTR [rsp + 140h]
    vmovups zmm12, ZMMWORD PTR [rsp + 180h]
    vmovups zmm13, ZMMWORD PTR [rsp + 1C0h]
    vmovups zmm14, ZMMWORD PTR [rsp + 200h]
    vmovups zmm15, ZMMWORD PTR [rsp + 240h]
    add rsp, 640
    vzeroupper
    mov eax, 1
    ret

kv_fail:
    xor eax, eax
    ret
kv_accumulate_avx512 ENDP

; ============================================================================
; AVX-512 fused scaled KV accumulation kernel
; dst[i] += src[i] * scale for i in [0, count)
; Win64 ABI:
;   rcx = src
;   rdx = dst
;   r8d = count (float elements)
;   xmm3 = scale
; Return:
;   eax = 1 success, 0 invalid args
; ============================================================================

kv_accumulate_scaled_avx512 PROC
    test rcx, rcx
    jz kv_scaled_fail
    test rdx, rdx
    jz kv_scaled_fail
    test r8d, r8d
    jle kv_scaled_fail

    sub rsp, 640
    vmovups ZMMWORD PTR [rsp + 000h], zmm6
    vmovups ZMMWORD PTR [rsp + 040h], zmm7
    vmovups ZMMWORD PTR [rsp + 080h], zmm8
    vmovups ZMMWORD PTR [rsp + 0C0h], zmm9
    vmovups ZMMWORD PTR [rsp + 100h], zmm10
    vmovups ZMMWORD PTR [rsp + 140h], zmm11
    vmovups ZMMWORD PTR [rsp + 180h], zmm12
    vmovups ZMMWORD PTR [rsp + 1C0h], zmm13
    vmovups ZMMWORD PTR [rsp + 200h], zmm14
    vmovups ZMMWORD PTR [rsp + 240h], zmm15

    vbroadcastss zmm1, xmm3

    mov r9d, r8d
    shr r9d, 7                  ; 8 zmm blocks = 128 floats
    jz kv_scaled_loop16_setup

kv_scaled_loop128:
    vmovups zmm0, ZMMWORD PTR [rcx + 000h]
    vmovups zmm2, ZMMWORD PTR [rcx + 040h]
    vmovups zmm3, ZMMWORD PTR [rcx + 080h]
    vmovups zmm4, ZMMWORD PTR [rcx + 0C0h]
    vmovups zmm5, ZMMWORD PTR [rcx + 100h]
    vmovups zmm6, ZMMWORD PTR [rcx + 140h]
    vmovups zmm7, ZMMWORD PTR [rcx + 180h]
    vmovups zmm8, ZMMWORD PTR [rcx + 1C0h]

    vmovups zmm9,  ZMMWORD PTR [rdx + 000h]
    vmovups zmm10, ZMMWORD PTR [rdx + 040h]
    vmovups zmm11, ZMMWORD PTR [rdx + 080h]
    vmovups zmm12, ZMMWORD PTR [rdx + 0C0h]
    vmovups zmm13, ZMMWORD PTR [rdx + 100h]
    vmovups zmm14, ZMMWORD PTR [rdx + 140h]
    vmovups zmm15, ZMMWORD PTR [rdx + 180h]

    vfmadd213ps zmm0, zmm1, zmm9
    vfmadd213ps zmm2, zmm1, zmm10
    vfmadd213ps zmm3, zmm1, zmm11
    vfmadd213ps zmm4, zmm1, zmm12
    vfmadd213ps zmm5, zmm1, zmm13
    vfmadd213ps zmm6, zmm1, zmm14
    vfmadd213ps zmm7, zmm1, zmm15

    vmovups zmm15, ZMMWORD PTR [rdx + 1C0h]
    vfmadd213ps zmm8, zmm1, zmm15

    vmovups ZMMWORD PTR [rdx + 000h], zmm0
    vmovups ZMMWORD PTR [rdx + 040h], zmm2
    vmovups ZMMWORD PTR [rdx + 080h], zmm3
    vmovups ZMMWORD PTR [rdx + 0C0h], zmm4
    vmovups ZMMWORD PTR [rdx + 100h], zmm5
    vmovups ZMMWORD PTR [rdx + 140h], zmm6
    vmovups ZMMWORD PTR [rdx + 180h], zmm7
    vmovups ZMMWORD PTR [rdx + 1C0h], zmm8

    add rcx, 512
    add rdx, 512
    dec r9d
    jnz kv_scaled_loop128

kv_scaled_loop16_setup:
    and r8d, 127
    mov r9d, r8d
    shr r9d, 4                  ; remaining zmm blocks of 16 floats
    jz kv_scaled_tail

kv_scaled_loop16:
    vmovups zmm0, ZMMWORD PTR [rcx]
    vmovups zmm2, ZMMWORD PTR [rdx]
    vfmadd213ps zmm0, zmm1, zmm2 ; zmm0 = (src * scale) + dst
    vmovups ZMMWORD PTR [rdx], zmm0
    add rcx, 64
    add rdx, 64
    dec r9d
    jnz kv_scaled_loop16

kv_scaled_tail:
    and r8d, 15
    jz kv_scaled_ok

kv_scaled_tail_loop:
    vmovss xmm0, DWORD PTR [rcx]
    vfmadd213ss xmm0, xmm3, DWORD PTR [rdx] ; xmm0 = (src * scale) + dst
    vmovss DWORD PTR [rdx], xmm0
    add rcx, 4
    add rdx, 4
    dec r8d
    jnz kv_scaled_tail_loop

kv_scaled_ok:
    vmovups zmm6,  ZMMWORD PTR [rsp + 000h]
    vmovups zmm7,  ZMMWORD PTR [rsp + 040h]
    vmovups zmm8,  ZMMWORD PTR [rsp + 080h]
    vmovups zmm9,  ZMMWORD PTR [rsp + 0C0h]
    vmovups zmm10, ZMMWORD PTR [rsp + 100h]
    vmovups zmm11, ZMMWORD PTR [rsp + 140h]
    vmovups zmm12, ZMMWORD PTR [rsp + 180h]
    vmovups zmm13, ZMMWORD PTR [rsp + 1C0h]
    vmovups zmm14, ZMMWORD PTR [rsp + 200h]
    vmovups zmm15, ZMMWORD PTR [rsp + 240h]
    add rsp, 640
    vzeroupper
    mov eax, 1
    ret

kv_scaled_fail:
    xor eax, eax
    ret
kv_accumulate_scaled_avx512 ENDP

END
