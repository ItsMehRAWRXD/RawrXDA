; ============================================================================
; rawrxd_ui_dispatcher.asm — x64 MASM Native Win32 Message Loop
; ============================================================================
.code

; Win32 API functions
EXTERN GetMessageA : PROC
EXTERN TranslateMessage : PROC
EXTERN DispatchMessageA : PROC
EXTERN PostQuitMessage : PROC
EXTERN DefWindowProcA : PROC

; Data for MSG structure (approx size)
; typedef struct tagMSG {
;   HWND   hwnd;
;   UINT   message;
;   WPARAM wParam;
;   LPARAM lParam;
;   DWORD  time;
;   POINT  pt;
;   DWORD  lPrivate;
; } MSG, *PMSG, *LPMSG;
; Size is roughly 48 bytes on x64

rawrxd_run_message_loop PROC
    sub rsp, 56         ; Space for MSG struct + shadow space
L_loop:
    lea rcx, [rsp+32]   ; Pointer to MSG struct
    xor rdx, rdx        ; HWND = NULL
    xor r8, r8          ; FilterMin = 0
    xor r9, r9          ; FilterMax = 0
    call GetMessageA
    
    test eax, eax
    jz L_exit
    
    lea rcx, [rsp+32]
    call TranslateMessage
    
    lea rcx, [rsp+32]
    call DispatchMessageA
    jmp L_loop

L_exit:
    add rsp, 56
    ret
rawrxd_run_message_loop ENDP

rawrxd_dispatch_window_message PROC
    ; RCX = HWND, RDX = UINT, R8 = WPARAM, R9 = LPARAM
    cmp edx, 2          ; WM_DESTROY
    jne L_default
    
    xor rcx, rcx        ; ExitCode = 0
    sub rsp, 32
    call PostQuitMessage
    add rsp, 32
    xor rax, rax
    ret

L_default:
    sub rsp, 32
    call DefWindowProcA
    add rsp, 32
    ret
rawrxd_dispatch_window_message ENDP

END
