; ============================================================================
; IDE_10_LOGGING.asm - Logging subsystem wrapper
; ============================================================================
include IDE_INC.ASM

; External from logging_phase3.asm
EXTERN LogSystem_Initialize:PROC
EXTERN LogMessage:PROC
EXTERN ShutdownLogging:PROC

PUBLIC ErrorLogging_Init
PUBLIC ErrorLogging_LogEvent
PUBLIC ErrorLogging_Shutdown

.code

ErrorLogging_Init PROC
    call LogSystem_Initialize
    LOG CSTR("ErrorLogging_Init"), eax
    ret
ErrorLogging_Init ENDP

ErrorLogging_LogEvent PROC dwLevel:DWORD, pMsg:DWORD
    push pMsg
    push dwLevel
    call LogMessage
    add esp, 8
    ret
ErrorLogging_LogEvent ENDP

ErrorLogging_Shutdown PROC
    call ShutdownLogging
    LOG CSTR("ErrorLogging_Shutdown"), eax
    ret
ErrorLogging_Shutdown ENDP

END
