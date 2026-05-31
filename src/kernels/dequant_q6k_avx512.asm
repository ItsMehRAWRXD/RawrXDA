; ============================================================================
; AVX-512 VPMULTISHIFTQB Dequantization Kernel for Q6_K Format (KV-Cache)
; ============================================================================

.data
; VPMULTISHIFTQB Control Mask for 6-bit extraction (8-bit targets)
q6k_extract_mask DB 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90
                 DB 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90
                 DB 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90
                 DB 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90

bit_mask_63      DD 0000003Fh

.code

PUBLIC dequant_q6k_avx512

dequant_q6k_avx512 PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rdi
    push rsi
    push r12
    push r13
    push r14
    push r15
    sub  rsp, 32                    ; shadow space

    ; Load args (rcx, rdx, r8)
    mov r9, rcx
    mov r10, rdx
    mov r11, r8

    ; Zero-check
    test r11, r11
    jle dequant_done

    ; Load block scales
    vbroadcastss zmm4, DWORD PTR [r9]      ; Load scale
    add r9, 4                              ; Advance

    ; Load masks from memory (using rip-relative or direct for data segment)
    vmovdqu64 zmm2, ZMMWORD PTR [q6k_extract_mask]
    vpbroadcastd zmm3, DWORD PTR [bit_mask_63]

dequant_loop:
    cmp r11, 16
    jl dequant_tail

    ; --- REAL 6-BIT UNPACKING ---
    vmovups xmm0, XMMWORD PTR [r9]
    vinserti32x4 zmm1, zmm1, xmm0, 0
    vshufi32x4 zmm1, zmm1, zmm1, 0

    vpmultishiftqb zmm0, zmm2, zmm1 
    vpandd zmm0, zmm0, zmm3

    vcvtdq2ps zmm0, zmm0
    vmulps zmm0, zmm0, zmm4
    vmovntps ZMMWORD PTR [r10], zmm0

    add r9, 12
    add r10, 64
    sub r11, 16
    jmp dequant_loop

dequant_tail:
    test r11, r11
    jle dequant_done
    
    mov rax, 1
    mov ecx, r11d
    shl rax, cl
    dec rax
    kmovq k1, rax

    ; Use masked load (dqu32 requires zmm destination even for smaller masks)
    vmovdqu32 zmm1{k1}{z}, ZMMWORD PTR [r9]
    
    vpmultishiftqb zmm0, zmm2, zmm1
    vpandd zmm0, zmm0, zmm3
    vcvtdq2ps zmm0, zmm0
    vmulps zmm0, zmm0, zmm4

    vmovups ZMMWORD PTR [r10]{k1}, zmm0

dequant_done:
    add  rsp, 32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    pop rbp
    mov eax, 1
    ret
dequant_q6k_avx512 ENDP

END

END

END
