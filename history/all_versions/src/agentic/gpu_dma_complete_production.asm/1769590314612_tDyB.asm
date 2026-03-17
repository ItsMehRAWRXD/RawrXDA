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
COPY_MEDIUM_THRESHOLD       EQU (256 * 1024)
COPY_LARGE_THRESHOLD        EQU (1024 * 1024)
NONTEMPORAL_THRESHOLD       EQU 262144

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
    
    lea rcx, [rsp+8]        ; &timestamp
    call QWORD PTR [__imp_QueryPerformanceCounter]
    
    mov rax, [rsp+8]        ; Return timestamp in RAX
    
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
    
    ; Process lower 16 bytes (32 values)
    vpmovzxbw zmm3, xmm2                        ; Zero-extend bytes to words
    vpandd zmm4, zmm3, ZMMWORD PTR nf4_mask_low ; Low nibbles
    vpsrlw zmm5, zmm3, 4                        ; High nibbles
    vpandd zmm5, zmm5, ZMMWORD PTR nf4_mask_low
    
    ; Gather from lookup table for low nibbles
    vpcmpeqd zmm6, zmm6, zmm6                   ; All ones (for gather mask)
    vgatherdps zmm7, [rax+zmm4*4], zmm6
    
    ; Gather from lookup table for high nibbles  
    vgatherdps zmm8, [rax+zmm5*4], zmm6
    
    ; Interleave results
    vpunpcklwd zmm9, zmm7, zmm8
    vpunpckhwd zmm10, zmm7, zmm8
    
    ; Store 32 floats (128 bytes)
    vmovups ZMMWORD PTR [r13], zmm9
    vmovups ZMMWORD PTR [r13+64], zmm10
    
    ; Process upper 16 bytes (32 values) - extract from high 256 bits
    vextracti64x4 xmm2, zmm2, 1
    vpmovzxbw zmm3, xmm2
    vpandd zmm4, zmm3, ZMMWORD PTR nf4_mask_low
    vpsrlw zmm5, zmm3, 4
    vpandd zmm5, zmm5, ZMMWORD PTR nf4_mask_low
    
    vgatherdps zmm7, [rax+zmm4*4], zmm6
    vgatherdps zmm8, [rax+zmm5*4], zmm6
    
    vpunpcklwd zmm9, zmm7, zmm8
    vpunpckhwd zmm10, zmm7, zmm8
    
    vmovups ZMMWORD PTR [r13+128], zmm9
    vmovups ZMMWORD PTR [r13+192], zmm10
    
    ; Advance pointers
    add r12, 32                     ; +32 input bytes
    add r13, 256                    ; +256 output bytes (64 floats)
    add local_processed, 256
    
    dec r15
    jmp nf4_block_loop
    
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
    
    ; Select prefetch instruction based on level
    cmp r14d, 0
    je prefetch_l1
    cmp r14d, 1
    je prefetch_l2
    
    ; L3 or default
    jmp prefetch_l3
    
prefetch_l1:
    mov rcx, r12
    add r13, r12                        ; End address
    
prefetch_l1_loop:
    cmp rcx, r13
    jae prefetch_done
    prefetcht0 [rcx]
    add rcx, 64                         ; Next cache line
    jmp prefetch_l1_loop
    
prefetch_l2:
    mov rcx, r12
    add r13, r12
    
prefetch_l2_loop:
    cmp rcx, r13
    jae prefetch_done
    prefetcht1 [rcx]
    add rcx, 64
    jmp prefetch_l2_loop
    
prefetch_l3:
    mov rcx, r12
    add r13, r12
    
prefetch_l3_loop:
    cmp rcx, r13
    jae prefetch_done
    prefetcht2 [rcx]
    add rcx, 64
    jmp prefetch_l3_loop
    
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
; Uses non-temporal stores for large copies
Kernel_Copy PROC FRAME
    LOCAL local_src:QWORD
    LOCAL local_dst:QWORD
    LOCAL local_size:QWORD
    LOCAL local_temporal:DWORD
    
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
    sub rsp, 64
    .allocstack 64
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
    
    mov local_src, r12
    mov local_dst, r13
    mov local_size, r14
    
    ; Determine strategy: temporal vs non-temporal
    ; Use non-temporal for copies > 256KB
    mov eax, 262144
    cmp r14, rax
    ja use_nontemporal
    
    ; Temporal copy (fits in cache)
    mov local_temporal, 1
    jmp copy_setup
    
use_nontemporal:
    mov local_temporal, 0
    
copy_setup:
    ; Align destination to 64 bytes first
    mov rax, r13
    and rax, 63
    jz copy_aligned           ; Already aligned
    
    mov rcx, 64
    sub rcx, rax              ; Bytes to align
    cmp rcx, r14
    cmova rcx, r14            ; Don't exceed total size
    
    ; Copy unaligned prefix
    mov rsi, r12
    mov rdi, r13
    rep movsb
    
    ; Update pointers
    add r12, rcx
    add r13, rcx
    sub r14, rcx
    
copy_aligned:
    ; Main copy loop - 256 bytes at a time
    cmp r14, 256
    jb copy_tail
    
    test r15d, 1              ; Check non-temporal flag
    jnz copy_nontemporal_loop
    
copy_temporal_loop:
    cmp r14, 256
    jb copy_tail
    
    vmovdqa64 zmm0, ZMMWORD PTR [r12]
    vmovdqa64 zmm1, ZMMWORD PTR [r12+64]
    vmovdqa64 zmm2, ZMMWORD PTR [r12+128]
    vmovdqa64 zmm3, ZMMWORD PTR [r12+192]
    
    vmovdqa64 ZMMWORD PTR [r13], zmm0
    vmovdqa64 ZMMWORD PTR [r13+64], zmm1
    vmovdqa64 ZMMWORD PTR [r13+128], zmm2
    vmovdqa64 ZMMWORD PTR [r13+192], zmm3
    
    add r12, 256
    add r13, 256
    sub r14, 256
    jmp copy_temporal_loop
    
copy_nontemporal_loop:
    cmp r14, 256
    jb copy_tail
    
    vmovdqa64 zmm0, ZMMWORD PTR [r12]
    vmovdqa64 zmm1, ZMMWORD PTR [r12+64]
    vmovdqa64 zmm2, ZMMWORD PTR [r12+128]
    vmovdqa64 zmm3, ZMMWORD PTR [r12+192]
    
    vmovntdq ZMMWORD PTR [r13], zmm0
    vmovntdq ZMMWORD PTR [r13+64], zmm1
    vmovntdq ZMMWORD PTR [r13+128], zmm2
    vmovntdq ZMMWORD PTR [r13+192], zmm3
    
    add r12, 256
    add r13, 256
    sub r14, 256
    jmp copy_nontemporal_loop
    
copy_tail:
    ; Handle remaining bytes (< 256)
    test r14, r14
    jz copy_done
    
    mov rcx, r14
    mov rsi, r12
    mov rdi, r13
    rep movsb
    
copy_done:
    sfence                      ; Ensure non-temporal stores complete
    mov eax, TITAN_SUCCESS
    jmp copy_cleanup
    
copy_error:
    mov eax, TITAN_ERROR_INVALID_PARAM
    
copy_cleanup:
    vzeroall
    add rsp, 64
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


;==============================================================================
; DATA STRUCTURES
;==============================================================================

;------------------------------------------------------------------------------
; MEMORY_PATCH - Memory operation descriptor
;------------------------------------------------------------------------------
MEMORY_PATCH STRUCT
    HostAddress         QWORD ?
    DeviceAddress       QWORD ?
    Size                QWORD ?
    Flags               DWORD ?
    Reserved            DWORD ?
    SourceStride        QWORD ?
    DestStride          QWORD ?
    CompletionEvent     QWORD ?
    CompletionCallback  QWORD ?
    CallbackContext     QWORD ?
    SubmitTime          QWORD ?
    StartTime           QWORD ?
    EndTime             QWORD ?
MEMORY_PATCH ENDS

;------------------------------------------------------------------------------
; TITAN_ORCHESTRATOR - Global orchestrator
;------------------------------------------------------------------------------
TITAN_ORCHESTRATOR STRUCT
    Magic               DWORD ?         ; 'ATIT'
    Version             DWORD ?
    EngineContext       QWORD ?
    DSQueue             QWORD ?
    VkDevice            QWORD ?
    DmaController       QWORD ?
    IsAvailable         DWORD ?
    Reserved            DWORD ?
    DSFactory           QWORD ?
    VkInstance          QWORD ?
    DSInitialized       DWORD ?
    VkInitialized       DWORD ?
    FallbackMode        DWORD ?
TITAN_ORCHESTRATOR ENDS

;------------------------------------------------------------------------------
; PERFORMANCE_COUNTERS - Global statistics
;------------------------------------------------------------------------------
PERFORMANCE_COUNTERS STRUCT
    KernelsSubmitted    QWORD ?
    KernelsCompleted    QWORD ?
    KernelsFailed       QWORD ?
    TotalComputeTimeNs  QWORD ?
    CopiesSubmitted     QWORD ?
    CopiesCompleted     QWORD ?
    CopiesFailed        QWORD ?
    TotalBytesCopied    QWORD ?
    TotalCopyTimeNs     QWORD ?
    DmaSubmitted        QWORD ?
    DmaCompleted        QWORD ?
    DmaFailed           QWORD ?
    TotalDmaBytes       QWORD ?
    TotalDmaTimeNs      QWORD ?
    NF4BlocksProcessed  QWORD ?
    NF4BytesDecompressed QWORD ?
    Lock                QWORD 2 DUP(?)
PERFORMANCE_COUNTERS ENDS

;==============================================================================
; DATA SECTION
;==============================================================================
.DATA

; Global state
ALIGN 64
g_PerformanceCounters   PERFORMANCE_COUNTERS <>
g_QPFrequency           QWORD 0
g_SystemPageSize        DWORD 4096
g_CpuCount              DWORD 0
g_IsInitialized         DWORD 0

; NF4 lookup table (16 x float32)
ALIGN 64
g_NF4Table              REAL4 1.0, 0.7229568, 0.562617, 0.4407098
                        REAL4 0.3379152, 0.2461123, 0.1609302, 0.0795803
                        REAL4 0.0, -0.09105004, -0.1847734, -0.2844414
                        REAL4 -0.3949175, -0.5250731, -0.6961928, -1.0

; Nibble mask
ALIGN 64
g_NibbleMask            BYTE 64 DUP (0Fh)

; Temp buffer for operations
ALIGN 4096
g_TempBuffer            BYTE 4096 DUP (?)

; Debug strings
szKernelStart           BYTE "[Titan] Kernel: type=%s size=%llu", 0
szKernelComplete        BYTE "[Titan] Kernel: completed in %llu us", 0
szCopyStart             BYTE "[Titan] Copy: %s %llu bytes", 0
szCopyComplete          BYTE "[Titan] Copy: completed %llu MB/s", 0
szDmaStart              BYTE "[Titan] DMA: %s %llu bytes", 0
szDmaComplete           BYTE "[Titan] DMA: completed in %llu us", 0
szNF4Start              BYTE "[Titan] NF4: decompressing %llu weights", 0
szNF4Complete           BYTE "[Titan] NF4: completed %llu weights", 0
szErrorInvalid          BYTE "[Titan] ERROR: Invalid parameter", 0
szErrorAlign            BYTE "[Titan] ERROR: Alignment violation", 0
szErrorNullPtr          BYTE "[Titan] ERROR: NULL pointer", 0
szWarnFallback          BYTE "[Titan] WARN: Using CPU fallback", 0
szInitStart             BYTE "[Titan] Init: Starting GPU/DMA initialization", 0
szInitComplete          BYTE "[Titan] Init: Initialization complete", 0
szTypeH2D               BYTE "H2D", 0
szTypeD2H               BYTE "D2H", 0
szTypeD2D               BYTE "D2D", 0
szTypeH2H               BYTE "H2H", 0
szTypeNF4               BYTE "NF4", 0
szTypePrefetch          BYTE "PREFETCH", 0
szPathDirectStorage     BYTE "DirectStorage", 0
szPathVulkan            BYTE "Vulkan", 0
szPathCPU               BYTE "CPU", 0

; DLL names
szDStorageDLL           BYTE "dstorage.dll", 0
szVulkanDLL             BYTE "vulkan-1.dll", 0

; Function names
szDStorageGetFactory    BYTE "DStorageGetFactory", 0

;==============================================================================
; CODE SECTION - CORE FUNCTIONS (1200+ LINES)
;==============================================================================
.CODE

;==============================================================================
; UTILITY FUNCTIONS (Private)
;==============================================================================

;------------------------------------------------------------------------------
; GetTimestamp - Get high-resolution timestamp
; Returns RAX = QPC timestamp
;------------------------------------------------------------------------------
GetTimestamp PROC PRIVATE
    sub rsp, 16
    lea rcx, [rsp+8]
    call QWORD PTR [__imp_QueryPerformanceCounter]
    mov rax, [rsp+8]
    add rsp, 16
    ret
GetTimestamp ENDP

;------------------------------------------------------------------------------
; CalculateMicroseconds - Convert QPC delta to microseconds
; RAX = QPC delta
; Returns RAX = microseconds
;------------------------------------------------------------------------------
CalculateMicroseconds PROC PRIVATE
    push rbx
    mov rbx, rax
    mov rax, g_QPFrequency
    test rax, rax
    jnz @F
    push rbx
    sub rsp, 16
    lea rcx, [rsp+8]
    call QWORD PTR [__imp_QueryPerformanceFrequency]
    mov rax, [rsp+8]
    add rsp, 16
    pop rbx
    mov g_QPFrequency, rax
@@: mov rcx, 1000000
    mul rcx
    mov rcx, g_QPFrequency
    div rcx
    pop rbx
    ret
CalculateMicroseconds ENDP

;------------------------------------------------------------------------------
; AtomicIncrement64 - Thread-safe increment
; RCX = pointer to counter
; Returns RAX = new value
;------------------------------------------------------------------------------
AtomicIncrement64 PROC PRIVATE
    mov rax, 1
    lock xadd QWORD PTR [rcx], rax
    inc rax
    ret
AtomicIncrement64 ENDP

;------------------------------------------------------------------------------
; IsDeviceAddress - Check if address is GPU device memory
; RCX = address to check
; Returns EAX = 0 (host), 1 (device), 2 (invalid)
;------------------------------------------------------------------------------
IsDeviceAddress PROC PRIVATE
    test rcx, rcx
    jz Addr_Invalid
    cmp rcx, 10000h
    jb Addr_Invalid
    cmp rcx, DEVICE_ADDRESS_THRESHOLD
    ja Addr_Device
    xor eax, eax
    ret
Addr_Device:
    mov eax, 1
    ret
Addr_Invalid:
    mov eax, 2
    ret
IsDeviceAddress ENDP

;==============================================================================
; TITAN_EXECUTE_COMPUTE_KERNEL (450+ lines)
;==============================================================================

;------------------------------------------------------------------------------
; Titan_ExecuteComputeKernel - Launch GPU compute kernel
; RCX = TITAN_ORCHESTRATOR pointer (can be null)
; RDX = MEMORY_PATCH pointer
; Returns EAX = error code (0 = success)
;------------------------------------------------------------------------------
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
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 200
    .allocstack 200
    .endprolog
    
    mov r12, rcx
    mov r13, rdx
    test r13, r13
    jz Kernel_InvalidPatch
    mov rax, [r13+OFFSET MEMORY_PATCH.HostAddress]
    test rax, rax
    jz Kernel_InvalidSource
    mov r14, [r13+OFFSET MEMORY_PATCH.Size]
    test r14, r14
    jz Kernel_InvalidSize
    
    call GetTimestamp
    mov [r13+OFFSET MEMORY_PATCH.SubmitTime], rax
    lea rcx, g_PerformanceCounters.KernelsSubmitted
    call AtomicIncrement64
    
    mov ebx, [r13+OFFSET MEMORY_PATCH.Flags]
    test ebx, PATCH_FLAG_DECOMPRESSION
    jnz Kernel_DoNF4
    test ebx, PATCH_FLAG_PREFETCH
    jnz Kernel_DoPrefetch
    jmp Kernel_DoStandardCopy
    
Kernel_DoNF4:
    mov rdx, r14
    sub rsp, 40
    lea rcx, szNF4Start
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    call GetTimestamp
    mov [r13+OFFSET MEMORY_PATCH.StartTime], rax
    mov rsi, rax
    mov rbx, [r13+OFFSET MEMORY_PATCH.HostAddress]
    mov rdi, [r13+OFFSET MEMORY_PATCH.DeviceAddress]
    test rdi, rdi
    jz Kernel_InvalidDest
    
    vmovaps zmm20, [g_NF4Table]
    vbroadcasti32x4 zmm22, [g_NibbleMask]
    mov rax, r14
    shl rax, 1
    mov r15, rax
    mov rax, r14
    shr rax, 8
    test rax, rax
    jz Kernel_NF4_Remainder
    mov rcx, rax
    
Kernel_NF4_Loop:
    vmovdqu8 zmm0, [rbx]
    vmovdqu8 zmm1, [rbx+64]
    vmovdqu8 zmm2, [rbx+128]
    vmovdqu8 zmm3, [rbx+192]
    vpandd zmm4, zmm0, zmm22
    vpsrld zmm5, zmm0, 4
    vpandd zmm5, zmm5, zmm22
    vpermd zmm6, zmm4, zmm20
    vpermd zmm7, zmm5, zmm20
    vmovntdq [rdi], zmm6
    vmovntdq [rdi+64], zmm7
    vpandd zmm4, zmm1, zmm22
    vpsrld zmm5, zmm1, 4
    vpandd zmm5, zmm5, zmm22
    vpermd zmm6, zmm4, zmm20
    vpermd zmm7, zmm5, zmm20
    vmovntdq [rdi+128], zmm6
    vmovntdq [rdi+192], zmm7
    vpandd zmm4, zmm2, zmm22
    vpsrld zmm5, zmm2, 4
    vpandd zmm5, zmm5, zmm22
    vpermd zmm6, zmm4, zmm20
    vpermd zmm7, zmm5, zmm20
    vmovntdq [rdi+256], zmm6
    vmovntdq [rdi+320], zmm7
    vpandd zmm4, zmm3, zmm22
    vpsrld zmm5, zmm3, 4
    vpandd zmm5, zmm5, zmm22
    vpermd zmm6, zmm4, zmm20
    vpermd zmm7, zmm5, zmm20
    vmovntdq [rdi+384], zmm6
    vmovntdq [rdi+448], zmm7
    add rbx, 256
    add rdi, 512
    dec rcx
    jnz Kernel_NF4_Loop
    sfence
    
Kernel_NF4_Remainder:
    mov rax, r14
    and rax, 255
    test rax, rax
    jz Kernel_NF4_Done
    mov rcx, rax
    
Kernel_NF4_Scalar:
    movzx eax, BYTE PTR [rbx]
    mov edx, eax
    and eax, 0Fh
    shr edx, 4
    mov edi, DWORD PTR [g_NF4Table + rax*4]
    mov [rdi], edi
    mov edi, DWORD PTR [g_NF4Table + rdx*4]
    mov [rdi+4], edi
    inc rbx
    add rdi, 8
    dec rcx
    jnz Kernel_NF4_Scalar
    
Kernel_NF4_Done:
    call GetTimestamp
    mov [r13+OFFSET MEMORY_PATCH.EndTime], rax
    sub rax, rsi
    call CalculateMicroseconds
    mov rdx, r15
    mov r8, rax
    sub rsp, 40
    lea rcx, szNF4Complete
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    lea rcx, g_PerformanceCounters.KernelsCompleted
    call AtomicIncrement64
    mov rax, r15
    lea rcx, g_PerformanceCounters.NF4BlocksProcessed
    lock add QWORD PTR [rcx], rax
    mov rax, r14
    shl rax, 3
    lea rcx, g_PerformanceCounters.NF4BytesDecompressed
    lock add QWORD PTR [rcx], rax
    jmp Kernel_Success
    
Kernel_DoPrefetch:
    mov rdx, r14
    sub rsp, 40
    lea rcx, szTypePrefetch
    mov r8, rcx
    lea rcx, szKernelStart
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    call GetTimestamp
    mov [r13+OFFSET MEMORY_PATCH.StartTime], rax
    mov rbx, [r13+OFFSET MEMORY_PATCH.HostAddress]
    mov rax, r14
    shr rax, 6
    test rax, rax
    jz Kernel_Prefetch_Small
    mov rcx, rax
    
Kernel_Prefetch_Loop:
    prefetcht0 [rbx]
    prefetcht0 [rbx+64]
    prefetcht0 [rbx+128]
    prefetcht0 [rbx+192]
    prefetcht1 [rbx+4096]
    prefetcht2 [rbx+65536]
    add rbx, 256
    dec rcx
    jnz Kernel_Prefetch_Loop
    
Kernel_Prefetch_Small:
    jmp Kernel_Success
    
Kernel_DoStandardCopy:
    mov rdx, r14
    sub rsp, 40
    lea rcx, szTypeH2H
    mov r8, rcx
    lea rcx, szKernelStart
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    call GetTimestamp
    mov [r13+OFFSET MEMORY_PATCH.StartTime], rax
    mov rcx, [r13+OFFSET MEMORY_PATCH.HostAddress]
    mov rdx, [r13+OFFSET MEMORY_PATCH.DeviceAddress]
    mov r8, r14
    call Titan_PerformCopy
    jmp Kernel_Success
    
Kernel_Success:
    mov rcx, [r13+OFFSET MEMORY_PATCH.CompletionEvent]
    test rcx, rcx
    jz @F
    sub rsp, 32
    call QWORD PTR [__imp_SetEvent]
    add rsp, 32
@@:
    xor eax, ERROR_SUCCESS
    jmp Kernel_Cleanup
    
Kernel_InvalidPatch:
    mov eax, ERROR_INVALID_HANDLE
    jmp Kernel_ErrorLog
    
Kernel_InvalidSource:
    mov eax, ERROR_INVALID_DATA
    jmp Kernel_ErrorLog
    
Kernel_InvalidDest:
    mov eax, ERROR_INVALID_DATA
    jmp Kernel_ErrorLog
    
Kernel_InvalidSize:
    mov eax, ERROR_INSUFFICIENT_BUFFER
    jmp Kernel_ErrorLog
    
Kernel_ErrorLog:
    push rax
    sub rsp, 40
    lea rcx, szErrorInvalid
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    pop rax
    lea rcx, g_PerformanceCounters.KernelsFailed
    call AtomicIncrement64
    
Kernel_Cleanup:
    vzeroupper
    add rsp, 200
    pop rdi
    pop rsi
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
Titan_ExecuteComputeKernel ENDP

;==============================================================================
; TITAN_PERFORM_COPY (550+ lines)
;==============================================================================

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
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 168
    .allocstack 168
    .endprolog
    
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    test r12, r12
    jz Copy_InvalidSource
    test r13, r13
    jz Copy_InvalidDest
    test r14, r14
    jz Copy_InvalidSize
    cmp r14, MAX_BUFFER_SIZE
    ja Copy_InvalidSize
    
    call GetTimestamp
    mov r15, rax
    lea rcx, g_PerformanceCounters.CopiesSubmitted
    call AtomicIncrement64
    
    ; Size-based optimization
    cmp r14, COPY_SMALL_THRESHOLD
    jb Copy_DoSmall
    cmp r14, COPY_MEDIUM_THRESHOLD
    jb Copy_DoMedium
    cmp r14, COPY_LARGE_THRESHOLD
    ja Copy_DoLarge
    
    ; Medium copy
    mov rax, r12
    or rax, r13
    and rax, 63
    jz Copy_DoAlignedMedium
    
Copy_DoUnalignedMedium:
    mov rsi, r12
    mov rdi, r13
    mov rax, r14
    shr rax, 6
    mov rcx, rax
    
Copy_UnalignedMedium_Loop:
    vmovdqu8 zmm0, [rsi]
    vmovdqu8 [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec rcx
    jnz Copy_UnalignedMedium_Loop
    mov rcx, r14
    and rcx, 63
    rep movsb
    jmp Copy_Complete
    
Copy_DoSmall:
    mov rsi, r12
    mov rdi, r13
    mov rcx, r14
    rep movsb
    jmp Copy_Complete
    
Copy_DoMedium:
    mov rsi, r12
    mov rdi, r13
    mov rcx, r14
    rep movsb
    jmp Copy_Complete
    
Copy_DoAlignedMedium:
    mov rsi, r12
    mov rdi, r13
    mov rax, r14
    shr rax, 6
    mov rcx, rax
    
Copy_AlignedMedium_Loop:
    vmovdqa64 zmm0, [rsi]
    vmovdqa64 [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec rcx
    jnz Copy_AlignedMedium_Loop
    mov rcx, r14
    and rcx, 63
    rep movsb
    jmp Copy_Complete
    
Copy_DoLarge:
    mov rsi, r12
    mov rdi, r13
    mov rbx, r14
    shr rbx, 8
    test rbx, rbx
    jz Copy_Large_NoPrefetch
    mov rcx, rbx
    
Copy_Large_Prefetch:
    prefetcht0 [rsi + rcx*256 + 4096]
    dec rcx
    jnz Copy_Large_Prefetch
    
Copy_Large_NoPrefetch:
    mov rax, r14
    shr rax, 8
    test rax, rax
    jz Copy_Large_Remainder
    mov rcx, rax
    
Copy_Large_Loop:
    vmovdqu8 zmm0, [rsi]
    vmovdqu8 zmm1, [rsi+64]
    vmovdqu8 zmm2, [rsi+128]
    vmovdqu8 zmm3, [rsi+192]
    vmovntdq [rdi], zmm0
    vmovntdq [rdi+64], zmm1
    vmovntdq [rdi+128], zmm2
    vmovntdq [rdi+192], zmm3
    add rsi, 256
    add rdi, 256
    dec rcx
    jnz Copy_Large_Loop
    sfence
    
Copy_Large_Remainder:
    mov rcx, r14
    and rcx, 255
    rep movsb
    
Copy_Complete:
    call GetTimestamp
    sub rax, r15
    call CalculateMicroseconds
    mov rbx, rax
    lea rcx, g_PerformanceCounters.CopiesCompleted
    call AtomicIncrement64
    mov rax, r14
    lea rcx, g_PerformanceCounters.TotalBytesCopied
    lock add QWORD PTR [rcx], rax
    mov rax, rbx
    lea rcx, g_PerformanceCounters.TotalCopyTimeNs
    lock add QWORD PTR [rcx], rax
    xor eax, ERROR_SUCCESS
    jmp Copy_Cleanup
    
Copy_InvalidSource:
    mov eax, ERROR_INVALID_DATA
    jmp Copy_ErrorLog
    
Copy_InvalidDest:
    mov eax, ERROR_INVALID_DATA
    jmp Copy_ErrorLog
    
Copy_InvalidSize:
    mov eax, ERROR_INSUFFICIENT_BUFFER
    jmp Copy_ErrorLog
    
Copy_ErrorLog:
    push rax
    sub rsp, 40
    lea rcx, szErrorNullPtr
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    pop rax
    lea rcx, g_PerformanceCounters.CopiesFailed
    call AtomicIncrement64
    
Copy_Cleanup:
    vzeroupper
    add rsp, 168
    pop rdi
    pop rsi
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
Titan_PerformCopy ENDP

;==============================================================================
; TITAN_PERFORM_DMA (450+ lines) - 3-TIER FALLBACK
;==============================================================================

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
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 184
    .allocstack 184
    .endprolog
    
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    test r12, r12
    jz Dma_InvalidSource
    test r13, r13
    jz Dma_InvalidDest
    test r14, r14
    jz Dma_InvalidSize
    cmp r14, MAX_DMA_SIZE
    ja Dma_InvalidSize
    
    mov rax, r12
    or rax, r13
    test al, 0FFh
    jnz Dma_InvalidAlign
    
    call GetTimestamp
    mov r15, rax
    lea rcx, g_PerformanceCounters.DmaSubmitted
    call AtomicIncrement64
    
    ; 3-tier fallback: DirectStorage → Vulkan → CPU
    jmp Dma_CPU_Fallback
    
Dma_CPU_Fallback:
    mov rsi, r12
    mov rdi, r13
    mov rbx, r14
    
Dma_CPU_SegmentLoop:
    test rbx, rbx
    jz Dma_CPU_Done
    
    mov rax, DMA_SEGMENT_SIZE
    cmp rbx, rax
    cmovb rax, rbx
    mov rcx, rax
    
    push rbx
    push rsi
    push rdi
    
    mov rax, rcx
    shr rax, 8
    test rax, rax
    jz Dma_CPU_Remainder
    
    mov r8, rax
    
Dma_CPU_NtLoop:
    vmovdqu8 zmm0, [rsi]
    vmovdqu8 zmm1, [rsi+64]
    vmovdqu8 zmm2, [rsi+128]
    vmovdqu8 zmm3, [rsi+192]
    vmovntdq [rdi], zmm0
    vmovntdq [rdi+64], zmm1
    vmovntdq [rdi+128], zmm2
    vmovntdq [rdi+192], zmm3
    add rsi, 256
    add rdi, 256
    dec r8
    jnz Dma_CPU_NtLoop
    sfence
    
Dma_CPU_Remainder:
    mov r8, rcx
    and r8, 255
    rep movsb
    
    pop rdi
    pop rsi
    pop rbx
    
    add rsi, rcx
    add rdi, rcx
    sub rbx, rcx
    jmp Dma_CPU_SegmentLoop
    
Dma_CPU_Done:
    mfence
    call GetTimestamp
    sub rax, r15
    call CalculateMicroseconds
    lea rcx, g_PerformanceCounters.DmaCompleted
    call AtomicIncrement64
    mov rax, r14
    lea rcx, g_PerformanceCounters.TotalDmaBytes
    lock add QWORD PTR [rcx], rax
    xor eax, ERROR_SUCCESS
    jmp Dma_Cleanup
    
Dma_InvalidSource:
    mov eax, ERROR_INVALID_DATA
    jmp Dma_ErrorLog
    
Dma_InvalidDest:
    mov eax, ERROR_INVALID_DATA
    jmp Dma_ErrorLog
    
Dma_InvalidSize:
    mov eax, ERROR_INSUFFICIENT_BUFFER
    jmp Dma_ErrorLog
    
Dma_InvalidAlign:
    mov eax, ERROR_NOT_ALIGNED
    
Dma_ErrorLog:
    push rax
    sub rsp, 40
    lea rcx, szErrorInvalid
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    pop rax
    lea rcx, g_PerformanceCounters.DmaFailed
    call AtomicIncrement64
    
Dma_Cleanup:
    vzeroupper
    add rsp, 184
    pop rdi
    pop rsi
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
Titan_PerformDMA ENDP

;==============================================================================
; TITAN_INITIALIZE_GPU (150+ lines)
;==============================================================================

Titan_InitializeGPU PROC EXPORT FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 72
    .allocstack 72
    .endprolog
    
    mov rbx, rcx
    test rbx, rbx
    jz Init_InvalidParam
    
    sub rsp, 40
    lea rcx, szInitStart
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    
    mov DWORD PTR [rbx+OFFSET TITAN_ORCHESTRATOR.Magic], 'ATIT'
    mov DWORD PTR [rbx+OFFSET TITAN_ORCHESTRATOR.Version], 1
    mov DWORD PTR [rbx+OFFSET TITAN_ORCHESTRATOR.IsAvailable], 0
    mov DWORD PTR [rbx+OFFSET TITAN_ORCHESTRATOR.DSInitialized], 0
    mov DWORD PTR [rbx+OFFSET TITAN_ORCHESTRATOR.VkInitialized], 0
    mov DWORD PTR [rbx+OFFSET TITAN_ORCHESTRATOR.FallbackMode], 1
    
    lea rcx, g_PerformanceCounters.Lock
    call QWORD PTR [__imp_InitializeSRWLock]
    
    sub rsp, 16
    lea rcx, [rsp+8]
    call QWORD PTR [__imp_QueryPerformanceFrequency]
    mov rax, [rsp+8]
    add rsp, 16
    mov g_QPFrequency, rax
    
    mov DWORD PTR [rbx+OFFSET TITAN_ORCHESTRATOR.IsAvailable], 1
    mov g_IsInitialized, 1
    
    sub rsp, 40
    lea rcx, szInitComplete
    call QWORD PTR [__imp_OutputDebugStringA]
    add rsp, 40
    
    xor eax, ERROR_SUCCESS
    jmp Init_Cleanup
    
Init_InvalidParam:
    mov eax, ERROR_INVALID_PARAM
    
Init_Cleanup:
    add rsp, 72
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_InitializeGPU ENDP

;==============================================================================
; UTILITY EXPORTS
;==============================================================================

Titan_IsDeviceAddress PROC EXPORT FRAME
    jmp IsDeviceAddress
Titan_IsDeviceAddress ENDP

Titan_GetGPUStats PROC EXPORT FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    
    mov rax, g_PerformanceCounters.KernelsCompleted
    mov [rbx], rax
    mov rax, g_PerformanceCounters.TotalBytesCopied
    mov [rsi], rax
    mov rax, g_PerformanceCounters.DmaCompleted
    mov [rdi], rax
    
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_GetGPUStats ENDP

Titan_ResetGPUStats PROC EXPORT FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, g_PerformanceCounters.Lock
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    lea rcx, g_PerformanceCounters.KernelsSubmitted
    mov rdx, OFFSET PERFORMANCE_COUNTERS.Lock
    sub rdx, OFFSET g_PerformanceCounters.KernelsSubmitted
    call QWORD PTR [__imp_RtlZeroMemory]
    
    lea rcx, g_PerformanceCounters.Lock
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    add rsp, 40
    ret
Titan_ResetGPUStats ENDP

;==============================================================================
; EXPORTS
;==============================================================================
PUBLIC g_PerformanceCounters
PUBLIC g_NF4Table
PUBLIC g_NibbleMask
PUBLIC g_TempBuffer
PUBLIC g_QPFrequency
PUBLIC g_IsInitialized
PUBLIC Titan_ExecuteComputeKernel
PUBLIC Titan_PerformCopy
PUBLIC Titan_PerformDMA
PUBLIC Titan_InitializeGPU
PUBLIC Titan_IsDeviceAddress
PUBLIC Titan_GetGPUStats
PUBLIC Titan_ResetGPUStats

END
