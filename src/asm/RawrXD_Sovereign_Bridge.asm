OPTION CASEMAP:NONE

SOVEREIGN_MARKER  EQU 01751431337h
WM_RAWRXD_DIAG    EQU 0400h + 1337h

.data
sovereign_marker DQ 1751431337h

.code
align 16

PUBLIC CheckAndNotify

; long long CheckAndNotify(unsigned int status, unsigned long long confidenceBits,
;                         void* hwnd, void* postMessageFn)
; RCX = status
; RDX = confidenceBits
; R8  = hwnd
; R9  = postMessageFn
CheckAndNotify PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov rax, 068A4C8E9h
    cmp rcx, rax
    jne notify_fail

    test r8, r8
    jz notify_fail

    test r9, r9
    jz notify_fail

    mov r10, r9
    mov r11d, edx
    mov rcx, r8
    mov edx, WM_RAWRXD_DIAG
    mov r8, 068A4C8E9h
    mov r9d, r11d
    call r10

    add rsp, 28h
    ret

notify_fail:
    xor eax, eax
    add rsp, 28h
    ret
CheckAndNotify ENDP

END