; GenesisP0_ExtensionHost.asm
; Native extension host bootstrap for MASM64 runtime path.

OPTION CASEMAP:NONE

includelib kernel32.lib

EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN UTC_LogEvent:PROC

MAX_EXTENSIONS EQU 64

.data
align 8
g_host_hwnd      dq 0
g_ext_count      dd 0
g_ext_table      dq MAX_EXTENSIONS dup(0)
sz_activate      db "activate", 0
sz_entry         db "RawrXD_ExtensionEntry", 0
sz_evt_ext_create db "[GenesisP0] ExtensionHost Create", 0

.code
PUBLIC Genesis_ExtensionHost_Create
PUBLIC Genesis_ExtensionHost_LoadExtension
PUBLIC Genesis_ExtensionHost_InvokeCommand

; int Genesis_ExtensionHost_Create(void* parentHwnd)
Genesis_ExtensionHost_Create PROC
    sub rsp, 28h
    mov qword ptr [rsp+20h], rcx

    lea rcx, sz_evt_ext_create
    call UTC_LogEvent

    mov rcx, qword ptr [rsp+20h]
    mov qword ptr [g_host_hwnd], rcx

    xor eax, eax
clear_loop:
    cmp eax, MAX_EXTENSIONS
    jae clear_done
    mov qword ptr [g_ext_table+rax*8], 0
    inc eax
    jmp clear_loop

clear_done:
    mov dword ptr [g_ext_count], 0
    mov eax, 1
    add rsp, 28h
    ret
Genesis_ExtensionHost_Create ENDP

; int Genesis_ExtensionHost_LoadExtension(const char* dllPath, void** outHandle)
; returns 0 on failure, otherwise 1-based extension id
Genesis_ExtensionHost_LoadExtension PROC
    sub rsp, 48h
    mov qword ptr [rsp+20h], rdx

    test rcx, rcx
    jz load_fail

    mov eax, dword ptr [g_ext_count]
    cmp eax, MAX_EXTENSIONS
    jae load_fail

    call LoadLibraryA
    test rax, rax
    jz load_fail

    mov edx, dword ptr [g_ext_count]
    mov qword ptr [g_ext_table+rdx*8], rax

    mov rcx, qword ptr [rsp+20h]
    test rcx, rcx
    jz load_no_out
    mov qword ptr [rcx], rax

load_no_out:
    inc dword ptr [g_ext_count]
    mov eax, edx
    inc eax
    add rsp, 48h
    ret

load_fail:
    xor eax, eax
    add rsp, 48h
    ret
Genesis_ExtensionHost_LoadExtension ENDP

; int64 Genesis_ExtensionHost_InvokeCommand(uint64 extId, const char* command, void* param)
Genesis_ExtensionHost_InvokeCommand PROC
    sub rsp, 48h
    mov qword ptr [rsp+20h], rdx
    mov qword ptr [rsp+28h], r8

    test rcx, rcx
    jz invoke_fail

    dec ecx
    mov eax, dword ptr [g_ext_count]
    cmp ecx, eax
    jae invoke_fail

    mov rax, qword ptr [g_ext_table+rcx*8]
    test rax, rax
    jz invoke_fail
    mov qword ptr [rsp+30h], rax

    mov rcx, rax
    lea rdx, sz_activate
    call GetProcAddress
    test rax, rax
    jnz invoke_call

    mov rcx, qword ptr [rsp+30h]
    lea rdx, sz_entry
    call GetProcAddress
    test rax, rax
    jz invoke_fail

invoke_call:
    mov r9, rax
    mov rcx, qword ptr [rsp+20h]
    mov rdx, qword ptr [rsp+28h]
    call r9

    add rsp, 48h
    ret

invoke_fail:
    mov rax, -1
    add rsp, 48h
    ret
Genesis_ExtensionHost_InvokeCommand ENDP

END
