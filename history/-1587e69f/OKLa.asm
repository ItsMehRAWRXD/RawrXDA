; Minimal test: simulate _start_entry -> MainDispatcher -> Print -> ExitProcess
includelib kernel32.lib
includelib user32.lib

extern GetStdHandle: proc
extern WriteConsoleA: proc
extern ExitProcess: proc
extern lstrlenA: proc
extern GetCommandLineA: proc

STD_OUTPUT_HANDLE equ -11

.data
    hStdOut dq 0
    szMsg db "Hello from MainDispatcher!",13,10,0
    dwWritten dd 0

.code

Print PROC
    push rbx
    push r12
    push r14
    sub rsp, 30h
    
    mov r12, rcx       ; save string ptr
    
    ; Get string length
    call lstrlenA
    mov r14d, eax       ; save length
    
    ; Get stdout handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rbx, rax
    
    ; WriteConsoleA
    mov rcx, rbx        ; hConsole
    mov rdx, r12        ; lpBuffer
    mov r8d, r14d       ; nNumberOfCharsToWrite
    lea r9, dwWritten   ; lpNumberOfCharsWritten
    mov qword ptr [rsp+20h], 0  ; lpReserved
    call WriteConsoleA
    
    add rsp, 30h
    pop r14
    pop r12
    pop rbx
    ret
Print ENDP

MainDispatcher PROC
    push r15
    sub rsp, 40h
    
    ; Just print a message and exit
    lea rcx, szMsg
    call Print
    
    add rsp, 40h
    pop r15
    ret
MainDispatcher ENDP

_start_entry PROC
    sub rsp, 28h
    call MainDispatcher
    xor ecx, ecx
    call ExitProcess
_start_entry ENDP

END
