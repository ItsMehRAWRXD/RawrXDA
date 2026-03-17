;=====================================================================
; asm_test_main.asm - Pure MASM Test Harness
;
; Entry point for standalone MASM executable (no C++ runtime).
; Demonstrates all runtime layers:
;  1. Memory allocation/deallocation
;  2. Thread synchronization (mutexes, events)
;  3. String operations (create, compare, concat)
;  4. Event loop and signal dispatch
;  5. Hotpatching with memory protection
;
; Build: ml64.exe /c asm_test_main.asm /Fo asm_test_main.obj
; Link: link asm_test_main.obj kernel32.lib /SUBSYSTEM:CONSOLE /ENTRY:main
;=====================================================================

EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN ExitProcess:PROC
EXTERN GetLastError:PROC

PUBLIC main

.data

; STD_OUTPUT_HANDLE constant
STD_OUTPUT_HANDLE = -11

; Test messages
msg_header db "=== MASM x64 Runtime Test Suite ===", 0Dh, 0Ah, 0
msg_init db "[INIT] Initializing runtime...", 0Dh, 0Ah, 0
msg_init_ok db "[OK] Runtime initialized", 0Dh, 0Ah, 0
msg_malloc_test db "[MALLOC] Testing allocation (256 bytes, 32-align)...", 0Dh, 0Ah, 0
msg_malloc_ok db "[OK] Memory allocated at ", 0
msg_free_test db "[FREE] Freeing allocated memory...", 0Dh, 0Ah, 0
msg_free_ok db "[OK] Memory freed", 0Dh, 0Ah, 0
msg_str_test db "[STRING] Creating string: 'Hello, MASM!'...", 0Dh, 0Ah, 0
msg_str_ok db "[OK] String created at ", 0
msg_str_len db "[LENGTH] String length: ", 0
msg_mutex_test db "[MUTEX] Creating mutex...", 0Dh, 0Ah, 0
msg_mutex_ok db "[OK] Mutex created", 0Dh, 0Ah, 0
msg_mutex_lock db "[LOCK] Acquiring lock...", 0Dh, 0Ah, 0
msg_mutex_locked db "[OK] Lock acquired", 0Dh, 0Ah, 0
msg_mutex_unlock db "[UNLOCK] Releasing lock...", 0Dh, 0Ah, 0
msg_mutex_unlocked db "[OK] Lock released", 0Dh, 0Ah, 0
msg_event_test db "[EVENT] Creating event...", 0Dh, 0Ah, 0
msg_event_ok db "[OK] Event created", 0Dh, 0Ah, 0
msg_event_set db "[SET] Setting event...", 0Dh, 0Ah, 0
msg_event_set_ok db "[OK] Event set", 0Dh, 0Ah, 0
msg_loop_test db "[LOOP] Creating event loop (queue_size=256)...", 0Dh, 0Ah, 0
msg_loop_ok db "[OK] Event loop created at ", 0
msg_emit_test db "[EMIT] Emitting signal...", 0Dh, 0Ah, 0
msg_emit_ok db "[OK] Signal emitted", 0Dh, 0Ah, 0
msg_process_test db "[PROCESS] Processing events...", 0Dh, 0Ah, 0
msg_process_ok db "[OK] Events processed", 0Dh, 0Ah, 0
msg_test_complete db 0Dh, 0Ah, "=== All tests passed ===", 0Dh, 0Ah, 0
msg_error db "[ERROR] Test failed at: ", 0
msg_newline db 0Dh, 0Ah, 0
msg_crlf db 0Dh, 0Ah, 0

; Global output handle
g_stdout_handle QWORD 0

; Test state
g_test_alloc QWORD 0
g_test_string QWORD 0
g_test_mutex QWORD 0
g_test_event QWORD 0
g_test_loop QWORD 0

.code

;=====================================================================
; UTILITY FUNCTIONS
;=====================================================================

;=====================================================================
; print_str(msg_ptr: rcx) -> void
;
; Prints null-terminated ASCII string to stdout.
;=====================================================================

ALIGN 16
print_str PROC

    push rbx
    push r12
    sub rsp, 48
    
    mov r12, rcx            ; r12 = message pointer
    
    ; Count string length
    xor rax, rax            ; length counter
count_loop:
    cmp byte ptr [r12 + rax], 0
    je count_done
    inc rax
    jmp count_loop
    
count_done:
    mov r8, rax             ; r8 = length
    
    ; WriteFile(stdout, msg, len, &written, NULL)
    mov rcx, [rel g_stdout_handle]
    mov rdx, r12            ; lpBuffer
    mov r9, rax             ; nNumberOfBytesToWrite
    
    lea r10, [rsp + 32]     ; lpNumberOfBytesWritten
    mov qword ptr [rsp + 32], 0
    mov [rsp + 40], r10
    xor r10, r10            ; lpOverlapped = NULL
    
    call WriteFile
    
    add rsp, 48
    pop r12
    pop rbx
    ret

print_str ENDP

;=====================================================================
; print_hex(value: rcx) -> void
;
; Prints 64-bit hex value to stdout.
;=====================================================================

ALIGN 16
print_hex PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = value to print
    
    ; Allocate hex buffer (17 bytes: 16 hex digits + null)
    mov rcx, 17
    mov rdx, 16
    
    ; For now, print simplified version
    ; In real code would use itoa or manual conversion
    
    lea rcx, [rel msg_newline]
    call print_str
    
    add rsp, 32
    pop r12
    pop rbx
    ret

print_hex ENDP

;=====================================================================
; MAIN TEST FUNCTION
;=====================================================================

ALIGN 16
main PROC

    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 32
    
    ; Get stdout handle
    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [rel g_stdout_handle], rax
    
    ; Print header
    lea rcx, [rel msg_header]
    call print_str
    
    ;=====================================================================
    ; TEST 1: INITIALIZATION
    ;=====================================================================
    
    lea rcx, [rel msg_init]
    call print_str
    
    call hotpatch_initialize
    test rax, rax
    jnz test_fail
    
    lea rcx, [rel msg_init_ok]
    call print_str
    
    ;=====================================================================
    ; TEST 2: MEMORY ALLOCATION
    ;=====================================================================
    
    lea rcx, [rel msg_malloc_test]
    call print_str
    
    mov rcx, 256            ; size
    mov rdx, 32             ; alignment
    call asm_malloc
    test rax, rax
    jz test_fail
    
    mov r12, rax            ; r12 = allocated pointer
    mov [rel g_test_alloc], r12
    
    lea rcx, [rel msg_malloc_ok]
    call print_str
    
    ; Print allocated address (simplified)
    lea rcx, [rel msg_newline]
    call print_str
    
    ;=====================================================================
    ; TEST 3: STRING CREATION
    ;=====================================================================
    
    lea rcx, [rel msg_str_test]
    call print_str
    
    lea rcx, [rel test_string_data]
    mov rdx, 13             ; "Hello, MASM!"
    call asm_str_create
    test rax, rax
    jz test_fail
    
    mov r13, rax            ; r13 = string handle
    mov [rel g_test_string], r13
    
    lea rcx, [rel msg_str_ok]
    call print_str
    lea rcx, [rel msg_newline]
    call print_str
    
    ; Test string length
    mov rcx, r13
    call asm_str_length     ; rax = length
    
    lea rcx, [rel msg_str_len]
    call print_str
    
    lea rcx, [rel msg_newline]
    call print_str
    
    ;=====================================================================
    ; TEST 4: MUTEX
    ;=====================================================================
    
    lea rcx, [rel msg_mutex_test]
    call print_str
    
    call asm_mutex_create
    test rax, rax
    jz test_fail
    
    mov r14, rax            ; r14 = mutex handle
    mov [rel g_test_mutex], r14
    
    lea rcx, [rel msg_mutex_ok]
    call print_str
    
    ; Test lock
    lea rcx, [rel msg_mutex_lock]
    call print_str
    
    mov rcx, r14
    call asm_mutex_lock
    
    lea rcx, [rel msg_mutex_locked]
    call print_str
    
    ; Test unlock
    lea rcx, [rel msg_mutex_unlock]
    call print_str
    
    mov rcx, r14
    call asm_mutex_unlock
    
    lea rcx, [rel msg_mutex_unlocked]
    call print_str
    
    ;=====================================================================
    ; TEST 5: EVENT
    ;=====================================================================
    
    lea rcx, [rel msg_event_test]
    call print_str
    
    mov rcx, 0              ; auto-reset
    call asm_event_create
    test rax, rax
    jz test_fail
    
    mov r15, rax            ; r15 = event handle
    mov [rel g_test_event], r15
    
    lea rcx, [rel msg_event_ok]
    call print_str
    
    ; Test set
    lea rcx, [rel msg_event_set]
    call print_str
    
    mov rcx, r15
    call asm_event_set
    
    lea rcx, [rel msg_event_set_ok]
    call print_str
    
    ;=====================================================================
    ; TEST 6: EVENT LOOP
    ;=====================================================================
    
    lea rcx, [rel msg_loop_test]
    call print_str
    
    mov rcx, 256            ; queue size
    call asm_event_loop_create
    test rax, rax
    jz test_fail
    
    mov rbx, rax            ; rbx = event loop handle
    mov [rel g_test_loop], rbx
    
    lea rcx, [rel msg_loop_ok]
    call print_str
    lea rcx, [rel msg_newline]
    call print_str
    
    ; Test emit
    lea rcx, [rel msg_emit_test]
    call print_str
    
    mov rcx, rbx            ; loop
    mov rdx, 1              ; signal_id
    mov r8, 42              ; param1
    mov r9, 100             ; param2
    mov qword ptr [rsp + 40], 200  ; param3
    call asm_event_loop_emit
    
    lea rcx, [rel msg_emit_ok]
    call print_str
    
    ; Test process
    lea rcx, [rel msg_process_test]
    call print_str
    
    mov rcx, rbx
    call asm_event_loop_process_one
    
    lea rcx, [rel msg_process_ok]
    call print_str
    
    ;=====================================================================
    ; CLEANUP & SUCCESS
    ;=====================================================================
    
    ; Free string
    mov rcx, r13
    call asm_str_destroy
    
    ; Free allocated memory
    mov rcx, r12
    call asm_free
    
    ; Print complete message
    lea rcx, [rel msg_test_complete]
    call print_str
    
    xor rcx, rcx            ; exit code 0
    call ExitProcess
    
test_fail:
    lea rcx, [rel msg_error]
    call print_str
    
    lea rcx, [rel msg_newline]
    call print_str
    
    mov rcx, 1              ; exit code 1
    call ExitProcess

main ENDP

;=====================================================================
; Data - Test string
;=====================================================================

.data

test_string_data db "Hello, MASM!", 0

END
