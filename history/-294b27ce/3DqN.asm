; GenesisP0_SelfHosting.asm
; Self-hosting helpers for genesis build flow.

OPTION CASEMAP:NONE

includelib kernel32.lib

EXTERN WinExec:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN GetFileAttributesA:PROC
EXTERN UTC_LogEvent:PROC

INVALID_FILE_ATTRIBUTES EQU 0FFFFFFFFh

.data
align 8
cmd_buffer             db 2048 dup(0)
sz_ml64_prefix         db "ml64.exe /c /nologo /Fo", 0
sz_link_prefix         db "link.exe /nologo /SUBSYSTEM:CONSOLE /NODEFAULTLIB /ENTRY:EntryPoint /OUT:", 0
sz_link_mid            db " ", 0
sz_link_suffix         db " kernel32.lib user32.lib", 0
sz_space               db " ", 0
sz_evt_self_compile    db "[GenesisP0] SelfHosting CompileASM", 0

.code
PUBLIC Genesis_SelfHosting_CompileASM
PUBLIC Genesis_SelfHosting_LinkEXE
PUBLIC Genesis_SelfHosting_Verify
PUBLIC Genesis_SelfHosting_Build

; int Genesis_SelfHosting_CompileASM(const char* sourcePath, const char* objPath)
Genesis_SelfHosting_CompileASM PROC
    sub rsp, 48h
    mov qword ptr [rsp+20h], rcx
    mov qword ptr [rsp+28h], rdx

    lea rcx, sz_evt_self_compile
    call UTC_LogEvent

    test rcx, rcx
    jz compile_fail
    test rdx, rdx
    jz compile_fail

    lea rcx, cmd_buffer
    lea rdx, sz_ml64_prefix
    call lstrcpyA

    lea rcx, cmd_buffer
    mov rdx, qword ptr [rsp+28h]
    call lstrcatA

    lea rcx, cmd_buffer
    lea rdx, sz_space
    call lstrcatA

    lea rcx, cmd_buffer
    mov rdx, qword ptr [rsp+20h]
    call lstrcatA

    lea rcx, cmd_buffer
    xor edx, edx
    call WinExec
    cmp eax, 32
    jbe compile_fail

    xor eax, eax
    add rsp, 48h
    ret

compile_fail:
    mov eax, 1
    add rsp, 48h
    ret
Genesis_SelfHosting_CompileASM ENDP

; int Genesis_SelfHosting_LinkEXE(const char* objPath, const char* exePath)
Genesis_SelfHosting_LinkEXE PROC
    sub rsp, 48h
    mov qword ptr [rsp+20h], rcx
    mov qword ptr [rsp+28h], rdx

    test rcx, rcx
    jz link_fail
    test rdx, rdx
    jz link_fail

    lea rcx, cmd_buffer
    lea rdx, sz_link_prefix
    call lstrcpyA

    lea rcx, cmd_buffer
    mov rdx, qword ptr [rsp+28h]
    call lstrcatA

    lea rcx, cmd_buffer
    lea rdx, sz_link_mid
    call lstrcatA

    lea rcx, cmd_buffer
    mov rdx, qword ptr [rsp+20h]
    call lstrcatA

    lea rcx, cmd_buffer
    lea rdx, sz_link_suffix
    call lstrcatA

    lea rcx, cmd_buffer
    xor edx, edx
    call WinExec
    cmp eax, 32
    jbe link_fail

    xor eax, eax
    add rsp, 48h
    ret

link_fail:
    mov eax, 1
    add rsp, 48h
    ret
Genesis_SelfHosting_LinkEXE ENDP

; int Genesis_SelfHosting_Verify(const char* exePath)
; returns 0 if file exists, 1 otherwise
Genesis_SelfHosting_Verify PROC
    sub rsp, 28h
    test rcx, rcx
    jz verify_fail

    call GetFileAttributesA
    cmp eax, INVALID_FILE_ATTRIBUTES
    je verify_fail

    xor eax, eax
    add rsp, 28h
    ret

verify_fail:
    mov eax, 1
    add rsp, 28h
    ret
Genesis_SelfHosting_Verify ENDP

; int Genesis_SelfHosting_Build(const char* target)
; minimal ABI bridge used by integration checks
Genesis_SelfHosting_Build PROC
    jmp Genesis_SelfHosting_Verify
Genesis_SelfHosting_Build ENDP

END
