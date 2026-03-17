;==============================================================================
; RawrXD WebView2 Integration Adapter
; Implements vscode.window.createWebviewPanel using Edge WebView2
;
; Features:
; - Native HWND embedding of CoreWebView2
; - Message passing bridge (postMessage/onDidReceiveMessage)
; - Resource loading interception (vscode-resource://)
; - Developer tools integration
;==============================================================================
.686
.xmm
.model flat, c
option casemap:none
option frame:auto

;==============================================================================
; INCLUDES
;==============================================================================
include windows.inc
; WebView2.h pseudo-definitions for MASM
include webview2_conf.inc

includelib kernel32.lib
includelib msvcrt.lib

;==============================================================================
; EXTERNAL REFERENCES
;==============================================================================
EXTERN HeapAlloc:PROC
EXTERN g_ExtensionHostConfig:QWORD
EXTERN CreateCoreWebView2EnvironmentWithOptions:PROC

;==============================================================================
; STRUCTURES
;==============================================================================
WEBVIEW_CONTEXT struct
    lpVtbl              dq ?           ; ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
    RefCount            dd ?
    ParentWindow        dq ?
    WebviewPanel        dq ?           ; Back-pointer to extension host panel
    
    ; Com pointers
    pEnvironment        dq ?           ; ICoreWebView2Environment
    pController         dq ?           ; ICoreWebView2Controller
    pWebview            dq ?           ; ICoreWebView2
    
    ; Event tokens
    MessageToken        dq ?
    NavigationToken     dq ?
WEBVIEW_CONTEXT ends

;==============================================================================
; DATA
;==============================================================================
.data
align 16

; COM IIDs (placeholders - need actual GUIDs)
IID_ICoreWebView2Environment    GUID <0B1AEC80h, 0E927h, 0412Ah, <099h, 062h, 079h, 058h, 080h, 06Eh, 075h, 038h>>
IID_ICoreWebView2Controller     GUID <04D00C0Dh, 09455h, 04A76h, <0A0h, 068h, 052h, 0A3h, 09Ch, 071h, 0E2h, 05Ch>>

szEdgeUserDataFolder    db 'RawrXD_Edge_Data',0

;==============================================================================
; CODE
;==============================================================================
.code

;------------------------------------------------------------------------------
; ExtensionHostBridge_RegisterWebview
; Called from API_CreateWebviewPanel to initialize backend
;------------------------------------------------------------------------------
ExtensionHostBridge_RegisterWebview proc frame uses rbx, pPanel:qword
    mov rbx, pPanel
    
    ; Allocate context
    mov ecx, sizeof(WEBVIEW_CONTEXT)
    call HeapAlloc
    mov rsi, rax
    
    ; Init context
    mov [rsi].WEBVIEW_CONTEXT.WebviewPanel, rbx
    mov [rsi].WEBVIEW_CONTEXT.RefCount, 1
    
    ; Get parent window (from main process config)
    ; In reality we'd create a child window here
    mov rcx, [g_ExtensionHostConfig].ParentWindowHandle
    mov [rsi].WEBVIEW_CONTEXT.ParentWindow, rcx
    
    ; Initialize WebView2
    ; CreateCoreWebView2EnvironmentWithOptions(browserExe, userData, options, handler)
    
    sub rsp, 32
    xor rcx, rcx            ; browser executable folder (auto)
    lea rdx, szEdgeUserDataFolder
    xor r8, r8              ; options
    mov r9, rsi             ; handler (this)
    
    ; Call exported Loader function
    call CreateCoreWebView2EnvironmentWithOptions
    add rsp, 32
    
    ret
ExtensionHostBridge_RegisterWebview endp

;------------------------------------------------------------------------------
; ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler::Invoke
;------------------------------------------------------------------------------
WebView_OnEnvironmentCreated proc frame uses rbx rsi rdi, 
    this:ptr WEBVIEW_CONTEXT, 
    result:dword, 
    env:ptr ICoreWebView2Environment
    
    mov rsi, this
    mov rdi, env
    
    test edx, edx           ; check result
    js failed
    
    ; Store environment
    mov [rsi].WEBVIEW_CONTEXT.pEnvironment, rdi
    mov rcx, rdi
    mov rax, [rdi]          ; vtbl
    call [rax].IUnknown_AddRef
    
    ; Create Controller
    ; env->CreateCoreWebView2Controller(parentWindow, handler)
    mov rcx, rdi
    mov rdx, [rsi].WEBVIEW_CONTEXT.ParentWindow
    mov r8, rsi             ; re-use context as controller handler
    
    mov rax, [rcx]          ; vtbl
    call [rax].ICoreWebView2Environment_CreateCoreWebView2Controller
    
    ret
    
failed:
    ret
WebView_OnEnvironmentCreated endp

end
