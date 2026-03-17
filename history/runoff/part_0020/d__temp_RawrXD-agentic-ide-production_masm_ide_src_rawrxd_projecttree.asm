;======================================================================
; RawrXD IDE - Project Tree View
; File browser with tree structure, drag-drop, context menu
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
g_hProjectTree          DQ ?
g_hTreeImageList        DQ ?
g_selectedTreeNode      DQ ?
g_draggedNode           DQ ?
g_isDraggingNode        DB 0
g_contextMenuHwnd       DQ ?
g_currentPath[MAX_PATH_LENGTH] DB MAX_PATH_LENGTH DUP(0)

; Tree item structures
TVITEM STRUCT
    mask                DQ ?
    hItem               DQ ?
    state               DQ ?
    stateMask           DQ ?
    pszText             DQ ?
    cchTextMax          DQ ?
    iImage              DQ ?
    iSelectedImage      DQ ?
    cChildren           DQ ?
    lParam              DQ ?
TVITEM ENDS

TVINSERTSTRUCT STRUCT
    hParent             DQ ?
    hInsertAfter        DQ ?
    item                TVITEM <>
TVINSERTSTRUCT ENDS

.CODE

;----------------------------------------------------------------------
; RawrXD_ProjectTree_Create - Create project tree view
;----------------------------------------------------------------------
RawrXD_ProjectTree_Create PROC hParent:QWORD
    LOCAL rect:RECT
    LOCAL hImageList:QWORD
    
    ; Get parent dimensions
    INVOKE GetClientRect, hParent, ADDR rect
    
    ; Create tree view control
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "SysTreeView32",
        NULL,
        WS_CHILD OR WS_VISIBLE OR WS_BORDER OR TVS_HASLINES OR TVS_LINESATROOT OR TVS_HASBUTTONS OR TVS_DRAGDROP OR TVS_SHOWSELALWAYS,
        0, 32, 200, rect.bottom - 52,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hProjectTree, rax
    test rax, rax
    jz @@fail
    
    ; Initialize common controls for tree view
    INVOKE InitCommonControlsEx, ADDR icc
    
    ; Create image list (16x16 icons)
    INVOKE ImageList_Create, 16, 16, ILC_COLOR32, 10, 5
    mov hImageList, rax
    mov g_hTreeImageList, rax
    
    ; Attach image list to tree
    INVOKE SendMessage, g_hProjectTree, TVM_SETIMAGELIST, TVSIL_NORMAL, hImageList
    
    ; Add root node
    INVOKE RawrXD_ProjectTree_AddRoot
    
    xor rax, rax
    ret
    
@@fail:
    mov rax, -1
    ret
    
RawrXD_ProjectTree_Create ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_AddRoot - Add root node to tree
;----------------------------------------------------------------------
RawrXD_ProjectTree_AddRoot PROC
    LOCAL tvi:TV_INSERTSTRUCT
    LOCAL tvitem:TVITEM
    
    test g_hProjectTree, g_hProjectTree
    jz @@ret
    
    ; Setup item structure
    INVOKE RtlZeroMemory, ADDR tvitem, SIZEOF TVITEM
    mov tvitem.mask, TVIF_TEXT
    mov tvitem.pszText, OFFSET g_currentPath
    
    ; Setup insert struct
    INVOKE RtlZeroMemory, ADDR tvi, SIZEOF TV_INSERTSTRUCT
    mov tvi.item.mask, TVIF_TEXT
    mov tvi.item.pszText, OFFSET szRootProject
    mov tvi.hParent, TVI_ROOT
    mov tvi.hInsertAfter, TVI_LAST
    
    ; Insert item
    INVOKE SendMessage, g_hProjectTree, TVM_INSERTITEM, 0, ADDR tvi
    
@@ret:
    ret
    
RawrXD_ProjectTree_AddRoot ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_LoadProject - Load project folder into tree
;----------------------------------------------------------------------
RawrXD_ProjectTree_LoadProject PROC pszPath:QWORD
    LOCAL findData:WIN32_FIND_DATAA
    LOCAL hFind:QWORD
    LOCAL szPath[260]:BYTE
    LOCAL szPathPattern[264]:BYTE
    
    ; Copy current path
    INVOKE lstrcpyA, ADDR g_currentPath, pszPath
    
    ; Build search pattern (path\*)
    INVOKE lstrcpyA, ADDR szPathPattern, pszPath
    INVOKE lstrcatA, ADDR szPathPattern, "\*"
    
    ; Find first file
    INVOKE FindFirstFileA, ADDR szPathPattern, ADDR findData
    mov hFind, rax
    
    cmp hFind, INVALID_HANDLE_VALUE
    je @@done
    
@@loop:
    ; Skip "." and ".."
    cmp findData.cFileName[0], '.'
    je @@next
    
    ; Check if directory or file (source only)
    mov eax, findData.dwFileAttributes
    and eax, FILE_ATTRIBUTE_DIRECTORY
    cmp eax, 0
    jne @@next  ; Skip folders for now
    
    ; Check file extension
    INVOKE RawrXD_ProjectTree_IsSourceFile, ADDR findData.cFileName
    test eax, eax
    jz @@next
    
    ; Add file to tree
    INVOKE RawrXD_ProjectTree_AddFile, ADDR findData.cFileName, pszPath
    
@@next:
    INVOKE FindNextFileA, hFind, ADDR findData
    test eax, eax
    jnz @@loop
    
@@done:
    INVOKE FindClose, hFind
    ret
    
RawrXD_ProjectTree_LoadProject ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_IsSourceFile - Check if file is source file
;----------------------------------------------------------------------
RawrXD_ProjectTree_IsSourceFile PROC pszFile:QWORD
    ; Check extensions: .asm, .c, .h, .inc
    INVOKE RawrXD_Util_HasExtension, pszFile, ".asm"
    test eax, eax
    jnz @@yes
    
    INVOKE RawrXD_Util_HasExtension, pszFile, ".c"
    test eax, eax
    jnz @@yes
    
    INVOKE RawrXD_Util_HasExtension, pszFile, ".h"
    test eax, eax
    jnz @@yes
    
    INVOKE RawrXD_Util_HasExtension, pszFile, ".inc"
    test eax, eax
    jnz @@yes
    
    xor eax, eax
    ret
    
@@yes:
    mov eax, 1
    ret
    
RawrXD_ProjectTree_IsSourceFile ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_AddFile - Add file to tree
;----------------------------------------------------------------------
RawrXD_ProjectTree_AddFile PROC pszFile:QWORD, pszPath:QWORD
    LOCAL tvi:TV_INSERTSTRUCT
    LOCAL szFullPath[260]:BYTE
    
    ; Build full path
    INVOKE lstrcpyA, ADDR szFullPath, pszPath
    INVOKE lstrcatA, ADDR szFullPath, "\"
    INVOKE lstrcatA, ADDR szFullPath, pszFile
    
    ; Setup insert struct
    INVOKE RtlZeroMemory, ADDR tvi, SIZEOF TV_INSERTSTRUCT
    mov tvi.item.mask, TVIF_TEXT OR TVIF_IMAGE OR TVIF_SELECTEDIMAGE
    mov tvi.item.pszText, pszFile
    mov tvi.item.iImage, 2  ; File icon
    mov tvi.item.iSelectedImage, 2
    mov tvi.hParent, TVI_ROOT  ; Would use selected folder in full impl
    mov tvi.hInsertAfter, TVI_LAST
    
    ; Insert item
    INVOKE SendMessage, g_hProjectTree, TVM_INSERTITEM, 0, ADDR tvi
    
    ret
    
RawrXD_ProjectTree_AddFile ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_OnClick - Handle tree item selection
;----------------------------------------------------------------------
RawrXD_ProjectTree_OnClick PROC hItem:QWORD
    LOCAL tvItem:TVITEM
    LOCAL szPath[512]:BYTE
    
    ; Get item info
    mov tvItem.mask, TVIF_TEXT
    mov tvItem.hItem, hItem
    mov tvItem.pszText, ADDR szPath
    mov tvItem.cchTextMax, 512
    
    INVOKE SendMessage, g_hProjectTree, TVM_GETITEM, 0, ADDR tvItem
    
    ; Store selected node
    mov g_selectedTreeNode, hItem
    
    ; Load file in editor
    INVOKE RawrXD_Editor_LoadFile, ADDR szPath
    
    ret
    
RawrXD_ProjectTree_OnClick ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_BeginDrag - Start drag operation
;----------------------------------------------------------------------
RawrXD_ProjectTree_BeginDrag PROC hItem:QWORD, hParent:QWORD
    mov g_draggedNode, hItem
    mov g_isDraggingNode, 1
    INVOKE SetCapture, hParent
    ret
RawrXD_ProjectTree_BeginDrag ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_EndDrag - End drag operation
;----------------------------------------------------------------------
RawrXD_ProjectTree_EndDrag PROC
    mov g_isDraggingNode, 0
    INVOKE ReleaseCapture
    ret
RawrXD_ProjectTree_EndDrag ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_ContextMenu - Show right-click context menu
;----------------------------------------------------------------------
RawrXD_ProjectTree_ContextMenu PROC hItem:QWORD, x:QWORD, y:QWORD, hParent:QWORD
    LOCAL hMenu:QWORD
    LOCAL menuId:QWORD
    
    ; Create context menu
    INVOKE CreatePopupMenu
    mov hMenu, rax
    
    ; Add menu items
    INVOKE AppendMenuA, hMenu, MFT_STRING, 1001, "New File"
    INVOKE AppendMenuA, hMenu, MFT_STRING, 1002, "New Folder"
    INVOKE AppendMenuA, hMenu, MFT_SEPARATOR, 0, NULL
    INVOKE AppendMenuA, hMenu, MFT_STRING, 1003, "Open in Explorer"
    INVOKE AppendMenuA, hMenu, MFT_SEPARATOR, 0, NULL
    INVOKE AppendMenuA, hMenu, MFT_STRING, 1004, "Rename"
    INVOKE AppendMenuA, hMenu, MFT_STRING, 1005, "Delete"
    INVOKE AppendMenuA, hMenu, MFT_SEPARATOR, 0, NULL
    INVOKE AppendMenuA, hMenu, MFT_STRING, 1006, "Properties"
    
    ; Track menu selection
    INVOKE TrackPopupMenu, hMenu, TPM_LEFTBUTTON, x, y, 0, hParent, NULL
    mov menuId, rax
    
    ; Handle menu selection based on ID
    cmp menuId, 1001
    je @@new_file
    cmp menuId, 1002
    je @@new_folder
    cmp menuId, 1003
    je @@open_explorer
    cmp menuId, 1004
    je @@rename
    cmp menuId, 1005
    je @@delete
    cmp menuId, 1006
    je @@properties
    
    jmp @@cleanup
    
@@new_file:
    ; Implementation: Create new file
    jmp @@cleanup
    
@@new_folder:
    ; Implementation: Create new folder
    jmp @@cleanup
    
@@open_explorer:
    ; Implementation: Open in Explorer
    jmp @@cleanup
    
@@rename:
    ; Enable inline rename
    INVOKE SendMessage, g_hProjectTree, TVM_EDITLABEL, 0, hItem
    jmp @@cleanup
    
@@delete:
    ; Implementation: Delete file/folder
    jmp @@cleanup
    
@@properties:
    ; Implementation: Show properties dialog
    jmp @@cleanup
    
@@cleanup:
    INVOKE DestroyMenu, hMenu
    ret
    
RawrXD_ProjectTree_ContextMenu ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_Refresh - Reload project tree
;----------------------------------------------------------------------
RawrXD_ProjectTree_Refresh PROC pszPath:QWORD
    ; Clear tree
    INVOKE SendMessage, g_hProjectTree, TVM_DELETEITEM, 0, TVI_ROOT
    
    ; Reload
    INVOKE RawrXD_ProjectTree_LoadProject, pszPath
    
    ret
    
RawrXD_ProjectTree_Refresh ENDP

END

;----------------------------------------------------------------------
; RawrXD_ProjectTree_OpenFolder - Open folder in project tree
;----------------------------------------------------------------------
RawrXD_ProjectTree_OpenFolder PROC pFolderPath:QWORD
    ; Store path
    INVOKE lstrcpy, ADDR g_currentPath, pFolderPath
    
    ; Refresh tree view
    INVOKE RawrXD_ProjectTree_Refresh
    
    ret
    
RawrXD_ProjectTree_OpenFolder ENDP

;----------------------------------------------------------------------
; RawrXD_ProjectTree_Refresh - Refresh tree view from file system
;----------------------------------------------------------------------
RawrXD_ProjectTree_Refresh PROC
    ; Clear existing items
    INVOKE SendMessage, g_hProjectTree, TVM_DELETEITEM, 0, TVI_ROOT
    
    ; Re-add root
    INVOKE RawrXD_ProjectTree_AddRoot
    
    ret
    
RawrXD_ProjectTree_Refresh ENDP

szRootProject           DB "Project", 0
icc                     INITCOMMONCONTROLSEX <SIZEOF INITCOMMONCONTROLSEX, ICC_TREEVIEW_CLASSES>

END
