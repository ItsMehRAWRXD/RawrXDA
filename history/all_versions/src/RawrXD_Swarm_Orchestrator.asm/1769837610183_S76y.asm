; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Swarm_Orchestrator.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; Swarm_Initialize
; ═══════════════════════════════════════════════════════════════════════════════
Swarm_Initialize PROC FRAME
    mov rax, 1
    ret
Swarm_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Swarm_SubmitJob
; ═══════════════════════════════════════════════════════════════════════════════
Swarm_SubmitJob PROC FRAME
    mov rax, 1
    ret
Swarm_SubmitJob ENDP

PUBLIC Swarm_Initialize
PUBLIC Swarm_SubmitJob

END
