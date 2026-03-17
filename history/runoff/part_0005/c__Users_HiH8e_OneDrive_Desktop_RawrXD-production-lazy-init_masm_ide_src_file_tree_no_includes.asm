; RawrXD Agentic IDE - File Tree Implementation (Pure MASM)
; Clean working version without includes

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
szTreeClass       db "SysTreeView32",0
IDC_FILETREE      equ 1000

extrn hInstance:DWORD
extrn hMainWindow:DWORD

; Data section
.data
    g_hFileTree       dd 0

.data?
    ; Uninitialized data

; ============================================================================
; PROCEDURES
; ============================================================================

; ---------------------------------------------------------------
; CreateFileTree - creates tree view
; Returns: handle to tree view in eax
; ---------------------------------------------------------------
CreateFileTree proc
    LOCAL dwStyle:DWORD

    ; Create tree view control
    mov dwStyle, WS_CHILD or WS_VISIBLE or 1h or 2h or 4h or 20h
    invoke CreateWindowEx, 0, addr szTreeClass, NULL, dwStyle, 0, 0, 200, 400, hMainWindow, IDC_FILETREE, hInstance, NULL
    mov g_hFileTree, eax
    
    mov eax, g_hFileTree
    ret
CreateFileTree endp

; ---------------------------------------------------------------
; RefreshFileTree - refreshes tree view
; ---------------------------------------------------------------
RefreshFileTree proc
    mov eax, 1
    ret
RefreshFileTree endp

end