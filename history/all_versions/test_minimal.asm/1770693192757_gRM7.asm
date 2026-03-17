; Minimal test to isolate crash
includelib kernel32.lib
includelib user32.lib
includelib advapi32.lib
includelib shell32.lib

extern ExitProcess: proc
extern GetStdHandle: proc
extern WriteConsoleA: proc
extern lstrlenA: proc

STD_OUTPUT_HANDLE equ -11

.data
    szTestMsg db "Test: Hello from minimal entry", 13, 10, 0
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
    lea rcx, szTestMsg
    call Print
    xor rcx, rcx
    call ExitProcess
_start_entry ENDP

END
