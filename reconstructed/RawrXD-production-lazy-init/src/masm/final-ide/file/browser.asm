;==============================================================================
; file_browser.asm - MASM File Browser & Project Explorer
; Purpose: Directory tree traversal, filtering, caching, project detection
; Author: RawrXD CI/CD
; Date: Dec 29, 2025
;==============================================================================

option casemap:none

include windows.inc
include masm_master_defs.inc
includelib kernel32.lib
includelib user32.lib

;==============================================================================
; CONSTANTS & STRUCTURES
;==============================================================================

; File types
FILE_TYPE_UNKNOWN       EQU 0
FILE_TYPE_FILE          EQU 1
FILE_TYPE_DIRECTORY     EQU 2
FILE_TYPE_SYMLINK       EQU 3

; Project types
PROJECT_TYPE_NONE       EQU 0
PROJECT_TYPE_GIT        EQU 1
PROJECT_TYPE_CMAKE      EQU 2
PROJECT_TYPE_PYTHON     EQU 3
PROJECT_TYPE_RUST       EQU 4

; Maximum limits
MAX_TREE_NODES          EQU 4096
MAX_FILE_PATH           EQU 260
MAX_FILTER_PATTERNS     EQU 32

; File node structure
FILE_NODE STRUCT
    nodeId                  DWORD ?
    parentId                DWORD ?
    fileType                DWORD ?     ; FILE_TYPE_*
    name                    BYTE 256 DUP(?)
    fullPath                BYTE 260 DUP(?)
    fileSize                QWORD ?
    modifiedTime            QWORD ?
    isHidden                DWORD ?     ; BOOL
    isSymlink               DWORD ?     ; BOOL
    childrenCount           DWORD ?
    isExpanded              DWORD ?     ; BOOL (for tree)
    icon                    QWORD ?     ; Icon resource
FILE_NODE ENDS

; File browser structure
FILE_BROWSER STRUCT
    nodes                   FILE_NODE MAX_TREE_NODES DUP(<>)
    nodeCount               DWORD ?
    rootPath                BYTE 260 DUP(?)
    heapHandle              QWORD ?
    browserMutex            QWORD ?
    
    ; Filtering
    filterPatterns          QWORD MAX_FILTER_PATTERNS DUP(?)
    filterCount             DWORD ?
    hideHiddenFiles         DWORD ?     ; BOOL
    
    ; Project detection
    projectType             DWORD ?     ; PROJECT_TYPE_*
    hasGitDir               DWORD ?     ; BOOL
    hasCMakeFile            DWORD ?     ; BOOL
    hasPyProject            DWORD ?     ; BOOL
    
    ; Caching
    cacheTime               QWORD ?
    needsRefresh            DWORD ?     ; BOOL
    
    isInitialized           DWORD ?     ; BOOL
FILE_BROWSER ENDS

;==============================================================================
; GLOBAL DATA
;==============================================================================

.data?
    g_fileBrowser           FILE_BROWSER <>
    g_initialized           DWORD 0

.data
    ; Filter strings
    szGitDir                BYTE ".git", 0
    szCMakeFile             BYTE "CMakeLists.txt", 0
    szPyProject             BYTE "pyproject.toml", 0
    szGitIgnore             BYTE ".gitignore", 0

;==============================================================================
; EXPORTED FUNCTIONS
;==============================================================================

PUBLIC masm_file_browser_init
PUBLIC masm_file_browser_shutdown
PUBLIC masm_file_browser_set_root
PUBLIC masm_file_browser_scan_directory
PUBLIC masm_file_browser_get_node
PUBLIC masm_file_browser_expand_node
PUBLIC masm_file_browser_collapse_node
PUBLIC masm_file_browser_list_children
PUBLIC masm_file_browser_add_filter
PUBLIC masm_file_browser_remove_filter
PUBLIC masm_file_browser_detect_project
PUBLIC masm_file_browser_get_project_type
PUBLIC masm_file_browser_search_files
PUBLIC masm_file_browser_watch_changes
PUBLIC masm_file_browser_refresh_tree

;==============================================================================
; FUNCTION IMPLEMENTATIONS
;==============================================================================

; masm_file_browser_init - Initialize file browser
; Returns: 1 = success, 0 = failure
masm_file_browser_init PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    cmp g_initialized, 1
    je @@init_already

    ; Get process heap
    call GetProcessHeap
    mov g_fileBrowser.heapHandle, rax

    ; Initialize state
    mov g_fileBrowser.nodeCount, 0
    mov g_fileBrowser.filterCount, 0
    mov g_fileBrowser.hideHiddenFiles, 1
    mov g_fileBrowser.projectType, PROJECT_TYPE_NONE
    mov g_fileBrowser.needsRefresh, 1

    ; Create mutex
    xor rcx, rcx
    xor rdx, rdx
    xor r8, r8
    call CreateMutexA
    mov g_fileBrowser.browserMutex, rax

    mov g_initialized, 1
    mov rax, 1
    jmp @@init_done
@@init_already:
    mov rax, 1
@@init_done:
    add rsp, 32
    pop rbp
    ret
masm_file_browser_init ENDP

; masm_file_browser_shutdown - Shutdown file browser
; Returns: 1 = success, 0 = failure
masm_file_browser_shutdown PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    cmp g_initialized, 0
    je @@shutdown_not_init

    ; Close mutex
    mov rcx, g_fileBrowser.browserMutex
    call CloseHandle

    ; Free all nodes
    xor rbx, rbx
@@free_nodes:
    cmp ebx, g_fileBrowser.nodeCount
    jge @@nodes_freed

    ; Free icon if allocated
    mov rax, SIZEOF FILE_NODE
    imul rax, rbx
    lea rax, [g_fileBrowser.nodes + rax]
    mov rcx, [rax].FILE_NODE.icon
    cmp rcx, 0
    je @@skip_icon_free
    mov rdx, g_fileBrowser.heapHandle
    xor r8d, r8d
    call HeapFree
@@skip_icon_free:

    inc rbx
    jmp @@free_nodes
@@nodes_freed:
    mov g_initialized, 0
    mov rax, 1
    jmp @@shutdown_done
@@shutdown_not_init:
    xor rax, rax
@@shutdown_done:
    add rsp, 32
    pop rbp
    ret
masm_file_browser_shutdown ENDP

; masm_file_browser_set_root - Set root directory for browsing
; Args: RCX = root path pointer
; Returns: 1 = success, 0 = failure
masm_file_browser_set_root PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    ; Copy root path
    mov rsi, rcx
    lea rdi, g_fileBrowser.rootPath
    mov ecx, 260
@@copy_root:
    cmp ecx, 0
    je @@root_copied
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jmp @@copy_root
@@root_copied:
    mov g_fileBrowser.needsRefresh, 1
    mov rax, 1

    add rsp, 32
    pop rbp
    ret
masm_file_browser_set_root ENDP

; masm_file_browser_scan_directory - Scan directory and build tree
; Returns: number of items found, or 0 if error
masm_file_browser_scan_directory PROC USES rbx rsi rdi r12 r13 r14 r15
    push rbp
    sub rsp, 32

    ; TODO: Use Windows FindFirstFileA/FindNextFileA to enumerate
    ; For now, return stub count
    mov rax, 1

    add rsp, 32
    pop rbp
    ret
masm_file_browser_scan_directory ENDP

; masm_file_browser_get_node - Get file node by ID
; Args: RCX = node ID
; Returns: node pointer, or 0 if not found
masm_file_browser_get_node PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r8d, ecx

    xor r9d, r9d
@@find_node:
    cmp r9d, g_fileBrowser.nodeCount
    jge @@node_not_found

    mov rax, SIZEOF FILE_NODE
    imul rax, r9
    lea rax, [g_fileBrowser.nodes + rax]
    cmp [rax].FILE_NODE.nodeId, r8d
    je @@node_found

    inc r9d
    jmp @@find_node
@@node_found:
    mov rax, SIZEOF FILE_NODE
    imul rax, r9
    lea rax, [g_fileBrowser.nodes + rax]
    jmp @@get_node_done
@@node_not_found:
    xor rax, rax
@@get_node_done:
    add rsp, 32
    pop rbp
    ret
masm_file_browser_get_node ENDP

; masm_file_browser_expand_node - Expand tree node
; Args: RCX = node ID
; Returns: 1 = success, 0 = failure
masm_file_browser_expand_node PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r8d, ecx

    xor r9d, r9d
@@find_expand:
    cmp r9d, g_fileBrowser.nodeCount
    jge @@expand_not_found

    mov rax, SIZEOF FILE_NODE
    imul rax, r9
    lea rax, [g_fileBrowser.nodes + rax]
    cmp [rax].FILE_NODE.nodeId, r8d
    je @@expand_found

    inc r9d
    jmp @@find_expand
@@expand_found:
    mov [rax].FILE_NODE.isExpanded, 1
    mov rax, 1
    jmp @@expand_done
@@expand_not_found:
    xor rax, rax
@@expand_done:
    add rsp, 32
    pop rbp
    ret
masm_file_browser_expand_node ENDP

; masm_file_browser_collapse_node - Collapse tree node
; Args: RCX = node ID
; Returns: 1 = success, 0 = failure
masm_file_browser_collapse_node PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r8d, ecx

    xor r9d, r9d
@@find_collapse:
    cmp r9d, g_fileBrowser.nodeCount
    jge @@collapse_not_found

    mov rax, SIZEOF FILE_NODE
    imul rax, r9
    lea rax, [g_fileBrowser.nodes + rax]
    cmp [rax].FILE_NODE.nodeId, r8d
    je @@collapse_found

    inc r9d
    jmp @@find_collapse
@@collapse_found:
    mov [rax].FILE_NODE.isExpanded, 0
    mov rax, 1
    jmp @@collapse_done
@@collapse_not_found:
    xor rax, rax
@@collapse_done:
    add rsp, 32
    pop rbp
    ret
masm_file_browser_collapse_node ENDP

; masm_file_browser_list_children - List children of a node
; Args: RCX = parent node ID, RDX = output buffer (DWORD array), R8 = max count
; Returns: actual count filled
masm_file_browser_list_children PROC USES rbx rsi rdi r12 r13
    push rbp
    sub rsp, 32

    mov r12d, ecx  ; Parent ID
    mov r13, rdx   ; Output buffer

    xor r10d, r10d ; Result count

    ; Find all nodes with matching parent
    xor r11d, r11d ; Index
@@list_loop:
    cmp r11d, g_fileBrowser.nodeCount
    jge @@list_done

    mov rax, SIZEOF FILE_NODE
    imul rax, r11
    lea rax, [g_fileBrowser.nodes + rax]
    cmp [rax].FILE_NODE.parentId, r12d
    jne @@skip_child

    cmp r10d, r8d
    jge @@list_done

    mov edx, [rax].FILE_NODE.nodeId
    mov [r13 + r10 * 4], edx
    inc r10d
@@skip_child:
    inc r11d
    jmp @@list_loop
@@list_done:
    mov rax, r10

    add rsp, 32
    pop rbp
    ret
masm_file_browser_list_children ENDP

; masm_file_browser_add_filter - Add file filter pattern
; Args: RCX = pattern string (e.g., "*.o", "*.tmp")
; Returns: 1 = success, 0 = failure
masm_file_browser_add_filter PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    cmp g_fileBrowser.filterCount, MAX_FILTER_PATTERNS
    jge @@filter_full

    ; TODO: Store filter pattern
    inc g_fileBrowser.filterCount
    mov rax, 1
    jmp @@add_filter_done
@@filter_full:
    xor rax, rax
@@add_filter_done:
    add rsp, 32
    pop rbp
    ret
masm_file_browser_add_filter ENDP

; masm_file_browser_remove_filter - Remove file filter pattern
; Args: RCX = pattern string
; Returns: 1 = success, 0 = failure
masm_file_browser_remove_filter PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    ; TODO: Find and remove filter
    mov rax, 1

    add rsp, 32
    pop rbp
    ret
masm_file_browser_remove_filter ENDP

; masm_file_browser_detect_project - Detect project type
; Returns: PROJECT_TYPE_*
masm_file_browser_detect_project PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    ; TODO: Check for .git, CMakeLists.txt, pyproject.toml, etc
    xor eax, eax  ; Default: no project

    add rsp, 32
    pop rbp
    ret
masm_file_browser_detect_project ENDP

; masm_file_browser_get_project_type - Get detected project type
; Returns: PROJECT_TYPE_*
masm_file_browser_get_project_type PROC
    mov eax, g_fileBrowser.projectType
    ret
masm_file_browser_get_project_type ENDP

; masm_file_browser_search_files - Search for files matching pattern
; Args: RCX = search pattern, RDX = output buffer, R8 = max count
; Returns: number of matches found
masm_file_browser_search_files PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    ; TODO: Perform wildcard search
    xor rax, rax

    add rsp, 32
    pop rbp
    ret
masm_file_browser_search_files ENDP

; masm_file_browser_watch_changes - Watch for directory changes
; Returns: 1 = success, 0 = failure
masm_file_browser_watch_changes PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    ; TODO: Use ReadDirectoryChangesW for monitoring
    mov rax, 1

    add rsp, 32
    pop rbp
    ret
masm_file_browser_watch_changes ENDP

; masm_file_browser_refresh_tree - Refresh file tree
; Returns: 1 = success, 0 = failure
masm_file_browser_refresh_tree PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    ; Clear existing nodes
    mov g_fileBrowser.nodeCount, 0

    ; Re-scan directory
    call masm_file_browser_scan_directory

    mov g_fileBrowser.needsRefresh, 0
    mov rax, 1

    add rsp, 32
    pop rbp
    ret
masm_file_browser_refresh_tree ENDP

END


