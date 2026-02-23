; ═══════════════════════════════════════════════════════════════════
; RawrXD WebView2 Shell — Sovereign Monaco Embedding (Production)
; Full COM callback lifecycle, correct vtable offsets from WebView2.h
;
; Architecture:
;   1. LoadLibraryW("WebView2Loader.dll") — dynamic load, graceful fallback
;   2. WebView2CreateEnvironment → EnvironmentCompletedHandler callback
;   3. EnvironmentCompleted → CreateCoreWebView2Controller → ControllerCompletedHandler
;   4. ControllerCompleted → get_CoreWebView2 → NavigateToString(Monaco HTML)
;      → add_WebMessageReceived → mark ready
;   5. Bridge: PostWebMessageAsJson ↔ beacon slot 6 for bidirectional IPC
;
; If WebView2 runtime is not installed, falls back to GDI editor (ui.asm).
;
; ═══════════════════════════════════════════════════════════════════
; COM Vtable Offsets (verified against WebView2.h ICoreWebView2Vtbl):
;
; ICoreWebView2:
;   Navigate             = 28h   (slot 5)
;   NavigateToString      = 30h   (slot 6)
;   ExecuteScript         = 0E8h  (slot 29)
;   PostWebMessageAsJson  = 100h  (slot 32)
;   add_WebMessageReceived = 110h  (slot 34)
;
; ICoreWebView2Controller:
;   put_Bounds            = 30h   (slot 6)
;   Close                 = 0C0h  (slot 24)
;   get_CoreWebView2      = 0C8h  (slot 25)
;
; ICoreWebView2Environment:
;   CreateCoreWebView2Controller = 18h  (slot 3)
;
; ICoreWebView2WebMessageReceivedEventArgs:
;   get_WebMessageAsJson  = 20h   (slot 4)
;
; All callback handlers (EnvironmentCompleted, ControllerCompleted,
;   WebMessageReceived): QI=0, AddRef=8, Release=10h, Invoke=18h
; ═══════════════════════════════════════════════════════════════════

EXTERN LoadLibraryW:PROC
EXTERN GetProcAddress:PROC
EXTERN FreeLibrary:PROC
EXTERN BeaconSend:PROC
EXTERN hMainWnd:QWORD
EXTERN g_hInstance:QWORD
EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN MoveWindow:PROC
EXTERN GetClientRect:PROC
EXTERN lstrcpyW:PROC
EXTERN lstrlenW:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN MultiByteToWideChar:PROC
EXTERN CoTaskMemFree:PROC

PUBLIC WebView2Init
PUBLIC WebView2Navigate
PUBLIC WebView2PostMessage
PUBLIC WebView2Resize
PUBLIC WebView2IsReady
PUBLIC WebView2Shutdown
PUBLIC WebView2CreateEnvironment
PUBLIC WebView2NavigateToString
PUBLIC WebView2ExecuteScript

; ── Constants ────────────────────────────────────────────────────
WEBVIEW2_BEACON_SLOT equ 6              ; beacon slot for WebView2 ↔ kernel IPC
WV2_MSG_RECEIVED    equ 3010h           ; beacon message: JS→ASM
WV2_READY_BEACON    equ 3011h           ; beacon: WebView2 fully ready
WV2_ERROR_BEACON    equ 3012h           ; beacon: WebView2 error

; COM HRESULT
S_OK                 equ 0
E_FAIL               equ 80004005h
E_NOINTERFACE        equ 80004002h
CLASS_E_NOAGGREGATION equ 80040110h

; Memory
MEM_COMMIT           equ 1000h
MEM_RESERVE          equ 2000h
MEM_RELEASE          equ 8000h
PAGE_READWRITE       equ 04h
CP_UTF8              equ 65001

; ── COM Vtable Offset Constants ──────────────────────────────────
; ICoreWebView2
WV2_Navigate                equ 028h
WV2_NavigateToString        equ 030h
WV2_ExecuteScript           equ 0E8h
WV2_PostWebMessageAsJson    equ 100h
WV2_add_WebMessageReceived  equ 110h

; ICoreWebView2Controller
WV2C_put_Bounds             equ 030h
WV2C_Close                  equ 0C0h
WV2C_get_CoreWebView2       equ 0C8h

; ICoreWebView2Environment
WV2E_CreateController       equ 018h

; ICoreWebView2WebMessageReceivedEventArgs
WV2A_get_WebMessageAsJson   equ 020h

; IUnknown
IU_QueryInterface           equ 000h
IU_AddRef                   equ 008h
IU_Release                  equ 010h

; Callback handler Invoke
CB_Invoke                   equ 018h

; ══════════════════════════════════════════════════════════════════
;                      DATA SECTIONS
; ══════════════════════════════════════════════════════════════════

.data?
align 8
g_hWebView2Dll   dq ?                   ; HMODULE for WebView2Loader.dll
g_pfnCreateEnv   dq ?                   ; CreateCoreWebView2EnvironmentWithOptions

; WebView2 COM interfaces (stored as raw pointers after creation callback)
g_pEnvironment   dq ?                   ; ICoreWebView2Environment*
g_pController    dq ?                   ; ICoreWebView2Controller*
g_pWebView       dq ?                   ; ICoreWebView2*

; Navigation URL buffer (520 bytes = MAX_PATH wchars)
g_navUrlW        dw 260 dup(?)

; Client rect for resize
g_wv2Rect        dd 4 dup(?)            ; left, top, right, bottom

; COM callback handler objects (16 bytes each: pVtable + refCount + pad)
; Layout: +0h = pVtable, +8h = refCount(dd), +Ch = pad(dd)
g_envCompletedObj    dq 2 dup(?)        ; EnvironmentCompletedHandler
g_ctrlCompletedObj   dq 2 dup(?)        ; ControllerCompletedHandler
g_webMsgObj          dq 2 dup(?)        ; WebMessageReceivedEventHandler

; WebMessage event registration token (8 bytes)
g_webMsgToken        dq ?

; Wide string buffer for NavigateToString/ExecuteScript (64KB)
g_wideHtmlBuf        dq ?               ; VirtualAlloc'd pointer

; Temp pointer for CoTaskMemFree
g_pTempStr           dq ?

.data
align 4
g_wv2Ready       dd 0                   ; 1 = WebView2 fully initialized
g_wv2Available   dd 0                   ; 1 = WebView2Loader.dll loaded

; ── Static COM Vtables ───────────────────────────────────────────
; EnvironmentCompletedHandler vtable
align 8
vtbl_EnvCompleted   dq _COM_QI_Stub
                    dq _COM_AddRef_Stub
                    dq _COM_Release_Stub
                    dq _COM_EnvCompleted_Invoke

; ControllerCompletedHandler vtable
align 8
vtbl_CtrlCompleted  dq _COM_QI_Stub
                    dq _COM_AddRef_Stub
                    dq _COM_Release_Stub
                    dq _COM_CtrlCompleted_Invoke

; WebMessageReceivedEventHandler vtable
align 8
vtbl_WebMsgReceived dq _COM_QI_Stub
                    dq _COM_AddRef_Stub
                    dq _COM_Release_Stub
                    dq _COM_WebMsgReceived_Invoke

.const
; DLL and function names (wide strings)
szWebView2Dll    dw 'W','e','b','V','i','e','w','2','L','o','a','d','e','r','.','d','l','l',0
; Narrow string for GetProcAddress
szCreateEnvFn    db "CreateCoreWebView2EnvironmentWithOptions",0

; Default editor URL (bundled HTML or localhost dev server)
szDefaultUrl     dw 'f','i','l','e',':','/','/','/','/','C',':','/','r','a','w','r','x','d'
                 dw '/','e','d','i','t','o','r','.','h','t','m','l',0

; IID_IUnknown = {00000000-0000-0000-C000-000000000046}
align 4
IID_IUnknown     dd 000000000h
                 dw 00000h, 00000h
                 db 0C0h, 000h, 000h, 000h, 000h, 000h, 000h, 046h

; ── Monaco Editor HTML (UTF-8, converted to wide at runtime) ─────
; Minimal self-contained Monaco editor loaded from cdnjs CDN.
; Injected via NavigateToString on controller-ready.
align 1
szMonacoHtml     db '<!DOCTYPE html><html><head><meta charset="utf-8">'
                 db '<style>'
                 db 'html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;}'
                 db '#editor{width:100%;height:100%;}'
                 db '</style>'
                 db '<link rel="stylesheet" data-name="vs/editor/editor.main"'
                 db ' href="https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.45.0'
                 db '/min/vs/editor/editor.main.min.css">'
                 db '</head><body><div id="editor"></div>'
                 db '<script src="https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.45.0'
                 db '/min/vs/loader.min.js"></script>'
                 db '<script>'
                 db 'require.config({paths:{vs:"https://cdnjs.cloudflare.com/ajax/libs'
                 db '/monaco-editor/0.45.0/min/vs"}});'
                 db 'require(["vs/editor/editor.main"],function(){'
                 db 'window._ed=monaco.editor.create(document.getElementById("editor"),{'
                 db 'value:"// RawrXD Sovereign Editor\n",language:"cpp",'
                 db 'theme:"vs-dark",fontSize:14,minimap:{enabled:true},'
                 db 'automaticLayout:true,scrollBeyondLastLine:false'
                 db '});'
                 db 'window._ed.onDidChangeModelContent(function(){'
                 db 'window.chrome.webview.postMessage(JSON.stringify({'
                 db 'type:"contentChanged",value:window._ed.getValue()}))'
                 db '});'
                 db 'window.chrome.webview.addEventListener("message",function(e){'
                 db 'var m=JSON.parse(e.data);'
                 db 'if(m.type==="setValue")window._ed.setValue(m.value);'
                 db 'if(m.type==="setLanguage")monaco.editor.setModelLanguage('
                 db 'window._ed.getModel(),m.value);'
                 db 'if(m.type==="setTheme")monaco.editor.setTheme(m.value);'
                 db 'if(m.type==="setFontSize")window._ed.updateOptions({fontSize:m.value});'
                 db 'if(m.type==="getValue")window.chrome.webview.postMessage('
                 db 'JSON.stringify({type:"value",value:window._ed.getValue()}));'
                 db 'if(m.type==="exec"){'
                 db 'try{var r=eval(m.value);window.chrome.webview.postMessage('
                 db 'JSON.stringify({type:"execResult",value:String(r)}))'
                 db '}catch(x){window.chrome.webview.postMessage('
                 db 'JSON.stringify({type:"execError",value:x.message}))}'
                 db '}'
                 db '});'
                 db 'window.chrome.webview.postMessage(JSON.stringify({type:"ready"}));'
                 db '});'
                 db '</script></body></html>',0
szMonacoHtmlEnd  label byte

.code
; ══════════════════════════════════════════════════════════════════
;              COM CALLBACK STUBS (shared by all handlers)
; ══════════════════════════════════════════════════════════════════

; ────────────────────────────────────────────────────────────────
; _COM_QI_Stub — Universal QueryInterface for static handlers
;   Always succeeds for IID_IUnknown; else returns self (permissive).
;   RCX = this, RDX = riid, R8 = ppvObject
; ────────────────────────────────────────────────────────────────
_COM_QI_Stub PROC
    test    r8, r8
    jz      @qi_fail
    mov     [r8], rcx                   ; *ppvObject = this
    ; AddRef
    mov     rax, [rcx]                  ; vptr
    jmp     qword ptr [rax + IU_AddRef] ; tail-call AddRef
@qi_fail:
    mov     eax, E_FAIL
    ret
_COM_QI_Stub ENDP

; ────────────────────────────────────────────────────────────────
; _COM_AddRef_Stub — increment refcount
;   RCX = this (pVtable at +0, refCount at +8)
; ────────────────────────────────────────────────────────────────
_COM_AddRef_Stub PROC
    lock inc dword ptr [rcx + 8]
    mov     eax, [rcx + 8]
    ret
_COM_AddRef_Stub ENDP

; ────────────────────────────────────────────────────────────────
; _COM_Release_Stub — decrement refcount (static objects: no delete)
;   RCX = this
; ────────────────────────────────────────────────────────────────
_COM_Release_Stub PROC
    lock dec dword ptr [rcx + 8]
    mov     eax, [rcx + 8]
    ; Static objects: never reach 0 or never freed
    ret
_COM_Release_Stub ENDP

; ══════════════════════════════════════════════════════════════════
;     CALLBACK: EnvironmentCompletedHandler::Invoke
;   RCX = this, EDX = errorCode, R8 = ICoreWebView2Environment*
; ══════════════════════════════════════════════════════════════════
_COM_EnvCompleted_Invoke PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    test    edx, edx                    ; HRESULT errorCode
    js      @eci_fail                   ; FAILED(hr)

    ; Store environment
    mov     g_pEnvironment, r8

    ; AddRef on environment
    mov     rcx, r8
    mov     rax, [rcx]                  ; vptr
    call    qword ptr [rax + IU_AddRef]

    ; Create controller: pEnv->CreateCoreWebView2Controller(hMainWnd, &ctrlCallback)
    mov     rcx, g_pEnvironment
    mov     rdx, hMainWnd
    lea     r8, g_ctrlCompletedObj
    mov     rax, [rcx]
    call    qword ptr [rax + WV2E_CreateController]

    test    eax, eax
    js      @eci_fail

    xor     eax, eax
    add     rsp, 30h
    pop     rbx
    ret

@eci_fail:
    ; Beacon error
    mov     ecx, WEBVIEW2_BEACON_SLOT
    mov     edx, eax
    mov     r8d, WV2_ERROR_BEACON
    call    BeaconSend
    mov     eax, S_OK
    add     rsp, 30h
    pop     rbx
    ret
_COM_EnvCompleted_Invoke ENDP

; ══════════════════════════════════════════════════════════════════
;     CALLBACK: ControllerCompletedHandler::Invoke
;   RCX = this, EDX = errorCode, R8 = ICoreWebView2Controller*
; ══════════════════════════════════════════════════════════════════
_COM_CtrlCompleted_Invoke PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 48h
    .allocstack 48h
    .endprolog

    test    edx, edx
    js      @cci_fail

    ; Store controller
    mov     g_pController, r8
    mov     rbx, r8

    ; AddRef controller
    mov     rcx, r8
    mov     rax, [rcx]
    call    qword ptr [rax + IU_AddRef]

    ; get_CoreWebView2 → g_pWebView
    mov     rcx, rbx
    lea     rdx, g_pWebView
    mov     rax, [rcx]
    call    qword ptr [rax + WV2C_get_CoreWebView2]
    test    eax, eax
    js      @cci_fail

    ; AddRef WebView2
    mov     rcx, g_pWebView
    test    rcx, rcx
    jz      @cci_fail
    mov     rax, [rcx]
    call    qword ptr [rax + IU_AddRef]

    ; ── put_Bounds to fill parent window ──────────────────────────
    mov     rcx, hMainWnd
    lea     rdx, g_wv2Rect
    call    GetClientRect

    mov     rcx, rbx                    ; controller
    lea     rdx, g_wv2Rect              ; RECT passed by hidden ptr (>8 bytes)
    mov     rax, [rcx]
    call    qword ptr [rax + WV2C_put_Bounds]

    ; ── Register WebMessageReceived handler ───────────────────────
    mov     rcx, g_pWebView
    lea     rdx, g_webMsgObj
    lea     r8, g_webMsgToken
    mov     rax, [rcx]
    call    qword ptr [rax + WV2_add_WebMessageReceived]

    ; ── Inject Monaco editor via NavigateToString ─────────────────
    ; Convert UTF-8 Monaco HTML to UTF-16
    call    _WV2_ConvertMonacoHtml
    test    rax, rax
    jz      @cci_nav_url

    ; NavigateToString(htmlContent)
    mov     rcx, g_pWebView
    mov     rdx, rax                    ; wide HTML
    mov     rax, [rcx]
    call    qword ptr [rax + WV2_NavigateToString]
    jmp     @cci_ready

@cci_nav_url:
    ; Fallback: navigate to file URL
    mov     rcx, g_pWebView
    lea     rdx, g_navUrlW
    ; Check if URL is populated
    cmp     word ptr [rdx], 0
    je      @cci_defurl
    mov     rax, [rcx]
    call    qword ptr [rax + WV2_Navigate]
    jmp     @cci_ready
@cci_defurl:
    lea     rdx, szDefaultUrl
    mov     rax, [rcx]
    call    qword ptr [rax + WV2_Navigate]

@cci_ready:
    mov     g_wv2Ready, 1

    ; Beacon: ready
    mov     ecx, WEBVIEW2_BEACON_SLOT
    xor     edx, edx
    mov     r8d, WV2_READY_BEACON
    call    BeaconSend

    xor     eax, eax
    add     rsp, 48h
    pop     rsi
    pop     rbx
    ret

@cci_fail:
    mov     ecx, WEBVIEW2_BEACON_SLOT
    mov     edx, eax
    mov     r8d, WV2_ERROR_BEACON
    call    BeaconSend
    mov     eax, S_OK
    add     rsp, 48h
    pop     rsi
    pop     rbx
    ret
_COM_CtrlCompleted_Invoke ENDP

; ══════════════════════════════════════════════════════════════════
;     CALLBACK: WebMessageReceivedEventHandler::Invoke
;   RCX = this, RDX = ICoreWebView2* sender,
;   R8  = ICoreWebView2WebMessageReceivedEventArgs*
; ══════════════════════════════════════════════════════════════════
_COM_WebMsgReceived_Invoke PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    mov     rbx, r8                     ; args

    ; get_WebMessageAsJson → pStr
    mov     rcx, rbx
    lea     rdx, g_pTempStr
    mov     rax, [rcx]
    call    qword ptr [rax + WV2A_get_WebMessageAsJson]
    test    eax, eax
    js      @wmr_done

    ; Beacon the message pointer to the kernel
    ; slot 6, data = pointer to wide JSON string
    mov     ecx, WEBVIEW2_BEACON_SLOT
    mov     rdx, g_pTempStr
    mov     r8d, WV2_MSG_RECEIVED
    call    BeaconSend

    ; Free the CoTaskMem-allocated string
    mov     rcx, g_pTempStr
    test    rcx, rcx
    jz      @wmr_done
    call    CoTaskMemFree
    xor     rax, rax
    mov     g_pTempStr, rax

@wmr_done:
    xor     eax, eax                    ; return S_OK
    add     rsp, 38h
    pop     rsi
    pop     rbx
    ret
_COM_WebMsgReceived_Invoke ENDP

; ══════════════════════════════════════════════════════════════════
; Internal: _WV2_ConvertMonacoHtml
;   Converts szMonacoHtml (UTF-8) → g_wideHtmlBuf (UTF-16)
;   Returns: RAX = wide string pointer, or NULL
; ══════════════════════════════════════════════════════════════════
_WV2_ConvertMonacoHtml PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Compute source length
    lea     rbx, szMonacoHtml
    lea     rax, szMonacoHtmlEnd
    sub     rax, rbx                    ; byte count including NUL

    ; First pass: get required wide char count
    mov     ecx, CP_UTF8               ; CodePage
    xor     edx, edx                   ; dwFlags
    mov     r8, rbx                    ; lpMultiByteStr
    mov     r9d, -1                    ; cbMultiByte (-1 = auto NUL)
    xor     eax, eax
    mov     [rsp+20h], rax             ; lpWideCharStr = NULL
    mov     [rsp+28h], rax             ; cchWideChar = 0
    call    MultiByteToWideChar
    test    eax, eax
    jz      @cvt_fail
    mov     [rsp+28h], eax             ; save char count

    ; Allocate wide buffer (chars * 2)
    mov     ecx, eax
    shl     ecx, 1
    add     ecx, 16                    ; safety margin
    push    rcx
    xor     ecx, ecx
    mov     edx, ecx
    pop     rdx
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @cvt_fail
    mov     g_wideHtmlBuf, rax

    ; Second pass: actual conversion
    mov     ecx, CP_UTF8
    xor     edx, edx
    mov     r8, rbx                    ; source
    mov     r9d, -1
    mov     rax, g_wideHtmlBuf
    mov     [rsp+20h], rax             ; lpWideCharStr
    mov     eax, [rsp+28h]
    mov     [rsp+28h], eax             ; cchWideChar
    call    MultiByteToWideChar
    test    eax, eax
    jz      @cvt_fail

    mov     rax, g_wideHtmlBuf
    add     rsp, 30h
    pop     rbx
    ret

@cvt_fail:
    xor     eax, eax
    add     rsp, 30h
    pop     rbx
    ret
_WV2_ConvertMonacoHtml ENDP

; ══════════════════════════════════════════════════════════════════
;                   PUBLIC API IMPLEMENTATIONS
; ══════════════════════════════════════════════════════════════════

; ════════════════════════════════════════════════════════════════
; WebView2Init — load WebView2Loader.dll + resolve CreateEnv
;   Returns: EAX = 0 if WebView2 available, -1 if fallback to GDI
;
; This is a graceful probe: if WebView2 is not installed,
; the kernel continues with the GDI editor from ui.asm.
; ════════════════════════════════════════════════════════════════
WebView2Init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Zero all state
    xor     eax, eax
    mov     g_pEnvironment, rax
    mov     g_pController, rax
    mov     g_pWebView, rax
    mov     g_wv2Ready, eax
    mov     g_wv2Available, eax
    mov     g_wideHtmlBuf, rax
    mov     g_pTempStr, rax

    ; Initialize COM callback objects (point to static vtables)
    lea     rax, vtbl_EnvCompleted
    mov     g_envCompletedObj, rax
    mov     dword ptr [g_envCompletedObj + 8], 1    ; refcount=1

    lea     rax, vtbl_CtrlCompleted
    mov     g_ctrlCompletedObj, rax
    mov     dword ptr [g_ctrlCompletedObj + 8], 1

    lea     rax, vtbl_WebMsgReceived
    mov     g_webMsgObj, rax
    mov     dword ptr [g_webMsgObj + 8], 1

    ; LoadLibraryW("WebView2Loader.dll")
    lea     rcx, szWebView2Dll
    call    LoadLibraryW
    test    rax, rax
    jz      @wv2_unavailable
    mov     g_hWebView2Dll, rax
    mov     rbx, rax

    ; GetProcAddress for CreateCoreWebView2EnvironmentWithOptions
    mov     rcx, rbx
    lea     rdx, szCreateEnvFn
    call    GetProcAddress
    test    rax, rax
    jz      @wv2_free_dll
    mov     g_pfnCreateEnv, rax

    ; WebView2Loader.dll is available
    mov     g_wv2Available, 1

    add     rsp, 30h
    pop     rbx
    xor     eax, eax                    ; success — WebView2 available
    ret

@wv2_free_dll:
    mov     rcx, rbx
    call    FreeLibrary
    xor     eax, eax
    mov     g_hWebView2Dll, rax

@wv2_unavailable:
    add     rsp, 30h
    pop     rbx
    mov     eax, -1                     ; fallback to GDI
    ret
WebView2Init ENDP

; ════════════════════════════════════════════════════════════════
; WebView2CreateEnvironment — kick off async COM creation chain
;   Call after WebView2Init succeeds and HWND is valid.
;   Returns: EAX = HRESULT from CreateCoreWebView2EnvironmentWithOptions
; ════════════════════════════════════════════════════════════════
WebView2CreateEnvironment PROC FRAME
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    cmp     g_wv2Available, 0
    je      @wce_fail

    ; Already created?
    mov     rax, g_pEnvironment
    test    rax, rax
    jnz     @wce_ok

    ; CreateCoreWebView2EnvironmentWithOptions(
    ;   browserExeFolder=NULL, userDataFolder=NULL,
    ;   options=NULL, handler=&envCompletedObj)
    xor     ecx, ecx                    ; browserExeFolder
    xor     edx, edx                    ; userDataFolder
    xor     r8d, r8d                    ; environmentOptions
    lea     r9, g_envCompletedObj       ; handler
    call    g_pfnCreateEnv
    test    eax, eax
    js      @wce_fail

@wce_ok:
    xor     eax, eax
    add     rsp, 38h
    ret

@wce_fail:
    mov     eax, E_FAIL
    add     rsp, 38h
    ret
WebView2CreateEnvironment ENDP

; ════════════════════════════════════════════════════════════════
; WebView2Navigate — navigate the WebView2 to a URL
;   RCX = url (LPCWSTR), NULL = use default editor URL
;   Returns: EAX = 0 on success, -1 if WebView2 not available
; ════════════════════════════════════════════════════════════════
WebView2Navigate PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    cmp     g_wv2Available, 0
    je      @nav_fail

    ; If URL is NULL, use default
    test    rcx, rcx
    jnz     @nav_has_url
    lea     rcx, szDefaultUrl
@nav_has_url:

    ; Copy URL to internal buffer
    lea     rdx, g_navUrlW
    xchg    rcx, rdx                    ; rcx = dst, rdx = src
    call    lstrcpyW

    ; If WebView2 COM object is live, call Navigate on it
    mov     rax, g_pWebView
    test    rax, rax
    jz      @nav_deferred

    ; ICoreWebView2::Navigate(url)
    mov     rcx, rax                    ; this
    lea     rdx, g_navUrlW              ; url
    mov     rax, [rcx]                  ; vptr
    call    qword ptr [rax + WV2_Navigate]

    add     rsp, 20h
    pop     rbx
    xor     eax, eax
    ret

@nav_deferred:
    ; URL stored; will be navigated when controller completes setup
    add     rsp, 20h
    pop     rbx
    xor     eax, eax
    ret

@nav_fail:
    add     rsp, 20h
    pop     rbx
    mov     eax, -1
    ret
WebView2Navigate ENDP

; ════════════════════════════════════════════════════════════════
; WebView2NavigateToString — inject raw HTML content
;   RCX = htmlContent (LPCWSTR, UTF-16)
;   Returns: EAX = HRESULT
; ════════════════════════════════════════════════════════════════
WebView2NavigateToString PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rax, g_pWebView
    test    rax, rax
    jz      @nts_fail

    mov     rdx, rcx                    ; htmlContent
    mov     rcx, rax                    ; this
    mov     rax, [rcx]
    call    qword ptr [rax + WV2_NavigateToString]

    add     rsp, 28h
    ret

@nts_fail:
    mov     eax, E_FAIL
    add     rsp, 28h
    ret
WebView2NavigateToString ENDP

; ════════════════════════════════════════════════════════════════
; WebView2PostMessage — send JSON message to WebView2 content
;   RCX = json (LPCWSTR)
;   Returns: EAX = 0 on success
; ════════════════════════════════════════════════════════════════
WebView2PostMessage PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rax, g_pWebView
    test    rax, rax
    jz      @pm_fail

    ; ICoreWebView2::PostWebMessageAsJson
    mov     rdx, rcx                    ; json string
    mov     rcx, rax                    ; this
    mov     rax, [rcx]                  ; vptr
    call    qword ptr [rax + WV2_PostWebMessageAsJson]

    add     rsp, 28h
    xor     eax, eax
    ret

@pm_fail:
    add     rsp, 28h
    mov     eax, -1
    ret
WebView2PostMessage ENDP

; ════════════════════════════════════════════════════════════════
; WebView2ExecuteScript — evaluate JavaScript in the WebView2
;   RCX = javaScript (LPCWSTR)
;   RDX = completionHandler (may be NULL for fire-and-forget)
;   Returns: EAX = HRESULT
; ════════════════════════════════════════════════════════════════
WebView2ExecuteScript PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rax, g_pWebView
    test    rax, rax
    jz      @es_fail

    ; ICoreWebView2::ExecuteScript(javaScript, handler)
    mov     r8, rdx                     ; completionHandler
    mov     rdx, rcx                    ; javaScript
    mov     rcx, rax                    ; this
    mov     rax, [rcx]
    call    qword ptr [rax + WV2_ExecuteScript]

    add     rsp, 28h
    ret

@es_fail:
    mov     eax, E_FAIL
    add     rsp, 28h
    ret
WebView2ExecuteScript ENDP

; ════════════════════════════════════════════════════════════════
; WebView2Resize — resize WebView2 controller to fill parent HWND
;   Called on WM_SIZE from ui.asm WndProc
; ════════════════════════════════════════════════════════════════
WebView2Resize PROC FRAME
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    mov     rax, g_pController
    test    rax, rax
    jz      @resize_skip

    ; GetClientRect(hMainWnd, &rect)
    mov     rcx, hMainWnd
    lea     rdx, g_wv2Rect
    call    GetClientRect

    ; ICoreWebView2Controller::put_Bounds(rect)
    mov     rcx, g_pController
    lea     rdx, g_wv2Rect
    mov     rax, [rcx]
    call    qword ptr [rax + WV2C_put_Bounds]

@resize_skip:
    add     rsp, 38h
    ret
WebView2Resize ENDP

; ════════════════════════════════════════════════════════════════
; WebView2IsReady — check if WebView2 is fully initialized
;   Returns: EAX = 1 if ready, 0 if not
; ════════════════════════════════════════════════════════════════
WebView2IsReady PROC
    mov     eax, g_wv2Ready
    ret
WebView2IsReady ENDP

; ════════════════════════════════════════════════════════════════
; WebView2Shutdown — release WebView2 resources
;   Called during WM_DESTROY cleanup
; ════════════════════════════════════════════════════════════════
WebView2Shutdown PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Release WebView2 COM objects via Release() (vtable offset 10h)
    mov     rcx, g_pWebView
    test    rcx, rcx
    jz      @ws_no_webview
    mov     rax, [rcx]
    call    qword ptr [rax + IU_Release]
    xor     eax, eax
    mov     g_pWebView, rax
@ws_no_webview:

    mov     rcx, g_pController
    test    rcx, rcx
    jz      @ws_no_ctrl
    ; Close controller before releasing
    mov     rax, [rcx]
    call    qword ptr [rax + WV2C_Close]
    mov     rcx, g_pController
    mov     rax, [rcx]
    call    qword ptr [rax + IU_Release]
    xor     eax, eax
    mov     g_pController, rax
@ws_no_ctrl:

    mov     rcx, g_pEnvironment
    test    rcx, rcx
    jz      @ws_no_env
    mov     rax, [rcx]
    call    qword ptr [rax + IU_Release]
    xor     eax, eax
    mov     g_pEnvironment, rax
@ws_no_env:

    ; Free wide HTML buffer
    mov     rcx, g_wideHtmlBuf
    test    rcx, rcx
    jz      @ws_no_html
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    xor     rax, rax
    mov     g_wideHtmlBuf, rax
@ws_no_html:

    ; Unload DLL
    mov     rcx, g_hWebView2Dll
    test    rcx, rcx
    jz      @ws_no_dll
    call    FreeLibrary
    xor     eax, eax
    mov     g_hWebView2Dll, rax
@ws_no_dll:

    mov     g_wv2Ready, 0
    mov     g_wv2Available, 0

    add     rsp, 28h
    ret
WebView2Shutdown ENDP

END
