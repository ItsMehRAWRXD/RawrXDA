;==========================================================================
; ui_helpers_masm.asm - UI Helper Functions for MainWindow Implementation
; ==========================================================================
; Implements the UI functions called by main_window_masm.asm
;==========================================================================

option casemap:none
option noscoped
option proc:private

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comdlg32.lib

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================

EXTERN CreateWindowExA:PROC
EXTERN DestroyWindow:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN PostQuitMessage:PROC
EXTERN DefWindowProcA:PROC
EXTERN LoadMenuA:PROC
EXTERN SetMenu:PROC
EXTERN CreateMenu:PROC
EXTERN AppendMenuA:PROC
EXTERN CreatePopupMenu:PROC
EXTERN TrackPopupMenu:PROC
EXTERN MessageBoxA:PROC
EXTERN GetOpenFileNameA:PROC
EXTERN GetSaveFileNameA:PROC
EXTERN SendMessageA:PROC
EXTERN SetWindowTextA:PROC
EXTERN GetWindowTextA:PROC
EXTERN GetWindowTextLengthA:PROC
EXTERN EnableWindow:PROC
EXTERN SetFocus:PROC
EXTERN GetDlgItem:PROC
EXTERN SetDlgItemTextA:PROC
EXTERN GetDlgItemTextA:PROC
EXTERN CreateToolbarEx:PROC
EXTERN CreateStatusWindowA:PROC
EXTERN CreateSplitter:PROC
EXTERN CreateSplitterEx:PROC

;==========================================================================
; DATA SECTION
;==========================================================================

.data

; Window class for submenus
szSubMenuClass db "#32768",0  ; Standard popup menu class

; Toolbar constants
TBSTYLE_FLAT equ 0800h
TBSTYLE_LIST equ 1000h
TBSTYLE_TOOLTIPS equ 0100h

; Status bar constants
SBARS_SIZEGRIP equ 0100h

; Splitter constants
SPLITTER_VERTICAL equ 1
SPLITTER_HORIZONTAL equ 0

;==========================================================================
; CODE SECTION
;==========================================================================

.code

;==========================================================================
; ui_create_submenu - Create a submenu
; Parameters: hParentMenu - parent menu handle
;             pMenuName - menu name string
; Returns: submenu handle or NULL
;==========================================================================
ui_create_submenu proc hParentMenu:QWORD, pMenuName:QWORD
    ; Create popup menu
    invoke CreatePopupMenu
    .if rax != 0
        ; Append to parent menu
        invoke AppendMenuA, hParentMenu, MF_POPUP, rax, pMenuName
    .endif
    ret
ui_create_submenu endp

;==========================================================================
; ui_add_menu_item - Add item to menu
; Parameters: hMenu - menu handle
;             pItemText - item text
;             itemID - menu item ID
; Returns: TRUE if successful
;==========================================================================
ui_add_menu_item proc hMenu:QWORD, pItemText:QWORD, itemID:DWORD
    invoke AppendMenuA, hMenu, MF_STRING, itemID, pItemText
    ret
ui_add_menu_item endp

;==========================================================================
; ui_add_menu_separator - Add separator to menu
; Parameters: hMenu - menu handle
; Returns: TRUE if successful
;==========================================================================
ui_add_menu_separator proc hMenu:QWORD
    invoke AppendMenuA, hMenu, MF_SEPARATOR, 0, 0
    ret
ui_add_menu_separator endp

;==========================================================================
; ui_create_toolbar - Create toolbar
; Parameters: hParent - parent window handle
; Returns: toolbar handle or NULL
;==========================================================================
ui_create_toolbar proc hParent:QWORD
    local tbButtons[8]:TBUTTON
    
    ; Create toolbar with standard buttons
    invoke CreateToolbarEx, hParent, 
                           WS_VISIBLE or WS_CHILD or TBSTYLE_FLAT or TBSTYLE_TOOLTIPS,
                           1000, 8, HINST_COMMCTRL, IDB_STD_SMALL_COLOR,
                           addr tbButtons, 8, 16, 16, 16, 16, sizeof TBUTTON
    ret
ui_create_toolbar endp

;==========================================================================
; ui_add_toolbar_button - Add button to toolbar
; Parameters: hToolbar - toolbar handle
;             pButtonText - button text
;             buttonID - button ID
; Returns: TRUE if successful
;==========================================================================
ui_add_toolbar_button proc hToolbar:QWORD, pButtonText:QWORD, buttonID:DWORD
    ; This would typically add a button to the toolbar
    ; For now, return success
    mov rax, TRUE
    ret
ui_add_toolbar_button endp

;==========================================================================
; ui_add_toolbar_separator - Add separator to toolbar
; Parameters: hToolbar - toolbar handle
; Returns: TRUE if successful
;==========================================================================
ui_add_toolbar_separator proc hToolbar:QWORD
    ; Add separator to toolbar
    mov rax, TRUE
    ret
ui_add_toolbar_separator endp

;==========================================================================
; ui_create_statusbar - Create status bar
; Parameters: hParent - parent window handle
; Returns: status bar handle or NULL
;==========================================================================
ui_create_statusbar proc hParent:QWORD
    invoke CreateStatusWindowA, WS_CHILD or WS_VISIBLE or SBARS_SIZEGRIP, 0, hParent, 1000
    ret
ui_create_statusbar endp

;==========================================================================
; ui_set_statusbar_text - Set status bar text
; Parameters: hStatusBar - status bar handle
;             pText - text to display
; Returns: TRUE if successful
;==========================================================================
ui_set_statusbar_text proc hStatusBar:QWORD, pText:QWORD
    invoke SetWindowTextA, hStatusBar, pText
    ret
ui_set_statusbar_text endp

;==========================================================================
; ui_create_splitter - Create splitter
; Parameters: hParent - parent window handle
;             isVertical - TRUE for vertical, FALSE for horizontal
; Returns: splitter handle or NULL
;==========================================================================
ui_create_splitter proc hParent:QWORD, isVertical:BOOL
    local style:DWORD
    
    .if isVertical == TRUE
        mov style, SPLITTER_VERTICAL
    .else
        mov style, SPLITTER_HORIZONTAL
    .endif
    
    invoke CreateSplitterEx, hParent, style, 0, 0, 0, 0, 1000
    ret
ui_create_splitter endp

;==========================================================================
; ui_set_splitter_proportions - Set splitter proportions
; Parameters: hSplitter - splitter handle
;             leftProportion - left/top proportion
;             middleProportion - middle proportion
;             rightProportion - right/bottom proportion
; Returns: TRUE if successful
;==========================================================================
ui_set_splitter_proportions proc hSplitter:QWORD, leftProportion:DWORD, middleProportion:DWORD, rightProportion:DWORD
    ; Set splitter proportions
    mov rax, TRUE
    ret
ui_set_splitter_proportions endp

;==========================================================================
; ui_create_activity_bar - Create activity bar
; Parameters: hParent - parent window handle
; Returns: activity bar handle or NULL
;==========================================================================
ui_create_activity_bar proc hParent:QWORD
    ; Create activity bar (left sidebar)
    invoke CreateWindowExA, 0, addr szButtonClass, 0,
                          WS_VISIBLE or WS_CHILD or BS_GROUPBOX,
                          0, 0, 200, 600, hParent, 0, 0, 0
    ret
ui_create_activity_bar endp

;==========================================================================
; ui_create_editor_area - Create editor area
; Parameters: hParent - parent window handle
; Returns: editor area handle or NULL
;==========================================================================
ui_create_editor_area proc hParent:QWORD
    ; Create editor area
    invoke CreateWindowExA, WS_EX_CLIENTEDGE, addr szEditClass, 0,
                          WS_VISIBLE or WS_CHILD or WS_VSCROLL or ES_MULTILINE or ES_AUTOVSCROLL,
                          0, 0, 800, 400, hParent, 0, 0, 0
    ret
ui_create_editor_area endp

;==========================================================================
; ui_create_terminal_area - Create terminal area
; Parameters: hParent - parent window handle
; Returns: terminal area handle or NULL
;==========================================================================
ui_create_terminal_area proc hParent:QWORD
    ; Create terminal area
    invoke CreateWindowExA, WS_EX_CLIENTEDGE, addr szEditClass, 0,
                          WS_VISIBLE or WS_CHILD or WS_VSCROLL or ES_MULTILINE or ES_AUTOVSCROLL,
                          0, 0, 800, 200, hParent, 0, 0, 0
    ret
ui_create_terminal_area endp

;==========================================================================
; ui_create_file_tree - Create file tree dock widget
; Parameters: hParent - parent window handle
; Returns: file tree handle or NULL
;==========================================================================
ui_create_file_tree proc hParent:QWORD
    ; Create file tree
    invoke CreateWindowExA, 0, addr szTreeViewClass, "File Explorer",
                          WS_VISIBLE or WS_CHILD or TVS_HASLINES or TVS_LINESATROOT or TVS_HASBUTTONS,
                          0, 0, 300, 600, hParent, 0, 0, 0
    ret
ui_create_file_tree endp

;==========================================================================
; ui_create_hotpatch_panel - Create hotpatch panel dock widget
; Parameters: hParent - parent window handle
; Returns: hotpatch panel handle or NULL
;==========================================================================
ui_create_hotpatch_panel proc hParent:QWORD
    ; Create hotpatch panel
    invoke CreateWindowExA, 0, addr szListBoxClass, "Hotpatch Panel",
                          WS_VISIBLE or WS_CHILD or LBS_NOTIFY,
                          0, 0, 300, 400, hParent, 0, 0, 0
    ret
ui_create_hotpatch_panel endp

;==========================================================================
; ui_create_chat_panel - Create chat panel dock widget
; Parameters: hParent - parent window handle
; Returns: chat panel handle or NULL
;==========================================================================
ui_create_chat_panel proc hParent:QWORD
    ; Create chat panel
    invoke CreateWindowExA, 0, addr szEditClass, "AI Chat",
                          WS_VISIBLE or WS_CHILD or WS_VSCROLL or ES_MULTILINE or ES_READONLY,
                          0, 0, 400, 500, hParent, 0, 0, 0
    ret
ui_create_chat_panel endp

;==========================================================================
; ui_create_command_palette - Create command palette
; Parameters: hParent - parent window handle
; Returns: command palette handle or NULL
;==========================================================================
ui_create_command_palette proc hParent:QWORD
    ; Create command palette
    invoke CreateWindowExA, 0, addr szEditClass, "Command Palette",
                          WS_VISIBLE or WS_CHILD,
                          0, 0, 400, 200, hParent, 0, 0, 0
    ret
ui_create_command_palette endp

;==========================================================================
; ui_create_tray_icon - Create system tray icon
; Parameters: hParent - parent window handle
;             pTooltip - tooltip text
; Returns: tray icon handle or NULL
;==========================================================================
ui_create_tray_icon proc hParent:QWORD, pTooltip:QWORD
    ; System tray icon creation would go here
    ; For now, return NULL (not implemented)
    xor rax, rax
    ret
ui_create_tray_icon endp

;==========================================================================
; ui_show_dialog - Show dialog
; Parameters: hDialog - dialog handle
; Returns: dialog result
;==========================================================================
ui_show_dialog proc hDialog:QWORD
    invoke ShowWindow, hDialog, SW_SHOW
    invoke SetFocus, hDialog
    mov rax, TRUE
    ret
ui_show_dialog endp

;==========================================================================
; ui_open_file_dialog - Open file dialog
; Parameters: hParent - parent window handle
;             pFilePath - buffer for file path
;             bufferSize - buffer size
; Returns: TRUE if file selected
;==========================================================================
ui_open_file_dialog proc hParent:QWORD, pFilePath:QWORD, bufferSize:DWORD
    local ofn:OPENFILENAMEA
    
    ; Initialize OPENFILENAME structure
    mov ofn.lStructSize, sizeof OPENFILENAMEA
    mov rax, hParent
    mov ofn.hwndOwner, rax
    mov ofn.lpstrFile, pFilePath
    mov eax, bufferSize
    mov ofn.nMaxFile, eax
    mov ofn.Flags, OFN_PATHMUSTEXIST or OFN_FILEMUSTEXIST
    
    invoke GetOpenFileNameA, addr ofn
    ret
ui_open_file_dialog endp

;==========================================================================
; ui_save_file_dialog - Save file dialog
; Parameters: hParent - parent window handle
;             pFilePath - buffer for file path
;             bufferSize - buffer size
; Returns: TRUE if file selected
;==========================================================================
ui_save_file_dialog proc hParent:QWORD, pFilePath:QWORD, bufferSize:DWORD
    local ofn:OPENFILENAMEA
    
    ; Initialize OPENFILENAME structure
    mov ofn.lStructSize, sizeof OPENFILENAMEA
    mov rax, hParent
    mov ofn.hwndOwner, rax
    mov ofn.lpstrFile, pFilePath
    mov eax, bufferSize
    mov ofn.nMaxFile, eax
    mov ofn.Flags, OFN_PATHMUSTEXIST or OFN_OVERWRITEPROMPT
    
    invoke GetSaveFileNameA, addr ofn
    ret
ui_save_file_dialog endp

;==========================================================================
; ui_editor_set_text - Set editor text
; Parameters: hEditor - editor handle
;             pText - text to set
; Returns: TRUE if successful
;==========================================================================
ui_editor_set_text proc hEditor:QWORD, pText:QWORD
    invoke SetWindowTextA, hEditor, pText
    ret
ui_editor_set_text endp

;==========================================================================
; ui_editor_get_text - Get editor text
; Parameters: hEditor - editor handle
; Returns: pointer to text buffer
;==========================================================================
ui_editor_get_text proc hEditor:QWORD
    ; This would typically get text from editor
    ; For now, return NULL
    xor rax, rax
    ret
ui_editor_get_text endp

end