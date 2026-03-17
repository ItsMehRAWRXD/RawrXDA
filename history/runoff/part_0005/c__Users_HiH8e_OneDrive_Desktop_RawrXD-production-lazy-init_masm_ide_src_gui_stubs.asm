; ==============================================================================
; gui_stubs.asm - GUI component stubs for missing UI functions
; ==============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

.code

public UIGguf_CreateMenuBar
public UIGguf_CreateToolbar
public UIGguf_CreateStatusPane
public GUI_InitAllComponents
public GUI_UpdateLayout
public GUI_HandleCommand
public LogSystem_Initialize

; UI GGUF integration stubs
UIGguf_CreateMenuBar proc hParent:DWORD
    xor eax, eax
    ret
UIGguf_CreateMenuBar endp

UIGguf_CreateToolbar proc hParent:DWORD
    xor eax, eax
    ret
UIGguf_CreateToolbar endp

UIGguf_CreateStatusPane proc hParent:DWORD
    xor eax, eax
    ret
UIGguf_CreateStatusPane endp

; GUI wiring stubs
GUI_InitAllComponents proc hParent:DWORD
    mov eax, TRUE
    ret
GUI_InitAllComponents endp

GUI_UpdateLayout proc hWindow:DWORD
    mov eax, TRUE
    ret
GUI_UpdateLayout endp

GUI_HandleCommand proc hWindow:DWORD, wParam:DWORD, lParam:DWORD
    xor eax, eax
    ret
GUI_HandleCommand endp

; Logging stub
LogSystem_Initialize proc
    mov eax, TRUE
    ret
LogSystem_Initialize endp

end
