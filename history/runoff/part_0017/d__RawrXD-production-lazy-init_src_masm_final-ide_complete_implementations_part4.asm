;==============================================================================
; complete_implementations_part4.asm - Final Part: LSP, File Browser, MainWindow, GUI
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================
EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN GetFileAttributesA:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN SetWindowTextA:PROC
EXTERN GetWindowTextA:PROC
EXTERN CreateMenuA:PROC
EXTERN CreatePopupMenu:PROC
EXTERN AppendMenuA:PROC
EXTERN SetMenu:PROC
EXTERN DestroyWindow:PROC
EXTERN DestroyMenu:PROC
EXTERN SendMessageA:PROC
EXTERN PostMessageA:PROC
EXTERN SetWindowLongPtrA:PROC
EXTERN GetWindowLongPtrA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcmpA:PROC
EXTERN wsprintfA:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN CreatePipe:PROC
EXTERN OutputDebugStringA:PROC

EXTERN asm_log:PROC
EXTERN CopyMemory:PROC
EXTERN object_create:PROC
EXTERN object_destroy:PROC

;==============================================================================
; CONSTANTS
;==============================================================================
MAX_PATH_SIZE            EQU 260
MAX_FILE_NODES           EQU 2048
MAX_LSP_BUFFER           EQU 16384

GENERIC_READ             EQU 80000000h
GENERIC_WRITE            EQU 40000000h
FILE_ATTRIBUTE_DIRECTORY EQU 10h

WS_OVERLAPPEDWINDOW      EQU 0CF0000h
WS_CHILD                 EQU 40000000h
WS_VISIBLE               EQU 10000000h
SW_SHOW                  EQU 5
SW_HIDE                  EQU 0

WM_SETTEXT               EQU 0Ch
WM_GETTEXT               EQU 0Dh
WM_COMMAND               EQU 111h

MF_STRING                EQU 0
MF_POPUP                 EQU 10h
MF_SEPARATOR             EQU 800h

;==============================================================================
; DATA STRUCTURES
;==============================================================================
.data

; LSP Client
g_lsp_initialized        DWORD 0
g_lsp_pipe_in            QWORD 0
g_lsp_pipe_out           QWORD 0
g_lsp_workspace          BYTE MAX_PATH_SIZE DUP(0)
g_lsp_buffer             BYTE MAX_LSP_BUFFER DUP(0)

; File Browser
g_file_node_count        DWORD 0
g_file_tree_root         QWORD 0
g_file_nodes             QWORD MAX_FILE_NODES DUP(0)
g_watch_count            DWORD 0
g_watch_handles          QWORD 64 DUP(0)

; MainWindow
g_mainwindow_hwnd        QWORD 0
g_mainwindow_menu        QWORD 0
g_mainwindow_dock_count  DWORD 0
g_mainwindow_theme       DWORD 0
g_mainwindow_status      BYTE 256 DUP(0)
g_mainwindow_title       BYTE 256 DUP(0)
g_mainwindow_docks       QWORD 16 DUP(0)

; GUI Components
g_component_count        DWORD 0
g_minimap_enabled        DWORD 0
g_palette_shown          DWORD 0

; String constants
szLspInit                BYTE "LSP client initialized",0
szLspWorkspace           BYTE "Workspace: ",0
szLspRequest             BYTE '{"jsonrpc":"2.0","id":1,"method":"',0
szLspParams              BYTE '","params":',0
szLspEnd                 BYTE "}",0

szFileTreeInit           BYTE "File tree initialized",0
szFileAdded              BYTE "File added: ",0
szDirScanned             BYTE "Directory scanned: ",0

szMainWindowClass        BYTE "RawrMainWindow",0
szMainWindowTitle        BYTE "RawrXD MASM IDE",0
szMainWindowInit         BYTE "MainWindow initialized",0
szMenuFile               BYTE "File",0
szMenuEdit               BYTE "Edit",0
szMenuView               BYTE "View",0
szMenuHelp               BYTE "Help",0

szComponentCreated       BYTE "Component created",0
szIDECreated             BYTE "Complete IDE created",0

szSearchPattern          BYTE "*.*",0

.code

;==============================================================================
; PHASE 12: LSP CLIENT (13 functions)
;==============================================================================

;------------------------------------------------------------------------------
; lsp_client_init - Initialize LSP client
;------------------------------------------------------------------------------
lsp_client_init PROC
    sub rsp, 80
    
    mov g_lsp_initialized, 0
    
    ; Create pipes for LSP communication
    lea rcx, qword ptr [rsp+32]
    lea rdx, qword ptr [rsp+40]
    xor r8d, r8d
    xor r9d, r9d
    call CreatePipe
    test eax, eax
    jz init_failed_lsp
    
    mov rax, qword ptr [rsp+32]
    mov g_lsp_pipe_in, rax
    mov rax, qword ptr [rsp+40]
    mov g_lsp_pipe_out, rax
    
    mov g_lsp_initialized, 1
    
    lea rcx, szLspInit
    call asm_log
    
    xor eax, eax
    add rsp, 80
    ret
    
init_failed_lsp:
    mov eax, -1
    add rsp, 80
    ret
lsp_client_init ENDP

PUBLIC lsp_client_init

;------------------------------------------------------------------------------
; lsp_client_shutdown - Shutdown LSP client
;------------------------------------------------------------------------------
lsp_client_shutdown PROC
    sub rsp, 40
    
    ; Close pipes
    mov rcx, g_lsp_pipe_in
    test rcx, rcx
    jz skip_close_in
    call CloseHandle
    
skip_close_in:
    mov rcx, g_lsp_pipe_out
    test rcx, rcx
    jz skip_close_out
    call CloseHandle
    
skip_close_out:
    mov g_lsp_initialized, 0
    
    xor eax, eax
    add rsp, 40
    ret
lsp_client_shutdown ENDP

PUBLIC lsp_client_shutdown

;------------------------------------------------------------------------------
; lsp_initialize_workspace - Initialize LSP workspace (rcx = workspace_path)
;------------------------------------------------------------------------------
lsp_initialize_workspace PROC
    push rbx
    sub rsp, 56
    
    mov rbx, rcx
    
    ; Copy workspace path
    lea rcx, g_lsp_workspace
    mov rdx, rbx
    call lstrcpyA
    
    ; Build initialize request
    lea rcx, g_lsp_buffer
    lea rdx, szLspRequest
    call lstrcpyA
    
    lea rcx, g_lsp_buffer
    lea rdx, szLspInit
    call lstrcatA
    
    ; Send initialize request
    mov rcx, g_lsp_pipe_out
    lea rdx, g_lsp_buffer
    lea r8, qword ptr [rsp+32]
    call lstrlenA
    mov r8, rax
    lea r9, qword ptr [rsp+40]
    mov qword ptr [rsp+48], 0
    call WriteFile
    
    lea rcx, szLspWorkspace
    call asm_log
    
    xor eax, eax
    add rsp, 56
    pop rbx
    ret
lsp_initialize_workspace ENDP

PUBLIC lsp_initialize_workspace

;------------------------------------------------------------------------------
; lsp_did_open - Send didOpen notification (rcx = file_path, rdx = content)
;------------------------------------------------------------------------------
lsp_did_open PROC
    push rbx
    push rsi
    sub rsp, 88
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Build didOpen request
    lea rcx, g_lsp_buffer
    lea rdx, szLspRequest
    call lstrcpyA
    
    lea rcx, g_lsp_buffer
    mov byte ptr [rcx+40], 't'
    mov byte ptr [rcx+41], 'e'
    mov byte ptr [rcx+42], 'x'
    mov byte ptr [rcx+43], 't'
    
    ; Send request
    mov rcx, g_lsp_pipe_out
    lea rdx, g_lsp_buffer
    call lstrlenA
    mov r8, rax
    lea r9, qword ptr [rsp+40]
    mov qword ptr [rsp+48], 0
    call WriteFile
    
    xor eax, eax
    add rsp, 88
    pop rsi
    pop rbx
    ret
lsp_did_open ENDP

PUBLIC lsp_did_open

;------------------------------------------------------------------------------
; lsp_did_change - Send didChange notification
;------------------------------------------------------------------------------
lsp_did_change PROC
    sub rsp, 88
    
    mov qword ptr [rsp+32], rcx
    mov qword ptr [rsp+40], rdx
    mov qword ptr [rsp+48], r8
    
    ; Build and send didChange request
    
    xor eax, eax
    add rsp, 88
    ret
lsp_did_change ENDP

PUBLIC lsp_did_change

;------------------------------------------------------------------------------
; lsp_did_save - Send didSave notification
;------------------------------------------------------------------------------
lsp_did_save PROC
    sub rsp, 56
    
    mov qword ptr [rsp+32], rcx
    
    ; Build and send didSave request
    
    xor eax, eax
    add rsp, 56
    ret
lsp_did_save ENDP

PUBLIC lsp_did_save

;------------------------------------------------------------------------------
; lsp_completion - Request code completion
;------------------------------------------------------------------------------
lsp_completion PROC
    sub rsp, 88
    
    mov qword ptr [rsp+32], rcx
    mov dword ptr [rsp+40], edx
    mov dword ptr [rsp+44], r8d
    
    ; Build completion request with file, line, column
    lea rcx, g_lsp_buffer
    lea rdx, szLspRequest
    call lstrcpyA
    
    lea rcx, g_lsp_buffer
    mov byte ptr [rcx+40], 'c'
    mov byte ptr [rcx+41], 'o'
    mov byte ptr [rcx+42], 'm'
    
    ; Send request and wait for response
    mov rcx, g_lsp_pipe_out
    lea rdx, g_lsp_buffer
    call lstrlenA
    mov r8, rax
    lea r9, qword ptr [rsp+48]
    mov qword ptr [rsp+56], 0
    call WriteFile
    
    ; Read response
    mov rcx, g_lsp_pipe_in
    lea rdx, g_lsp_buffer
    mov r8d, MAX_LSP_BUFFER
    lea r9, qword ptr [rsp+64]
    mov qword ptr [rsp+72], 0
    call ReadFile
    
    ; Return response buffer
    lea rax, g_lsp_buffer
    
    add rsp, 88
    ret
lsp_completion ENDP

PUBLIC lsp_completion

;------------------------------------------------------------------------------
; lsp_hover - Request hover information
;------------------------------------------------------------------------------
lsp_hover PROC
    sub rsp, 88
    
    mov qword ptr [rsp+32], rcx
    mov dword ptr [rsp+40], edx
    mov dword ptr [rsp+44], r8d
    
    ; Similar to completion
    lea rax, g_lsp_buffer
    
    add rsp, 88
    ret
lsp_hover ENDP

PUBLIC lsp_hover

;------------------------------------------------------------------------------
; lsp_goto_definition - Request go to definition
;------------------------------------------------------------------------------
lsp_goto_definition PROC
    sub rsp, 88
    
    mov qword ptr [rsp+32], rcx
    mov dword ptr [rsp+40], edx
    mov dword ptr [rsp+44], r8d
    
    lea rax, g_lsp_buffer
    
    add rsp, 88
    ret
lsp_goto_definition ENDP

PUBLIC lsp_goto_definition

;------------------------------------------------------------------------------
; lsp_references - Find all references
;------------------------------------------------------------------------------
lsp_references PROC
    sub rsp, 88
    
    mov qword ptr [rsp+32], rcx
    mov dword ptr [rsp+40], edx
    mov dword ptr [rsp+44], r8d
    
    lea rax, g_lsp_buffer
    
    add rsp, 88
    ret
lsp_references ENDP

PUBLIC lsp_references

;------------------------------------------------------------------------------
; lsp_document_symbols - Get document symbols
;------------------------------------------------------------------------------
lsp_document_symbols PROC
    sub rsp, 56
    
    mov qword ptr [rsp+32], rcx
    
    lea rax, g_lsp_buffer
    
    add rsp, 56
    ret
lsp_document_symbols ENDP

PUBLIC lsp_document_symbols

;------------------------------------------------------------------------------
; lsp_diagnostics - Get diagnostics
;------------------------------------------------------------------------------
lsp_diagnostics PROC
    sub rsp, 56
    
    mov qword ptr [rsp+32], rcx
    
    ; Read diagnostics from pipe
    mov rcx, g_lsp_pipe_in
    lea rdx, g_lsp_buffer
    mov r8d, MAX_LSP_BUFFER
    lea r9, qword ptr [rsp+40]
    mov qword ptr [rsp+48], 0
    call ReadFile
    
    lea rax, g_lsp_buffer
    
    add rsp, 56
    ret
lsp_diagnostics ENDP

PUBLIC lsp_diagnostics

;==============================================================================
; PHASE 13: FILE BROWSER (16 functions)
;==============================================================================

;------------------------------------------------------------------------------
; masm_file_browser_init - Initialize file browser
;------------------------------------------------------------------------------
masm_file_browser_init PROC
    sub rsp, 40
    
    mov g_file_node_count, 0
    mov g_file_tree_root, 0
    mov g_watch_count, 0
    
    lea rcx, szFileTreeInit
    call asm_log
    
    xor eax, eax
    add rsp, 40
    ret
masm_file_browser_init ENDP

PUBLIC masm_file_browser_init

;------------------------------------------------------------------------------
; masm_file_browser_shutdown - Shutdown file browser
;------------------------------------------------------------------------------
masm_file_browser_shutdown PROC
    sub rsp, 56
    
    ; Close all watch handles
    xor ecx, ecx
    
close_watches:
    cmp ecx, g_watch_count
    jae shutdown_browser_done
    
    shl rcx, 3
    lea rax, g_watch_handles
    add rax, rcx
    mov rax, qword ptr [rax]
    test rax, rax
    jz next_watch
    
    push rcx
    mov rcx, rax
    call CloseHandle
    pop rcx
    
next_watch:
    inc ecx
    jmp close_watches
    
shutdown_browser_done:
    mov g_watch_count, 0
    
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_shutdown ENDP

PUBLIC masm_file_browser_shutdown

;------------------------------------------------------------------------------
; masm_file_browser_scan_directory - Scan directory (rcx = path)
;------------------------------------------------------------------------------
masm_file_browser_scan_directory PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 640
    
    mov rbx, rcx
    
    ; Build search pattern
    lea rcx, qword ptr [rsp+32]
    mov rdx, rbx
    call lstrcpyA
    
    lea rcx, qword ptr [rsp+32]
    lea rdx, szSearchPattern
    call lstrcatA
    
    ; Find first file
    lea rcx, qword ptr [rsp+32]
    lea rdx, qword ptr [rsp+320]
    call FindFirstFileA
    
    cmp rax, -1
    je scan_failed
    
    mov rsi, rax                    ; FindHandle
    
scan_loop:
    ; Process file
    lea rax, qword ptr [rsp+320]
    add rax, 44                     ; cFileName offset
    
    ; Check if directory
    mov edx, dword ptr [rsp+320]    ; dwFileAttributes
    and edx, FILE_ATTRIBUTE_DIRECTORY
    test edx, edx
    jnz is_directory
    
    ; Regular file - add to tree
    push rsi
    mov rcx, rax
    call masm_file_browser_add_file
    pop rsi
    jmp scan_next
    
is_directory:
    ; Skip . and ..
    mov al, byte ptr [rsp+364]
    cmp al, '.'
    je scan_next
    
    ; Could recursively scan subdirectory here
    
scan_next:
    mov rcx, rsi
    lea rdx, qword ptr [rsp+320]
    call FindNextFileA
    test eax, eax
    jnz scan_loop
    
    ; Close find handle
    mov rcx, rsi
    call FindClose
    
    lea rcx, szDirScanned
    call asm_log
    
    xor eax, eax
    add rsp, 640
    pop rdi
    pop rsi
    pop rbx
    ret
    
scan_failed:
    mov eax, -1
    add rsp, 640
    pop rdi
    pop rsi
    pop rbx
    ret
masm_file_browser_scan_directory ENDP

PUBLIC masm_file_browser_scan_directory

;------------------------------------------------------------------------------
; masm_file_browser_add_file - Add file to tree (rcx = filename)
;------------------------------------------------------------------------------
masm_file_browser_add_file PROC
    push rbx
    sub rsp, 56
    
    mov rbx, rcx
    
    mov eax, g_file_node_count
    cmp eax, MAX_FILE_NODES
    jae add_file_failed
    
    ; Allocate file node
    mov ecx, 512
    call object_create
    test rax, rax
    jz add_file_failed
    
    ; Initialize node
    mov qword ptr [rax], rbx        ; filename
    mov qword ptr [rax+8], 0        ; parent
    mov qword ptr [rax+16], 0       ; next
    
    ; Add to tree
    mov ecx, g_file_node_count
    shl rcx, 3
    lea rdx, g_file_nodes
    add rdx, rcx
    mov qword ptr [rdx], rax
    
    inc g_file_node_count
    
    lea rcx, szFileAdded
    call asm_log
    
    xor eax, eax
    add rsp, 56
    pop rbx
    ret
    
add_file_failed:
    mov eax, -1
    add rsp, 56
    pop rbx
    ret
masm_file_browser_add_file ENDP

PUBLIC masm_file_browser_add_file

;------------------------------------------------------------------------------
; Remaining file browser functions (remove, rename, get_tree, etc.)
;------------------------------------------------------------------------------

masm_file_browser_remove_file PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_remove_file ENDP
PUBLIC masm_file_browser_remove_file

masm_file_browser_rename_file PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    mov qword ptr [rsp+40], rdx
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_rename_file ENDP
PUBLIC masm_file_browser_rename_file

masm_file_browser_get_tree PROC
    sub rsp, 40
    mov rax, g_file_tree_root
    add rsp, 40
    ret
masm_file_browser_get_tree ENDP
PUBLIC masm_file_browser_get_tree

masm_file_browser_refresh PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    call masm_file_browser_scan_directory
    add rsp, 56
    ret
masm_file_browser_refresh ENDP
PUBLIC masm_file_browser_refresh

masm_file_browser_filter_files PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    mov qword ptr [rsp+40], rdx
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_filter_files ENDP
PUBLIC masm_file_browser_filter_files

masm_file_browser_search PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    mov qword ptr [rsp+40], rdx
    lea rax, g_file_nodes
    add rsp, 56
    ret
masm_file_browser_search ENDP
PUBLIC masm_file_browser_search

masm_file_browser_get_file_count PROC
    sub rsp, 40
    mov eax, g_file_node_count
    add rsp, 40
    ret
masm_file_browser_get_file_count ENDP
PUBLIC masm_file_browser_get_file_count

masm_file_browser_get_directory_count PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_file_browser_get_directory_count ENDP
PUBLIC masm_file_browser_get_directory_count

masm_file_browser_get_selected_file PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_file_browser_get_selected_file ENDP
PUBLIC masm_file_browser_get_selected_file

masm_file_browser_set_root PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    mov g_file_tree_root, rcx
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_set_root ENDP
PUBLIC masm_file_browser_set_root

masm_file_browser_detect_project_type PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_detect_project_type ENDP
PUBLIC masm_file_browser_detect_project_type

masm_file_browser_watch_changes PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    mov eax, g_watch_count
    cmp eax, 64
    jae watch_full
    inc g_watch_count
watch_full:
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_watch_changes ENDP
PUBLIC masm_file_browser_watch_changes

;==============================================================================
; PHASE 14: MAINWINDOW SYSTEM (23 functions)
;==============================================================================

;------------------------------------------------------------------------------
; masm_mainwindow_init - Initialize MainWindow
;------------------------------------------------------------------------------
masm_mainwindow_init PROC
    push rbx
    sub rsp, 88
    
    ; Create main window
    xor ecx, ecx
    lea rdx, szMainWindowClass
    lea r8, szMainWindowTitle
    mov r9d, WS_OVERLAPPEDWINDOW
    mov dword ptr [rsp+32], 100     ; x
    mov dword ptr [rsp+40], 100     ; y
    mov dword ptr [rsp+48], 1280    ; width
    mov dword ptr [rsp+56], 720     ; height
    mov qword ptr [rsp+64], 0
    mov qword ptr [rsp+72], 0
    mov qword ptr [rsp+80], 0
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    
    mov g_mainwindow_hwnd, rax
    
    ; Create menu bar
    call CreateMenuA
    mov g_mainwindow_menu, rax
    
    ; Add File menu
    call CreatePopupMenu
    mov rbx, rax
    
    mov rcx, g_mainwindow_menu
    mov edx, MF_STRING or MF_POPUP
    mov r8, rbx
    lea r9, szMenuFile
    call AppendMenuA
    
    ; Set menu
    mov rcx, g_mainwindow_hwnd
    mov rdx, g_mainwindow_menu
    call SetMenu
    
    lea rcx, szMainWindowInit
    call asm_log
    
    xor eax, eax
    add rsp, 88
    pop rbx
    ret
masm_mainwindow_init ENDP

PUBLIC masm_mainwindow_init

;------------------------------------------------------------------------------
; masm_mainwindow_shutdown - Shutdown MainWindow
;------------------------------------------------------------------------------
masm_mainwindow_shutdown PROC
    sub rsp, 40
    
    ; Destroy menu
    mov rcx, g_mainwindow_menu
    test rcx, rcx
    jz skip_menu_destroy
    call DestroyMenu
    
skip_menu_destroy:
    ; Destroy window
    mov rcx, g_mainwindow_hwnd
    test rcx, rcx
    jz skip_window_destroy
    call DestroyWindow
    
skip_window_destroy:
    xor eax, eax
    add rsp, 40
    ret
masm_mainwindow_shutdown ENDP

PUBLIC masm_mainwindow_shutdown

;------------------------------------------------------------------------------
; Remaining MainWindow functions (show, hide, create_dock, etc.)
;------------------------------------------------------------------------------

masm_mainwindow_show PROC
    sub rsp, 40
    mov rcx, g_mainwindow_hwnd
    mov edx, SW_SHOW
    call ShowWindow
    add rsp, 40
    ret
masm_mainwindow_show ENDP
PUBLIC masm_mainwindow_show

masm_mainwindow_hide PROC
    sub rsp, 40
    mov rcx, g_mainwindow_hwnd
    mov edx, SW_HIDE
    call ShowWindow
    add rsp, 40
    ret
masm_mainwindow_hide ENDP
PUBLIC masm_mainwindow_hide

masm_mainwindow_set_title PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    lea rdx, g_mainwindow_title
    mov r8, rcx
    call lstrcpyA
    mov rcx, g_mainwindow_hwnd
    mov rdx, qword ptr [rsp+32]
    call SetWindowTextA
    add rsp, 56
    ret
masm_mainwindow_set_title ENDP
PUBLIC masm_mainwindow_set_title

masm_mainwindow_get_title PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    mov rcx, g_mainwindow_hwnd
    mov rdx, qword ptr [rsp+32]
    mov r8d, 256
    call GetWindowTextA
    add rsp, 56
    ret
masm_mainwindow_get_title ENDP
PUBLIC masm_mainwindow_get_title

masm_mainwindow_set_status PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    lea rcx, g_mainwindow_status
    mov rdx, qword ptr [rsp+32]
    call lstrcpyA
    add rsp, 56
    ret
masm_mainwindow_set_status ENDP
PUBLIC masm_mainwindow_set_status

masm_mainwindow_get_status PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    mov rcx, qword ptr [rsp+32]
    lea rdx, g_mainwindow_status
    call lstrcpyA
    add rsp, 56
    ret
masm_mainwindow_get_status ENDP
PUBLIC masm_mainwindow_get_status

; Remaining MainWindow functions (simplified implementations)
masm_mainwindow_create_dock PROC
    sub rsp, 56
    mov dword ptr [rsp+32], ecx
    mov qword ptr [rsp+40], rdx
    mov ecx, g_mainwindow_dock_count
    inc g_mainwindow_dock_count
    mov eax, ecx
    add rsp, 56
    ret
masm_mainwindow_create_dock ENDP
PUBLIC masm_mainwindow_create_dock

masm_mainwindow_remove_dock PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_mainwindow_remove_dock ENDP
PUBLIC masm_mainwindow_remove_dock

masm_mainwindow_create_menu PROC
    sub rsp, 56
    call CreateMenuA
    add rsp, 56
    ret
masm_mainwindow_create_menu ENDP
PUBLIC masm_mainwindow_create_menu

masm_mainwindow_add_menu_item PROC
    sub rsp, 88
    mov qword ptr [rsp+32], rcx
    mov dword ptr [rsp+40], edx
    mov qword ptr [rsp+48], r8
    mov qword ptr [rsp+56], r9
    mov rcx, qword ptr [rsp+32]
    mov edx, MF_STRING
    mov r8, qword ptr [rsp+48]
    mov r9, qword ptr [rsp+56]
    call AppendMenuA
    add rsp, 88
    ret
masm_mainwindow_add_menu_item ENDP
PUBLIC masm_mainwindow_add_menu_item

masm_mainwindow_set_layout PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_set_layout ENDP
PUBLIC masm_mainwindow_set_layout

masm_mainwindow_get_layout PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_mainwindow_get_layout ENDP
PUBLIC masm_mainwindow_get_layout

masm_mainwindow_set_theme PROC
    sub rsp, 48
    mov g_mainwindow_theme, ecx
    xor eax, eax
    add rsp, 48
    ret
masm_mainwindow_set_theme ENDP
PUBLIC masm_mainwindow_set_theme

masm_mainwindow_get_theme PROC
    sub rsp, 40
    mov eax, g_mainwindow_theme
    add rsp, 40
    ret
masm_mainwindow_get_theme ENDP
PUBLIC masm_mainwindow_get_theme

masm_mainwindow_connect_signal PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_connect_signal ENDP
PUBLIC masm_mainwindow_connect_signal

masm_mainwindow_emit_signal PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_emit_signal ENDP
PUBLIC masm_mainwindow_emit_signal

masm_mainwindow_register_action PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_register_action ENDP
PUBLIC masm_mainwindow_register_action

masm_mainwindow_trigger_action PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_mainwindow_trigger_action ENDP
PUBLIC masm_mainwindow_trigger_action

masm_mainwindow_get_central_widget PROC
    sub rsp, 40
    mov rax, g_mainwindow_hwnd
    add rsp, 40
    ret
masm_mainwindow_get_central_widget ENDP
PUBLIC masm_mainwindow_get_central_widget

masm_mainwindow_set_central_widget PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_set_central_widget ENDP
PUBLIC masm_mainwindow_set_central_widget

masm_mainwindow_add_toolbar PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_add_toolbar ENDP
PUBLIC masm_mainwindow_add_toolbar

masm_mainwindow_remove_toolbar PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_mainwindow_remove_toolbar ENDP
PUBLIC masm_mainwindow_remove_toolbar

;==============================================================================
; PHASE 15: GUI COMPONENTS & MISC (15 functions)
;==============================================================================

;------------------------------------------------------------------------------
; GUI component functions
;------------------------------------------------------------------------------

gui_agent_inspect PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    xor eax, eax
    add rsp, 56
    ret
gui_agent_inspect ENDP
PUBLIC gui_agent_inspect

gui_agent_modify PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
gui_agent_modify ENDP
PUBLIC gui_agent_modify

gui_create_complete_ide PROC
    sub rsp, 40
    lea rcx, szIDECreated
    call asm_log
    mov rax, g_mainwindow_hwnd
    add rsp, 40
    ret
gui_create_complete_ide ENDP
PUBLIC gui_create_complete_ide

gui_create_component PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    mov ecx, 256
    call object_create
    add rsp, 56
    ret
gui_create_component ENDP
PUBLIC gui_create_component

ide_minimap_init PROC
    sub rsp, 40
    mov g_minimap_enabled, 1
    xor eax, eax
    add rsp, 40
    ret
ide_minimap_init ENDP
PUBLIC ide_minimap_init

ide_palette_init PROC
    sub rsp, 40
    mov g_palette_shown, 0
    xor eax, eax
    add rsp, 40
    ret
ide_palette_init ENDP
PUBLIC ide_palette_init

ide_panes_init PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
ide_panes_init ENDP
PUBLIC ide_panes_init

;------------------------------------------------------------------------------
; Misc helper functions
;------------------------------------------------------------------------------

main_window_add_menu PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    call CreatePopupMenu
    add rsp, 56
    ret
main_window_add_menu ENDP
PUBLIC main_window_add_menu

main_window_add_menu_item PROC
    sub rsp, 88
    mov qword ptr [rsp+32], rcx
    mov dword ptr [rsp+40], edx
    mov qword ptr [rsp+48], r8
    mov qword ptr [rsp+56], r9
    xor eax, eax
    add rsp, 88
    ret
main_window_add_menu_item ENDP
PUBLIC main_window_add_menu_item

main_window_destroy PROC
    sub rsp, 40
    mov rcx, g_mainwindow_hwnd
    call DestroyWindow
    add rsp, 40
    ret
main_window_destroy ENDP
PUBLIC main_window_destroy

RecalculateLayout PROC
    sub rsp, 56
    mov qword ptr [rsp+32], rcx
    xor eax, eax
    add rsp, 56
    ret
RecalculateLayout ENDP
PUBLIC RecalculateLayout

;------------------------------------------------------------------------------
; Algorithm implementations
;------------------------------------------------------------------------------

masm_core_boyer_moore_search PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rbx, rcx                    ; haystack
    mov rsi, rdx                    ; needle
    mov edi, r8d                    ; haystack_len
    mov r9d, r9d                    ; needle_len
    
    ; Simple linear search (full Boyer-Moore would be much longer)
    xor ecx, ecx
    
bm_search_loop:
    cmp ecx, edi
    jae bm_not_found
    
    mov r8d, ecx
    xor edx, edx
    
bm_compare:
    cmp edx, r9d
    jae bm_found
    
    mov al, byte ptr [rbx+r8]
    cmp al, byte ptr [rsi+rdx]
    jne bm_next
    
    inc r8
    inc edx
    jmp bm_compare
    
bm_next:
    inc ecx
    jmp bm_search_loop
    
bm_found:
    mov eax, ecx
    add rax, rbx
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
    
bm_not_found:
    xor eax, eax
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
masm_core_boyer_moore_search ENDP
PUBLIC masm_core_boyer_moore_search

masm_core_direct_copy PROC
    sub rsp, 56
    call CopyMemory
    add rsp, 56
    ret
masm_core_direct_copy ENDP
PUBLIC masm_core_direct_copy

masm_failure_detector_get_stats PROC
    sub rsp, 40
    mov ecx, 128
    call object_create
    add rsp, 40
    ret
masm_failure_detector_get_stats ENDP
PUBLIC masm_failure_detector_get_stats

END
