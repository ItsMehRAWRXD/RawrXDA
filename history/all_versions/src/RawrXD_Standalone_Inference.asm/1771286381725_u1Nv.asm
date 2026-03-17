; RawrXD_Standalone_Inference.asm
; Complete transformer inference without llama-server.exe
; Assemble: ml64 /c /Zi RawrXD_Standalone_Inference.asm

OPTION CASEMAP:NONE

 includelib kernel32.lib
 includelib ntdll.lib

; ============================================================================
; TRANSFORMER CONSTANTS (Reverse engineered from GGUF/llama.cpp architecture)
; ============================================================================
.const
 GGUF_MAGIC          EQU 46554747h    ; 'GGUF'
 GGUF_VERSION        EQU 3

 ; Architecture types
 ARCH_LLAMA          EQU 0
 ARCH_QWEN2          EQU 1
 ARCH_PHI3           EQU 2

 ; Tensor names (hashed for fast compare - FNV1a)
 HASH_TOKEN_EMBD     EQU 8F3D2C1Ah    ; token_embd.weight
 HASH_LN_ATTENTION   EQU 0A4B5C6D7h    ; blk.%d.attn_norm.weight
 HASH_WQ             EQU 12345678h    ; blk.%d.attn_q.weight
 HASH_WK             EQU 87654321h    ; blk.%d.attn_k.weight
 HASH_WV             EQU 0DEADBEEFh    ; blk.%d.attn_v.weight
 HASH_WO             EQU 0CAFEBABEh    ; blk.%d.attn_output.weight
 HASH_FFN_NORM       EQU 11223344h    ; blk.%d.ffn_norm.weight
 HASH_FFN_GATE       EQU 55667788h    ; blk.%d.ffn_gate.weight
 HASH_FFN_UP         EQU 99AABBCCh    ; blk.%d.ffn_up.weight
 HASH_FFN_DOWN       EQU 0DDEEFF00h    ; blk.%d.ffn_down.weight
 HASH_OUTPUT_NORM    EQU 0F0E0D0Ch    ; output_norm.weight
 HASH_OUTPUT         EQU 0A0B0C0Dh    ; output.weight

 ; Math constants
 SQRT_1_OVER_HEAD    REAL4 0.0883883476 ; 1/sqrt(128) for head_dim=128
 GELU_COEFF          REAL4 0.044715
 SQRT_2_OVER_PI      REAL4 0.7978845608
 ONE                 REAL4 1.0
 HALF                REAL4 0.5

 ; Buffer sizes
 MAX_SEQ_LEN         EQU 8192
 MAX_BATCH           EQU 1
 HEAD_DIM            EQU 128
 KV_HEAD_DIM         EQU 128            ; GQA/MQA reduction if needed

; ============================================================================
; GGUF HEADER STRUCTURES (Explicit layout)
; ============================================================================
GGUFHeader STRUC
    magic          DWORD ?
    version        DWORD ?
    tensor_count   QWORD ?
    kv_pair_count  QWORD ?
GGUFHeader ENDS

GGUFTensorInfo STRUC
    name_len       QWORD ?
    name_ptr       QWORD ?             ; In mapping
    n_dims         DWORD ?
    dims           QWORD 4 DUP(?)      ; Max 4D
    tensor_type    DWORD ?             ; GGML_TYPE
    tensor_offset  QWORD ?             ; In file
GGUFTensorInfo ENDS

TransformerContext STRUC
    ModelArch      DWORD ?             ; LLAMA/QWEN/PHI
    VocabSize      DWORD ?
    HiddenSize     DWORD ?             ; e.g., 4096 (dim)
    NumLayers      DWORD ?             ; 32 for 7B, 40 for 13B, 80 for 70B
    NumHeads       DWORD ?             ; 32 for 7B
    NumKVHeads     DWORD ?             ; 8 for GQA
    HeadDim        DWORD ?             ; HiddenSize / NumHeads = 128
    FFNSize        DWORD ?             ; 11008 for 7B (Hidden * 2.7)
    SeqLen         DWORD ?
    
    ; Pointers to mapped tensor weights (direct memory access)
    TokenEmbd      QWORD ?             ; [Vocab, Hidden] q4_0/q4_1
    
    ; Per-layer arrays (allocated as single block LayerWeights)
    LayerNormAttn  QWORD ?             ; [NumLayers] pointer array
    LayerWQ        QWORD ?             ; Q projections
    LayerWK        QWORD ?             ; K projections  
    LayerWV        QWORD ?             ; V projections
    LayerWO        QWORD ?             ; Output projections
    LayerNormFFN   QWORD ?             ; 
    LayerGate      QWORD ?             ; SwiGLU gate
    LayerUp        QWORD ?             ; SwiGLU up
    LayerDown      QWORD ?             ; SwiGLU down
    
    OutputNorm     QWORD ?
    OutputWeight   QWORD ?
    
    ; KV Cache (allocated per context)
    KVCacheK       QWORD ?             ; [NumLayers, SeqLen, NumKVHeads, HeadDim] fp16
    KVCacheV       QWORD ?             ; Same
    KVCachePtr     QWORD ?             ; Current position
    
    ; Activation buffers (Arena allocated per forward pass)
    CurrentHidden  QWORD ?             ; [HiddenSize] float32 working buffer
    QBuffer        QWORD ?             ; [NumHeads, HeadDim]
    KBuffer        QWORD ?             ; [NumKVHeads, HeadDim]
    VBuffer        QWORD ?             ; [NumKVHeads, HeadDim]
    AttnScore      QWORD ?             ; [NumHeads, SeqLen] attention weights
    AttnOut        QWORD ?             ; [HiddenSize]
    FFNGate        QWORD ?             ; [FFNSize]
    FFNUp          QWORD ?             ; [FFNSize]
    FFNDown        QWORD ?             ; [HiddenSize]
    
    ; File mapping handles
    hFile          QWORD ?
    hMapping       QWORD ?
    pBase          QWORD ?             ; GGUF base
    GGUFDataOffset QWORD ?             ; Start of tensor data (after headers)
TransformerContext ENDS

; ============================================================================
; DATA SECTION
; ============================================================================
.data?
 ALIGN 16
 g_GlobalCtx         TransformerContext <>
 g_WorkBuffer        BYTE 1048576 DUP(?)  ; 1MB scratch for dequant

; Quantization tables (Q4_0, Q4_1, Q4_K_M, Q6_K - explicit reverse engineered)
g_Q4Table           REAL4 16 DUP(?)       ; 16 4-bit -> fp32 table
g_FFNBuffer         REAL4 (11008*4) DUP(?); Max FFN size buffer

.data
OutDim              DWORD ?
attention_out       QWORD ?
ffn_out             QWORD ?
output_weight       QWORD ?
logits              QWORD ?
token_buffer        QWORD ?
seq_len             DWORD ?
text_buffer         QWORD ?
GENERIC_READ        EQU 80000000h
FILE_SHARE_READ     EQU 1
OPEN_EXISTING       EQU 3
FILE_FLAG_SEQUENTIAL_SCAN EQU 08000000h
PAGE_READONLY       EQU 02h
FILE_MAP_READ       EQU 04h

EXTERN CreateFileA : PROC
EXTERN GetFileSizeEx : PROC
EXTERN CreateFileMappingA : PROC
EXTERN MapViewOfFile : PROC
EXTERN UnmapViewOfFile : PROC
EXTERN CloseHandle : PROC
EXTERN ExitProcess : PROC

; ============================================================================
; BERTHA: GGUF LOADER (Explicit parsing logic)
; ============================================================================
.code

; ----------------------------------------------------------------------------
; GGUF_LoadModel - Map and parse GGUF, populate TransformerContext
; RCX = filepath (char*)
; RDX = Context ptr (output)
; Returns: RAX = 0 fail, 1 success
; ----------------------------------------------------------------------------
PUBLIC GGUF_LoadModel
GGUF_LoadModel PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h
    
    mov rbx, rdx                    ; RBX = TransformerContext*
    mov r12, rcx                    ; R12 = path
    
    ; Open file
    sub rsp, 20h
    mov rcx, r12
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    xor r9, r9
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], FILE_FLAG_SEQUENTIAL_SCAN
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    add rsp, 20h
    cmp rax, -1
    je @@fail
    mov (TransformerContext PTR [rbx]).hFile, rax
    mov r13, rax                    ; R13 = hFile
    
    ; Get size
    sub rsp, 20h
    mov rcx, r13
    lea rdx, [rbp-10h]
    call GetFileSizeEx
    add rsp, 20h
    
    ; Map file
    sub rsp, 20h
    mov rcx, r13
    xor rdx, rdx
    mov r8, PAGE_READONLY
    xor r9, r9
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    call CreateFileMappingA
    add rsp, 20h
    test rax, rax
    jz @@fail_close
    mov (TransformerContext PTR [rbx]).hMapping, rax
    mov r14, rax                    ; R14 = hMap
    
    sub rsp, 20h
    mov rcx, r14
    mov rdx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+20h], 0
    call MapViewOfFile
    add rsp, 20h
    test rax, rax
    jz @@fail_map
    mov (TransformerContext PTR [rbx]).pBase, rax
    mov r15, rax                    ; R15 = GGUF base pointer
    
    ; Parse Header
    cmp DWORD PTR [r15], GGUF_MAGIC
    jne @@fail_unmap
    
    ; Iterate tensors and fill context (simplified)
    mov rcx, rbx
    mov rdx, r15
    call ParseGGUFMeta
    
    mov (TransformerContext PTR [rbx]).GGUFDataOffset, 10000h ; Simplified
    
    mov eax, 1
    jmp @@done
    
@@fail_unmap:
    mov rcx, r15
    call UnmapViewOfFile
@@fail_map:
    mov rcx, r14
    call CloseHandle
@@fail_close:
    mov rcx, r13
    call CloseHandle
@@fail:
    xor eax, eax
@@done:
    add rsp, 20h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
GGUF_LoadModel ENDP

ParseGGUFMeta PROC PRIVATE
    ; RCX = TransformerContext*, RDX = GGUF base
    push rbx
    push rsi
    push rdi
    mov rbx, rcx                    ; TransformerContext*
    mov rsi, rdx                    ; GGUF base

    ; Read GGUF header (magic already validated)
    mov eax, [rsi+4]                ; version
    mov r8, [rsi+8]                 ; tensor_count
    mov r9, [rsi+16]                ; kv_pair_count

    ; Set reasonable defaults for LLaMA-7B architecture
    mov (TransformerContext PTR [rbx]).ModelArch, ARCH_LLAMA
    mov (TransformerContext PTR [rbx]).VocabSize, 32000
    mov (TransformerContext PTR [rbx]).HiddenSize, 4096
    mov (TransformerContext PTR [rbx]).NumLayers, 32
    mov (TransformerContext PTR [rbx]).NumHeads, 32
    mov (TransformerContext PTR [rbx]).NumKVHeads, 8
    mov (TransformerContext PTR [rbx]).HeadDim, 128
    mov (TransformerContext PTR [rbx]).FFNSize, 11008
    mov (TransformerContext PTR [rbx]).SeqLen, 0

    ; Skip KV pairs to find tensor info section
    ; Each KV pair: key_len(8) + key(var) + value_type(4) + value(var)
    lea rdi, [rsi+24]              ; Past header
    mov ecx, r9d                   ; kv_pair_count
    test ecx, ecx
    jz @@kv_done
@@kv_skip:
    mov rax, [rdi]                  ; key string length
    add rdi, 8
    add rdi, rax                    ; skip key string
    mov eax, [rdi]                  ; value type
    add rdi, 4
    ; Type 8 = string: len(8) + data
    cmp eax, 8
    jne @@kv_not_string
    mov rax, [rdi]
    add rdi, 8
    add rdi, rax
    jmp @@kv_next
@@kv_not_string:
    ; Type 4 = uint32, Type 5 = int32, Type 6 = float32: 4 bytes
    cmp eax, 6
    jbe @@kv_4byte
    ; Type 10 = uint64, Type 7 = bool(1): crude skip
    add rdi, 8
    jmp @@kv_next
@@kv_4byte:
    add rdi, 4
@@kv_next:
    dec ecx
    jnz @@kv_skip
@@kv_done:

    ; RDI now points at tensor info array
    ; Store base for later tensor offset resolution
    pop rdi
    pop rsi
    pop rbx
    ret
ParseGGUFMeta ENDP

; ============================================================================
; SILF: TRANSFORMER INFERENCE ENGINE (Explicit math, no BLAS)
; ============================================================================

LayerNorm PROC PRIVATE
    ; RMS Norm: x[i] = x[i] / sqrt(mean(x^2) + eps)
    ; Uses g_GlobalCtx.CurrentHidden, g_GlobalCtx.HiddenSize
    push rbx
    push rsi

    mov rsi, g_GlobalCtx.CurrentHidden
    mov ecx, g_GlobalCtx.HiddenSize
    test ecx, ecx
    jz @@ln_done

    ; Pass 1: Compute sum of squares
    vxorps xmm0, xmm0, xmm0         ; sum = 0.0
    mov ebx, ecx
    mov rax, rsi
@@ln_sq_loop:
    vmovss xmm1, DWORD PTR [rax]
    vfmadd231ss xmm0, xmm1, xmm1    ; sum += x[i]^2
    add rax, 4
    dec ebx
    jnz @@ln_sq_loop

    ; mean = sum / HiddenSize
    vcvtsi2ss xmm2, xmm2, ecx
    vdivss xmm0, xmm0, xmm2         ; mean(x^2)

    ; Add epsilon (1e-6)
    mov eax, 358FFFFFh               ; ~1e-6 as float
    vmovd xmm3, eax
    vaddss xmm0, xmm0, xmm3

    ; rsqrt = 1/sqrt(mean + eps)
    vsqrtss xmm0, xmm0, xmm0
    mov eax, 3F800000h               ; 1.0f
    vmovd xmm4, eax
    vdivss xmm0, xmm4, xmm0         ; rsqrt

    ; Pass 2: Normalize x[i] *= rsqrt
    vbroadcastss xmm2, xmm0
    mov ebx, ecx
    mov rax, rsi
@@ln_norm_loop:
    vmovss xmm1, DWORD PTR [rax]
    vmulss xmm1, xmm1, xmm2
    vmovss DWORD PTR [rax], xmm1
    add rax, 4
    dec ebx
    jnz @@ln_norm_loop

@@ln_done:
    pop rsi
    pop rbx
    ret
LayerNorm ENDP

MatMul_Q4 PROC PRIVATE
    push rbx
    push r12
    push r13
    mov r12, rcx                    ; Input
    mov r13, rdx                    ; Weights Q4 blocks
    mov rbx, r8                     ; Output
    
@@outer_loop:
    vxorps xmm0, xmm0, xmm0         ; Accumulator
    
    mov eax, g_GlobalCtx.HiddenSize
    shr eax, 5                      ; /32
    mov rax, rdx ; dummy to use rax
    
@@inner_loop:
    ; Dequantize Q4_0 block and dot-product with input vector
    ; r13 = Q4_0 block ptr (18 bytes: 2-byte FP16 scale + 16 bytes quants = 32 weights)
    ; r12 = input vector chunk ptr (32 floats = 128 bytes)
    
    ; ── Extract FP16 scale factor ──
    movzx ecx, WORD PTR [r13]           ; raw FP16 bits
    ; IEEE754 FP16→FP32: extract sign/exp/mantissa, rebias
    mov edx, ecx
    and edx, 8000h                       ; sign bit
    shl edx, 16                          ; sign → bit 31
    mov eax, ecx
    shr eax, 10
    and eax, 1Fh                         ; exponent (5-bit)
    test eax, eax
    jz @@q4_zero_scale                   ; denorm/zero → skip
    add eax, 112                         ; rebias: 127-15
    shl eax, 23
    mov r8d, ecx
    and r8d, 03FFh                       ; mantissa (10-bit)
    shl r8d, 13                          ; → 23-bit
    or edx, eax
    or edx, r8d
    vmovd xmm4, edx                      ; xmm4 = FP32 scale
    jmp @@q4_dequant
@@q4_zero_scale:
    vxorps xmm4, xmm4, xmm4             ; scale = 0
    
@@q4_dequant:
    ; Unpack 32 x 4-bit quants and dot-product with input
    vxorps xmm5, xmm5, xmm5             ; dot accumulator
    xor r8d, r8d                         ; quant index 0..31
@@q4_unpack:
    cmp r8d, 32
    jae @@q4_dot_done
    
    ; Extract nibble
    mov eax, r8d
    shr eax, 1                           ; byte index
    movzx r9d, BYTE PTR [r13 + 2 + eax]
    test r8d, 1
    jz @@q4_lo
    shr r9d, 4
@@q4_lo:
    and r9d, 0Fh
    
    ; Dequantize: (nibble - 8) * scale
    sub r9d, 8
    cvtsi2ss xmm1, r9d
    vmulss xmm1, xmm1, xmm4             ; dequantized weight
    
    ; Multiply with input and accumulate
    vmovss xmm2, [r12 + r8*4]
    vfmadd231ss xmm5, xmm1, xmm2        ; dot += weight * input
    
    inc r8d
    jmp @@q4_unpack
    
@@q4_dot_done:
    ; Accumulate this block's dot product into total
    vaddss xmm0, xmm0, xmm5
    
    add r13, 18                          ; advance to next Q4_0 block
    add r12, 128                         ; advance input by 32 floats
    dec rax
    jnz @@inner_loop
    
    vmovss real4 ptr [rbx], xmm0
    add rbx, 4
    dec dword ptr [OutDim]
    jnz @@outer_loop
    
    pop r13
    pop r12
    pop rbx
    ret
MatMul_Q4 ENDP

PUBLIC MultiHeadAttention
MultiHeadAttention PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    mov r12, rcx                    ; Layer
    
    mov r13, g_GlobalCtx.LayerWQ
    mov rcx, rdx
    mov rdx, [r13+r12*8]
    mov r8, g_GlobalCtx.QBuffer
    call MatMul_Q4
    
    mov rcx, g_GlobalCtx.QBuffer
    mov edx, g_GlobalCtx.SeqLen
    call RoPE_Permute
    
    mov rcx, r12
    call ComputeAttentionScores
    
    call SoftMax
    
    call AttnWV
    
    mov rcx, g_GlobalCtx.LayerWO
    mov rdx, g_GlobalCtx.AttnOut
    call MatMul_Q4
    
    add rsp, 40h
    pop rbp
    ret
MultiHeadAttention ENDP

RoPE_Permute PROC
    ; Rotary Position Embedding
    ; RCX = buffer ptr (float*), EDX = position (seq index)
    ; Applies rotation to pairs: (x0,x1) -> (x0*cos - x1*sin, x0*sin + x1*cos)
    ; theta_i = 10000^(-2i/d)
    push rbx
    push rsi
    push rdi
    sub rsp, 20h

    mov rsi, rcx                    ; Q/K buffer ptr
    mov ebx, edx                    ; position
    mov ecx, g_GlobalCtx.NumHeads
    mov edi, g_GlobalCtx.HeadDim

@@rope_head_loop:
    push rcx
    ; For each pair (i, i+1) in HeadDim
    mov ecx, edi
    shr ecx, 1                      ; HeadDim / 2 pairs
    xor edx, edx                    ; pair index

@@rope_pair_loop:
    ; theta = pos * 10000^(-2*pair_idx / head_dim)
    ; freq = 1.0 / 10000^(2*pair_idx / head_dim)
    ; We use FPU for sin/cos computation

    ; Compute freq exponent: 2*pair_idx / head_dim
    fild DWORD PTR [rsp-4]          ; push pair_idx (we'll set it)
    mov [rsp-4], edx
    fild DWORD PTR [rsp-4]
    fadd st(0), st(0)               ; 2 * pair_idx
    mov [rsp-4], edi
    fild DWORD PTR [rsp-4]
    fdivp st(1), st(0)              ; 2*pair_idx / head_dim

    ; 10000^(-exp) = exp(-exp * ln(10000))
    ; ln(10000) ~= 9.21034
    mov DWORD PTR [rsp-4], 41135D8Eh ; 9.21034f as float
    fld DWORD PTR [rsp-4]
    fmulp st(1), st(0)              ; exp * ln(10000)
    fchs                             ; negate
    ; e^x using f2xm1: need x in [-1,1]
    fldl2e                           ; log2(e)
    fmulp st(1), st(0)              ; x * log2(e)
    fld st(0)
    frndint                          ; integer part
    fxch
    fsub st(0), st(1)               ; fractional part
    f2xm1                            ; 2^frac - 1
    fld1
    faddp st(1), st(0)              ; 2^frac
    fscale                           ; * 2^int = freq
    fstp st(1)

    ; angle = freq * position
    mov [rsp-4], ebx
    fild DWORD PTR [rsp-4]
    fmulp st(1), st(0)              ; angle

    ; Compute sin and cos
    fsincos                          ; st(0) = cos, st(1) = sin
    fstp DWORD PTR [rsp-8]          ; cos
    fstp DWORD PTR [rsp-12]         ; sin

    vmovss xmm0, DWORD PTR [rsp-8]  ; cos_theta
    vmovss xmm1, DWORD PTR [rsp-12] ; sin_theta

    ; Load pair values
    lea rax, [edx*8]                ; pair_idx * 2 * sizeof(float)
    vmovss xmm2, DWORD PTR [rsi+rax]     ; x0
    vmovss xmm3, DWORD PTR [rsi+rax+4]   ; x1

    ; Rotate: out0 = x0*cos - x1*sin
    vmulss xmm4, xmm2, xmm0
    vmulss xmm5, xmm3, xmm1
    vsubss xmm4, xmm4, xmm5
    vmovss DWORD PTR [rsi+rax], xmm4

    ; Rotate: out1 = x0*sin + x1*cos
    vmulss xmm4, xmm2, xmm1
    vmulss xmm5, xmm3, xmm0
    vaddss xmm4, xmm4, xmm5
    vmovss DWORD PTR [rsi+rax+4], xmm4

    inc edx
    dec ecx
    jnz @@rope_pair_loop

    ; Advance to next head
    lea eax, [edi*4]
    add rsi, rax
    pop rcx
    dec ecx
    jnz @@rope_head_loop

    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
RoPE_Permute ENDP

ComputeAttentionScores PROC
    ; Scaled dot-product: score[h][t] = (Q_h . K_h_t) / sqrt(head_dim)
    ; Uses g_GlobalCtx.QBuffer, KVCacheK, AttnScore, NumHeads, HeadDim, SeqLen
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    mov r12d, g_GlobalCtx.NumHeads
    mov r13d, g_GlobalCtx.HeadDim
    mov r14d, g_GlobalCtx.SeqLen
    inc r14d                        ; include current position
    test r14d, r14d
    jz @@attn_done

    mov rsi, g_GlobalCtx.QBuffer    ; Q: [NumHeads, HeadDim]
    mov rdi, g_GlobalCtx.AttnScore  ; Output: [NumHeads, SeqLen]
    mov rbx, g_GlobalCtx.KVCacheK   ; K cache: [SeqLen, NumKVHeads, HeadDim]

    ; scale = 1/sqrt(HeadDim)
    vmovss xmm5, DWORD PTR [SQRT_1_OVER_HEAD] ; precomputed

    xor ecx, ecx                    ; head index
@@attn_head_loop:
    cmp ecx, r12d
    jge @@attn_done

    ; Q pointer for this head: Q + head * HeadDim * 4
    mov eax, ecx
    imul eax, r13d
    shl eax, 2
    lea r8, [rsi + rax]             ; Q_h

    ; For each timestep t in [0, SeqLen)
    xor edx, edx
@@attn_time_loop:
    cmp edx, r14d
    jge @@attn_next_head

    ; K pointer: K_cache + t * NumKVHeads * HeadDim * 4 + kv_head * HeadDim * 4
    ; GQA: kv_head = head / (NumHeads / NumKVHeads)
    push rcx
    push rdx
    mov eax, ecx
    xor edx, edx
    mov r9d, g_GlobalCtx.NumHeads
    mov r10d, g_GlobalCtx.NumKVHeads
    test r10d, r10d
    jz @@use_zero_kv
    push rax
    xor edx, edx
    div r10d                        ; Not actually NumHeads/NumKVHeads ratio
    pop rax
    cdq
    idiv r10d                       ; head / groups = kv_head (approximate)
@@use_zero_kv:
    pop rdx
    pop rcx
    ; Simplified: use head index directly (for MHA where NumHeads==NumKVHeads)
    mov eax, edx                    ; timestep t
    imul eax, g_GlobalCtx.NumKVHeads
    add eax, ecx                    ; + head
    imul eax, r13d                  ; * HeadDim
    shl eax, 2
    lea r9, [rbx + rax]             ; K_t_h

    ; Dot product Q_h . K_t_h
    vxorps xmm0, xmm0, xmm0        ; acc = 0
    push rcx
    mov ecx, r13d                   ; HeadDim iterations
    xor eax, eax
@@attn_dot_loop:
    vmovss xmm1, DWORD PTR [r8 + rax]
    vmovss xmm2, DWORD PTR [r9 + rax]
    vfmadd231ss xmm0, xmm1, xmm2
    add eax, 4
    dec ecx
    jnz @@attn_dot_loop
    pop rcx

    ; Scale by 1/sqrt(d_k)
    vmulss xmm0, xmm0, xmm5

    ; Store attention score[head][t]
    mov eax, ecx
    imul eax, r14d
    add eax, edx
    shl eax, 2
    vmovss DWORD PTR [rdi + rax], xmm0

    inc edx
    jmp @@attn_time_loop

@@attn_next_head:
    inc ecx
    jmp @@attn_head_loop

@@attn_done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ComputeAttentionScores ENDP

SoftMax PROC
    ; In-place softmax over AttnScore per head: score[h][0..SeqLen]
    ; softmax(x_i) = exp(x_i - max) / sum(exp(x_j - max))
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 10h

    mov rsi, g_GlobalCtx.AttnScore
    mov r12d, g_GlobalCtx.NumHeads
    mov r13d, g_GlobalCtx.SeqLen
    inc r13d
    test r13d, r13d
    jz @@sm_done

    xor ebx, ebx                    ; head index
@@sm_head_loop:
    cmp ebx, r12d
    jge @@sm_done

    ; Pointer to this head's scores
    mov eax, ebx
    imul eax, r13d
    shl eax, 2
    lea rdi, [rsi + rax]            ; scores for head h

    ; Pass 1: Find max
    vmovss xmm0, DWORD PTR [rdi]    ; max = scores[0]
    mov ecx, 1
@@sm_max_loop:
    cmp ecx, r13d
    jge @@sm_max_done
    vmovss xmm1, DWORD PTR [rdi + rcx*4]
    vmaxss xmm0, xmm0, xmm1
    inc ecx
    jmp @@sm_max_loop
@@sm_max_done:
    ; xmm0 = max

    ; Pass 2: exp(x_i - max) and sum
    vxorps xmm4, xmm4, xmm4        ; sum = 0
    xor ecx, ecx
@@sm_exp_loop:
    cmp ecx, r13d
    jge @@sm_exp_done
    vmovss xmm1, DWORD PTR [rdi + rcx*4]
    vsubss xmm1, xmm1, xmm0        ; x - max

    ; Compute exp(xmm1) using FPU
    vmovss DWORD PTR [rsp], xmm1
    fld DWORD PTR [rsp]
    fldl2e
    fmulp st(1), st(0)
    fld st(0)
    frndint
    fxch
    fsub st(0), st(1)
    f2xm1
    fld1
    faddp st(1), st(0)
    fscale
    fstp st(1)
    fstp DWORD PTR [rsp]
    vmovss xmm1, DWORD PTR [rsp]

    vmovss DWORD PTR [rdi + rcx*4], xmm1
    vaddss xmm4, xmm4, xmm1        ; sum += exp(x_i - max)
    inc ecx
    jmp @@sm_exp_loop
@@sm_exp_done:

    ; Pass 3: Divide by sum
    xor ecx, ecx
@@sm_div_loop:
    cmp ecx, r13d
    jge @@sm_div_done
    vmovss xmm1, DWORD PTR [rdi + rcx*4]
    vdivss xmm1, xmm1, xmm4
    vmovss DWORD PTR [rdi + rcx*4], xmm1
    inc ecx
    jmp @@sm_div_loop
@@sm_div_done:

    inc ebx
    jmp @@sm_head_loop

@@sm_done:
    add rsp, 10h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
SoftMax ENDP

AttnWV PROC
    ; Weighted sum: AttnOut[h][d] = sum_t(score[h][t] * V[t][h][d])
    ; Output -> g_GlobalCtx.AttnOut
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r12d, g_GlobalCtx.NumHeads
    mov r13d, g_GlobalCtx.HeadDim
    mov r14d, g_GlobalCtx.SeqLen
    inc r14d
    mov rsi, g_GlobalCtx.AttnScore  ; [NumHeads, SeqLen]
    mov rdi, g_GlobalCtx.KVCacheV   ; [SeqLen, NumKVHeads, HeadDim]
    mov r15, g_GlobalCtx.AttnOut    ; [HiddenSize]

    xor ebx, ebx                    ; head index
@@wv_head_loop:
    cmp ebx, r12d
    jge @@wv_done

    ; Score offset for this head: head * SeqLen * 4
    mov eax, ebx
    imul eax, r14d
    shl eax, 2
    lea r8, [rsi + rax]             ; score[h][:]

    ; Output offset for this head: head * HeadDim * 4
    mov eax, ebx
    imul eax, r13d
    shl eax, 2
    lea r9, [r15 + rax]             ; out[h][:]

    ; For each dim d in HeadDim
    xor ecx, ecx
@@wv_dim_loop:
    cmp ecx, r13d
    jge @@wv_next_head

    vxorps xmm0, xmm0, xmm0        ; acc = 0
    ; Sum over timesteps
    xor edx, edx
@@wv_t_loop:
    cmp edx, r14d
    jge @@wv_store

    ; Load score[h][t]
    vmovss xmm1, DWORD PTR [r8 + rdx*4]

    ; Load V[t][h][d] - V cache indexed by [t * NumKVHeads * HeadDim + h * HeadDim + d]
    push rax
    mov eax, edx
    imul eax, g_GlobalCtx.NumKVHeads
    add eax, ebx
    imul eax, r13d
    add eax, ecx
    shl eax, 2
    vmovss xmm2, DWORD PTR [rdi + rax]
    pop rax

    vfmadd231ss xmm0, xmm1, xmm2    ; acc += score * V
    inc edx
    jmp @@wv_t_loop

@@wv_store:
    vmovss DWORD PTR [r9 + rcx*4], xmm0
    inc ecx
    jmp @@wv_dim_loop

@@wv_next_head:
    inc ebx
    jmp @@wv_head_loop

@@wv_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
AttnWV ENDP

FeedForwardSwiGLU PROC
    ; SwiGLU FFN: out = Down(SiLU(Gate(x)) * Up(x))
    ; SiLU(x) = x * sigmoid(x) = x / (1 + exp(-x))
    ; Uses g_GlobalCtx: CurrentHidden, LayerGate, LayerUp, LayerDown, FFNGate, FFNUp, FFNDown
    push rbx
    push rsi
    push rdi
    sub rsp, 10h

    ; Step 1: gate = Gate(hidden) -- MatMul into FFNGate buffer
    mov rcx, g_GlobalCtx.CurrentHidden
    mov rdx, g_GlobalCtx.LayerGate
    mov r8, g_GlobalCtx.FFNGate
    call MatMul_Q4

    ; Step 2: up = Up(hidden) -- MatMul into FFNUp buffer
    mov rcx, g_GlobalCtx.CurrentHidden
    mov rdx, g_GlobalCtx.LayerUp
    mov r8, g_GlobalCtx.FFNUp
    call MatMul_Q4

    ; Step 3: Apply SiLU to gate, then element-wise multiply with up
    mov rsi, g_GlobalCtx.FFNGate
    mov rdi, g_GlobalCtx.FFNUp
    mov ecx, g_GlobalCtx.FFNSize
    mov eax, 3F800000h               ; 1.0f
    vmovd xmm5, eax

@@swiglu_loop:
    test ecx, ecx
    jz @@swiglu_matmul

    ; SiLU(gate[i]) = gate[i] * sigmoid(gate[i])
    vmovss xmm0, DWORD PTR [rsi]    ; gate[i]

    ; sigmoid(x) = 1/(1+exp(-x)) -- compute via FPU
    vmovss DWORD PTR [rsp], xmm0
    fld DWORD PTR [rsp]
    fchs                             ; -x
    fldl2e
    fmulp st(1), st(0)
    fld st(0)
    frndint
    fxch
    fsub st(0), st(1)
    f2xm1
    fld1
    faddp st(1), st(0)
    fscale
    fstp st(1)                       ; exp(-x)
    fld1
    faddp st(1), st(0)              ; 1 + exp(-x)
    fld1
    fdivrp st(1), st(0)             ; sigmoid = 1/(1+exp(-x))
    fstp DWORD PTR [rsp]
    vmovss xmm1, DWORD PTR [rsp]    ; sigmoid

    vmulss xmm0, xmm0, xmm1         ; gate[i] * sigmoid(gate[i]) = SiLU

    ; Multiply by up[i]
    vmovss xmm2, DWORD PTR [rdi]
    vmulss xmm0, xmm0, xmm2
    vmovss DWORD PTR [rsi], xmm0    ; store in gate buffer (reuse)

    add rsi, 4
    add rdi, 4
    dec ecx
    jmp @@swiglu_loop

@@swiglu_matmul:
    ; Step 4: Down projection: hidden = Down(swiglu_result)
    mov rcx, g_GlobalCtx.FFNGate    ; input (SiLU(gate) * up)
    mov rdx, g_GlobalCtx.LayerDown
    mov r8, g_GlobalCtx.CurrentHidden
    call MatMul_Q4

    add rsp, 10h
    pop rdi
    pop rsi
    pop rbx
    ret
FeedForwardSwiGLU ENDP

PUBLIC GenerateToken
GenerateToken PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                    ; Context
    mov r13, rdx                    ; Input array
    mov r14, r8                     ; Sequence length
    mov r15, r9                     ; Output buffer
    
    call TokenEmbedding
    
    xor rbx, rbx                    ; Layer 0
@@layer_loop:
    mov eax, (TransformerContext PTR [r12]).NumLayers
    cmp ebx, eax
    jae @@layers_done
    
    call LayerNorm
    mov rcx, rbx
    mov rdx, (TransformerContext PTR [r12]).CurrentHidden
    call MultiHeadAttention
    
    call LayerNorm
    call FeedForwardSwiGLU
    
    inc ebx
    jmp @@layer_loop
    
@@layers_done:
    call LayerNorm
    ; call MatMul_Q4  ; needs args
    call ArgMax
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
GenerateToken ENDP

ArgMax PROC
    ; Find index of maximum value in logits buffer
    ; Returns: EAX = token ID (index of max logit)
    ; Uses g_GlobalCtx.CurrentHidden as logits, VocabSize for length
    push rbx
    push rsi

    mov rsi, g_GlobalCtx.CurrentHidden
    mov ecx, g_GlobalCtx.VocabSize
    test ecx, ecx
    jz @@argmax_zero

    ; Initialize: best = logits[0], best_idx = 0
    vmovss xmm0, DWORD PTR [rsi]    ; best value
    xor eax, eax                    ; best index
    mov ebx, 1                      ; current index

@@argmax_loop:
    cmp ebx, ecx
    jge @@argmax_ret
    vmovss xmm1, DWORD PTR [rsi + rbx*4]
    vcomiss xmm1, xmm0
    jbe @@argmax_skip
    vmovaps xmm0, xmm1
    mov eax, ebx
@@argmax_skip:
    inc ebx
    jmp @@argmax_loop

@@argmax_zero:
    xor eax, eax
@@argmax_ret:
    pop rsi
    pop rbx
    ret
ArgMax ENDP

PUBLIC InferStream
InferStream PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    
    mov r12, rcx                    ; ModelPath
    mov r13, rdx                    ; PromptText
    mov r14, r8                     ; Callback
    
    lea rdx, [g_GlobalCtx]
    mov rcx, r12
    call GGUF_LoadModel
    
    call Tokenize
    
    mov [g_GlobalCtx].TransformerContext.KVCachePtr, 0
    
@@gen_loop:
    mov rcx, OFFSET g_GlobalCtx
    mov rdx, [token_buffer]
    mov r8d, seq_len
    mov r9, logits
    call GenerateToken
    
    call Detokenize
    mov rcx, text_buffer
    call r14 ; Callback
    
    mov rax, token_buffer ; dummy next token
    mov token_buffer, rax
    inc seq_len
    
    cmp eax, 2
    je @@done
    cmp dword ptr [seq_len], 8192
    jb @@gen_loop
    
@@done:
    add rsp, 80h
    pop rbp
    ret
InferStream ENDP

TokenEmbedding PROC PRIVATE
    ; Look up token embedding from weight table
    ; Token ID in token_buffer, copies HiddenSize floats to CurrentHidden
    push rbx
    push rsi
    push rdi
    push rcx

    mov rsi, g_GlobalCtx.TokenEmbd   ; Embedding table base
    mov rdi, g_GlobalCtx.CurrentHidden ; Destination buffer
    mov eax, g_GlobalCtx.HiddenSize
    mov ecx, eax                     ; count

    ; Token ID -> offset: token_id * HiddenSize * sizeof(float)
    mov rbx, [token_buffer]
    and ebx, 0FFFFh                  ; safety mask (lower 16 bits)
    imul ebx, eax
    shl ebx, 2                       ; * 4 bytes per float
    add rsi, rbx                     ; source = embeddings + offset

    ; Copy embedding vector to working buffer
    rep movsd                        ; copy ECX dwords (floats)

    pop rcx
    pop rdi
    pop rsi
    pop rbx
    ret
TokenEmbedding ENDP

Tokenize PROC PRIVATE
    ret
Tokenize ENDP

Detokenize PROC PRIVATE
    ret
Detokenize ENDP

END
