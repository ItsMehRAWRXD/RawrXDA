OPTION CASEMAP:NONE
EXTERN PEWriter_CreateExecutable:PROC
EXTERN FindOrCreateDLLEntry:PROC
EXTERN AddFunctionEntry:PROC
EXTERN BuildStringTable:PROC
EXTERN BuildImportByNameStructures:PROC
EXTERN CalculateImportRVAs:PROC
EXTERN BuildImportThunks:PROC
EXTERN BuildImportDescriptors:PROC
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

    ; Add first function manually
    mov rdi, rbx
    lea rsi, k32
    call FindOrCreateDLLEntry
    cmp rax, -1
    je fail3
    mov rcx, rbx
    mov rdx, rax
    lea r8, fn_exit
    call AddFunctionEntry
    test rax, rax
    jz fail4

    ; Add second function manually
    mov rdi, rbx
    lea rsi, k32
    call FindOrCreateDLLEntry
    cmp rax, -1
    je fail5
    mov rcx, rbx
    mov rdx, rax
    lea r8, fn_std
    call AddFunctionEntry
    test rax, rax
    jz fail6

    ; Probe BuildImportTables stages
    mov rdi, rbx
    call BuildStringTable
    test rax, rax
    jz fail7

    mov rdi, rbx
    call BuildImportByNameStructures
    test rax, rax
    jz fail8

    mov rdi, rbx
    call CalculateImportRVAs
    test rax, rax
    jz fail9

    mov rdi, rbx
    call BuildImportThunks
    test rax, rax
    jz fail10

    mov rdi, rbx
    call BuildImportDescriptors
    test rax, rax
    jz fail11

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
fail9:
    mov ecx, 9
    call ExitProcess
fail10:
    mov ecx, 10
    call ExitProcess
fail11:
    mov ecx, 11
    call ExitProcess
main ENDP
END
