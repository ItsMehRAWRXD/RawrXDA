;============================================================================
; OS Explorer Interceptor - Simplified Version
; Compatible with Visual Studio's ml64.exe
;============================================================================

TRUE equ 1

.code

;============================================================================
; DLL MAIN
;============================================================================

DllMain PROC hInstDLL:QWORD, fdwReason:DWORD, lpvReserved:QWORD
    mov rax, TRUE
    ret
DllMain ENDP

;============================================================================
; EXPORTED FUNCTIONS
;============================================================================

; Install OS hooks
InstallOSHooks PROC dwTargetPID:DWORD
    mov rax, 1
    ret
InstallOSHooks ENDP

; Start interceptor
StartInterceptor PROC
    mov rax, 1
    ret
StartInterceptor ENDP

; Stop interceptor
StopInterceptor PROC
    mov rax, 1
    ret
StopInterceptor ENDP

; Show status
ShowStatus PROC
    mov rax, 1
    ret
ShowStatus ENDP

; Show statistics
ShowStats PROC
    mov rax, 1
    ret
ShowStats ENDP

; Clear log
ClearLog PROC
    mov rax, 1
    ret
ClearLog ENDP

; Show help
ShowHelp PROC
    mov rax, 1
    ret
ShowHelp ENDP

; Stream to PowerShell
StreamToPowerShell PROC pMessage:QWORD
    mov rax, 1
    ret
StreamToPowerShell ENDP

; Stream success message
StreamSuccessMessage PROC pMessage:QWORD
    mov rax, 1
    ret
StreamSuccessMessage ENDP

; Stream error message
StreamErrorMessage PROC pMessage:QWORD
    mov rax, 1
    ret
StreamErrorMessage ENDP

; Initialize OS interceptor
InitOSInterceptor PROC
    mov rax, 1
    ret
InitOSInterceptor ENDP

END
