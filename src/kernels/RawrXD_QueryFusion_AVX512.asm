; ============================================================================
; RawrXD Phase 8: Weighted Fusion Kernel (AVX-512 FMA)
; Computes out[c] = sum_i(score_matrix[i * cand_count + c] * weights[i])
;
; Win64 ABI:
;   rcx = score_matrix (float*)
;   rdx = weights (float*)
;   r8d = sub_query_count
;   r9d = candidate_count
;   [rsp+40] = out_scores (float*)
;
; Return:
;   eax = 1 success, 0 invalid args
; ============================================================================

PUBLIC rawr_weighted_fusion_fma_avx512
PUBLIC rawr_weighted_fusion_ew75_p15_avx512
PUBLIC rawr_apply_penalty_phrase_gate_avx512

.const
rawr_w_075 REAL4 0.75
rawr_w_025 REAL4 0.25

.code

rawr_weighted_fusion_fma_avx512 PROC
    mov r10, rcx
    mov r11, rdx
    mov rdx, QWORD PTR [rsp + 40]

    test r10, r10
    jz fusion_fail
    test r11, r11
    jz fusion_fail
    test rdx, rdx
    jz fusion_fail
    test r8d, r8d
    jz fusion_fail
    test r9d, r9d
    jz fusion_fail

    ; Zero out_scores[c] for all candidates.
    vxorps zmm0, zmm0, zmm0
    mov eax, r9d
    mov rcx, rdx
    shr eax, 4
    jz fusion_zero_tail

fusion_zero_loop16:
    vmovups ZMMWORD PTR [rcx], zmm0
    add rcx, 64
    dec eax
    jnz fusion_zero_loop16

fusion_zero_tail:
    mov eax, r9d
    and eax, 15
    jz fusion_outer_setup

fusion_zero_scalar:
    mov DWORD PTR [rcx], 0
    add rcx, 4
    dec eax
    jnz fusion_zero_scalar

fusion_outer_setup:
    xor eax, eax                  ; i = 0

fusion_outer_loop:
    cmp eax, r8d
    jae fusion_ok

    ; src = score_matrix + (i * cand_count)
    mov ecx, eax
    imul ecx, r9d
    movsxd rcx, ecx
    lea rcx, [r10 + rcx * 4]

    ; weight broadcast for sub-query i
    vbroadcastss zmm2, DWORD PTR [r11 + rax * 4]

    ; Vector loop over candidates.
    mov esi, r9d
    mov rdi, rdx
    shr esi, 4
    jz fusion_tail

fusion_vec_loop:
    vmovups zmm0, ZMMWORD PTR [rdi]
    vmovups zmm1, ZMMWORD PTR [rcx]
    vfmadd231ps zmm0, zmm1, zmm2
    vmovups ZMMWORD PTR [rdi], zmm0
    add rdi, 64
    add rcx, 64
    dec esi
    jnz fusion_vec_loop

fusion_tail:
    mov esi, r9d
    and esi, 15
    jz fusion_next_i

fusion_tail_loop:
    vmovss xmm0, DWORD PTR [rdi]
    vmovss xmm1, DWORD PTR [rcx]
    vfmadd231ss xmm0, xmm1, xmm2
    vmovss DWORD PTR [rdi], xmm0
    add rdi, 4
    add rcx, 4
    dec esi
    jnz fusion_tail_loop

fusion_next_i:
    inc eax
    jmp fusion_outer_loop

fusion_ok:
    vzeroupper
    mov eax, 1
    ret

fusion_fail:
    xor eax, eax
    ret
rawr_weighted_fusion_fma_avx512 ENDP

; ============================================================================
; Fixed-profile weighted fusion for Phase 9 fast path (ew75_p15)
; out[c] = row0[c] * 0.75 + row1[c] * 0.25
;
; Win64 ABI:
;   rcx = score_row0 (float*)
;   rdx = score_row1 (float*)
;   r8d = candidate_count
;   r9  = out_scores (float*)
; ============================================================================

rawr_weighted_fusion_ew75_p15_avx512 PROC
    test rcx, rcx
    jz fusion75_fail
    test rdx, rdx
    jz fusion75_fail
    test r9, r9
    jz fusion75_fail
    test r8d, r8d
    jz fusion75_fail

    vbroadcastss zmm30, DWORD PTR [rawr_w_075]
    vbroadcastss zmm31, DWORD PTR [rawr_w_025]

    mov r10, rcx
    mov r11, rdx
    mov rax, r9

    mov ecx, r8d
    shr ecx, 4
    jz fusion75_tail

fusion75_vec_loop:
    vmovups zmm0, ZMMWORD PTR [r10]
    vmovups zmm1, ZMMWORD PTR [r11]
    vmulps zmm0, zmm0, zmm30
    vfmadd231ps zmm0, zmm1, zmm31
    vmovups ZMMWORD PTR [rax], zmm0
    add r10, 64
    add r11, 64
    add rax, 64
    dec ecx
    jnz fusion75_vec_loop

fusion75_tail:
    mov ecx, r8d
    and ecx, 15
    jz fusion75_ok

fusion75_tail_loop:
    vmovss xmm0, DWORD PTR [r10]
    vmovss xmm1, DWORD PTR [r11]
    vmulss xmm0, xmm0, DWORD PTR [rawr_w_075]
    vfmadd231ss xmm0, xmm1, DWORD PTR [rawr_w_025]
    vmovss DWORD PTR [rax], xmm0
    add r10, 4
    add r11, 4
    add rax, 4
    dec ecx
    jnz fusion75_tail_loop

fusion75_ok:
    vzeroupper
    mov eax, 1
    ret

fusion75_fail:
    xor eax, eax
    ret
rawr_weighted_fusion_ew75_p15_avx512 ENDP

; ============================================================================
; Applies score = max(0, score - missing_terms[i] * penalty_per_missing)
; then clamps score to clamp_max when phrase_gate_failures[i] != 0.
;
; Win64 ABI:
;   rcx = scores (float*)
;   rdx = missing_term_counts (uint32_t*)
;   r8  = phrase_gate_failures (uint32_t*)
;   r9d = candidate_count
;   [rsp+40] = penalty_per_missing (float)
;   [rsp+48] = clamp_max (float)
; ============================================================================

rawr_apply_penalty_phrase_gate_avx512 PROC
    test rcx, rcx
    jz gate_fail
    test rdx, rdx
    jz gate_fail
    test r8, r8
    jz gate_fail
    test r9d, r9d
    jz gate_fail

    vbroadcastss zmm4, DWORD PTR [rsp + 40]
    vbroadcastss zmm5, DWORD PTR [rsp + 48]
    vxorps zmm6, zmm6, zmm6
    vpxord zmm7, zmm7, zmm7

    mov r10, rcx
    mov r11, rdx
    mov rax, r8
    mov ecx, r9d
    shr ecx, 4
    jz gate_tail

gate_vec_loop:
    vmovups zmm0, ZMMWORD PTR [r10]
    vcvtdq2ps zmm1, ZMMWORD PTR [r11]
    vmulps zmm1, zmm1, zmm4
    vsubps zmm0, zmm0, zmm1
    vmaxps zmm0, zmm0, zmm6
    vmovdqu32 zmm2, ZMMWORD PTR [rax]
    vpcmpd k1, zmm2, zmm7, 4
    vminps zmm0{k1}, zmm0, zmm5
    vmovups ZMMWORD PTR [r10], zmm0
    add r10, 64
    add r11, 64
    add rax, 64
    dec ecx
    jnz gate_vec_loop

gate_tail:
    mov ecx, r9d
    and ecx, 15
    jz gate_ok

    vmovss xmm4, DWORD PTR [rsp + 40]
    vmovss xmm5, DWORD PTR [rsp + 48]
    vxorps xmm6, xmm6, xmm6

gate_tail_loop:
    vmovss xmm0, DWORD PTR [r10]
    mov edx, DWORD PTR [r11]
    test edx, edx
    jz gate_no_penalty
    vcvtsi2ss xmm1, xmm6, edx
    vmulss xmm1, xmm1, xmm4
    vsubss xmm0, xmm0, xmm1
gate_no_penalty:
    vmaxss xmm0, xmm0, xmm6
    cmp DWORD PTR [rax], 0
    je gate_store_scalar
    vminss xmm0, xmm0, xmm5
gate_store_scalar:
    vmovss DWORD PTR [r10], xmm0
    add r10, 4
    add r11, 4
    add rax, 4
    dec ecx
    jnz gate_tail_loop

gate_ok:
    vzeroupper
    mov eax, 1
    ret

gate_fail:
    xor eax, eax
    ret
rawr_apply_penalty_phrase_gate_avx512 ENDP

END
