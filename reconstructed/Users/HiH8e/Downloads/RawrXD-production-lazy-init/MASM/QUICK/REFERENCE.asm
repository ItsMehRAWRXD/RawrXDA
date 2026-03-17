; =====================================================================
; MASM RUNTIME QUICK REFERENCE - x64 ASSEMBLY DEVELOPERS
; =====================================================================
; Fast lookup for all exported functions and their signatures
; Calling convention: Microsoft x64 (rcx, rdx, r8, r9, [rsp+40] for extra args)
; =====================================================================

; =====================================================================
; MEMORY MANAGEMENT
; =====================================================================

; asm_malloc(size: rcx, alignment: rdx) -> rax
; Example:
;   mov rcx, 1024          ; 1024 bytes
;   mov rdx, 64            ; 64-byte alignment
;   call asm_malloc
;   test rax, rax          ; Check for NULL
;   jz allocation_failed
;   mov r12, rax           ; r12 = allocated pointer

; asm_free(ptr: rcx) -> void
; Example:
;   mov rcx, r12           ; rcx = pointer from asm_malloc
;   call asm_free

; asm_realloc(ptr: rcx, new_size: rdx) -> rax
; Example:
;   mov rcx, old_ptr
;   mov rdx, 2048          ; new size
;   call asm_realloc
;   ; rax = new pointer (or NULL on failure)

; =====================================================================
; THREAD SYNCHRONIZATION - MUTEXES
; =====================================================================

; asm_mutex_create() -> rax
; Example:
;   call asm_mutex_create
;   test rax, rax
;   jz mutex_create_failed
;   mov r8, rax            ; r8 = mutex handle

; asm_mutex_lock(handle: rcx) -> void
; Example:
;   mov rcx, r8            ; rcx = mutex handle
;   call asm_mutex_lock    ; Blocking lock
;   ; ... critical section ...

; asm_mutex_unlock(handle: rcx) -> void
; Example:
;   mov rcx, r8            ; rcx = mutex handle
;   call asm_mutex_unlock

; asm_mutex_destroy(handle: rcx) -> void
; Example:
;   mov rcx, r8
;   call asm_mutex_destroy

; =====================================================================
; THREAD SYNCHRONIZATION - EVENTS
; =====================================================================

; asm_event_create(manual_reset: rcx) -> rax
; Example:
;   mov rcx, 0             ; 0 = auto-reset, 1 = manual-reset
;   call asm_event_create
;   mov r9, rax            ; r9 = event handle

; asm_event_set(handle: rcx) -> void
; Example:
;   mov rcx, r9
;   call asm_event_set     ; Signals all waiters

; asm_event_reset(handle: rcx) -> void
; Example:
;   mov rcx, r9
;   call asm_event_reset

; asm_event_wait(handle: rcx, timeout_ms: rdx) -> rax
; Returns: 0 = signaled, 258 = timeout, -1 = error
; Example:
;   mov rcx, r9
;   mov rdx, 1000          ; 1 second timeout
;   call asm_event_wait
;   test rax, rax
;   jz event_signaled      ; rax = 0

; asm_event_destroy(handle: rcx) -> void
; Example:
;   mov rcx, r9
;   call asm_event_destroy

; =====================================================================
; ATOMIC OPERATIONS (no synchronization needed)
; =====================================================================

; asm_atomic_increment(ptr: rcx) -> rax
; Returns: new value
; Example:
;   lea rcx, [rel g_counter]
;   call asm_atomic_increment
;   ; rax = new value (e.g., 1, 2, 3, ...)

; asm_atomic_decrement(ptr: rcx) -> rax
; Example:
;   call asm_atomic_decrement

; asm_atomic_add(ptr: rcx, value: rdx) -> rax
; Example:
;   lea rcx, [rel g_total]
;   mov rdx, 256
;   call asm_atomic_add    ; Atomically adds 256
;   ; rax = new total

; asm_atomic_cmpxchg(ptr: rcx, old: rdx, new: r8) -> rax
; Returns: 1 if successful, 0 if failed
; Example:
;   lea rcx, [rel g_state]
;   mov rdx, 0             ; expected old value
;   mov r8, 1              ; new value to store
;   call asm_atomic_cmpxchg
;   cmp rax, 1
;   je swap_succeeded
;   jne swap_failed

; asm_atomic_xchg(ptr: rcx, value: rdx) -> rax
; Returns: old value
; Example:
;   lea rcx, [rel g_flags]
;   mov rdx, 0x0F
;   call asm_atomic_xchg
;   ; rax = old value

; =====================================================================
; STRING OPERATIONS
; =====================================================================

; asm_str_create(utf8_ptr: rcx, length: rdx) -> rax
; Example:
;   lea rcx, [rel my_string]  ; rcx = "Hello"
;   mov rdx, 5               ; rdx = 5 chars
;   call asm_str_create
;   test rax, rax
;   jz string_creation_failed
;   mov r10, rax             ; r10 = string handle

; asm_str_destroy(handle: rcx) -> void
; Example:
;   mov rcx, r10
;   call asm_str_destroy

; asm_str_length(handle: rcx) -> rax
; Returns: character count
; Example:
;   mov rcx, r10
;   call asm_str_length
;   ; rax = length (e.g., 5)

; asm_str_concat(str1: rcx, str2: rdx) -> rax
; Example:
;   mov rcx, r10             ; first string
;   mov rdx, r11             ; second string
;   call asm_str_concat
;   test rax, rax
;   jz concat_failed
;   mov r12, rax             ; r12 = new concatenated string

; asm_str_compare(str1: rcx, str2: rdx) -> rax
; Returns: -1, 0, or 1
; Example:
;   mov rcx, r10
;   mov rdx, r11
;   call asm_str_compare
;   cmp rax, 0
;   je strings_equal
;   jl str1_less_than_str2
;   jg str1_greater_than_str2

; asm_str_find(haystack: rcx, needle: rdx) -> rax
; Returns: offset in haystack, or -1 if not found
; Example:
;   mov rcx, r10             ; haystack
;   mov rdx, r11             ; needle
;   call asm_str_find
;   cmp rax, -1
;   je not_found
;   ; rax = offset where needle starts

; asm_str_substring(str: rcx, start: rdx, length: r8) -> rax
; Example:
;   mov rcx, r10
;   mov rdx, 2               ; start at offset 2
;   mov r8, 3                ; extract 3 chars
;   call asm_str_substring
;   ; rax = new substring string handle

; asm_str_to_utf16(utf8_handle: rcx) -> rax
; Returns: pointer to UTF-16 data (caller must asm_free)
; Example:
;   mov rcx, r10
;   call asm_str_to_utf16
;   ; rax = pointer to UTF-16 wide chars
;   ; ... use with Win32 APIs ...
;   ; mov rcx, rax
;   ; call asm_free    ; Free UTF-16 buffer

; asm_str_from_utf16(utf16_ptr: rcx) -> rax
; Example:
;   lea rcx, [rel wide_string]
;   call asm_str_from_utf16
;   ; rax = UTF-8 string handle

; =====================================================================
; EVENT LOOP & SIGNALS
; =====================================================================

; asm_event_loop_create(queue_size: rcx) -> rax
; Example:
;   mov rcx, 256           ; queue can hold 256 events
;   call asm_event_loop_create
;   test rax, rax
;   jz loop_create_failed
;   mov r13, rax           ; r13 = event loop handle

; asm_event_loop_register_signal(loop: rcx, signal_id: rdx, handler: r8) -> void
; handler signature: void handler(p1: rcx, p2: rdx, p3: r8)
; Example:
;   ; Define handler
;   my_handler PROC
;       ; rcx = param1, rdx = param2, r8 = param3
;       push rbx
;       ; ... handler code ...
;       pop rbx
;       ret
;   my_handler ENDP
;
;   ; Register it
;   mov rcx, r13           ; loop handle
;   mov rdx, 42            ; signal_id = 42
;   lea r8, [rel my_handler]  ; handler function address
;   call asm_event_loop_register_signal

; asm_event_loop_emit(loop: rcx, signal_id: rdx, p1: r8, p2: r9, p3: [rsp+40]) -> void
; Example:
;   mov rcx, r13           ; loop
;   mov rdx, 42            ; signal_id
;   mov r8, 100            ; param1
;   mov r9, 200            ; param2
;   mov qword ptr [rsp + 40], 300   ; param3
;   call asm_event_loop_emit

; asm_event_loop_process_one(loop: rcx) -> rax
; Returns: 1 if processed, 0 if queue empty, -1 if error
; Example:
;   mov rcx, r13
;   call asm_event_loop_process_one
;   cmp rax, 1
;   je event_was_processed
;   cmp rax, 0
;   je queue_was_empty
;   ; rax = -1, error

; asm_event_loop_process_all(loop: rcx) -> rax
; Returns: count of processed events
; Example:
;   mov rcx, r13
;   call asm_event_loop_process_all
;   ; rax = number of events processed

; asm_event_loop_destroy(loop: rcx) -> void
; Example:
;   mov rcx, r13
;   call asm_event_loop_destroy

; =====================================================================
; HOTPATCH OPERATIONS
; =====================================================================

; hotpatch_initialize() -> void
; Must call once at startup
; Example:
;   call hotpatch_initialize

; hotpatch_apply(target_ptr: rcx, patch_data: rdx, size: r8) -> rax
; Returns: 0 = success, -1 = error
; Example:
;   mov rcx, target_address   ; target memory to patch
;   lea rdx, [rel patch_data] ; patch bytes
;   mov r8, patch_size        ; number of bytes
;   call hotpatch_apply
;   test rax, rax
;   jz patch_succeeded

; =====================================================================
; EXAMPLE: COMPLETE WORKFLOW
; =====================================================================

; Hypothetical complete example combining all layers:

complete_example PROC
    push rbx
    sub rsp, 32
    
    ; 1. Initialize
    call hotpatch_initialize
    
    ; 2. Allocate buffer
    mov rcx, 1024
    mov rdx, 64
    call asm_malloc
    mov rbx, rax            ; rbx = buffer
    
    ; 3. Create string
    lea rcx, [rel msg]
    mov rdx, 13
    call asm_str_create
    mov r12, rax            ; r12 = string handle
    
    ; 4. Create mutex for critical section
    call asm_mutex_create
    mov r13, rax            ; r13 = mutex
    
    ; 5. Lock
    mov rcx, r13
    call asm_mutex_lock
    
    ; 6. Critical section work
    mov rcx, rbx
    mov rax, 0x90           ; NOP instruction
    mov [rcx], al
    
    ; 7. Unlock
    mov rcx, r13
    call asm_mutex_unlock
    
    ; 8. Cleanup
    mov rcx, r12
    call asm_str_destroy
    
    mov rcx, rbx
    call asm_free
    
    mov rcx, r13
    call asm_mutex_destroy
    
    add rsp, 32
    pop rbx
    ret
complete_example ENDP

; =====================================================================
; ERROR HANDLING PATTERNS
; =====================================================================

; Pattern 1: Allocation with immediate check
allocate_or_fail PROC
    mov rcx, 256
    mov rdx, 16
    call asm_malloc
    test rax, rax
    jz allocation_failed    ; NULL = failure
    ; ... success path ...
    ret
allocation_failed:
    ; Handle error
    ret
allocate_or_fail ENDP

; Pattern 2: Atomic operation with retry
atomic_retry PROC
    mov rcx, 0              ; retry count
retry_loop:
    cmp rcx, 10
    jge atomic_failed
    
    lea r8, [rel g_value]
    mov rdx, 0              ; expected
    mov r8, 1               ; new
    call asm_atomic_cmpxchg
    
    cmp rax, 1              ; Success?
    je atomic_success
    
    inc rcx
    jmp retry_loop
    
atomic_success:
    ret
atomic_failed:
    ret
atomic_retry ENDP

; =====================================================================
; DATA ALIGNMENT & LAYOUT HELPERS
; =====================================================================

; Allocate aligned string buffer
alloc_string_buffer PROC
    mov rcx, rdx            ; rdx = length
    add rcx, 40 + 1         ; +40 metadata, +1 null terminator
    mov rdx, 16
    call asm_malloc
    ret
alloc_string_buffer ENDP

; =====================================================================
; PERFORMANCE NOTES
; =====================================================================
; - asm_malloc: ~500 ns (cached heap)
; - asm_mutex_lock: ~50 ns uncontended
; - asm_atomic_increment: ~10 ns (1-2 CPU cycles)
; - asm_event_emit: ~300 ns (ring buffer + atomic)
; - asm_str_concat: ~2 μs (2x memcpy)
; - event dispatch: ~500 ns (dequeue + call)
; =====================================================================

; =====================================================================
; DEBUGGING TIPS
; =====================================================================
; 1. Validate return values (rax) immediately after calls
; 2. Use registers r8-r15 (non-volatile) to save state across calls
; 3. Check magic markers in metadata for corruption: 0xDEADBEEFCAFEBABE
; 4. Use /Fl flag when assembling to generate .lst files:
;    ml64.exe /c /Zf /Fl /Fo obj.obj src.asm
; 5. For critical sections, use asm_atomic_* when possible
;    (lower latency than asm_mutex_lock)
; 6. Set breakpoints on asm_free to catch double-free
; =====================================================================

END
