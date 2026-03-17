; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Model_StateMachine.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_Initialize
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_Initialize PROC FRAME
    mov rax, 1
    ret
ModelState_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_Transition
; RCX = ModelHandle, RDX = NewState
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_Transition PROC FRAME
    mov rax, 1
    ret
ModelState_Transition ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_AcquireInstance
; Returns a dummy handle
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_AcquireInstance PROC FRAME
    mov rax, 100h ; Dummy handle
    ret
ModelState_AcquireInstance ENDP

PUBLIC ModelState_Initialize
PUBLIC ModelState_Transition
PUBLIC ModelState_AcquireInstance

END
