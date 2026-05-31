; ============================================================================
; unresolved_asm_stubs.asm — extern "C" ASM Functions for x64 Windows
; ============================================================================
; This file implements all unresolved extern "C" symbols that require ASM.
; Assembled with ml64.exe (MASM64)
;
; Categories:
;   - Memory management (AllocateDMABuffer, FreeDMABuffer)
;   - Scheduler functions (Scheduler_*)
;   - Heartbeat functions (Heartbeat_*)
;   - Atomic operations
;   - CPU feature detection
;   - Low-level utilities
;
; Build: ml64 /c /Fo unresolved_asm_stubs.obj unresolved_asm_stubs.asm
; ============================================================================

.code

OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

;-----------------------------------------------------------------------------
; Align helper macro
;-----------------------------------------------------------------------------
ALIGN_16 MACRO
    ALIGN 16
ENDM

; ============================================================================
; SECTION 1: Memory Management
; ============================================================================

;-----------------------------------------------------------------------------
; void* AllocateDMABuffer(uint64_t size, uint32_t alignment)
; rcx = size
; rdx = alignment
; Returns: pointer in rax, or NULL on failure
;-----------------------------------------------------------------------------
ALIGN_16
AllocateDMABuffer PROC
    ; Standard x64 stack frame
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48                     ; Shadow space + locals
    
    ; Save non-volatile registers
    push    rbx
    push    rsi
    push    rdi
    
    ; Save parameters
    mov     rsi, rcx                    ; size
    mov     edi, edx                    ; alignment
    
    ; Calculate allocation size: size + alignment + sizeof(void*)
    mov     rax, rsi
    add     rax, rdi                    ; size + alignment
    add     rax, 8                      ; + sizeof(void*)
    
    ; Call VirtualAlloc
    xor     ecx, ecx                    ; lpAddress = NULL
    mov     rdx, rax                    ; dwSize
    mov     r8d, 3000h                  ; MEM_COMMIT | MEM_RESERVE
    mov     r9d, 04h                    ; PAGE_READWRITE
    
    ; Prepare for call
    sub     rsp, 32
    mov     rax, QWORD PTR [__imp_VirtualAlloc]
    test    rax, rax
    jz      alloc_fallback
    call    rax
    add     rsp, 32
    
    ; Check allocation result
    test    rax, rax
    jz      alloc_exit
    
    ; Align the pointer
    mov     rbx, rax                    ; Save original pointer
    add     rax, 8                      ; Skip space for original pointer
    
    ; Round up to alignment
    mov     rcx, rdi                    ; alignment
    dec     rcx                         ; alignment - 1
    add     rax, rcx                    ; ptr + (alignment - 1)
    not     rcx                         ; ~(alignment - 1)
    and     rax, rcx                    ; Aligned pointer
    
    ; Store original pointer just before aligned pointer
    mov     QWORD PTR [rax - 8], rbx
    
    jmp     alloc_exit

alloc_fallback:
    xor     eax, eax                    ; Return NULL

alloc_exit:
    ; Restore registers
    pop     rdi
    pop     rsi
    pop     rbx
    
    ; Standard epilogue
    mov     rsp, rbp
    pop     rbp
    ret
AllocateDMABuffer ENDP

;-----------------------------------------------------------------------------
; void FreeDMABuffer(void* ptr)
; rcx = ptr
;-----------------------------------------------------------------------------
ALIGN_16
FreeDMABuffer PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32                     ; Shadow space
    
    ; Validate pointer
    test    rcx, rcx
    jz      free_exit
    
    ; Get original pointer from [ptr - 8]
    mov     rcx, QWORD PTR [rcx - 8]
    
    ; Call VirtualFree
    xor     edx, edx                    ; dwSize = 0
    mov     r8d, 8000h                  ; MEM_RELEASE
    
    mov     rax, QWORD PTR [__imp_VirtualFree]
    test    rax, rax
    jz      free_exit
    call    rax

free_exit:
    mov     rsp, rbp
    pop     rbp
    ret
FreeDMABuffer ENDP

; ============================================================================
; SECTION 2: Scheduler Functions
; ============================================================================

;-----------------------------------------------------------------------------
; bool Scheduler_IsRunning(void)
;-----------------------------------------------------------------------------
ALIGN_16
Scheduler_IsRunning PROC
    lea     rcx, [g_scheduler_state]
    mov     eax, DWORD PTR [rcx]
    cmp     eax, 1                      ; 1 = running
    je      sir_yes
    xor     eax, eax                    ; Not running -> return 0
    ret
sir_yes:
    mov     eax, 1                      ; Running -> return 1
    ret
Scheduler_IsRunning ENDP

;-----------------------------------------------------------------------------
; bool Scheduler_Start(void)
;-----------------------------------------------------------------------------
ALIGN_16
Scheduler_Start PROC
    push    rbp
    mov     rbp, rsp
    push    rbx

    ; Acquire spinlock
    lea     rbx, [g_scheduler_lock]
sst_spin:
    xor     eax, eax
    mov     ecx, 1
    lock cmpxchg DWORD PTR [rbx], ecx
    je      sst_locked
    pause
    jmp     sst_spin
sst_locked:

    ; If already running (state==1), release lock, return 0
    lea     rcx, [g_scheduler_state]
    mov     eax, DWORD PTR [rcx]
    cmp     eax, 1
    jne     sst_do

    ; Already running - release lock, return failure
    mov     DWORD PTR [rbx], 0
    mfence
    xor     eax, eax                    ; return 0 (failure)
    pop     rbx
    pop     rbp
    ret

sst_do:
    ; Set state to 1 (running)
    lea     rcx, [g_scheduler_state]
    mov     DWORD PTR [rcx], 1

    ; Record TSC epoch
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    lea     rcx, [g_scheduler_epoch]
    mov     QWORD PTR [rcx], rax

    ; Zero task_head, task_tail, task_count
    lea     rcx, [g_scheduler_task_head]
    mov     QWORD PTR [rcx], 0
    lea     rcx, [g_scheduler_task_tail]
    mov     QWORD PTR [rcx], 0
    lea     rcx, [g_scheduler_task_count]
    mov     DWORD PTR [rcx], 0

    ; Release spinlock
    mov     DWORD PTR [rbx], 0
    mfence

    mov     eax, 1                      ; return 1 (success)
    pop     rbx
    pop     rbp
    ret
Scheduler_Start ENDP

;-----------------------------------------------------------------------------
; void Scheduler_Stop(void)
;-----------------------------------------------------------------------------
ALIGN_16
Scheduler_Stop PROC
    push    rbx

    ; Acquire spinlock
    lea     rbx, [g_scheduler_lock]
ssp_spin:
    xor     eax, eax
    mov     ecx, 1
    lock cmpxchg DWORD PTR [rbx], ecx
    je      ssp_locked
    pause
    jmp     ssp_spin
ssp_locked:

    ; If not running or paused, release lock, return
    lea     rcx, [g_scheduler_state]
    mov     eax, DWORD PTR [rcx]
    cmp     eax, 1
    je      ssp_do
    cmp     eax, 2
    je      ssp_do

    ; Not running/paused - release lock, return
    mov     DWORD PTR [rbx], 0
    mfence
    pop     rbx
    ret

ssp_do:
    ; Set state to 0 (stopped)
    lea     rcx, [g_scheduler_state]
    mov     DWORD PTR [rcx], 0

    ; Zero task_head, task_tail, task_count
    lea     rcx, [g_scheduler_task_head]
    mov     QWORD PTR [rcx], 0
    lea     rcx, [g_scheduler_task_tail]
    mov     QWORD PTR [rcx], 0
    lea     rcx, [g_scheduler_task_count]
    mov     DWORD PTR [rcx], 0

    ; Release spinlock
    mov     DWORD PTR [rbx], 0
    mfence

    pop     rbx
    ret
Scheduler_Stop ENDP

;-----------------------------------------------------------------------------
; void Scheduler_Pause(void)
;-----------------------------------------------------------------------------
ALIGN_16
Scheduler_Pause PROC
    push    rbx

    ; Acquire spinlock
    lea     rbx, [g_scheduler_lock]
spa_spin:
    xor     eax, eax
    mov     ecx, 1
    lock cmpxchg DWORD PTR [rbx], ecx
    je      spa_locked
    pause
    jmp     spa_spin
spa_locked:

    ; If state != 1 (not running), release lock, return
    lea     rcx, [g_scheduler_state]
    mov     eax, DWORD PTR [rcx]
    cmp     eax, 1
    je      spa_do

    ; Not running - release lock, return
    mov     DWORD PTR [rbx], 0
    mfence
    pop     rbx
    ret

spa_do:
    ; Set state to 2 (paused)
    lea     rcx, [g_scheduler_state]
    mov     DWORD PTR [rcx], 2

    ; Release spinlock
    mov     DWORD PTR [rbx], 0
    mfence

    pop     rbx
    ret
Scheduler_Pause ENDP

;-----------------------------------------------------------------------------
; void Scheduler_Resume(void)
;-----------------------------------------------------------------------------
ALIGN_16
Scheduler_Resume PROC
    push    rbx

    ; Acquire spinlock
    lea     rbx, [g_scheduler_lock]
sre_spin:
    xor     eax, eax
    mov     ecx, 1
    lock cmpxchg DWORD PTR [rbx], ecx
    je      sre_locked
    pause
    jmp     sre_spin
sre_locked:

    ; If state != 2 (not paused), release lock, return
    lea     rcx, [g_scheduler_state]
    mov     eax, DWORD PTR [rcx]
    cmp     eax, 2
    je      sre_do

    ; Not paused - release lock, return
    mov     DWORD PTR [rbx], 0
    mfence
    pop     rbx
    ret

sre_do:
    ; Set state to 1 (running)
    lea     rcx, [g_scheduler_state]
    mov     DWORD PTR [rcx], 1

    ; Release spinlock
    mov     DWORD PTR [rbx], 0
    mfence

    pop     rbx
    ret
Scheduler_Resume ENDP

;-----------------------------------------------------------------------------
; uint64_t Scheduler_GetTickCount(void)
;-----------------------------------------------------------------------------
ALIGN_16
Scheduler_GetTickCount PROC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    ret
Scheduler_GetTickCount ENDP

;-----------------------------------------------------------------------------
; void Scheduler_Yield(void)
;-----------------------------------------------------------------------------
ALIGN_16
Scheduler_Yield PROC
    pause
    ret
Scheduler_Yield ENDP

; ============================================================================
; SECTION 3: Heartbeat Functions
; ============================================================================

;-----------------------------------------------------------------------------
; void Heartbeat_Tick(void)
;-----------------------------------------------------------------------------
ALIGN_16
Heartbeat_Tick PROC
    ; Atomic increment of heartbeat counter
    lea     rcx, [g_heartbeat_counter]
    lock inc QWORD PTR [rcx]

    ; Update last TSC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    lea     rcx, [g_heartbeat_last_tsc]
    mov     QWORD PTR [rcx], rax
    ret
Heartbeat_Tick ENDP

;-----------------------------------------------------------------------------
; void Heartbeat_Start(void)
;-----------------------------------------------------------------------------
ALIGN_16
Heartbeat_Start PROC
    push    rbp
    mov     rbp, rsp

    ; Set g_heartbeat_active = 1
    lea     rcx, [g_heartbeat_active]
    mov     DWORD PTR [rcx], 1

    ; Read rdtsc and store in g_heartbeat_last_tsc
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    lea     rcx, [g_heartbeat_last_tsc]
    mov     QWORD PTR [rcx], rax

    ; Set g_heartbeat_threshold ~5 sec at ~3GHz = 15,000,000,000 = 0x37E11D600
    lea     rcx, [g_heartbeat_threshold]
    mov     rax, 037E11D600h
    mov     QWORD PTR [rcx], rax

    ; Zero g_heartbeat_counter
    lea     rcx, [g_heartbeat_counter]
    mov     QWORD PTR [rcx], 0

    pop     rbp
    ret
Heartbeat_Start ENDP

;-----------------------------------------------------------------------------
; void Heartbeat_Stop(void)
;-----------------------------------------------------------------------------
ALIGN_16
Heartbeat_Stop PROC
    ; Set g_heartbeat_active to 0
    lea     rcx, [g_heartbeat_active]
    mov     DWORD PTR [rcx], 0
    mfence

    ; Zero g_heartbeat_last_tsc
    lea     rcx, [g_heartbeat_last_tsc]
    mov     QWORD PTR [rcx], 0
    ret
Heartbeat_Stop ENDP

;-----------------------------------------------------------------------------
; bool Heartbeat_IsAlive(void)
;-----------------------------------------------------------------------------
ALIGN_16
Heartbeat_IsAlive PROC
    ; If g_heartbeat_active == 0, return 0
    lea     rcx, [g_heartbeat_active]
    mov     eax, DWORD PTR [rcx]
    test    eax, eax
    jz      hia_dead

    ; Read current TSC
    rdtsc
    shl     rdx, 32
    or      rax, rdx                    ; rax = current TSC

    ; Compute delta = current_tsc - g_heartbeat_last_tsc
    lea     rcx, [g_heartbeat_last_tsc]
    mov     rcx, QWORD PTR [rcx]
    sub     rax, rcx                    ; rax = delta

    ; If delta > g_heartbeat_threshold, return 0 (dead)
    lea     rcx, [g_heartbeat_threshold]
    mov     rcx, QWORD PTR [rcx]
    cmp     rax, rcx
    ja      hia_dead

    ; Alive
    mov     eax, 1
    ret

hia_dead:
    xor     eax, eax
    ret
Heartbeat_IsAlive ENDP

;-----------------------------------------------------------------------------
; uint64_t Heartbeat_GetCount(void)
;-----------------------------------------------------------------------------
ALIGN_16
Heartbeat_GetCount PROC
    lea     rcx, [g_heartbeat_counter]
    mov     rax, QWORD PTR [rcx]
    ret
Heartbeat_GetCount ENDP

;-----------------------------------------------------------------------------
; void Heartbeat_Reset(void)
;-----------------------------------------------------------------------------
ALIGN_16
Heartbeat_Reset PROC
    lea     rcx, [g_heartbeat_counter]
    mov     QWORD PTR [rcx], 0
    ret
Heartbeat_Reset ENDP

; ============================================================================
; SECTION 4: Atomic Operations
; ============================================================================

;-----------------------------------------------------------------------------
; int64_t Atomic_Add64(volatile int64_t* ptr, int64_t value)
; rcx = ptr
; rdx = value
; Returns: previous value
;-----------------------------------------------------------------------------
ALIGN_16
Atomic_Add64 PROC
    mov     rax, rdx
    lock xadd QWORD PTR [rcx], rax
    ret
Atomic_Add64 ENDP

;-----------------------------------------------------------------------------
; int64_t Atomic_Sub64(volatile int64_t* ptr, int64_t value)
;-----------------------------------------------------------------------------
ALIGN_16
Atomic_Sub64 PROC
    neg     rdx
    mov     rax, rdx
    lock xadd QWORD PTR [rcx], rax
    ret
Atomic_Sub64 ENDP

;-----------------------------------------------------------------------------
; int64_t Atomic_CmpXchg64(volatile int64_t* ptr, int64_t expected, int64_t desired)
; rcx = ptr
; rdx = expected
; r8  = desired
; Returns: original value at ptr
;-----------------------------------------------------------------------------
ALIGN_16
Atomic_CmpXchg64 PROC
    mov     rax, rdx
    lock cmpxchg QWORD PTR [rcx], r8
    ret
Atomic_CmpXchg64 ENDP

;-----------------------------------------------------------------------------
; int32_t Atomic_CmpXchg32(volatile int32_t* ptr, int32_t expected, int32_t desired)
;-----------------------------------------------------------------------------
ALIGN_16
Atomic_CmpXchg32 PROC
    mov     eax, edx
    lock cmpxchg DWORD PTR [rcx], r8d
    ret
Atomic_CmpXchg32 ENDP

;-----------------------------------------------------------------------------
; int64_t Atomic_Load64(volatile int64_t* ptr)
;-----------------------------------------------------------------------------
ALIGN_16
Atomic_Load64 PROC
    mov     rax, QWORD PTR [rcx]
    ret
Atomic_Load64 ENDP

;-----------------------------------------------------------------------------
; void Atomic_Store64(volatile int64_t* ptr, int64_t value)
;-----------------------------------------------------------------------------
ALIGN_16
Atomic_Store64 PROC
    mov     QWORD PTR [rcx], rdx
    mfence
    ret
Atomic_Store64 ENDP

;-----------------------------------------------------------------------------
; void Atomic_MemoryBarrier(void)
;-----------------------------------------------------------------------------
ALIGN_16
Atomic_MemoryBarrier PROC
    mfence
    ret
Atomic_MemoryBarrier ENDP

; ============================================================================
; SECTION 5: CPU Feature Detection
; ============================================================================

;-----------------------------------------------------------------------------
; uint32_t CPU_GetFeatures(void)
; Returns: bitfield of CPU features
;-----------------------------------------------------------------------------
ALIGN_16
CPU_GetFeatures PROC
    push    rbx
    push    rcx
    push    rdx
    
    ; Call CPUID with EAX=1
    mov     eax, 1
    cpuid
    
    ; EDX and ECX contain feature bits
    mov     eax, edx                    ; Return EDX features
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret
CPU_GetFeatures ENDP

;-----------------------------------------------------------------------------
; bool CPU_HasAVX(void)
;-----------------------------------------------------------------------------
ALIGN_16
CPU_HasAVX PROC
    push    rbx
    
    mov     eax, 1
    cpuid
    
    ; Check ECX bit 28 (AVX)
    bt      ecx, 28
    setc    al
    movzx   eax, al
    
    pop     rbx
    ret
CPU_HasAVX ENDP

;-----------------------------------------------------------------------------
; bool CPU_HasAVX2(void)
;-----------------------------------------------------------------------------
ALIGN_16
CPU_HasAVX2 PROC
    push    rbx
    
    ; Check max extended function
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    
    ; Check EBX bit 5 (AVX2)
    bt      ebx, 5
    setc    al
    movzx   eax, al
    
    pop     rbx
    ret
CPU_HasAVX2 ENDP

;-----------------------------------------------------------------------------
; bool CPU_HasAVX512(void)
;-----------------------------------------------------------------------------
ALIGN_16
CPU_HasAVX512 PROC
    push    rbx
    
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    
    ; Check EBX bit 16 (AVX512F)
    bt      ebx, 16
    setc    al
    movzx   eax, al
    
    pop     rbx
    ret
CPU_HasAVX512 ENDP

;-----------------------------------------------------------------------------
; uint32_t CPU_GetCoreCount(void)
;-----------------------------------------------------------------------------
ALIGN_16
CPU_GetCoreCount PROC
    push    rbx
    
    ; Get processor info
    mov     eax, 1
    cpuid
    
    ; EBX[23:16] contains logical processor count
    shr     ebx, 16
    and     ebx, 0FFh
    mov     eax, ebx
    
    ; Ensure at least 1
    test    eax, eax
    jnz     core_count_done
    mov     eax, 1
    
core_count_done:
    pop     rbx
    ret
CPU_GetCoreCount ENDP

; ============================================================================
; SECTION 6: Low-Level Utilities
; ============================================================================

;-----------------------------------------------------------------------------
; void FastMemcpy(void* dst, const void* src, uint64_t count)
; rcx = dst
; rdx = src
; r8  = count
;-----------------------------------------------------------------------------
ALIGN_16
FastMemcpy PROC
    push    rdi
    push    rsi
    
    mov     rdi, rcx                    ; dst
    mov     rsi, rdx                    ; src
    mov     rcx, r8                     ; count
    
    ; Use REP MOVSB for simplicity - CPU will optimize
    cld
    rep     movsb
    
    pop     rsi
    pop     rdi
    ret
FastMemcpy ENDP

;-----------------------------------------------------------------------------
; void FastMemset(void* dst, int value, uint64_t count)
; rcx = dst
; rdx = value (only low byte used)
; r8  = count
;-----------------------------------------------------------------------------
ALIGN_16
FastMemset PROC
    push    rdi
    
    mov     rdi, rcx                    ; dst
    mov     eax, edx                    ; value
    mov     rcx, r8                     ; count
    
    cld
    rep     stosb
    
    pop     rdi
    ret
FastMemset ENDP

;-----------------------------------------------------------------------------
; int FastMemcmp(const void* a, const void* b, uint64_t count)
; Returns: 0 if equal, <0 if a<b, >0 if a>b
;-----------------------------------------------------------------------------
ALIGN_16
FastMemcmp PROC
    push    rdi
    push    rsi
    
    mov     rdi, rcx                    ; a
    mov     rsi, rdx                    ; b
    mov     rcx, r8                     ; count
    
    cld
    repe    cmpsb
    
    ; Get comparison result
    movzx   eax, BYTE PTR [rdi - 1]
    movzx   edx, BYTE PTR [rsi - 1]
    sub     eax, edx
    
    pop     rsi
    pop     rdi
    ret
FastMemcmp ENDP

;-----------------------------------------------------------------------------
; uint64_t GetTimestampCounter(void)
;-----------------------------------------------------------------------------
ALIGN_16
GetTimestampCounter PROC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    ret
GetTimestampCounter ENDP

;-----------------------------------------------------------------------------
; void SpinWait(uint32_t iterations)
;-----------------------------------------------------------------------------
ALIGN_16
SpinWait PROC
    mov     eax, ecx
spin_loop:
    pause
    dec     eax
    jnz     spin_loop
    ret
SpinWait ENDP

;-----------------------------------------------------------------------------
; void PrefetchData(const void* ptr)
;-----------------------------------------------------------------------------
ALIGN_16
PrefetchData PROC
    prefetcht0 [rcx]
    ret
PrefetchData ENDP

;-----------------------------------------------------------------------------
; void FlushCacheLine(const void* ptr)
;-----------------------------------------------------------------------------
ALIGN_16
FlushCacheLine PROC
    clflush [rcx]
    ret
FlushCacheLine ENDP

; ============================================================================
; SECTION 7: Stack and Frame Helpers
; ============================================================================

;-----------------------------------------------------------------------------
; uint64_t GetStackPointer(void)
;-----------------------------------------------------------------------------
ALIGN_16
GetStackPointer PROC
    mov     rax, rsp
    add     rax, 8                      ; Account for return address
    ret
GetStackPointer ENDP

;-----------------------------------------------------------------------------
; uint64_t GetFramePointer(void)
;-----------------------------------------------------------------------------
ALIGN_16
GetFramePointer PROC
    mov     rax, rbp
    ret
GetFramePointer ENDP

;-----------------------------------------------------------------------------
; uint64_t GetReturnAddress(void)
;-----------------------------------------------------------------------------
ALIGN_16
GetReturnAddress PROC
    mov     rax, QWORD PTR [rsp]
    ret
GetReturnAddress ENDP

; ============================================================================
; SECTION 8: Inference Kernel Helpers
; ============================================================================

;-----------------------------------------------------------------------------
; void MatMulKernel_AVX2(float* C, const float* A, const float* B,
;                        uint32_t M, uint32_t N, uint32_t K)
; rcx = C (output M×N), rdx = A (M×K), r8 = B (K×N)
; r9d = M, [rbp+48] = N, [rbp+56] = K
; Full AVX2/FMA GEMM: C = A * B
;-----------------------------------------------------------------------------
ALIGN_16
MatMulKernel_AVX2 PROC
    ; ---- prologue (manual, OPTION PROLOGUE:NONE) ----
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 24                          ; locals + 16-byte align
    ; [rsp+0] = N_stride (QWORD, N*sizeof(float))

    ; ---- save parameters ----
    mov     rbx, rcx                         ; rbx = C
    mov     rsi, rdx                         ; rsi = A
    mov     rdi, r8                          ; rdi = B
    mov     r12d, r9d                        ; r12 = M
    mov     r13d, DWORD PTR [rbp+48]         ; r13 = N  (5th param)
    mov     r14d, DWORD PTR [rbp+56]         ; r14 = K  (6th param)

    ; Precompute byte stride between B rows
    mov     rax, r13
    shl     rax, 2
    mov     QWORD PTR [rsp], rax             ; N_stride = N * 4

    ; Bail on zero dimensions
    test    r12d, r12d
    jz      mm_done
    test    r13d, r13d
    jz      mm_done
    test    r14d, r14d
    jz      mm_done

    mov     r11, QWORD PTR [rsp]             ; r11 = N_stride (hoisted)
    xor     r15d, r15d                       ; i = 0 (row index)

mm_row_loop:
    ; A_row = A + i*K (floats)
    mov     rax, r15
    imul    rax, r14
    lea     r8, [rsi + rax*4]                ; r8 = &A[i*K]

    ; C_row = C + i*N (floats)
    mov     rax, r15
    imul    rax, r13
    lea     r9, [rbx + rax*4]                ; r9 = &C[i*N]

    xor     ecx, ecx                         ; j = 0 (column index)
    mov     edx, r13d
    and     edx, 0FFFFFFF8h                   ; edx = N_aligned = N & ~7

    cmp     ecx, edx
    jge     mm_tail_cols

mm_col_loop:
    vxorps  ymm0, ymm0, ymm0                ; accumulator = 0
    lea     r10, [rdi + rcx*4]               ; r10 = B_ptr = &B[0, j]
    xor     eax, eax                          ; k = 0

mm_k_loop:
    ; Broadcast A[i*K + k] across 8 lanes
    vbroadcastss ymm1, DWORD PTR [r8 + rax*4]
    ; Load B[k*N + j .. j+7]
    vmovups ymm2, YMMWORD PTR [r10]
    ; acc += A_val * B_vec  (fused multiply-add)
    vfmadd231ps ymm0, ymm1, ymm2

    add     r10, r11                          ; B_ptr += N_stride
    inc     eax
    cmp     eax, r14d                         ; k < K?
    jb      mm_k_loop

    ; Store C[i*N + j .. j+7]
    vmovups YMMWORD PTR [r9 + rcx*4], ymm0

    add     ecx, 8
    cmp     ecx, edx                          ; j < N_aligned?
    jb      mm_col_loop

mm_tail_cols:
    ; Scalar fallback for remaining N mod 8 columns
    cmp     ecx, r13d
    jge     mm_next_row

mm_tail_loop:
    vxorps  xmm0, xmm0, xmm0                ; scalar accumulator = 0
    lea     r10, [rdi + rcx*4]               ; B_ptr = &B[0, j]
    xor     eax, eax                          ; k = 0

mm_tail_k:
    vmovss  xmm1, DWORD PTR [r8 + rax*4]    ; A[i*K+k]
    vmovss  xmm2, DWORD PTR [r10]            ; B[k*N+j]
    vfmadd231ss xmm0, xmm1, xmm2            ; acc += A*B
    add     r10, r11
    inc     eax
    cmp     eax, r14d
    jb      mm_tail_k

    vmovss  DWORD PTR [r9 + rcx*4], xmm0    ; C[i*N+j] = acc
    inc     ecx
    cmp     ecx, r13d
    jb      mm_tail_loop

mm_next_row:
    inc     r15d
    cmp     r15d, r12d
    jb      mm_row_loop

mm_done:
    vzeroupper
    add     rsp, 24
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
MatMulKernel_AVX2 ENDP

;-----------------------------------------------------------------------------
; void SoftmaxKernel_AVX2(float* output, const float* input, uint32_t size)
; rcx = output, rdx = input, r8d = size
; Numerically stable softmax: max → exp(x-max) → normalise
; Uses Cephes polynomial exp approximation with AVX2/FMA
;-----------------------------------------------------------------------------
ALIGN_16
SoftmaxKernel_AVX2 PROC
    ; ---- prologue ----
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    r12
    sub     rsp, 40                          ; locals + 16-byte align
    ; [rsp+0]  = max_val  (DWORD, float)
    ; [rsp+4]  = sum_val  (DWORD, float)

    ; ---- save parameters ----
    mov     rbx, rcx                         ; rbx = output
    mov     rsi, rdx                         ; rsi = input
    mov     r12d, r8d                        ; r12 = size

    ; Bail on zero size
    test    r12d, r12d
    jz      sm_done

    ; ================================================================
    ; Pass 1: find max(input[0 .. size-1])
    ; ================================================================
    vbroadcastss ymm0, DWORD PTR [rsi]       ; init all lanes = input[0]
    xor     ecx, ecx                          ; i = 0
    mov     edx, r12d
    and     edx, 0FFFFFFF8h                   ; size_aligned

    cmp     edx, 8
    jb      sm_max_hreduce

sm_max_vec:
    vmovups ymm1, YMMWORD PTR [rsi + rcx*4]
    vmaxps  ymm0, ymm0, ymm1
    add     ecx, 8
    cmp     ecx, edx
    jb      sm_max_vec

sm_max_hreduce:
    ; Horizontal max of ymm0 → xmm0[0]
    vextractf128 xmm1, ymm0, 1
    vmaxps  xmm0, xmm0, xmm1
    vpermilps xmm1, xmm0, 4Eh                ; swap 64-bit halves
    vmaxps  xmm0, xmm0, xmm1
    vpermilps xmm1, xmm0, 0B1h               ; swap adjacent 32-bit
    vmaxps  xmm0, xmm0, xmm1

    ; Scalar tail for max
sm_max_scalar:
    cmp     ecx, r12d
    jge     sm_max_done
    vmovss  xmm1, DWORD PTR [rsi + rcx*4]
    vmaxss  xmm0, xmm0, xmm1
    inc     ecx
    jmp     sm_max_scalar

sm_max_done:
    vmovss  DWORD PTR [rsp], xmm0            ; store max

    ; ================================================================
    ; Pass 2: exp(x - max) for each element, accumulate sum
    ; ================================================================
    vxorps  ymm5, ymm5, ymm5                ; ymm5 = sum accumulator
    xor     ecx, ecx
    mov     edx, r12d
    and     edx, 0FFFFFFF8h

    cmp     edx, 8
    jb      sm_exp_hsum

sm_exp_vec:
    ; Load 8 inputs and subtract max
    vmovups ymm0, YMMWORD PTR [rsi + rcx*4]
    vbroadcastss ymm1, DWORD PTR [rsp]       ; broadcast max
    vsubps  ymm0, ymm0, ymm1                 ; x = input - max

    ; ---- inline vectorised exp(ymm0) → ymm0 ----
    ; Clamp to prevent overflow / underflow
    vminps  ymm0, ymm0, YMMWORD PTR [sm_exp_hi]
    vmaxps  ymm0, ymm0, YMMWORD PTR [sm_exp_lo]

    ; n = round(x * log2(e))
    vmulps  ymm1, ymm0, YMMWORD PTR [sm_log2ef]
    vroundps ymm1, ymm1, 0                   ; round to nearest

    ; Cahan range reduction: x = x - n*C1 - n*C2
    vmulps  ymm2, ymm1, YMMWORD PTR [sm_exp_C1]
    vsubps  ymm0, ymm0, ymm2
    vmulps  ymm2, ymm1, YMMWORD PTR [sm_exp_C2]
    vsubps  ymm0, ymm0, ymm2

    ; z = x*x  (for final composition)
    vmulps  ymm3, ymm0, ymm0

    ; Horner polynomial p(x) = p0·x⁵ + p1·x⁴ + … + p5
    vmovups ymm2, YMMWORD PTR [sm_exp_p0]
    vfmadd213ps ymm2, ymm0, YMMWORD PTR [sm_exp_p1]
    vfmadd213ps ymm2, ymm0, YMMWORD PTR [sm_exp_p2]
    vfmadd213ps ymm2, ymm0, YMMWORD PTR [sm_exp_p3]
    vfmadd213ps ymm2, ymm0, YMMWORD PTR [sm_exp_p4]
    vfmadd213ps ymm2, ymm0, YMMWORD PTR [sm_exp_p5]

    ; result = p(x)·z + x + 1
    vfmadd213ps ymm2, ymm3, ymm0
    vaddps  ymm2, ymm2, YMMWORD PTR [sm_exp_one]

    ; Scale by 2^n  (integer manipulation of float exponent)
    vcvtps2dq ymm4, ymm1                     ; int(n)
    vpaddd  ymm4, ymm4, YMMWORD PTR [sm_exp_127i]  ; +127 bias
    vpslld  ymm4, ymm4, 23                   ; shift to exponent field
    vmulps  ymm0, ymm2, ymm4                 ; poly * 2^n
    ; ---- end inline exp ----

    vaddps  ymm5, ymm5, ymm0                 ; sum += exp(x-max)
    vmovups YMMWORD PTR [rbx + rcx*4], ymm0  ; store to output

    add     ecx, 8
    cmp     ecx, edx
    jb      sm_exp_vec

sm_exp_hsum:
    ; Horizontal sum of ymm5 → xmm0[0]
    vextractf128 xmm0, ymm5, 1
    vaddps  xmm0, xmm0, xmm5
    vpermilps xmm1, xmm0, 4Eh
    vaddps  xmm0, xmm0, xmm1
    vpermilps xmm1, xmm0, 0B1h
    vaddss  xmm0, xmm0, xmm1
    vmovss  DWORD PTR [rsp+4], xmm0          ; store partial sum

    ; Scalar tail: exp + sum for remaining elements
sm_exp_tail:
    cmp     ecx, r12d
    jge     sm_exp_tail_done

    vmovss  xmm0, DWORD PTR [rsi + rcx*4]
    vsubss  xmm0, xmm0, DWORD PTR [rsp]     ; x - max

    ; ---- inline scalar exp(xmm0) → xmm0 ----
    vminss  xmm0, xmm0, DWORD PTR [sm_exp_hi]
    vmaxss  xmm0, xmm0, DWORD PTR [sm_exp_lo]
    vmulss  xmm1, xmm0, DWORD PTR [sm_log2ef]
    vroundss xmm1, xmm1, xmm1, 0
    vmulss  xmm2, xmm1, DWORD PTR [sm_exp_C1]
    vsubss  xmm0, xmm0, xmm2
    vmulss  xmm2, xmm1, DWORD PTR [sm_exp_C2]
    vsubss  xmm0, xmm0, xmm2
    vmulss  xmm3, xmm0, xmm0                 ; z = x*x
    vmovss  xmm2, DWORD PTR [sm_exp_p0]
    vfmadd213ss xmm2, xmm0, DWORD PTR [sm_exp_p1]
    vfmadd213ss xmm2, xmm0, DWORD PTR [sm_exp_p2]
    vfmadd213ss xmm2, xmm0, DWORD PTR [sm_exp_p3]
    vfmadd213ss xmm2, xmm0, DWORD PTR [sm_exp_p4]
    vfmadd213ss xmm2, xmm0, DWORD PTR [sm_exp_p5]
    vfmadd213ss xmm2, xmm3, xmm0
    vaddss  xmm2, xmm2, DWORD PTR [sm_exp_one]
    vcvtss2si eax, xmm1
    add     eax, 127
    shl     eax, 23
    vmovd   xmm4, eax
    vmulss  xmm0, xmm2, xmm4
    ; ---- end scalar exp ----

    vmovss  DWORD PTR [rbx + rcx*4], xmm0   ; store exp result
    vmovss  xmm1, DWORD PTR [rsp+4]
    vaddss  xmm1, xmm1, xmm0
    vmovss  DWORD PTR [rsp+4], xmm1          ; sum += exp

    inc     ecx
    jmp     sm_exp_tail

sm_exp_tail_done:

    ; ================================================================
    ; Pass 3: divide every element by sum  →  output[i] /= sum
    ; ================================================================
    vmovss  xmm1, DWORD PTR [rsp+4]         ; total sum
    vbroadcastss ymm1, xmm1                  ; broadcast to 8 lanes

    xor     ecx, ecx
    mov     edx, r12d
    and     edx, 0FFFFFFF8h

    cmp     edx, 8
    jb      sm_div_tail

sm_div_vec:
    vmovups ymm0, YMMWORD PTR [rbx + rcx*4]
    vdivps  ymm0, ymm0, ymm1
    vmovups YMMWORD PTR [rbx + rcx*4], ymm0
    add     ecx, 8
    cmp     ecx, edx
    jb      sm_div_vec

sm_div_tail:
    cmp     ecx, r12d
    jge     sm_done

sm_div_scalar:
    vmovss  xmm0, DWORD PTR [rbx + rcx*4]
    vdivss  xmm0, xmm0, xmm1                ; xmm1[0] = sum
    vmovss  DWORD PTR [rbx + rcx*4], xmm0
    inc     ecx
    cmp     ecx, r12d
    jb      sm_div_scalar

sm_done:
    vzeroupper
    add     rsp, 40
    pop     r12
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SoftmaxKernel_AVX2 ENDP

;-----------------------------------------------------------------------------
; void RoPEKernel_AVX2(float* output, const float* input,
;                      const float* freqs, uint32_t dim)
; rcx = output, rdx = input, r8 = freqs (interleaved cos/sin), r9d = dim
; Rotary Position Embedding: complex rotation of adjacent pairs.
;   out[2i]   = x0*cos - x1*sin
;   out[2i+1] = x0*sin + x1*cos
; where freqs = [cos0, sin0, cos1, sin1, …]
;-----------------------------------------------------------------------------
ALIGN_16
RoPEKernel_AVX2 PROC
    ; ---- prologue ----
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    push    r12
    ; 5 pushes total → stack is 16-byte aligned, no sub rsp needed

    ; ---- save parameters ----
    mov     rbx, rcx                         ; rbx = output
    mov     rsi, rdx                         ; rsi = input
    mov     rdi, r8                          ; rdi = freqs
    mov     r12d, r9d                        ; r12 = dim (element count, even)

    ; Need at least one pair (2 floats)
    cmp     r12d, 2
    jb      rp_done

    xor     ecx, ecx                         ; offset = 0 (float index)
    mov     edx, r12d
    and     edx, 0FFFFFFF8h                   ; dim_aligned = dim & ~7

    cmp     edx, 8
    jb      rp_tail

rp_vec_loop:
    ; Load 4 pairs (8 floats) of input
    vmovups ymm0, YMMWORD PTR [rsi + rcx*4]
    ; Load 4 cos/sin pairs from freqs (interleaved)
    vmovups ymm3, YMMWORD PTR [rdi + rcx*4]

    ; Separate even/odd input elements (within 128-bit lanes)
    ; x_even = [x0,x0,x2,x2 | x4,x4,x6,x6]
    vshufps ymm1, ymm0, ymm0, 0A0h
    ; x_odd  = [x1,x1,x3,x3 | x5,x5,x7,x7]
    vshufps ymm2, ymm0, ymm0, 0F5h

    ; Swap cos↔sin in freq vector
    ; freqs_swap = [s0,c0,s1,c1 | s2,c2,s3,c3]
    vshufps ymm4, ymm3, ymm3, 0B1h

    ; Multiply paths
    vmulps  ymm1, ymm1, ymm3                 ; [x0c0, x0s0, x2c1, x2s1, …]
    vmulps  ymm2, ymm2, ymm4                 ; [x1s0, x1c0, x3s1, x3c1, …]

    ; Combine: even lanes subtract, odd lanes add  →  complex multiply
    ; vaddsubps dest[even] = src1 - src2, dest[odd] = src1 + src2
    vaddsubps ymm0, ymm1, ymm2

    ; Store 8 result floats
    vmovups YMMWORD PTR [rbx + rcx*4], ymm0

    add     ecx, 8
    cmp     ecx, edx
    jb      rp_vec_loop

rp_tail:
    ; Scalar fallback: process one pair (2 floats) at a time
    cmp     ecx, r12d
    jge     rp_done

rp_tail_loop:
    vmovss  xmm0, DWORD PTR [rsi + rcx*4]        ; x0
    vmovss  xmm1, DWORD PTR [rsi + rcx*4 + 4]    ; x1
    vmovss  xmm2, DWORD PTR [rdi + rcx*4]        ; cos_i
    vmovss  xmm3, DWORD PTR [rdi + rcx*4 + 4]    ; sin_i

    ; out[2i] = x0*cos - x1*sin
    vmulss  xmm4, xmm0, xmm2                     ; x0*cos
    vfnmadd231ss xmm4, xmm1, xmm3                ; -= x1*sin
    vmovss  DWORD PTR [rbx + rcx*4], xmm4

    ; out[2i+1] = x0*sin + x1*cos
    vmulss  xmm4, xmm1, xmm2                     ; x1*cos
    vfmadd231ss xmm4, xmm0, xmm3                 ; += x0*sin
    vmovss  DWORD PTR [rbx + rcx*4 + 4], xmm4

    add     ecx, 2
    cmp     ecx, r12d
    jb      rp_tail_loop

rp_done:
    vzeroupper
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
RoPEKernel_AVX2 ENDP

; ============================================================================
; Constant data for AVX2 softmax exp approximation (Cephes coefficients)
; Each constant is replicated 8× to fill a 256-bit YMM register.
; ============================================================================
.data

; Scheduler state
g_scheduler_state       DWORD   0       ; 0=stopped, 1=running, 2=paused
g_scheduler_task_head   QWORD   0       ; Head of task ring buffer
g_scheduler_task_tail   QWORD   0       ; Tail of task ring buffer
g_scheduler_task_count  DWORD   0       ; Number of pending tasks
g_scheduler_lock        DWORD   0       ; Spinlock for scheduler
g_scheduler_epoch       QWORD   0       ; TSC at scheduler start

; Heartbeat state
g_heartbeat_counter     QWORD   0       ; Monotonic heartbeat counter
g_heartbeat_active      DWORD   0       ; 0=stopped, 1=active
g_heartbeat_last_tsc    QWORD   0       ; TSC at last heartbeat tick
g_heartbeat_threshold   QWORD   0       ; Max TSC delta before declaring dead (5 sec worth of cycles)

ALIGN 16
sm_exp_hi       REAL4 8 DUP(88.3762626647949)
sm_exp_lo       REAL4 8 DUP(-88.3762626647949)
sm_log2ef       REAL4 8 DUP(1.44269504088896341)
sm_exp_C1       REAL4 8 DUP(0.693359375)
sm_exp_C2       REAL4 8 DUP(-2.12194440E-4)
sm_exp_p0       REAL4 8 DUP(1.9875691500E-4)
sm_exp_p1       REAL4 8 DUP(1.3981999507E-3)
sm_exp_p2       REAL4 8 DUP(8.3334519073E-3)
sm_exp_p3       REAL4 8 DUP(4.1665795894E-2)
sm_exp_p4       REAL4 8 DUP(1.6666665459E-1)
sm_exp_p5       REAL4 8 DUP(5.0000001201E-1)
sm_exp_one      REAL4 8 DUP(1.0)
sm_exp_127i     DWORD 8 DUP(127)

.code

; ============================================================================
; External imports
; ============================================================================

EXTERN __imp_VirtualAlloc:QWORD
EXTERN __imp_VirtualFree:QWORD

; ============================================================================
; End of unresolved_asm_stubs.asm
; ============================================================================

END
