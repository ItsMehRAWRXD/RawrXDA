OPTION CASEMAP:NONE
EXTERN PEWriter_CreateExecutable:PROC
EXTERN FindOrCreateDLLEntry:PROC
EXTERN AddFunctionEntry:PROC
EXTERN FindFunctionByDLLAndIndex:PROC
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
    mov rdi, rax

    lea rsi, k32
    call FindOrCreateDLLEntry
    cmp rax, -1
    je fail3
    mov rcx, rdi
    mov rdx, rax
    lea r8, fn_exit
    call AddFunctionEntry
    test rax, rax
    jz fail4

    lea rsi, k32
    call FindOrCreateDLLEntry
    cmp rax, -1
    je fail5
    mov rcx, rdi
    mov rdx, rax
    lea r8, fn_std
    call AddFunctionEntry
    test rax, rax
    jz fail6

    mov r9, [rdi+8Ch] ; first DLL entry pointer
    mov rcx, 0
    call FindFunctionByDLLAndIndex
    test rax, rax
    jz fail7

    mov r9, [rdi+8Ch]
    mov rcx, 1
    call FindFunctionByDLLAndIndex
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
