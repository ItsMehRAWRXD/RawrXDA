; boot.asm – Minimal Win32 entry point that forwards to C++ main()
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc
includelib kernel32.lib
includelib user32.lib

; Declare the C++ entry point (extern "C" int main(int argc, char** argv))
extrn  main:proc

.data
    szCmdLine db 256 dup (0)

.code
start:
    ; Get command line (Unicode) and convert to ANSI for C++ main
    invoke GetCommandLineA
    mov  esi, eax
    ; Copy to buffer (simple, no quoting handling)
    mov  edi, offset szCmdLine
    mov  ecx, sizeof szCmdLine-1
    rep  movsb
    mov  byte ptr [edi],0

    ; Call C++ main (argc = 1, argv[0] = program name)
    push 0                ; argv (NULL for now)
    push 1                ; argc
    call  main

    ; Exit process with return code from main
    push  eax
    call  ExitProcess
end start