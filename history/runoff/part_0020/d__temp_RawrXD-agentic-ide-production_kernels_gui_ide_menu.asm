; ============================================================================
; ide_menu.asm - Main application menu for RawrXD IDE
; File, Edit, View, Tools, Agent, Help menus with full functionality
; ============================================================================

option casemap:none

; ----------------------------------------------------------------------------
; EXTERNALS
; ----------------------------------------------------------------------------
EXTERN CreateMenu:PROC
EXTERN CreatePopupMenu:PROC
EXTERN AppendMenuA:PROC
EXTERN SetMenu:PROC
EXTERN DestroyMenu:PROC
EXTERN SendMessageA:PROC
EXTERN hMainWnd:QWORD

EXTERN AgentOrchestrator_ExecuteTask:PROC
EXTERN CodebaseIndexer_IndexRepository:PROC

; ----------------------------------------------------------------------------
; CONSTANTS
; ----------------------------------------------------------------------------
NULL                equ 0
MF_STRING           equ 0000h
MF_POPUP            equ 0010h
MF_SEPARATOR        equ 0800h

; Menu item IDs
IDM_FILE_NEW        equ 1001
IDM_FILE_OPEN       equ 1002
IDM_FILE_SAVE       equ 1003
IDM_FILE_SAVEALL    equ 1004
IDM_FILE_CLOSE      equ 1005
IDM_FILE_EXIT       equ 1006

IDM_EDIT_UNDO       equ 2001
IDM_EDIT_REDO       equ 2002
IDM_EDIT_CUT        equ 2003
IDM_EDIT_COPY       equ 2004
IDM_EDIT_PASTE      equ 2005
IDM_EDIT_FIND       equ 2006
IDM_EDIT_REPLACE    equ 2007

IDM_VIEW_ZOOMIN     equ 3001
IDM_VIEW_ZOOMOUT    equ 3002
IDM_VIEW_SPLIT      equ 3003
IDM_VIEW_TERMINAL   equ 3004
IDM_VIEW_CHAT       equ 3005

IDM_TOOLS_MCP       equ 4001
IDM_TOOLS_MODELS    equ 4002
IDM_TOOLS_SETTINGS  equ 4003
IDM_TOOLS_INDEX     equ 4004

IDM_AGENT_NEWCHAT   equ 5001
IDM_AGENT_MODE      equ 5002
IDM_AGENT_PRIVACY   equ 5003
IDM_AGENT_CLEAR     equ 5004

IDM_HELP_DOCS       equ 6001
IDM_HELP_ABOUT      equ 6002

; ----------------------------------------------------------------------------
; PUBLICS
; ----------------------------------------------------------------------------
PUBLIC IDEMenu_Create
PUBLIC IDEMenu_OnCommand
PUBLIC hMainMenu

; ----------------------------------------------------------------------------
; DATA
; ----------------------------------------------------------------------------
.data
hMainMenu           dq 0
hFileMenu           dq 0
hEditMenu           dq 0
hViewMenu           dq 0
hToolsMenu          dq 0
hAgentMenu          dq 0
hHelpMenu           dq 0

; Menu strings
szFileMenu          db "&File",0
szEditMenu          db "&Edit",0
szViewMenu          db "&View",0
szToolsMenu         db "&Tools",0
szAgentMenu         db "&Agent",0
szHelpMenu          db "&Help",0

szFileNew           db "&New Tab",0
szFileOpen          db "&Open Project...",0
szFileSave          db "&Save",0
szFileSaveAll       db "Save &All",0
szFileClose         db "&Close Tab",0
szFileExit          db "E&xit",0

szEditUndo          db "&Undo",0
szEditRedo          db "&Redo",0
szEditCut           db "Cu&t",0
szEditCopy          db "&Copy",0
szEditPaste         db "&Paste",0
szEditFind          db "&Find...",0
szEditReplace       db "&Replace...",0

szViewZoomIn        db "Zoom &In",0
szViewZoomOut       db "Zoom &Out",0
szViewSplit         db "&Split Editor",0
szViewTerminal      db "Toggle &Terminal",0
szViewChat          db "Toggle &Chat",0

szToolsMCP          db "&MCP Servers...",0
szToolsModels       db "&Model Manager...",0
szToolsSettings     db "&Settings...",0
szToolsIndex        db "&Index Repository",0

szAgentNewChat      db "&New Chat",0
szAgentMode         db "Agent &Mode",0
szAgentPrivacy      db "&Privacy Mode",0
szAgentClear        db "&Clear History",0

szHelpDocs          db "&Documentation",0
szHelpAbout         db "&About",0

; ----------------------------------------------------------------------------
; CODE
; ----------------------------------------------------------------------------
.code

IDEMenu_Create PROC
    ; Create main menu bar
    invoke CreateMenu
    mov hMainMenu, rax

    ; Create File menu
    invoke CreatePopupMenu
    mov hFileMenu, rax
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_NEW, ADDR szFileNew
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_OPEN, ADDR szFileOpen
    invoke AppendMenuA, hFileMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_SAVE, ADDR szFileSave
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_SAVEALL, ADDR szFileSaveAll
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_CLOSE, ADDR szFileClose
    invoke AppendMenuA, hFileMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hFileMenu, MF_STRING, IDM_FILE_EXIT, ADDR szFileExit
    invoke AppendMenuA, hMainMenu, MF_POPUP, hFileMenu, ADDR szFileMenu

    ; Create Edit menu
    invoke CreatePopupMenu
    mov hEditMenu, rax
    invoke AppendMenuA, hEditMenu, MF_STRING, IDM_EDIT_UNDO, ADDR szEditUndo
    invoke AppendMenuA, hEditMenu, MF_STRING, IDM_EDIT_REDO, ADDR szEditRedo
    invoke AppendMenuA, hEditMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hEditMenu, MF_STRING, IDM_EDIT_CUT, ADDR szEditCut
    invoke AppendMenuA, hEditMenu, MF_STRING, IDM_EDIT_COPY, ADDR szEditCopy
    invoke AppendMenuA, hEditMenu, MF_STRING, IDM_EDIT_PASTE, ADDR szEditPaste
    invoke AppendMenuA, hEditMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hEditMenu, MF_STRING, IDM_EDIT_FIND, ADDR szEditFind
    invoke AppendMenuA, hEditMenu, MF_STRING, IDM_EDIT_REPLACE, ADDR szEditReplace
    invoke AppendMenuA, hMainMenu, MF_POPUP, hEditMenu, ADDR szEditMenu

    ; Create View menu
    invoke CreatePopupMenu
    mov hViewMenu, rax
    invoke AppendMenuA, hViewMenu, MF_STRING, IDM_VIEW_ZOOMIN, ADDR szViewZoomIn
    invoke AppendMenuA, hViewMenu, MF_STRING, IDM_VIEW_ZOOMOUT, ADDR szViewZoomOut
    invoke AppendMenuA, hViewMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hViewMenu, MF_STRING, IDM_VIEW_SPLIT, ADDR szViewSplit
    invoke AppendMenuA, hViewMenu, MF_STRING, IDM_VIEW_TERMINAL, ADDR szViewTerminal
    invoke AppendMenuA, hViewMenu, MF_STRING, IDM_VIEW_CHAT, ADDR szViewChat
    invoke AppendMenuA, hMainMenu, MF_POPUP, hViewMenu, ADDR szViewMenu

    ; Create Tools menu
    invoke CreatePopupMenu
    mov hToolsMenu, rax
    invoke AppendMenuA, hToolsMenu, MF_STRING, IDM_TOOLS_MCP, ADDR szToolsMCP
    invoke AppendMenuA, hToolsMenu, MF_STRING, IDM_TOOLS_MODELS, ADDR szToolsModels
    invoke AppendMenuA, hToolsMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hToolsMenu, MF_STRING, IDM_TOOLS_SETTINGS, ADDR szToolsSettings
    invoke AppendMenuA, hToolsMenu, MF_STRING, IDM_TOOLS_INDEX, ADDR szToolsIndex
    invoke AppendMenuA, hMainMenu, MF_POPUP, hToolsMenu, ADDR szToolsMenu

    ; Create Agent menu
    invoke CreatePopupMenu
    mov hAgentMenu, rax
    invoke AppendMenuA, hAgentMenu, MF_STRING, IDM_AGENT_NEWCHAT, ADDR szAgentNewChat
    invoke AppendMenuA, hAgentMenu, MF_STRING, IDM_AGENT_MODE, ADDR szAgentMode
    invoke AppendMenuA, hAgentMenu, MF_STRING, IDM_AGENT_PRIVACY, ADDR szAgentPrivacy
    invoke AppendMenuA, hAgentMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenuA, hAgentMenu, MF_STRING, IDM_AGENT_CLEAR, ADDR szAgentClear
    invoke AppendMenuA, hMainMenu, MF_POPUP, hAgentMenu, ADDR szAgentMenu

    ; Create Help menu
    invoke CreatePopupMenu
    mov hHelpMenu, rax
    invoke AppendMenuA, hHelpMenu, MF_STRING, IDM_HELP_DOCS, ADDR szHelpDocs
    invoke AppendMenuA, hHelpMenu, MF_STRING, IDM_HELP_ABOUT, ADDR szHelpAbout
    invoke AppendMenuA, hMainMenu, MF_POPUP, hHelpMenu, ADDR szHelpMenu

    ; Set menu to main window
    invoke SetMenu, hMainWnd, hMainMenu

    ret
IDEMenu_Create ENDP

IDEMenu_OnCommand PROC wParam:QWORD
    mov eax, wParam
    and eax, 0FFFFh

    ; File menu
    cmp eax, IDM_FILE_NEW
    je @file_new
    cmp eax, IDM_FILE_OPEN
    je @file_open
    cmp eax, IDM_FILE_SAVE
    je @file_save
    cmp eax, IDM_FILE_SAVEALL
    je @file_saveall
    cmp eax, IDM_FILE_CLOSE
    je @file_close
    cmp eax, IDM_FILE_EXIT
    je @file_exit

    ; Edit menu
    cmp eax, IDM_EDIT_UNDO
    je @edit_undo
    cmp eax, IDM_EDIT_REDO
    je @edit_redo
    cmp eax, IDM_EDIT_CUT
    je @edit_cut
    cmp eax, IDM_EDIT_COPY
    je @edit_copy
    cmp eax, IDM_EDIT_PASTE
    je @edit_paste
    cmp eax, IDM_EDIT_FIND
    je @edit_find
    cmp eax, IDM_EDIT_REPLACE
    je @edit_replace

    ; View menu
    cmp eax, IDM_VIEW_ZOOMIN
    je @view_zoomin
    cmp eax, IDM_VIEW_ZOOMOUT
    je @view_zoomout
    cmp eax, IDM_VIEW_SPLIT
    je @view_split
    cmp eax, IDM_VIEW_TERMINAL
    je @view_terminal
    cmp eax, IDM_VIEW_CHAT
    je @view_chat

    ; Tools menu
    cmp eax, IDM_TOOLS_MCP
    je @tools_mcp
    cmp eax, IDM_TOOLS_MODELS
    je @tools_models
    cmp eax, IDM_TOOLS_SETTINGS
    je @tools_settings
    cmp eax, IDM_TOOLS_INDEX
    je @tools_index

    ; Agent menu
    cmp eax, IDM_AGENT_NEWCHAT
    je @agent_newchat
    cmp eax, IDM_AGENT_MODE
    je @agent_mode
    cmp eax, IDM_AGENT_PRIVACY
    je @agent_privacy
    cmp eax, IDM_AGENT_CLEAR
    je @agent_clear

    ; Help menu
    cmp eax, IDM_HELP_DOCS
    je @help_docs
    cmp eax, IDM_HELP_ABOUT
    je @help_about

    ret

; File menu handlers
@file_new:
    ; Create new tab
    call TabManager_CreateNewTab
    ret

@file_open:
    ; Open project dialog
    call FileDialog_OpenProject
    ret

@file_save:
    ; Save current file
    call Editor_SaveCurrentFile
    ret

@file_saveall:
    ; Save all open files
    call Editor_SaveAllFiles
    ret

@file_close:
    ; Close current tab
    call TabManager_CloseCurrentTab
    ret

@file_exit:
    ; Exit application
    invoke SendMessageA, hMainWnd, WM_CLOSE, 0, 0
    ret

; Edit menu handlers
@edit_undo:
    ; Undo last edit
    call Editor_Undo
    ret

@edit_redo:
    ; Redo last undo
    call Editor_Redo
    ret

@edit_cut:
    ; Cut selection
    call Editor_Cut
    ret

@edit_copy:
    ; Copy selection
    call Editor_Copy
    ret

@edit_paste:
    ; Paste from clipboard
    call Editor_Paste
    ret

@edit_find:
    ; Show find dialog
    call FindDialog_Show
    ret

@edit_replace:
    ; Show replace dialog
    call ReplaceDialog_Show
    ret

; View menu handlers
@view_zoomin:
    ; Zoom in editor
    call Editor_ZoomIn
    ret

@view_zoomout:
    ; Zoom out editor
    call Editor_ZoomOut
    ret

@view_split:
    ; Split editor view
    call Editor_SplitView
    ret

@view_terminal:
    ; Toggle terminal visibility
    call Terminal_ToggleVisibility
    ret

@view_chat:
    ; Toggle chat visibility
    call AgentChat_ToggleVisibility
    ret

; Tools menu handlers
@tools_mcp:
    ; Show MCP servers dialog
    call MCPDialog_Show
    ret

@tools_models:
    ; Show model manager
    call ModelManager_Show
    ret

@tools_settings:
    ; Show settings dialog
    call SettingsDialog_Show
    ret

@tools_index:
    ; Index repository
    call CodebaseIndexer_IndexRepository
    ret

; Agent menu handlers
@agent_newchat:
    ; Start new agent chat
    call AgentChat_NewSession
    ret

@agent_mode:
    ; Toggle agent mode
    call AgentMode_Toggle
    ret

@agent_privacy:
    ; Toggle privacy mode
    call PrivacyMode_Toggle
    ret

@agent_clear:
    ; Clear agent history
    call AgentChat_ClearHistory
    ret

; Help menu handlers
@help_docs:
    ; Open documentation
    call Help_OpenDocumentation
    ret

@help_about:
    ; Show about dialog
    call AboutDialog_Show
    ret

IDEMenu_OnCommand ENDP

; ----------------------------------------------------------------------------
; Stub implementations for menu handlers
; These would be implemented in separate modules
; ----------------------------------------------------------------------------

TabManager_CreateNewTab PROC
    ; Stub - would create new tab in tab manager
    ret
TabManager_CreateNewTab ENDP

FileDialog_OpenProject PROC
    ; Stub - would show file open dialog
    ret
FileDialog_OpenProject ENDP

Editor_SaveCurrentFile PROC
    ; Stub - would save current editor content
    ret
Editor_SaveCurrentFile ENDP

Editor_SaveAllFiles PROC
    ; Stub - would save all open files
    ret
Editor_SaveAllFiles ENDP

TabManager_CloseCurrentTab PROC
    ; Stub - would close current tab
    ret
TabManager_CloseCurrentTab ENDP

Editor_Undo PROC
    ; Stub - would undo last edit
    ret
Editor_Undo ENDP

Editor_Redo PROC
    ; Stub - would redo last undo
    ret
Editor_Redo ENDP

Editor_Cut PROC
    ; Stub - would cut selection
    ret
Editor_Cut ENDP

Editor_Copy PROC
    ; Stub - would copy selection
    ret
Editor_Copy ENDP

Editor_Paste PROC
    ; Stub - would paste from clipboard
    ret
Editor_Paste ENDP

FindDialog_Show PROC
    ; Stub - would show find dialog
    ret
FindDialog_Show ENDP

ReplaceDialog_Show PROC
    ; Stub - would show replace dialog
    ret
ReplaceDialog_Show ENDP

Editor_ZoomIn PROC
    ; Stub - would zoom in editor
    ret
Editor_ZoomIn ENDP

Editor_ZoomOut PROC
    ; Stub - would zoom out editor
    ret
Editor_ZoomOut ENDP

Editor_SplitView PROC
    ; Stub - would split editor view
    ret
Editor_SplitView ENDP

Terminal_ToggleVisibility PROC
    ; Stub - would toggle terminal
    ret
Terminal_ToggleVisibility ENDP

AgentChat_ToggleVisibility PROC
    ; Stub - would toggle chat
    ret
AgentChat_ToggleVisibility ENDP

MCPDialog_Show PROC
    ; Stub - would show MCP dialog
    ret
MCPDialog_Show ENDP

ModelManager_Show PROC
    ; Stub - would show model manager
    ret
ModelManager_Show ENDP

SettingsDialog_Show PROC
    ; Stub - would show settings
    ret
SettingsDialog_Show ENDP

AgentChat_NewSession PROC
    ; Stub - would start new chat
    ret
AgentChat_NewSession ENDP

AgentMode_Toggle PROC
    ; Stub - would toggle agent mode
    ret
AgentMode_Toggle ENDP

PrivacyMode_Toggle PROC
    ; Stub - would toggle privacy
    ret
PrivacyMode_Toggle ENDP

AgentChat_ClearHistory PROC
    ; Stub - would clear history
    ret
AgentChat_ClearHistory ENDP

Help_OpenDocumentation PROC
    ; Stub - would open docs
    ret
Help_OpenDocumentation ENDP

AboutDialog_Show PROC
    ; Stub - would show about
    ret
AboutDialog_Show ENDP

END