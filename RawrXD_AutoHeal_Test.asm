;=============================================================================
; RawrXD_AutoHeal_Test.asm
; Autonomous Compilation/Test Loop & Self-Healing Validator
; PURE X64 MASM - ZERO STUBS - ZERO CRT
;=============================================================================

OPTION CASEMAP:NONE

EXTERN ExitProcess:PROC
EXTERN InitializeAmphibiousCore:PROC
EXTERN RunAutonomousCycle_ml64:PROC
EXTERN GetStageMask_ml64:PROC
EXTERN PrintSuccess_ml64:PROC
EXTERN PrintFailure_ml64:PROC

STAGE_COMPLETE EQU 003Fh
TEST_CYCLES    EQU 3
MODE_CLI       EQU 0

.data
    szTestPrompt db "Generate AVX2 matrix multiply MASM with local runtime streaming.",0

.code
Main PROC FRAME
    push rbx
    .PUSHREG rbx
    and rsp, -16
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG

    xor rcx, rcx                        ; hMainWindow = NULL
    xor rdx, rdx                        ; hEditorWindow = NULL
    lea r8, [szTestPrompt]              ; prompt address
    mov r9d, MODE_CLI                   ; mode = CLI
    call InitializeAmphibiousCore
    test rax, rax
    jz TestFail

    mov ebx, TEST_CYCLES
CycleLoop:
    test ebx, ebx
    jz VerifyDone
    call RunAutonomousCycle_ml64
    dec ebx
    jmp CycleLoop

VerifyDone:
    ; Query final stage mask
    call GetStageMask_ml64
    cmp eax, STAGE_COMPLETE             ; All stages complete (0x3F)?
    jne TestFail
    
    ; SUCCESS: Print success message and exit 0
    call PrintSuccess_ml64
    xor ecx, ecx
    jmp ExitProcess

TestFail:
    call PrintFailure_ml64
    mov ecx, 1
    jmp ExitProcess
Main ENDP

END
