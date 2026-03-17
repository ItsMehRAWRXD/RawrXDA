; pifabric_ui_model_browser.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc

includelib user32.lib

PUBLIC  UiModelBrowser_Init
PUBLIC  UiModelBrowser_Select
PUBLIC  UiModelBrowser_Show
PUBLIC  UiModelBrowser_Hide

modelBrowserHandle dq 0

.code

UiModelBrowser_Init PROC USES esi edi hParent:DWORD
    ; Create a listbox for model selection
    invoke CreateWindowEx, 0, "LISTBOX", NULL,
        WS_CHILD or WS_VISIBLE or LBS_STANDARD,
        0, 0, 200, 300,
        hParent, 0, NULL, NULL
    mov modelBrowserHandle, eax
    mov eax, 1
    ret
UiModelBrowser_Init ENDP

UiModelBrowser_Select PROC USES esi edi
    ; Stub for model selection logic
    mov eax, 1
    ret
UiModelBrowser_Select ENDP

UiModelBrowser_Show PROC
    mov eax, modelBrowserHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_SHOW
@done:
    xor eax, eax
    ret
UiModelBrowser_Show ENDP

UiModelBrowser_Hide PROC
    mov eax, modelBrowserHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_HIDE
@done:
    xor eax, eax
    ret
UiModelBrowser_Hide ENDP

END