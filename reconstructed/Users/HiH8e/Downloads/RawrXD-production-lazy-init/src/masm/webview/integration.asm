;==============================================================================
; webview_integration.asm - Pure MASM64 WebView2 Browser Integration
; ==========================================================================
; Implements COM interop for Microsoft Edge WebView2.
; Zero C++ runtime dependencies.
;==============================================================================

.686p
.xmm
.model flat, c
option casemap:none

include windows.inc
includelib ole32.lib
includelib WebView2Loader.lib

;==============================================================================
; COM INTERFACES (Simplified)
;==============================================================================
; ICoreWebView2 interface vtable offsets
ICoreWebView2_Navigate          equ 8 * 8
ICoreWebView2_get_Settings      equ 9 * 8
ICoreWebView2_ExecuteScript     equ 10 * 8

;==============================================================================
; DATA SEGMENT
;==============================================================================
.data
    szWebView2Dll       BYTE "WebView2Loader.dll",0
    szCreateEnvFunc     BYTE "CreateCoreWebView2EnvironmentWithOptions",0
    szDefaultURL        BYTE "https://www.google.com",0
    
    CLSID_WebView2      GUID {0x26D341DB, 0x4A13, 0x4B9A, {0x8E, 0x5E, 0xDE, 0x1F, 0x1B, 0x6D, 0x27, 0x2E}}
    IID_ICoreWebView2   GUID {0x76ECEACD, 0x0462, 0x4D9A, {0xAC, 0x70, 0x13, 0x15, 0x7A, 0x83, 0x5B, 0x68}}

.data?
    pWebView            QWORD ?
    pController         QWORD ?

.code

;==============================================================================
; WebViewInitialize - Initializes the WebView2 environment
;==============================================================================
WebViewInitialize PROC uses rbx rsi rdi hWnd:HWND
    LOCAL hDll:QWORD
    LOCAL pCreateEnv:QWORD
    
    ; 1. Initialize COM for apartment threading
    xor rcx, rcx
    mov edx, 0              ; COINIT_APARTMENTTHREADED
    call CoInitializeEx
    test eax, eax
    jnz init_failed
    
    ; 2. Load WebView2Loader.dll dynamically
    lea rcx, [szWebView2Dll]
    call LoadLibraryA
    test rax, rax
    jz init_failed
    mov hDll, rax
    
    ; 3. Get CreateCoreWebView2EnvironmentWithOptions function
    mov rcx, hDll
    lea rdx, [szCreateEnvFunc]
    call GetProcAddress
    test rax, rax
    jz init_failed
    mov pCreateEnv, rax
    
    ; 4. Initialize WebView2 environment
    mov rcx, hWnd           ; Parent window handle
    xor edx, edx            ; user data folder = NULL
    xor r8, r8              ; options = NULL
    
    sub rsp, 20h
    mov rax, pCreateEnv
    call rax
    add rsp, 20h
    
    mov pWebView, rax
    mov eax, 1
    ret
    
init_failed:
    xor eax, eax
    ret
WebViewInitialize ENDP

;==============================================================================
; WebViewNavigate - Navigate WebView to URL
;==============================================================================
WebViewNavigate PROC uses rbx rsi rdi lpURL:QWORD
    LOCAL pVTable:QWORD
    LOCAL pNavigateFunc:QWORD
    
    ; Check if WebView is initialized
    mov rcx, pWebView
    test rcx, rcx
    jz nav_failed
    
    ; Get vTable from WebView COM object
    mov rax, [rcx]
    mov pVTable, rax
    
    ; Get Navigate method (offset 8*8 = 64)
    mov rax, [pVTable + 64]
    mov pNavigateFunc, rax
    
    ; Call Navigate(lpURL)
    mov rcx, pWebView
    mov rdx, lpURL
    
    sub rsp, 20h
    mov rax, pNavigateFunc
    call rax
    add rsp, 20h
    
    test eax, eax
    jnz nav_failed
    
    mov eax, 1
    ret
    
nav_failed:
    xor eax, eax
    ret
WebViewNavigate ENDP

END
