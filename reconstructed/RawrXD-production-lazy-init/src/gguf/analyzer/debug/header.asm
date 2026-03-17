; Minimal GGUF Header Validator for Debugging
.code

EXTERN GetCommandLineW: PROC
EXTERN CommandLineToArgvW: PROC
EXTERN CreateFileW: PROC
EXTERN CreateFileMappingW: PROC
EXTERN MapViewOfFile: PROC
EXTERN CloseHandle: PROC
EXTERN GetStdHandle: PROC
EXTERN WriteFile: PROC
EXTERN ExitProcess: PROC

GGUF_MAGIC EQU 46554747h
STD_OUTPUT_HANDLE EQU 0FFFFFFF5h

.data
g_hFile dq 0
g_hMapping dq 0
g_pFileView dq 0
msgOK db "Header valid!", 13, 10, 0
msgBad db "Header invalid!", 13, 10, 0

.code
main PROC
    sub rsp, 48h
    
    call GetCommandLineW
    mov rcx, rax
    lea rdx, [rsp+38h]
    call CommandLineToArgvW
    mov rbx, rax
    
    mov ecx, dword ptr [rsp+38h]
    cmp ecx, 2
    jl exit_bad
    
    mov rsi, [rbx+8]
    
    ; Open file
    mov rcx, rsi
    mov edx, 80000000h
    mov r8d, 1h
    xor r9d, r9d
    mov dword ptr [rsp+20h], 3
    mov dword ptr [rsp+28h], 80h
    mov qword ptr [rsp+30h], 0
    call CreateFileW
    cmp rax, -1
    je exit_bad
    mov g_hFile, rax
    
    ; Create mapping
    mov rcx, rax
    xor edx, edx
    mov r8d, 2h
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    call CreateFileMappingW
    test rax, rax
    jz exit_bad
    mov g_hMapping, rax
    
    ; Map view
    mov rcx, rax
    mov edx, 4h
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    call MapViewOfFile
    test rax, rax
    jz exit_bad
    mov g_pFileView, rax
    
    ; Check magic
    mov edx, dword ptr [rax]
    cmp edx, GGUF_MAGIC
    jne exit_bad
    
    ; Check version
    mov edx, dword ptr [rax+4]
    cmp edx, 3
    jne exit_bad
    
    ; Read counts
    mov r8, qword ptr [rax+8]    ; tensor_count
    mov r9, qword ptr [rax+16]   ; kv_count
    
    ; Print OK
    lea rcx, msgOK
    call Print
    
    call Cleanup
    xor ecx, ecx
    call ExitProcess
    
exit_bad:
    lea rcx, msgBad
    call Print
    call Cleanup
    mov ecx, 1
    call ExitProcess
main ENDP

Print PROC
    sub rsp, 38h
    mov rbx, rcx
    xor esi, esi
len_loop:
    mov al, byte ptr [rcx+rsi]
    test al, al
    jz len_done
    inc rsi
    jmp len_loop
len_done:
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    mov rdx, rbx
    mov r8d, esi
    lea r9, [rsp+28h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    add rsp, 38h
    ret
Print ENDP

Cleanup PROC
    sub rsp, 28h
    cmp qword ptr g_pFileView, 0
    je c1
    mov rcx, g_pFileView
    call CloseHandle
c1:
    cmp qword ptr g_hMapping, 0
    je c2
    mov rcx, g_hMapping
    call CloseHandle
c2:
    cmp qword ptr g_hFile, 0
    je cd
    mov rcx, g_hFile
    call CloseHandle
cd:
    add rsp, 28h
    ret
Cleanup ENDP

END
