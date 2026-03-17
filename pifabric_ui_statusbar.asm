; pifabric_ui_statusbar.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc

includelib user32.lib

PUBLIC  UiStatusBar_Init
PUBLIC  UiStatusBar_SetText
PUBLIC  UiStatusBar_Show
PUBLIC  UiStatusBar_Hide

; Handle to status bar window
statusBarHandle dq 0

.code

UiStatusBar_Init PROC USES esi edi hParent:DWORD
    ; Create a simple static text control as status bar
    invoke CreateWindowEx, 0, "STATIC", NULL,
        WS_CHILD or WS_VISIBLE or SS_SIMPLE,
        0, 0, 0, 0,
        hParent, 0, NULL, NULL
    mov statusBarHandle, eax
    mov eax, 1
    ret
UiStatusBar_Init ENDP

UiStatusBar_SetText PROC USES esi edi lpText:DWORD
    mov esi, statusBarHandle
    test esi, esi
    jz @done
    invoke SetWindowText, esi, lpText
@done:
    xor eax, eax
    ret
UiStatusBar_SetText ENDP

UiStatusBar_Show PROC
    mov eax, statusBarHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_SHOW
@done:
    xor eax, eax
    ret
UiStatusBar_Show ENDP

UiStatusBar_Hide PROC
    mov eax, statusBarHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_HIDE
@done:
    xor eax, eax
    ret
UiStatusBar_Hide ENDP

END
