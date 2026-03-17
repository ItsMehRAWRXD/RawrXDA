;======================================================================
; RawrXD IDE - Menu System Implementation
;======================================================================
INCLUDE rawrxd_includes.inc

.CONST
; File menu
ID_FILE_NEW         EQU 1001
ID_FILE_OPEN        EQU 1002
ID_FILE_SAVE        EQU 1003
ID_FILE_SAVE_AS     EQU 1004
ID_FILE_SAVE_ALL    EQU 1005
ID_FILE_CLOSE       EQU 1006
ID_FILE_EXIT        EQU 1007

; Edit menu
ID_EDIT_UNDO        EQU 2001
ID_EDIT_REDO        EQU 2002
ID_EDIT_CUT         EQU 2003
ID_EDIT_COPY        EQU 2004
ID_EDIT_PASTE       EQU 2005
ID_EDIT_FIND        EQU 2006
ID_EDIT_REPLACE     EQU 2007
ID_EDIT_SELECT_ALL  EQU 2008

; View menu
ID_VIEW_PROJECT     EQU 3001
ID_VIEW_OUTPUT      EQU 3002
ID_VIEW_FULLSCREEN  EQU 3003

; Build menu
ID_BUILD_COMPILE    EQU 4001
ID_BUILD_RUN        EQU 4002
ID_BUILD_DEBUG      EQU 4003

; Agent menu
ID_AGENT_CHAT       EQU 5001
ID_AGENT_COMPLETE   EQU 5002
ID_AGENT_REFACTOR   EQU 5003

; Help menu
ID_HELP_ABOUT       EQU 6001

.DATA
g_hMenuBar          DQ ?

szFile              DB "&File", 0
szEdit              DB "&Edit", 0
szView              DB "&View", 0
szBuild             DB "&Build", 0
szAgent             DB "&Agent", 0
szHelp              DB "&Help", 0

szNew               DB "&New\tCtrl+N", 0
szOpen              DB "&Open\tCtrl+O", 0
szSave              DB "&Save\tCtrl+S", 0
szSaveAll           DB "Save &All\tCtrl+Shift+S", 0
szExit              DB "E&xit", 0

szUndo              DB "&Undo\tCtrl+Z", 0
szRedo              DB "&Redo\tCtrl+Y", 0
szFind              DB "&Find\tCtrl+F", 0
szReplace           DB "&Replace\tCtrl+H", 0

szCompile           DB "&Compile\tF7", 0
szRun               DB "&Run\tF5", 0
szDebug             DB "&Debug\tCtrl+F5", 0

szAgentChat         DB "&Chat\tCtrl+Space", 0
szAgentComplete     DB "&Complete\tTab", 0

szAbout             DB "&About...", 0

.CODE

;----------------------------------------------------------------------
; RawrXD_Menu_Create - Create main menu bar
;----------------------------------------------------------------------
RawrXD_Menu_Create PROC hParent:QWORD
    LOCAL hMenuFile:QWORD
    LOCAL hMenuEdit:QWORD
    LOCAL hMenuView:QWORD
    LOCAL hMenuBuild:QWORD
    LOCAL hMenuAgent:QWORD
    LOCAL hMenuHelp:QWORD
    
    ; Create menu bar
    INVOKE CreateMenu
    mov g_hMenuBar, rax
    test rax, rax
    jz @@fail
    
    ; Create File menu
    INVOKE CreatePopupMenu
    mov hMenuFile, rax
    INVOKE AppendMenu, hMenuFile, MF_STRING, ID_FILE_NEW, ADDR szNew
    INVOKE AppendMenu, hMenuFile, MF_STRING, ID_FILE_OPEN, ADDR szOpen
    INVOKE AppendMenu, hMenuFile, MF_SEPARATOR, 0, NULL
    INVOKE AppendMenu, hMenuFile, MF_STRING, ID_FILE_SAVE, ADDR szSave
    INVOKE AppendMenu, hMenuFile, MF_STRING, ID_FILE_SAVE_ALL, ADDR szSaveAll
    INVOKE AppendMenu, hMenuFile, MF_SEPARATOR, 0, NULL
    INVOKE AppendMenu, hMenuFile, MF_STRING, ID_FILE_EXIT, ADDR szExit
    INVOKE AppendMenu, g_hMenuBar, MF_POPUP, hMenuFile, ADDR szFile
    
    ; Create Edit menu
    INVOKE CreatePopupMenu
    mov hMenuEdit, rax
    INVOKE AppendMenu, hMenuEdit, MF_STRING, ID_EDIT_UNDO, ADDR szUndo
    INVOKE AppendMenu, hMenuEdit, MF_STRING, ID_EDIT_REDO, ADDR szRedo
    INVOKE AppendMenu, hMenuEdit, MF_SEPARATOR, 0, NULL
    INVOKE AppendMenu, hMenuEdit, MF_STRING, ID_EDIT_FIND, ADDR szFind
    INVOKE AppendMenu, hMenuEdit, MF_STRING, ID_EDIT_REPLACE, ADDR szReplace
    INVOKE AppendMenu, g_hMenuBar, MF_POPUP, hMenuEdit, ADDR szEdit
    
    ; Create Build menu
    INVOKE CreatePopupMenu
    mov hMenuBuild, rax
    INVOKE AppendMenu, hMenuBuild, MF_STRING, ID_BUILD_COMPILE, ADDR szCompile
    INVOKE AppendMenu, hMenuBuild, MF_STRING, ID_BUILD_RUN, ADDR szRun
    INVOKE AppendMenu, hMenuBuild, MF_STRING, ID_BUILD_DEBUG, ADDR szDebug
    INVOKE AppendMenu, g_hMenuBar, MF_POPUP, hMenuBuild, ADDR szBuild
    
    ; Create Agent menu
    INVOKE CreatePopupMenu
    mov hMenuAgent, rax
    INVOKE AppendMenu, hMenuAgent, MF_STRING, ID_AGENT_CHAT, ADDR szAgentChat
    INVOKE AppendMenu, hMenuAgent, MF_STRING, ID_AGENT_COMPLETE, ADDR szAgentComplete
    INVOKE AppendMenu, g_hMenuBar, MF_POPUP, hMenuAgent, ADDR szAgent
    
    ; Create Help menu
    INVOKE CreatePopupMenu
    mov hMenuHelp, rax
    INVOKE AppendMenu, hMenuHelp, MF_STRING, ID_HELP_ABOUT, ADDR szAbout
    INVOKE AppendMenu, g_hMenuBar, MF_POPUP, hMenuHelp, ADDR szHelp
    
    ; Attach to window
    INVOKE SetMenu, hParent, g_hMenuBar
    INVOKE DrawMenuBar, hParent
    
    xor rax, rax
    ret
    
@@fail:
    mov rax, -1
    ret
    
RawrXD_Menu_Create ENDP

;----------------------------------------------------------------------
; RawrXD_Menu_Destroy - Destroy menu
;----------------------------------------------------------------------
RawrXD_Menu_Destroy PROC
    test g_hMenuBar, g_hMenuBar
    jz @@ret
    INVOKE DestroyMenu, g_hMenuBar
    mov g_hMenuBar, NULL
@@ret:
    ret
RawrXD_Menu_Destroy ENDP

END
