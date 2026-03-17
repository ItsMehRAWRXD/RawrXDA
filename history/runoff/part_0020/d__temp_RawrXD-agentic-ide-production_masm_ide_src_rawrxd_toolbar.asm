;======================================================================
; RawrXD IDE - Toolbar Component
; Button controls, dropdown menus, icon handling, event handlers
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
g_hToolbar              DQ ?
g_hToolbarImageList     DQ ?
g_toolbarHeight         DQ 32
g_buttonSize            DQ 24

; Toolbar button IDs
BTN_NEW_FILE            EQU 1001
BTN_OPEN_FILE           EQU 1002
BTN_SAVE_FILE           EQU 1003
BTN_SAVE_ALL            EQU 1004
BTN_CLOSE_FILE          EQU 1005
BTN_CUT                 EQU 1006
BTN_COPY                EQU 1007
BTN_PASTE               EQU 1008
BTN_UNDO                EQU 1009
BTN_REDO                EQU 1010
BTN_BUILD               EQU 1011
BTN_BUILD_RUN           EQU 1012
BTN_STOP                EQU 1013
BTN_DEBUG               EQU 1014
BTN_SETTINGS            EQU 1015
BTN_HELP                EQU 1016

.CODE

;----------------------------------------------------------------------
; RawrXD_Toolbar_Create - Create toolbar with standard buttons
;----------------------------------------------------------------------
RawrXD_Toolbar_Create PROC hParent:QWORD
    LOCAL hImageList:QWORD
    LOCAL rect:RECT
    
    ; Create toolbar control
    INVOKE CreateWindowEx,
        0,
        "ToolbarWindow32",
        NULL,
        WS_CHILD OR WS_VISIBLE OR TBSTYLE_FLAT OR TBSTYLE_TOOLTIPS,
        0, 0, 0, g_toolbarHeight,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hToolbar, rax
    test rax, rax
    jz @@fail
    
    ; Initialize common controls
    INVOKE InitCommonControlsEx, ADDR icc
    
    ; Create image list for toolbar (24x24 icons)
    INVOKE ImageList_Create, 24, 24, ILC_COLOR32, 16, 4
    mov hImageList, rax
    mov g_hToolbarImageList, rax
    
    ; Attach image list to toolbar
    INVOKE SendMessage, g_hToolbar, TB_SETIMAGELIST, 0, hImageList
    
    ; Set button size
    INVOKE SendMessage, g_hToolbar, TB_SETBUTTONSIZE, 0, 00180018h  ; 24x24
    
    ; Add buttons
    INVOKE RawrXD_Toolbar_AddButtons
    
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_Toolbar_Create ENDP

;----------------------------------------------------------------------
; RawrXD_Toolbar_AddButtons - Add all toolbar buttons
;----------------------------------------------------------------------
RawrXD_Toolbar_AddButtons PROC
    LOCAL buttons[20]:TBBUTTON
    LOCAL idx:QWORD
    
    ; Initialize button array
    mov idx, 0
    
    ; File operations group
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_NEW_FILE, 0
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_OPEN_FILE, 1
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_SAVE_FILE, 2
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_SAVE_ALL, 3
    INVOKE RawrXD_Toolbar_AddSeparator, ADDR buttons, ADDR idx
    
    ; Edit operations
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_CUT, 4
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_COPY, 5
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_PASTE, 6
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_UNDO, 7
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_REDO, 8
    INVOKE RawrXD_Toolbar_AddSeparator, ADDR buttons, ADDR idx
    
    ; Build operations
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_BUILD, 9
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_BUILD_RUN, 10
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_STOP, 11
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_DEBUG, 12
    INVOKE RawrXD_Toolbar_AddSeparator, ADDR buttons, ADDR idx
    
    ; Settings
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_SETTINGS, 13
    INVOKE RawrXD_Toolbar_AddButton, ADDR buttons, ADDR idx, BTN_HELP, 14
    
    ; Add all buttons to toolbar
    mov eax, idx
    INVOKE SendMessage, g_hToolbar, TB_ADDBUTTONS, rax, ADDR buttons
    
    ; Auto-resize toolbar
    INVOKE SendMessage, g_hToolbar, TB_AUTOSIZE, 0, 0
    
    ret
    
RawrXD_Toolbar_AddButtons ENDP

;----------------------------------------------------------------------
; RawrXD_Toolbar_AddButton - Add single button to array
;----------------------------------------------------------------------
RawrXD_Toolbar_AddButton PROC pButtons:QWORD, pIdx:QWORD, buttonId:QWORD, imageIdx:QWORD
    LOCAL pBtn:QWORD
    LOCAL idx:QWORD
    
    ; Get current index
    mov rax, pIdx
    mov idx, [rax]
    
    ; Calculate button pointer
    mov rax, pButtons
    mov rcx, idx
    imul rcx, SIZEOF TBBUTTON
    add rax, rcx
    mov pBtn, rax
    
    ; Fill button structure
    mov rcx, pBtn
    mov TBBUTTON.idCommand[rcx], buttonId
    mov TBBUTTON.iBitmap[rcx], imageIdx
    mov TBBUTTON.fsState[rcx], TBSTATE_ENABLED
    mov TBBUTTON.fsStyle[rcx], TBSTYLE_BUTTON
    mov TBBUTTON.dwData[rcx], 0
    mov TBBUTTON.iString[rcx], -1
    
    ; Increment index
    mov rax, pIdx
    mov rcx, [rax]
    inc rcx
    mov [rax], rcx
    
    ret
    
RawrXD_Toolbar_AddButton ENDP

;----------------------------------------------------------------------
; RawrXD_Toolbar_AddSeparator - Add separator to button array
;----------------------------------------------------------------------
RawrXD_Toolbar_AddSeparator PROC pButtons:QWORD, pIdx:QWORD
    LOCAL pBtn:QWORD
    LOCAL idx:QWORD
    
    ; Get current index
    mov rax, pIdx
    mov idx, [rax]
    
    ; Calculate button pointer
    mov rax, pButtons
    mov rcx, idx
    imul rcx, SIZEOF TBBUTTON
    add rax, rcx
    mov pBtn, rax
    
    ; Fill button structure as separator
    mov rcx, pBtn
    mov TBBUTTON.idCommand[rcx], 0
    mov TBBUTTON.iBitmap[rcx], 0
    mov TBBUTTON.fsState[rcx], 0
    mov TBBUTTON.fsStyle[rcx], TBSTYLE_SEP
    mov TBBUTTON.dwData[rcx], 0
    mov TBBUTTON.iString[rcx], 0
    
    ; Increment index
    mov rax, pIdx
    mov rcx, [rax]
    inc rcx
    mov [rax], rcx
    
    ret
    
RawrXD_Toolbar_AddSeparator ENDP

;----------------------------------------------------------------------
; RawrXD_Toolbar_OnClick - Handle toolbar button click
;----------------------------------------------------------------------
RawrXD_Toolbar_OnClick PROC buttonId:QWORD
    ; Dispatch to appropriate handler based on button ID
    
    cmp buttonId, BTN_NEW_FILE
    je @@new_file
    
    cmp buttonId, BTN_OPEN_FILE
    je @@open_file
    
    cmp buttonId, BTN_SAVE_FILE
    je @@save_file
    
    cmp buttonId, BTN_SAVE_ALL
    je @@save_all
    
    cmp buttonId, BTN_CLOSE_FILE
    je @@close_file
    
    cmp buttonId, BTN_CUT
    je @@cut
    
    cmp buttonId, BTN_COPY
    je @@copy
    
    cmp buttonId, BTN_PASTE
    je @@paste
    
    cmp buttonId, BTN_UNDO
    je @@undo
    
    cmp buttonId, BTN_REDO
    je @@redo
    
    cmp buttonId, BTN_BUILD
    je @@build
    
    cmp buttonId, BTN_BUILD_RUN
    je @@build_run
    
    cmp buttonId, BTN_STOP
    je @@stop
    
    cmp buttonId, BTN_DEBUG
    je @@debug
    
    cmp buttonId, BTN_SETTINGS
    je @@settings
    
    cmp buttonId, BTN_HELP
    je @@help
    
    jmp @@done
    
@@new_file:
    INVOKE RawrXD_Editor_NewFile
    jmp @@done
    
@@open_file:
    INVOKE RawrXD_Editor_OpenFileDialog
    jmp @@done
    
@@save_file:
    INVOKE RawrXD_Editor_SaveFile
    jmp @@done
    
@@save_all:
    INVOKE RawrXD_Editor_SaveAllFiles
    jmp @@done
    
@@close_file:
    INVOKE RawrXD_Editor_CloseFile
    jmp @@done
    
@@cut:
    INVOKE RawrXD_Editor_Cut
    jmp @@done
    
@@copy:
    INVOKE RawrXD_Editor_Copy
    jmp @@done
    
@@paste:
    INVOKE RawrXD_Editor_Paste
    jmp @@done
    
@@undo:
    INVOKE RawrXD_Editor_Undo
    jmp @@done
    
@@redo:
    INVOKE RawrXD_Editor_Redo
    jmp @@done
    
@@build:
    INVOKE RawrXD_Build_Compile
    jmp @@done
    
@@build_run:
    INVOKE RawrXD_Build_CompileAndRun
    jmp @@done
    
@@stop:
    INVOKE RawrXD_Build_Stop
    jmp @@done
    
@@debug:
    INVOKE RawrXD_Debug_Start
    jmp @@done
    
@@settings:
    INVOKE RawrXD_Settings_Show
    jmp @@done
    
@@help:
    INVOKE RawrXD_Help_Show
    jmp @@done
    
@@done:
    ret
    
RawrXD_Toolbar_OnClick ENDP

;----------------------------------------------------------------------
; RawrXD_Toolbar_SetButtonState - Enable/disable toolbar button
;----------------------------------------------------------------------
RawrXD_Toolbar_SetButtonState PROC buttonId:QWORD, state:QWORD
    ; state: 0 = disabled, 1 = enabled
    
    cmp state, 1
    je @@enable
    
    ; Disable button
    INVOKE SendMessage, g_hToolbar, TB_ENABLEBUTTON, buttonId, 0
    jmp @@done
    
@@enable:
    INVOKE SendMessage, g_hToolbar, TB_ENABLEBUTTON, buttonId, 1
    
@@done:
    ret
    
RawrXD_Toolbar_SetButtonState ENDP

;----------------------------------------------------------------------
; RawrXD_Toolbar_GetHeight - Get toolbar height
;----------------------------------------------------------------------
RawrXD_Toolbar_GetHeight PROC
    mov rax, g_toolbarHeight
    ret
RawrXD_Toolbar_GetHeight ENDP

;----------------------------------------------------------------------
; RawrXD_Toolbar_Resize - Resize toolbar on parent resize
;----------------------------------------------------------------------
RawrXD_Toolbar_Resize PROC hParent:QWORD, cx:QWORD
    ; Resize toolbar to parent width
    INVOKE MoveWindow, g_hToolbar, 0, 0, cx, g_toolbarHeight, TRUE
    
    ; Auto-resize content
    INVOKE SendMessage, g_hToolbar, TB_AUTOSIZE, 0, 0
    
    ret
    
RawrXD_Toolbar_Resize ENDP

END
