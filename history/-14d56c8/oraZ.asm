; boot.asm – Minimal Win32 entry point
.386
.model flat, stdcall

; Function prototypes
ExitProcess PROTO STDCALL :DWORD
GetCommandLineA PROTO STDCALL

; C++ main function
extrn _main:proc

.code
_start:
    ; Get command line
    call GetCommandLineA
    
    ; Call C++ main with basic arguments
    push 0        ; argv
    push 1        ; argc  
    call _main
    
    ; Exit with return code
    push eax
    call ExitProcess
end _start