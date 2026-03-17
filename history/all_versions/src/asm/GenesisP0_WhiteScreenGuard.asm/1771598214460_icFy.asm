; ============================================================
; GenesisP0_WhiteScreenGuard.asm — Prevent white screen of death
; Monitors message pump health, forces repaint if stalled > 100ms
; Exports: WSG_Initialize, WSG_Tick, WSG_ForceRedraw, WSG_Shutdown
; ============================================================
OPTION CASEMAP:NONE

EXTERN GetTickCount64:PROC
EXTERN GetForegroundWindow:PROC
EXTERN PostMessageA:PROC
EXTERN InvalidateRect:PROC
EXTERN UpdateWindow:PROC
EXTERN SetTimer:PROC
EXTERN KillTimer:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN BitBlt:PROC
EXTERN CreateCompatibleDC:PROC
EXTERN CreateCompatibleBitmap:PROC
EXTERN SelectObject:PROC
EXTERN DeleteDC:PROC
EXTERN DeleteObject:PROC

PUBLIC WSG_Initialize
PUBLIC WSG_Tick
PUBLIC WSG_ForceRedraw
PUBLIC WSG_Shutdown

.data
ALIGN 8
g_hwnd          QWORD 0
g_lastTick      QWORD 0
g_hdcBackup     QWORD 0
g_hbmBackup     QWORD 0
g_guardEnabled  BYTE 0
g_stallThreshold QWORD 100    ; 100ms threshold
WM_FORCE_REDRAW EQU 0x0400+1  ; WM_USER+1

.code
; ------------------------------------------------------------
; WSG_Initialize(HWND hwnd) -> BOOL
; ------------------------------------------------------------
WSG_Initialize PROC
    push rbx
    sub rsp, 28h

    mov [g_hwnd], rcx
    mov rax, 1
    mov [g_guardEnabled], al

    call GetTickCount64
    mov [g_lastTick], rax

    ; Create backup DC for emergency redraw
    mov rcx, [g_hwnd]
    call GetDC
    mov rbx, rax

    mov rcx, rbx
    call CreateCompatibleDC
    mov [g_hdcBackup], rax

    ; Create 1x1 bitmap placeholder
    mov rcx, rbx
    mov edx, 1
    mov r8d, 1
    call CreateCompatibleBitmap
    mov [g_hbmBackup], rax

    mov rcx, [g_hdcBackup]
    mov rdx, rax
    call SelectObject

    mov rcx, [g_hwnd]
    mov rdx, rbx
    call ReleaseDC

    mov eax, 1
    add rsp, 28h
    pop rbx
    ret
WSG_Initialize ENDP

; ------------------------------------------------------------
; WSG_Tick() -> BOOL (TRUE if healthy, FALSE if stall detected)
; Call this from main message pump every iteration
; ------------------------------------------------------------
WSG_Tick PROC
    push rbx
    sub rsp, 28h

    mov al, [g_guardEnabled]
    test al, al
    jz @tick_ok

    call GetTickCount64
    mov rbx, rax
    sub rbx, [g_lastTick]

    cmp rbx, [g_stallThreshold]
    jb @update_tick

    ; Stall detected
    mov rcx, [g_hwnd]
    xor edx, edx
    xor r8d, r8d
    call InvalidateRect

    mov rcx, [g_hwnd]
    call UpdateWindow

    xor eax, eax              ; Return FALSE (stall detected)
    jmp @tick_done

@update_tick:
    mov [g_lastTick], rax

@tick_ok:
    mov eax, 1

@tick_done:
    add rsp, 28h
    pop rbx
    ret
WSG_Tick ENDP

; ------------------------------------------------------------
; WSG_ForceRedraw() -> void
; ------------------------------------------------------------
WSG_ForceRedraw PROC
    push rbx
    sub rsp, 28h

    mov rcx, [g_hwnd]
    test rcx, rcx
    jz @redraw_done

    xor edx, edx
    xor r8d, r8d
    call InvalidateRect

    mov rcx, [g_hwnd]
    call UpdateWindow

@redraw_done:
    add rsp, 28h
    pop rbx
    ret
WSG_ForceRedraw ENDP

; ------------------------------------------------------------
; WSG_Shutdown() -> void
; ------------------------------------------------------------
WSG_Shutdown PROC
    push rbx
    sub rsp, 28h

    xor eax, eax
    mov [g_guardEnabled], al

    mov rcx, [g_hbmBackup]
    test rcx, rcx
    jz @skip_del_bitmap
    call DeleteObject

@skip_del_bitmap:
    mov rcx, [g_hdcBackup]
    test rcx, rcx
    jz @skip_del_dc
    call DeleteDC

@skip_del_dc:
    xor eax, eax
    mov [g_hwnd], rax

    add rsp, 28h
    pop rbx
    ret
WSG_Shutdown ENDP

END
