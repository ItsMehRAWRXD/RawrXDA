;==============================================================================
; RawrXD GGUF Inference Engine - x64 Assembly
;==============================================================================

OPTION CASEMAP:NONE

;==============================================================================
; Constants
;==============================================================================

GGUF_MAGIC equ 0x46554747
GGUF_VERSION equ 3

;==============================================================================
; External Functions
;==============================================================================

EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSizeEx:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memcpy:PROC
EXTERN TlsAlloc:PROC
EXTERN TlsFree:PROC

;==============================================================================
; Globals
;==============================================================================

.DATA?

gTlsIndex DWORD ?
gModelCache QWORD ?
gRopeTableSin QWORD ?
gRopeTableCos QWORD ?
gExpTable QWORD ?
gLogTable QWORD ?
gTempBuffer QWORD ?

;==============================================================================
; DllMain Entry Point
;==============================================================================

.CODE

DllMain PROC hInstance:QWORD, dwReason:QWORD, lpReserved:QWORD

    mov rax, rdx
    mov ecx, dword ptr [rax]
    
    cmp rcx, 1
    je @@attach
    cmp rcx, 0
    je @@detach
    mov eax, 1
    ret
    
@@attach:
    mov eax, 1
    ret
    
@@detach:
    mov eax, 1
    ret

DllMain ENDP

;==============================================================================
; Load GGUF Model File
;==============================================================================

LoadModelNative PROC

    mov rbx, rcx
    mov r12, rdx
    
    ; Open file: CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
    mov rcx, r12
    mov edx, 80000000h
    mov r8d, 1
    xor r9, r9
    call CreateFileA
    
    cmp rax, -1
    je @@load_err
    mov r13, rax
    
    ; Get file size
    mov rcx, r13
    lea rdx, [rsp]
    call GetFileSizeEx
    test eax, eax
    jz @@load_err
    
    ; Create memory mapping
    mov rcx, r13
    call CreateFileMappingA
    
    test rax, rax
    jz @@load_err
    mov r15, rax
    
    ; Map the file view
    mov rcx, r15
    call MapViewOfFile
    
    test rax, rax
    jz @@load_err
    mov rsi, rax
    
    ; Validate header
    mov eax, DWORD PTR [rsi]
    cmp eax, 2175401975
    jne @@load_err
    
    ; Store context
    mov [rbx], r13
    mov [rbx + 8], r15
    mov [rbx + 16], rsi
    
    mov eax, 1
    ret
    
@@load_err:
    xor eax, eax
    ret

LoadModelNative ENDP

;==============================================================================
; GetTokenEmbedding - Look up token embedding from weight table
; RCX = pEmbedTable (FP16 weights), RDX = tokenId, R8 = pOutput, R9 = embedDim
;==============================================================================

GetTokenEmbedding PROC

    ; Calculate offset: tokenId * embedDim * 2 (FP16 = 2 bytes)
    mov rax, rdx
    imul rax, r9
    shl rax, 1
    add rcx, rax
    
    ; Copy embedDim * 4 bytes (FP32 output) - simple FP16->FP32 expand
    mov r10, r9
    
@@emb_loop:
    movzx eax, word ptr [rcx]
    shl eax, 16
    mov [r8], eax
    add rcx, 2
    add r8, 4
    dec r10
    jnz @@emb_loop
    
    mov eax, 1
    ret

GetTokenEmbedding ENDP

;==============================================================================
; ApplyRoPE - Apply rotary position embeddings to Q/K vectors
; RCX = pVec (FP32), RDX = position, R8 = headDim, R9 = ropeTheta
;==============================================================================

ApplyRoPE PROC

    ; For each pair (i, i+1) in head dimension
    mov r10, r8
    shr r10, 1
    xor r11, r11
    
@@rope_loop:
    ; Load pair
    movss xmm0, dword ptr [rcx]
    add rcx, 4
    movss xmm1, dword ptr [rcx]
    sub rcx, 4
    
    ; Apply rotation (simplified swap)
    movaps xmm2, xmm0
    subss xmm0, xmm1
    addss xmm1, xmm2
    
    ; Store back
    movss dword ptr [rcx], xmm0
    add rcx, 4
    movss dword ptr [rcx], xmm1
    add rcx, 4
    
    inc r11
    cmp r11, r10
    jb @@rope_loop
    
    mov eax, 1
    ret

ApplyRoPE ENDP

;==============================================================================
; ComputeQKV - Project input to Query, Key, Value
; RCX = pInput (FP32), RDX = pWq, R8 = pWk, R9 = pWv
; [rsp+28h] = pQ, [rsp+30h] = pK, [rsp+38h] = pV, [rsp+40h] = dim
;==============================================================================

ComputeQKV PROC

    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx
    mov rsi, [rsp + 28h + 24]
    mov rdi, [rsp + 30h + 24]
    mov r10, [rsp + 38h + 24]
    mov r11, [rsp + 40h + 24]
    
    ; Q = Input @ Wq (simplified: copy input to Q for now)
    mov rcx, rsi
    mov rdx, rbx
    mov r8, r11
    shl r8, 2
    call memcpy
    
    ; K = Input @ Wk
    mov rcx, rdi
    mov rdx, rbx
    mov r8, r11
    shl r8, 2
    call memcpy
    
    ; V = Input @ Wv
    mov rcx, r10
    mov rdx, rbx
    mov r8, r11
    shl r8, 2
    call memcpy
    
    pop rdi
    pop rsi
    pop rbx
    mov eax, 1
    ret

ComputeQKV ENDP

;==============================================================================
; ComputeAttention - Compute attention scores and weighted sum
; RCX = pQ, RDX = pK, R8 = pV, R9 = pOut
; [rsp+28h] = seqLen, [rsp+30h] = headDim
;==============================================================================

ComputeAttention PROC

    push rbx
    push rsi
    
    mov rbx, r9
    mov rsi, [rsp + 28h + 16]
    mov r10, [rsp + 30h + 16]
    
    ; Simplified: copy V to output (placeholder for full attention)
    mov rcx, rbx
    mov rdx, r8
    mov r8, r10
    shl r8, 2
    call memcpy
    
    pop rsi
    pop rbx
    mov eax, 1
    ret

ComputeAttention ENDP

;==============================================================================
; FeedForward_SwiGLU - Feed-forward network with SwiGLU activation
; RCX = pInput, RDX = pWgate, R8 = pWup, R9 = pWdown
; [rsp+28h] = pOutput, [rsp+30h] = dim, [rsp+38h] = ffDim
;==============================================================================

FeedForward_SwiGLU PROC

    push rbx
    push rsi
    
    mov rbx, rcx
    mov rsi, [rsp + 28h + 16]
    mov r10, [rsp + 30h + 16]
    
    ; Simplified: copy input to output (placeholder for full FFN)
    mov rcx, rsi
    mov rdx, rbx
    mov r8, r10
    shl r8, 2
    call memcpy
    
    pop rsi
    pop rbx
    mov eax, 1
    ret

FeedForward_SwiGLU ENDP

;==============================================================================
; RMSNorm - Root Mean Square Layer Normalization
; RCX = pInput (FP32), RDX = pWeight (FP32), R8 = pOutput (FP32), R9 = dim
;==============================================================================

RMSNorm PROC

    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, r9
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    
    ; Compute sum of squares
    xorps xmm0, xmm0
    mov r10, r12
    mov r11, rbx
    
@@rms_sum:
    movss xmm1, dword ptr [r10]
    mulss xmm1, xmm1
    addss xmm0, xmm1
    add r10, 4
    dec r11
    jnz @@rms_sum
    
    ; Compute RMS = sqrt(sum/dim)
    cvtsi2ss xmm1, ebx
    divss xmm0, xmm1
    sqrtss xmm0, xmm0
    
    ; Add epsilon (1e-6) and compute 1/rms
    mov eax, 358637BDh
    movd xmm1, eax
    addss xmm0, xmm1
    mov eax, 3F800000h
    movd xmm2, eax
    divss xmm2, xmm0
    
    ; Normalize: output[i] = input[i] * weight[i] / rms
    mov r10, rbx
    mov rcx, r12
    mov rdx, r13
    mov r8, r14
    
@@rms_norm:
    movss xmm0, dword ptr [rcx]
    mulss xmm0, xmm2
    movss xmm1, dword ptr [rdx]
    mulss xmm0, xmm1
    movss dword ptr [r8], xmm0
    add rcx, 4
    add rdx, 4
    add r8, 4
    dec r10
    jnz @@rms_norm
    
    pop r14
    pop r13
    pop r12
    pop rbx
    mov eax, 1
    ret

RMSNorm ENDP

;==============================================================================
; ForwardPass - Main inference loop through all transformer layers
; RCX = pCtx (model context), RDX = pTokenIds, R8 = numTokens, R9 = pOutput
;==============================================================================

ForwardPass PROC

    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r12, r9
    
    ; Get model base pointer from context
    mov r13, [rbx + 16]
    
    ; Process each token
    mov r10, rdi
    
@@token_loop:
    ; Load token ID
    mov eax, [rsi]
    
    ; Store token ID as output (placeholder)
    mov [r12], eax
    
    add rsi, 4
    add r12, 4
    dec r10
    jnz @@token_loop
    
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov eax, 1
    ret

ForwardPass ENDP

InitMathTables PROC

    ; Allocate sin table: 128 MB
    mov rcx, 134217728
    call malloc
    test rax, rax
    jz @@init_err
    mov [gRopeTableSin], rax
    
    ; Allocate cos table: 128 MB
    mov rcx, 134217728
    call malloc
    test rax, rax
    jz @@init_err
    mov [gRopeTableCos], rax
    
    ; Allocate exp table: 32 KB
    mov rcx, 32768
    call malloc
    test rax, rax
    jz @@init_err
    mov [gExpTable], rax
    
    ; Allocate log table: 32 KB
    mov rcx, 32768
    call malloc
    test rax, rax
    jz @@init_err
    mov [gLogTable], rax
    
    ; Allocate temp buffer: 64 MB
    mov rcx, 67108864
    call malloc
    test rax, rax
    jz @@init_err
    mov [gTempBuffer], rax
    
    mov eax, 1
    ret
    
@@init_err:
    xor eax, eax
    ret

InitMathTables ENDP

CleanupMathTables PROC

    ; Free sin table
    mov rax, [gRopeTableSin]
    test rax, rax
    jz @@skip_sin
    mov rcx, rax
    call free
    mov qword ptr [gRopeTableSin], 0
    
@@skip_sin:
    ; Free cos table
    mov rax, [gRopeTableCos]
    test rax, rax
    jz @@skip_cos
    mov rcx, rax
    call free
    mov qword ptr [gRopeTableCos], 0
    
@@skip_cos:
    ; Free exp table
    mov rax, [gExpTable]
    test rax, rax
    jz @@skip_exp
    mov rcx, rax
    call free
    mov qword ptr [gExpTable], 0
    
@@skip_exp:
    ; Free log table
    mov rax, [gLogTable]
    test rax, rax
    jz @@skip_log
    mov rcx, rax
    call free
    mov qword ptr [gLogTable], 0
    
@@skip_log:
    ; Free temp buffer
    mov rax, [gTempBuffer]
    test rax, rax
    jz @@skip_temp
    mov rcx, rax
    call free
    mov qword ptr [gTempBuffer], 0
    
@@skip_temp:
    mov eax, 1
    ret

CleanupMathTables ENDP

END
