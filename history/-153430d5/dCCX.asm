; Hello World in MASM (Microsoft Macro Assembler)
; 64-bit Windows console application

.code

extern ExitProcess: proc
extern printf: proc

.data
    msg db "Hello, World!", 13, 10, 0  ; Message with newline and null terminator

.code
main proc
    ; Reserve shadow space (32 bytes for calling convention)
    sub rsp, 40
    
    ; Print message
    lea rcx, msg        ; First parameter: pointer to format string
    call printf
    
    ; Exit program
    xor ecx, ecx        ; Return code 0
    call ExitProcess
    
    ; Clean up stack (never reached but good practice)
    add rsp, 40
    ret
main endp

END
