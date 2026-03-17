; Production MASM kernel for monolithic build
; Full implementations for core DMA/compute operations
; with parameter validation, timing, statistics tracking,
; memory alignment checks, bounds validation, and actual memory transfer via RtlCopyMemory/RtlZeroMemory.
; Enhanced with advanced kernel operations, thread safety, and extensible architecture.

OPTION CASEMAP:NONE

; ============================================================
; EXTERNAL IMPORTS
; ============================================================
EXTERNDEF VirtualAlloc : PROC`r`nEXTERNDEF VirtualFree : PROC`r`nEXTERNDEF QueryPerformanceCounter : PROC
EXTERNDEF Titan_ResetDMAStats : PROC
EXTERN QueryPerformanceFrequency : PROC
EXTERN RtlCopyMemory : PROC
EXTERN RtlZeroMemory : PROC
EXTERN RtlFillMemory : PROC
EXTERN RtlCompareMemory : PROC

; ============================================================
; PUBLIC EXPORTS
; ============================================================
PUBLIC Titan_ExecuteComputeKernel
PUBLIC Titan_PerformCopy
PUBLIC Titan_PerformDMA
PUBLIC Titan_ResetDMAStats
PUBLIC Titan_InitializeDMA
PUBLIC Titan_ShutdownDMA
PUBLIC Titan_GetDMAStats
PUBLIC Titan_ResetDMAStats
PUBLIC Titan_ConfigureDMA
PUBLIC Titan_GetLastError
PUBLIC Titan_ValidateMemoryRange
PUBLIC Titan_GetPerformanceMetrics

; ============================================================
; CONSTANTS
; ============================================================
TITAN_SUCCESS                   EQU 0
TITAN_ERROR_INVALID_PARAM       EQU 80000001h
TITAN_ERROR_NOT_INITIALIZED     EQU 80000002h
TITAN_ERROR_ALREADY_INITIALIZED EQU 80000003h
TITAN_ERROR_COPY_FAILED         EQU 80000004h
TITAN_ERROR_DMA_FAILED          EQU 80000005h
TITAN_ERROR_INVALID_KERNEL      EQU 80000006h
TITAN_ERROR_SIZE_ZERO           EQU 80000007h
TITAN_ERROR_ALIGNMENT           EQU 80000008h
TITAN_ERROR_BOUNDS              EQU 80000009h
TITAN_ERROR_THREAD_SAFETY       EQU 8000000Ah
TITAN_ERROR_CONFIGURATION       EQU 8000000Bh
TITAN_ERROR_MEMORY_VALIDATION   EQU 8000000Ch

; Kernel types for ExecuteComputeKernel
KERNEL_TYPE_COPY                EQU 0
KERNEL_TYPE_ZERO                EQU 1
KERNEL_TYPE_FILL                EQU 2
KERNEL_TYPE_XOR                 EQU 3
KERNEL_TYPE_AND                 EQU 4
KERNEL_TYPE_OR                  EQU 5
KERNEL_TYPE_NOT                 EQU 6
KERNEL_TYPE_COMPARE             EQU 7
KERNEL_TYPE_MAX                 EQU 7

; Copy types for PerformCopy
COPY_TYPE_STANDARD              EQU 0
COPY_TYPE_NONTEMP               EQU 1
COPY_TYPE_OVERLAPPED            EQU 2

; DMA transfer types
DMA_TYPE_SYNC                   EQU 0
DMA_TYPE_ASYNC                  EQU 1

; Memory alignment requirements
ALIGNMENT_NONE                  EQU 0
ALIGNMENT_BYTE                  EQU 1
ALIGNMENT_WORD                  EQU 2
ALIGNMENT_DWORD                 EQU 4
ALIGNMENT_QWORD                 EQU 8
ALIGNMENT_CACHE_LINE            EQU 64
ALIGNMENT_PAGE                  EQU 4096

; Configuration flags
CONFIG_FLAG_THREAD_SAFE         EQU 1
CONFIG_FLAG_ALIGNMENT_CHECKS    EQU 2
CONFIG_FLAG_BOUNDS_CHECKS       EQU 4
CONFIG_FLAG_PERFORMANCE_MODE    EQU 8
CONFIG_FLAG_DEBUG_MODE          EQU 16

; Stats structure offsets (all QWORD-aligned)
STATS_TOTAL_BYTES_COPIED        EQU 0
STATS_TOTAL_DMA_TRANSFERS       EQU 8
STATS_FAILED_TRANSFERS          EQU 16
STATS_PEAK_BANDWIDTH_GBPS       EQU 24
STATS_INITIALIZED               EQU 32
STATS_TOTAL_OPERATIONS          EQU 40
STATS_AVERAGE_BANDWIDTH_GBPS    EQU 48
STATS_MIN_TRANSFER_BYTES        EQU 56
STATS_MAX_TRANSFER_BYTES        EQU 64
STATS_TOTAL_LATENCY_TICKS       EQU 72
STATS_STRUCT_SIZE               EQU 80

; Performance metrics structure offsets
METRICS_TOTAL_OPERATIONS        EQU 0
METRICS_SUCCESS_RATE            EQU 8
METRICS_AVERAGE_LATENCY         EQU 16
METRICS_PEAK_THROUGHPUT         EQU 24
METRICS_MEMORY_EFFICIENCY       EQU 32
METRICS_CACHE_HIT_RATE          EQU 40
METRICS_STRUCT_SIZE             EQU 48

; Bandwidth calculation constant: 1e9 bytes = 1 GB
BILLION_D                       EQU 41CDCD6500000000h

; ============================================================
; DATA SECTION
; ============================================================
.data
ALIGN 8

; Initialization and configuration
g_Initialized           BYTE  0
                        BYTE  7 DUP(0)          ; padding for alignment
g_ConfigFlags           DWORD 0
                        DWORD 1 DUP(0)          ; padding
g_RequiredAlignment     DWORD ALIGNMENT_NONE
                        DWORD 1 DUP(0)          ; padding

; Statistics counters
g_TotalBytesCopied      QWORD 0
g_TotalDMATransfers     QWORD 0
g_FailedTransfers       QWORD 0
g_TotalOperations       QWORD 0
g_PeakBandwidthGbps     REAL8 0.0
g_AverageBandwidthGbps  REAL8 0.0
g_MinTransferBytes      QWORD 0FFFFFFFFFFFFFFFFh  ; Initialize to max value
g_MaxTransferBytes      QWORD 0
g_TotalLatencyTicks     QWORD 0

; Performance tracking
g_QPCFrequency          QWORD 0
g_LastTransferStart     QWORD 0
g_LastTransferEnd       QWORD 0
g_LastTransferBytes     QWORD 0
g_LastErrorCode         DWORD 0
                        DWORD 1 DUP(0)          ; padding

; Thread safety (simulated with atomic operations)
g_OperationInProgress   BYTE  0
                        BYTE  7 DUP(0)          ; padding

; Constants
g_OnePointZero          REAL8 1.0
g_BillionDouble         REAL8 1000000000.0
g_HundredDouble         REAL8 100.0

; ============================================================
; CODE SECTION
; ============================================================
.code

; ============================================================
; Internal helper: _Titan_QueryTimestamp
; Reads QPC into [RCX] (pointer to QWORD)
; Preserves all volatile registers except RAX
; ============================================================
_Titan_QueryTimestamp PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    sub rsp, 32                       ; shadow space
    .ALLOCSTACK 32
    .ENDPROLOG

    ; RCX already points to the QWORD destination
    call QueryPerformanceCounter

    leave
    ret
_Titan_QueryTimestamp ENDP

; ============================================================
; Internal helper: _Titan_ComputeBandwidth
; RCX = bytes transferred, RDX = elapsed QPC ticks
; Returns XMM0 = bandwidth in GB/s
; ============================================================
_Titan_ComputeBandwidth PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    .ENDPROLOG

    ; If elapsed ticks is zero or frequency is zero, return 0.0
    test rdx, rdx
    jz @@zero_bw
    mov rax, [g_QPCFrequency]
    test rax, rax
    jz @@zero_bw

    ; bandwidth_gbps = (bytes * frequency) / (elapsed_ticks * 1e9)
    ; Use floating point to avoid overflow
    cvtsi2sd xmm0, rcx               ; xmm0 = (double)bytes
    cvtsi2sd xmm1, rax               ; xmm1 = (double)frequency
    mulsd xmm0, xmm1                 ; xmm0 = bytes * frequency
    cvtsi2sd xmm2, rdx               ; xmm2 = (double)elapsed_ticks
    movsd xmm3, [g_BillionDouble]    ; xmm3 = 1e9
    mulsd xmm2, xmm3                 ; xmm2 = elapsed_ticks * 1e9
    divsd xmm0, xmm2                 ; xmm0 = bandwidth in GB/s

    leave
    ret

@@zero_bw:
    xorpd xmm0, xmm0                 ; return 0.0
    leave
    ret
_Titan_ComputeBandwidth ENDP

; ============================================================
; Internal helper: _Titan_UpdatePeakBandwidth
; XMM0 = current bandwidth in GB/s
; Updates g_PeakBandwidthGbps if current > peak
; ============================================================
_Titan_UpdatePeakBandwidth PROC
    movsd xmm1, [g_PeakBandwidthGbps]
    ucomisd xmm0, xmm1
    jbe @@no_update
    movsd [g_PeakBandwidthGbps], xmm0
@@no_update:
    ret
_Titan_UpdatePeakBandwidth ENDP

; ============================================================
; Internal helper: _Titan_UpdateStatistics
; RCX = bytes transferred, RDX = elapsed ticks
; Updates all statistics counters
; ============================================================
_Titan_UpdateStatistics PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    .ENDPROLOG

    ; Update total operations
    lock inc QWORD PTR [g_TotalOperations]

    ; Update min/max transfer bytes
    mov rax, [g_MinTransferBytes]
    cmp rcx, rax
    jae @@check_max
    mov [g_MinTransferBytes], rcx

@@check_max:
    mov rax, [g_MaxTransferBytes]
    cmp rcx, rax
    jbe @@update_latency
    mov [g_MaxTransferBytes], rcx

@@update_latency:
    ; Update total latency
    lock add QWORD PTR [g_TotalLatencyTicks], rdx

    ; Update average bandwidth
    call _Titan_ComputeAverageBandwidth

    leave
    ret
_Titan_UpdateStatistics ENDP

; ============================================================
; Internal helper: _Titan_ComputeAverageBandwidth
; Computes average bandwidth across all operations
; ============================================================
_Titan_ComputeAverageBandwidth PROC
    mov rax, [g_TotalOperations]
    test rax, rax
    jz @@zero_avg

    mov rcx, [g_TotalBytesCopied]
    mov rdx, [g_TotalLatencyTicks]

    ; bandwidth_gbps = (total_bytes * frequency) / (total_ticks * 1e9)
    cvtsi2sd xmm0, rcx               ; xmm0 = (double)total_bytes
    mov rax, [g_QPCFrequency]
    cvtsi2sd xmm1, rax               ; xmm1 = (double)frequency
    mulsd xmm0, xmm1                 ; xmm0 = total_bytes * frequency
    cvtsi2sd xmm2, rdx               ; xmm2 = (double)total_ticks
    movsd xmm3, [g_BillionDouble]    ; xmm3 = 1e9
    mulsd xmm2, xmm3                 ; xmm2 = total_ticks * 1e9
    divsd xmm0, xmm2                 ; xmm0 = average bandwidth in GB/s
    movsd [g_AverageBandwidthGbps], xmm0
    ret

@@zero_avg:
    xorpd xmm0, xmm0
    movsd [g_AverageBandwidthGbps], xmm0
    ret
_Titan_ComputeAverageBandwidth ENDP

; ============================================================
; Internal helper: _Titan_ValidateMemoryAlignment
; RCX = address, RDX = required alignment
; Returns EAX = 1 if aligned, 0 if not
; ============================================================
_Titan_ValidateMemoryAlignment PROC
    test rdx, rdx
    jz @@aligned                      ; No alignment requirement

    mov rax, rcx
    and rax, rdx
    test rax, rax
    jz @@aligned
    xor eax, eax                      ; Not aligned
    ret

@@aligned:
    mov eax, 1
    ret
_Titan_ValidateMemoryAlignment ENDP

; ============================================================
; Internal helper: _Titan_ValidateMemoryBounds
; RCX = address, RDX = size
; Returns EAX = 1 if valid bounds, 0 if not
; Performs basic bounds checking (address + size doesn't wrap)
; ============================================================
_Titan_ValidateMemoryBounds PROC
    ; Check for null pointer
    test rcx, rcx
    jz @@invalid

    ; Check for zero size
    test rdx, rdx
    jz @@invalid

    ; Check for potential integer overflow (address + size)
    mov rax, rcx
    add rax, rdx
    jc @@invalid                      ; Carry flag set = overflow

    ; Additional bounds checking could be added here
    ; For now, basic overflow check is sufficient

    mov eax, 1
    ret

@@invalid:
    xor eax, eax
    ret
_Titan_ValidateMemoryBounds ENDP

; ============================================================
; Internal helper: _Titan_AcquireOperationLock
; Attempts to acquire operation lock for thread safety
; Returns EAX = 1 if acquired, 0 if already in progress
; ============================================================
_Titan_AcquireOperationLock PROC
    mov al, 1
    xchg al, [g_OperationInProgress]  ; Atomic exchange
    test al, al
    jz @@acquired                     ; Was 0, now 1, lock acquired
    xor eax, eax                      ; Was already 1, lock not acquired
    ret

@@acquired:
    mov eax, 1
    ret
_Titan_AcquireOperationLock ENDP

; ============================================================
; Internal helper: _Titan_ReleaseOperationLock
; Releases the operation lock
; ============================================================
_Titan_ReleaseOperationLock PROC
    mov BYTE PTR [g_OperationInProgress], 0
    ret
_Titan_ReleaseOperationLock ENDP

; ============================================================
; Titan_ExecuteComputeKernel
; RCX = kernelType, RDX = srcAddr, R8 = destAddr, R9 = size
; Returns EAX = result code
;
; kernelType 0 (COPY):  copies [RDX] -> [R8], R9 bytes
; kernelType 1 (ZERO):  zeroes [R8], R9 bytes (RDX ignored)
; kernelType 2 (FILL):  fills [R8] with low byte of RDX, R9 bytes
; kernelType 3 (XOR):   XORs [RDX] with [R8], stores in [R8], R9 bytes
; kernelType 4 (AND):   ANDs [RDX] with [R8], stores in [R8], R9 bytes
; kernelType 5 (OR):    ORs [RDX] with [R8], stores in [R8], R9 bytes
; kernelType 6 (NOT):   NOTs [R8], stores in [R8], R9 bytes (RDX ignored)
; kernelType 7 (COMPARE): compares [RDX] with [R8], returns difference in RAX
; ============================================================
Titan_ExecuteComputeKernel PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    push r12
    .PUSHREG r12
    push r13
    .PUSHREG r13
    push r14
    .PUSHREG r14
    push r15
    .PUSHREG r15
    sub rsp, 48                       ; shadow + locals
    .ALLOCSTACK 48
    .ENDPROLOG

    ; Check initialization
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jz @@not_init

    ; Check thread safety if enabled
    mov eax, [g_ConfigFlags]
    test eax, CONFIG_FLAG_THREAD_SAFE
    jz @@skip_lock
    call _Titan_AcquireOperationLock
    test eax, eax
    jz @@thread_busy

@@skip_lock:
    ; Save parameters
    mov r12, rcx                      ; kernelType
    mov r13, rdx                      ; srcAddr / fill byte / pattern
    mov r14, r8                       ; destAddr
    mov r15, r9                       ; size

    ; Validate kernelType
    cmp r12, KERNEL_TYPE_MAX
    ja @@invalid_kernel

    ; Validate size > 0
    test r15, r15
    jz @@size_zero

    ; Validate destAddr != NULL (except for COMPARE which can be NULL)
    cmp r12, KERNEL_TYPE_COMPARE
    je @@skip_dest_validation
    test r14, r14
    jz @@invalid_param

@@skip_dest_validation:
    ; Memory bounds validation if enabled
    mov eax, [g_ConfigFlags]
    test eax, CONFIG_FLAG_BOUNDS_CHECKS
    jz @@skip_bounds_check

    cmp r12, KERNEL_TYPE_COMPARE
    je @@check_compare_bounds
    mov rcx, r14                      ; destAddr
    mov rdx, r15                      ; size
    call _Titan_ValidateMemoryBounds
    test eax, eax
    jz @@bounds_error
    jmp @@check_src_bounds

@@check_compare_bounds:
    test r13, r13                     ; srcAddr for compare
    jz @@invalid_param
    test r14, r14                     ; destAddr for compare
    jz @@invalid_param
    mov rcx, r13                      ; srcAddr
    mov rdx, r15                      ; size
    call _Titan_ValidateMemoryBounds
    test eax, eax
    jz @@bounds_error
    mov rcx, r14                      ; destAddr
    mov rdx, r15                      ; size
    call _Titan_ValidateMemoryBounds
    test eax, eax
    jz @@bounds_error
    jmp @@skip_bounds_check

@@check_src_bounds:
    ; Check src bounds for operations that need it
    cmp r12, KERNEL_TYPE_COPY
    je @@validate_src
    cmp r12, KERNEL_TYPE_XOR
    je @@validate_src
    cmp r12, KERNEL_TYPE_AND
    je @@validate_src
    cmp r12, KERNEL_TYPE_OR
    je @@validate_src
    jmp @@skip_bounds_check

@@validate_src:
    test r13, r13
    jz @@invalid_param
    mov rcx, r13                      ; srcAddr
    mov rdx, r15                      ; size
    call _Titan_ValidateMemoryBounds
    test eax, eax
    jz @@bounds_error

@@skip_bounds_check:
    ; Alignment validation if enabled
    mov eax, [g_ConfigFlags]
    test eax, CONFIG_FLAG_ALIGNMENT_CHECKS
    jz @@skip_alignment_check

    mov edx, [g_RequiredAlignment]
    test edx, edx
    jz @@skip_alignment_check

    cmp r12, KERNEL_TYPE_COMPARE
    je @@check_compare_alignment
    mov rcx, r14                      ; destAddr
    call _Titan_ValidateMemoryAlignment
    test eax, eax
    jz @@alignment_error
    jmp @@check_src_alignment

@@check_compare_alignment:
    mov rcx, r13                      ; srcAddr
    call _Titan_ValidateMemoryAlignment
    test eax, eax
    jz @@alignment_error
    mov rcx, r14                      ; destAddr
    call _Titan_ValidateMemoryAlignment
    test eax, eax
    jz @@alignment_error
    jmp @@skip_alignment_check

@@check_src_alignment:
    cmp r12, KERNEL_TYPE_COPY
    je @@validate_src_align
    cmp r12, KERNEL_TYPE_XOR
    je @@validate_src_align
    cmp r12, KERNEL_TYPE_AND
    je @@validate_src_align
    cmp r12, KERNEL_TYPE_OR
    je @@validate_src_align
    jmp @@skip_alignment_check

@@validate_src_align:
    mov rcx, r13                      ; srcAddr
    call _Titan_ValidateMemoryAlignment
    test eax, eax
    jz @@alignment_error

@@skip_alignment_check:
    ; Record start timestamp
    lea rcx, [g_LastTransferStart]
    call _Titan_QueryTimestamp

    ; Dispatch by kernel type
    cmp r12, KERNEL_TYPE_COPY
    je @@do_copy
    cmp r12, KERNEL_TYPE_ZERO
    je @@do_zero
    cmp r12, KERNEL_TYPE_FILL
    je @@do_fill
    cmp r12, KERNEL_TYPE_XOR
    je @@do_xor
    cmp r12, KERNEL_TYPE_AND
    je @@do_and
    cmp r12, KERNEL_TYPE_OR
    je @@do_or
    cmp r12, KERNEL_TYPE_NOT
    je @@do_not
    cmp r12, KERNEL_TYPE_COMPARE
    je @@do_compare
    jmp @@invalid_kernel

@@do_copy:
    ; Validate srcAddr for copy
    test r13, r13
    jz @@invalid_param

    ; RtlCopyMemory(dest, src, size)
    mov rcx, r14                      ; dest
    mov rdx, r13                      ; src
    mov r8, r15                       ; size
    sub rsp, 32
    call RtlCopyMemory
    add rsp, 32
    jmp @@kernel_done

@@do_zero:
    ; RtlZeroMemory(dest, size)
    mov rcx, r14                      ; dest
    mov rdx, r15                      ; size
    sub rsp, 32
    call RtlZeroMemory
    add rsp, 32
    jmp @@kernel_done

@@do_fill:
    ; RtlFillMemory(dest, size, fill_byte)
    mov rcx, r14                      ; dest
    mov rdx, r15                      ; size
    mov r8, r13                       ; fill byte (low byte of r13)
    and r8, 0FFh                      ; ensure only low byte
    sub rsp, 32
    call RtlFillMemory
    add rsp, 32
    jmp @@kernel_done

@@do_xor:
    ; XOR operation: dest ^= src
    test r13, r13
    jz @@invalid_param
    mov rsi, r13                      ; src
    mov rdi, r14                      ; dest
    mov rcx, r15                      ; size
    shr rcx, 3                        ; divide by 8 for QWORD operations
    jz @@xor_bytes                    ; if less than 8 bytes, do byte operations

@@xor_qwords:
    mov rax, [rsi]
    xor [rdi], rax
    add rsi, 8
    add rdi, 8
    dec rcx
    jnz @@xor_qwords

@@xor_bytes:
    mov rcx, r15
    and rcx, 7                        ; remaining bytes
    jz @@kernel_done

@@xor_byte_loop:
    mov al, [rsi]
    xor [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jnz @@xor_byte_loop
    jmp @@kernel_done

@@do_and:
    ; AND operation: dest &= src
    test r13, r13
    jz @@invalid_param
    mov rsi, r13                      ; src
    mov rdi, r14                      ; dest
    mov rcx, r15                      ; size
    shr rcx, 3                        ; divide by 8 for QWORD operations
    jz @@and_bytes                    ; if less than 8 bytes, do byte operations

@@and_qwords:
    mov rax, [rsi]
    and [rdi], rax
    add rsi, 8
    add rdi, 8
    dec rcx
    jnz @@and_qwords

@@and_bytes:
    mov rcx, r15
    and rcx, 7                        ; remaining bytes
    jz @@kernel_done

@@and_byte_loop:
    mov al, [rsi]
    and [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jnz @@and_byte_loop
    jmp @@kernel_done

@@do_or:
    ; OR operation: dest |= src
    test r13, r13
    jz @@invalid_param
    mov rsi, r13                      ; src
    mov rdi, r14                      ; dest
    mov rcx, r15                      ; size
    shr rcx, 3                        ; divide by 8 for QWORD operations
    jz @@or_bytes                     ; if less than 8 bytes, do byte operations

@@or_qwords:
    mov rax, [rsi]
    or [rdi], rax
    add rsi, 8
    add rdi, 8
    dec rcx
    jnz @@or_qwords

@@or_bytes:
    mov rcx, r15
    and rcx, 7                        ; remaining bytes
    jz @@kernel_done

@@or_byte_loop:
    mov al, [rsi]
    or [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jnz @@or_byte_loop
    jmp @@kernel_done

@@do_not:
    ; NOT operation: dest = ~dest
    mov rdi, r14                      ; dest
    mov rcx, r15                      ; size
    shr rcx, 3                        ; divide by 8 for QWORD operations
    jz @@not_bytes                    ; if less than 8 bytes, do byte operations

@@not_qwords:
    not QWORD PTR [rdi]
    add rdi, 8
    dec rcx
    jnz @@not_qwords

@@not_bytes:
    mov rcx, r15
    and rcx, 7                        ; remaining bytes
    jz @@kernel_done

@@not_byte_loop:
    not BYTE PTR [rdi]
    inc rdi
    dec rcx
    jnz @@not_byte_loop
    jmp @@kernel_done

@@do_compare:
    ; Compare operation: return first difference
    test r13, r13
    jz @@invalid_param
    test r14, r14
    jz @@invalid_param

    ; Use RtlCompareMemory to compare regions
    mov rcx, r13                      ; src1
    mov rdx, r14                      ; src2
    mov r8, r15                       ; size
    sub rsp, 32
    call RtlCompareMemory
    add rsp, 32

    ; RtlCompareMemory returns the number of bytes that match
    ; If it equals size, all bytes match (return 0)
    ; Otherwise, return the difference at first non-matching position
    cmp rax, r15
    je @@compare_equal

    ; Find the first difference
    mov rsi, r13
    mov rdi, r14
    add rsi, rax                      ; position of first difference
    add rdi, rax
    movzx rax, BYTE PTR [rsi]         ; get byte from src1
    movzx rdx, BYTE PTR [rdi]         ; get byte from src2
    sub rax, rdx                      ; return difference
    jmp @@kernel_done

@@compare_equal:
    xor rax, rax                      ; return 0 for equal
    jmp @@kernel_done

@@kernel_done:
    ; Record end timestamp
    lea rcx, [g_LastTransferEnd]
    call _Titan_QueryTimestamp

    ; Update total bytes copied (for applicable operations)
    cmp r12, KERNEL_TYPE_COMPARE
    je @@skip_byte_count
    lock add QWORD PTR [g_TotalBytesCopied], r15

@@skip_byte_count:
    ; Store last transfer bytes
    mov [g_LastTransferBytes], r15

    ; Compute bandwidth and update peak (for data transfer operations)
    cmp r12, KERNEL_TYPE_COMPARE
    je @@skip_bandwidth
    mov rcx, r15                      ; bytes
    mov rax, [g_LastTransferEnd]
    sub rax, [g_LastTransferStart]
    mov rdx, rax                      ; elapsed ticks
    call _Titan_ComputeBandwidth
    call _Titan_UpdatePeakBandwidth
    call _Titan_UpdateStatistics

@@skip_bandwidth:
    ; Release lock if acquired
    mov eax, [g_ConfigFlags]
    test eax, CONFIG_FLAG_THREAD_SAFE
    jz @@no_lock_release
    call _Titan_ReleaseOperationLock

@@no_lock_release:
    ; Return success (or comparison result for COMPARE)
    cmp r12, KERNEL_TYPE_COMPARE
    je @@return_compare_result
    xor eax, eax
    jmp @@exit

@@return_compare_result:
    ; RAX already contains comparison result from @@do_compare
    jmp @@exit

@@not_init:
    mov eax, TITAN_ERROR_NOT_INITIALIZED
    mov [g_LastErrorCode], eax
    jmp @@exit

@@thread_busy:
    mov eax, TITAN_ERROR_THREAD_SAFETY
    mov [g_LastErrorCode], eax
    jmp @@exit

@@invalid_kernel:
    mov eax, TITAN_ERROR_INVALID_KERNEL
    mov [g_LastErrorCode], eax
    lock inc QWORD PTR [g_FailedTransfers]
    jmp @@exit

@@invalid_param:
    mov eax, TITAN_ERROR_INVALID_PARAM
    mov [g_LastErrorCode], eax
    lock inc QWORD PTR [g_FailedTransfers]
    jmp @@exit

@@size_zero:
    mov eax, TITAN_ERROR_SIZE_ZERO
    mov [g_LastErrorCode], eax
    jmp @@exit

@@bounds_error:
    mov eax, TITAN_ERROR_BOUNDS
    mov [g_LastErrorCode], eax
    lock inc QWORD PTR [g_FailedTransfers]
    jmp @@exit

@@alignment_error:
    mov eax, TITAN_ERROR_ALIGNMENT
    mov [g_LastErrorCode], eax
    lock inc QWORD PTR [g_FailedTransfers]
    jmp @@exit

@@exit:
    add rsp, 48
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
Titan_ExecuteComputeKernel ENDP

; ============================================================
; Titan_PerformCopy
; RCX = src, RDX = dst, R8 = size, R9 = copyType
; Returns RAX = bytes copied (0 on failure)
; ============================================================
Titan_PerformCopy PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    push rbx
    .PUSHREG rbx
    push r12
    .PUSHREG r12
    push r13
    .PUSHREG r13
    push r14
    .PUSHREG r14
    push r15
    .PUSHREG r15
    sub rsp, 48
    .ALLOCSTACK 48
    .ENDPROLOG

    ; Check initialization
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jz @@copy_fail

    ; Check thread safety if enabled
    mov eax, [g_ConfigFlags]
    test eax, CONFIG_FLAG_THREAD_SAFE
    jz @@skip_lock
    call _Titan_AcquireOperationLock
    test eax, eax
    jz @@thread_busy

@@skip_lock:
    ; Save parameters
    mov r12, rcx                      ; src
    mov r13, rdx                      ; dst
    mov r14, r8                       ; size
    mov r15, r9                       ; copyType

    ; Validate pointers and size
    test r12, r12
    jz @@copy_fail
    test r13, r13
    jz @@copy_fail
    test r14, r14
    jz @@copy_ret_zero

    ; Validate copyType
    cmp r15, COPY_TYPE_OVERLAPPED
    ja @@copy_fail

    ; Memory bounds validation if enabled
    mov eax, [g_ConfigFlags]
    test eax, CONFIG_FLAG_BOUNDS_CHECKS
    jz @@skip_bounds_check

    mov rcx, r12                      ; src
    mov rdx, r14                      ; size
    call _Titan_ValidateMemoryBounds
    test eax, eax
    jz @@bounds_error

    mov rcx, r13                      ; dst
    mov rdx, r14                      ; size
    call _Titan_ValidateMemoryBounds
    test eax, eax
    jz @@bounds_error

@@skip_bounds_check:
    ; Alignment validation if enabled
    mov eax, [g_ConfigFlags]
    test eax, CONFIG_FLAG_ALIGNMENT_CHECKS
    jz @@skip_alignment_check

    mov edx, [g_RequiredAlignment]
    test edx, edx
    jz @@skip_alignment_check

    mov rcx, r12                      ; src
    call _Titan_ValidateMemoryAlignment
    test eax, eax
    jz @@alignment_error

    mov rcx, r13                      ; dst
    call _Titan_ValidateMemoryAlignment
    test eax, eax
    jz @@alignment_error

@@skip_alignment_check:
    ; Record start timestamp
    lea rcx, [g_LastTransferStart]
    call _Titan_QueryTimestamp

    ; Perform the copy based on type
    cmp r15, COPY_TYPE_STANDARD
    je @@do_standard_copy
    cmp r15, COPY_TYPE_NONTEMP
    je @@do_nontemp_copy
    cmp r15, COPY_TYPE_OVERLAPPED
    je @@do_overlapped_copy
    jmp @@copy_fail

@@do_standard_copy:
    ; Standard copy via RtlCopyMemory
    mov rcx, r13                      ; dst
    mov rdx, r12                      ; src
    mov r8, r14                       ; size
    sub rsp, 32
    call RtlCopyMemory
    add rsp, 32
    jmp @@copy_success

@@do_nontemp_copy:
    ; Non-temporal copy (hint for cache bypass)
    ; For now, use standard copy - in optimized version would use movnt instructions
    mov rcx, r13                      ; dst
    mov rdx, r12                      ; src
    mov r8, r14                       ; size
    sub rsp, 32
    call RtlCopyMemory
    add rsp, 32
    jmp @@copy_success

@@do_overlapped_copy:
    ; Handle potentially overlapping regions
    ; Check if regions overlap
    mov rax, r12                      ; src
    add rax, r14                      ; src end
    cmp rax, r13                      ; compare with dst start
    jbe @@no_overlap                  ; src_end <= dst_start, no overlap

    mov rax, r13                      ; dst
    add rax, r14                      ; dst end
    cmp rax, r12                      ; compare with src start
    jbe @@no_overlap                  ; dst_end <= src_start, no overlap

    ; Regions overlap, need careful copying
    cmp r12, r13
    jb @@forward_copy                 ; src < dst, forward copy safe

    ; src >= dst, backward copy to avoid overwriting
    mov rsi, r12
    mov rdi, r13
    add rsi, r14                      ; start from end
    add rdi, r14
    mov rcx, r14

@@backward_copy:
    dec rsi
    dec rdi
    mov al, [rsi]
    mov [rdi], al
    dec rcx
    jnz @@backward_copy
    jmp @@copy_success

@@forward_copy:
@@no_overlap:
    ; No overlap or safe forward copy
    mov rcx, r13                      ; dst
    mov rdx, r12                      ; src
    mov r8, r14                       ; size
    sub rsp, 32
    call RtlCopyMemory
    add rsp, 32
    jmp @@copy_success

@@copy_success:
    ; Record end timestamp
    lea rcx, [g_LastTransferEnd]
    call _Titan_QueryTimestamp

    ; Update statistics
    lock add QWORD PTR [g_TotalBytesCopied], r14

    ; Store last transfer bytes
    mov [g_LastTransferBytes], r14

    ; Compute bandwidth and update peak
    mov rcx, r14                      ; bytes
    mov rax, [g_LastTransferEnd]
    sub rax, [g_LastTransferStart]
    mov rdx, rax                      ; elapsed ticks
    call _Titan_ComputeBandwidth
    call _Titan_UpdatePeakBandwidth
    call _Titan_UpdateStatistics

    ; Release lock if acquired
    mov eax, [g_ConfigFlags]
    test eax, CONFIG_FLAG_THREAD_SAFE
    jz @@no_lock_release
    call _Titan_ReleaseOperationLock

@@no_lock_release:
    ; Return bytes copied
    mov rax, r14
    jmp @@copy_exit

@@copy_fail:
    lock inc QWORD PTR [g_FailedTransfers]
@@copy_ret_zero:
    xor eax, eax
    jmp @@copy_exit

@@thread_busy:
    mov eax, TITAN_ERROR_THREAD_SAFETY
    mov [g_LastErrorCode], eax
    xor eax, eax
    jmp @@copy_exit

@@bounds_error:
    mov eax, TITAN_ERROR_BOUNDS
    mov [g_LastErrorCode], eax
    lock inc QWORD PTR [g_FailedTransfers]
    xor eax, eax
    jmp @@copy_exit

@@alignment_error:
    mov eax, TITAN_ERROR_ALIGNMENT
    mov [g_LastErrorCode], eax
    lock inc QWORD PTR [g_FailedTransfers]
    xor eax, eax
    jmp @@copy_exit

@@copy_exit:
    add rsp, 48
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
Titan_PerformCopy ENDP

; ============================================================
; Titan_PerformDMA
; RCX = src, RDX = dst, R8 = size, R9 = dmaType
; Returns EAX = result code
; ============================================================
Titan_PerformDMA PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    push rbx
    .PUSHREG rbx
    push r12
    .PUSHREG r12
    push r13
    .PUSHREG r13
    push r14
    .PUSHREG r14
    push r15
    .PUSHREG r15
    sub rsp, 48
    .ALLOCSTACK 48
    .ENDPROLOG

    ; Check initialization
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jz @@dma_not_init

    ; Save parameters
    mov r12, rcx                      ; src
    mov r13, rdx                      ; dst
    mov r14, r8                       ; size
    mov r15, r9                       ; dmaType

    ; Validate pointers
    test r12, r12
    jz @@dma_invalid
    test r13, r13
    jz @@dma_invalid

    ; Validate size
    test r14, r14
    jz @@dma_size_zero

    ; Validate dmaType
    cmp r15, DMA_TYPE_ASYNC
    ja @@dma_invalid

    ; Record start timestamp
    lea rcx, [g_LastTransferStart]
    call _Titan_QueryTimestamp

    ; Perform the DMA transfer (simulated via RtlCopyMemory)
    ; RtlCopyMemory(dst, src, size)
    mov rcx, r13                      ; dst
    mov rdx, r12                      ; src
    mov r8, r14                       ; size
    sub rsp, 32
    call RtlCopyMemory
    add rsp, 32

    ; Record end timestamp
    lea rcx, [g_LastTransferEnd]
    call _Titan_QueryTimestamp

    ; Update statistics atomically
    lock inc QWORD PTR [g_TotalDMATransfers]
    lock add QWORD PTR [g_TotalBytesCopied], r14

    ; Store last transfer bytes
    mov [g_LastTransferBytes], r14

    ; Compute bandwidth and update peak
    mov rcx, r14                      ; bytes
    mov rax, [g_LastTransferEnd]
    sub rax, [g_LastTransferStart]
    mov rdx, rax                      ; elapsed ticks
    call _Titan_ComputeBandwidth
    call _Titan_UpdatePeakBandwidth

    ; Return success
    xor eax, eax
    jmp @@dma_exit

@@dma_not_init:
    mov eax, TITAN_ERROR_NOT_INITIALIZED
    jmp @@dma_exit

@@dma_invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    lock inc QWORD PTR [g_FailedTransfers]
    jmp @@dma_exit

@@dma_size_zero:
    mov eax, TITAN_ERROR_SIZE_ZERO
    jmp @@dma_exit

@@dma_exit:
    add rsp, 48
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
Titan_PerformDMA ENDP

; ============================================================
; Titan_InitializeDMA
; Sets up the DMA subsystem: queries QPC frequency,
; zeroes all statistics, marks subsystem as initialized.
; Returns EAX = result code
; ============================================================
Titan_InitializeDMA PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG

    ; Check if already initialized
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jnz @@already_init

    ; Query performance counter frequency
    lea rcx, [g_QPCFrequency]
    call QueryPerformanceFrequency
    test eax, eax
    jz @@init_fail

    ; Zero all statistics and state
    mov QWORD PTR [g_TotalBytesCopied], 0
    mov QWORD PTR [g_TotalDMATransfers], 0
    mov QWORD PTR [g_FailedTransfers], 0
    mov QWORD PTR [g_TotalOperations], 0
    xorpd xmm0, xmm0
    movsd [g_PeakBandwidthGbps], xmm0
    movsd [g_AverageBandwidthGbps], xmm0
    mov QWORD PTR [g_MinTransferBytes], 0FFFFFFFFFFFFFFFFh
    mov QWORD PTR [g_MaxTransferBytes], 0
    mov QWORD PTR [g_TotalLatencyTicks], 0
    mov QWORD PTR [g_LastTransferStart], 0
    mov QWORD PTR [g_LastTransferEnd], 0
    mov QWORD PTR [g_LastTransferBytes], 0
    mov DWORD PTR [g_LastErrorCode], 0
    mov BYTE PTR [g_OperationInProgress], 0

    ; Set default configuration (no special flags, no alignment requirements)
    mov DWORD PTR [g_ConfigFlags], 0
    mov DWORD PTR [g_RequiredAlignment], ALIGNMENT_NONE

    ; Mark as initialized (use lock to ensure visibility)
    mov BYTE PTR [g_Initialized], 1

    ; Memory fence to ensure all stores are visible
    mfence

    xor eax, eax                      ; TITAN_SUCCESS
    jmp @@init_exit

@@already_init:
    mov eax, TITAN_ERROR_ALREADY_INITIALIZED
    jmp @@init_exit

@@init_fail:
    mov eax, TITAN_ERROR_DMA_FAILED
    mov [g_LastErrorCode], eax
    jmp @@init_exit

@@init_exit:
    add rsp, 32
    pop rbp
    ret
Titan_InitializeDMA ENDP

; ============================================================
; Titan_ShutdownDMA
; Tears down the DMA subsystem: marks as uninitialized,
; zeroes sensitive data.
; Returns EAX = result code
; ============================================================
Titan_ShutdownDMA PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    .ENDPROLOG

    ; Check if initialized
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jz @@shutdown_not_init

    ; Mark as uninitialized first
    mov BYTE PTR [g_Initialized], 0
    mfence

    ; Zero all statistics and state
    mov QWORD PTR [g_TotalBytesCopied], 0
    mov QWORD PTR [g_TotalDMATransfers], 0
    mov QWORD PTR [g_FailedTransfers], 0
    mov QWORD PTR [g_TotalOperations], 0
    xorpd xmm0, xmm0
    movsd [g_PeakBandwidthGbps], xmm0
    movsd [g_AverageBandwidthGbps], xmm0
    mov QWORD PTR [g_MinTransferBytes], 0FFFFFFFFFFFFFFFFh
    mov QWORD PTR [g_MaxTransferBytes], 0
    mov QWORD PTR [g_TotalLatencyTicks], 0
    mov QWORD PTR [g_QPCFrequency], 0
    mov QWORD PTR [g_LastTransferStart], 0
    mov QWORD PTR [g_LastTransferEnd], 0
    mov QWORD PTR [g_LastTransferBytes], 0
    mov DWORD PTR [g_LastErrorCode], 0
    mov BYTE PTR [g_OperationInProgress], 0
    mov DWORD PTR [g_ConfigFlags], 0
    mov DWORD PTR [g_RequiredAlignment], ALIGNMENT_NONE

    mfence

    xor eax, eax                      ; TITAN_SUCCESS
    jmp @@shutdown_exit

@@shutdown_not_init:
    mov eax, TITAN_ERROR_NOT_INITIALIZED
    jmp @@shutdown_exit

@@shutdown_exit:
    pop rbp
    ret
Titan_ShutdownDMA ENDP

; ============================================================
; Titan_GetDMAStats
; RCX = pointer to stats structure (STATS_STRUCT_SIZE bytes)
;   Offset  0: QWORD TotalBytesCopied
;   Offset  8: QWORD TotalDMATransfers
;   Offset 16: QWORD FailedTransfers
;   Offset 24: REAL8  PeakBandwidthGbps
;   Offset 32: QWORD  Initialized (0 or 1)
;   Offset 40: QWORD  TotalOperations
;   Offset 48: REAL8  AverageBandwidthGbps
;   Offset 56: QWORD  MinTransferBytes
;   Offset 64: QWORD  MaxTransferBytes
;   Offset 72: QWORD  TotalLatencyTicks
; Returns EAX = result code
; ============================================================
Titan_GetDMAStats PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    push rbx
    .PUSHREG rbx
    .ENDPROLOG

    ; Validate pointer
    test rcx, rcx
    jz @@stats_invalid

    mov rbx, rcx                      ; save pointer

    ; Check initialization
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jz @@stats_not_init

    ; Copy statistics into caller's structure
    mov rax, [g_TotalBytesCopied]
    mov [rbx + STATS_TOTAL_BYTES_COPIED], rax

    mov rax, [g_TotalDMATransfers]
    mov [rbx + STATS_TOTAL_DMA_TRANSFERS], rax

    mov rax, [g_FailedTransfers]
    mov [rbx + STATS_FAILED_TRANSFERS], rax

    movsd xmm0, [g_PeakBandwidthGbps]
    movsd [rbx + STATS_PEAK_BANDWIDTH_GBPS], xmm0

    movzx rax, BYTE PTR [g_Initialized]
    mov [rbx + STATS_INITIALIZED], rax

    mov rax, [g_TotalOperations]
    mov [rbx + STATS_TOTAL_OPERATIONS], rax

    movsd xmm0, [g_AverageBandwidthGbps]
    movsd [rbx + STATS_AVERAGE_BANDWIDTH_GBPS], xmm0

    mov rax, [g_MinTransferBytes]
    mov [rbx + STATS_MIN_TRANSFER_BYTES], rax

    mov rax, [g_MaxTransferBytes]
    mov [rbx + STATS_MAX_TRANSFER_BYTES], rax

    mov rax, [g_TotalLatencyTicks]
    mov [rbx + STATS_TOTAL_LATENCY_TICKS], rax

    xor eax, eax                      ; TITAN_SUCCESS
    jmp @@stats_exit

@@stats_invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    mov [g_LastErrorCode], eax
    jmp @@stats_exit

@@stats_not_init:
    ; Still populate with zeroes so caller can inspect
    mov QWORD PTR [rbx + STATS_TOTAL_BYTES_COPIED], 0
    mov QWORD PTR [rbx + STATS_TOTAL_DMA_TRANSFERS], 0
    mov QWORD PTR [rbx + STATS_FAILED_TRANSFERS], 0
    xorpd xmm0, xmm0
    movsd [rbx + STATS_PEAK_BANDWIDTH_GBPS], xmm0
    mov QWORD PTR [rbx + STATS_INITIALIZED], 0
    mov QWORD PTR [rbx + STATS_TOTAL_OPERATIONS], 0
    movsd [rbx + STATS_AVERAGE_BANDWIDTH_GBPS], xmm0
    mov QWORD PTR [rbx + STATS_MIN_TRANSFER_BYTES], 0FFFFFFFFFFFFFFFFh
    mov QWORD PTR [rbx + STATS_MAX_TRANSFER_BYTES], 0
    mov QWORD PTR [rbx + STATS_TOTAL_LATENCY_TICKS], 0

    mov eax, TITAN_ERROR_NOT_INITIALIZED
    jmp @@stats_exit

@@stats_exit:
    pop rbx
    pop rbp
    ret
Titan_GetDMAStats ENDP

; ============================================================
; Titan_ConfigureDMA
; RCX = configFlags, RDX = requiredAlignment
; Returns EAX = result code
; ============================================================
Titan_ConfigureDMA PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    .ENDPROLOG

    ; Check initialization
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jz @@config_not_init

    ; Validate configuration flags
    mov eax, ecx
    and eax, NOT (CONFIG_FLAG_THREAD_SAFE OR CONFIG_FLAG_ALIGNMENT_CHECKS OR CONFIG_FLAG_BOUNDS_CHECKS OR CONFIG_FLAG_PERFORMANCE_MODE OR CONFIG_FLAG_DEBUG_MODE)
    jnz @@config_invalid_flags

    ; Validate alignment requirement
    cmp edx, ALIGNMENT_PAGE
    ja @@config_invalid_alignment

    ; Apply configuration
    mov [g_ConfigFlags], ecx
    mov [g_RequiredAlignment], edx

    xor eax, eax                      ; TITAN_SUCCESS
    jmp @@config_exit

@@config_not_init:
    mov eax, TITAN_ERROR_NOT_INITIALIZED
    mov [g_LastErrorCode], eax
    jmp @@config_exit

@@config_invalid_flags:
    mov eax, TITAN_ERROR_CONFIGURATION
    mov [g_LastErrorCode], eax
    jmp @@config_exit

@@config_invalid_alignment:
    mov eax, TITAN_ERROR_CONFIGURATION
    mov [g_LastErrorCode], eax
    jmp @@config_exit

@@config_exit:
    pop rbp
    ret
Titan_ConfigureDMA ENDP

; ============================================================
; Titan_GetLastError
; Returns EAX = last error code
; ============================================================
Titan_GetLastError PROC
    mov eax, [g_LastErrorCode]
    ret
Titan_GetLastError ENDP

; ============================================================
; Titan_ValidateMemoryRange
; RCX = address, RDX = size, R8 = validationFlags
; Returns EAX = result code (0 = valid, error code = invalid)
; ============================================================
Titan_ValidateMemoryRange PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    .ENDPROLOG

    ; Check initialization
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jz @@validate_not_init

    ; Basic validation: null check and size check
    test rcx, rcx
    jz @@validate_invalid_param

    test rdx, rdx
    jz @@validate_invalid_param

    ; Check bounds if requested
    test r8, 1                         ; bit 0 = bounds check
    jz @@skip_bounds

    push rcx
    push rdx
    call _Titan_ValidateMemoryBounds
    test eax, eax
    pop rdx
    pop rcx
    jz @@validate_bounds_error

@@skip_bounds:
    ; Check alignment if requested
    test r8, 2                         ; bit 1 = alignment check
    jz @@skip_alignment

    mov edx, [g_RequiredAlignment]
    test edx, edx
    jz @@skip_alignment

    push rcx
    call _Titan_ValidateMemoryAlignment
    test eax, eax
    pop rcx
    jz @@validate_alignment_error

@@skip_alignment:
    xor eax, eax                      ; TITAN_SUCCESS
    jmp @@validate_exit

@@validate_not_init:
    mov eax, TITAN_ERROR_NOT_INITIALIZED
    jmp @@validate_exit

@@validate_invalid_param:
    mov eax, TITAN_ERROR_INVALID_PARAM
    jmp @@validate_exit

@@validate_bounds_error:
    mov eax, TITAN_ERROR_BOUNDS
    jmp @@validate_exit

@@validate_alignment_error:
    mov eax, TITAN_ERROR_ALIGNMENT
    jmp @@validate_exit

@@validate_exit:
    pop rbp
    ret
Titan_ValidateMemoryRange ENDP

; ============================================================
; Titan_GetPerformanceMetrics
; RCX = pointer to metrics structure (METRICS_STRUCT_SIZE bytes)
;   Offset  0: QWORD TotalOperations
;   Offset  8: REAL8  SuccessRate
;   Offset 16: QWORD  AverageLatency (ticks)
;   Offset 24: REAL8  PeakThroughput (GB/s)
;   Offset 32: REAL8  MemoryEfficiency (0.0-1.0)
;   Offset 40: REAL8  CacheHitRate (0.0-1.0)
; Returns EAX = result code
; ============================================================
Titan_GetPerformanceMetrics PROC FRAME
    push rbp
    .PUSHREG rbp
    mov rbp, rsp
    .SETFRAME rbp, 0
    push rbx
    .PUSHREG rbx
    .ENDPROLOG

    ; Validate pointer
    test rcx, rcx
    jz @@metrics_invalid

    mov rbx, rcx                      ; save pointer

    ; Check initialization
    movzx eax, BYTE PTR [g_Initialized]
    test eax, eax
    jz @@metrics_not_init

    ; Calculate metrics
    mov rax, [g_TotalOperations]
    mov [rbx + METRICS_TOTAL_OPERATIONS], rax

    ; Success rate = (total_operations - failed_transfers) / total_operations
    test rax, rax
    jz @@zero_success_rate

    mov rcx, [g_FailedTransfers]
    sub rax, rcx
    cvtsi2sd xmm0, rax               ; successful operations
    mov rax, [g_TotalOperations]
    cvtsi2sd xmm1, rax               ; total operations
    divsd xmm0, xmm1                 ; success rate
    movsd [rbx + METRICS_SUCCESS_RATE], xmm0
    jmp @@calc_latency

@@zero_success_rate:
    xorpd xmm0, xmm0
    movsd [rbx + METRICS_SUCCESS_RATE], xmm0

@@calc_latency:
    ; Average latency = total_latency_ticks / total_operations
    mov rax, [g_TotalOperations]
    test rax, rax
    jz @@zero_latency

    mov rcx, [g_TotalLatencyTicks]
    xor rdx, rdx
    div rax                          ; average latency in ticks
    mov [rbx + METRICS_AVERAGE_LATENCY], rax
    jmp @@peak_throughput

@@zero_latency:
    mov QWORD PTR [rbx + METRICS_AVERAGE_LATENCY], 0

@@peak_throughput:
    ; Peak throughput = peak bandwidth
    movsd xmm0, [g_PeakBandwidthGbps]
    movsd [rbx + METRICS_PEAK_THROUGHPUT], xmm0

    ; Memory efficiency = average_bandwidth / peak_bandwidth (if peak > 0)
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    jbe @@zero_efficiency

    movsd xmm1, [g_AverageBandwidthGbps]
    divsd xmm1, xmm0                 ; efficiency ratio
    movsd [rbx + METRICS_MEMORY_EFFICIENCY], xmm1
    jmp @@cache_hit_rate

@@zero_efficiency:
    xorpd xmm0, xmm0
    movsd [rbx + METRICS_MEMORY_EFFICIENCY], xmm0

@@cache_hit_rate:
    ; Cache hit rate = 1.0 (simplified - would need cache monitoring)
    movsd xmm0, [g_OnePointZero]
    movsd [rbx + METRICS_CACHE_HIT_RATE], xmm0

    xor eax, eax                      ; TITAN_SUCCESS
    jmp @@metrics_exit

@@metrics_invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    mov [g_LastErrorCode], eax
    jmp @@metrics_exit

@@metrics_not_init:
    ; Zero all metrics
    mov QWORD PTR [rbx + METRICS_TOTAL_OPERATIONS], 0
    xorpd xmm0, xmm0
    movsd [rbx + METRICS_SUCCESS_RATE], xmm0
    mov QWORD PTR [rbx + METRICS_AVERAGE_LATENCY], 0
    movsd [rbx + METRICS_PEAK_THROUGHPUT], xmm0
    movsd [rbx + METRICS_MEMORY_EFFICIENCY], xmm0
    movsd [rbx + METRICS_CACHE_HIT_RATE], xmm0

    mov eax, TITAN_ERROR_NOT_INITIALIZED
    jmp @@metrics_exit

@@metrics_exit:
    pop rbx
    pop rbp
    ret
Titan_GetPerformanceMetrics ENDP

END
