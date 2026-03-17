; RawrXD Agentic IDE - File Tree Implementation (Pure MASM)
; Simple working version with drive support

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

extrn hInstance:DWORD
extrn hMainWindow:DWORD

; Simple constants
TVI_ROOT        equ 0xFFFFFFFF
TVI_LAST        equ 0xFFFFFFFF
TVIF_TEXT       equ 0x0001
TVM_INSERTITEM  equ (WM_USER + 50)
TVS_HASBUTTONS  equ 0x0001
TVS_HASLINES    equ 0x0002
TVS_LINESATROOT equ 0x0004
TVS_SHOWSELALWAYS equ 0x0020

; Data section
.data
    szRootText        db "Project",0
    szDrive           db 4 dup(0)
    g_hFileTree       dd 0
    g_hRootItem       dd 0

.data?
    ; Uninitialized data

; ============================================================================
; PROCEDURES
; ============================================================================

; ---------------------------------------------------------------
; CreateFileTree - creates tree view with drives
; Returns: handle to tree view in eax
; ---------------------------------------------------------------
CreateFileTree proc
    LOCAL dwStyle:DWORD
    LOCAL dwDrives:DWORD
    LOCAL i:DWORD

    ; Create tree view control
    mov dwStyle, WS_CHILD or WS_VISIBLE or TVS_HASBUTTONS or TVS_HASLINES or TVS_LINESATROOT or TVS_SHOWSELALWAYS
    invoke CreateWindowEx, 0, addr szTreeClass, NULL, dwStyle, 0, 0, 200, 400, hMainWindow, IDC_FILETREE, hInstance, NULL
    mov g_hFileTree, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif

    ; Add root item using SendMessage
    push 0
    push offset szRootText
    push TVIF_TEXT
    push TVI_LAST
    push TVI_ROOT
    push TVM_INSERTITEM
    push g_hFileTree
    call SendMessage
    mov g_hRootItem, eax

    ; Enumerate drives
    invoke GetLogicalDrives
    mov dwDrives, eax
    mov i, 0
    
    jmp @CheckLoop
    
@DriveLoop:
    mov eax, 1
    mov ecx, i
    shl eax, cl
    and eax, dwDrives
    .if eax != 0
        ; Build drive string
        mov al, 'A'
        add al, BYTE PTR i
        mov BYTE PTR szDrive[0], al
        mov BYTE PTR szDrive[1], ':'
        mov BYTE PTR szDrive[2], '\\'
        mov BYTE PTR szDrive[3], 0
        
        ; Add drive to tree
        push 0
        push offset szDrive
        push TVIF_TEXT
        push TVI_LAST
        push g_hRootItem
        push TVM_INSERTITEM
        push g_hFileTree
        call SendMessage
    .endif
    
    inc i
    
@CheckLoop:
    cmp i, 26
    jl @DriveLoop
    
    mov eax, g_hFileTree
    ret
CreateFileTree endp

; ---------------------------------------------------------------
; RefreshFileTree - refreshes tree view
; ---------------------------------------------------------------
RefreshFileTree proc
    ; Clear and repopulate tree
    invoke SendMessage, g_hFileTree, TVM_DELETEITEM, 0, 0
    call CreateFileTree
    ret
RefreshFileTree endp

; ---------------------------------------------------------------
; CreateMinimap - creates minimap control (placeholder)
; ---------------------------------------------------------------
CreateMinimap proc
    ; Minimap placeholder - will be implemented later
    mov eax, 1
    ret
CreateMinimap endp

; ---------------------------------------------------------------
; UpdateMinimap - updates minimap display (placeholder)
; ---------------------------------------------------------------
UpdateMinimap proc
    ; Minimap placeholder
    mov eax, 1
    ret
UpdateMinimap endp

end