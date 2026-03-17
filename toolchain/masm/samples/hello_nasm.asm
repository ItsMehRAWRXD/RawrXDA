default rel

extern ExitProcess

section .text
global start
start:
    sub rsp, 40
    xor ecx, ecx
    call ExitProcess
