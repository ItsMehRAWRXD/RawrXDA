; boot.asm – Minimal Win32 entry point that forwards to C++ main()
.386
.model flat, stdcall

; Basic function prototypes
ExitProcess PROTO STDCALL :DWORD
GetCommandLineA PROTO STDCALL

; Declare the C++ entry point (extern "C" int main(int argc, char** argv))
extrn  main:proc

.data
    szCmdLine db 256 dup (0)

.code
start:
    ; Get command line
    call GetCommandLineA
    mov  esi, eax
    ; Copy to buffer
    mov  edi, offset szCmdLine
    mov  ecx, 255
copy_loop:
    mov  al, [esi]
    mov  [edi], al
    inc  esi
    inc  edi
    dec  ecx
    jnz  copy_loop
    mov  byte ptr [edi], 0

    ; Call C++ main (argc = 1, argv[0] = program name)
    push 0                ; argv (NULL for now)
    push 1                ; argc
    call  main

    ; Exit process with return code from main
    push  eax
    call  ExitProcess
end start