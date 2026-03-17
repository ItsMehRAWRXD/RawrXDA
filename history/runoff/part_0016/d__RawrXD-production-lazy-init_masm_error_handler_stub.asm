; ============================================================================
; ERROR HANDLER STUB - Minimal implementation for extended harness
; ============================================================================
; Provides ErrorHandler functions needed by extended harness linking
; ============================================================================

OPTION CASEMAP:NONE

public ErrorHandler_Initialize
public ErrorHandler_Cleanup
public ErrorHandler_CaptureError
public ErrorHandler_GenerateReport
public ErrorHandler_GetErrorCount
public ErrorHandler_ClearErrors

; Global state
.data
align 8
dwErrorCount dd 0
dwReserved dd 0

.code

; ============================================================================
; ErrorHandler_Initialize
; RCX = pointer to error handler structure
; Returns: EAX = success (0)
; ============================================================================
ErrorHandler_Initialize PROC
    lea rax, [dwErrorCount]
    mov dword ptr [rax], 0
    xor eax, eax
    ret
ErrorHandler_Initialize ENDP

; ============================================================================
; ErrorHandler_Cleanup
; RCX = pointer to error handler structure
; Returns: EAX = success (0)
; ============================================================================
ErrorHandler_Cleanup PROC
    xor eax, eax
    ret
ErrorHandler_Cleanup ENDP

; ============================================================================
; ErrorHandler_CaptureError
; RCX = pointer to error handler
; RDX = error code
; R8 = error message string
; R9 = severity level
; Returns: EAX = success (0)
; ============================================================================
ErrorHandler_CaptureError PROC
    lea rax, [dwErrorCount]
    add dword ptr [rax], 1
    xor eax, eax
    ret
ErrorHandler_CaptureError ENDP

; ============================================================================
; ErrorHandler_GenerateReport
; RCX = pointer to error handler
; RDX = report buffer
; R8 = buffer size
; Returns: EAX = report length
; ============================================================================
ErrorHandler_GenerateReport PROC
    xor eax, eax
    ret
ErrorHandler_GenerateReport ENDP

; ============================================================================
; ErrorHandler_GetErrorCount
; RCX = pointer to error handler
; Returns: EAX = error count
; ============================================================================
ErrorHandler_GetErrorCount PROC
    lea rax, [dwErrorCount]
    mov eax, dword ptr [rax]
    ret
ErrorHandler_GetErrorCount ENDP

; ============================================================================
; ErrorHandler_ClearErrors
; RCX = pointer to error handler
; Returns: EAX = success (0)
; ============================================================================
ErrorHandler_ClearErrors PROC
    lea rax, [dwErrorCount]
    mov dword ptr [rax], 0
    xor eax, eax
    ret
ErrorHandler_ClearErrors ENDP

end
