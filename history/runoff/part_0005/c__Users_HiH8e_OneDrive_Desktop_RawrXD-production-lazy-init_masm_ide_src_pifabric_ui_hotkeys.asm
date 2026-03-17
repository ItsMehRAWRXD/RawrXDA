; pifabric_ui_hotkeys.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include kernel32.inc

includelib user32.lib
includelib kernel32.lib

PUBLIC UiHotkeys_Register
PUBLIC UiHotkeys_Unregister
PUBLIC UiHotkeys_Process

.data
HK_LAYOUT_DEFAULT EQU 1
HK_LAYOUT_SPLIT   EQU 2
HK_LAYOUT_FOCUS   EQU 3

.code

UiHotkeys_Register PROC hWnd:DWORD
    invoke RegisterHotKey,hWnd,HK_LAYOUT_DEFAULT,MOD_CONTROL,VK_F1
    invoke RegisterHotKey,hWnd,HK_LAYOUT_SPLIT,MOD_CONTROL,VK_F2
    invoke RegisterHotKey,hWnd,HK_LAYOUT_FOCUS,MOD_CONTROL,VK_F3
    mov eax,1
    ret
UiHotkeys_Register ENDP

UiHotkeys_Unregister PROC hWnd:DWORD
    invoke UnregisterHotKey,hWnd,HK_LAYOUT_DEFAULT
    invoke UnregisterHotKey,hWnd,HK_LAYOUT_SPLIT
    invoke UnregisterHotKey,hWnd,HK_LAYOUT_FOCUS
    mov eax,1
    ret
UiHotkeys_Unregister ENDP

UiHotkeys_Process PROC hWnd:DWORD,wParam:DWORD
    cmp wParam,HK_LAYOUT_DEFAULT
    je @default
    cmp wParam,HK_LAYOUT_SPLIT
    je @split
    cmp wParam,HK_LAYOUT_FOCUS
    je @focus
    jmp @done

@default:
    invoke PostMessage,hWnd,WM_UISHELL_LAYOUT_CHANGE,LAYOUT_DEFAULT,0
    jmp @done
@split:
    invoke PostMessage,hWnd,WM_UISHELL_LAYOUT_CHANGE,LAYOUT_SPLIT,0
    jmp @done
@focus:
    invoke PostMessage,hWnd,WM_UISHELL_LAYOUT_CHANGE,LAYOUT_FOCUS,0
    jmp @done

@done:
    ret
UiHotkeys_Process ENDP

END