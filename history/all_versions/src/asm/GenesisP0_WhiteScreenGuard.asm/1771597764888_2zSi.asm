// ==============================================================================
// GenesisP0_WhiteScreenGuard.asm — UI Hang Detection & Recovery
// Exports: Genesis_WhiteScreenGuard_StartMonitoring, Genesis_WhiteScreenGuard_Ping, Genesis_WhiteScreenGuard_ForceRepaint
// ==============================================================================
OPTION DOTNAME
EXTERN CreateThread:PROC, TerminateThread:PROC, CloseHandle:PROC
EXTERN GetTickCount64:PROC, Sleep:PROC, PostMessageA:PROC
EXTERN InvalidateRect:PROC, UpdateWindow:PROC, IsWindow:PROC
EXTERN MessageBoxA:PROC, ExitProcess:PROC

.data
    align 8
    g_wsgHwnd           DQ 0
    g_wsgThread         DQ 0
    g_lastPing          DQ 0
    g_watchdogActive    DB 0
    g_thresholdMs       DQ 5000           ; 5 second timeout
    
    WM_FORCE_REPAINT    EQU 0x0400+0x100  ; WM_USER+256

.code
; ------------------------------------------------------------------------------
; WatchdogThread — Background thread monitors ping timestamps
; ------------------------------------------------------------------------------
WatchdogThread PROC
    sub rsp, 40
    
_wdg_loop:
    cmp g_watchdogActive, 0
    je _wdg_sleep
    
    call GetTickCount64
    mov rcx, rax
    sub rcx, g_lastPing
    
    cmp rcx, g_thresholdMs
    jb _wdg_sleep
    
    ; Timeout detected — force repaint
    mov rcx, g_wsgHwnd
    xor rdx, rdx
    mov r8, 1
    call InvalidateRect
    
    mov rcx, g_wsgHwnd
    call UpdateWindow
    
    ; Reset ping to avoid spam
    call GetTickCount64
    mov g_lastPing, rax
    
_wdg_sleep:
    mov rcx, 100                    ; Check every 100ms
    call Sleep
    jmp _wdg_loop
    
    add rsp, 40
    ret
WatchdogThread ENDP

; ------------------------------------------------------------------------------
; Genesis_WhiteScreenGuard_StartMonitoring — Start watchdog for hwnd
; RCX = hwnd
; ------------------------------------------------------------------------------
Genesis_WhiteScreenGuard_StartMonitoring PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 56
    
    mov g_wsgHwnd, rcx
    mov g_watchdogActive, 1
    
    call GetTickCount64
    mov g_lastPing, rax
    
    ; Create watchdog thread
    xor ecx, ecx                    ; Security
    xor edx, edx                    ; Stack size
    lea r8, WatchdogThread          ; Start address
    xor r9, r9                      ; Parameter
    mov [rsp+32], r9                ; Creation flags
    mov [rsp+40], r9                ; Thread ID ptr
    call CreateThread
    
    mov g_wsgThread, rax
    
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret
Genesis_WhiteScreenGuard_StartMonitoring ENDP

; ------------------------------------------------------------------------------
; Genesis_WhiteScreenGuard_Ping — Reset watchdog timer (call from main thread periodically)
; ------------------------------------------------------------------------------
Genesis_WhiteScreenGuard_Ping PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    call GetTickCount64
    mov g_lastPing, rax
    
    mov rsp, rbp
    pop rbp
    ret
Genesis_WhiteScreenGuard_Ping ENDP

; ------------------------------------------------------------------------------
; Genesis_WhiteScreenGuard_ForceRepaint — Immediate WM_PAINT trigger
; RCX = hwnd
; ------------------------------------------------------------------------------
Genesis_WhiteScreenGuard_ForceRepaint PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    xor rdx, rdx
    mov r8, 1
    call InvalidateRect
    
    mov rcx, [rbp+16]               ; hwnd from param
    call UpdateWindow
    
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret
Genesis_WhiteScreenGuard_ForceRepaint ENDP

END
