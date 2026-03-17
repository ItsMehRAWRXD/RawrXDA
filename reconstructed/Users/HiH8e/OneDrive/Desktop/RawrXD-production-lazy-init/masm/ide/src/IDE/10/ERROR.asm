; ============================================================================
; IDE_10_ERROR.asm - Error logging with forwarding to common log
; ============================================================================
include IDE_INC.ASM

PUBLIC ErrorLogging_LogEvent

.code

ErrorLogging_LogEvent PROC dwLevel:DWORD, pMsg:DWORD
    ; forward to common log
    LOG CSTR("ErrorLogging_LogEvent"), dwLevel
    ret
ErrorLogging_LogEvent ENDP

END
