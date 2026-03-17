; Simple File Tree Implementation for RawrXD Agentic IDE (Pure MASM)
; Basic version that creates a tree view with minimal functionality.

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
    LOCAL dwDrives:DWORD
    LOCAL i:DWORD

    ; Create the tree view control
    mov dwStyle, WS_CHILD or WS_VISIBLE or 0x0001 or 0x0002 or 0x0004 or 0x0020  ; TVS flags
    invoke CreateWindowEx, 0, addr szTreeClass, NULL, dwStyle, 0, 0, 200, 400, hMainWindow, IDC_FILETREE, hInstance, NULL
    mov g_hFileTree, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif

    ; Add root item "Project" using SendMessage with raw values
    push 0
    push offset szRootText
    push 0x0001  ; TVIF_TEXT
    push 0xFFFFFFFF  ; TVI_LAST
    push 0xFFFFFFFF  ; TVI_ROOT
    push 0x450  ; TVM_INSERTITEM (WM_USER + 50)
    push g_hFileTree
    call SendMessage
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
            ; Insert drive item under root using SendMessage with raw values
            push 0
            push offset szDrive
            push 0x0001  ; TVIF_TEXT
            push 0xFFFFFFFF  ; TVI_LAST
            push g_hRootItem
            push 0x450  ; TVM_INSERTITEM (WM_USER + 50)
            push g_hFileTree
            call SendMessage
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