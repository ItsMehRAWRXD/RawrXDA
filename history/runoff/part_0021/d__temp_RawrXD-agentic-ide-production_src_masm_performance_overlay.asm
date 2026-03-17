;==============================================================================
; File 18: performance_overlay.asm - FPS & Memory Display
;==============================================================================
include windows.inc

.code
;==============================================================================
; Create Performance Overlay
;==============================================================================
PerformanceOverlay_Create PROC
    invoke CreateWindowEx, 0x00000020 or 0x00000080 or 0x00000008,
        OFFSET szPerfClass,
        NULL,
        0x80000000,
        0, 0, 200, 60,
        [hMainWnd], NULL, [hInstance], NULL
    mov [hPerfWnd], rax
    
    invoke SetLayeredWindowAttributes, rax, 0, 200, 0x00000002
    invoke SetTimer, rax, 1, 100, NULL
    
    LOG_INFO "Performance overlay created"
    
    ret
PerformanceOverlay_Create ENDP

;==============================================================================
; Window Procedure
;==============================================================================
PerformanceOverlay_WndProc PROC hWnd:QWORD, uMsg:QWORD,
                                  wParam:QWORD, lParam:QWORD
    SWITCH uMsg
        CASE WM_TIMER
            .if wParam == 1
                call PerformanceOverlay_Update
            .endif
            ret 0
            
        CASE WM_PAINT
            call PerformanceOverlay_OnPaint
            ret 0
    ENDSW
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
PerformanceOverlay_WndProc ENDP

;==============================================================================
; Update Metrics
;==============================================================================
PerformanceOverlay_Update PROC
    invoke QueryPerformanceCounter, ADDR currentTime
    
    mov rax, currentTime
    sub rax, lastTime
    mov frameTime, rax
    mov lastTime, currentTime
    
    .if frameTime > 0
        mov rax, 10000000
        xor rdx, rdx
        div frameTime
        mov [fps], eax
    .endif
    
    invoke InvalidateRect, [hPerfWnd], NULL, FALSE
    
    ret
PerformanceOverlay_Update ENDP

;==============================================================================
; Paint Overlay
;==============================================================================
PerformanceOverlay_OnPaint PROC
    LOCAL ps:PAINTSTRUCT
    
    invoke BeginPaint, [hPerfWnd], ADDR ps
    mov [hdc], rax
    
    invoke CreateSolidBrush, 0x00000000
    invoke FillRect, [hdc], ADDR ps.rcPaint, rax
    invoke DeleteObject, rax
    
    invoke SetBkMode, [hdc], 0  ; TRANSPARENT
    invoke SetTextColor, [hdc], 0x0000FF00
    
    invoke wsprintf, ADDR fpsBuffer, OFFSET szFPSFormat, [fps]
    invoke TextOut, [hdc], 10, 10, ADDR fpsBuffer, 8
    
    invoke EndPaint, [hPerfWnd], ADDR ps
    
    ret
PerformanceOverlay_OnPaint ENDP

;==============================================================================
; Data
;==============================================================================
.data
szPerfClass         db 'RawrXD_PerformanceOverlay',0
hPerfWnd            dq ?
hdc                 dq ?
currentTime         dq ?
lastTime            dq ?
frameTime           dq ?
fps                 dd 0
fpsBuffer           db 32 dup(?)
szFPSFormat         db '%d FPS',0

END
