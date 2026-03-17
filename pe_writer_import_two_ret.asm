OPTION CASEMAP:NONE
EXTERN PEWriter_CreateExecutable:PROC
EXTERN PEWriter_AddImport:PROC
EXTERN ExitProcess:PROC

.data
k32 db "kernel32.dll",0
fn_exit db "ExitProcess",0
fn_std db "GetStdHandle",0

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
    je ok

    mov ecx, eax
    and ecx, 0FFh
    call ExitProcess

ok:
    xor ecx, ecx
    call ExitProcess
fail2:
    mov ecx, 2
    call ExitProcess
fail3:
    mov ecx, 3
    call ExitProcess
main ENDP
END
