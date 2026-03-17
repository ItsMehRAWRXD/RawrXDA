; RawrXD_ChatInput.asm - Hooking WM_CHAR for Input Capture
; Specialized for Sovereign Agentic Environment (x64)

include masm64_compat.inc
include RawrXD_Common.inc

.data
    szRouteChat db "Routing to Agentic Engine: %s", 0
    chatBuffer  db 4096 dup(0)

.code
; Hook WM_CHAR in chat edit control
; hWnd: handle to the edit control
; uMsg: window message
; wParam: key code
; lParam: repeat count etc
ChatEdit_WndProc PROC FRAME
    LOCAL bShiftPressed:DWORD
    
    ; Setup stack frame
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 32
    .endprolog

    ; Check if Shift is down
    invoke GetKeyState, VK_SHIFT
    and eax, 8000h
    mov bShiftPressed, eax

    .IF edx == WM_CHAR
        .IF r8 == VK_RETURN
            .IF bShiftPressed == 0
                ; Capture input text
                invoke GetWindowTextA, rcx, ADDR chatBuffer, 4096
                
                ; Clear control
                invoke SetWindowTextA, rcx, ADDR szEmptyString
                
                ; Dispatch to Message Router
                invoke RouteToAgenticEngine, ADDR chatBuffer
                
                xor eax, eax
                jmp _Exit
            .ENDIF
        .ENDIF
    .ENDIF

    ; Chain to original WndProc (stored in global lpfnOldEditProc)
    invoke CallWindowProcA, lpfnOldEditProc, rcx, edx, r8, r9

_Exit:
    add rsp, 32
    pop rbp
    ret
ChatEdit_WndProc ENDP

END
