; Simple File Tree Implementation for RawrXD Agentic IDE (Pure MASM)
; Creates a basic tree view window without complex functionality.

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\shell32.inc
include \masm32\include\shlwapi.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\shell32.lib
includelib \masm32\lib\shlwapi.lib

 .data
include constants.inc
include macros.inc

extrn hInstance:DWORD
extrn hMainWindow:DWORD

; Data for the tree view
.data
    g_hFileTree   dd 0

.data?
    ; none

; ---------------------------------------------------------------
; CreateFileTree - creates the tree view window
; Returns: handle to the tree view in eax
; ---------------------------------------------------------------
CreateFileTree proc
    LOCAL dwStyle:DWORD

    ; Create the tree view control
    mov dwStyle, WS_CHILD or WS_VISIBLE or 1h or 2h or 4h or 20h  ; TVS flags
    invoke CreateWindowEx, 0, addr szTreeClass, NULL, dwStyle, 0, 0, 200, 400, hMainWindow, IDC_FILETREE, hInstance, NULL
    mov g_hFileTree, eax
    
    mov eax, g_hFileTree
    ret
CreateFileTree endp

; ---------------------------------------------------------------
; RefreshFileTree - placeholder for future implementation
; ---------------------------------------------------------------
RefreshFileTree proc
    mov eax, 1
    ret
RefreshFileTree endp

end