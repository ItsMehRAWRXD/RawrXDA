; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Model_StateMachine.asm
; Stub implementation for Model State Machine
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
    ; Stub: return NULL until real instance allocation is implemented.
    ; Callers must check for null before use. Previously returned [rsp] which
    ; was invalid after return (stack address).
    xor rax, rax
    ret
ModelState_AcquireInstance ENDP

PUBLIC ModelState_Initialize
PUBLIC ModelState_Transition
PUBLIC ModelState_AcquireInstance

END