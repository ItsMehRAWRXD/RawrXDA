; RawrXD_DualEngineStreamer.asm
; Dual-engine loader: Engine0=FP32, Engine1=INT8/INT4 quantized
; Hot-patches microkernel calls during streaming

include masm_hotpatch.inc

EXTERN RawrXD_HotPatchEnginePath: PROC
EXTERN RawrXD_ProcessStreamingChunk: PROC
EXTERN RawrXD_PerformanceLogMetrics: PROC

.data
g_streamBuffer QWORD 0
g_streamBufferSize QWORD 16777216  ; 16MB chunks
g_hActiveEngine DWORD 0            ; 0=FP32, 1=Quantized
g_patchSite QWORD 0                ; Code path to patch
g_originalBytes BYTE 15 DUP(0)     ; Backup for rollback
g_hStreamingCounter QWORD 0
g_bytesRead DWORD 0

.const
ENGINE_FP32 EQU 0
ENGINE_QUANTIZED EQU 1
CHUNK_SIZE EQU 16777216

.code

RawrXD_DualEngineStreamInit PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Allocate streaming buffer
    mov rcx, g_streamBufferSize
    mov rdx, 4096                ; Page alignment
    call asm_malloc
    test rax, rax
    jz INIT_FAILED
    mov g_streamBuffer, rax
    
    ; Determine active engine from model header
    call RawrXD_DetectModelFormat
    mov g_hActiveEngine, eax
    
    ; Hot-patch dispatch table immediately
    call RawrXD_HotPatchEnginePath
    
    xor eax, eax                 ; Success
    jmp INIT_EXIT
    
INIT_FAILED:
    mov eax, -1
INIT_EXIT:
    add rsp, 32
    pop rbx
    ret
RawrXD_DualEngineStreamInit ENDP

RawrXD_DualEngineStreamChunk PROC FRAME
    push rbx
    push rbp
    .pushreg rbx
    .pushreg rbp
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx                 ; File handle
    mov rbp, rdx                 ; Bytes to read
    
    ; Start performance timing
    rdtscp
    shl rdx, 32
    or rax, rdx
    mov QWORD PTR [g_hStreamingCounter], rax
    
    ; Read chunk
    lea rcx, g_bytesRead
    mov rdx, g_streamBuffer
    mov r8, rbp
    xor r9, r9
    mov rax, rbx
    call ReadFile
    
    ; Process chunk while streaming
    call RawrXD_ProcessStreamingChunk
    
    ; Performance logging
    rdtscp
    shl rdx, 32
    or rax, rdx
    sub rax, QWORD PTR [g_hStreamingCounter]
    mov rdx, rax
    lea rcx, g_hStreamingCounter
    call RawrXD_PerformanceLogMetrics
    
    add rsp, 48
    pop rbp
    pop rbx
    ret
RawrXD_DualEngineStreamChunk ENDP

; Detect quantization format from GGUF metadata
RawrXD_DetectModelFormat PROC
    mov rax, g_streamBuffer
    mov eax, DWORD PTR [rax]     ; Read first 4 bytes
    cmp eax, 046554747h          ; "GGUF" magic (no 0x prefix in MASM)
    jne DEFAULT_FP32
    
    ; Check tensor types at offset 64
    mov rax, g_streamBuffer
    mov al, BYTE PTR [rax+64]
    cmp al, 2                    ; Q8_0 type
    je SET_QUANTIZED
    cmp al, 3                    ; Q4_0 type
    je SET_QUANTIZED
    
DEFAULT_FP32:
    mov eax, ENGINE_FP32
    ret
SET_QUANTIZED:
    mov eax, ENGINE_QUANTIZED
    ret
RawrXD_DetectModelFormat ENDP

END
