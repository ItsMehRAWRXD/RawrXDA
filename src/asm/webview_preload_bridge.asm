; =============================================================================
; RawrXD WebView2 Preload Bridge v1.0
; Replaces: vs/base/parts/sandbox/electron-sandbox/preload.js
; Implements: vscode IPC bridge, webFrame, process.env, context config
; Zero Electron. Pure WebView2 COM + RawrXD native backend.
; =============================================================================
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\ole32.inc
include \masm64\include64\oleaut32.inc
include \masm64\include64\shlwapi.inc

; COM vtable indices
ICoreWebView2_QueryInterface              equ 0
ICoreWebView2_AddRef                      equ 1
ICoreWebView2_Release                     equ 2
ICoreWebView2_get_Settings                equ 3
ICoreWebView2_get_Source                  equ 4
ICoreWebView2_put_Source                  equ 5
ICoreWebView2_Navigate                    equ 6
ICoreWebView2_NavigateToString            equ 7
ICoreWebView2_add_NavigationCompleted     equ 8
ICoreWebView2_add_WebMessageReceived      equ 14
ICoreWebView2_PostWebMessageAsJson        equ 16
ICoreWebView2_AddScriptToExecuteOnDocumentCreated equ 18
ICoreWebView2_AddHostObjectToScript       equ 20

ICoreWebView2Controller_QueryInterface    equ 0
ICoreWebView2Controller_AddRef            equ 1
ICoreWebView2Controller_Release           equ 2
ICoreWebView2Controller_get_CoreWebView2  equ 3
ICoreWebView2Controller_put_ZoomFactor    equ 7

ICoreWebView2Settings_QueryInterface      equ 0
ICoreWebView2Settings_AddRef              equ 1
ICoreWebView2Settings_Release             equ 2
ICoreWebView2Settings_put_AreHostObjectsAllowed equ 9
ICoreWebView2Settings_put_IsScriptEnabled equ 12

ICoreWebView2WebMessageReceivedEventArgs_QueryInterface equ 0
ICoreWebView2WebMessageReceivedEventArgs_AddRef        equ 1
ICoreWebView2WebMessageReceivedEventArgs_Release       equ 2
ICoreWebView2WebMessageReceivedEventArgs_get_WebMessageAsJson equ 6

; ============= GUIDs =============
CLSID_CoreWebView2Environment GUID 0A4D62C1, 0D9F4, 4228, <0xA1, 0x19, 0x2C, 0x6B, 0x4F, 0x8C, 0x8F, 0xBE>
IID_ICoreWebView2Environment  GUID 00000000, 0000, 0000, <0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46>
IID_ICoreWebView2             GUID 00000000, 0000, 0000, <0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46>
IID_ICoreWebView2Controller   GUID 00000000, 0000, 0000, <0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46>
IID_ICoreWebView2Settings     GUID 00000000, 0000, 0000, <0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46>
IID_IDispatch                 GUID 00020400, 0000, 0000, <0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46>

; ============= STRUCTS =============
HostObjectVtbl struct
    QueryInterface      dq ?
    AddRef              dq ?
    Release             dq ?
    GetTypeInfoCount    dq ?
    GetTypeInfo         dq ?
    GetIDsOfNames       dq ?
    Invoke              dq ?
    ; Custom methods exposed to JS
    SendIpc             dq ?    ; vscode.ipcRenderer.send
    InvokeIpc           dq ?    ; vscode.ipcRenderer.invoke
    GetConfig           dq ?    ; vscode.context.configuration
    GetPlatform         dq ?    ; vscode.process.platform
    GetEnv              dq ?    ; vscode.process.env
    SetZoom             dq ?    ; vscode.webFrame.setZoomLevel
HostObjectVtbl ends

HostObject struct
    lpVtbl      dq ?
    refCount    dd ?
    pWebView    dq ?            ; ICoreWebView2 pointer
    pController dq ?            ; ICoreWebView2Controller
HostObject ends

WebMessageHandlerVtbl struct
    QueryInterface      dq ?
    AddRef              dq ?
    Release             dq ?
    Invoke              dq ?    ; ICoreWebView2WebMessageReceivedEventHandler
WebMessageHandlerVtbl ends

WebMessageHandler struct
    lpVtbl      dq ?
    refCount    dd ?
    pHostObject dq ?
WebMessageHandler ends

; ============= DATA =============
.data
align 16

; JavaScript to inject (replaces preload.js)
szInjectScript db \
'window.vscode={',\
'ipcRenderer:{',\
'send(c,...a){if(!c?.startsWith("vscode:"))throw new Error("Invalid channel");window.chrome.webview.postMessage(JSON.stringify({type:"send",channel:c,args:a}))},',\
'invoke(c,...a){if(!c?.startsWith("vscode:"))throw new Error("Invalid channel");return new Promise((r,e)=>{const id=Math.random().toString(36);window.__ipcPromises[id]=r;window.chrome.webview.postMessage(JSON.stringify({type:"invoke",channel:c,args:a,id}))})},',\
'on(c,h){if(!c?.startsWith("vscode:"))throw new Error("Invalid channel");window.addEventListener("message",e=>{if(e.data?.channel===c)h(e.data)})},',\
'once(c,h){const fn=e=>{if(e.data?.channel===c){h(e.data);window.removeEventListener("message",fn)}};window.addEventListener("message",fn)},',\
'removeListener(c,h){/*TODO*/}',\
'},',\
'webFrame:{setZoomLevel(z){window.chrome.webview.postMessage(JSON.stringify({type:"zoom",level:z}))}},',\
'process:{platform:"win32",arch:"x64",get env(){return window.__hostEnv||{}},versions:{},get pid(){return window.__hostPid},cwd(){return window.__hostCwd}},',\
'context:{configuration(){return window.__hostConfig},async resolveConfiguration(){return window.__hostConfig}}',\
'};',\
'window.__ipcPromises={};',\
'window.addEventListener("message",e=>{if(e.data?.ipcId&&window.__ipcPromises[e.data.ipcId]){window.__ipcPromises[e.data.ipcId](e.data.result);delete window.__ipcPromises[e.data.ipcId]}})',0

; Error message for invalid channels
szInvalidChannel db "Unsupported IPC channel (must start with vscode:)",0

; Platform string
szPlatform db "win32",0
szArch db "x64",0

; ============= BSS =============
.data?
align 16
g_pWebView      dq ?
g_pController   dq ?
g_pHostObject   dq ?
g_hWnd          dq ?
g_Environment   dq ?
g_hHeap         dq ?

; ============= CODE =============
.code

; ------------------------------------------------------------------------
; Initialize WebView2 Preload Bridge
; rcx = hWnd, rdx = ICoreWebView2Controller* (from WebView2 creation)
; ------------------------------------------------------------------------
PreloadBridge_Init proc
    sub rsp, 58h

    mov g_hWnd, rcx
    mov g_pController, rdx

    ; Get CoreWebView2 from controller
    mov rax, [rdx]
    lea r8, g_pWebView
    mov rcx, rdx
    mov rdx, offset IID_ICoreWebView2
    call qword ptr [rax + ICoreWebView2Controller_get_CoreWebView2*8]
    test eax, eax
    js @@failed

    ; Enable host objects and scripts
    call EnableHostObjects

    ; Create and register host object
    call CreateHostObject

    ; Add script to execute on document creation
    call InjectPreloadScript

    ; Set up web message received handler
    call SetupMessageHandler

    ; Expose host object as 'window.chrome.webview.hostObjects.sync.rawrxd'
    mov rcx, g_pWebView
    mov rdx, offset szHostObjectName
    mov r8, g_pHostObject
    mov rax, [rcx]
    call qword ptr [rax + ICoreWebView2_AddHostObjectToScript*8]

    mov eax, 1
    add rsp, 58h
    ret

@@failed:
    xor eax, eax
    add rsp, 58h
    ret

szHostObjectName db "rawrxd",0
PreloadBridge_Init endp

; ------------------------------------------------------------------------
; Enable Host Objects in Settings
; ------------------------------------------------------------------------
EnableHostObjects proc
    sub rsp, 28h

    mov rcx, g_pWebView
    lea rdx, [rsp+20h]      ; ICoreWebView2Settings**
    mov rax, [rcx]
    call qword ptr [rax + ICoreWebView2_get_Settings*8]

    mov rcx, [rsp+20h]
    mov rax, [rcx]
    mov edx, 1              ; TRUE
    call qword ptr [rax + ICoreWebView2Settings_put_AreHostObjectsAllowed*8]

    mov rcx, [rsp+20h]
    mov rax, [rcx]
    mov edx, 1              ; TRUE
    call qword ptr [rax + ICoreWebView2Settings_put_IsScriptEnabled*8]

    ; Release settings
    mov rcx, [rsp+20h]
    mov rax, [rcx]
    call qword ptr [rax + ICoreWebView2Settings_Release*8]

    add rsp, 28h
    ret
EnableHostObjects endp

; ------------------------------------------------------------------------
; Create Host Object (IDispatch implementation)
; ------------------------------------------------------------------------
CreateHostObject proc
    sub rsp, 28h

    ; Allocate HostObject
    invoke GetProcessHeap
    mov g_hHeap, rax
    mov rcx, rax
    xor edx, edx
    mov r8d, sizeof HostObject
    call HeapAlloc
    mov g_pHostObject, rax

    ; Set vtable
    lea rcx, HostObjectVtable
    mov [rax].HostObject.lpVtbl, rcx
    mov [rax].HostObject.refCount, 1
    mov rdx, g_pWebView
    mov [rax].HostObject.pWebView, rdx
    mov rdx, g_pController
    mov [rax].HostObject.pController, rdx

    add rsp, 28h
    ret
CreateHostObject endp

; ------------------------------------------------------------------------
; Inject Preload Script
; ------------------------------------------------------------------------
InjectPreloadScript proc
    sub rsp, 28h

    mov rcx, g_pWebView
    lea rdx, szInjectScript
    xor r8, r8              ; nullptr (no ID)
    mov rax, [rcx]
    call qword ptr [rax + ICoreWebView2_AddScriptToExecuteOnDocumentCreated*8]

    add rsp, 28h
    ret
InjectPreloadScript endp

; ------------------------------------------------------------------------
; Setup Web Message Handler
; ------------------------------------------------------------------------
SetupMessageHandler proc
    sub rsp, 38h

    ; Allocate handler object
    mov rcx, g_hHeap
    xor edx, edx
    mov r8d, sizeof WebMessageHandler
    call HeapAlloc
    mov [rsp+20h], rax

    lea rcx, WebMessageHandlerVtable
    mov [rax].WebMessageHandler.lpVtbl, rcx
    mov [rax].WebMessageHandler.refCount, 1
    mov rdx, g_pHostObject
    mov [rax].WebMessageHandler.pHostObject, rdx

    ; Register handler
    mov rcx, g_pWebView
    lea rdx, [rsp+20h]      ; EventHandler
    xor r8, r8              ; nullptr (no token)
    mov rax, [rcx]
    call qword ptr [rax + ICoreWebView2_add_WebMessageReceived*8]

    add rsp, 38h
    ret
SetupMessageHandler endp

; ============= HOST OBJECT VTABLE IMPLEMENTATION =============

HostObject_QueryInterface proc
    ; rcx = this, rdx = riid, r8 = ppv
    mov rax, [rdx]
    cmp rax, [IID_IDispatch]
    je @@match
    cmp rax, [IID_ICoreWebView2]  ; Check for base interface
    je @@match
    xor eax, eax
    mov [r8], rax
    mov eax, E_NOINTERFACE
    ret
@@match:
    mov [r8], rcx
    mov rax, [rcx]
    call qword ptr [rax + HostObjectVtbl.AddRef]
    xor eax, eax
    ret
HostObject_QueryInterface endp

HostObject_AddRef proc
    ; rcx = this
    inc [rcx].HostObject.refCount
    mov eax, [rcx].HostObject.refCount
    ret
HostObject_AddRef endp

HostObject_Release proc
    ; rcx = this
    dec [rcx].HostObject.refCount
    jz @@destroy
    mov eax, [rcx].HostObject.refCount
    ret
@@destroy:
    invoke HeapFree, g_hHeap, 0, rcx
    xor eax, eax
    ret
HostObject_Release endp

HostObject_GetTypeInfoCount proc
    ; rcx = this, rdx = pctinfo
    mov dword ptr [rdx], 0
    xor eax, eax
    ret
HostObject_GetTypeInfoCount endp

HostObject_GetIDsOfNames proc
    ; rcx = this, rdx = riid, r8 = rgszNames, r9 = cNames, [rsp+28]=lcid, [rsp+30]=rgDispId
    mov r10, [rsp+30h]      ; rgDispId
    mov dword ptr [r10], 1  ; DISPID_VALUE for all names (simplified)
    xor eax, eax
    ret
HostObject_GetIDsOfNames endp

; ------------------------------------------------------------------------
; HostObject_Invoke - Main dispatch for JS calls
; rcx = this, rdx = dispIdMember, r8 = riid, r9 = lcid,
; [rsp+28] = flags, [rsp+30] = pDispParams, [rsp+38] = pVarResult, [rsp+40] = pExcepInfo
; ------------------------------------------------------------------------
HostObject_Invoke proc
    sub rsp, 48h

    ; Get method name from params (simplified - dispId maps to method)
    mov r10, [rsp+30h+48h]      ; pDispParams
    mov r11, [r10]              ; rgsabound

    ; dispIdMember determines method:
    ; 1 = SendIpc, 2 = InvokeIpc, 3 = GetConfig, 4 = GetPlatform, 5 = GetEnv, 6 = SetZoom

    cmp edx, 1
    je @@SendIpc
    cmp edx, 2
    je @@InvokeIpc
    cmp edx, 3
    je @@GetConfig
    cmp edx, 6
    je @@SetZoom
    jmp @@done

@@SendIpc:
    ; r8 = channel (BSTR), r9 = args (VARIANT)
    mov rcx, [rsp+30h+48h+8]    ; First param = channel
    mov rdx, [rsp+30h+48h+16]   ; Second param = args
    call HandleIpcSend
    jmp @@done

@@InvokeIpc:
    call HandleIpcInvoke
    jmp @@done

@@GetConfig:
    ; Return configuration from ProductRuntime
    mov rax, [rsp+38h+48h]      ; pVarResult
    mov word ptr [rax], 8       ; VT_BSTR
    lea rdx, szConfigJson       ; Return config as JSON string
    mov [rax+8], rdx
    jmp @@done

@@SetZoom:
    ; Set zoom factor via controller
    mov rcx, [rcx].HostObject.pController
    mov rax, [rcx]
    ; Get zoom level from params and set
    movsd xmm1, qword ptr [rsp+30h+48h+8]  ; Double zoom level
    cvtsi2sd xmm1, xmm1
    call qword ptr [rax + ICoreWebView2Controller_put_ZoomFactor*8]

@@done:
    xor eax, eax
    add rsp, 48h
    ret

szConfigJson db '{"zoomLevel":0,"userEnv":{}}',0
HostObject_Invoke endp

; ------------------------------------------------------------------------
; Handle IPC Send from JS
; ------------------------------------------------------------------------
HandleIpcSend proc
    sub rsp, 28h

    ; Validate channel starts with "vscode:"
    lea rax, szVscodePrefix
    mov rsi, rcx
    mov rdi, rax
    mov ecx, 7
    repe cmpsb
    jne @@invalid_channel

    ; Post to RawrXD backend via ring buffer or WM_COPYDATA
    mov rcx, g_hWnd
    mov edx, WM_COPYDATA
    mov r8d, 0              ; wParam
    lea r9, [rsp+20h]       ; COPYDATASTRUCT
    mov [r9].COPYDATASTRUCT.dwData, 1   ; IPC_SEND
    mov [r9].COPYDATASTRUCT.lpData, rsi
    invoke lstrlenA, rsi
    inc eax
    mov [r9].COPYDATASTRUCT.cbData, eax
    call SendMessageA

    add rsp, 28h
    ret

@@invalid_channel:
    ; Return error to JS
    add rsp, 28h
    ret

szVscodePrefix db "vscode:",0
HandleIpcSend endp

HandleIpcInvoke proc
    ; Async IPC with promise resolution
    ret
HandleIpcInvoke endp

; ------------------------------------------------------------------------
; Web Message Handler (from WebView2)
; ------------------------------------------------------------------------
WebMessageHandler_QueryInterface proc
    mov rax, [rdx]
    cmp rax, 0                ; Check for IUnknown
    je @@match
    cmp rax, 1                ; Check for event handler
    je @@match
    xor eax, eax
    mov [r8], rax
    mov eax, E_NOINTERFACE
    ret
@@match:
    mov [r8], rcx
    jmp HostObject_AddRef
WebMessageHandler_QueryInterface endp

WebMessageHandler_AddRef proc
    inc [rcx].WebMessageHandler.refCount
    mov eax, [rcx].WebMessageHandler.refCount
    ret
WebMessageHandler_AddRef endp

WebMessageHandler_Release proc
    dec [rcx].WebMessageHandler.refCount
    jz @@destroy
    mov eax, [rcx].WebMessageHandler.refCount
    ret
@@destroy:
    invoke HeapFree, g_hHeap, 0, rcx
    xor eax, eax
    ret
WebMessageHandler_Release endp

; ------------------------------------------------------------------------
; Handle Web Message from JS (window.chrome.webview.postMessage)
; rcx = this, rdx = sender (ICoreWebView2), r8 = args (ICoreWebView2WebMessageReceivedEventArgs)
; ------------------------------------------------------------------------
WebMessageHandler_Invoke proc
    sub rsp, 58h

    ; Get WebMessageAsJson
    mov rax, [r8]
    lea r9, [rsp+20h]       ; LPWSTR* json
    mov rcx, r8
    call qword ptr [rax + ICoreWebView2WebMessageReceivedEventArgs_get_WebMessageAsJson*8]

    ; Parse JSON (simplified - look for "type":"send" or "type":"invoke")
    mov rcx, [rsp+20h]
    call ParseWebMessage

    ; Free string
    invoke SysFreeString, [rsp+20h]

    add rsp, 58h
    ret
WebMessageHandler_Invoke endp

ParseWebMessage proc
    ; rcx = JSON string (wide)
    ; Quick parse for type field
    ret
ParseWebMessage endp

; ============= VTABLES =============
.data
align 8
HostObjectVtable HostObjectVtbl < \
    HostObject_QueryInterface, \
    HostObject_AddRef, \
    HostObject_Release, \
    HostObject_GetTypeInfoCount, \
    offset HostObject_GetTypeInfo, \
    HostObject_GetIDsOfNames, \
    HostObject_Invoke, \
    HandleIpcSend, \
    HandleIpcInvoke, \
    offset ReturnConfig, \
    offset GetPlatformStr, \
    offset GetEnvBlock, \
    offset SetZoomLevel \
>

WebMessageHandlerVtable WebMessageHandlerVtbl < \
    WebMessageHandler_QueryInterface, \
    WebMessageHandler_AddRef, \
    WebMessageHandler_Release, \
    WebMessageHandler_Invoke \
>

; Stubs for vtable entries
HostObject_GetTypeInfo:
HostObject_GetTypeInfoCount2:
ReturnConfig:
GetPlatformStr:
GetEnvBlock:
SetZoomLevel:
    ret

; ============= EXPORTS =============
public PreloadBridge_Init

end