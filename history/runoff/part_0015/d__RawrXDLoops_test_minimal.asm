; Minimal x64 Windows application test
option casemap:none

extern MessageBoxA:proc
extern ExitProcess:proc

.data
szTitle     db "Test",0
szMessage   db "Hello from x64 ASM!",0

.code

WinMain proc
    push rbp
    mov rbp, rsp
    sub rsp, 28h
    
    xor rcx, rcx
    lea rdx, szMessage
    lea r8, szTitle
    mov r9d, 40h
    call MessageBoxA
    
    xor rcx, rcx
    call ExitProcess
WinMain endp

end
