; Hello World - MASM x86 (32-bit) console sample
; Build: .\Unified-PowerShell-Compiler-RawrXD.ps1 -Source samples\hello_masm_x86.asm -Architecture x86 -SubSystem console -Entry main
.386
.MODEL flat, stdcall
OPTION casemap:none

EXTERN GetStdHandle@4 : PROC
EXTERN WriteFile@20 : PROC
EXTERN ExitProcess@4 : PROC

STD_OUTPUT_HANDLE = -11

.DATA
msg DB "Hello from MASM x86 (RawrXD toolchain)", 13, 10, 0
msgLen = $ - msg

.CODE
main PROC
    push STD_OUTPUT_HANDLE
    call GetStdHandle@4
    push 0
    push 0
    push msgLen
    push OFFSET msg
    push eax
    call WriteFile@20
    push 0
    call ExitProcess@4
main ENDP
END main
