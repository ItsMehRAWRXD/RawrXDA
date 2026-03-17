;=====================================================================
; asm_sync.asm - x64 MASM Thread Synchronization Primitives
; MUTEXES, EVENTS, SEMAPHORES, ATOMIC OPERATIONS (WIN32 INTEGRATED)
;=====================================================================

; External dependencies from asm_memory.asm
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC

; Win32 API Externals
EXTERN InitializeCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN CreateEventExW:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN CreateSemaphoreExW:PROC
EXTERN ReleaseSemaphore:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC

.code

; Export all public synchronization functions
PUBLIC asm_mutex_create, asm_mutex_lock, asm_mutex_unlock, asm_mutex_destroy
PUBLIC asm_event_create, asm_event_set, asm_event_reset, asm_event_wait, asm_event_destroy
PUBLIC asm_semaphore_create, asm_semaphore_wait, asm_semaphore_release, asm_semaphore_destroy
PUBLIC asm_atomic_increment, asm_atomic_decrement, asm_atomic_add, asm_atomic_cmpxchg, asm_atomic_xchg

;=====================================================================
; asm_mutex_create() -> rax
;=====================================================================
ALIGN 16
asm_mutex_create PROC
    push rbx
    sub rsp, 32
    
    ; Allocate CRITICAL_SECTION (40 bytes)
    mov rcx, 40
    mov rdx, 8
    call asm_malloc
    
    test rax, rax
    jz mutex_create_fail
    
    mov rbx, rax            ; Save pointer
    mov rcx, rax
    call InitializeCriticalSection
    
    mov rax, rbx            ; Return pointer
    jmp mutex_create_done
    
mutex_create_fail:
    xor rax, rax
    
mutex_create_done:
    add rsp, 32
    pop rbx
    ret
asm_mutex_create ENDP

;=====================================================================
; asm_mutex_lock(handle: rcx)
;=====================================================================
ALIGN 16
asm_mutex_lock PROC
    test rcx, rcx
    jz mutex_lock_done
    sub rsp, 32
    call EnterCriticalSection
    add rsp, 32
mutex_lock_done:
    ret
asm_mutex_lock ENDP

;=====================================================================
; asm_mutex_unlock(handle: rcx)
;=====================================================================
ALIGN 16
asm_mutex_unlock PROC
    test rcx, rcx
    jz mutex_unlock_done
    sub rsp, 32
    call LeaveCriticalSection
    add rsp, 32
mutex_unlock_done:
    ret
asm_mutex_unlock ENDP

;=====================================================================
; asm_mutex_destroy(handle: rcx)
;=====================================================================
ALIGN 16
asm_mutex_destroy PROC
    push rbx
    sub rsp, 32
    test rcx, rcx
    jz mutex_destroy_done
    
    mov rbx, rcx
    call DeleteCriticalSection
    
    mov rcx, rbx
    call asm_free
    
mutex_destroy_done:
    add rsp, 32
    pop rbx
    ret
asm_mutex_destroy ENDP

;=====================================================================
; asm_event_create(manual_reset: rcx) -> rax
;=====================================================================
ALIGN 16
asm_event_create PROC
    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; manual_reset
    
    ; Allocate event structure (16 bytes: handle, manual_reset)
    mov rcx, 16
    mov rdx, 8
    call asm_malloc
    test rax, rax
    jz event_create_fail
    
    mov rbx, rax
    mov [rbx + 8], r12      ; Store manual_reset
    
    ; CreateEventExW(lpEventAttributes, lpName, dwFlags, dwDesiredAccess)
    xor rcx, rcx            ; NULL
    xor rdx, rdx            ; NULL
    
    mov r8d, 0
    test r12, r12
    jz @f
    mov r8d, 1              ; CREATE_EVENT_MANUAL_RESET = 0x1
@@:
    mov r9d, 1F0003h        ; EVENT_ALL_ACCESS
    call CreateEventExW
    
    test rax, rax
    jz event_create_fail_free
    
    mov [rbx], rax          ; Store handle
    mov rax, rbx            ; Return structure pointer
    jmp event_create_done

event_create_fail_free:
    mov rcx, rbx
    call asm_free
event_create_fail:
    xor rax, rax
event_create_done:
    add rsp, 32
    pop r12
    pop rbx
    ret
asm_event_create ENDP

;=====================================================================
; asm_event_set(handle: rcx)
;=====================================================================
ALIGN 16
asm_event_set PROC
    test rcx, rcx
    jz @f
    mov rcx, [rcx]          ; Get real handle
    sub rsp, 32
    call SetEvent
    add rsp, 32
@@:
    ret
asm_event_set ENDP

;=====================================================================
; asm_event_reset(handle: rcx)
;=====================================================================
ALIGN 16
asm_event_reset PROC
    test rcx, rcx
    jz @f
    mov rcx, [rcx]
    sub rsp, 32
    call ResetEvent
    add rsp, 32
@@:
    ret
asm_event_reset ENDP

;=====================================================================
; asm_event_wait(handle: rcx, timeout_ms: rdx) -> rax
;=====================================================================
ALIGN 16
asm_event_wait PROC
    test rcx, rcx
    jz event_wait_fail
    mov rcx, [rcx]
    sub rsp, 32
    call WaitForSingleObject
    add rsp, 32
    ret
event_wait_fail:
    mov rax, -1
    ret
asm_event_wait ENDP

;=====================================================================
; asm_event_destroy(handle: rcx)
;=====================================================================
ALIGN 16
asm_event_destroy PROC
    push rbx
    sub rsp, 32
    test rcx, rcx
    jz @f
    mov rbx, rcx
    mov rcx, [rbx]
    call CloseHandle
    mov rcx, rbx
    call asm_free
@@:
    add rsp, 32
    pop rbx
    ret
asm_event_destroy ENDP

;=====================================================================
; asm_semaphore_create(initial_count: rcx, max_count: rdx) -> rax
;=====================================================================
ALIGN 16
asm_semaphore_create PROC
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rcx            ; initial
    mov r13, rdx            ; max
    
    mov rcx, 8
    mov rdx, 8
    call asm_malloc
    test rax, rax
    jz sem_fail
    
    mov rbx, rax
    
    ; CreateSemaphoreExW(lpAttributes, lInitialCount, lMaximumCount, lpName, dwFlags, dwDesiredAccess)
    xor rcx, rcx
    mov rdx, r12
    mov r8, r13
    xor r9, r9
    
    ; Stack params for CreateSemaphoreExW
    mov qword ptr [rsp + 32], 0 ; dwFlags
    mov qword ptr [rsp + 40], 1F0003h ; SEMAPHORE_ALL_ACCESS
    
    call CreateSemaphoreExW
    test rax, rax
    jz sem_fail_free
    
    mov [rbx], rax
    mov rax, rbx
    jmp sem_done

sem_fail_free:
    mov rcx, rbx
    call asm_free
sem_fail:
    xor rax, rax
sem_done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
asm_semaphore_create ENDP

;=====================================================================
; asm_semaphore_wait(handle: rcx, timeout_ms: rdx) -> rax
;=====================================================================
ALIGN 16
asm_semaphore_wait PROC
    test rcx, rcx
    jz sem_wait_fail
    mov rcx, [rcx]
    sub rsp, 32
    call WaitForSingleObject
    add rsp, 32
    ret
sem_wait_fail:
    mov rax, -1
    ret
asm_semaphore_wait ENDP

;=====================================================================
; asm_semaphore_release(handle: rcx, count: rdx) -> rax
;=====================================================================
ALIGN 16
asm_semaphore_release PROC
    test rcx, rcx
    jz sem_rel_fail
    mov rcx, [rcx]
    mov r8, 0               ; lpPreviousCount
    sub rsp, 32
    call ReleaseSemaphore
    add rsp, 32
    ret
sem_rel_fail:
    xor rax, rax
    ret
asm_semaphore_release ENDP

;=====================================================================
; asm_semaphore_destroy(handle: rcx)
;=====================================================================
ALIGN 16
asm_semaphore_destroy PROC
    push rbx
    sub rsp, 32
    test rcx, rcx
    jz @f
    mov rbx, rcx
    mov rcx, [rbx]
    call CloseHandle
    mov rcx, rbx
    call asm_free
@@:
    add rsp, 32
    pop rbx
    ret
asm_semaphore_destroy ENDP

;=====================================================================
; ATOMIC OPERATIONS
;=====================================================================
ALIGN 16
asm_atomic_increment PROC
    lock add qword ptr [rcx], 1
    mov rax, [rcx]
    ret
asm_atomic_increment ENDP

ALIGN 16
asm_atomic_decrement PROC
    lock sub qword ptr [rcx], 1
    mov rax, [rcx]
    ret
asm_atomic_decrement ENDP

ALIGN 16
asm_atomic_add PROC
    lock add qword ptr [rcx], rdx
    mov rax, [rcx]
    ret
asm_atomic_add ENDP

ALIGN 16
asm_atomic_cmpxchg PROC
    mov rax, rdx
    lock cmpxchg qword ptr [rcx], r8
    jz @f
    xor rax, rax
    ret
@@:
    mov rax, 1
    ret
asm_atomic_cmpxchg ENDP

ALIGN 16
asm_atomic_xchg PROC
    mov rax, rdx
    lock xchg qword ptr [rcx], rax
    ret
asm_atomic_xchg ENDP

END

