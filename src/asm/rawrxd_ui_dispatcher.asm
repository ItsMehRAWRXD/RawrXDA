;-------------------------------------------------------------------------
; rawrxd_ui_dispatcher.asm - MASM64
; Zero-bloat Win32 Message Loop & UI Dispatcher
; Replaces Qt Event Loop for <500MB RAM footprint
;-------------------------------------------------------------------------

extern GetMessageA : proc
extern TranslateMessage : proc
extern DispatchMessageA : proc
extern PostQuitMessage : proc
extern DefWindowProcA : proc

.code

;-------------------------------------------------------------------------
; rawrxd_run_ui_loop()
; Executes the primary Win32 message loop with zero overhead
;-------------------------------------------------------------------------
rawrxd_run_ui_loop proc
    sub rsp, 40 ; Shadow space

    ; MSG structure is 48 bytes on x64
    ; Allocate on stack
    sub rsp, 48
    mov rdi, rsp ; RDI points to MSG

@loop:
    xor rdx, rdx ; hWnd = NULL
    xor r8, r8   ; wMsgFilterMin = 0
    xor r9, r9   ; wMsgFilterMax = 0
    mov rcx, rdi ; lpMsg
    call GetMessageA
    
    test eax, eax
    jz @exit ; WM_QUIT
    
    mov rcx, rdi
    call TranslateMessage
    
    mov rcx, rdi
    call DispatchMessageA
    
    jmp @loop

@exit:
    add rsp, 48
    add rsp, 40
    ret
rawrxd_run_ui_loop endp

;-------------------------------------------------------------------------
; rawrxd_ui_wndproc(hwnd, msg, wparam, lparam)
; Standard Win32 Window Procedure
;-------------------------------------------------------------------------
rawrxd_ui_wndproc proc
    ; RCX=hwnd, RDX=msg, R8=wparam, R9=lparam
    cmp edx, 0002h ; WM_DESTROY
    je @destroy
    
    jmp DefWindowProcA

@destroy:
    xor ecx, ecx
    call PostQuitMessage
    xor eax, eax
    ret
rawrxd_ui_wndproc endp

end
