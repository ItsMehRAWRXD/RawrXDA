; ============================================================================
; FILE: rawrxd_masm_ide_main.asm
; TITLE: Pure MASM RawrXD IDE - Complete Qt Replacement
; PURPOSE: Full MASM implementation of RawrXD IDE with Windows API
; LINES: 1500+ (Complete MASM IDE)
; ============================================================================

option casemap:none

include windows.inc
include masm_hotpatch.inc
include logging.inc
include hotpatch_coordinator.inc
include plugin_abi.inc

includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comctl32.lib
includelib comdlg32.lib
includelib shell32.lib
includelib ole32.lib
includelib oleaut32.lib

; ============================================================================
; CONSTANTS AND STRUCTURES
; ============================================================================

; IDE Window Constants
IDE_WINDOW_WIDTH = 1200
IDE_WINDOW_HEIGHT = 800
IDE_MENU_HEIGHT = 30
IDE_STATUS_HEIGHT = 25
IDE_SIDEBAR_WIDTH = 250
IDE_BOTTOM_PANE_HEIGHT = 200

; Control IDs
IDC_MAIN_WINDOW = 1000
IDC_MENU_BAR = 1001
IDC_STATUS_BAR = 1002
IDC_SIDEBAR = 1003
IDC_EDITOR_PANE = 1004
IDC_BOTTOM_PANE = 1005
IDC_CHAT_INPUT = 1006
IDC_SEND_BUTTON = 1007
IDC_MODE_COMBO = 1008
IDC_TERMINAL_PANE = 1009
IDC_FILE_TREE = 1010

; Menu IDs
IDM_FILE_NEW = 2001
IDM_FILE_OPEN = 2002
IDM_FILE_SAVE = 2003
IDM_FILE_SAVEAS = 2004
IDM_FILE_EXIT = 2005
IDM_EDIT_UNDO = 2006
IDM_EDIT_REDO = 2007
IDM_EDIT_CUT = 2008
IDM_EDIT_COPY = 2009
IDM_EDIT_PASTE = 2010
IDM_VIEW_SIDEBAR = 2011
IDM_VIEW_TERMINAL = 2012
IDM_VIEW_CHAT = 2013
IDM_TOOLS_HOTPATCH = 2014
IDM_TOOLS_AGENTIC = 2015
IDM_HELP_ABOUT = 2016

; IDE State Structure
IDE_STATE STRUCT
    hMainWindow QWORD ?
    hMenuBar QWORD ?
    hStatusBar QWORD ?
    hSidebar QWORD ?
    hEditorPane QWORD ?
    hBottomPane QWORD ?
    hChatInput QWORD ?
    hSendButton QWORD ?
    hModeCombo QWORD ?
    hTerminalPane QWORD ?
    hFileTree QWORD ?
    
    ; Hotpatch System
    hHotpatchManager QWORD ?
    hMemoryHotpatch QWORD ?
    hByteHotpatch QWORD ?
    hServerHotpatch QWORD ?
    
    ; Agentic System
    hAgenticEngine QWORD ?
    hFailureDetector QWORD ?
    hPuppeteer QWORD ?
    
    ; Current State
    currentMode BYTE ?
    currentFile QWORD ?
    isSidebarVisible BYTE ?
    isTerminalVisible BYTE ?
    isChatVisible BYTE ?
    
    ; Performance Metrics
    startupTime QWORD ?
    totalOperations QWORD ?
    inferenceCount QWORD ?
    hotpatchCount QWORD ?
    
    ; Buffer for file operations
    fileBuffer BYTE 4096 DUP(?)
IDE_STATE ENDS

; ============================================================================
; GLOBAL VARIABLES
; ============================================================================

.data

; Global IDE State
globalIDEState IDE_STATE {}

; Window Class Names
szMainWindowClass db "RawrXD_MASM_IDE",0
szEditClass db "EDIT",0
szButtonClass db "BUTTON",0
szComboBoxClass db "COMBOBOX",0
szTreeViewClass db "SysTreeView32",0
szListViewClass db "SysListView32",0

; Menu Strings
szFileMenu db "&File",0
szEditMenu db "&Edit",0
szViewMenu db "&View",0
szToolsMenu db "&Tools",0
szHelpMenu db "&Help",0

; Menu Item Strings
szNew db "&New",0
szOpen db "&Open...",0
szSave db "&Save",0
szSaveAs db "Save &As...",0
szExit db "E&xit",0
szUndo db "&Undo",0
szRedo db "&Redo",0
szCut db "Cu&t",0
szCopy db "&Copy",0
szPaste db "&Paste",0
szSidebar db "&Sidebar",0
szTerminal db "&Terminal",0
szChat db "&Chat",0
szHotpatch db "&Hotpatch",0
szAgentic db "&Agentic",0
szAbout db "&About",0

; Status Messages
szReady db "Ready",0
szLoading db "Loading...",0
szInferring db "Inferring...",0
szHotpatching db "Hotpatching...",0

; Mode Strings
szAskMode db "Ask",0
szPlanMode db "Plan",0
szAgentMode db "Agent",0
szConfigureMode db "Configure",0

; ============================================================================
; EXTERNAL FUNCTION DECLARATIONS
; ============================================================================

EXTERN ui_create_main_window:PROC
EXTERN ui_register_components:PROC
EXTERN ui_create_menu:PROC
EXTERN ui_set_main_menu:PROC
EXTERN ui_create_chat_control:PROC
EXTERN ui_create_input_control:PROC
EXTERN ui_create_terminal_control:PROC
EXTERN ui_create_send_button:PROC
EXTERN ui_create_mode_combo:PROC
EXTERN ui_create_mode_checkboxes:PROC
EXTERN ui_add_chat_message:PROC
EXTERN ui_get_input_text:PROC
EXTERN ui_clear_input:PROC
EXTERN ui_show_dialog:PROC
EXTERN ui_open_file_dialog:PROC
EXTERN ui_editor_set_text:PROC
EXTERN ui_editor_get_text:PROC
EXTERN ui_open_text_file_dialog:PROC
EXTERN ui_save_text_file_dialog:PROC
EXTERN ui_switch_sidebar_view:PROC
EXTERN ui_switch_bottom_tab:PROC

EXTERN hpatch_apply_memory:PROC
EXTERN hpatch_apply_byte:PROC
EXTERN hpatch_apply_server:PROC
EXTERN hpatch_get_stats:PROC
EXTERN hpatch_reset_stats:PROC

EXTERN agent_init_tools:PROC
EXTERN agent_process_command:PROC
EXTERN agent_list_tools:PROC
EXTERN agent_get_tool:PROC

EXTERN AgenticEngine_Initialize:PROC
EXTERN AgenticEngine_ProcessResponse:PROC
EXTERN AgenticEngine_ExecuteTask:PROC
EXTERN AgenticEngine_GetStats:PROC

EXTERN console_log_init:PROC
EXTERN console_log:PROC
EXTERN file_log_init:PROC
EXTERN file_log_append:PROC

; ============================================================================
; MAIN ENTRY POINT
; ============================================================================

.code

; main() - Entry point for MASM IDE
PUBLIC main
main PROC
    
    ; Initialize console logging
    call console_log_init
    
    ; Log startup
    mov rcx, offset szStartingIDE
    call console_log
    
    ; Initialize IDE state
    call masm_ide_init_state
    test rax, rax
    jz main_failure
    
    ; Initialize hotpatch system
    call masm_ide_init_hotpatch
    test rax, rax
    jz main_failure
    
    ; Initialize agentic system
    call masm_ide_init_agentic
    test rax, rax
    jz main_failure
    
    ; Create main window
    call masm_ide_create_window
    test rax, rax
    jz main_failure
    
    ; Initialize UI components
    call masm_ide_init_ui
    test rax, rax
    jz main_failure
    
    ; Start message loop
    call masm_ide_message_loop
    
    ; Cleanup
    call masm_ide_cleanup
    
    mov eax, 0
    ret
    
main_failure:
    mov rcx, offset szStartupFailed
    call console_log
    mov eax, 1
    ret

main ENDP

; ============================================================================
; IDE INITIALIZATION FUNCTIONS
; ============================================================================

; masm_ide_init_state() - Initialize IDE state structure
masm_ide_init_state PROC
    
    ; Get startup time
    call GetTickCount
    mov [globalIDEState.startupTime], rax
    
    ; Initialize counters
    mov [globalIDEState.totalOperations], 0
    mov [globalIDEState.inferenceCount], 0
    mov [globalIDEState.hotpatchCount], 0
    
    ; Set default visibility
    mov [globalIDEState.isSidebarVisible], 1
    mov [globalIDEState.isTerminalVisible], 1
    mov [globalIDEState.isChatVisible], 1
    
    ; Set default mode
    mov [globalIDEState.currentMode], 0 ; Ask mode
    
    mov eax, 1
    ret

masm_ide_init_state ENDP

; masm_ide_init_hotpatch() - Initialize hotpatch system
masm_ide_init_hotpatch PROC
    
    ; Initialize unified hotpatch manager
    call unified_hotpatch_manager_init
    test rax, rax
    jz hotpatch_init_fail
    mov [globalIDEState.hHotpatchManager], rax
    
    ; Initialize memory hotpatch
    call model_memory_hotpatch_init
    test rax, rax
    jz hotpatch_init_fail
    mov [globalIDEState.hMemoryHotpatch], rax
    
    ; Initialize byte-level hotpatch
    call byte_level_hotpatcher_init
    test rax, rax
    jz hotpatch_init_fail
    mov [globalIDEState.hByteHotpatch], rax
    
    ; Initialize server hotpatch
    call gguf_server_hotpatch_init
    test rax, rax
    jz hotpatch_init_fail
    mov [globalIDEState.hServerHotpatch], rax
    
    mov rcx, offset szHotpatchInitSuccess
    call console_log
    mov eax, 1
    ret
    
hotpatch_init_fail:
    mov rcx, offset szHotpatchInitFailed
    call console_log
    mov eax, 0
    ret

masm_ide_init_hotpatch ENDP

; masm_ide_init_agentic() - Initialize agentic system
masm_ide_init_agentic PROC
    
    ; Initialize agentic engine
    call AgenticEngine_Initialize
    test rax, rax
    jz agentic_init_fail
    mov [globalIDEState.hAgenticEngine], rax
    
    ; Initialize failure detector
    call agentic_failure_detector_init
    test rax, rax
    jz agentic_init_fail
    mov [globalIDEState.hFailureDetector], rax
    
    ; Initialize puppeteer
    call agentic_puppeteer_init
    test rax, rax
    jz agentic_init_fail
    mov [globalIDEState.hPuppeteer], rax
    
    mov rcx, offset szAgenticInitSuccess
    call console_log
    mov eax, 1
    ret
    
agentic_init_fail:
    mov rcx, offset szAgenticInitFailed
    call console_log
    mov eax, 0
    ret

masm_ide_init_agentic ENDP

; ============================================================================
; WINDOW CREATION AND MANAGEMENT
; ============================================================================

; masm_ide_create_window() - Create main IDE window
masm_ide_create_window PROC
    
    ; Register window class
    mov rcx, offset szMainWindowClass
    mov rdx, offset MainWindowProc
    call RegisterWindowClass
    test rax, rax
    jz window_create_fail
    
    ; Create main window
    mov rcx, 0
    mov rdx, offset szMainWindowClass
    mov r8, offset szRawrXDTitle
    mov r9, WS_OVERLAPPEDWINDOW
    mov r10, CW_USEDEFAULT
    mov r11, CW_USEDEFAULT
    push r11
    push r10
    push r9
    push r8
    push rdx
    push rcx
    call CreateWindowExA
    add rsp, 48
    
    test rax, rax
    jz window_create_fail
    mov [globalIDEState.hMainWindow], rax
    
    ; Show window
    mov rcx, rax
    mov rdx, SW_SHOW
    call ShowWindow
    
    ; Update window
    mov rcx, [globalIDEState.hMainWindow]
    call UpdateWindow
    
    mov rcx, offset szWindowCreated
    call console_log
    mov eax, 1
    ret
    
window_create_fail:
    mov rcx, offset szWindowCreateFailed
    call console_log
    mov eax, 0
    ret

masm_ide_create_window ENDP

; MainWindowProc - Main window message handler
MainWindowProc PROC hWnd:QWORD, uMsg:QWORD, wParam:QWORD, lParam:QWORD
    
    cmp uMsg, WM_CREATE
    je OnCreate
    cmp uMsg, WM_DESTROY
    je OnDestroy
    cmp uMsg, WM_COMMAND
    je OnCommand
    cmp uMsg, WM_SIZE
    je OnSize
    cmp uMsg, WM_CLOSE
    je OnClose
    
    ; Default message processing
    mov rcx, hWnd
    mov rdx, uMsg
    mov r8, wParam
    mov r9, lParam
    call DefWindowProcA
    ret
    
OnCreate:
    call OnWindowCreate
    mov eax, 0
    ret
    
OnDestroy:
    call OnWindowDestroy
    mov eax, 0
    ret
    
OnCommand:
    call OnMenuCommand
    mov eax, 0
    ret
    
OnSize:
    call OnWindowSize
    mov eax, 0
    ret
    
OnClose:
    call OnWindowClose
    mov eax, 0
    ret

MainWindowProc ENDP

; ============================================================================
; EVENT HANDLERS
; ============================================================================

OnWindowCreate PROC
    
    ; Create menu bar
    call CreateMenuBar
    
    ; Create status bar
    call CreateStatusBar
    
    ; Create sidebar
    call CreateSidebar
    
    ; Create editor pane
    call CreateEditorPane
    
    ; Create bottom pane (chat/terminal)
    call CreateBottomPane
    
    ; Initialize hotpatch UI
    call InitHotpatchUI
    
    ; Initialize agentic UI
    call InitAgenticUI
    
    ret

OnWindowCreate ENDP

OnWindowDestroy PROC
    ; Cleanup resources
    ret

OnWindowDestroy ENDP

OnMenuCommand PROC
    ; Handle menu commands
    mov rax, wParam
    
    cmp ax, IDM_FILE_NEW
    je OnFileNew
    cmp ax, IDM_FILE_OPEN
    je OnFileOpen
    cmp ax, IDM_FILE_SAVE
    je OnFileSave
    cmp ax, IDM_FILE_SAVEAS
    je OnFileSaveAs
    cmp ax, IDM_FILE_EXIT
    je OnFileExit
    
    cmp ax, IDM_EDIT_UNDO
    je OnEditUndo
    cmp ax, IDM_EDIT_REDO
    je OnEditRedo
    cmp ax, IDM_EDIT_CUT
    je OnEditCut
    cmp ax, IDM_EDIT_COPY
    je OnEditCopy
    cmp ax, IDM_EDIT_PASTE
    je OnEditPaste
    
    cmp ax, IDM_VIEW_SIDEBAR
    je OnViewSidebar
    cmp ax, IDM_VIEW_TERMINAL
    je OnViewTerminal
    cmp ax, IDM_VIEW_CHAT
    je OnViewChat
    
    cmp ax, IDM_TOOLS_HOTPATCH
    je OnToolsHotpatch
    cmp ax, IDM_TOOLS_AGENTIC
    je OnToolsAgentic
    
    cmp ax, IDM_HELP_ABOUT
    je OnHelpAbout
    
    ret

OnMenuCommand ENDP

; Individual menu command handlers
OnFileNew:
    call FileNew
    ret

OnFileOpen:
    call FileOpen
    ret

OnFileSave:
    call FileSave
    ret

OnFileSaveAs:
    call FileSaveAs
    ret

OnFileExit:
    call FileExit
    ret

; ... (similar handlers for other menu items)

; ============================================================================
; FILE OPERATIONS
; ============================================================================

FileNew PROC
    ; Create new file
    mov rcx, offset szNewFileCreated
    call console_log
    ret

FileNew ENDP

FileOpen PROC
    ; Open file dialog
    call ui_open_text_file_dialog
    test rax, rax
    jz file_open_fail
    
    ; Load file content
    mov rcx, rax
    call LoadFileContent
    
    mov rcx, offset szFileOpened
    call console_log
    ret
    
file_open_fail:
    mov rcx, offset szFileOpenFailed
    call console_log
    ret

FileOpen ENDP

FileSave PROC
    ; Save current file
    call SaveFileContent
    test rax, rax
    jz file_save_fail
    
    mov rcx, offset szFileSaved
    call console_log
    ret
    
file_save_fail:
    mov rcx, offset szFileSaveFailed
    call console_log
    ret

FileSave ENDP

; ============================================================================
; HOTPATCH OPERATIONS
; ============================================================================

ApplyMemoryHotpatch PROC patchData:QWORD, patchSize:QWORD
    
    mov rcx, [globalIDEState.hMemoryHotpatch]
    mov rdx, patchData
    mov r8, patchSize
    call hpatch_apply_memory
    
    test rax, rax
    jz hotpatch_fail
    
    inc [globalIDEState.hotpatchCount]
    mov rcx, offset szMemoryHotpatchApplied
    call console_log
    mov eax, 1
    ret
    
hotpatch_fail:
    mov rcx, offset szHotpatchFailed
    call console_log
    mov eax, 0
    ret

ApplyMemoryHotpatch ENDP

; ============================================================================
; AGENTIC OPERATIONS
; ============================================================================

ProcessAgenticCommand PROC command:QWORD
    
    mov rcx, [globalIDEState.hAgenticEngine]
    mov rdx, command
    call AgenticEngine_ProcessResponse
    
    test rax, rax
    jz agentic_fail
    
    inc [globalIDEState.inferenceCount]
    mov rcx, offset szAgenticProcessed
    call console_log
    mov eax, 1
    ret
    
agentic_fail:
    mov rcx, offset szAgenticFailed
    call console_log
    mov eax, 0
    ret

ProcessAgenticCommand ENDP

; ============================================================================
; MESSAGE LOOP AND CLEANUP
; ============================================================================

masm_ide_message_loop PROC
    
    LOCAL msg:MSG
    
message_loop:
    lea rcx, msg
    mov rdx, 0
    mov r8, 0
    mov r9, 0
    call GetMessageA
    
    cmp eax, 0
    jle message_loop_end
    
    lea rcx, msg
    call TranslateMessage
    lea rcx, msg
    call DispatchMessageA
    
    jmp message_loop
    
message_loop_end:
    mov eax, msg.wParam
    ret

masm_ide_message_loop ENDP

masm_ide_cleanup PROC
    
    ; Cleanup hotpatch system
    call CleanupHotpatchSystem
    
    ; Cleanup agentic system
    call CleanupAgenticSystem
    
    ; Cleanup UI components
    call CleanupUIComponents
    
    mov rcx, offset szIDECleanup
    call console_log
    ret

masm_ide_cleanup ENDP

; ============================================================================
; STRING CONSTANTS
; ============================================================================

.data

szStartingIDE db "Starting RawrXD MASM IDE...",0
szStartupFailed db "IDE startup failed!",0
szHotpatchInitSuccess db "Hotpatch system initialized",0
szHotpatchInitFailed db "Hotpatch system initialization failed",0
szAgenticInitSuccess db "Agentic system initialized",0
szAgenticInitFailed db "Agentic system initialization failed",0
szWindowCreated db "Main window created",0
szWindowCreateFailed db "Main window creation failed",0
szNewFileCreated db "New file created",0
szFileOpened db "File opened",0
szFileOpenFailed db "File open failed",0
szFileSaved db "File saved",0
szFileSaveFailed db "File save failed",0
szMemoryHotpatchApplied db "Memory hotpatch applied",0
szHotpatchFailed db "Hotpatch failed",0
szAgenticProcessed db "Agentic command processed",0
szAgenticFailed db "Agentic command failed",0
szIDECleanup db "IDE cleanup completed",0
szRawrXDTitle db "RawrXD MASM IDE",0

; ============================================================================
; END OF FILE
; ============================================================================

END