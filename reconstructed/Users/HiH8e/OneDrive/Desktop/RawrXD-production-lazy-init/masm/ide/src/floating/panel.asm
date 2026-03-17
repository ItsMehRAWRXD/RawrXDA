; ============================================================================
; RawrXD Agentic IDE - Floating Panel System Implementation
; Pure MASM - Draggable, Resizable, Always-on-Top Windows
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

 .data
include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; External declarations
; ============================================================================

extern hMainWindow:DWORD
extern hMainFont:DWORD
extern clrBackground:DWORD
extern clrForeground:DWORD
extern clrAccent:DWORD

; ============================================================================
; Constants
; ============================================================================

; Floating panel styles
FP_STYLE_TOOL       equ 1
FP_STYLE_HELP       equ 2
FP_STYLE_SNIPPETS   equ 3
FP_STYLE_STATS      equ 4

; Window messages for floating panels
WM_FP_DRAG_START    equ WM_USER + 200
WM_FP_DRAG_MOVE     equ WM_USER + 201
WM_FP_DRAG_END      equ WM_USER + 202
WM_FP_RESIZE_START  equ WM_USER + 203
WM_FP_RESIZE_MOVE   equ WM_USER + 204
WM_FP_RESIZE_END    equ WM_USER + 205

; ============================================================================
; Structures
; ============================================================================

FLOATING_PANEL struct
    hWnd            dd ?    ; Panel window handle
    szTitle         db 128 dup(?)  ; Panel title
    dwStyle         dd ?    ; Panel style/type
    rectPos         RECT <> ; Current position and size
    bVisible        dd ?    ; Visibility state
    bDocked         dd ?    ; Docked state
    bDragging       dd ?    ; Drag operation in progress
    bResizing       dd ?    ; Resize operation in progress
    ptDragStart     POINT <> ; Starting point for drag
    ptResizeStart   POINT <> ; Starting point for resize
    hParent         dd ?    ; Parent window (main IDE)
    pUserData       dd ?    ; User data pointer
    pfnOnCreate     dd ?    ; Creation callback
    pfnOnDestroy    dd ?    ; Destruction callback
    pfnOnResize     dd ?    ; Resize callback
    pfnOnMove       dd ?    ; Move callback
FLOATING_PANEL ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szFloatingPanelClass db "RawrXDFloatingPanel", 0
    szDockedText        db "Docked", 0
    szFloatingText      db "Floating", 0
    szCloseText         db "Close", 0
    szDockText          db "Dock", 0
    szFloatText         db "Float", 0
    
    ; Global floating panel registry
    g_PanelCount        dd 0
    g_MaxPanels         dd 16
    g_pPanels           dd 16 dup(0)
    
    ; UI metrics
    g_dwPanelWidth      dd 300
    g_dwPanelHeight     dd 400
    g_dwMinWidth        dd 200
    g_dwMinHeight       dd 150
    g_dwBorderWidth     dd 5
    g_dwTitleHeight     dd 25
    
    ; Colors
    clrPanelBg          dd 002D2D30h
    clrPanelTitleBg     dd 003C3C41h
    clrPanelTitleText   dd 00E0E0E0h
    clrPanelBorder      dd 00404040h
    clrPanelAccent      dd 0007ACCh

.data?
    g_hPanelBrush       dd ?
    g_hTitleBrush       dd ?
    g_hBorderBrush      dd ?
    g_hAccentBrush      dd ?

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; FloatingPanel_Init - Initialize floating panel system
; Returns: TRUE in eax on success
; ============================================================================
FloatingPanel_Init proc
    LOCAL wc:WNDCLASSEX
    
    ; Register floating panel window class
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW or CS_DBLCLKS
    mov wc.lpfnWndProc, offset FloatingPanelProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    push hInstance
    pop wc.hInstance
    invoke LoadIcon, NULL, IDI_APPLICATION
    mov wc.hIcon, eax
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    invoke CreateSolidBrush, clrPanelBg
    mov g_hPanelBrush, eax
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, offset szFloatingPanelClass
    mov wc.hIconSm, 0
    
    invoke RegisterClassEx, addr wc
    test eax, eax
    jz @Exit
    
    ; Initialize brush resources
    invoke CreateSolidBrush, clrPanelTitleBg
    mov g_hTitleBrush, eax
    invoke CreateSolidBrush, clrPanelBorder
    mov g_hBorderBrush, eax
    invoke CreateSolidBrush, clrPanelAccent
    mov g_hAccentBrush, eax
    
    mov eax, TRUE  ; Success
    jmp @Exit
    
@Exit:
    ret
FloatingPanel_Init endp

; ============================================================================
; FloatingPanel_Create - Create a new floating panel
; Input: pszTitle, dwStyle, x, y, width, height, pParent, pUserData
; Returns: Panel handle in eax
; ============================================================================
FloatingPanel_Create proc pszTitle:DWORD, dwStyle:DWORD, x:DWORD, y:DWORD, 
                      width:DWORD, height:DWORD, pParent:DWORD, pUserData:DWORD
    LOCAL pPanel:DWORD
    LOCAL dwExStyle:DWORD
    LOCAL dwWinStyle:DWORD
    LOCAL rect:RECT
    LOCAL index:DWORD
    
    ; Allocate panel structure
    MemAlloc sizeof FLOATING_PANEL
    mov pPanel, eax
    test eax, eax
    jz @Exit
    
    ; Initialize panel structure
    MemZero pPanel, sizeof FLOATING_PANEL
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    
    ; Set panel properties
    szCopy addr [eax].szTitle, pszTitle
    mov ecx, dwStyle
    mov [eax].dwStyle, ecx
    mov ecx, x
    mov [eax].rectPos.left, ecx
    mov ecx, y
    mov [eax].rectPos.top, ecx
    mov ecx, width
    add ecx, x
    mov [eax].rectPos.right, ecx
    mov ecx, height
    add ecx, y
    mov [eax].rectPos.bottom, ecx
    mov ecx, pParent
    mov [eax].hParent, ecx
    mov ecx, pUserData
    mov [eax].pUserData, ecx
    mov [eax].bVisible, TRUE
    mov [eax].bDocked, FALSE
    
    assume eax:nothing
    
    ; Add to panel registry
    mov eax, g_PanelCount
    mov index, eax
    mov ecx, pPanel
    mov g_pPanels[eax*4], ecx
    inc g_PanelCount
    
    ; Create window
    mov dwExStyle, WS_EX_TOPMOST or WS_EX_TOOLWINDOW
    mov dwWinStyle, WS_POPUP or WS_BORDER or WS_SYSMENU or WS_CAPTION
    
    invoke CreateWindowEx, dwExStyle,
        addr szFloatingPanelClass,
        pszTitle,
        dwWinStyle,
        x, y, width, height,
        pParent, NULL, hInstance, pPanel
    
    mov ecx, pPanel
    assume ecx:ptr FLOATING_PANEL
    mov [ecx].hWnd, eax
    assume ecx:nothing
    
    test eax, eax
    jz @Cleanup
    
    ; Show window
    invoke ShowWindow, eax, SW_SHOW
    invoke UpdateWindow, eax
    
    mov eax, pPanel
    jmp @Exit
    
@Cleanup:
    .if pPanel != 0
        mov eax, pPanel
        MemFree eax
        mov eax, index
        mov g_pPanels[eax*4], 0
        dec g_PanelCount
    .endif
    xor eax, eax
    
@Exit:
    ret
FloatingPanel_Create endp

; ============================================================================
; FloatingPanelProc - Window procedure for floating panels
; ============================================================================
FloatingPanelProc proc hWnd:HWND, uMsg:UINT, wParam:WPARAM, lParam:LPARAM
    LOCAL pPanel:DWORD
    
    ; Get panel pointer from window user data
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov pPanel, eax
    
    .if uMsg == WM_CREATE
        push lParam
        call OnFloatingPanelCreate
        xor eax, eax
        ret
        
    .elseif uMsg == WM_NCCALCSIZE
        push lParam
        push wParam
        call OnFloatingPanelNCCalcSize
        mov eax, 0
        ret
        
    .elseif uMsg == WM_NCPAINT
        push lParam
        call OnFloatingPanelNCPaint
        xor eax, eax
        ret
        
    .elseif uMsg == WM_NCHITTEST
        push lParam
        push wParam
        call OnFloatingPanelNCHitTest
        ret
        
    .elseif uMsg == WM_NCLBUTTONDOWN
        push lParam
        push wParam
        call OnFloatingPanelNCLButtonDown
        xor eax, eax
        ret
        
    .elseif uMsg == WM_LBUTTONDOWN
        push lParam
        push wParam
        call OnFloatingPanelLButtonDown
        xor eax, eax
        ret
        
    .elseif uMsg == WM_LBUTTONUP
        push lParam
        push wParam
        call OnFloatingPanelLButtonUp
        xor eax, eax
        ret
        
    .elseif uMsg == WM_MOUSEMOVE
        push lParam
        push wParam
        call OnFloatingPanelMouseMove
        xor eax, eax
        ret
        
    .elseif uMsg == WM_SIZE
        push lParam
        push wParam
        call OnFloatingPanelSize
        xor eax, eax
        ret
        
    .elseif uMsg == WM_MOVE
        push lParam
        push wParam
        call OnFloatingPanelMove
        xor eax, eax
        ret
        
    .elseif uMsg == WM_PAINT
        call OnFloatingPanelPaint
        xor eax, eax
        ret
        
    .elseif uMsg == WM_COMMAND
        push lParam
        push wParam
        call OnFloatingPanelCommand
        xor eax, eax
        ret
        
    .elseif uMsg == WM_CLOSE
        push pPanel
        call OnFloatingPanelClose
        xor eax, eax
        ret
        
    .elseif uMsg == WM_DESTROY
        push pPanel
        call OnFloatingPanelDestroy
        xor eax, eax
        ret
        
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
FloatingPanelProc endp

; ============================================================================
; OnFloatingPanelCreate - Handle WM_CREATE for floating panel
; ============================================================================
OnFloatingPanelCreate proc lParam:LPARAM
    LOCAL pCreateStruct:DWORD
    LOCAL pPanel:DWORD
    LOCAL hWnd:HWND
    
    ; Get create structure
    mov eax, lParam
    mov pCreateStruct, eax
    assume eax:ptr CREATESTRUCT
    mov eax, [eax].lpCreateParams
    assume eax:nothing
    mov pPanel, eax
    mov eax, pCreateStruct
    mov eax, [eax].hwnd
    mov hWnd, eax
    
    ; Store panel pointer in window user data
    invoke SetWindowLong, hWnd, GWL_USERDATA, pPanel
    
    ; Call user creation callback if provided
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    .if [eax].pfnOnCreate != 0
        push hWnd
        call [eax].pfnOnCreate
    .endif
    assume eax:nothing
    
    ret
OnFloatingPanelCreate endp

; ============================================================================
; OnFloatingPanelNCCalcSize - Handle WM_NCCALCSIZE for custom non-client area
; ============================================================================
OnFloatingPanelNCCalcSize proc wParam:WPARAM, lParam:LPARAM
    LOCAL pNCCalcSize:DWORD
    LOCAL rect:RECT
    
    .if wParam != 0  ; TRUE - process RECTs
        mov eax, lParam
        mov pNCCalcSize, eax
        assume eax:ptr NCCALCSIZE_PARAMS
        mov esi, addr [eax].rgrc
        assume esi:ptr RECT
        mov eax, [esi].left
        add eax, g_dwBorderWidth
        mov [esi].left, eax
        mov eax, [esi].top
        add eax, g_dwTitleHeight
        mov [esi].top, eax
        mov eax, [esi].right
        sub eax, g_dwBorderWidth
        mov [esi].right, eax
        mov eax, [esi].bottom
        sub eax, g_dwBorderWidth
        mov [esi].bottom, eax
        assume esi:nothing
        assume eax:nothing
    .endif
    
    ret
OnFloatingPanelNCCalcSize endp

; ============================================================================
; OnFloatingPanelNCPaint - Handle WM_NCPAINT for custom non-client area
; ============================================================================
OnFloatingPanelNCPaint proc lParam:LPARAM
    LOCAL hdc:HDC
    LOCAL rect:RECT
    LOCAL hOldBrush:DWORD
    LOCAL hOldPen:DWORD
    LOCAL pPanel:DWORD
    LOCAL szTitle db 128 dup(0)
    
    ; Get window DC for non-client area
    invoke GetWindowDC, hWnd
    mov hdc, eax
    
    ; Get window rectangle
    invoke GetWindowRect, hWnd, addr rect
    invoke OffsetRect, addr rect, -rect.left, -rect.top
    
    ; Draw border
    invoke SelectObject, hdc, g_hBorderBrush
    mov hOldBrush, eax
    invoke SelectObject, hdc, GetStockObject(NULL_PEN)
    mov hOldPen, eax
    
    ; Top border
    invoke PatBlt, hdc, 0, 0, rect.right, g_dwBorderWidth, PATCOPY
    ; Left border
    invoke PatBlt, hdc, 0, 0, g_dwBorderWidth, rect.bottom, PATCOPY
    ; Right border
    mov eax, rect.right
    sub eax, g_dwBorderWidth
    invoke PatBlt, hdc, eax, 0, g_dwBorderWidth, rect.bottom, PATCOPY
    ; Bottom border
    mov eax, rect.bottom
    sub eax, g_dwBorderWidth
    invoke PatBlt, hdc, 0, eax, rect.right, g_dwBorderWidth, PATCOPY
    
    ; Draw title bar
    invoke SelectObject, hdc, g_hTitleBrush
    invoke PatBlt, hdc, g_dwBorderWidth, g_dwBorderWidth, 
        rect.right, g_dwTitleHeight, PATCOPY
    
    ; Get panel pointer and title
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov pPanel, eax
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    szCopy addr szTitle, addr [eax].szTitle
    assume eax:nothing
    
    ; Draw title text
    invoke SetTextColor, hdc, clrPanelTitleText
    invoke SetBkMode, hdc, TRANSPARENT
    invoke TextOut, hdc, g_dwBorderWidth + 10, g_dwBorderWidth + 5, 
        addr szTitle, szLen(addr szTitle)
    
    ; Restore DC
    invoke SelectObject, hdc, hOldBrush
    invoke SelectObject, hdc, hOldPen
    invoke ReleaseDC, hWnd, hdc
    
    ret
OnFloatingPanelNCPaint endp

; ============================================================================
; OnFloatingPanelNCHitTest - Handle WM_NCHITTEST for custom hit testing
; ============================================================================
OnFloatingPanelNCHitTest proc wParam:WPARAM, lParam:LPARAM
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL rect:RECT
    LOCAL borderWidth:DWORD
    LOCAL titleHeight:DWORD
    
    ; Extract coordinates
    mov eax, lParam
    and eax, 0FFFFh  ; LOWORD = x
    mov x, eax
    mov eax, lParam
    shr eax, 16      ; HIWORD = y
    mov y, eax
    
    ; Get window rectangle
    invoke GetWindowRect, hWnd, addr rect
    
    ; Convert screen coordinates to client coordinates
    sub x, rect.left
    sub y, rect.top
    
    mov eax, g_dwBorderWidth
    mov borderWidth, eax
    mov eax, g_dwTitleHeight
    mov titleHeight, eax
    
    ; Check for title bar hit (for dragging)
    .if y >= borderWidth && y < borderWidth + titleHeight
        .if x >= borderWidth && x < rect.right - rect.left - borderWidth
            mov eax, HTCAPTION
            ret
        .endif
    .endif
    
    ; Check for border hits (for resizing)
    .if x < borderWidth
        .if y < borderWidth
            mov eax, HTTOPLEFT
        .elseif y >= rect.bottom - rect.top - borderWidth
            mov eax, HTBOTTOMLEFT
        .else
            mov eax, HTLEFT
        .endif
    .elseif x >= rect.right - rect.left - borderWidth
        .if y < borderWidth
            mov eax, HTTOPRIGHT
        .elseif y >= rect.bottom - rect.top - borderWidth
            mov eax, HTBOTTOMRIGHT
        .else
            mov eax, HTRIGHT
        .endif
    .elseif y < borderWidth
        mov eax, HTTOP
    .elseif y >= rect.bottom - rect.top - borderWidth
        mov eax, HTBOTTOM
    .else
        mov eax, HTCLIENT
    .endif
    
    ret
OnFloatingPanelNCHitTest endp

; ============================================================================
; OnFloatingPanelNCLButtonDown - Handle WM_NCLBUTTONDOWN
; ============================================================================
OnFloatingPanelNCLButtonDown proc wParam:WPARAM, lParam:LPARAM
    LOCAL pPanel:DWORD
    LOCAL pt:POINT
    
    ; Get panel pointer
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov pPanel, eax
    
    ; Extract coordinates
    mov eax, lParam
    and eax, 0FFFFh  ; LOWORD = x
    mov pt.x, eax
    mov eax, lParam
    shr eax, 16      ; HIWORD = y
    mov pt.y, eax
    
    ; Check hit test result
    mov eax, wParam
    .if eax == HTCAPTION
        ; Start dragging
        mov eax, pPanel
        assume eax:ptr FLOATING_PANEL
        mov [eax].bDragging, TRUE
        mov ecx, pt.x
        mov [eax].ptDragStart.x, ecx
        mov ecx, pt.y
        mov [eax].ptDragStart.y, ecx
        assume eax:nothing
        invoke SetCapture, hWnd
    .elseif eax >= HTLEFT && eax <= HTBOTTOMRIGHT
        ; Start resizing
        mov eax, pPanel
        assume eax:ptr FLOATING_PANEL
        mov [eax].bResizing, TRUE
        mov ecx, pt.x
        mov [eax].ptResizeStart.x, ecx
        mov ecx, pt.y
        mov [eax].ptResizeStart.y, ecx
        assume eax:nothing
        invoke SetCapture, hWnd
    .endif
    
    ret
OnFloatingPanelNCLButtonDown endp

; ============================================================================
; OnFloatingPanelLButtonDown - Handle WM_LBUTTONDOWN
; ============================================================================
OnFloatingPanelLButtonDown proc wParam:WPARAM, lParam:LPARAM
    LOCAL pPanel:DWORD
    LOCAL pt:POINT
    
    ; Get panel pointer
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov pPanel, eax
    
    ; Extract coordinates
    mov eax, lParam
    and eax, 0FFFFh  ; LOWORD = x
    mov pt.x, eax
    mov eax, lParam
    shr eax, 16      ; HIWORD = y
    mov pt.y, eax
    
    ret
OnFloatingPanelLButtonDown endp

; ============================================================================
; OnFloatingPanelLButtonUp - Handle WM_LBUTTONUP
; ============================================================================
OnFloatingPanelLButtonUp proc wParam:WPARAM, lParam:LPARAM
    LOCAL pPanel:DWORD
    
    ; Get panel pointer
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov pPanel, eax
    
    ; Stop dragging/resizing
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    mov [eax].bDragging, FALSE
    mov [eax].bResizing, FALSE
    assume eax:nothing
    
    invoke ReleaseCapture
    
    ret
OnFloatingPanelLButtonUp endp

; ============================================================================
; OnFloatingPanelMouseMove - Handle WM_MOUSEMOVE
; ============================================================================
OnFloatingPanelMouseMove proc wParam:WPARAM, lParam:LPARAM
    LOCAL pPanel:DWORD
    LOCAL pt:POINT
    LOCAL rect:RECT
    LOCAL xDelta:DWORD
    LOCAL yDelta:DWORD
    
    ; Get panel pointer
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov pPanel, eax
    
    ; Extract coordinates
    mov eax, lParam
    and eax, 0FFFFh  ; LOWORD = x
    mov pt.x, eax
    mov eax, lParam
    shr eax, 16      ; HIWORD = y
    mov pt.y, eax
    
    ; Check if dragging
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    .if [eax].bDragging
        ; Calculate delta
        mov ecx, pt.x
        sub ecx, [eax].ptDragStart.x
        mov xDelta, ecx
        mov ecx, pt.y
        sub ecx, [eax].ptDragStart.y
        mov yDelta, ecx
        
        ; Get current window position
        invoke GetWindowRect, hWnd, addr rect
        
        ; Move window
        mov eax, rect.left
        add eax, xDelta
        mov rect.left, eax
        mov eax, rect.right
        add eax, xDelta
        mov rect.right, eax
        mov eax, rect.top
        add eax, yDelta
        mov rect.top, eax
        mov eax, rect.bottom
        add eax, yDelta
        mov rect.bottom, eax
        
        invoke MoveWindow, hWnd, rect.left, rect.top, 
            rect.right, rect.bottom, TRUE
    .endif
    assume eax:nothing
    
    ret
OnFloatingPanelMouseMove endp

; ============================================================================
; OnFloatingPanelSize - Handle WM_SIZE
; ============================================================================
OnFloatingPanelSize proc wParam:WPARAM, lParam:LPARAM
    LOCAL pPanel:DWORD
    LOCAL width:DWORD
    LOCAL height:DWORD
    
    ; Get panel pointer
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov pPanel, eax
    
    ; Extract size
    mov eax, lParam
    and eax, 0FFFFh  ; LOWORD = width
    mov width, eax
    mov eax, lParam
    shr eax, 16      ; HIWORD = height
    mov height, eax
    
    ; Update panel size
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    invoke GetWindowRect, hWnd, addr [eax].rectPos
    assume eax:nothing
    
    ; Call resize callback if provided
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    .if [eax].pfnOnResize != 0
        push height
        push width
        push hWnd
        call [eax].pfnOnResize
    .endif
    assume eax:nothing
    
    ret
OnFloatingPanelSize endp

; ============================================================================
; OnFloatingPanelMove - Handle WM_MOVE
; ============================================================================
OnFloatingPanelMove proc wParam:WPARAM, lParam:LPARAM
    LOCAL pPanel:DWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    
    ; Get panel pointer
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov pPanel, eax
    
    ; Extract position
    mov eax, lParam
    and eax, 0FFFFh  ; LOWORD = x
    mov x, eax
    mov eax, lParam
    shr eax, 16      ; HIWORD = y
    mov y, eax
    
    ; Update panel position
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    invoke GetWindowRect, hWnd, addr [eax].rectPos
    assume eax:nothing
    
    ret
OnFloatingPanelMove endp

; ============================================================================
; OnFloatingPanelPaint - Handle WM_PAINT
; ============================================================================
OnFloatingPanelPaint proc
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:HDC
    LOCAL rect:RECT
    LOCAL hOldFont:DWORD
    LOCAL hOldBrush:DWORD
    LOCAL hOldPen:DWORD
    
    invoke BeginPaint, hWnd, addr ps
    mov hdc, eax
    
    ; Get client rectangle
    invoke GetClientRect, hWnd, addr rect
    
    ; Save current settings
    invoke SelectObject, hdc, hMainFont
    mov hOldFont, eax
    invoke SelectObject, hdc, g_hPanelBrush
    mov hOldBrush, eax
    invoke SelectObject, hdc, GetStockObject(NULL_PEN)
    mov hOldPen, eax
    
    ; Fill background
    invoke PatBlt, hdc, 0, 0, rect.right, rect.bottom, PATCOPY
    
    ; Call user paint callback if provided
    invoke GetWindowLong, hWnd, GWL_USERDATA
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    .if [eax].pfnOnPaint != 0
        push hdc
        push hWnd
        call [eax].pfnOnPaint
    .endif
    assume eax:nothing
    
    ; Restore settings
    invoke SelectObject, hdc, hOldFont
    invoke SelectObject, hdc, hOldBrush
    invoke SelectObject, hdc, hOldPen
    
    invoke EndPaint, hWnd, addr ps
    ret
OnFloatingPanelPaint endp

; ============================================================================
; OnFloatingPanelCommand - Handle WM_COMMAND
; ============================================================================
OnFloatingPanelCommand proc wParam:WPARAM, lParam:LPARAM
    LOCAL controlID:WORD
    LOCAL notifyCode:WORD
    
    ; Extract notification code and control ID
    mov eax, wParam
    shr eax, 16
    mov notifyCode, ax
    mov eax, wParam
    and eax, 0FFFFh
    mov controlID, ax
    
    ret
OnFloatingPanelCommand endp

; ============================================================================
; OnFloatingPanelClose - Handle WM_CLOSE
; ============================================================================
OnFloatingPanelClose proc pPanel:DWORD
    ; Hide window instead of destroying
    invoke ShowWindow, hWnd, SW_HIDE
    ret
OnFloatingPanelClose endp

; ============================================================================
; OnFloatingPanelDestroy - Handle WM_DESTROY
; ============================================================================
OnFloatingPanelDestroy proc pPanel:DWORD
    ; Call user destruction callback if provided
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    .if [eax].pfnOnDestroy != 0
        push hWnd
        call [eax].pfnOnDestroy
    .endif
    assume eax:nothing
    
    ; Free panel structure
    .if pPanel != 0
        MemFree pPanel
    .endif
    
    ret
OnFloatingPanelDestroy endp

; ============================================================================
; FloatingPanel_Show - Show/hide a floating panel
; ============================================================================
FloatingPanel_Show proc pPanel:DWORD, bShow:DWORD
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    mov ecx, bShow
    mov [eax].bVisible, ecx
    .if ecx
        invoke ShowWindow, [eax].hWnd, SW_SHOW
    .else
        invoke ShowWindow, [eax].hWnd, SW_HIDE
    .endif
    assume eax:nothing
    ret
FloatingPanel_Show endp

; ============================================================================
; FloatingPanel_SetDocked - Set docked state of a floating panel
; ============================================================================
FloatingPanel_SetDocked proc pPanel:DWORD, bDocked:DWORD
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    mov ecx, bDocked
    mov [eax].bDocked, ecx
    assume eax:nothing
    ret
FloatingPanel_SetDocked endp

; ============================================================================
; FloatingPanel_SetPosition - Set position and size of a floating panel
; ============================================================================
FloatingPanel_SetPosition proc pPanel:DWORD, x:DWORD, y:DWORD, width:DWORD, height:DWORD
    LOCAL rect:RECT
    
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    mov ecx, x
    mov [eax].rectPos.left, ecx
    mov ecx, y
    mov [eax].rectPos.top, ecx
    mov ecx, width
    add ecx, x
    mov [eax].rectPos.right, ecx
    mov ecx, height
    add ecx, y
    mov [eax].rectPos.bottom, ecx
    invoke MoveWindow, [eax].hWnd, x, y, width, height, TRUE
    assume eax:nothing
    ret
FloatingPanel_SetPosition endp

; ============================================================================
; FloatingPanel_GetHandle - Get window handle of a floating panel
; ============================================================================
FloatingPanel_GetHandle proc pPanel:DWORD
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    mov eax, [eax].hWnd
    assume eax:nothing
    ret
FloatingPanel_GetHandle endp

; ============================================================================
; FloatingPanel_SetCallbacks - Set callback functions for a floating panel
; ============================================================================
FloatingPanel_SetCallbacks proc pPanel:DWORD, pfnCreate:DWORD, pfnDestroy:DWORD, 
                            pfnResize:DWORD, pfnPaint:DWORD
    mov eax, pPanel
    assume eax:ptr FLOATING_PANEL
    mov ecx, pfnCreate
    mov [eax].pfnOnCreate, ecx
    mov ecx, pfnDestroy
    mov [eax].pfnOnDestroy, ecx
    mov ecx, pfnResize
    mov [eax].pfnOnResize, ecx
    mov ecx, pfnPaint
    mov [eax].pfnOnPaint, ecx
    assume eax:nothing
    ret
FloatingPanel_SetCallbacks endp

; ============================================================================
; FloatingPanel_Cleanup - Cleanup floating panel system
; ============================================================================
FloatingPanel_Cleanup proc
    LOCAL i:DWORD
    LOCAL pPanel:DWORD
    LOCAL hWnd:HWND
    
    ; Destroy all panels
    mov i, 0
    .while i < g_PanelCount
        mov eax, g_pPanels
        mov ecx, [eax + i * 4]
        mov pPanel, ecx
        .if ecx != 0
            mov eax, pPanel
            assume eax:ptr FLOATING_PANEL
            mov eax, [eax].hWnd
            mov hWnd, eax
            assume eax:nothing
            .if eax != 0
                invoke DestroyWindow, hWnd
            .endif
            MemFree pPanel
            mov g_pPanels[i * 4], 0
        .endif
        inc i
    .endw
    
    ; Clean up brushes
    .if g_hPanelBrush != 0
        invoke DeleteObject, g_hPanelBrush
        mov g_hPanelBrush, 0
    .endif
    .if g_hTitleBrush != 0
        invoke DeleteObject, g_hTitleBrush
        mov g_hTitleBrush, 0
    .endif
    .if g_hBorderBrush != 0
        invoke DeleteObject, g_hBorderBrush
        mov g_hBorderBrush, 0
    .endif
    .if g_hAccentBrush != 0
        invoke DeleteObject, g_hAccentBrush
        mov g_hAccentBrush, 0
    .endif
    
    ret
FloatingPanel_Cleanup endp

; ============================================================================
; Data
; ============================================================================

.data
    szToolPanelTitle    db "Tools", 0
    szHelpPanelTitle    db "Help", 0
    szSnippetsPanelTitle db "Snippets", 0
    szStatsPanelTitle   db "Statistics", 0

end