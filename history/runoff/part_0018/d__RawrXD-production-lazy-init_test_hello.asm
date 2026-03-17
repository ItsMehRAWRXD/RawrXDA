option casemap:none

externdef ExitProcess:qword
externdef GetStdHandle:qword
externdef WriteConsoleA:qword

.data
msg db "Hello from MASM64!", 10, 0

.code
_start proc
    sub     rsp, 48

    mov     ecx, -11
    call    GetStdHandle

    mov     rcx, rax
    lea     rdx, msg
    mov     r8, 19
    lea     r9, [rsp+40]
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA

    xor     ecx, ecx
    call    ExitProcess
_start endp
end
