;-------------------------------------------------------------------------
; RawrXD_Native_Core.asm - Titan JIT Emitter (Stage 9)
; Base Native Core for Self-Hosting Bootstrap
;-------------------------------------------------------------------------

extern CreateFileA : proc
extern WriteFile  : proc
extern CloseHandle : proc
extern GetStdHandle : proc
extern GetTickCount64 : proc

.data
    ; Log prefix for the native core
    g_LogPrefix db "[RawrXD-Native] ", 0
    g_Newline   db 0Dh, 0Ah, 0
    
.code

;-------------------------------------------------------------------------
; RawrXD_Native_Log(LPCSTR message)
; Writes a timestamped log entry to stdout without .NET/PS overhead
;-------------------------------------------------------------------------
RawrXD_Native_Log proc
    push rbp
    mov rbp, rsp
    sub rsp, 32 ; Shadow space

    mov r12, rcx ; Save message pointer

    ; Get Stdout Handle
    mov ecx, -11 ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r13, rax ; Stdout Handle

    ; --- Write Prefix ---
    mov rcx, r13
    lea rdx, g_LogPrefix
    mov r8, 16 ; Prefix length
    xor r9, r9
    sub rsp, 8 ; Align/Dummy
    push 0
    call WriteFile
    add rsp, 16

    ; --- Write Message ---
    mov rcx, r13
    mov rdx, r12
    
    ; Simple strlen (manual)
    xor r8, r8
@len_loop:
    cmp byte ptr [r12 + r8], 0
    je @len_done
    inc r8
    jmp @len_loop
@len_done:
    xor r9, r9
    sub rsp, 8
    push 0
    call WriteFile
    add rsp, 16

    ; --- Write Newline ---
    mov rcx, r13
    lea rdx, g_Newline
    mov r8, 2
    xor r9, r9
    sub rsp, 8
    push 0
    call WriteFile
    add rsp, 16

    mov rsp, rbp
    pop rbp
    ret
RawrXD_Native_Log endp

;-------------------------------------------------------------------------
; RawrXD_Native_WriteFile(LPCSTR path, LPCSTR content)
; High-performance raw file I/O for self-hosting audit
;-------------------------------------------------------------------------
RawrXD_Native_WriteFile proc
    push rbp
    mov rbp, rsp
    sub rsp, 64

    mov r14, rdx ; Save content

    ; CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
    mov rdx, 40000000h ; GENERIC_WRITE
    xor r8, r8         ; Share mode 0
    xor r9, r9         ; Security NULL
    sub rsp, 32
    mov qword ptr [rsp + 32], 2 ; CREATE_ALWAYS
    mov qword ptr [rsp + 40], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 48], 0
    call CreateFileA
    add rsp, 32
    
    cmp rax, -1 ; INVALID_HANDLE_VALUE
    je @fail

    mov r15, rax ; Handle

    ; Write content
    mov rcx, r15
    mov rdx, r14
    
    ; strlen
    xor r8, r8
@slen:
    cmp byte ptr [r14 + r8], 0
    je @slen_d
    inc r8
    jmp @slen
@slen_d:
    xor r9, r9
    sub rsp, 8
    push 0
    call WriteFile
    add rsp, 16

    ; Close
    mov rcx, r15
    call CloseHandle
    
    mov eax, 1 ; Success
    jmp @exit

@fail:
    xor eax, eax ; Fail

@exit:
    mov rsp, rbp
    pop rbp
    ret
RawrXD_Native_WriteFile endp

end
