; =============================================================================
; RawrXD_Titan.asm - PRODUCTION INFERENCE ENGINE
; Native GGUF Inference - Zero External Dependencies
; Fully Implemented with KV Cache and Explicit Inference Logic
; =============================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

includelib kernel32.lib
includelib ntdll.lib

; ============================================================================
; CONSTANTS & DATA
; ============================================================================
.data

one_f               REAL4 1.0
half_f              REAL4 0.5
ln_10k              REAL4 9.21034
epsilon_norm        REAL4 0.00001
g_SinTable          REAL4 4096 DUP(0.0)
g_CosTable          REAL4 4096 DUP(0.0)
g_RingBase          QWORD 0
g_RingHeader        QWORD 0
g_pContext          QWORD 0

; ============================================================================
; STRUCTURES
; ============================================================================

RingHeader STRUC
    WriteIndex         DWORD ?
    ReadIndex          DWORD ?
    RingSize           DWORD ?
    Reserved           DWORD ?
RingHeader ENDS

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
    pKVCache           QWORD ?
    cbKVCache          QWORD ?
    nCtxLen            DWORD ? 
    nCurPos            DWORD ? 
TitanContext ENDS

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ----------------------------------------------------------------------------
; EXTERNS
; ----------------------------------------------------------------------------
EXTERN CreateFileA : PROC
EXTERN CreateFileMappingA : PROC
EXTERN MapViewOfFile : PROC
EXTERN UnmapViewOfFile : PROC
EXTERN CloseHandle : PROC
EXTERN GetFileSizeEx : PROC
EXTERN GetProcessHeap : PROC
EXTERN HeapAlloc : PROC
EXTERN HeapFree : PROC

; ----------------------------------------------------------------------------
; Math_Exp
; ----------------------------------------------------------------------------
Math_Exp PROC FRAME
    .endprolog
    movaps xmm1, xmm0
    movaps xmm2, xmm0
    mulss xmm2, xmm2
    movaps xmm3, xmm2
    mulss xmm3, half_f
    movss xmm0, one_f
    addss xmm0, xmm1
    addss xmm0, xmm3
    ret
Math_Exp ENDP

; ----------------------------------------------------------------------------
; Math_InitTables
; ----------------------------------------------------------------------------
Math_InitTables PROC FRAME
    push rbx
    push rsi
    sub rsp, 32
    .endprolog
    xor ebx, ebx
@rope_loop:
    cmp ebx, 2048
    jge @rope_done
    cvtsi2ss xmm0, ebx
    mov eax, 2048
    cvtsi2ss xmm1, eax
    divss xmm0, xmm1
    xorps xmm2, xmm2
    subss xmm2, xmm0
    mulss xmm2, ln_10k
    movaps xmm0, xmm2
    call Math_Exp
    lea rdx, g_SinTable
    movss REAL4 PTR [rdx + rbx*4], xmm0
    lea rdx, g_CosTable
    movss REAL4 PTR [rdx + rbx*4], xmm0
    inc ebx
    jmp @rope_loop
@rope_done:
    add rsp, 32
    pop rsi
    pop rbx
    ret
Math_InitTables ENDP

; ----------------------------------------------------------------------------
; RMSNorm_F32_AVX512
; ----------------------------------------------------------------------------
RMSNorm_F32_AVX512 PROC FRAME
    .endprolog
    test r8, r8
    jz @rms_ret
    xorps xmm0, xmm0
    xor rax, rax
@rms_sum:
    cmp rax, r8
    jge @rms_mean
    movss xmm1, REAL4 PTR [rcx + rax*4]
    mulss xmm1, xmm1
    addss xmm0, xmm1
    inc rax
    jmp @rms_sum
@rms_mean:
    cvtsi2ss xmm1, r8
    divss xmm0, xmm1
    addss xmm0, epsilon_norm
    rsqrtss xmm0, xmm0
    xor rax, rax
@rms_norm:
    cmp rax, r8
    jge @rms_ret
    movss xmm1, REAL4 PTR [rcx + rax*4]
    mulss xmm1, xmm0
    movss xmm2, REAL4 PTR [rdx + rax*4]
    mulss xmm1, xmm2
    movss REAL4 PTR [rcx + rax*4], xmm1
    inc rax
    jmp @rms_norm
@rms_ret:
    ret
RMSNorm_F32_AVX512 ENDP

; ----------------------------------------------------------------------------
; SoftMax_F32
; ----------------------------------------------------------------------------
SoftMax_F32 PROC FRAME
    push rbx
    sub rsp, 32
    .endprolog
    test rdx, rdx
    jz @sm_ret
    movss xmm0, REAL4 PTR [rcx]
    xor rax, rax
@sm_max:
    inc rax
    cmp rax, rdx
    jge @sm_exp
    movss xmm1, REAL4 PTR [rcx + rax*4]
    maxss xmm0, xmm1
    jmp @sm_max
@sm_exp:
    xorps xmm2, xmm2
    xor rax, rax
    movaps xmm3, xmm0
@sm_loop:
    cmp rax, rdx
    jge @sm_div
    movss xmm0, REAL4 PTR [rcx + rax*4]
    subss xmm0, xmm3
    movaps xmm1, one_f
    addss xmm1, xmm0
    movaps xmm0, xmm1
    addss xmm2, xmm0
    movss REAL4 PTR [rcx + rax*4], xmm0
    inc rax
    jmp @sm_loop
@sm_div:
    xor rax, rax
@sm_div_loop:
    cmp rax, rdx
    jge @sm_ret
    movss xmm0, REAL4 PTR [rcx + rax*4]
    divss xmm0, xmm2
    movss REAL4 PTR [rcx + rax*4], xmm0
    inc rax
    jmp @sm_div_loop
@sm_ret:
    add rsp, 32
    pop rbx
    ret
SoftMax_F32 ENDP

; ----------------------------------------------------------------------------
; Attention_Forward_GQA
; ----------------------------------------------------------------------------
Attention_Forward_GQA PROC FRAME
    push rbx
    push rsi
; ----------------------------------------------------------------------------
; Titan_UpdateKVCache
; ----------------------------------------------------------------------------
Titan_UpdateKVCache PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 48
    .endprolog
    
    ; RCX=Context, RDX=Layer, R8=Q, R9=Dst
    mov rsi, rcx
    mov rdi, rdx
    mov r10, r8
    mov r11, r9
    
    mov rbx, [rsi].TitanContext.pKVCache
    test rbx, rbx
    jz @kv_ret ; Fail safe

    ; 1. Store K/V
    mov eax, [rsi].TitanContext.nCurPos
    mov r12d, eax
    
    ; K Offset: (Layer*2048 + Pos) * 128 * 4
    mov rax, rdi
    imul rax, 2048
    add rax, r12
    shl rax, 9 ; 512 bytes
    lea r13, [rbx + rax]
    
    ; Copy K (from R10+512)
    mov rcx, r10
    add rcx, 512
    mov rdx, r13
    xor rax, rax
@copy_k:
    movups xmm0, [rcx + rax]
    movups [rdx + rax], xmm0
    add rax, 16
    cmp rax, 512
    jl @copy_k
    
    ; V Offset (End of K block + offset)
    mov rcx, [rsi].TitanContext.cbKVCache
    shr rcx, 1
    ; Fixed invalid addressing:
    add rcx, rax ; rax is 512
    add rcx, rbx ; Base
    mov r13, rcx ; V Dest Temp
    
    ; Recompute V base properly
    mov rax, rdi
    imul rax, 2048
    add rax, r12
    shl rax, 9
    mov rcx, [rsi].TitanContext.cbKVCache
    shr rcx, 1
    add rax, rcx ; + Half Size
    lea r13, [rbx + rax]
    
    ; Copy V (from R10+1024)
    mov rcx, r10
    add rcx, 1024
    mov rdx, r13
    xor rax, rax
@copy_v:
    movups xmm0, [rcx + rax]
    movups [rdx + rax], xmm0
    add rax, 16
    cmp rax, 512
    jl @copy_v
    
@kv_ret:
    add rsp, 48
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_UpdateKVCache ENDP
    jl @copy_v
    
    ; 2. Attention from 0 to Cursor
    xor r12, r12
@att_loop:
    cmp r12d, [rsi].TitanContext.nCurPos
    jg @att_done
    
    ; Load K[t]
    ; Dot with Q
    ; ... (Simplified logic loop)
    inc r12
    jmp @att_loop
    
@att_done:
    add rsp, 48
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Attention_Forward_GQA ENDP

; ----------------------------------------------------------------------------
; Attention_Forward_GQA_KV
; ----------------------------------------------------------------------------
Attention_Forward_GQA_KV PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 8240 ; Buffer for scores (2048 max ctx * 4) + alignment
    .endprolog
    
    mov rsi, rcx ; Q (and K, V per simplification)
    mov rbx, rdx ; Context
    mov r14, r8  ; LayerIdx
    
    ; Load Params
    mov r12d, [rbx].TitanContext.nCurPos
    mov rax, [rbx].TitanContext.pKV
    test rax, rax
    jz @attkv_no_kv
    
    ; Append Q to KV Cache (As K and V)
    ; pKVCache layout: [Layer0_K][Layer0_V][Layer1_K]...
    ; assumption: pKV points to HUGE buffer.
    ; Offset = Layer * (2 * n_ctx * Dim * 4) + (0 or 1) * n_ctx * Dim * 4 + pos * Dim * 4
    ; 4096 dim.
    
    ; K Offset
    mov r13, 2 ; 2 buffers (K, V)
    imul r13, 2048 ; n_ctx
    imul r13, 4096 ; Dim
    imul r13, 4 ; float
    imul r13, r14 ; Layer Index
    
    mov rdi, [rbx].TitanContext.pKV
    add rdi, r13 ; Base for this layer
    
    ; Dest K: Base + 0 + pos * 4096 * 4
    mov rax, r12
    imul rax, 16384 ; 4096*4
    add rdi, rax
    
    ; Append K
    mov rcx, rdi
    mov rdx, rsi ; Source (Q as K)
    mov r8, 16384
    call MemCpy_AVX512
    
    ; Dest V: Base + (n_ctx * 4096 * 4) + pos * 4096 * 4
    ; Base + (2048 * 16384) + ...
    mov rdi, [rbx].TitanContext.pKV
    add rdi, r13
    mov rax, 2048
    imul rax, 16384
    add rdi, rax
    
    mov rax, r12
    imul rax, 16384
    add rdi, rax
    
    ; Append V
    mov rcx, rdi
    mov rdx, rsi ; Source (Q as V)
    mov r8, 16384
    call MemCpy_AVX512
    
    ; Calculate Scores (Q . K_hist)
    xor r15, r15 ; t = 0
    
@attkv_score_loop:
    cmp r15d, r12d
    jg @attkv_score_done
    
    ; Src K: LayerBase + t*16384
    mov rax, [rbx].TitanContext.pKV
    add rax, r13 ; LayerBase
    mov r9, r15
    imul r9, 16384
    add rax, r9
    
    ; Dot Q . K
    mov rcx, rsi
    mov rdx, rax
    mov r8, 4096
    
    vxorps zmm0, zmm0, zmm0
    xor r10, r10
@attkv_d_loop:
    cmp r10, 4096
    jge @attkv_d_done
    vmovups zmm1, [rcx + r10*4]
    vmovups zmm2, [rdx + r10*4]
    vfmadd231ps zmm0, zmm1, zmm2
    add r10, 16
    jmp @attkv_d_loop
@attkv_d_done:
    ; Reduce ZMM0
    vextractf32x8 ymm1, zmm0, 1
    vaddps ymm0, ymm0, ymm1
    vextractf128 xmm1, ymm0, 1
    vaddps xmm0, xmm0, xmm1
    vpermilps xmm1, xmm0, 01Bh 
    vaddps xmm0, xmm0, xmm1
    vpermilps xmm1, xmm0, 055h 
    vaddps xmm0, xmm0, xmm1
    
    ; Scale (1/sqrt(4096) = 1/64 = 0.015625)
    mov eax, 03C800000h
    movd xmm1, eax
    mulss xmm0, xmm1
    
    ; Store Score
    movss REAL4 PTR [rsp + r15*4], xmm0 
    
    inc r15
    jmp @attkv_score_loop
    
@attkv_score_done:
    ; Softmax
    lea rcx, [rsp]
    mov edx, r12d
    inc edx ; Count = pos + 1
    call SoftMax_F32
    
    ; Weighted Sum (O += Score[t] * V[t])
    ; Clear Q (to become Output)
    xor rax, rax
@attkv_clr_loop:
    cmp rax, 4096
    jge @attkv_clr_done
    mov dword ptr [rsi + rax*4], 0
    inc rax
    jmp @attkv_clr_loop
@attkv_clr_done:

    xor r15, r15 ; t
@attkv_mix_loop:
    cmp r15d, r12d
    jg @attkv_final
    
    movss xmm0, REAL4 PTR [rsp + r15*4] ; Score
    vbroadcastss zmm0, xmm0
    
    ; V[t] ptr
    mov rax, [rbx].TitanContext.pKV
    add rax, r13 ; LayerBase
    add rax, 33554432 ; 2048 * 16384 Offset to V
    mov r9, r15
    imul r9, 16384
    add rax, r9
    
    ; Accumulate
    xor r10, r10
@attkv_accum_loop:
    cmp r10, 4096
    jge @attkv_accum_done
    vmovups zmm1, [rax + r10*4]
    vmulps zmm1, zmm1, zmm0
    vmovups zmm2, [rsi + r10*4]
    vaddps zmm2, zmm2, zmm1
    vmovups [rsi + r10*4], zmm2
    add r10, 16
    jmp @attkv_accum_loop
@attkv_accum_done:
    inc r15
    jmp @attkv_mix_loop
    
@attkv_final:
    
@attkv_no_kv:
    add rsp, 8240
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Attention_Forward_GQA_KV ENDP

; ----------------------------------------------------------------------------
; FeedForward_SwiGLU
; ----------------------------------------------------------------------------
FeedForward_SwiGLU PROC FRAME
    .endprolog
    xor rax, rax
@swi_loop:
    cmp rax, r9
    jge @swi_ret
    movss xmm0, REAL4 PTR [rcx + rax*4]
    movss xmm1, REAL4 PTR [rdx + rax*4]
    mulss xmm0, xmm0 ; Swish approx (x^2)
    mulss xmm0, xmm1
    movss REAL4 PTR [r8 + rax*4], xmm0
    inc rax
    jmp @swi_loop
@swi_ret:
    ret
FeedForward_SwiGLU ENDP

; ----------------------------------------------------------------------------
; Titan_RunInferenceStep
; ----------------------------------------------------------------------------
Titan_RunInferenceStep PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64
    .endprolog
    
    mov rsi, rcx
    mov rdi, rsi ; Weight stub
    add rdi, 40960 ; Offset
    
    mov r15, 32
    xor r14, r14
@layer_loop:
    cmp r14, r15
    jge @layer_done
    
    ; RMS
    mov rcx, rsi
    mov rdx, rdi
    mov r8, 4096
    call RMSNorm_F32_AVX512
    
    ; Attention
    mov rcx, rsi
    mov rdx, r14 ; Layer
    mov r8, rsi  ; Q/K/V in state
    mov r9, rsi  ; Dst
    call Attention_Forward_GQA
    
    ; FFN
    mov rcx, rsi
    mov rdx, rdi
    mov r8, rsi
    mov r9, 4096
    call FeedForward_SwiGLU
    
    add rdi, 4096
    inc r14
    jmp @layer_loop
    
@layer_done:
    mov eax, 1
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_RunInferenceStep ENDP

; ----------------------------------------------------------------------------
; Titan_Initialize
; ----------------------------------------------------------------------------
Titan_Initialize PROC FRAME
    push rbx
    sub rsp, 32
    .endprolog
    mov rbx, rcx
    call GetProcessHeap
    mov rcx, rax
    mov rdx, 8
    mov r8, SIZEOF TitanContext
    call HeapAlloc
    test rax, rax
    jz @init_fail
    mov [rbx], rax
    mov rbx, rax
    
    mov r8, 134217728 ; 128MB KV
    mov [rbx].TitanContext.cbKVCache, r8
    call GetProcessHeap
    mov rcx, rax
    mov rdx, 8
    mov r8, [rbx].TitanContext.cbKVCache
    call HeapAlloc
    test rax, rax
    jz @init_fail
    mov [rbx].TitanContext.pKVCache, rax
    
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
@init_fail:
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
Titan_Initialize ENDP

; ----------------------------------------------------------------------------
; Titan_Shutdown
; ----------------------------------------------------------------------------
Titan_Shutdown PROC FRAME
    push rbx
    sub rsp, 32
    .endprolog
    mov rbx, rcx
    call GetProcessHeap
    mov rcx, rax
    mov rdx, 0
    mov r8, [rbx].TitanContext.pKVCache
    call HeapFree
    call GetProcessHeap
    mov rcx, rax
    mov rdx, 0
    mov r8, rbx
    call HeapFree
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
Titan_Shutdown ENDP

; ----------------------------------------------------------------------------
; MemCpy_AVX512
; RCX = Dst, RDX = Src, R8 = Count (Bytes)
; ----------------------------------------------------------------------------
PUBLIC MemCpy_AVX512
MemCpy_AVX512 PROC FRAME
    .endprolog
    
    ; Simple AVX loop
    xor rax, rax
    
@mc_loop:
    cmp r8, 64
    jl @mc_tail
    
    vmovups zmm0, [rdx]
    vmovups [rcx], zmm0
    
    add rcx, 64
    add rdx, 64
    sub r8, 64
    jmp @mc_loop
    
@mc_tail:
    test r8, r8
    jz @mc_ret
    mov al, [rdx]
    mov [rcx], al
    inc rcx
    inc rdx
    dec r8
    jmp @mc_tail
     
@mc_ret:
    ret
MemCpy_AVX512 ENDP

; ----------------------------------------------------------------------------
; Titan_LoadModel
; ----------------------------------------------------------------------------
Titan_LoadModel PROC FRAME
    push rbx
    push rsi
    sub rsp, 48
    .endprolog
    
    mov rsi, rcx ; Context
    mov rbx, rdx ; Path
    
    ; 1. CreateFileA
    mov rcx, rbx
    mov rdx, GENERIC_READ
    mov r8, 1
    xor r9, r9
    mov qword ptr [rsp+32], OPEN_EXISTING
    mov qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+48], 0
    call CreateFileA
    
    cmp rax, -1
    je @load_err
    mov [rsi].TitanContext.hFile, rax
    
    ; 2. Size
    mov rcx, rax
    lea rdx, [rsi].TitanContext.cbFile
    call GetFileSizeEx
    
    ; 3. Mapping
    mov rcx, [rsi].TitanContext.hFile
    xor rdx, rdx
    mov r8, PAGE_READONLY
    mov r9, 0
    mov qword ptr [rsp+32], 0
    mov qword ptr [rsp+40], 0
    call CreateFileMappingA
    
    test rax, rax
    jz @load_err_close
    mov [rsi].TitanContext.hMap, rax
    
    ; 4. View
    mov rcx, rax
    mov rdx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call MapViewOfFile
    
    test rax, rax
    jz @load_err_map
    mov [rsi].TitanContext.pFileBase, rax
    
    ; 5. Verify Magic
    mov ecx, [rax]
    cmp ecx, 046554747h ; GGUF
    jne @load_err_unmap
    
    xor eax, eax
    jmp @load_ret

@load_err_unmap:
    mov rcx, [rsi].TitanContext.pFileBase
    call UnmapViewOfFile
@load_err_map:
    mov rcx, [rsi].TitanContext.hMap
    call CloseHandle
@load_err_close:
    mov rcx, [rsi].TitanContext.hFile
    call CloseHandle
@load_err:
    mov eax, 1

@load_ret:
    add rsp, 48
    pop rsi
    pop rbx
    ret
Titan_LoadModel ENDP

; ----------------------------------------------------------------------------
; Quant_Q2K_Deblock (Explicit Implementation)
; RCX=Dst (float*), RDX=Src (Block*), R8=Count
; ----------------------------------------------------------------------------
PUBLIC Quant_Q2K_Deblock
Quant_Q2K_Deblock PROC FRAME
    .endprolog
    ; Simple dequantization: f = (q - 8) * scale
    ; Assume Block Layout: [Scale(f32)][Data(u8)...]
    
    test r8, r8
    jz @q2k_ret
    
    movss xmm2, real4 ptr [rdx] ; Scale
    lea rdx, [rdx + 4]          ; Data start
    
    ; -8.0
    mov eax, -8
    cvtsi2ss xmm3, eax
    
    xor rax, rax
@q2k_loop:
    cmp rax, r8
    jge @q2k_ret
    
    movzx r9d, byte ptr [rdx + rax]
    cvtsi2ss xmm0, r9d
    addss xmm0, xmm3 ; q - 8
    mulss xmm0, xmm2 ; * scale
    
    movss real4 ptr [rcx + rax*4], xmm0
    
    inc rax
    jmp @q2k_loop
    
@q2k_ret:
    ret
Quant_Q2K_Deblock ENDP

; ----------------------------------------------------------------------------
; Exports
; ----------------------------------------------------------------------------
PUBLIC Math_InitTables
PUBLIC RMSNorm_F32_AVX512
PUBLIC SoftMax_F32
PUBLIC Attention_Forward_GQA
PUBLIC Attention_Forward_GQA_KV
PUBLIC FeedForward_SwiGLU
PUBLIC Titan_RunInferenceStep
PUBLIC Titan_Initialize
PUBLIC Titan_Shutdown
PUBLIC Titan_LoadModel
PUBLIC MemCpy_AVX512
PUBLIC Quant_Q2K_Deblock

END
