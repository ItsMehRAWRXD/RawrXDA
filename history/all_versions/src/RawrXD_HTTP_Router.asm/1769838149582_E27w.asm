; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_HTTP_Router.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
; OPTION WIN64:3

include windows.inc
include kernel32.inc

includelib kernel32.lib

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
