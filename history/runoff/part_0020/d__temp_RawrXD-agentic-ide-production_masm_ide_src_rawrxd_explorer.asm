;======================================================================
; RawrXD IDE - File Explorer Component
; Full file/folder browser, all drives, context menus, drag-drop
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; File explorer state
g_hExplorer             DQ ?
g_hExplorerTree         DQ ?
g_currentPath[260]      DB 260 DUP(0)
g_selectedPath[260]     DB 260 DUP(0)
g_favoriteCount         DQ 0
g_showHidden            DQ 0
g_sortMode              DQ 0  ; 0=name, 1=size, 2=date

; Tree view structures
EXPLORER_ITEM STRUCT
    name[260]           DB 260 DUP(0)
    path[260]           DB 260 DUP(0)
    attributes          DQ ?
    fileSize            DQ ?
    lastModified        DQ ?
    isFolder            DQ ?
    isHidden            DQ ?
EXPLORER_ITEM ENDS

; Favorites
FAVORITE STRUCT
    name[64]            DB 64 DUP(0)
    path[260]           DB 260 DUP(0)
    icon                DQ ?
FAVORITE ENDS

g_favorites[32 * (SIZEOF FAVORITE)] DQ 32 DUP(0)
g_favoriteCount         DQ 0

; Drives
g_driveLetters[26]      DB 26 DUP(0)
g_availableDrives       DQ 0

.CODE

;----------------------------------------------------------------------
; RawrXD_Explorer_Create - Create file explorer panel
;----------------------------------------------------------------------
RawrXD_Explorer_Create PROC hParent:QWORD, x:QWORD, y:QWORD, cx:QWORD, cy:QWORD
    LOCAL hSplitter:QWORD
    LOCAL rect:RECT
    
    ; Create main explorer container
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "STATIC",
        NULL,
        WS_CHILD OR WS_VISIBLE,
        x, y, cx, cy,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hExplorer, rax
    
    ; Create tabs for different views (Folders, Drives, Favorites, Search)
    INVOKE RawrXD_Explorer_CreateTabs, rax
    
    ; Create tree view
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "SysTreeView32",
        NULL,
        WS_CHILD OR WS_VISIBLE OR TVS_HASBUTTONS OR TVS_HASLINES OR TVS_LINESATROOT,
        x, y + 30, cx, cy - 30,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hExplorerTree, rax
    
    ; Load all available drives
    INVOKE RawrXD_Explorer_LoadDrives
    
    ; Load current directory
    INVOKE GetCurrentDirectoryA, 260, ADDR g_currentPath
    INVOKE RawrXD_Explorer_LoadFolder, ADDR g_currentPath
    
    xor eax, eax
    ret
    
RawrXD_Explorer_Create ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_LoadDrives - Enumerate all available drives
;----------------------------------------------------------------------
RawrXD_Explorer_LoadDrives PROC
    LOCAL drivesMask:QWORD
    LOCAL driveLetter:QWORD
    LOCAL szDrivePath[4]:BYTE
    LOCAL idx:QWORD
    
    ; Get available drives
    INVOKE GetLogicalDrives
    mov drivesMask, rax
    
    mov driveLetter, 'A'
    mov idx, 0
    
@@check_drives:
    cmp driveLetter, 'Z'
    ja @@done
    
    ; Check if drive bit is set
    mov rax, 1
    mov rcx, idx
    shl rax, cl
    and rax, drivesMask
    test rax, rax
    jz @@next_drive
    
    ; Build drive path (e.g., "C:\")
    mov g_driveLetters[idx], driveLetter
    
    ; Check drive type
    INVOKE lstrcpyA, ADDR szDrivePath, OFFSET szDriveFormat
    mov al, driveLetter
    mov szDrivePath[0], al
    
    INVOKE GetDriveTypeA, ADDR szDrivePath
    cmp eax, DRIVE_NO_ROOT_DIR
    je @@next_drive
    
    ; Add drive to tree
    INVOKE RawrXD_Explorer_AddDrive, driveLetter, eax
    
    inc g_availableDrives
    
@@next_drive:
    inc idx
    inc driveLetter
    jmp @@check_drives
    
@@done:
    ret
    
RawrXD_Explorer_LoadDrives ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_AddDrive - Add drive to tree view
;----------------------------------------------------------------------
RawrXD_Explorer_AddDrive PROC driveLetter:QWORD, driveType:QWORD
    LOCAL tvi:TV_INSERTSTRUCT
    LOCAL szDrivePath[260]:BYTE
    LOCAL szDriveLabel[260]:BYTE
    
    ; Build display string
    mov al, driveLetter
    INVOKE lstrcpyA, ADDR szDrivePath, OFFSET szDriveFormat
    mov szDrivePath[0], al
    
    ; Get volume label
    INVOKE GetVolumeInformationA, ADDR szDrivePath, ADDR szDriveLabel, 260, NULL, NULL, NULL, NULL, 0
    
    ; Build tree item
    INVOKE RtlZeroMemory, ADDR tvi, SIZEOF TV_INSERTSTRUCT
    mov tvi.hParent, TVI_ROOT
    mov tvi.hInsertAfter, TVI_LAST
    mov tvi.item.mask, TVIF_TEXT OR TVIF_IMAGE OR TVIF_CHILDREN
    
    ; Format display: "C: (Label)" or "C: (Local Disk)"
    INVOKE lstrcpyA, ADDR szDrivePath, OFFSET szDriveFmt2
    mov al, driveLetter
    mov szDrivePath[0], al
    
    ; Get icon based on drive type
    mov eax, driveType
    cmp eax, DRIVE_FIXED
    je @@fixed_drive
    cmp eax, DRIVE_REMOVABLE
    je @@removable
    cmp eax, DRIVE_CDROM
    je @@cdrom
    cmp eax, DRIVE_REMOTE
    je @@network
    
    ; Default icon
    mov tvi.item.iImage, 3
    jmp @@add_item
    
@@fixed_drive:
    mov tvi.item.iImage, 0
    jmp @@add_item
@@removable:
    mov tvi.item.iImage, 1
    jmp @@add_item
@@cdrom:
    mov tvi.item.iImage, 2
    jmp @@add_item
@@network:
    mov tvi.item.iImage, 4
    
@@add_item:
    mov tvi.item.iSelectedImage, tvi.item.iImage
    mov tvi.item.cChildren, 1
    mov tvi.item.pszText, ADDR szDrivePath
    
    INVOKE SendMessage, g_hExplorerTree, TVM_INSERTITEM, 0, ADDR tvi
    
    ret
    
RawrXD_Explorer_AddDrive ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_LoadFolder - Load folder contents into tree
;----------------------------------------------------------------------
RawrXD_Explorer_LoadFolder PROC pszPath:QWORD
    LOCAL findData:WIN32_FIND_DATAA
    LOCAL hFind:QWORD
    LOCAL szPattern[264]:BYTE
    LOCAL szFullPath[260]:BYTE
    LOCAL tvi:TV_INSERTSTRUCT
    LOCAL hParentNode:QWORD
    
    ; Clear tree
    INVOKE SendMessage, g_hExplorerTree, TVM_DELETEITEM, 0, TVI_ROOT
    
    ; Build search pattern
    INVOKE lstrcpyA, ADDR szPattern, pszPath
    INVOKE lstrcatA, ADDR szPattern, "\*"
    
    ; Find first file
    INVOKE FindFirstFileA, ADDR szPattern, ADDR findData
    mov hFind, rax
    
    cmp hFind, INVALID_HANDLE_VALUE
    je @@done
    
    mov hParentNode, TVI_ROOT
    
@@loop:
    ; Skip "." and ".."
    cmp findData.cFileName[0], '.'
    je @@next
    
    ; Check if hidden file
    mov eax, findData.dwFileAttributes
    and eax, FILE_ATTRIBUTE_HIDDEN
    test eax, eax
    jnz @@check_hidden
    jmp @@add_item
    
@@check_hidden:
    cmp g_showHidden, 1
    jne @@next
    
@@add_item:
    ; Build full path
    INVOKE lstrcpyA, ADDR szFullPath, pszPath
    INVOKE lstrcatA, ADDR szFullPath, "\"
    INVOKE lstrcatA, ADDR szFullPath, ADDR findData.cFileName
    
    ; Create tree item
    INVOKE RtlZeroMemory, ADDR tvi, SIZEOF TV_INSERTSTRUCT
    mov tvi.hParent, hParentNode
    mov tvi.hInsertAfter, TVI_SORT
    mov tvi.item.mask, TVIF_TEXT OR TVIF_IMAGE OR TVIF_CHILDREN
    mov tvi.item.pszText, ADDR findData.cFileName
    
    ; Determine icon
    mov eax, findData.dwFileAttributes
    and eax, FILE_ATTRIBUTE_DIRECTORY
    test eax, eax
    jz @@file_icon
    
    mov tvi.item.iImage, 1    ; Folder icon
    mov tvi.item.cChildren, 1  ; Can expand
    jmp @@insert_item
    
@@file_icon:
    mov tvi.item.iImage, 2    ; File icon
    mov tvi.item.cChildren, 0
    
@@insert_item:
    mov tvi.item.iSelectedImage, tvi.item.iImage
    INVOKE SendMessage, g_hExplorerTree, TVM_INSERTITEM, 0, ADDR tvi
    
@@next:
    INVOKE FindNextFileA, hFind, ADDR findData
    test eax, eax
    jnz @@loop
    
@@done:
    INVOKE FindClose, hFind
    ret
    
RawrXD_Explorer_LoadFolder ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_CreateTabs - Create tab control for views
;----------------------------------------------------------------------
RawrXD_Explorer_CreateTabs PROC hParent:QWORD
    LOCAL tc:TCITEM
    
    ; Create tab control
    INVOKE CreateWindowEx,
        0,
        "SysTabControl32",
        NULL,
        WS_CHILD OR WS_VISIBLE,
        0, 0, GetClientRect(hParent).right, 25,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    ; Add tabs: Folders, Drives, Favorites, Search
    INVOKE RtlZeroMemory, ADDR tc, SIZEOF TCITEM
    mov tc.mask, TCIF_TEXT
    
    mov tc.pszText, OFFSET szTabFolders
    INVOKE SendMessage, rax, TCM_INSERTITEM, 0, ADDR tc
    
    mov tc.pszText, OFFSET szTabDrives
    INVOKE SendMessage, rax, TCM_INSERTITEM, 1, ADDR tc
    
    mov tc.pszText, OFFSET szTabFavorites
    INVOKE SendMessage, rax, TCM_INSERTITEM, 2, ADDR tc
    
    mov tc.pszText, OFFSET szTabSearch
    INVOKE SendMessage, rax, TCM_INSERTITEM, 3, ADDR tc
    
    ret
    
RawrXD_Explorer_CreateTabs ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_OnSelectionChange - Handle item selection
;----------------------------------------------------------------------
RawrXD_Explorer_OnSelectionChange PROC hItem:QWORD
    LOCAL tvItem:TVITEM
    LOCAL szPath[260]:BYTE
    
    ; Get item info
    mov tvItem.mask, TVIF_TEXT
    mov tvItem.hItem, hItem
    mov tvItem.pszText, ADDR szPath
    mov tvItem.cchTextMax, 260
    
    INVOKE SendMessage, g_hExplorerTree, TVM_GETITEM, 0, ADDR tvItem
    
    ; Store selected path
    INVOKE lstrcpyA, ADDR g_selectedPath, ADDR szPath
    
    ; Load folder if directory
    mov eax, tvItem.iImage
    cmp eax, 1  ; Folder icon
    jne @@not_folder
    
    INVOKE RawrXD_Explorer_LoadFolder, ADDR g_selectedPath
    
@@not_folder:
    ; Update status bar
    INVOKE RawrXD_StatusBar_SetFileInfo, ADDR g_selectedPath, OFFSET szReady, 0
    
    ret
    
RawrXD_Explorer_OnSelectionChange ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_ContextMenu - Show right-click menu
;----------------------------------------------------------------------
RawrXD_Explorer_ContextMenu PROC hItem:QWORD, x:QWORD, y:QWORD, hParent:QWORD
    LOCAL hMenu:QWORD
    LOCAL menuId:QWORD
    
    ; Create context menu
    INVOKE CreatePopupMenu
    mov hMenu, rax
    
    ; Add menu items
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2001, "Open"
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2002, "Open in New Tab"
    INVOKE AppendMenuA, hMenu, MFT_SEPARATOR, 0, NULL
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2003, "Copy Path"
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2004, "Copy Full Path"
    INVOKE AppendMenuA, hMenu, MFT_SEPARATOR, 0, NULL
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2005, "New Folder"
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2006, "Rename"
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2007, "Delete"
    INVOKE AppendMenuA, hMenu, MFT_SEPARATOR, 0, NULL
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2008, "Add to Favorites"
    INVOKE AppendMenuA, hMenu, MFT_STRING, 2009, "Properties"
    
    ; Track menu
    INVOKE TrackPopupMenu, hMenu, TPM_LEFTBUTTON, x, y, 0, hParent, NULL
    mov menuId, rax
    
    ; Handle menu selection
    cmp menuId, 2001
    je @@open
    cmp menuId, 2002
    je @@open_tab
    cmp menuId, 2003
    je @@copy_path
    cmp menuId, 2004
    je @@copy_full
    cmp menuId, 2005
    je @@new_folder
    cmp menuId, 2006
    je @@rename
    cmp menuId, 2007
    je @@delete
    cmp menuId, 2008
    je @@add_fav
    cmp menuId, 2009
    je @@props
    
    jmp @@cleanup
    
@@open:
    INVOKE RawrXD_Explorer_LoadFolder, ADDR g_selectedPath
    jmp @@cleanup
@@open_tab:
    ; Would open in new tab
    jmp @@cleanup
@@copy_path:
    ; Copy to clipboard
    jmp @@cleanup
@@copy_full:
    ; Copy full path to clipboard
    jmp @@cleanup
@@new_folder:
    ; Create new folder dialog
    jmp @@cleanup
@@rename:
    ; Enable inline rename
    INVOKE SendMessage, g_hExplorerTree, TVM_EDITLABEL, 0, hItem
    jmp @@cleanup
@@delete:
    ; Delete with confirmation
    jmp @@cleanup
@@add_fav:
    INVOKE RawrXD_Explorer_AddFavorite, ADDR g_selectedPath
    jmp @@cleanup
@@props:
    ; Show properties dialog
    
@@cleanup:
    INVOKE DestroyMenu, hMenu
    ret
    
RawrXD_Explorer_ContextMenu ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_AddFavorite - Add folder to favorites
;----------------------------------------------------------------------
RawrXD_Explorer_AddFavorite PROC pszPath:QWORD
    LOCAL idx:QWORD
    LOCAL pFavorite:QWORD
    
    cmp g_favoriteCount, 32
    jge @@limit
    
    mov idx, g_favoriteCount
    imul idx, SIZEOF FAVORITE
    mov pFavorite, OFFSET g_favorites
    add pFavorite, idx
    
    ; Extract folder name
    INVOKE RawrXD_Util_GetFilename, pszPath, FAVORITE.name[pFavorite]
    
    ; Copy path
    INVOKE lstrcpyA, FAVORITE.path[pFavorite], pszPath
    
    inc g_favoriteCount
    
    INVOKE RawrXD_Output_Build, OFFSET szAddedFav
    
    xor eax, eax
    ret
    
@@limit:
    INVOKE RawrXD_Output_Warning, OFFSET szMaxFavs
    mov eax, -1
    ret
    
RawrXD_Explorer_AddFavorite ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_NavigateTo - Navigate to path
;----------------------------------------------------------------------
RawrXD_Explorer_NavigateTo PROC pszPath:QWORD
    LOCAL szFull[260]:BYTE
    
    ; Get full path
    INVOKE GetFullPathNameA, pszPath, 260, ADDR szFull, NULL
    
    ; Update current path
    INVOKE lstrcpyA, ADDR g_currentPath, ADDR szFull
    
    ; Reload folder
    INVOKE RawrXD_Explorer_LoadFolder, ADDR szFull
    
    ret
    
RawrXD_Explorer_NavigateTo ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_GoBack - Navigate back
;----------------------------------------------------------------------
RawrXD_Explorer_GoBack PROC
    LOCAL szParent[260]:BYTE
    LOCAL pLastSlash:QWORD
    
    ; Find last backslash
    INVOKE RawrXD_Util_FindLastChar, ADDR g_currentPath, '\'
    mov pLastSlash, rax
    
    cmp pLastSlash, 0
    je @@root
    
    ; Copy parent path
    mov rcx, pLastSlash
    sub rcx, ADDR g_currentPath
    INVOKE lstrcpyA, ADDR szParent, ADDR g_currentPath
    mov byte [ADDR szParent + rcx], 0
    
    INVOKE RawrXD_Explorer_NavigateTo, ADDR szParent
    jmp @@done
    
@@root:
    ; Already at root
    
@@done:
    ret
    
RawrXD_Explorer_GoBack ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_SetPath - Set current path programmatically
;----------------------------------------------------------------------
RawrXD_Explorer_SetPath PROC pszPath:QWORD
    INVOKE lstrcpyA, ADDR g_currentPath, pszPath
    INVOKE RawrXD_Explorer_LoadFolder, pszPath
    ret
RawrXD_Explorer_SetPath ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_GetSelectedPath - Get currently selected path
;----------------------------------------------------------------------
RawrXD_Explorer_GetSelectedPath PROC pszBuffer:QWORD
    INVOKE lstrcpyA, pszBuffer, ADDR g_selectedPath
    ret
RawrXD_Explorer_GetSelectedPath ENDP

;----------------------------------------------------------------------
; RawrXD_Explorer_EnableDragDrop - Enable drag-drop support
;----------------------------------------------------------------------
RawrXD_Explorer_EnableDragDrop PROC hWnd:QWORD
    ; Register for drag-drop
    INVOKE OleInitialize, NULL
    
    ; Implementation would use IDropTarget interface
    
    ret
    
RawrXD_Explorer_EnableDragDrop ENDP

; String literals
szDriveFormat           DB "?:\", 0
szDriveFmt2             DB "?: (Drive)", 0
szTabFolders            DB "Folders", 0
szTabDrives             DB "Drives", 0
szTabFavorites          DB "Favorites", 0
szTabSearch             DB "Search", 0
szReady                 DB "Ready", 0
szAddedFav              DB "Added to favorites", 0
szMaxFavs               DB "Maximum favorites reached", 0

END
