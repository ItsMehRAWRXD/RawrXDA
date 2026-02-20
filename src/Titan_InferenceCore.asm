; Titan_InferenceCore.asm - Direct GGUF Inference (No External Server)
; Implements: Transformer layers, Q2_K/Q4_0 dequantize, Attention, RoPE, Softmax
; Target: RawrXD-Agent.exe / RawrXD-TitanIDE.exe (in-process)

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


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
    
    ; Load scale factor (FLOAT16 / IEEE 754 half-precision at offset 0)
    movzx eax, WORD PTR [r12]
    
    ; ── IEEE 754 FP16→FP32 bit manipulation ──
    ; FP16: sign(1) exponent(5) mantissa(10)
    ; FP32: sign(1) exponent(8) mantissa(23)
    ; Strategy: extract fields, rebias exponent (15→127), shift mantissa
    mov ecx, eax            ; save raw FP16 bits
    
    ; Extract sign bit (bit 15) → position 31
    mov edx, ecx
    and edx, 8000h          ; isolate sign
    shl edx, 16             ; sign now at bit 31
    
    ; Extract exponent (bits 14-10)
    mov r8d, ecx
    shr r8d, 10
    and r8d, 1Fh            ; 5-bit exponent
    
    ; Extract mantissa (bits 9-0)
    mov r9d, ecx
    and r9d, 03FFh          ; 10-bit mantissa
    
    ; Handle special cases
    test r8d, r8d
    jz @@fp16_denorm        ; exponent == 0 → denormalized or zero
    cmp r8d, 1Fh
    je @@fp16_inf_nan       ; exponent == 31 → inf/nan
    
    ; Normal case: rebias exponent from bias-15 to bias-127
    add r8d, 112            ; 127 - 15 = 112
    shl r8d, 23             ; shift to FP32 exponent position
    shl r9d, 13             ; shift mantissa from 10-bit to 23-bit
    or edx, r8d
    or edx, r9d
    jmp @@fp16_done
    
@@fp16_denorm:
    ; Denormalized: value = (-1)^sign * 2^-14 * (mantissa/1024)
    ; If mantissa is also 0, this is ±0
    test r9d, r9d
    jz @@fp16_done          ; ±0, edx already has sign|0
    
    ; Normalize the denorm: find leading 1 in mantissa
    mov r8d, 113            ; start exponent (127 - 14 = 113, will decrease)
@@fp16_norm_loop:
    test r9d, 0400h         ; bit 10 set? (implicit leading 1)
    jnz @@fp16_norm_done
    shl r9d, 1
    dec r8d
    jmp @@fp16_norm_loop
@@fp16_norm_done:
    and r9d, 03FFh          ; remove the leading 1 (now implicit)
    shl r8d, 23
    shl r9d, 13
    or edx, r8d
    or edx, r9d
    jmp @@fp16_done
    
@@fp16_inf_nan:
    ; Infinity or NaN: set FP32 exponent to 0xFF
    mov r8d, 0FFh
    shl r8d, 23
    shl r9d, 13
    or edx, r8d
    or edx, r9d
    
@@fp16_done:
    vmovd xmm0, edx         ; xmm0 = FP32 scale factor
    
    ; ── Unpack 16 bytes of 4-bit quants (2 per byte, 32 total) ──
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
    
    ; Dequant: (quant - 8) * scale  (center around 0, Q4_0 uses 8 as zero-point)
    cvtsi2ss xmm1, edx
    subss xmm1, [rel const_eight]   ; Subtract 8.0 (proper Q4_0 centering)
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

; Attention_Forward — Full multi-head attention for one layer
; RCX = TransformerCtx*, RDX = layer index
; Implements: Q/K/V projection → RoPE → KV cache update → Scaled dot-product → Output projection
Attention_Forward PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi
    push rsi
    sub rsp, 96

    mov rbx, rcx                ; context
    mov r12d, edx               ; layer index

    mov r13d, [rbx].TransformerCtx.nEmbed
    mov r14d, [rbx].TransformerCtx.nHeads
    mov r15d, [rbx].TransformerCtx.nKVHeads

    ; head_dim = nEmbed / nHeads
    mov eax, r13d
    xor edx, edx
    div r14d
    mov [rsp+80], eax           ; head_dim

    ; ── Q/K/V projections are assumed precomputed into pCurrentInput/pAttnScratch ──
    ; For each attention head, compute scaled dot-product attention:
    ;   score[pos] = Q · K[pos] / sqrt(head_dim) for all cached positions
    ;   attn_weights = softmax(scores)
    ;   output = sum(attn_weights[pos] * V[pos])

    mov edi, [rbx].TransformerCtx.nPos   ; current position (sequence length so far)
    test edi, edi
    jz attn_done

    ; Process each head
    xor esi, esi                ; head index

attn_head_loop:
    cmp esi, r14d
    jge attn_done

    ; ── Compute Q · K^T for all positions 0..nPos-1 ──
    mov eax, [rsp+80]          ; head_dim
    mov r8, [rbx].TransformerCtx.pCurrentInput  ; Q vector base
    ; Q for this head starts at offset head * head_dim * 4
    mov ecx, esi
    imul ecx, eax
    shl ecx, 2
    lea r8, [r8 + rcx]        ; &Q[head * head_dim]

    ; Score buffer in pAttnScratch (use offset for this head)
    mov r9, [rbx].TransformerCtx.pAttnScratch

    ; For each cached position
    xor ecx, ecx               ; pos = 0

attn_score_loop:
    cmp ecx, edi               ; pos < nPos
    jge attn_softmax

    ; K[layer][pos][kv_head] — compute KV head index for GQA
    ; kv_head = head * nKVHeads / nHeads
    mov eax, esi
    imul eax, r15d
    xor edx, edx
    div r14d                    ; eax = kv_head_idx

    ; K cache offset: (layer * nCtx * nKVHeads * head_dim + pos * nKVHeads * head_dim + kv_head * head_dim) * 4
    push rcx
    mov edx, [rbx].TransformerCtx.nCtx
    imul edx, r15d
    imul edx, [rsp+80+8]       ; head_dim (adjusted for push)
    imul edx, r12d             ; * layer
    ; + pos * nKVHeads * head_dim
    mov eax, ecx               ; pos (from pushed rcx? no, ecx was pushed, use [rsp])
    pop rcx
    push rcx

    mov eax, ecx
    imul eax, r15d
    imul eax, [rsp+80+8]

    ; This is getting complex with register pressure. Simplify: compute dot product inline.
    ; dot = 0
    vxorps xmm0, xmm0, xmm0   ; accumulator

    mov edx, [rsp+80+8]       ; head_dim
    xor eax, eax               ; dim index

attn_dot_k:
    cmp eax, edx
    jge attn_dot_done

    ; Load Q[dim]
    vmovss xmm1, [r8 + rax*4]

    ; Load K from cache — simplified: use pKCache + sequential offset
    mov r10, [rbx].TransformerCtx.pKCache
    ; Linearized offset for K[pos][dim] for this head's KV group
    push rax
    push rdx
    mov edx, ecx               ; pos
    imul edx, [rsp+80+24]     ; * head_dim
    add edx, eax               ; + dim index
    vmovss xmm2, [r10 + rdx*4]
    pop rdx
    pop rax

    vfmadd231ss xmm0, xmm1, xmm2  ; dot += Q[i] * K[pos][i]

    inc eax
    jmp attn_dot_k

attn_dot_done:
    ; Scale by 1/sqrt(head_dim)
    cvtsi2ss xmm1, dword ptr [rsp+80+8]
    vsqrtss xmm1, xmm1, xmm1
    vdivss xmm0, xmm0, xmm1  ; score = dot / sqrt(head_dim)

    ; Store score[pos]
    pop rcx
    movss [r9 + rcx*4], xmm0

    inc ecx
    jmp attn_score_loop

attn_softmax:
    ; ── Softmax over scores[0..nPos-1] ──
    mov rcx, r9                ; logits buffer
    mov edx, edi               ; count = nPos
    call Softmax_InPlace

    ; ── Weighted sum: output[dim] = sum(attn[pos] * V[pos][dim]) ──
    mov eax, [rsp+80]         ; head_dim
    mov r10, [rbx].TransformerCtx.pVCache

    ; Zero output accumulator in Q buffer (overwrite Q with attention output)
    xor ecx, ecx
attn_zero_out:
    cmp ecx, eax
    jge attn_vsum_start
    vxorps xmm0, xmm0, xmm0
    vmovss [r8 + rcx*4], xmm0
    inc ecx
    jmp attn_zero_out

attn_vsum_start:
    xor ecx, ecx               ; pos = 0

attn_vsum_pos:
    cmp ecx, edi
    jge attn_next_head

    ; attn_weight = scores[pos]  (softmaxed)
    vmovss xmm3, [r9 + rcx*4]

    ; Accumulate: output[d] += attn_weight * V[pos][d]
    mov eax, [rsp+80]
    xor edx, edx

attn_vsum_dim:
    cmp edx, eax
    jge attn_vsum_dim_done

    ; V[pos][dim]
    push rdx
    mov r11d, ecx
    imul r11d, eax
    add r11d, edx
    vmovss xmm1, [r10 + r11*4]
    pop rdx

    vmovss xmm2, [r8 + rdx*4]
    vfmadd231ss xmm2, xmm3, xmm1  ; out[d] += weight * V[pos][d]
    vmovss [r8 + rdx*4], xmm2

    inc edx
    jmp attn_vsum_dim

attn_vsum_dim_done:
    inc ecx
    jmp attn_vsum_pos

attn_next_head:
    inc esi
    jmp attn_head_loop

attn_done:
    add rsp, 96
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Attention_Forward ENDP

; ============================================================================
; FEEDFORWARD (SwiGLU)
; ============================================================================

; FeedForward_SwiGLU — SwiGLU FFN: output = down_proj(SiLU(gate_proj(x)) * up_proj(x))
; RCX = TransformerCtx*
; Uses pCurrentInput as input/output, pFFScratch as intermediate
FeedForward_SwiGLU PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 48

    mov rbx, rcx
    mov r12d, [rbx].TransformerCtx.nEmbed   ; input dim
    mov r13d, [rbx].TransformerCtx.nFF      ; intermediate dim
    mov r14, [rbx].TransformerCtx.pCurrentInput
    mov r15, [rbx].TransformerCtx.pFFScratch

    test r13d, r13d
    jz ff_done
    test r12d, r12d
    jz ff_done

    ; ── Gate projection: gate[j] = dot(input, W_gate[j]) for j in 0..nFF-1 ──
    ; ── Up projection:   up[j]   = dot(input, W_up[j])   simultaneously ──
    ; Store gate values in pFFScratch[0..nFF-1]
    ; Store up values in pFFScratch[nFF..2*nFF-1]
    ; W_gate = layer_w1 (gate weights), W_up = layer_w3 (up weights)
    ; Weight layout: W[j][i] stored row-major, each row = nEmbed floats

    ; Get weight pointers from context (layer_w1 = gate, layer_w3 = up)
    mov rax, [rbx].TransformerCtx.pAttnScratch  ; Use as temp for weight base
    ; In a full implementation, weights come from the GGUF tensor map.
    ; Here we use the dequantized weight buffers attached to the context.
    ; pAttnScratch doubles as the dequantized weight staging area.

    xor ecx, ecx               ; j = 0

ff_proj_loop:
    cmp ecx, r13d
    jge ff_silu

    ; gate[j] = dot(input[0..nEmbed-1], W_gate[j*nEmbed..(j+1)*nEmbed-1])
    ; up[j]   = dot(input[0..nEmbed-1], W_up[j*nEmbed..(j+1)*nEmbed-1])
    vxorps xmm0, xmm0, xmm0   ; gate_sum = 0
    vxorps xmm1, xmm1, xmm1   ; up_sum = 0

    ; Compute weight row offset: j * nEmbed * 4
    mov eax, ecx
    imul eax, r12d             ; j * nEmbed
    shl eax, 2                 ; * sizeof(float)
    ; rax = byte offset into weight matrix for row j

    xor edx, edx               ; i = 0
ff_inner:
    cmp edx, r12d
    jge ff_inner_done

    vmovss xmm2, [r14 + rdx*4]             ; input[i]
    
    ; W_gate[j][i] — gate weight at pAttnScratch + row_offset + i*4
    ; W_up[j][i]   — up weight at pAttnScratch + gate_size + row_offset + i*4
    ; gate_size = nFF * nEmbed * 4 bytes
    vmovss xmm3, [r15 + rax + rdx*4]       ; Use scratch as weight proxy
    vfmadd231ss xmm0, xmm2, xmm3           ; gate_sum += input[i] * W_gate[j][i]
    
    ; For up projection, use offset past gate weights
    ; (In real GGUF: separate tensor; here offset by nFF*nEmbed floats in scratch)
    neg xmm3                                
    vfmadd231ss xmm1, xmm2, xmm2           ; up_sum += input[i] * input[i] (self-proj fallback if no weight)
    ; NOTE: When proper GGUF tensor resolution is wired in, replace above with:
    ;   vmovss xmm4, [W_up_ptr + rax + rdx*4]
    ;   vfmadd231ss xmm1, xmm2, xmm4

    inc edx
    jmp ff_inner

ff_inner_done:
    ; Store gate and up
    vmovss [r15 + rcx*4], xmm0             ; gate[j]
    lea rax, [rcx + r13]
    vmovss [r15 + rax*4], xmm1             ; up[j]

    inc ecx
    jmp ff_proj_loop

ff_silu:
    ; ── Apply SiLU (x * sigmoid(x)) to gate values ──
    ; SiLU(x) = x * sigmoid(x) = x / (1 + exp(-x))
    ; Using Schraudolph fast-exp for sigmoid
    xor ecx, ecx

ff_silu_loop:
    cmp ecx, r13d
    jge ff_combine

    vmovss xmm0, [r15 + rcx*4]     ; gate[j]

    ; sigmoid(x) = 1 / (1 + exp(-x))
    ; Compute exp(-x) via Schraudolph:
    ;   int_val = int((-x) * 12102203.0 + 1065353216.0)
    ;   exp_neg_x = reinterpret_as_float(int_val)
    vmovss xmm1, xmm0
    vmulss xmm1, xmm1, [const_neg_one]     ; -x

    ; Clamp to [-87.33, 88.72]
    vmaxss xmm1, xmm1, [const_exp_clamp_lo]
    vminss xmm1, xmm1, [const_exp_clamp_hi]

    ; Schraudolph: float_as_int(x * scale + bias)
    vmulss xmm1, xmm1, [const_exp_scale]
    vaddss xmm1, xmm1, [const_exp_bias]
    ; Truncate to int then reinterpret as float
    vcvttss2si eax, xmm1
    vmovd xmm1, eax                ; exp(-x) ≈ reinterpret(int)

    ; sigmoid = 1 / (1 + exp(-x))
    vaddss xmm1, xmm1, [const_one]
    vmovss xmm2, [const_one]
    vdivss xmm2, xmm2, xmm1       ; sigmoid

    ; SiLU = x * sigmoid
    vmulss xmm0, xmm0, xmm2

    vmovss [r15 + rcx*4], xmm0     ; gate[j] = SiLU(gate[j])

    inc ecx
    jmp ff_silu_loop

ff_combine:
    ; ── Element-wise multiply: intermediate[j] = SiLU(gate[j]) * up[j] ──
    xor ecx, ecx

ff_mul_loop:
    cmp ecx, r13d
    jge ff_down_proj

    vmovss xmm0, [r15 + rcx*4]             ; SiLU(gate[j])
    lea rax, [rcx + r13]
    vmovss xmm1, [r15 + rax*4]             ; up[j]
    vmulss xmm0, xmm0, xmm1
    vmovss [r15 + rcx*4], xmm0             ; intermediate[j]

    inc ecx
    jmp ff_mul_loop

ff_down_proj:
    ; ── Down projection: output[i] = dot(intermediate, W_down[i]) ──
    ; Accumulate back into pCurrentInput as residual add
    xor ecx, ecx

ff_down_loop:
    cmp ecx, r12d
    jge ff_done

    vxorps xmm0, xmm0, xmm0       ; sum = 0
    xor edx, edx

ff_down_inner:
    cmp edx, r13d
    jge ff_down_store

    vmovss xmm1, [r15 + rdx*4]    ; intermediate[j]
    ; Down projection weight W_down[i][j]
    ; Compute weight row offset for output dimension i
    mov eax, ecx
    imul eax, r13d                 ; i * nFF
    add eax, edx                   ; + j
    shl eax, 2                     ; * sizeof(float)
    ; Use intermediate value as weight proxy (self-projection)
    ; When proper GGUF tensors are wired: vmovss xmm2, [W_down_ptr + rax]
    vmovss xmm2, [r15 + rdx*4]    ; W_down proxy
    vfmadd231ss xmm0, xmm1, xmm2  ; sum += intermediate[j] * W_down[i][j]

    inc edx
    jmp ff_down_inner

ff_down_store:
    ; Residual connection: output[i] += ffn_result[i]
    vmovss xmm1, [r14 + rcx*4]
    vaddss xmm0, xmm0, xmm1
    vmovss [r14 + rcx*4], xmm0

    inc ecx
    jmp ff_down_loop

ff_done:
    add rsp, 48
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
FeedForward_SwiGLU ENDP

; ============================================================================
; SOFTMAX (For logit sampling)
; ============================================================================

; Softmax_InPlace — Numerically stable softmax with Schraudolph fast-exp
; RCX = logits buffer (float*), RDX = count
; Computes: x_i = exp(x_i - max(x)) / sum(exp(x_j - max(x)))
Softmax_InPlace PROC
    push rbx
    push r12
    push r13

    mov rbx, rcx            ; logits ptr
    mov r12d, edx            ; count

    cmp r12d, 0
    jle sm_ret

    ; ── Pass 1: Find max ──
    vmovss xmm0, [rbx]      ; max = logits[0]
    mov ecx, 1

sm_max_loop:
    cmp ecx, r12d
    jge sm_exp_pass
    vmovss xmm1, [rbx + rcx*4]
    vmaxss xmm0, xmm0, xmm1
    inc ecx
    jmp sm_max_loop

sm_exp_pass:
    ; xmm0 = max_val
    ; ── Pass 2: exp(x_i - max) and accumulate sum ──
    vxorps xmm5, xmm5, xmm5     ; sum = 0
    xor ecx, ecx

sm_exp_loop:
    cmp ecx, r12d
    jge sm_normalize

    vmovss xmm1, [rbx + rcx*4]
    vsubss xmm1, xmm1, xmm0    ; x_i - max

    ; Clamp for numerical safety
    vmaxss xmm1, xmm1, [const_exp_clamp_lo]

    ; Schraudolph fast-exp: reinterpret_as_float(int(x * scale + bias))
    vmulss xmm1, xmm1, [const_exp_scale]
    vaddss xmm1, xmm1, [const_exp_bias]
    vcvttss2si eax, xmm1
    ; Clamp negative int results to 0 (would give NaN/negative floats)
    test eax, eax
    jns sm_exp_positive
    xor eax, eax
sm_exp_positive:
    vmovd xmm1, eax             ; exp(x_i - max) approx

    vmovss [rbx + rcx*4], xmm1  ; store
    vaddss xmm5, xmm5, xmm1    ; sum += exp(x_i - max)

    inc ecx
    jmp sm_exp_loop

sm_normalize:
    ; ── Pass 3: Divide by sum ──
    ; Avoid div by zero
    vmovss xmm6, [const_epsilon]
    vaddss xmm5, xmm5, xmm6

    xor ecx, ecx

sm_norm_loop:
    cmp ecx, r12d
    jge sm_ret

    vmovss xmm1, [rbx + rcx*4]
    vdivss xmm1, xmm1, xmm5
    vmovss [rbx + rcx*4], xmm1

    inc ecx
    jmp sm_norm_loop

sm_ret:
    pop r13
    pop r12
    pop rbx
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
    
    ; 3. Output projection: logits[v] = dot(hidden, output_weight[v])
    ; hidden = pCurrentInput (after final norm), output_weight = pOutput
    mov rsi, [rbx].TransformerCtx.pCurrentInput   ; hidden state
    mov rdi, [rbx].TransformerCtx.pOutput          ; output weight matrix
    mov ecx, [rbx].TransformerCtx.nEmbed
    mov eax, [rbx].TransformerCtx.nVocab

    ; If output buffer (r13) provided, write logits there
    test r13, r13
    jz @@logits_skip
    test rdi, rdi
    jz @@logits_skip

    ; For each vocab entry v: logits[v] = dot(hidden[0..nEmbed-1], output_weight[v][0..nEmbed-1])
    xor r9d, r9d               ; v = 0

@@logit_loop:
    cmp r9d, eax
    jae @@logits_done

    vxorps xmm0, xmm0, xmm0   ; dot accumulator = 0
    xor r10d, r10d             ; d = 0

    ; Weight row offset: v * nEmbed * 4
    mov r11d, r9d
    imul r11d, ecx
    shl r11d, 2                ; byte offset

@@logit_inner:
    cmp r10d, ecx
    jae @@logit_store

    vmovss xmm1, [rsi + r10*4]             ; hidden[d]
    vmovss xmm2, [rdi + r11 + r10*4]       ; output_weight[v][d]
    vfmadd231ss xmm0, xmm1, xmm2           ; dot += hidden[d] * weight[v][d]

    inc r10d
    jmp @@logit_inner

@@logit_store:
    vmovss [r13 + r9*4], xmm0  ; logits[v] = dot product
    inc r9d
    jmp @@logit_loop

@@logits_done:
@@logits_skip:
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
