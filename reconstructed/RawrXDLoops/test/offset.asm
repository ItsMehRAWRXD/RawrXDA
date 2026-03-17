; Minimal x64 Windows test with MOV offset
option casemap:none

extern MessageBoxA:proc
extern ExitProcess:proc

.data
szTitle     db "Test",0
szMessage   db "Hello",0

.code

WinMain proc
    sub rsp, 28h
    
    xor ecx, ecx
    mov rdx, offset szMessage
    mov r8, offset szTitle
    mov r9d, 40h
    call MessageBoxA
    
    xor ecx, ecx
    call ExitProcess
WinMain endp

end
