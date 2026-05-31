; ============================================================================
; RawrXD Sovereign Spin-Lock & Memory Barrier Primitives
; Zero-CRT x64 MASM for Thread-Safe Expert Gating
; ============================================================================
; Purpose: Provide lightweight lock primitives for memory-mapped file access
;          during MoE expert selection. Uses lock bts for atomic bit locking.
; ============================================================================

; --- External Win32 Symbols ---
extern GetCurrentThreadId    : proc
extern Sleep                 : proc
extern OutputDebugStringA    : proc

; --- Constants ---
SPIN_MAX_ITERATIONS        equ 10000
SPIN_YIELD_THRESHOLD       equ 1000
SPIN_SLEEP_MS              equ 1

; --- Data Section ---
.data
    ; Error strings
    err_spin_timeout         db "[SpinLock] Timeout acquiring lock", 0
    info_spin_acquired        db "[SpinLock] Lock acquired", 0
    info_spin_released        db "[SpinLock] Lock released", 0
    
    ; Lock word (64-bit, each bit = one lock slot)
    ALIGN 8
    g_sovereignLockWord      dq 0
    
    ; Per-slot ownership tracking
    ALIGN 8
    g_lockOwners             dq 64 dup(0)    ; Thread ID per slot
    g_lockRecursionCount     dd 64 dup(0)    ; Recursion depth per slot

; --- Code Section ---
.code

; ============================================================================
; SovSpin_Init
; Initialize the spin-lock subsystem.
; ============================================================================
SovSpin_Init proc
    xor eax, eax
    mov g_sovereignLockWord, rax
    
    ; Clear ownership array
    lea rdi, g_lockOwners
    mov rcx, 64
    xor rax, rax
    rep stosq
    
    ; Clear recursion array
    lea rdi, g_lockRecursionCount
    mov rcx, 64
    xor eax, eax
    rep stosd
    
    xor eax, eax                ; Return SUCCESS
    ret
SovSpin_Init endp

; ============================================================================
; SovSpin_Acquire
; Acquire a spin-lock slot using atomic bit test-and-set.
;
; Parameters:
;   RCX = Lock slot index (0-63)
;
; Returns:
;   RAX = 1 if acquired, 0 if timeout
; ============================================================================
SovSpin_Acquire proc
    sub rsp, 40                 ; Shadow space
    
    mov r8, rcx                 ; R8 = slot index
    cmp r8, 63
    ja _spin_invalid_slot
    
    ; Check for recursive acquisition by same thread
    call GetCurrentThreadId
    mov r9, rax                 ; R9 = current thread ID
    
    lea rax, g_lockOwners
    mov rdx, [rax + r8*8]       ; Current owner
    cmp rdx, r9
    jne _spin_try_acquire
    
    ; Same thread - increment recursion count
    lea rax, g_lockRecursionCount
    inc dword ptr [rax + r8*4]
    mov rax, 1
    add rsp, 40
    ret
    
_spin_try_acquire:
    mov r10, SPIN_MAX_ITERATIONS
    
_spin_loop:
    ; Atomic bit test-and-set on g_sovereignLockWord
    mov rax, 1
    mov rcx, r8
    lock bts qword ptr [g_sovereignLockWord], rcx
    
    ; If CF=0, bit was clear (lock acquired)
    jnc _spin_acquired
    
    ; Lock is held - spin
    dec r10
    jz _spin_timeout
    
    ; Yield after threshold
    cmp r10, SPIN_YIELD_THRESHOLD
    jg _spin_pause
    
    ; Sleep for 1ms to prevent CPU burn
    mov rcx, SPIN_SLEEP_MS
    call Sleep
    jmp _spin_loop
    
_spin_pause:
    ; CPU pause hint for hyperthreading efficiency
    pause
    jmp _spin_loop
    
_spin_acquired:
    ; Record ownership
    lea rax, g_lockOwners
    mov [rax + r8*8], r9
    lea rax, g_lockRecursionCount
    mov dword ptr [rax + r8*4], 1
    
    ; Memory barrier to ensure stores are visible
    lock add qword ptr [rsp], 0
    
    mov rax, 1
    add rsp, 40
    ret
    
_spin_timeout:
    lea rcx, err_spin_timeout
    call OutputDebugStringA
    xor eax, eax
    add rsp, 40
    ret
    
_spin_invalid_slot:
    xor eax, eax
    add rsp, 40
    ret
SovSpin_Acquire endp

; ============================================================================
; SovSpin_Release
; Release a spin-lock slot.
;
; Parameters:
;   RCX = Lock slot index (0-63)
; ============================================================================
SovSpin_Release proc
    sub rsp, 40
    
    mov r8, rcx                 ; R8 = slot index
    cmp r8, 63
    ja _spin_release_done
    
    ; Decrement recursion count
    lea rax, g_lockRecursionCount
    mov edx, [rax + r8*4]
    cmp edx, 0
    jle _spin_release_done
    
    dec edx
    mov [rax + r8*4], edx
    cmp edx, 0
    jg _spin_release_done      ; Still recursive - don't release
    
    ; Clear ownership
    lea rax, g_lockOwners
    mov qword ptr [rax + r8*8], 0
    
    ; Memory barrier before releasing lock
    lock add qword ptr [rsp], 0
    
    ; Atomic bit clear
    mov rax, 1
    mov rcx, r8
    lock btr qword ptr [g_sovereignLockWord], rcx
    
_spin_release_done:
    add rsp, 40
    ret
SovSpin_Release endp

; ============================================================================
; SovSpin_TryAcquire
; Non-blocking attempt to acquire a lock slot.
;
; Parameters:
;   RCX = Lock slot index (0-63)
;
; Returns:
;   RAX = 1 if acquired, 0 if already held
; ============================================================================
SovSpin_TryAcquire proc
    sub rsp, 40
    
    mov r8, rcx
    cmp r8, 63
    ja _try_fail
    
    ; Atomic bit test-and-set
    mov rax, 1
    mov rcx, r8
    lock bts qword ptr [g_sovereignLockWord], rcx
    
    ; If CF=0, bit was clear (acquired)
    jnc _try_success
    
_try_fail:
    xor eax, eax
    add rsp, 40
    ret
    
_try_success:
    call GetCurrentThreadId
    lea rdx, g_lockOwners
    mov [rdx + r8*8], rax
    lea rdx, g_lockRecursionCount
    mov dword ptr [rdx + r8*4], 1
    
    ; Memory barrier
    lock add qword ptr [rsp], 0
    
    mov rax, 1
    add rsp, 40
    ret
SovSpin_TryAcquire endp

; ============================================================================
; SovSpin_IsHeld
; Check if a lock slot is currently held.
;
; Parameters:
;   RCX = Lock slot index (0-63)
;
; Returns:
;   RAX = 1 if held, 0 if free
; ============================================================================
SovSpin_IsHeld proc
    mov r8, rcx
    cmp r8, 63
    ja _held_no
    
    mov rax, 1
    bt qword ptr [g_sovereignLockWord], r8
    jc _held_yes
    
_held_no:
    xor eax, eax
    ret
    
_held_yes:
    mov rax, 1
    ret
SovSpin_IsHeld endp

; ============================================================================
; SovSpin_GetOwner
; Get the thread ID of the lock holder.
;
; Parameters:
;   RCX = Lock slot index (0-63)
;
; Returns:
;   RAX = Thread ID, or 0 if not held
; ============================================================================
SovSpin_GetOwner proc
    mov r8, rcx
    cmp r8, 63
    ja _owner_none
    
    lea rax, g_lockOwners
    mov rax, [rax + r8*8]
    ret
    
_owner_none:
    xor eax, eax
    ret
SovSpin_GetOwner endp

; ============================================================================
; SovMem_ReadWithLock
; Read from memory-mapped region with spin-lock protection.
;
; Parameters:
;   RCX = Lock slot index
;   RDX = Source address (mapped memory)
;   R8  = Destination buffer
;   R9  = Size in bytes
;
; Returns:
;   RAX = 1 if read completed, 0 if lock failed
; ============================================================================
SovMem_ReadWithLock proc
    sub rsp, 56
    mov [rsp+48], rsi
    mov [rsp+40], rdi
    
    mov r10, rcx                ; R10 = lock slot
    mov rsi, rdx                ; RSI = source
    mov rdi, r8                 ; RDI = destination
    mov rcx, r9                 ; RCX = size
    
    ; Acquire lock
    mov rcx, r10
    call SovSpin_Acquire
    test rax, rax
    jz _read_lock_fail
    
    ; Copy memory (rep movsb is efficient for large copies)
    cld
    rep movsb
    
    ; Memory fence after read
    lock add qword ptr [rsp], 0
    
    ; Release lock
    mov rcx, r10
    call SovSpin_Release
    
    mov rax, 1
    jmp _read_done
    
_read_lock_fail:
    xor eax, eax
    
_read_done:
    mov rsi, [rsp+48]
    mov rdi, [rsp+40]
    add rsp, 56
    ret
SovMem_ReadWithLock endp

; ============================================================================
; SovMem_WriteWithLock
; Write to memory-mapped region with spin-lock protection.
;
; Parameters:
;   RCX = Lock slot index
;   RDX = Destination address (mapped memory)
;   R8  = Source buffer
;   R9  = Size in bytes
;
; Returns:
;   RAX = 1 if write completed, 0 if lock failed
; ============================================================================
SovMem_WriteWithLock proc
    sub rsp, 56
    mov [rsp+48], rsi
    mov [rsp+40], rdi
    
    mov r10, rcx                ; R10 = lock slot
    mov rdi, rdx                ; RDI = destination
    mov rsi, r8                 ; RSI = source
    mov rcx, r9                 ; RCX = size
    
    ; Acquire lock
    mov rcx, r10
    call SovSpin_Acquire
    test rax, rax
    jz _write_lock_fail
    
    ; Copy memory
    cld
    rep movsb
    
    ; Memory fence after write
    lock add qword ptr [rsp], 0
    
    ; Release lock
    mov rcx, r10
    call SovSpin_Release
    
    mov rax, 1
    jmp _write_done
    
_write_lock_fail:
    xor eax, eax
    
_write_done:
    mov rsi, [rsp+48]
    mov rdi, [rsp+40]
    add rsp, 56
    ret
SovMem_WriteWithLock endp

end
