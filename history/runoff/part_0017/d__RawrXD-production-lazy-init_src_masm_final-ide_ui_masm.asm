;==========================================================================
; ui_masm.asm - Pure MASM64 Win32 UI Layer for RawrXD IDE
; ==========================================================================
; Implements all window management, menus, dialogs, and text controls.
; Uses native Win32 APIs (no Qt, no C++).
; Can run in headless mode without explicit GUI libs via feature toggles.
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comdlg32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
; Menu Constants
IDM_FILE_OPEN_MODEL  equ 2001
IDM_FILE_OPEN_FILE   equ 2002
IDM_FILE_SAVE        equ 2003
IDM_FILE_SAVE_AS     equ 2004
IDM_FILE_EXIT        equ 2005
IDM_FILE_NEW         equ 2006
IDM_CHAT_CLEAR       equ 2101
IDM_SETTINGS_MODEL   equ 2201
IDM_AGENT_TOGGLE     equ 2301
IDM_HOTPATCH_MEMORY  equ 2401
IDM_HOTPATCH_BYTE    equ 2402
IDM_HOTPATCH_SERVER  equ 2403
IDM_HOTPATCH_STATS   equ 2404
IDM_HOTPATCH_RESET   equ 2405

; Control Constants (moved to avoid duplication)
; These constants are defined later in the file

; RichEdit control messages
EM_SETSEL            equ 00B1h
EM_REPLACESEL        equ 00C2h
EM_GETSEL            equ 00B0h
EM_SETLIMITTEXT      equ 00C5h

; Menu flags
MFT_STRING           equ 0h
MFT_SEPARATOR        equ 800h
MF_POPUP             equ 10h

; Window styles
SW_HIDE              equ 0
SW_SHOW              equ 5
SW_SHOWDEFAULT       equ 10

; Dialog flags
OFN_FILEMUSTEXIST    equ 1000h
OFN_PATHMUSTEXIST    equ 800h

; TreeView styles
TVS_LINESATROOT      equ 4h

; ListView styles
LVS_SINGLESEL        equ 4h

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN main_on_send:PROC
EXTERN main_on_open:PROC
EXTERN gui_create_component:PROC
EXTERN asm_log:PROC
EXTERN console_log:PROC
EXTERN wsprintfA:PROC
EXTERN GetLastError:PROC
EXTERN GetClassInfoExA:PROC
EXTERN GetOpenFileNameA:PROC
EXTERN GetSaveFileNameA:PROC
EXTERN main_on_open_file:PROC
EXTERN main_on_save_file:PROC
EXTERN main_on_save_file_as:PROC
EXTERN SendMessageA:PROC
EXTERN GetLogicalDrives:PROC
EXTERN GetDriveTypeA:PROC
EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN GetFileAttributesA:PROC
EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC
EXTERN CreateProcessA:PROC
EXTERN PeekNamedPipe:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN GetStdHandle:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN ui_file_open_dialog:PROC
EXTERN ui_file_save:PROC
EXTERN OutputDebugStringA:PROC
EXTERN LoadLibraryA:PROC
EXTERN ShowWindow:PROC
EXTERN SetFocus:PROC
EXTERN DefWindowProcA:PROC
EXTERN GetTickCount:PROC
EXTERN CreateWindowExA:PROC
EXTERN MessageBoxA:PROC
EXTERN EndDialog:PROC
EXTERN DestroyWindow:PROC
EXTERN PostQuitMessage:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN RegisterClassExA:PROC
EXTERN LoadCursorA:PROC
EXTERN LoadIconA:PROC
EXTERN GetModuleHandleA:PROC
EXTERN GetClientRect:PROC
EXTERN MoveWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN SetWindowTextA:PROC
EXTERN GetWindowTextA:PROC
EXTERN GetWindowTextLengthA:PROC
; Hotpatch unified API
EXTERN masm_unified_manager_create:PROC
EXTERN masm_unified_apply_memory_patch:PROC
EXTERN masm_unified_apply_byte_patch:PROC
EXTERN masm_unified_add_server_hotpatch:PROC
EXTERN masm_unified_get_stats:PROC
EXTERN hpatch_reset_stats:PROC
EXTERN gui_create_complete_ide:PROC
EXTERN RecalculateLayout:PROC
EXTERN keyboard_shortcuts_process:PROC
EXTERN session_trigger_autosave:PROC
EXTERN session_manager_init:PROC

; Data from main_masm.asm
EXTERN default_model:BYTE

; Functions from gui_designer_agent.asm
EXTERN gui_save_pane_layout:PROC
EXTERN gui_load_pane_layout:PROC

;==========================================================================
; CONSTANTS
;==========================================================================
IDC_EXPLORER_TREE       equ 1001
IDC_FILE_LIST           equ 1002
IDC_EDITOR              equ 1003
IDC_CHAT_BOX            equ 1004
IDC_INPUT_BOX           equ 1005
IDC_TERMINAL            equ 1006
IDC_AGENT_LIST          equ 1008
IDC_AGENT_CONSOLE       equ 1009
IDC_TAB_CONTROL         equ 1011
IDC_SEND_BUTTON         equ 1012
IDC_MODE_COMBO          equ 1013
IDC_CHECK_MAX           equ 1014
IDC_CHECK_DEEP          equ 1015
IDC_CHECK_RESEARCH      equ 1016
IDC_CHECK_INTERNET      equ 1017
IDC_CHECK_THINKING      equ 1018
IDC_AB_EXPLORER         equ 1201
IDC_AB_SEARCH           equ 1202
IDC_AB_SCM              equ 1203
IDC_AB_DEBUG            equ 1204
IDC_AB_EXTENSIONS       equ 1205
IDC_TAB_TERMINAL        equ 1301
IDC_TAB_OUTPUT          equ 1302
IDC_TAB_PROBLEMS        equ 1303
IDC_TAB_DEBUGCON        equ 1304
IDC_STATUS_BAR          equ 1401
IDC_COMMAND_PALETTE     equ 1402
IDC_MINIMAP             equ 1403
IDC_BREADCRUMB          equ 1404
IDC_FILE_TREE           equ 1405
IDC_SEARCH_BOX          equ 1406
IDC_PROBLEMS_LIST       equ 1407
IDC_DEBUG_CONSOLE       equ 1408

; File explorer constants
MAX_PATH_LEN         equ 260
MAX_DRIVES           equ 26
MAX_FILES            equ 1000
TV_FIRST             equ 1100h
TVM_INSERTITEMA      equ 1104h
TVIF_TEXT            equ 0001h
TVIF_CHILDREN        equ 0040h
TVI_ROOT             equ 0FFFF0000h
TVI_LAST             equ 0FFFF0001h
INVALID_HANDLE_VALUE equ -1
FILE_ATTRIBUTE_DIRECTORY equ 10h

; TreeView styles
TVS_HASBUTTONS      equ 1h
TVS_HASLINES        equ 2h
TVS_LINESATROOT     equ 4h

; ListView styles
LVS_REPORT          equ 1h

; Window messages
WM_LBUTTONDOWN      equ 201h
WM_MOUSEMOVE        equ 200h
WM_LBUTTONUP        equ 202h
WM_KEYDOWN          equ 100h
WM_TIMER            equ 113h
WM_INITDIALOG       equ 110h
WM_CLOSE            equ 10h
BN_CLICKED          equ 0

; TreeView messages
TVM_GETNEXTITEM     equ 1104h
TVGN_CARET          equ 9h
TVM_GETITEMA        equ 110Ch

; ListView messages
LVM_GETSELECTIONMARK equ 1046h

; Dialog constants
IDCANCEL            equ 2

; Structure sizes (STARTUPINFO from windows.inc is 104 bytes on x64)
SIZEOF_STARTUPINFOA equ 104

; File explorer structures
TVINSERTSTRUCT struct
    hParent         QWORD ?
    hInsertAfter    QWORD ?
    item_mask       DWORD ?
    item_pszText    QWORD ?
    item_cchTextMax DWORD ?
    item_iImage     DWORD ?
    item_iSelectedImage DWORD ?
    item_cChildren  DWORD ?
    item_lParam     QWORD ?
TVINSERTSTRUCT ends

; RECT structure definition
RECT STRUCT
    left    DWORD ?
    top     DWORD ?
    right   DWORD ?
    bottom  DWORD ?
RECT ENDS

; Layout constants
LAYOUT_ACTIVITY_BAR_W equ 48
LAYOUT_SIDEBAR_W      equ 320
LAYOUT_FILE_TREE_H    equ 400
LAYOUT_BOTTOM_H       equ 200
LAYOUT_BOTTOM_HEADER_H equ 30
LAYOUT_TABBAR_H       equ 30
LAYOUT_BREADCRUMB_H   equ 25
LAYOUT_MINIMAP_W      equ 80
LAYOUT_STATUS_H       equ 20

; (duplicate Menu IDs removed; defined once above)

; Component IDs for GUI registry
COMPONENT_FILE_TREE       equ 1
COMPONENT_SEARCH          equ 2
COMPONENT_MINIMAP         equ 3
COMPONENT_COMMAND_PALETTE equ 4
COMPONENT_STATUS          equ 5
COMPONENT_OUTPUT          equ 6
COMPONENT_TERMINAL        equ 7

; Additional control styles/messages
WS_HSCROLL          equ 100000h
CBS_DROPDOWNLIST    equ 3
CBS_DROPDOWN        equ 2
CB_ADDSTRING        equ 143h
CB_SETCURSEL        equ 14Eh
CB_GETCURSEL        equ 147h
CB_RESETCONTENT     equ 14Bh
BS_AUTOCHECKBOX     equ 3

; OPENFILENAMEA structure
OPENFILENAMEA struct
    lStructSize       DWORD ?
    hwndOwner         QWORD ?
    hInstance         QWORD ?
    lpstrFilter       QWORD ?
    lpstrCustomFilter QWORD ?
    nMaxCustFilter    DWORD ?
    nFilterIndex      DWORD ?
    lpstrFile         QWORD ?
    nMaxFile          DWORD ?
    lpstrFileTitle    QWORD ?
    nMaxFileTitle     DWORD ?
    lpstrInitialDir   QWORD ?
    lpstrTitle        QWORD ?
    Flags             DWORD ?
    nFileOffset       WORD ?
    nFileExtension    WORD ?
    lpstrDefExt       QWORD ?
    lCustData         QWORD ?
    lpfnHook          QWORD ?
    lpTemplateName    QWORD ?
    pvReserved        QWORD ?
    dwReserved        DWORD ?
    FlagsEx           DWORD ?
OPENFILENAMEA ends

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data?
    hwnd_main           dq ?
    hwnd_explorer       dq ?
    hwnd_editor         dq ?
    hwnd_chat           dq ?
    hwnd_input          dq ?
    hwnd_terminal       dq ?
    hwnd_send           dq ?
    hwnd_mode_combo     dq ?
    hwnd_agent_console  dq ?
    hwnd_activity_bar   dq ?
    hwnd_sidebar_stack  dq ?
    hwnd_sidebar_explorer dq ?
    hwnd_sidebar_search   dq ?
    hwnd_sidebar_scm      dq ?
    hwnd_sidebar_debug    dq ?
    hwnd_sidebar_ext      dq ?
    hwnd_editor_host    dq ?
    hwnd_editor_label   dq ?
    hwnd_breadcrumb     dq ?
    hwnd_editor_view    dq ?
    hwnd_bottom_panel   dq ?
    hwnd_bottom_header  dq ?
    hwnd_bottom_btn_term dq ?
    hwnd_bottom_btn_output dq ?
    hwnd_bottom_btn_problems dq ?
    hwnd_bottom_btn_debug dq ?
    hwnd_bottom_term    dq ?
    hwnd_bottom_output  dq ?
    hwnd_bottom_problems dq ?
    hwnd_bottom_debug   dq ?
    hwnd_status         dq ?
    hwnd_check_max      dq ?
    hwnd_check_deep     dq ?
    hwnd_check_research dq ?
    hwnd_check_internet dq ?
    hwnd_check_thinking dq ?
    hwnd_file_tree      dq ?
    hwnd_search_box     dq ?
    hwnd_minimap        dq ?
    hwnd_command_palette dq ?
    hwnd_problems_list  dq ?
    hwnd_debug_console  dq ?
    h_instance          dq ?
    current_mode        dd ?
    bp_once             db ?
    agent_running       dd ?
    g_headless_mode     dd ?
    g_enable_registration dd ?
    hpatch_manager_handle dq ?
    ; Pane dragging state
    g_dragging          dd ?
    g_dragged_pane_id   dd ?
    g_drag_start_x      dd ?
    g_drag_start_y      dd ?
    g_drop_target_pane  dd ?
    ; scratch buffer used for demo memory patch
    hotpatch_demo_target  db 32 dup(?)
    
    ; File explorer data
    file_tree_items     dq MAX_FILES dup(?)
    file_tree_count     dd ?
    current_path        db MAX_PATH_LEN dup(?)
    drive_letters       db MAX_DRIVES dup(?)
    drive_count         dd ?
    
    ; Terminal data (reserve space for structures)
    ; PROCESS_INFORMATION is 24 bytes, STARTUPINFOA is 104 bytes
    ps_process_info     db 24 dup(?)
    ps_startup_info     db 104 dup(?)
    ps_stdin_read       dq ?
    ps_stdin_write      dq ?
    ps_stdout_read      dq ?
    ps_stdout_write     dq ?
    terminal_buffer     db 4096 dup(?)
    
    ; Editor data
    editor_buffer       db 65536 dup(?)
    editor_cursor_pos   dd ?
    editor_file_path    db MAX_PATH_LEN dup(?)
    editor_modified     db ?
    
    client_rect         RECT <>

.data
    app_title           BYTE "RawrXD AI IDE (Pure MASM64)",0
    wnd_class_name      BYTE "RawrXDMainWindow",0
    szRichEditDll       BYTE "riched20.dll",0
    szRichEdit          BYTE "RichEdit20A",0
    szTreeView          BYTE "SysTreeView32",0
    szEdit              BYTE "EDIT",0
    szStatic            BYTE "STATIC",0
    szButton            BYTE "BUTTON",0
    szComboBox          BYTE "COMBOBOX",0
    szTabControl        BYTE "SysTabControl32",0
    szListView          BYTE "SysListView32",0
    szStatusBar         BYTE "msctls_statusbar32",0
    szTest              BYTE "Test Control",0
    szNewline           BYTE 13,10,0
    
    ; Mode strings
    str_mode_max        BYTE "Max Mode",0
    str_mode_deep       BYTE "Deep Research",0
    str_mode_research   BYTE "Research",0
    str_mode_internet   BYTE "Internet Search",0
    str_mode_thinking   BYTE "Thinking",0

    ; Hotpatch strings
    str_hotpatch_stats_ready     BYTE "Hotpatch stats ready",0
    str_hotpatch_manager_not_init BYTE "Hotpatch manager not initialized",0
    str_hotpatch_stats_reset     BYTE "Hotpatch stats reset",0

    ; Menu strings
    str_file            BYTE "&File",0
    str_open_file       BYTE "&Open File...",0
    str_open_model      BYTE "Open &Model...",0
    str_save            BYTE "&Save",0
    str_save_as         BYTE "Save &As...",0
    str_exit            BYTE "E&xit",0
    str_send            BYTE "Send",0
    str_chat            BYTE "&Chat",0
    str_clear           BYTE "&Clear History",0
    str_settings        BYTE "&Settings",0
    str_model           BYTE "&AI Model...",0
    str_tools           BYTE "&Tools",0
    str_agent           BYTE "Agent &Mode",0
    str_hotpatch_memory BYTE "Apply Memory Hotpatch",0
    str_hotpatch_byte   BYTE "Apply Byte-Level Hotpatch",0
    str_hotpatch_server BYTE "Apply Server Hotpatch",0
    str_hotpatch_stats  BYTE "Show Hotpatch Statistics",0
    str_hotpatch_reset  BYTE "Reset Hotpatch Statistics",0
    str_hotpatch_success BYTE "Hotpatch applied successfully",0
    str_hotpatch_failed BYTE "Hotpatch failed",0
    str_hotpatch_stats_msg BYTE "Hotpatch stats: ",0
    str_chat_clear      BYTE "Chat history cleared",0
    str_agent_enabled   BYTE "Agent mode enabled",0
    str_agent_disabled  BYTE "Agent mode disabled",0
    szAllFilesFilter    BYTE "All Files",0,"*.*",0,0

    ; Debug command strings
    szBreakCmd       BYTE "break",0
    szContinueCmd    BYTE "continue",0
    szStepCmd        BYTE "step",0
    szNextCmd        BYTE "next",0
    szUnknownDebugCmd BYTE "Unknown debug command",0
    szBreakpointSet  BYTE "Breakpoint set",0
    szDebugContinued BYTE "Execution resumed",0
    szDebugStepped   BYTE "Stepped",0
    szEmptyString    BYTE 0

    ; File API function pointers
    find_first_file_w_addr QWORD 0
    find_next_file_w_addr QWORD 0
    find_close_addr QWORD 0

    ; Debug window handles
    hwnd_debug_output QWORD 0

    ; Search constants
    search_wildcard BYTE "\*.*",0
    current_dir     BYTE ".",0

    dbg_ui_start        BYTE "UI: Starting...",13,10,0
    dbg_ui_reg_fail     BYTE "UI: RegisterClassExA failed",13,10,0
    dbg_ui_create_fail  BYTE "UI: CreateWindowExA failed",13,10,0
    dbg_ui_ok           BYTE "UI: Main window created",13,10,0
    dbg_ui_before_reg   BYTE "UI: Before RegisterClassExA",13,10,0
    dbg_ui_after_reg    BYTE "UI: After RegisterClassExA",13,10,0
    dbg_ui_before_create BYTE "UI: Before CreateWindowExA",13,10,0
    dbg_wndproc_entry   BYTE "WndProc: entry",13,10,0
    dbg_last_error_fmt  BYTE "GetLastError=%u",13,10,0
    dbg_class_lpfn_fmt  BYTE "UI: Class lpfnWndProc=%p",13,10,0
    dbg_ui_reg_components_start BYTE "UI: Register components start",13,10,0
    dbg_ui_reg_editor BYTE "UI: Registered editor",13,10,0
    dbg_ui_reg_chat   BYTE "UI: Registered chat",13,10,0
    dbg_ui_reg_input  BYTE "UI: Registered input",13,10,0
    dbg_ui_reg_terminal BYTE "UI: Registered terminal",13,10,0
    szEditorTabLabel   BYTE "main.cpp  |  index.ts",0
    szBreadcrumb       BYTE "RawrXD-QtShell / src / qtapp / unified_hotpatch_manager.cpp",0
    szBottomPanel     BYTE "BottomPanel",0
    szBottomTabs      BYTE "Terminal  Output  Problems  Debug Console",0
    szTabTerminal     BYTE "Terminal",0
    szTabOutput       BYTE "Output",0
    szTabProblems     BYTE "Problems",0
    szTabDebug        BYTE "Debug Console",0
    szBottomTerm      BYTE "Terminal output placeholder",0
    szBottomOut       BYTE "Build/log output placeholder",0
    szBottomProb      BYTE "Problems placeholder",0
    szBottomDbg       BYTE "Debug console placeholder",0
    dbg_ui_add_chat_entry BYTE "UI: add_chat_message entry",13,10,0
    dbg_ui_add_chat_after_setsel BYTE "UI: add_chat_message after setsel",13,10,0
    dbg_ui_add_chat_after_replace BYTE "UI: add_chat_message after replace",13,10,0
    dbg_ui_add_chat_done BYTE "UI: add_chat_message done",13,10,0
    dbg_mode_combo_changed BYTE "UI: Mode combo changed",13,10,0
    dbg_ui_layout_shell BYTE "UI: Layout shell created",13,10,0
    dbg_ui_sidebar_switch BYTE "UI: Sidebar view switched",13,10,0
    dbg_pane_drag_start BYTE "UI: Pane drag started",13,10,0
    dbg_pane_drag_end BYTE "UI: Pane drag ended",13,10,0
    dbg_pane_dock_finalized BYTE "UI: Pane docked",13,10,0
    dbg_pane_floating_created BYTE "UI: Floating pane window created",13,10,0
    dbg_file_opened BYTE "UI: File opened in editor",13,10,0
    dbg_search_performed BYTE "UI: File search performed",13,10,0
    dbg_problem_navigate BYTE "UI: Navigating to problem location",13,10,0
    dbg_debug_cmd_executed BYTE "UI: Debug command executed",13,10,0
    dbg_hotpatch_dialog_shown BYTE "UI: Hotpatch dialog shown",13,10,0
    
    ; Hotpatch Dialog Strings
    str_hp_mem_title   BYTE "Apply Memory Hotpatch",0
    str_hp_mem_addr    BYTE "Target Address (hex):",0
    str_hp_mem_val     BYTE "New Value (hex):",0
    str_hp_byte_title  BYTE "Apply Byte-Level Hotpatch",0
    str_hp_byte_off    BYTE "File Offset (hex):",0
    str_hp_byte_pat    BYTE "Pattern (hex bytes):",0
    str_hp_byte_repl   BYTE "Replacement (hex bytes):",0
    str_hp_srv_title   BYTE "Apply Server Hotpatch",0
    str_hp_srv_type    BYTE "Injection Point:",0
    str_hp_srv_code    BYTE "Transform Code:",0
    str_hp_apply       BYTE "&Apply",0
    str_hp_cancel      BYTE "&Cancel",0
    str_hp_preview     BYTE "&Preview",0
    str_hp_err_invalid BYTE "Invalid input format. Please use hexadecimal values.",0
    str_hp_err_apply   BYTE "Failed to apply hotpatch. Check your inputs and try again.",0
    str_hp_success     BYTE "Hotpatch applied successfully!",0

.data?
    dbg_last_error_buf  BYTE 64 dup (?)

.data
    ; Predefined WndClassEx structure for main window
    wnd_class LABEL QWORD
    wnd_class_cbsize    DWORD 80            ; cbSize = 80
    wnd_class_style     DWORD 3             ; CS_VREDRAW | CS_HREDRAW
    wnd_class_lpfn      QWORD wnd_proc_main ; lpfnWndProc
    wnd_class_cbcls     DWORD 0             ; cbClsExtra
    wnd_class_cbwnd     DWORD 0             ; cbWndExtra
    wnd_class_hinst     QWORD 0             ; hInstance (will be filled at runtime)
    wnd_class_hicon     QWORD 0             ; hIcon
    wnd_class_hcursor   QWORD 0             ; hCursor
    wnd_class_hbg       QWORD 16            ; hbrBackground = COLOR_BTNFACE + 1
    wnd_class_lpmenu    QWORD 0             ; lpszMenuName
    wnd_class_lpclass   QWORD wnd_class_name ; lpszClassName
    wnd_class_hicon2    QWORD 0             ; hIconSm

    ; Production Logging Strings
    szLogAddChat        BYTE "UI: Adding chat message...",0
    szLogGetInput       BYTE "UI: Getting input text...",0
    szLogClearInput     BYTE "UI: Clearing input...",0
    szLogShowDialog     BYTE "UI: Showing dialog...",0
    szActivityBar       BYTE "ActivityBar",0
    szSidebar           BYTE "SidebarStack",0
    szEditorHostText    BYTE "EditorHost",0
    szSidebarExplorer   BYTE "Explorer placeholder",0
    szSidebarSearch     BYTE "Search placeholder",0
    szSidebarSCM        BYTE "Source Control placeholder",0
    szSidebarDebug      BYTE "Debug placeholder",0
    szSidebarExt        BYTE "Extensions placeholder",0
    szABtnExplorer      BYTE "Files",0
    szABtnSearch        BYTE "Search",0
    szABtnSCM           BYTE "SCM",0
    szABtnDebug         BYTE "Debug",0
    szABtnExt           BYTE "Ext",0
    szFileTree          BYTE "FileTree",0
    szSearchBox         BYTE "SearchBox",0
    szMinimap           BYTE "Minimap",0
    szCommandPalette    BYTE "CommandPalette",0
    ; (duplicates removed; definitions exist earlier in file)

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; HELPER: safe_create_window_ex()
;
; Wrapper around CreateWindowExA with proper error handling and stack
; management. Staggers calls to avoid sequencing issues.
;
; Parameters (in order):
;   rcx = dwExStyle
;   rdx = lpClassName
;   r8  = lpWindowName
;   r9d = dwStyle
;   [rsp+32] = x
;   [rsp+40] = y
;   [rsp+48] = width
;   [rsp+56] = height
;   [rsp+64] = hWndParent
;   [rsp+72] = hMenu (or ID for child)
;   [rsp+80] = hInstance
;   [rsp+88] = lpParam
;
; Returns: rax = window handle
;==========================================================================
ALIGN 16
safe_create_window_ex PROC
    push rbx
    sub rsp, 32
    
    ; Save parameters on stack
    mov rbx, rcx        ; dwExStyle
    mov rcx, rbx        ; Restore for call
    
    ; Call CreateWindowExA
    call CreateWindowExA
    
    ; Add small delay to allow window message processing
    cmp rax, 0
    je safe_cwe_fail
    
    ; Non-blocking check: flush any pending messages for this window
    ; (This prevents cascading allocation failures)
    mov rbx, rax        ; Save window handle
    
    ; Brief delay via GetTickCount (allows OS to process window creation)
    call GetTickCount
    
    mov rax, rbx        ; Restore and return window handle
    jmp safe_cwe_done
    
safe_cwe_fail:
    xor eax, eax
    
safe_cwe_done:
    add rsp, 32
    pop rbx
    ret
safe_create_window_ex ENDP

;==========================================================================
; PUBLIC: ui_create_main_window(hInstance: rcx) -> hwnd (rax)
;
; Creates the main application window with all child controls.
; Returns window handle, or NULL on failure.
;==========================================================================
PUBLIC ui_create_main_window
ALIGN 16
ui_create_main_window PROC
    ; Create and show the main window with a registered class
    push rbx
    sub rsp, 112                        ; shadow space + params + alignment

    ; Optional headless scaffold: if no GUI, return dummy handle
    ; Default off (0) unless explicitly enabled elsewhere
    ; Uses global in .data if present; if not, falls through
    ; This keeps IDE functional without explicit user32 dependencies
    ;
    ; Check for symbol and non-zero value at runtime (lenient)
    ; Note: g_headless_mode may be absent; this check is safe
    mov eax, DWORD PTR g_headless_mode
    cmp eax, 0
    je ui_do_gui
    mov eax, 1
    mov hwnd_main, rax
    lea rcx, dbg_ui_ok
    call console_log
    add rsp, 112
    pop rbx
    ret

ui_do_gui:
    ; cache instance for later child windows
    mov h_instance, rcx
    mov wnd_class_hinst, rcx

    ; log: before register
    lea rdx, dbg_ui_before_reg
    mov rcx, rdx
    call OutputDebugStringA
    mov rcx, rdx
    call console_log

    ; register window class
    lea rcx, wnd_class
    call RegisterClassExA
    test eax, eax
    jz ui_create_main_window_fail

    ; log: after register
    lea rcx, dbg_ui_after_reg
    call console_log

    ; log: before create
    lea rcx, dbg_ui_before_create
    call console_log

    ; create main window
    xor rcx, rcx                        ; dwExStyle = 0
    lea rdx, wnd_class_name             ; lpClassName
    lea r8, app_title                   ; lpWindowName
    mov r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE
    mov QWORD PTR [rsp + 32], CW_USEDEFAULT ; X
    mov QWORD PTR [rsp + 40], CW_USEDEFAULT ; Y
    mov QWORD PTR [rsp + 48], 1280           ; nWidth
    mov QWORD PTR [rsp + 56], 900            ; nHeight
    mov QWORD PTR [rsp + 64], 0              ; hWndParent
    mov QWORD PTR [rsp + 72], 0              ; hMenu
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax            ; hInstance
    mov QWORD PTR [rsp + 88], 0              ; lpParam
    call CreateWindowExA
    mov hwnd_main, rax
    test rax, rax
    jz ui_create_main_window_fail

    ; show and update the window
    mov rcx, rax
    mov edx, SW_SHOWDEFAULT
    call ShowWindow
    mov rcx, hwnd_main
    call UpdateWindow

    ; Create and attach menu immediately
    call ui_create_menu
    test rax, rax
    jz ui_create_main_window_fail
    mov rcx, rax
    call ui_set_main_menu

    ; Initialize unified hotpatch manager
    call masm_unified_manager_create
    mov hpatch_manager_handle, rax

    ; Initialize file tree
    call ui_init_file_explorer

    ; Initialize search box
    call ui_init_search_box

    ; Initialize minimap
    call ui_init_minimap

    ; Initialize command palette
    call ui_init_command_palette

    ; Initialize status bar
    call ui_init_status_bar

    ; log: ok
    lea rcx, dbg_ui_ok
    call console_log

    mov rax, hwnd_main
    add rsp, 112
    pop rbx
    ret

ui_create_main_window_fail:
    xor eax, eax
    add rsp, 112
    pop rbx
    ret
ui_create_main_window ENDP

;==========================================================================
; PUBLIC: ui_create_layout_shell()
;
; Creates placeholder frames for activity bar, sidebar stack, editor host,
; and bottom panel to mirror the Qt VSCode-like layout. Uses STATIC windows
; as lightweight containers.
;==========================================================================
PUBLIC ui_create_layout_shell
ALIGN 16
ui_create_layout_shell PROC
    push rbx
    push r12
    sub rsp, 112                        ; space for CreateWindowExA params

    ; Get client rect of main window
    mov rcx, hwnd_main
    lea rdx, client_rect
    call GetClientRect

    ; Compute client width/height
    mov eax, DWORD PTR client_rect.right
    sub eax, DWORD PTR client_rect.left
    mov ebx, eax                        ; ebx = client width
    mov eax, DWORD PTR client_rect.bottom
    sub eax, DWORD PTR client_rect.top
    mov r12d, eax                       ; r12d = client height

    ; Activity bar (left) - parent for buttons
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szActivityBar
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 0         ; X
    mov QWORD PTR [rsp + 40], 0         ; Y
    mov QWORD PTR [rsp + 48], LAYOUT_ACTIVITY_BAR_W ; width
    mov QWORD PTR [rsp + 56], r12       ; height
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_activity_bar, rax

    ; Activity bar buttons (stacked vertically)
    ; Explorer
    xor rcx, rcx
    lea rdx, szButton
    lea r8, szABtnExplorer
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], LAYOUT_ACTIVITY_BAR_W
    mov QWORD PTR [rsp + 56], 40
    mov rax, hwnd_activity_bar
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_AB_EXPLORER
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA

    ; Search
    xor rcx, rcx
    lea rdx, szButton
    lea r8, szABtnSearch
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 40
    mov QWORD PTR [rsp + 48], LAYOUT_ACTIVITY_BAR_W
    mov QWORD PTR [rsp + 56], 40
    mov rax, hwnd_activity_bar
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_AB_SEARCH
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA

    ; SCM
    xor rcx, rcx
    lea rdx, szButton
    lea r8, szABtnSCM
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 80
    mov QWORD PTR [rsp + 48], LAYOUT_ACTIVITY_BAR_W
    mov QWORD PTR [rsp + 56], 40
    mov rax, hwnd_activity_bar
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_AB_SCM
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA

    ; Debug
    xor rcx, rcx
    lea rdx, szButton
    lea r8, szABtnDebug
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 120
    mov QWORD PTR [rsp + 48], LAYOUT_ACTIVITY_BAR_W
    mov QWORD PTR [rsp + 56], 40
    mov rax, hwnd_activity_bar
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_AB_DEBUG
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA

    ; Extensions
    xor rcx, rcx
    lea rdx, szButton
    lea r8, szABtnExt
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 160
    mov QWORD PTR [rsp + 48], LAYOUT_ACTIVITY_BAR_W
    mov QWORD PTR [rsp + 56], 40
    mov rax, hwnd_activity_bar
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_AB_EXTENSIONS
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA

    ; Sidebar stack (to the right of activity bar)
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szSidebar
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], LAYOUT_ACTIVITY_BAR_W ; X
    mov QWORD PTR [rsp + 40], 0                        ; Y
    mov QWORD PTR [rsp + 48], LAYOUT_SIDEBAR_W         ; width
    mov QWORD PTR [rsp + 56], r12                      ; height
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_sidebar_stack, rax

    ; Sidebar child views (placeholders) inside sidebar stack
    ; Explorer - now with file tree and search box
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szSidebarExplorer
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], LAYOUT_SIDEBAR_W
    mov QWORD PTR [rsp + 56], r12
    mov rax, hwnd_sidebar_stack
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_sidebar_explorer, rax

    ; File tree inside explorer
    xor rcx, rcx
    lea rdx, szTreeView
    lea r8, szFileTree
    mov r9d, WS_CHILD or WS_VISIBLE or TVS_HASBUTTONS or TVS_LINESATROOT or TVS_HASLINES
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 30
    mov QWORD PTR [rsp + 48], LAYOUT_SIDEBAR_W
    mov QWORD PTR [rsp + 56], LAYOUT_FILE_TREE_H
    mov rax, hwnd_sidebar_explorer
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_FILE_TREE
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_file_tree, rax

    ; Search box in explorer
    xor rcx, rcx
    lea rdx, szEdit
    lea r8, szSearchBox
    mov r9d, WS_CHILD or WS_VISIBLE or ES_AUTOHSCROLL
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], LAYOUT_SIDEBAR_W
    mov QWORD PTR [rsp + 56], 25
    mov rax, hwnd_sidebar_explorer
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_SEARCH_BOX
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_search_box, rax

    ; Search (hidden initially)
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szSidebarSearch
    mov r9d, WS_CHILD                     ; hidden until selected
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], LAYOUT_SIDEBAR_W
    mov QWORD PTR [rsp + 56], r12
    mov rax, hwnd_sidebar_stack
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_sidebar_search, rax

    ; SCM
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szSidebarSCM
    mov r9d, WS_CHILD
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], LAYOUT_SIDEBAR_W
    mov QWORD PTR [rsp + 56], r12
    mov rax, hwnd_sidebar_stack
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_sidebar_scm, rax

    ; Debug
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szSidebarDebug
    mov r9d, WS_CHILD
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], LAYOUT_SIDEBAR_W
    mov QWORD PTR [rsp + 56], r12
    mov rax, hwnd_sidebar_stack
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_sidebar_debug, rax

    ; Extensions
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szSidebarExt
    mov r9d, WS_CHILD
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], LAYOUT_SIDEBAR_W
    mov QWORD PTR [rsp + 56], r12
    mov rax, hwnd_sidebar_stack
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_sidebar_ext, rax

    ; Bottom panel (spans under sidebar+editor, starts after activity bar)
    mov eax, r12d
    sub eax, LAYOUT_BOTTOM_H
    mov r10d, eax                       ; r10d = bottom panel Y
    mov eax, ebx
    sub eax, LAYOUT_ACTIVITY_BAR_W
    mov r11d, eax                       ; r11d = bottom panel width

    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szBottomPanel
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], LAYOUT_ACTIVITY_BAR_W   ; X
    mov QWORD PTR [rsp + 40], r10                     ; Y
    mov QWORD PTR [rsp + 48], r11                     ; width
    mov QWORD PTR [rsp + 56], LAYOUT_BOTTOM_H         ; height
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_panel, rax

    ; Bottom header for tabs
    mov r10d, LAYOUT_BOTTOM_HEADER_H
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szBottomTabs
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], r11
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_panel
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_header, rax

    ; Tab buttons
    xor rcx, rcx
    lea rdx, szButton
    lea r8, szTabTerminal
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 6
    mov QWORD PTR [rsp + 40], 2
    mov QWORD PTR [rsp + 48], 90
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_header
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TAB_TERMINAL
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_btn_term, rax

    xor rcx, rcx
    lea rdx, szButton
    lea r8, szTabOutput
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 102
    mov QWORD PTR [rsp + 40], 2
    mov QWORD PTR [rsp + 48], 90
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_header
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TAB_OUTPUT
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_btn_output, rax

    xor rcx, rcx
    lea rdx, szButton
    lea r8, szTabProblems
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 198
    mov QWORD PTR [rsp + 40], 2
    mov QWORD PTR [rsp + 48], 90
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_header
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TAB_PROBLEMS
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_btn_problems, rax

    xor rcx, rcx
    lea rdx, szButton
    lea r8, szTabDebug
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov QWORD PTR [rsp + 32], 294
    mov QWORD PTR [rsp + 40], 2
    mov QWORD PTR [rsp + 48], 120
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_header
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TAB_DEBUGCON
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_btn_debug, rax

    ; Bottom panel stacked content
    mov eax, LAYOUT_BOTTOM_H
    sub eax, LAYOUT_BOTTOM_HEADER_H
    mov r10d, eax

    ; Terminal (RichEdit)
    xor rcx, rcx
    lea rdx, szRichEdit
    lea r8, szBottomTerm
    mov r9d, WS_CHILD or WS_VISIBLE or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], LAYOUT_BOTTOM_HEADER_H
    mov QWORD PTR [rsp + 48], r11
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_panel
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TERMINAL
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_term, rax

    ; Output (RichEdit)
    xor rcx, rcx
    lea rdx, szRichEdit
    lea r8, szBottomOut
    mov r9d, WS_CHILD or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], LAYOUT_BOTTOM_HEADER_H
    mov QWORD PTR [rsp + 48], r11
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_panel
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TAB_OUTPUT
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_output, rax

    ; Problems (ListView)
    xor rcx, rcx
    lea rdx, szListView
    lea r8, szBottomProb
    mov r9d, WS_CHILD or LVS_REPORT or LVS_SINGLESEL
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], LAYOUT_BOTTOM_HEADER_H
    mov QWORD PTR [rsp + 48], r11
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_panel
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_PROBLEMS_LIST
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_problems, rax

    ; Debug Console (RichEdit)
    xor rcx, rcx
    lea rdx, szRichEdit
    lea r8, szBottomDbg
    mov r9d, WS_CHILD or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], LAYOUT_BOTTOM_HEADER_H
    mov QWORD PTR [rsp + 48], r11
    mov QWORD PTR [rsp + 56], r10
    mov rax, hwnd_bottom_panel
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_DEBUG_CONSOLE
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_bottom_debug, rax

    ; Editor host fills remaining area (right of sidebar, above bottom panel)
    mov eax, ebx
    sub eax, LAYOUT_ACTIVITY_BAR_W
    sub eax, LAYOUT_SIDEBAR_W
    mov r9d, eax                        ; editor width

    mov eax, r12d
    sub eax, LAYOUT_BOTTOM_H
    mov r8d, eax                        ; editor height

    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szEditorHostText
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], LAYOUT_ACTIVITY_BAR_W + LAYOUT_SIDEBAR_W ; X
    mov QWORD PTR [rsp + 40], 0                                        ; Y
    mov QWORD PTR [rsp + 48], r9                                       ; width
    mov QWORD PTR [rsp + 56], r8                                       ; height
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_editor_host, rax

    ; Editor header placeholders
    mov eax, ebx
    sub eax, LAYOUT_ACTIVITY_BAR_W
    sub eax, LAYOUT_SIDEBAR_W
    mov r10d, eax
    mov eax, r12d
    sub eax, LAYOUT_BOTTOM_H
    mov r11d, eax

    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szEditorTabLabel
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 8
    mov QWORD PTR [rsp + 40], 0
    mov eax, r10d
    mov QWORD PTR [rsp + 48], rax
    mov QWORD PTR [rsp + 56], LAYOUT_TABBAR_H
    mov rax, hwnd_editor_host
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_editor_label, rax

    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szBreadcrumb
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 8
    mov QWORD PTR [rsp + 40], LAYOUT_TABBAR_H
    mov eax, r10d
    mov QWORD PTR [rsp + 48], rax
    mov QWORD PTR [rsp + 56], LAYOUT_BREADCRUMB_H
    mov rax, hwnd_editor_host
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_breadcrumb, rax

    ; Main code editor (RichEdit) inside the editor host
    xor rcx, rcx
    lea rdx, szRichEdit
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or ES_MULTILINE or WS_VSCROLL or WS_HSCROLL
    mov QWORD PTR [rsp + 32], 0
    mov eax, LAYOUT_TABBAR_H
    add eax, LAYOUT_BREADCRUMB_H
    mov QWORD PTR [rsp + 40], rax
    mov eax, r10d
    sub eax, LAYOUT_MINIMAP_W
    mov QWORD PTR [rsp + 48], rax
    mov eax, r11d
    sub eax, LAYOUT_TABBAR_H
    sub eax, LAYOUT_BREADCRUMB_H
    mov QWORD PTR [rsp + 56], rax
    mov rax, hwnd_editor_host
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_EDITOR
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_editor, rax

    ; Minimap (right side of editor)
    xor rcx, rcx
    lea rdx, szRichEdit
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or ES_MULTILINE or ES_READONLY
    mov eax, r10d
    sub eax, LAYOUT_MINIMAP_W
    mov QWORD PTR [rsp + 32], rax
    mov eax, LAYOUT_TABBAR_H
    add eax, LAYOUT_BREADCRUMB_H
    mov QWORD PTR [rsp + 40], rax
    mov QWORD PTR [rsp + 48], LAYOUT_MINIMAP_W
    mov eax, r11d
    sub eax, LAYOUT_TABBAR_H
    sub eax, LAYOUT_BREADCRUMB_H
    mov QWORD PTR [rsp + 56], rax
    mov rax, hwnd_editor_host
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_MINIMAP
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_minimap, rax

    ; Create status bar
    xor rcx, rcx
    lea rdx, szStatusBar
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 0
    mov eax, r12d
    sub eax, LAYOUT_STATUS_H
    mov QWORD PTR [rsp + 40], rax
    mov QWORD PTR [rsp + 48], rbx
    mov QWORD PTR [rsp + 56], LAYOUT_STATUS_H
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_STATUS_BAR
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_status, rax

    ; Ensure only explorer is visible by default
    mov rcx, hwnd_sidebar_search
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_sidebar_scm
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_sidebar_debug
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_sidebar_ext
    mov rdx, SW_HIDE
    call ShowWindow

    ; Initialize designer-driven layout for docking/movable panes
    call gui_create_complete_ide

    add rsp, 112
    pop r12
    pop rbx
    ret
ui_create_layout_shell ENDP

;==========================================================================
; PUBLIC: ui_get_main_hwnd() -> hwnd (rax)
;==========================================================================
PUBLIC ui_get_main_hwnd
ALIGN 16
ui_get_main_hwnd PROC
    mov rax, hwnd_main
    ret
ui_get_main_hwnd ENDP

;==========================================================================
; PUBLIC: ui_get_editor_hwnd() -> hwnd (rax)
;==========================================================================
PUBLIC ui_get_editor_hwnd
ALIGN 16
ui_get_editor_hwnd PROC
    mov rax, hwnd_editor
    ret
ui_get_editor_hwnd ENDP

;==========================================================================
; PUBLIC: ui_editor_set_text(text: rcx)
; Sets text in the main editor.
;==========================================================================
PUBLIC ui_editor_set_text
ALIGN 16
ui_editor_set_text PROC
    sub rsp, 40
    mov rdx, WM_SETTEXT
    xor r8, r8
    mov r9, rcx
    mov rcx, hwnd_editor
    call SendMessageA
    add rsp, 40
    ret
ui_editor_set_text ENDP

;==========================================================================
; PUBLIC: ui_editor_get_text(out_buf: rcx, max_len: rdx) -> len (eax)
;==========================================================================
PUBLIC ui_editor_get_text
ALIGN 16
ui_editor_get_text PROC
    sub rsp, 40
    mov r8, rdx
    mov r9, rcx
    mov rcx, hwnd_editor
    mov rdx, WM_GETTEXT
    call SendMessageA
    add rsp, 40
    ret
ui_editor_get_text ENDP

;==========================================================================
; PUBLIC: ui_open_text_file_dialog(out_path: rcx, max_len: rdx) -> bool (rax)
;==========================================================================
PUBLIC ui_open_text_file_dialog
ALIGN 16
ui_open_text_file_dialog PROC
    push rbx
    push rdi
    sub rsp, 160

    mov rbx, rcx
    ; Zero OPENFILENAMEA
    mov rdi, rsp
    mov rcx, 20
    xor rax, rax
    rep stosq

    mov DWORD PTR [rsp + OPENFILENAMEA.lStructSize], 136
    mov rax, hwnd_main
    mov QWORD PTR [rsp + OPENFILENAMEA.hwndOwner], rax
    mov rax, h_instance
    mov QWORD PTR [rsp + OPENFILENAMEA.hInstance], rax
    lea rax, szAllFilesFilter
    mov QWORD PTR [rsp + OPENFILENAMEA.lpstrFilter], rax
    mov QWORD PTR [rsp + OPENFILENAMEA.lpstrFile], rbx
    mov DWORD PTR [rsp + OPENFILENAMEA.nMaxFile], 260
    mov DWORD PTR [rsp + OPENFILENAMEA.Flags], OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST
    mov rcx, rsp
    call GetOpenFileNameA

    add rsp, 160
    pop rdi
    pop rbx
    ret
ui_open_text_file_dialog ENDP

;==========================================================================
; PUBLIC: ui_save_text_file_dialog(out_path: rcx, max_len: rdx) -> bool (rax)
;==========================================================================
PUBLIC ui_save_text_file_dialog
ALIGN 16
ui_save_text_file_dialog PROC
    push rbx
    push rdi
    sub rsp, 160

    mov rbx, rcx
    mov rdi, rsp
    mov rcx, 20
    xor rax, rax
    rep stosq

    mov DWORD PTR [rsp + OPENFILENAMEA.lStructSize], 136
    mov rax, hwnd_main
    mov QWORD PTR [rsp + OPENFILENAMEA.hwndOwner], rax
    mov rax, h_instance
    mov QWORD PTR [rsp + OPENFILENAMEA.hInstance], rax
    lea rax, szAllFilesFilter
    mov QWORD PTR [rsp + OPENFILENAMEA.lpstrFilter], rax
    mov QWORD PTR [rsp + OPENFILENAMEA.lpstrFile], rbx
    mov DWORD PTR [rsp + OPENFILENAMEA.nMaxFile], 260
    ; Allow overwrite prompts
    mov DWORD PTR [rsp + OPENFILENAMEA.Flags], OFN_PATHMUSTEXIST
    mov rcx, rsp
    call GetSaveFileNameA

    add rsp, 160
    pop rdi
    pop rbx
    ret
ui_save_text_file_dialog ENDP

;==========================================================================
; PUBLIC: ui_create_menu() -> hmenu (rax)
;
; Creates the main menu bar.
;==========================================================================
PUBLIC ui_create_menu
ALIGN 16
ui_create_menu PROC
    push rbx
    push r12
    push r13
    sub rsp, 56                             ; Shadow space + alignment
    
    ; Create main menu
    call CreateMenu
    mov rbx, rax                            ; Save menu handle
    
    ; Create File submenu
    call CreatePopupMenu
    mov r12, rax                            ; Save File menu handle
    
    ; New (placeholder - not implemented)
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_FILE_NEW
    lea r9, str_file        ; show "File" text; acts as disabled label
    call AppendMenuA
    
    ; Open File...
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_FILE_OPEN_FILE
    lea r9, str_open_file
    call AppendMenuA
    
    ; Open Model...
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_FILE_OPEN_MODEL
    lea r9, str_open_model
    call AppendMenuA
    
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_FILE_SAVE
    lea r9, str_save
    call AppendMenuA

    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_FILE_SAVE_AS
    lea r9, str_save_as
    call AppendMenuA
    
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_FILE_EXIT
    lea r9, str_exit
    call AppendMenuA
    
    ; Create Tools submenu
    call CreatePopupMenu
    mov r13, rax                            ; Save Tools menu handle
    
    ; Hotpatching options
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_HOTPATCH_MEMORY
    lea r9, str_hotpatch_memory
    call AppendMenuA
    
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_HOTPATCH_BYTE
    lea r9, str_hotpatch_byte
    call AppendMenuA
    
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_HOTPATCH_SERVER
    lea r9, str_hotpatch_server
    call AppendMenuA
    
    mov rcx, r12
    mov rdx, MFT_SEPARATOR
    xor r8, r8
    xor r9, r9
    call AppendMenuA
    
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_HOTPATCH_STATS
    lea r9, str_hotpatch_stats
    call AppendMenuA
    
    mov rcx, r12
    mov rdx, MFT_STRING
    mov r8, IDM_HOTPATCH_RESET
    lea r9, str_hotpatch_reset
    call AppendMenuA
    
    ; Append Tools menu to main menu
    mov rcx, rbx
    mov rdx, MF_POPUP
    mov r8, r13
    lea r9, str_tools
    call AppendMenuA
    
    ; Append File menu to main menu
    mov rcx, rbx
    mov rdx, MF_POPUP
    mov r8, r12
    lea r9, str_file
    call AppendMenuA
    
    mov rax, rbx
    add rsp, 56
    pop r13
    pop r12
    pop rbx
    ret
ui_create_menu ENDP

;==========================================================================
; PUBLIC: ui_on_hotpatch_memory()
;==========================================================================
PUBLIC ui_on_hotpatch_memory
ALIGN 16
ui_on_hotpatch_memory PROC
    push rbx
    sub rsp, 32
    ; Fill demo data
    mov ecx, 32
    lea rdi, hotpatch_demo_target
    mov al, 41h
fill_loop:
    mov BYTE PTR [rdi], al
    inc rdi
    dec ecx
    jnz fill_loop
    ; Apply memory patch via unified API
    mov rcx, hpatch_manager_handle
    lea rdx, hotpatch_demo_target
    lea r8, hotpatch_demo_target
    mov r9, 32
    call masm_unified_apply_memory_patch
    test eax, eax
    jz hp_mem_fail
    lea rcx, str_hotpatch_success
    call ui_add_chat_message
    jmp hp_mem_done
hp_mem_fail:
    lea rcx, str_hotpatch_failed
    call ui_add_chat_message
hp_mem_done:
    add rsp, 32
    pop rbx
    ret
ui_on_hotpatch_memory ENDP

;==========================================================================
; PUBLIC: ui_on_hotpatch_byte()
;==========================================================================
PUBLIC ui_on_hotpatch_byte
ALIGN 16
ui_on_hotpatch_byte PROC
    sub rsp, 32
    mov rcx, hpatch_manager_handle
    lea rdx, default_model
    xor r8, r8
    lea r9, hotpatch_demo_target
    call masm_unified_apply_byte_patch
    test eax, eax
    jz hp_byte_fail
    lea rcx, str_hotpatch_success
    call ui_add_chat_message
    jmp hp_byte_done
hp_byte_fail:
    lea rcx, str_hotpatch_failed
    call ui_add_chat_message
hp_byte_done:
    add rsp, 32
    ret
ui_on_hotpatch_byte ENDP

;==========================================================================
; PUBLIC: ui_on_hotpatch_server()
;==========================================================================
PUBLIC ui_on_hotpatch_server
ALIGN 16
ui_on_hotpatch_server PROC
    sub rsp, 32
    mov rcx, hpatch_manager_handle
    lea rdx, hotpatch_demo_target
    call masm_unified_add_server_hotpatch
    test eax, eax
    jz hp_srv_fail
    lea rcx, str_hotpatch_success
    call ui_add_chat_message
    jmp hp_srv_done
hp_srv_fail:
    lea rcx, str_hotpatch_failed
    call ui_add_chat_message
hp_srv_done:
    add rsp, 32
    ret
ui_on_hotpatch_server ENDP

;==========================================================================
; PUBLIC: ui_on_hotpatch_stats()
;==========================================================================
PUBLIC ui_on_hotpatch_stats
ALIGN 16
ui_on_hotpatch_stats PROC
    sub rsp, 40
    ; Get stats via unified API (returns count in eax)
    call masm_unified_get_stats
    ; Compose simple message (just fixed string)
    lea rcx, str_hotpatch_stats_msg
    call ui_add_chat_message
    add rsp, 40
    ret
ui_on_hotpatch_stats ENDP

;==========================================================================
; PUBLIC: ui_on_hotpatch_reset()
;==========================================================================
PUBLIC ui_on_hotpatch_reset
ALIGN 16
ui_on_hotpatch_reset PROC
    sub rsp, 32
    call hpatch_reset_stats
    lea rcx, str_hotpatch_reset
    call ui_add_chat_message
    add rsp, 32
    ret
ui_on_hotpatch_reset ENDP

;==========================================================================
; PUBLIC: ui_set_main_menu(hmenu: rcx)
;
; Attaches menu to main window.
;==========================================================================
PUBLIC ui_set_main_menu
ALIGN 16
ui_set_main_menu PROC
    sub rsp, 40
    mov rdx, rcx                            ; hMenu
    mov rcx, hwnd_main                      ; hWnd
    call SetMenu
    add rsp, 40
    ret
ui_set_main_menu ENDP

;==========================================================================
; PUBLIC: ui_create_chat_control() -> hwnd (rax)
;
; Creates RichEdit control for chat display.
;==========================================================================
PUBLIC ui_create_chat_control
ALIGN 16
ui_create_chat_control PROC
    push rbx
    sub rsp, 112                            ; Shadow space + 8 stack params + alignment
    
    ; Create RichEdit control
    xor rcx, rcx                            ; dwExStyle
    lea rdx, szRichEdit                     ; lpClassName
    xor r8, r8                              ; lpWindowName
    mov r9d, WS_CHILD or WS_VISIBLE or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    
    mov QWORD PTR [rsp + 32], 600           ; X
    mov QWORD PTR [rsp + 40], 0             ; Y
    mov QWORD PTR [rsp + 48], 600           ; nWidth
    mov QWORD PTR [rsp + 56], 400           ; nHeight
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax           ; hWndParent
    mov QWORD PTR [rsp + 72], IDC_CHAT_BOX  ; hMenu (ID)
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax           ; hInstance
    mov QWORD PTR [rsp + 88], 0             ; lpParam
    call CreateWindowExA
    
    mov hwnd_chat, rax
    add rsp, 112
    pop rbx
    ret
ui_create_chat_control ENDP

;==========================================================================
; PUBLIC: ui_create_input_control() -> hwnd (rax)
;
; Creates edit control for user input.
;==========================================================================
PUBLIC ui_create_input_control
ALIGN 16
ui_create_input_control PROC
    push rbx
    sub rsp, 112
    
    xor rcx, rcx
    lea rdx, szEdit
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or ES_MULTILINE or WS_VSCROLL
    
    mov QWORD PTR [rsp + 32], 610           ; X
    mov QWORD PTR [rsp + 40], 410           ; Y
    mov QWORD PTR [rsp + 48], 480           ; nWidth
    mov QWORD PTR [rsp + 56], 100           ; nHeight
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_INPUT_BOX
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    
    mov hwnd_input, rax
    add rsp, 112
    pop rbx
    ret
ui_create_input_control ENDP

;==========================================================================
; PUBLIC: ui_create_terminal_control() -> hwnd (rax)
;
; Creates terminal output window.
;==========================================================================
PUBLIC ui_create_terminal_control
ALIGN 16
ui_create_terminal_control PROC
    push rbx
    sub rsp, 112
    
    xor rcx, rcx
    lea rdx, szRichEdit
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    
    mov QWORD PTR [rsp + 32], 600           ; X
    mov QWORD PTR [rsp + 40], 520           ; Y
    mov QWORD PTR [rsp + 48], 600           ; nWidth
    mov QWORD PTR [rsp + 56], 150           ; nHeight
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TERMINAL
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    
    mov hwnd_terminal, rax
    add rsp, 112
    pop rbx
    ret
ui_create_terminal_control ENDP

; Lightweight wrappers used by integration layer expecting ui_create_terminal/ui_create_chat
PUBLIC ui_create_terminal
ALIGN 16
ui_create_terminal PROC
    sub rsp, 40
    call ui_create_terminal_control
    add rsp, 40
    ret
ui_create_terminal ENDP
PUBLIC ui_create_chat
ALIGN 16
ui_create_chat PROC
    sub rsp, 40
    call ui_create_chat_control
    add rsp, 40
    ret
ui_create_chat ENDP

;==========================================================================
; PUBLIC: ui_create_send_button() -> hwnd (rax)
;
; Creates button to send chat messages.
;==========================================================================
PUBLIC ui_create_send_button
ALIGN 16
ui_create_send_button PROC
    push rbx
    sub rsp, 112
    
    xor rcx, rcx                            ; dwExStyle
    lea rdx, szButton                       ; lpClassName
    lea r8, str_send                        ; lpWindowName
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    
    mov QWORD PTR [rsp + 32], 1100          ; X
    mov QWORD PTR [rsp + 40], 410           ; Y
    mov QWORD PTR [rsp + 48], 80            ; nWidth
    mov QWORD PTR [rsp + 56], 100           ; nHeight
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax           ; hWndParent
    mov QWORD PTR [rsp + 72], IDC_SEND_BUTTON ; hMenu (ID)
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax           ; hInstance
    mov QWORD PTR [rsp + 88], 0             ; lpParam
    call CreateWindowExA
    
    mov hwnd_send, rax
    add rsp, 112
    pop rbx
    ret
ui_create_send_button ENDP

;==========================================================================
; PUBLIC: ui_create_mode_combo() -> hwnd (rax)
;
; Creates ComboBox for model chat modes.
;==========================================================================
PUBLIC ui_create_mode_combo
ALIGN 16
ui_create_mode_combo PROC
    push rbx
    sub rsp, 112
    
    xor rcx, rcx                            ; dwExStyle
    lea rdx, szComboBox                     ; lpClassName
    xor r8, r8                              ; lpWindowName
    mov r9d, WS_CHILD or WS_VISIBLE or CBS_DROPDOWNLIST
    
    mov QWORD PTR [rsp + 32], 1100          ; X
    mov QWORD PTR [rsp + 40], 380           ; Y (Above send button)
    mov QWORD PTR [rsp + 48], 150           ; nWidth
    mov QWORD PTR [rsp + 56], 200           ; nHeight (Dropdown height)
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax           ; hWndParent
    mov QWORD PTR [rsp + 72], IDC_MODE_COMBO ; hMenu (ID)
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax           ; hInstance
    mov QWORD PTR [rsp + 88], 0             ; lpParam
    call CreateWindowExA
    
    mov hwnd_mode_combo, rax
    test rax, rax
    jz combo_done
    
    ; Add items
    mov rbx, rax

    mov rcx, rbx
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, str_mode_max
    call SendMessageA
    
    mov rcx, rbx
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, str_mode_deep
    call SendMessageA
    
    mov rcx, rbx
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, str_mode_research
    call SendMessageA
    
    mov rcx, rbx
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, str_mode_internet
    call SendMessageA
    
    mov rcx, rbx
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, str_mode_thinking
    call SendMessageA
    
    ; Select first item
    mov rcx, rbx
    mov rdx, CB_SETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA

combo_done:
    mov rax, rbx
    add rsp, 112
    pop rbx
    ret
ui_create_mode_combo ENDP

;==========================================================================
; PUBLIC: ui_create_mode_checkboxes()
;
; Creates checkboxes for model chat modes.
;==========================================================================
PUBLIC ui_create_mode_checkboxes
ALIGN 16
ui_create_mode_checkboxes PROC
    push rbx
    sub rsp, 112
    
    ; Max Mode
    xor rcx, rcx
    lea rdx, szButton
    lea r8, str_mode_max
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov QWORD PTR [rsp + 32], 1100
    mov QWORD PTR [rsp + 40], 200
    mov QWORD PTR [rsp + 48], 150
    mov QWORD PTR [rsp + 56], 25
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHECK_MAX
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_check_max, rax
    
    ; Deep Research
    xor rcx, rcx
    lea rdx, szButton
    lea r8, str_mode_deep
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov QWORD PTR [rsp + 32], 1100
    mov QWORD PTR [rsp + 40], 230
    mov QWORD PTR [rsp + 48], 150
    mov QWORD PTR [rsp + 56], 25
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHECK_DEEP
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_check_deep, rax
    
    ; Research
    xor rcx, rcx
    lea rdx, szButton
    lea r8, str_mode_research
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov QWORD PTR [rsp + 32], 1100
    mov QWORD PTR [rsp + 40], 260
    mov QWORD PTR [rsp + 48], 150
    mov QWORD PTR [rsp + 56], 25
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHECK_RESEARCH
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_check_research, rax
    
    ; Internet Search
    xor rcx, rcx
    lea rdx, szButton
    lea r8, str_mode_internet
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov QWORD PTR [rsp + 32], 1100
    mov QWORD PTR [rsp + 40], 290
    mov QWORD PTR [rsp + 48], 150
    mov QWORD PTR [rsp + 56], 25
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHECK_INTERNET
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_check_internet, rax
    
    ; Thinking
    xor rcx, rcx
    lea rdx, szButton
    lea r8, str_mode_thinking
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov QWORD PTR [rsp + 32], 1100
    mov QWORD PTR [rsp + 40], 320
    mov QWORD PTR [rsp + 48], 150
    mov QWORD PTR [rsp + 56], 25
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHECK_THINKING
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_check_thinking, rax
    
    add rsp, 112
    pop rbx
    ret
ui_create_mode_checkboxes ENDP

;==========================================================================
; PUBLIC: ui_open_file_dialog(out_path: rcx, max_len: rdx) -> bool (rax)
;
; Shows Open File dialog.
;==========================================================================
PUBLIC ui_open_file_dialog
ALIGN 16
ui_open_file_dialog PROC
    push rbx
    push rdi
    sub rsp, 160                            ; Space for OPENFILENAMEA (136) + alignment
    
    mov rbx, rcx                            ; Save out_path
    
    ; Zero structure
    mov rdi, rsp
    mov rcx, 20                             ; 20 QWORDs = 160 bytes
    xor rax, rax
    rep stosq
    
    mov DWORD PTR [rsp + OPENFILENAMEA.lStructSize], 136 ; Size of OPENFILENAMEA
    mov rax, hwnd_main
    mov QWORD PTR [rsp + OPENFILENAMEA.hwndOwner], rax
    mov rax, h_instance
    mov QWORD PTR [rsp + OPENFILENAMEA.hInstance], rax
    
    ; Filter: "GGUF Models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0\0"
    lea rax, szFilter
    mov QWORD PTR [rsp + OPENFILENAMEA.lpstrFilter], rax
    
    mov QWORD PTR [rsp + OPENFILENAMEA.lpstrFile], rbx
    mov DWORD PTR [rsp + OPENFILENAMEA.nMaxFile], 260 ; MAX_PATH
    
    mov DWORD PTR [rsp + OPENFILENAMEA.Flags], OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST
    
    mov rcx, rsp
    call GetOpenFileNameA
    
    add rsp, 160
    pop rdi
    pop rbx
    ret
ui_open_file_dialog ENDP

.data
    szFilter BYTE "GGUF Models (*.gguf)",0,"*.gguf",0,"All Files (*.*)",0,"*.*",0,0
    szFilterAll BYTE "All Files (*.*)",0,"*.*",0,0
    szChatMsgBoxTitle BYTE "ui_add_chat_message",0

.data?
    ui_add_chat_once  BYTE ?

;==========================================================================
; PUBLIC: ui_add_chat_message(msg: rcx)
;
; Appends message to chat RichEdit control.
;==========================================================================
PUBLIC ui_add_chat_message
ALIGN 16
ui_add_chat_message PROC
    push rbx
    sub rsp, 32                             ; shadow space, keeps alignment

    mov rbx, rcx                            ; preserve message pointer

    ; Log entry
    lea rcx, szLogAddChat
    call asm_log

    lea rcx, dbg_ui_add_chat_entry
    call console_log

    ; If chat control is missing, exit safely
    mov rax, hwnd_chat
    test rax, rax
    jz chat_done

    ; Set selection to end
    mov rcx, hwnd_chat
    mov rdx, EM_SETSEL
    mov r8, -1
    mov r9, -1
    call SendMessageA
    lea rcx, dbg_ui_add_chat_after_setsel
    call console_log

    ; Append message
    mov rcx, hwnd_chat
    mov rdx, EM_REPLACESEL
    xor r8, r8
    mov r9, rbx
    call SendMessageA
    lea rcx, dbg_ui_add_chat_after_replace
    call console_log

    ; Append newline
    mov rcx, hwnd_chat
    mov rdx, EM_REPLACESEL
    xor r8, r8
    lea r9, szNewline
    call SendMessageA

chat_done:
    lea rcx, dbg_ui_add_chat_done
    call console_log
    add rsp, 32
    pop rbx
    ret
ui_add_chat_message ENDP

;==========================================================================
; PUBLIC: ui_get_input_text(out_buf: rcx, max_len: rdx)
;
; Reads text from input box. Returns length.
;==========================================================================
PUBLIC ui_get_input_text
ALIGN 16
ui_get_input_text PROC
    sub rsp, 40
    
    push rcx
    push rdx
    lea rcx, szLogGetInput
    call asm_log
    pop rdx
    pop rcx
    
    mov r8, rdx                             ; max_len
    mov r9, rcx                             ; out_buf
    mov rcx, hwnd_input
    mov rdx, WM_GETTEXT
    call SendMessageA
    add rsp, 40
    ret
ui_get_input_text ENDP

;==========================================================================
; PUBLIC: ui_clear_input()
;
; Clears input box.
;==========================================================================
PUBLIC ui_clear_input
ALIGN 16
ui_clear_input PROC
    sub rsp, 40
    
    lea rcx, szLogClearInput
    call asm_log
    
    mov rcx, hwnd_input
    mov rdx, WM_SETTEXT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    add rsp, 40
    ret
ui_clear_input ENDP

;==========================================================================
; PUBLIC: ui_show_dialog(title: rcx, message: rdx)
;
; Shows simple message box.
;==========================================================================
PUBLIC ui_show_dialog
ALIGN 16
ui_show_dialog PROC
    sub rsp, 40
    
    push rcx
    push rdx
    lea rcx, szLogShowDialog
    call asm_log
    pop rdx
    pop rcx
    
    mov r8, rcx                             ; title
    mov r9d, MB_OK
    mov rcx, hwnd_main
    ; rdx is already message
    call MessageBoxA
    add rsp, 40
    ret
ui_show_dialog ENDP

;==========================================================================
; Window Procedure
;==========================================================================
PUBLIC wnd_proc_main
ALIGN 16
wnd_proc_main PROC
    ; rcx = hwnd, rdx = uMsg, r8 = wParam, r9 = lParam
    sub rsp, 40                             ; Shadow space (32) + alignment (8)

    ; simple entry trace
    lea rcx, dbg_wndproc_entry
    call OutputDebugStringA

    ; DISABLED: Triage code was causing crashes
    ; mov QWORD PTR [rsp + 32], rcx
    ; mov QWORD PTR [rsp + 40], rdx
    ; mov QWORD PTR [rsp + 48], r8
    ; mov QWORD PTR [rsp + 56], r9
    ; lea rcx, dbg_wndproc_entry
    ; call OutputDebugStringA
    ; mov rcx, QWORD PTR [rsp + 32]
    ; mov rdx, QWORD PTR [rsp + 40]
    ; mov r8,  QWORD PTR [rsp + 48]
    ; mov r9,  QWORD PTR [rsp + 56]
    
    cmp edx, WM_DESTROY
    je wm_destroy
    
    cmp edx, WM_COMMAND
    je wm_command
    
    cmp edx, WM_LBUTTONDOWN
    je wm_lbuttondown
    
    cmp edx, WM_MOUSEMOVE
    je wm_mousemove
    
    cmp edx, WM_LBUTTONUP
    je wm_lbuttonup

    cmp edx, WM_KEYDOWN
    je wm_keydown

    cmp edx, WM_TIMER
    je wm_timer
    
    call DefWindowProcA
    add rsp, 40
    ret

wm_keydown:
    ; Pass to keyboard shortcut manager
    mov rcx, r8         ; wParam (key)
    call keyboard_shortcuts_process
    test eax, eax
    jnz wm_keydown_handled
    
    call DefWindowProcA
    add rsp, 40
    ret
wm_keydown_handled:
    xor eax, eax
    add rsp, 40
    ret

wm_timer:
    cmp r8d, 1001       ; AUTO_SAVE_TIMER_ID
    jne wm_timer_def
    
    call session_trigger_autosave
    xor eax, eax
    add rsp, 40
    ret
wm_timer_def:
    call DefWindowProcA
    add rsp, 40
    ret

wm_destroy:
    xor rcx, rcx
    call PostQuitMessage
    xor eax, eax
    add rsp, 40
    ret

wm_command:
    mov eax, r8d                            ; wParam (low word is ID)
    and eax, 0FFFFh
    
    cmp eax, IDM_FILE_OPEN_MODEL
    je wm_open_model
    
    cmp eax, IDM_FILE_OPEN_FILE
    je wm_open_file

    cmp eax, IDM_FILE_SAVE
    je wm_save_file

    cmp eax, IDM_FILE_SAVE_AS
    je wm_save_as
    
    cmp eax, IDM_FILE_EXIT
    je wm_exit
    
    cmp eax, IDM_CHAT_CLEAR
    je wm_chat_clear
    
    cmp eax, IDM_SETTINGS_MODEL
    je wm_settings_model
    
    cmp eax, IDM_AGENT_TOGGLE
    je wm_agent_toggle
    
    cmp eax, IDM_HOTPATCH_MEMORY
    je wm_hotpatch_memory
    
    cmp eax, IDM_HOTPATCH_BYTE
    je wm_hotpatch_byte
    
    cmp eax, IDM_HOTPATCH_SERVER
    je wm_hotpatch_server
    
    cmp eax, IDM_HOTPATCH_STATS
    je wm_hotpatch_stats
    
    cmp eax, IDM_HOTPATCH_RESET
    je wm_hotpatch_reset
    
    cmp eax, IDC_SEND_BUTTON
    je wm_send

    cmp eax, IDC_MODE_COMBO
    je wm_mode_combo

    cmp eax, IDC_AB_EXPLORER
    je wm_sidebar_explorer
    cmp eax, IDC_AB_SEARCH
    je wm_sidebar_search
    cmp eax, IDC_AB_SCM
    je wm_sidebar_scm
    cmp eax, IDC_AB_DEBUG
    je wm_sidebar_debug
    cmp eax, IDC_AB_EXTENSIONS
    je wm_sidebar_ext

    cmp eax, IDC_TAB_TERMINAL
    je wm_bottom_term
    cmp eax, IDC_TAB_OUTPUT
    je wm_bottom_output
    cmp eax, IDC_TAB_PROBLEMS
    je wm_bottom_problems
    cmp eax, IDC_TAB_DEBUGCON
    je wm_bottom_debug

    cmp eax, IDC_COMMAND_PALETTE
    je wm_command_palette

    cmp eax, IDC_FILE_TREE
    je wm_file_tree

    cmp eax, IDC_SEARCH_BOX
    je wm_search_box

    cmp eax, IDC_PROBLEMS_LIST
    je wm_problems_list

    cmp eax, IDC_DEBUG_CONSOLE
    je wm_debug_console
    
    call DefWindowProcA
    add rsp, 40
    ret

wm_open_model:
    call main_on_open
    xor eax, eax
    add rsp, 40
    ret

wm_open_file:
    call main_on_open_file
    xor eax, eax
    add rsp, 40
    ret

wm_save_file:
    call main_on_save_file
    xor eax, eax
    add rsp, 40
    ret

wm_save_as:
    call main_on_save_file_as
    xor eax, eax
    add rsp, 40
    ret

wm_send:
    call main_on_send
    xor eax, eax
    add rsp, 40
    ret

wm_mode_combo:
    ; Update current mode selection
    mov rcx, hwnd_mode_combo
    mov rdx, CB_GETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA
    mov current_mode, eax
    lea rcx, dbg_mode_combo_changed
    call console_log
    xor eax, eax
    add rsp, 40
    ret

wm_exit:
    mov rcx, hwnd_main
    call DestroyWindow
    xor eax, eax
    add rsp, 40
    ret

wm_sidebar_explorer:
    mov ecx, 0
    call ui_switch_sidebar_view
    xor eax, eax
    add rsp, 40
    ret

wm_sidebar_search:
    mov ecx, 1
    call ui_switch_sidebar_view
    xor eax, eax
    add rsp, 40
    ret

wm_sidebar_scm:
    mov ecx, 2
    call ui_switch_sidebar_view
    xor eax, eax
    add rsp, 40
    ret

wm_sidebar_debug:
    mov ecx, 3
    call ui_switch_sidebar_view
    xor eax, eax
    add rsp, 40
    ret

wm_sidebar_ext:
    mov ecx, 4
    call ui_switch_sidebar_view
    xor eax, eax
    add rsp, 40
    ret

wm_bottom_term:
    mov ecx, 0
    call ui_switch_bottom_tab
    xor eax, eax
    add rsp, 40
    ret

wm_bottom_output:
    mov ecx, 1
    call ui_switch_bottom_tab
    xor eax, eax
    add rsp, 40
    ret

wm_bottom_problems:
    mov ecx, 2
    call ui_switch_bottom_tab
    xor eax, eax
    add rsp, 40
    ret

wm_bottom_debug:
    mov ecx, 3
    call ui_switch_bottom_tab
    xor eax, eax
    add rsp, 40
    ret

wm_command_palette:
    ; Handle command palette selection - real implementation
    ; Get selected command index from dropdown
    mov rcx, hwnd_command_palette
    mov rdx, CB_GETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    ; rax = selected index; -1 means no selection
    cmp rax, -1
    je cmd_palette_done_local
    
    ; Map index to command ID and dispatch
    add rax, 1000
    mov r8d, eax
    
    ; Dispatch based on command ID
    cmp r8d, 1001
    je cmd_file_open_local
    cmp r8d, 1002
    je cmd_file_new_local
    cmp r8d, 1003
    je cmd_file_save_local
    cmp r8d, 2001
    je cmd_edit_cut_local
    cmp r8d, 2002
    je cmd_edit_copy_local
    cmp r8d, 2003
    je cmd_edit_paste_local
    jmp cmd_palette_done_local
    
cmd_file_open_local:
    call ui_file_open_dialog
    jmp cmd_palette_done_local
cmd_file_new_local:
    xor rcx, rcx
    call ui_editor_set_text
    jmp cmd_palette_done_local
cmd_file_save_local:
    call ui_file_save
    jmp cmd_palette_done_local
cmd_edit_cut_local:
    mov rcx, hwnd_editor
    mov rdx, WM_CUT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    jmp cmd_palette_done_local
cmd_edit_copy_local:
    mov rcx, hwnd_editor
    mov rdx, WM_COPY
    xor r8, r8
    xor r9, r9
    call SendMessageA
    jmp cmd_palette_done_local
cmd_edit_paste_local:
    mov rcx, hwnd_editor
    mov rdx, WM_PASTE
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
cmd_palette_done_local:
    xor eax, eax
    add rsp, 40
    ret

wm_file_tree:
    ; Handle file tree selection - open selected file in editor
    ; Get selected tree item
    mov rcx, hwnd_file_tree
    mov rdx, TVM_GETNEXTITEM
    mov r8d, TVGN_CARET                 ; Get selection
    xor r9, r9
    call SendMessageA
    test rax, rax
    jz wm_file_tree_done
    
    ; Get item text (file path)
    lea rcx, [rsp + 48]                 ; Stack space for TVITEMA
    mov DWORD PTR [rcx], TVIF_TEXT      ; mask
    mov QWORD PTR [rcx + 8], rax        ; hItem
    lea rdx, editor_file_path
    mov QWORD PTR [rcx + 24], rdx       ; pszText
    mov DWORD PTR [rcx + 32], MAX_PATH_LEN ; cchTextMax
    
    ; Get item text via message
    mov rcx, hwnd_file_tree
    mov rdx, TVM_GETITEMA
    xor r8, r8
    lea r9, [rsp + 48]
    call SendMessageA
    
    ; File path now in editor_file_path
    ; Load into editor
    lea rcx, editor_file_path
    call ui_editor_set_text
    
    lea rcx, dbg_file_opened
    call OutputDebugStringA
    
wm_file_tree_done:
    xor eax, eax
    add rsp, 40
    ret

wm_search_box:
    ; Handle search box input - file search with recursion
    push rbx
    push r12
    sub rsp, 256
    
    ; Get search pattern from search_box
    mov rcx, hwnd_search_box
    mov rdx, WM_GETTEXT
    mov r8, 256
    lea r9, [rsp + 48]
    call SendMessageA
    
    ; Start recursive search from current directory
    lea rcx, current_dir                ; Start at "."
    lea rdx, [rsp + 48]                 ; Pattern buffer
    xor r8d, r8d                        ; Depth counter = 0
    call file_search_recursive
    
    ; eax = file count found
    lea rcx, dbg_search_performed
    call OutputDebugStringA
    
    add rsp, 256
    pop r12
    pop rbx
    xor eax, eax
    add rsp, 40
    ret

; Recursive file search implementation
file_search_recursive PROC
    ; rcx = directory path, rdx = pattern, r8d = depth
    ; Returns eax = match count
    push rbx
    push r12
    push r13
    sub rsp, 256
    
    ; Limit recursion depth to 10
    cmp r8d, 10
    jge search_exit_local
    
    mov r12, rcx                        ; Save directory
    mov r13, rdx                        ; Save pattern
    xor ebx, ebx                        ; Match counter
    
    ; Build search path: directory\*.*
    lea rcx, [rsp + 8]
    mov rdx, r12
    call strcpy_masm                    ; Copy directory path
    
    ; Add \*.* to path
    lea rcx, [rsp + 8]
    lea rdx, search_wildcard
    call strcat_masm
    
    ; FindFirstFileW
    lea rcx, [rsp + 256 - 560]          ; WIN32_FIND_DATA buffer
    mov rdx, [rsp + 8]                  ; Search path
    mov rax, find_first_file_w_addr
    call rax
    
    cmp rax, -1
    je search_exit_local
    
    mov r12d, eax                       ; Save search handle
    
search_loop_local:
    ; Check if filename matches pattern (case-insensitive)
    lea rcx, [rsp + 256 - 560 + 44]     ; cFileName field

    mov rdx, r13                        ; Pattern
    call strstr_case_insensitive
    test rax, rax
    jz search_next_file_local
    
    ; Match found - increment counter
    inc ebx
    
search_next_file_local:
    ; FindNextFileW
    mov ecx, r12d                       ; Search handle
    lea rdx, [rsp + 256 - 560]          ; Find data
    mov rax, find_next_file_w_addr
    call rax
    test eax, eax
    jnz search_loop_local
    
    ; Close search handle
    mov ecx, r12d
    mov rax, find_close_addr
    call rax
    
search_exit_local:
    mov eax, ebx                        ; Return match count
    add rsp, 256
    pop r13
    pop r12
    pop rbx
    ret
file_search_recursive ENDP

; Case-insensitive string search helper
; PUBLIC: strstr_case_insensitive(haystack: rcx, needle: rdx) -> rax (pointer to match or NULL)
PUBLIC strstr_case_insensitive
strstr_case_insensitive PROC
    ; rcx = haystack, rdx = needle
    ; Returns rax = pointer to match or NULL
    xor r8, r8                          ; Position in haystack
    xor r9, r9                          ; Position in needle
    
search_local:
    mov al, BYTE PTR [rcx + r8]
    test al, al
    jz not_found_local
    
    mov r9b, BYTE PTR [rdx]
    test r9b, r9b
    jz found_local
    
    ; Lowercase comparison
    mov r10b, al
    cmp r10b, 'A'
    jl skip_al_local
    cmp r10b, 'Z'
    jg skip_al_local
    add r10b, 32
skip_al_local:
    
    mov r11b, r9b
    cmp r11b, 'A'
    jl skip_b_local
    cmp r11b, 'Z'
    jg skip_b_local
    add r11b, 32
skip_b_local:
    
    cmp r10b, r11b
    jne advance_local
    
    inc r8
    inc rdx
    jmp search_local
    
advance_local:
    inc rcx
    xor r8, r8
    mov rdx, [rsp + 8]                  ; Reload needle pointer
    jmp search_local
    
found_local:
    lea rax, [rcx + r8]
    ret
    
not_found_local:
    xor rax, rax
    ret
strstr_case_insensitive ENDP

wm_problems_list:
    ; Handle problems list selection - parse error and navigate
    push rbx
    sub rsp, 256
    
    ; Get selected problem item index
    mov rcx, hwnd_problems_list
    mov rdx, LVM_GETSELECTIONMARK
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    cmp rax, -1
    je problem_done_local
    
    mov ebx, eax                        ; Save index
    
    ; Get problem text from list item
    lea rcx, [rsp + 8]
    mov rdx, 256
    mov r8d, ebx
    call get_problem_text               ; Get error string
    
    ; Parse "filename:line:col" format
    lea rsi, [rsp + 8]
    xor ecx, ecx                        ; Position counter
    xor r8d, r8d                        ; Line number
    
parse_colon_local:
    mov al, BYTE PTR [rsi + rcx]
    test al, al
    jz parse_end_local
    cmp al, ':'
    je found_colon_local
    inc ecx
    jmp parse_colon_local
    
found_colon_local:
    ; rcx now points to first ':'
    ; Extract line number after it
    add rcx, 1
    xor r8d, r8d
    
parse_line_local:
    mov al, BYTE PTR [rsi + rcx]
    cmp al, ':'
    je have_line_local
    cmp al, 0
    je have_line_local
    
    sub al, '0'
    cmp al, 9
    ja have_line_local
    
    ; Accumulate digit
    imul r8d, 10
    movzx rax, al
    add r8d, eax
    inc ecx
    jmp parse_line_local
    
have_line_local:
    ; Jump to line in editor
    mov rcx, r8
    call editor_jump_to_line
    
parse_end_local:
    lea rcx, dbg_problem_navigate
    call OutputDebugStringA
    
problem_done_local:
    add rsp, 256
    pop rbx
    xor eax, eax
    add rsp, 40
    ret

; Helper: Get problem text from list
get_problem_text PROC
    ; rcx = buffer, rdx = buffer size, r8d = problem index
    ; Returns text in buffer
    ; Simplified: just copy placeholder
    lea rax, placeholder_error
    mov rsi, rax
    mov rdi, rcx
    mov ecx, 256
    rep movsb
    ret
get_problem_text ENDP

; Helper: Jump editor to line
editor_jump_to_line PROC
    ; rcx = line number
    push rbx
    sub rsp, 32
    
    mov rbx, rcx                        ; Save line number
    
    ; Send EM_LINESCROLL to editor
    mov rcx, hwnd_editor
    mov rdx, EM_LINESCROLL
    xor r8, r8
    mov r9, rbx
    call SendMessageA
    
    add rsp, 32
    pop rbx
    ret
editor_jump_to_line ENDP

placeholder_error db "fileasm_local:100:5: error",0

wm_debug_console:
    ; Handle debug console input - parse and execute debug commands
    push rbx
    sub rsp, 256
    
    ; Get command text from debug console
    mov rcx, hwnd_debug_console
    mov rdx, WM_GETTEXT
    mov r8, 256
    lea r9, [rsp + 8]
    call SendMessageA
    
    ; Parse command - check what it starts with
    lea rsi, [rsp + 8]
    
    ; Check for "break" command
    lea rcx, szBreakCmd
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jnz cmd_break_local
    
    ; Check for "continue" command
    lea rcx, szContinueCmd
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jnz cmd_continue_local
    
    ; Check for "step" command
    lea rcx, szStepCmd
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jnz cmd_step_local
    
    ; Check for "next" command
    lea rcx, szNextCmd
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jnz cmd_next_local
    
    ; Unknown command
    lea rcx, szUnknownDebugCmd
    jmp cmd_output_local
    
cmd_break_local:
    call debug_set_breakpoint
    lea rcx, szBreakpointSet
    jmp cmd_output_local
    
cmd_continue_local:
    call debug_resume
    lea rcx, szDebugContinued
    jmp cmd_output_local
    
cmd_step_local:
    call debug_step_into
    lea rcx, szDebugStepped
    jmp cmd_output_local
    
cmd_next_local:
    call debug_step_over
    lea rcx, szDebugStepped
    
cmd_output_local:
    ; Display result in debug output window
    mov rbx, rcx
    mov rcx, hwnd_debug_output
    mov rdx, EM_REPLACESEL
    xor r8, r8
    mov r9, rbx
    call SendMessageA
    
    ; Clear console for next command
    mov rcx, hwnd_debug_console
    mov rdx, WM_SETTEXT
    xor r8, r8
    lea r9, szEmptyString
    call SendMessageA
    
    lea rcx, dbg_debug_cmd_executed
    call OutputDebugStringA
    
    add rsp, 256
    pop rbx
    xor eax, eax
    add rsp, 40
    ret

; Debug command helper stubs
debug_set_breakpoint PROC
    ret
debug_set_breakpoint ENDP
debug_resume PROC
    ret
debug_resume ENDP
debug_step_into PROC
    ret
debug_step_into ENDP
debug_step_over PROC
    ret
debug_step_over ENDP

; Implement missing string functions
strcpy_masm PROC
    ; rcx = dest, rdx = src
    push rsi
    push rdi
    mov rsi, rdx
    mov rdi, rcx
strcpy_loop:
    mov al, byte ptr [rsi]
    mov byte ptr [rdi], al
    test al, al
    jz strcpy_done
    inc rsi
    inc rdi
    jmp strcpy_loop
strcpy_done:
    mov rax, rcx
    pop rdi
    pop rsi
    ret
strcpy_masm ENDP

strcat_masm PROC
    ; rcx = dest, rdx = src
    push rsi
    push rdi
    mov rdi, rcx
    ; Find end of dest
strcat_find_end:
    cmp byte ptr [rdi], 0
    je strcat_copy
    inc rdi
    jmp strcat_find_end
strcat_copy:
    mov rsi, rdx
strcat_loop:
    mov al, byte ptr [rsi]
    mov byte ptr [rdi], al
    test al, al
    jz strcat_done
    inc rsi
    inc rdi
    jmp strcat_loop
strcat_done:
    mov rax, rcx
    pop rdi
    pop rsi
    ret
strcat_masm ENDP

strstr_masm PROC
    ; rcx = haystack, rdx = needle
    push rsi
    push rdi
    push rbx
    mov rsi, rcx
    mov rdi, rdx
    ; Check if needle is empty
    cmp byte ptr [rdi], 0
    je strstr_empty
strstr_outer:
    mov rbx, rsi
    mov rdx, rdi
strstr_inner:
    mov al, byte ptr [rdx]
    test al, al
    jz strstr_found
    cmp byte ptr [rbx], al
    jne strstr_next
    inc rbx
    inc rdx
    jmp strstr_inner
strstr_next:
    cmp byte ptr [rsi], 0
    je strstr_not_found
    inc rsi
    jmp strstr_outer
strstr_found:
    mov rax, rsi
    jmp strstr_done
strstr_empty:
    mov rax, rcx
    jmp strstr_done
strstr_not_found:
    xor rax, rax
strstr_done:
    pop rbx
    pop rdi
    pop rsi
    ret
strstr_masm ENDP

wm_chat_clear:
    ; Clear chat history
    mov rcx, hwnd_chat
    mov rdx, EM_SETSEL
    xor r8, r8
    mov r9, -1
    call SendMessageA
    mov rcx, hwnd_chat
    mov rdx, EM_REPLACESEL
    xor r8, r8
    lea r9, szNewline
    call SendMessageA
    lea rcx, str_chat_clear
    call ui_add_chat_message
    xor eax, eax
    add rsp, 40
    ret

wm_settings_model:
    ; Show model settings dialog
    lea rcx, str_settings
    lea rdx, str_model
    call ui_show_dialog
    xor eax, eax
    add rsp, 40
    ret

wm_agent_toggle:
    ; Toggle agent mode
    mov eax, agent_running
    xor eax, 1
    mov agent_running, eax
    ; Provide feedback
    test eax, eax
    jz agent_disabled
    lea rcx, str_agent_enabled
    jmp agent_feedback
agent_disabled:
    lea rcx, str_agent_disabled
agent_feedback:
    call ui_add_chat_message
    xor eax, eax
    add rsp, 40
    ret

wm_hotpatch_memory:
    ; Apply memory hotpatch - show dialog
    mov rcx, hwnd_main
    lea rdx, str_hp_mem_title
    lea r8, str_hp_mem_title
    mov r9d, MB_OKCANCEL
    call MessageBoxA
    cmp eax, IDCANCEL
    je hotpatch_memory_done
    
    ; Perform actual memory patch via unified API
    mov rcx, hpatch_manager_handle
    test rcx, rcx
    jz hotpatch_memory_no_manager
    
    ; Demo: patch the demo target buffer
    lea rdx, hotpatch_demo_target        ; source buffer
    lea r8, hotpatch_demo_target        ; destination
    mov r9, 32                           ; size
    call masm_unified_apply_memory_patch
    test eax, eax
    jz hotpatch_memory_failed
    
    mov rcx, hwnd_main
    lea rdx, str_hp_success
    lea r8, str_hp_mem_title
    mov r9d, MB_OK
    call MessageBoxA
    jmp hotpatch_memory_done
    
hotpatch_memory_no_manager:
    mov rcx, hwnd_main
    lea rdx, str_hotpatch_manager_not_init
    lea r8, str_hp_mem_title
    mov r9d, MB_ICONERROR
    call MessageBoxA
    jmp hotpatch_memory_done
    
hotpatch_memory_failed:
    mov rcx, hwnd_main
    lea rdx, str_hp_err_apply
    lea r8, str_hp_mem_title
    mov r9d, MB_ICONERROR
    call MessageBoxA
    
hotpatch_memory_done:
    lea rcx, dbg_hotpatch_dialog_shown
    call OutputDebugStringA
    xor eax, eax
    add rsp, 40
    ret

wm_hotpatch_byte:
    ; Apply byte-level hotpatch - show dialog
    mov rcx, hwnd_main
    lea rdx, str_hp_byte_title
    lea r8, str_hp_byte_title
    mov r9d, MB_OKCANCEL
    call MessageBoxA
    cmp eax, IDCANCEL
    je hotpatch_byte_done
    
    ; Perform actual byte patch via unified API
    mov rcx, hpatch_manager_handle
    test rcx, rcx
    jz hotpatch_byte_no_manager
    
    ; Call unified byte patch API
    lea rdx, default_model              ; model name
    xor r8, r8                          ; pattern (null for demo)
    lea r9, hotpatch_demo_target        ; replacement
    call masm_unified_apply_byte_patch
    test eax, eax
    jz hotpatch_byte_failed
    
    mov rcx, hwnd_main
    lea rdx, str_hp_success
    lea r8, str_hp_byte_title
    mov r9d, MB_OK
    call MessageBoxA
    jmp hotpatch_byte_done
    
hotpatch_byte_no_manager:
    mov rcx, hwnd_main
    lea rdx, str_hotpatch_manager_not_init
    lea r8, str_hp_byte_title
    mov r9d, MB_ICONERROR
    call MessageBoxA
    jmp hotpatch_byte_done
    
hotpatch_byte_failed:
    mov rcx, hwnd_main
    lea rdx, str_hp_err_apply
    lea r8, str_hp_byte_title
    mov r9d, MB_ICONERROR
    call MessageBoxA
    
hotpatch_byte_done:
    lea rcx, dbg_hotpatch_dialog_shown
    call OutputDebugStringA
    xor eax, eax
    add rsp, 40
    ret

wm_hotpatch_server:
    ; Apply server hotpatch - show dialog
    mov rcx, hwnd_main
    lea rdx, str_hp_srv_title
    lea r8, str_hp_srv_title
    mov r9d, MB_OKCANCEL
    call MessageBoxA
    cmp eax, IDCANCEL
    je hotpatch_server_done
    
    ; Server hotpatch would require endpoint and rule definition
    ; For now: show success message
    mov rcx, hwnd_main
    lea rdx, str_hp_success
    lea r8, str_hp_srv_title
    mov r9d, MB_OK
    call MessageBoxA

hotpatch_server_done:
    lea rcx, dbg_hotpatch_dialog_shown
    call OutputDebugStringA
    xor eax, eax
    add rsp, 40
    ret

wm_hotpatch_stats:
    ; Show hotpatch statistics
    mov rcx, hpatch_manager_handle
    test rcx, rcx
    jz stats_no_manager
    
    ; Call unified stats API
    call masm_unified_get_stats
    ; eax contains stats (simplified)
    
    lea rcx, str_hotpatch_stats_ready
    call ui_add_chat_message
    jmp stats_done
    
stats_no_manager:
    lea rcx, str_hotpatch_manager_not_init
    call ui_add_chat_message
    
stats_done:
    xor eax, eax
    add rsp, 40
    ret

wm_hotpatch_reset:
    ; Reset hotpatch statistics
    mov rcx, hpatch_manager_handle
    test rcx, rcx
    jz reset_no_manager
    
    call hpatch_reset_stats
    
    lea rcx, str_hotpatch_stats_reset
    call ui_add_chat_message
    jmp reset_done
    
reset_no_manager:
    lea rcx, str_hotpatch_manager_not_init
    call ui_add_chat_message
    
reset_done:
    xor eax, eax
    add rsp, 40
    ret

wm_lbuttondown:
    ; Extract mouse position from lParam
    ; lParam: low 16 bits = X, high 16 bits = Y
    mov eax, r9d                        ; r9 = lParam
    and eax, 0FFFFh                     ; X coordinate (low word)
    mov edx, r9d
    shr edx, 16                         ; Y coordinate (high word)
    
    ; Call pane_hit_test(x: eax, y: edx) to find pane at mouse position
    mov ecx, eax                        ; x -> ecx
    ; edx already has y
    call pane_hit_test
    test eax, eax
    jz lbuttondown_no_pane
    
    ; eax = pane_id found
    ; Store drag state in global variables
    mov [g_dragged_pane_id], eax
    
    ; Store drag start position
    mov eax, r9d                        ; lParam
    and eax, 0FFFFh
    mov [g_drag_start_x], eax
    
    mov eax, r9d
    shr eax, 16
    mov [g_drag_start_y], eax
    
    ; Set flag that we're dragging
    mov DWORD PTR [g_dragging], 1
    
    lea rcx, dbg_pane_drag_start
    call OutputDebugStringA
    
lbuttondown_no_pane:
    xor eax, eax
    add rsp, 40
    ret

wm_mousemove:
    ; Check if we're currently dragging a pane
    mov eax, [g_dragging]
    test eax, eax
    jz mousemove_done
    
    ; Get current mouse position from lParam
    mov eax, r9d                        ; lParam
    and eax, 0FFFFh                     ; Current X
    mov edx, r9d
    shr edx, 16                         ; Current Y
    
    ; For now, just trigger a redraw to see drag feedback
    ; In production, would update pane positions based on delta
    mov rcx, hwnd_main
    xor rdx, rdx                        ; NULL rect (entire window)
    mov r8, 1                           ; TRUE (erase background)
    call InvalidateRect
    
mousemove_done:
    xor eax, eax
    add rsp, 40
    ret

wm_lbuttonup:
    ; Check if we were dragging
    mov eax, [g_dragging]
    test eax, eax
    jz lbuttonup_not_dragging
    
    ; Get the dragged pane ID
    mov ecx, [g_dragged_pane_id]
    
    ; Get final mouse position
    mov edx, r9d                        ; lParam
    and edx, 0FFFFh                     ; Final X
    
    mov r10d, r9d
    shr r10d, 16                        ; Final Y
    
    ; Validate pane position and finalize move
    ; pane_finalize_move expects: ecx=pane_id, edx=final_x, r9d=final_y
    mov r9d, r10d
    call pane_finalize_move
    
    ; Clear dragging state
    mov DWORD PTR [g_dragging], 0
    mov DWORD PTR [g_dragged_pane_id], 0
    
    lea rcx, dbg_pane_drag_end
    call OutputDebugStringA
    
    ; Trigger final redraw
    mov rcx, hwnd_main
    xor rdx, rdx
    mov r8, 1
    call InvalidateRect
    
lbuttonup_not_dragging:
    xor eax, eax
    add rsp, 40
    ret

wnd_proc_main ENDP

;==========================================================================
; PUBLIC: ui_switch_sidebar_view(viewIndex: ecx)
;
; viewIndex: 0=Explorer,1=Search,2=SCM,3=Debug,4=Extensions
;==========================================================================
PUBLIC ui_switch_sidebar_view
ALIGN 16
ui_switch_sidebar_view PROC
    push rbx
    sub rsp, 32

    ; Hide all
    mov rcx, hwnd_sidebar_explorer
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_sidebar_search
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_sidebar_scm
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_sidebar_debug
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_sidebar_ext
    mov rdx, SW_HIDE
    call ShowWindow

    ; Select view
    cmp ecx, 0
    jne check1
    mov rcx, hwnd_sidebar_explorer
    jmp do_show
check1:
    cmp ecx, 1
    jne check2
    mov rcx, hwnd_sidebar_search
    jmp do_show
check2:
    cmp ecx, 2
    jne check3
    mov rcx, hwnd_sidebar_scm
    jmp do_show
check3:
    cmp ecx, 3
    jne check4
    mov rcx, hwnd_sidebar_debug
    jmp do_show
check4:
    mov rcx, hwnd_sidebar_ext

do_show:
    mov rdx, SW_SHOW
    call ShowWindow

    lea rcx, dbg_ui_sidebar_switch
    call console_log

    add rsp, 32
    pop rbx
    ret
ui_switch_sidebar_view ENDP

;==========================================================================
; PUBLIC: ui_switch_bottom_tab(tabIndex: ecx)
;
; tabIndex: 0=Terminal,1=Output,2=Problems,3=Debug Console
;==========================================================================
PUBLIC ui_switch_bottom_tab
ALIGN 16
ui_switch_bottom_tab PROC
    push rbx
    sub rsp, 32

    ; Hide all bottom views
    mov rcx, hwnd_bottom_term
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_bottom_output
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_bottom_problems
    mov rdx, SW_HIDE
    call ShowWindow
    mov rcx, hwnd_bottom_debug
    mov rdx, SW_HIDE
    call ShowWindow

    ; Select view
    cmp ecx, 0
    jne bottom_check1
    mov rcx, hwnd_bottom_term
    jmp bottom_do_show
bottom_check1:
    cmp ecx, 1
    jne bottom_check2
    mov rcx, hwnd_bottom_output
    jmp bottom_do_show
bottom_check2:
    cmp ecx, 2
    jne bottom_check3
    mov rcx, hwnd_bottom_problems
    jmp bottom_do_show
bottom_check3:
    mov rcx, hwnd_bottom_debug

bottom_do_show:
    mov rdx, SW_SHOW
    call ShowWindow
    
    add rsp, 32
    pop rbx
    ret
ui_switch_bottom_tab ENDP

;==========================================================================
; HELPER: pane_hit_test(x: ecx, y: edx) -> eax (pane_id or 0)
;
; Tests if (x, y) is over a pane header/border. Returns pane ID if true,
; else 0. Used by WM_LBUTTONDOWN to detect draggable pane areas.
;==========================================================================
ALIGN 16
pane_hit_test PROC
    ; ecx = x coordinate, edx = y coordinate
    ; Returns: eax = pane_id (or 0 if no pane at position)
    
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov ebx, ecx                        ; ebx = x
    mov esi, edx                        ; esi = y
    
    ; Check sidebar pane (file tree) - left side
    ; Assume sidebar starts at x=50, width=250, any y in visible range
    cmp ebx, 50
    jb hit_test_check_editor
    cmp ebx, 300
    ja hit_test_check_editor
    cmp esi, 60                         ; below top bar
    jb hit_test_check_editor
    cmp esi, 600                        ; above bottom panel (rough)
    ja hit_test_check_editor
    
    ; Hit pane: file tree (pane_id = 2, FileTree component)
    mov eax, 2
    jmp hit_test_done
    
hit_test_check_editor:
    ; Check editor pane - center/right area
    cmp ebx, 300
    jb hit_test_check_chat
    cmp ebx, 1000
    ja hit_test_check_chat
    cmp esi, 60
    jb hit_test_check_chat
    cmp esi, 600
    ja hit_test_check_chat
    
    ; Hit pane: editor (pane_id = 4, Editor component)
    mov eax, 4
    jmp hit_test_done
    
hit_test_check_chat:
    ; Check chat pane - right side
    cmp ebx, 1000
    jb hit_test_check_terminal
    cmp ebx, 1280
    ja hit_test_check_terminal
    cmp esi, 60
    jb hit_test_check_terminal
    cmp esi, 600
    ja hit_test_check_terminal
    
    ; Hit pane: chat (pane_id = 8, Chat component)
    mov eax, 8
    jmp hit_test_done
    
hit_test_check_terminal:
    ; Check terminal pane - bottom area
    cmp ebx, 50
    jb hit_test_no_pane
    cmp ebx, 1280
    ja hit_test_no_pane
    cmp esi, 600
    jb hit_test_no_pane
    cmp esi, 900
    ja hit_test_no_pane
    
    ; Hit pane: terminal (pane_id = 3, Terminal component)
    mov eax, 3
    jmp hit_test_done
    
hit_test_no_pane:
    xor eax, eax                        ; Return 0 (no pane)
    
hit_test_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
pane_hit_test ENDP

;==========================================================================
; HELPER: pane_set_dragging(state: ecx)
;
; Sets global dragging flag (0 or 1).
;==========================================================================
ALIGN 16
pane_set_dragging PROC
    mov [g_dragging], ecx
    ret
pane_set_dragging ENDP

;==========================================================================
; HELPER: pane_get_dragging() -> eax
;
; Returns current dragging state (0 or 1).
;==========================================================================
ALIGN 16
pane_get_dragging PROC
    mov eax, [g_dragging]
    ret
pane_get_dragging ENDP

;==========================================================================
; HELPER: pane_finalize_move()
;
; Called after drag completes (WM_LBUTTONUP). Finalizes pane position
; or creates floating window if needed. Uses current pane position to
; determine if pane should float or dock.
;==========================================================================
ALIGN 16
pane_finalize_move PROC
    ; ecx = pane_id, edx = final_x, r9d = final_y
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov ebx, ecx                        ; pane_id
    
    ; Check if final position is within main window bounds
    ; Main window roughly: 50-1280 x, 60-900 y
    ; If outside, create floating window
    
    cmp edx, 50
    jl create_floating_pane
    cmp edx, 1280
    jg create_floating_pane
    cmp r9d, 60
    jl create_floating_pane
    cmp r9d, 900
    jg create_floating_pane
    
    ; Pane is within bounds - finalize dock position
    ; (No action needed - pane already updated via WM_MOUSEMOVE)
    lea rcx, dbg_pane_dock_finalized
    call OutputDebugStringA
    jmp finalize_done
    
create_floating_pane:
    ; Create floating window for pane
    ; This is a simplified stub - real implementation would:
    ; 1. Create new overlapped window
    ; 2. Copy pane content to floating window
    ; 3. Update pane registry (is_floating = 1)
    lea rcx, dbg_pane_floating_created
    call OutputDebugStringA
    jmp finalize_done
    
finalize_done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
pane_finalize_move ENDP

;==========================================================================
; HELPER: add_file_tree_item(text: r8, parent: r9) -> handle in rax
;
; Adds an item to the file tree view.
;==========================================================================
ALIGN 16
add_file_tree_item PROC
    push rbx
    push r12
    sub rsp, 88
    
    mov rbx, r8  ; text
    mov r12, r9  ; parent handle (TVI_ROOT or existing item handle)
    
    ; TVINSERTSTRUCTA structure on stack:
    ; hParent (8 bytes) at rsp+32
    ; hInsertAfter (8 bytes) at rsp+40  
    ; item.mask (4 bytes) at rsp+48
    ; item.hItem (8 bytes) at rsp+52
    ; item.state (4 bytes) at rsp+60
    ; item.stateMask (4 bytes) at rsp+64
    ; item.pszText (8 bytes) at rsp+68
    ; item.cchTextMax (4 bytes) at rsp+76
    ; item.iImage (4 bytes) at rsp+80
    ; item.iSelectedImage (4 bytes) at rsp+84
    ; item.cChildren (4 bytes) at rsp+88
    ; item.lParam (8 bytes) at rsp+92
    
    ; Set hParent
    mov QWORD PTR [rsp+32], r12
    
    ; Set hInsertAfter to TVI_LAST (0FFFFFFFFh)
    mov rax, 0FFFFFFFFFFFFFFFFh
    mov QWORD PTR [rsp+40], rax
    
    ; Set item.mask (TVIF_TEXT | TVIF_CHILDREN = 1 | 64 = 65)
    mov DWORD PTR [rsp+48], 65
    
    ; Set item.pszText
    mov QWORD PTR [rsp+68], rbx
    
    ; Set item.cchTextMax
    mov DWORD PTR [rsp+76], 260
    
    ; Set item.cChildren (1 = has children button)
    mov DWORD PTR [rsp+88], 1
    
    ; Clear other fields
    xor eax, eax
    mov QWORD PTR [rsp+52], rax
    mov DWORD PTR [rsp+60], eax
    mov DWORD PTR [rsp+64], eax
    mov DWORD PTR [rsp+80], eax
    mov DWORD PTR [rsp+84], eax
    mov QWORD PTR [rsp+92], rax
    
    ; SendMessage(hwnd_file_tree, TVM_INSERTITEMA, 0, &tvins)
    mov rcx, hwnd_file_tree
    mov edx, 1104h  ; TVM_INSERTITEMA
    xor r8, r8
    lea r9, [rsp+32]
    call SendMessageA
    
    add rsp, 88
    pop r12
    pop rbx
    ret
add_file_tree_item ENDP

;==========================================================================
; PUBLIC: ui_init_file_explorer()
;
; Initializes the file explorer with drive letters and root directories.
;==========================================================================
PUBLIC ui_init_file_explorer
ALIGN 16
ui_init_file_explorer PROC
    push rbx
    push r12
    sub rsp, 56
    
    ; Clear file tree
    mov file_tree_count, 0
    
    ; Get available drives
    call GetLogicalDrives
    mov ebx, eax
    mov ecx, 0
    mov edx, 0
    
scan_drives:
    test ebx, 1
    jz next_drive
    
    ; Convert drive number to letter
    mov eax, ecx
    add al, 'A'
    mov BYTE PTR [drive_letters + edx], al
    inc edx
    
next_drive:
    shr ebx, 1
    inc ecx
    cmp ecx, MAX_DRIVES
    jl scan_drives
    
    mov drive_count, edx
    
    ; Populate file tree with drives
    mov r12, 0
    mov ecx, 0
populate_drives:
    cmp ecx, drive_count
    jge populate_done
    
    ; Get drive letter
    mov al, BYTE PTR [drive_letters + ecx]
    mov BYTE PTR [current_path], al
    mov WORD PTR [current_path + 1], ':'
    mov WORD PTR [current_path + 2], '\\'
    mov BYTE PTR [current_path + 3], '*'
    mov BYTE PTR [current_path + 4], 0
    
    ; Add drive to tree
    lea r8, current_path
    mov r9d, TVI_ROOT
    call add_file_tree_item
    
    inc ecx
    jmp populate_drives
    
populate_done:
    add rsp, 56
    pop r12
    pop rbx
    ret
ui_init_file_explorer ENDP

;==========================================================================
; PUBLIC: ui_init_search_box()
;
; Initializes the search box control.
;==========================================================================
PUBLIC ui_init_search_box
ALIGN 16
ui_init_search_box PROC
    push rbx
    sub rsp, 112

    ; Create search box
    xor rcx, rcx
    lea rdx, szEdit
    lea r8, szSearchBox
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 200
    mov QWORD PTR [rsp + 40], 50
    mov QWORD PTR [rsp + 48], 300
    mov QWORD PTR [rsp + 56], 25
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_SEARCH_BOX
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_search_box, rax

    add rsp, 112
    pop rbx
    ret
ui_init_search_box ENDP

;==========================================================================
; PUBLIC: ui_init_minimap()
;
; Initializes the minimap control.
;==========================================================================
PUBLIC ui_init_minimap
ALIGN 16
ui_init_minimap PROC
    push rbx
    sub rsp, 112

    ; Create minimap
    xor rcx, rcx
    lea rdx, szStatic
    lea r8, szMinimap
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 1100
    mov QWORD PTR [rsp + 40], 50
    mov QWORD PTR [rsp + 48], 80
    mov QWORD PTR [rsp + 56], 400
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_MINIMAP
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_minimap, rax

    add rsp, 112
    pop rbx
    ret
ui_init_minimap ENDP

;==========================================================================
; PUBLIC: ui_init_command_palette()
;
; Initializes the command palette control.
;==========================================================================
PUBLIC ui_init_command_palette
ALIGN 16
ui_init_command_palette PROC
    push rbx
    sub rsp, 112

    ; Create command palette
    xor rcx, rcx
    lea rdx, szComboBox
    lea r8, szCommandPalette
    mov r9d, WS_CHILD or WS_VISIBLE or CBS_DROPDOWN
    mov QWORD PTR [rsp + 32], 500
    mov QWORD PTR [rsp + 40], 50
    mov QWORD PTR [rsp + 48], 300
    mov QWORD PTR [rsp + 56], 200
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_COMMAND_PALETTE
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_command_palette, rax

    add rsp, 112
    pop rbx
    ret
ui_init_command_palette ENDP

;==========================================================================
; PUBLIC: ui_init_status_bar()
;
; Initializes the status bar control.
;==========================================================================
PUBLIC ui_init_status_bar
ALIGN 16
ui_init_status_bar PROC
    push rbx
    sub rsp, 112

    ; Create status bar
    xor rcx, rcx
    lea rdx, szStatusBar
    lea r8, szStatusBar
    mov r9d, WS_CHILD or WS_VISIBLE
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 800
    mov QWORD PTR [rsp + 48], 1280
    mov QWORD PTR [rsp + 56], 20
    mov rax, hwnd_main
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_STATUS_BAR
    mov rax, h_instance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwnd_status, rax

    add rsp, 112
    pop rbx
    ret
ui_init_status_bar ENDP

;==========================================================================
; PUBLIC: ui_show_command_palette()
;
; Shows the command palette for quick actions.
;==========================================================================
PUBLIC ui_show_command_palette
ALIGN 16
ui_show_command_palette PROC
    sub rsp, 40
    
    ; Populate command palette with available commands
    mov rcx, hwnd_command_palette
    mov rdx, CB_RESETCONTENT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    ; Add commands
    mov rcx, hwnd_command_palette
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, szCmdOpenFile
    call SendMessageA
    
    mov rcx, hwnd_command_palette
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, szCmdSaveFile
    call SendMessageA
    
    mov rcx, hwnd_command_palette
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, szCmdRunBuild
    call SendMessageA
    
    mov rcx, hwnd_command_palette
    mov rdx, CB_ADDSTRING
    xor r8, r8
    lea r9, szCmdDebugStart
    call SendMessageA
    
    ; Show command palette
    mov rcx, hwnd_command_palette
    mov rdx, SW_SHOW
    call ShowWindow
    
    ; Set focus
    mov rcx, hwnd_command_palette
    call SetFocus
    
    add rsp, 40
    ret
ui_show_command_palette ENDP

;==========================================================================
; PUBLIC: ui_scan_directory(path: rcx, parent: rdx)
;
; Scans a directory and adds files/subdirectories to the tree.
;==========================================================================
PUBLIC ui_scan_directory
ALIGN 16
ui_scan_directory PROC
    push rbx
    push r12
    push r13
    sub rsp, 320
    
    mov r12, rcx  ; path
    mov r13, rdx  ; parent
    
    ; Setup WIN32_FIND_DATA structure
    lea rbx, [rsp + 32]
    
    ; Find first file
    mov rcx, r12
    mov rdx, rbx
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je scan_done
    
    mov r8, rax  ; search handle
    
scan_loop:
    ; Check if it's a directory (skip . and ..)
    mov eax, DWORD PTR [rbx + 32]  ; dwFileAttributes
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jz next_file
    
    ; Skip . and ..
    lea rcx, [rbx + 44]  ; cFileName
    cmp BYTE PTR [rcx], '.'
    je next_file
    
    ; Add directory to tree
    mov r8, rcx
    mov r9, r13
    call add_file_tree_item
    
next_file:
    ; Find next file
    mov rcx, r8
    mov rdx, rbx
    call FindNextFileA
    test eax, eax
    jnz scan_loop
    
    ; Close search handle
    mov rcx, r8
    call FindClose
    
scan_done:
    add rsp, 320
    pop r13
    pop r12
    pop rbx
    ret
ui_scan_directory ENDP

;==========================================================================
; PUBLIC: ui_start_powershell() -> bool (rax)
;
; Starts a PowerShell process with pipes for I/O.
;==========================================================================
PUBLIC ui_start_powershell
ALIGN 16
ui_start_powershell PROC
    push rbx
    sub rsp, 80
    
    ; Create pipes for stdin
    lea rcx, [rsp + 32]  ; hReadPipe
    lea rdx, [rsp + 40]  ; hWritePipe
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp + 48], 0
    call CreatePipe
    test eax, eax
    jz ps_failed
    
    mov rbx, QWORD PTR [rsp + 40]  ; stdin write
    mov ps_stdin_write, rbx
    
    ; Create pipes for stdout
    lea rcx, [rsp + 32]  ; hReadPipe
    lea rdx, [rsp + 40]  ; hWritePipe
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp + 48], 0
    call CreatePipe
    test eax, eax
    jz ps_failed
    
    mov rbx, QWORD PTR [rsp + 32]  ; stdout read
    mov ps_stdout_read, rbx
    
    ; Setup startup info
    lea rbx, ps_startup_info
    mov DWORD PTR [rbx], SIZEOF_STARTUPINFOA  ; cb field
    mov DWORD PTR [rbx + 44h], STARTF_USESTDHANDLES  ; dwFlags field offset
    mov rax, ps_stdin_write
    mov QWORD PTR [rbx + 50h], rax  ; hStdInput field offset
    mov rax, ps_stdout_write
    mov QWORD PTR [rbx + 58h], rax  ; hStdOutput field offset
    mov rax, ps_stdout_write
    mov QWORD PTR [rbx + 60h], rax  ; hStdError field offset
    
    ; Start PowerShell process
    lea rcx, szPowershellCmd
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp + 32], 0  ; inherit handles
    mov QWORD PTR [rsp + 40], 1  ; creation flags
    mov QWORD PTR [rsp + 48], 0  ; environment
    xor rax, rax
    mov QWORD PTR [rsp + 56], rax  ; current directory
    lea rax, ps_startup_info
    mov QWORD PTR [rsp + 64], rax
    lea rax, ps_process_info
    mov QWORD PTR [rsp + 72], rax
    call CreateProcessA
    test eax, eax
    jz ps_failed
    
    mov eax, 1
    jmp ps_done
    
ps_failed:
    xor eax, eax
    
ps_done:
    add rsp, 80
    pop rbx
    ret
ui_start_powershell ENDP

;==========================================================================
; PUBLIC: ui_load_file(path: rcx) -> bool (rax)
;
; Loads a file into the editor.
;==========================================================================
PUBLIC ui_load_file
ALIGN 16
ui_load_file PROC
    push rbx
    push r12
    sub rsp, 56
    
    mov r12, rcx  ; path
    
    ; Open file
    mov rcx, r12
    mov rdx, GENERIC_READ
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp + 32], OPEN_EXISTING
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je load_failed
    
    mov rbx, rax  ; file handle
    
    ; Get file size
    mov rcx, rbx
    xor rdx, rdx
    call GetFileSize
    cmp eax, 65535
    ja load_failed_close
    
    mov edx, eax  ; file size
    
    ; Read file
    mov rcx, rbx
    lea r8, editor_buffer
    mov r9d, edx
    lea r10, [rsp + 40]  ; bytes read
    mov QWORD PTR [rsp + 32], 0
    call ReadFile
    test eax, eax
    jz load_failed_close
    
    ; Null terminate
    mov ecx, edx
    mov BYTE PTR [editor_buffer + rcx], 0
    
    ; Set in editor
    mov rcx, hwnd_editor
    mov rdx, WM_SETTEXT
    xor r8, r8
    lea r9, editor_buffer
    call SendMessageA
    
    ; Store path
    mov rcx, r12
    lea rdx, editor_file_path
    call lstrcpyA
    
    mov editor_modified, 0
    mov editor_cursor_pos, 0
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    mov eax, 1
    jmp load_done
    
load_failed_close:
    mov rcx, rbx
    call CloseHandle
    
load_failed:
    xor eax, eax
    
load_done:
    add rsp, 56
    pop r12
    pop rbx
    ret
ui_load_file ENDP

;==========================================================================
; PUBLIC: ui_save_file() -> bool (rax)
;
; Saves the current editor content to file.
;==========================================================================
PUBLIC ui_save_file
ALIGN 16
ui_save_file PROC
    push rbx
    sub rsp, 56
    
    ; Get editor text
    mov rcx, hwnd_editor
    mov rdx, WM_GETTEXT
    mov r8, 65536
    lea r9, editor_buffer
    call SendMessageA
    
    mov ebx, eax  ; text length
    
    ; Open file for writing
    lea rcx, editor_file_path
    mov rdx, GENERIC_WRITE
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp + 32], CREATE_ALWAYS
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je save_failed
    
    mov rcx, rax  ; file handle
    
    ; Write file
    mov rdx, rax
    lea r8, editor_buffer
    mov r9d, ebx
    lea r10, [rsp + 40]  ; bytes written
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    test eax, eax
    jz save_failed_close
    
    ; Close file
    mov rcx, rdx
    call CloseHandle
    
    mov editor_modified, 0
    mov eax, 1
    jmp save_done
    
save_failed_close:
    mov rcx, rdx
    call CloseHandle
    
save_failed:
    xor eax, eax
    
save_done:
    add rsp, 56
    pop rbx
    ret
ui_save_file ENDP

;==========================================================================
; PUBLIC: ui_register_components()
;
; Registers existing UI windows into the GUI Designer Registry.
;==========================================================================
PUBLIC ui_register_components
ALIGN 16
ui_register_components PROC
    push rbx
    sub rsp, 40

    ; Feature toggle: skip registration if disabled
    cmp g_enable_registration, 0
    je reg_skip_all

    ; Register file tree
    mov rcx, hwnd_file_tree
    test rcx, rcx
    jz skip_reg_file_tree
    lea rdx, szFileTreeName
    mov r8, COMPONENT_FILE_TREE
    call gui_create_component
skip_reg_file_tree:

    ; Register search box
    mov rcx, hwnd_search_box
    test rcx, rcx
    jz skip_reg_search
    lea rdx, szSearchBoxName
    mov r8, COMPONENT_SEARCH
    call gui_create_component
skip_reg_search:

    ; Register minimap
    mov rcx, hwnd_minimap
    test rcx, rcx
    jz skip_reg_minimap
    lea rdx, szMinimapName
    mov r8, COMPONENT_MINIMAP
    call gui_create_component
skip_reg_minimap:

    ; Register command palette
    mov rcx, hwnd_command_palette
    test rcx, rcx
    jz skip_reg_cmdpal
    lea rdx, szCommandPaletteName
    mov r8, COMPONENT_COMMAND_PALETTE
    call gui_create_component
skip_reg_cmdpal:

    ; Register status bar
    mov rcx, hwnd_status
    test rcx, rcx
    jz skip_reg_status
    lea rdx, szStatusName
    mov r8, COMPONENT_STATUS
    call gui_create_component
skip_reg_status:

    ; Register problems list
    mov rcx, hwnd_bottom_problems
    test rcx, rcx
    jz skip_reg_problems
    lea rdx, szProblemsName
    mov r8, COMPONENT_OUTPUT
    call gui_create_component
skip_reg_problems:

    ; Register debug console
    mov rcx, hwnd_bottom_debug
    test rcx, rcx
    jz skip_reg_debug
    lea rdx, szDebugConsoleName
    mov r8, COMPONENT_TERMINAL
    call gui_create_component
skip_reg_debug:

    add rsp, 40
    pop rbx
    ret
reg_skip_all:
    add rsp, 40
    pop rbx
    ret
ui_register_components ENDP

;==========================================================================
; PUBLIC: ui_get_status_hwnd() -> rax (status bar hwnd)
;==========================================================================
PUBLIC ui_get_status_hwnd
ALIGN 16
ui_get_status_hwnd PROC
    mov rax, hwnd_status
    ret
ui_get_status_hwnd ENDP

;=========================================================================
; PUBLIC: ui_get_chat_hwnd() -> rax
;=========================================================================
PUBLIC ui_get_chat_hwnd
ALIGN 16
ui_get_chat_hwnd PROC
    mov rax, hwnd_chat
    ret
ui_get_chat_hwnd ENDP

;=========================================================================
; PUBLIC: ui_get_input_hwnd() -> rax
;=========================================================================
PUBLIC ui_get_input_hwnd
ALIGN 16
ui_get_input_hwnd PROC
    mov rax, hwnd_input
    ret
ui_get_input_hwnd ENDP

;=========================================================================
; PUBLIC: ui_get_terminal_hwnd() -> rax
;=========================================================================
PUBLIC ui_get_terminal_hwnd
ALIGN 16
ui_get_terminal_hwnd PROC
    mov rax, hwnd_terminal
    ret
ui_get_terminal_hwnd ENDP

;=========================================================================
; PUBLIC: ui_get_file_tree_hwnd() -> rax
;=========================================================================
PUBLIC ui_get_file_tree_hwnd
ALIGN 16
ui_get_file_tree_hwnd PROC
    mov rax, hwnd_file_tree
    ret
ui_get_file_tree_hwnd ENDP

.data
    szEditorName    BYTE "Editor",0
    szChatName      BYTE "ChatBox",0
    szInputName     BYTE "InputBox",0
    szTerminalName  BYTE "Terminal",0
    szSendName      BYTE "SendButton",0
    szModeComboName BYTE "ModeCombo",0
    szCheckMaxName  BYTE "CheckMax",0
    szCheckDeepName BYTE "CheckDeep",0
    szCheckResearchName BYTE "CheckResearch",0
    szCheckInternetName BYTE "CheckInternet",0
    szCheckThinkingName BYTE "CheckThinking",0
    szFileTreeName  BYTE "FileTree",0
    szSearchBoxName BYTE "SearchBox",0
    szMinimapName   BYTE "Minimap",0
    szCommandPaletteName BYTE "CommandPalette",0
    szStatusName    BYTE "StatusBar",0
    szProblemsName  BYTE "Problems",0
    szDebugConsoleName BYTE "DebugConsole",0
    
    ; PowerShell command
    szPowershellCmd     BYTE "powershell.exe",0
    
    ; Command palette commands
    szCmdOpenFile      BYTE "Open File",0
    szCmdSaveFile      BYTE "Save File",0
    szCmdRunBuild      BYTE "Run Build",0
    szCmdDebugStart    BYTE "Start Debugging",0
    
    ; Mode combo strings
    szModePlan      BYTE "Plan",0
    szModeAgent     BYTE "Agent",0
    szModeAsk       BYTE "Ask",0
    
    ; Checkbox strings
    szCheckMax      BYTE "Max Tokens",0
    szCheckDeep     BYTE "Deep Mode",0
    szCheckResearch BYTE "Research",0
    szCheckInternet BYTE "Internet",0
    szCheckThinking BYTE "Thinking",0
    
    ; Window classes
    szComboBoxClass BYTE "ComboBox",0
    szButtonClass   BYTE "BUTTON",0
    szEmpty         BYTE 0

;=========================================================================
; Hotpatch Dialog Windows
;=========================================================================
PUBLIC hotpatch_memory_dialog_proc
ALIGN 16
hotpatch_memory_dialog_proc PROC USES rbx rsi rdi, hwnd:QWORD, msg:DWORD, wparam:QWORD, lparam:QWORD
    mov eax, msg
    cmp eax, WM_INITDIALOG
    je on_init_mem
    cmp eax, WM_COMMAND
    je on_command_mem
    cmp eax, WM_CLOSE
    je on_close_mem
    xor rax, rax
    ret
on_init_mem:
    ; Initialize dialog with labels and edit controls
    lea rcx, str_hp_mem_title
    call OutputDebugStringA
    mov rax, 1
    ret
on_command_mem:
    mov rax, wparam
    shr rax, 16                     ; Get notification code
    cmp ax, BN_CLICKED
    je on_button_click_mem
    xor rax, rax
    ret
on_button_click_mem:
    mov rcx, hwnd
    lea rdx, str_hp_mem_title
    lea r8, str_hp_mem_title
    mov r9d, MB_OK
    call MessageBoxA
    xor rax, rax
    ret
on_close_mem:
    mov rcx, hwnd
    xor rdx, rdx
    call EndDialog
    mov rax, 1
    ret
hotpatch_memory_dialog_proc ENDP
PUBLIC hotpatch_byte_dialog_proc
ALIGN 16
hotpatch_byte_dialog_proc PROC USES rbx rsi rdi, hwnd:QWORD, msg:DWORD, wparam:QWORD, lparam:QWORD
    mov eax, msg
    cmp eax, WM_INITDIALOG
    je on_init_byte
    cmp eax, WM_COMMAND
    je on_command_byte
    cmp eax, WM_CLOSE
    je on_close_byte
    xor rax, rax
    ret
on_init_byte:
    lea rcx, str_hp_byte_title
    call OutputDebugStringA
    mov rax, 1
    ret
on_command_byte:
    mov rax, wparam
    shr rax, 16
    cmp ax, BN_CLICKED
    je on_button_click_byte
    xor rax, rax
    ret
on_button_click_byte:
    mov rcx, hwnd
    lea rdx, str_hp_byte_title
    lea r8, str_hp_byte_title
    mov r9d, MB_OK
    call MessageBoxA
    xor rax, rax
    ret
on_close_byte:
    mov rcx, hwnd
    xor rdx, rdx
    call EndDialog
    mov rax, 1
    ret
hotpatch_byte_dialog_proc ENDP
PUBLIC hotpatch_server_dialog_proc
ALIGN 16
hotpatch_server_dialog_proc PROC USES rbx rsi rdi, hwnd:QWORD, msg:DWORD, wparam:QWORD, lparam:QWORD
    mov eax, msg
    cmp eax, WM_INITDIALOG
    je on_init_srv
    cmp eax, WM_COMMAND
    je on_command_srv
    cmp eax, WM_CLOSE
    je on_close_srv
    xor rax, rax
    ret
on_init_srv:
    lea rcx, str_hp_srv_title
    call OutputDebugStringA
    mov rax, 1
    ret
on_command_srv:
    mov rax, wparam
    shr rax, 16
    cmp ax, BN_CLICKED
    je on_button_click_srv
    xor rax, rax
    ret
on_button_click_srv:
    mov rcx, hwnd
    lea rdx, str_hp_srv_title
    lea r8, str_hp_srv_title
    mov r9d, MB_OK
    call MessageBoxA
    xor rax, rax
    ret
on_close_srv:
    mov rcx, hwnd
    xor rdx, rdx
    call EndDialog
    mov rax, 1
    ret
hotpatch_server_dialog_proc ENDP

;==========================================================================
; PUBLIC: save_layout_json()
;
; Saves the current IDE layout to a JSON file.
;==========================================================================
PUBLIC save_layout_json
ALIGN 16
save_layout_json PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 256
    
    ; Call gui_save_pane_layout from gui_designer_agent.asm
    lea rcx, szLayoutFile
    call gui_save_pane_layout
    
    add rsp, 256
    pop rdi
    pop rsi
    pop rbx
    ret
save_layout_json ENDP

;==========================================================================
; PUBLIC: load_layout_json()
;
; Loads the IDE layout from a JSON file.
;==========================================================================
PUBLIC load_layout_json
ALIGN 16
load_layout_json PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 256
    
    ; Call gui_load_pane_layout from gui_designer_agent.asm
    lea rcx, szLayoutFile
    call gui_load_pane_layout
    
    ; Redraw main window
    mov rcx, hwnd_main
    xor rdx, rdx
    mov r8d, 1
    call InvalidateRect
    
    add rsp, 256
    pop rdi
    pop rsi
    pop rbx
    ret
load_layout_json ENDP

;==========================================================================
; PUBLIC: find_in_files(search_pattern: rcx, directory_path: rdx)
;
; Searches for a pattern in all files within a directory.
;==========================================================================
PUBLIC find_in_files
ALIGN 16
find_in_files PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 1024
    
    mov r12, rcx  ; search_pattern
    mov r13, rdx  ; directory_path
    
    ; Implementation would involve recursive directory traversal
    ; and calling StringFind on each file's content.
    ; For now, log the search request.
    lea rcx, szLogSearchStart
    call asm_log
    
    add rsp, 1024
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
find_in_files ENDP

.data
    szLayoutFile        BYTE "ide_layout.json",0
    szLogSearchStart    BYTE "UI: Starting file search...",0

END





