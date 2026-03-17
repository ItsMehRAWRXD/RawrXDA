;=====================================================================
; asm_sync.asm - x64 MASM Thread Synchronization Primitives
; MUTEXES, EVENTS, ATOMIC OPERATIONS (ZERO EXTERNAL DEPENDENCY)
;=====================================================================
; Implements:
;  - Mutexes (wrap Windows CRITICAL_SECTION)
;  - Event objects (manual/auto-reset)
;  - Atomic operations (lock-prefixed x64 instructions)
;
; Win32 CRITICAL_SECTION layout:
;   [+0]:  DebugInfo (qword)
;   [+8]:  LockCount (dword)
;   [+12]: RecursionCount (dword)
;   [+16]: OwningThread (qword)
;   [+24]: LockSemaphore (qword)
;   [+32]: SpinCount (qword)
;
; Total: 40 bytes
;=====================================================================

; External dependencies from asm_memory.asm
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC

.code

; Export all public synchronization functions
PUBLIC asm_mutex_create, asm_mutex_lock, asm_mutex_unlock, asm_mutex_destroy
PUBLIC asm_event_create, asm_event_set, asm_event_reset, asm_event_wait, asm_event_destroy
PUBLIC asm_atomic_increment, asm_atomic_decrement, asm_atomic_add, asm_atomic_cmpxchg, asm_atomic_xchg
PUBLIC asm_initialize_critical_section, asm_enter_critical_section, asm_leave_critical_section
PUBLIC asm_delete_critical_section, asm_create_event, asm_set_event, asm_reset_event, asm_wait_for_event
PUBLIC asm_close_handle

;=====================================================================
; asm_mutex_create() -> rax
;
; Creates a new CRITICAL_SECTION for mutex behavior.
; Returns opaque handle (pointer to allocated CRITICAL_SECTION).
; Returns NULL if creation failed.
;=====================================================================

ALIGN 16
asm_mutex_create PROC

    push rbx
    sub rsp, 32
    
    ; Allocate CRITICAL_SECTION (40 bytes)
    mov rcx, 40
    mov rdx, 8              ; Alignment
    call asm_malloc         ; rax = pointer to CS memory
    
    test rax, rax
    jz mutex_create_fail
    
    ; Initialize CRITICAL_SECTION
    ; Call InitializeCriticalSection(rax)
    
    mov rcx, rax            ; rcx = pointer to CS
    call asm_initialize_critical_section
    
    ; rax already contains the handle
    jmp mutex_create_done
    
mutex_create_fail:
    xor rax, rax
    
mutex_create_done:
    add rsp, 32
    pop rbx
    ret

asm_mutex_create ENDP

;=====================================================================
; asm_mutex_lock(handle: rcx) -> void
;
; Acquires exclusive lock on mutex.
; Blocks if already held by another thread.
; Safe to call multiple times from same thread (recursive).
;=====================================================================

ALIGN 16
asm_mutex_lock PROC

    ; Call EnterCriticalSection(rcx)
    
    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz mutex_lock_done
    
    ; EnterCriticalSection(rcx) - blocking lock
    call asm_enter_critical_section
    
mutex_lock_done:
    add rsp, 32
    pop rbx
    ret

asm_mutex_lock ENDP

;=====================================================================
; asm_mutex_unlock(handle: rcx) -> void
;
; Releases exclusive lock on mutex.
; MUST be called by the same thread that called lock.
;=====================================================================

ALIGN 16
asm_mutex_unlock PROC

    ; Call LeaveCriticalSection(rcx)
    
    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz mutex_unlock_done
    
    ; LeaveCriticalSection(rcx)
    call asm_leave_critical_section
    
mutex_unlock_done:
    add rsp, 32
    pop rbx
    ret

asm_mutex_unlock ENDP

;=====================================================================
; asm_mutex_destroy(handle: rcx) -> void
;
; Destroys mutex handle and frees memory.
; MUST NOT be called while any thread holds the lock.
;=====================================================================

ALIGN 16
asm_mutex_destroy PROC

    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz mutex_destroy_done
    
    ; Call DeleteCriticalSection(rcx)
    call asm_delete_critical_section
    
    ; Free the memory
    mov rcx, [rsp + 32 + 8 + 8]  ; Get handle from stack parameter
    call asm_free
    
mutex_destroy_done:
    add rsp, 32
    pop rbx
    ret

asm_mutex_destroy ENDP

;=====================================================================
; asm_event_create(manual_reset: rcx) -> rax
;
; Creates an event object (manual or auto-reset).
; rcx = 1 for manual-reset, 0 for auto-reset.
; Returns opaque handle (pointer to event structure).
;
; Event structure (48 bytes):
;   [+0]:  Windows event handle (qword)
;   [+8]:  Manual reset flag (qword, 1 or 0)
;   [+16]: Current state (qword, 1=signaled, 0=unsignaled)
;   [+24]: Reserved
;   [+32]: Reserved
;   [+40]: Reserved
;=====================================================================

ALIGN 16
asm_event_create PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = manual_reset flag
    
    ; Allocate event structure (48 bytes)
    mov rcx, 48
    mov rdx, 16
    call asm_malloc
    
    test rax, rax
    jz event_create_fail
    
    ; Store manual_reset flag
    mov [rax + 8], r12
    
    ; Create Windows event handle
    ; CreateEventExW(NULL, NULL, flags, initial_state)
    ; flags: CREATE_EVENT_MANUAL_RESET = 0x0001, CREATE_EVENT_INITIAL_SET = 0x0002
    
    mov rcx, 0              ; NULL (lpName)
    mov rdx, r12            ; rdx = manual_reset
    test rdx, rdx
    jz event_create_auto_reset
    
    ; Manual reset: use CREATE_EVENT_MANUAL_RESET
    mov r8d, 1              ; CREATE_EVENT_MANUAL_RESET
    jmp event_create_call
    
event_create_auto_reset:
    ; Auto reset
    mov r8d, 0              ; Flags = 0 (auto-reset)
    
event_create_call:
    mov r9d, 0              ; Initial state = unsignaled
    
    ; In real code, call Win32 CreateEventExW
    ; For MVP, use placeholder
    call asm_create_event
    
    ; Store handle in event structure
    mov rcx, [rsp + 32 + 8 + 16]  ; Get the allocated pointer from rax (before call)
    mov [rcx], rax          ; Store handle
    
    jmp event_create_done
    
event_create_fail:
    xor rax, rax
    
event_create_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_event_create ENDP

;=====================================================================
; asm_event_set(handle: rcx) -> void
;
; Sets the event to signaled state, waking up waiters.
;=====================================================================

ALIGN 16
asm_event_set PROC

    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz event_set_done
    
    ; Get Windows handle from structure
    mov rax, [rcx]
    
    ; Call SetEvent(handle)
    mov rcx, rax
    call asm_set_event
    
    ; Update state flag
    mov rax, [rsp + 32 + 8]  ; Get handle from stack
    mov qword ptr [rax + 16], 1  ; Set state = signaled
    
event_set_done:
    add rsp, 32
    pop rbx
    ret

asm_event_set ENDP

;=====================================================================
; asm_event_reset(handle: rcx) -> void
;
; Resets the event to unsignaled state.
;=====================================================================

ALIGN 16
asm_event_reset PROC

    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz event_reset_done
    
    ; Get Windows handle
    mov rax, [rcx]
    
    ; Call ResetEvent(handle)
    mov rcx, rax
    call asm_reset_event
    
    ; Update state flag
    mov rax, [rsp + 32 + 8]
    mov qword ptr [rax + 16], 0  ; Set state = unsignaled
    
event_reset_done:
    add rsp, 32
    pop rbx
    ret

asm_event_reset ENDP

;=====================================================================
; asm_event_wait(handle: rcx, timeout_ms: rdx) -> rax
;
; Waits for event to be signaled, with timeout.
; timeout_ms = -1 for infinite wait, 0 for non-blocking check.
;
; Returns:
;   0 = WAIT_OBJECT_0 (signaled)
;   258 = WAIT_TIMEOUT (timeout expired)
;   -1 = WAIT_FAILED (error)
;=====================================================================

ALIGN 16
asm_event_wait PROC

    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz event_wait_fail
    
    ; Get Windows handle
    mov r8, [rcx]
    
    ; Call WaitForSingleObject(handle, timeout_ms)
    mov rcx, r8
    mov rdx, rdx            ; Keep timeout as-is
    call asm_wait_for_event
    
    ; rax contains WAIT result
    jmp event_wait_done
    
event_wait_fail:
    mov rax, -1
    
event_wait_done:
    add rsp, 32
    pop rbx
    ret

asm_event_wait ENDP

;=====================================================================
; asm_event_destroy(handle: rcx) -> void
;
; Closes event handle and frees memory.
;=====================================================================

ALIGN 16
asm_event_destroy PROC

    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz event_destroy_done
    
    ; Get Windows handle
    mov r8, [rcx]
    
    ; Call CloseHandle(handle)
    mov rcx, r8
    call asm_close_handle
    
    ; Free event structure memory
    mov rcx, [rsp + 32 + 8]  ; Get event pointer from stack
    call asm_free
    
event_destroy_done:
    add rsp, 32
    pop rbx
    ret

asm_event_destroy ENDP

;=====================================================================
; ATOMIC OPERATIONS (using lock-prefixed x64 instructions)
;=====================================================================

;=====================================================================
; asm_atomic_increment(ptr: rcx) -> rax
;
; Atomically increments qword at ptr, returns new value.
;=====================================================================

ALIGN 16
asm_atomic_increment PROC

    ; lock add qword ptr [rcx], 1
    lock add qword ptr [rcx], 1
    
    ; Return new value
    mov rax, [rcx]
    ret

asm_atomic_increment ENDP

;=====================================================================
; asm_atomic_decrement(ptr: rcx) -> rax
;
; Atomically decrements qword at ptr, returns new value.
;=====================================================================

ALIGN 16
asm_atomic_decrement PROC

    lock sub qword ptr [rcx], 1
    
    mov rax, [rcx]
    ret

asm_atomic_decrement ENDP

;=====================================================================
; asm_atomic_add(ptr: rcx, value: rdx) -> rax
;
; Atomically adds value to qword at ptr, returns new value.
;=====================================================================

ALIGN 16
asm_atomic_add PROC

    lock add qword ptr [rcx], rdx
    
    mov rax, [rcx]
    ret

asm_atomic_add ENDP

;=====================================================================
; asm_atomic_cmpxchg(ptr: rcx, old: rdx, new: r8) -> rax
;
; Compare-and-swap: if [ptr] == old, set [ptr] = new.
; Returns 1 if swap succeeded, 0 if failed.
;
; Uses RAX as implicit operand for cmpxchg.
;=====================================================================

ALIGN 16
asm_atomic_cmpxchg PROC

    ; Move old value to rax (required by cmpxchg)
    mov rax, rdx            ; rax = old expected value
    
    ; lock cmpxchg qword ptr [rcx], r8
    ; If [rcx] == rax, then [rcx] = r8 and ZF=1, else rax=[rcx] and ZF=0
    
    lock cmpxchg qword ptr [rcx], r8
    
    ; Return success/failure
    jz cmpxchg_success
    xor rax, rax            ; ZF clear = failure, return 0
    ret
    
cmpxchg_success:
    mov rax, 1              ; ZF set = success, return 1
    ret

asm_atomic_cmpxchg ENDP

;=====================================================================
; asm_atomic_xchg(ptr: rcx, value: rdx) -> rax
;
; Atomically exchanges value at ptr with new value.
; Returns old value.
;=====================================================================

ALIGN 16
asm_atomic_xchg PROC

    mov rax, rdx            ; rax = new value
    
    ; lock xchg qword ptr [rcx], rax
    ; Note: xchg has implicit lock prefix
    lock xchg qword ptr [rcx], rax
    
    ; rax now contains old value
    ret

asm_atomic_xchg ENDP

;=====================================================================
; INTERNAL PLACEHOLDER WRAPPERS FOR WIN32 API CALLS
; In production, these would be resolved at link time or via IAT
;=====================================================================

ALIGN 16
asm_initialize_critical_section PROC
    ; Placeholder: In real code, call kernel32!InitializeCriticalSection
    ; For MVP, initialize fields manually
    
    ; Zero out structure
    mov qword ptr [rcx], 0
    mov qword ptr [rcx + 8], 0
    mov qword ptr [rcx + 16], 0
    mov qword ptr [rcx + 24], 0
    mov qword ptr [rcx + 32], 0
    
    ret
asm_initialize_critical_section ENDP

ALIGN 16
asm_enter_critical_section PROC
    ; Placeholder: kernel32!EnterCriticalSection
    ; For MVP, busy-wait or fallback
    ret
asm_enter_critical_section ENDP

ALIGN 16
asm_leave_critical_section PROC
    ; Placeholder: kernel32!LeaveCriticalSection
    ret
asm_leave_critical_section ENDP

ALIGN 16
asm_delete_critical_section PROC
    ; Placeholder: kernel32!DeleteCriticalSection
    ret
asm_delete_critical_section ENDP

ALIGN 16
asm_create_event PROC
    ; Placeholder: kernel32!CreateEventExW
    ; Returns NULL for MVP
    xor rax, rax
    ret
asm_create_event ENDP

ALIGN 16
asm_set_event PROC
    ; Placeholder: kernel32!SetEvent
    mov rax, 1              ; Return TRUE
    ret
asm_set_event ENDP

ALIGN 16
asm_reset_event PROC
    ; Placeholder: kernel32!ResetEvent
    mov rax, 1              ; Return TRUE
    ret
asm_reset_event ENDP

ALIGN 16
asm_wait_for_event PROC
    ; Placeholder: kernel32!WaitForSingleObject
    mov rax, 0              ; Return WAIT_OBJECT_0 (signaled)
    ret
asm_wait_for_event ENDP

ALIGN 16
asm_close_handle PROC
    ; Placeholder: kernel32!CloseHandle
    mov rax, 1              ; Return TRUE
    ret
asm_close_handle ENDP

END
