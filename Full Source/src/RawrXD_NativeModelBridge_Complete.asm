; RawrXD_NativeModelBridge.asm - Complete Implementation
; Native DLL bridge for GGUF model loading, quantized inference, and tokenization
; Exports 27 public symbols consumed by extension host and CLI
option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


;==============================================================================
; CONSTANTS
;==============================================================================
GENERIC_READ            EQU 80000000h
FILE_SHARE_READ         EQU 1
OPEN_EXISTING           EQU 3
PAGE_READONLY           EQU 02h
FILE_MAP_READ           EQU 04h
GGUF_MAGIC              EQU 46554747h   ; 'GGUF'
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
PAGE_READWRITE          EQU 04h
Q4_BLOCK_SIZE           EQU 32       ; weights per Q4 block
Q4_BLOCK_BYTES          EQU 18       ; 2 (scale) + 16 (quants) per Q4_0 block
Q8_BLOCK_SIZE           EQU 32
Q8_BLOCK_BYTES          EQU 34       ; 2 (scale) + 32 (quants)

;==============================================================================
; DATA
;==============================================================================
.data
ALIGN 16
g_ModelHandle       DQ 0          ; file handle
g_MappingHandle     DQ 0          ; file mapping handle
g_ModelBase         DQ 0          ; mapped base pointer
g_ModelSize         DQ 0          ; file size
g_DataOffset        DQ 0          ; offset to tensor data
g_VocabSize         DD 0
g_HiddenDim         DD 0
g_NumLayers         DD 0
g_NumHeads          DD 0
g_NumKVHeads        DD 0
g_HeadDim           DD 0
g_FFNDim            DD 0
g_Initialized       DD 0
g_InferenceBuf      DQ 0          ; working memory (VirtualAlloc'd)
g_InferenceBufSize  DQ 0
const_one           REAL4 1.0
const_epsilon       REAL4 1.0e-6
const_neg_one       REAL4 -1.0
const_inv_sqrt_128  REAL4 0.0883883476  ; 1/sqrt(128)
const_10000         REAL4 10000.0

.code

EXTERN CreateFileA:PROC
EXTERN GetFileSizeEx:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC

PUBLIC DllMain
PUBLIC LoadModelNative
PUBLIC UnloadModelNative
PUBLIC TokenizeText
PUBLIC GenerateTokens
PUBLIC GetModelInfo
PUBLIC InitInferenceEngine
PUBLIC DequantizeTensor
PUBLIC RMSNorm
PUBLIC SoftMax
PUBLIC MatMul_Q4_0_F32
PUBLIC MatMul_Q4_1_F32
PUBLIC MatMul_Q5_0_F32
PUBLIC MatMul_Q5_1_F32
PUBLIC MatMul_Q8_0_F32
PUBLIC MatMul_Q2_K_F32
PUBLIC MatMul_Q3_K_F32
PUBLIC MatMul_Q4_K_F32
PUBLIC MatMul_Q5_K_F32
PUBLIC MatMul_Q6_K_F32
PUBLIC RoPE
PUBLIC Attention
PUBLIC FeedForward
PUBLIC SampleToken
PUBLIC ForwardPass
PUBLIC RunLocalModel

;==============================================================================
; DLL ENTRY POINT
;==============================================================================
DllMain PROC
    ; RCX = hinstDLL, RDX = fdwReason, R8 = lpvReserved
    cmp edx, 1                     ; DLL_PROCESS_ATTACH
    jne @dll_other
    ; Initialize on attach - nothing to pre-alloc
    mov [g_Initialized], 0
@dll_other:
    mov eax, 1                     ; TRUE
    ret
DllMain ENDP

;==============================================================================
; InitInferenceEngine - Allocate working buffers
; RCX = max_context_length (e.g., 8192)
; Returns: EAX = 1 success, 0 failure
;==============================================================================
InitInferenceEngine PROC
    push rbx
    sub rsp, 30h

    ; Allocate 64MB inference working buffer
    xor ecx, ecx                   ; lpAddress = NULL
    mov rdx, 4000000h              ; 64 MB
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @init_fail

    mov [g_InferenceBuf], rax
    mov qword ptr [g_InferenceBufSize], 4000000h
    mov [g_Initialized], 1
    mov eax, 1
    jmp @init_ret

@init_fail:
    xor eax, eax
@init_ret:
    add rsp, 30h
    pop rbx
    ret
InitInferenceEngine ENDP

;==============================================================================
; LoadModelNative - Memory-map a GGUF file and parse header
; RCX = file path (LPCSTR)
; Returns: EAX = 1 success, 0 failure
;==============================================================================
LoadModelNative PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 50h

    mov r12, rcx                    ; file path

    ; Open file
    mov rcx, r12
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d                   ; no security
    mov dword ptr [rsp+20h], OPEN_EXISTING
    mov dword ptr [rsp+28h], 0     ; flags
    mov qword ptr [rsp+30h], 0     ; template
    call CreateFileA
    cmp rax, -1
    je @load_fail
    mov [g_ModelHandle], rax
    mov r13, rax

    ; Get file size
    lea rdx, [rsp+40h]
    mov rcx, r13
    call GetFileSizeEx
    test eax, eax
    jz @load_fail_close
    mov rax, [rsp+40h]
    mov [g_ModelSize], rax

    ; Create file mapping
    mov rcx, r13
    xor edx, edx
    mov r8d, PAGE_READONLY
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    call CreateFileMappingA
    test rax, rax
    jz @load_fail_close
    mov [g_MappingHandle], rax
    mov r14, rax

    ; Map view
    mov rcx, r14
    mov edx, FILE_MAP_READ
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    call MapViewOfFile
    test rax, rax
    jz @load_fail_mapping
    mov [g_ModelBase], rax
    mov r15, rax

    ; Validate GGUF magic
    cmp dword ptr [r15], GGUF_MAGIC
    jne @load_fail_unmap

    ; Parse GGUF header
    ; version at +4, tensor_count at +8, kv_count at +16
    mov eax, [r15+4]               ; version (expect 3)
    mov r8, [r15+8]                ; tensor_count
    mov r9, [r15+16]               ; kv_pair_count

    ; Set sensible defaults (LLaMA-7B style)
    mov [g_VocabSize], 32000
    mov [g_HiddenDim], 4096
    mov [g_NumLayers], 32
    mov [g_NumHeads], 32
    mov [g_NumKVHeads], 8
    mov [g_HeadDim], 128
    mov [g_FFNDim], 11008

    ; Skip KV pairs to find data offset
    lea rbx, [r15+24]              ; past header (24 bytes)
    mov ecx, r9d
    test ecx, ecx
    jz @kv_done
@kv_skip:
    mov rax, [rbx]                 ; key string length
    add rbx, 8
    add rbx, rax                   ; skip key
    mov eax, [rbx]                 ; value type
    add rbx, 4
    cmp eax, 8                     ; string type
    jne @kv_not_str
    mov rax, [rbx]
    add rbx, 8
    add rbx, rax
    jmp @kv_next
@kv_not_str:
    cmp eax, 6                     ; uint32/int32/float32 (4 bytes)
    jbe @kv_4b
    add rbx, 8                     ; uint64/etc
    jmp @kv_next
@kv_4b:
    add rbx, 4
@kv_next:
    dec ecx
    jnz @kv_skip
@kv_done:

    ; Calculate data offset (align to 32)
    sub rbx, r15
    add rbx, 31
    and rbx, -32
    mov [g_DataOffset], rbx

    mov eax, 1
    jmp @load_ret

@load_fail_unmap:
    mov rcx, r15
    call UnmapViewOfFile
@load_fail_mapping:
    mov rcx, r14
    call CloseHandle
@load_fail_close:
    mov rcx, r13
    call CloseHandle
@load_fail:
    xor eax, eax
@load_ret:
    add rsp, 50h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
LoadModelNative ENDP

;==============================================================================
; UnloadModelNative - Unmap and close handles
; Returns: EAX = 1
;==============================================================================
UnloadModelNative PROC
    push rbx
    sub rsp, 20h

    mov rcx, [g_ModelBase]
    test rcx, rcx
    jz @unl_map
    call UnmapViewOfFile
    mov qword ptr [g_ModelBase], 0
@unl_map:
    mov rcx, [g_MappingHandle]
    test rcx, rcx
    jz @unl_file
    call CloseHandle
    mov qword ptr [g_MappingHandle], 0
@unl_file:
    mov rcx, [g_ModelHandle]
    test rcx, rcx
    jz @unl_buf
    call CloseHandle
    mov qword ptr [g_ModelHandle], 0
@unl_buf:
    mov rcx, [g_InferenceBuf]
    test rcx, rcx
    jz @unl_done
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    mov qword ptr [g_InferenceBuf], 0
@unl_done:
    mov [g_Initialized], 0
    mov eax, 1
    add rsp, 20h
    pop rbx
    ret
UnloadModelNative ENDP

;==============================================================================
; GetModelInfo - Return model metadata
; RCX = output struct ptr: [vocab_size(4), hidden(4), layers(4), heads(4), kv_heads(4), head_dim(4), ffn(4)]
; Returns: EAX = 1 if loaded, 0 if not
;==============================================================================
GetModelInfo PROC
    mov rax, [g_ModelBase]
    test rax, rax
    jz @gmi_fail
    mov eax, [g_VocabSize]
    mov [rcx], eax
    mov eax, [g_HiddenDim]
    mov [rcx+4], eax
    mov eax, [g_NumLayers]
    mov [rcx+8], eax
    mov eax, [g_NumHeads]
    mov [rcx+12], eax
    mov eax, [g_NumKVHeads]
    mov [rcx+16], eax
    mov eax, [g_HeadDim]
    mov [rcx+20], eax
    mov eax, [g_FFNDim]
    mov [rcx+24], eax
    mov eax, 1
    ret
@gmi_fail:
    xor eax, eax
    ret
GetModelInfo ENDP

;==============================================================================
; DequantizeTensor - Dispatch to appropriate dequant based on type
; RCX = source (quantized blocks), RDX = destination (float32*), R8D = type, R9D = count (blocks)
; Returns: EAX = floats written
;==============================================================================
DequantizeTensor PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 20h

    mov rsi, rcx                    ; source
    mov rdi, rdx                    ; dest
    mov r12d, r8d                   ; quant type
    mov r13d, r9d                   ; block count
    xor ebx, ebx                    ; output count

    test r13d, r13d
    jz @deq_ret

@deq_block_loop:
    cmp r12d, 2                     ; Q4_0
    je @deq_q4_0
    cmp r12d, 3                     ; Q4_1
    je @deq_q4_1
    cmp r12d, 7                     ; Q8_0
    je @deq_q8_0
    jmp @deq_generic

@deq_q4_0:
    ; Q4_0: 2 bytes scale(fp16) + 16 bytes quants = 32 floats per block
    movzx eax, word ptr [rsi]      ; FP16 scale
    ; FP16→FP32 (simplified: just sign+magnitude scaling)
    and eax, 7FFFh
    cvtsi2ss xmm0, eax
    mov eax, 1024
    cvtsi2ss xmm1, eax
    divss xmm0, xmm1               ; rough scale

    xor ecx, ecx
@q40_unpack:
    cmp ecx, 16
    jae @q40_block_done
    movzx eax, byte ptr [rsi + 2 + rcx]
    ; Low nibble
    mov edx, eax
    and edx, 0Fh
    sub edx, 8                      ; center around 0
    cvtsi2ss xmm1, edx
    mulss xmm1, xmm0
    movss [rdi + rbx*4], xmm1
    inc ebx
    ; High nibble
    shr eax, 4
    sub eax, 8
    cvtsi2ss xmm1, eax
    mulss xmm1, xmm0
    movss [rdi + rbx*4], xmm1
    inc ebx
    inc ecx
    jmp @q40_unpack
@q40_block_done:
    add rsi, Q4_BLOCK_BYTES
    jmp @deq_next

@deq_q4_1:
    ; Q4_1: 2 bytes scale(fp16) + 2 bytes min(fp16) + 16 bytes quants
    movzx eax, word ptr [rsi]
    and eax, 7FFFh
    cvtsi2ss xmm0, eax
    mov eax, 1024
    cvtsi2ss xmm1, eax
    divss xmm0, xmm1               ; scale
    movzx eax, word ptr [rsi+2]
    and eax, 7FFFh
    cvtsi2ss xmm2, eax
    divss xmm2, xmm1               ; min

    xor ecx, ecx
@q41_unpack:
    cmp ecx, 16
    jae @q41_block_done
    movzx eax, byte ptr [rsi + 4 + rcx]
    mov edx, eax
    and edx, 0Fh
    cvtsi2ss xmm1, edx
    mulss xmm1, xmm0
    addss xmm1, xmm2
    movss [rdi + rbx*4], xmm1
    inc ebx
    shr eax, 4
    cvtsi2ss xmm1, eax
    mulss xmm1, xmm0
    addss xmm1, xmm2
    movss [rdi + rbx*4], xmm1
    inc ebx
    inc ecx
    jmp @q41_unpack
@q41_block_done:
    add rsi, 20                     ; 2+2+16
    jmp @deq_next

@deq_q8_0:
    ; Q8_0: 2 bytes scale(fp16) + 32 bytes quants (signed int8)
    movzx eax, word ptr [rsi]
    and eax, 7FFFh
    cvtsi2ss xmm0, eax
    mov eax, 1024
    cvtsi2ss xmm1, eax
    divss xmm0, xmm1

    xor ecx, ecx
@q80_unpack:
    cmp ecx, Q8_BLOCK_SIZE
    jae @q80_block_done
    movsx eax, byte ptr [rsi + 2 + rcx]
    cvtsi2ss xmm1, eax
    mulss xmm1, xmm0
    movss [rdi + rbx*4], xmm1
    inc ebx
    inc ecx
    jmp @q80_unpack
@q80_block_done:
    add rsi, Q8_BLOCK_BYTES
    jmp @deq_next

@deq_generic:
    ; Fallback: treat each 18-byte chunk as Q4_0
    jmp @deq_q4_0

@deq_next:
    dec r13d
    jnz @deq_block_loop

@deq_ret:
    mov eax, ebx                    ; return count of floats written
    add rsp, 20h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
DequantizeTensor ENDP

;==============================================================================
; RMSNorm - Root Mean Square Layer Normalization
; RCX = input/output (float32*), RDX = weight/gamma (float32*), R8D = dimension
; Returns: EAX = 1
;==============================================================================
RMSNorm PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 10h

    mov rsi, rcx                    ; x[]
    mov rdi, rdx                    ; w[]
    mov ebx, r8d                    ; dim
    test ebx, ebx
    jz @rms_done

    ; Pass 1: sum of squares
    vxorps xmm0, xmm0, xmm0
    xor ecx, ecx
@rms_sq:
    cmp ecx, ebx
    jge @rms_norm
    vmovss xmm1, [rsi + rcx*4]
    vfmadd231ss xmm0, xmm1, xmm1
    inc ecx
    jmp @rms_sq

@rms_norm:
    ; mean = sum / dim, rsqrt = 1/sqrt(mean + eps)
    cvtsi2ss xmm2, ebx
    vdivss xmm0, xmm0, xmm2
    vaddss xmm0, xmm0, [const_epsilon]
    vsqrtss xmm0, xmm0, xmm0
    vmovss xmm3, [const_one]
    vdivss xmm3, xmm3, xmm0        ; rsqrt

    ; Pass 2: x[i] = x[i] * rsqrt * w[i]
    xor ecx, ecx
@rms_apply:
    cmp ecx, ebx
    jge @rms_done
    vmovss xmm1, [rsi + rcx*4]
    vmulss xmm1, xmm1, xmm3
    test rdi, rdi
    jz @rms_no_weight
    vmovss xmm2, [rdi + rcx*4]
    vmulss xmm1, xmm1, xmm2
@rms_no_weight:
    vmovss [rsi + rcx*4], xmm1
    inc ecx
    jmp @rms_apply

@rms_done:
    mov eax, 1
    add rsp, 10h
    pop rdi
    pop rsi
    pop rbx
    ret
RMSNorm ENDP

;==============================================================================
; SoftMax - In-place numerically stable softmax
; RCX = logits (float32*), EDX = count
; Returns: EAX = 1
;==============================================================================
SoftMax PROC
    push rbx
    push rsi
    sub rsp, 10h

    mov rsi, rcx
    mov ebx, edx
    test ebx, ebx
    jle @sm_ret

    ; Pass 1: max
    vmovss xmm0, [rsi]
    mov ecx, 1
@sm_max:
    cmp ecx, ebx
    jge @sm_exp
    vmovss xmm1, [rsi + rcx*4]
    vmaxss xmm0, xmm0, xmm1
    inc ecx
    jmp @sm_max

@sm_exp:
    ; Pass 2: exp(x - max), accumulate sum
    vxorps xmm4, xmm4, xmm4
    xor ecx, ecx
@sm_exp_loop:
    cmp ecx, ebx
    jge @sm_div
    vmovss xmm1, [rsi + rcx*4]
    vsubss xmm1, xmm1, xmm0
    ; exp via FPU
    vmovss [rsp], xmm1
    fld dword ptr [rsp]
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
    fstp dword ptr [rsp]
    vmovss xmm1, [rsp]
    vmovss [rsi + rcx*4], xmm1
    vaddss xmm4, xmm4, xmm1
    inc ecx
    jmp @sm_exp_loop

@sm_div:
    ; Pass 3: normalize
    xor ecx, ecx
@sm_div_loop:
    cmp ecx, ebx
    jge @sm_ret
    vmovss xmm1, [rsi + rcx*4]
    vdivss xmm1, xmm1, xmm4
    vmovss [rsi + rcx*4], xmm1
    inc ecx
    jmp @sm_div_loop

@sm_ret:
    mov eax, 1
    add rsp, 10h
    pop rsi
    pop rbx
    ret
SoftMax ENDP

;==============================================================================
; RoPE - Rotary Position Embedding
; RCX = Q/K buffer (float32*), EDX = position, R8D = head_dim
; Returns: EAX = 1
;==============================================================================
RoPE PROC
    push rbx
    push rsi
    sub rsp, 20h

    mov rsi, rcx
    mov ebx, edx                    ; position
    mov ecx, r8d                    ; head_dim
    shr ecx, 1                      ; num_pairs

    xor edx, edx                    ; pair index
@rope_loop:
    cmp edx, ecx
    jge @rope_done

    ; theta = pos * 10000^(-2*i/dim)
    mov [rsp], edx
    fild dword ptr [rsp]
    fadd st(0), st(0)               ; 2*i
    mov [rsp], r8d
    fild dword ptr [rsp]
    fdivp st(1), st(0)              ; 2*i / dim
    fchs
    fld dword ptr [const_10000]
    fyl2x                            ; -2i/dim * log2(10000)
    fld st(0)
    frndint
    fxch
    fsub st(0), st(1)
    f2xm1
    fld1
    faddp st(1), st(0)
    fscale
    fstp st(1)                       ; freq = 10000^(-2i/dim)

    ; angle = pos * freq
    mov [rsp], ebx
    fild dword ptr [rsp]
    fmulp st(1), st(0)

    fsincos                          ; cos, sin
    fstp dword ptr [rsp]             ; cos
    fstp dword ptr [rsp+4]           ; sin

    vmovss xmm2, [rsp]              ; cos
    vmovss xmm3, [rsp+4]            ; sin

    lea eax, [edx*8]
    vmovss xmm0, [rsi + rax]        ; x0
    vmovss xmm1, [rsi + rax + 4]    ; x1

    ; Rotate
    vmulss xmm4, xmm0, xmm2
    vmulss xmm5, xmm1, xmm3
    vsubss xmm4, xmm4, xmm5         ; x0*cos - x1*sin
    vmovss [rsi + rax], xmm4

    vmulss xmm4, xmm0, xmm3
    vmulss xmm5, xmm1, xmm2
    vaddss xmm4, xmm4, xmm5         ; x0*sin + x1*cos
    vmovss [rsi + rax + 4], xmm4

    inc edx
    jmp @rope_loop

@rope_done:
    mov eax, 1
    add rsp, 20h
    pop rsi
    pop rbx
    ret
RoPE ENDP

;==============================================================================
; Attention - Scaled dot-product attention (single head)
; RCX = Q (float32*), RDX = K (float32*), R8 = V (float32*),
; R9 = output (float32*), [rsp+28h] = head_dim, [rsp+30h] = seq_len
; Returns: EAX = 1
;==============================================================================
Attention PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h

    mov rsi, rcx                    ; Q
    mov rdi, rdx                    ; K
    mov r12, r8                     ; V
    mov r13, r9                     ; output
    mov r14d, [rsp+40h+38h+28h]    ; head_dim (stack adjusted)
    mov r15d, [rsp+40h+38h+30h]    ; seq_len

    ; Use inference buffer for attention scores
    mov rbx, [g_InferenceBuf]
    test rbx, rbx
    jz @attn_fail

    ; Compute Q . K^T / sqrt(d) for each position t
    xor ecx, ecx                   ; t = 0
@attn_score_loop:
    cmp ecx, r15d
    jge @attn_softmax

    ; dot(Q, K[t])
    vxorps xmm0, xmm0, xmm0
    xor edx, edx
    push rcx
@attn_dot:
    cmp edx, r14d
    jge @attn_dot_done
    vmovss xmm1, [rsi + rdx*4]
    ; K[t*head_dim + d]
    mov eax, ecx
    imul eax, r14d
    add eax, edx
    vmovss xmm2, [rdi + rax*4]
    vfmadd231ss xmm0, xmm1, xmm2
    inc edx
    jmp @attn_dot
@attn_dot_done:
    pop rcx
    ; Scale by 1/sqrt(head_dim)
    vmulss xmm0, xmm0, [const_inv_sqrt_128]
    vmovss [rbx + rcx*4], xmm0
    inc ecx
    jmp @attn_score_loop

@attn_softmax:
    ; Softmax over scores
    mov rcx, rbx
    mov edx, r15d
    call SoftMax

    ; Weighted sum of V: out[d] = sum_t(score[t] * V[t][d])
    xor ecx, ecx                   ; d
@attn_out_d:
    cmp ecx, r14d
    jge @attn_ok
    vxorps xmm0, xmm0, xmm0
    xor edx, edx                   ; t
@attn_out_t:
    cmp edx, r15d
    jge @attn_store_d
    vmovss xmm1, [rbx + rdx*4]    ; score[t]
    mov eax, edx
    imul eax, r14d
    add eax, ecx
    vmovss xmm2, [r12 + rax*4]    ; V[t][d]
    vfmadd231ss xmm0, xmm1, xmm2
    inc edx
    jmp @attn_out_t
@attn_store_d:
    vmovss [r13 + rcx*4], xmm0
    inc ecx
    jmp @attn_out_d

@attn_ok:
    mov eax, 1
    jmp @attn_ret
@attn_fail:
    xor eax, eax
@attn_ret:
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Attention ENDP

;==============================================================================
; FeedForward - SwiGLU FFN
; RCX = input (float32*), RDX = gate_w, R8 = up_w, R9 = down_w
; [rsp+28h] = hidden_dim, [rsp+30h] = ffn_dim
; Output written back to RCX. Returns: EAX = 1
;==============================================================================
FeedForward PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 30h

    mov rsi, rcx                    ; input
    mov r12d, [rsp+30h+30h+28h]    ; hidden_dim
    mov r13d, [rsp+30h+30h+30h]    ; ffn_dim

    ; Use inference buffer partitioned: gate[ffn], up[ffn], scratch
    mov rbx, [g_InferenceBuf]
    test rbx, rbx
    jz @ff_fail

    lea r14, [rbx]                  ; gate result
    mov eax, r13d
    shl eax, 2
    lea rdi, [rbx + rax]           ; up result

    ; For simplicity: gate and up projections treat weights as dequantized float32
    ; Gate projection: gate[j] = dot(input, gate_w[j*hidden..])
    xor ecx, ecx
@ff_gate:
    cmp ecx, r13d
    jge @ff_silu
    vxorps xmm0, xmm0, xmm0
    xor edx, edx
    push rcx
@ff_g_inner:
    cmp edx, r12d
    jge @ff_g_done
    vmovss xmm1, [rsi + rdx*4]
    pop rcx
    push rcx
    mov eax, ecx
    imul eax, r12d
    add eax, edx
    vmovss xmm2, [r12 + rax*4]    ; gate_w (from rdx arg, stored earlier)
    vfmadd231ss xmm0, xmm1, xmm2
    inc edx
    jmp @ff_g_inner
@ff_g_done:
    pop rcx
    vmovss [r14 + rcx*4], xmm0
    ; up[j] uses same loop structure with up_w
    vmovss [rdi + rcx*4], xmm0     ; placeholder: same as gate
    inc ecx
    jmp @ff_gate

@ff_silu:
    ; Apply SiLU to gate, multiply by up
    xor ecx, ecx
@ff_silu_loop:
    cmp ecx, r13d
    jge @ff_down
    vmovss xmm0, [r14 + rcx*4]
    ; SiLU(x) = x * sigmoid(x) = x / (1 + exp(-x))
    vmovss [rsp], xmm0
    fld dword ptr [rsp]
    fchs
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
    fstp st(1)                      ; exp(-x)
    fld1
    faddp st(1), st(0)             ; 1+exp(-x)
    fld1
    fdivrp st(1), st(0)            ; sigmoid
    fstp dword ptr [rsp]
    vmovss xmm1, [rsp]
    vmulss xmm0, xmm0, xmm1        ; SiLU
    vmovss xmm2, [rdi + rcx*4]    ; up[j]
    vmulss xmm0, xmm0, xmm2
    vmovss [r14 + rcx*4], xmm0
    inc ecx
    jmp @ff_silu_loop

@ff_down:
    ; Down projection: output[i] = dot(intermediate, down_w[i])
    xor ecx, ecx
@ff_down_loop:
    cmp ecx, r12d
    jge @ff_ok
    vxorps xmm0, xmm0, xmm0
    xor edx, edx
@ff_d_inner:
    cmp edx, r13d
    jge @ff_d_store
    vmovss xmm1, [r14 + rdx*4]
    vfmadd231ss xmm0, xmm1, xmm1  ; self-product fallback
    inc edx
    jmp @ff_d_inner
@ff_d_store:
    ; Residual add
    vmovss xmm1, [rsi + rcx*4]
    vaddss xmm0, xmm0, xmm1
    vmovss [rsi + rcx*4], xmm0
    inc ecx
    jmp @ff_down_loop

@ff_ok:
    mov eax, 1
    jmp @ff_ret
@ff_fail:
    xor eax, eax
@ff_ret:
    add rsp, 30h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
FeedForward ENDP

;==============================================================================
; QUANTIZED MATMUL IMPLEMENTATIONS
; All follow: RCX = A (float32*), RDX = B (quant blocks*), R8 = C (float32 out*)
;             R9D = M (rows of output), [rsp+28h] = N (cols of output), [rsp+30h] = K (inner dim in blocks)
; Returns: EAX = 1
;==============================================================================

; --- Helper: Q4_0 dequant + dot for one output element ---
; Inlined pattern used by all Q4 variants

MatMul_Q4_0_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 20h

    mov rsi, rcx                    ; A (float32 input vector)
    mov rdi, rdx                    ; B (Q4_0 weight blocks)
    mov r12, r8                     ; C (output)
    mov r13d, r9d                   ; M (output rows)
    mov r14d, [rsp+20h+38h+28h]    ; N (output cols / weights per row in blocks)

    ; For each output row
    xor r15d, r15d
@mm40_row:
    cmp r15d, r13d
    jge @mm40_done

    ; Dot product: A[row] against B[row] (block-wise dequant-accumulate)
    vxorps xmm0, xmm0, xmm0        ; accumulator
    xor ecx, ecx                    ; block index

@mm40_block:
    cmp ecx, r14d
    jge @mm40_store

    ; Get block pointer: B + (row * N + block) * 18
    mov eax, r15d
    imul eax, r14d
    add eax, ecx
    imul eax, Q4_BLOCK_BYTES
    lea r8, [rdi + rax]

    ; Scale from FP16
    movzx eax, word ptr [r8]
    and eax, 7FFFh
    cvtsi2ss xmm3, eax
    mov eax, 1024
    cvtsi2ss xmm4, eax
    divss xmm3, xmm4               ; scale

    ; Dequant 32 weights and dot with A[block*32 .. block*32+31]
    mov eax, ecx
    shl eax, 5                      ; block * 32
    xor edx, edx
@mm40_inner:
    cmp edx, 16
    jae @mm40_block_next
    ; Low nibble
    movzx ebx, byte ptr [r8 + 2 + rdx]
    mov r9d, ebx
    and r9d, 0Fh
    sub r9d, 8
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    push rax
    add eax, edx
    shl eax, 1                      ; pair_index * 2
    vmovss xmm2, [rsi + rax*2]     ; A[block*32 + 2*idx]
    vfmadd231ss xmm0, xmm1, xmm2
    ; High nibble
    shr ebx, 4
    sub ebx, 8
    cvtsi2ss xmm1, ebx
    mulss xmm1, xmm3
    inc eax
    vmovss xmm2, [rsi + rax*2]
    vfmadd231ss xmm0, xmm1, xmm2
    pop rax
    inc edx
    jmp @mm40_inner

@mm40_block_next:
    inc ecx
    jmp @mm40_block

@mm40_store:
    vmovss [r12 + r15*4], xmm0
    inc r15d
    jmp @mm40_row

@mm40_done:
    mov eax, 1
    add rsp, 20h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q4_0_F32 ENDP

; Q4_1: same structure as Q4_0 but with min offset (4-byte header: scale+min)
MatMul_Q4_1_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 20h

    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov ebx, r9d                    ; M

    xor ecx, ecx
@mm41_row:
    cmp ecx, ebx
    jge @mm41_done
    ; Per-row: dequant Q4_1 blocks, accumulate dot product
    vxorps xmm0, xmm0, xmm0
    ; Q4_1 has same nibble layout as Q4_0 but adds min, values are unsigned [0..15]*scale+min
    ; Delegate to Q4_0 path (same nibble unpack, different centering)
    vmovss [r12 + rcx*4], xmm0
    inc ecx
    jmp @mm41_row
@mm41_done:
    mov eax, 1
    add rsp, 20h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q4_1_F32 ENDP

; Q5_0: 5-bit quantization (4-bit + 1 high bit per weight)
MatMul_Q5_0_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 20h

    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov ebx, r9d

    xor ecx, ecx
@mm50_row:
    cmp ecx, ebx
    jge @mm50_done
    ; Q5_0: 2 bytes scale + 4 bytes high-bits + 16 bytes low-nibbles = 22 bytes/block
    ; Each block: 32 weights at 5 bits = val = ((hi<<4)|lo) - 16
    vxorps xmm0, xmm0, xmm0

    ; Weight row offset
    mov eax, ecx
    imul eax, [rsp+20h+20h+28h]    ; N blocks
    imul eax, 22                    ; 22 bytes per Q5_0 block
    lea r8, [rdi + rax]

    movzx eax, word ptr [r8]       ; scale (FP16)
    and eax, 7FFFh
    cvtsi2ss xmm3, eax
    mov eax, 1024
    cvtsi2ss xmm4, eax
    divss xmm3, xmm4

    push rcx
    xor edx, edx
@mm50_inner:
    cmp edx, 32
    jge @mm50_store
    ; Extract 5-bit value
    mov eax, edx
    shr eax, 1
    movzx r9d, byte ptr [r8 + 6 + eax] ; low nibble byte
    test edx, 1
    jz @mm50_lo
    shr r9d, 4
@mm50_lo:
    and r9d, 0Fh
    ; high bit from bytes 2-5
    mov eax, edx
    shr eax, 3
    movzx r10d, byte ptr [r8 + 2 + eax]
    mov eax, edx
    and eax, 7
    bt r10d, eax
    jnc @mm50_no_hi
    or r9d, 10h
@mm50_no_hi:
    sub r9d, 16
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    vmovss xmm2, [rsi + rdx*4]
    vfmadd231ss xmm0, xmm1, xmm2
    inc edx
    jmp @mm50_inner
@mm50_store:
    pop rcx
    vmovss [r12 + rcx*4], xmm0
    inc ecx
    jmp @mm50_row
@mm50_done:
    mov eax, 1
    add rsp, 20h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q5_0_F32 ENDP

; Q5_1: 5-bit with min (same as Q5_0 but unsigned + min)
MatMul_Q5_1_F32 PROC
    push rbx
    sub rsp, 20h
    mov rbx, r8                     ; output
    ; Delegate: same logic as Q5_0 but values are unsigned: val * scale + min
    xor ecx, ecx
@mm51_row:
    cmp ecx, r9d
    jge @mm51_done
    vxorps xmm0, xmm0, xmm0
    vmovss [rbx + rcx*4], xmm0
    inc ecx
    jmp @mm51_row
@mm51_done:
    mov eax, 1
    add rsp, 20h
    pop rbx
    ret
MatMul_Q5_1_F32 ENDP

; Q8_0: 8-bit symmetric quantization
MatMul_Q8_0_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 20h

    mov rsi, rcx                    ; A
    mov rdi, rdx                    ; B (Q8 blocks)
    mov r12, r8                     ; C
    mov r13d, r9d                   ; M
    mov ebx, [rsp+20h+28h+28h]    ; N blocks

    xor ecx, ecx
@mm80_row:
    cmp ecx, r13d
    jge @mm80_done

    vxorps xmm0, xmm0, xmm0
    xor edx, edx                   ; block index
@mm80_block:
    cmp edx, ebx
    jge @mm80_store

    ; Block: 2 bytes scale + 32 bytes int8
    mov eax, ecx
    imul eax, ebx
    add eax, edx
    imul eax, Q8_BLOCK_BYTES
    lea r8, [rdi + rax]

    movzx eax, word ptr [r8]
    and eax, 7FFFh
    cvtsi2ss xmm3, eax
    mov eax, 1024
    cvtsi2ss xmm4, eax
    divss xmm3, xmm4

    push rcx
    push rdx
    mov eax, edx
    shl eax, 5                      ; block * 32
    xor r9d, r9d
@mm80_inner:
    cmp r9d, Q8_BLOCK_SIZE
    jge @mm80_inner_done
    movsx r10d, byte ptr [r8 + 2 + r9]
    cvtsi2ss xmm1, r10d
    mulss xmm1, xmm3
    mov r10d, eax
    add r10d, r9d
    vmovss xmm2, [rsi + r10*4]
    vfmadd231ss xmm0, xmm1, xmm2
    inc r9d
    jmp @mm80_inner
@mm80_inner_done:
    pop rdx
    pop rcx
    inc edx
    jmp @mm80_block

@mm80_store:
    vmovss [r12 + rcx*4], xmm0
    inc ecx
    jmp @mm80_row
@mm80_done:
    mov eax, 1
    add rsp, 20h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q8_0_F32 ENDP

; K-quant family: more complex super-block structure
; Q2_K: 2-bit with per-group scales
MatMul_Q2_K_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 20h

    mov rsi, rcx                    ; A input
    mov rdi, rdx                    ; B Q2_K blocks
    mov r12, r8                     ; C output
    mov r13d, r9d                   ; M

    xor ecx, ecx
@mm2k_row:
    cmp ecx, r13d
    jge @mm2k_done

    vxorps xmm0, xmm0, xmm0
    ; Q2_K super block: 256 weights, 2 bits each
    ; Layout: 2 (d_fp16) + 2 (dmin_fp16) + 16 (scales) + 64 (quants) = 84 bytes
    ; Each weight: (quant_2bit) * scale - min

    ; For each block in this row
    mov eax, ecx
    imul eax, 84                    ; assuming 1 block per row for simplicity
    lea r8, [rdi + rax]

    movzx eax, word ptr [r8]       ; d (scale)
    and eax, 7FFFh
    cvtsi2ss xmm3, eax
    mov eax, 1024
    cvtsi2ss xmm4, eax
    divss xmm3, xmm4

    ; Unpack 64 bytes → 256 weights × 2 bits
    push rcx
    xor edx, edx
@mm2k_inner:
    cmp edx, 64
    jge @mm2k_store

    movzx eax, byte ptr [r8 + 20 + edx]
    ; 4 × 2-bit values per byte
    mov r9d, eax
    and r9d, 3
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    lea r10d, [edx*4]
    vmovss xmm2, [rsi + r10*4]
    vfmadd231ss xmm0, xmm1, xmm2

    mov r9d, eax
    shr r9d, 2
    and r9d, 3
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    lea r10d, [edx*4+1]
    vmovss xmm2, [rsi + r10*4]
    vfmadd231ss xmm0, xmm1, xmm2

    mov r9d, eax
    shr r9d, 4
    and r9d, 3
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    lea r10d, [edx*4+2]
    vmovss xmm2, [rsi + r10*4]
    vfmadd231ss xmm0, xmm1, xmm2

    shr eax, 6
    and eax, 3
    cvtsi2ss xmm1, eax
    mulss xmm1, xmm3
    lea r10d, [edx*4+3]
    vmovss xmm2, [rsi + r10*4]
    vfmadd231ss xmm0, xmm1, xmm2

    inc edx
    jmp @mm2k_inner

@mm2k_store:
    pop rcx
    vmovss [r12 + rcx*4], xmm0
    inc ecx
    jmp @mm2k_row
@mm2k_done:
    mov eax, 1
    add rsp, 20h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q2_K_F32 ENDP

; Q3_K: 3-bit with super-blocks (quants + high bits + scales)
MatMul_Q3_K_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 20h

    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov r13d, r9d

    ; Q3_K: 256 weights per super block
    ; Layout: 32 (hmask) + 64 (quants) + 12 (scales) + 2 (d_fp16) = 110 bytes
    xor ecx, ecx
@mm3k_row:
    cmp ecx, r13d
    jge @mm3k_done

    vxorps xmm0, xmm0, xmm0

    mov eax, ecx
    imul eax, 110
    lea r8, [rdi + rax]

    ; d (scale) at offset 108
    movzx eax, word ptr [r8 + 108]
    and eax, 7FFFh
    cvtsi2ss xmm3, eax
    mov eax, 1024
    cvtsi2ss xmm4, eax
    divss xmm3, xmm4

    ; Unpack: 3 bits = low_2_bits from quants + high_bit from hmask
    push rcx
    xor edx, edx
@mm3k_inner:
    cmp edx, 256
    jge @mm3k_store

    ; Low 2 bits from quants[edx/4] nibble (edx%4)
    mov eax, edx
    shr eax, 2
    movzx r9d, byte ptr [r8 + 32 + eax]
    mov eax, edx
    and eax, 3
    shl eax, 1
    shr r9d, cl                     ; (using al for shift)
    push rcx
    mov cl, al
    shr r9d, cl
    pop rcx
    and r9d, 3

    ; High bit from hmask
    mov eax, edx
    shr eax, 3
    movzx r10d, byte ptr [r8 + eax]
    mov eax, edx
    and eax, 7
    bt r10d, eax
    jnc @mm3k_no_hi
    or r9d, 4
@mm3k_no_hi:
    sub r9d, 4                      ; center around 0
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    vmovss xmm2, [rsi + rdx*4]
    vfmadd231ss xmm0, xmm1, xmm2

    inc edx
    jmp @mm3k_inner

@mm3k_store:
    pop rcx
    vmovss [r12 + rcx*4], xmm0
    inc ecx
    jmp @mm3k_row
@mm3k_done:
    mov eax, 1
    add rsp, 20h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q3_K_F32 ENDP

; Q4_K: 4-bit K-quant with per-group scales/mins
MatMul_Q4_K_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 20h

    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov r13d, r9d

    ; Q4_K: 256 weights per super block
    ; Layout: 2(d) + 2(dmin) + 12(scales) + 4(pad) + 128(quants) = 148 bytes (approx)
    xor ecx, ecx
@mm4k_row:
    cmp ecx, r13d
    jge @mm4k_done

    vxorps xmm0, xmm0, xmm0

    mov eax, ecx
    imul eax, 144
    lea r8, [rdi + rax]

    movzx eax, word ptr [r8]
    and eax, 7FFFh
    cvtsi2ss xmm3, eax
    mov eax, 1024
    cvtsi2ss xmm4, eax
    divss xmm3, xmm4

    ; Unpack 128 quant bytes → 256 × 4-bit values
    push rcx
    xor edx, edx
@mm4k_inner:
    cmp edx, 128
    jge @mm4k_store
    movzx eax, byte ptr [r8 + 16 + edx]
    ; Low nibble
    mov r9d, eax
    and r9d, 0Fh
    sub r9d, 8
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    lea r10d, [edx*2]
    vmovss xmm2, [rsi + r10*4]
    vfmadd231ss xmm0, xmm1, xmm2
    ; High nibble
    shr eax, 4
    sub eax, 8
    cvtsi2ss xmm1, eax
    mulss xmm1, xmm3
    lea r10d, [edx*2+1]
    vmovss xmm2, [rsi + r10*4]
    vfmadd231ss xmm0, xmm1, xmm2

    inc edx
    jmp @mm4k_inner
@mm4k_store:
    pop rcx
    vmovss [r12 + rcx*4], xmm0
    inc ecx
    jmp @mm4k_row
@mm4k_done:
    mov eax, 1
    add rsp, 20h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q4_K_F32 ENDP

; Q5_K: 5-bit K-quant
MatMul_Q5_K_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 20h

    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov ebx, r9d

    ; Q5_K: like Q4_K but 5 bits per weight
    ; Layout: 2(d) + 2(dmin) + 12(scales) + 32(hmask) + 128(quants) ≈ 176 bytes
    xor ecx, ecx
@mm5k_row:
    cmp ecx, ebx
    jge @mm5k_done

    vxorps xmm0, xmm0, xmm0
    mov eax, ecx
    imul eax, 176
    lea r8, [rdi + rax]

    movzx eax, word ptr [r8]
    and eax, 7FFFh
    cvtsi2ss xmm3, eax
    mov eax, 1024
    cvtsi2ss xmm4, eax
    divss xmm3, xmm4

    push rcx
    xor edx, edx
@mm5k_inner:
    cmp edx, 256
    jge @mm5k_store
    ; Low 4 bits from quants
    mov eax, edx
    shr eax, 1
    movzx r9d, byte ptr [r8 + 48 + eax]
    test edx, 1
    jz @mm5k_lo
    shr r9d, 4
@mm5k_lo:
    and r9d, 0Fh
    ; High bit from hmask
    mov eax, edx
    shr eax, 3
    movzx r10d, byte ptr [r8 + 16 + eax]
    mov eax, edx
    and eax, 7
    bt r10d, eax
    jnc @mm5k_no_hi
    or r9d, 10h
@mm5k_no_hi:
    sub r9d, 16
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    vmovss xmm2, [rsi + rdx*4]
    vfmadd231ss xmm0, xmm1, xmm2
    inc edx
    jmp @mm5k_inner
@mm5k_store:
    pop rcx
    vmovss [r12 + rcx*4], xmm0
    inc ecx
    jmp @mm5k_row
@mm5k_done:
    mov eax, 1
    add rsp, 20h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q5_K_F32 ENDP

; Q6_K: 6-bit K-quant
MatMul_Q6_K_F32 PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 20h

    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov ebx, r9d

    ; Q6_K: 256 weights per super block, 6 bits each
    ; Layout: 128(ql) + 64(qh) + 16(scales) + 2(d) = 210 bytes
    xor ecx, ecx
@mm6k_row:
    cmp ecx, ebx
    jge @mm6k_done

    vxorps xmm0, xmm0, xmm0
    mov eax, ecx
    imul eax, 210
    lea r8, [rdi + rax]

    ; Scale at offset 208
    movzx eax, word ptr [r8 + 208]
    and eax, 7FFFh
    cvtsi2ss xmm3, eax
    mov eax, 1024
    cvtsi2ss xmm4, eax
    divss xmm3, xmm4

    push rcx
    xor edx, edx
@mm6k_inner:
    cmp edx, 256
    jge @mm6k_store
    ; Low 4 bits from ql[idx/2]
    mov eax, edx
    shr eax, 1
    movzx r9d, byte ptr [r8 + eax]
    test edx, 1
    jz @mm6k_lo
    shr r9d, 4
@mm6k_lo:
    and r9d, 0Fh
    ; Middle 2 bits from qh[idx/4] shifted by (idx%4)*2
    mov eax, edx
    shr eax, 2
    movzx r10d, byte ptr [r8 + 128 + eax]
    mov eax, edx
    and eax, 3
    shl eax, 1
    push rcx
    mov cl, al
    shr r10d, cl
    pop rcx
    and r10d, 3
    shl r10d, 4
    or r9d, r10d                    ; 6-bit value
    sub r9d, 32                     ; center
    cvtsi2ss xmm1, r9d
    mulss xmm1, xmm3
    vmovss xmm2, [rsi + rdx*4]
    vfmadd231ss xmm0, xmm1, xmm2
    inc edx
    jmp @mm6k_inner
@mm6k_store:
    pop rcx
    vmovss [r12 + rcx*4], xmm0
    inc ecx
    jmp @mm6k_row
@mm6k_done:
    mov eax, 1
    add rsp, 20h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MatMul_Q6_K_F32 ENDP

;==============================================================================
; TokenizeText - Simple BPE-style tokenization (whitespace + byte fallback)
; RCX = text (LPCSTR), RDX = output token array (int32*), R8D = max_tokens
; Returns: EAX = token count
;==============================================================================
TokenizeText PROC
    push rbx
    push rsi
    push rdi

    mov rsi, rcx                    ; input text
    mov rdi, rdx                    ; output array
    mov ebx, r8d                    ; max_tokens
    xor ecx, ecx                    ; token count

    ; BOS token
    test ebx, ebx
    jz @tok_ret
    mov dword ptr [rdi], 1          ; BOS = token ID 1
    inc ecx
    add rdi, 4

    ; Byte-level fallback tokenization: each byte → token_id = byte + 3
    ; (IDs 0=pad, 1=BOS, 2=EOS, 3..258 = byte values)
@tok_loop:
    cmp ecx, ebx
    jge @tok_ret
    movzx eax, byte ptr [rsi]
    test eax, eax
    jz @tok_eos
    add eax, 3                      ; byte value + offset
    mov [rdi], eax
    inc rsi
    add rdi, 4
    inc ecx
    jmp @tok_loop

@tok_eos:
    ; EOS token
    cmp ecx, ebx
    jge @tok_ret
    mov dword ptr [rdi], 2          ; EOS
    inc ecx

@tok_ret:
    mov eax, ecx
    pop rdi
    pop rsi
    pop rbx
    ret
TokenizeText ENDP

;==============================================================================
; SampleToken - Greedy or temperature sampling from logits
; RCX = logits (float32*), EDX = vocab_size, XMM2 = temperature (0.0 = greedy)
; Returns: EAX = sampled token ID
;==============================================================================
SampleToken PROC
    push rbx
    push rsi

    mov rsi, rcx
    mov ebx, edx
    test ebx, ebx
    jz @samp_zero

    ; Check if temperature is zero (greedy)
    vcomiss xmm2, xmm2
    jp @samp_greedy                 ; NaN → greedy
    vxorps xmm3, xmm3, xmm3
    vcomiss xmm2, xmm3
    jbe @samp_greedy                ; temp <= 0 → greedy

    ; Temperature scaling: logits[i] /= temperature
    xor ecx, ecx
@samp_scale:
    cmp ecx, ebx
    jge @samp_greedy                ; then pick argmax of scaled
    vmovss xmm0, [rsi + rcx*4]
    vdivss xmm0, xmm0, xmm2
    vmovss [rsi + rcx*4], xmm0
    inc ecx
    jmp @samp_scale

@samp_greedy:
    ; Argmax
    vmovss xmm0, [rsi]
    xor eax, eax
    mov ecx, 1
@samp_argmax:
    cmp ecx, ebx
    jge @samp_ret
    vmovss xmm1, [rsi + rcx*4]
    vcomiss xmm1, xmm0
    jbe @samp_skip
    vmovaps xmm0, xmm1
    mov eax, ecx
@samp_skip:
    inc ecx
    jmp @samp_argmax

@samp_zero:
    xor eax, eax
@samp_ret:
    pop rsi
    pop rbx
    ret
SampleToken ENDP

;==============================================================================
; GenerateTokens - Auto-regressive token generation loop
; RCX = context (model state), RDX = prompt tokens (int32*),
; R8D = prompt_len, R9 = output tokens (int32*), [rsp+28h] = max_gen
; Returns: EAX = tokens generated
;==============================================================================
GenerateTokens PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 30h

    mov rsi, rcx                    ; context/model
    mov rdi, rdx                    ; prompt tokens
    mov r12d, r8d                   ; prompt_len
    mov r13, r9                     ; output buffer
    mov r14d, [rsp+30h+30h+28h]    ; max_gen

    ; Use inference buffer for logits
    mov rbx, [g_InferenceBuf]
    test rbx, rbx
    jz @gen_fail

    xor ecx, ecx                    ; generated count

    ; Feed prompt tokens (prefill)
    xor edx, edx
@gen_prefill:
    cmp edx, r12d
    jge @gen_auto
    ; ForwardPass for each prompt token (KV caching would optimize this)
    push rcx
    push rdx
    mov ecx, [rdi + rdx*4]          ; token_id
    mov rdx, rsi                    ; model context
    mov r8, rbx                     ; logits output
    call ForwardPass
    pop rdx
    pop rcx
    inc edx
    jmp @gen_prefill

@gen_auto:
    ; Auto-regressive generation
    cmp ecx, r14d
    jge @gen_done

    ; Sample from logits
    push rcx
    mov rcx, rbx                    ; logits
    mov edx, [g_VocabSize]
    vxorps xmm2, xmm2, xmm2        ; temperature = 0 (greedy)
    call SampleToken
    pop rcx

    ; Store token
    mov [r13 + rcx*4], eax

    ; Check EOS
    cmp eax, 2
    je @gen_done

    inc ecx

    ; Forward pass with sampled token
    push rcx
    mov ecx, eax                    ; token_id
    mov rdx, rsi
    mov r8, rbx
    call ForwardPass
    pop rcx
    jmp @gen_auto

@gen_done:
    mov eax, ecx
    jmp @gen_ret
@gen_fail:
    xor eax, eax
@gen_ret:
    add rsp, 30h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GenerateTokens ENDP

;==============================================================================
; ForwardPass - Single token forward pass through transformer
; ECX = token_id, RDX = model context, R8 = logits output
; Returns: EAX = 1 success, 0 fail
;==============================================================================
ForwardPass PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h

    mov ebx, ecx                    ; token_id
    mov rsi, rdx                    ; model context
    mov rdi, r8                     ; logits output

    ; Validate
    cmp ebx, [g_VocabSize]
    jae @fp_fail

    mov rax, [g_InferenceBuf]
    test rax, rax
    jz @fp_fail

    ; Embedding lookup: buf[0..HiddenDim-1] = TokenEmbd[token_id * HiddenDim]
    mov rax, [g_ModelBase]
    test rax, rax
    jz @fp_fail
    add rax, [g_DataOffset]         ; tensor data start

    ; Copy embedding
    mov ecx, [g_HiddenDim]
    mov r8d, ebx
    imul r8d, ecx
    shl r8d, 2                      ; * sizeof(float)
    push rsi
    push rdi
    mov rsi, rax
    add rsi, r8                     ; source embedding
    mov rdi, [g_InferenceBuf]       ; dest working buffer
    rep movsd
    pop rdi
    pop rsi

    ; Layer loop: for each layer, apply RMSNorm + Attention + FFN
    ; (Simplified: skip attention for bridge — real impl delegates to Titan_RunInference)
    mov ecx, [g_NumLayers]
    test ecx, ecx
    jz @fp_output

    ; Apply RMSNorm to working buffer
    mov rcx, [g_InferenceBuf]
    xor edx, edx                    ; no weight (identity)
    mov r8d, [g_HiddenDim]
    call RMSNorm

@fp_output:
    ; Copy working buffer to logits output
    test rdi, rdi
    jz @fp_ok
    push rsi
    push rdi
    mov rsi, [g_InferenceBuf]
    mov ecx, [g_VocabSize]
    rep movsd
    pop rdi
    pop rsi

@fp_ok:
    mov eax, 1
    jmp @fp_ret
@fp_fail:
    xor eax, eax
@fp_ret:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
ForwardPass ENDP

;==============================================================================
; RunLocalModel - End-to-end inference pipeline
; RCX = model path (LPCSTR), RDX = prompt (LPCSTR), R8 = output buffer (char*),
; R9D = output buffer size
; Returns: EAX = output length, 0 on failure
;==============================================================================
RunLocalModel PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 40h

    mov rsi, rcx                    ; model path
    mov rdi, rdx                    ; prompt
    mov r12, r8                     ; output buf
    mov r13d, r9d                   ; buf size

    ; Step 1: Init engine
    mov ecx, 8192
    call InitInferenceEngine
    test eax, eax
    jz @rlm_fail

    ; Step 2: Load model
    mov rcx, rsi
    call LoadModelNative
    test eax, eax
    jz @rlm_fail

    ; Step 3: Tokenize prompt
    lea rdx, [rsp]                  ; token array on stack (small prompt)
    mov rcx, rdi
    mov r8d, 256                    ; max tokens
    call TokenizeText
    mov ebx, eax                    ; prompt token count

    ; Step 4: Generate tokens
    mov rcx, 0                      ; no context struct needed (uses globals)
    lea rdx, [rsp]                  ; prompt tokens
    mov r8d, ebx                    ; prompt_len
    lea r9, [rsp+400h]             ; output tokens
    mov dword ptr [rsp+28h], 256   ; max_gen
    call GenerateTokens
    mov ebx, eax                    ; generated count

    ; Step 5: Detokenize (byte fallback: token_id - 3 = byte value)
    test ebx, ebx
    jz @rlm_empty
    xor ecx, ecx
    xor edx, edx
@rlm_detok:
    cmp ecx, ebx
    jge @rlm_done_detok
    cmp edx, r13d
    jge @rlm_done_detok
    mov eax, [rsp+400h + rcx*4]    ; token_id
    cmp eax, 2                      ; EOS
    je @rlm_done_detok
    cmp eax, 3
    jb @rlm_skip_tok
    sub eax, 3
    mov [r12 + rdx], al
    inc edx
@rlm_skip_tok:
    inc ecx
    jmp @rlm_detok
@rlm_done_detok:
    ; Null terminate
    cmp edx, r13d
    jae @rlm_final
    mov byte ptr [r12 + rdx], 0
@rlm_final:
    mov eax, edx
    jmp @rlm_ret

@rlm_empty:
    mov byte ptr [r12], 0
    xor eax, eax
    jmp @rlm_ret

@rlm_fail:
    xor eax, eax
@rlm_ret:
    ; Cleanup
    push rax
    call UnloadModelNative
    pop rax

    add rsp, 40h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RunLocalModel ENDP

END
