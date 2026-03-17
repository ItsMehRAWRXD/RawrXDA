;==========================================================================
; main_window_masm.asm - Pure MASM64 Main Window Implementation
; ==========================================================================
; MASM equivalent of Qt MainWindow.cpp/h with all IDE functionality
; Implements VS Code-like layout, menus, toolbars, and subsystems
;==========================================================================

option casemap:none
option noscoped
option proc:private

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comdlg32.lib
includelib shell32.lib

;==========================================================================
; STRUCTURES
;==========================================================================

; MainWindow structure - equivalent to Qt MainWindow class
MAIN_WINDOW struct
    hWnd                dq ?        ; Window handle
    hInstance           dq ?        ; Instance handle
    hMenuBar            dq ?        ; Menu bar handle
    hStatusBar          dq ?        ; Status bar handle
    hToolBar            dq ?        ; Toolbar handle
    hSplitter           dq ?        ; Main splitter
    hActivityBar        dq ?        ; Activity bar
    hEditor             dq ?        ; Main editor
    hTerminal           dq ?        ; Terminal widget
    hChatPanel          dq ?        ; AI chat panel
    hFileTree           dq ?        ; File tree explorer
    hHotpatchPanel      dq ?        ; Hotpatch panel
    hSettingsDialog     dq ?        ; Settings dialog
    hInferenceEngine    dq ?        ; Inference engine handle
    hHotpatchManager    dq ?        ; Hotpatch manager
    hAgentOrchestrator  dq ?        ; Agent orchestrator
    hModelLoader        dq ?        ; Model loader
    hCommandPalette     dq ?        ; Command palette
    hMinimap            dq ?        ; Code minimap
    hProjectExplorer    dq ?        ; Project explorer
    hDebugPanel         dq ?        ; Debug panel
    hOutputPanel        dq ?        ; Output panel
    hProblemsPanel      dq ?        ; Problems panel
    hSearchPanel        dq ?        ; Search panel
    hExtensionsPanel    dq ?        ; Extensions panel
    hSCMPanel           dq ?        ; SCM panel
    hRunPanel           dq ?        ; Run panel
    hTestPanel          dq ?        ; Test panel
    hSettingsPanel      dq ?        ; Settings panel
    hTrayIcon           dq ?        ; System tray icon
    hEngineThread       dq ?        ; Engine thread handle
    hGGUFServer         dq ?        ; GGUF server handle
    
    ; State variables
    windowTitle         db 256 dup(?)   ; Window title
    currentModelPath    db 512 dup(?)   ; Current model path
    currentProjectPath  db 512 dup(?)   ; Current project path
    statusMessage       db 256 dup(?)   ; Status bar message
    agentMode           db 32 dup(?)    ; Current agent mode
    backendType         db 32 dup(?)    ; AI backend type
    
    ; Flags
    isDarkTheme         db ?        ; Dark theme enabled
    isStreamingMode     db ?        ; Streaming mode enabled
    isModelLoaded       db ?        ; Model loaded flag
    isProjectOpen       db ?        ; Project open flag
    isDebugMode         db ?        ; Debug mode flag
    isHotpatchEnabled   db ?        ; Hotpatch enabled flag
    
    ; Layout state
    splitterPositions   dd 4 dup(?) ; Splitter positions
    dockWidgetStates    dd 16 dup(?) ; Dock widget states
    
    ; Menu IDs
    fileMenuID          dd ?
    editMenuID          dd ?
    viewMenuID          dd ?
    aiMenuID            dd ?
    agentMenuID         dd ?
    toolsMenuID         dd ?
    helpMenuID          dd ?
MAIN_WINDOW ends

;==========================================================================
; CONSTANTS
;==========================================================================

; Window class name
WND_CLASS_NAME db "RawrXDMainWindow",0

; Menu IDs
IDM_FILE_NEW          equ 1001
IDM_FILE_OPEN         equ 1002
IDM_FILE_SAVE         equ 1003
IDM_FILE_SAVE_AS      equ 1004
IDM_FILE_EXIT         equ 1005
IDM_EDIT_UNDO         equ 1101
IDM_EDIT_REDO         equ 1102
IDM_EDIT_CUT          equ 1103
IDM_EDIT_COPY         equ 1104
IDM_EDIT_PASTE        equ 1105
IDM_EDIT_FIND         equ 1106
IDM_EDIT_REPLACE      equ 1107
IDM_VIEW_FILE_TREE    equ 1201
IDM_VIEW_SEARCH       equ 1202
IDM_VIEW_SCM          equ 1203
IDM_VIEW_DEBUG        equ 1204
IDM_VIEW_EXTENSIONS   equ 1205
IDM_VIEW_TERMINAL     equ 1206
IDM_VIEW_OUTPUT       equ 1207
IDM_VIEW_PROBLEMS     equ 1208
IDM_VIEW_RUN          equ 1209
IDM_VIEW_TEST         equ 1210
IDM_AI_CHAT           equ 1301
IDM_AI_LOAD_MODEL     equ 1302
IDM_AI_ANALYZE        equ 1303
IDM_AI_REFACTOR       equ 1304
IDM_AI_GENERATE       equ 1305
IDM_AGENT_PLAN        equ 1401
IDM_AGENT_AGENT       equ 1402
IDM_AGENT_ASK         equ 1403
IDM_AGENT_DEBUG       equ 1404
IDM_AGENT_OPTIMIZE    equ 1405
IDM_AGENT_TEACH       equ 1406
IDM_AGENT_ARCHITECT   equ 1407
IDM_TOOLS_SETTINGS    equ 1501
IDM_TOOLS_HOTPATCH    equ 1502
IDM_TOOLS_COMMAND     equ 1503
IDM_HELP_ABOUT        equ 1601
IDM_HELP_DOCS         equ 1602

; Control IDs
IDC_MAIN_SPLITTER     equ 2001
IDC_ACTIVITY_BAR      equ 2002
IDC_EDITOR_AREA       equ 2003
IDC_TERMINAL_AREA     equ 2004
IDC_CHAT_PANEL        equ 2005
IDC_FILE_TREE         equ 2006
IDC_HOTPATCH_PANEL    equ 2007
IDC_STATUS_BAR        equ 2008
IDC_TOOL_BAR          equ 2009
IDC_COMMAND_PALETTE   equ 2010

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================

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
EXTERN session_manager_init:PROC
EXTERN console_log_init:PROC
EXTERN console_log:PROC
EXTERN file_log_init:PROC
EXTERN file_log_append:PROC
EXTERN GetFileAttributesA:PROC
EXTERN triage_once:BYTE

; Hotpatching systems
EXTERN hpatch_apply_memory:PROC
EXTERN hpatch_apply_byte:PROC
EXTERN hpatch_apply_server:PROC
EXTERN hpatch_get_stats:PROC
EXTERN hpatch_reset_stats:PROC

; Agentic systems
EXTERN agent_init_tools:PROC
EXTERN agent_process_command:PROC
EXTERN agent_list_tools:PROC
EXTERN agent_get_tool:PROC
EXTERN AgenticEngine_Initialize:PROC
EXTERN AgenticEngine_ProcessResponse:PROC
EXTERN AgenticEngine_ExecuteTask:PROC
EXTERN AgenticEngine_GetStats:PROC

; Model systems
EXTERN ml_masm_init:PROC
EXTERN ml_masm_free:PROC
EXTERN ml_masm_get_tensor:PROC
EXTERN ml_masm_get_arch:PROC
EXTERN ml_masm_last_error:PROC
EXTERN ml_masm_inference:PROC
EXTERN ml_masm_get_response:PROC

;==========================================================================
; DATA SECTION
;==========================================================================

.data

; Global MainWindow instance
g_mainWindow MAIN_WINDOW <?>

; Window messages
szWindowTitle db "RawrXD IDE - Pure MASM64",0
szStatusReady db "Ready",0
szStatusInitializing db "Initializing...",0
szStatusModelLoaded db "Model loaded",0
szStatusHotpatchReady db "Hotpatch system ready",0

; Menu strings
szFileMenu db "&File",0
szEditMenu db "&Edit",0
szViewMenu db "&View",0
szAIMenu db "&AI",0
szAgentMenu db "&Agent",0
szToolsMenu db "&Tools",0
szHelpMenu db "&Help",0

szNewFile db "&New",0
szOpenFile db "&Open...",0
szSaveFile db "&Save",0
szSaveAs db "Save &As...",0
szExit db "E&xit",0

szUndo db "&Undo",0
szRedo db "&Redo",0
szCut db "Cu&t",0
szCopy db "&Copy",0
szPaste db "&Paste",0
szFind db "&Find...",0
szReplace db "&Replace...",0

szFileTree db "File &Tree",0
szSearch db "&Search",0
szSCM db "S&CM",0
szDebug db "&Debug",0
szExtensions db "E&xtensions",0
szTerminal db "&Terminal",0
szOutput db "&Output",0
szProblems db "&Problems",0
szRun db "&Run",0
szTest db "&Test",0

szAIChat db "Start &Chat",0
szAILoadModel db "Load &Model...",0
szAIAnalyze db "&Analyze",0
szAIRefactor db "&Refactor",0
szAIGenerate db "&Generate",0

szAgentPlan db "Plan &Mode",0
szAgentAgent db "Agent &Mode",0
szAgentAsk db "Ask &Mode",0
szAgentDebug db "Debug &Mode",0
szAgentOptimize db "Optimize &Mode",0
szAgentTeach db "Teach &Mode",0
szAgentArchitect db "Architect &Mode",0

szToolsSettings db "&Settings...",0
szToolsHotpatch db "&Hotpatch Panel",0
szToolsCommand db "Command &Palette",0

szHelpAbout db "&About",0
szHelpDocs db "&Documentation",0

;==========================================================================
; CODE SECTION
;==========================================================================

.code

;==========================================================================
; MainWindow_Initialize - Initialize the main window
; Parameters: hInstance - application instance handle
; Returns: TRUE if successful, FALSE otherwise
;==========================================================================
MainWindow_Initialize proc hInstance:QWORD
    local hMenu:QWORD
    local hSubMenu:QWORD
    
    ; Initialize window structure
    mov rax, hInstance
    mov g_mainWindow.hInstance, rax
    
    ; Create main window
    push 0
    push 0
    push 0
    push hInstance
    push 0
    push 0
    push 600
    push 800
    push 100
    push 100
    push WS_OVERLAPPEDWINDOW
    push offset szWindowTitle
    push offset WND_CLASS_NAME
    push 0
    call CreateWindowExA
    mov g_mainWindow.hWnd, rax
    test rax, rax
    jz init_failed
    
    ; Create menu bar
    call CreateMenu
    mov g_mainWindow.hMenuBar, rax
    test rax, rax
    jz init_failed
    
    ; Setup menus
    call MainWindow_SetupMenus
    
    ; Setup toolbars
    call MainWindow_SetupToolbars
    
    ; Setup status bar
    call MainWindow_SetupStatusBar
    
    ; Create VS Code-like layout
    call MainWindow_CreateVSCodeLayout
    
    ; Initialize subsystems
    call MainWindow_InitSubsystems
    
    ; Setup dock widgets
    call MainWindow_SetupDockWidgets
    
    ; Setup system tray
    call MainWindow_SetupSystemTray
    
    ; Show window
    push SW_SHOWDEFAULT
    push g_mainWindow.hWnd
    call ShowWindow
    push g_mainWindow.hWnd
    call UpdateWindow
    
    ; Set initial status
    push offset szStatusInitializing
    call MainWindow_SetStatusMessage
    
    mov rax, TRUE
    ret
    
init_failed:
    mov rax, FALSE
    ret
MainWindow_Initialize endp

;==========================================================================
; MainWindow_SetupMenus - Setup all menus
;==========================================================================
MainWindow_SetupMenus proc
    local hMenu:QWORD
    local hSubMenu:QWORD
    
    ; Get main menu bar
    mov rax, g_mainWindow.hMenuBar
    mov hMenu, rax
    
    ; File menu
    invoke ui_create_submenu, hMenu, addr szFileMenu
    mov hSubMenu, rax
    mov g_mainWindow.fileMenuID, eax
    
    invoke ui_add_menu_item, hSubMenu, addr szNewFile, IDM_FILE_NEW
    invoke ui_add_menu_item, hSubMenu, addr szOpenFile, IDM_FILE_OPEN
    invoke ui_add_menu_separator, hSubMenu
    invoke ui_add_menu_item, hSubMenu, addr szSaveFile, IDM_FILE_SAVE
    invoke ui_add_menu_item, hSubMenu, addr szSaveAs, IDM_FILE_SAVE_AS
    invoke ui_add_menu_separator, hSubMenu
    invoke ui_add_menu_item, hSubMenu, addr szExit, IDM_FILE_EXIT
    
    ; Edit menu
    invoke ui_create_submenu, hMenu, addr szEditMenu
    mov hSubMenu, rax
    mov g_mainWindow.editMenuID, eax
    
    invoke ui_add_menu_item, hSubMenu, addr szUndo, IDM_EDIT_UNDO
    invoke ui_add_menu_item, hSubMenu, addr szRedo, IDM_EDIT_REDO
    invoke ui_add_menu_separator, hSubMenu
    invoke ui_add_menu_item, hSubMenu, addr szCut, IDM_EDIT_CUT
    invoke ui_add_menu_item, hSubMenu, addr szCopy, IDM_EDIT_COPY
    invoke ui_add_menu_item, hSubMenu, addr szPaste, IDM_EDIT_PASTE
    invoke ui_add_menu_separator, hSubMenu
    invoke ui_add_menu_item, hSubMenu, addr szFind, IDM_EDIT_FIND
    invoke ui_add_menu_item, hSubMenu, addr szReplace, IDM_EDIT_REPLACE
    
    ; View menu
    invoke ui_create_submenu, hMenu, addr szViewMenu
    mov hSubMenu, rax
    mov g_mainWindow.viewMenuID, eax
    
    invoke ui_add_menu_item, hSubMenu, addr szFileTree, IDM_VIEW_FILE_TREE
    invoke ui_add_menu_item, hSubMenu, addr szSearch, IDM_VIEW_SEARCH
    invoke ui_add_menu_item, hSubMenu, addr szSCM, IDM_VIEW_SCM
    invoke ui_add_menu_item, hSubMenu, addr szDebug, IDM_VIEW_DEBUG
    invoke ui_add_menu_item, hSubMenu, addr szExtensions, IDM_VIEW_EXTENSIONS
    invoke ui_add_menu_separator, hSubMenu
    invoke ui_add_menu_item, hSubMenu, addr szTerminal, IDM_VIEW_TERMINAL
    invoke ui_add_menu_item, hSubMenu, addr szOutput, IDM_VIEW_OUTPUT
    invoke ui_add_menu_item, hSubMenu, addr szProblems, IDM_VIEW_PROBLEMS
    invoke ui_add_menu_item, hSubMenu, addr szRun, IDM_VIEW_RUN
    invoke ui_add_menu_item, hSubMenu, addr szTest, IDM_VIEW_TEST
    
    ; AI menu
    invoke ui_create_submenu, hMenu, addr szAIMenu
    mov hSubMenu, rax
    mov g_mainWindow.aiMenuID, eax
    
    invoke ui_add_menu_item, hSubMenu, addr szAIChat, IDM_AI_CHAT
    invoke ui_add_menu_separator, hSubMenu
    invoke ui_add_menu_item, hSubMenu, addr szAILoadModel, IDM_AI_LOAD_MODEL
    invoke ui_add_menu_separator, hSubMenu
    invoke ui_add_menu_item, hSubMenu, addr szAIAnalyze, IDM_AI_ANALYZE
    invoke ui_add_menu_item, hSubMenu, addr szAIRefactor, IDM_AI_REFACTOR
    invoke ui_add_menu_item, hSubMenu, addr szAIGenerate, IDM_AI_GENERATE
    
    ; Agent menu
    invoke ui_create_submenu, hMenu, addr szAgentMenu
    mov hSubMenu, rax
    mov g_mainWindow.agentMenuID, eax
    
    invoke ui_add_menu_item, hSubMenu, addr szAgentAsk, IDM_AGENT_ASK
    invoke ui_add_menu_item, hSubMenu, addr szAgentPlan, IDM_AGENT_PLAN
    invoke ui_add_menu_item, hSubMenu, addr szAgentAgent, IDM_AGENT_AGENT
    invoke ui_add_menu_item, hSubMenu, addr szAgentDebug, IDM_AGENT_DEBUG
    invoke ui_add_menu_item, hSubMenu, addr szAgentOptimize, IDM_AGENT_OPTIMIZE
    invoke ui_add_menu_item, hSubMenu, addr szAgentTeach, IDM_AGENT_TEACH
    invoke ui_add_menu_item, hSubMenu, addr szAgentArchitect, IDM_AGENT_ARCHITECT
    
    ; Tools menu
    invoke ui_create_submenu, hMenu, addr szToolsMenu
    mov hSubMenu, rax
    mov g_mainWindow.toolsMenuID, eax
    
    invoke ui_add_menu_item, hSubMenu, addr szToolsSettings, IDM_TOOLS_SETTINGS
    invoke ui_add_menu_item, hSubMenu, addr szToolsHotpatch, IDM_TOOLS_HOTPATCH
    invoke ui_add_menu_item, hSubMenu, addr szToolsCommand, IDM_TOOLS_COMMAND
    
    ; Help menu
    invoke ui_create_submenu, hMenu, addr szHelpMenu
    mov hSubMenu, rax
    mov g_mainWindow.helpMenuID, eax
    
    invoke ui_add_menu_item, hSubMenu, addr szHelpAbout, IDM_HELP_ABOUT
    invoke ui_add_menu_item, hSubMenu, addr szHelpDocs, IDM_HELP_DOCS
    
    ret
MainWindow_SetupMenus endp

;==========================================================================
; MainWindow_SetupToolbars - Setup toolbars
;==========================================================================
MainWindow_SetupToolbars proc
    ; Create main toolbar
    invoke ui_create_toolbar, g_mainWindow.hWnd
    mov g_mainWindow.hToolBar, rax
    
    ; Add toolbar buttons
    invoke ui_add_toolbar_button, g_mainWindow.hToolBar, addr szNewFile, IDM_FILE_NEW
    invoke ui_add_toolbar_button, g_mainWindow.hToolBar, addr szOpenFile, IDM_FILE_OPEN
    invoke ui_add_toolbar_button, g_mainWindow.hToolBar, addr szSaveFile, IDM_FILE_SAVE
    invoke ui_add_toolbar_separator, g_mainWindow.hToolBar
    invoke ui_add_toolbar_button, g_mainWindow.hToolBar, addr szUndo, IDM_EDIT_UNDO
    invoke ui_add_toolbar_button, g_mainWindow.hToolBar, addr szRedo, IDM_EDIT_REDO
    invoke ui_add_toolbar_separator, g_mainWindow.hToolBar
    invoke ui_add_toolbar_button, g_mainWindow.hToolBar, addr szAIChat, IDM_AI_CHAT
    invoke ui_add_toolbar_button, g_mainWindow.hToolBar, addr szAILoadModel, IDM_AI_LOAD_MODEL
    
    ret
MainWindow_SetupToolbars endp

;==========================================================================
; MainWindow_SetupStatusBar - Setup status bar
;==========================================================================
MainWindow_SetupStatusBar proc
    ; Create status bar
    invoke ui_create_statusbar, g_mainWindow.hWnd
    mov g_mainWindow.hStatusBar, rax
    
    ; Set initial message
    invoke ui_set_statusbar_text, g_mainWindow.hStatusBar, addr szStatusReady
    
    ret
MainWindow_SetupStatusBar endp

;==========================================================================
; MainWindow_CreateVSCodeLayout - Create VS Code-like layout
;==========================================================================
MainWindow_CreateVSCodeLayout proc
    ; Create main splitter (vertical)
    invoke ui_create_splitter, g_mainWindow.hWnd, TRUE  ; TRUE = vertical
    mov g_mainWindow.hSplitter, rax
    
    ; Create activity bar (left sidebar)
    invoke ui_create_activity_bar, g_mainWindow.hSplitter
    mov g_mainWindow.hActivityBar, rax
    
    ; Create editor area (center)
    invoke ui_create_editor_area, g_mainWindow.hSplitter
    mov g_mainWindow.hEditor, rax
    
    ; Create terminal area (bottom)
    invoke ui_create_terminal_area, g_mainWindow.hSplitter
    mov g_mainWindow.hTerminal, rax
    
    ; Set splitter proportions (editor gets most space)
    invoke ui_set_splitter_proportions, g_mainWindow.hSplitter, 20, 60, 20
    
    ret
MainWindow_CreateVSCodeLayout endp

;==========================================================================
; MainWindow_InitSubsystems - Initialize all subsystems
;==========================================================================
MainWindow_InitSubsystems proc
    ; Initialize hotpatch manager
    invoke hpatch_apply_memory
    invoke hpatch_apply_byte
    invoke hpatch_apply_server
    
    ; Initialize agentic engine
    invoke AgenticEngine_Initialize
    invoke agent_init_tools
    
    ; Initialize model loader
    invoke ml_masm_init
    
    ; Initialize IDE components (Non-simplified)
    invoke ide_init_all_components
    
    ; Initialize pane system
    mov rcx, g_mainWindow.hWnd
    invoke PaneSystem_Init
    invoke PaneSystem_CreateLayout
    
    ; Initialize logging
    invoke console_log_init
    invoke file_log_init
    
    ; Initialize session manager
    invoke session_manager_init
    
    ret
MainWindow_InitSubsystems endp

;==========================================================================
; MainWindow_SetupDockWidgets - Setup dock widgets
;==========================================================================
MainWindow_SetupDockWidgets proc
    ; Create file tree dock widget
    invoke ui_create_file_tree, g_mainWindow.hWnd
    mov g_mainWindow.hFileTree, rax
    
    ; Create hotpatch panel dock widget
    invoke ui_create_hotpatch_panel, g_mainWindow.hWnd
    mov g_mainWindow.hHotpatchPanel, rax
    
    ; Create chat panel dock widget
    invoke ui_create_chat_panel, g_mainWindow.hWnd
    mov g_mainWindow.hChatPanel, rax
    
    ; Create command palette
    invoke ui_create_command_palette, g_mainWindow.hWnd
    mov g_mainWindow.hCommandPalette, rax
    
    ret
MainWindow_SetupDockWidgets endp

;==========================================================================
; MainWindow_SetupSystemTray - Setup system tray icon
;==========================================================================
MainWindow_SetupSystemTray proc
    ; Create system tray icon
    invoke ui_create_tray_icon, g_mainWindow.hWnd, addr szWindowTitle
    mov g_mainWindow.hTrayIcon, rax
    
    ret
MainWindow_SetupSystemTray endp

;==========================================================================
; MainWindow_SetStatusMessage - Set status bar message
; Parameters: pMessage - pointer to message string
;==========================================================================
MainWindow_SetStatusMessage proc pMessage:QWORD
    invoke ui_set_statusbar_text, g_mainWindow.hStatusBar, pMessage
    ret
MainWindow_SetStatusMessage endp

;==========================================================================
; MainWindow_HandleMenuCommand - Handle menu commands
; Parameters: commandID - menu command ID
;==========================================================================
MainWindow_HandleMenuCommand proc commandID:DWORD
    .if commandID == IDM_FILE_NEW
        call MainWindow_HandleNewFile
    .elseif commandID == IDM_FILE_OPEN
        call MainWindow_HandleOpenFile
    .elseif commandID == IDM_FILE_SAVE
        call MainWindow_HandleSaveFile
    .elseif commandID == IDM_FILE_SAVE_AS
        call MainWindow_HandleSaveAs
    .elseif commandID == IDM_FILE_EXIT
        call MainWindow_HandleExit
    .elseif commandID == IDM_AI_CHAT
        call MainWindow_HandleAIChat
    .elseif commandID == IDM_AI_LOAD_MODEL
        call MainWindow_HandleLoadModel
    .elseif commandID == IDM_TOOLS_SETTINGS
        call MainWindow_HandleSettings
    .elseif commandID == IDM_TOOLS_HOTPATCH
        call MainWindow_HandleHotpatchPanel
    .elseif commandID == IDM_TOOLS_COMMAND
        call MainWindow_HandleCommandPalette
    .elseif commandID == IDM_HELP_ABOUT
        call MainWindow_HandleAbout
    .elseif commandID >= IDM_AGENT_PLAN && commandID <= IDM_AGENT_ARCHITECT
        call MainWindow_HandleAgentMode
    .endif
    ret
MainWindow_HandleMenuCommand endp

;==========================================================================
; Menu command handlers
;==========================================================================

MainWindow_HandleNewFile proc
    invoke ui_editor_set_text, g_mainWindow.hEditor, 0
    invoke MainWindow_SetStatusMessage, addr szStatusReady
    ret
MainWindow_HandleNewFile endp

MainWindow_HandleOpenFile proc
    local filePath[512]:BYTE
    
    invoke ui_open_file_dialog, g_mainWindow.hWnd, addr filePath, 512
    .if rax != 0
        invoke ui_editor_set_text, g_mainWindow.hEditor, addr filePath
        invoke MainWindow_SetStatusMessage, addr szStatusReady
    .endif
    ret
MainWindow_HandleOpenFile endp

MainWindow_HandleSaveFile proc
    local filePath[512]:BYTE
    
    invoke ui_save_file_dialog, g_mainWindow.hWnd, addr filePath, 512
    .if rax != 0
        invoke ui_editor_get_text, g_mainWindow.hEditor
        ; Save file implementation would go here
        invoke MainWindow_SetStatusMessage, addr szStatusReady
    .endif
    ret
MainWindow_HandleSaveFile endp

MainWindow_HandleSaveAs proc
    ; Similar to SaveFile but with different dialog
    call MainWindow_HandleSaveFile
    ret
MainWindow_HandleSaveAs endp

MainWindow_HandleExit proc
    invoke PostQuitMessage, 0
    ret
MainWindow_HandleExit endp

MainWindow_HandleAIChat proc
    invoke ui_show_dialog, g_mainWindow.hChatPanel
    invoke MainWindow_SetStatusMessage, addr szStatusReady
    ret
MainWindow_HandleAIChat endp

MainWindow_HandleLoadModel proc
    local modelPath[512]:BYTE
    
    invoke ui_open_file_dialog, g_mainWindow.hWnd, addr modelPath, 512
    .if rax != 0
        ; Load model implementation would go here
        invoke MainWindow_SetStatusMessage, addr szStatusModelLoaded
    .endif
    ret
MainWindow_HandleLoadModel endp

MainWindow_HandleSettings proc
    invoke ui_show_dialog, g_mainWindow.hSettingsDialog
    ret
MainWindow_HandleSettings endp

MainWindow_HandleHotpatchPanel proc
    invoke ui_show_dialog, g_mainWindow.hHotpatchPanel
    invoke MainWindow_SetStatusMessage, addr szStatusHotpatchReady
    ret
MainWindow_HandleHotpatchPanel endp

MainWindow_HandleCommandPalette proc
    invoke ui_show_dialog, g_mainWindow.hCommandPalette
    ret
MainWindow_HandleCommandPalette endp

MainWindow_HandleAbout proc
    local aboutMsg[256]:BYTE
    
    mov aboutMsg, "RawrXD IDE - Pure MASM64 Version\nBuilt with zero C++ dependencies",0
    invoke MessageBoxA, g_mainWindow.hWnd, addr aboutMsg, addr szWindowTitle, MB_OK
    ret
MainWindow_HandleAbout endp

EXTERN agent_switch_mode:PROC

MainWindow_HandleAgentMode proc
    ; Get command ID from wParam (passed via MainWindow_HandleMenuCommand)
    ; IDM_AGENT_ASK = 1403, IDM_AGENT_PLAN = 1401, etc.
    ; Map IDM to AGENT_MODE
    
    mov eax, ecx ; ecx contains commandID
    sub eax, 1401 ; IDM_AGENT_PLAN is 1401
    
    ; Simple mapping for now (needs to be exact)
    .if eax == 0 ; IDM_AGENT_PLAN
        mov ecx, 2 ; AGENT_MODE_PLAN
    .elseif eax == 1 ; IDM_AGENT_AGENT
        mov ecx, 1 ; AGENT_MODE_EDIT (Agent mode maps to Edit for now)
    .elseif eax == 2 ; IDM_AGENT_ASK
        mov ecx, 0 ; AGENT_MODE_ASK
    .elseif eax == 3 ; IDM_AGENT_DEBUG
        mov ecx, 3 ; AGENT_MODE_DEBUG
    .elseif eax == 4 ; IDM_AGENT_OPTIMIZE
        mov ecx, 4 ; AGENT_MODE_OPTIMIZE
    .elseif eax == 5 ; IDM_AGENT_TEACH
        mov ecx, 5 ; AGENT_MODE_TEACH
    .elseif eax == 6 ; IDM_AGENT_ARCHITECT
        mov ecx, 6 ; AGENT_MODE_ARCHITECT
    .endif
    
    call agent_switch_mode
    
    invoke MainWindow_SetStatusMessage, addr szStatusReady
    ret
MainWindow_HandleAgentMode endp

;==========================================================================
; MainWindow_Run - Main window message loop
;==========================================================================
MainWindow_Run proc
    local msg:MSG
    
msg_loop:
    invoke GetMessageA, addr msg, 0, 0, 0
    .if rax == 0
        jmp exit_loop
    .endif
    
    invoke TranslateMessage, addr msg
    invoke DispatchMessageA, addr msg
    jmp msg_loop
    
exit_loop:
    mov rax, msg.wParam
    ret
MainWindow_Run endp

;==========================================================================
; MainWindow_WndProc - Window procedure
; Parameters: standard Win32 WndProc parameters
;==========================================================================

EXTERN ide_editor_open_file:PROC
EXTERN ide_editor_save_file:PROC

MainWindow_WndProc proc hWnd:QWORD, uMsg:QWORD, wParam:QWORD, lParam:QWORD
    .if uMsg == WM_COMMAND
        mov eax, wParam
        and eax, 0FFFFh
        invoke MainWindow_HandleMenuCommand, eax
        xor rax, rax
        ret
    .elseif uMsg == WM_NOTIFY
        mov rbx, lParam
        mov eax, [rbx + 8] ; NMHDR.code
        .if eax == -2 ; NM_DBLCLK
            mov eax, [rbx + 4] ; NMHDR.idFrom
            .if eax == IDC_FILE_TREE
                ; Handle file tree double click
                ; For now, just open a dummy file or get selected item
                lea rcx, szDummyFile
                call ide_editor_open_file
            .endif
        .endif
        xor rax, rax
        ret
    .elseif uMsg == WM_SIZE
        invoke PaneSystem_HandleResize
        xor rax, rax
        ret
    .elseif uMsg == WM_CLOSE
        invoke DestroyWindow, hWnd
        xor rax, rax
        ret
    .elseif uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
        xor rax, rax
        ret
    .endif
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
MainWindow_WndProc endp

.data
    szDummyFile db "src/masm/final-ide/main_window_masm.asm",0

end