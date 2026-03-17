; ============================================================================
; RawrXD Agentic IDE - Tab Control Implementation (Pure MASM)
; Multi-tab interface for editor - Fixed version
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

; External globals from engine/window
extern g_hInstance:DWORD
extern g_hMainWindow:DWORD
extern g_hMainFont:DWORD

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szTabClass         db "SysTabControl32", 0
    szNewTab           db "New File", 0
    szModified         db " *", 0
    
    ; Tab state
    g_hTabControl      dd 0
    g_dwTabCount       dd 0
    g_pTabData         dd 0
    g_dwCurrentTab     dd 0

.data?
    szCurrentText      db 260 dup(?)
    szNewText          db 260 dup(?)

; ============================================================================
; TCITEM structure (local definition to avoid conflicts)
; ============================================================================
TC_ITEM struct
    imask       dd ?
    dwState     dd ?
    dwStateMask dd ?
    pszText     dd ?
    cchTextMax  dd ?
    iImage      dd ?
    lParam      dd ?
TC_ITEM ends

; Constants
TCIF_TEXT           equ 0001h
TCS_FIXEDWIDTH      equ 0400h
TCS_FOCUSNEVER      equ 8000h
TCS_OWNERDRAWFIXED  equ 2000h
TCM_INSERTITEM      equ 1307h
TCM_DELETEITEM      equ 1308h
TCM_SETCURSEL       equ 130Ch
TCM_GETCURSEL       equ 130Bh
TCM_SETITEM         equ 1306h
TCM_GETITEM         equ 1305h

; Control IDs
IDC_TABCONTROL      equ 1001

; ============================================================================
; PROCEDURES
; ============================================================================

.code

public CreateTabControl

; ============================================================================
; CreateTabControl - Create tab control
; Returns: Tab handle in eax
; ============================================================================
CreateTabControl proc
    LOCAL dwStyle:DWORD
    LOCAL hTab:DWORD
    LOCAL tci:TC_ITEM
    
    ; Create tab control with close buttons support
    mov dwStyle, WS_CHILD or WS_VISIBLE or TCS_FIXEDWIDTH or TCS_FOCUSNEVER
    
    ; Ensure common controls are initialized
    invoke InitCommonControls
    
    push NULL
    push g_hInstance
    push IDC_TABCONTROL
    push g_hMainWindow
    push 400
    push 600
    push 0
    push 0
    push dwStyle
    push NULL
    push offset szTabClass
    push 0
    call CreateWindowEx
    mov hTab, eax
    mov g_hTabControl, eax
    
    cmp eax, 0
    je @fail
    
    ; Set font if available
    cmp g_hMainFont, 0
    je @skipFont
    push TRUE
    push g_hMainFont
    push WM_SETFONT
    push hTab
    call SendMessage
@skipFont:
    
    ; Add initial tab
    mov tci.imask, TCIF_TEXT
    mov tci.pszText, offset szNewTab
    mov tci.cchTextMax, 0
    mov tci.iImage, 0
    mov tci.lParam, 0
    
    lea eax, tci
    push eax
    push 0
    push TCM_INSERTITEM
    push hTab
    call SendMessage
    
    mov g_dwTabCount, 1
    mov g_dwCurrentTab, 0
    
    mov eax, hTab
    ret

@fail:
    xor eax, eax
    ret
CreateTabControl endp

; ============================================================================
; TabControl_AddTab - Add new tab
; Input: pszFileName
; Returns: Tab index in eax
; ============================================================================
TabControl_AddTab proc pszFileName:DWORD
    LOCAL tci:TC_ITEM
    LOCAL dwIndex:DWORD
    
    mov tci.imask, TCIF_TEXT
    mov eax, pszFileName
    mov tci.pszText, eax
    mov tci.cchTextMax, 0
    mov tci.iImage, 0
    mov tci.lParam, 0
    
    lea eax, tci
    push eax
    push g_dwTabCount
    push TCM_INSERTITEM
    push g_hTabControl
    call SendMessage
    mov dwIndex, eax
    
    inc g_dwTabCount
    
    mov eax, dwIndex
    ret
TabControl_AddTab endp

; ============================================================================
; TabControl_CloseTab - Close tab
; Input: dwIndex
; ============================================================================
TabControl_CloseTab proc dwIndex:DWORD
    push 0
    push dwIndex
    push TCM_DELETEITEM
    push g_hTabControl
    call SendMessage
    
    dec g_dwTabCount
    ret
TabControl_CloseTab endp

; ============================================================================
; TabControl_SetCurrentTab - Set current tab
; Input: dwIndex
; ============================================================================
TabControl_SetCurrentTab proc dwIndex:DWORD
    push 0
    push dwIndex
    push TCM_SETCURSEL
    push g_hTabControl
    call SendMessage
    
    mov eax, dwIndex
    mov g_dwCurrentTab, eax
    ret
TabControl_SetCurrentTab endp

; ============================================================================
; TabControl_GetCurrentTab - Get current tab index
; Returns: Index in eax
; ============================================================================
TabControl_GetCurrentTab proc
    push 0
    push 0
    push TCM_GETCURSEL
    push g_hTabControl
    call SendMessage
    
    mov g_dwCurrentTab, eax
    ret
TabControl_GetCurrentTab endp

; ============================================================================
; TabControl_SetTabText - Set tab text
; Input: dwIndex, pszText
; ============================================================================
TabControl_SetTabText proc dwIndex:DWORD, pszText:DWORD
    LOCAL tci:TC_ITEM
    
    mov tci.imask, TCIF_TEXT
    mov eax, pszText
    mov tci.pszText, eax
    
    lea eax, tci
    push eax
    push dwIndex
    push TCM_SETITEM
    push g_hTabControl
    call SendMessage
    ret
TabControl_SetTabText endp

; ============================================================================
; TabControl_MarkModified - Mark tab as modified (simplified)
; Input: dwIndex, bModified
; ============================================================================
TabControl_MarkModified proc dwIndex:DWORD, bModified:DWORD
    LOCAL tci:TC_ITEM
    
    ; Get current text
    mov tci.imask, TCIF_TEXT
    mov tci.pszText, offset szCurrentText
    mov tci.cchTextMax, 259
    
    lea eax, tci
    push eax
    push dwIndex
    push TCM_GETITEM
    push g_hTabControl
    call SendMessage
    
    ; Copy to new text buffer
    invoke lstrcpy, offset szNewText, offset szCurrentText
    
    ; If modified, append marker
    cmp bModified, 0
    je @setText
    invoke lstrcat, offset szNewText, offset szModified
    
@setText:
    ; Set new text
    mov tci.imask, TCIF_TEXT
    mov tci.pszText, offset szNewText
    
    lea eax, tci
    push eax
    push dwIndex
    push TCM_SETITEM
    push g_hTabControl
    call SendMessage
    ret
TabControl_MarkModified endp

; ============================================================================
; TabControl_GetTabCount - Get number of tabs
; Returns: Count in eax
; ============================================================================
TabControl_GetTabCount proc
    mov eax, g_dwTabCount
    ret
TabControl_GetTabCount endp

; ============================================================================
; TabControl_Cleanup - Cleanup tab resources
; ============================================================================
TabControl_Cleanup proc
    cmp g_hTabControl, 0
    je @noWnd
    invoke DestroyWindow, g_hTabControl
    mov g_hTabControl, 0
@noWnd:
    cmp g_pTabData, 0
    je @noData
    invoke GlobalFree, g_pTabData
    mov g_pTabData, 0
@noData:
    ret
TabControl_Cleanup endp

end
