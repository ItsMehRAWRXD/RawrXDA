; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_HTTP_Router.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; HttpRouter_Initialize
; ═══════════════════════════════════════════════════════════════════════════════
HttpRouter_Initialize PROC FRAME
    mov rax, 1
    ret
HttpRouter_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; QueueInferenceJob
; ═══════════════════════════════════════════════════════════════════════════════
QueueInferenceJob PROC FRAME
    mov rax, 1
    ret
QueueInferenceJob ENDP

PUBLIC HttpRouter_Initialize
PUBLIC QueueInferenceJob

END
