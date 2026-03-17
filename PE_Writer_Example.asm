; PE_Writer_Example.asm
; Demonstrates usage of the RawrXD PE Writer
; Creates a simple "Hello World" executable

OPTION CASEMAP:NONE

EXTERN PEWriter_CreateExecutable: PROC
EXTERN PEWriter_AddImport: PROC
EXTERN PEWriter_AddCode: PROC
EXTERN PEWriter_WriteFile: PROC
EXTERN GetStdHandle: PROC
EXTERN WriteConsoleA: PROC
EXTERN ExitProcess: PROC

.data
output_filename db "hello.exe", 0
dll_kernel32 db "kernel32.dll", 0
func_GetStdHandle db "GetStdHandle", 0
func_WriteConsoleA db "WriteConsoleA", 0
func_ExitProcess db "ExitProcess", 0

; Sample machine code for a Hello World program
hello_code db 048h, 083h, 0ECh, 028h                    ; sub rsp, 28h
          db 048h, 0C7h, 0C1h, 0F5h, 0FFh, 0FFh, 0FFh   ; mov rcx, -11 (STD_OUTPUT_HANDLE)
          db 0FFh, 015h, 000h, 000h, 000h, 000h          ; call GetStdHandle (placeholder address)
          db 048h, 089h, 0C1h                            ; mov rcx, rax
          db 048h, 0C7h, 0C2h, 000h, 000h, 000h, 000h    ; mov rdx, hello_msg (placeholder address)
          db 041h, 0B8h, 00Dh, 000h, 000h, 000h          ; mov r8d, 13 (message length)
          db 049h, 0C7h, 0C1h, 000h, 000h, 000h, 000h    ; mov r9, 0
          db 048h, 0C7h, 044h, 024h, 020h, 000h, 000h, 000h, 000h ; mov qword ptr [rsp+20h], 0
          db 0FFh, 015h, 000h, 000h, 000h, 000h          ; call WriteConsoleA (placeholder address)
          db 048h, 0C7h, 0C1h, 000h, 000h, 000h, 000h    ; mov rcx, 0
          db 0FFh, 015h, 000h, 000h, 000h, 000h          ; call ExitProcess (placeholder address)
          db 090h                                         ; nop

hello_code_size equ $ - hello_code

.code
main PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Create PE executable
    mov rcx, 0          ; Use default image base
    mov rdx, 1000h      ; Entry point RVA
    call PEWriter_CreateExecutable
    test rax, rax
    jz error_exit
    mov rbx, rax        ; Save PE context
    
    ; Add imports
    mov rcx, rbx
    mov rdx, offset dll_kernel32
    mov r8, offset func_GetStdHandle
    call PEWriter_AddImport
    test rax, rax
    jz error_exit
    
    mov rcx, rbx
    mov rdx, offset dll_kernel32
    mov r8, offset func_WriteConsoleA
    call PEWriter_AddImport
    test rax, rax
    jz error_exit
    
    mov rcx, rbx
    mov rdx, offset dll_kernel32
    mov r8, offset func_ExitProcess
    call PEWriter_AddImport
    test rax, rax
    jz error_exit
    
    ; Add machine code
    mov rcx, rbx
    mov rdx, offset hello_code
    mov r8, hello_code_size
    call PEWriter_AddCode
    test rax, rax
    jz error_exit
    
    ; Write file
    mov rcx, rbx
    mov rdx, offset output_filename
    call PEWriter_WriteFile
    test rax, rax
    jz error_exit
    
    ; Success
    mov rcx, 0
    call ExitProcess
    
error_exit:
    mov rcx, 1
    call ExitProcess

main ENDP

END