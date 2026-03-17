;==========================================================================
; file_tree_context_menu.asm - File Tree Context Menu Operations
; ==========================================================================
; Features:
; - Copy full path to clipboard
; - Copy relative path to clipboard
; - Open terminal here (cmd.exe)
; - Open folder in Explorer
; - Refresh folder contents
; - Properties dialog
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib shell32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_PATH_LEN        EQU 260
MAX_CONTEXT_ITEMS   EQU 8

; Context menu item IDs
IDM_COPY_PATH       EQU 1001
IDM_COPY_REL_PATH   EQU 1002
IDM_OPEN_TERMINAL   EQU 1003
IDM_OPEN_EXPLORER   EQU 1004
IDM_REFRESH         EQU 1005
IDM_PROPERTIES      EQU 1006
IDM_NEW_FILE        EQU 1007
IDM_NEW_FOLDER      EQU 1008

; Clipboard format
CF_TEXT             EQU 1

;==========================================================================
; STRUCTURES
;==========================================================================
CONTEXT_ITEM STRUCT
    id              DWORD ?
    label           BYTE 32 DUP (?)
    flags           DWORD ?             ; MFT_STRING, etc.
CONTEXT_ITEM ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Context menu items
    szCopyPath      BYTE "Copy Full Path",0
    szCopyRelPath   BYTE "Copy Relative Path",0
    szOpenTerminal  BYTE "Open Terminal Here",0
    szOpenExplorer  BYTE "Open in Explorer",0
    szRefresh       BYTE "Refresh",0
    szProperties    BYTE "Properties",0
    szNewFile       BYTE "New File",0
    szNewFolder     BYTE "New Folder",0
    
    ; Strings
    szCmd           BYTE "cmd.exe",0
    szExplorer      BYTE "explorer.exe",0
    szExplorerArgs  BYTE "/select,",0
    
    ; Buffer for path operations
    CurrentPath     BYTE MAX_PATH_LEN DUP (0)
    RootPath        BYTE MAX_PATH_LEN DUP (0)

.data?
    ; Selected tree item info
    hSelectedItem   QWORD ?
    SelectedPath    BYTE MAX_PATH_LEN DUP (0)
    
    ; Context menu handle
    hContextMenu    QWORD ?
    
    ; Parent window handle
    hParentWindow   QWORD ?

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: filetree_context_init(hParent: rcx, root_path: rdx) -> rax
; Initialize context menu system with root path
;==========================================================================
PUBLIC filetree_context_init
filetree_context_init PROC
    push rbx
    
    mov hParentWindow, rcx
    
    ; Copy root path
    lea rax, RootPath
    mov rcx, rax
    mov rdx, rax
    call copy_path_safe
    
    pop rbx
    xor eax, eax
    ret
filetree_context_init ENDP

;==========================================================================
; PUBLIC: filetree_show_context_menu(hItem: rcx, x: edx, y: r8d) -> eax
; Show context menu for tree item at screen coordinates
;==========================================================================
PUBLIC filetree_show_context_menu
filetree_show_context_menu PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    mov hSelectedItem, rcx
    mov ebx, edx                        ; ebx = x
    mov edi, r8d                        ; edi = y
    
    ; Get path from tree item
    mov rcx, hSelectedItem
    lea rdx, SelectedPath
    call get_tree_item_path
    
    ; Create popup menu
    call CreatePopupMenu
    test rax, rax
    jz .menu_fail
    
    mov hContextMenu, rax
    
    ; Add menu items
    mov rcx, hContextMenu
    mov edx, IDM_COPY_PATH
    lea r8, szCopyPath
    call append_menu_item
    
    mov rcx, hContextMenu
    mov edx, IDM_COPY_REL_PATH
    lea r8, szCopyRelPath
    call append_menu_item
    
    mov rcx, hContextMenu
    mov edx, IDM_OPEN_TERMINAL
    lea r8, szOpenTerminal
    call append_menu_item
    
    mov rcx, hContextMenu
    mov edx, IDM_OPEN_EXPLORER
    lea r8, szOpenExplorer
    call append_menu_item
    
    mov rcx, hContextMenu
    mov edx, IDM_REFRESH
    lea r8, szRefresh
    call append_menu_item
    
    mov rcx, hContextMenu
    mov edx, IDM_PROPERTIES
    lea r8, szProperties
    call append_menu_item
    
    ; Show menu
    mov rcx, hContextMenu
    mov edx, 0                          ; uFlags (0=left align, default)
    mov r8d, ebx                        ; x
    mov r9d, edi                        ; y
    mov rax, hParentWindow
    mov QWORD PTR [rsp], rax            ; hWnd
    
    call TrackPopupMenuEx
    
    ; Handle menu selection (result in eax)
    test eax, eax
    jz .menu_cancel
    
    ; Dispatch menu command
    mov ecx, eax
    call handle_context_menu_command
    
.menu_cancel:
    ; Destroy menu
    mov rcx, hContextMenu
    call DestroyMenu
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
    
.menu_fail:
    xor eax, eax
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
output_show_context_menu ENDP

;==========================================================================
; PRIVATE: append_menu_item(menu: rcx, id: edx, label: r8) -> eax
;==========================================================================
PRIVATE append_menu_item
append_menu_item PROC
    push rbx
    
    ; AppendMenu(hMenu, MFT_STRING, id, label)
    mov r9, r8                          ; r9 = label
    ; Call AppendMenu with Windows calling convention
    ; (This is pseudocode; actual Win32 API call needed)
    
    pop rbx
    xor eax, eax
    ret
append_menu_item ENDP

;==========================================================================
; PRIVATE: get_tree_item_path(hItem: rcx, buffer: rdx) -> eax
; Get full path for tree item
;==========================================================================
PRIVATE get_tree_item_path
get_tree_item_path PROC
    push rbx
    
    ; Walk up tree from item to root, accumulating path
    ; TODO: Implement tree traversal
    
    pop rbx
    xor eax, eax
    ret
get_tree_item_path ENDP

;==========================================================================
; PRIVATE: handle_context_menu_command(command: ecx) -> void
;==========================================================================
PRIVATE handle_context_menu_command
handle_context_menu_command PROC
    push rbx
    
    cmp ecx, IDM_COPY_PATH
    je .copy_full_path
    
    cmp ecx, IDM_COPY_REL_PATH
    je .copy_rel_path
    
    cmp ecx, IDM_OPEN_TERMINAL
    je .open_terminal
    
    cmp ecx, IDM_OPEN_EXPLORER
    je .open_explorer
    
    cmp ecx, IDM_REFRESH
    je .refresh_folder
    
    cmp ecx, IDM_PROPERTIES
    je .show_properties
    
    jmp .command_done
    
.copy_full_path:
    lea rcx, SelectedPath
    call copy_to_clipboard
    jmp .command_done
    
.copy_rel_path:
    lea rcx, SelectedPath
    lea rdx, RootPath
    call get_relative_path
    mov rcx, rax
    call copy_to_clipboard
    jmp .command_done
    
.open_terminal:
    lea rcx, SelectedPath
    call open_terminal_at_path
    jmp .command_done
    
.open_explorer:
    lea rcx, SelectedPath
    call open_explorer_at_path
    jmp .command_done
    
.refresh_folder:
    mov rcx, hSelectedItem
    call refresh_tree_folder
    jmp .command_done
    
.show_properties:
    lea rcx, SelectedPath
    call show_file_properties
    
.command_done:
    pop rbx
    ret
handle_context_menu_command ENDP

;==========================================================================
; PRIVATE: copy_to_clipboard(text: rcx) -> eax
; Copy text to Windows clipboard
;==========================================================================
PRIVATE copy_to_clipboard
copy_to_clipboard PROC
    push rbx
    push rdi
    sub rsp, 32
    
    ; Calculate text length
    mov rdi, rcx
    xor eax, eax
    
.len_loop:
    cmp BYTE PTR [rdi + rax], 0
    je .len_done
    inc eax
    jmp .len_loop
    
.len_done:
    mov ebx, eax                        ; ebx = text length
    
    ; OpenClipboard
    mov rcx, 0
    call OpenClipboard
    test eax, eax
    jz .clip_fail
    
    ; Allocate global memory for text
    mov ecx, ebx
    add ecx, 1                          ; Include null terminator
    mov edx, GMEM_MOVEABLE
    call GlobalAlloc
    test rax, rax
    jz .clip_fail
    
    ; Lock and copy text
    mov rcx, rax
    call GlobalLock
    mov rdx, rdi                        ; Source text
    mov r8d, ebx                        ; Length
    call copy_memory_safe
    
    ; Empty clipboard and set new text
    call EmptyClipboard
    mov rcx, hData                      ; From GlobalAlloc
    mov edx, CF_TEXT
    call SetClipboardData
    
    ; Close clipboard
    call CloseClipboard
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rdi
    pop rbx
    ret
    
.clip_fail:
    xor eax, eax
    add rsp, 32
    pop rdi
    pop rbx
    ret
copy_to_clipboard ENDP

;==========================================================================
; PRIVATE: get_relative_path(path: rcx, root: rdx) -> rax
; Calculate relative path from root to path
;==========================================================================
PRIVATE get_relative_path
get_relative_path PROC
    push rbx
    
    ; TODO: Implement path relativization
    ; For now, just return input path
    mov rax, rcx
    
    pop rbx
    ret
get_relative_path ENDP

;==========================================================================
; PRIVATE: open_terminal_at_path(path: rcx) -> eax
; Launch cmd.exe in specified directory
;==========================================================================
PRIVATE open_terminal_at_path
open_terminal_at_path PROC
    push rbx
    sub rsp, 32
    
    ; ShellExecute(NULL, "open", "cmd.exe", NULL, path, SW_SHOW)
    mov r8d, 1                          ; SW_SHOW
    mov r9, rcx                         ; lpDirectory = path
    lea rax, szCmd
    mov QWORD PTR [rsp], 0              ; lpVerb = NULL
    mov QWORD PTR [rsp + 8], rax        ; lpFile = cmd.exe
    mov QWORD PTR [rsp + 16], 0         ; lpParameters = NULL
    
    call ShellExecuteA
    
    add rsp, 32
    pop rbx
    ret
open_terminal_at_path ENDP

;==========================================================================
; PRIVATE: open_explorer_at_path(path: rcx) -> eax
; Open Explorer window to path
;==========================================================================
PRIVATE open_explorer_at_path
open_explorer_at_path PROC
    push rbx
    sub rsp, 32
    
    ; ShellExecute(NULL, "open", "explorer.exe", "/select,<path>", NULL, SW_SHOW)
    ; TODO: Implement proper argument building
    
    add rsp, 32
    pop rbx
    ret
open_explorer_at_path ENDP

;==========================================================================
; PRIVATE: refresh_tree_folder(hItem: rcx) -> eax
;==========================================================================
PRIVATE refresh_tree_folder
refresh_tree_folder PROC
    ; TODO: Implement tree item refresh
    xor eax, eax
    ret
refresh_tree_folder ENDP

;==========================================================================
; PRIVATE: show_file_properties(path: rcx) -> eax
;==========================================================================
PRIVATE show_file_properties
show_file_properties PROC
    ; ShellExecuteA(NULL, "properties", path, NULL, NULL, SW_SHOW)
    xor eax, eax
    ret
show_file_properties ENDP

;==========================================================================
; PRIVATE: copy_memory_safe(dst: rax, src: rdx, len: r8d) -> void
;==========================================================================
PRIVATE copy_memory_safe
copy_memory_safe PROC
    xor ecx, ecx
    
.copy_loop:
    cmp ecx, r8d
    jge .copy_done
    
    movzx ebx, BYTE PTR [rdx + rcx]
    mov BYTE PTR [rax + rcx], bl
    
    inc ecx
    jmp .copy_loop
    
.copy_done:
    ret
copy_memory_safe ENDP

;==========================================================================
; PRIVATE: copy_path_safe(dst: rcx, src: rdx) -> void
;==========================================================================
PRIVATE copy_path_safe
copy_path_safe PROC
    xor eax, eax
    
.copy_loop:
    cmp eax, MAX_PATH_LEN - 1
    jge .copy_done
    
    movzx ebx, BYTE PTR [rdx + rax]
    mov BYTE PTR [rcx + rax], bl
    
    test bl, bl
    je .copy_done
    
    inc eax
    jmp .copy_loop
    
.copy_done:
    mov BYTE PTR [rcx + rax], 0
    ret
copy_path_safe ENDP

END
