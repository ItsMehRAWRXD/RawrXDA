;==============================================================================
; RawrXD Language Server Protocol - Pure MASM Implementation
; Phase 14: JSON-RPC, Symbol Resolution, and Code Intelligence
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include kernel32.inc

includelib user32.lib
includelib kernel32.lib

;==============================================================================
; LSP CONSTANTS
;==============================================================================
LSP_METHOD_INITIALIZE   equ 1
LSP_METHOD_DID_OPEN     equ 2
LSP_METHOD_COMPLETION   equ 3
LSP_METHOD_HOVER        equ 4
LSP_METHOD_DEFINITION   equ 5

;==============================================================================
; LSP STRUCTURES
;==============================================================================
LSP_MESSAGE struct
    id              dd ?
    method          dd ?
    pParams         dd ?
    pResult         dd ?
LSP_MESSAGE ends

LSP_COMPLETION_ITEM struct
    label           db 64 dup(?)
    kind            dd ?
    detail          db 128 dup(?)
    documentation   db 256 dup(?)
LSP_COMPLETION_ITEM ends

;==============================================================================
; LSP STATE
;==============================================================================
.data?
lspInitialized          dd ?
lspCapabilities         dd ?
currentMessageId        dd ?

.code

;==============================================================================
; Phase 14.1: LSP Initialization
;==============================================================================
LSP_Initialize proc
    mov lspInitialized, FALSE
    mov currentMessageId, 1
    
    ; Send initialize request
    invoke LSP_SendRequest, LSP_METHOD_INITIALIZE, addr szInitParams
    
    ret
    szInitParams db '{"processId":1234,"rootUri":"file:///workspace","capabilities":{}}',0
LSP_Initialize endp

;==============================================================================
; Phase 14.2: JSON-RPC Communication
;==============================================================================
LSP_SendRequest proc method:DWORD, pParams:DWORD
    local buffer[2048]:BYTE
    local methodStr[64]:DWORD
    
    ; Map method ID to string
    .if method == LSP_METHOD_INITIALIZE
        jmp @F
        szInit db "initialize",0
@@:
        mov eax, offset szInit
    .elseif method == LSP_METHOD_COMPLETION
        jmp @F
        szComp db "textDocument/completion",0
@@:
        mov eax, offset szComp
    .endif
    mov methodStr, eax
    
    ; Format JSON-RPC message
    jmp @F
    szRpcFmt db '{"jsonrpc":"2.0","id":%d,"method":"%s","params":%s}',0
@@:
    invoke wsprintf, addr buffer, addr szRpcFmt, currentMessageId, methodStr, pParams
    inc currentMessageId
    
    ; Send to language server (stdout/pipe)
    invoke LSP_WriteToPipe, addr buffer
    
    ret
LSP_SendRequest endp

;==============================================================================
; Phase 14.3: Symbol Resolution
;==============================================================================
LSP_GetCompletions proc pDocUri:DWORD, line:DWORD, col:DWORD
    local params[512]:BYTE
    
    jmp @F
    szCompParamsFmt db '{"textDocument":{"uri":"%s"},"position":{"line":%d,"character":%d}}',0
@@:
    invoke wsprintf, addr params, addr szCompParamsFmt, pDocUri, line, col
    
    invoke LSP_SendRequest, LSP_METHOD_COMPLETION, addr params
    
    ret
LSP_GetCompletions endp

;==============================================================================
; Phase 14.4: Message Handling
;==============================================================================
LSP_ProcessResponse proc pResponse:DWORD
    ; Parse JSON-RPC response
    ; Extract result and dispatch to UI
    ret
LSP_ProcessResponse endp

;==============================================================================
; Phase 14.5: Pipe Communication
;==============================================================================
LSP_WriteToPipe proc pData:DWORD
    ; Implementation of writing to the language server's stdin pipe
    ret
LSP_WriteToPipe endp

end
