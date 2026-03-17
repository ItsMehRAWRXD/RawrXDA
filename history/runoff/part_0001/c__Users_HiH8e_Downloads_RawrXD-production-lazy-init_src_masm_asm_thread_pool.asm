;=====================================================================
; asm_thread_pool.asm - x64 MASM Thread Pool Implementation
; PURE ASSEMBLY - ZERO C++ DEPENDENCIES
;=====================================================================

include asm_sync.inc

; Win32 API Externals
EXTERN CreateThread:PROC
EXTERN CloseHandle:PROC
EXTERN GetCurrentThreadId:PROC
EXTERN ExitThread:PROC

; External dependencies from asm_memory.asm
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC

.data
    ; Task structure
    ; [+0]: TaskProc (qword)
    ; [+8]: Context (qword)
    ; [+16]: NextTask (qword)

    ; Thread Pool structure
    ; [+0]:  Mutex (qword)
    ; [+8]:  Semaphore (qword)
    ; [+16]: TaskQueueHead (qword)
    ; [+24]: TaskQueueTail (qword)
    ; [+32]: ThreadHandles (qword ptr)
    ; [+40]: NumThreads (dword)
    ; [+44]: ShutdownFlag (dword)

.code

PUBLIC asm_thread_pool_create, asm_thread_pool_enqueue, asm_thread_pool_destroy

;=====================================================================
; Worker Thread Procedure
;=====================================================================
ALIGN 16
worker_thread_proc PROC
    push rbx
    push rsi
    push rdi
    mov rbx, rcx            ; rbx = Pool pointer
    
worker_loop:
    ; Wait for a task (semaphore)
    mov rcx, [rbx + 8]      ; Semaphore
    mov rdx, -1             ; Infinite wait
    call asm_semaphore_wait
    
    ; Check shutdown flag
    mov eax, [rbx + 44]
    test eax, eax
    jnz worker_exit
    
    ; Lock queue
    mov rcx, [rbx]          ; Mutex
    call asm_mutex_lock
    
    ; Pop task from queue
    mov rsi, [rbx + 16]     ; Head
    test rsi, rsi
    jz queue_empty
    
    mov rax, [rsi + 16]     ; Next
    mov [rbx + 16], rax     ; New head
    test rax, rax
    jnz @f
    mov qword ptr [rbx + 24], 0 ; Tail = NULL if queue empty
@@:
    
    ; Unlock queue
    mov rcx, [rbx]
    call asm_mutex_unlock
    
    ; Execute task
    mov rax, [rsi]          ; TaskProc
    mov rcx, [rsi + 8]      ; Context
    call rax
    
    ; Free task structure
    mov rcx, rsi
    call asm_free
    
    jmp worker_loop

queue_empty:
    mov rcx, [rbx]
    call asm_mutex_unlock
    jmp worker_loop

worker_exit:
    xor ecx, ecx
    call ExitThread
    pop rdi
    pop rsi
    pop rbx
    ret
worker_thread_proc ENDP

;=====================================================================
; asm_thread_pool_create(num_threads: rcx) -> rax
;=====================================================================
ALIGN 16
asm_thread_pool_create PROC
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rcx            ; num_threads
    
    ; Allocate pool structure (48 bytes)
    mov rcx, 48
    mov rdx, 8
    call asm_malloc
    test rax, rax
    jz create_fail
    
    mov rbx, rax
    
    ; Create Mutex
    call asm_mutex_create
    mov [rbx], rax
    
    ; Create Semaphore (initial=0, max=1000)
    mov rcx, 0
    mov rdx, 1000
    call asm_semaphore_create
    mov [rbx + 8], rax
    
    ; Init queue
    mov qword ptr [rbx + 16], 0 ; Head
    mov qword ptr [rbx + 24], 0 ; Tail
    mov dword ptr [rbx + 44], 0 ; Shutdown = 0
    mov [rbx + 40], r12d        ; NumThreads
    
    ; Allocate thread handles array
    mov rax, r12
    shl rax, 3              ; num * 8
    mov rcx, rax
    mov rdx, 8
    call asm_malloc
    mov [rbx + 32], rax
    
    ; Create threads
    xor r13, r13            ; index
create_threads_loop:
    cmp r13, r12
    jae create_done
    
    ; CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId)
    xor rcx, rcx
    xor rdx, rdx
    lea r8, worker_thread_proc
    mov r9, rbx             ; Parameter = Pool
    mov qword ptr [rsp + 32], 0
    mov qword ptr [rsp + 40], 0
    
    call CreateThread
    
    mov rdx, [rbx + 32]     ; Handles array
    mov [rdx + r13 * 8], rax
    
    inc r13
    jmp create_threads_loop

create_done:
    mov rax, rbx
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

create_fail:
    xor rax, rax
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
asm_thread_pool_create ENDP

;=====================================================================
; asm_thread_pool_enqueue(pool: rcx, proc: rdx, context: r8) -> rax
;=====================================================================
ALIGN 16
asm_thread_pool_enqueue PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 32
    
    mov rbx, rcx            ; pool
    mov r12, rdx            ; proc
    mov r13, r8             ; context
    
    ; Allocate task structure (24 bytes)
    mov rcx, 24
    mov rdx, 8
    call asm_malloc
    test rax, rax
    jz enqueue_fail
    
    mov r14, rax
    mov [r14], r12          ; Proc
    mov [r14 + 8], r13      ; Context
    mov qword ptr [r14 + 16], 0 ; Next
    
    ; Lock queue
    mov rcx, [rbx]
    call asm_mutex_lock
    
    ; Add to tail
    mov rax, [rbx + 24]     ; Tail
    test rax, rax
    jz queue_empty_add
    
    mov [rax + 16], r14     ; Tail->Next = New
    mov [rbx + 24], r14     ; Tail = New
    jmp enqueue_done_lock
    
queue_empty_add:
    mov [rbx + 16], r14     ; Head = New
    mov [rbx + 24], r14     ; Tail = New
    
enqueue_done_lock:
    ; Unlock queue
    mov rcx, [rbx]
    call asm_mutex_unlock
    
    ; Signal semaphore
    mov rcx, [rbx + 8]
    mov rdx, 1
    call asm_semaphore_release
    
    mov rax, 1              ; Success
    jmp enqueue_exit

enqueue_fail:
    xor rax, rax
enqueue_exit:
    add rsp, 32
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
asm_thread_pool_enqueue ENDP

;=====================================================================
; asm_thread_pool_destroy(pool: rcx)
;=====================================================================
ALIGN 16
asm_thread_pool_destroy PROC
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov rbx, rcx
    test rbx, rbx
    jz destroy_exit
    
    ; Set shutdown flag
    mov dword ptr [rbx + 44], 1
    
    ; Signal all threads to wake up and exit
    mov r12d, [rbx + 40]    ; NumThreads
    xor r13, r13
signal_loop:
    cmp r13, r12
    jae wait_threads
    
    mov rcx, [rbx + 8]      ; Semaphore
    mov rdx, 1
    call asm_semaphore_release
    
    inc r13
    jmp signal_loop

wait_threads:
    ; In real code, WaitForMultipleObjects here
    ; For MVP, just close handles
    xor r13, r13
close_loop:
    cmp r13, r12
    jae cleanup
    
    mov rax, [rbx + 32]     ; Handles array
    mov rcx, [rax + r13 * 8]
    call CloseHandle
    
    inc r13
    jmp close_loop

cleanup:
    ; Destroy sync objects
    mov rcx, [rbx]
    call asm_mutex_destroy
    
    mov rcx, [rbx + 8]
    call asm_semaphore_destroy
    
    ; Free handles array
    mov rcx, [rbx + 32]
    call asm_free
    
    ; Free pool structure
    mov rcx, rbx
    call asm_free

destroy_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
asm_thread_pool_destroy ENDP

END
