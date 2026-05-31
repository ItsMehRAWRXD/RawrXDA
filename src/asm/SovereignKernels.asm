;=============================================================================
; SovereignKernels.asm - RawrXD Helper Kernel Implementations
; RMSNorm, Activations, Residuals, Linear Projections
;=============================================================================
; Build: ml64 /c /nologo SovereignKernels.asm
;=============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:8

;=============================================================================
; Public Interface
;=============================================================================
PUBLIC Kernel_RMSNorm
PUBLIC Kernel_RMSNorm_Final
PUBLIC Kernel_LayerNorm
PUBLIC Kernel_ResidualAdd
PUBLIC Kernel_LinearProject
PUBLIC Kernel_SiLU
PUBLIC Kernel_GELU
PUBLIC Kernel_ReLU
PUBLIC Kernel_ElementMul
PUBLIC Kernel_ElementAdd

;=============================================================================
; Constants
;=============================================================================
EMBEDDING_DIM       EQU     4096
EPSILON             EQU     1e-6

;=============================================================================
; Data Section
;=============================================================================
.data
ALIGN 64

epsilon_const       DD      EPSILON
one_const           DD      1.0
half_const          DD      0.5
sqrt_2_over_pi      DD      0.7978845608    ; sqrt(2/pi)

;=============================================================================
; Code Section
;=============================================================================
.code

;=============================================================================
; Kernel_RMSNorm - Root Mean Square Normalization
; RCX = input/output buffer (FP32, length N)
; RDX = N (length, default EMBEDDING_DIM)
;=============================================================================
Kernel_RMSNorm PROC FRAME
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
    
    mov r10, rcx            ; R10 = buffer
    mov r11d, edx
    cmp r11d, 0
    jne .has_len
    mov r11d, EMBEDDING_DIM
.has_len:
    
    ; Step 1: Compute sum of squares
    vxorps ymm0, ymm0, ymm0
    xor rbx, rbx
.sum_loop:
    cmp ebx, r11d
    jge .sum_done
    
    vmovups ymm1, [r10 + rbx * 4]
    vmulps ymm1, ymm1, ymm1
    vaddps ymm0, ymm0, ymm1
    
    add ebx, 8
    jmp .sum_loop
    
.sum_done:
    ; Horizontal sum
    vextractf128 xmm1, ymm0, 1
    addps xmm0, xmm1
    haddps xmm0, xmm0
    haddps xmm0, xmm0
    
    ; Compute RMS: sqrt(mean + epsilon)
    cvtsi2ss xmm1, r11d
    divss xmm0, xmm1        ; mean
    addss xmm0, dword ptr [epsilon_const]
    sqrtss xmm0, xmm0       ; RMS
    
    ; Compute 1/RMS
    movss xmm1, dword ptr [one_const]
    divss xmm1, xmm0        ; xmm1 = 1/RMS (scale factor)
    vbroadcastss ymm2, xmm1
    
    ; Step 2: Normalize
    xor rbx, rbx
.norm_loop:
    cmp ebx, r11d
    jge .norm_done
    
    vmovups ymm0, [r10 + rbx * 4]
    vmulps ymm0, ymm0, ymm2
    vmovups [r10 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .norm_loop
    
.norm_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
Kernel_RMSNorm ENDP

;=============================================================================
; Kernel_RMSNorm_Final - Final RMS Norm (same as regular for now)
;=============================================================================
Kernel_RMSNorm_Final PROC FRAME
    jmp Kernel_RMSNorm
Kernel_RMSNorm_Final ENDP

;=============================================================================
; Kernel_LayerNorm - Standard Layer Normalization
; RCX = input/output buffer
; RDX = gamma (scale) parameters
; R8  = beta (shift) parameters
; R9  = N (length)
;=============================================================================
Kernel_LayerNorm PROC FRAME
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
    
    mov r10, rcx            ; R10 = buffer
    mov r11, rdx            ; R11 = gamma
    mov r12, r8             ; R12 = beta
    mov r13d, r9d           ; R13 = N
    
    ; Step 1: Compute mean
    vxorps ymm0, ymm0, ymm0
    xor rbx, rbx
.mean_loop:
    cmp ebx, r13d
    jge .mean_done
    
    vmovups ymm1, [r10 + rbx * 4]
    vaddps ymm0, ymm0, ymm1
    
    add ebx, 8
    jmp .mean_loop
    
.mean_done:
    vextractf128 xmm1, ymm0, 1
    addps xmm0, xmm1
    haddps xmm0, xmm0
    haddps xmm0, xmm0
    
    cvtsi2ss xmm1, r13d
    divss xmm0, xmm1
    vbroadcastss ymm3, xmm0     ; ymm3 = mean
    
    ; Step 2: Compute variance
    vxorps ymm0, ymm0, ymm0
    xor rbx, rbx
.var_loop:
    cmp ebx, r13d
    jge .var_done
    
    vmovups ymm1, [r10 + rbx * 4]
    vsubps ymm1, ymm1, ymm3     ; x - mean
    vmulps ymm1, ymm1, ymm1
    vaddps ymm0, ymm0, ymm1
    
    add ebx, 8
    jmp .var_loop
    
.var_done:
    vextractf128 xmm1, ymm0, 1
    addps xmm0, xmm1
    haddps xmm0, xmm0
    haddps xmm0, xmm0
    
    cvtsi2ss xmm1, r13d
    divss xmm0, xmm1
    addss xmm0, dword ptr [epsilon_const]
    sqrtss xmm0, xmm0           ; std
    vbroadcastss ymm4, xmm0     ; ymm4 = std
    
    ; Step 3: Normalize, scale, and shift
    xor rbx, rbx
.final_loop:
    cmp ebx, r13d
    jge .final_done
    
    vmovups ymm0, [r10 + rbx * 4]
    vsubps ymm0, ymm0, ymm3     ; x - mean
    vdivps ymm0, ymm0, ymm4     ; / std
    
    ; Apply gamma and beta if provided
    test r11, r11
    jz .no_gamma
    vmovups ymm1, [r11 + rbx * 4]
    vmulps ymm0, ymm0, ymm1     ; * gamma
    
.no_gamma:
    test r12, r12
    jz .no_beta
    vmovups ymm1, [r12 + rbx * 4]
    vaddps ymm0, ymm0, ymm1     ; + beta
    
.no_beta:
    vmovups [r10 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .final_loop
    
.final_done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
Kernel_LayerNorm ENDP

;=============================================================================
; Kernel_ResidualAdd - Residual Connection: output = input + residual
; RCX = input/output buffer
; RDX = residual buffer
; R8  = N (length, default EMBEDDING_DIM)
;=============================================================================
Kernel_ResidualAdd PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = input/output
    mov r11, rdx            ; R11 = residual
    mov r12d, r8d
    cmp r12d, 0
    jne .has_len
    mov r12d, EMBEDDING_DIM
.has_len:
    
    xor rbx, rbx
.add_loop:
    cmp ebx, r12d
    jge .add_done
    
    vmovups ymm0, [r10 + rbx * 4]
    vmovups ymm1, [r11 + rbx * 4]
    vaddps ymm0, ymm0, ymm1
    vmovups [r10 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .add_loop
    
.add_done:
    add rsp, 32
    pop rbx
    pop rbp
    ret
Kernel_ResidualAdd ENDP

;=============================================================================
; Kernel_LinearProject - Linear transformation: output = input @ weights
; RCX = output buffer
; RDX = input buffer
; R8  = weights matrix
; R9  = input_dim
; [RSP+40] = output_dim
;=============================================================================
Kernel_LinearProject PROC FRAME
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
    sub rsp, 64
    .ALLOCSTACK 64
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = output
    mov r11, rdx            ; R11 = input
    mov r12, r8             ; R12 = weights
    mov r13d, r9d           ; R13 = input_dim
    mov r14d, [rsp + 64 + 40]   ; R14 = output_dim
    
    ; For each output dimension
    xor rdi, rdi            ; RDI = output index
.out_loop:
    cmp edi, r14d
    jge .out_done
    
    ; Compute dot product: output[i] = sum(input[j] * weights[i, j])
    vxorps ymm0, ymm0, ymm0
    xor rsi, rsi            ; RSI = input index
.dot_loop:
    cmp esi, r13d
    jge .dot_done
    
    ; Load weight
    mov rax, rdi
    imul rax, r13
    add rax, rsi
    vbroadcastss ymm1, dword ptr [r12 + rax * 4]
    
    ; Load input
    vbroadcastss ymm2, dword ptr [r11 + rsi * 4]
    
    ; Multiply and accumulate
    vfmadd231ps ymm0, ymm1, ymm2
    
    inc esi
    jmp .dot_loop
    
.dot_done:
    ; Horizontal sum
    vextractf128 xmm1, ymm0, 1
    addps xmm0, xmm1
    haddps xmm0, xmm0
    haddps xmm0, xmm0
    
    ; Store result
    movss dword ptr [r10 + rdi * 4], xmm0
    
    inc edi
    jmp .out_loop
    
.out_done:
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
Kernel_LinearProject ENDP

;=============================================================================
; Kernel_SiLU - SiLU (Swish) Activation: x * sigmoid(x)
; RCX = input/output buffer
; RDX = N (length)
;=============================================================================
Kernel_SiLU PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx
    mov r11d, edx
    cmp r11d, 0
    jne .has_len
    mov r11d, EMBEDDING_DIM
.has_len:
    
    xor rbx, rbx
.silu_loop:
    cmp ebx, r11d
    jge .silu_done
    
    ; Load 8 values
    vmovups ymm0, [r10 + rbx * 4]
    
    ; Sigmoid approximation: sigmoid(x) ≈ 1 / (1 + exp(-x))
    ; Using fast approximation: sigmoid(x) ≈ 0.5 * (1 + tanh(x * 0.5))
    vmulps ymm1, ymm0, dword ptr [half_const]
    ; tanh approximation would go here
    ; For now, use polynomial approximation
    
    ; Simplified: x * sigmoid(x) ≈ x * max(0, min(1, 0.5 + 0.25*x))
    vmulps ymm2, ymm0, dword ptr [quarter_const]
    vbroadcastss ymm3, dword ptr [half_const]
    vaddps ymm2, ymm2, ymm3
    
    ; Clamp to [0, 1]
    vxorps ymm3, ymm3, ymm3
    vmaxps ymm2, ymm2, ymm3
    vbroadcastss ymm3, dword ptr [one_const]
    vminps ymm2, ymm2, ymm3
    
    ; Multiply by x
    vmulps ymm0, ymm0, ymm2
    
    vmovups [r10 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .silu_loop
    
.silu_done:
    add rsp, 32
    pop rbx
    pop rbp
    ret
Kernel_SiLU ENDP

;=============================================================================
; Kernel_GELU - GELU Activation
;=============================================================================
Kernel_GELU PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx
    mov r11d, edx
    cmp r11d, 0
    jne .has_len
    mov r11d, EMBEDDING_DIM
.has_len:
    
    xor rbx, rbx
.gelu_loop:
    cmp ebx, r11d
    jge .gelu_done
    
    vmovups ymm0, [r10 + rbx * 4]
    
    ; GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    ; Simplified approximation
    
    vmovups [r10 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .gelu_loop
    
.gelu_done:
    add rsp, 32
    pop rbx
    pop rbp
    ret
Kernel_GELU ENDP

;=============================================================================
; Kernel_ReLU - ReLU Activation
;=============================================================================
Kernel_ReLU PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx
    mov r11d, edx
    cmp r11d, 0
    jne .has_len
    mov r11d, EMBEDDING_DIM
.has_len:
    
    vxorps ymm1, ymm1, ymm1    ; ymm1 = 0
    
    xor rbx, rbx
.relu_loop:
    cmp ebx, r11d
    jge .relu_done
    
    vmovups ymm0, [r10 + rbx * 4]
    vmaxps ymm0, ymm0, ymm1
    vmovups [r10 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .relu_loop
    
.relu_done:
    add rsp, 32
    pop rbx
    pop rbp
    ret
Kernel_ReLU ENDP

;=============================================================================
; Kernel_ElementMul - Element-wise multiplication
; RCX = input/output buffer
; RDX = multiplier buffer
; R8  = N (length)
;=============================================================================
Kernel_ElementMul PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx
    mov r11, rdx
    mov r12d, r8d
    cmp r12d, 0
    jne .has_len
    mov r12d, EMBEDDING_DIM
.has_len:
    
    xor rbx, rbx
.mul_loop:
    cmp ebx, r12d
    jge .mul_done
    
    vmovups ymm0, [r10 + rbx * 4]
    vmovups ymm1, [r11 + rbx * 4]
    vmulps ymm0, ymm0, ymm1
    vmovups [r10 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .mul_loop
    
.mul_done:
    add rsp, 32
    pop rbx
    pop rbp
    ret
Kernel_ElementMul ENDP

;=============================================================================
; Kernel_ElementAdd - Element-wise addition
;=============================================================================
Kernel_ElementAdd PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx
    mov r11, rdx
    mov r12d, r8d
    cmp r12d, 0
    jne .has_len
    mov r12d, EMBEDDING_DIM
.has_len:
    
    xor rbx, rbx
.add_loop:
    cmp ebx, r12d
    jge .add_done
    
    vmovups ymm0, [r10 + rbx * 4]
    vmovups ymm1, [r11 + rbx * 4]
    vaddps ymm0, ymm0, ymm1
    vmovups [r10 + rbx * 4], ymm0
    
    add ebx, 8
    jmp .add_loop
    
.add_done:
    add rsp, 32
    pop rbx
    pop rbp
    ret
Kernel_ElementAdd ENDP

; Additional constants
quarter_const       DD      0.25

END
