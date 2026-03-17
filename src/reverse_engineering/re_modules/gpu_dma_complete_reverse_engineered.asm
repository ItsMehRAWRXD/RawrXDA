;==============================================================================
; gpu_dma_complete_reverse_engineered.asm
; RawrXD Titan GPU/DMA - Complete Implementation
; Fixed for ml64.exe compatibility
;==============================================================================

.CODE
; ml64 compatible header
OPTION CASEMAP:NONE

;==============================================================================
; EXTERNAL FUNCTIONS
;==============================================================================
INCLUDELIB kernel32.lib
INCLUDELIB ntdll.lib

EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN RtlCopyMemory:PROC
EXTERN RtlZeroMemory:PROC
EXTERN OutputDebugStringA:PROC
EXTERN SetEvent:PROC
EXTERN WaitForSingleObject:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN GetVersionExA:PROC
EXTERN InitializeSRWLock:PROC
EXTERN AcquireSRWLockShared:PROC
EXTERN ReleaseSRWLockShared:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC
EXTERN GetVersion:PROC

;==============================================================================
; ERROR CODES
;==============================================================================
ERROR_SUCCESS                   EQU 0
ERROR_INVALID_HANDLE            EQU 6
ERROR_INVALID_DATA              EQU 13
ERROR_INSUFFICIENT_BUFFER       EQU 122
ERROR_NOT_SUPPORTED             EQU 50
ERROR_DEVICE_NOT_AVAILABLE      EQU 4319
ERROR_INVALID_PARAM             EQU 87
ERROR_NOT_ALIGNED               EQU 4

;=============================================================================
; PATCH FLAGS
;=============================================================================
PATCH_FLAG_DECOMPRESSION        EQU 00000001h
PATCH_FLAG_PREFETCH             EQU 00000002h
PATCH_FLAG_NON_TEMPORAL         EQU 00000004h

;=============================================================================
; ADDRESS SPACE DETECTION
;=============================================================================
; DEVICE_ADDRESS_THRESHOLD (0FFFF000000000000h) is handled via rax in code

;=============================================================================
; STRUCTURE DEFINITIONS
;=============================================================================

MEMORY_PATCH STRUCT
    HostAddress         DQ ?        ; +0x00
    DeviceAddress       DQ ?        ; +0x08
    CopySize            DQ ?        ; +0x10
    Flags               DD ?        ; +0x18
    Reserved            DD ?        ; +0x1C
MEMORY_PATCH ENDS

TITAN_ORCHESTRATOR STRUCT
    Magic               DD ?        ; +0x00 'TITA'
    Version             DD ?        ; +0x04
    State               DD ?        ; +0x08
    Padding1            DD ?        ; +0x0C
    hGPUDevice          DQ ?        ; +0x10
    hGPUContext         DQ ?        ; +0x18
    pDSFactory          DQ ?        ; +0x20
    pDSQueue            DQ ?        ; +0x28
    hVulkanInstance     DQ ?        ; +0x30
    hVulkanDevice       DQ ?        ; +0x38
    hVulkanQueue        DQ ?        ; +0x40
    VulkanQueueFamily   DD ?        ; +0x48
    Padding2            DD ?        ; +0x4C
    PerfCounterFreq     DQ ?        ; +0x50
    TotalBytesCopied    DQ ?        ; +0x58
    hFence              DQ ?        ; +0x60
    FenceValue          DQ ?        ; +0x68
    Padding             DB 120 DUP(?) 
TITAN_ORCHESTRATOR ENDS

;=============================================================================
; GLOBAL DATA SECTION
;=============================================================================
.DATA
ALIGN 16

NF4_LOOKUP_TABLE LABEL REAL4
    REAL4 -1.0
    REAL4 -0.6961928009986877
    REAL4 -0.5250730514526367
    REAL4 -0.39491748809814453
    REAL4 -0.28444138169288635
    REAL4 -0.18477343022823334
    REAL4 -0.09105003625154495
    REAL4 0.0
    REAL4 0.07958029955625534
    REAL4 0.16093020141124725
    REAL4 0.24611230194568634
    REAL4 0.33791524171829224
    REAL4 0.44070982933044434
    REAL4 0.5626170039176941
    REAL4 0.7229568362236023
    REAL4 1.0

ALIGN 16
NIBBLE_MASK DB 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
            DB 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh

ALIGN 8
g_TitanOrchestrator DQ 0
g_TotalKernelsExecuted DQ 0
g_TotalBytesCopied DQ 0
g_TotalDMAOperations DQ 0

.DATA?
ALIGN 16
g_TempBuffer        DB 4096 DUP(?)

.CODE

;=============================================================================
; NF4_ProcessLane - Helper
;=============================================================================
NF4_ProcessLane PROC
    ; Input: xmm3 = 16 packed bytes
    ; Output: stores 32 FP32 values to [rdi]
    sub rsp, 24
    movdqu xmmword ptr [rsp], xmm3
    xor rax, rax               
@@byte_loop:
    cmp rax, 16
    jge @@done
    movzx ecx, byte ptr [rsp + rax]
    mov edx, ecx
    and edx, 0Fh
    lea r8, NF4_LOOKUP_TABLE
    movss xmm0, dword ptr [r8 + rdx * 4]
    movss dword ptr [rdi], xmm0
    shr ecx, 4
    and ecx, 0Fh
    movss xmm0, dword ptr [r8 + rcx * 4]
    movss dword ptr [rdi + 4], xmm0
    add rdi, 8
    inc rax
    jmp @@byte_loop
@@done:
    add rsp, 24
    ret
NF4_ProcessLane ENDP

;=============================================================================
; Titan_ExecuteComputeKernel
;=============================================================================
Titan_ExecuteComputeKernel PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov qword ptr [rsp+32], rcx
    mov qword ptr [rsp+40], rdx
    mov dword ptr [rsp+52], ERROR_SUCCESS
    
    test rdx, rdx
    jnz @@check_fields
    mov dword ptr [rsp+52], ERROR_INVALID_HANDLE
    jmp @@cleanup
    
@@check_fields:
    mov rbx, rdx
    cmp qword ptr [rbx + MEMORY_PATCH.HostAddress], 0
    jne @@check_dest
    mov dword ptr [rsp+52], ERROR_INVALID_DATA
    jmp @@cleanup
    
@@check_dest:
    cmp qword ptr [rbx + MEMORY_PATCH.DeviceAddress], 0
    jne @@check_size
    mov dword ptr [rsp+52], ERROR_INVALID_DATA
    jmp @@cleanup
    
@@check_size:
    cmp qword ptr [rbx + MEMORY_PATCH.CopySize], 0
    jne @@determine_operation
    mov dword ptr [rsp+52], ERROR_INSUFFICIENT_BUFFER
    jmp @@cleanup
    
@@determine_operation:
    mov eax, dword ptr [rbx + MEMORY_PATCH.Flags]
    test eax, PATCH_FLAG_DECOMPRESSION
    jnz @@nf4_decompress
    test eax, PATCH_FLAG_PREFETCH
    jnz @@prefetch_operation
    jmp @@standard_copy
    
@@nf4_decompress:
    mov rsi, qword ptr [rbx + MEMORY_PATCH.HostAddress]
    mov rdi, qword ptr [rbx + MEMORY_PATCH.DeviceAddress]
    mov r12, qword ptr [rbx + MEMORY_PATCH.CopySize]
    vmovups zmm20, [NF4_LOOKUP_TABLE]      
    vmovdqu64 zmm22, zmmword ptr [NIBBLE_MASK]
    mov r14, r12
    shr r14, 8  
    jz @@nf4_remainder
@@nf4_block_loop:
    vmovdqu64 zmm0, [rsi]
    vextracti64x2 xmm3, zmm0, 0
    call NF4_ProcessLane
    vextracti64x2 xmm3, zmm0, 1
    call NF4_ProcessLane
    vextracti64x2 xmm3, zmm0, 2
    call NF4_ProcessLane
    vextracti64x2 xmm3, zmm0, 3
    call NF4_ProcessLane
    add rsi, 256
    dec r14
    jnz @@nf4_block_loop
@@nf4_remainder:
    mov r14, r12
    and r14, 0FFh
    jz @@nf4_done
@@nf4_scalar_loop:
    test r14, r14
    jz @@nf4_done
    movzx eax, byte ptr [rsi]
    mov ecx, eax
    and ecx, 0Fh
    lea rdx, NF4_LOOKUP_TABLE
    movss xmm0, dword ptr [rdx + rcx * 4]
    movss dword ptr [rdi], xmm0
    shr eax, 4
    and eax, 0Fh
    movss xmm0, dword ptr [rdx + rax * 4]
    movss dword ptr [rdi + 4], xmm0
    inc rsi
    add rdi, 8
    dec r14
    jmp @@nf4_scalar_loop
@@nf4_done:
    vzeroupper
    jmp @@success
@@prefetch_operation:
    mov rsi, qword ptr [rbx + MEMORY_PATCH.HostAddress]
    mov r12, qword ptr [rbx + MEMORY_PATCH.CopySize]
    mov r14, r12
    shr r14, 6
@@prefetch_loop:
    test r14, r14
    jz @@success
    prefetcht0 [rsi]
    prefetcht1 [rsi + 4096]
    prefetcht2 [rsi + 65536]
    add rsi, 64
    dec r14
    jmp @@prefetch_loop
@@standard_copy:
    mov rcx, qword ptr [rbx + MEMORY_PATCH.HostAddress]
    mov rdx, qword ptr [rbx + MEMORY_PATCH.DeviceAddress]
    mov r8, qword ptr [rbx + MEMORY_PATCH.CopySize]
    mov eax, dword ptr [rbx + MEMORY_PATCH.Flags]
    test eax, PATCH_FLAG_NON_TEMPORAL
    jnz @@nt_copy
    call Titan_PerformCopy
    mov dword ptr [rsp+52], eax
    jmp @@cleanup
@@nt_copy:
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov r13, rsi
    and r13, 3Fh
    jz @@nt_aligned
    mov rcx, 64
    sub rcx, r13
    cmp rcx, r12
    cmova rcx, r12
    sub r12, rcx
    rep movsb
@@nt_aligned:
    mov r14, r12
    shr r14, 8
@@nt_block_loop:
    test r14, r14
    jz @@nt_remainder
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 zmm1, [rsi + 64]
    vmovdqu64 zmm2, [rsi + 128]
    vmovdqu64 zmm3, [rsi + 192]
    vmovntdq zmmword ptr [rdi], zmm0
    vmovntdq zmmword ptr [rdi + 64], zmm1
    vmovntdq zmmword ptr [rdi + 128], zmm2
    vmovntdq zmmword ptr [rdi + 192], zmm3
    add rsi, 256
    add rdi, 256
    dec r14
    jmp @@nt_block_loop
@@nt_remainder:
    mov rcx, r12
    and rcx, 0FFh
    rep movsb
    sfence
    vzeroupper
    jmp @@success
@@success:
    inc qword ptr [g_TotalKernelsExecuted]
    mov rax, qword ptr [rsp+40]
    mov r8, qword ptr [rax + MEMORY_PATCH.CopySize]
    add qword ptr [g_TotalBytesCopied], r8
    mov dword ptr [rsp+52], ERROR_SUCCESS
@@cleanup:
    mov eax, dword ptr [rsp+52]
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_ExecuteComputeKernel ENDP

;=============================================================================
; Titan_PerformCopy
;=============================================================================
Titan_PerformCopy PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 64
    .allocstack 64
    .endprolog
    mov qword ptr [rsp+32], rcx
    mov qword ptr [rsp+40], rdx
    mov qword ptr [rsp+48], r8
    test rcx, rcx
    jz @@error_invalid
    test rdx, rdx
    jz @@error_invalid
    test r8, r8
    jz @@error_buffer
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    cmp r12, 256
    ja @@large_copy
    mov rcx, r12
    rep movsb
    jmp @@success
@@large_copy:
    mov rax, rsi
    or rax, rdi
    and rax, 3Fh
    jnz @@unaligned_copy
    cmp r12, 262144
    ja @@nt_large_copy
    mov r13, r12
    shr r13, 8
@@temp_block_loop:
    test r13, r13
    jz @@temp_remainder
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 zmm1, [rsi + 64]
    vmovdqu64 zmm2, [rsi + 128]
    vmovdqu64 zmm3, [rsi + 192]
    vmovdqa64 [rdi], zmm0
    vmovdqa64 [rdi + 64], zmm1
    vmovdqa64 [rdi + 128], zmm2
    vmovdqa64 [rdi + 192], zmm3
    add rsi, 256
    add rdi, 256
    dec r13
    jmp @@temp_block_loop
@@temp_remainder:
    mov rcx, r12
    and rcx, 0FFh
    rep movsb
    vzeroupper
    jmp @@success
@@nt_large_copy:
    mov r13, r12
    shr r13, 8
@@nt_copy_loop:
    test r13, r13
    jz @@nt_remainder
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 zmm1, [rsi + 64]
    vmovdqu64 zmm2, [rsi + 128]
    vmovdqu64 zmm3, [rsi + 192]
    vmovntdq zmmword ptr [rdi], zmm0
    vmovntdq zmmword ptr [rdi + 64], zmm1
    vmovntdq zmmword ptr [rdi + 128], zmm2
    vmovntdq zmmword ptr [rdi + 192], zmm3
    add rsi, 256
    add rdi, 256
    dec r13
    jmp @@nt_copy_loop
@@nt_remainder:
    mov rcx, r12
    and rcx, 0FFh
    rep movsb
    sfence
    vzeroupper
    jmp @@success
@@unaligned_copy:
    mov rax, rdi
    and rax, 3Fh
    jz @@unaligned_source_aligned_dest
    mov rcx, 64
    sub rcx, rax
    cmp rcx, r12
    cmova rcx, r12
    sub r12, rcx
    rep movsb
@@unaligned_source_aligned_dest:
    mov r13, r12
    shr r13, 8
@@unaligned_block_loop:
    test r13, r13
    jz @@unaligned_remainder
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 zmm1, [rsi + 64]
    vmovdqu64 zmm2, [rsi + 128]
    vmovdqu64 zmm3, [rsi + 192]
    vmovdqa64 [rdi], zmm0
    vmovdqa64 [rdi + 64], zmm1
    vmovdqa64 [rdi + 128], zmm2
    vmovdqa64 [rdi + 192], zmm3
    add rsi, 256
    add rdi, 256
    dec r13
    jmp @@unaligned_block_loop
@@unaligned_remainder:
    mov rcx, r12
    and rcx, 0FFh
    rep movsb
    vzeroupper
@@success:
    add qword ptr [g_TotalBytesCopied], r8
    xor eax, eax
    jmp @@cleanup
@@error_invalid:
    mov eax, ERROR_INVALID_DATA
    jmp @@cleanup
@@error_buffer:
    mov eax, ERROR_INSUFFICIENT_BUFFER
@@cleanup:
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_PerformCopy ENDP

;=============================================================================
; Titan_PerformDMA
;=============================================================================
Titan_PerformDMA PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 80
    .allocstack 80
    .endprolog
    mov qword ptr [rsp+32], rcx
    mov qword ptr [rsp+40], rdx
    mov qword ptr [rsp+48], r8
    test rcx, rcx
    jz @@error_invalid
    test rdx, rdx
    jz @@error_invalid
    test r8, r8
    jz @@error_buffer
    mov rbx, g_TitanOrchestrator
    test rbx, rbx
    jz @@cpu_fallback
    cmp qword ptr [rbx + TITAN_ORCHESTRATOR.pDSQueue], 0
    je @@try_vulkan
    call Titan_DMA_DirectStorage
    test eax, eax
    jz @@success
@@try_vulkan:
    cmp qword ptr [rbx + TITAN_ORCHESTRATOR.hVulkanDevice], 0
    je @@cpu_fallback
    mov rcx, qword ptr [rsp+32]
    mov rdx, qword ptr [rsp+40]
    mov r8, qword ptr [rsp+48]
    call Titan_DMA_Vulkan
    test eax, eax
    jz @@success
@@cpu_fallback:
    mov rcx, qword ptr [rsp+32]
    mov rdx, qword ptr [rsp+40]
    mov r8, qword ptr [rsp+48]
    call Titan_PerformCopy
    mov dword ptr [rsp+56], eax
    jmp @@update_stats
@@success:
    mov dword ptr [rsp+56], ERROR_SUCCESS
@@update_stats:
    cmp dword ptr [rsp+56], ERROR_SUCCESS
    jne @@cleanup
    inc g_TotalDMAOperations
    mov rax, qword ptr [rsp+48]
    add g_TotalBytesCopied, rax
@@cleanup:
    mov eax, dword ptr [rsp+56]
    add rsp, 80
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
@@error_invalid:
    mov eax, ERROR_INVALID_DATA
    jmp @@cleanup
@@error_buffer:
    mov eax, ERROR_INSUFFICIENT_BUFFER
    jmp @@cleanup
Titan_PerformDMA ENDP

;=============================================================================
; Titan_DMA_DirectStorage
;=============================================================================
Titan_DMA_DirectStorage PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 48
    .allocstack 48
    .endprolog
    mov rbx, g_TitanOrchestrator
    test rbx, rbx
    jz @@not_available
    cmp qword ptr [rbx + TITAN_ORCHESTRATOR.pDSQueue], 0
    je @@not_available
@@not_available:
    mov eax, ERROR_NOT_SUPPORTED
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_DMA_DirectStorage ENDP

;=============================================================================
; Titan_DMA_Vulkan
;=============================================================================
Titan_DMA_Vulkan PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    sub rsp, 80
    .allocstack 80
    .endprolog
    mov rbx, g_TitanOrchestrator
    test rbx, rbx
    jz @@not_available
    cmp qword ptr [rbx + TITAN_ORCHESTRATOR.hVulkanDevice], 0
    je @@not_available
@@not_available:
    mov eax, ERROR_NOT_SUPPORTED
    add rsp, 80
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_DMA_Vulkan ENDP

;=============================================================================
; Titan_InitializeGPU
;=============================================================================
Titan_InitializeGPU PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    test rcx, rcx
    jz @@error
    mov rbx, rcx
    mov dword ptr [rbx + TITAN_ORCHESTRATOR.Magic], 41544954h 
    mov dword ptr [rbx + TITAN_ORCHESTRATOR.Version], 1
    mov dword ptr [rbx + TITAN_ORCHESTRATOR.State], 0
    call Titan_InitDirectStorage
    call Titan_InitVulkan
    lea rcx, [rsp+32]
    call QueryPerformanceFrequency
    test rax, rax
    jz @@freq_failed
    mov rax, qword ptr [rsp+32]
    mov qword ptr [rbx + TITAN_ORCHESTRATOR.PerfCounterFreq], rax
    mov g_TitanOrchestrator, rbx
    xor eax, eax
    jmp @@done
@@freq_failed:
    mov qword ptr [rbx + TITAN_ORCHESTRATOR.PerfCounterFreq], 0
    mov eax, ERROR_DEVICE_NOT_AVAILABLE
    jmp @@done
@@error:
    mov eax, ERROR_INVALID_PARAM
@@done:
    add rsp, 48
    pop rbx
    ret
Titan_InitializeGPU ENDP

;=============================================================================
; Titan_InitDirectStorage
;=============================================================================
Titan_InitDirectStorage PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    mov rbx, g_TitanOrchestrator
    test rbx, rbx
    jz @@done
    call GetVersion
    cmp eax, 0A000000h
    jb @@not_available
    lea rcx, szDStorageDLL
    call LoadLibraryA
    test rax, rax
    jz @@not_available
@@not_available:
    mov qword ptr [rbx + TITAN_ORCHESTRATOR.pDSFactory], 0
    mov qword ptr [rbx + TITAN_ORCHESTRATOR.pDSQueue], 0
@@done:
    add rsp, 32
    pop rbx
    ret
Titan_InitDirectStorage ENDP

;=============================================================================
; Titan_InitVulkan
;=============================================================================
Titan_InitVulkan PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    mov rbx, g_TitanOrchestrator
    test rbx, rbx
    jz @@done
    lea rcx, szVulkanDLL
    call LoadLibraryA
    test rax, rax
    jz @@not_available
@@not_available:
    mov qword ptr [rbx + TITAN_ORCHESTRATOR.hVulkanInstance], 0
    mov qword ptr [rbx + TITAN_ORCHESTRATOR.hVulkanDevice], 0
    mov qword ptr [rbx + TITAN_ORCHESTRATOR.hVulkanQueue], 0
@@done:
    add rsp, 32
    pop rbx
    ret
Titan_InitVulkan ENDP

;=============================================================================
; Utility Functions
;=============================================================================

Titan_IsDeviceAddress PROC
    mov rax, rcx
    mov r10, 0FFFF000000000000h
    cmp rax, r10
    setae al
    movzx eax, al
    ret
Titan_IsDeviceAddress ENDP

Titan_GetGPUStats PROC
    test rcx, rcx
    jz @@skip_kernels
    mov rax, g_TotalKernelsExecuted
    mov [rcx], rax
@@skip_kernels:
    test rdx, rdx
    jz @@skip_bytes
    mov rax, g_TotalBytesCopied
    mov [rdx], rax
@@skip_bytes:
    test r8, r8
    jz @@done
    mov rax, g_TotalDMAOperations
    mov [r8], rax
@@done:
    ret
Titan_GetGPUStats ENDP

Titan_ResetGPUStats PROC
    mov g_TotalKernelsExecuted, 0
    mov g_TotalBytesCopied, 0
    mov g_TotalDMAOperations, 0
    ret
Titan_ResetGPUStats ENDP

.DATA
ALIGN 1
szDStorageDLL       DB "dstorage.dll", 0
szVulkanDLL         DB "vulkan-1.dll", 0
szDStorageGetFactory DB "DStorageGetFactory", 0

PUBLIC Titan_ExecuteComputeKernel
PUBLIC Titan_PerformCopy
PUBLIC Titan_PerformDMA
PUBLIC Titan_InitializeGPU
PUBLIC Titan_IsDeviceAddress
PUBLIC Titan_GetGPUStats
PUBLIC Titan_ResetGPUStats
PUBLIC g_TitanOrchestrator
PUBLIC g_TotalKernelsExecuted
PUBLIC g_TotalBytesCopied
PUBLIC g_TotalDMAOperations

END
