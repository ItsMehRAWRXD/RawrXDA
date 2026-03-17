;==============================================================================
; File 21: gpu_memory_display.asm - VRAM Usage Display
;==============================================================================
include windows.inc

.code
;==============================================================================
; Create GPU Memory Widget
;==============================================================================
GPUMemory_Create PROC
    invoke CreateWindowEx, 0x00000020 or 0x00000080,
        OFFSET szGPUMemoryClass,
        NULL,
        0x80000000,
        0, 60, 200, 20,
        [hMainWnd], NULL, [hInstance], NULL
    mov [hGPUMemoryWnd], rax
    
    invoke SetLayeredWindowAttributes, rax, 0, 180, 0x00000002
    invoke SetTimer, rax, 1, 1000, NULL
    
    LOG_INFO "GPU memory display created"
    
    ret
GPUMemory_Create ENDP

;==============================================================================
; Update GPU Memory
;==============================================================================
GPUMemory_Update PROC
    ; Get GPU memory usage (stub)
    mov [usedGB], 4
    mov [totalGB], 8
    
    invoke wsprintf, ADDR vramBuffer,
        OFFSET szVRAMFormat,
        [usedGB], [totalGB]
    
    ret
GPUMemory_Update ENDP

;==============================================================================
; Window Procedure
;==============================================================================
GPUMemory_WndProc PROC hWnd:QWORD, uMsg:QWORD, wParam:QWORD, lParam:QWORD
    SWITCH uMsg
        CASE WM_TIMER
            .if wParam == 1
                call GPUMemory_Update
                invoke InvalidateRect, hWnd, NULL, FALSE
            .endif
            ret 0
            
        CASE WM_PAINT
            call GPUMemory_OnPaint
            ret 0
    ENDSW
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
GPUMemory_WndProc ENDP

;==============================================================================
; Paint Display
;==============================================================================
GPUMemory_OnPaint PROC
    LOCAL ps:PAINTSTRUCT
    
    invoke BeginPaint, [hGPUMemoryWnd], ADDR ps
    mov [hdc], rax
    
    invoke CreateSolidBrush, 0x00000000
    invoke FillRect, [hdc], ADDR ps.rcPaint, rax
    invoke DeleteObject, rax
    
    invoke SetBkMode, [hdc], 0
    invoke SetTextColor, [hdc], 0x0000FFFFh
    
    invoke TextOut, [hdc], 10, 0,
        ADDR vramBuffer, 12
    
    invoke EndPaint, [hGPUMemoryWnd], ADDR ps
    
    ret
GPUMemory_OnPaint ENDP

;==============================================================================
; Data
;==============================================================================
.data
szGPUMemoryClass   db 'RawrXD_GPUMemory',0
hGPUMemoryWnd      dq ?
hdc                dq ?

usedGB              dd 0
totalGB             dd 0
vramBuffer          db 32 dup(?)
szVRAMFormat        db 'VRAM: %d/%d GB',0

END
