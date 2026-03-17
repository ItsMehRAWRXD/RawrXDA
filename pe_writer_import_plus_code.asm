OPTION CASEMAP:NONE
EXTERN PEWriter_CreateExecutable:PROC
EXTERN PEWriter_AddImport:PROC
EXTERN PEWriter_AddCode:PROC
EXTERN ExitProcess:PROC

.data
k32 db "kernel32.dll",0
u32 db "user32.dll",0
ntd db "ntdll.dll",0
fn_exit db "ExitProcess",0
fn_std db "GetStdHandle",0
fn_write db "WriteFile",0
fn_msg db "MessageBoxA",0
fn_rtl db "RtlInitUnicodeString",0
code_bytes db 31h,0C9h,0E8h,0,0,0,0,0C3h
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
    lea rdx, k32
    lea r8, fn_std
    call PEWriter_AddImport
    cmp rax, 1
    jne fail4
    mov rcx, rbx
    lea rdx, k32
    lea r8, fn_write
    call PEWriter_AddImport
    cmp rax, 1
    jne fail5
    mov rcx, rbx
    lea rdx, u32
    lea r8, fn_msg
    call PEWriter_AddImport
    cmp rax, 1
    jne fail6
    mov rcx, rbx
    lea rdx, ntd
    lea r8, fn_rtl
    call PEWriter_AddImport
    cmp rax, 1
    jne fail7

    mov rcx, rbx
    lea rdx, code_bytes
    mov r8d, code_size
    call PEWriter_AddCode
    test rax, rax
    jz fail8

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
fail6:
    mov ecx, 6
    call ExitProcess
fail7:
    mov ecx, 7
    call ExitProcess
fail8:
    mov ecx, 8
    call ExitProcess
main ENDP
END
