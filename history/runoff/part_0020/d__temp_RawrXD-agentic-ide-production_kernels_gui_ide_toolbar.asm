; ============================================================================
; ide_toolbar.asm - Toolbar with agent controls for quick access
; ============================================================================

option casemap:none

; ----------------------------------------------------------------------------
; EXTERNALS
; ----------------------------------------------------------------------------
EXTERN CreateWindowExA:PROC
EXTERN SendMessageA:PROC
EXTERN hMainWnd:QWORD
EXTERN hInstance:QWORD

EXTERN IDEMenu_OnCommand:PROC

; ----------------------------------------------------------------------------
; CONSTANTS
; ----------------------------------------------------------------------------
NULL                equ 0
WS_CHILD            equ 40000000h
WS_VISIBLE          equ 10000000h
TBSTYLE_FLAT        equ 0800h
TBSTYLE_TOOLTIPS    equ 0100h
TBSTYLE_WRAPABLE    equ 0200h
TB_ADDBUTTONS       equ 0414h
TB_AUTOSIZE         equ 0410h
TB_SETBUTTONSIZE    equ 0411h
TB_SETBITMAPSIZE    equ 040Bh

; Button IDs (match menu IDs for consistency)
IDTB_NEW            equ 1001
IDTB_OPEN           equ 1002
IDTB_SAVE           equ 1003
IDTB_UNDO           equ 2001
IDTB_REDO           equ 2002
IDTB_AGENT          equ 5001
IDTB_PRIVACY        equ 5003
IDTB_SETTINGS       equ 4003

; ----------------------------------------------------------------------------
; STRUCTURES
; ----------------------------------------------------------------------------
TBBUTTON STRUCT
    iBitmap          dd ?
    idCommand        dd ?
    fsState          db ?
    fsStyle          db ?
    bReserved        db 2 dup(?)
    dwData           dq ?
    iString          dq ?
TBBUTTON ENDS

; ----------------------------------------------------------------------------
; PUBLICS
; ----------------------------------------------------------------------------
PUBLIC IDEToolbar_Create
PUBLIC hToolbar

; ----------------------------------------------------------------------------
; DATA
; ----------------------------------------------------------------------------
.data
hToolbar            dq 0
szToolbarClass      db "ToolbarWindow32",0

; Button definitions
tbButtons TBBUTTON 9 dup(<?>)

; ----------------------------------------------------------------------------
; CODE
; ----------------------------------------------------------------------------
.code

IDEToolbar_Create PROC
    ; Create toolbar window
    invoke CreateWindowExA, 0, ADDR szToolbarClass, NULL,
        WS_CHILD or WS_VISIBLE or TBSTYLE_FLAT or TBSTYLE_TOOLTIPS or TBSTYLE_WRAPABLE,
        0, 0, 0, 0, hMainWnd, NULL, hInstance, NULL
    mov hToolbar, rax

    ; Configure toolbar
    invoke SendMessageA, hToolbar, TB_SETBITMAPSIZE, 0, 0  ; 16x16 icons
    invoke SendMessageA, hToolbar, TB_SETBUTTONSIZE, 0, 000000200020h  ; 32x32 buttons

    ; Initialize button array
    ; New button
    mov tbButtons[0*sizeof TBBUTTON].iBitmap, 0
    mov tbButtons[0*sizeof TBBUTTON].idCommand, IDTB_NEW
    mov tbButtons[0*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[0*sizeof TBBUTTON].fsStyle, TBSTYLE_BUTTON
    mov tbButtons[0*sizeof TBBUTTON].dwData, 0
    mov tbButtons[0*sizeof TBBUTTON].iString, 0

    ; Open button
    mov tbButtons[1*sizeof TBBUTTON].iBitmap, 1
    mov tbButtons[1*sizeof TBBUTTON].idCommand, IDTB_OPEN
    mov tbButtons[1*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[1*sizeof TBBUTTON].fsStyle, TBSTYLE_BUTTON
    mov tbButtons[1*sizeof TBBUTTON].dwData, 0
    mov tbButtons[1*sizeof TBBUTTON].iString, 0

    ; Save button
    mov tbButtons[2*sizeof TBBUTTON].iBitmap, 2
    mov tbButtons[2*sizeof TBBUTTON].idCommand, IDTB_SAVE
    mov tbButtons[2*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[2*sizeof TBBUTTON].fsStyle, TBSTYLE_BUTTON
    mov tbButtons[2*sizeof TBBUTTON].dwData, 0
    mov tbButtons[2*sizeof TBBUTTON].iString, 0

    ; Separator
    mov tbButtons[3*sizeof TBBUTTON].iBitmap, 0
    mov tbButtons[3*sizeof TBBUTTON].idCommand, 0
    mov tbButtons[3*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[3*sizeof TBBUTTON].fsStyle, TBSTYLE_SEP
    mov tbButtons[3*sizeof TBBUTTON].dwData, 0
    mov tbButtons[3*sizeof TBBUTTON].iString, 0

    ; Undo button
    mov tbButtons[4*sizeof TBBUTTON].iBitmap, 3
    mov tbButtons[4*sizeof TBBUTTON].idCommand, IDTB_UNDO
    mov tbButtons[4*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[4*sizeof TBBUTTON].fsStyle, TBSTYLE_BUTTON
    mov tbButtons[4*sizeof TBBUTTON].dwData, 0
    mov tbButtons[4*sizeof TBBUTTON].iString, 0

    ; Redo button
    mov tbButtons[5*sizeof TBBUTTON].iBitmap, 4
    mov tbButtons[5*sizeof TBBUTTON].idCommand, IDTB_REDO
    mov tbButtons[5*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[5*sizeof TBBUTTON].fsStyle, TBSTYLE_BUTTON
    mov tbButtons[5*sizeof TBBUTTON].dwData, 0
    mov tbButtons[5*sizeof TBBUTTON].iString, 0

    ; Separator
    mov tbButtons[6*sizeof TBBUTTON].iBitmap, 0
    mov tbButtons[6*sizeof TBBUTTON].idCommand, 0
    mov tbButtons[6*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[6*sizeof TBBUTTON].fsStyle, TBSTYLE_SEP
    mov tbButtons[6*sizeof TBBUTTON].dwData, 0
    mov tbButtons[6*sizeof TBBUTTON].iString, 0

    ; Agent button
    mov tbButtons[7*sizeof TBBUTTON].iBitmap, 5
    mov tbButtons[7*sizeof TBBUTTON].idCommand, IDTB_AGENT
    mov tbButtons[7*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[7*sizeof TBBUTTON].fsStyle, TBSTYLE_BUTTON
    mov tbButtons[7*sizeof TBBUTTON].dwData, 0
    mov tbButtons[7*sizeof TBBUTTON].iString, 0

    ; Privacy button
    mov tbButtons[8*sizeof TBBUTTON].iBitmap, 6
    mov tbButtons[8*sizeof TBBUTTON].idCommand, IDTB_PRIVACY
    mov tbButtons[8*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[8*sizeof TBBUTTON].fsStyle, TBSTYLE_BUTTON
    mov tbButtons[8*sizeof TBBUTTON].dwData, 0
    mov tbButtons[8*sizeof TBBUTTON].iString, 0

    ; Settings button
    mov tbButtons[9*sizeof TBBUTTON].iBitmap, 7
    mov tbButtons[9*sizeof TBBUTTON].idCommand, IDTB_SETTINGS
    mov tbButtons[9*sizeof TBBUTTON].fsState, TBSTATE_ENABLED
    mov tbButtons[9*sizeof TBBUTTON].fsStyle, TBSTYLE_BUTTON
    mov tbButtons[9*sizeof TBBUTTON].dwData, 0
    mov tbButtons[9*sizeof TBBUTTON].iString, 0

    ; Add buttons to toolbar
    invoke SendMessageA, hToolbar, TB_ADDBUTTONS, 10, ADDR tbButtons

    ; Auto-size toolbar
    invoke SendMessageA, hToolbar, TB_AUTOSIZE, 0, 0

    ret
IDEToolbar_Create ENDP

END