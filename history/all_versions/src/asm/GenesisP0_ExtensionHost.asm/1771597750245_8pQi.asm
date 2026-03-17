// ==============================================================================
// GenesisP0_ExtensionHost.asm — MASM64 Extension Host (Node.js replacement)
// Exports: Genesis_ExtensionHost_Create, Genesis_ExtensionHost_LoadExtension, Genesis_ExtensionHost_InvokeCommand
// ==============================================================================
OPTION DOTNAME
EXTERN LoadLibraryA:PROC, GetProcAddress:PROC, FreeLibrary:PROC
EXTERN VirtualAlloc:PROC, VirtualFree:PROC, RtlZeroMemory:PROC
EXTERN CreateMutexA:PROC, ReleaseMutex:PROC, CloseHandle:PROC
EXTERN Sleep:PROC, ExitProcess:PROC

.data
    align 8
    g_extCount          DD 0
    g_extHandles        DQ 32 DUP(0)      ; Max 32 extensions
    g_extNames          DB 32*256 DUP(0)   ; Name table
    g_hostMutex         DQ 0
    
    szExtApi            DB "RawrXD_ExtensionEntry", 0
    szHostMutex         DB "RawrXD_ExtHost_Mutex", 0

.code
; ------------------------------------------------------------------------------
; Genesis_ExtensionHost_Create — Initialize extension host context
; ------------------------------------------------------------------------------
Genesis_ExtensionHost_Create PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    ; Create singleton mutex
    xor rcx, rcx
    lea rdx, szHostMutex
    xor r8, r8
    xor r9, r9
    call CreateMutexA
    mov g_hostMutex, rax
    
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret
Genesis_ExtensionHost_Create ENDP

; ------------------------------------------------------------------------------
; Genesis_ExtensionHost_LoadExtension — Load DLL extension
; RCX = pathToDll (ASCII), RDX = outHandle
; Returns: RAX = 0 success, 1 = max extensions reached, 2 = load failed
; ------------------------------------------------------------------------------
Genesis_ExtensionHost_LoadExtension PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 56
    
    mov rsi, rdx                    ; Save outHandle ptr
    
    ; Check extension limit
    mov eax, g_extCount
    cmp eax, 32
    jae _ext_load_max
    
    ; Load the DLL
    call LoadLibraryA
    test rax, rax
    jz _ext_load_fail
    
    ; Store handle
    mov rcx, g_extCount
    mov [g_extHandles + rcx*8], rax
    
    ; Get entry point
    mov rcx, rax
    lea rdx, szExtApi
    call GetProcAddress
    
    inc g_extCount
    mov [rsi], rax                  ; Return entry point in outHandle
    xor rax, rax
    jmp _ext_load_exit
    
_ext_load_max:
    mov rax, 1
    jmp _ext_load_exit
    
_ext_load_fail:
    mov rax, 2
    
_ext_load_exit:
    mov rsp, rbp
    pop rbp
    ret
Genesis_ExtensionHost_LoadExtension ENDP

; ------------------------------------------------------------------------------
; Genesis_ExtensionHost_InvokeCommand — Call extension command by index
; RCX = extIndex, RDX = commandId, R8 = paramPtr
; ------------------------------------------------------------------------------
Genesis_ExtensionHost_InvokeCommand PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    cmp ecx, 32
    jae _ext_cmd_invalid
    
    mov rax, [g_extHandles + rcx*8]
    test rax, rax
    jz _ext_cmd_invalid
    
    ; Call extension entry (cdecl to stdcall bridge)
    mov rcx, rdx                    ; commandId
    mov rdx, r8                     ; paramPtr
    call rax
    
_ext_cmd_exit:
    mov rsp, rbp
    pop rbp
    ret
    
_ext_cmd_invalid:
    mov rax, -1
    jmp _ext_cmd_exit
Genesis_ExtensionHost_InvokeCommand ENDP

END
