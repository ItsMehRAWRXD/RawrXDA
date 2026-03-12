;=============================================================================
; RawrXD_AutoHeal_Test_v2.asm
; Autonomous Compilation/Test Loop & Self-Healing Validator
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
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    xor rcx, rcx
    xor rdx, rdx
    lea r8, szTestPrompt
    mov r9d, MODE_CLI
    call InitializeAmphibiousCore

    mov ebx, TEST_CYCLES
CycleLoop:
    test ebx, ebx
    jz VerifyDone
    call RunAutonomousCycle_ml64
    dec ebx
    jmp CycleLoop

VerifyDone:
    call GetStageMask_ml64
    cmp eax, STAGE_COMPLETE
    jne TestFail

    call PrintSuccess_ml64
    xor ecx, ecx
    jmp ExitProcess

TestFail:
    call PrintFailure_ml64
    mov ecx, 1
    jmp ExitProcess
Main ENDP

END
