;=====================================================================
; minimal_test.asm - Minimal x64 MASM test of heap operations
;=====================================================================

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
    ; Get stdout handle
    mov rcx, -11
    sub rsp, 32
    call GetStdHandle
    add rsp, 32
    mov r15, rax        ; r15 = console handle
    
    ; Get process heap
    sub rsp, 32
    call GetProcessHeap
    add rsp, 32
    mov r14, rax        ; r14 = heap handle
    
    test r14, r14
    jz fail_heap
    
    ; Try to allocate 1024 bytes
    mov rcx, r14        ; heap
    xor rdx, rdx        ; flags
    mov r8, 1024        ; size
    sub rsp, 32
    call HeapAlloc
    add rsp, 32
    
    test rax, rax
    jz fail_alloc
    
    ; Write succeeded
    mov rcx, r15
    mov rdx, OFFSET msg_ok
    mov r8d, 10
    lea r9, [rsp]
    mov qword ptr [rsp + 32], 0
    sub rsp, 40
    call WriteConsoleA
    add rsp, 40
    
    ; Free the memory
    mov rcx, r14        ; heap
    xor rdx, rdx        ; flags
    mov r8, [r14 - 8]   ; This is wrong, but let's see
    sub rsp, 32
    call HeapFree
    add rsp, 32
    
    xor rcx, rcx
    call ExitProcess
    
fail_heap:
    mov rcx, r15
    mov rdx, OFFSET msg_heap_fail
    mov r8d, 15
    lea r9, [rsp]
    mov qword ptr [rsp + 32], 0
    sub rsp, 40
    call WriteConsoleA
    add rsp, 40
    mov rcx, 1
    call ExitProcess
    
fail_alloc:
    mov rcx, r15
    mov rdx, OFFSET msg_alloc_fail
    mov r8d, 16
    lea r9, [rsp]
    mov qword ptr [rsp + 32], 0
    sub rsp, 40
    call WriteConsoleA
    add rsp, 40
    mov rcx, 2
    call ExitProcess
    
    ret
mainCRTStartup ENDP

.data
msg_ok DB "OK: Alloc", 13, 10, 0
msg_heap_fail DB "Fail: heap", 13, 10, 0
msg_alloc_fail DB "Fail: alloc", 13, 10, 0

END
