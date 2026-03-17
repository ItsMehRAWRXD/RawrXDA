;==============================================================================
; File 15: tab_control.asm - Tab Control Implementation
;==============================================================================
include windows.inc

.code
;==============================================================================
; Tab Window Procedure (Subclassed)
;==============================================================================
TabWndProc PROC hWnd:QWORD, uMsg:QWORD, wParam:QWORD, lParam:QWORD
    SWITCH uMsg
        CASE WM_LBUTTONDOWN
            call Tab_GetCloseButtonRect, wParam, lParam
            .if eax == TRUE
                call Tab_CloseAtPosition, wParam, lParam
                ret 0
            .endif
    
        CASE WM_RBUTTONUP
            call Tab_ShowContextMenu, wParam, lParam
            ret 0
    
        CASE WM_MBUTTONUP
            call Tab_CloseAtPosition, wParam, lParam
            ret 0
    
        CASE WM_PAINT
            call Tab_OnPaint, hWnd
            ret 0
    ENDSW
    
    invoke CallWindowProc, [oldTabProc], hWnd, uMsg, wParam, lParam
    ret
TabWndProc ENDP

;==============================================================================
; Get Close Button Rectangle
;==============================================================================
Tab_GetCloseButtonRect PROC xPos:DWORD, yPos:DWORD
    LOCAL rect:RECT
    
    invoke SendMessage, [hTabControl], 0x0D00, 0,
        ADDR rect
    
    mov eax, [rect.right]
    sub eax, 20
    mov [closeBtnRect.left], eax
    mov [closeBtnRect.top], [rect.top]
    mov [closeBtnRect.right], [rect.right]
    mov [closeBtnRect.bottom], [rect.bottom]
    
    mov eax, xPos
    .if eax >= [closeBtnRect.left] && eax <= [closeBtnRect.right]
        mov rax, TRUE
    .else
        mov rax, FALSE
    .endif
    
    ret
Tab_GetCloseButtonRect ENDP

;==============================================================================
; Data
;==============================================================================
.data
oldTabProc          dq ?
hTabControl         dq ?
currentTabIndex     dd 0
closeBtnRect        RECT <>

END
