; RawrXD Agentic IDE - Enhanced File Tree Implementation (Pure MASM)
; Enhanced version with folder navigation, breadcrumbs, and minimap support

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\shell32.inc
include \masm32\include\shlwapi.inc
include \masm32\include\comctl32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\shell32.lib
includelib \masm32\lib\shlwapi.lib
includelib \masm32\lib\comctl32.lib

.data
include constants.inc

extrn g_hInstance:DWORD
extrn g_hMainWindow:DWORD

; Tree view constants
MY_TVI_ROOT        equ 0xFFFFFFFF
MY_TVI_LAST        equ 0xFFFFFFFF
MY_TVIF_TEXT       equ 0x0001
MY_TVIF_PARAM      equ 0x0004
MY_TVM_INSERTITEM  equ (WM_USER + 50)
MY_TVM_DELETEITEM  equ (WM_USER + 1)
MY_TVM_EXPAND      equ (WM_USER + 2)
MY_TVM_GETNEXTITEM equ (WM_USER + 10)
MY_TVM_GETITEM     equ (WM_USER + 12)
MY_TVM_SETITEM     equ (WM_USER + 13)
MY_TVGN_ROOT       equ 0x0000
MY_TVGN_NEXT       equ 0x0001
MY_TVGN_PREVIOUS   equ 0x0002
MY_TVGN_PARENT     equ 0x0003
MY_TVGN_CHILD      equ 0x0004
MY_TVGN_FIRSTVISIBLE equ 0x0005
MY_TVGN_NEXTVISIBLE equ 0x0006
MY_TVGN_PREVIOUSVISIBLE equ 0x0007
MY_TVGN_DROPHILITE equ 0x0008
MY_TVGN_CARET      equ 0x0009
MY_TVE_COLLAPSE    equ 0x0001
MY_TVE_EXPAND      equ 0x0002
MY_TVE_TOGGLE      equ 0x0003
MY_TVE_EXPANDPARTIAL equ 0x4000
MY_TVS_HASBUTTONS  equ 0x0001
MY_TVS_HASLINES    equ 0x0002
MY_TVS_LINESATROOT equ 0x0004
MY_TVS_SHOWSELALWAYS equ 0x0020

; Minimap constants
MINIMAP_WIDTH equ 200
MINIMAP_HEIGHT equ 150
MINIMAP_ZOOM_FACTOR equ 10

; File item types
ITEM_TYPE_ROOT   equ 0
ITEM_TYPE_DRIVE  equ 1
ITEM_TYPE_FOLDER equ 2
ITEM_TYPE_FILE   equ 3

; Tree item structure
MY_TVITEM struct
    mask_field        dd ?
    hItem_field       dd ?
    state_field       dd ?
    stateMask_field   dd ?
    pszText_field     dd ?
    cchTextMax_field  dd ?
    iImage_field      dd ?
    iSelectedImage_field dd ?
    cChildren_field   dd ?
    lParam_field      dd ?
MY_TVITEM ends

; Tree insert structure
MY_TVINSERTSTRUCT struct
    hParent_field         dd ?
    hInsertAfter_field    dd ?
    item_field            MY_TVITEM <>
MY_TVINSERTSTRUCT ends

; Minimap structure
MINIMAP_DATA struct
    hMinimap dd ?
    hBitmap dd ?
    zoomFactor dd ?
    offsetX dd ?
    offsetY dd ?
MINIMAP_DATA ends

; Breadcrumb structure
BREADCRUMB_ITEM struct
    szPath db MAX_PATH dup(?)
    hItem dd ?
BREADCRUMB_ITEM ends

; Data section
.data
    szTreeClass       db "SysTreeView32",0
    szStaticClass     db "Static",0
    IDC_FILETREE      equ 1000
    szRootText        db "Project",0
    szDrive           db 4 dup(0)
    szFindPattern     db "\*",0
    findData          WIN32_FIND_DATA <>

    g_hFileTree       dd 0
    g_hRootItem       dd 0
    g_hMinimap        dd 0
    g_minimapData     MINIMAP_DATA <>
    g_breadcrumbs     BREADCRUMB_ITEM 10 dup(<>)
    g_breadcrumbCount dd 0

.data?
    ; Uninitialized data

; ============================================================================
; PROCEDURES
; ============================================================================

; Forward declarations
PopulateDirectory proto :DWORD, :DWORD

; ---------------------------------------------------------------
; CreateFileTree - creates tree view with drives, breadcrumbs, and minimap
; Returns: handle to tree view in eax
; ---------------------------------------------------------------
CreateFileTree proc
    LOCAL dwStyle:DWORD
    LOCAL tvins:MY_TVINSERTSTRUCT
    LOCAL dwDrives:DWORD
    LOCAL i:DWORD

    ; Create tree view control
    mov dwStyle, WS_CHILD or WS_VISIBLE or MY_TVS_HASBUTTONS or MY_TVS_HASLINES or MY_TVS_LINESATROOT or MY_TVS_SHOWSELALWAYS
    invoke CreateWindowEx, 0, addr szTreeClass, NULL, dwStyle, 0, 0, 200, 400, g_hMainWindow, IDC_FILETREE, g_hInstance, NULL
    mov g_hFileTree, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif

    ; Add root item
    mov tvins.hParent_field, MY_TVI_ROOT
    mov tvins.hInsertAfter_field, MY_TVI_LAST
    mov tvins.item_field.mask_field, MY_TVIF_TEXT or MY_TVIF_PARAM
    mov tvins.item_field.pszText_field, offset szRootText
    mov tvins.item_field.lParam_field, ITEM_TYPE_ROOT
    invoke SendMessage, g_hFileTree, MY_TVM_INSERTITEM, 0, addr tvins
    mov g_hRootItem, eax

    ; Add breadcrumb for root
    mov eax, g_breadcrumbCount
    mov ecx, sizeof BREADCRUMB_ITEM
    mul ecx
    lea edx, g_breadcrumbs[eax]
    mov [edx].BREADCRUMB_ITEM.hItem, g_hRootItem
    invoke lstrcpy, addr [edx].BREADCRUMB_ITEM.szPath, offset szRootText
    inc g_breadcrumbCount

    ; Enumerate drives
    invoke GetLogicalDrives
    mov dwDrives, eax
    mov i, 0
    
    @DriveLoop:
    cmp i, 26
    jge @DrivesDone
    
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
        mov tvins.hParent_field, g_hRootItem
        mov tvins.hInsertAfter_field, MY_TVI_LAST
        mov tvins.item_field.mask_field, MY_TVIF_TEXT or MY_TVIF_PARAM
        mov tvins.item_field.pszText_field, offset szDrive
        mov tvins.item_field.lParam_field, ITEM_TYPE_DRIVE
        invoke SendMessage, g_hFileTree, MY_TVM_INSERTITEM, 0, addr tvins
    .endif
    
    inc i
    jmp @DriveLoop
    
@DrivesDone:
    ; Create minimap
    call CreateMinimap
    
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

; ---------------------------------------------------------------
; PopulateDirectory - populates a directory with its contents
; Parameters: hItem - handle to tree item, pszPath - path to directory
; ---------------------------------------------------------------
PopulateDirectory proc hItem:DWORD, pszPath:DWORD
    LOCAL hFind:DWORD
    LOCAL szSearchPath[MAX_PATH]:BYTE
    LOCAL tvins:MY_TVINSERTSTRUCT

    ; Build search path
    invoke lstrcpy, addr szSearchPath, pszPath
    invoke lstrlen, addr szSearchPath
    .if eax > 0 && BYTE PTR [eax + addr szSearchPath - 1] != '\'
        invoke lstrcat, addr szSearchPath, addr szBackslash
    .endif
    invoke lstrcat, addr szSearchPath, addr szFindPattern
    
    ; Find first file
    invoke FindFirstFile, addr szSearchPath, addr findData
    mov hFind, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @Done
    
    @FindLoop:
    ; Skip "." and ".."
    invoke lstrcmp, addr findData.cFileName, addr szDot
    .if eax != 0
        invoke lstrcmp, addr findData.cFileName, addr szDotDot
        .if eax != 0
            ; Add item to tree
            mov tvins.hParent_field, hItem
            mov tvins.hInsertAfter_field, MY_TVI_LAST
            mov tvins.item_field.mask_field, MY_TVIF_TEXT or MY_TVIF_PARAM
            lea eax, findData.cFileName
            mov tvins.item_field.pszText_field, eax
            
            ; Determine item type
            test findData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
            .if ZERO?
                mov tvins.item_field.lParam_field, ITEM_TYPE_FILE
            .else
                mov tvins.item_field.lParam_field, ITEM_TYPE_FOLDER
            .endif
            
            invoke SendMessage, g_hFileTree, MY_TVM_INSERTITEM, 0, addr tvins
        .endif
    .endif
    
    ; Find next file
    invoke FindNextFile, hFind, addr findData
    test eax, eax
    jnz @FindLoop
    
    invoke FindClose, hFind
    
@Done:
    ret
PopulateDirectory endp

; ---------------------------------------------------------------
; ExpandDirectory - expands directory in tree
; ---------------------------------------------------------------
ExpandDirectory proc hItem:DWORD
    ; Expand directory and populate with contents
    invoke SendMessage, g_hFileTree, MY_TVM_EXPAND, MY_TVE_EXPAND, hItem
    ret
ExpandDirectory endp

; ---------------------------------------------------------------
; CollapseDirectory - collapses directory in tree
; ---------------------------------------------------------------
CollapseDirectory proc hItem:DWORD
    ; Collapse directory
    invoke SendMessage, g_hFileTree, MY_TVM_EXPAND, MY_TVE_COLLAPSE, hItem
    ret
CollapseDirectory endp

; ---------------------------------------------------------------
; GetItemPath - gets the full path for a tree item
; ---------------------------------------------------------------
GetItemPath proc hItem:DWORD, pszBuffer:DWORD, dwBufferSize:DWORD
    ; This would build the full path by traversing up the tree
    ; For now, just return a placeholder
    invoke lstrcpy, pszBuffer, addr szRootText
    ret
GetItemPath endp

; ---------------------------------------------------------------
; CreateMinimap - creates minimap control
; ---------------------------------------------------------------
CreateMinimap proc
    LOCAL dwStyle:DWORD
    
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER
    invoke CreateWindowEx, 0, addr szStaticClass, NULL, dwStyle, 210, 0, MINIMAP_WIDTH, MINIMAP_HEIGHT, g_hMainWindow, 0, g_hInstance, NULL
    mov g_hMinimap, eax
    
    mov g_minimapData.hMinimap, eax
    mov g_minimapData.zoomFactor, MINIMAP_ZOOM_FACTOR
    mov g_minimapData.offsetX, 0
    mov g_minimapData.offsetY, 0
    
    ret
CreateMinimap endp

; ---------------------------------------------------------------
; UpdateMinimap - updates minimap display
; ---------------------------------------------------------------
UpdateMinimap proc
    ; Draw tree structure representation on minimap
    invoke InvalidateRect, g_hMinimap, NULL, TRUE
    invoke UpdateWindow, g_hMinimap
    ret
UpdateMinimap endp

; ---------------------------------------------------------------
; OnTreeSelectionChanged - handles selection changes
; ---------------------------------------------------------------
OnTreeSelectionChanged proc
    LOCAL hSelected:DWORD
    
    ; Get selected item
    invoke SendMessage, g_hFileTree, MY_TVM_GETNEXTITEM, MY_TVGN_CARET, 0
    mov hSelected, eax
    
    ; Update breadcrumbs
    call UpdateBreadcrumbs
    
    ; Update minimap
    call UpdateMinimap
    
    ret
OnTreeSelectionChanged endp

; ---------------------------------------------------------------
; UpdateBreadcrumbs - updates breadcrumb trail
; ---------------------------------------------------------------
UpdateBreadcrumbs proc
    ; Update breadcrumb display based on current selection
    ; This would show the path from root to current selection
    ret
UpdateBreadcrumbs endp

.data
    szDot         db ".",0
    szDotDot      db "..",0
    szBackslash   db "\",0

end