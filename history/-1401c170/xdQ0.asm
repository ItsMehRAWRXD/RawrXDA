; pifabric_ui_hotkeys.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc

includelib user32.lib

PUBLIC  UiHotkeys_Register
PUBLIC  UiHotkeys_Translate

.code

UiHotkeys_Register PROC USES esi edi
    ; Register hotkeys (stub)
    mov eax, 1
    ret
UiHotkeys_Register ENDP

UiHotkeys_Translate PROC USES esi edi hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    ; Translate hotkey messages (stub)
    xor eax, eax
    ret
UiHotkeys_Translate ENDP

END