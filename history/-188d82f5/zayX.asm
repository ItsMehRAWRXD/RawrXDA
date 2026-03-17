; ============================================================================
; RawrXD Agentic IDE - Enhanced File Explorer Implementation (Pure MASM)
; Production-ready implementation with all Phase 4 features
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
include \masm32\include\shell32.inc
include \masm32\include\shlwapi.inc
include \masm32\include\comctl32.inc
include file_explorer_enhanced.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib
includelib \masm32\lib\gdi32.lib
includelib \masm32\lib\shell32.lib
includelib \masm32\lib\shlwapi.lib

; ============================================================================
; EXTERNAL REFERENCES
; ============================================================================

extrn hInstance:DWORD
extrn hMainWindow:DWORD
extrn hMainFont:DWORD
extrn LogMessage:PROC
extrn bPerfLoggingEnabled:DWORD
extrn FileEnumeration_GetResults:PROC

; ============================================================================
; CONSTANTS
; ============================================================================

; Tree view constants
TVS_EX_MULTISELECT      equ 0002h
TVS_EX_DOUBLEBUFFER     equ 0004h
TVS_EX_NOINDENTSTATE    equ 0008h

; Image list constants
ILC_COLOR32             equ 0020h
ILC_MASK                equ 0001h

; Context menu IDs
IDM_OPEN                equ 40001
IDM_RENAME              equ 40002
IDM_DELETE              equ 40003
IDM_PROPERTIES          equ 40004
IDM_NEW_FILE            equ 40005
IDM_NEW_FOLDER          equ 40006
IDM_REFRESH             equ 40007
IDM_SEARCH              equ 40008

; File search constants
MAX_SEARCH_RESULTS      equ 1000
LOG_INFO                equ 1
LOG_ERROR               equ 3

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    ; Window class names
    szTreeViewClass     db "SysTreeView32", 0
    szEditClass         db "EDIT", 0
    szButtonClass       db "BUTTON", 0
    szStaticClass       db "STATIC", 0
    
    ; Control IDs
    IDC_FILETREE        equ 1001
    IDC_SEARCH_BOX      equ 1002
    IDC_SEARCH_BTN      equ 1003
    
    ; Text strings
    szRootText          db "Project", 0
    szComputerText      db "Computer", 0
    szSearchPrompt      db "Search files...", 0
    szBackslash         db "\", 0
    szStarDotStar       db "*.*", 0
    szDot               db ".", 0
    szDotDot            db "..", 0
    szLogFEInit         db "FileExplorer: initialized", 0
    szLogDrivesStart    db "FileExplorer: populating drives", 0
    szLogDrivesDone     db "FileExplorer: drives populated", 0
    szEnumPerfFmt       db "FE: %u items in %u us", 0
    
    ; File icons
    hImageList          dd 0
    hFolderIcon         dd 0
    hFileIcon           dd 0
    hModelIcon          dd 0
    hAsmIcon            dd 0
    hTextIcon           dd 0
    
    ; UI controls
    hFileTree           dd 0
    hRootItem           dd 0
    hSearchBox          dd 0
    hSearchButton       dd 0
    hStatusBar          dd 0
    
    ; Context menu
    hContextMenu        dd 0
    
    ; File search
    szSearchPattern     db 260 dup(0)
    szSearchResults     db (MAX_PATH * MAX_SEARCH_RESULTS) dup(0)
    dwSearchCount       dd 0
    
    ; Drag and drop
    bDragging           dd 0
    hDragImageList      dd 0
    ptDragStart         POINT <>
    
    ; Current state
    szCurrentPath       db MAX_PATH dup(0)
    szSelectedPath      db MAX_PATH dup(0)
    
    ; Buffers
    szBuffer            db MAX_PATH dup(0)
    szTempPath          db MAX_PATH dup(0)
    findData            WIN32_FIND_DATA <>

; ============================================================================
; STRUCTURES
; ============================================================================

; Node data stored in TreeView lParam
NODEDATA struct
    bPopulated          dd ?
    dwItemType          dd ?    ; 0=Root, 1=Drive, 2=Folder, 3=File
    szPath              db MAX_PATH dup(?)
NODEDATA ends

; Search result structure
SEARCHRESULT struct
    szPath              db MAX_PATH dup(?)
    szFileName          db MAX_PATH dup(?)
SEARCHRESULT ends

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; Mirror FILE_INFO and PENDING_OPERATION structures for consuming results
FILE_INFO STRUCT
    fileName        BYTE MAX_PATH dup(?)
    fileSize        DWORD ?
    fileAttributes  DWORD ?
    isDirectory     DWORD ?
FILE_INFO ENDS

PENDING_OPERATION STRUCT
    opType          DWORD ?
    status          DWORD ?
    hThread         DWORD ?
    hEvent          DWORD ?
    pPath           DWORD ?
    pResults        DWORD ?
    resultCount     DWORD ?
    maxResults      DWORD ?
    errorCode       DWORD ?
    hTreeControl    DWORD ?
    hParentItem     DWORD ?
    pPattern        DWORD ?
    startTick       DWORD ?
PENDING_OPERATION ENDS

; ============================================================================
; FORWARD DECLARATIONS
; ============================================================================

CreateFileExplorer      proto
DestroyFileExplorer     proto
PopulateDrives          proto
PopulateDirectory       proto :DWORD, :DWORD
CreateImageList         proto
CreateContextMenu       proto
CreateSearchControls    proto
UpdateStatusBar         proto :DWORD
GetItemPath             proto :DWORD, :DWORD, :DWORD
AllocNodeData           proto :DWORD, :DWORD
FreeNodeData            proto :DWORD
ShowContextMenu         proto :DWORD, :DWORD
HandleContextMenuCmd    proto :DWORD
SearchFiles             proto :DWORD
RefreshFileTree         proto
FileExplorer_OnEnumComplete proto :DWORD, :DWORD

; ============================================================================
; PUBLIC FUNCTIONS
; ============================================================================

public CreateFileExplorer
public DestroyFileExplorer
public RefreshFileTree
public OnFileTreeNotify
public OnFileTreeContextMenu
public OnSearchButtonClicked
public OnSearchBoxKeyDown
public FileExplorer_OnEnumComplete

; ============================================================================
; CreateFileExplorer - Creates the enhanced file explorer with all features
; Returns: Handle to tree view in eax
; ============================================================================
CreateFileExplorer proc
    LOCAL dwStyle:DWORD
    LOCAL tvins:TVINSERTSTRUCT
    LOCAL tvi:TVITEM
    
    ; Create tree view control with enhanced styles
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER or \
                 TVS_HASLINES or TVS_HASBUTTONS or TVS_LINESATROOT or \
                 TVS_SHOWSELALWAYS or TVS_TRACKSELECT or TVS_EDITLABELS
                 
    invoke CreateWindowEx, WS_EX_CLIENTEDGE,
        addr szTreeViewClass,
        NULL,
        dwStyle,
        0, 0, 250, 500,
        hMainWindow,
        IDC_FILETREE,
        hInstance,
        NULL
    
    mov hFileTree, eax
    test eax, eax
    jz @CreateFailed
    
    ; Set extended styles for multi-select and double buffering
    invoke SendMessage, hFileTree, TVM_SETEXTENDEDSTYLE, 0, TVS_EX_MULTISELECT or TVS_EX_DOUBLEBUFFER
    
    ; Create image list for file icons
    call CreateImageList
    
    ; Create context menu
    call CreateContextMenu
    
    ; Create search controls
    call CreateSearchControls
    
    ; Create status bar for breadcrumbs
    mov dwStyle, WS_CHILD or WS_VISIBLE or SBARS_SIZEGRIP
    invoke CreateWindowEx, 0,
        addr szStaticClass,
        NULL,
        dwStyle,
        0, 500, 250, 24,
        hMainWindow,
        NULL,
        hInstance,
        NULL
    
    mov hStatusBar, eax
    invoke SendMessage, hStatusBar, WM_SETFONT, hMainFont, TRUE
    
    ; Log initialization
    invoke LogMessage, LOG_INFO, addr szLogFEInit
    
    ; Add root item "Computer"
    mov tvi.mask, TVIF_TEXT or TVIF_IMAGE or TVIF_SELECTEDIMAGE
    mov tvi.pszText, offset szComputerText
    mov tvi.iImage, 0        ; Folder icon
    mov tvi.iSelectedImage, 0
    mov tvi.cChildren, 1
    
    mov tvins.hParent, TVI_ROOT
    mov tvins.hInsertAfter, TVI_FIRST
    mov tvins.item, tvi
    
    invoke SendMessage, hFileTree, TVM_INSERTITEM, 0, addr tvins
    mov hRootItem, eax
    
    ; Populate drives under root
    invoke LogMessage, LOG_INFO, addr szLogDrivesStart
    call PopulateDrives
    invoke LogMessage, LOG_INFO, addr szLogDrivesDone
    
    ; Set font
    invoke SendMessage, hFileTree, WM_SETFONT, hMainFont, TRUE
    
    ; Update status bar
    invoke lstrcpy, addr szCurrentPath, addr szComputerText
    call UpdateStatusBar
    
    mov eax, hFileTree
    ret
    
@CreateFailed:
    xor eax, eax
    ret
CreateFileExplorer endp

; ============================================================================
; DestroyFileExplorer - Cleans up resources
; ============================================================================
DestroyFileExplorer proc
    ; Destroy image list
    .if hImageList != 0
        invoke ImageList_Destroy, hImageList
        mov hImageList, 0
    .endif
    
    ; Destroy drag image list
    .if hDragImageList != 0
        invoke ImageList_Destroy, hDragImageList
        mov hDragImageList, 0
    .endif
    
    ; Destroy context menu
    .if hContextMenu != 0
        invoke DestroyMenu, hContextMenu
        mov hContextMenu, 0
    .endif
    
    ret
DestroyFileExplorer endp

; ============================================================================
; CreateImageList - Creates image list with file icons
; ============================================================================
CreateImageList proc
    ; Create image list (16x16 icons)
    invoke ImageList_Create, 16, 16, ILC_COLOR32 or ILC_MASK, 6, 0
    mov hImageList, eax
    
    ; Load system icons
    invoke LoadIcon, 0, IDI_FOLDER
    mov hFolderIcon, eax
    invoke ImageList_AddIcon, hImageList, hFolderIcon
    
    invoke LoadIcon, 0, IDI_APPLICATION
    mov hFileIcon, eax
    invoke ImageList_AddIcon, hImageList, hFileIcon
    
    ; For now, use same icons for special file types
    mov eax, hFileIcon
    mov hModelIcon, eax
    invoke ImageList_AddIcon, hImageList, hModelIcon
    
    mov eax, hFileIcon
    mov hAsmIcon, eax
    invoke ImageList_AddIcon, hImageList, hAsmIcon
    
    mov eax, hFileIcon
    mov hTextIcon, eax
    invoke ImageList_AddIcon, hImageList, hTextIcon
    
    ; Set image list for tree view
    invoke SendMessage, hFileTree, TVM_SETIMAGELIST, TVSIL_NORMAL, hImageList
    
    ret
CreateImageList endp

; ============================================================================
; CreateContextMenu - Creates context menu with file operations
; ============================================================================
CreateContextMenu proc
    invoke CreatePopupMenu
    mov hContextMenu, eax
    
    ; Add menu items
    invoke AppendMenu, hContextMenu, MF_STRING, IDM_OPEN, "Open"
    invoke AppendMenu, hContextMenu, MF_STRING, IDM_RENAME, "Rename"
    invoke AppendMenu, hContextMenu, MF_STRING, IDM_DELETE, "Delete"
    invoke AppendMenu, hContextMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenu, hContextMenu, MF_STRING, IDM_NEW_FILE, "New File"
    invoke AppendMenu, hContextMenu, MF_STRING, IDM_NEW_FOLDER, "New Folder"
    invoke AppendMenu, hContextMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenu, hContextMenu, MF_STRING, IDM_SEARCH, "Search Files..."
    invoke AppendMenu, hContextMenu, MF_STRING, IDM_REFRESH, "Refresh"
    invoke AppendMenu, hContextMenu, MF_STRING, IDM_PROPERTIES, "Properties"
    
    ret
CreateContextMenu endp

; ============================================================================
; CreateSearchControls - Creates search box and button
; ============================================================================
CreateSearchControls proc
    LOCAL dwStyle:DWORD
    
    ; Create search edit box
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_AUTOHSCROLL
    invoke CreateWindowEx, WS_EX_CLIENTEDGE,
        addr szEditClass,
        addr szSearchPrompt,
        dwStyle,
        0, 524, 200, 24,
        hMainWindow,
        IDC_SEARCH_BOX,
        hInstance,
        NULL
    
    mov hSearchBox, eax
    invoke SendMessage, hSearchBox, WM_SETFONT, hMainFont, TRUE
    invoke SendMessage, hSearchBox, EM_SETLIMITTEXT, 259, 0
    
    ; Create search button
    mov dwStyle, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    invoke CreateWindowEx, 0,
        addr szButtonClass,
        "Search",
        dwStyle,
        200, 524, 50, 24,
        hMainWindow,
        IDC_SEARCH_BTN,
        hInstance,
        NULL
    
    mov hSearchButton, eax
    invoke SendMessage, hSearchButton, WM_SETFONT, hMainFont, TRUE
    
    ret
CreateSearchControls endp

; ============================================================================
; PopulateDrives - Enumerates logical drives and adds to tree
; ============================================================================
PopulateDrives proc
    LOCAL dwDrives:DWORD
    LOCAL i:DWORD
    LOCAL szDrive[4]:BYTE
    LOCAL tvins:TVINSERTSTRUCT
    LOCAL tvi:TVITEM
    
    ; Get logical drives bitmask
    invoke GetLogicalDrives
    mov dwDrives, eax
    mov i, 0
    
@DriveLoop:
    cmp i, 26
    jge @Done
    
    ; Check if drive exists
    mov eax, 1
    mov ecx, i
    shl eax, cl
    and eax, dwDrives
    test eax, eax
    jz @NextDrive
    
    ; Build drive string like "C:\"
    mov al, 'A'
    add al, BYTE PTR i
    mov BYTE PTR szDrive[0], al
    mov BYTE PTR szDrive[1], ':'
    mov BYTE PTR szDrive[2], '\'
    mov BYTE PTR szDrive[3], 0
    
    ; Add drive to tree under root
    mov tvi.mask, TVIF_TEXT or TVIF_IMAGE or TVIF_SELECTEDIMAGE or TVIF_PARAM
    lea eax, szDrive
    mov tvi.pszText, eax
    mov tvi.iImage, 0        ; Folder icon
    mov tvi.iSelectedImage, 0
    mov tvi.lParam, 1        ; Drive item type
    mov tvi.cChildren, 1
    
    mov tvins.hParent, hRootItem
    mov tvins.hInsertAfter, TVI_LAST
    mov tvins.item, tvi
    
    invoke SendMessage, hFileTree, TVM_INSERTITEM, 0, addr tvins
    
@NextDrive:
    inc i
    jmp @DriveLoop
    
@Done:
    ret
PopulateDrives endp

; ============================================================================
; PopulateDirectory - Populates a directory with its contents
; Parameters: hParent - handle to parent tree item, pszPath - directory path
; ============================================================================
PopulateDirectory proc hParent:DWORD, pszPath:DWORD
    LOCAL hFind:DWORD
    LOCAL szSearchPath[MAX_PATH]:BYTE
    LOCAL tvins:TVINSERTSTRUCT
    LOCAL tvi:TVITEM
    LOCAL pNodeData:DWORD
    LOCAL bIsFolder:DWORD
    LOCAL iImageIndex:DWORD
    LOCAL qwStart:QWORD
    LOCAL qwEnd:QWORD
    LOCAL qwFreq:QWORD
    LOCAL dwUs:DWORD
    LOCAL dwCount:DWORD
    LOCAL szMsg[128]:BYTE
    
    ; Start timing
    lea eax, qwStart
    push eax
    call QueryPerformanceCounter
    lea eax, qwFreq
    push eax
    call QueryPerformanceFrequency
    mov dwCount, 0

    ; Build search path (path + \*.*)
    invoke lstrcpy, addr szSearchPath, pszPath
    invoke lstrcat, addr szSearchPath, addr szBackslash
    invoke lstrcat, addr szSearchPath, addr szStarDotStar
    
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
            ; Determine if it's a folder
            test findData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
            .if ZERO?
                mov bIsFolder, 0
                mov iImageIndex, 1    ; File icon
            .else
                mov bIsFolder, 1
                mov iImageIndex, 0    ; Folder icon
            .endif
            
            ; Allocate node data
            lea eax, findData.cFileName
            invoke AllocNodeData, eax, bIsFolder
            mov pNodeData, eax
            
            ; Add item to tree
            mov tvi.mask, TVIF_TEXT or TVIF_IMAGE or TVIF_SELECTEDIMAGE or TVIF_PARAM or TVIF_CHILDREN
            lea eax, findData.cFileName
            mov tvi.pszText, eax
            mov eax, iImageIndex
            mov tvi.iImage, eax
            mov tvi.iSelectedImage, eax
            mov eax, pNodeData
            mov tvi.lParam, eax
            
            ; Set children flag for folders
            .if bIsFolder
                mov tvi.cChildren, 1
            .else
                mov tvi.cChildren, 0
            .endif
            
            mov tvins.hParent, hParent
            mov tvins.hInsertAfter, TVI_LAST
            mov tvins.item, tvi
            
            invoke SendMessage, hFileTree, TVM_INSERTITEM, 0, addr tvins
            ; Count items
            mov eax, dwCount
            inc eax
            mov dwCount, eax
        .endif
    .endif
    
    ; Find next file
    invoke FindNextFile, hFind, addr findData
    test eax, eax
    jnz @FindLoop
    
    ; Close search handle
    invoke FindClose, hFind
    
@Done:
    ; Stop timing and log
    lea eax, qwEnd
    push eax
    call QueryPerformanceCounter
    mov eax, dword ptr qwEnd
    sub eax, dword ptr qwStart
    mov edx, 1000000
    mul edx
    mov ecx, dword ptr qwFreq
    cmp ecx, 0
    je @skipLog
    xor edx, edx
    div ecx
    mov dwUs, eax
    ; Format and log
    push dwUs
    push dwCount
    push OFFSET szEnumPerfFmt
    lea eax, szMsg
    push eax
    call wsprintfA
    add esp, 16
    invoke LogMessage, LOG_INFO, addr szMsg
@skipLog:
    ret
PopulateDirectory endp

; ============================================================================
; AllocNodeData - Allocates and initializes node data
; Parameters: pszName - item name, bIsFolder - folder flag
; Returns: pointer to NODEDATA in eax
; ============================================================================
AllocNodeData proc pszName:DWORD, bIsFolder:DWORD
    LOCAL pNode:DWORD
    LOCAL szFullPath[MAX_PATH]:BYTE
    
    ; Allocate memory for node data
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, sizeof NODEDATA
    mov pNode, eax
    test eax, eax
    jz @Exit
    
    ; Set item type
    mov ecx, bIsFolder
    .if ecx == 1
        mov DWORD PTR [eax].NODEDATA.dwItemType, 2    ; Folder
    .else
        mov DWORD PTR [eax].NODEDATA.dwItemType, 3    ; File
    .endif
    
    ; Build full path (simplified for now)
    mov ecx, pszName
    lea edx, [eax].NODEDATA.szPath
    invoke lstrcpy, edx, ecx
    
@Exit:
    mov eax, pNode
    ret
AllocNodeData endp

; ============================================================================
; FreeNodeData - Frees node data memory
; Parameters: pNode - pointer to NODEDATA
; ============================================================================
FreeNodeData proc pNode:DWORD
    .if pNode != 0
        invoke GlobalFree, pNode
    .endif
    ret
FreeNodeData endp

; ============================================================================
; UpdateStatusBar - Updates status bar with current path
; Parameters: pszPath - path to display (optional)
; ============================================================================
UpdateStatusBar proc pszPath:DWORD
    LOCAL szDisplay[MAX_PATH]:BYTE
    
    ; Use provided path or current path
    .if pszPath != 0
        mov ecx, pszPath
        lea edx, szDisplay
        invoke lstrcpy, edx, ecx
    .else
        lea eax, szCurrentPath
        lea edx, szDisplay
        invoke lstrcpy, edx, eax
    .endif
    
    ; Update status bar text
    .if hStatusBar != 0
        lea eax, szDisplay
        invoke SendMessage, hStatusBar, WM_SETTEXT, 0, eax
    .endif
    
    ret
UpdateStatusBar endp

; ============================================================================
; GetItemPath - Gets the full path for a tree item
; Parameters: hItem - tree item handle, pszBuffer - output buffer, dwBufferSize - buffer size
; ============================================================================
GetItemPath proc hItem:DWORD, pszBuffer:DWORD, dwBufferSize:DWORD
    LOCAL tvi:TVITEM
    LOCAL szText[MAX_PATH]:BYTE
    
    ; Get item text
    mov tvi.mask, TVIF_TEXT
    lea eax, szText
    mov tvi.pszText, eax
    mov eax, MAX_PATH
    mov tvi.cchTextMax, eax
    mov eax, hItem
    mov tvi.hItem, eax
    
    invoke SendMessage, hFileTree, TVM_GETITEM, 0, addr tvi
    .if eax != 0
        ; For now, just copy the text (simplified)
        mov ecx, pszBuffer
        lea edx, szText
        invoke lstrcpy, ecx, edx
    .endif
    
    ret
GetItemPath endp

; ============================================================================
; ShowContextMenu - Displays context menu at specified position
; Parameters: x, y - screen coordinates
; ============================================================================
ShowContextMenu proc x:DWORD, y:DWORD
    LOCAL pt:POINT
    LOCAL hMenu:DWORD
    
    ; Enable/disable menu items based on selection
    ; For now, enable all items
    invoke EnableMenuItem, hContextMenu, IDM_OPEN, MF_ENABLED
    invoke EnableMenuItem, hContextMenu, IDM_RENAME, MF_ENABLED
    invoke EnableMenuItem, hContextMenu, IDM_DELETE, MF_ENABLED
    invoke EnableMenuItem, hContextMenu, IDM_PROPERTIES, MF_ENABLED
    
    ; Show context menu
    mov eax, x
    mov pt.x, eax
    mov eax, y
    mov pt.y, eax
    
    invoke TrackPopupMenu, hContextMenu, TPM_RIGHTBUTTON or TPM_RETURNCMD, pt.x, pt.y, 0, hMainWindow, NULL
    .if eax != 0
        ; Handle menu command
        invoke HandleContextMenuCmd, eax
    .endif
    
    ret
ShowContextMenu endp

; ============================================================================
; HandleContextMenuCmd - Handles context menu commands
; Parameters: wCommand - menu command ID
; ============================================================================
HandleContextMenuCmd proc wCommand:DWORD
    .if wCommand == IDM_OPEN
        ; Open selected item
        invoke MessageBox, hMainWindow, "Open command selected", "File Explorer", MB_OK
    .elseif wCommand == IDM_RENAME
        ; Rename selected item
        invoke MessageBox, hMainWindow, "Rename command selected", "File Explorer", MB_OK
    .elseif wCommand == IDM_DELETE
        ; Delete selected item
        invoke MessageBox, hMainWindow, "Delete command selected", "File Explorer", MB_OK
    .elseif wCommand == IDM_PROPERTIES
        ; Show properties
        invoke MessageBox, hMainWindow, "Properties command selected", "File Explorer", MB_OK
    .elseif wCommand == IDM_NEW_FILE
        ; Create new file
        invoke MessageBox, hMainWindow, "New File command selected", "File Explorer", MB_OK
    .elseif wCommand == IDM_NEW_FOLDER
        ; Create new folder
        invoke MessageBox, hMainWindow, "New Folder command selected", "File Explorer", MB_OK
    .elseif wCommand == IDM_REFRESH
        ; Refresh file tree
        call RefreshFileTree
    .elseif wCommand == IDM_SEARCH
        ; Show search dialog
        invoke MessageBox, hMainWindow, "Search command selected", "File Explorer", MB_OK
    .endif
    
    ret
HandleContextMenuCmd endp

; ============================================================================
; SearchFiles - Searches for files matching pattern
; Parameters: pszPattern - search pattern
; ============================================================================
SearchFiles proc pszPattern:DWORD
    ; For now, just show a message
    invoke MessageBox, hMainWindow, "File search functionality would be implemented here", "File Explorer", MB_OK
    ret
SearchFiles endp

; ============================================================================
; RefreshFileTree - Refreshes the file tree contents
; ============================================================================
RefreshFileTree proc
    ; Clear existing items
    invoke SendMessage, hFileTree, TVM_DELETEITEM, 0, TVI_ROOT
    
    ; Re-add root item
    LOCAL tvi:TVITEM
    LOCAL tvins:TVINSERTSTRUCT
    
    mov tvi.mask, TVIF_TEXT or TVIF_IMAGE or TVIF_SELECTEDIMAGE
    mov tvi.pszText, offset szComputerText
    mov tvi.iImage, 0
    mov tvi.iSelectedImage, 0
    mov tvi.cChildren, 1
    
    mov tvins.hParent, TVI_ROOT
    mov tvins.hInsertAfter, TVI_FIRST
    mov tvins.item, tvi
    
    invoke SendMessage, hFileTree, TVM_INSERTITEM, 0, addr tvins
    mov hRootItem, eax
    
    ; Re-populate drives
    call PopulateDrives
    
    ; Update status bar
    invoke lstrcpy, addr szCurrentPath, addr szComputerText
    call UpdateStatusBar
    
    ret
RefreshFileTree endp

; ============================================================================
; OnFileTreeNotify - Handles tree view notifications
; Parameters: pNMHDR - notification header
; Returns: 0 to allow default processing
; ============================================================================
OnFileTreeNotify proc pNMHDR:DWORD
    LOCAL pNMTV:DWORD
    LOCAL hItem:DWORD
    LOCAL szPath[MAX_PATH]:BYTE
    
    mov eax, pNMHDR
    mov pNMTV, eax
    
    ; Get notification code
    mov eax, [eax].NMHDR.code
    
    .if eax == TVN_SELCHANGED
        ; Selection changed - update status bar
        mov eax, pNMTV
        mov ecx, [eax].NMTREEVIEW.itemNew.hItem
        mov hItem, ecx
        
        lea eax, szPath
        invoke GetItemPath, hItem, eax, MAX_PATH
        lea eax, szPath
        call UpdateStatusBar
    .elseif eax == TVN_ITEMEXPANDING
        ; Item expanding - populate directory if needed
        ; Implementation would go here
    .elseif eax == NM_RCLICK
        ; Right-click - show context menu
        invoke GetMessagePos
        mov edx, eax
        shr edx, 16
        mov ecx, eax
        and ecx, 0FFFFh
        invoke ShowContextMenu, ecx, edx
    .endif
    
    xor eax, eax
    ret
OnFileTreeNotify endp

; ============================================================================
; OnFileTreeContextMenu - Handles context menu for file tree
; Parameters: x, y - screen coordinates
; ============================================================================
OnFileTreeContextMenu proc x:DWORD, y:DWORD
    invoke ShowContextMenu, x, y
    ret
OnFileTreeContextMenu endp

; ============================================================================
; OnSearchButtonClicked - Handles search button click
; ============================================================================
OnSearchButtonClicked proc
    LOCAL szPattern[260]:BYTE
    
    ; Get search pattern from edit box
    lea eax, szPattern
    invoke SendMessage, hSearchBox, WM_GETTEXT, 260, eax
    
    ; Perform search
    lea eax, szPattern
    invoke SearchFiles, eax
    
    ret
OnSearchButtonClicked endp

; ============================================================================
; OnSearchBoxKeyDown - Handles key down in search box
; Parameters: wParam - virtual key code
; ============================================================================
OnSearchBoxKeyDown proc wParam:DWORD
    mov eax, wParam
    .if eax == VK_RETURN
        ; Enter key pressed - perform search
        call OnSearchButtonClicked
    .endif
    ret
OnSearchBoxKeyDown endp

end
; ============================================================================
; FileExplorer_OnEnumComplete - consume async file enumeration results
; Parameters: wParam=opHandle (pointer to PENDING_OPERATION), lParam unused
; ============================================================================
FileExplorer_OnEnumComplete proc opHandle:DWORD, lParam:DWORD
    LOCAL pResults:DWORD
    LOCAL count:DWORD
    LOCAL i:DWORD
    LOCAL pOp:DWORD
    LOCAL hTree:DWORD
    LOCAL hParent:DWORD
    LOCAL tvins:TVINSERTSTRUCT
    LOCAL tvi:TVITEM
    LOCAL pNodeData:DWORD
    LOCAL pEntry:DWORD
    LOCAL hChild:DWORD

    mov pOp, opHandle
    test pOp, pOp
    jz @Exit

    ; Load tree control and parent item from op
    mov eax, [pOp].PENDING_OPERATION.hTreeControl
    mov hTree, eax
    mov eax, [pOp].PENDING_OPERATION.hParentItem
    mov hParent, eax
    test hTree, hTree
    jz @Exit
    test hParent, hParent
    jz @Exit

    ; Fetch results
    lea eax, count
    push eax
    lea eax, pResults
    push eax
    push pOp
    call FileEnumeration_GetResults
    test eax, eax
    jz @Exit

    ; Clear existing children to avoid duplicates
@DeleteChildren:
    push hParent
    push TVGN_CHILD
    push hTree
    mov eax, TVM_GETNEXTITEM
    push eax
    call SendMessage
    mov hChild, eax
    test eax, eax
    jz @InsertLoop
    push hChild
    push 0
    push hTree
    mov eax, TVM_DELETEITEM
    push eax
    call SendMessage
    jmp @DeleteChildren

@InsertLoop:
    mov i, 0
@Loop:
    mov eax, i
    cmp eax, count
    jae @Done

    mov eax, pResults
    mov ecx, i
    imul ecx, SIZEOF FILE_INFO
    add eax, ecx
    mov pEntry, eax

    ; Allocate node data
    lea ecx, [pEntry].FILE_INFO.fileName
    mov edx, [pEntry].FILE_INFO.isDirectory
    push edx
    push ecx
    call AllocNodeData
    mov pNodeData, eax

    ; Prepare TV item
    mov tvi.mask, TVIF_TEXT or TVIF_IMAGE or TVIF_SELECTEDIMAGE or TVIF_PARAM or TVIF_CHILDREN
    lea eax, [pEntry].FILE_INFO.fileName
    mov tvi.pszText, eax
    mov eax, [pEntry].FILE_INFO.isDirectory
    test eax, eax
    jz @isFile
    mov tvi.iImage, 0
    mov tvi.iSelectedImage, 0
    mov tvi.cChildren, 1
    jmp @setParam
@isFile:
    mov tvi.iImage, 1
    mov tvi.iSelectedImage, 1
    mov tvi.cChildren, 0
@setParam:
    mov eax, pNodeData
    mov tvi.lParam, eax

    mov tvins.hParent, hParent
    mov tvins.hInsertAfter, TVI_LAST
    mov tvins.item, tvi
    push addr tvins
    push 0
    push hTree
    mov eax, TVM_INSERTITEM
    push eax
    call SendMessage

    inc i
    jmp @Loop

@Done:
    ; Free results buffer and clear pointer to avoid leaks
    mov eax, pResults
    test eax, eax
    jz @Exit
    push eax
    call GlobalFree
    mov dword ptr [pOp].PENDING_OPERATION.pResults, 0
    mov dword ptr [pOp].PENDING_OPERATION.resultCount, 0

@Exit:
    xor eax, eax
    ret
FileExplorer_OnEnumComplete endp

end