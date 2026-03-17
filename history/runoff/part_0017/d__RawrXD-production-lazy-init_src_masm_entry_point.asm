; =====================================================================
; RAWRXD IDE - ENTRY POINT
; Minimal main entry point for Windows console application
; =====================================================================

PUBLIC mainCRTStartup

EXTERN AgenticKernelInit:PROC
EXTERN ExitProcess:PROC

.code

mainCRTStartup PROC
    ; Initialize agentic kernel
    call    AgenticKernelInit
    
    ; Exit with success code
    xor     ecx, ecx
    call    ExitProcess
    
mainCRTStartup ENDP

END
