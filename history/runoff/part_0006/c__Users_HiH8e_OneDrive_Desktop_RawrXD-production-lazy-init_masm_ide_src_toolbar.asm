; ============================================================================
; RawrXD Agentic IDE - Toolbar Implementation (Pure MASM)
; Complete toolbar with all essential IDE functions
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib

.data
include constants.inc

extrn g_hInstance:DWORD
extrn g_hMainWindow:DWORD

; Toolbar button IDs (must match menu IDs)
TB_NEW          equ 1001
TB_OPEN         equ 1002  
TB_SAVE         equ 1003
TB_SEPARATOR1   equ 0
TB_WISH         equ 2001
TB_LOOP         equ 2002
TB_SEPARATOR2   equ 0
TB_REGISTRY     equ 3001
TB_GGUF         equ 3002
TB_SEPARATOR3   equ 0
TB_FLOATING     equ 4001
TB_REFRESH      equ 4002

; Toolbar data
    szToolbarClass  db "ToolbarWindow32",0
    szTooltipNew    db "New File (Ctrl+N)",0
    szTooltipOpen   db "Open File (Ctrl+O)",0
    szTooltipSave   db "Save File (Ctrl+S)",0
    szTooltipWish   db "Magic Wand - Make a Wish",0
    szTooltipLoop   db "Start Autonomous Loop",0
    szTooltipRegistry db "Tool Registry",0
    szTooltipGGUF   db "Load GGUF Model",0
    szTooltipFloating db "Toggle Floating Panel",0
    szTooltipRefresh db "Refresh File Tree",0
    
    g_hToolbar      dd 0
    g_hTooltip      dd 0

; Button bitmaps (using system standard icons for now)
    g_nButtons      dd 10

.data?
    tbButtons       TBBUTTON 10 dup(<>)

.code

; Forward declarations
CreateToolbar proto
AddToolbarButton proto :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CreateTooltips proto

; ============================================================================
; CreateToolbar - Create complete toolbar
; Returns: Toolbar handle in eax
; ============================================================================
CreateToolbar proc
    LOCAL dwStyle:DWORD
    LOCAL dwExStyle:DWORD
    
    ; Initialize common controls
    invoke InitCommonControls
    
    ; Create toolbar
    mov dwStyle, WS_CHILD or WS_VISIBLE or TBSTYLE_TOOLTIPS or TBSTYLE_FLAT or CCS_ADJUSTABLE
    mov dwExStyle, 0
    
    invoke CreateWindowEx, dwExStyle, addr szToolbarClass, NULL, dwStyle,
           0, 0, 0, 0, g_hMainWindow, IDC_TOOLBAR, g_hInstance, NULL
    mov g_hToolbar, eax
    
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Set button size
    invoke SendMessage, g_hToolbar, TB_SETBUTTONSIZE, 0, 001600020h  ; 32x22
    
    ; Set bitmap size  
    invoke SendMessage, g_hToolbar, TB_SETBITMAPSIZE, 0, 001000010h  ; 16x16
    
    ; Add standard system bitmap
    invoke SendMessage, g_hToolbar, TB_LOADIMAGES, IDB_STD_SMALL_COLOR, HINST_COMMCTRL
    
    ; Initialize button array
    call InitializeButtons
    
    ; Add buttons
    invoke SendMessage, g_hToolbar, TB_ADDBUTTONS, g_nButtons, addr tbButtons
    
    ; Auto size toolbar
    invoke SendMessage, g_hToolbar, TB_AUTOSIZE, 0, 0
    
    ; Create tooltips
    call CreateTooltips
    
    mov eax, g_hToolbar
    ret
CreateToolbar endp

; ============================================================================
; InitializeButtons - Setup button structures
; ============================================================================
InitializeButtons proc
    LOCAL pButton:DWORD
    
    lea eax, tbButtons
    mov pButton, eax
    
    ; New File button
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, STD_FILENEW
    mov [eax].TBBUTTON.idCommand, TB_NEW
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; Open File button
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, STD_FILEOPEN
    mov [eax].TBBUTTON.idCommand, TB_OPEN
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; Save File button
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, STD_FILESAVE
    mov [eax].TBBUTTON.idCommand, TB_SAVE
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; Separator
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, 0
    mov [eax].TBBUTTON.idCommand, TB_SEPARATOR1
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_SEP
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; Magic Wand button
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, STD_FIND
    mov [eax].TBBUTTON.idCommand, TB_WISH
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; Loop button
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, STD_REDOIT
    mov [eax].TBBUTTON.idCommand, TB_LOOP
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; Separator
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, 0
    mov [eax].TBBUTTON.idCommand, TB_SEPARATOR2
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_SEP
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; Registry button
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, STD_PROPERTIES
    mov [eax].TBBUTTON.idCommand, TB_REGISTRY
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; GGUF Model button
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, STD_CONNECT
    mov [eax].TBBUTTON.idCommand, TB_GGUF
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    add pButton, sizeof TBBUTTON
    
    ; Refresh button
    mov eax, pButton
    mov [eax].TBBUTTON.iBitmap, STD_REFRESH
    mov [eax].TBBUTTON.idCommand, TB_REFRESH
    mov [eax].TBBUTTON.fsState, TBSTATE_ENABLED
    mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
    mov [eax].TBBUTTON.dwData, 0
    mov [eax].TBBUTTON.iString, 0
    
    ret
InitializeButtons endp

; ============================================================================
; CreateTooltips - Create tooltip control for toolbar
; ============================================================================
CreateTooltips proc
    LOCAL ti:TOOLINFO
    
    ; Tooltips are automatically created by the toolbar with TBSTYLE_TOOLTIPS
    ; But we can enhance them here if needed
    
    ret
CreateTooltips endp

; ============================================================================
; ResizeToolbar - Resize toolbar when window resizes  
; ============================================================================
ResizeToolbar proc
    .if g_hToolbar != 0
        invoke SendMessage, g_hToolbar, TB_AUTOSIZE, 0, 0
    .endif
    ret
ResizeToolbar endp

; ============================================================================
; EnableToolbarButton - Enable/disable specific toolbar button
; ============================================================================
EnableToolbarButton proc nButtonID:DWORD, bEnable:DWORD
    .if g_hToolbar != 0
        .if bEnable
            invoke SendMessage, g_hToolbar, TB_ENABLEBUTTON, nButtonID, TRUE
        .else
            invoke SendMessage, g_hToolbar, TB_ENABLEBUTTON, nButtonID, FALSE
        .endif
    .endif
    ret
EnableToolbarButton endp

; ============================================================================
; SetToolbarButtonState - Set button state (pressed, checked, etc)
; ============================================================================
SetToolbarButtonState proc nButtonID:DWORD, dwState:DWORD
    .if g_hToolbar != 0
        invoke SendMessage, g_hToolbar, TB_SETSTATE, nButtonID, dwState
    .endif
    ret
SetToolbarButtonState endp

; ============================================================================
; GetToolbarHeight - Get toolbar height for layout calculations
; ============================================================================
GetToolbarHeight proc
    LOCAL rect:RECT
    
    .if g_hToolbar != 0
        invoke GetWindowRect, g_hToolbar, addr rect
        mov eax, rect.bottom
        sub eax, rect.top
        ret
    .endif
    
    mov eax, 28  ; Default height
    ret
GetToolbarHeight endp

; ============================================================================
; Public interface
; ============================================================================
public CreateToolbar
public ResizeToolbar
public EnableToolbarButton
public SetToolbarButtonState
public GetToolbarHeight

end