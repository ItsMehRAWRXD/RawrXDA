; boot.asm – Minimal x64 entry point for RawrXD
option casemap:none

extern ExitProcess:proc
extern GetModuleHandleA:proc
extern WinMain:proc

.code
_start proc
    ; Reserve shadow space (32 bytes) and keep 16-byte alignment
    sub rsp, 40h

    ; hInstance = GetModuleHandleA(NULL)
    xor rcx, rcx
    call GetModuleHandleA

    ; Invoke WinMain(hInstance, NULL, NULL, SW_SHOWDEFAULT)
    mov rcx, rax    ; HINSTANCE
    xor rdx, rdx    ; hPrevInstance
    xor r8,  r8     ; lpCmdLine
    mov r9d, 10     ; nCmdShow = SW_SHOWDEFAULT
    call WinMain

    ; Exit with main's return code
    mov ecx, eax
    add rsp, 40h
    jmp ExitProcess
_start endp

end