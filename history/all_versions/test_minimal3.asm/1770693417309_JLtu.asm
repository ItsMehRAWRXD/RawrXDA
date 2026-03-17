; Minimal test v3 - with sub rsp, 20h
includelib kernel32.lib

extern ExitProcess: proc
extern GetStdHandle: proc
extern WriteConsoleA: proc
extern lstrlenA: proc

STD_OUTPUT_HANDLE equ -11

.data
    szMsg1 db "Step 1: sub rsp, 20h test", 13, 10, 0
    dwWritten dq 0

.code

Print PROC
    push rbx
    push r12
    push r14
    sub rsp, 30h
    
    mov rbx, rcx
    call lstrlenA
    mov r12, rax
    
    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r14, rax
    
    mov rcx, r14
    mov rdx, rbx
    mov r8, r12
    lea r9, dwWritten
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA
    
    add rsp, 30h
    pop r14
    pop r12
    pop rbx
    ret
Print ENDP

_start_entry PROC
    sub rsp, 20h
    lea rcx, szMsg1
    call Print
    mov ecx, 42
    call ExitProcess
_start_entry ENDP

END
