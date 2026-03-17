; ============================================================================
; RawrXD Agentic IDE - Minimal Tab Control (Pure MASM)
; Provides CreateTabControl for the minimal build.
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
include \masm32\include\comctl32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib
includelib \masm32\lib\comctl32.lib

; External declarations
extern g_hMainWindow:DWORD
extern g_hMainFont:DWORD
extern g_hInstance:DWORD

.data
include constants.inc

    szWelcome           db "Welcome", 0
    szEditor            db "Editor", 0
    szOutput            db "Output", 0
    szCloseButton       db "×", 0  ; Unicode multiplication sign for close
    
    ; Tab control state
    g_hTabControl       dd 0
    g_dwTabCount        dd 3
    
    ; Close button dimensions
    CLOSE_BTN_WIDTH     equ 16
    CLOSE_BTN_HEIGHT    equ 14
    CLOSE_BTN_MARGIN    equ 4

.code

public CreateTabControl
public TabControl_OwnerDraw
public TabControl_HandleClick

; ============================================================================
; CreateTabControl - Create basic tab control
; Returns: Tab handle in eax
; ============================================================================
CreateTabControl proc
    LOCAL dwStyle:DWORD
    LOCAL hTab:DWORD
    ; Minimal TCITEM fields (7 DWORDs contiguous)
    LOCAL tciMask:DWORD
    LOCAL tciState:DWORD
    LOCAL tciStateMask:DWORD
    LOCAL tciPszText:DWORD
    LOCAL tciCchTextMax:DWORD
    LOCAL tciIImage:DWORD
    LOCAL tciLParam:DWORD

    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or TCS_OWNERDRAWFIXED

    ; Ensure common controls are loaded
    invoke InitCommonControls

    invoke CreateWindowEx, 0,
        offset szTabClass,
        NULL,
        dwStyle,
        240, 0, 600, 420,
        g_hMainWindow,
        IDC_TABCONTROL,
        g_hInstance,
        NULL
    mov hTab, eax
    mov g_hTabControl, eax
    test eax, eax
    jz @Exit

    ; Ensure we have a valid font to avoid crashes
    mov eax, g_hMainFont
    test eax, eax
    jnz @HaveFont
    invoke GetStockObject, DEFAULT_GUI_FONT
    mov g_hMainFont, eax
@HaveFont:
    invoke SendMessage, hTab, WM_SETFONT, g_hMainFont, TRUE

    ; Insert three tabs using TCM_INSERTITEM and local TCITEM layout
    ; Welcome
    mov tciMask, 1            ; TCIF_TEXT
    mov tciState, 0
    mov tciStateMask, 0
    mov tciPszText, offset szWelcome
    mov tciCchTextMax, 0
    mov tciIImage, 0
    mov tciLParam, 0
    invoke SendMessage, hTab, 4871, 0, addr tciMask

    ; Editor
    mov tciMask, 1
    mov tciPszText, offset szEditor
    invoke SendMessage, hTab, 4871, 1, addr tciMask

    ; Output
    mov tciMask, 1
    mov tciPszText, offset szOutput
    invoke SendMessage, hTab, 4871, 2, addr tciMask

    mov eax, hTab
@Exit:
    ret
CreateTabControl endp

; ============================================================================
; TabControl_OwnerDraw - Handle owner-draw for tabs with close buttons
; Input: lParam points to DRAWITEMSTRUCT
; Returns: TRUE if handled
; ============================================================================
TabControl_OwnerDraw proc lParam:DWORD
    LOCAL rect:RECT
    LOCAL textRect:RECT
    LOCAL closeRect:RECT
    LOCAL szText[64]:BYTE
    LOCAL hDC:DWORD
    LOCAL itemID:DWORD
    LOCAL hBrush:DWORD
    
    ; Get DRAWITEMSTRUCT pointer
    mov eax, lParam
    test eax, eax
    jz @Done
    
    ; Store pointer in ecx
    mov ecx, eax
    
    ; Extract hDC and itemID
    mov eax, [ecx + 8]   ; hDC
    mov hDC, eax
    mov eax, [ecx + 12]  ; itemID
    mov itemID, eax
    
    ; Get item rectangle
    mov eax, [ecx + 16]  ; rcItem.left
    mov rect.left, eax
    mov eax, [ecx + 20]  ; rcItem.top
    mov rect.top, eax
    mov eax, [ecx + 24]  ; rcItem.right
    mov rect.right, eax
    mov eax, [ecx + 28]  ; rcItem.bottom
    mov rect.bottom, eax
    
    ; Draw tab background
    invoke GetSysColor, COLOR_BTNFACE
    invoke CreateSolidBrush, eax
    mov hBrush, eax
    invoke FillRect, hDC, addr rect, hBrush
    invoke DeleteObject, hBrush
    
    ; Get tab text using inline TCITEM structure
    push 0                       ; lParam (7)
    push 0                       ; iImage (6)
    push 0                       ; cchTextMax (5)
    lea eax, szText
    push eax                     ; pszText (4)
    push 0                       ; stateMask (3)
    push 0                       ; state (2)
    push TCIF_TEXT               ; mask (1)
    mov eax, esp
    invoke SendMessage, g_hTabControl, TCM_GETITEM, itemID, eax
    add esp, 28                  ; Clean up stack (7 DWORDs)
    
    ; Calculate text rectangle (leave space for close button)
    mov eax, rect.left
    add eax, 8
    mov textRect.left, eax
    mov eax, rect.top
    add eax, 4
    mov textRect.top, eax
    mov eax, rect.right
    sub eax, CLOSE_BTN_WIDTH + CLOSE_BTN_MARGIN + 8
    mov textRect.right, eax
    mov eax, rect.bottom
    sub eax, 4
    mov textRect.bottom, eax
    
    ; Draw tab text
    invoke SetBkMode, hDC, TRANSPARENT
    invoke DrawText, hDC, addr szText, -1, addr textRect, DT_SINGLELINE or DT_VCENTER or DT_LEFT
    
    ; Calculate close button rectangle
    mov eax, rect.right
    sub eax, CLOSE_BTN_WIDTH + CLOSE_BTN_MARGIN
    mov closeRect.left, eax
    mov eax, rect.bottom
    sub eax, rect.top
    sub eax, CLOSE_BTN_HEIGHT
    shr eax, 1
    add eax, rect.top
    mov closeRect.top, eax
    mov eax, closeRect.left
    add eax, CLOSE_BTN_WIDTH
    mov closeRect.right, eax
    mov eax, closeRect.top
    add eax, CLOSE_BTN_HEIGHT
    mov closeRect.bottom, eax
    
    ; Draw close button (X)
    invoke DrawText, hDC, addr szCloseButton, -1, addr closeRect, DT_SINGLELINE or DT_CENTER or DT_VCENTER
    
    mov eax, TRUE
    ret
    
@Done:
    mov eax, FALSE
    ret
TabControl_OwnerDraw endp

; ============================================================================
; TabControl_HandleClick - Handle mouse clicks on tabs and close buttons
; Input: lParam (mouse coordinates)
; Returns: TRUE if handled (close button clicked), FALSE otherwise
; ============================================================================
TabControl_HandleClick proc lParam:DWORD
    LOCAL hitTest:TCHITTESTINFO
    LOCAL rect:RECT
    LOCAL closeRect:RECT
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL tabIndex:DWORD
    
    ; Extract mouse coordinates
    mov eax, lParam
    mov x, eax
    and x, 0FFFFh
    mov y, eax
    shr y, 16
    
    ; Set up hit test structure
    mov eax, x
    mov hitTest.pt.x, eax
    mov eax, y
    mov hitTest.pt.y, eax
    mov hitTest.flags, 0
    
    ; Hit test to find which tab was clicked
    invoke SendMessage, g_hTabControl, TCM_HITTEST, 0, addr hitTest
    mov tabIndex, eax
    
    ; Check if valid tab index
    cmp eax, -1
    je @NotHandled
    
    ; Get tab rectangle
    invoke SendMessage, g_hTabControl, TCM_GETITEMRECT, tabIndex, addr rect
    test eax, eax
    jz @NotHandled
    
    ; Calculate close button rectangle
    mov eax, rect.right
    sub eax, CLOSE_BTN_WIDTH + CLOSE_BTN_MARGIN
    mov closeRect.left, eax
    
    ; Calculate vertical center for close button
    mov eax, rect.bottom
    sub eax, rect.top
    sub eax, CLOSE_BTN_HEIGHT
    shr eax, 1
    add eax, rect.top
    mov closeRect.top, eax
    
    mov eax, closeRect.left
    add eax, CLOSE_BTN_WIDTH
    mov closeRect.right, eax
    mov eax, closeRect.top
    add eax, CLOSE_BTN_HEIGHT
    mov closeRect.bottom, eax
    
    ; Check if click is within close button
    mov eax, x
    cmp eax, closeRect.left
    jl @NotHandled
    cmp eax, closeRect.right
    jg @NotHandled
    mov eax, y
    cmp eax, closeRect.top
    jl @NotHandled
    cmp eax, closeRect.bottom
    jg @NotHandled
    
    ; Close button was clicked - handle tab removal
    ; For now, don't allow closing if it's the last tab
    cmp g_dwTabCount, 1
    jle @NotHandled
    
    ; Delete the tab
    invoke SendMessage, g_hTabControl, TCM_DELETEITEM, tabIndex, 0
    dec g_dwTabCount
    
    ; Select adjacent tab if current was deleted
    invoke SendMessage, g_hTabControl, TCM_GETCURSEL, 0, 0
    cmp eax, tabIndex
    jne @CloseHandled
    
    ; Current tab was closed, select the previous one
    mov eax, tabIndex
    test eax, eax
    jz @SelectNext
    dec eax
    jmp @SetSelection
    
@SelectNext:
    xor eax, eax
    
@SetSelection:
    invoke SendMessage, g_hTabControl, TCM_SETCURSEL, eax, 0
    
@CloseHandled:
    mov eax, TRUE
    ret
    
@NotHandled:
    mov eax, FALSE
    ret
TabControl_HandleClick endp

end
