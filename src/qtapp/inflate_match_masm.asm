; inflate_match_masm.asm - Ultra-optimized DEFLATE decompression match copier
; Part of the God-mode compression suite
; ml64 /c /Fo inflate_match_masm.obj inflate_match_masm.asm

OPTION casemap:none

PUBLIC inflate_match
PUBLIC inflate_match_safe

OVERLAP_THRESHOLD   EQU 32
AVX2_BLOCK_SIZE     EQU 32
SSE_BLOCK_SIZE      EQU 16
MIN_REP_MOVSB       EQU 8

.code

inflate_match PROC
    push    rsi
    push    r12
    
    mov     eax, r12d
    mov     r8, rdi
    sub     r8, r11
    mov     rsi, r8
    
    cmp     r11d, 4
    jbe     rle_pattern_copy
    
    cmp     r11d, OVERLAP_THRESHOLD
    jae     large_distance_path

medium_distance_path:
    cmp     eax, SSE_BLOCK_SIZE
    jl      scalar_copy_remainder

medium_sse_loop:
    cmp     eax, SSE_BLOCK_SIZE
    jl      scalar_copy_remainder
    
    movdqu  xmm0, xmmword ptr [rsi]
    movdqu  xmmword ptr [rdi], xmm0
    
    add     rsi, SSE_BLOCK_SIZE
    add     rdi, SSE_BLOCK_SIZE
    sub     eax, SSE_BLOCK_SIZE
    jmp     medium_sse_loop

large_distance_path:
    test    rdi, 1Fh
    jnz     avx2_unaligned_loop

avx2_aligned_loop:
    cmp     eax, AVX2_BLOCK_SIZE * 2
    jl      avx2_finish_block
    
    vmovdqu ymm0, ymmword ptr [rsi]
    vmovdqa ymmword ptr [rdi], ymm0
    
    vmovdqu ymm1, ymmword ptr [rsi + AVX2_BLOCK_SIZE]
    vmovdqa ymmword ptr [rdi + AVX2_BLOCK_SIZE], ymm1
    
    add     rsi, AVX2_BLOCK_SIZE * 2
    add     rdi, AVX2_BLOCK_SIZE * 2
    sub     eax, AVX2_BLOCK_SIZE * 2
    
    jmp     avx2_aligned_loop

avx2_unaligned_loop:
    cmp     eax, AVX2_BLOCK_SIZE
    jl      scalar_copy_remainder
    
    vmovdqu ymm0, ymmword ptr [rsi]
    vmovdqu ymmword ptr [rdi], ymm0
    
    add     rsi, AVX2_BLOCK_SIZE
    add     rdi, AVX2_BLOCK_SIZE
    sub     eax, AVX2_BLOCK_SIZE
    jmp     avx2_unaligned_loop

avx2_finish_block:
    cmp     eax, AVX2_BLOCK_SIZE
    jge     avx2_unaligned_loop_single
    jmp     scalar_copy_remainder
    
avx2_unaligned_loop_single:
    vmovdqu ymm0, ymmword ptr [rsi]
    vmovdqu ymmword ptr [rdi], ymm0
    add     rsi, AVX2_BLOCK_SIZE
    add     rdi, AVX2_BLOCK_SIZE
    sub     eax, AVX2_BLOCK_SIZE
    jmp     scalar_copy_remainder

rle_pattern_copy:
    cmp     r11d, 1
    je      rle_dist1
    cmp     r11d, 2
    je      rle_dist2
    cmp     r11d, 3
    je      rle_dist3
    
rle_dist4:
    mov     r8d, dword ptr [rsi]
rle_dist4_loop:
    cmp     eax, 4
    jl      scalar_copy_remainder
    mov     dword ptr [rdi], r8d
    add     rdi, 4
    sub     eax, 4
    jmp     rle_dist4_loop

rle_dist3:
    mov     r8d, dword ptr [rsi]
    and     r8d, 00FFFFFFh
    mov     r9d, r8d
    
rle_dist3_loop:
    cmp     eax, 3
    jl      scalar_copy_remainder
    
    mov     word ptr [rdi], r8w
    shr     r8d, 16
    mov     byte ptr [rdi+2], r8b
    shl     r8d, 16
    or      r8d, r9d
    
    add     rdi, 3
    sub     eax, 3
    jmp     rle_dist3_loop

rle_dist2:
    mov     r8w, word ptr [rsi]
rle_dist2_loop:
    cmp     eax, 2
    jl      scalar_copy_remainder
    mov     word ptr [rdi], r8w
    add     rdi, 2
    sub     eax, 2
    jmp     rle_dist2_loop

rle_dist1:
    mov     al, byte ptr [rsi]
    mov     rcx, r12
    rep     stosb
    jmp     match_done_no_vzero

scalar_copy_remainder:
    cmp     eax, MIN_REP_MOVSB
    jl      scalar_byte_loop
    
    mov     rcx, rax
    rep movsb
    jmp     match_done

scalar_byte_loop:
    test    eax, eax
    jz      match_done
    
    mov     dl, byte ptr [rsi]
    mov     byte ptr [rdi], dl
    inc     rsi
    inc     rdi
    dec     eax
    jmp     scalar_byte_loop

match_done:
    vzeroupper
match_done_no_vzero:
    mov     rax, rdi
    pop     r12
    pop     rsi
    ret

inflate_match ENDP

inflate_match_safe PROC
    push    rsi
    push    r12
    push    r13
    push    r14
    
    cmp     r11d, r13d
    ja      error_dist_src_range
    
    mov     rax, rdi
    add     rax, r12
    cmp     rax, r14
    ja      error_overflow
    
    mov     rax, rdi
    sub     rax, r11
    cmp     rax, r10
    jb      error_dist_src_range
    
    pop     r14
    pop     r13
    pop     r12
    pop     rsi
    jmp     inflate_match

error_dist_src_range:
    mov     eax, 0
    jmp     match_safe_done

error_overflow:
    mov     eax, 0
    jmp     match_safe_done
    
match_safe_done:
    pop     r14
    pop     r13
    pop     r12
    pop     rsi
    ret

inflate_match_safe ENDP

END
