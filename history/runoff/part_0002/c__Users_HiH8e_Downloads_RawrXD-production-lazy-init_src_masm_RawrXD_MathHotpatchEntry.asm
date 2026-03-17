; RawrXD_MathHotpatchEntry.asm
; Entry point for the MASM subsystem

include masm_hotpatch.inc

.code

EXTERN RawrXD_DualEngineStreamInit:PROC
EXTERN RawrXD_ManagerInit:PROC
EXTERN RawrXD_OptimizationLoop:PROC

; Initialize the entire MASM subsystem
; Returns: 0 on success, error code otherwise
RawrXD_InitializeSubsystem PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Initialize Streamer
    lea rcx, [RawrXD_DefaultModelPath]
    call RawrXD_DualEngineStreamInit
    test eax, eax
    jnz INIT_FAIL
    
    ; Initialize Manager
    call RawrXD_ManagerInit
    
    ; Start Optimization Loop (in a new thread ideally, but here we just init)
    ; CreateThread would be called here
    
    xor eax, eax                 ; Success
    jmp INIT_EXIT

INIT_FAIL:
    mov eax, 1                   ; Generic error

INIT_EXIT:
    add rsp, 40
    ret
RawrXD_InitializeSubsystem ENDP

.data
RawrXD_DefaultModelPath DB "model.gguf", 0

END
