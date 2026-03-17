; console_log_simple.asm - Simple console logging for MASM64
; Minimal implementation to support orchestration stubs

option casemap:none

.code

; External functions we need
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN lstrlenA:PROC
EXTERN GetCurrentProcess:PROC

; Simple console output function
.data?
hStdOut QWORD 0

.code

; console_log(msg: LPCSTR)
; Simple console logging function
PUBLIC console_log
console_log PROC
    ; rcx = message pointer
    
    ; Get stdout handle if not already cached
    cmp [hStdOut], 0
    jne write_message
    
    mov rcx, -11  ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [hStdOut], rax
    
write_message:
    ; Write message to console
    mov rcx, [hStdOut]
    mov rdx, rcx    ; message pointer in rdx
    call lstrlenA   ; get message length
    mov r8, rax     ; length in r8
    xor r9, r9      ; no overlap
    mov QWORD PTR [rsp + 32], 0  ; no event
    
    ; Write to console
    call WriteFile
    
    ; Add newline
    mov rcx, [hStdOut]
    lea rdx, newline
    mov r8b, 2      ; length of newline (CRLF)
    xor r9, r9
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    
    ret
console_log ENDP

.data
newline BYTE 13, 10, 0

.code

END
