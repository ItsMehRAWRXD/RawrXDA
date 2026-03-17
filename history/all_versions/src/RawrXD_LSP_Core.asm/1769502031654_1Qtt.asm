; RawrXD_LSP_Core.asm
; LSP core bridge + handshake sequence

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
    Lock            dq ?
LSP_SERVER_INSTANCE ENDS

.data
align 16
RawrXD_UI_Notification_String db "LSP",0

align 16
LSP_Initialize_Clangd db "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"processId\":0,\"rootUri\":null,\"capabilities\":{},\"clientInfo\":{\"name\":\"RawrXD\",\"version\":\"1.0\"}}}",0
align 16
LSP_Initialize_Pyright db "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"processId\":0,\"rootUri\":null,\"capabilities\":{},\"clientInfo\":{\"name\":\"RawrXD\",\"version\":\"1.0\"}}}",0
align 16
LSP_Initialized db "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}",0

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

align 16
LSP_Handshake_Sequence proc frame
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    mov [rsp+32], rcx
    lea rdx, [LSP_Initialize_Clangd]
    call LSP_Transport_Write
    mov rcx, [rsp+32]
    lea rdx, [LSP_Initialized]
    call LSP_Transport_Write
    add rsp, 40h
    ret
LSP_Handshake_Sequence endp

align 16
LSP_Init_Handshake proc frame
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    call LSP_Handshake_Sequence
    add rsp, 20h
    ret
LSP_Init_Handshake endp

align 16
LSP_Engine_Entry proc frame
    sub rsp, 128
    .allocstack 128
    .endprolog
    lea rcx, [rsp+64]
    call GetStartupInfoW
    mov qword ptr [rsp+48], 0
    mov qword ptr [rsp+40], 0
    mov dword ptr [rsp+32], 0
    lea r9, [rsp+64]
    xor r8, r8
    xor rdx, rdx
    call CreateProcessW
    test rax, rax
    jz _err
    mov rcx, [rsp+48]
    call LSP_Init_Handshake
_err:
    add rsp, 128
    ret
LSP_Engine_Entry endp

END
