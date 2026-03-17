; Minimal test v2 to isolate crash
includelib kernel32.lib

extern ExitProcess: proc
extern GetStdHandle: proc
extern WriteConsoleA: proc
extern lstrlenA: proc

STD_OUTPUT_HANDLE equ -11

.data
    szMsg1 db "Step 1: Before ExitProcess", 13, 10, 0
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
    sub rsp, 28h   ; 28h to align: entry(0 mod 16) + 28h = 40 = 8 mod 16 for call targets
    lea rcx, szMsg1
    call Print
    mov ecx, 42    ; exit with code 42 so we know it reached here
    call ExitProcess
    ; never returns
_start_entry ENDP

END
