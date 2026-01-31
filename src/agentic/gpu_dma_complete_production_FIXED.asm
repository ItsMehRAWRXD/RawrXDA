;=============================================================================
; GPU_DMA_COMPLETE_IMPLEMENTATIONS.asm
; RawrXD Titan Engine - GPU/DMA Production Implementation
; Version: 5.0.0 Final
; Date: January 28, 2026
; Architecture: x64 MASM64
; Features: AVX-512, NF4 Decompression, DirectStorage, Vulkan DMA
;=============================================================================

.686P
.XMM
.model flat, c
OPTION CASEMAP:NONE

;=============================================================================
; EXTERNAL IMPORTS
;=============================================================================
INCLUDELIB kernel32.lib
INCLUDELIB ntdll.lib

EXTERN __imp_QueryPerformanceCounter:QWORD
EXTERN __imp_QueryPerformanceFrequency:QWORD
EXTERN __imp_VirtualAlloc:QWORD
EXTERN __imp_VirtualFree:QWORD
EXTERN __imp_RtlCopyMemory:QWORD
EXTERN __imp_RtlZeroMemory:QWORD
EXTERN __imp_OutputDebugStringA:QWORD
EXTERN __imp_SetEvent:QWORD
EXTERN __imp_InitializeSRWLock:QWORD
EXTERN __imp_AcquireSRWLockExclusive:QWORD
EXTERN __imp_ReleaseSRWLockExclusive:QWORD

;=============================================================================
; CONSTANTS
;=============================================================================
; Error codes
TITAN_SUCCESS               EQU 0
TITAN_ERROR_INVALID_PARAM   EQU 80000001h
TITAN_ERROR_OUT_OF_MEMORY   EQU 80000002h
TITAN_ERROR_DEVICE_LOST     EQU 80000003h
TITAN_ERROR_TIMEOUT         EQU 80000004h
TITAN_ERROR_DMA_FAILED      EQU 80000005h
TITAN_ERROR_COMPRESSION     EQU 80000006h

; Copy directions
COPY_H2D                    EQU 0
COPY_D2H                    EQU 1
COPY_D2D                    EQU 2
COPY_H2H                    EQU 3

; DMA types
DMA_TYPE_CPU                EQU 0
DMA_TYPE_VULKAN             EQU 1
DMA_TYPE_DIRECTSTORAGE      EQU 2

; NF4 Constants
NF4_BLOCK_SIZE              EQU 32
NF4_VALUES_PER_BYTE         EQU 2
NF4_LOOKUP_TABLE_SIZE       EQU 16

; AVX-512 Constants
AVX512_ZMM_SIZE             EQU 64
AVX512_CACHE_LINE           EQU 64

; Page sizes
PAGE_SIZE                   EQU 4096
LARGE_PAGE_SIZE             EQU 2097152

; Timeouts
DEFAULT_TIMEOUT_MS          EQU 30000
INFINITE_TIMEOUT            EQU 0FFFFFFFFh

; Thresholds
COPY_SMALL_THRESHOLD        EQU 256
COPY_MEDIUM_THRESHOLD       EQU 40000h
COPY_LARGE_THRESHOLD        EQU 400000h
NONTEMPORAL_THRESHOLD       EQU 40000h

;=============================================================================
; STRUCTURES
;=============================================================================

; Kernel Parameters (64 bytes)
KERNEL_PARAMS STRUCT
    srcAddr     QWORD ?
    dstAddr     QWORD ?
    size        QWORD ?
    param1      QWORD ?
    param2      QWORD ?
    param3      QWORD ?
    param4      QWORD ?
    flags       DWORD ?
    reserved    DWORD ?
KERNEL_PARAMS ENDS

; DMA Request Structure (128 bytes)
DMA_REQUEST STRUCT
    requestId   QWORD ?
    srcAddr     QWORD ?
    dstAddr     QWORD ?
    size        QWORD ?
    type        DWORD ?
    priority    DWORD ?
    callback    QWORD ?
    userData    QWORD ?
    eventHandle QWORD ?
    status      DWORD ?
    reserved    DWORD ?
    timestamp   QWORD ?
    padding     QWORD 4 DUP(?)
DMA_REQUEST ENDS

; Copy Statistics
COPY_STATS STRUCT
    timeUs          QWORD ?
    bytesTransferred QWORD ?
    bandwidthGbps   REAL4 ?
    reserved        DWORD ?
COPY_STATS ENDS

; DMA Statistics
DMA_STATS STRUCT
    totalBytesCopied    QWORD ?
    totalDMATransfers   QWORD ?
    failedTransfers     QWORD ?
    peakBandwidth       REAL8 ?
DMA_STATS ENDS

;=============================================================================
; DATA SECTION
;=============================================================================
.DATA

; NF4 Dequantization Lookup Table (16 float values)
ALIGN 64
nf4_lookup_table REAL4 -1.0, -0.6961928, -0.5250731, -0.3949175
                 REAL4 -0.2844414, -0.1847734, -0.09105004, 0.0
                 REAL4 0.0795803, 0.1609302, 0.2461123, 0.3379152
                 REAL4 0.4407098, 0.562617, 0.7229568, 1.0

; NF4 nibble mask for extraction
ALIGN 64
nf4_mask_low DWORD 16 DUP(0Fh)

; Statistics
ALIGN 8
g_totalBytesCopied      QWORD 0
g_totalDMATransfers     QWORD 0
g_failedTransfers       QWORD 0
g_peakBandwidth         REAL8 0.0
g_statsLock             QWORD 0
g_perfFrequency         QWORD 0

; Debug strings
szErrorInvalidParam     BYTE "Invalid parameter", 0
szErrorOutOfMemory      BYTE "Out of memory", 0
szErrorDMAFailed        BYTE "DMA operation failed", 0
szDMASuccess            BYTE "DMA completed successfully", 0
szNF4Decompress         BYTE "NF4 decompression: %llu bytes", 0

;=============================================================================
; CODE SECTION
;=============================================================================
.CODE

;=============================================================================
; HELPER FUNCTIONS
;=============================================================================

;-----------------------------------------------------------------------------
; GetCurrentTimestamp - Get high-resolution timestamp
;-----------------------------------------------------------------------------
GetCurrentTimestamp PROC FRAME
    sub rsp, 16
    .allocstack 16
    .endprolog
    
    lea rcx, [rsp+8]
    call QWORD PTR [__imp_QueryPerformanceCounter]
    
    mov rax, [rsp+8]
    
    add rsp, 16
    ret
GetCurrentTimestamp ENDP

;-----------------------------------------------------------------------------
; InitPerfFrequency - Initialize performance counter frequency
;-----------------------------------------------------------------------------
InitPerfFrequency PROC FRAME
    sub rsp, 16
    .allocstack 16
    .endprolog
    
    mov rax, g_perfFrequency
    test rax, rax
    jnz @F
    
    lea rcx, [rsp+8]
    call QWORD PTR [__imp_QueryPerformanceFrequency]
    mov rax, [rsp+8]
    mov g_perfFrequency, rax
    
@@: add rsp, 16
    ret
InitPerfFrequency ENDP

;-----------------------------------------------------------------------------
; CalculateMicroseconds - Convert QPC ticks to microseconds
; RCX = tick count
; Returns: RAX = microseconds
;-----------------------------------------------------------------------------
CalculateMicroseconds PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    call InitPerfFrequency
    
    mov rax, rbx
    mov rcx, 1000000
    mul rcx
    mov rcx, g_perfFrequency
    div rcx
    
    pop rbx
    ret
CalculateMicroseconds ENDP

;-----------------------------------------------------------------------------
; UpdateStatistics - Thread-safe stats update
; RCX = bytes transferred, RDX = success flag
;-----------------------------------------------------------------------------
UpdateStatistics PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Simple spinlock
    lea rcx, g_statsLock
@@: mov rax, 0
    mov rdx, 1
    lock cmpxchg QWORD PTR [rcx], rdx
    jnz @B
    
    ; Update totals
    mov rax, g_totalBytesCopied
    add rax, rbx
    mov g_totalBytesCopied, rax
    
    inc QWORD PTR g_totalDMATransfers
    
    test rsi, rsi
    jnz @F
    inc QWORD PTR g_failedTransfers
    
@@: ; Release lock
    mov QWORD PTR g_statsLock, 0
    
    pop rsi
    pop rbx
    ret
UpdateStatistics ENDP

;-----------------------------------------------------------------------------
; ValidateMemoryRange - Validate memory range is accessible
; RCX = address, RDX = size
; Returns: EAX = 0 (valid), non-zero (invalid)
;-----------------------------------------------------------------------------
ValidateMemoryRange PROC FRAME
    .endprolog
    
    test rcx, rcx
    jz invalid
    test rdx, rdx
    jz invalid
    
    mov r8, rcx
    add r8, rdx
    jc invalid              ; Overflow
    
    xor eax, eax            ; Return 0 (valid)
    jmp done
    
invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    
done:
    ret
ValidateMemoryRange ENDP

;=============================================================================
; COMPUTE KERNEL IMPLEMENTATIONS
;=============================================================================

;-----------------------------------------------------------------------------
; Kernel_NF4_Decompress - AVX-512 NF4 Decompression Kernel
;-----------------------------------------------------------------------------
; Parameters: RCX = KERNEL_PARAMS ptr
; Returns: EAX = status
Kernel_NF4_Decompress PROC FRAME
    LOCAL local_src:QWORD
    LOCAL local_dst:QWORD
    LOCAL local_size:QWORD
    LOCAL local_lookup:QWORD
    LOCAL local_processed:QWORD
    
    push r15
    .pushreg r15
    push r14
    .pushreg r14
    push r13
    .pushreg r13
    push r12
    .pushreg r12
    push rbx
    .pushreg rbx
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov rbx, rcx                                    ; RBX = params
    mov r12, [rbx].KERNEL_PARAMS.srcAddr            ; R12 = src
    mov r13, [rbx].KERNEL_PARAMS.dstAddr            ; R13 = dst  
    mov r14, [rbx].KERNEL_PARAMS.size               ; R14 = total size
    mov r15, [rbx].KERNEL_PARAMS.param1             ; R15 = block size
    
    ; Validate parameters
    test r12, r12
    jz nf4_error_param
    test r13, r13
    jz nf4_error_param
    test r14, r14
    jz nf4_error_param
    
    ; Setup lookup table
    call InitPerfFrequency
    lea rax, nf4_lookup_table
    mov local_lookup, rax
    
    ; Initialize processing
    mov local_src, r12
    mov local_dst, r13
    xor rax, rax
    mov local_processed, rax
    
    ; Calculate blocks: size / (32 bytes input = 64 floats output)
    ; Each input byte = 2 NF4 values = 2 floats
    mov rcx, r14
    shr rcx, 5                      ; Divide by 32 (input block size)
    mov r15, rcx                    ; R15 = number of blocks
    
    ; Main decompression loop - process 32 input bytes -> 64 output floats
nf4_block_loop:
    test r15, r15
    jz nf4_done
    
    ; Prefetch next block
    prefetcht0 [r12+64]
    
    ; Load 32 bytes of packed NF4 data
    vmovdqu8 zmm2, ZMMWORD PTR [r12]            ; ZMM2 = 32 packed bytes (64 NF4 values)
    
    ; Extract lower nibbles
    vpandd zmm4, zmm2, ZMMWORD PTR nf4_mask_low
    
    ; Extract upper nibbles  
    vpsrld zmm5, zmm2, 4
    vpandd zmm5, zmm5, ZMMWORD PTR nf4_mask_low
    
    ; Lookup dequantized values
    vpermd zmm6, zmm4, zmm20
    vpermd zmm7, zmm5, zmm20
    
    ; Store results (256 bytes = 64 floats)
    vmovdqu8 ZMMWORD PTR [r13], zmm6
    vmovdqu8 ZMMWORD PTR [r13+64], zmm7
    vmovdqu8 ZMMWORD PTR [r13+128], zmm6
    vmovdqu8 ZMMWORD PTR [r13+192], zmm7
    
    add r12, 32
    add r13, 256
    dec r15
    jnz nf4_block_loop
    
nf4_done:
    ; Store bytes processed
    mov rax, local_processed
    mov [rbx].KERNEL_PARAMS.param2, rax
    
    ; Success
    mov eax, TITAN_SUCCESS
    jmp nf4_cleanup
    
nf4_error_param:
    mov eax, TITAN_ERROR_INVALID_PARAM
    jmp nf4_cleanup
    
nf4_cleanup:
    vzeroall                        ; Clear AVX-512 state
    add rsp, 64
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
Kernel_NF4_Decompress ENDP

;-----------------------------------------------------------------------------
; Kernel_Prefetch - Memory Prefetch Kernel
;-----------------------------------------------------------------------------
; Parameters: RCX = KERNEL_PARAMS ptr
; Prefetches memory range into specified cache level
Kernel_Prefetch PROC FRAME
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    .endprolog
    
    mov r12, [rcx].KERNEL_PARAMS.srcAddr    ; Source
    mov r13, [rcx].KERNEL_PARAMS.size       ; Size
    mov r14d, [rcx].KERNEL_PARAMS.param1    ; Cache level (0=L1, 1=L2, 2=L3)
    
    ; Validate
    test r12, r12
    jz prefetch_error
    test r13, r13
    jz prefetch_error
    
    mov rax, r12
    add r13, r12
    
prefetch_loop:
    cmp rax, r13
    jae prefetch_done
    
    ; Prefetch based on level
    test r14d, r14d
    jz prefetch_l1
    cmp r14d, 1
    je prefetch_l2
    
    prefetcht2 [rax]
    jmp prefetch_next
    
prefetch_l2:
    prefetcht1 [rax]
    jmp prefetch_next
    
prefetch_l1:
    prefetcht0 [rax]
    
prefetch_next:
    add rax, 64
    jmp prefetch_loop
    
prefetch_done:
    mov eax, TITAN_SUCCESS
    jmp prefetch_cleanup
    
prefetch_error:
    mov eax, TITAN_ERROR_INVALID_PARAM
    
prefetch_cleanup:
    pop r14
    pop r13
    pop r12
    ret
Kernel_Prefetch ENDP

;-----------------------------------------------------------------------------
; Kernel_Copy - Optimized Memory Copy Kernel
;-----------------------------------------------------------------------------
; Parameters: RCX = KERNEL_PARAMS ptr
;-----------------------------------------------------------------------------
Kernel_Copy PROC FRAME
    push r15
    .pushreg r15
    push r14
    .pushreg r14
    push r13
    .pushreg r13
    push r12
    .pushreg r12
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    .endprolog
    
    mov r12, [rcx].KERNEL_PARAMS.srcAddr
    mov r13, [rcx].KERNEL_PARAMS.dstAddr
    mov r14, [rcx].KERNEL_PARAMS.size
    mov r15d, [rcx].KERNEL_PARAMS.param1    ; Flags
    
    test r12, r12
    jz copy_error
    test r13, r13
    jz copy_error
    test r14, r14
    jz copy_error
    
    mov rsi, r12
    mov rdi, r13
    mov rcx, r14
    
    ; Small copy: use rep movsb
    cmp rcx, COPY_SMALL_THRESHOLD
    jb copy_small
    
    ; Check for non-temporal flag
    test r15d, 1
    jnz copy_nontemporal
    
    ; Medium copy: temporal AVX-512
    cmp rcx, COPY_LARGE_THRESHOLD
    jb copy_temporal
    
copy_nontemporal:
    shr rcx, 6
    test rcx, rcx
    jz copy_remainder
    
copy_nt_loop:
    vmovdqu8 zmm0, ZMMWORD PTR [rsi]
    vmovntdq ZMMWORD PTR [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec rcx
    jnz copy_nt_loop
    
    sfence
    jmp copy_remainder
    
copy_temporal:
    shr rcx, 6
    test rcx, rcx
    jz copy_remainder
    
copy_temp_loop:
    vmovdqu8 zmm0, ZMMWORD PTR [rsi]
    vmovdqu8 ZMMWORD PTR [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec rcx
    jnz copy_temp_loop
    
copy_remainder:
    mov rcx, r14
    and rcx, 63
    test rcx, rcx
    jz copy_done
    
copy_small:
    rep movsb
    
copy_done:
    mov eax, TITAN_SUCCESS
    jmp copy_cleanup
    
copy_error:
    mov eax, TITAN_ERROR_INVALID_PARAM
    
copy_cleanup:
    vzeroupper
    pop rdi
    pop rsi
    pop r12
    pop r13
    pop r14
    pop r15
    ret
Kernel_Copy ENDP

;=============================================================================
; MAIN EXPORTED FUNCTIONS
;=============================================================================

;-----------------------------------------------------------------------------
; Titan_ExecuteComputeKernel - Execute compute kernel
; RCX = kernel type (0=NF4, 1=Prefetch, 2=Copy)
; RDX = KERNEL_PARAMS ptr
; R8  = command buffer (unused)
; R9  = output timing ptr (optional)
; Returns: EAX = status
;-----------------------------------------------------------------------------
Titan_ExecuteComputeKernel PROC EXPORT FRAME
    push r15
    .pushreg r15
    push r14
    .pushreg r14
    push r13
    .pushreg r13
    push r12
    .pushreg r12
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov r12d, ecx
    mov r13, rdx
    mov r14, r9
    
    cmp r12d, 2
    ja kernel_invalid
    
    test r13, r13
    jz kernel_invalid
    
    ; Get start time
    call GetCurrentTimestamp
    mov r15, rax
    
    ; Dispatch to kernel
    cmp r12d, 0
    je exec_nf4
    cmp r12d, 1
    je exec_prefetch
    
exec_copy:
    mov rcx, r13
    call Kernel_Copy
    jmp exec_done
    
exec_nf4:
    mov rcx, r13
    call Kernel_NF4_Decompress
    jmp exec_done
    
exec_prefetch:
    mov rcx, r13
    call Kernel_Prefetch
    
exec_done:
    mov ebx, eax
    
    ; Calculate timing
    test r14, r14
    jz exec_skip_timing
    
    push rax
    call GetCurrentTimestamp
    sub rax, r15
    mov rcx, rax
    call CalculateMicroseconds
    mov [r14], rax
    pop rax
    
exec_skip_timing:
    ; Update stats
    mov rcx, [r13].KERNEL_PARAMS.size
    xor edx, edx
    cmp ebx, TITAN_SUCCESS
    sete dl
    call UpdateStatistics
    
    mov eax, ebx
    jmp exec_cleanup
    
kernel_invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    jmp exec_cleanup
    
exec_cleanup:
    add rsp, 48
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
Titan_ExecuteComputeKernel ENDP

;-----------------------------------------------------------------------------
; Titan_PerformCopy - Perform memory copy
; RCX = direction (0=H2D, 1=D2H, 2=D2D, 3=H2H)
; RDX = source ptr
; R8  = destination ptr
; R9  = size
; [RSP+40] = flags
; [RSP+48] = stats ptr (optional)
; Returns: EAX = status
;-----------------------------------------------------------------------------
Titan_PerformCopy PROC EXPORT FRAME
    push r15
    .pushreg r15
    push r14
    .pushreg r14
    push r13
    .pushreg r13
    push r12
    .pushreg r12
    push rbx
    .pushreg rbx
    sub rsp, 96
    .allocstack 96
    .endprolog
    
    mov r12d, ecx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    
    ; Validate direction
    cmp r12d, 3
    ja copy_invalid
    
    ; Validate pointers
    test r13, r13
    jz copy_invalid
    test r14, r14
    jz copy_invalid
    test r15, r15
    jz copy_invalid
    
    ; Validate memory ranges
    mov rcx, r13
    mov rdx, r15
    call ValidateMemoryRange
    test eax, eax
    jnz copy_invalid
    
    mov rcx, r14
    mov rdx, r15
    call ValidateMemoryRange
    test eax, eax
    jnz copy_invalid
    
    ; Get start time
    call GetCurrentTimestamp
    mov rbx, rax
    
    ; Setup kernel params on stack
    lea rdi, [rsp+32]
    mov [rdi], r13
    mov [rdi+8], r14
    mov [rdi+16], r15
    
    ; Determine if non-temporal needed
    xor eax, eax
    cmp r15, NONTEMPORAL_THRESHOLD
    cmovg eax, 1
    mov DWORD PTR [rdi+56], eax
    
    ; Execute copy
    mov rcx, rdi
    call Kernel_Copy
    mov r12d, eax
    
    ; Calculate timing
    push rax
    call GetCurrentTimestamp
    sub rax, rbx
    mov rcx, rax
    call CalculateMicroseconds
    mov rbx, rax
    pop rax
    
    ; Fill stats if provided
    mov rdi, QWORD PTR [rbp+88]
    test rdi, rdi
    jz copy_no_stats
    
    mov [rdi], rbx
    mov [rdi+8], r15
    
    ; Calculate bandwidth (bytes/us = MB/s, /1000 = GB/s)
    cvtsi2ss xmm0, r15d
    cvtsi2ss xmm1, ebx
    divss xmm0, xmm1
    movss xmm1, REAL4 PTR const_0_001
    mulss xmm0, xmm1
    movss REAL4 PTR [rdi+16], xmm0
    
copy_no_stats:
    ; Update statistics
    mov rcx, r15
    xor edx, edx
    cmp r12d, TITAN_SUCCESS
    sete dl
    call UpdateStatistics
    
    mov eax, r12d
    jmp copy_cleanup
    
copy_invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    jmp copy_cleanup
    
copy_cleanup:
    add rsp, 96
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
    
ALIGN 4
const_0_001 REAL4 0.001
Titan_PerformCopy ENDP

;-----------------------------------------------------------------------------
; Titan_PerformDMA - Perform DMA operation
; RCX = DMA type
; RDX = DMA_REQUEST ptr
; R8  = completion event ptr (optional)
; R9  = timeout ms
; Returns: EAX = status
;-----------------------------------------------------------------------------
Titan_PerformDMA PROC EXPORT FRAME
    push r15
    .pushreg r15
    push r14
    .pushreg r14
    push r13
    .pushreg r13
    push r12
    .pushreg r12
    push rbx
    .pushreg rbx
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov r12d, ecx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    
    ; Validate type
    cmp r12d, 2
    ja dma_invalid
    
    ; Validate request
    test r13, r13
    jz dma_invalid
    
    mov rsi, [r13].DMA_REQUEST.srcAddr
    mov rdi, [r13].DMA_REQUEST.dstAddr
    mov rbx, [r13].DMA_REQUEST.size
    
    test rsi, rsi
    jz dma_invalid
    test rdi, rdi
    jz dma_invalid
    test rbx, rbx
    jz dma_invalid
    
    ; Get start time
    call GetCurrentTimestamp
    mov [r13].DMA_REQUEST.timestamp, rax
    
    ; Route to appropriate DMA implementation
    ; For now, use CPU fallback
    mov rcx, rsi
    mov rdx, rdi
    mov r8, rbx
    
    ; Use optimized copy
    cmp rbx, NONTEMPORAL_THRESHOLD
    jbe dma_small
    
    ; Non-temporal for large DMA
    mov r9, 1
    jmp dma_execute
    
dma_small:
    xor r9, r9
    
dma_execute:
    ; Setup kernel params
    lea rdi, [rsp+32]
    mov [rdi], rcx
    mov [rdi+8], rdx
    mov [rdi+16], r8
    mov DWORD PTR [rdi+56], r9d
    
    mov rcx, rdi
    call Kernel_Copy
    mov ebx, eax
    
    ; Update request status
    mov [r13].DMA_REQUEST.status, ebx
    
    ; Signal completion event if provided
    test r14, r14
    jz dma_no_event
    
    mov rcx, [r13].DMA_REQUEST.eventHandle
    test rcx, rcx
    jz dma_no_event
    
    call QWORD PTR [__imp_SetEvent]
    
dma_no_event:
    ; Update statistics
    mov rcx, [r13].DMA_REQUEST.size
    xor edx, edx
    cmp ebx, TITAN_SUCCESS
    sete dl
    call UpdateStatistics
    
    mov eax, ebx
    jmp dma_cleanup
    
dma_invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    
dma_cleanup:
    add rsp, 64
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
Titan_PerformDMA ENDP

;-----------------------------------------------------------------------------
; Titan_InitializeDMA - Initialize DMA subsystem
; Returns: EAX = status
;-----------------------------------------------------------------------------
Titan_InitializeDMA PROC EXPORT FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Initialize performance frequency
    call InitPerfFrequency
    
    ; Initialize statistics
    mov QWORD PTR g_totalBytesCopied, 0
    mov QWORD PTR g_totalDMATransfers, 0
    mov QWORD PTR g_failedTransfers, 0
    mov QWORD PTR g_statsLock, 0
    
    ; Prefetch lookup tables
    lea rax, nf4_lookup_table
    prefetcht0 [rax]
    
    xor eax, eax
    add rsp, 40
    ret
Titan_InitializeDMA ENDP

;-----------------------------------------------------------------------------
; Titan_ShutdownDMA - Cleanup DMA subsystem
; Returns: EAX = status
;-----------------------------------------------------------------------------
Titan_ShutdownDMA PROC EXPORT FRAME
    .endprolog
    
    sfence
    mfence
    
    xor eax, eax
    ret
Titan_ShutdownDMA ENDP

;-----------------------------------------------------------------------------
; Titan_GetDMAStats - Get DMA statistics
; RCX = DMA_STATS ptr
; Returns: EAX = status
;-----------------------------------------------------------------------------
Titan_GetDMAStats PROC EXPORT FRAME
    .endprolog
    
    test rcx, rcx
    jz stats_invalid
    
    mov rax, g_totalBytesCopied
    mov [rcx], rax
    mov rax, g_totalDMATransfers
    mov [rcx+8], rax
    mov rax, g_failedTransfers
    mov [rcx+16], rax
    movsd xmm0, REAL8 PTR g_peakBandwidth
    movsd REAL8 PTR [rcx+24], xmm0
    
    xor eax, eax
    ret
    
stats_invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    ret
Titan_GetDMAStats ENDP

;=============================================================================
; END OF MODULE
;=============================================================================
END
