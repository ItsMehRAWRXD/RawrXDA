; boot.asm – Minimal x64 entry point for RawrXD
option casemap:none

extern ExitProcess:proc
extern GetCommandLineA:proc
extern main:proc

.code
_start proc
    ; Reserve shadow space (32 bytes) and keep 16-byte alignment
    sub rsp, 40h

    ; Touch command line to mirror prior behavior (return value unused)
    call GetCommandLineA

    ; Invoke C++ main(int argc, char** argv) with stub args (1, NULL)
    mov rcx, 1
    xor rdx, rdx
    call main

    ; Exit with main's return code
    mov ecx, eax
    add rsp, 40h
    jmp ExitProcess
_start endp

end