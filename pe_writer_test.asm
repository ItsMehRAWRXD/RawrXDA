;=============================================================================
; pe_writer_test.asm
; Simple test program demonstrating PE writer functionality
; Builds standalone executable that uses the PE writer functions
;=============================================================================

OPTION CASEMAP:NONE

; External functions from RawrXD_PE_Writer
EXTERN PEWriter_CreateExecutable: PROC
EXTERN PEWriter_AddImport: PROC
EXTERN PEWriter_AddCode: PROC  
EXTERN PEWriter_WriteFile: PROC

; Windows API
EXTERN ExitProcess: PROC
EXTERN GetStdHandle: PROC
EXTERN WriteConsoleA: PROC

.data
    hello_msg db "PE Writer Test - Hello World!", 13, 10, 0
    hello_len equ $ - hello_msg - 1
    
    ; Sample machine code - simple "mov rax, 42; ret" function
    sample_code db 48h, 0B8h, 2Ah, 00h, 00h, 00h, 00h, 00h, 00h, 00h  ; mov rax, 42
                db 0C3h                                                 ; ret
    sample_code_size equ $ - sample_code

.code

main PROC
    sub rsp, 40h        ; Shadow space + alignment

    ; Print hello message
    mov rcx, -11        ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax        ; Console handle
    mov rdx, offset hello_msg
    mov r8, hello_len
    mov r9, 0           ; Reserved
    push 0              ; Reserved
    call WriteConsoleA
    add rsp, 8          ; Clean stack
    
    ; Test PE writer functionality
    mov rcx, 0140000000h    ; Default image base
    mov rdx, 1000h          ; Entry point RVA  
    call PEWriter_CreateExecutable
    test rax, rax
    jz test_failed
    
    mov rbx, rax        ; Save PE context
    
    ; Add a test import (simplified)
    mov rcx, rbx        ; PE context
    mov rdx, offset sample_code    ; DLL name placeholder
    mov r8, offset sample_code     ; Function name placeholder
    call PEWriter_AddImport
    
    ; Add sample code
    mov rcx, rbx        ; PE context
    mov rdx, offset sample_code    ; Code buffer
    mov r8, sample_code_size       ; Code size
    call PEWriter_AddCode
    
    ; Try to write file (simplified)
    ; In real usage you'd provide a filename
    mov rcx, rbx        ; PE context
    mov rdx, 0          ; Filename placeholder
    call PEWriter_WriteFile
    
test_success:
    mov rcx, 0          ; Exit code 0 (success)
    jmp exit_program
    
test_failed:
    mov rcx, 1          ; Exit code 1 (failure)
    
exit_program:
    call ExitProcess
    
main ENDP

END