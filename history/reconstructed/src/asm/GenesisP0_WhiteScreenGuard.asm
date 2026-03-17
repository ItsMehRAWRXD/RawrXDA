; GenesisP0_WhiteScreenGuard.asm
; UI repaint watchdog for Qt-free render path.

OPTION CASEMAP:NONE

includelib kernel32.lib
includelib user32.lib

EXTERN GetTickCount64:PROC
EXTERN InvalidateRect:PROC
EXTERN UpdateWindow:PROC
EXTERN UTC_LogEvent:PROC

.data
align 8
g_guard_hwnd       dq 0
g_last_ping        dq 0
g_timeout_ms       dq 5000
g_guard_active     db 0
sz_evt_wsg_start   db "[GenesisP0] WhiteScreenGuard Start", 0

.code
PUBLIC Genesis_WhiteScreenGuard_StartMonitoring
PUBLIC Genesis_WhiteScreenGuard_Ping
PUBLIC Genesis_WhiteScreenGuard_ForceRepaint
PUBLIC Genesis_WhiteScreenGuard_Patch

; int Genesis_WhiteScreenGuard_StartMonitoring(HWND hwnd, uint64 optionalTimeoutMs)
Genesis_WhiteScreenGuard_StartMonitoring PROC
    sub rsp, 28h

    mov qword ptr [rsp+20h], rcx
    lea rcx, sz_evt_wsg_start
    call UTC_LogEvent
    mov rcx, qword ptr [rsp+20h]

    mov qword ptr [g_guard_hwnd], rcx
    cmp rdx, 0
    jz start_keep_timeout
    mov qword ptr [g_timeout_ms], rdx

start_keep_timeout:
    call GetTickCount64
    mov qword ptr [g_last_ping], rax
    mov byte ptr [g_guard_active], 1

    mov eax, 1
    add rsp, 28h
    ret
Genesis_WhiteScreenGuard_StartMonitoring ENDP

; int Genesis_WhiteScreenGuard_Ping(void)
Genesis_WhiteScreenGuard_Ping PROC
    sub rsp, 28h

    cmp byte ptr [g_guard_active], 1
    jne ping_inactive

    call GetTickCount64
    mov qword ptr [g_last_ping], rax
    mov eax, 1
    add rsp, 28h
    ret

ping_inactive:
    xor eax, eax
    add rsp, 28h
    ret
Genesis_WhiteScreenGuard_Ping ENDP

; int Genesis_WhiteScreenGuard_ForceRepaint(HWND hwnd)
Genesis_WhiteScreenGuard_ForceRepaint PROC
    sub rsp, 28h

    test rcx, rcx
    jnz repaint_have_hwnd
    mov rcx, qword ptr [g_guard_hwnd]

repaint_have_hwnd:
    test rcx, rcx
    jz repaint_fail

    mov qword ptr [rsp+20h], rcx
    xor rdx, rdx
    mov r8d, 1
    call InvalidateRect

    mov rcx, qword ptr [rsp+20h]
    call UpdateWindow

    mov eax, 1
    add rsp, 28h
    ret

repaint_fail:
    xor eax, eax
    add rsp, 28h
    ret
Genesis_WhiteScreenGuard_ForceRepaint ENDP

; int Genesis_WhiteScreenGuard_Patch(HDC hdc)
; HDC is currently unused; this is an ABI alias requested by integration code.
Genesis_WhiteScreenGuard_Patch PROC
    mov rcx, qword ptr [g_guard_hwnd]
    jmp Genesis_WhiteScreenGuard_ForceRepaint
Genesis_WhiteScreenGuard_Patch ENDP

END
