; ==============================================================================
; RUNTIME_STUBS.ASM - Complete stub implementations for all missing symbols
; These are stdcall stubs that match the calling convention of main_complete.asm
; ==============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comdlg32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comdlg32.lib

.data
    szFilePath      db MAX_PATH dup(0)
    szAppTitle      db "RawrXD", 0
    szFindTitle     db "Find", 0
    szReplaceTitle  db "Replace", 0
    szFileMenu      db "&File", 0
    szEditMenu      db "&Edit", 0

.code

; ==============================================================================
; File Dialog Stubs (stdcall, called with parameters on stack)
; ==============================================================================

public _FileDialog_Open@4
_FileDialog_Open@4 proc hWnd:DWORD
    xor eax, eax
    ret
_FileDialog_Open@4 endp

public _FileDialog_SaveAs@4
_FileDialog_SaveAs@4 proc hWnd:DWORD
    xor eax, eax
    ret
_FileDialog_SaveAs@4 endp

public _ShowFindDialog@4
_ShowFindDialog@4 proc hWnd:DWORD
    invoke MessageBoxA, hWnd, offset szFindTitle, offset szAppTitle, MB_OK
    xor eax, eax
    ret
_ShowFindDialog@4 endp

public _ShowReplaceDialog@4
_ShowReplaceDialog@4 proc hWnd:DWORD
    invoke MessageBoxA, hWnd, offset szReplaceTitle, offset szAppTitle, MB_OK
    xor eax, eax
    ret
_ShowReplaceDialog@4 endp

; ==============================================================================
; Editor Stubs (stdcall)
; ==============================================================================

public _Editor_WndProc@16
_Editor_WndProc@16 proc hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
_Editor_WndProc@16 endp

public _Editor_Init@0
_Editor_Init@0 proc
    mov eax, TRUE
    ret
_Editor_Init@0 endp

; ==============================================================================
; Pane System Stubs (stdcall)
; ==============================================================================

public _IDEPaneSystem_Initialize@0
_IDEPaneSystem_Initialize@0 proc
    mov eax, TRUE
    ret
_IDEPaneSystem_Initialize@0 endp

public _IDEPaneSystem_CreateDefaultLayout@4
_IDEPaneSystem_CreateDefaultLayout@4 proc hParent:DWORD
    mov eax, TRUE
    ret
_IDEPaneSystem_CreateDefaultLayout@4 endp

; ==============================================================================
; UI Creation Stubs (stdcall)
; ==============================================================================

public _UIGguf_CreateMenuBar@4
_UIGguf_CreateMenuBar@4 proc hParent:DWORD
    LOCAL hMenu:DWORD
    
    invoke CreateMenu
    mov hMenu, eax
    
    ; Create File menu
    invoke CreatePopupMenu
    invoke AppendMenuA, hMenu, MF_POPUP, eax, offset szFileMenu
    
    ; Create Edit menu
    invoke CreatePopupMenu
    invoke AppendMenuA, hMenu, MF_POPUP, eax, offset szEditMenu
    
    ; Set menu on parent
    invoke SetMenu, hParent, hMenu
    mov eax, hMenu
    ret
_UIGguf_CreateMenuBar@4 endp

public _UIGguf_CreateToolbar@4
_UIGguf_CreateToolbar@4 proc hParent:DWORD
    xor eax, eax
    ret
_UIGguf_CreateToolbar@4 endp

public _UIGguf_CreateStatusPane@4
_UIGguf_CreateStatusPane@4 proc hParent:DWORD
    xor eax, eax
    ret
_UIGguf_CreateStatusPane@4 endp

; ==============================================================================
; GGUF/Backend Stubs (stdcall)
; ==============================================================================

public _GgufUnified_Init@0
_GgufUnified_Init@0 proc
    mov eax, TRUE
    ret
_GgufUnified_Init@0 endp

public _InferenceBackend_Init@0
_InferenceBackend_Init@0 proc
    mov eax, TRUE
    ret
_InferenceBackend_Init@0 endp

public _AgentSystem_Init@0
_AgentSystem_Init@0 proc
    mov eax, TRUE
    ret
_AgentSystem_Init@0 endp

; ==============================================================================
; Settings Stubs (stdcall)
; ==============================================================================

public _IDESettings_Initialize@0
_IDESettings_Initialize@0 proc
    mov eax, TRUE
    ret
_IDESettings_Initialize@0 endp

public _IDESettings_LoadFromFile@0
_IDESettings_LoadFromFile@0 proc
    mov eax, TRUE
    ret
_IDESettings_LoadFromFile@0 endp

public _IDESettings_ApplyTheme@0
_IDESettings_ApplyTheme@0 proc
    mov eax, TRUE
    ret
_IDESettings_ApplyTheme@0 endp

public _FileDialogs_Initialize@0
_FileDialogs_Initialize@0 proc
    mov eax, TRUE
    ret
_FileDialogs_Initialize@0 endp

; ==============================================================================
; InitCommonControls (may be provided by comctl32 or we stub it)
; ==============================================================================

public _InitCommonControlsEx@4
_InitCommonControlsEx@4 proc pInitCtrls:DWORD
    mov eax, TRUE
    ret
_InitCommonControlsEx@4 endp

end
