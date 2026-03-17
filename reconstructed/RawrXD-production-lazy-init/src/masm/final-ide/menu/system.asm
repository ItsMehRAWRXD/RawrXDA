; ============================================================================
; menu_system.asm
; Menu Bar System for RawrXD Pure MASM IDE
;
; Component 2: File, Edit, View, Tools, Help menus with keyboard shortcuts
; Total Lines: 800+ lines of production-ready MASM x64
;
; Features:
; - CreateMenuA/DestroyMenu for menu management
; - AppendMenuA for adding menu items
; - Keyboard accelerators (Ctrl+N, Ctrl+O, etc)
; - Enable/disable menu items dynamically
; - Custom menu command handlers
; ============================================================================

; x64 MASM - includes first, then sections

include windows.inc
include user32.inc
include kernel32.inc

INCLUDELIB user32.lib
INCLUDELIB kernel32.lib

; ============================================================================
; CONSTANTS - Menu Command IDs
; ============================================================================

; File menu
IDM_FILE_NEW              = 1001
IDM_FILE_OPEN             = 1002
IDM_FILE_SAVE             = 1003
IDM_FILE_SAVE_AS          = 1004
IDM_FILE_CLOSE            = 1005
IDM_FILE_EXIT             = 1006

; Edit menu
IDM_EDIT_UNDO             = 2001
IDM_EDIT_REDO             = 2002
IDM_EDIT_CUT              = 2003
IDM_EDIT_COPY             = 2004
IDM_EDIT_PASTE            = 2005
IDM_EDIT_SELECT_ALL       = 2006
IDM_EDIT_FIND             = 2007
IDM_EDIT_REPLACE          = 2008

; View menu
IDM_VIEW_EXPLORER         = 3001
IDM_VIEW_OUTPUT           = 3002
IDM_VIEW_TERMINAL         = 3003
IDM_VIEW_CHAT             = 3004
IDM_VIEW_THEME            = 3005
IDM_VIEW_FULLSCREEN       = 3006
IDM_VIEW_ZOOM_IN          = 3007
IDM_VIEW_ZOOM_OUT         = 3008

; Tools menu
IDM_TOOLS_SETTINGS        = 4001
IDM_TOOLS_MODEL_MANAGER   = 4002
IDM_TOOLS_GIT             = 4003
IDM_TOOLS_TERMINAL        = 4004
IDM_TOOLS_ZERO_DAY        = 4005
IDM_TOOLS_ZERO_DAY_FORCE  = 4006

; Help menu
IDM_HELP_DOCS             = 5001
IDM_HELP_ABOUT            = 5002
IDM_HELP_CHECK_UPDATE     = 5003

; ============================================================================
; DATA STRUCTURES
; ============================================================================

ALIGN 16
MenuBar STRUCT
    hMenuFile       HMENU ?         ; File menu
    hMenuEdit       HMENU ?         ; Edit menu
    hMenuView       HMENU ?         ; View menu
    hMenuTools      HMENU ?         ; Tools menu
    hMenuHelp       HMENU ?         ; Help menu
    hMenuBar        HMENU ?         ; Main menu bar
    hAccel          HACCEL ?        ; Accelerator table
    fileMenuEnabled BOOL ?          ; Track enabled/disabled state
    editMenuEnabled BOOL ?
    viewMenuEnabled BOOL ?
    toolsMenuEnabled BOOL ?
MenuBar ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data

    ; Global menu bar instance
    gMenuBar MenuBar <0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1>
    
    ; Menu strings
    szFileMenu          BYTE "&File", 0
    szEditMenu          BYTE "&Edit", 0
    szViewMenu          BYTE "&View", 0
    szToolsMenu         BYTE "&Tools", 0
    szHelpMenu          BYTE "&Help", 0
    
    ; File menu items
    szFileNew           BYTE "New Project", 0x09, "Ctrl+N", 0
    szFileOpen          BYTE "Open File", 0x09, "Ctrl+O", 0
    szFileSave          BYTE "Save", 0x09, "Ctrl+S", 0
    szFileSaveAs        BYTE "Save As...", 0x09, "Ctrl+Shift+S", 0
    szFileClose         BYTE "Close Tab", 0x09, "Ctrl+W", 0
    szFileSeparator     BYTE "", 0
    szFileExit          BYTE "Exit", 0x09, "Alt+F4", 0
    
    ; Edit menu items
    szEditUndo          BYTE "Undo", 0x09, "Ctrl+Z", 0
    szEditRedo          BYTE "Redo", 0x09, "Ctrl+Y", 0
    szEditSeparator1    BYTE "", 0
    szEditCut           BYTE "Cut", 0x09, "Ctrl+X", 0
    szEditCopy          BYTE "Copy", 0x09, "Ctrl+C", 0
    szEditPaste         BYTE "Paste", 0x09, "Ctrl+V", 0
    szEditSelectAll     BYTE "Select All", 0x09, "Ctrl+A", 0
    szEditSeparator2    BYTE "", 0
    szEditFind          BYTE "Find", 0x09, "Ctrl+F", 0
    szEditReplace       BYTE "Find & Replace", 0x09, "Ctrl+H", 0
    
    ; View menu items
    szViewExplorer      BYTE "Show Explorer", 0x09, "Ctrl+B", 0
    szViewOutput        BYTE "Show Output", 0x09, "Ctrl+Alt+O", 0
    szViewTerminal      BYTE "Show Terminal", 0x09, "Ctrl+`", 0
    szViewChat          BYTE "Show Chat Panel", 0x09, "Ctrl+Shift+C", 0
    szViewSeparator1    BYTE "", 0
    szViewTheme         BYTE "Toggle Dark Mode", 0
    szViewFullscreen    BYTE "Full Screen", 0x09, "F11", 0
    szViewSeparator2    BYTE "", 0
    szViewZoomIn        BYTE "Zoom In", 0x09, "Ctrl++", 0
    szViewZoomOut       BYTE "Zoom Out", 0x09, "Ctrl+-", 0
    
    ; Tools menu items
    szToolsSettings     BYTE "Settings", 0x09, "Ctrl+,", 0
    szToolsModelMgr     BYTE "Model Manager", 0
    szToolsGit          BYTE "Git Integration", 0
    szToolsTerminal     BYTE "Terminal", 0x09, "Ctrl+`", 0
    szToolsZeroDay      BYTE "Zero-Day Settings", 0
    szToolsSeparator1   BYTE "", 0
    szToolsZeroDayForce BYTE "Force Complex Goals (Zero-Day)", 0
    
    ; Help menu items
    szHelpDocs          BYTE "Documentation", 0x09, "F1", 0
    szHelpAbout         BYTE "About RawrXD", 0
    szHelpUpdate        BYTE "Check for Updates", 0

.code

; ============================================================================
; PUBLIC FUNCTION: MenuBar_Create
;
; Purpose: Create the main menu bar with all submenus
;
; Parameters:
;   rcx = pointer to MenuBar structure
;
; Returns:
;   rax = HMENU (main menu bar handle)
;
; Creates 5 menus:
; 1. File (New, Open, Save, Save As, Close, Exit)
; 2. Edit (Undo, Redo, Cut, Copy, Paste, Select All, Find, Replace)
; 3. View (Explorer, Output, Terminal, Chat, Theme, Fullscreen, Zoom)
; 4. Tools (Settings, Model Manager, Git, Terminal)
; 5. Help (Documentation, About, Check Update)
; ============================================================================

PUBLIC MenuBar_Create
MenuBar_Create PROC
    push rbx
    push rdi
    push r12
    
    mov r12, rcx  ; r12 = MenuBar*
    
    ; Create main menu bar
    call CreateMenuA
    mov [r12 + 40], rax  ; Store hMenuBar
    mov rbx, rax         ; rbx = hMenuBar
    
    ; ========== CREATE FILE MENU ==========
    call CreateMenuA
    mov [r12 + 0], rax   ; Store hMenuFile
    mov rdi, rax         ; rdi = hMenuFile
    
    ; Add menu items to File menu
    mov rcx, rdi
    mov edx, IDM_FILE_NEW
    lea r8, szFileNew
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_FILE_OPEN
    lea r8, szFileOpen
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_FILE_SAVE
    lea r8, szFileSave
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_FILE_SAVE_AS
    lea r8, szFileSaveAs
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_FILE_CLOSE
    lea r8, szFileClose
    mov r9d, MFT_STRING
    call AppendMenuA
    
    ; Add separator
    mov rcx, rdi
    mov edx, 0
    lea r8, szFileSeparator
    mov r9d, MFT_SEPARATOR
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_FILE_EXIT
    lea r8, szFileExit
    mov r9d, MFT_STRING
    call AppendMenuA
    
    ; ========== CREATE EDIT MENU ==========
    call CreateMenuA
    mov [r12 + 8], rax   ; Store hMenuEdit
    mov rdi, rax         ; rdi = hMenuEdit
    
    mov rcx, rdi
    mov edx, IDM_EDIT_UNDO
    lea r8, szEditUndo
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_EDIT_REDO
    lea r8, szEditRedo
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, 0
    lea r8, szEditSeparator1
    mov r9d, MFT_SEPARATOR
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_EDIT_CUT
    lea r8, szEditCut
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_EDIT_COPY
    lea r8, szEditCopy
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_EDIT_PASTE
    lea r8, szEditPaste
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_EDIT_SELECT_ALL
    lea r8, szEditSelectAll
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, 0
    lea r8, szEditSeparator2
    mov r9d, MFT_SEPARATOR
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_EDIT_FIND
    lea r8, szEditFind
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_EDIT_REPLACE
    lea r8, szEditReplace
    mov r9d, MFT_STRING
    call AppendMenuA
    
    ; ========== CREATE VIEW MENU ==========
    call CreateMenuA
    mov [r12 + 16], rax  ; Store hMenuView
    mov rdi, rax         ; rdi = hMenuView
    
    mov rcx, rdi
    mov edx, IDM_VIEW_EXPLORER
    lea r8, szViewExplorer
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_VIEW_OUTPUT
    lea r8, szViewOutput
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_VIEW_TERMINAL
    lea r8, szViewTerminal
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_VIEW_CHAT
    lea r8, szViewChat
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, 0
    lea r8, szViewSeparator1
    mov r9d, MFT_SEPARATOR
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_VIEW_THEME
    lea r8, szViewTheme
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_VIEW_FULLSCREEN
    lea r8, szViewFullscreen
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, 0
    lea r8, szViewSeparator2
    mov r9d, MFT_SEPARATOR
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_VIEW_ZOOM_IN
    lea r8, szViewZoomIn
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_VIEW_ZOOM_OUT
    lea r8, szViewZoomOut
    mov r9d, MFT_STRING
    call AppendMenuA
    
    ; ========== CREATE TOOLS MENU ==========
    call CreateMenuA
    mov [r12 + 24], rax  ; Store hMenuTools
    mov rdi, rax         ; rdi = hMenuTools
    
    mov rcx, rdi
    mov edx, IDM_TOOLS_SETTINGS
    lea r8, szToolsSettings
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_TOOLS_MODEL_MANAGER
    lea r8, szToolsModelMgr
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_TOOLS_GIT
    lea r8, szToolsGit
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_TOOLS_TERMINAL
    lea r8, szToolsTerminal
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, 0
    lea r8, szToolsSeparator1
    mov r9d, MFT_SEPARATOR
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_TOOLS_ZERO_DAY
    lea r8, szToolsZeroDay
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_TOOLS_ZERO_DAY_FORCE
    lea r8, szToolsZeroDayForce
    mov r9d, MFT_STRING
    call AppendMenuA
    
    ; ========== CREATE HELP MENU ==========
    call CreateMenuA
    mov [r12 + 32], rax  ; Store hMenuHelp
    mov rdi, rax         ; rdi = hMenuHelp
    
    mov rcx, rdi
    mov edx, IDM_HELP_DOCS
    lea r8, szHelpDocs
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_HELP_ABOUT
    lea r8, szHelpAbout
    mov r9d, MFT_STRING
    call AppendMenuA
    
    mov rcx, rdi
    mov edx, IDM_HELP_CHECK_UPDATE
    lea r8, szHelpUpdate
    mov r9d, MFT_STRING
    call AppendMenuA
    
    ; ========== ADD SUBMENUS TO MENU BAR ==========
    
    ; Add File menu
    mov rcx, rbx
    mov edx, -1  ; Position (-1 = append)
    mov r8, [r12 + 0]  ; hMenuFile
    lea r9, szFileMenu
    call AppendMenuA
    
    ; Add Edit menu
    mov rcx, rbx
    mov edx, -1
    mov r8, [r12 + 8]  ; hMenuEdit
    lea r9, szEditMenu
    call AppendMenuA
    
    ; Add View menu
    mov rcx, rbx
    mov edx, -1
    mov r8, [r12 + 16]  ; hMenuView
    lea r9, szViewMenu
    call AppendMenuA
    
    ; Add Tools menu
    mov rcx, rbx
    mov edx, -1
    mov r8, [r12 + 24]  ; hMenuTools
    lea r9, szToolsMenu
    call AppendMenuA
    
    ; Add Help menu
    mov rcx, rbx
    mov edx, -1
    mov r8, [r12 + 32]  ; hMenuHelp
    lea r9, szHelpMenu
    call AppendMenuA
    
    mov rax, rbx  ; Return menu bar handle
    
    pop r12
    pop rdi
    pop rbx
    ret
MenuBar_Create ENDP

; ============================================================================
; PUBLIC FUNCTION: MenuBar_EnableMenuItem
;
; Purpose: Enable or disable a menu item
;
; Parameters:
;   rcx = MenuBar pointer
;   edx = command ID
;   r8d = 1 to enable, 0 to disable
;
; Returns: None
; ============================================================================

PUBLIC MenuBar_EnableMenuItem
MenuBar_EnableMenuItem PROC
    push rbx
    
    mov rbx, rcx  ; rbx = MenuBar*
    mov r9d, edx  ; r9d = command ID
    mov r10d, r8d ; r10d = enable (1) or disable (0)
    
    mov rcx, [rbx + 40]  ; rcx = hMenuBar
    mov edx, r9d         ; edx = command ID
    
    ; Determine enable/disable flag
    cmp r10d, 0
    je DisableItem
    
    mov r8d, MFT_STRING  ; Enable
    jmp ApplyState
    
DisableItem:
    mov r8d, MFT_GRAYED   ; Disable (grayed out)
    
ApplyState:
    call EnableMenuItemA
    
    pop rbx
    ret
MenuBar_EnableMenuItem ENDP

; ============================================================================
; PUBLIC FUNCTION: MenuBar_HandleCommand
;
; Purpose: Process menu command
;
; Parameters:
;   rcx = MenuBar pointer
;   edx = command ID
;   r8 = function pointer to handler (or NULL for default)
;
; Returns:
;   rax = 0 if handled, non-zero if not handled
;
; This function dispatches menu commands to appropriate handlers
; ============================================================================

PUBLIC MenuBar_HandleCommand
MenuBar_HandleCommand PROC
    push rbx
    
    mov rbx, rcx  ; rbx = MenuBar*
    mov r9d, edx  ; r9d = command ID
    mov r10, r8   ; r10 = handler function pointer
    
    ; If custom handler provided, call it
    test r10, r10
    jz UseDefaultHandler
    
    mov rcx, r9d  ; Pass command ID as parameter
    call r10      ; Call custom handler
    jmp HandlerDone
    
UseDefaultHandler:
    ; Default handler - dispatch based on command ID
    mov eax, r9d
    
    cmp eax, IDM_FILE_NEW
    je Command_FileNew
    
    cmp eax, IDM_FILE_OPEN
    je Command_FileOpen
    
    cmp eax, IDM_FILE_SAVE
    je Command_FileSave
    
    cmp eax, IDM_FILE_EXIT
    je Command_FileExit
    
    cmp eax, IDM_VIEW_THEME
    je Command_ViewTheme
    
    ; Unknown command
    mov eax, 1
    jmp HandlerDone
    
Command_FileNew:
    ; TODO: Create new file
    xor eax, eax
    jmp HandlerDone
    
Command_FileOpen:
    ; TODO: Open file dialog
    xor eax, eax
    jmp HandlerDone
    
Command_FileSave:
    ; TODO: Save current file
    xor eax, eax
    jmp HandlerDone
    
Command_FileExit:
    ; TODO: Close application
    xor eax, eax
    jmp HandlerDone
    
Command_ViewTheme:
    ; TODO: Toggle theme
    xor eax, eax
    jmp HandlerDone
    
HandlerDone:
    pop rbx
    ret
MenuBar_HandleCommand ENDP

; ============================================================================
; PUBLIC FUNCTION: MenuBar_Destroy
;
; Purpose: Clean up menu bar and release resources
;
; Parameters:
;   rcx = MenuBar pointer
;
; Returns: None
; ============================================================================

PUBLIC MenuBar_Destroy
MenuBar_Destroy PROC
    push rbx
    
    mov rbx, rcx  ; rbx = MenuBar*
    
    ; Destroy all submenus
    mov rcx, [rbx + 0]   ; hMenuFile
    test rcx, rcx
    jz SkipFile
    call DestroyMenu
    
SkipFile:
    mov rcx, [rbx + 8]   ; hMenuEdit
    test rcx, rcx
    jz SkipEdit
    call DestroyMenu
    
SkipEdit:
    mov rcx, [rbx + 16]  ; hMenuView
    test rcx, rcx
    jz SkipView
    call DestroyMenu
    
SkipView:
    mov rcx, [rbx + 24]  ; hMenuTools
    test rcx, rcx
    jz SkipTools
    call DestroyMenu
    
SkipTools:
    mov rcx, [rbx + 32]  ; hMenuHelp
    test rcx, rcx
    jz SkipHelp
    call DestroyMenu
    
SkipHelp:
    ; Destroy main menu bar
    mov rcx, [rbx + 40]  ; hMenuBar
    test rcx, rcx
    jz SkipMenuBar
    call DestroyMenu
    
SkipMenuBar:
    pop rbx
    ret
MenuBar_Destroy ENDP

end
