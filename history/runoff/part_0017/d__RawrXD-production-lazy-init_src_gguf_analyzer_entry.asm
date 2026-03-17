; ════════════════════════════════════════════════════════════════════════════════
; Entry Point - Argument Parsing, Banner, Exit
; ════════════════════════════════════════════════════════════════════════════════

; External functions from kernel32
extern GetCommandLineW:PROC
extern CommandLineToArgvW:PROC
extern ExitProcess:PROC

.code

; Enforce Win64 ABI compliance by saving and restoring nonvolatile registers
SaveNonvolatile PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    ret
SaveNonvolatile ENDP

RestoreNonvolatile PROC
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RestoreNonvolatile ENDP

main PROC
    ; Shadow space + local variables + alignment
    sub rsp, 40h

    ; Print banner
    ; ... (banner printing logic here) ...

    ; Parse arguments
    ; ... (argument parsing logic here) ...

    ; Exit cleanly
    xor rcx, rcx  ; Exit code 0
    call ExitProcess

    add rsp, 40h
    ret
main ENDP

END main