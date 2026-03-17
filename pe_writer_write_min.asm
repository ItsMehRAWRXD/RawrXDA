OPTION CASEMAP:NONE
EXTERN PEWriter_CreateExecutable:PROC
EXTERN PEWriter_AddImport:PROC
EXTERN PEWriter_AddCode:PROC
EXTERN PEWriter_WriteFile:PROC
EXTERN ExitProcess:PROC

.data
out_name db "D:\\RawrXD\\test_output_min.exe",0
k32 db "kernel32.dll",0
fn_exit db "ExitProcess",0
code_bytes db 31h,0C9h,0C3h
code_size equ $-code_bytes

.code
main PROC
    sub rsp, 28h
    mov rcx, 140000000h
    mov rdx, 1000h
    call PEWriter_CreateExecutable
    test rax, rax
    jz fail2
    mov rbx, rax

    mov rcx, rbx
    lea rdx, k32
    lea r8, fn_exit
    call PEWriter_AddImport
    cmp rax, 1
    jne fail3

    mov rcx, rbx
    lea rdx, code_bytes
    mov r8d, code_size
    call PEWriter_AddCode
    test rax, rax
    jz fail4

    mov rcx, rbx
    lea rdx, out_name
    call PEWriter_WriteFile
    test rax, rax
    jz fail5

    xor ecx, ecx
    call ExitProcess
fail2:
    mov ecx, 2
    call ExitProcess
fail3:
    mov ecx, 3
    call ExitProcess
fail4:
    mov ecx, 4
    call ExitProcess
fail5:
    mov ecx, 5
    call ExitProcess
main ENDP
END
