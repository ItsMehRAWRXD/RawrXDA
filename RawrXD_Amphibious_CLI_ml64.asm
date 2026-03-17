;=============================================================================
; RawrXD_Amphibious_CLI_ml64.asm
; Console entry for amphibious core with real HTTP inference
;=============================================================================
OPTION CASEMAP:NONE

EXTERN ExitProcess:PROC
EXTERN InitializeAmphibiousCore:PROC
EXTERN RunAutonomousCycle_ml64:PROC
EXTERN WriteTelemetryJson_ml64:PROC
EXTERN GetStageMask_ml64:PROC
EXTERN GetCycleCount_ml64:PROC
EXTERN PrintSuccess_ml64:PROC
EXTERN PrintFailure_ml64:PROC

STAGE_COMPLETE EQU 003Fh
TEST_CYCLES    EQU 6
MODE_CLI       EQU 0

.data
    szCliPrompt DB "Explain x86-64 MASM calling conventions and stack frame setup.",0

.code

main PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 40
    .ALLOCSTACK 40
    .ENDPROLOG

    ; Initialize Core2 with real inference
    xor rcx, rcx                  ; hMainWindow = NULL
    xor rdx, rdx                  ; hEditorWindow = NULL
    lea r8, [szCliPrompt]
    mov r9d, MODE_CLI
    call InitializeAmphibiousCore

    test eax, eax
    jz cli_init_fail

    ; Run inference cycles
    mov ebx, TEST_CYCLES
cli_cycle_loop:
    test ebx, ebx
    jz cli_verify

    call RunAutonomousCycle_ml64
    
    test eax, eax
    jz cli_cycle_fail

    dec ebx
    jmp cli_cycle_loop

cli_verify:
    ; Check completion mask
    call GetStageMask_ml64
    cmp rax, STAGE_COMPLETE
    jne cli_fail

    ; Write telemetry
    xor rcx, rcx
    call WriteTelemetryJson_ml64

    call PrintSuccess_ml64
    xor ecx, ecx
    jmp cli_exit

cli_cycle_fail:
cli_init_fail:
cli_fail:
    call PrintFailure_ml64
    mov ecx, 1

cli_exit:
    add rsp, 40
    pop rbx
    pop rbp
    call ExitProcess
main ENDP

END
