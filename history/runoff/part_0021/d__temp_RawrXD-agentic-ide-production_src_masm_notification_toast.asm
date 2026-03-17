;==============================================================================
; File 16: notification_toast.asm - Toast Notifications
;==============================================================================
include windows.inc

.code
;==============================================================================
; Show Toast Notification
;==============================================================================
Toast_Show PROC lpTitle:QWORD, lpMessage:QWORD, type:DWORD
    LOCAL hWndToast:HWND
    
    ; Position at bottom-right
    invoke GetSystemMetrics, 0  ; SM_CXSCREEN
    sub eax, 350
    mov [toastX], eax
    
    invoke GetSystemMetrics, 1  ; SM_CYSCREEN
    sub eax, 100
    mov [toastY], eax
    
    ; Create toast window
    invoke CreateWindowEx, 0x00000008,  ; WS_EX_TOOLWINDOW
        OFFSET szToastClass,
        lpTitle,
        0x80000000 or 0x00000080,  ; WS_POPUP, WS_VISIBLE
        [toastX], [toastY],
        320, 80,
        NULL, NULL, [hInstance], NULL
    mov [hWndToast], rax
    
    ; Store message
    invoke SetWindowLongPtr, rax, 0, lpMessage
    
    ; Auto-hide after 5 seconds
    invoke SetTimer, rax, 100, 5000, NULL
    
    LOG_INFO "Toast notification shown: {}", lpTitle
    
    ret
Toast_Show ENDP

;==============================================================================
; Toast Window Procedure
;==============================================================================
Toast_WndProc PROC hWnd:QWORD, uMsg:QWORD, wParam:QWORD, lParam:QWORD
    SWITCH uMsg
        CASE WM_TIMER
            .if wParam == 100
                invoke KillTimer, hWnd, 100
                invoke DestroyWindow, hWnd
            .endif
            ret 0
            
        CASE WM_PAINT
            call Toast_OnPaint, hWnd
            ret 0
    ENDSW
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
Toast_WndProc ENDP

;==============================================================================
; Paint Toast
;==============================================================================
Toast_OnPaint PROC hWnd:QWORD
    LOCAL ps:PAINTSTRUCT
    
    invoke BeginPaint, hWnd, ADDR ps
    mov [hdc], rax
    
    ; Draw background
    invoke CreateSolidBrush, 0x00000000
    invoke FillRect, [hdc], ADDR ps.rcPaint, rax
    invoke DeleteObject, rax
    
    invoke EndPaint, hWnd, ADDR ps
    
    ret
Toast_OnPaint ENDP

;==============================================================================
; Data
;==============================================================================
.data
szToastClass        db 'RawrXD_Toast',0
toastX              dd 0
toastY              dd 0
hdc                 dq ?

END
