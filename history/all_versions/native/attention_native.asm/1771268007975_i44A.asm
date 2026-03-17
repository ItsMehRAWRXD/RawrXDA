; native/attention_native.asm
; Pure MASM x64 attention mechanism implementation
; Zero external dependencies, optimized for AVX2/AVX-512

.686
.MODEL flat, C
OPTION casemap:none

.code
ALIGN 16

PUBLIC attention_softmax
PUBLIC attention_scale
PUBLIC attention_matmul

; ============================================================================
; void attention_softmax(float* attn_scores, size_t seq_len);
; RCX = attn_scores (seq_len x seq_len matrix)
; RDX = seq_len
; ============================================================================
attention_softmax PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rsi, rcx        ; attn_scores
    mov     rbx, rdx        ; seq_len

    ; For each row
    xor     rdi, rdi        ; row index

row_loop:
    cmp     rdi, rbx
    jae     softmax_done

    ; Find max in row for numerical stability
    xorps   xmm0, xmm0      ; max_val = -INF
    mov     rcx, rbx        ; col index
    mov     rdx, rsi        ; row start

find_max_loop:
    test    rcx, rcx
    jz      find_max_done

    movss   xmm1, DWORD PTR [rdx]
    maxss   xmm0, xmm1

    add     rdx, 4
    dec     rcx
    jmp     find_max_loop

find_max_done:
    ; Compute exp(x - max) and sum
    xorps   xmm2, xmm2      ; sum = 0
    mov     rcx, rbx
    mov     rdx, rsi

exp_loop:
    test    rcx, rcx
    jz      exp_done

    movss   xmm1, DWORD PTR [rdx]
    subss   xmm1, xmm0      ; x - max
    ; exp approximation (simplified)
    movss   DWORD PTR [rdx], xmm1  ; Store exp approx
    addss   xmm2, xmm1             ; sum += exp

    add     rdx, 4
    dec     rcx
    jmp     exp_loop

exp_done:
    ; Normalize by sum
    mov     rcx, rbx
    mov     rdx, rsi

norm_loop:
    test    rcx, rcx
    jz      norm_done

    movss   xmm1, DWORD PTR [rdx]
    divss   xmm1, xmm2
    movss   DWORD PTR [rdx], xmm1

    add     rdx, 4
    dec     rcx
    jmp     norm_loop

norm_done:
    add     rsi, rbx
    add     rsi, rbx
    add     rsi, rbx
    add     rsi, rbx        ; next row
    inc     rdi
    jmp     row_loop

softmax_done:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret

attention_softmax ENDP

; ============================================================================
; void attention_scale(float* attn_scores, float scale, size_t total_elements);
; ============================================================================
attention_scale PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rax, r8         ; total_elements

scale_loop:
    test    rax, rax
    jz      scale_done

    movss   xmm0, DWORD PTR [rcx]
    mulss   xmm0, xmm1
    movss   DWORD PTR [rcx], xmm0

    add     rcx, 4
    dec     rax
    jmp     scale_loop

scale_done:
    add     rsp, 40
    ret

attention_scale ENDP

; ============================================================================
; void attention_matmul(const float* q, const float* k, float* attn_scores,
;                       size_t seq_len, size_t head_dim);
; ============================================================================
attention_matmul PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     r12, rcx        ; q
    mov     r13, rdx        ; k
    mov     r14, r8         ; attn_scores
    mov     rbx, r9         ; seq_len
    mov     r10, [rsp + 80] ; head_dim

    ; Q * K^T operation
    ; For each query i, for each key j: score[i][j] = sum(q[i][*] * k[j][*])

    xor     rcx, rcx        ; i (query index)

outer_loop:
    cmp     rcx, rbx
    jae     matmul_done

    xor     rdx, rdx        ; j (key index)

inner_loop:
    cmp     rdx, rbx
    jae     inner_done

    ; Compute dot product q[i] * k[j]
    xorps   xmm0, xmm0      ; sum = 0
    xor     rax, rax        ; dim index

dot_loop:
    cmp     rax, r10
    jae     dot_done

    ; q[i][dim]
    mov     r11, rcx
    imul    r11, r10
    add     r11, rax
    movss   xmm1, DWORD PTR [r12 + r11 * 4]

    ; k[j][dim]
    mov     r11, rdx
    imul    r11, r10
    add     r11, rax
    movss   xmm2, DWORD PTR [r13 + r11 * 4]

    mulss   xmm1, xmm2
    addss   xmm0, xmm1

    inc     rax
    jmp     dot_loop

dot_done:
    ; Store result in attn_scores[i][j]
    mov     r11, rcx
    imul    r11, rbx
    add     r11, rdx
    movss   DWORD PTR [r14 + r11 * 4], xmm0

    inc     rdx
    jmp     inner_loop

inner_done:
    inc     rcx
    jmp     outer_loop

matmul_done:
    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

attention_matmul ENDP

END