; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Model_StateMachine.asm
; Model State Machine (x64)
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

ModelState_Initialize PROC FRAME
    mov rax, 1 ; Success
    ret
ModelState_Initialize ENDP

ModelState_Transition PROC FRAME
    mov rax, 1
    ret
ModelState_Transition ENDP

ModelState_AcquireInstance PROC FRAME
    ; Returns a stable instance pointer (process lifetime).
    lea rax, g_ModelState_Instance
    ret
ModelState_AcquireInstance ENDP

PUBLIC ModelState_Initialize
PUBLIC ModelState_Transition
PUBLIC ModelState_AcquireInstance

.DATA
align 16
g_ModelState_Instance db 256 dup(0)

END