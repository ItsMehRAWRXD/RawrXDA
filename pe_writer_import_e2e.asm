OPTION CASEMAP:NONE

EXTERN PEWriter_CreateExecutable:PROC
EXTERN PEWriter_AddImport:PROC
EXTERN PEWriter_AddCode:PROC
EXTERN PEWriter_WriteFile:PROC
EXTERN ExitProcess:PROC

.data
out_name db "D:\\RawrXD\\test_output_imports.exe",0
k32 db "kernel32.dll",0
u32 db "user32.dll",0
ntd db "ntdll.dll",0
fn_exit db "ExitProcess",0
fn_std db "GetStdHandle",0
fn_write db "WriteFile",0
fn_msg db "MessageBoxA",0
fn_rtl db "RtlInitUnicodeString",0
code_bytes db 31h,0C9h,0E8h,0,0,0,0,0C3h ; xor ecx,ecx; call rel32(placeholder); ret
code_size equ $-code_bytes

.code
main PROC
    sub rsp, 28h

    mov rcx, 140000000h
    mov rdx, 1000h
    call PEWriter_CreateExecutable
    test rax, rax
    jz fail
    mov rbx, rax

    mov rcx, rbx
    lea rdx, k32
    lea r8, fn_exit
    call PEWriter_AddImport
    test rax, rax
    jz fail

    mov rcx, rbx
    lea rdx, k32
    lea r8, fn_std
    call PEWriter_AddImport
    test rax, rax
    jz fail

    mov rcx, rbx
    lea rdx, k32
    lea r8, fn_write
    call PEWriter_AddImport
    test rax, rax
    jz fail

    mov rcx, rbx
    lea rdx, u32
    lea r8, fn_msg
    call PEWriter_AddImport
    test rax, rax
    jz fail

    mov rcx, rbx
    lea rdx, ntd
    lea r8, fn_rtl
    call PEWriter_AddImport
    test rax, rax
    jz fail

    mov rcx, rbx
    lea rdx, code_bytes
    mov r8d, code_size
    call PEWriter_AddCode
    test rax, rax
    jz fail

    mov rcx, rbx
    lea rdx, out_name
    call PEWriter_WriteFile
    test rax, rax
    jz fail

    xor ecx, ecx
    call ExitProcess

fail:
    mov ecx, 1
    call ExitProcess
main ENDP

END
