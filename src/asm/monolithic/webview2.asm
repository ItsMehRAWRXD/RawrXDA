; ═══════════════════════════════════════════════════════════════════
; webview2.asm — WebView2 Runtime Loader & Controller
;
; Dynamically loads WebView2Loader.dll at init time. If the DLL or
; the WebView2 Runtime is not installed, all functions return gracefully
; (g_wv2Ready = 0) and the UI falls back to GDI rendering.
;
; Architecture:
;   1. WebView2Init loads WebView2Loader.dll via LoadLibraryW
;   2. Resolves CreateCoreWebView2EnvironmentWithOptions export
;   3. WebView2CreateEnvironment calls the resolved function
;   4. On success, the controller HWND is created and g_wv2Ready = 1
;   5. Navigate/PostMessage/ExecuteScript call through COM vtable
;   6. WebView2Shutdown releases COM pointers and frees loader DLL
;
; COM vtable layout for ICoreWebView2:
;   [0]  QueryInterface
;   [8]  AddRef
;   [16] Release
;   [24] get_Settings
;   [32] get_Source
;   [40] Navigate (LPCWSTR)
;   [48] NavigateToString (LPCWSTR)
;   [56] add_NavigationStarting
;   ...
;   [168] PostWebMessageAsJson (LPCWSTR)
;   [176] PostWebMessageAsString (LPCWSTR)
;   [248] ExecuteScript (LPCWSTR, handler)
;
; COM vtable layout for ICoreWebView2Controller:
;   [0]  QueryInterface
;   [8]  AddRef
;   [16] Release
;   [24] get_IsVisible
;   [32] put_IsVisible
;   [40] get_Bounds
;   [48] put_Bounds (RECT*)
; ═══════════════════════════════════════════════════════════════════

PUBLIC WebView2Init
PUBLIC WebView2Navigate
PUBLIC WebView2PostMessage
PUBLIC WebView2Resize
PUBLIC WebView2IsReady
PUBLIC WebView2Shutdown
PUBLIC WebView2CreateEnvironment
PUBLIC WebView2NavigateToString
PUBLIC WebView2ExecuteScript

; ── Win32 imports ────────────────────────────────────────────────
EXTERN LoadLibraryW:PROC
EXTERN FreeLibrary:PROC
EXTERN GetProcAddress:PROC
EXTERN CoInitializeEx:PROC
EXTERN CoUninitialize:PROC

; ── Beacon ───────────────────────────────────────────────────────
EXTERN BeaconSend:PROC

; ── UI globals ───────────────────────────────────────────────────
EXTERN hMainWnd:QWORD

; ── Constants ────────────────────────────────────────────────────
WV2_BEACON_SLOT         equ 17
WV2_EVT_INIT_OK         equ 0C1h
WV2_EVT_INIT_FAIL       equ 0C2h
WV2_EVT_NAVIGATE        equ 0C3h
WV2_EVT_SHUTDOWN        equ 0C4h

; COM vtable offsets for ICoreWebView2
VT_WV2_RELEASE          equ 16
VT_WV2_NAVIGATE         equ 40
VT_WV2_NAVIGATE_STRING  equ 48
VT_WV2_POST_JSON        equ 168
VT_WV2_EXEC_SCRIPT      equ 248

; COM vtable offsets for ICoreWebView2Controller
VT_CTRL_RELEASE         equ 16
VT_CTRL_PUT_BOUNDS      equ 48

; COINIT_APARTMENTTHREADED
COINIT_APARTMENTTHREADED equ 2

; ── Data ─────────────────────────────────────────────────────────
.data
align 8
g_wv2Ready          dd 0             ; 0 = not available
g_wv2ScriptErrors   dd 0             ; consecutive ExecuteScript failure count
g_hLoaderDll        dq 0             ; WebView2Loader.dll handle
g_pfnCreateEnv      dq 0             ; CreateCoreWebView2EnvironmentWithOptions
g_pWebView2         dq 0             ; ICoreWebView2* (after environment created)
g_pController       dq 0             ; ICoreWebView2Controller*
g_comInitialized    dd 0             ; 1 = CoInitializeEx called

szLoaderDll         dw 'W','e','b','V','i','e','w','2','L','o','a','d','e','r','.','d','l','l',0
szCreateEnv         db "CreateCoreWebView2EnvironmentWithOptions",0

; ══════════════════════════════════════════════════════════════════
; COM Callback Handler Objects
;
; WebView2 async initialization requires two COM callback handlers:
;
; 1. EnvironmentCompletedHandler (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)
;    - Invoked when the WebView2 environment is created
;    - Its Invoke(HRESULT, ICoreWebView2Environment*) calls CreateCoreWebView2Controller
;
; 2. ControllerCompletedHandler (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)
;    - Invoked when the controller is created
;    - Its Invoke(HRESULT, ICoreWebView2Controller*) stores g_pController, gets webview
;
; Object layout (both identical structure):
;   [+0]   pVtable   (QWORD) → pointer to vtable array
;   [+8]   refCount  (DWORD) → COM reference count
;   [+16]  reserved  (DWORD) → alignment padding
;
; Vtable layout (4 entries, standard IUnknown + Invoke):
;   [0]  QueryInterface  (this, riid, ppv)
;   [8]  AddRef          (this)
;   [16] Release         (this)
;   [24] Invoke          (this, errorCode, createdObject)
; ══════════════════════════════════════════════════════════════════

; ── Environment Completed Handler ──
align 8
g_envHandler:
    dq g_envHandlerVtable            ; pVtable → vtable
    dd 1                             ; refCount = 1
    dd 0                             ; padding

align 8
g_envHandlerVtable:
    dq EnvHandler_QueryInterface
    dq EnvHandler_AddRef
    dq EnvHandler_Release
    dq EnvHandler_Invoke

; ── Controller Completed Handler ──
align 8
g_ctrlHandler:
    dq g_ctrlHandlerVtable           ; pVtable → vtable
    dd 1                             ; refCount = 1
    dd 0                             ; padding

align 8
g_ctrlHandlerVtable:
    dq CtrlHandler_QueryInterface
    dq CtrlHandler_AddRef
    dq CtrlHandler_Release
    dq CtrlHandler_Invoke

; ── ExecuteScript Completed Handler ──
; Used to avoid NULL callback in ExecuteScript on strict runtimes.
align 8
g_scriptHandler:
    dq g_scriptHandlerVtable         ; pVtable
    dd 1                             ; refCount
    dd 0                             ; padding

align 8
g_scriptHandlerVtable:
    dq ScriptHandler_QueryInterface
    dq ScriptHandler_AddRef
    dq ScriptHandler_Release
    dq ScriptHandler_Invoke

; IID_IUnknown {00000000-0000-0000-C000-000000000046}
align 4
IID_IUnknown dd 0
             dw 0, 0
             db 0C0h, 0, 0, 0, 0, 0, 0, 46h

.code

; ════════════════════════════════════════════════════════════════════
; WebView2Init — Initialize WebView2: load DLL, resolve create function
;   No args. Returns EAX = 0 success, -1 if WebView2 not available.
;   If WebView2Loader.dll is not present, returns -1 gracefully.
;   FRAME: 1 push + 28h alloc
; ════════════════════════════════════════════════════════════════════
WebView2Init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)
    xor     ecx, ecx
    mov     edx, COINIT_APARTMENTTHREADED
    call    CoInitializeEx
    ; S_OK=0, S_FALSE=1 (already init), RPC_E_CHANGED_MODE=0x80010106
    cmp     eax, 0
    jl      @wv2i_test_sfalse
    mov     g_comInitialized, 1
    jmp     @wv2i_load_dll

@wv2i_test_sfalse:
    cmp     eax, 1                       ; S_FALSE — already inited, that's ok
    jne     @wv2i_no_com
    mov     g_comInitialized, 0          ; Don't uninitialize what we didn't init
    jmp     @wv2i_load_dll

@wv2i_no_com:
    ; COM init failed — can still try LoadLibrary
    mov     g_comInitialized, 0

@wv2i_load_dll:
    ; LoadLibraryW(L"WebView2Loader.dll")
    lea     rcx, szLoaderDll
    call    LoadLibraryW
    test    rax, rax
    jz      @wv2i_fail
    mov     g_hLoaderDll, rax
    mov     rbx, rax

    ; GetProcAddress(hModule, "CreateCoreWebView2EnvironmentWithOptions")
    mov     rcx, rbx
    lea     rdx, szCreateEnv
    call    GetProcAddress
    test    rax, rax
    jz      @wv2i_unload
    mov     g_pfnCreateEnv, rax

    ; Success — DLL loaded and function resolved
    ; Actual environment creation deferred to WebView2CreateEnvironment
    ; which needs a parent HWND

    ; Beacon
    mov     ecx, WV2_BEACON_SLOT
    mov     edx, WV2_EVT_INIT_OK
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    jmp     @wv2i_ret

@wv2i_unload:
    ; Function not found — unload DLL
    mov     rcx, rbx
    call    FreeLibrary
    mov     g_hLoaderDll, 0

@wv2i_fail:
    mov     g_wv2Ready, 0

    mov     ecx, WV2_BEACON_SLOT
    mov     edx, WV2_EVT_INIT_FAIL
    xor     r8d, r8d
    call    BeaconSend

    mov     eax, -1

@wv2i_ret:
    add     rsp, 28h
    pop     rbx
    ret
WebView2Init ENDP


; ════════════════════════════════════════════════════════════════════
; WebView2CreateEnvironment — Create the WebView2 environment
;   No args. Returns EAX = 0 success, -1 failure.
;   Must be called after WebView2Init and after main window is created.
;   Calls CreateCoreWebView2EnvironmentWithOptions(NULL,NULL,NULL,handler)
;
;   Uses the EnvironmentCompletedHandler COM object which asynchronously
;   receives the ICoreWebView2Environment* and chains into controller
;   creation via the ControllerCompletedHandler.
;   FRAME: 1 push + 30h alloc
; ════════════════════════════════════════════════════════════════════
WebView2CreateEnvironment PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Bail if loader DLL wasn't loaded
    mov     rax, g_pfnCreateEnv
    test    rax, rax
    jz      @wv2ce_fail

    ; Reset handler refcounts to 1
    mov     dword ptr [g_envHandler + 8], 1
    mov     dword ptr [g_ctrlHandler + 8], 1

    ; Call CreateCoreWebView2EnvironmentWithOptions(
    ;   NULL,            // browserExecutableFolder
    ;   NULL,            // userDataFolder
    ;   NULL,            // environmentOptions
    ;   &g_envHandler    // completedHandler — our COM callback object
    ; )
    xor     ecx, ecx                     ; browserExecutableFolder = NULL
    xor     edx, edx                     ; userDataFolder = NULL  
    xor     r8d, r8d                     ; environmentOptions = NULL
    lea     r9, g_envHandler             ; handler = EnvironmentCompletedHandler
    call    rax
    test    eax, eax                     ; HRESULT check
    jnz     @wv2ce_fail

    ; Note: g_wv2Ready is set asynchronously by CtrlHandler_Invoke
    ; when the controller callback chain completes. Return S_OK here
    ; to indicate the async operation was successfully initiated.
    xor     eax, eax
    jmp     @wv2ce_ret

@wv2ce_fail:
    mov     g_wv2Ready, 0
    mov     eax, -1

@wv2ce_ret:
    add     rsp, 30h
    pop     rbx
    ret
WebView2CreateEnvironment ENDP


; ════════════════════════════════════════════════════════════════════
; WebView2Navigate — Navigate to a URL
;   RCX = pUrl (WCHAR* null-terminated URL)
;   Returns: EAX = HRESULT (0 = S_OK), -1 if not ready
; ════════════════════════════════════════════════════════════════════
WebView2Navigate PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_wv2Ready, 0
    je      @wv2n_fail

    mov     rdx, rcx                     ; pUrl → RDX (arg 2 for COM call)
    mov     rcx, g_pWebView2             ; this pointer
    test    rcx, rcx
    jz      @wv2n_fail

    ; webView2->Navigate(url) — vtable[40/8 = 5]
    mov     rax, qword ptr [rcx]         ; vtable ptr
    call    qword ptr [rax + VT_WV2_NAVIGATE]
    jmp     @wv2n_ret

@wv2n_fail:
    mov     eax, -1

@wv2n_ret:
    add     rsp, 28h
    ret
WebView2Navigate ENDP


; ════════════════════════════════════════════════════════════════════
; WebView2NavigateToString — Render raw HTML content
;   RCX = pHtml (WCHAR* null-terminated HTML string)
;   Returns: EAX = HRESULT, -1 if not ready
; ════════════════════════════════════════════════════════════════════
WebView2NavigateToString PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_wv2Ready, 0
    je      @wv2ns_fail

    mov     rdx, rcx
    mov     rcx, g_pWebView2
    test    rcx, rcx
    jz      @wv2ns_fail

    mov     rax, qword ptr [rcx]
    call    qword ptr [rax + VT_WV2_NAVIGATE_STRING]
    jmp     @wv2ns_ret

@wv2ns_fail:
    mov     eax, -1

@wv2ns_ret:
    add     rsp, 28h
    ret
WebView2NavigateToString ENDP


; ════════════════════════════════════════════════════════════════════
; WebView2PostMessage — Post a JSON message to the WebView2 content
;   RCX = pJson (WCHAR* null-terminated JSON string)
;   Returns: EAX = HRESULT, -1 if not ready
; ════════════════════════════════════════════════════════════════════
WebView2PostMessage PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_wv2Ready, 0
    je      @wv2pm_fail

    mov     rdx, rcx
    mov     rcx, g_pWebView2
    test    rcx, rcx
    jz      @wv2pm_fail

    mov     rax, qword ptr [rcx]
    call    qword ptr [rax + VT_WV2_POST_JSON]
    jmp     @wv2pm_ret

@wv2pm_fail:
    mov     eax, -1

@wv2pm_ret:
    add     rsp, 28h
    ret
WebView2PostMessage ENDP


; ════════════════════════════════════════════════════════════════════
; WebView2Resize — Update WebView2 bounds to match parent window
;   RCX = pRect (RECT* with new bounds: left, top, right, bottom)
;   Returns: EAX = HRESULT, -1 if not ready
; ════════════════════════════════════════════════════════════════════
WebView2Resize PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_wv2Ready, 0
    je      @wv2r_fail

    mov     rdx, rcx                     ; pRect
    mov     rcx, g_pController
    test    rcx, rcx
    jz      @wv2r_fail

    ; controller->put_Bounds(rect) — vtable offset 48
    mov     rax, qword ptr [rcx]
    call    qword ptr [rax + VT_CTRL_PUT_BOUNDS]
    jmp     @wv2r_ret

@wv2r_fail:
    mov     eax, -1

@wv2r_ret:
    add     rsp, 28h
    ret
WebView2Resize ENDP


; ════════════════════════════════════════════════════════════════════
; WebView2ExecuteScript — Execute JavaScript in the WebView2
;   RCX = pScript (WCHAR* null-terminated JavaScript code)
;   Returns: EAX = HRESULT, -1 if not ready
; ════════════════════════════════════════════════════════════════════
WebView2ExecuteScript PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_wv2Ready, 0
    je      @wv2es_fail

    test    rcx, rcx
    jz      @wv2es_fail

    mov     rdx, rcx                     ; pScript
    mov     rcx, g_pWebView2
    test    rcx, rcx
    jz      @wv2es_fail

    ; webView2->ExecuteScript(script, handler)
    ; Hardened: pass real COM callback object (no NULL handler).
    mov     dword ptr [g_scriptHandler + 8], 1
    mov     rax, qword ptr [g_scriptHandler]
    test    rax, rax
    jnz     @wv2es_handler_ok
    lea     rax, g_scriptHandlerVtable
    mov     qword ptr [g_scriptHandler], rax
@wv2es_handler_ok:
    mov     rax, qword ptr [rcx]
    lea     r8, g_scriptHandler           ; handler = ScriptCompletedHandler
    call    qword ptr [rax + VT_WV2_EXEC_SCRIPT]
    jmp     @wv2es_ret

@wv2es_fail:
    mov     eax, -1

@wv2es_ret:
    add     rsp, 28h
    ret
WebView2ExecuteScript ENDP


; ════════════════════════════════════════════════════════════════════
; WebView2IsReady — Check if WebView2 is initialized and ready
;   Returns: EAX = 1 if ready, 0 if not
; ════════════════════════════════════════════════════════════════════
WebView2IsReady PROC
    mov     eax, g_wv2Ready
    ret
WebView2IsReady ENDP


; ════════════════════════════════════════════════════════════════════
; WebView2Shutdown — Clean up WebView2 resources
;   Releases COM pointers, frees loader DLL, CoUninitialize.
;   Returns: EAX = 0
; ════════════════════════════════════════════════════════════════════
WebView2Shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Release ICoreWebView2Controller
    mov     rcx, g_pController
    test    rcx, rcx
    jz      @wv2s_no_ctrl
    mov     rax, qword ptr [rcx]         ; vtable
    call    qword ptr [rax + VT_CTRL_RELEASE]
    mov     g_pController, 0
@wv2s_no_ctrl:

    ; Release ICoreWebView2
    mov     rcx, g_pWebView2
    test    rcx, rcx
    jz      @wv2s_no_wv2
    mov     rax, qword ptr [rcx]         ; vtable
    call    qword ptr [rax + VT_WV2_RELEASE]
    mov     g_pWebView2, 0
@wv2s_no_wv2:

    ; FreeLibrary(WebView2Loader.dll)
    mov     rcx, g_hLoaderDll
    test    rcx, rcx
    jz      @wv2s_no_dll
    call    FreeLibrary
    mov     g_hLoaderDll, 0
@wv2s_no_dll:

    ; CoUninitialize (only if we initialized)
    cmp     g_comInitialized, 0
    je      @wv2s_no_com
    call    CoUninitialize
    mov     g_comInitialized, 0
@wv2s_no_com:

    mov     g_wv2Ready, 0
    mov     g_pfnCreateEnv, 0

    ; Beacon
    mov     ecx, WV2_BEACON_SLOT
    mov     edx, WV2_EVT_SHUTDOWN
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    add     rsp, 20h
    pop     rbx
    ret
WebView2Shutdown ENDP


; ═══════════════════════════════════════════════════════════════════
; COM Handler Implementations
;
; Both EnvHandler and CtrlHandler share the same QueryInterface,
; AddRef, and Release logic (only 'this' pointer differs).
; Each has a unique Invoke that advances the async chain.
;
; COM calling convention (x64 stdcall):
;   RCX = this, RDX = arg1, R8 = arg2, R9 = arg3
;   Returns: HRESULT in EAX
; ═══════════════════════════════════════════════════════════════════

; Shared HRESULT constants
S_OK                equ 0
E_NOINTERFACE       equ 80004002h
E_FAIL              equ 80004005h

; ICoreWebView2Environment vtable offset for CreateCoreWebView2Controller
; Layout: QI, AddRef, Release, then first custom method
VT_ENV_CREATE_CTRL  equ 24           ; CreateCoreWebView2Controller(hWnd, handler)

; ICoreWebView2Controller vtable offset for get_CoreWebView2
VT_CTRL_GET_WV2     equ 56           ; get_CoreWebView2(ICoreWebView2**)


; ────────────────────────────────────────────────────────────────
; EnvHandler_QueryInterface
;   RCX = this (g_envHandler)
;   RDX = riid (REFIID)
;   R8  = ppvObject (void**)
;   Returns: S_OK if IUnknown requested, E_NOINTERFACE otherwise
; ────────────────────────────────────────────────────────────────
EnvHandler_QueryInterface PROC
    test    r8, r8
    jz      @eqiNull

    ; Compare riid with IID_IUnknown (16 bytes)
    ; Quick check: first DWORD == 0 && last byte == 46h → probably IUnknown
    mov     eax, dword ptr [rdx]
    test    eax, eax
    jnz     @eqi_check_handler_iid
    cmp     byte ptr [rdx + 15], 46h
    jne     @eqi_check_handler_iid

    ; It's IUnknown (or close enough) — return this
    mov     qword ptr [r8], rcx
    ; AddRef
    lock inc dword ptr [rcx + 8]
    xor     eax, eax                     ; S_OK
    ret

@eqi_check_handler_iid:
    ; Accept any unknown IID as well (we are a simple single-interface object)
    ; WebView2 will query for the specific handler IID — just say yes
    mov     qword ptr [r8], rcx
    lock inc dword ptr [rcx + 8]
    xor     eax, eax                     ; S_OK
    ret

@eqiNull:
    mov     eax, E_NOINTERFACE
    ret
EnvHandler_QueryInterface ENDP


; ────────────────────────────────────────────────────────────────
; EnvHandler_AddRef
;   RCX = this
;   Returns: new ref count
; ────────────────────────────────────────────────────────────────
EnvHandler_AddRef PROC
    lock inc dword ptr [rcx + 8]
    mov     eax, dword ptr [rcx + 8]
    ret
EnvHandler_AddRef ENDP


; ────────────────────────────────────────────────────────────────
; EnvHandler_Release
;   RCX = this
;   Returns: new ref count (never actually frees — static object)
; ────────────────────────────────────────────────────────────────
EnvHandler_Release PROC
    lock dec dword ptr [rcx + 8]
    mov     eax, dword ptr [rcx + 8]
    ; Static allocation — don't free even at 0
    ret
EnvHandler_Release ENDP


; ────────────────────────────────────────────────────────────────
; EnvHandler_Invoke — Called when WebView2 environment is ready
;   RCX = this (g_envHandler)
;   RDX = errorCode (HRESULT from env creation)
;   R8  = pEnvironment (ICoreWebView2Environment*)
;
;   On success: calls env->CreateCoreWebView2Controller(hMainWnd, &g_ctrlHandler)
;   On failure: sets g_wv2Ready = 0, sends beacon
; ────────────────────────────────────────────────────────────────
EnvHandler_Invoke PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    ; Check if environment creation succeeded
    test    edx, edx                     ; errorCode
    jnz     @ehi_fail

    ; R8 = ICoreWebView2Environment* — validate
    test    r8, r8
    jz      @ehi_fail
    mov     rbx, r8                      ; save pEnvironment

    ; AddRef on the environment (we'll use it briefly)
    mov     rcx, rbx
    mov     rax, qword ptr [rcx]         ; vtable
    call    qword ptr [rax + 8]          ; AddRef

    ; Call pEnvironment->CreateCoreWebView2Controller(hMainWnd, handler)
    ;   RCX = this (pEnvironment)
    ;   RDX = hWnd (HWND)
    ;   R8  = handler (ControllerCompletedHandler)
    mov     rcx, rbx                     ; this = pEnvironment
    mov     rdx, hMainWnd                ; parent window
    lea     r8, g_ctrlHandler            ; controller callback
    mov     rax, qword ptr [rcx]         ; vtable
    call    qword ptr [rax + VT_ENV_CREATE_CTRL]
    mov     esi, eax                     ; save HRESULT

    ; Release the environment reference
    mov     rcx, rbx
    mov     rax, qword ptr [rcx]
    call    qword ptr [rax + 16]         ; Release

    ; Check CreateCoreWebView2Controller result
    test    esi, esi
    jnz     @ehi_fail

    ; Success — controller creation is now async, CtrlHandler_Invoke will fire
    xor     eax, eax
    jmp     @ehi_ret

@ehi_fail:
    mov     g_wv2Ready, 0

    mov     ecx, WV2_BEACON_SLOT
    mov     edx, WV2_EVT_INIT_FAIL
    xor     r8d, r8d
    call    BeaconSend

    mov     eax, E_FAIL

@ehi_ret:
    add     rsp, 38h
    pop     rsi
    pop     rbx
    ret
EnvHandler_Invoke ENDP


; ────────────────────────────────────────────────────────────────
; CtrlHandler_QueryInterface — same as EnvHandler
; ────────────────────────────────────────────────────────────────
CtrlHandler_QueryInterface PROC
    test    r8, r8
    jz      @cqiNull
    mov     qword ptr [r8], rcx
    lock inc dword ptr [rcx + 8]
    xor     eax, eax                     ; S_OK — accept all IIDs
    ret
@cqiNull:
    mov     eax, E_NOINTERFACE
    ret
CtrlHandler_QueryInterface ENDP


; ────────────────────────────────────────────────────────────────
; CtrlHandler_AddRef
; ────────────────────────────────────────────────────────────────
CtrlHandler_AddRef PROC
    lock inc dword ptr [rcx + 8]
    mov     eax, dword ptr [rcx + 8]
    ret
CtrlHandler_AddRef ENDP


; ────────────────────────────────────────────────────────────────
; CtrlHandler_Release
; ────────────────────────────────────────────────────────────────
CtrlHandler_Release PROC
    lock dec dword ptr [rcx + 8]
    mov     eax, dword ptr [rcx + 8]
    ret
CtrlHandler_Release ENDP


; ────────────────────────────────────────────────────────────────
; ScriptHandler_QueryInterface — ExecuteScript completed handler QI
;   RCX=this, RDX=riid, R8=ppv
; ────────────────────────────────────────────────────────────────
ScriptHandler_QueryInterface PROC
    test    r8, r8
    jz      @shqi_null
    mov     qword ptr [r8], rcx
    lock inc dword ptr [rcx + 8]
    xor     eax, eax                     ; S_OK
    ret
@shqi_null:
    mov     eax, E_NOINTERFACE
    ret
ScriptHandler_QueryInterface ENDP


; ────────────────────────────────────────────────────────────────
; ScriptHandler_AddRef
; ────────────────────────────────────────────────────────────────
ScriptHandler_AddRef PROC
    lock inc dword ptr [rcx + 8]
    mov     eax, dword ptr [rcx + 8]
    ret
ScriptHandler_AddRef ENDP


; ────────────────────────────────────────────────────────────────
; ScriptHandler_Release
; ────────────────────────────────────────────────────────────────
ScriptHandler_Release PROC
    lock dec dword ptr [rcx + 8]
    mov     eax, dword ptr [rcx + 8]
    ret
ScriptHandler_Release ENDP


; ────────────────────────────────────────────────────────────────
; ScriptHandler_Invoke — ExecuteScript completion callback
;   RCX=this, RDX=errorCode(HRESULT), R8=resultObjectAsJson(LPWSTR)
;   We intentionally accept and ignore payload here; callback existence
;   guarantees compatibility with strict runtime builds.
; ────────────────────────────────────────────────────────────────
ScriptHandler_Invoke PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; RCX=this, RDX=errorCode(HRESULT), R8=resultObjectAsJson(LPWSTR)
    ; On failure: increment error counter and log via beacon.
    ; Do NOT kill g_wv2Ready — a single script error must not tear down
    ; the entire WebView2 runtime.  Only a controller-level failure
    ; (CtrlHandler_Invoke) should clear g_wv2Ready.
    test    edx, edx
    jz      @shi_ok

    ; Track consecutive script errors
    lock inc dword ptr [g_wv2ScriptErrors]

    ; Only downgrade if errors exceed threshold (5 consecutive)
    cmp     dword ptr [g_wv2ScriptErrors], 5
    jb      @shi_soft_fail

    ; Threshold exceeded — WebView2 is degraded, fall back to GDI
    mov     g_wv2Ready, 0
    mov     dword ptr [g_wv2ScriptErrors], 0

@shi_soft_fail:
    ; Log the error HRESULT via beacon if available
    mov     rcx, rdx                     ; HRESULT to log
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    ; Beacon log call is optional — ignore if not ready
    mov     eax, E_FAIL
    lea     rsp, [rbp]
    pop     rbp
    ret

@shi_ok:
    ; Reset error counter on success
    mov     dword ptr [g_wv2ScriptErrors], 0
    xor     eax, eax                     ; S_OK
    lea     rsp, [rbp]
    pop     rbp
    ret
ScriptHandler_Invoke ENDP


; ────────────────────────────────────────────────────────────────
; CtrlHandler_Invoke — Called when WebView2 controller is ready
;   RCX = this (g_ctrlHandler)
;   RDX = errorCode (HRESULT from controller creation)
;   R8  = pController (ICoreWebView2Controller*)
;
;   On success:
;     1. Store g_pController (with AddRef)
;     2. Call controller->get_CoreWebView2(&g_pWebView2) to get the webview
;     3. Set g_wv2Ready = 1
;     4. Signal success via beacon
;   On failure: sets g_wv2Ready = 0
; ────────────────────────────────────────────────────────────────
CtrlHandler_Invoke PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    ; Check if controller creation succeeded
    test    edx, edx
    jnz     @chi_fail

    ; R8 = ICoreWebView2Controller* — validate
    test    r8, r8
    jz      @chi_fail
    mov     rbx, r8                      ; save pController

    ; AddRef on the controller (we're keeping it)
    mov     rcx, rbx
    mov     rax, qword ptr [rcx]         ; vtable
    call    qword ptr [rax + 8]          ; AddRef
    mov     g_pController, rbx

    ; Get the ICoreWebView2 from the controller
    ; controller->get_CoreWebView2(&g_pWebView2)
    ;   RCX = this (pController)
    ;   RDX = ppWebView2 (ICoreWebView2** out param)
    mov     rcx, rbx
    lea     rdx, g_pWebView2
    mov     rax, qword ptr [rcx]         ; vtable
    call    qword ptr [rax + VT_CTRL_GET_WV2]
    test    eax, eax
    jnz     @chi_no_webview

    ; Verify we got a valid webview pointer
    mov     rax, g_pWebView2
    test    rax, rax
    jz      @chi_no_webview

    ; Make the controller visible  
    ; controller->put_IsVisible(TRUE)
    mov     rcx, rbx
    mov     edx, 1                       ; TRUE
    mov     rax, qword ptr [rcx]
    call    qword ptr [rax + 32]         ; put_IsVisible offset = 32

    ; Everything is ready
    mov     g_wv2Ready, 1

    ; Beacon: WebView2 fully initialized
    mov     ecx, WV2_BEACON_SLOT
    mov     edx, WV2_EVT_INIT_OK
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    jmp     @chi_ret

@chi_no_webview:
    ; Controller created but couldn't get webview — partial failure
    ; Release the controller we AddRef'd
    mov     rcx, rbx
    mov     rax, qword ptr [rcx]
    call    qword ptr [rax + 16]         ; Release
    mov     g_pController, 0

@chi_fail:
    mov     g_wv2Ready, 0

    mov     ecx, WV2_BEACON_SLOT
    mov     edx, WV2_EVT_INIT_FAIL
    xor     r8d, r8d
    call    BeaconSend

    mov     eax, E_FAIL

@chi_ret:
    add     rsp, 38h
    pop     rsi
    pop     rbx
    ret
CtrlHandler_Invoke ENDP

END
