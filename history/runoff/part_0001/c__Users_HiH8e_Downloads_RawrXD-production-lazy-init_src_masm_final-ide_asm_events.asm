;=====================================================================
; asm_events.asm - x64 MASM Event Loop & Signal Routing System
; ASYNC SIGNAL DISPATCH WITH RING BUFFER QUEUE
;=====================================================================
; Implements:
;  - Thread-safe event queue (ring buffer)
;  - Signal handler registry (hash map of signal_id -> handler fn)
;  - Async event dispatch with handler invocation
;  - Event ordering and batching
;
; Event Queue Entry (64 bytes for alignment):
;   [+0]:   Signal ID (qword)
;   [+8]:   Handler Function Ptr (qword)
;   [+16]:  Param1 (qword)
;   [+24]:  Param2 (qword)
;   [+32]:  Param3 (qword)
;   [+40]:  Timestamp (qword)
;   [+48]:  Status (qword)
;   [+56]:  Reserved
;
; Event Loop Structure (256 bytes):
;   [+0]:   Queue base pointer (qword)
;   [+8]:   Queue size (qword)
;   [+16]:  Head pointer (qword)
;   [+24]:  Tail pointer (qword)
;   [+32]:  Queue mutex handle (qword)
;   [+40]:  Handler registry map ptr (qword)
;   [+48]:  Event count (qword, atomic)
;   [+56]:  Processed count (qword)
;   [+64]:  Discarded count (qword)
;   [+72]:  Last error (qword)
;   [+80]:  Reserved...
;=====================================================================

; External dependencies
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_mutex_create:PROC
EXTERN asm_mutex_destroy:PROC
EXTERN asm_mutex_lock:PROC
EXTERN asm_mutex_unlock:PROC

; External dependencies from kernel32.lib
EXTERN QueryPerformanceCounter:PROC

.data

; Global event loop statistics
g_total_events      QWORD 0
g_processed_events  QWORD 0
g_dropped_events    QWORD 0

.code

;=====================================================================
; asm_event_loop_create(queue_size: rcx) -> rax
;
; Creates new event loop with specified queue size.
; queue_size = number of events the queue can hold.
;
; Returns opaque event loop handle, or NULL if failed.
;=====================================================================

ALIGN 16
asm_event_loop_create PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = queue_size
    
    ; Validate queue size
    test r12, r12
    jz event_loop_create_fail
    
    cmp r12, 100000h        ; Max 1M events (no 0x in MASM)
    jg event_loop_create_fail
    
    ; Allocate event loop structure (256 bytes)
    mov rcx, 256
    mov rdx, 32
    call asm_malloc
    
    test rax, rax
    jz event_loop_create_fail
    
    mov rbx, rax            ; rbx = event loop pointer
    
    ; Allocate queue (queue_size * 64 bytes per entry)
    mov rcx, r12
    imul rcx, 64
    mov rdx, 64
    call asm_malloc
    
    test rax, rax
    jz event_loop_create_cleanup
    
    ; Initialize event loop structure
    mov [rbx], rax          ; Queue base pointer
    mov [rbx + 8], r12      ; Queue size
    mov qword ptr [rbx + 16], 0  ; Head = 0
    mov qword ptr [rbx + 24], 0  ; Tail = 0
    
    ; Create mutex for queue
    call asm_mutex_create
    mov [rbx + 32], rax     ; Store mutex handle
    
    ; Create handler registry (placeholder: use simple array)
    mov rcx, 256            ; 256 signal slots max
    imul rcx, 8             ; 8 bytes per entry (handler ptr)
    mov rdx, 16
    call asm_malloc
    
    mov [rbx + 40], rax     ; Store handler registry ptr
    
    ; Initialize stats
    mov qword ptr [rbx + 48], 0   ; Event count
    mov qword ptr [rbx + 56], 0   ; Processed count
    mov qword ptr [rbx + 64], 0   ; Discarded count
    mov qword ptr [rbx + 72], 0   ; Last error
    
    mov rax, rbx            ; Return loop handle
    jmp event_loop_create_done
    
event_loop_create_cleanup:
    mov rcx, rbx
    call asm_free
    xor rax, rax
    
event_loop_create_fail:
    xor rax, rax
    
event_loop_create_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_event_loop_create ENDP

;=====================================================================
; asm_event_loop_register_signal(loop: rcx, signal_id: rdx, handler: r8) -> void
;
; Registers a signal handler function for a given signal ID.
; signal_id = unique identifier (0-255, for now)
; handler = function pointer to invoke when signal fires
;=====================================================================

ALIGN 16
asm_event_loop_register_signal PROC

    ; Validate inputs
    test rcx, rcx
    jz register_signal_done
    
    test rdx, rdx
    jz register_signal_done
    
    ; signal_id must fit in registry
    cmp rdx, 256
    jge register_signal_done
    
    ; Get handler registry
    mov rax, [rcx + 40]
    test rax, rax
    jz register_signal_done
    
    ; Store handler at registry[signal_id]
    mov [rax + rdx*8], r8
    
register_signal_done:
    ret

asm_event_loop_register_signal ENDP

;=====================================================================
; asm_event_loop_emit(loop: rcx, signal_id: rdx, p1: r8, p2: r9, p3: [rsp+40]) -> void
;
; Queues an event to be processed by the event loop.
; Does not immediately invoke handler (async).
;
; Returns immediately after queueing.
;=====================================================================

ALIGN 16
asm_event_loop_emit PROC

    push rbx
    push r12
    push r13
    sub rsp, 48             ; 40 bytes shadow + 8 for p3
    
    mov r12, rcx            ; r12 = loop handle
    mov r13b, dl            ; r13b = signal_id (low byte for simpler indexing)
    
    ; Get p3 from stack
    mov rax, [rsp + 48 + 8 + 40]   ; p3 parameter
    mov r13, [rsp + 48 + 40]       ; Retrieve p3 properly
    
    test r12, r12
    jz emit_done
    
    ; Acquire queue lock
    mov rcx, [r12 + 32]     ; Get mutex handle
    call asm_mutex_lock
    
    ; Get queue info
    mov rax, [r12]          ; rax = queue base
    mov rbx, [r12 + 8]      ; rbx = queue size
    mov rcx, [r12 + 24]     ; rcx = tail (write position)
    
    ; Check if queue is full
    mov r8, rcx
    inc r8
    cmp r8, rbx
    je emit_queue_full
    
    ; Queue has space
    ; Calculate queue entry address
    mov r8, rcx
    imul r8, 64             ; r8 = tail * 64
    add r8, rax             ; r8 = queue_base + tail*64
    
    ; Write event entry
    mov [r8 + 0], rdx       ; Signal ID
    mov qword ptr [r8 + 8], 0  ; Handler = NULL (will look up at dispatch)
    mov [r8 + 16], r8       ; Param1
    mov [r8 + 24], r9       ; Param2
    
    ; Get p3 from caller's stack
    mov rax, [rsp + 48 + 40]
    mov [r8 + 32], rax      ; Param3
    
    ; Timestamp using QueryPerformanceCounter
    lea rcx, [r8 + 40]      ; Address to store timestamp
    call QueryPerformanceCounter
    
    lock add [g_total_events], 1
    
    mov qword ptr [r8 + 48], 0  ; Status = pending
    
    ; Advance tail
    mov rax, [r12 + 24]
    inc rax
    cmp rax, rbx
    jl emit_tail_ok
    xor rax, rax            ; Wrap around
    
emit_tail_ok:
    mov [r12 + 24], rax
    
    ; Unlock
    mov rcx, [r12 + 32]
    call asm_mutex_unlock
    
    jmp emit_done
    
emit_queue_full:
    ; Queue overflow
    lock add [g_dropped_events], 1
    mov qword ptr [r12 + 72], 1  ; Set error flag
    
    ; Unlock
    mov rcx, [r12 + 32]
    call asm_mutex_unlock
    
emit_done:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret

asm_event_loop_emit ENDP

;=====================================================================
; asm_event_loop_process_one(loop: rcx) -> rax
;
; Processes one event from the queue.
; Looks up handler, invokes it with parameters.
;
; Returns:
;   1 if event was processed
;   0 if queue was empty
;   -1 if error occurred
;=====================================================================

ALIGN 16
asm_event_loop_process_one PROC

    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rcx            ; r12 = loop handle
    
    test r12, r12
    jz process_one_fail
    
    ; Acquire lock
    mov rcx, [r12 + 32]
    call asm_mutex_lock
    
    ; Check if queue has events
    mov rax, [r12 + 16]     ; head
    mov rdx, [r12 + 24]     ; tail
    
    cmp rax, rdx
    je process_one_empty    ; Queue empty
    
    ; Get queue entry
    mov rcx, [r12]          ; queue base
    mov r8, rax
    imul r8, 64             ; r8 = head * 64
    add r8, rcx             ; r8 = queue_base + head*64
    
    ; Load event entry
    mov r9d, [r8 + 0]       ; r9d = signal_id
    mov r10, [r8 + 16]      ; r10 = param1
    mov r11, [r8 + 24]      ; r11 = param2
    mov r13, [r8 + 32]      ; r13 = param3
    
    ; Look up handler
    mov r8, [r12 + 40]      ; r8 = handler registry
    test r8, r8
    jz process_one_no_handler
    
    mov rax, [r8 + r9*8]    ; rax = handler function ptr
    test rax, rax
    jz process_one_no_handler
    
    ; Prepare to call handler(p1, p2, p3)
    ; Microsoft x64 calling convention: rcx, rdx, r8, r9, [rsp+40]
    
    mov rcx, r10            ; param1 -> rcx
    mov rdx, r11            ; param2 -> rdx
    mov r8, r13             ; param3 -> r8
    
    ; Invoke handler
    sub rsp, 32             ; Allocate shadow space
    call rax
    add rsp, 32
    
    ; Advance head
    mov rax, [r12 + 16]
    inc rax
    mov rbx, [r12 + 8]
    cmp rax, rbx
    jl process_one_advance_ok
    xor rax, rax
    
process_one_advance_ok:
    mov [r12 + 16], rax
    
    ; Update stats
    lock add [g_processed_events], 1
    lock add qword ptr [r12 + 56], 1  ; processed_count++
    
    ; Unlock
    mov rcx, [r12 + 32]
    call asm_mutex_unlock
    
    mov rax, 1              ; Return success
    jmp process_one_exit
    
process_one_no_handler:
    ; No handler registered, skip event
    mov rax, [r12 + 16]
    inc rax
    mov rbx, [r12 + 8]
    cmp rax, rbx
    jl process_one_advance_ok2
    xor rax, rax
    
process_one_advance_ok2:
    mov [r12 + 16], rax
    
    mov rcx, [r12 + 32]
    call asm_mutex_unlock
    
    mov rax, 0              ; No handler but queue not empty
    jmp process_one_exit
    
process_one_empty:
    mov rcx, [r12 + 32]
    call asm_mutex_unlock
    
    xor rax, rax
    jmp process_one_exit
    
process_one_fail:
    mov rax, -1
    
process_one_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

asm_event_loop_process_one ENDP

;=====================================================================
; asm_event_loop_process_all(loop: rcx) -> rax
;
; Processes all pending events in the queue.
; Returns count of events processed.
;=====================================================================

ALIGN 16
asm_event_loop_process_all PROC

    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = loop handle
    xor rax, rax            ; rax = processed count
    
    test rbx, rbx
    jz process_all_done
    
process_all_loop:
    mov rcx, rbx
    call asm_event_loop_process_one
    
    test rax, rax
    jz process_all_done      ; Queue empty
    
    cmp rax, -1
    je process_all_done      ; Error
    
    ; rax is 1 from process_one, accumulate in counter
    ; (rax=1 means event was processed)
    mov rcx, [rsp + 32]      ; Get loop from saved rbx position
    mov rcx, rbx             ; rbx still has loop handle
    add rax, [rsp + 32 + 40] ; Add to counter on stack
    mov [rsp + 32 + 40], rax ; Save counter
    
    jmp process_all_loop
    
process_all_done:
    mov rax, rbx
    add rsp, 32
    pop rbx
    ret

asm_event_loop_process_all ENDP

;=====================================================================
; asm_event_loop_destroy(loop: rcx) -> void
;
; Destroys event loop and frees all associated memory.
; MUST NOT be called while events are being processed.
;=====================================================================

ALIGN 16
asm_event_loop_destroy PROC

    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz loop_destroy_done
    
    mov rbx, rcx            ; rbx = loop handle
    
    ; Free queue
    mov rcx, [rbx]
    call asm_free
    
    ; Free handler registry
    mov rcx, [rbx + 40]
    call asm_free
    
    ; Destroy mutex
    mov rcx, [rbx + 32]
    call asm_mutex_destroy
    
    ; Free loop structure itself
    mov rcx, rbx
    call asm_free
    
loop_destroy_done:
    add rsp, 32
    pop rbx
    ret

asm_event_loop_destroy ENDP

;=====================================================================
; Event Statistics & Debugging
;=====================================================================

ALIGN 16
asm_event_stats PROC
    ; Return total events processed
    mov rax, [g_processed_events]
    ret
asm_event_stats ENDP

ALIGN 16
asm_event_loop_stats PROC
    ; rcx = loop handle
    test rcx, rcx
    jz loop_stats_zero
    
    mov rax, [rcx + 56]     ; Return processed_count
    ret
    
loop_stats_zero:
    xor rax, rax
    ret
asm_event_loop_stats ENDP

END
