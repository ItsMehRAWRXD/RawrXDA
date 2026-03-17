; RawrXD Agentic IDE - File Tree Implementation (Pure MASM)
; Clean fixed version with drives, breadcrumbs (status bar), dropdown (combo)

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc
include \masm32\include\shell32.inc
include \masm32\include\shlwapi.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib
includelib \masm32\lib\shell32.lib
includelib \masm32\lib\shlwapi.lib

.data
include constants.inc

extrn g_hInstance:DWORD
extrn g_hMainWindow:DWORD
extrn g_hMainFont:DWORD

; Local TV constants (avoid conflicts)
MY_TVI_ROOT        equ 0FFFFFFFFh
MY_TVI_LAST        equ 0FFFFFFFFh
MY_TVIF_TEXT       equ 0001h
MY_TVIF_PARAM      equ 0004h
MY_TVIF_CHILDREN   equ 0040h

TV_FIRST           equ 1100h
MY_TVM_INSERTITEM  equ (TV_FIRST + 0)
MY_TVM_DELETEITEM  equ (TV_FIRST + 1)
MY_TVM_EXPAND      equ (TV_FIRST + 2)
MY_TVM_SETIMAGELIST equ (TV_FIRST + 9)
MY_TVM_GETNEXTITEM equ (TV_FIRST + 10)
MY_TVM_GETITEM     equ (TV_FIRST + 12)

; Tree notification constants
MY_TVN_FIRST       equ 0FFFFFE70h     ; -400
MY_TVN_SELCHANGEDA equ (MY_TVN_FIRST - 2)
MY_TVN_ITEMEXPANDINGA equ (MY_TVN_FIRST - 5)

; TVN_ITEMEXPANDING action values
TVE_COLLAPSE equ 0001h
TVE_EXPAND   equ 0002h

; Status bar messages
MY_SB_SETTEXTA equ (WM_USER + 1)

; Image list constants
ILC_COLOR32 equ 0020h
ILC_MASK    equ 0001h

; Minimap sizing
MINIMAP_WIDTH  equ 160
MINIMAP_HEIGHT equ 200
MINIMAP_MARGIN equ 4

.data
    szRootText      db "Project",0
    szDrive         db 4 dup(0)
    g_hFileTree     dd 0
    g_hRootItem     dd 0
    g_hBreadcrumb   dd 0
    szBreadcrumb    db 260 dup(0)
    szComboClass    db "COMBOBOX",0
    g_hDriveCombo   dd 0
    g_hMinimap      dd 0
    szStaticClass   db "STATIC",0
    g_hImageList    dd 0
    szCurrentPath   db 260 dup(0)

    szStar          db "\\*",0
    szSlash         db "\\",0
    szDot           db ".",0
    szDotDot        db "..",0

; Minimal TVITEM and TVINSERTSTRUCT (with unique field names)
MY_TVITEM struct
    mask_f          dd ?
    hItem_f         dd ?
    state_f         dd ?
    stateMask_f     dd ?
    pszText_f       dd ?
    cchTextMax_f    dd ?
    iImage_f        dd ?
    iSelectedImage_f dd ?
    cChildren_f     dd ?
    lParam_f        dd ?
MY_TVITEM ends

MY_TVINSERTSTRUCT struct
    hParent_f       dd ?
    hInsertAfter_f  dd ?
    item_f          MY_TVITEM <>
MY_TVINSERTSTRUCT ends

; Tree notification structure
MY_NMTREEVIEW struct
    hdr             NMHDR <>
    action          dd ?
    itemOld         MY_TVITEM <>
    itemNew         MY_TVITEM <>
    ptDrag          POINT <>
MY_NMTREEVIEW ends

; Node data stored in TreeView lParam
NODEDATA struct
    bPopulated      dd ?
    szPath          db MAX_PATH dup(?)
NODEDATA ends

.data?
    g_findData      WIN32_FIND_DATA <>
    g_szFindPath    db (MAX_PATH*2) dup(?)
    g_szChildPath   db (MAX_PATH*2) dup(?)

.code

; ---------------------------------------------------------------
; AllocNode - allocate NODEDATA and copy path
; Returns: pointer in eax (or 0)
; ---------------------------------------------------------------
AllocNode proc pszPath:DWORD
    LOCAL pNode:DWORD

    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, sizeof NODEDATA
    mov pNode, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif

    mov eax, pNode
    mov [eax].NODEDATA.bPopulated, 0
    lea ecx, [eax].NODEDATA.szPath
    invoke lstrcpyn, ecx, pszPath, MAX_PATH

    mov eax, pNode
    ret
AllocNode endp

BuildFindPath proc pNode:DWORD
    mov eax, pNode
    lea ecx, [eax].NODEDATA.szPath
    invoke lstrcpy, addr g_szFindPath, ecx
    invoke lstrlen, addr g_szFindPath
    .if eax != 0
        mov ecx, eax
        dec ecx
        mov edx, offset g_szFindPath
        mov al, BYTE PTR [edx + ecx]
        .if al != 5Ch
            invoke lstrcat, addr g_szFindPath, addr szSlash
        .endif
    .endif
    invoke lstrcat, addr g_szFindPath, addr szStar
    ret
BuildFindPath endp

BuildChildPath proc pNode:DWORD, pszName:DWORD
    mov eax, pNode
    lea ecx, [eax].NODEDATA.szPath
    invoke lstrcpy, addr g_szChildPath, ecx
    invoke lstrlen, addr g_szChildPath
    .if eax != 0
        mov ecx, eax
        dec ecx
        mov edx, offset g_szChildPath
        mov al, BYTE PTR [edx + ecx]
        .if al != 5Ch
            invoke lstrcat, addr g_szChildPath, addr szSlash
        .endif
    .endif
    invoke lstrcat, addr g_szChildPath, pszName
    ret
BuildChildPath endp

PopulateNodeChildren proc hParentItem:DWORD, pNode:DWORD
    LOCAL hFind:DWORD
    LOCAL tvins:MY_TVINSERTSTRUCT
    LOCAL pszName:DWORD
    LOCAL pChild:DWORD

    .if pNode == 0
        ret
    .endif
    mov eax, pNode
    mov ecx, [eax].NODEDATA.bPopulated
    .if ecx != 0
        ret
    .endif

    invoke BuildFindPath, pNode
    invoke FindFirstFile, addr g_szFindPath, addr g_findData
    mov hFind, eax
    .if eax == INVALID_HANDLE_VALUE
        mov eax, pNode
        mov [eax].NODEDATA.bPopulated, 1
        ret
    .endif

@@loop:
    lea eax, g_findData.cFileName
    mov pszName, eax

    invoke lstrcmp, pszName, addr szDot
    .if eax == 0
        jmp @@next
    .endif
    invoke lstrcmp, pszName, addr szDotDot
    .if eax == 0
        jmp @@next
    .endif

    invoke BuildChildPath, pNode, pszName
    invoke AllocNode, addr g_szChildPath
    mov pChild, eax

    mov eax, hParentItem
    mov tvins.hParent_f, eax
    mov tvins.hInsertAfter_f, MY_TVI_LAST
    mov tvins.item_f.mask_f, MY_TVIF_TEXT or MY_TVIF_PARAM or MY_TVIF_CHILDREN
    mov eax, pszName
    mov tvins.item_f.pszText_f, eax
    mov eax, pChild
    mov tvins.item_f.lParam_f, eax

    mov eax, g_findData.dwFileAttributes
    and eax, FILE_ATTRIBUTE_DIRECTORY
    .if eax != 0
        mov tvins.item_f.cChildren_f, 1
    .else
        mov tvins.item_f.cChildren_f, 0
    .endif

    invoke SendMessage, g_hFileTree, MY_TVM_INSERTITEM, 0, addr tvins

@@next:
    invoke FindNextFile, hFind, addr g_findData
    .if eax != 0
        jmp @@loop
    .endif

    invoke FindClose, hFind
    mov eax, pNode
    mov [eax].NODEDATA.bPopulated, 1
    ret
PopulateNodeChildren endp

; Forward declarations for handlers used by FileTree_OnNotify
OnTreeSelChanged proto :DWORD
OnTreeItemExpanding proto :DWORD

public FileTree_OnNotify
FileTree_OnNotify proc lParam:DWORD
    mov eax, lParam
    assume eax:ptr NMHDR
    mov ecx, [eax].idFrom
    mov edx, [eax].code
    assume eax:nothing

    .if ecx == IDC_FILETREE
        .if edx == MY_TVN_SELCHANGEDA
            invoke OnTreeSelChanged, lParam
        .elseif edx == MY_TVN_ITEMEXPANDINGA
            invoke OnTreeItemExpanding, lParam
        .endif
    .endif

    xor eax, eax
    ret
FileTree_OnNotify endp

public CreateFileTree
CreateFileTree proc
    LOCAL dwStyle:DWORD
    LOCAL tvins:MY_TVINSERTSTRUCT
    LOCAL dwDrives:DWORD
    LOCAL i:DWORD

    ; Create tree view control
    mov dwStyle, WS_CHILD or WS_VISIBLE or 0001h or 0002h or 0004h or 0020h ; TVS_* flags
    invoke CreateWindowEx, 0, addr szTreeClass, NULL, dwStyle, 0, 0, 240, 420, g_hMainWindow, IDC_FILETREE, g_hInstance, NULL
    mov g_hFileTree, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif
    invoke SendMessage, g_hFileTree, WM_SETFONT, g_hMainFont, TRUE

    ; Root item
    mov tvins.hParent_f, MY_TVI_ROOT
    mov tvins.hInsertAfter_f, MY_TVI_LAST
    mov tvins.item_f.mask_f, MY_TVIF_TEXT or MY_TVIF_PARAM or MY_TVIF_CHILDREN
    mov tvins.item_f.pszText_f, offset szRootText
    invoke AllocNode, offset szRootText
    mov tvins.item_f.lParam_f, eax
    mov tvins.item_f.cChildren_f, 1
    invoke SendMessage, g_hFileTree, MY_TVM_INSERTITEM, 0, ADDR tvins
    mov g_hRootItem, eax

    ; Breadcrumb status bar
    invoke CreateWindowEx, 0, addr szStatusClass, NULL, WS_CHILD or WS_VISIBLE, 0, 424, 240, 24, g_hMainWindow, 0, g_hInstance, NULL
    mov g_hBreadcrumb, eax
    invoke SendMessage, g_hBreadcrumb, WM_SETFONT, g_hMainFont, TRUE
    invoke lstrcpy, addr szBreadcrumb, offset szRootText
    invoke SendMessage, g_hBreadcrumb, MY_SB_SETTEXTA, 0, addr szBreadcrumb

    ; Drive dropdown (combo box)
    invoke CreateWindowEx, 0, addr szComboClass, NULL, WS_CHILD or WS_VISIBLE or WS_BORDER or CBS_DROPDOWNLIST, 244, 0, 160, 200, g_hMainWindow, 0, g_hInstance, NULL
    mov g_hDriveCombo, eax
    invoke SendMessage, g_hDriveCombo, WM_SETFONT, g_hMainFont, TRUE

    ; Minimap (STATIC with owner-draw)
    invoke CreateWindowEx, 0, addr szStaticClass, NULL, WS_CHILD or WS_VISIBLE or WS_BORDER, 244, 220, MINIMAP_WIDTH, MINIMAP_HEIGHT, g_hMainWindow, 0, g_hInstance, NULL
    mov g_hMinimap, eax
    invoke SendMessage, g_hMinimap, WM_SETFONT, g_hMainFont, TRUE

    ; Create image list for tree icons
    invoke ImageList_Create, 16, 16, ILC_COLOR32 or ILC_MASK, 3, 0
    mov g_hImageList, eax
    invoke SendMessage, g_hFileTree, MY_TVM_SETIMAGELIST, 0, g_hImageList

    ; Populate drives in combo and tree
    invoke GetLogicalDrives
    mov dwDrives, eax
    mov i, 0

DriveLoop:
    cmp i, 26
    jge DrivesDone
    mov eax, 1
    mov ecx, i
    shl eax, cl
    and eax, dwDrives
    test eax, eax
    jz NextDrive
    ; build "X:\" text
    mov al, 'A'
     mov ecx, i
     add al, cl
    mov BYTE PTR szDrive[0], al
    mov BYTE PTR szDrive[1], ':'
     mov BYTE PTR szDrive[2], 5Ch
    mov BYTE PTR szDrive[3], 0
    ; add to tree
    mov eax, g_hRootItem
    mov tvins.hParent_f, eax
    mov tvins.hInsertAfter_f, MY_TVI_LAST
    mov tvins.item_f.mask_f, MY_TVIF_TEXT or MY_TVIF_PARAM or MY_TVIF_CHILDREN
    mov tvins.item_f.pszText_f, OFFSET szDrive
    invoke AllocNode, OFFSET szDrive
    mov tvins.item_f.lParam_f, eax
    mov tvins.item_f.cChildren_f, 1
    invoke SendMessage, g_hFileTree, MY_TVM_INSERTITEM, 0, ADDR tvins
    ; add to dropdown
     invoke SendMessage, g_hDriveCombo, CB_ADDSTRING, 0, OFFSET szDrive
NextDrive:
    inc i
    jmp DriveLoop
DrivesDone:

    mov eax, g_hFileTree
    ret
CreateFileTree endp

public RefreshFileTree
RefreshFileTree proc
    ; simple refresh: re-create root and drives
    invoke SendMessage, g_hFileTree, MY_TVM_DELETEITEM, 0, MY_TVI_ROOT
    call CreateFileTree
    mov eax, 1
    ret
RefreshFileTree endp

; ============================================================================
; Tree Notification Handlers
; ============================================================================

public OnTreeSelChanged
OnTreeSelChanged proc pNMHDR:DWORD
    LOCAL pNMTV:DWORD
    LOCAL tvi:MY_TVITEM
    LOCAL szText[260]:BYTE

    mov eax, pNMHDR
    mov pNMTV, eax
    mov eax, pNMTV
    assume eax:ptr MY_NMTREEVIEW
    mov ecx, [eax].itemNew.hItem_f
    assume eax:nothing

    .if ecx != 0
        mov tvi.mask_f, MY_TVIF_TEXT or MY_TVIF_PARAM
        mov tvi.hItem_f, ecx
        lea eax, szText
        mov tvi.pszText_f, eax
        mov tvi.cchTextMax_f, 260

        invoke SendMessage, g_hFileTree, MY_TVM_GETITEM, 0, addr tvi
        .if eax != 0
            mov eax, tvi.lParam_f
            .if eax != 0
                lea eax, [eax].NODEDATA.szPath
                invoke SendMessage, g_hBreadcrumb, MY_SB_SETTEXTA, 0, eax
            .else
                invoke lstrcpy, addr szBreadcrumb, addr szText
                invoke SendMessage, g_hBreadcrumb, MY_SB_SETTEXTA, 0, addr szBreadcrumb
            .endif

            ; Update minimap
            call UpdateMinimap
        .endif
    .endif
    
    mov eax, 0
    ret
OnTreeSelChanged endp

public OnTreeItemExpanding
OnTreeItemExpanding proc pNMHDR:DWORD
    LOCAL pNMTV:DWORD
    LOCAL action:DWORD
    LOCAL hItem:DWORD
    LOCAL pNode:DWORD

    mov eax, pNMHDR
    mov pNMTV, eax
    mov eax, pNMTV
    assume eax:ptr MY_NMTREEVIEW
    mov ecx, [eax].action
    mov action, ecx
    mov ecx, [eax].itemNew.hItem_f
    mov hItem, ecx
    mov ecx, [eax].itemNew.lParam_f
    mov pNode, ecx
    assume eax:nothing

    ; Only populate when expanding
    mov eax, action
    .if eax == TVE_EXPAND
        invoke PopulateNodeChildren, hItem, pNode
    .endif

    mov eax, 0
    ret
OnTreeItemExpanding endp

; ============================================================================
; Minimap Functions
; ============================================================================

public UpdateMinimap
UpdateMinimap proc
    LOCAL hDC:DWORD
    LOCAL hBrush:DWORD
    LOCAL rc:RECT
    
    ; Get minimap DC
    invoke GetDC, g_hMinimap
    mov hDC, eax
    
    ; Clear minimap
    invoke GetClientRect, g_hMinimap, addr rc
    invoke GetSysColorBrush, COLOR_WINDOW
    mov hBrush, eax
    invoke FillRect, hDC, addr rc, hBrush
    
    ; Draw simple tree representation
    invoke GetSysColorBrush, COLOR_HIGHLIGHT
    mov hBrush, eax
    mov rc.left, MINIMAP_MARGIN
    mov rc.top, MINIMAP_MARGIN
    mov rc.right, MINIMAP_WIDTH - MINIMAP_MARGIN
    mov rc.bottom, MINIMAP_MARGIN + 20
    invoke FillRect, hDC, addr rc, hBrush
    
    ; Draw current path text
    invoke SetBkMode, hDC, TRANSPARENT
    invoke SetTextColor, hDC, 000000h
    invoke DrawText, hDC, addr szBreadcrumb, -1, addr rc, DT_LEFT or DT_SINGLELINE or DT_VCENTER
    
    invoke ReleaseDC, g_hMinimap, hDC
    ret
UpdateMinimap endp

; ============================================================================
; Layout Management
; ============================================================================

public UpdateLayout
UpdateLayout proc dwWidth:DWORD, dwHeight:DWORD
    ; Reposition controls based on new window size
    
    ; Tree view (left panel)
    invoke MoveWindow, g_hFileTree, 0, 0, 240, dwHeight - 24, TRUE
    
    ; Breadcrumb (bottom)
    invoke MoveWindow, g_hBreadcrumb, 0, dwHeight - 24, 240, 24, TRUE
    
    ; Drive dropdown (top right)
    invoke MoveWindow, g_hDriveCombo, 244, 0, dwWidth - 244, 200, TRUE
    
    ; Minimap (bottom right)
    invoke MoveWindow, g_hMinimap, 244, 200, dwWidth - 244, dwHeight - 200, TRUE
    
    ret
UpdateLayout endp

end


