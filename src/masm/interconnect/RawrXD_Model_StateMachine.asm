; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Model_StateMachine.asm
; Stub implementation for Model State Machine
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.DATA
; Stable storage to avoid returning stack addresses.
; The interconnect expects a non-null handle-like pointer.
ModelState_Instance dq 0

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
    ; Returns a stable mock instance pointer (non-null, valid after return).
    ; Previously returned [rsp] which was invalid after return (stack address).
    lea rax, ModelState_Instance
    ret
ModelState_AcquireInstance ENDP

PUBLIC ModelState_Initialize
PUBLIC ModelState_Transition
PUBLIC ModelState_AcquireInstance

END