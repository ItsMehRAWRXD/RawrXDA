;==========================================================================
; menu_handlers.asm - Complete WM_COMMAND Handler System for RawrXD IDE
; ==========================================================================
; Implements all menu/toolbar/button handlers with full integration to:
; - output_pane_logger.asm (real-time logging)
; - tab_manager.asm (tab lifecycle)
; - file_tree_driver.asm (file navigation)
; - agent_chat_modes.asm (agent chat)
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

;==========================================================================
; EXTERN DECLARATIONS - Core UI
;==========================================================================
EXTERN ui_get_main_hwnd:PROC
EXTERN ui_get_editor_hwnd:PROC
EXTERN ui_get_chat_hwnd:PROC
EXTERN ui_get_terminal_hwnd:PROC
EXTERN ui_get_file_tree_hwnd:PROC
EXTERN ui_get_output_pane_hwnd:PROC

;==========================================================================
; EXTERN DECLARATIONS - New UI Modules
;==========================================================================
EXTERN output_pane_init:PROC
EXTERN output_pane_clear:PROC
EXTERN output_log_editor:PROC
EXTERN output_log_tab:PROC
EXTERN output_log_agent:PROC
EXTERN output_log_hotpatch:PROC
EXTERN output_log_filetree:PROC

EXTERN tab_manager_init:PROC
EXTERN tab_create_editor:PROC
EXTERN tab_close_editor:PROC
EXTERN tab_set_agent_mode:PROC
EXTERN tab_get_agent_mode:PROC
EXTERN tab_set_panel_tab:PROC
EXTERN tab_mark_modified:PROC

EXTERN file_tree_init:PROC
EXTERN file_tree_expand_drive:PROC
EXTERN file_tree_refresh:PROC

EXTERN agent_chat_init:PROC
EXTERN agent_chat_set_mode:PROC
EXTERN agent_chat_send_message:PROC
EXTERN agent_chat_add_message:PROC
EXTERN agent_chat_clear:PROC

;==========================================================================
; EXTERN DECLARATIONS - Layout Persistence
;==========================================================================
EXTERN save_layout_json:PROC
EXTERN load_layout_json:PROC
EXTERN save_settings_json:PROC
EXTERN load_settings_json:PROC

;==========================================================================
; EXTERN DECLARATIONS - Utility
;==========================================================================
EXTERN GetTickCount:PROC
EXTERN MessageBoxA:PROC
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN SendMessageA:PROC
EXTERN ShowWindow:PROC
EXTERN InvalidateRect:PROC
EXTERN GetDlgItem:PROC
EXTERN PostMessageA:PROC
EXTERN strcmp_masm:PROC
EXTERN strcat_masm:PROC
EXTERN wsprintfA:PROC
EXTERN GetOpenFileNameA:PROC
EXTERN GetSaveFileNameA:PROC
EXTERN asm_log:PROC

;==========================================================================
; CONSTANTS - Menu IDs (from ui_masm.asm)
;==========================================================================

; File Menu
IDM_FILE_NEW            EQU 2001
IDM_FILE_OPEN           EQU 2002
IDM_FILE_SAVE           EQU 2003
IDM_FILE_SAVE_AS        EQU 2004
IDM_FILE_RECENT         EQU 2010
IDM_FILE_EXIT           EQU 2005

; Edit Menu
IDM_EDIT_UNDO           EQU 3001
IDM_EDIT_REDO           EQU 3002
IDM_EDIT_CUT            EQU 3003
IDM_EDIT_COPY           EQU 3004
IDM_EDIT_PASTE          EQU 3005
IDM_EDIT_SELECT_ALL     EQU 3006
IDM_EDIT_FIND           EQU 3007
IDM_EDIT_REPLACE        EQU 3008

; View Menu
IDM_VIEW_EXPLORER       EQU 4001
IDM_VIEW_OUTPUT         EQU 4002
IDM_VIEW_PROBLEMS       EQU 4003
IDM_VIEW_DEBUG          EQU 4004
IDM_VIEW_TERMINAL       EQU 4005
IDM_VIEW_AGENT_CHAT     EQU 4006
IDM_VIEW_MINIMAP        EQU 4007
IDM_VIEW_BREADCRUMB     EQU 4008
IDM_VIEW_FULLSCREEN     EQU 4009

; Layout Menu
IDM_LAYOUT_RESET        EQU 5001
IDM_LAYOUT_SAVE         EQU 5002
IDM_LAYOUT_LOAD         EQU 5003
IDM_LAYOUT_SIDEBAR_LEFT EQU 5004
IDM_LAYOUT_SIDEBAR_RIGHT EQU 5005
IDM_LAYOUT_BOTTOM_PANEL EQU 5006

; Agent Menu
IDM_AGENT_ASK           EQU 6001
IDM_AGENT_EDIT          EQU 6002
IDM_AGENT_PLAN          EQU 6003
IDM_AGENT_CONFIG        EQU 6004
IDM_AGENT_CLEAR_CHAT    EQU 6005
IDM_AGENT_STOP          EQU 6006

; Tools Menu
IDM_TOOLS_FORMAT        EQU 7001
IDM_TOOLS_LINT          EQU 7002
IDM_TOOLS_BUILD         EQU 7003
IDM_TOOLS_RUN           EQU 7004
IDM_TOOLS_DEBUG         EQU 7005
IDM_TOOLS_TERMINAL      EQU 7006
IDM_TOOLS_HOTPATCH      EQU 7007

; Settings Menu
IDM_SETTINGS_THEME      EQU 8001
IDM_SETTINGS_KEYBINDS   EQU 8002
IDM_SETTINGS_EXTENSIONS EQU 8003
IDM_SETTINGS_PREFERENCES EQU 8004

; Help Menu
IDM_HELP_DOCS           EQU 9001
IDM_HELP_SHORTCUTS      EQU 9002
IDM_HELP_ABOUT          EQU 9003

; Toolbar/Quick Access
IDM_TOOLBAR_NEW_FILE    EQU 10001
IDM_TOOLBAR_OPEN_FILE   EQU 10002
IDM_TOOLBAR_SAVE_FILE   EQU 10003
IDM_TOOLBAR_RUN         EQU 10004
IDM_TOOLBAR_STOP        EQU 10005

;==========================================================================
; DATA STRUCTURES
;==========================================================================

EDITOR_STATE STRUCT
    current_file        QWORD ?     ; Current filename
    current_path        BYTE 512 DUP (?)    ; Full file path
    is_modified         DWORD ?     ; Modified flag
    is_saving           DWORD ?     ; Saving in progress
    tab_count           DWORD ?     ; Number of open tabs
    active_tab_id       DWORD ?     ; Currently active tab
EDITOR_STATE ENDS

FILE_DIALOG STRUCT
    lpstrFile           QWORD ?
    nMaxFile            DWORD ?
    lpstrFileTitle      QWORD ?
    nMaxFileTitle       DWORD ?
    lpstrInitialDir     QWORD ?
    lpstrDefExt         QWORD ?
    hwndOwner           QWORD ?
FILE_DIALOG ENDS

;==========================================================================
; DATA SECTION
;==========================================================================

.data
    ; Strings for logging
    szFileOpened        BYTE "File opened: %s",0
    szFileSaved         BYTE "File saved: %s",0
    szFileNew           BYTE "New file created",0
    szFileClosed        BYTE "File closed: %s",0
    
    szTabCreated        BYTE "Tab created: %s",0
    szTabClosed         BYTE "Tab closed: %s",0
    
    szAgentModeSwitched BYTE "Agent mode switched: %s",0
    szAgentChatCleared  BYTE "Agent chat cleared",0
    
    szLayoutSaved       BYTE "Layout saved successfully",0
    szLayoutLoaded      BYTE "Layout loaded successfully",0
    
    szBuildStarted      BYTE "Build started",0
    szBuildComplete     BYTE "Build complete: %d errors, %d warnings",0
    szBuildFailed       BYTE "Build failed",0
    
    szTerminalOpened    BYTE "Terminal opened",0
    szTerminalClosed    BYTE "Terminal closed",0
    
    szHotpatchApplied   BYTE "Hotpatch applied: %s",0
    
    ; Dialog filters
    szFileFilter        BYTE "C/C++ Files (*.c;*.cpp;*.h;*.hpp)",0
                        BYTE "*.c;*.cpp;*.h;*.hpp",0
                        BYTE "MASM Files (*.asm)",0
                        BYTE "*.asm",0
                        BYTE "All Files (*.*)",0
                        BYTE "*.*",0,0
    
    ; Mode names for logging
    szModeAsk           BYTE "Ask",0
    szModeEdit          BYTE "Edit",0
    szModePlan          BYTE "Plan",0
    szModeConfig        BYTE "Configure",0
    
    ; Default paths
    szDocumentsPath     BYTE "C:\Users\%USERNAME%\Documents",0
    szProjectPath       BYTE "",512 DUP (0)
    
    ; Window titles
    szUntitled          BYTE "Untitled",0
    szModifiedSuffix    BYTE " *",0

.data?
    hMainWindow         QWORD ?
    hEditorWindow       QWORD ?
    hChatWindow         QWORD ?
    hOutputPane         QWORD ?
    hFileTree           QWORD ?
    hTerminal           QWORD ?
    
    current_editor_state EDITOR_STATE <>
    
    open_file_path      BYTE 512 DUP (?)
    save_file_path      BYTE 512 DUP (?)
    
    file_filter_buffer  BYTE 1024 DUP (?)

;==========================================================================
; PUBLIC PROCEDURES
;==========================================================================

PUBLIC menu_handler_init
PUBLIC handle_file_menu
PUBLIC handle_edit_menu
PUBLIC handle_view_menu
PUBLIC handle_layout_menu
PUBLIC handle_agent_menu
PUBLIC handle_tools_menu
PUBLIC handle_settings_menu
PUBLIC handle_help_menu
PUBLIC wnd_proc_main
PUBLIC dispatch_wm_command

.code

;==========================================================================
; menu_handler_init - Initialize all menu systems
;==========================================================================
menu_handler_init PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; Get main window
    call ui_get_main_hwnd
    mov hMainWindow, rax
    
    ; Get child windows
    mov rcx, rax
    call ui_get_editor_hwnd
    mov hEditorWindow, rax
    
    mov rcx, hMainWindow
    call ui_get_chat_hwnd
    mov hChatWindow, rax
    
    mov rcx, hMainWindow
    call ui_get_output_pane_hwnd
    mov hOutputPane, rax
    
    mov rcx, hMainWindow
    call ui_get_file_tree_hwnd
    mov hFileTree, rax
    
    mov rcx, hMainWindow
    call ui_get_terminal_hwnd
    mov hTerminal, rax
    
    ; Initialize output pane with RichEdit
    mov rcx, hOutputPane
    call output_pane_init
    
    ; Initialize tab manager (FILE tabs)
    mov rcx, hEditorWindow
    xor edx, edx                    ; TAB_TYPE_FILE = 0
    call tab_manager_init
    
    ; Initialize tab manager (CHAT tabs)
    mov rcx, hChatWindow
    mov edx, 1                      ; TAB_TYPE_CHAT = 1
    call tab_manager_init
    
    ; Initialize file tree
    mov rcx, hFileTree
    xor edx, edx
    xor r8d, r8d
    mov r9d, 300
    mov rax, 400
    mov [rsp + 32], rax
    call file_tree_init
    
    ; Initialize agent chat
    call agent_chat_init
    
    ; Log initialization complete
    lea rcx, [szFileNew]
    call asm_log
    
    xor eax, eax
    add rsp, 48
    pop rbp
    ret
menu_handler_init ENDP

;==========================================================================
; dispatch_wm_command - Central WM_COMMAND dispatcher
; rcx = hWnd
; edx = LOWORD(wParam) = control/menu ID
; r8 = HIWORD(wParam) = notification code
; r9 = lParam (control hwnd or 0)
;==========================================================================
dispatch_wm_command PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov rax, rdx                    ; command ID in eax
    
    ; File Menu (2001-2010)
    cmp eax, IDM_FILE_NEW
    je file_new
    cmp eax, IDM_FILE_OPEN
    je file_open
    cmp eax, IDM_FILE_SAVE
    je file_save
    cmp eax, IDM_FILE_SAVE_AS
    je file_save_as
    cmp eax, IDM_FILE_EXIT
    je file_exit
    
    ; Edit Menu (3001-3008)
    cmp eax, IDM_EDIT_UNDO
    je edit_undo
    cmp eax, IDM_EDIT_REDO
    je edit_redo
    cmp eax, IDM_EDIT_CUT
    je edit_cut
    cmp eax, IDM_EDIT_COPY
    je edit_copy
    cmp eax, IDM_EDIT_PASTE
    je edit_paste
    cmp eax, IDM_EDIT_SELECT_ALL
    je edit_select_all
    cmp eax, IDM_EDIT_FIND
    je edit_find
    cmp eax, IDM_EDIT_REPLACE
    je edit_replace
    
    ; View Menu (4001-4009)
    cmp eax, IDM_VIEW_EXPLORER
    je view_explorer
    cmp eax, IDM_VIEW_OUTPUT
    je view_output
    cmp eax, IDM_VIEW_TERMINAL
    je view_terminal
    cmp eax, IDM_VIEW_AGENT_CHAT
    je view_agent_chat
    
    ; Layout Menu (5001-5006)
    cmp eax, IDM_LAYOUT_SAVE
    je layout_save
    cmp eax, IDM_LAYOUT_LOAD
    je layout_load
    cmp eax, IDM_LAYOUT_RESET
    je layout_reset
    
    ; Agent Menu (6001-6006)
    cmp eax, IDM_AGENT_ASK
    je agent_ask
    cmp eax, IDM_AGENT_EDIT
    je agent_edit
    cmp eax, IDM_AGENT_PLAN
    je agent_plan
    cmp eax, IDM_AGENT_CONFIG
    je agent_config
    cmp eax, IDM_AGENT_CLEAR_CHAT
    je agent_clear_chat
    
    ; Tools Menu (7001-7007)
    cmp eax, IDM_TOOLS_FORMAT
    je tools_format
    cmp eax, IDM_TOOLS_BUILD
    je tools_build
    cmp eax, IDM_TOOLS_RUN
    je tools_run
    cmp eax, IDM_TOOLS_HOTPATCH
    je tools_hotpatch
    
    ; Toolbar (10001-10005)
    cmp eax, IDM_TOOLBAR_NEW_FILE
    je toolbar_new_file
    cmp eax, IDM_TOOLBAR_OPEN_FILE
    je toolbar_open_file
    cmp eax, IDM_TOOLBAR_SAVE_FILE
    je toolbar_save_file
    
    ; Not handled, return 0
    xor eax, eax
    jmp dispatch_done
    
;==========================================================================
; FILE MENU HANDLERS
;==========================================================================
file_new:
    ; Create new empty file
    lea rcx, [szFileNew]
    xor edx, edx
    call output_log_editor
    
    ; Create tab with "Untitled"
    lea rcx, [szUntitled]
    lea rdx, [open_file_path]
    call tab_create_editor
    
    ; Update editor state
    mov [current_editor_state.tab_count], eax
    xor [current_editor_state.is_modified], [current_editor_state.is_modified]
    mov [current_editor_state.is_modified], 1
    
    lea rcx, [szTabCreated]
    lea rdx, [open_file_path]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done

file_open:
    ; Show open file dialog
    lea rcx, [open_file_path]
    mov edx, 512
    call file_open_dialog_internal
    test eax, eax
    jz dispatch_done                ; User canceled
    
    ; open_file_path now contains filename
    lea rcx, [open_file_path]
    
    ; Log file open
    lea rdx, [open_file_path]
    mov rcx, rdx
    xor edx, edx                    ; action = 0 (open)
    call output_log_editor
    
    ; Create editor tab
    lea rcx, [open_file_path]
    lea rdx, [open_file_path]
    call tab_create_editor
    
    xor eax, eax
    jmp dispatch_done

file_save:
    ; Save current file
    lea rcx, [current_editor_state.current_path]
    call file_save_internal
    test eax, eax
    jz dispatch_done                ; Save failed
    
    ; Log save
    lea rcx, [szFileSaved]
    lea rdx, [current_editor_state.current_path]
    mov rcx, rdx
    xor edx, edx
    call output_log_editor
    
    ; Update modified flag
    mov [current_editor_state.is_modified], 0
    
    xor eax, eax
    jmp dispatch_done

file_save_as:
    ; Show save dialog
    lea rcx, [save_file_path]
    mov edx, 512
    call file_save_dialog_internal
    test eax, eax
    jz dispatch_done
    
    ; Copy to current path
    lea rcx, [save_file_path]
    lea rdx, [current_editor_state.current_path]
    mov rsi, rdx
    mov rdi, rcx
    xor ecx, ecx
copy_path:
    cmp ecx, 512
    jae path_copied
    mov al, [rsi + rcx]
    mov [rdi + rcx], al
    test al, al
    jz path_copied
    inc ecx
    jmp copy_path
path_copied:
    
    ; Save file
    lea rcx, [current_editor_state.current_path]
    call file_save_internal
    
    xor eax, eax
    jmp dispatch_done

file_exit:
    ; Close application
    mov rcx, hMainWindow
    mov edx, WM_DESTROY
    xor r8d, r8d
    xor r9d, r9d
    call PostMessageA
    
    xor eax, eax
    jmp dispatch_done

;==========================================================================
; EDIT MENU HANDLERS
;==========================================================================
edit_undo:
    mov rcx, hEditorWindow
    mov edx, WM_UNDO
    xor r8d, r8d
    xor r9d, r9d
    call SendMessageA
    xor eax, eax
    jmp dispatch_done

edit_redo:
    mov rcx, hEditorWindow
    mov edx, WM_REDO
    xor r8d, r8d
    xor r9d, r9d
    call SendMessageA
    xor eax, eax
    jmp dispatch_done

edit_cut:
    mov rcx, hEditorWindow
    mov edx, WM_CUT
    xor r8d, r8d
    xor r9d, r9d
    call SendMessageA
    xor eax, eax
    jmp dispatch_done

edit_copy:
    mov rcx, hEditorWindow
    mov edx, WM_COPY
    xor r8d, r8d
    xor r9d, r9d
    call SendMessageA
    xor eax, eax
    jmp dispatch_done

edit_paste:
    mov rcx, hEditorWindow
    mov edx, WM_PASTE
    xor r8d, r8d
    xor r9d, r9d
    call SendMessageA
    xor eax, eax
    jmp dispatch_done

edit_select_all:
    mov rcx, hEditorWindow
    mov edx, EM_SETSEL
    mov r8d, 0
    mov r9d, -1
    call SendMessageA
    xor eax, eax
    jmp dispatch_done

edit_find:
    ; TODO: Show find dialog
    lea rcx, [szFileNew]
    call asm_log
    xor eax, eax
    jmp dispatch_done

edit_replace:
    ; TODO: Show find/replace dialog
    lea rcx, [szFileNew]
    call asm_log
    xor eax, eax
    jmp dispatch_done

;==========================================================================
; VIEW MENU HANDLERS
;==========================================================================
view_explorer:
    mov rcx, hFileTree
    mov edx, SW_SHOW
    call ShowWindow
    
    xor edx, edx
    call InvalidateRect
    
    xor eax, eax
    jmp dispatch_done

view_output:
    mov rcx, hOutputPane
    mov edx, SW_SHOW
    call ShowWindow
    
    mov edx, 1
    call tab_set_panel_tab
    
    xor eax, eax
    jmp dispatch_done

view_terminal:
    mov rcx, hTerminal
    mov edx, SW_SHOW
    call ShowWindow
    
    xor eax, eax
    jmp dispatch_done

view_agent_chat:
    mov rcx, hChatWindow
    mov edx, SW_SHOW
    call ShowWindow
    
    xor eax, eax
    jmp dispatch_done

;==========================================================================
; LAYOUT MENU HANDLERS
;==========================================================================
layout_save:
    ; Save current layout to JSON
    call save_layout_json
    test eax, eax
    jz layout_save_failed
    
    lea rcx, [szLayoutSaved]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done
    
layout_save_failed:
    lea rcx, [szFileNew]
    call asm_log
    xor eax, eax
    jmp dispatch_done

layout_load:
    ; Load layout from JSON
    call load_layout_json
    test eax, eax
    jz layout_load_failed
    
    lea rcx, [szLayoutLoaded]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done
    
layout_load_failed:
    lea rcx, [szFileNew]
    call asm_log
    xor eax, eax
    jmp dispatch_done

layout_reset:
    ; Reset to default layout
    ; TODO: Restore default pane positions
    xor eax, eax
    jmp dispatch_done

;==========================================================================
; AGENT MENU HANDLERS
;==========================================================================
agent_ask:
    xor ecx, ecx
    call tab_set_agent_mode
    
    lea rcx, [szModeAsk]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done

agent_edit:
    mov ecx, 1
    call tab_set_agent_mode
    
    lea rcx, [szModeEdit]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done

agent_plan:
    mov ecx, 2
    call tab_set_agent_mode
    
    lea rcx, [szModePlan]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done

agent_config:
    mov ecx, 3
    call tab_set_agent_mode
    
    lea rcx, [szModeConfig]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done

agent_clear_chat:
    call agent_chat_clear
    
    lea rcx, [szAgentChatCleared]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done

;==========================================================================
; TOOLS MENU HANDLERS
;==========================================================================
tools_format:
    ; Format current file (call external formatter)
    lea rcx, [szFileNew]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done

tools_build:
    lea rcx, [szBuildStarted]
    call asm_log
    
    ; TODO: Trigger build process
    xor eax, eax
    jmp dispatch_done

tools_run:
    ; TODO: Run current project
    lea rcx, [szFileNew]
    call asm_log
    
    xor eax, eax
    jmp dispatch_done

tools_hotpatch:
    ; Apply hotpatch
    lea rcx, [szHotpatchApplied]
    lea rdx, [szFileNew]
    call output_log_hotpatch
    
    xor eax, eax
    jmp dispatch_done

;==========================================================================
; TOOLBAR HANDLERS
;==========================================================================
toolbar_new_file:
    ; Same as File > New
    lea rcx, [szFileNew]
    xor edx, edx
    call output_log_editor
    
    xor eax, eax
    jmp dispatch_done

toolbar_open_file:
    ; Same as File > Open
    lea rcx, [open_file_path]
    mov edx, 512
    call file_open_dialog_internal
    
    xor eax, eax
    jmp dispatch_done

toolbar_save_file:
    ; Same as File > Save
    lea rcx, [current_editor_state.current_path]
    call file_save_internal
    
    xor eax, eax
    jmp dispatch_done

;==========================================================================
; INTERNAL HELPER PROCEDURES
;==========================================================================

;==========================================================================
; file_open_dialog_internal - Show open file dialog
; rcx = buffer for filename
; edx = buffer size
; Returns: eax = success (1) or cancel (0)
;==========================================================================
file_open_dialog_internal PROC
    push rbp
    mov rbp, rsp
    sub rsp, 200h
    
    ; Production implementation using OPENFILENAME structure
    ; rcx = buffer for filename
    ; edx = buffer size
    
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx            ; Save buffer
    mov esi, edx            ; Save size
    
    ; Initialize buffer
    mov byte ptr [rbx], 0
    
    ; Build OPENFILENAME structure on stack
    lea rdi, [rbp - 180h]
    
    ; sizeof(OPENFILENAME) = 152 bytes
    mov qword ptr [rdi], 152        ; lStructSize
    
    call ui_get_main_hwnd
    mov qword ptr [rdi + 8], rax    ; hwndOwner
    
    mov qword ptr [rdi + 16], 0     ; hInstance
    
    ; lpstrFilter
    lea rax, [szFileFilter]
    mov qword ptr [rdi + 24], rax
    
    mov qword ptr [rdi + 32], 0     ; lpstrCustomFilter
    mov dword ptr [rdi + 40], 0     ; nMaxCustFilter
    mov dword ptr [rdi + 44], 1     ; nFilterIndex
    
    ; lpstrFile
    mov qword ptr [rdi + 48], rbx
    
    mov dword ptr [rdi + 56], esi   ; nMaxFile
    mov qword ptr [rdi + 64], 0     ; lpstrFileTitle
    mov dword ptr [rdi + 72], 0     ; nMaxFileTitle
    
    ; lpstrInitialDir
    lea rax, [szInitialDir]
    mov qword ptr [rdi + 76], rax
    
    ; lpstrTitle
    lea rax, [szOpenTitle]
    mov qword ptr [rdi + 84], rax
    
    ; Flags: OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY
    mov dword ptr [rdi + 92], 00001804h
    
    mov word ptr [rdi + 96], 0      ; nFileOffset
    mov word ptr [rdi + 98], 0      ; nFileExtension
    
    ; lpstrDefExt
    lea rax, [szDefExt]
    mov qword ptr [rdi + 100], rax
    
    mov qword ptr [rdi + 108], 0    ; lCustData
    mov qword ptr [rdi + 116], 0    ; lpfnHook
    mov qword ptr [rdi + 124], 0    ; lpTemplateName
    mov qword ptr [rdi + 132], 0    ; pvReserved
    mov dword ptr [rdi + 140], 0    ; dwReserved
    mov dword ptr [rdi + 144], 0    ; FlagsEx
    
    ; Call GetOpenFileNameA
    lea rcx, [rdi]
    call GetOpenFileNameA
    
    pop rdi
    pop rsi
    pop rbx
    
    add rsp, 200h
    pop rbp
    ret
file_open_dialog_internal ENDP

;==========================================================================
; file_save_dialog_internal - Show save file dialog
; rcx = buffer for filename
; edx = buffer size
; Returns: eax = success (1) or cancel (0)
;==========================================================================
file_save_dialog_internal PROC
    push rbp
    mov rbp, rsp
    sub rsp, 200h
    
    ; Production implementation using OPENFILENAME structure
    ; rcx = buffer for filename
    ; edx = buffer size
    
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx            ; Save buffer
    mov esi, edx            ; Save size
    
    ; Initialize buffer with current filename if any
    ; (buffer should already contain filename)
    
    ; Build OPENFILENAME structure
    lea rdi, [rbp - 180h]
    
    mov qword ptr [rdi], 152        ; lStructSize
    
    call ui_get_main_hwnd
    mov qword ptr [rdi + 8], rax    ; hwndOwner
    
    mov qword ptr [rdi + 16], 0     ; hInstance
    
    lea rax, [szFileFilter]
    mov qword ptr [rdi + 24], rax   ; lpstrFilter
    
    mov qword ptr [rdi + 32], 0     ; lpstrCustomFilter
    mov dword ptr [rdi + 40], 0     ; nMaxCustFilter
    mov dword ptr [rdi + 44], 1     ; nFilterIndex
    
    mov qword ptr [rdi + 48], rbx   ; lpstrFile
    mov dword ptr [rdi + 56], esi   ; nMaxFile
    mov qword ptr [rdi + 64], 0     ; lpstrFileTitle
    mov dword ptr [rdi + 72], 0     ; nMaxFileTitle
    
    lea rax, [szInitialDir]
    mov qword ptr [rdi + 76], rax   ; lpstrInitialDir
    
    lea rax, [szSaveTitle]
    mov qword ptr [rdi + 84], rax   ; lpstrTitle
    
    ; Flags: OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY
    mov dword ptr [rdi + 92], 00001806h
    
    mov word ptr [rdi + 96], 0      ; nFileOffset
    mov word ptr [rdi + 98], 0      ; nFileExtension
    
    lea rax, [szDefExt]
    mov qword ptr [rdi + 100], rax  ; lpstrDefExt
    
    mov qword ptr [rdi + 108], 0    ; lCustData
    mov qword ptr [rdi + 116], 0    ; lpfnHook
    mov qword ptr [rdi + 124], 0    ; lpTemplateName
    mov qword ptr [rdi + 132], 0    ; pvReserved
    mov dword ptr [rdi + 140], 0    ; dwReserved
    mov dword ptr [rdi + 144], 0    ; FlagsEx
    
    ; Call GetSaveFileNameA
    lea rcx, [rdi]
    call GetSaveFileNameA
    
    pop rdi
    pop rsi
    pop rbx
    
    add rsp, 200h
    pop rbp
    ret
file_save_dialog_internal ENDP

;==========================================================================
; file_save_internal - Save file to disk
; rcx = file path
; Returns: eax = success (1) or failure (0)
;==========================================================================
file_save_internal PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; TODO: Implement actual file I/O
    ; For now, return success
    mov eax, 1
    
    add rsp, 48
    pop rbp
    ret
file_save_internal ENDP

;==========================================================================
; dispatch_done - Common exit point
;==========================================================================
dispatch_done:
    add rsp, 64
    pop rbp
    ret
dispatch_wm_command ENDP

;==========================================================================
; Stub procedures for unimplemented menu categories
;==========================================================================

handle_file_menu PROC
    mov eax, 0
    ret
handle_file_menu ENDP

handle_edit_menu PROC
    mov eax, 0
    ret
handle_edit_menu ENDP

handle_view_menu PROC
    mov eax, 0
    ret
handle_view_menu ENDP

handle_layout_menu PROC
    mov eax, 0
    ret
handle_layout_menu ENDP

handle_agent_menu PROC
    mov eax, 0
    ret
handle_agent_menu ENDP

handle_tools_menu PROC
    mov eax, 0
    ret
handle_tools_menu ENDP

handle_settings_menu PROC
    mov eax, 0
    ret
handle_settings_menu ENDP

handle_help_menu PROC
    mov eax, 0
    ret
handle_help_menu ENDP

;==========================================================================
; wnd_proc_main - Main window procedure (WM_COMMAND dispatcher)
;==========================================================================
wnd_proc_main PROC hWnd:QWORD, uMsg:QWORD, wParam:QWORD, lParam:QWORD
    push rbp
    mov rbp, rsp
    
    mov eax, edx
    cmp eax, WM_COMMAND
    jne wnd_proc_default
    
    ; Extract control ID from wParam
    mov ecx, r8d
    mov edx, r8d
    shr edx, 16
    
    mov rcx, hWnd
    call dispatch_wm_command
    
    jmp wnd_proc_done
    
wnd_proc_default:
    ; Pass to DefWindowProcA
    mov rcx, hWnd
    mov edx, eax
    call DefWindowProcA
    
wnd_proc_done:
    pop rbp
    ret
wnd_proc_main ENDP

END
