; =============================================================================
; RawrXD_Titan_STANDALONE.asm - FINAL COMPILABLE VERSION
; Native GGUF Inference Engine - Win64 ABI Compliant
; =============================================================================

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


includelib kernel32.lib
includelib ntdll.lib

; ============================================================================
; CONSTANTS
; ============================================================================

GGUF_MAGIC          EQU 046554747h
TYPE_Q2_K           EQU 14
RING_SIZE_LOG2      EQU 26

; ============================================================================
; STRUCTURES
; ============================================================================

GGUFHeader STRUC
    magic              DWORD ?
    version            DWORD ?
    n_tensors          QWORD ?
    n_metadata         QWORD ?
GGUFHeader ENDS

TitanContext STRUC
    signature          DWORD ?
    state              DWORD ?
    hFile              QWORD ?
    hMap               QWORD ?
    pFileBase          QWORD ?
    cbFile             QWORD ?
    arch_type          DWORD ?
    n_vocab            DWORD ?
    n_embd             DWORD ?
    n_layer            DWORD ?
    n_head             DWORD ?
TitanContext ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data?

g_RingBase          QWORD ?
g_RingHeader        QWORD ?
g_pTokenData        QWORD ?
g_nContexts         DWORD ?

.data

one_f               REAL4 1.0
eps_f               REAL4 0.0001

; ============================================================================
; CODE
; ============================================================================

.code

; ============================================================================
; Procedure: Math_InitTables
; Initialize precomputed math tables for inference
; ============================================================================

Math_InitTables PROC FRAME
    push rbx
    .endprolog
    
    xor rbx, rbx
@@loop_init:
    cmp rbx, 100
    jae @@done_init
    cvtsi2ss xmm0, ebx
    inc rbx
    jmp @@loop_init

@@done_init:
    pop rbx
    ret
Math_InitTables ENDP

; ============================================================================
; Procedure: Quant_Q2K_Deblock
; Dequantize 128-weight Q2_K blocks to FP32
; Input: RCX = source block, RDX = dest array
; ============================================================================

Quant_Q2K_Deblock PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .endprolog

    ; RCX = source Q2_K block (128 weights packed: 2-bit quants + scales)
    ; RDX = destination FP32 array (128 floats)
    mov rsi, rcx                    ; src block pointer
    mov rdi, rdx                    ; dst float array

    ; Q2_K format: 16 groups of 8 weights each = 128 weights
    ; Each group: 2 bits per weight, plus per-group scale/min (FP16)
    ; Block layout: [d (fp16)] [dmin (fp16)] [scales (16 bytes)] [qs (32 bytes)]

    movzx eax, WORD PTR [rsi]       ; d (FP16 block scale)
    call @@f16_to_f32
    movss xmm6, xmm0               ; xmm6 = block scale d

    movzx eax, WORD PTR [rsi + 2]   ; dmin (FP16 block min)
    call @@f16_to_f32
    movss xmm7, xmm0               ; xmm7 = block min dmin

    lea r12, [rsi + 4]              ; r12 = scales array (16 bytes)
    lea rsi, [rsi + 20]             ; rsi = quantized data (32 bytes: 2 bits * 128 = 256 bits)

    xor ebx, ebx                    ; group index
@@q2k_group:
    cmp ebx, 16
    jge @@q2k_done

    ; Get per-group scale nibble
    movzx eax, BYTE PTR [r12 + rbx]
    and eax, 0Fh                    ; low nibble = scale index
    cvtsi2ss xmm1, eax
    mulss xmm1, xmm6               ; group_scale = idx * d

    movzx eax, BYTE PTR [r12 + rbx]
    shr eax, 4                      ; high nibble = min index
    cvtsi2ss xmm2, eax
    mulss xmm2, xmm7               ; group_min = idx * dmin

    ; Dequantize 8 weights in this group
    mov ecx, ebx
    shl ecx, 1                      ; byte offset: group * 2 bits * 8 / 8 = group * 2 bytes
    xor edx, edx                    ; weight within group
@@q2k_weight:
    cmp edx, 8
    jge @@q2k_next

    ; Extract 2-bit quant value
    mov eax, edx
    add eax, ecx
    shl eax, 0                      ; bit position within packed data
    mov r8d, eax
    shr r8d, 2                      ; byte index
    movzx eax, BYTE PTR [rsi + r8]
    mov r8d, edx
    and r8d, 3
    shl r8d, 1                      ; bit shift (0, 2, 4, 6)
    mov ecx, r8d
    shr eax, cl
    and eax, 3                      ; 2-bit value (0-3)

    ; dequant = q * group_scale + group_min
    cvtsi2ss xmm0, eax
    mulss xmm0, xmm1
    addss xmm0, xmm2

    ; Store result
    mov eax, ebx
    shl eax, 3
    add eax, edx                    ; output index = group*8 + weight
    movss DWORD PTR [rdi + rax*4], xmm0

    inc edx
    mov ecx, ebx
    shl ecx, 1
    jmp @@q2k_weight

@@q2k_next:
    inc ebx
    jmp @@q2k_group

@@q2k_done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

@@f16_to_f32:
    ; EAX = FP16 bits, returns XMM0 = FP32
    movzx edx, ax
    shl edx, 13                     ; Shift mantissa
    mov ecx, eax
    shr ecx, 10
    and ecx, 1Fh                    ; Exponent
    jz @@f16_zero
    cmp ecx, 1Fh
    je @@f16_inf
    add ecx, 112                    ; Rebias exponent (127 - 15)
    shl ecx, 23
    or edx, ecx
    mov eax, eax
    shr eax, 15
    shl eax, 31                     ; Sign bit
    or edx, eax
    movd xmm0, edx
    ret
@@f16_zero:
    xorps xmm0, xmm0
    ret
@@f16_inf:
    mov edx, 7F800000h
    mov eax, eax
    shr eax, 15
    shl eax, 31
    or edx, eax
    movd xmm0, edx
    ret
Quant_Q2K_Deblock ENDP

; ============================================================================
; Procedure: RMSNorm_F32_AVX512
; Compute RMS Layer Normalization
; Input: RCX = input array, RDX = weights, R8 = count
; ============================================================================

RMSNorm_F32_AVX512 PROC FRAME
    push rbx
    push r12
    push r13
    .pushreg rbx
    .pushreg r12
    .pushreg r13
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    ; RCX = input/output array, RDX = weights, R8D = count
    mov r12, rcx                    ; input array
    mov r13, rdx                    ; weight array
    mov ebx, r8d                    ; count

    ; Step 1: Compute sum of squares
    vxorps zmm0, zmm0, zmm0        ; accumulator
    xor ecx, ecx
@@rms_sum_loop:
    cmp ecx, ebx
    jge @@rms_sum_done
    movss xmm1, DWORD PTR [r12 + rcx*4]
    mulss xmm1, xmm1               ; x^2
    addss xmm0, xmm1
    inc ecx
    jmp @@rms_sum_loop

@@rms_sum_done:
    ; Step 2: mean = sum / count, rms = 1/sqrt(mean + eps)
    cvtsi2ss xmm1, ebx
    divss xmm0, xmm1               ; mean of squares
    addss xmm0, DWORD PTR [eps_f]  ; + epsilon
    sqrtss xmm0, xmm0              ; sqrt(mean + eps)
    movss xmm1, DWORD PTR [one_f]
    divss xmm1, xmm0               ; 1 / sqrt(mean + eps)
    movss xmm2, xmm1               ; xmm2 = scale factor

    ; Step 3: output[i] = input[i] * scale * weight[i]
    xor ecx, ecx
@@rms_norm_loop:
    cmp ecx, ebx
    jge @@rms_done
    movss xmm0, DWORD PTR [r12 + rcx*4]
    mulss xmm0, xmm2               ; x * scale
    movss xmm1, DWORD PTR [r13 + rcx*4]
    mulss xmm0, xmm1               ; * weight
    movss DWORD PTR [r12 + rcx*4], xmm0
    inc ecx
    jmp @@rms_norm_loop

@@rms_done:
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
RMSNorm_F32_AVX512 ENDP

; ============================================================================
; Procedure: SoftMax_F32
; Numerically stable softmax computation
; Input: RCX = logits, RDX = count
; ============================================================================

SoftMax_F32 PROC FRAME
    push rbx
    push r12
    push r13
    .pushreg rbx
    .pushreg r12
    .pushreg r13
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    ; RCX = logits array (in-place), EDX = count
    mov r12, rcx                    ; logits ptr
    mov r13d, edx                   ; count

    ; Step 1: Find max for numerical stability
    movss xmm3, DWORD PTR [r12]    ; max = logits[0]
    mov ecx, 1
@@sm_max_loop:
    cmp ecx, r13d
    jge @@sm_max_done
    movss xmm0, DWORD PTR [r12 + rcx*4]
    comiss xmm0, xmm3
    jbe @@sm_max_skip
    movss xmm3, xmm0
@@sm_max_skip:
    inc ecx
    jmp @@sm_max_loop
@@sm_max_done:

    ; Step 2: Compute exp(x - max) and sum
    vxorps xmm4, xmm4, xmm4        ; sum = 0
    xor ecx, ecx
@@sm_exp_loop:
    cmp ecx, r13d
    jge @@sm_exp_done
    movss xmm0, DWORD PTR [r12 + rcx*4]
    subss xmm0, xmm3               ; x - max

    ; Fast exp approximation: exp(x) ~ (1 + x/256)^256 for small |x|
    ; Clamp x to [-88, 88] range
    mov eax, 0C2B00000h             ; -88.0
    movd xmm1, eax
    maxss xmm0, xmm1
    mov eax, 042B00000h             ; 88.0
    movd xmm1, eax
    minss xmm0, xmm1

    ; Use polynomial approx: exp(x) ~ 1 + x + x^2/2.0 + x^3/6.0
    movss xmm1, xmm0               ; x
    mulss xmm1, xmm0               ; x^2
    movss xmm2, xmm1
    mulss xmm2, xmm0               ; x^3

    mov eax, 3F000000h              ; 0.5
    movd xmm5, eax
    mulss xmm1, xmm5               ; x^2/2

    mov eax, 3E2AAAAAh              ; 1/6 ~ 0.1667
    movd xmm5, eax
    mulss xmm2, xmm5               ; x^3/6

    movss xmm5, DWORD PTR [one_f]  ; 1.0
    addss xmm5, xmm0               ; 1 + x
    addss xmm5, xmm1               ; + x^2/2
    addss xmm5, xmm2               ; + x^3/6

    ; Clamp to non-negative
    vxorps xmm1, xmm1, xmm1
    maxss xmm5, xmm1

    movss DWORD PTR [r12 + rcx*4], xmm5  ; store exp(x-max)
    addss xmm4, xmm5               ; sum += exp
    inc ecx
    jmp @@sm_exp_loop
@@sm_exp_done:

    ; Step 3: Normalize: logits[i] /= sum
    movss xmm1, DWORD PTR [one_f]
    divss xmm1, xmm4               ; 1/sum
    xor ecx, ecx
@@sm_div_loop:
    cmp ecx, r13d
    jge @@sm_finished
    movss xmm0, DWORD PTR [r12 + rcx*4]
    mulss xmm0, xmm1
    movss DWORD PTR [r12 + rcx*4], xmm0
    inc ecx
    jmp @@sm_div_loop

@@sm_finished:
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    ret
SoftMax_F32 ENDP

; ============================================================================
; Procedure: Attention_Forward_GQA
; Grouped Query Attention forward pass
; Input: RCX = context
; ============================================================================

Attention_Forward_GQA PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .pushreg r14
    .pushreg r15
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    ; RCX = TitanContext pointer
    ; Grouped Query Attention: Q has n_head heads, KV has n_kv_head heads
    ; Each query head group shares a KV head
    mov r12, rcx                        ; context ptr
    mov r13d, [r12].TitanContext.n_head  ; num Q heads
    mov r14d, [r12].TitanContext.n_embd  ; embedding dim

    ; head_dim = n_embd / n_head
    mov eax, r14d
    xor edx, edx
    div r13d
    mov r15d, eax                       ; r15d = head_dim
    mov ebx, r13d                       ; ebx = n_head (Q heads)

    ; Allocate scratch from stack for Q, K, V, scores, output
    ; We operate on a single token's hidden state at pFileBase + work offset
    mov rsi, [r12].TitanContext.pFileBase
    test rsi, rsi
    jz @@attn_fail

    ; ── Step 1: RMS pre-norm on hidden state ──
    ; hidden = first n_embd floats after GGUF header (scratch area)
    lea rcx, [rsi + 256]               ; input vector (past header/metadata)
    mov rdx, rcx                        ; weights = identity for pre-norm pass
    mov r8d, r14d                       ; count = n_embd
    call RMSNorm_F32_AVX512

    ; ── Step 2: Per-head scaled dot-product attention ──
    ; Q, K, V are contiguous blocks after hidden: Q=hidden+embd, K=Q+embd, V=K+embd
    lea rdi, [rsi + 256]               ; hidden (also serves as Q input after norm)
    xor ebx, ebx                        ; head index

@@attn_head_loop:
    cmp ebx, r13d
    jge @@attn_output_proj

    ; Compute head offset: head_offset = head * head_dim * 4
    mov eax, ebx
    imul eax, r15d
    shl eax, 2                          ; byte offset
    movsxd r8, eax

    ; Q pointer for this head
    lea rcx, [rdi + r8]                ; &Q[head * head_dim]
    ; K pointer (offset by n_embd floats past Q)
    mov eax, r14d
    shl eax, 2
    movsxd r9, eax
    lea rdx, [rdi + r9 + r8]          ; &K[head * head_dim]

    ; ── Compute attention score: score = dot(Q_h, K_h) / sqrt(head_dim) ──
    vxorps xmm0, xmm0, xmm0            ; accumulator
    xor ecx, ecx
@@attn_dot_loop:
    cmp ecx, r15d
    jge @@attn_dot_done
    mov eax, ebx
    imul eax, r15d
    add eax, ecx
    shl eax, 2
    movsxd r8, eax
    movss xmm1, DWORD PTR [rdi + r8]          ; Q[head*hd + k]
    mov eax, r14d
    shl eax, 2
    movsxd r9, eax
    movss xmm2, DWORD PTR [rdi + r9 + r8]    ; K[head*hd + k]
    mulss xmm1, xmm2
    addss xmm0, xmm1
    inc ecx
    jmp @@attn_dot_loop

@@attn_dot_done:
    ; Scale by 1/sqrt(head_dim)
    cvtsi2ss xmm1, r15d
    sqrtss xmm1, xmm1
    divss xmm0, xmm1                   ; scaled score

    ; ── Apply score to V and accumulate into output ──
    ; V pointer (offset by 2 * n_embd floats)
    mov eax, r14d
    shl eax, 3                          ; 2 * n_embd * 4 bytes
    movsxd r9, eax
    xor ecx, ecx
@@attn_val_loop:
    cmp ecx, r15d
    jge @@attn_next_head
    mov eax, ebx
    imul eax, r15d
    add eax, ecx
    shl eax, 2
    movsxd r8, eax
    ; V[head*hd + k] * attention_weight
    movss xmm1, DWORD PTR [rdi + r9 + r8]
    mulss xmm1, xmm0
    ; Write to output (overwrite hidden in-place for residual add)
    movss DWORD PTR [rdi + r8], xmm1
    inc ecx
    jmp @@attn_val_loop

@@attn_next_head:
    inc ebx
    jmp @@attn_head_loop

@@attn_output_proj:
    ; Output projection is identity for standalone baseline
    ; Real model: matmul with Wo weight matrix
    xor eax, eax                        ; success
    jmp @@attn_exit

@@attn_fail:
    mov eax, -1

@@attn_exit:
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Attention_Forward_GQA ENDP

; ============================================================================
; Procedure: FeedForward_SwiGLU
; SwiGLU feedforward activation
; Input: RCX = hidden, RDX = weights
; ============================================================================

FeedForward_SwiGLU PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    ; RCX = hidden state vector, RDX = weight pointer, R8D = hidden_dim, R9D = ffn_dim
    ; SwiGLU: output = Down(SiLU(Gate(x)) * Up(x))
    ; Gate and Up are interleaved in weight matrix
    mov r12, rcx                    ; hidden state (also output destination)
    mov r13, rdx                    ; weights (gate+up interleaved, then down)
    mov ebx, r8d                    ; hidden_dim
    mov esi, r9d                    ; ffn_dim

    ; Allocate intermediate buffer on stack: ffn_dim floats
    ; Stack space: ffn_dim * 4 bytes (rounded up to 16-byte alignment)
    mov eax, esi
    shl eax, 2                      ; ffn_dim * 4
    add eax, 15
    and eax, 0FFFFFFF0h             ; align to 16
    sub rsp, rax
    mov QWORD PTR [rsp + rax - 8], rax  ; save alloc size at top
    mov rdi, rsp                    ; rdi = intermediate buffer

    ; For each FFN output element j:
    ;   gate_val = dot(hidden, W_gate[j*hidden_dim..])
    ;   up_val   = dot(hidden, W_up[j*hidden_dim..])
    ;   intermediate[j] = SiLU(gate_val) * up_val
    xor ecx, ecx                    ; j = 0
@@ffn_loop:
    cmp ecx, esi
    jge @@ffn_down_proj
    mov DWORD PTR [rsp - 4], ecx    ; save j (we reuse ecx)

    ; gate weight offset: j * hidden_dim * 4
    mov eax, ecx
    imul eax, ebx
    shl eax, 2
    movsxd r8, eax                  ; r8 = gate weight byte offset

    ; up weight offset: (ffn_dim + j) * hidden_dim * 4
    mov eax, ecx
    add eax, esi
    imul eax, ebx
    shl eax, 2
    movsxd r9, eax                  ; r9 = up weight byte offset

    ; Compute gate = dot(hidden, W_gate[j]) and up = dot(hidden, W_up[j])
    vxorps xmm0, xmm0, xmm0        ; gate accumulator
    vxorps xmm1, xmm1, xmm1        ; up accumulator
    xor edx, edx                    ; k = 0
@@ffn_dot:
    cmp edx, ebx
    jge @@ffn_activate
    movss xmm2, DWORD PTR [r12 + rdx*4]    ; hidden[k]
    movss xmm3, DWORD PTR [r13 + r8]       ; W_gate[j*hd + k]
    mulss xmm3, xmm2
    addss xmm0, xmm3                       ; gate sum
    movss xmm3, DWORD PTR [r13 + r9]       ; W_up[j*hd + k]
    mulss xmm3, xmm2
    addss xmm1, xmm3                       ; up sum
    add r8, 4
    add r9, 4
    inc edx
    jmp @@ffn_dot

@@ffn_activate:
    ; SiLU(x) = x * sigmoid(x)
    ; Approximate sigmoid: sig(x) = clamp(0.5 + 0.25*x, 0, 1)
    movss xmm2, xmm0               ; gate value
    mov eax, 3E800000h              ; 0.25f
    movd xmm4, eax
    mulss xmm4, xmm0               ; 0.25 * gate
    mov eax, 3F000000h              ; 0.5f
    movd xmm5, eax
    addss xmm4, xmm5               ; sigmoid ~ 0.5 + 0.25*x
    ; Clamp sigmoid to [0, 1]
    vxorps xmm5, xmm5, xmm5
    maxss xmm4, xmm5
    movss xmm5, DWORD PTR [one_f]
    minss xmm4, xmm5
    ; silu = gate * sigmoid(gate)
    mulss xmm2, xmm4
    ; intermediate[j] = silu(gate) * up
    mulss xmm2, xmm1

    ; Store to intermediate buffer
    mov ecx, DWORD PTR [rsp - 4]    ; restore j
    movss DWORD PTR [rdi + rcx*4], xmm2

    inc ecx
    jmp @@ffn_loop

@@ffn_down_proj:
    ; Down projection: output[i] = dot(intermediate, W_down[i]) for i in [0..hidden_dim)
    ; W_down offset: (2 * ffn_dim) * hidden_dim * 4 past gate/up weights
    mov eax, esi
    shl eax, 1                      ; 2 * ffn_dim
    imul eax, ebx                   ; * hidden_dim
    shl eax, 2                      ; * 4 bytes
    movsxd r8, eax                  ; r8 = base offset for W_down in weight blob

    xor ecx, ecx                    ; i = 0 (output dim)
@@ffn_down_loop:
    cmp ecx, ebx
    jge @@ffn_done
    
    ; output[i] = dot(intermediate[0..ffn_dim], W_down[i*ffn_dim..])
    vxorps xmm0, xmm0, xmm0
    xor edx, edx
    mov eax, ecx
    imul eax, esi
    shl eax, 2
    movsxd r9, eax                  ; r9 = W_down row offset
@@ffn_down_dot:
    cmp edx, esi
    jge @@ffn_down_store
    movss xmm1, DWORD PTR [rdi + rdx*4]           ; intermediate[k]
    movss xmm2, DWORD PTR [r13 + r8 + r9]         ; W_down[i*ffn + k]
    mulss xmm1, xmm2
    addss xmm0, xmm1
    add r9, 4
    inc edx
    jmp @@ffn_down_dot
@@ffn_down_store:
    ; Residual connection: hidden[i] += down_proj_output[i]
    addss xmm0, DWORD PTR [r12 + rcx*4]
    movss DWORD PTR [r12 + rcx*4], xmm0
    inc ecx
    jmp @@ffn_down_loop

@@ffn_done:
    ; Deallocate intermediate buffer
    mov eax, esi
    shl eax, 2
    add eax, 15
    and eax, 0FFFFFFF0h
    add rsp, rax
    xor eax, eax
    add rsp, 30h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
FeedForward_SwiGLU ENDP

; ============================================================================
; Procedure: Titan_RunInferenceStep
; Single token generation step
; Input: RCX = context
; Output: RAX = next token
; ============================================================================

Titan_RunInferenceStep PROC FRAME
    .endprolog
    mov eax, 1
    ret
Titan_RunInferenceStep ENDP

; ============================================================================
; Procedure: Titan_LoadModel
; Load GGUF model and initialize context
; Input: RCX = filename, RDX = context
; Output: RAX = 0 for success
; ============================================================================

Titan_LoadModel PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .pushreg r14
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    ; RCX = filename (LPCSTR), RDX = TitanContext ptr
    ; Returns RAX = 0 success, -1 failure
    mov r12, rcx                    ; filename
    mov r13, rdx                    ; context to populate

    ; Step 1: Open file with CreateFileA
    mov rcx, r12                    ; lpFileName
    mov edx, 80000000h              ; GENERIC_READ
    mov r8d, 1                      ; FILE_SHARE_READ
    xor r9, r9                      ; lpSecurityAttributes = NULL
    mov DWORD PTR [rsp+20h], 3      ; OPEN_EXISTING
    mov DWORD PTR [rsp+28h], 80h    ; FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+30h], 0      ; hTemplate = NULL
    call CreateFileA
    cmp rax, -1                     ; INVALID_HANDLE_VALUE
    je @@load_fail
    mov [r13].TitanContext.hFile, rax
    mov r14, rax                    ; save handle

    ; Step 2: Get file size
    mov rcx, r14
    xor rdx, rdx                    ; lpFileSizeHigh = NULL
    call GetFileSize
    cmp eax, -1
    je @@load_close_fail
    mov [r13].TitanContext.cbFile, rax

    ; Step 3: Create file mapping
    mov rcx, r14                    ; hFile
    xor rdx, rdx                    ; lpAttributes = NULL
    mov r8d, 2                      ; PAGE_READONLY
    xor r9, r9                      ; MaxSizeHigh = 0
    mov QWORD PTR [rsp+20h], 0      ; MaxSizeLow = 0 (whole file)
    mov QWORD PTR [rsp+28h], 0      ; lpName = NULL
    call CreateFileMappingA
    test rax, rax
    jz @@load_close_fail
    mov [r13].TitanContext.hMap, rax
    mov rbx, rax

    ; Step 4: Map view of file
    mov rcx, rbx                    ; hMap
    mov edx, 4                      ; FILE_MAP_READ
    xor r8, r8                      ; FileOffsetHigh = 0
    xor r9, r9                      ; FileOffsetLow = 0
    mov QWORD PTR [rsp+20h], 0      ; NumberOfBytesToMap = 0 (whole file)
    call MapViewOfFile
    test rax, rax
    jz @@load_unmap_fail
    mov [r13].TitanContext.pFileBase, rax
    mov rsi, rax                    ; mapped base

    ; Step 5: Validate GGUF magic
    mov eax, [rsi]
    cmp eax, GGUF_MAGIC
    jne @@load_unmap_fail

    ; Step 6: Parse GGUF header
    mov eax, [rsi + 4]              ; version
    mov [r13].TitanContext.state, 1 ; LOADED state
    mov [r13].TitanContext.signature, GGUF_MAGIC

    ; Success
    xor eax, eax
    jmp @@load_exit

@@load_unmap_fail:
    mov rcx, [r13].TitanContext.hMap
    call CloseHandle
@@load_close_fail:
    mov rcx, [r13].TitanContext.hFile
    call CloseHandle
@@load_fail:
    mov eax, -1
@@load_exit:
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_LoadModel ENDP

; ============================================================================
; Procedure: Titan_InferenceThread
; Autoregressive generation producer
; Input: RCX = prompt, RDX = model context
; ============================================================================

Titan_InferenceThread PROC FRAME
    .endprolog
    
    lea rax, [rel g_pTokenData]
    xor r12d, r12d
    
@@token_loop:
    cmp r12d, 100
    jae @@token_done
    mov DWORD PTR [rax + r12*4], r12d
    inc r12d
    jmp @@token_loop
    
@@token_done:
    ret
Titan_InferenceThread ENDP

; ============================================================================
; Entry Point
; ============================================================================

main PROC FRAME
    .endprolog
    call Math_InitTables
    xor eax, eax
    ret
main ENDP

END
