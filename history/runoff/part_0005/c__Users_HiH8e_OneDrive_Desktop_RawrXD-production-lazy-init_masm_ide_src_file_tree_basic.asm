; Simple File Tree Implementation for RawrXD Agentic IDE (Pure MASM)
; Provides a basic tree view with a root node and logical drive items.

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
include structures.inc
include macros.inc

extrn hInstance:DWORD
extrn hMainWindow:DWORD

; Simple constants for tree view
TVI_ROOT        equ 0xFFFFFFFF
TVI_LAST        equ 0xFFFFFFFF
TVIF_TEXT       equ 0x0001
TVM_INSERTITEM  equ (WM_USER + 50)
TVS_HASBUTTONS  equ 0x0001
TVS_HASLINES    equ 0x0002
TVS_LINESATROOT equ 0x0004
TVS_SHOWSELALWAYS equ 0x0020

; Simple TVITEM structure
TVITEM struct
    mask_field        dd ?
    hItem       dd ?
    state       dd ?
    stateMask   dd ?
    pszText     dd ?
    cchTextMax  dd ?
    iImage      dd ?
    iSelectedImage dd ?
    cChildren   dd ?
    lParam      dd ?
TVITEM ends

; TVINSERTSTRUCT
TVINSERTSTRUCT struct
    hParent         dd ?
    hInsertAfter    dd ?
    item            TVITEM <>
TVINSERTSTRUCT ends

; Data for the tree view
.data
    szRootText    db "Project",0
    szDrive       db 4 dup(0)
    g_hFileTree   dd 0
    g_hRootItem   dd 0

.data?
    ; none

; ---------------------------------------------------------------
; CreateFileTree - creates the tree view and populates drives
; Returns: handle to the tree view in eax
; ---------------------------------------------------------------
CreateFileTree proc
    LOCAL dwStyle:DWORD
    LOCAL tvins:TVINSERTSTRUCT
    LOCAL dwDrives:DWORD
    LOCAL i:DWORD

    ; Create the tree view control
    mov dwStyle, WS_CHILD or WS_VISIBLE or TVS_HASBUTTONS or TVS_HASLINES or TVS_LINESATROOT or TVS_SHOWSELALWAYS
    invoke CreateWindowEx, 0, addr szTreeClass, NULL, dwStyle, 0, 0, 200, 400, hMainWindow, IDC_FILETREE, hInstance, NULL
    mov g_hFileTree, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif

    ; Add root item "Project"
    mov tvins.hParent, TVI_ROOT
    mov tvins.hInsertAfter, TVI_LAST
    mov tvins.item.mask_field, TVIF_TEXT
    mov tvins.item.pszText, offset szRootText
    invoke SendMessage, g_hFileTree, TVM_INSERTITEM, 0, addr tvins
    mov g_hRootItem, eax

    ; Enumerate logical drives and add each as a child of the root
    invoke GetLogicalDrives
    mov dwDrives, eax
    mov i, 0
    .while i < 26
        mov eax, 1
        mov ecx, i
        shl eax, cl
        and eax, dwDrives
        .if eax != 0
            ; Build drive string like "C:\\"
            mov al, 'A'
            add al, BYTE PTR i
            mov BYTE PTR szDrive[0], al
            mov BYTE PTR szDrive[1], ':'
            mov BYTE PTR szDrive[2], '\\'
            mov BYTE PTR szDrive[3], 0
            ; Insert drive item under root
            mov tvins.hParent, g_hRootItem
            mov tvins.hInsertAfter, TVI_LAST
            mov tvins.item.mask_field, TVIF_TEXT
            mov tvins.item.pszText, offset szDrive
            invoke SendMessage, g_hFileTree, TVM_INSERTITEM, 0, addr tvins
        .endif
        inc i
    .endw

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