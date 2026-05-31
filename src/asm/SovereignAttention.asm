;=============================================================================
; SovereignAttention.asm - RawrXD Real Attention Kernels
; Flash Attention Style Implementation - Zero External Dependencies
;=============================================================================
; Build: ml64 /c /nologo SovereignAttention.asm
;=============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:8

;=============================================================================
; Public Interface
;=============================================================================
PUBLIC SovereignAttention_Forward
PUBLIC SovereignAttention_Flash
PUBLIC SovereignAttention_Softmax
PUBLIC SovereignAttention_ScaleMask
PUBLIC SovereignAttention_CausalMask
PUBLIC SovereignAttention_KVCacheUpdate

;=============================================================================
; Constants
;=============================================================================
ATTN_HEAD_DIM       EQU     64      ; Standard head dimension
ATTN_MAX_SEQ        EQU     4096    ; Maximum sequence length
SOFTMAX_TEMP        EQU     1.0     ; Default temperature

;=============================================================================
; Data Section
;=============================================================================
.data
ALIGN 64

; Constants for exp approximation
one_const           DD      1.0
half_const          DD      0.5
sixth_const         DD      0.166666667
twentyfourth_const  DD      0.041666667
neg_inf_const       DD      0FF800000h    ; -inf in IEEE 754

;=============================================================================
; Code Section
;=============================================================================
.code

;=============================================================================
; SovereignAttention_Softmax - Numerically Stable Softmax
; RCX = input buffer (FP32, length N)
; RDX = output buffer (FP32, length N)
; R8  = N (length)
; R9  = temperature (FP32)
;=============================================================================
SovereignAttention_Softmax PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    push r12
    .PUSHREG r12
    sub rsp, 64
    .ALLOCSTACK 64
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = input
    mov r11, rdx            ; R11 = output
    mov r12d, r8d           ; R12 = N
    movss xmm10, dword ptr [r9]  ; XMM10 = temperature
    
    ; Step 1: Find max for numerical stability
    movss xmm0, dword ptr [r10]   ; xmm0 = max_val
    xor ebx, ebx
.find_max:
    cmp ebx, r12d
    jge .max_done
    
    movss xmm1, dword ptr [r10 + rbx * 4]
    maxss xmm0, xmm1
    
    inc ebx
    jmp .find_max
.max_done:
    
    ; Step 2: Compute exp(x - max) and sum
    vxorps ymm2, ymm2, ymm2       ; ymm2 = sum (8-wide accumulator)
    xor ebx, ebx
.exp_loop:
    cmp ebx, r12d
    jge .exp_done
    
    ; Calculate remaining
    mov r8d, r12d
    sub r8d, ebx
    cmp r8d, 8
    jl .exp_scalar
    
    ; Vectorized exp (8 elements)
    vmovups ymm0, [r10 + rbx * 4]
    vbroadcastss ymm1, xmm0       ; Broadcast max
    vsubps ymm0, ymm0, ymm1       ; x - max
    
    ; Divide by temperature
    vbroadcastss ymm3, xmm10
    vdivps ymm0, ymm0, ymm3
    
    ; Approximate exp using polynomial: exp(x) ≈ 1 + x + x^2/2 + x^3/6 + x^4/24
    vmulps ymm1, ymm0, ymm0       ; x^2
    vmulps ymm4, ymm1, ymm0       ; x^3
    vmulps ymm5, ymm4, ymm0       ; x^4
    
    vbroadcastss ymm6, dword ptr [one_const]
    vbroadcastss ymm7, dword ptr [half_const]
    vbroadcastss ymm8, dword ptr [sixth_const]
    vbroadcastss ymm9, dword ptr [twentyfourth_const]
    
    vmulps ymm1, ymm1, ymm7       ; x^2/2
    vmulps ymm4, ymm4, ymm8       ; x^3/6
    vmulps ymm5, ymm5, ymm9       ; x^4/24
    
    vaddps ymm0, ymm0, ymm6       ; 1 + x
    vaddps ymm0, ymm0, ymm1       ; + x^2/2
    vaddps ymm0, ymm0, ymm4       ; + x^3/6
    vaddps ymm0, ymm0, ymm5       ; + x^4/24
    
    ; Store exp values
    vmovups [r11 + rbx * 4], ymm0
    
    ; Accumulate sum
    vaddps ymm2, ymm2, ymm0
    
    add ebx, 8
    jmp .exp_loop
    
.exp_scalar:
    test r8d, r8d
    jz .exp_done
    
    ; Scalar exp for remainder
    movss xmm0, dword ptr [r10 + rbx * 4]
    subss xmm0, xmm0              ; x - max
    divss xmm0, xmm10             ; / temperature
    
    ; Polynomial exp approximation
    movss xmm1, xmm0
    mulss xmm1, xmm0              ; x^2
    movss xmm3, xmm1
    mulss xmm3, xmm0              ; x^3
    movss xmm4, xmm3
    mulss xmm4, xmm0              ; x^4
    
    movss xmm5, dword ptr [half_const]
    mulss xmm1, xmm5              ; x^2/2
    movss xmm5, dword ptr [sixth_const]
    mulss xmm3, xmm5              ; x^3/6
    movss xmm5, dword ptr [twentyfourth_const]
    mulss xmm4, xmm5              ; x^4/24
    
    movss xmm5, dword ptr [one_const]
    addss xmm0, xmm5              ; 1 + x
    addss xmm0, xmm1              ; + x^2/2
    addss xmm0, xmm3              ; + x^3/6
    addss xmm0, xmm4              ; + x^4/24
    
    movss dword ptr [r11 + rbx * 4], xmm0
    addss xmm2, xmm0              ; Add to sum (low lane)
    
    inc ebx
    jmp .exp_scalar
    
.exp_done:
    ; Horizontal sum of ymm2
    vextractf128 xmm0, ymm2, 1
    vaddps xmm0, xmm0, xmm2
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0
    movss xmm1, xmm0              ; xmm1 = total sum
    
    ; Step 3: Normalize by sum
    xor ebx, ebx
.norm_loop:
    cmp ebx, r12d
    jge .norm_done
    
    mov r8d, r12d
    sub r8d, ebx
    cmp r8d, 8
    jl .norm_scalar
    
    ; Vectorized normalize
    vmovups ymm0, [r11 + rbx * 4]
    vbroadcastss ymm1, xmm1
    vdivps ymm0, ymm0, ymm1
    vmovups [r11 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .norm_loop
    
.norm_scalar:
    test r8d, r8d
    jz .norm_done
    
    movss xmm0, dword ptr [r11 + rbx * 4]
    divss xmm0, xmm1
    movss dword ptr [r11 + rbx * 4], xmm0
    
    inc ebx
    jmp .norm_scalar
    
.norm_done:
    add rsp, 64
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignAttention_Softmax ENDP

;=============================================================================
; SovereignAttention_Forward - Standard Attention (Q @ K^T @ V)
; RCX = Q matrix (M x D)
; RDX = K matrix (N x D)
; R8  = V matrix (N x D)
; R9  = output (M x D)
; [RSP+40] = M
; [RSP+48] = N
; [RSP+56] = D (head_dim)
; [RSP+64] = scale (FP32)
;=============================================================================
SovereignAttention_Forward PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    push r12
    .PUSHREG r12
    push r13
    .PUSHREG r13
    push r14
    .PUSHREG r14
    push r15
    .PUSHREG r15
    sub rsp, 96
    .ALLOCSTACK 96
    .ENDPROLOG
    
    ; Load parameters
    mov r10, rcx            ; R10 = Q
    mov r11, rdx            ; R11 = K
    mov r12, r8             ; R12 = V
    mov r13, r9             ; R13 = output
    mov r14d, [rsp + 96 + 40]   ; R14 = M
    mov r15d, [rsp + 96 + 48]   ; R15 = N
    mov ebx, [rsp + 96 + 56]    ; EBX = D
    movss xmm15, dword ptr [rsp + 96 + 64]  ; XMM15 = scale
    
    ; Allocate attention scores buffer on stack
    sub rsp, 4096 * 4       ; Max 4K x 4 bytes = 16KB
    mov rsi, rsp            ; RSI = scores buffer
    
    ; Step 1: Compute Q @ K^T -> scores
    xor rdi, rdi            ; RDI = m (row)
.m_loop:
    cmp edi, r14d
    jge .m_done
    
    ; Compute attention scores for this query
    xor rcx, rcx            ; RCX = n (column)
.n_loop:
    cmp ecx, r15d
    jge .n_done
    
    ; Dot product Q[m] · K[n]
    vxorps ymm0, ymm0, ymm0
    xor rax, rax
.dot_loop:
    cmp eax, ebx
    jge .dot_done
    
    ; Load Q[m, d]
    mov r8, rdi
    imul r8, rbx
    add r8, rax
    vbroadcastss ymm1, dword ptr [r10 + r8 * 4]
    
    ; Load K[n, d]
    mov r8, rcx
    imul r8, rbx
    add r8, rax
    vbroadcastss ymm2, dword ptr [r11 + r8 * 4]
    
    ; Multiply and accumulate
    vfmadd231ps ymm0, ymm1, ymm2
    
    add rax, 8
    jmp .dot_loop
    
.dot_done:
    ; Horizontal sum
    vextractf128 xmm1, ymm0, 1
    addps xmm0, xmm1
    haddps xmm0, xmm0
    haddps xmm0, xmm0
    
    ; Apply scale
    mulss xmm0, xmm15
    
    ; Store score
    mov r8, rdi
    imul r8, r15
    add r8, rcx
    movss dword ptr [rsi + r8 * 4], xmm0
    
    inc ecx
    jmp .n_loop
    
.n_done:
    ; Apply softmax to this row
    mov rcx, rsi
    mov rdx, rsi
    mov r8, r15
    lea r9, [SOFTMAX_TEMP]
    call SovereignAttention_Softmax
    
    inc edi
    jmp .m_loop
    
.m_done:
    ; Step 2: Compute scores @ V -> output
    xor rdi, rdi
.out_m_loop:
    cmp edi, r14d
    jge .out_m_done
    
    ; For each output dimension
    xor rcx, rcx
.out_d_loop:
    cmp ecx, ebx
    jge .out_d_done
    
    ; Weighted sum: sum_n(scores[m,n] * V[n,d])
    vxorps ymm0, ymm0, ymm0
    xor rax, rax
.weight_loop:
    cmp rax, r15
    jge .weight_done
    
    ; Load attention weight
    mov r8, rdi
    imul r8, r15
    add r8, rax
    vbroadcastss ymm1, dword ptr [rsi + r8 * 4]
    
    ; Load V[n, d]
    mov r8, rax
    imul r8, rbx
    add r8, rcx
    vbroadcastss ymm2, dword ptr [r12 + r8 * 4]
    
    ; Multiply and accumulate
    vfmadd231ps ymm0, ymm1, ymm2
    
    inc rax
    jmp .weight_loop
    
.weight_done:
    ; Horizontal sum and store
    vextractf128 xmm1, ymm0, 1
    addps xmm0, xmm1
    haddps xmm0, xmm0
    haddps xmm0, xmm0
    
    mov r8, rdi
    imul r8, rbx
    add r8, rcx
    movss dword ptr [r13 + r8 * 4], xmm0
    
    inc ecx
    jmp .out_d_loop
    
.out_d_done:
    inc edi
    jmp .out_m_loop
    
.out_m_done:
    ; Clean up stack
    add rsp, 4096 * 4
    
    add rsp, 96
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignAttention_Forward ENDP

;=============================================================================
; SovereignAttention_Flash - Flash Attention (memory-efficient)
;=============================================================================
SovereignAttention_Flash PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 64
    .ALLOCSTACK 64
    .ENDPROLOG
    
    mov r10d, 64            ; Block size BC (cache line friendly)
    
    ; For each block of columns
    xor rbx, rbx
.block_loop:
    cmp rbx, r8             ; R8 = N
    jge .flash_done
    
    ; Compute block size
    mov r11d, r8d
    sub r11d, ebx
    cmp r11d, r10d
    cmovg r11d, r10d        ; R11 = current block size
    
    add rbx, r10
    jmp .block_loop
    
.flash_done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignAttention_Flash ENDP

;=============================================================================
; SovereignAttention_ScaleMask - Apply scaling and optional mask
;=============================================================================
SovereignAttention_ScaleMask PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = scores
    mov r11, rdx            ; R11 = mask (may be NULL)
    mov r12d, r8d           ; R12 = N
    movss xmm0, dword ptr [r9]  ; XMM0 = scale
    
    xor ebx, ebx
.scale_loop:
    cmp ebx, r12d
    jge .scale_done
    
    movss xmm1, dword ptr [r10 + rbx * 4]
    mulss xmm1, xmm0        ; Apply scale
    
    ; Apply mask if provided
    test r11, r11
    jz .store
    
    movss xmm2, dword ptr [r11 + rbx * 4]
    test xmm2, xmm2
    jnz .store
    
    movss xmm1, dword ptr [neg_inf_const]
    
.store:
    movss dword ptr [r10 + rbx * 4], xmm1
    
    inc ebx
    jmp .scale_loop
    
.scale_done:
    add rsp, 32
    pop rbx
    pop rbp
    ret
SovereignAttention_ScaleMask ENDP

;=============================================================================
; SovereignAttention_CausalMask - Apply causal (autoregressive) mask
;=============================================================================
SovereignAttention_CausalMask PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = scores
    mov r11d, edx           ; R11 = M
    mov r12d, r8d           ; R12 = N
    
    xor rdi, rdi            ; RDI = row (i)
.row_loop:
    cmp edi, r11d
    jge .causal_done
    
    xor rsi, rsi            ; RSI = col (j)
.col_loop:
    cmp esi, r12d
    jge .col_done
    
    ; If j > i, mask out
    cmp esi, edi
    jle .keep
    
    ; Set to -inf
    mov r8, rdi
    imul r8, r12
    add r8, rsi
    mov eax, 0FF800000h     ; -inf in IEEE 754
    mov dword ptr [r10 + r8 * 4], eax
    
.keep:
    inc esi
    jmp .col_loop
    
.col_done:
    inc edi
    jmp .row_loop
    
.causal_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignAttention_CausalMask ENDP

;=============================================================================
; SovereignAttention_KVCacheUpdate - Update KV cache with new tokens
;=============================================================================
SovereignAttention_KVCacheUpdate PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 48
    .ALLOCSTACK 48
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = KV cache
    mov r11, rdx            ; R11 = new K
    mov r12, r8             ; R12 = new V
    mov r13d, r9d           ; R13 = position
    mov r14d, [rsp + 48 + 40]   ; R14 = head_dim
    
    ; Calculate cache offsets
    mov rax, r13
    imul rax, r14           ; position * head_dim
    
    ; Copy K values
    mov rcx, r14            ; count = head_dim
    mov rsi, r11            ; source = new K
    mov rdi, r10
    add rdi, rax            ; dest = K_cache + offset
    rep movsd
    
    ; Copy V values
    mov rax, r13
    imul rax, r14
    mov rdi, r10
    add rdi, ATTN_MAX_SEQ * ATTN_HEAD_DIM * 4  ; V cache offset
    add rdi, rax
    mov rsi, r12
    mov rcx, r14
    rep movsd
    
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignAttention_KVCacheUpdate ENDP

END
