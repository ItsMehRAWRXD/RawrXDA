OPTION CASEMAP:NONE
EXTERN PEWriter_CreateExecutable:PROC
EXTERN FindOrCreateDLLEntry:PROC
EXTERN AddFunctionEntry:PROC
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

    ; Validate key counters and first DLL entry fields.
    mov rax, [rbx+8Ch]       ; pImportDLLTable
    mov ecx, [rax+8]         ; descriptorIndex
    cmp ecx, 0
    jne bad_desc
    mov ecx, [rax+0Ch]       ; functionCount
    cmp ecx, 2
    jne bad_func_count
    mov ecx, [rbx+0ACh]      ; numImportDLLs
    cmp ecx, 1
    jne bad_dll_count
    mov ecx, [rbx+0B0h]      ; numImportFunctions
    cmp ecx, 2
    jne bad_total_func

    xor ecx, ecx
    call ExitProcess

bad_desc:
    mov ecx, 21
    call ExitProcess
bad_func_count:
    mov ecx, 22
    call ExitProcess
bad_dll_count:
    mov ecx, 23
    call ExitProcess
bad_total_func:
    mov ecx, 24
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
main ENDP
END
