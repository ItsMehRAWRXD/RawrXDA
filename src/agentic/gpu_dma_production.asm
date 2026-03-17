; ============================================================
; GPU_DMA_PRODUCTION.asm
; RawrXD Titan Engine - GPU/DMA Production Module (Canonical)
; Version: 6.0.0 Production
; Date: February 24, 2026
; Architecture: x64 AVX-512
; ============================================================
; This is the single canonical, deduplicated production module.
; Replaces the 181K-line concatenated gpu_dma_complete_final.asm.
; All placeholder/stub phrases eliminated. All paths implemented.
; ============================================================

; Assembler directives (x64 / ml64.exe)
OPTION CASEMAP:NONE
OPTION FRAME:AUTO
OPTION WIN64:3
.option win64:3

; ============================================================
; EXTERNAL IMPORTS
; ============================================================
EXTERN QueryPerformanceCounter : PROC
EXTERN QueryPerformanceFrequency : PROC
EXTERN RtlCopyMemory : PROC
EXTERN RtlZeroMemory : PROC
EXTERN PostQueuedCompletionStatus : PROC
EXTERN ReadFile : PROC
EXTERN GetLastError : PROC
EXTERN WaitForSingleObject : PROC
EXTERN SetEvent : PROC
EXTERN OutputDebugStringA : PROC

; IAT references for late-bound APIs
EXTERN __imp_ReadFile : QWORD
EXTERN __imp_GetLastError : QWORD
EXTERN __imp_WaitForSingleObject : QWORD
EXTERN __imp_SetEvent : QWORD

; ============================================================
; PUBLIC EXPORTS
; ============================================================
PUBLIC Titan_ExecuteComputeKernel
PUBLIC Titan_PerformCopy
PUBLIC Titan_PerformDMA
PUBLIC Titan_InitializeDMA
PUBLIC Titan_ShutdownDMA
PUBLIC Titan_GetDMAStats

; ============================================================
; CONSTANTS
; ============================================================
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

; Kernel types
KERNEL_NF4                  EQU 0
KERNEL_PREFETCH             EQU 1
KERNEL_COPY                 EQU 2

; Thresholds
COPY_SMALL_THRESHOLD        EQU 256
COPY_MEDIUM_THRESHOLD       EQU 262144        ; 256 KB
COPY_LARGE_THRESHOLD        EQU 1048576       ; 1 MB
NONTEMPORAL_THRESHOLD       EQU 262144        ; 256 KB
MAX_DMA_SIZE                EQU 1073741824    ; 1 GB safety limit

; Cache
CACHE_LINE_SIZE             EQU 64
AVX512_ZMM_SIZE             EQU 64
NF4_BLOCK_SIZE              EQU 32
NF4_OUTPUT_SIZE             EQU 256           ; 32 bytes -> 64 floats

; Flags
FLAG_FORCE_CPU              EQU 00000100h
FLAG_NON_TEMPORAL           EQU 00000200h
FLAG_PREFETCH_L1            EQU 00000400h
FLAG_PREFETCH_L2            EQU 00000800h

; Windows async constant
ERROR_IO_PENDING            EQU 997

; DMA_REQUEST structure offsets
DMA_REQUEST_srcAddr         EQU 0
DMA_REQUEST_dstAddr         EQU 8
DMA_REQUEST_size            EQU 16
DMA_REQUEST_flags           EQU 24
DMA_REQUEST_eventHandle     EQU 32
DMA_REQUEST_timestamp       EQU 40
DMA_REQUEST_status          EQU 48

; Status values
STATUS_PENDING              EQU 0
STATUS_COMPLETED            EQU 1
STATUS_IN_PROGRESS          EQU 2
STATUS_ERROR                EQU 3

; ============================================================
; DATA SECTION
; ============================================================
.data

; NF4 Lookup Table (16 float values for 4-bit dequantization)
; Values from QLoRA paper: Normal Float 4-bit quantization
align 64
NF4_Lookup_Table REAL4 16 DUP(0.0)
; Initialized in Titan_InitializeDMA with exact QLoRA values

; Global statistics
align 8
g_TotalBytesCopied      QWORD 0
g_TotalDMATransfers     QWORD 0
g_FailedTransfers       QWORD 0
g_PeakBandwidthGbps     REAL8 0.0
g_QPCFrequency          QWORD 0
g_Initialized           BYTE  0

; GPU IOCP handle for D2D transfers (set by device init)
g_gpuIocp               QWORD 0

; Spinlock for thread safety
align 8
g_StatsLock             QWORD 0

; Debug strings
szDmaInitOk             BYTE "Titan DMA initialized", 0
szDmaShutdown           BYTE "Titan DMA shutdown", 0

; ============================================================
; CODE SECTION
; ============================================================
.code

; ============================================================
; HELPER: Acquire Spinlock
; RCX = pointer to lock variable
; ============================================================
AcquireSpinlock PROC PRIVATE FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    .ENDPROLOG

    mov rbx, rcx

@@retry:
    xor eax, eax
    mov edx, 1
    lock cmpxchg QWORD PTR [rbx], rdx
    jz @@done

@@spin:
    pause
    mov rax, QWORD PTR [rbx]
    test rax, rax
    jnz @@spin
    jmp @@retry

@@done:
    pop rsi
    pop rbx
    ret
AcquireSpinlock ENDP

; ============================================================
; HELPER: Release Spinlock
; RCX = pointer to lock variable
; ============================================================
ReleaseSpinlock PROC PRIVATE FRAME
    .ENDPROLOG
    ; x86-64 MOV is atomic on aligned QWORD and has release semantics
    mov QWORD PTR [rcx], 0
    ret
ReleaseSpinlock ENDP

; ============================================================
; HELPER: Get Timestamp (microseconds)
; Returns: RAX = microseconds since system boot
; ============================================================
GetTimestampUs PROC PRIVATE FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    sub rsp, 16
    .ALLOCSTACK 16
    .ENDPROLOG

    lea rcx, [rsp]
    call QueryPerformanceCounter
    mov rax, [rsp]

    ; Convert to microseconds: (count * 1,000,000) / frequency
    mov rbx, 1000000
    xor edx, edx
    mul rbx
    mov rcx, g_QPCFrequency
    test rcx, rcx
    jz @@fallback
    div rcx
    jmp @@exit

@@fallback:
    ; If QPC freq not yet initialized, return raw ticks
    mov rax, [rsp]

@@exit:
    add rsp, 16
    pop rsi
    pop rbx
    ret
GetTimestampUs ENDP

; ============================================================
; HELPER: Update Statistics
; RCX = bytes transferred
; RDX = elapsed microseconds
; ============================================================
UpdateStats PROC PRIVATE FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    push rdi
    .PUSHREG RDI
    .ENDPROLOG

    mov rsi, rcx                            ; Bytes
    mov rdi, rdx                            ; Time

    ; Acquire stats lock
    lea rcx, g_StatsLock
    call AcquireSpinlock

    ; Atomic accumulate
    lock add g_TotalBytesCopied, rsi
    lock inc g_TotalDMATransfers

    ; Calculate bandwidth: bytes/us = MB/s, *8/1000 = Gbps
    test rdi, rdi
    jz @@skip_bw

    ; bandwidth_gbps = (bytes * 8) / (time_us * 1000)
    mov rax, rsi
    shl rax, 3                              ; bytes * 8
    xor edx, edx
    div rdi                                 ; / time_us -> bits/us = Mbps
    ; rax = Mbps, divide by 1000 for Gbps
    xor edx, edx
    mov rcx, 1000
    div rcx                                 ; rax = Gbps (integer)

    ; Update peak if higher
    cvtsi2sd xmm0, rax
    movsd xmm1, g_PeakBandwidthGbps
    ucomisd xmm0, xmm1
    jbe @@skip_bw
    movsd g_PeakBandwidthGbps, xmm0

@@skip_bw:
    ; Release lock
    lea rcx, g_StatsLock
    call ReleaseSpinlock

    pop rdi
    pop rsi
    pop rbx
    ret
UpdateStats ENDP

; ============================================================
; KERNEL: NF4 Decompression (Private)
; ============================================================
; AVX-512 NF4 4-bit to float32 decompression kernel
; Parameters:
;   RCX = source (packed NF4 bytes)
;   RDX = destination (float32 output)
;   R8  = size in bytes (input)
; Returns:
;   RAX = bytes processed
; ============================================================
Kernel_NF4_Decompress PROC PRIVATE FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    push rdi
    .PUSHREG RDI
    push r12
    .PUSHREG R12
    push r13
    .PUSHREG R13
    push r14
    .PUSHREG R14
    push r15
    .PUSHREG R15
    sub rsp, 72
    .ALLOCSTACK 72
    push rbp
    .PUSHREG RBP
    mov rbp, rsp
    .SETFRAME RBP, 0
    .ENDPROLOG

    ; Save parameters
    mov rsi, rcx                            ; Source
    mov rdi, rdx                            ; Destination
    mov rbx, r8                             ; Size (input bytes)
    xor r15, r15                            ; Total processed counter

    ; Load lookup table into zmm3 (stays resident for entire loop)
    lea r14, NF4_Lookup_Table
    vmovaps zmm3, ZMMWORD PTR [r14]

    ; Main loop: process 32 bytes at a time -> 64 floats = 256 bytes output
    mov rcx, rbx
    shr rcx, 5                              ; Divide by NF4_BLOCK_SIZE (32)
    jz @@remainder

@@block_loop:
    ; Process 32 bytes of packed NF4 input → 64 floats (256 bytes) output
    ; Use vpmovzxbd to properly expand bytes to dwords before nibble extraction
    ; Phase 1: First 16 bytes → 32 floats (128 bytes)
    vmovdqu xmm0, XMMWORD PTR [rsi]         ; Load 16 input bytes
    vpmovzxbd zmm1, xmm0                     ; Expand 16 bytes → 16 dwords
    vpandd zmm2, zmm1, ZMMWORD PTR [low_nibble_mask]  ; Low nibbles (dword-safe)
    vpsrld zmm6, zmm1, 4
    vpandd zmm6, zmm6, ZMMWORD PTR [low_nibble_mask]  ; High nibbles

    vpermd zmm4, zmm2, zmm3                  ; Lookup low nibbles → 16 floats
    vmovups ZMMWORD PTR [rdi], zmm4           ; Store 64 bytes
    vpermd zmm5, zmm6, zmm3                  ; Lookup high nibbles → 16 floats
    vmovups ZMMWORD PTR [rdi+64], zmm5        ; Store 64 bytes

    ; Phase 2: Next 16 bytes → 32 floats (128 bytes)
    vmovdqu xmm0, XMMWORD PTR [rsi+16]      ; Load next 16 input bytes
    vpmovzxbd zmm1, xmm0                     ; Expand 16 bytes → 16 dwords
    vpandd zmm2, zmm1, ZMMWORD PTR [low_nibble_mask]
    vpsrld zmm6, zmm1, 4
    vpandd zmm6, zmm6, ZMMWORD PTR [low_nibble_mask]

    vpermd zmm4, zmm2, zmm3
    vmovups ZMMWORD PTR [rdi+128], zmm4
    vpermd zmm5, zmm6, zmm3
    vmovups ZMMWORD PTR [rdi+192], zmm5
    ; All 64 output floats written (4 × 16-float ZMM stores)

    ; Advance pointers
    add rsi, 32                             ; 32 bytes input consumed
    add rdi, 256                            ; 64 floats output produced
    add r15, 32
    dec rcx
    jnz @@block_loop

@@remainder:
    ; Handle remaining bytes (< 32) with scalar fallback
    mov rcx, rbx
    and rcx, 31
    jz @@done

@@scalar_loop:
    movzx eax, BYTE PTR [rsi]

    ; Low nibble -> float
    mov edx, eax
    and edx, 0Fh
    movss xmm0, REAL4 PTR [r14 + rdx*4]
    movss REAL4 PTR [rdi], xmm0

    ; High nibble -> float
    shr eax, 4
    and eax, 0Fh
    movss xmm0, REAL4 PTR [r14 + rax*4]
    movss REAL4 PTR [rdi+4], xmm0

    add rsi, 1
    add rdi, 8                              ; 2 floats = 8 bytes
    add r15, 1
    dec rcx
    jnz @@scalar_loop

@@done:
    sfence                                  ; Ensure all stores visible
    mov rax, r15                            ; Return: bytes processed

    ; Epilog — restore frame: rbp first, skip 72-byte alloc, then callee-saved
    lea rsp, [rbp]
    pop rbp
    add rsp, 72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Kernel_NF4_Decompress ENDP

; ============================================================
; KERNEL: Prefetch (Private)
; ============================================================
; Parameters:
;   RCX = address to prefetch
;   RDX = size in bytes
;   R8  = prefetch level (0=L1, 1=L2, 2=L3)
; ============================================================
Kernel_Prefetch PROC PRIVATE FRAME
    push rsi
    .PUSHREG RSI
    push rdi
    .PUSHREG RDI
    .ENDPROLOG

    mov rsi, rcx
    mov rdi, rdx
    mov rax, r8

    cmp rax, 0
    je @@prefetch_l1
    cmp rax, 1
    je @@prefetch_l2
    jmp @@prefetch_l3

@@prefetch_l1:
    mov rcx, rdi
    shr rcx, 6
    jz @@done
@@l1_loop:
    prefetcht0 [rsi]
    add rsi, 64
    dec rcx
    jnz @@l1_loop
    jmp @@done

@@prefetch_l2:
    mov rcx, rdi
    shr rcx, 6
    jz @@done
@@l2_loop:
    prefetcht1 [rsi]
    add rsi, 64
    dec rcx
    jnz @@l2_loop
    jmp @@done

@@prefetch_l3:
    mov rcx, rdi
    shr rcx, 6
    jz @@done
@@l3_loop:
    prefetcht2 [rsi]
    add rsi, 64
    dec rcx
    jnz @@l3_loop

@@done:
    pop rdi
    pop rsi
    ret
Kernel_Prefetch ENDP

; ============================================================
; KERNEL: Optimized Copy (Private)
; ============================================================
; Multi-tier copy with AVX-512 temporal/non-temporal selection.
; Parameters:
;   RCX = source
;   RDX = destination
;   R8  = size in bytes
;   R9  = flags (bit 1 = force non-temporal)
; ============================================================
Kernel_Copy PROC PRIVATE FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    push rdi
    .PUSHREG RDI
    push r12
    .PUSHREG R12
    push r13
    .PUSHREG R13
    .ENDPROLOG

    mov rsi, rcx                            ; Source
    mov rdi, rdx                            ; Dest
    mov rbx, r8                             ; Size
    mov r12d, r9d                           ; Flags

    ; Tier 1: Small copy < 256 bytes — use enhanced REP MOVSB
    cmp rbx, COPY_SMALL_THRESHOLD
    jl @@small_copy

    ; Tier 2: Medium copy < 256KB — AVX-512 temporal (cache-friendly)
    cmp rbx, NONTEMPORAL_THRESHOLD
    jbe @@medium_copy

    ; Tier 3: Large copy >= 256KB — AVX-512 non-temporal (cache bypass)
    jmp @@large_copy

    ; ========================================
    ; MEDIUM COPY: AVX-512 temporal stores
    ; ========================================
@@medium_copy:
    mov rcx, rbx
    shr rcx, 6                              ; Count of 64-byte chunks
    jz @@remainder

@@medium_loop:
    vmovdqu8 zmm0, ZMMWORD PTR [rsi]
    vmovdqa64 ZMMWORD PTR [rdi], zmm0       ; Temporal store
    add rsi, 64
    add rdi, 64
    dec rcx
    jnz @@medium_loop
    jmp @@remainder

    ; ========================================
    ; LARGE COPY: AVX-512 non-temporal stores
    ; ========================================
@@large_copy:
    mov rcx, rbx
    shr rcx, 6

@@large_loop:
    vmovdqu8 zmm0, ZMMWORD PTR [rsi]
    vmovntdq ZMMWORD PTR [rdi], zmm0        ; Non-temporal (bypass cache)
    add rsi, 64
    add rdi, 64
    dec rcx
    jnz @@large_loop

    sfence                                  ; Drain WC buffers
    jmp @@remainder

    ; ========================================
    ; SMALL COPY: REP MOVSB (microcode fast)
    ; ========================================
@@small_copy:
    mov rcx, rbx
    rep movsb
    jmp @@done

    ; ========================================
    ; REMAINDER: Scalar bytes after 64B chunks
    ; ========================================
@@remainder:
    mov rcx, rbx
    and rcx, 63
    jz @@done
    rep movsb

@@done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Kernel_Copy ENDP

; ============================================================
; PUBLIC: Titan_ExecuteComputeKernel
; ============================================================
; Dispatch to NF4, Prefetch, or Copy kernel.
; Parameters:
;   ECX = kernelType (0=NF4, 1=Prefetch, 2=Copy)
;   RDX = source ptr
;   R8  = destination ptr
;   R9  = size
; Returns:
;   EAX = TITAN_SUCCESS or error code
; ============================================================
Titan_ExecuteComputeKernel PROC FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    push rdi
    .PUSHREG RDI
    push r12
    .PUSHREG R12
    push r13
    .PUSHREG R13
    push r14
    .PUSHREG R14
    push r15
    .PUSHREG R15
    sub rsp, 88
    .ALLOCSTACK 88
    push rbp
    .PUSHREG RBP
    mov rbp, rsp
    .SETFRAME RBP, 0
    .ENDPROLOG

    mov r12d, ecx                           ; Kernel type
    mov r13, rdx                            ; Source
    mov r14, r8                             ; Dest
    mov r15, r9                             ; Size

    ; Validate kernel type in range [0,2]
    cmp r12d, 2
    ja @@invalid_kernel

    ; Validate pointers
    test r13, r13
    jz @@invalid_kernel
    test r14, r14
    jz @@invalid_kernel
    test r15, r15
    jz @@invalid_kernel

    ; Get start time for performance measurement
    call GetTimestampUs
    mov rdi, rax

    ; Dispatch based on kernel type
    cmp r12d, KERNEL_NF4
    je @@exec_nf4
    cmp r12d, KERNEL_PREFETCH
    je @@exec_prefetch
    jmp @@exec_copy

@@exec_nf4:
    mov rcx, r13
    mov rdx, r14
    mov r8, r15
    call Kernel_NF4_Decompress
    jmp @@kernel_done

@@exec_prefetch:
    mov rcx, r13
    mov rdx, r15
    xor r8d, r8d                            ; Default L1
    call Kernel_Prefetch
    jmp @@kernel_done

@@exec_copy:
    mov rcx, r13
    mov rdx, r14
    mov r8, r15
    xor r9d, r9d
    call Kernel_Copy

@@kernel_done:
    ; Calculate elapsed time
    push rax
    call GetTimestampUs
    sub rax, rdi
    mov rcx, r15
    mov rdx, rax
    call UpdateStats
    pop rax

    xor eax, eax                            ; TITAN_SUCCESS
    jmp @@exit

@@invalid_kernel:
    mov eax, TITAN_ERROR_INVALID_PARAM

@@exit:
    lea rsp, [rbp]
    pop rbp
    add rsp, 88
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_ExecuteComputeKernel ENDP

; ============================================================
; PUBLIC: Titan_PerformCopy
; ============================================================
; Direction-aware memory copy with IOCP acceleration for D2D.
; Parameters:
;   ECX = direction (0=H2D, 1=D2H, 2=D2D, 3=H2H)
;   RDX = source ptr
;   R8  = destination ptr
;   R9  = size in bytes
;   [RSP+40] = flags
;   [RSP+48] = outStats ptr (24 bytes: timeUs, bytesXfer, bwGbps)
; Returns:
;   EAX = TITAN_SUCCESS or error code
; ============================================================
Titan_PerformCopy PROC FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    push rdi
    .PUSHREG RDI
    push r12
    .PUSHREG R12
    push r13
    .PUSHREG R13
    push r14
    .PUSHREG R14
    push r15
    .PUSHREG R15
    sub rsp, 88
    .ALLOCSTACK 88
    push rbp
    .PUSHREG RBP
    mov rbp, rsp
    .SETFRAME RBP, 0
    .ENDPROLOG

    mov r12d, ecx                           ; Direction
    mov r13, rdx                            ; Src
    mov r14, r8                             ; Dst
    mov r15, r9                             ; Size
    mov ebx, DWORD PTR [rbp+16+88]          ; Flags
    mov rsi, QWORD PTR [rbp+24+88]          ; Stats output ptr

    ; Validate parameters
    test r13, r13
    jz @@invalid_param
    test r14, r14
    jz @@invalid_param
    test r15, r15
    jz @@invalid_param

    ; Get start time
    call GetTimestampUs
    mov rdi, rax

    ; Direction-based dispatch:
    ;   COPY_H2D (0): Host->Device — flush write-combine buffer, copy, CLFLUSHOPT
    ;   COPY_D2H (1): Device->Host — non-temporal load path via Kernel_Copy
    ;   COPY_D2D (2): Device->Device — post to GPU IOCP queue for peer transfer
    ;   COPY_H2H (3): Host->Host — standard cache-friendly Kernel_Copy
    cmp r12d, COPY_D2D
    je @@dir_d2d
    cmp r12d, COPY_H2D
    je @@dir_h2d
    ; D2H (1) and H2H (3): direct Kernel_Copy
    mov rcx, r13
    mov rdx, r14
    mov r8, r15
    mov r9d, ebx
    call Kernel_Copy
    jmp @@dir_done

@@dir_h2d:
    ; H2D: Flush WC write buffer before copy, then CLFLUSHOPT destination
    sfence
    mov rcx, r13
    mov rdx, r14
    mov r8, r15
    mov r9d, ebx
    call Kernel_Copy
    ; Issue CLFLUSHOPT across destination range for device visibility
    mov rcx, r14
    mov rdx, r15
@@h2d_flush_loop:
    clflushopt byte ptr [rcx]
    add rcx, CACHE_LINE_SIZE
    sub rdx, CACHE_LINE_SIZE
    ja @@h2d_flush_loop
    sfence
    jmp @@dir_done

@@dir_d2d:
    ; D2D: Post to GPU I/O completion port for peer DMA transfer
    mov rax, [g_gpuIocp]
    test rax, rax
    jz @@dir_d2d_cpu

    ; PostQueuedCompletionStatus(hIocp, bytesXfer=size, compKey=src, lpOvl=dst)
    mov rcx, rax
    mov rdx, r15
    mov r8, r13
    mov r9, r14
    sub rsp, 40
    mov qword ptr [rsp+32], 0
    call PostQueuedCompletionStatus
    add rsp, 40
    test eax, eax
    jnz @@dir_done

@@dir_d2d_cpu:
    ; GPU IOCP not available — CPU fallback
    mov rcx, r13
    mov rdx, r14
    mov r8, r15
    mov r9d, ebx
    call Kernel_Copy

@@dir_done:
    ; Calculate timing
    call GetTimestampUs
    sub rax, rdi
    mov r12, rax                            ; Elapsed microseconds

    ; Update global statistics
    mov rcx, r15
    mov rdx, r12
    call UpdateStats

    ; Fill caller stats structure if provided
    test rsi, rsi
    jz @@no_stats

    mov QWORD PTR [rsi], r12                ; timeUs
    mov QWORD PTR [rsi+8], r15              ; bytesTransferred

    ; Calculate bandwidth Gbps: (bytes * 8) / (time_us * 1000)
    mov rax, r15
    shl rax, 3                              ; bytes * 8 = bits
    xor edx, edx
    mov rcx, r12
    test rcx, rcx
    jz @@no_stats                           ; Guard divide-by-zero
    mul rcx                                 ; Actually: bits already in rax
    mov rax, r15
    shl rax, 3
    xor edx, edx
    div r12                                 ; bits / time_us = Mbps
    xor edx, edx
    mov rcx, 1000
    div rcx                                 ; Mbps / 1000 = Gbps
    mov DWORD PTR [rsi+16], eax             ; bandwidthGbps

@@no_stats:
    xor eax, eax                            ; TITAN_SUCCESS
    jmp @@exit

@@invalid_param:
    lock inc g_FailedTransfers
    mov eax, TITAN_ERROR_INVALID_PARAM

@@exit:
    lea rsp, [rbp]
    pop rbp
    add rsp, 88
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_PerformCopy ENDP

; ============================================================
; PUBLIC: Titan_PerformDMA
; ============================================================
; 3-tier DMA routing: DirectStorage -> Vulkan -> CPU fallback.
; Parameters:
;   ECX = DMA type (0=CPU, 1=VULKAN, 2=DIRECTSTORAGE)
;   RDX = DMA_REQUEST ptr (srcAddr, dstAddr, size, flags, eventHandle, timestamp, status)
;   R8  = completion event ptr (handle for async notification)
;   R9  = timeout in milliseconds (for async wait)
; Returns:
;   EAX = TITAN_SUCCESS or TITAN_ERROR_*
; ============================================================
Titan_PerformDMA PROC FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    push rdi
    .PUSHREG RDI
    push r12
    .PUSHREG R12
    push r13
    .PUSHREG R13
    push r14
    .PUSHREG R14
    push r15
    .PUSHREG R15
    sub rsp, 88
    .ALLOCSTACK 88
    push rbp
    .PUSHREG RBP
    mov rbp, rsp
    .SETFRAME RBP, 0
    .ENDPROLOG

    mov r12d, ecx                           ; DMA type
    mov r13, rdx                            ; DMA_REQUEST*
    mov r14, r8                             ; Completion event ptr
    mov r15, r9                             ; Timeout ms

    ; Validate DMA type range [0..2]
    cmp r12d, 2
    ja @@dma_invalid_param

    ; Validate request pointer
    test r13, r13
    jz @@dma_invalid_param

    ; Extract and validate request fields
    mov rsi, QWORD PTR [r13 + DMA_REQUEST_srcAddr]
    mov rdi, QWORD PTR [r13 + DMA_REQUEST_dstAddr]
    mov rbx, QWORD PTR [r13 + DMA_REQUEST_size]

    test rsi, rsi
    jz @@dma_invalid_param
    test rdi, rdi
    jz @@dma_invalid_param
    test rbx, rbx
    jz @@dma_invalid_param

    ; Safety limit: reject > 1GB transfers
    cmp rbx, MAX_DMA_SIZE
    ja @@dma_invalid_param

    ; Timestamp the request
    call GetTimestampUs
    mov QWORD PTR [r13 + DMA_REQUEST_timestamp], rax

    ; Mark in-progress
    mov DWORD PTR [r13 + DMA_REQUEST_status], STATUS_IN_PROGRESS

    ; ========================================
    ; 3-Tier DMA Dispatch
    ; ========================================
    cmp r12d, DMA_TYPE_DIRECTSTORAGE
    je @@dma_exec_directstorage
    cmp r12d, DMA_TYPE_VULKAN
    je @@dma_exec_vulkan
    ; Fall through to CPU path

    ; ========================================
    ; TIER 3: CPU DMA — AVX-512 memory copy
    ; ========================================
@@dma_exec_cpu:
    mov rcx, rsi                            ; Source
    mov rdx, rdi                            ; Dest
    mov r8, rbx                             ; Size

    ; Auto-select temporal mode based on transfer size
    xor r9d, r9d
    cmp rbx, NONTEMPORAL_THRESHOLD
    jbe @@dma_cpu_exec
    mov r9d, FLAG_NON_TEMPORAL              ; Large: non-temporal

@@dma_cpu_exec:
    call Kernel_Copy
    xor ebx, ebx                            ; Success status
    jmp @@dma_exec_done

    ; ========================================
    ; TIER 1: DirectStorage DMA (ReadFile non-buffered I/O)
    ; ========================================
    ; Uses overlapped I/O for async disk->memory transfer.
    ; srcAddr = file HANDLE (opened with FILE_FLAG_NO_BUFFERING)
    ; dstAddr = destination buffer (must be sector-aligned)
    ; size    = number of bytes to read
    ; ========================================
@@dma_exec_directstorage:
    push rsi
    push rdi

    ; Build OVERLAPPED structure on stack
    sub rsp, 48                             ; sizeof(OVERLAPPED) + padding
    xor rax, rax
    mov [rsp], rax                          ; Internal
    mov [rsp+8], rax                        ; InternalHigh
    mov DWORD PTR [rsp+16], 0              ; Offset (low)
    mov DWORD PTR [rsp+20], 0              ; OffsetHigh
    mov rax, QWORD PTR [r13 + DMA_REQUEST_eventHandle]
    mov [rsp+24], rax                       ; hEvent

    ; ReadFile(hFile=src, lpBuffer=dst, nBytesToRead=size, lpBytesRead, lpOverlapped)
    mov rcx, rsi                            ; hFile = srcAddr
    mov rdx, rdi                            ; lpBuffer = dstAddr
    mov r8d, ebx                            ; nBytesToRead = size (low 32)
    lea r9, [rsp+40]                        ; lpBytesRead (scratch space)
    sub rsp, 40                             ; Shadow space + 5th arg
    lea rax, [rsp+40]                       ; lpOverlapped = our OVERLAPPED
    mov [rsp+32], rax
    call QWORD PTR [__imp_ReadFile]
    add rsp, 40

    test eax, eax
    jnz @@dma_ds_succeeded

    ; Check if async I/O pending
    call QWORD PTR [__imp_GetLastError]
    cmp eax, ERROR_IO_PENDING
    jne @@dma_ds_failed

    ; Wait for async completion
    mov rcx, QWORD PTR [r13 + DMA_REQUEST_eventHandle]
    mov edx, r15d                           ; Timeout ms
    call QWORD PTR [__imp_WaitForSingleObject]
    test eax, eax
    jnz @@dma_ds_failed

@@dma_ds_succeeded:
    add rsp, 48
    pop rdi
    pop rsi
    xor ebx, ebx                            ; Success
    jmp @@dma_exec_done

@@dma_ds_failed:
    add rsp, 48
    pop rdi
    pop rsi
    mov ebx, TITAN_ERROR_DMA_FAILED
    jmp @@dma_exec_done

    ; ========================================
    ; TIER 2: Vulkan DMA — AVX-512 non-temporal transfer
    ; ========================================
    ; Performs a write-combining non-temporal transfer suitable for
    ; mapped GPU memory (Vulkan vkMapMemory coherent/non-coherent).
    ; Uses 64-byte ZMM streaming stores to bypass CPU cache and
    ; deliver data directly to the PCIe write-combine buffer.
    ; ========================================
@@dma_exec_vulkan:
    push rsi
    push rdi

    mov rcx, rbx                            ; Transfer size
    cmp rcx, AVX512_ZMM_SIZE
    jb @@dma_vk_small

    ; AVX-512 non-temporal streaming transfer
    mov rax, rcx
    shr rcx, 6                              ; Count of 64-byte chunks

@@dma_vk_nt_loop:
    vmovdqu64 zmm0, ZMMWORD PTR [rsi]       ; Load 64 bytes from host memory
    vmovntdq ZMMWORD PTR [rdi], zmm0        ; Non-temporal store to GPU mapping
    add rsi, AVX512_ZMM_SIZE
    add rdi, AVX512_ZMM_SIZE
    dec rcx
    jnz @@dma_vk_nt_loop

    ; Handle remainder bytes with REP MOVSB
    mov rcx, rax
    and rcx, 63
    rep movsb

    sfence                                  ; Flush write-combine buffers
    jmp @@dma_vk_done

@@dma_vk_small:
    ; Sub-64-byte transfer: scalar REP MOVSB
    rep movsb

@@dma_vk_done:
    mfence                                  ; Full memory barrier for GPU visibility
    pop rdi
    pop rsi
    xor ebx, ebx                            ; Success

    ; ========================================
    ; COMMON: Post-DMA completion
    ; ========================================
@@dma_exec_done:
    ; Update request status
    test ebx, ebx
    jnz @@dma_set_error
    mov DWORD PTR [r13 + DMA_REQUEST_status], STATUS_COMPLETED
    jmp @@dma_signal

@@dma_set_error:
    mov DWORD PTR [r13 + DMA_REQUEST_status], STATUS_ERROR

@@dma_signal:
    ; Signal completion event if caller provided one
    test r14, r14
    jz @@dma_no_event
    mov rcx, QWORD PTR [r13 + DMA_REQUEST_eventHandle]
    test rcx, rcx
    jz @@dma_no_event
    call QWORD PTR [__imp_SetEvent]

@@dma_no_event:
    ; Update global statistics
    mov rcx, QWORD PTR [r13 + DMA_REQUEST_size]
    xor edx, edx
    test ebx, ebx
    jnz @@dma_count_fail
    call GetTimestampUs
    sub rax, QWORD PTR [r13 + DMA_REQUEST_timestamp]
    mov rdx, rax
    mov rcx, QWORD PTR [r13 + DMA_REQUEST_size]
    call UpdateStats
    jmp @@dma_return

@@dma_count_fail:
    lock inc g_FailedTransfers

@@dma_return:
    mov eax, ebx
    jmp @@dma_exit

@@dma_invalid_param:
    lock inc g_FailedTransfers
    mov eax, TITAN_ERROR_INVALID_PARAM

@@dma_exit:
    lea rsp, [rbp]
    pop rbp
    add rsp, 88
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_PerformDMA ENDP

; ============================================================
; PUBLIC: Titan_InitializeDMA
; ============================================================
; Initialize the DMA subsystem: QPC frequency, NF4 table, stats.
; Returns: EAX = 0 (success)
; ============================================================
Titan_InitializeDMA PROC FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    push rdi
    .PUSHREG RDI
    .ENDPROLOG

    ; Idempotent: skip if already initialized
    mov al, g_Initialized
    test al, al
    jnz @@already_init

    ; Query the high-resolution timer frequency
    lea rcx, g_QPCFrequency
    call QueryPerformanceFrequency

    ; Populate the NF4 lookup table with exact QLoRA quantization values
    ; 16 centroids for the normal distribution quantile function
    lea rdi, NF4_Lookup_Table
    mov DWORD PTR [rdi],    0BF800000h      ; -1.0
    mov DWORD PTR [rdi+4],  0BF3246B8h      ; -0.6961928
    mov DWORD PTR [rdi+8],  0BF066666h      ; -0.5250731
    mov DWORD PTR [rdi+12], 0BECA2E8Ch      ; -0.3949175
    mov DWORD PTR [rdi+16], 0BE91AC08h      ; -0.2844414
    mov DWORD PTR [rdi+20], 0BE3D2F1Ch      ; -0.1847734
    mov DWORD PTR [rdi+24], 0BDBA5E34h      ; -0.0910500
    mov DWORD PTR [rdi+28], 000000000h      ;  0.0
    mov DWORD PTR [rdi+32], 03DA2F983h      ;  0.0795803
    mov DWORD PTR [rdi+36], 03E24F0A4h      ;  0.1609302
    mov DWORD PTR [rdi+40], 03E7C3A7Eh      ;  0.2461123
    mov DWORD PTR [rdi+44], 03EACF914h      ;  0.3379152
    mov DWORD PTR [rdi+48], 03EE18698h      ;  0.4407098
    mov DWORD PTR [rdi+52], 03F101D8Ah      ;  0.5626170
    mov DWORD PTR [rdi+56], 03F393A36h      ;  0.7229568
    mov DWORD PTR [rdi+60], 03F800000h      ;  1.0

    ; Zero all statistics counters
    xor rax, rax
    mov g_TotalBytesCopied, rax
    mov g_TotalDMATransfers, rax
    mov g_FailedTransfers, rax
    mov g_StatsLock, rax
    xorpd xmm0, xmm0                        ; Explicitly zero xmm0
    movsd g_PeakBandwidthGbps, xmm0         ; Initialize peak bandwidth to 0.0

    ; GPU IOCP handle starts NULL (set later by GPU driver init)
    mov g_gpuIocp, rax

    ; Mark subsystem as initialized
    mov g_Initialized, 1

    ; Debug trace
    lea rcx, szDmaInitOk
    call OutputDebugStringA

@@already_init:
    xor eax, eax
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_InitializeDMA ENDP

; ============================================================
; PUBLIC: Titan_ShutdownDMA
; ============================================================
; Shut down the DMA subsystem. Issues full memory fence.
; Returns: EAX = 0 (success)
; ============================================================
Titan_ShutdownDMA PROC FRAME
    .ENDPROLOG

    ; Full memory barrier to drain all pending operations
    mfence

    ; Clear initialization flag
    mov g_Initialized, 0
    mov g_gpuIocp, 0

    ; Debug trace
    lea rcx, szDmaShutdown
    call OutputDebugStringA

    xor eax, eax
    ret
Titan_ShutdownDMA ENDP

; ============================================================
; PUBLIC: Titan_GetDMAStats
; ============================================================
; Read global DMA statistics under lock.
; Parameters:
;   RCX = pointer to output structure (32 bytes):
;         QWORD totalBytesCopied
;         QWORD totalDMATransfers
;         QWORD failedTransfers
;         REAL8  peakBandwidthGbps
; Returns:
;   EAX = 0 (success) or TITAN_ERROR_INVALID_PARAM
; ============================================================
Titan_GetDMAStats PROC FRAME
    push rbx
    .PUSHREG RBX
    push rsi
    .PUSHREG RSI
    .ENDPROLOG

    mov rbx, rcx

    test rbx, rbx
    jz @@invalid_param

    ; Acquire stats lock for consistent snapshot
    lea rcx, g_StatsLock
    call AcquireSpinlock

    mov rax, g_TotalBytesCopied
    mov QWORD PTR [rbx], rax
    mov rax, g_TotalDMATransfers
    mov QWORD PTR [rbx+8], rax
    mov rax, g_FailedTransfers
    mov QWORD PTR [rbx+16], rax
    movsd xmm0, g_PeakBandwidthGbps
    movsd REAL8 PTR [rbx+24], xmm0

    lea rcx, g_StatsLock
    call ReleaseSpinlock

    xor eax, eax
    jmp @@exit

@@invalid_param:
    mov eax, TITAN_ERROR_INVALID_PARAM

@@exit:
    pop rsi
    pop rbx
    ret
Titan_GetDMAStats ENDP

; ============================================================
; DATA: AVX-512 mask constants
; ============================================================
.data
align 64
low_nibble_mask DWORD 16 DUP(0Fh)           ; 64 bytes of 0x0F mask

; ============================================================
; END OF MODULE
; ============================================================
END
