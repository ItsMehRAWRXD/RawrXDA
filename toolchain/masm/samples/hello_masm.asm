; Hello World - MASM x64 console sample
; Build: .\Unified-PowerShell-Compiler-RawrXD.ps1 -Source samples\hello_masm.asm -Architecture x64 -SubSystem console -Entry main
; Or:   .\Build-MASM-Standalone.ps1 -Source samples\hello_masm.asm -Architecture x64 -Entry main
EXTERN ExitProcess : PROC
EXTERN GetStdHandle : PROC
EXTERN WriteFile : PROC

.DATA
msg DB "Hello from MASM (RawrXD toolchain)", 13, 10, 0
msgLen = $ - msg

.CODE
main PROC
    sub rsp, 28h
    mov rcx, -11
    call GetStdHandle
    mov rcx, rax
    lea rdx, msg
    mov r8, msgLen
    xor r9, r9
    mov qword ptr [rsp + 20h], 0
    call WriteFile
    xor ecx, ecx
    call ExitProcess
main ENDP
END
