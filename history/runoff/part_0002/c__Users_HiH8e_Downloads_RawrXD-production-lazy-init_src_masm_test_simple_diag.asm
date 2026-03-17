;=====================================================================
; test_simple_diag.asm - Minimal diagnostic test for memory allocator
; Tests only heap allocation without complex logging
;=====================================================================

.data

.code

EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN ExitProcess:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC

PUBLIC mainCRTStartup

ALIGN 16
mainCRTStartup PROC
    ; Get console handle
    mov rcx, -11
    sub rsp, 32
    call GetStdHandle
    add rsp, 32
    mov rbx, rax
    
    ; Write initial message
    mov rcx, rbx
    mov rdx, OFFSET msg1
    mov r8d, 20
    lea r9, [rsp]
    mov qword ptr [rsp + 32], 0
    sub rsp, 40
    call WriteConsoleA
    add rsp, 40
    
    ; Try GetProcessHeap
    sub rsp, 32
    call GetProcessHeap
    add rsp, 32
    test rax, rax
    jz heap_fail
    
    ; Try HeapAlloc(heap, 0, 1024)
    mov rcx, rax
    mov rdx, 0
    mov r8, 1024
    sub rsp, 32
    call HeapAlloc
    add rsp, 32
    test rax, rax
    jz alloc_fail
    
    ; Write success message
    mov rcx, rbx
    mov rdx, OFFSET msg2
    mov r8d, 20
    lea r9, [rsp]
    mov qword ptr [rsp + 32], 0
    sub rsp, 40
    call WriteConsoleA
    add rsp, 40
    
    xor rcx, rcx
    jmp do_exit
    
heap_fail:
    mov rcx, rbx
    mov rdx, OFFSET msg_heap_fail
    mov r8d, 25
    lea r9, [rsp]
    mov qword ptr [rsp + 32], 0
    sub rsp, 40
    call WriteConsoleA
    add rsp, 40
    mov rcx, 1
    jmp do_exit
    
alloc_fail:
    mov rcx, rbx
    mov rdx, OFFSET msg_alloc_fail
    mov r8d, 25
    lea r9, [rsp]
    mov qword ptr [rsp + 32], 0
    sub rsp, 40
    call WriteConsoleA
    add rsp, 40
    mov rcx, 2
    
do_exit:
    call ExitProcess
    ret
mainCRTStartup ENDP

.data
msg1 DB "Testing heap...", 13, 10, 0
msg2 DB "SUCCESS: Alloc OK", 13, 10, 0
msg_heap_fail DB "FAIL: GetProcessHeap", 13, 10, 0
msg_alloc_fail DB "FAIL: HeapAlloc returned NULL", 13, 10, 0

END
