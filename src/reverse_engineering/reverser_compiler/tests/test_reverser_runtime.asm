; test_reverser_runtime.asm
; Test program for Reverser runtime and bytecode generator
; Demonstrates stack-based execution of Reverser programs

%include "reverser_runtime.asm"
%include "reverser_bytecode_gen.asm"

section .data
    ; Test messages
    msg_runtime_init db "Initializing Reverser runtime...", 10, 0
    msg_runtime_init_len equ $ - msg_runtime_init - 1
    
    msg_stack_test db "Testing stack operations...", 10, 0
    msg_stack_test_len equ $ - msg_stack_test - 1
    
    msg_heap_test db "Testing heap operations...", 10, 0
    msg_heap_test_len equ $ - msg_heap_test - 1
    
    msg_bytecode_test db "Testing bytecode generation...", 10, 0
    msg_bytecode_test_len equ $ - msg_bytecode_test - 1
    
    msg_gc_test db "Testing garbage collection...", 10, 0
    msg_gc_test_len equ $ - msg_gc_test - 1
    
    msg_test_complete db "All tests completed successfully!", 10, 0
    msg_test_complete_len equ $ - msg_test_complete - 1
    
    ; Test bytecode
    test_bytecode db 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A  ; PUSH_IMM 42
                  db 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A  ; PUSH_IMM 10
                  db 0x04  ; ADD
                  db 0x0E  ; DUP
                  db 0x0F  ; SWAP
                  db 0x10  ; ROT
                  db 0x09  ; RET
    test_bytecode_len equ $ - test_bytecode

section .bss
    ; Test variables
    test_result resq 1
    test_objects resq 10
    test_bytecode_buffer resb 1024

section .text
    global _start
    extern runtime_init
    extern runtime_cleanup
    extern stack_push
    extern stack_pop
    extern stack_peek
    extern stack_dup
    extern stack_swap
    extern stack_rot
    extern heap_alloc
    extern heap_free
    extern gc_collect
    extern runtime_execute
    extern bytecode_init
    extern bytecode_generate

; Print a string
; Input: rdi = pointer to string, rsi = string length
print_string:
    push rax
    push rdx
    push rcx
    
    mov rax, 1      ; syscall number for write
    mov rdx, rsi    ; string length
    mov rsi, rdi    ; string pointer
    mov rdi, 1      ; file descriptor (stdout)
    syscall
    
    pop rcx
    pop rdx
    pop rax
    ret

; Test stack operations
test_stack_operations:
    push rbx
    push rcx
    push rdx
    
    ; Print test message
    mov rdi, msg_stack_test
    mov rsi, msg_stack_test_len
    call print_string
    
    ; Test push and pop
    mov rdi, 42
    call stack_push
    mov rdi, 10
    call stack_push
    
    ; Test peek
    call stack_peek
    mov [test_result], rax
    
    ; Test pop
    call stack_pop
    mov [test_result], rax
    
    ; Test dup
    mov rdi, 100
    call stack_push
    call stack_dup
    
    ; Test swap
    mov rdi, 200
    call stack_push
    call stack_swap
    
    ; Test rot
    mov rdi, 300
    call stack_push
    call stack_rot
    
    ; Clear stack
    call stack_pop
    call stack_pop
    call stack_pop
    call stack_pop
    call stack_pop
    
    pop rdx
    pop rcx
    pop rbx
    ret

; Test heap operations
test_heap_operations:
    push rbx
    push rcx
    push rdx
    
    ; Print test message
    mov rdi, msg_heap_test
    mov rsi, msg_heap_test_len
    call print_string
    
    ; Allocate some objects
    mov rdi, 64
    call heap_alloc
    mov [test_objects], rax
    
    mov rdi, 128
    call heap_alloc
    mov [test_objects + 8], rax
    
    mov rdi, 256
    call heap_alloc
    mov [test_objects + 16], rax
    
    ; Free some objects
    mov rdi, [test_objects + 8]
    call heap_free
    
    ; Allocate more objects to trigger GC
    mov rbx, 0
.alloc_loop:
    cmp rbx, 20
    jge .alloc_done
    
    mov rdi, 32
    call heap_alloc
    mov [test_objects + rbx * 8], rax
    
    inc rbx
    jmp .alloc_loop

.alloc_done:
    pop rdx
    pop rcx
    pop rbx
    ret

; Test garbage collection
test_garbage_collection:
    push rbx
    push rcx
    push rdx
    
    ; Print test message
    mov rdi, msg_gc_test
    mov rsi, msg_gc_test_len
    call print_string
    
    ; Allocate many objects
    mov rbx, 0
.alloc_loop:
    cmp rbx, 100
    jge .alloc_done
    
    mov rdi, 16
    call heap_alloc
    mov [test_objects + rbx * 8], rax
    
    inc rbx
    jmp .alloc_loop

.alloc_done:
    ; Free some objects
    mov rbx, 0
.free_loop:
    cmp rbx, 50
    jge .free_done
    
    mov rdi, [test_objects + rbx * 8]
    call heap_free
    
    inc rbx
    jmp .free_loop

.free_done:
    ; Trigger garbage collection
    call gc_collect
    
    pop rdx
    pop rcx
    pop rbx
    ret

; Test bytecode generation
test_bytecode_generation:
    push rbx
    push rcx
    push rdx
    
    ; Print test message
    mov rdi, msg_bytecode_test
    mov rsi, msg_bytecode_test_len
    call print_string
    
    ; Initialize bytecode generator
    call bytecode_init
    
    ; Generate test bytecode
    mov rdi, test_bytecode
    mov rsi, test_bytecode_len
    call runtime_execute
    
    pop rdx
    pop rcx
    pop rbx
    ret

; Test runtime execution
test_runtime_execution:
    push rbx
    push rcx
    push rdx
    
    ; Execute test bytecode
    mov rdi, test_bytecode
    mov rsi, test_bytecode_len
    call runtime_execute
    
    ; Get result from stack
    call stack_pop
    mov [test_result], rax
    
    pop rdx
    pop rcx
    pop rbx
    ret

; Main test function
_start:
    ; Print initialization message
    mov rdi, msg_runtime_init
    mov rsi, msg_runtime_init_len
    call print_string
    
    ; Initialize runtime
    call runtime_init
    
    ; Run tests
    call test_stack_operations
    call test_heap_operations
    call test_garbage_collection
    call test_bytecode_generation
    call test_runtime_execution
    
    ; Print completion message
    mov rdi, msg_test_complete
    mov rsi, msg_test_complete_len
    call print_string
    
    ; Cleanup runtime
    call runtime_cleanup
    
    ; Exit
    mov rax, 60     ; syscall number for exit
    xor rdi, rdi    ; exit code 0
    syscall
