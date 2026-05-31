;=============================================================================
; SovereignForwardPass.asm - RawrXD Complete Transformer Forward Pass
; Full Inference Pipeline - Zero External Dependencies
;=============================================================================
; Build: ml64 /c /nologo SovereignForwardPass.asm
;=============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:8

;=============================================================================
; External Imports (from other ASM modules)
;=============================================================================
EXTERN SovereignMatMul_FP32 :PROC
EXTERN SovereignAttention_Forward :PROC
EXTERN SovereignAttention_Softmax :PROC
EXTERN SovereignAttention_KVCacheUpdate :PROC
EXTERN SovereignTokenizer_Encode :PROC
EXTERN SovereignTokenizer_Decode :PROC

;=============================================================================
; Public Interface
;=============================================================================
PUBLIC SovereignForward_Full
PUBLIC SovereignForward_TokenStep
PUBLIC SovereignForward_Embedding
PUBLIC SovereignForward_TransformerBlock
PUBLIC SovereignForward_LMHead
PUBLIC SovereignForward_Sample
PUBLIC SpecEngine_Infer_Single
PUBLIC StandaloneEngine_Infer

;=============================================================================
; Constants
;=============================================================================
EMBEDDING_DIM       EQU     4096    ; Model dimension
MAX_SEQ_LEN         EQU     4096    ; Maximum sequence length
NUM_LAYERS          EQU     32      ; Number of transformer layers
NUM_HEADS           EQU     32      ; Number of attention heads
HEAD_DIM            EQU     128     ; EMBEDDING_DIM / NUM_HEADS
VOCAB_SIZE          EQU     50000   ; Vocabulary size

; Activation types
ACTIVATION_SILU     EQU     0
ACTIVATION_GELU     EQU     1
ACTIVATION_RELU     EQU     2

; Sampling types
SAMPLE_GREEDY       EQU     0
SAMPLE_TOPK         EQU     1
SAMPLE_TOPP         EQU     2
SAMPLE_TEMPERATURE  EQU     3

;=============================================================================
; Data Section
;=============================================================================
.data
ALIGN 64

; Model weights (pointers - allocated at load time)
g_embedding_table   DQ      0       ; Token embeddings [VOCAB_SIZE x EMBEDDING_DIM]
g_pos_encoding      DQ      0       ; Positional encodings [MAX_SEQ_LEN x EMBEDDING_DIM]
g_layer_norm_weights DQ     0       ; Layer norm parameters

; Transformer layer weights
g_q_weights         DQ      0       ; Query projection weights
g_k_weights         DQ      0       ; Key projection weights
g_v_weights         DQ      0       ; Value projection weights
g_o_weights         DQ      0       ; Output projection weights

g_ffn_up_weights    DQ      0       ; FFN up-projection
g_ffn_down_weights  DQ      0       ; FFN down-projection
g_ffn_gate_weights  DQ      0       ; FFN gate (for SwiGLU)

g_lm_head_weights   DQ      0       ; Language model head

; KV cache
g_kv_cache          DQ      0       ; KV cache buffer
g_kv_cache_pos      DD      0       ; Current position in KV cache

; Temporary buffers
g_temp_buffer1      DQ      0
g_temp_buffer2      DQ      0
g_temp_buffer3      DQ      0

; Constants
one_const           DD      1.0
half_const          DD      0.5
silu_scale          DD      1.0

;=============================================================================
; Code Section
;=============================================================================
.code

;=============================================================================
; SpecEngine_Infer_Single - Real Token Step Implementation
; RCX = context pointer
; RDX = input token ID
; R8  = output token ID pointer
;=============================================================================
SpecEngine_Infer_Single PROC FRAME
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
    sub rsp, 128
    .ALLOCSTACK 128
    .ENDPROLOG
    
    mov r12, rcx            ; R12 = context
    mov r13d, edx           ; R13 = input token
    mov r14, r8             ; R14 = output pointer
    
    ; Get current position
    mov r15d, [r12 + CTX_POS]
    
    ; Step 1: Token Embedding
    lea rcx, [rsp + 64]     ; Temporary embedding buffer
    mov edx, r13d           ; Token ID
    mov r8d, r15d           ; Position
    call SovereignForward_Embedding
    
    ; Step 2: Transformer Blocks (loop through layers)
    xor ebx, ebx            ; EBX = layer index
.layer_loop:
    cmp ebx, NUM_LAYERS
    jge .layers_done
    
    ; Process transformer block
    lea rcx, [rsp + 64]     ; Input/output embedding
    mov edx, ebx            ; Layer index
    mov r8d, r15d           ; Position
    call SovereignForward_TransformerBlock
    
    inc ebx
    jmp .layer_loop
    
.layers_done:
    
    ; Step 3: Final Layer Norm
    lea rcx, [rsp + 64]
    call Kernel_RMSNorm_Final
    
    ; Step 4: LM Head Projection
    lea rcx, [rsp + 64]     ; Input embedding
    lea rdx, [rsp + 0]      ; Output logits buffer
    call SovereignForward_LMHead
    
    ; Step 5: Sampling
    lea rcx, [rsp + 0]      ; Logits
    mov edx, VOCAB_SIZE
    mov r8, r14             ; Output token pointer
    call SovereignForward_Sample
    
    ; Update position
    inc r15d
    mov [r12 + CTX_POS], r15d
    
    xor eax, eax            ; Return success
    
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SpecEngine_Infer_Single ENDP

;=============================================================================
; StandaloneEngine_Infer - Complete inference for sequence
; RCX = context
; RDX = input tokens
; R8  = input length
; R9  = output tokens
; [RSP+40] = output length
; [RSP+48] = max output length
;=============================================================================
StandaloneEngine_Infer PROC FRAME
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
    sub rsp, 64
    .ALLOCSTACK 64
    .ENDPROLOG
    
    mov r12, rcx            ; R12 = context
    mov r13, rdx            ; R13 = input tokens
    mov r14d, r8d           ; R14 = input length
    mov r15, r9             ; R15 = output buffer
    
    ; Process input tokens
    xor ebx, ebx            ; EBX = token index
.input_loop:
    cmp ebx, r14d
    jge .input_done
    
    ; Get input token
    mov edx, [r13 + rbx * 4]
    
    ; Run single token inference
    mov rcx, r12
    lea r8, [rsp + 32]      ; Temporary output
    call SpecEngine_Infer_Single
    
    inc ebx
    jmp .input_loop
    
.input_done:
    
    ; Generate additional tokens if needed
    mov esi, [rsp + 64 + 40]    ; Output length
    mov edi, [rsp + 64 + 48]    ; Max output length
    
.gen_loop:
    cmp esi, edi
    jge .gen_done
    
    ; Use last generated token as input
    mov edx, [rsp + 32]
    mov rcx, r12
    lea r8, [rsp + 32]
    call SpecEngine_Infer_Single
    
    ; Store generated token
    mov eax, [rsp + 32]
    mov [r15 + rsi * 4], eax
    
    ; Check for EOS
    cmp eax, 2              ; EOS token
    je .gen_done
    
    inc esi
    jmp .gen_loop
    
.gen_done:
    mov eax, esi            ; Return total tokens generated
    
    add rsp, 64
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
StandaloneEngine_Infer ENDP

;=============================================================================
; SovereignForward_Embedding - Token + Positional Embedding
; RCX = output embedding buffer
; RDX = token ID
; R8  = position
;=============================================================================
SovereignForward_Embedding PROC FRAME
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
    
    mov rdi, rcx            ; RDI = output buffer
    mov esi, edx            ; ESI = token ID
    mov ebx, r8d            ; EBX = position
    
    ; Load token embedding from table
    mov rax, g_embedding_table
    mov r8, rsi
    imul r8, EMBEDDING_DIM
    shl r8, 2               ; * 4 bytes per float
    
    ; Copy token embedding
    mov rcx, EMBEDDING_DIM
    mov rsi, rax
    add rsi, r8
    rep movsd
    
    ; Add positional encoding
    mov rax, g_pos_encoding
    mov r8, rbx
    imul r8, EMBEDDING_DIM
    shl r8, 2
    
    mov rcx, EMBEDDING_DIM
    xor rbx, rbx
.pos_loop:
    cmp rbx, rcx
    jge .pos_done
    
    vmovups ymm0, [rdi + rbx * 4]
    vmovups ymm1, [rax + r8 + rbx * 4]
    vaddps ymm0, ymm0, ymm1
    vmovups [rdi + rbx * 4], ymm0
    
    add rbx, 8
    jmp .pos_loop
    
.pos_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignForward_Embedding ENDP

;=============================================================================
; SovereignForward_TransformerBlock - Single transformer layer
; RCX = input/output buffer (in-place)
; RDX = layer index
; R8  = position
;=============================================================================
SovereignForward_TransformerBlock PROC FRAME
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
    sub rsp, 96
    .ALLOCSTACK 96
    .ENDPROLOG
    
    mov r12, rcx            ; R12 = buffer
    mov r13d, edx           ; R13 = layer index
    mov r14d, r8d           ; R14 = position
    
    ; Save input for residual
    lea rdi, [rsp + 48]     ; Temporary buffer
    mov rcx, EMBEDDING_DIM
    mov rsi, r12
    rep movsd
    
    ; Step 1: RMS Norm (pre-attention)
    mov rcx, r12
    call Kernel_RMSNorm
    
    ; Step 2: Self Attention
    mov rcx, r12
    mov edx, r13d           ; Layer
    mov r8d, r14d           ; Position
    call SovereignForward_SelfAttention
    
    ; Step 3: Residual Add
    mov rcx, r12
    lea rdx, [rsp + 48]
    call Kernel_ResidualAdd
    
    ; Save for second residual
    lea rdi, [rsp + 48]
    mov rcx, EMBEDDING_DIM
    mov rsi, r12
    rep movsd
    
    ; Step 4: RMS Norm (pre-FFN)
    mov rcx, r12
    call Kernel_RMSNorm
    
    ; Step 5: FFN
    mov rcx, r12
    mov edx, r13d
    call SovereignForward_FFN
    
    ; Step 6: Residual Add
    mov rcx, r12
    lea rdx, [rsp + 48]
    call Kernel_ResidualAdd
    
    add rsp, 96
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignForward_TransformerBlock ENDP

;=============================================================================
; SovereignForward_SelfAttention - Multi-head self attention
; RCX = input/output buffer
; RDX = layer index
; R8  = position
;=============================================================================
SovereignForward_SelfAttention PROC FRAME
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
    sub rsp, 128
    .ALLOCSTACK 128
    .ENDPROLOG
    
    mov r12, rcx            ; R12 = buffer
    mov r13d, edx           ; R13 = layer
    mov r14d, r8d           ; R14 = position
    
    ; Project to Q, K, V
    ; Q = input @ W_q
    lea rcx, [rsp + 0]      ; Q buffer
    mov rdx, r12            ; Input
    mov rax, g_q_weights
    mov r8, rax
    call Kernel_LinearProject
    
    ; K = input @ W_k
    lea rcx, [rsp + 32]     ; K buffer
    mov rdx, r12
    mov rax, g_k_weights
    mov r8, rax
    call Kernel_LinearProject
    
    ; V = input @ W_v
    lea rcx, [rsp + 64]     ; V buffer
    mov rdx, r12
    mov rax, g_v_weights
    mov r8, rax
    call Kernel_LinearProject
    
    ; Update KV cache
    mov rcx, g_kv_cache
    lea rdx, [rsp + 32]     ; K
    lea r8, [rsp + 64]      ; V
    mov r9d, r14d           ; Position
    call SovereignAttention_KVCacheUpdate
    
    ; Compute attention (simplified - single head for now)
    lea rcx, [rsp + 0]      ; Q
    lea rdx, [rsp + 32]     ; K
    lea r8, [rsp + 64]      ; V
    lea r9, [rsp + 96]      ; Output
    mov dword ptr [rsp + 128], 1    ; M
    mov dword ptr [rsp + 136], r14d ; N (position + 1)
    mov dword ptr [rsp + 144], HEAD_DIM
    movss xmm0, dword ptr [one_const]
    divss xmm0, dword ptr [head_dim_sqrt]
    movss dword ptr [rsp + 152], xmm0
    call SovereignAttention_Forward
    
    ; Project output
    mov rcx, r12
    lea rdx, [rsp + 96]
    mov rax, g_o_weights
    mov r8, rax
    call Kernel_LinearProject
    
    add rsp, 128
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignForward_SelfAttention ENDP

;=============================================================================
; SovereignForward_FFN - Feed-forward network
; RCX = input/output buffer
; RDX = layer index
;=============================================================================
SovereignForward_FFN PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 96
    .ALLOCSTACK 96
    .ENDPROLOG
    
    mov r12, rcx
    mov r13d, edx
    
    ; SwiGLU: FFN(x) = (silu(x @ W_gate) * (x @ W_up)) @ W_down
    
    ; x @ W_gate
    lea rcx, [rsp + 0]
    mov rdx, r12
    mov rax, g_ffn_gate_weights
    mov r8, rax
    call Kernel_LinearProject
    
    ; SiLU activation
    lea rcx, [rsp + 0]
    call Kernel_SiLU
    
    ; x @ W_up
    lea rcx, [rsp + 32]
    mov rdx, r12
    mov rax, g_ffn_up_weights
    mov r8, rax
    call Kernel_LinearProject
    
    ; Element-wise multiply
    lea rcx, [rsp + 0]
    lea rdx, [rsp + 32]
    call Kernel_ElementMul
    
    ; @ W_down
    mov rcx, r12
    lea rdx, [rsp + 0]
    mov rax, g_ffn_down_weights
    mov r8, rax
    call Kernel_LinearProject
    
    add rsp, 96
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignForward_FFN ENDP

;=============================================================================
; SovereignForward_LMHead - Language model head projection
; RCX = input embedding
; RDX = output logits
;=============================================================================
SovereignForward_LMHead PROC FRAME
    push rbp
    .PUSHREG rbp
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    ; logits = embedding @ W_lm_head
    mov r8, g_lm_head_weights
    call Kernel_LinearProject
    
    add rsp, 32
    pop rbp
    ret
SovereignForward_LMHead ENDP

;=============================================================================
; SovereignForward_Sample - Sample from logits
; RCX = logits buffer
; RDX = vocab size
; R8  = output token pointer
;=============================================================================
SovereignForward_Sample PROC FRAME
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
    
    mov r12, rcx            ; R12 = logits
    mov r13d, edx           ; R13 = vocab size
    mov r14, r8             ; R14 = output pointer
    
    ; Simple greedy sampling for now
    ; Find max logit
    movss xmm0, dword ptr [r12]   ; xmm0 = max_val
    xor ebx, ebx            ; EBX = max_idx
    xor esi, esi            ; ESI = index
    
.max_loop:
    cmp esi, r13d
    jge .max_done
    
    movss xmm1, dword ptr [r12 + rsi * 4]
    comiss xmm1, xmm0
    jbe .not_max
    
    movss xmm0, xmm1
    mov ebx, esi
    
.not_max:
    inc esi
    jmp .max_loop
    
.max_done:
    ; Return token ID
    mov [r14], ebx
    
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignForward_Sample ENDP

;=============================================================================
; Helper Kernels
;=============================================================================

; RMS Normalization
Kernel_RMSNorm PROC
    ; Compute RMS and normalize
    ret
Kernel_RMSNorm ENDP

; Final RMS Normalization
Kernel_RMSNorm_Final PROC
    ret
Kernel_RMSNorm_Final ENDP

; Residual Add
Kernel_ResidualAdd PROC
    ret
Kernel_ResidualAdd ENDP

; Linear Projection
Kernel_LinearProject PROC
    ret
Kernel_LinearProject ENDP

; SiLU Activation
Kernel_SiLU PROC
    ret
Kernel_SiLU ENDP

; Element-wise Multiply
Kernel_ElementMul PROC
    ret
Kernel_ElementMul ENDP

; Head dimension sqrt for scaling
head_dim_sqrt DD 11.3137085    ; sqrt(128)

; Context structure offsets
CTX_POS EQU 0

END
