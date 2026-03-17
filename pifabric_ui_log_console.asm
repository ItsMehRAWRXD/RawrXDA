; pifabric_ui_log_console.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc

includelib user32.lib

PUBLIC  UiLogConsole_Init
PUBLIC  UiLogConsole_Log
PUBLIC  UiLogConsole_Clear

logConsoleHandle dq 0

.code

UiLogConsole_Init PROC USES esi edi hParent:DWORD
    ; Create an edit control for logging
    invoke CreateWindowEx, 0, "EDIT", NULL,
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or ES_MULTILINE or ES_READONLY,
        0, 0, 400, 200,
        hParent, 0, NULL, NULL
    mov logConsoleHandle, eax
    mov eax, 1
    ret
UiLogConsole_Init ENDP

UiLogConsole_Log PROC USES esi edi lpText:DWORD
    mov esi, logConsoleHandle
    test esi, esi
    jz @done
    ; Append text to log console
    invoke SendMessage, esi, EM_SETSEL, -1, -1
    invoke SendMessage, esi, EM_REPLACESEL, 0, lpText
@done:
    xor eax, eax
    ret
UiLogConsole_Log ENDP

UiLogConsole_Clear PROC
    mov eax, logConsoleHandle
    test eax, eax
    jz @done
    invoke SetWindowText, eax, ""
@done:
    xor eax, eax
    ret
UiLogConsole_Clear ENDP

END