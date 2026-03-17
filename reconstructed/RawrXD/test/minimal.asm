; Minimal test: just print a message and exit
; Tests if our toolchain + basic Win API calls work

extern GetStdHandle: proc
extern WriteConsoleA: proc
extern ExitProcess: proc

STD_OUTPUT_HANDLE equ -11

.data
    szMsg db "Hello from MASM64!", 13, 10, 0
    dwWritten dd 0

.code

_start_entry PROC
    ; RSP is 16-aligned from loader
    sub rsp, 28h          ; shadow space (32) + 8 alignment = 40 = 0x28

    ; GetStdHandle(-11)
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    ; rax = stdout handle

    ; WriteConsoleA(handle, buf, len, &written, 0)
    mov rcx, rax          ; handle
    lea rdx, szMsg        ; buffer
    mov r8d, 20           ; length
    lea r9, dwWritten     ; bytes written
    mov qword ptr [rsp+20h], 0  ; reserved
    call WriteConsoleA

    ; ExitProcess(0)
    xor ecx, ecx
    call ExitProcess
_start_entry ENDP

END
