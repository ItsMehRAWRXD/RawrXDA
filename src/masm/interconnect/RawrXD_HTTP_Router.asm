; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_HTTP_Router.asm
; Stub implementation for HTTP routing
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

HttpRouter_Initialize PROC FRAME
    mov rax, 1 ; Success
    ret
HttpRouter_Initialize ENDP

QueueInferenceJob PROC FRAME
    ; RCX = Job Ptr
    mov rax, 1
    ret
QueueInferenceJob ENDP

PUBLIC HttpRouter_Initialize
PUBLIC QueueInferenceJob

END