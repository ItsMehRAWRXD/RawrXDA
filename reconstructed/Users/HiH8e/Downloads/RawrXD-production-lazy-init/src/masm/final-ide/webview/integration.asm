;==============================================================================
; webview_integration.asm - Pure MASM64 WebView2 Browser Integration
; ==========================================================================
; Implements COM interop for Microsoft Edge WebView2.
; Zero C++ runtime dependencies.
;==============================================================================

option casemap:none

include windows.inc
includelib ole32.lib
includelib WebView2Loader.lib

;==============================================================================
; COM INTERFACES (Simplified)
;==============================================================================
; IUnknown interface vtable offsets
IUnknown_QueryInterface         equ 0 * 8
IUnknown_AddRef                 equ 1 * 8
IUnknown_Release                equ 2 * 8

; ICoreWebView2 interface vtable offsets
ICoreWebView2_Navigate          equ 8 * 8
ICoreWebView2_get_Settings      equ 9 * 8
ICoreWebView2_ExecuteScript     equ 10 * 8

; ICoreWebView2Controller interface vtable offsets
ICoreWebView2Controller_get_CoreWebView2 equ 25 * 8
ICoreWebView2Controller_put_Bounds       equ 6 * 8

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================
EXTERN CoInitializeEx:PROC
EXTERN CoCreateInstance:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN GetClientRect:PROC
EXTERN MultiByteToWideChar:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC

;==============================================================================
; STRUCTURES
;==============================================================================
; COM Object structure for handlers
COM_OBJECT STRUCT
    lpVtbl          QWORD ?
    refCount        DWORD ?
    hWnd            HWND ?
COM_OBJECT ENDS

;==============================================================================
; DATA SEGMENT
;==============================================================================
.data
    szWebView2Dll       BYTE "WebView2Loader.dll",0
    szDefaultURL        BYTE "https://www.google.com",0
    szCreateEnv         BYTE "CreateCoreWebView2EnvironmentWithOptions",0
    
    CLSID_WebView2      GUID {0x26D341DB, 0x4A13, 0x4B9A, {0x8E, 0x5E, 0xDE, 0x1F, 0x1B, 0x6D, 0x27, 0x2E}}
    IID_ICoreWebView2   GUID {0x76ECEACD, 0x0462, 0x4D9A, {0xAC, 0x70, 0x13, 0x15, 0x7A, 0x83, 0x5B, 0x68}}
    IID_ICoreWebView2Environment GUID {0xB96D755E, 0x0319, 0x4E92, {0xA2, 0x9E, 0x23, 0xF6, 0x0A, 0x84, 0xA0, 0x05}}

.data?
    pWebView            QWORD ?
    pController         QWORD ?
    pEnvironment        QWORD ?

;==============================================================================
; CODE SEGMENT
;==============================================================================
.code

;==============================================================================
; WebViewInitialize - Initializes the WebView2 environment
;==============================================================================
WebViewInitialize PROC uses rbx rsi rdi hWnd:HWND
    LOCAL hDll:QWORD
    LOCAL pCreateEnv:QWORD
    
    ; 1. Initialize COM
    invoke CoInitializeEx, NULL, 2 ; COINIT_APARTMENTTHREADED
    
    ; 2. Load WebView2Loader.dll
    invoke LoadLibraryA, addr szWebView2Dll
    .if rax == 0
        ret
    .endif
    mov hDll, rax
    
    ; 3. Get CreateCoreWebView2EnvironmentWithOptions
    invoke GetProcAddress, hDll, addr szCreateEnv
    .if rax == 0
        ret
    .endif
    mov pCreateEnv, rax
    
    ; 4. Create Environment Completed Handler
    call CreateEnvHandler
    mov r8, rax         ; handler
    
    ; 5. Call CreateCoreWebView2EnvironmentWithOptions
    ; HRESULT CreateCoreWebView2EnvironmentWithOptions(
    ;     PCWSTR browserExecutableFolder,
    ;     PCWSTR userDataFolder,
    ;     ICoreWebView2EnvironmentOptions* environmentOptions,
    ;     ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler);
    
    xor rcx, rcx        ; browserExecutableFolder
    xor rdx, rdx        ; userDataFolder
    xor r9, r9          ; environmentOptions
    sub rsp, 32
    mov [rsp + 32], r8  ; handler
    call pCreateEnv
    add rsp, 32
    
    mov rax, TRUE
    ret
WebViewInitialize ENDP

;==============================================================================
; CreateEnvHandler - Creates a COM object for EnvironmentCompletedHandler
;==============================================================================
CreateEnvHandler PROC
    push rbx
    sub rsp, 32
    
    mov ecx, sizeof COM_OBJECT
    call asm_malloc
    mov rbx, rax
    
    lea rax, EnvHandlerVtbl
    mov [rbx + COM_OBJECT.lpVtbl], rax
    mov [rbx + COM_OBJECT.refCount], 1
    
    mov rax, rbx
    add rsp, 32
    pop rbx
    ret
CreateEnvHandler ENDP

;==============================================================================
; EnvHandler_Invoke - Called when environment creation is complete
;==============================================================================
EnvHandler_Invoke PROC this:QWORD, hr:DWORD, pEnv:QWORD
    push rbx
    sub rsp, 32
    
    .if hr == 0 ; S_OK
        mov rax, pEnv
        mov pEnvironment, rax
        
        ; Create Controller
        ; pEnv->CreateCoreWebView2Controller(hWnd, handler)
        ; ...
    .endif
    
    xor eax, eax ; S_OK
    add rsp, 32
    pop rbx
    ret
EnvHandler_Invoke ENDP

;==============================================================================
; WebViewNavigate - Navigates to a URL
;==============================================================================
WebViewNavigate PROC uses rbx rsi rdi lpURL:QWORD
    LOCAL szURLWide[512]:WORD
    
    .if pWebView == 0
        ret
    .endif
    
    ; Convert ANSI URL to Unicode
    invoke MultiByteToWideChar, 0, 0, lpURL, -1, addr szURLWide, 512
    
    ; Call pWebView->Navigate(lpURLWide)
    mov rax, pWebView
    mov rbx, [rax] ; vtable
    mov rbx, [rbx + ICoreWebView2_Navigate]
    
    sub rsp, 32
    mov rcx, rax
    lea rdx, szURLWide
    call rbx
    add rsp, 32
    
    ret
WebViewNavigate ENDP

.data
    EnvHandlerVtbl QWORD IUnknown_QueryInterface_Stub
                   QWORD IUnknown_AddRef_Stub
                   QWORD IUnknown_Release_Stub
                   QWORD EnvHandler_Invoke

.code
IUnknown_QueryInterface_Stub PROC this:QWORD, riid:QWORD, ppv:QWORD
    mov rax, 0x80004002 ; E_NOINTERFACE
    ret
IUnknown_QueryInterface_Stub ENDP

IUnknown_AddRef_Stub PROC this:QWORD
    mov rax, this
    inc dword ptr [rax + COM_OBJECT.refCount]
    mov eax, [rax + COM_OBJECT.refCount]
    ret
IUnknown_AddRef_Stub ENDP

IUnknown_Release_Stub PROC this:QWORD
    mov rax, this
    dec dword ptr [rax + COM_OBJECT.refCount]
    mov eax, [rax + COM_OBJECT.refCount]
    .if eax == 0
        invoke asm_free, this
    .endif
    ret
IUnknown_Release_Stub ENDP

END

END
