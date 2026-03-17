; RawrXD_LSP_Core.asm
; LSP core bridge + handshake sequence (extended)

option casemap:none

EXTERN RawrXD_Marketplace_ResolveSymbol:PROC
EXTERN RawrXD_UI_Push_Notify:PROC
EXTERN RawrXD_JSON_Stringify:PROC
EXTERN RawrXD_Calc_ContentLength:PROC
EXTERN WriteFile:PROC
EXTERN GetStartupInfoW:PROC
EXTERN CreateProcessW:PROC

LSP_SERVER_INSTANCE STRUCT
    hProcess        dq ?
    hStdInWrite     dq ?
    hStdOutRead     dq ?
    dwProcessId     dd ?
    _pad            dd ?
    pMessageBuffer  dq ?
    LockValue       dq ?
LSP_SERVER_INSTANCE ENDS

.data
align 16
RawrXD_UI_Notification_String db "LSP",0

align 16
LSP_Initialize_Clangd db "{""jsonrpc"":""2.0"",""id"":1,""method"":""initialize"",""params"":{""processId"":0,""rootUri"":null,""capabilities"":{},""clientInfo"":{""name"":""RawrXD"",""version"":""1.0""}}}",0
align 16
LSP_Initialize_Pyright db "{""jsonrpc"":""2.0"",""id"":1,""method"":""initialize"",""params"":{""processId"":0,""rootUri"":null,""capabilities"":{},""clientInfo"":{""name"":""RawrXD"",""version"":""1.0""}}}",0
align 16
LSP_Initialized db "{""jsonrpc"":""2.0"",""method"":""initialized"",""params"":{}}",0

.code
align 16
LSP_Internal_Dispatch proc frame
    sub rsp, 48
    .allocstack 48
    .endprolog
    mov [rsp+32], rcx
    mov [rsp+40], rdx
    lea r8, [rdx+16]
    xor r9, r9
    call RawrXD_Marketplace_ResolveSymbol
    test rax, rax
    jz @f
    mov rcx, [rsp+32]
    mov rdx, [rsp+40]
    call rax
@@: add rsp, 48
    ret
LSP_Internal_Dispatch endp

align 16
LSP_Shim_ShowMessage proc
    mov r8, rdx
    mov rdx, rcx
    lea rcx, [RawrXD_UI_Notification_String]
    jmp RawrXD_UI_Push_Notify
LSP_Shim_ShowMessage endp

align 16
LSP_Transport_Write proc frame
    sub rsp, 72
    .allocstack 72
    .endprolog
    mov [rsp+64], rcx
    mov [rsp+56], rdx
    mov rcx, [rsp+56]
    call RawrXD_JSON_Stringify
    mov rdx, rax
    lea rcx, [rsp+32]
    call RawrXD_Calc_ContentLength
    mov r8, rax
    mov r9, rdx
    mov rcx, [rsp+64]
    mov rcx, [rcx+LSP_SERVER_INSTANCE.hStdInWrite]
    sub rsp, 32
    call WriteFile
    add rsp, 104
    ret
LSP_Transport_Write endp

EXTERN LSP_Handshake_Sequence:PROC

align 16
LSP_Init_Handshake proc frame
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    ; Forward to the extended implementation
    call LSP_Handshake_Sequence
    add rsp, 20h
    ret
LSP_Init_Handshake endp

; ═══════════════════════════════════════════════════════════════════════════════
; LSP_Engine_Entry
; RCX = Command Line (Wide String)
; ═══════════════════════════════════════════════════════════════════════════════
align 16
LSP_Engine_Entry proc frame
    push rbx
    push rdi
    sub rsp, 128
    .allocstack 128
    .endprolog
    
    mov rbx, rcx        ; Save Command Line
    
    ; Initialize StartupInfo
    lea rcx, [rsp+64]
    call GetStartupInfoW
    
    ; Prepare CreateProcessW args on stack (Shadow Space + Args 5-10)
    ; Stack Layout:
    ; [rsp + 32] = Shadow Store (R9)
    ; [rsp + 24] = Shadow Store (R8)
    ; [rsp + 16] = Shadow Store (RDX)
    ; [rsp + 8]  = Shadow Store (RCX)
    ; [rsp + 0]  = Return Address (for the call)
    
    ; Actually, when we call, we push args onto stack above the shadow space.
    ; Microsoft x64: First 4 args in regs. Remaining on stack.
    ; Arg 5 is at [RSP + 32] relative to the CALLEE (so just pushed or moved to [rsp+32] of CALLER?)
    ; CALLER allocates 32 bytes shadow space.
    ; Arg 5 is at [RSP + 32]
    ; Arg 6 at [RSP + 40]
    ; Arg 7 at [RSP + 48]
    ; Arg 8 at [RSP + 56]
    ; Arg 9 at [RSP + 64]
    ; Arg 10 at [RSP + 72]
    
    ; We have rsp (frame)
    ; We need to setup [rsp + 32]...[rsp + 72]
    ; StartupInfo is at [rsp+64] in our frame logic, but CreateProcessW expects pointer.
    
    ; Let's use clean stack locations.
    ; rsp+80 = ProcessInfo (16 bytes)
    
    ; Arg 10: lpProcessInformation
    lea rax, [rsp+80]
    mov [rsp + 32 + 40], rax
    
    ; Arg 9: lpStartupInfo
    lea rax, [rsp+64]
    mov [rsp + 32 + 32], rax
    
    ; Arg 8: lpCurrentDirectory
    mov qword ptr [rsp + 32 + 24], 0
    
    ; Arg 7: lpEnvironment
    mov qword ptr [rsp + 32 + 16], 0
    
    ; Arg 6: dwCreationFlags
    mov dword ptr [rsp + 32 + 8], 0
    
    ; Arg 5: bInheritHandles
    mov qword ptr [rsp + 32], 1
    
    ; Register Args
    xor r9, r9          ; lpThreadAttributes
    xor r8, r8          ; lpSecurityAttributes
    mov rdx, rbx        ; lpCommandLine (From RCX)
    xor rcx, rcx        ; lpApplicationName (NULL - use cmdline)
    
    call CreateProcessW
    test rax, rax
    jz _err
    
    ; Process Created. PI at [rsp+80] (hProcess is first QWORD)
    mov rcx, [rsp+80]
    call LSP_Init_Handshake
    
_err:
    add rsp, 128
    pop rdi
    pop rbx
    ret
LSP_Engine_Entry endp

END
