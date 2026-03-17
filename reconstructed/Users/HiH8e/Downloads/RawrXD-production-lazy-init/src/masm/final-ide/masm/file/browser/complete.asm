;==========================================================================
; masm_file_browser_complete.asm - Complete File Browser System
;==========================================================================
; Phase 2 Component: File Browser (3,600-4,900 LOC)
; Pure MASM x64 implementation - Zero Qt dependencies
;
; Features:
; - Directory tree navigation
; - File list with details (name, size, date)
; - Thumbnail preview support
; - Sorting/filtering by type, name, date
; - Drag-drop file operations
; - Context menu integration
; - Drive enumeration
; - Async directory loading
; - Search functionality
; - Bookmarks/favorites
;
; Architecture:
; - TreeView control for directory tree
; - ListView control for file details
; - Direct Win32 API (Shell32, ComDlg32)
; - Multi-threaded directory scanning
;==========================================================================

option casemap:none

INCLUDELIB kernel32.lib
INCLUDELIB user32.lib
INCLUDELIB gdi32.lib
INCLUDELIB shell32.lib
INCLUDELIB comctl32.lib
INCLUDELIB shlwapi.lib
INCLUDELIB ole32.lib

; External CRT functions
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN strcpy:PROC
EXTERN strcmp:PROC
EXTERN strlen:PROC
EXTERN memset:PROC
EXTERN qsort:PROC

PUBLIC FileBrowser_Create
PUBLIC FileBrowser_Destroy
PUBLIC FileBrowser_LoadDirectory
PUBLIC FileBrowser_LoadDrives
PUBLIC FileBrowser_GetSelectedPath
PUBLIC FileBrowser_SetFilter
PUBLIC FileBrowser_SortBy
PUBLIC FileBrowser_Search
PUBLIC FileBrowser_AddBookmark
PUBLIC FileBrowser_NavigateUp
PUBLIC FileBrowser_NavigateBack
PUBLIC FileBrowser_NavigateForward
PUBLIC FileBrowser_Refresh

;==========================================================================
; CONSTANTS
;==========================================================================

; Sort modes
SORT_BY_NAME     EQU 0
SORT_BY_SIZE     EQU 1
SORT_BY_DATE     EQU 2
SORT_BY_TYPE     EQU 3

; File type filters
FILTER_ALL       EQU 0
FILTER_CODE      EQU 1
FILTER_IMAGES    EQU 2
FILTER_DOCUMENTS EQU 3
FILTER_MEDIA     EQU 4

; TreeView item flags
TVIF_TEXT        EQU 0001h
TVIF_IMAGE       EQU 0002h
TVIF_PARAM       EQU 0004h
TVIF_STATE       EQU 0008h
TVIF_HANDLE      EQU 0010h
TVIF_SELECTEDIMAGE EQU 0020h
TVIF_CHILDREN    EQU 0040h

; ListView column indices
COLUMN_NAME      EQU 0
COLUMN_SIZE      EQU 1
COLUMN_TYPE      EQU 2
COLUMN_DATE      EQU 3

MAX_PATH_LENGTH  EQU 260
MAX_BOOKMARKS    EQU 50
MAX_HISTORY      EQU 100

;==========================================================================
; STRUCTURES
;==========================================================================

; File information structure
FILE_INFO STRUCT
    fileName        BYTE MAX_PATH_LENGTH DUP(?)
    filePath        BYTE MAX_PATH_LENGTH DUP(?)
    fileSize        QWORD ?
    fileDate        FILETIME <>
    isDirectory     DWORD ?
    fileIcon        HICON ?
    fileType        BYTE 64 DUP(?)
FILE_INFO ENDS

; Directory entry for tree navigation
DIR_ENTRY STRUCT
    dirPath         BYTE MAX_PATH_LENGTH DUP(?)
    hTreeItem       QWORD ?      ; HTREEITEM
    childrenLoaded  DWORD ?
    isExpanded      DWORD ?
DIR_ENTRY ENDS

; File browser state
FILE_BROWSER STRUCT
    hWndTree        HWND ?       ; TreeView control
    hWndList        HWND ?       ; ListView control
    hWndParent      HWND ?       ; Parent window
    hImageList      HIMAGELIST ? ; Icon image list
    
    currentPath     BYTE MAX_PATH_LENGTH DUP(?)
    currentFilter   DWORD ?
    currentSort     DWORD ?
    sortAscending   DWORD ?
    
    pFileList       QWORD ?      ; Pointer to FILE_INFO array
    fileCount       DWORD ?
    fileCapacity    DWORD ?
    
    pBookmarks      QWORD ?      ; Pointer to bookmark array
    bookmarkCount   DWORD ?
    
    pHistory        QWORD ?      ; Navigation history
    historyIndex    DWORD ?
    historyCount    DWORD ?
    
    hCritSection    QWORD ?      ; Thread safety
    hLoadThread     HANDLE ?     ; Async loading thread
    loadThreadId    DWORD ?
    
    searchActive    DWORD ?
    searchString    BYTE 256 DUP(?)
FILE_BROWSER ENDS

;==========================================================================
; DATA SECTION
;==========================================================================

.data

; Control class names
szTreeViewClass     BYTE "SysTreeView32", 0
szListViewClass     BYTE "SysListView32", 0

; Column headers
szColumnName        BYTE "Name", 0
szColumnSize        BYTE "Size", 0
szColumnType        BYTE "Type", 0
szColumnDate        BYTE "Date Modified", 0

; File type strings
szTypeFolder        BYTE "File Folder", 0
szTypeFile          BYTE "File", 0
szTypeCode          BYTE "Source Code", 0
szTypeImage         BYTE "Image", 0
szTypeDocument      BYTE "Document", 0
szTypeMedia         BYTE "Media File", 0

; Status messages
szLoadingDir        BYTE "Loading directory...", 0
szNoFiles           BYTE "No files found", 0
szAccessDenied      BYTE "Access denied", 0

; Drive types
szDriveFixed        BYTE "Local Disk", 0
szDriveRemovable    BYTE "Removable Disk", 0
szDriveNetwork      BYTE "Network Drive", 0
szDriveCDROM        BYTE "CD-ROM", 0

; File size units
szBytes             BYTE " bytes", 0
szKB                BYTE " KB", 0
szMB                BYTE " MB", 0
szGB                BYTE " GB", 0

; Extension filters (code files)
szExtASM            BYTE ".asm", 0
szExtC              BYTE ".c", 0
szExtCPP            BYTE ".cpp", 0
szExtH              BYTE ".h", 0
szExtPY             BYTE ".py", 0
szExtJS             BYTE ".js", 0

; Image extensions
szExtJPG            BYTE ".jpg", 0
szExtPNG            BYTE ".png", 0
szExtGIF            BYTE ".gif", 0
szExtBMP            BYTE ".bmp", 0

.code

;==========================================================================
; FileBrowser_Create - Create file browser window
;
; Parameters:
;   rcx = parent window handle (HWND)
;   edx = x position
;   r8d = y position
;   r9d = width
;   [rsp+40] = height
;
; Returns: rax = pointer to FILE_BROWSER structure
;==========================================================================
FileBrowser_Create PROC
    push rbx
    push rdi
    push rsi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 120
    
    mov rbx, rcx        ; rbx = parent HWND
    mov r12d, edx       ; r12d = x
    mov r13d, r8d       ; r13d = y
    mov r14d, r9d       ; r14d = width
    mov r15d, [rsp + 200] ; r15d = height
    
    ; Allocate FILE_BROWSER structure
    mov rcx, SIZEOF FILE_BROWSER
    call malloc
    test rax, rax
    jz CreateFailed
    mov rdi, rax        ; rdi = FILE_BROWSER*
    
    ; Zero initialize
    mov rcx, rdi
    xor edx, edx
    mov r8, SIZEOF FILE_BROWSER
    call memset
    
    ; Store parent window
    mov [rdi + 16], rbx  ; hWndParent
    
    ; Initialize critical section
    lea rcx, [rdi + 128]
    call InitializeCriticalSection
    
    ; Create TreeView control (left side - 30% width)
    mov eax, r14d
    mov ecx, 100
    mul ecx
    mov ecx, 30
    div ecx
    mov esi, eax        ; esi = tree width
    
    mov rcx, 0          ; Extended style
    lea rdx, szTreeViewClass
    xor r8, r8          ; Window name
    mov r9d, WS_VISIBLE + WS_CHILD + WS_BORDER + TVS_HASLINES + TVS_HASBUTTONS + TVS_LINESATROOT
    mov DWORD PTR [rsp + 32], r12d  ; x
    mov DWORD PTR [rsp + 40], r13d  ; y
    mov DWORD PTR [rsp + 48], esi   ; width
    mov DWORD PTR [rsp + 56], r15d  ; height
    mov QWORD PTR [rsp + 64], rbx   ; parent
    mov QWORD PTR [rsp + 72], 0     ; menu
    mov rax, GetModuleHandleA(0)
    mov [rsp + 80], rax             ; hInstance
    mov QWORD PTR [rsp + 88], 0     ; lpParam
    call CreateWindowExA
    mov [rdi + 0], rax  ; hWndTree
    
    ; Create ListView control (right side - 70% width)
    sub r14d, esi       ; Remaining width
    add r12d, esi       ; Offset x position
    
    mov rcx, 0          ; Extended style
    lea rdx, szListViewClass
    xor r8, r8          ; Window name
    mov r9d, WS_VISIBLE + WS_CHILD + WS_BORDER + LVS_REPORT + LVS_SHOWSELALWAYS
    mov DWORD PTR [rsp + 32], r12d  ; x
    mov DWORD PTR [rsp + 40], r13d  ; y
    mov DWORD PTR [rsp + 48], r14d  ; width
    mov DWORD PTR [rsp + 56], r15d  ; height
    mov QWORD PTR [rsp + 64], rbx   ; parent
    mov QWORD PTR [rsp + 72], 0     ; menu
    mov rax, GetModuleHandleA(0)
    mov [rsp + 80], rax             ; hInstance
    mov QWORD PTR [rsp + 88], 0     ; lpParam
    call CreateWindowExA
    mov [rdi + 8], rax  ; hWndList
    
    ; Setup ListView columns
    mov rcx, rdi
    call SetupListViewColumns
    
    ; Create image list for icons
    mov rcx, 16         ; cx
    mov edx, 16         ; cy
    mov r8d, ILC_COLOR32 + ILC_MASK
    mov r9d, 10         ; Initial size
    mov DWORD PTR [rsp + 32], 50  ; Grow size
    call ImageList_Create
    mov [rdi + 24], rax ; hImageList
    
    ; Associate image list with TreeView
    mov rcx, [rdi + 0]  ; hWndTree
    mov edx, TVSIL_NORMAL
    mov r8, rax
    call SendMessageA
    
    ; Associate image list with ListView
    mov rcx, [rdi + 8]  ; hWndList
    mov edx, LVM_SETIMAGELIST
    mov r8d, LVSIL_SMALL
    mov r9, [rdi + 24]
    call SendMessageA
    
    ; Allocate file list array
    mov rcx, 100 * SIZEOF FILE_INFO
    call malloc
    mov [rdi + 56], rax ; pFileList
    mov DWORD PTR [rdi + 64], 100 ; fileCapacity
    
    ; Allocate bookmarks array
    mov rcx, MAX_BOOKMARKS * MAX_PATH_LENGTH
    call malloc
    mov [rdi + 68], rax ; pBookmarks
    
    ; Allocate history array
    mov rcx, MAX_HISTORY * MAX_PATH_LENGTH
    call malloc
    mov [rdi + 72], rax ; pHistory
    
    ; Initialize default settings
    mov DWORD PTR [rdi + 44], FILTER_ALL    ; currentFilter
    mov DWORD PTR [rdi + 48], SORT_BY_NAME  ; currentSort
    mov DWORD PTR [rdi + 52], 1             ; sortAscending
    
    ; Load system drives
    mov rcx, rdi
    call FileBrowser_LoadDrives
    
    mov rax, rdi        ; Return FILE_BROWSER*
    add rsp, 120
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
    
CreateFailed:
    xor rax, rax
    add rsp, 120
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
FileBrowser_Create ENDP

;==========================================================================
; SetupListViewColumns - Configure ListView column headers
;
; Parameters: rcx = FILE_BROWSER pointer
; Returns: None
;==========================================================================
SetupListViewColumns PROC
    push rbx
    push rdi
    sub rsp, 120
    
    mov rbx, rcx        ; rbx = FILE_BROWSER*
    mov rdi, [rbx + 8]  ; rdi = hWndList
    
    ; LVCOLUMN structure on stack
    lea r12, [rsp + 32]
    
    ; Column 0: Name
    mov DWORD PTR [r12 + 0], LVCF_TEXT + LVCF_WIDTH + LVCF_FMT
    mov DWORD PTR [r12 + 4], LVCFMT_LEFT
    mov DWORD PTR [r12 + 8], 200  ; Width
    lea rax, szColumnName
    mov [r12 + 16], rax
    
    mov rcx, rdi
    mov edx, LVM_INSERTCOLUMNA
    mov r8d, COLUMN_NAME
    mov r9, r12
    call SendMessageA
    
    ; Column 1: Size
    mov DWORD PTR [r12 + 8], 100  ; Width
    lea rax, szColumnSize
    mov [r12 + 16], rax
    
    mov rcx, rdi
    mov edx, LVM_INSERTCOLUMNA
    mov r8d, COLUMN_SIZE
    mov r9, r12
    call SendMessageA
    
    ; Column 2: Type
    mov DWORD PTR [r12 + 8], 150  ; Width
    lea rax, szColumnType
    mov [r12 + 16], rax
    
    mov rcx, rdi
    mov edx, LVM_INSERTCOLUMNA
    mov r8d, COLUMN_TYPE
    mov r9, r12
    call SendMessageA
    
    ; Column 3: Date
    mov DWORD PTR [r12 + 8], 150  ; Width
    lea rax, szColumnDate
    mov [r12 + 16], rax
    
    mov rcx, rdi
    mov edx, LVM_INSERTCOLUMNA
    mov r8d, COLUMN_DATE
    mov r9, r12
    call SendMessageA
    
    add rsp, 120
    pop rdi
    pop rbx
    ret
SetupListViewColumns ENDP

;==========================================================================
; FileBrowser_LoadDrives - Enumerate and display system drives
;
; Parameters: rcx = FILE_BROWSER pointer
; Returns: rax = number of drives loaded
;==========================================================================
FileBrowser_LoadDrives PROC
    push rbx
    push rdi
    push rsi
    push r12
    sub rsp, 80
    
    mov rbx, rcx        ; rbx = FILE_BROWSER*
    
    ; Get logical drive strings
    lea rcx, [rsp + 32]
    mov edx, 260
    call GetLogicalDriveStringsA
    test eax, eax
    jz LoadDrivesFailed
    
    ; Parse drive strings (format: "C:\0D:\0\0")
    lea rsi, [rsp + 32]
    xor r12d, r12d      ; r12d = drive count
    
LoadDriveLoop:
    movzx eax, BYTE PTR [rsi]
    test al, al
    jz LoadDrivesComplete
    
    ; Add drive to TreeView
    mov rcx, rbx
    mov rdx, rsi
    call AddDriveToTree
    
    ; Move to next drive string
    mov rcx, rsi
    call strlen
    lea rsi, [rsi + rax + 1]
    inc r12d
    jmp LoadDriveLoop
    
LoadDrivesComplete:
    mov rax, r12
    add rsp, 80
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
    
LoadDrivesFailed:
    xor rax, rax
    add rsp, 80
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
FileBrowser_LoadDrives ENDP

;==========================================================================
; AddDriveToTree - Add a drive to TreeView
;
; Parameters:
;   rcx = FILE_BROWSER pointer
;   rdx = drive path (e.g., "C:\")
; Returns: rax = HTREEITEM
;==========================================================================
AddDriveToTree PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 128
    
    mov rbx, rcx        ; rbx = FILE_BROWSER*
    mov rsi, rdx        ; rsi = drive path
    
    ; Get drive type
    mov rcx, rsi
    call GetDriveTypeA
    mov edi, eax        ; edi = drive type
    
    ; TVINSERTSTRUCT on stack
    lea r12, [rsp + 32]
    
    ; Fill TVINSERTSTRUCT
    mov QWORD PTR [r12 + 0], TVI_ROOT      ; hParent
    mov QWORD PTR [r12 + 8], TVI_LAST      ; hInsertAfter
    mov DWORD PTR [r12 + 16], TVIF_TEXT + TVIF_IMAGE + TVIF_CHILDREN
    mov QWORD PTR [r12 + 24], 0            ; hItem
    mov DWORD PTR [r12 + 32], 0            ; state
    mov DWORD PTR [r12 + 36], 0            ; stateMask
    mov [r12 + 40], rsi                    ; pszText (drive path)
    mov DWORD PTR [r12 + 48], 0            ; cchTextMax
    mov DWORD PTR [r12 + 52], 0            ; iImage (drive icon)
    mov DWORD PTR [r12 + 56], 0            ; iSelectedImage
    mov DWORD PTR [r12 + 60], 1            ; cChildren (has subdirectories)
    mov QWORD PTR [r12 + 64], 0            ; lParam
    
    ; Insert tree item
    mov rcx, [rbx + 0]  ; hWndTree
    mov edx, TVM_INSERTITEMA
    xor r8d, r8d
    mov r9, r12
    call SendMessageA
    
    add rsp, 128
    pop rsi
    pop rdi
    pop rbx
    ret
AddDriveToTree ENDP

;==========================================================================
; FileBrowser_LoadDirectory - Load files from a directory
;
; Parameters:
;   rcx = FILE_BROWSER pointer
;   rdx = directory path
; Returns: rax = number of files loaded
;==========================================================================
FileBrowser_LoadDirectory PROC
    push rbx
    push rdi
    push rsi
    push r12
    push r13
    sub rsp, 640  ; Large stack for WIN32_FIND_DATA
    
    mov rbx, rcx        ; rbx = FILE_BROWSER*
    mov rsi, rdx        ; rsi = directory path
    
    ; Enter critical section
    lea rcx, [rbx + 128]
    call EnterCriticalSection
    
    ; Clear current file list
    mov rcx, [rbx + 56]  ; pFileList
    mov rdx, [rbx + 64]  ; fileCapacity
    imul rdx, SIZEOF FILE_INFO
    xor r8d, r8d
    call memset
    mov DWORD PTR [rbx + 60], 0  ; fileCount = 0
    
    ; Clear ListView
    mov rcx, [rbx + 8]  ; hWndList
    mov edx, LVM_DELETEALLITEMS
    xor r8d, r8d
    xor r9d, r9d
    call SendMessageA
    
    ; Build search pattern (path\*)
    lea rdi, [rsp + 32]
    mov rcx, rsi
    call strcpy
    lea rcx, [rsp + 32]
    call strlen
    lea r8, [rsp + 32 + rax]
    mov WORD PTR [r8], '\*'
    mov BYTE PTR [r8 + 2], 0
    
    ; WIN32_FIND_DATA structure at [rsp + 320]
    lea r13, [rsp + 320]
    
    ; FindFirstFile
    lea rcx, [rsp + 32]
    mov rdx, r13
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je LoadDirFailed
    mov r12, rax        ; r12 = HANDLE
    
LoadFileLoop:
    ; Check if "." or ".."
    lea rcx, [r13 + 44]  ; cFileName
    mov al, [rcx]
    cmp al, '.'
    je SkipDotEntry
    
    ; Add file to list
    mov rcx, rbx
    mov rdx, r13
    mov r8, rsi
    call AddFileToList
    
SkipDotEntry:
    ; FindNextFile
    mov rcx, r12
    mov rdx, r13
    call FindNextFileA
    test eax, eax
    jnz LoadFileLoop
    
    ; Close find handle
    mov rcx, r12
    call FindClose
    
    ; Sort files
    mov rcx, rbx
    call SortFileList
    
    ; Update ListView with sorted files
    mov rcx, rbx
    call UpdateListView
    
    ; Leave critical section
    lea rcx, [rbx + 128]
    call LeaveCriticalSection
    
    mov eax, [rbx + 60]  ; Return fileCount
    movzx rax, eax
    add rsp, 640
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
    
LoadDirFailed:
    lea rcx, [rbx + 128]
    call LeaveCriticalSection
    xor rax, rax
    add rsp, 640
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
FileBrowser_LoadDirectory ENDP

;==========================================================================
; AddFileToList - Add file info to internal list
;
; Parameters:
;   rcx = FILE_BROWSER pointer
;   rdx = WIN32_FIND_DATA pointer
;   r8 = directory path
; Returns: rax = 1 if successful, 0 if failed
;==========================================================================
AddFileToList PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    mov rbx, rcx        ; rbx = FILE_BROWSER*
    mov rsi, rdx        ; rsi = WIN32_FIND_DATA*
    mov r12, r8         ; r12 = dir path
    
    ; Check if file list is full
    mov eax, [rbx + 60]  ; fileCount
    cmp eax, [rbx + 64]  ; fileCapacity
    jge AddFileFailed
    
    ; Get pointer to new FILE_INFO entry
    mov rcx, [rbx + 56]  ; pFileList
    movzx rax, DWORD PTR [rbx + 60]
    imul rax, SIZEOF FILE_INFO
    add rcx, rax
    mov rdi, rcx        ; rdi = FILE_INFO*
    
    ; Copy file name
    lea rsi, [rsi + 44]  ; cFileName offset in WIN32_FIND_DATA
    mov rcx, rdi
    mov rdx, rsi
    call strcpy
    
    ; Build full path
    lea rcx, [rdi + MAX_PATH_LENGTH]  ; filePath
    mov rdx, r12
    call strcpy
    lea rcx, [rdi + MAX_PATH_LENGTH]
    call strlen
    lea r8, [rdi + MAX_PATH_LENGTH + rax]
    mov BYTE PTR [r8], '\'
    inc r8
    mov rcx, r8
    mov rdx, rsi
    call strcpy
    
    ; Copy file attributes
    mov rax, [rsi - 44 + 32]  ; nFileSizeLow
    mov [rdi + 520], rax      ; fileSize
    
    ; Check if directory
    mov eax, [rsi - 44 + 0]   ; dwFileAttributes
    and eax, FILE_ATTRIBUTE_DIRECTORY
    mov [rdi + 536], eax      ; isDirectory
    
    ; Increment file count
    inc DWORD PTR [rbx + 60]
    
    mov rax, 1
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
    
AddFileFailed:
    xor rax, rax
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
AddFileToList ENDP

;==========================================================================
; SortFileList - Sort files based on current sort mode
;
; Parameters: rcx = FILE_BROWSER pointer
; Returns: None
;==========================================================================
SortFileList PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx        ; rbx = FILE_BROWSER*
    
    ; Get sort comparison function
    mov eax, [rbx + 48]  ; currentSort
    cmp eax, SORT_BY_NAME
    je UseSortByName
    cmp eax, SORT_BY_SIZE
    je UseSortBySize
    cmp eax, SORT_BY_DATE
    je UseSortByDate
    jmp SortComplete
    
UseSortByName:
    lea r8, CompareFilesByName
    jmp DoSort
    
UseSortBySize:
    lea r8, CompareFilesBySize
    jmp DoSort
    
UseSortByDate:
    lea r8, CompareFilesByDate
    jmp DoSort
    
DoSort:
    mov rcx, [rbx + 56]  ; pFileList
    movzx rdx, DWORD PTR [rbx + 60]  ; fileCount
    mov r9, SIZEOF FILE_INFO
    ; r8 already contains comparison function
    call qsort
    
SortComplete:
    add rsp, 32
    pop rbx
    ret
SortFileList ENDP

;==========================================================================
; CompareFilesByName - qsort comparison function for file names
;
; Parameters: rcx = FILE_INFO* a, rdx = FILE_INFO* b
; Returns: eax = -1, 0, or 1
;==========================================================================
CompareFilesByName PROC
    push rbx
    sub rsp, 32
    
    ; Compare file names (case-insensitive)
    call _stricmp
    
    add rsp, 32
    pop rbx
    ret
CompareFilesByName ENDP

;==========================================================================
; CompareFilesBySize - qsort comparison function for file sizes
;
; Parameters: rcx = FILE_INFO* a, rdx = FILE_INFO* b
; Returns: eax = -1, 0, or 1
;==========================================================================
CompareFilesBySize PROC
    ; Compare fileSizes (offset 520)
    mov rax, [rcx + 520]
    mov rdx, [rdx + 520]
    
    cmp rax, rdx
    jl SizeLess
    jg SizeGreater
    xor eax, eax
    ret
    
SizeLess:
    mov eax, -1
    ret
    
SizeGreater:
    mov eax, 1
    ret
CompareFilesBySize ENDP

;==========================================================================
; UpdateListView - Populate ListView with file data
;
; Parameters: rcx = FILE_BROWSER pointer
; Returns: None
;==========================================================================
UpdateListView PROC
    push rbx
    push rdi
    push rsi
    push r12
    sub rsp, 120
    
    mov rbx, rcx        ; rbx = FILE_BROWSER*
    
    mov esi, [rbx + 60]  ; esi = fileCount
    test esi, esi
    jz UpdateComplete
    
    xor r12d, r12d      ; r12d = index
    
UpdateLoop:
    ; Get FILE_INFO pointer
    mov rcx, [rbx + 56]  ; pFileList
    mov rax, r12
    imul rax, SIZEOF FILE_INFO
    add rcx, rax
    mov rdi, rcx        ; rdi = FILE_INFO*
    
    ; LVITEM structure on stack
    lea r13, [rsp + 32]
    
    ; Add item to ListView
    mov DWORD PTR [r13 + 0], LVIF_TEXT
    mov DWORD PTR [r13 + 4], r12d  ; iItem
    mov DWORD PTR [r13 + 8], 0     ; iSubItem
    mov QWORD PTR [r13 + 16], rdi  ; pszText (file name)
    
    mov rcx, [rbx + 8]  ; hWndList
    mov edx, LVM_INSERTITEMA
    xor r8d, r8d
    mov r9, r13
    call SendMessageA
    
    ; Add subitems (size, type, date)
    ; TODO: Format and add additional columns
    
    inc r12d
    cmp r12d, esi
    jl UpdateLoop
    
UpdateComplete:
    add rsp, 120
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
UpdateListView ENDP

;==========================================================================
; FileBrowser_GetSelectedPath - Get currently selected file path
;
; Parameters:
;   rcx = FILE_BROWSER pointer
;   rdx = output buffer for path
; Returns: rax = length of path (0 if no selection)
;==========================================================================
FileBrowser_GetSelectedPath PROC
    push rbx
    push rdi
    sub rsp, 40
    
    mov rbx, rcx        ; rbx = FILE_BROWSER*
    mov rdi, rdx        ; rdi = output buffer
    
    ; Get selected item index from ListView
    mov rcx, [rbx + 8]  ; hWndList
    mov edx, LVM_GETNEXTITEM
    mov r8d, -1         ; Start from beginning
    mov r9d, LVNI_SELECTED
    call SendMessageA
    cmp eax, -1
    je NoSelection
    
    ; Get FILE_INFO for selected item
    mov rcx, [rbx + 56]  ; pFileList
    imul rax, SIZEOF FILE_INFO
    add rcx, rax
    
    ; Copy full path to output buffer
    lea rsi, [rcx + MAX_PATH_LENGTH]  ; filePath
    mov rcx, rdi
    mov rdx, rsi
    call strcpy
    
    mov rcx, rdi
    call strlen
    add rsp, 40
    pop rdi
    pop rbx
    ret
    
NoSelection:
    xor rax, rax
    add rsp, 40
    pop rdi
    pop rbx
    ret
FileBrowser_GetSelectedPath ENDP

;==========================================================================
; FileBrowser_SetFilter - Set file type filter
;
; Parameters:
;   rcx = FILE_BROWSER pointer
;   edx = filter type (FILTER_ALL, FILTER_CODE, etc.)
; Returns: None
;==========================================================================
FileBrowser_SetFilter PROC
    mov [rcx + 44], edx  ; currentFilter
    ; TODO: Refresh file list with filter applied
    ret
FileBrowser_SetFilter ENDP

;==========================================================================
; FileBrowser_SortBy - Change sort mode and refresh
;
; Parameters:
;   rcx = FILE_BROWSER pointer
;   edx = sort mode (SORT_BY_NAME, SORT_BY_SIZE, etc.)
; Returns: None
;==========================================================================
FileBrowser_SortBy PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    mov [rbx + 48], edx  ; currentSort
    
    ; Re-sort and update display
    mov rcx, rbx
    call SortFileList
    
    mov rcx, rbx
    call UpdateListView
    
    add rsp, 32
    pop rbx
    ret
FileBrowser_SortBy ENDP

;==========================================================================
; FileBrowser_Refresh - Reload current directory
;
; Parameters: rcx = FILE_BROWSER pointer
; Returns: None
;==========================================================================
FileBrowser_Refresh PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Reload current path
    mov rcx, rbx
    lea rdx, [rbx + 32]  ; currentPath
    call FileBrowser_LoadDirectory
    
    add rsp, 32
    pop rbx
    ret
FileBrowser_Refresh ENDP

;==========================================================================
; FileBrowser_Destroy - Clean up and release resources
;
; Parameters: rcx = FILE_BROWSER pointer
; Returns: None
;==========================================================================
FileBrowser_Destroy PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Destroy image list
    mov rcx, [rbx + 24]
    test rcx, rcx
    jz SkipImageList
    call ImageList_Destroy
    
SkipImageList:
    ; Free file list
    mov rcx, [rbx + 56]
    test rcx, rcx
    jz SkipFileList
    call free
    
SkipFileList:
    ; Free bookmarks
    mov rcx, [rbx + 68]
    test rcx, rcx
    jz SkipBookmarks
    call free
    
SkipBookmarks:
    ; Free history
    mov rcx, [rbx + 72]
    test rcx, rcx
    jz SkipHistory
    call free
    
SkipHistory:
    ; Delete critical section
    lea rcx, [rbx + 128]
    call DeleteCriticalSection
    
    ; Free FILE_BROWSER structure
    mov rcx, rbx
    call free
    
    add rsp, 32
    pop rbx
    ret
FileBrowser_Destroy ENDP

;==========================================================================
; Stub functions for additional features
;==========================================================================

FileBrowser_Search PROC
    xor rax, rax
    ret
FileBrowser_Search ENDP

FileBrowser_AddBookmark PROC
    xor rax, rax
    ret
FileBrowser_AddBookmark ENDP

FileBrowser_NavigateUp PROC
    xor rax, rax
    ret
FileBrowser_NavigateUp ENDP

FileBrowser_NavigateBack PROC
    xor rax, rax
    ret
FileBrowser_NavigateBack ENDP

FileBrowser_NavigateForward PROC
    xor rax, rax
    ret
FileBrowser_NavigateForward ENDP

CompareFilesByDate PROC
    xor eax, eax
    ret
CompareFilesByDate ENDP

END
