;==============================================================================
; File 12: ide_dpi.asm - DPI Awareness & Dynamic Scaling
;==============================================================================
include windows.inc

.code
;==============================================================================
; Initialize DPI System
;==============================================================================
DPI_Init PROC
    LOCAL hdcScreen:HANDLE
    
    ; Get primary monitor DPI
    invoke GetDC, NULL
    mov [hdcScreen], rax
    
    invoke GetDeviceCaps, rax, 88  ; LOGPIXELSX
    mov [systemDPI], eax
    
    invoke ReleaseDC, NULL, [hdcScreen]
    
    ; Calculate scale factor
    mov eax, [systemDPI]
    xor edx, edx
    mov ecx, 96
    div ecx
    mov [dpiScaleFactor], eax
    
    LOG_INFO "DPI initialized: {} (scale: {})",
        [systemDPI], [dpiScaleFactor]
    
    ret
DPI_Init ENDP

;==============================================================================
; Handle DPI Change
;==============================================================================
DPI_OnChange PROC suggestedRect:QWORD
    LOCAL newRect:RECT
    
    mov rax, suggestedRect
    mov eax, [rax]
    mov [newRect.left], eax
    
    mov rax, suggestedRect
    mov eax, [rax + 4]
    mov [newRect.top], eax
    
    invoke SetWindowPos, [hMainWnd], NULL,
        [newRect.left], [newRect.top], 0, 0,
        0x0010 or 0x0020
    
    LOG_INFO "DPI changed, windows repositioned"
    
    ret
DPI_OnChange ENDP

;==============================================================================
; Data
;==============================================================================
.data
systemDPI           dd 96
dpiScaleFactor      dd 1

END
