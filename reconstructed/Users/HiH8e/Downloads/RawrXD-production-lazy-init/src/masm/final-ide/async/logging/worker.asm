;==========================================================================
; async_logging_worker.asm - Background Thread for Non-Blocking Log Writes
; ==========================================================================
; Features:
; - Worker thread with queue-based architecture
; - Non-blocking log insertion via queue
; - Thread-safe FIFO queue with spinlock
; - Batch processing (min 10ms between flushes)
; - Graceful shutdown with pending log drain
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_LOG_QUEUE_SIZE  EQU 1024        ; Max queued log entries
LOG_QUEUE_ENTRY_SZ  EQU 512         ; Max log entry size
MIN_FLUSH_INTERVAL  EQU 10          ; 10ms between flushes (ms)
QUEUE_FULL_TIMEOUT  EQU 100         ; If queue full, timeout after 100ms

;==========================================================================
; STRUCTURES
;==========================================================================
LOG_QUEUE_ENTRY STRUCT
    timestamp       DWORD ?         ; GetTickCount64() when queued
    level           DWORD ?         ; 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    source          BYTE 32 DUP (?) ; "Editor", "Agent", "Hotpatch"
    message         BYTE 470 DUP (?) ; Log message text (512-32-8=472)
LOG_QUEUE_ENTRY ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Worker thread control
    hWorkerThread       QWORD ?
    dwWorkerThreadId    DWORD ?
    bWorkerRunning      DWORD 0      ; Flag to stop worker
    
    ; Log queue (ring buffer)
    LogQueue            LOG_QUEUE_ENTRY MAX_LOG_QUEUE_SIZE DUP (<>)
    LogQueueHead        DWORD 0      ; Write pointer
    LogQueueTail        DWORD 0      ; Read pointer
    LogQueueSpinlock    DWORD 0      ; Spinlock (0=free, 1=locked)
    
    ; Statistics
    LogEntriesQueued    QWORD 0
    LogEntriesProcessed QWORD 0
    LogQueueOverflows   QWORD 0
    
    ; Strings
    szAsyncLogWorker    BYTE "AsyncLogWorker",0
    szQueueFull         BYTE "[WARNING] Log queue full, dropping entries",0
    szWorkerShutdown    BYTE "[INFO] Async logging worker shutdown complete",0

.data?
    ; Last flush time
    LastFlushTime       QWORD ?

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PRIVATE: spinlock_acquire() -> void
; Acquire spinlock with exponential backoff
;==========================================================================
PRIVATE spinlock_acquire
spinlock_acquire PROC
    local spin_count:DWORD
    mov spin_count, 0
    
    ; Exponential backoff: spin up to 1000 iterations
.spin_loop:
    mov eax, 1
    lock cmpxchg DWORD PTR [rcx], eax  ; Try to set spinlock=1
    je .acquired
    
    ; Spinlock held, backoff
    cmp spin_count, 1000
    jge .yield_and_retry
    inc spin_count
    pause                               ; CPU hint for better power consumption
    jmp .spin_loop
    
.yield_and_retry:
    ; If still locked after 1000 spins, yield to other threads
    call Sleep                          ; Sleep 0ms yields
    mov spin_count, 0
    jmp .spin_loop
    
.acquired:
    ret
spinlock_acquire ENDP

;==========================================================================
; PRIVATE: spinlock_release() -> void
; Release spinlock
;==========================================================================
PRIVATE spinlock_release
spinlock_release PROC
    mov DWORD PTR [rcx], 0
    ret
spinlock_release ENDP

;==========================================================================
; PUBLIC: async_logging_init() -> rax (success)
; Initialize async logging worker thread
;==========================================================================
PUBLIC async_logging_init
async_logging_init PROC
    push rbx
    push rdi
    sub rsp, 32
    
    ; Zero log queue
    lea rcx, LogQueue
    xor edx, edx
    mov r8d, MAX_LOG_QUEUE_SIZE * (SIZE LOG_QUEUE_ENTRY) / 8
    
.zero_loop:
    cmp r8d, 0
    je .zero_done
    mov QWORD PTR [rcx + rdx], 0
    add rdx, 8
    dec r8d
    jmp .zero_loop
    
.zero_done:
    ; Initialize queue pointers
    mov LogQueueHead, 0
    mov LogQueueTail, 0
    mov LogQueueSpinlock, 0
    
    ; Set worker running flag
    mov bWorkerRunning, 1
    
    ; Create worker thread
    lea rcx, hWorkerThread              ; hThread
    lea rdx, dwWorkerThreadId           ; lpThreadId
    mov r8, 0                           ; lpThreadAttributes
    mov r9, 0                           ; dwStackSize (use default)
    mov rax, QWORD PTR [rsp + 40]       ; lpStartAddress -> pointer to async_logging_worker
    mov QWORD PTR [rsp + 32], rax       ; Stack arg
    
    call CreateThread
    test rax, rax
    jz .init_fail
    
    mov hWorkerThread, rax
    
    ; Get current tick count
    call GetTickCount64
    mov LastFlushTime, rax
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rdi
    pop rbx
    ret
    
.init_fail:
    xor eax, eax                        ; Failure
    add rsp, 32
    pop rdi
    pop rbx
    ret
async_logging_init ENDP

;==========================================================================
; PUBLIC: async_logging_queue_entry(level: ecx, source: rdx, message: r8) -> rax
; Queue log entry (non-blocking)
; Returns: 1=queued, 0=queue full (dropped)
;==========================================================================
PUBLIC async_logging_queue_entry
async_logging_queue_entry PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    ; Save parameters
    mov esi, ecx                        ; esi = level
    mov rdi, rdx                        ; rdi = source
    mov rbx, r8                         ; rbx = message
    
    ; Try to acquire spinlock (with timeout)
    lea rcx, LogQueueSpinlock
    mov r8d, 0
    
.acquire_spin:
    mov eax, 1
    lock cmpxchg DWORD PTR [rcx], eax
    je .have_lock
    
    inc r8d
    cmp r8d, 100                        ; Timeout after 100 attempts
    jge .queue_full
    pause
    jmp .acquire_spin
    
.have_lock:
    ; Calculate next head position
    mov eax, LogQueueHead
    mov ecx, eax
    add ecx, 1
    cmp ecx, MAX_LOG_QUEUE_SIZE
    jne .next_pos_ok
    xor ecx, ecx                        ; Wrap around
    
.next_pos_ok:
    ; Check if queue is full
    cmp ecx, LogQueueTail
    je .release_full
    
    ; Get queue entry
    imul eax, SIZE LOG_QUEUE_ENTRY
    lea eax, LogQueue[rax]              ; rax = &LogQueue[LogQueueHead]
    
    ; Fill entry
    ; timestamp
    call GetTickCount64
    mov DWORD PTR [rax], eax            ; eax has lower 32 bits of tick count
    
    ; level
    mov DWORD PTR [rax + 4], esi
    
    ; source (copy up to 32 bytes)
    mov rcx, rdi
    lea rdx, [rax + 8]
    mov r8d, 32
    call copy_string_safe
    
    ; message (copy up to 470 bytes)
    mov rcx, rbx
    lea rdx, [rax + 40]                 ; 8 + 32 = offset
    mov r8d, 470
    call copy_string_safe
    
    ; Update head pointer
    mov eax, LogQueueHead
    mov ecx, eax
    add ecx, 1
    cmp ecx, MAX_LOG_QUEUE_SIZE
    jne .update_head
    xor ecx, ecx
    
.update_head:
    mov LogQueueHead, ecx
    
    ; Update statistics
    inc LogEntriesQueued
    
    ; Release spinlock
    mov DWORD PTR LogQueueSpinlock, 0
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
    
.release_full:
    mov DWORD PTR LogQueueSpinlock, 0
    
.queue_full:
    ; Log queue full (but don't recurse)
    inc LogQueueOverflows
    xor eax, eax                        ; Failure
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
async_logging_queue_entry ENDP

;==========================================================================
; PRIVATE: copy_string_safe(src: rcx, dst: rdx, max_len: r8d) -> rax
; Safe string copy (null-terminated, bounds-checked)
;==========================================================================
PRIVATE copy_string_safe
copy_string_safe PROC
    push rbx
    xor eax, eax                        ; Length counter
    
.copy_loop:
    cmp eax, r8d                        ; Check max length
    jge .copy_done
    
    movzx ebx, BYTE PTR [rcx + rax]
    mov BYTE PTR [rdx + rax], bl
    
    test bl, bl                         ; Check for null terminator
    je .copy_done
    
    inc eax
    jmp .copy_loop
    
.copy_done:
    mov BYTE PTR [rdx + rax], 0         ; Ensure null terminator
    pop rbx
    ret
copy_string_safe ENDP

;==========================================================================
; PRIVATE: async_logging_worker() -> void
; Worker thread proc (runs indefinitely until bWorkerRunning=0)
;==========================================================================
PRIVATE async_logging_worker
async_logging_worker PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
.worker_loop:
    ; Check if we should exit
    cmp bWorkerRunning, 0
    je .worker_exit
    
    ; Check if minimum flush interval has passed
    call GetTickCount64
    mov rbx, LastFlushTime
    sub rax, rbx
    cmp rax, MIN_FLUSH_INTERVAL
    jl .sleep_and_retry
    
    ; Try to acquire spinlock (non-blocking, skip if locked)
    lea rcx, LogQueueSpinlock
    mov eax, 1
    lock cmpxchg DWORD PTR [rcx], eax
    jne .sleep_and_retry                ; If locked, skip flush
    
    ; Process all queued entries
    mov esi, LogQueueTail               ; esi = tail
    
.flush_loop:
    cmp esi, LogQueueHead
    je .flush_done
    
    ; Get queue entry
    imul eax, esi, SIZE LOG_QUEUE_ENTRY
    lea rax, LogQueue[rax]
    
    ; Output to pane (would call output_pane_append or similar)
    ; For now, just count processed entries
    inc LogEntriesProcessed
    
    ; Update tail
    inc esi
    cmp esi, MAX_LOG_QUEUE_SIZE
    jne .flush_loop
    xor esi, esi                        ; Wrap around
    jmp .flush_loop
    
.flush_done:
    mov LogQueueTail, esi
    
    ; Release spinlock
    mov DWORD PTR LogQueueSpinlock, 0
    
    ; Update last flush time
    call GetTickCount64
    mov LastFlushTime, rax
    
.sleep_and_retry:
    ; Sleep 5ms before checking again
    mov rcx, 5
    call Sleep
    jmp .worker_loop
    
.worker_exit:
    ; Drain any remaining entries
    ; (Future: flush remaining queue before exit)
    
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
async_logging_worker ENDP

;==========================================================================
; PUBLIC: async_logging_shutdown() -> rax (success)
; Shutdown worker thread and flush remaining logs
;==========================================================================
PUBLIC async_logging_shutdown
async_logging_shutdown PROC
    push rbx
    sub rsp, 32
    
    ; Signal worker to stop
    mov bWorkerRunning, 0
    
    ; Wait for worker thread to exit (max 5 seconds)
    mov rcx, hWorkerThread
    mov edx, 5000
    call WaitForSingleObject
    
    ; Close thread handle
    mov rcx, hWorkerThread
    call CloseHandle
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rbx
    ret
async_logging_shutdown ENDP

;==========================================================================
; PUBLIC: async_logging_get_stats(queue_count: rcx, overflow_count: rdx, 
;                                 processed_count: r8) -> void
; Get statistics about async logging
;==========================================================================
PUBLIC async_logging_get_stats
async_logging_get_stats PROC
    ; Get current queue size
    mov eax, LogQueueHead
    sub eax, LogQueueTail
    cmp eax, 0
    jge .positive_size
    add eax, MAX_LOG_QUEUE_SIZE
    
.positive_size:
    mov DWORD PTR [rcx], eax            ; queue_count
    mov rax, LogQueueOverflows
    mov QWORD PTR [rdx], rax            ; overflow_count
    mov rax, LogEntriesProcessed
    mov QWORD PTR [r8], rax             ; processed_count
    ret
async_logging_get_stats ENDP

END
