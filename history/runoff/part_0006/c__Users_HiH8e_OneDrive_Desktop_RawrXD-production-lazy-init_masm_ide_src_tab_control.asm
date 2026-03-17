; ============================================================================
; RawrXD Agentic IDE - Tab Control Implementation (Pure MASM)
; Multi-tab interface for editor
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc
include \masm32\include\gdi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib
includelib \masm32\lib\gdi32.lib

; External globals from engine/window
extern g_hInstance:DWORD
extern g_hMainWindow:DWORD
extern g_hMainFont:DWORD

include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szTabClass         db "SysTabControl32", 0
    szNewTab           db "New File", 0
    szModified         db " *", 0
    szCloseButton      db "×", 0  ; Unicode multiplication sign for close button
    
    ; Tab state
    g_hTabControl      dd 0
    g_dwTabCount       dd 0
    g_pTabData         dd 0
    g_dwCurrentTab     dd 0
    
    ; Close button constants
    CLOSE_BUTTON_SIZE  equ 16
    CLOSE_BUTTON_MARGIN equ 4
    CLOSE_BUTTON_COLOR dd 00000000h  ; Black
    CLOSE_BUTTON_HOVER_COLOR dd 00FF0000h  ; Red
    
    ; Tab data structure
    TAB_DATA struct
        hEditor        dd ?
        szFileName     db MAX_PATH dup(?)
        bModified      dd ?
    TAB_DATA ends

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; CreateTabControl - Create tab control
; Returns: Tab handle in eax
; ============================================================================
CreateTabControl proc
    LOCAL dwStyle:DWORD
    LOCAL hTab:DWORD
    LOCAL tci:TCITEM
    
    ; Create tab control with close buttons support
        mov dwStyle, WS_CHILD or WS_VISIBLE or TCS_FIXEDWIDTH or TCS_FOCUSNEVER or TCS_OWNERDRAWFIXED

        ; Ensure common controls are initialized (for SysTabControl32)
        invoke InitCommonControls
    
    invoke CreateWindowEx, 0, addr szTabClass, NULL, dwStyle, 0, 0, 600, 400, g_hMainWindow, IDC_TABCONTROL, g_hInstance, NULL
    mov hTab, eax
    mov g_hTabControl, eax
    
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Set font
    invoke SendMessage, hTab, WM_SETFONT, g_hMainFont, TRUE
    
    ; Add initial tab
    mov tci.mask, TCIF_TEXT
    mov tci.pszText, offset szNewTab
    
    invoke SendMessage, hTab, TCM_INSERTITEM, 0, addr tci
    mov g_dwTabCount, 1
    mov g_dwCurrentTab, 0
    
    mov eax, hTab
    ret
CreateTabControl endp

; ============================================================================
; TabControl_AddTab - Add new tab
; Input: pszFileName
; Returns: Tab index in eax
; ============================================================================
TabControl_AddTab proc pszFileName:DWORD
    LOCAL tci:TCITEM
    LOCAL dwIndex:DWORD
    
    mov tci.mask, TCIF_TEXT
    mov tci.pszText, pszFileName
    
    invoke SendMessage, g_hTabControl, TCM_INSERTITEM, g_dwTabCount, addr tci
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
    invoke SendMessage, g_hTabControl, TCM_DELETEITEM, dwIndex, 0
    dec g_dwTabCount
    ret
TabControl_CloseTab endp

; ============================================================================
; TabControl_SetCurrentTab - Set current tab
; Input: dwIndex
; ============================================================================
TabControl_SetCurrentTab proc dwIndex:DWORD
    invoke SendMessage, g_hTabControl, TCM_SETCURSEL, dwIndex, 0
    mov g_dwCurrentTab, dwIndex
    ret
TabControl_SetCurrentTab endp

; ============================================================================
; TabControl_GetCurrentTab - Get current tab index
; Returns: Index in eax
; ============================================================================
TabControl_GetCurrentTab proc
    invoke SendMessage, g_hTabControl, TCM_GETCURSEL, 0, 0
    mov g_dwCurrentTab, eax
    ret
TabControl_GetCurrentTab endp

; ============================================================================
; TabControl_SetTabText - Set tab text
; Input: dwIndex, pszText
; ============================================================================
TabControl_SetTabText proc dwIndex:DWORD, pszText:DWORD
    LOCAL tci:TCITEM
    
    mov tci.mask, TCIF_TEXT
    mov tci.pszText, pszText
    
    invoke SendMessage, g_hTabControl, TCM_SETITEM, dwIndex, addr tci
    ret
TabControl_SetTabText endp

; ============================================================================
; TabControl_MarkModified - Mark tab as modified
; Input: dwIndex, bModified
; ============================================================================
TabControl_MarkModified proc dwIndex:DWORD, bModified:DWORD
    LOCAL szCurrentText db MAX_PATH dup(0)
    LOCAL szNewText db MAX_PATH dup(0)
    LOCAL tci:TCITEM
    
    ; Get current text
    mov tci.mask, TCIF_TEXT
    mov tci.pszText, addr szCurrentText
    mov tci.cchTextMax, MAX_PATH
    
    invoke SendMessage, g_hTabControl, TCM_GETITEM, dwIndex, addr tci
    
    ; Remove modified marker if present
    invoke lstrlen, addr szCurrentText
    mov ecx, eax
    .if ecx > 2
        mov eax, offset szCurrentText
        add eax, ecx
        sub eax, 2
        invoke lstrcmp, eax, addr szModified
        .if eax == 0
            ; Remove marker
            mov byte ptr [eax], 0
        .endif
    .endif
    
    ; Add marker if modified
    .if bModified
        szCopy addr szNewText, addr szCurrentText
        szCat addr szNewText, addr szModified
    .else
        szCopy addr szNewText, addr szCurrentText
    .endif
    
    ; Set new text
    mov tci.mask, TCIF_TEXT
    mov tci.pszText, addr szNewText
    
    invoke SendMessage, g_hTabControl, TCM_SETITEM, dwIndex, addr tci
    ret
TabControl_MarkModified endp

; ============================================================================
; TabControl_OwnerDraw - Handle owner-draw for tabs with close buttons
; Input: lParam points to NMCUSTOMDRAW structure
; Returns: CDRF_DODEFAULT or CDRF_SKIPDEFAULT
; ============================================================================
TabControl_OwnerDraw proc lParam:DWORD
    LOCAL nmcd:NMCUSTOMDRAW
    LOCAL rc:RECT
    LOCAL hdc:DWORD
    LOCAL hFont:DWORD
    LOCAL hOldFont:DWORD
    LOCAL szText db MAX_PATH dup(?)
    LOCAL tci:TCITEM
    
    ; Copy the structure
    invoke RtlMoveMemory, addr nmcd, lParam, sizeof NMCUSTOMDRAW
    
    .if nmcd.dwDrawStage == CDDS_PREPAINT
        mov eax, CDRF_NOTIFYITEMDRAW
        ret
    .endif
    
    .if nmcd.dwDrawStage == CDDS_ITEMPREPAINT
        mov hdc, nmcd.hdc
        
        ; Get tab text
        mov tci.mask, TCIF_TEXT
        mov tci.pszText, addr szText
        mov tci.cchTextMax, MAX_PATH
        invoke SendMessage, g_hTabControl, TCM_GETITEM, nmcd.dwItemSpec, addr tci
        
        ; Get tab rectangle
        invoke SendMessage, g_hTabControl, TCM_GETITEMRECT, nmcd.dwItemSpec, addr rc
        
        ; Draw tab background
        invoke FillRect, hdc, addr rc, COLOR_BTNFACE + 1
        
        ; Draw close button (simple X)
        invoke MoveToEx, hdc, rc.left, rc.top, NULL
        invoke LineTo, hdc, rc.right, rc.bottom
        invoke MoveToEx, hdc, rc.right, rc.top, NULL
        invoke LineTo, hdc, rc.left, rc.bottom
        
        ; Restore font
        invoke SelectObject, hdc, hOldFont
        
        mov eax, CDRF_SKIPDEFAULT
        ret
    .endif
    
    mov eax, CDRF_DODEFAULT
    ret
TabControl_OwnerDraw endp

; ============================================================================
; TabControl_HitTest - Test if mouse click is on close button
; Input: dwIndex, x, y
; Returns: TRUE if on close button, FALSE otherwise
; ============================================================================
TabControl_HitTest proc dwIndex:DWORD, x:DWORD, y:DWORD
    LOCAL rc:RECT
    LOCAL closeRect:RECT
    
    ; Get tab rectangle
    invoke SendMessage, g_hTabControl, TCM_GETITEMRECT, dwIndex, addr rc
    
    ; Calculate close button rectangle
    mov eax, rc.right
    sub eax, CLOSE_BUTTON_SIZE
    sub eax, CLOSE_BUTTON_MARGIN
    mov closeRect.left, eax
    add eax, CLOSE_BUTTON_SIZE
    mov closeRect.right, eax
    
    ; Center vertically
    mov eax, rc.bottom
    sub eax, rc.top
    sub eax, CLOSE_BUTTON_SIZE
    shr eax, 1
    add eax, rc.top
    mov closeRect.top, eax
    add eax, CLOSE_BUTTON_SIZE
    mov closeRect.bottom, eax
    
    ; Test if point is in close button
    invoke PtInRect, addr closeRect, x, y
    ret
TabControl_HitTest endp

; ============================================================================
; TabControl_HandleClick - Handle mouse clicks on tabs
; Input: lParam (mouse coordinates)
; Returns: TRUE if handled, FALSE otherwise
; ============================================================================
TabControl_HandleClick proc lParam:DWORD
    LOCAL hitTest:TCHITTESTINFO
    LOCAL dwIndex:DWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    
    ; Extract mouse coordinates
    mov eax, lParam
    mov x, eax
    and x, 0FFFFh
    mov y, eax
    shr y, 16
    
    ; Hit test to find which tab was clicked
    mov hitTest.pt.x, x
    mov hitTest.pt.y, y
    invoke SendMessage, g_hTabControl, TCM_HITTEST, 0, addr hitTest
    mov dwIndex, eax
    
    .if eax != -1  ; Valid tab index
        ; Test if click was on close button
        invoke TabControl_HitTest, dwIndex, x, y
        .if eax
            ; Close the tab
            invoke TabControl_CloseTab, dwIndex
            mov eax, TRUE
            ret
        .else
            ; Switch to the clicked tab
            invoke TabControl_SetCurrentTab, dwIndex
            mov eax, TRUE
            ret
        .endif
    .endif
    
    mov eax, FALSE
    ret
TabControl_HandleClick endp

; ============================================================================
; TabControl_GetTabCount - Get number of tabs
; Returns: Count in eax
; ============================================================================
TabControl_GetTabCount proc
    invoke SendMessage, g_hTabControl, TCM_GETITEMCOUNT, 0, 0
    mov g_dwTabCount, eax
    ret
TabControl_GetTabCount endp

; ============================================================================
; TabControl_GetHandle - Get tab control handle
; Returns: Handle in eax
; ============================================================================
TabControl_GetHandle proc
    mov eax, g_hTabControl
    ret
TabControl_GetHandle endp

; ============================================================================
; Export procedures for external use
; ============================================================================
public CreateTabControl
public TabControl_AddTab
public TabControl_CloseTab
public TabControl_SetCurrentTab
public TabControl_GetCurrentTab
public TabControl_SetTabText
public TabControl_MarkModified
public TabControl_GetTabCount
public TabControl_GetHandle
public TabControl_OwnerDraw
public TabControl_HandleClick

end
; ============================================================================
; TabControl_GetTabCount - Get number of tabs
; Returns: Count in eax
; ============================================================================
TabControl_GetTabCount proc
    mov eax, g_dwTabCount
    ret
TabControl_GetTabCount endp

; ============================================================================
; TabControl_GetHandle - Get tab control handle
; Returns: Handle in eax
; ============================================================================
TabControl_GetHandle proc
    mov eax, g_hTabControl
    ret
TabControl_GetHandle endp

; ============================================================================
; TabControl_Cleanup - Cleanup tab resources
; ============================================================================
TabControl_Cleanup proc
    ; Destroy the tab control window if it exists
    .if g_hTabControl != 0
        invoke DestroyWindow, g_hTabControl
        mov g_hTabControl, 0
    .endif
    ; Free any allocated tab data buffer
    .if g_pTabData != 0
        invoke GlobalFree, g_pTabData
        mov g_pTabData, 0
    .endif
    ret
TabControl_Cleanup endp

; ============================================================================
; Export procedures for external use
; ============================================================================
public CreateTabControl
public TabControl_AddTab
public TabControl_CloseTab
public TabControl_SetCurrentTab
public TabControl_GetCurrentTab
public TabControl_SetTabText
public TabControl_MarkModified
public TabControl_GetTabCount
public TabControl_GetHandle
public TabControl_OwnerDraw
public TabControl_HandleClick

end