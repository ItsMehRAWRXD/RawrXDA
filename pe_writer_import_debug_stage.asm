OPTION CASEMAP:NONE

EXTERN PEWriter_CreateExecutable:PROC
EXTERN PEWriter_AddImport:PROC
EXTERN FindOrCreateDLLEntry:PROC
EXTERN AddFunctionEntry:PROC
EXTERN BuildImportTables:PROC
EXTERN BuildStringTable:PROC
EXTERN BuildImportByNameStructures:PROC
EXTERN CalculateImportRVAs:PROC
EXTERN BuildImportThunks:PROC
EXTERN BuildImportDescriptors:PROC
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
    test rax, rax
    jz fail3

    mov rcx, rbx
    lea rdx, k32
    lea r8, fn_std
    call PEWriter_AddImport
    test rax, rax
    jz fail4

    mov rcx, rbx
    lea rdx, k32
    lea r8, fn_write
    call PEWriter_AddImport
    test rax, rax
    jz fail5

    mov rcx, rbx
    lea rdx, u32
    lea r8, fn_msg
    call PEWriter_AddImport
    test rax, rax
    jz fail6

    ; Debug the 5th import stages directly
    mov rdi, rbx
    lea rsi, ntd
    call FindOrCreateDLLEntry
    cmp rax, -1
    je fail10
    mov r12, rax

    mov rcx, rbx
    mov rdx, r12
    lea r8, fn_rtl
    call AddFunctionEntry
    test rax, rax
    jz fail11

    mov rcx, rbx
    call BuildImportTables
    test rax, rax
    jz fail12

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
fail10:
    mov ecx, 10
    call ExitProcess
fail11:
    mov ecx, 11
    call ExitProcess
fail12:
    mov rcx, rbx
    call BuildStringTable
    test rax, rax
    jz fail13

    mov rcx, rbx
    call BuildImportByNameStructures
    test rax, rax
    jz fail14

    mov rcx, rbx
    call CalculateImportRVAs
    test rax, rax
    jz fail15

    mov rcx, rbx
    call BuildImportThunks
    test rax, rax
    jz fail16

    mov rcx, rbx
    call BuildImportDescriptors
    test rax, rax
    jz fail17

    mov ecx, 12
    call ExitProcess
fail13:
    mov ecx, 13
    call ExitProcess
fail14:
    mov ecx, 14
    call ExitProcess
fail15:
    mov ecx, 15
    call ExitProcess
fail16:
    mov ecx, 16
    call ExitProcess
fail17:
    mov ecx, 17
    call ExitProcess
main ENDP

END
