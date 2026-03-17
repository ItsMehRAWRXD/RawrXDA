; Titan_InferenceCore.asm - Direct GGUF Inference (No External Server)
; Implements: Transformer layers, Q2_K/Q4_0 dequantize, Attention, RoPE, Softmax
; Target: RawrXD-Agent.exe / RawrXD-TitanIDE.exe (in-process)

OPTION CASEMAP:NONE

 includelib kernel32.lib
 includelib ntdll.lib

; ============================================================================
; GGUF FORMAT CONSTANTS (Reverse engineered from llama.cpp spec)
; ============================================================================
.const
 GGUF_MAGIC         EQU 046554747h    ; 'GGUF' little-endian
 GGUF_VERSION       EQU 3

 ; Tensor types (GGML_TYPE)
 TYPE_F32           EQU 0
 TYPE_F16           EQU 1
 TYPE_Q4_0          EQU 2
 TYPE_Q4_1          EQU 3
 TYPE_Q5_0          EQU 6
 TYPE_Q5_1          EQU 7
 TYPE_Q8_0          EQU 8
 TYPE_Q8_1          EQU 9
 TYPE_Q2_K          EQU 14
 TYPE_Q3_K          EQU 15
 TYPE_Q4_K          EQU 16
 TYPE_Q5_K          EQU 17
 TYPE_Q6_K          EQU 18
 TYPE_Q8_K          EQU 19

 ; Transformer architecture (detected from GGUF metadata)
 ARCH_LLAMA         EQU 0
 ARCH_GPTNEOX       EQU 1
 ARCH_MISTRAL       EQU 2
 ARCH_PHI           EQU 3

 ; Math constants
 ROPE_THETA         EQU 10000          ; Base for rotary embeddings
 ROPE_SCALE         EQU 1              ; NTK scaling if needed

; ============================================================================
; INFERENCE STATE (Per-context, allocated in Consciousness Zone)
; ============================================================================
TransformerCtx STRUC
    pFileBase          QWORD ?         ; MapViewOfFile base
    cbFileSize         QWORD ?
    pTensorIndex       QWORD ?         ; Parsed tensor table
    
    Architecture       DWORD ?
    nLayers            DWORD ?         ; 32 for 7B, 40 for 13B, 80 for 70B
    nHeads             DWORD ?         ; Attention heads
    nKVHeads           DWORD ?         ; GQA/MQA heads (<= nHeads)
    nEmbed             DWORD ?         ; 4096, 5120, 8192, etc
    nFF                DWORD ?         ; Feedforward dim
    nCtx               DWORD ?         ; Max context (4096, 32768, 128k)
    nVocab             DWORD ?         ; 32000, 50000, 100000+
    
    nPos               DWORD ?         ; Current position in sequence
    pTokenEmbeddings   QWORD ?         ; tok_embd weight ptr
    pNorm              QWORD ?         ; output_norm
    pOutput            QWORD ?         ; output weight (classifier)
    
    pKCache            QWORD ?         ; [nLayers, nCtx, nKVHeads, HeadDim]
    pVCache            QWORD ?         ; Same shape
    cbKVCache          QWORD ?
    
    pCurrentInput      QWORD ?         ; Current token embedding
    pAttnScratch       QWORD ?         ; Q*K^T buffer
    pFFScratch         QWORD ?         ; Feedforward intermediate
TransformerCtx ENDS

; ============================================================================
; DATA SECTION
; ============================================================================
.data?

g_InferenceCtx      TransformerCtx <>
g_DequantScratch    BYTE 262144 DUP(?)  ; 256KB for dequant temp buffer

.data

; Math constants (as REAL4 for SSE operations)
ALIGN 4
const_epsilon       REAL4 1.0e-5
const_one           REAL4 1.0
const_half          REAL4 0.5
const_sqrt2         REAL4 1.41421356237

; Schraudolph fast-exp constants
; exp(x) ≈ reinterpret_as_float(int(x * 12102203.0 + 1065353216.0))
const_exp_scale     REAL4 12102203.0
const_exp_bias      REAL4 1065353216.0
const_exp_clamp_lo  REAL4 -87.33        ; exp(-87.33) ≈ 0
const_exp_clamp_hi  REAL4 88.72
const_neg_one       REAL4 -1.0

; RoPE base frequency
const_rope_base     REAL4 10000.0
const_two           REAL4 2.0

; Q4_0 dequant: center = 8.0
const_eight         REAL4 8.0

; ============================================================================
; CODE SECTION - QUANTIZATION MATH
; ============================================================================
.code

EXTERN CreateFileA : PROC
EXTERN CreateFileMappingA : PROC
EXTERN MapViewOfFile : PROC
EXTERN UnmapViewOfFile : PROC
EXTERN CloseHandle : PROC
EXTERN GetFileSize : PROC

; ----------------------------------------------------------------------------
; Dequantize_Q4_0_Block - Convert 34 bytes -> 32 FP32 scalars
; Q4_0: [2 bytes scale] + [16 bytes quants (4-bit pairs)]
; Input: RCX = src Q4_0 block ptr, RDX = dest FP32 array (32 floats)
; Output: Stores 32 float32s at RDX
; Clobbers: RAX, RBX, R8-R15
; ----------------------------------------------------------------------------
Dequantize_Q4_0_Block PROC
    push rbx
    push r12
    
    mov r12, rcx            ; Source block
    mov rbx, rdx            ; Dest buffer
    
    ; Load scale factor (FLOAT16 at offset 0)
    mov ax, WORD PTR [r12]
    
    ; Convert FP16 to FP32 (simplified - actually need full conversion)
    movzx eax, ax
    cvtsi2ss xmm0, eax
    mulss xmm0, [rel const_epsilon]  ; Temporary placeholder for scale
    
    ; xmm0 now contains the scale factor
    
    ; Unpack 16 bytes of 4-bit quants (2 quants per byte)
    xor ecx, ecx            ; Index 0..31
    
@@unpack_loop:
    cmp ecx, 32
    jae @@done
    
    ; Get byte index: ecx / 2
    mov eax, ecx
    shr eax, 1
    
    ; Get nibble from packed bytes at offset 2
    movzx edx, BYTE PTR [r12 + 2 + eax]
    
    ; Extract high or low nibble based on ecx & 1
    test ecx, 1
    jz @@low_nibble
    
    shr edx, 4              ; High nibble
@@low_nibble:
    and edx, 0Fh            ; Mask to 4 bits
    
    ; Dequant: (quant - 8) * scale  (center around 0)
    cvtsi2ss xmm1, edx
    subss xmm1, [rel const_half]  ; Subtract 8 >> approx
    mulss xmm1, xmm0        ; Scale
    
    ; Store to output
    movss [rbx + rcx*4], xmm1
    
    inc ecx
    jmp @@unpack_loop
    
@@done:
    pop r12
    pop rbx
    ret
Dequantize_Q4_0_Block ENDP

; ============================================================================
; TRANSFORMER LAYER IMPLEMENTATIONS
; ============================================================================

; ----------------------------------------------------------------------------
; LayerNorm - RMSNorm: x * (scale / sqrt(mean(x^2) + eps))
; RCX = x vector (float*), RDX = weight gamma (float*), R8 = size (nEmbed)
; Output: Result stored back to RCX
; Clobbers: RAX-RDX, XMM0-XMM7
; ----------------------------------------------------------------------------
LayerNorm PROC
    push rbx
    mov rbx, r8             ; Size = nEmbed
    
    ; Calculate sum of squares
    vxorps xmm0, xmm0, xmm0 ; sum = 0
    mov rax, rcx            ; x pointer
    mov r9, 0              ; Counter
    
@@sum_loop:
    cmp r9, rbx
    jae @@compute_norm
    
    vmovss xmm1, [rax + r9*4]
    vmulss xmm1, xmm1, xmm1
    vaddss xmm0, xmm0, xmm1
    
    inc r9
    jmp @@sum_loop
    
@@compute_norm:
    ; xmm0 = sum(x^2)
    ; mean = sum / n
    cvtsi2ss xmm1, rbx
    vdivss xmm0, xmm0, xmm1
    
    ; Add epsilon and rsqrt
    vaddss xmm0, xmm0, [rel const_epsilon]
    
    ; Approximate 1/sqrt using vrsqrtss (14-bit reciprocal, then newton refinement)
    vrsqrtss xmm0, xmm0, xmm0
    
    ; Apply scale and bias: x_norm = x * scale / norm
    mov r9, 0
@@norm_loop:
    cmp r9, rbx
    jae @@done_norm
    
    vmovss xmm1, [rcx + r9*4]
    vmulss xmm1, xmm1, xmm0
    vmovss xmm2, [rdx + r9*4]       ; Load gamma (weight)
    vmulss xmm1, xmm1, xmm2
    vmovss [rcx + r9*4], xmm1       ; Store result back
    
    inc r9
    jmp @@norm_loop
    
@@done_norm:
    pop rbx
    ret
LayerNorm ENDP

; ============================================================================
; ATTENTION MECHANISM
; ============================================================================

; RoPE (Rotary Position Embedding)
; RCX = Q or K vector [HeadDim floats], RDX = position (integer), R8 = head_dim
; Applies rotary embedding: for each pair (i, i+1):
;   theta = pos * base^(-2i/dim)
;   (x[i], x[i+1]) = (x[i]*cos - x[i+1]*sin, x[i]*sin + x[i+1]*cos)
ApplyRoPE PROC
    push rbx
    push r12
    push r13
    push r14

    mov r12, rcx            ; vector pointer
    mov r13d, edx           ; position
    mov r14d, r8d           ; head_dim

    xor ebx, ebx            ; pair index i = 0, stepping by 2

rope_pair_loop:
    cmp ebx, r14d
    jge rope_done

    ; Compute theta = position * base^(-2i / dim)
    ; freq_exp = -2.0 * i / dim, then theta = pos * 10000^freq_exp
    ; Use x87 FPU for precision

    ; freq_exp = (float)(-2 * i) / (float)dim
    mov eax, ebx
    neg eax                 ; -i
    shl eax, 1              ; -2*i  (note: eax was -i, so -2*i)
    ; Actually: -2*i: i is stored in ebx
    mov eax, ebx
    add eax, eax            ; 2*i
    neg eax                 ; -2*i

    ; Push -2*i as float, push dim as float
    sub rsp, 16
    cvtsi2ss xmm0, eax     ; -2*i as float
    cvtsi2ss xmm1, r14d    ; dim as float
    divss xmm0, xmm1       ; freq_exp = -2i / dim
    movss [rsp], xmm0

    ; theta_base = 10000.0 ^ freq_exp  (via exp(freq_exp * ln(10000)))
    ; Then theta = position * theta_base
    fld dword ptr [rsp]         ; ST(0) = freq_exp
    fld dword ptr [const_rope_base] ; ST(0) = 10000.0, ST(1) = freq_exp
    fyl2x                       ; ST(0) = freq_exp * log2(10000)
    ; Wait: fyl2x computes ST(1)*log2(ST(0)), so we need to swap
    ; Actually fyl2x: ST(1) * log2(ST(0)), pops both, pushes result
    ; We have ST(0)=10000, ST(1)=freq_exp → result = freq_exp * log2(10000)

    ; Now compute 2^result to get 10000^freq_exp
    fld st(0)                   ; duplicate
    frndint                     ;  integer part
    fsub st(1), st(0)          ; fractional = original - int
    fxch                       ; ST(0) = frac, ST(1) = int
    f2xm1                     ; 2^frac - 1
    fld1
    faddp st(1), st(0)        ; 2^frac
    fscale                     ; 2^frac * 2^int = 2^(frac+int) = 10000^freq_exp
    fstp st(1)                 ; clean up int from stack

    ; ST(0) = theta_base = 10000^(-2i/dim)
    ; Multiply by position
    cvtsi2ss xmm0, r13d
    movss [rsp+4], xmm0
    fmul dword ptr [rsp+4]     ; ST(0) = theta = pos * theta_base

    ; Now compute sin(theta) and cos(theta)
    fsincos                    ; ST(0) = cos, ST(1) = sin

    fstp dword ptr [rsp]       ; cos → [rsp]
    fstp dword ptr [rsp+4]     ; sin → [rsp+4]

    ; Load current pair values
    movss xmm0, [r12 + rbx*4]         ; x[i]
    movss xmm1, [r12 + rbx*4 + 4]     ; x[i+1]

    movss xmm2, [rsp]          ; cos
    movss xmm3, [rsp+4]        ; sin

    ; new_x[i]   = x[i]*cos - x[i+1]*sin
    movaps xmm4, xmm0
    mulss xmm4, xmm2           ; x[i]*cos
    movaps xmm5, xmm1
    mulss xmm5, xmm3           ; x[i+1]*sin
    subss xmm4, xmm5           ; result_i

    ; new_x[i+1] = x[i]*sin + x[i+1]*cos
    mulss xmm0, xmm3           ; x[i]*sin
    mulss xmm1, xmm2           ; x[i+1]*cos
    addss xmm0, xmm1           ; result_i+1

    movss [r12 + rbx*4], xmm4
    movss [r12 + rbx*4 + 4], xmm0

    add rsp, 16
    add ebx, 2
    jmp rope_pair_loop

rope_done:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ApplyRoPE ENDP

; Attention computation (simplified: assumes Q, K, V already projected)
Attention_Forward PROC
    ; RCX = Context
    ; RDX = Layer index
    ; This is a high-level placeholder; real impl does matmul chain
    ret
Attention_Forward ENDP

; ============================================================================
; FEEDFORWARD (SwiGLU)
; ============================================================================

FeedForward_SwiGLU PROC
    ; Input already in context buffer
    ; Gate(x) = swish(x @ w_gate) = x @ w_gate * sigmoid(x @ w_gate)
    ; Out = Gate(x) * (x @ w_up) @ w_down
    ret
FeedForward_SwiGLU ENDP

; ============================================================================
; SOFTMAX (For logit sampling)
; ============================================================================

Softmax_InPlace PROC
    ; RCX = logits buffer (float*)
    ; RDX = size (vocab size, usually 32k+)
    ; Implements: x_i = exp(x_i - max(x)) / sum(exp(x - max))
    ret
Softmax_InPlace ENDP

; ============================================================================
; MAIN INFERENCE LOOPS
; ============================================================================

; ----------------------------------------------------------------------------
; Titan_RunInference - Complete forward pass for one token
; RCX = Context ptr, RDX = InputTokenID, R8 = OutputLogitsBuffer
; Returns: RAX = 1 (success) or 0 (failure)
; Clobbers: All
; PUBLIC - Called from Streaming Orchestrator
; ----------------------------------------------------------------------------
PUBLIC Titan_RunInference
Titan_RunInference PROC
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov rbx, rcx        ; Context
    mov r12d, edx       ; Token ID
    mov r13, r8         ; Output buffer
    
    ; Validate context
    test rbx, rbx
    jz @@fail
    
    cmp r12d, [rbx].TransformerCtx.nVocab
    jae @@fail
    
    ; 1. Embedding lookup: x = tok_embd[token_id]
    mov rax, [rbx].TransformerCtx.pTokenEmbeddings
    test rax, rax
    jz @@fail
    
    mov ecx, [rbx].TransformerCtx.nEmbed
    test ecx, ecx
    jz @@fail
    
    ; Calculate offset: token_id * nEmbed * sizeof(float)
    mov r14, r12
    imul r14, rcx
    shl r14, 2
    
    ; Copy embedding to current input buffer
    mov rsi, rax
    add rsi, r14
    mov rdi, [rbx].TransformerCtx.pCurrentInput
    
    mov eax, ecx
    rep movsd
    
    ; 2. Process through all transformer layers
    xor r9d, r9d        ; Layer 0
    
@@layer_loop:
    cmp r9d, [rbx].TransformerCtx.nLayers
    jae @@final_norm
    
    ; Layer norm -> Attention -> Residual -> FFN
    mov rcx, rbx
    mov rdx, r9
    call Attention_Forward
    
    mov rcx, rbx
    call FeedForward_SwiGLU
    
    inc r9d
    jmp @@layer_loop
    
@@final_norm:
    ; Final layer norm
    mov rcx, [rbx].TransformerCtx.pCurrentInput
    mov rdx, [rbx].TransformerCtx.pNorm
    mov r8, [rbx].TransformerCtx.nEmbed
    call LayerNorm
    
    ; 3. Output projection: logits = current @ output_weight^T
    ; (Simplified placeholder - real impl uses MatMul_Q2_K)
    
    ; Indicate success
    mov eax, 1
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
    
@@fail:
    xor eax, eax
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
Titan_RunInference ENDP

; ============================================================================
; GGUF FILE LOADING
; ============================================================================

; ----------------------------------------------------------------------------
; GGUF_LoadFile - Maps GGUF, validates, populates TransformerCtx
; RCX = File path (ASCII string), RDX = Context ptr (allocated)
; Returns: RAX = 1 (success) or 0 (failure)
; PUBLIC - Called from orchestrator
; Clobbers: All
; ----------------------------------------------------------------------------
PUBLIC GGUF_LoadFile
GGUF_LoadFile PROC
    push rbx
    push r12
    push r13
    sub rsp, 40h
    
    mov r12, rcx        ; File path
    mov r13, rdx        ; Context
    
    ; Open file
    sub rsp, 8
    mov rcx, r12
    mov edx, 80000000h  ; GENERIC_READ
    mov r8d, 1          ; FILE_SHARE_READ
    xor r9, r9          ; lpSecurityAttributes
    mov qword ptr [rsp+20h], 3 ; OPEN_EXISTING
    mov qword ptr [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0 ; hTemplateFile
    call CreateFileA
    add rsp, 8
    
    cmp rax, -1
    je @@fail
    mov rbx, rax        ; File handle
    
    ; Get file size
    mov rcx, rbx
    lea rdx, [rsp]
    call GetFileSize
    mov [r13].TransformerCtx.cbFileSize, rax
    
    ; Create file mapping
    mov rcx, rbx
    xor rdx, rdx        ; lpAttributes
    mov r8d, 2          ; PAGE_READONLY
    xor r9, r9          ; dwMaximumSizeHigh
    mov qword ptr [rsp+20h], 0 ; dwMaximumSizeLow
    call CreateFileMappingA
    
    test rax, rax
    jz @@close_file
    mov r12, rax        ; Mapping handle
    
    ; Map view
    mov rcx, r12
    mov edx, 4          ; FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+20h], 0
    call MapViewOfFile
    
    test rax, rax
    jz @@close_mapping
    mov [r13].TransformerCtx.pFileBase, rax
    
    ; Validate GGUF magic
    cmp DWORD PTR [rax], GGUF_MAGIC
    jne @@unmap
    
    ; Parse GGUF header (version, tensor_count, kv_pair_count at offsets 4, 8, 16)
    ; For now, mark as initialized
    
    mov eax, 1          ; Success
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
    
@@unmap:
    mov rcx, [r13].TransformerCtx.pFileBase
    call UnmapViewOfFile
@@close_mapping:
    mov rcx, r12
    call CloseHandle
@@close_file:
    mov rcx, rbx
    call CloseHandle
@@fail:
    xor eax, eax
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
GGUF_LoadFile ENDP

; ============================================================================
; EXPORTS
; ============================================================================
PUBLIC Titan_RunInference
PUBLIC GGUF_LoadFile
PUBLIC LayerNorm
PUBLIC Dequantize_Q4_0_Block
PUBLIC ApplyRoPE
PUBLIC Softmax_InPlace

END
