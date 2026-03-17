; Ultra minimal x64 NASM test for Windows
bits 64
default rel

section .data
    szTitle:     db "Test", 0
    szMessage:   db "Hello NASM!", 0

section .text
global WinMain
extern MessageBoxA
extern ExitProcess

WinMain:
    push rbp
    mov rbp, rsp
    sub rsp, 32         ; shadow space
    
    xor ecx, ecx        ; hWnd = NULL
    lea rdx, [szMessage]
    lea r8, [szTitle]
    mov r9d, 0x40       ; MB_ICONINFORMATION
    call MessageBoxA
    
    xor ecx, ecx
    call ExitProcess
