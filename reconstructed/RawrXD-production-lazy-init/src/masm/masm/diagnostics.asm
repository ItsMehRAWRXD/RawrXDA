; MASM Diagnostic Utilities
; Real-time error checking and diagnostics for MASM code

.686
.model flat, stdcall
option casemap:none

include \\masm32\\include\\masm32rt.inc
include \\masm32\\include\\kernel32.inc
include \\masm32\\include\\user32.inc
includelib \\masm32\\lib\\masm32.lib
includelib \\masm32\\lib\\kernel32.lib
includelib \\masm32\\lib\\user32.lib

.data
    ; Diagnostic messages
    szDiagTitle db "MASM Diagnostics",0
    szErrorFormat db "Error: %s at line %d",0
    szWarningFormat db "Warning: %s at line %d",0
    szInfoFormat db "Info: %s at line %d",0
    
    ; Error types
    szStackImbalance db "Stack imbalance detected",0
    szRegisterClobber db "Register clobbered without saving",0
    szMemoryLeak db "Potential memory leak",0
    szNullPointer db "Null pointer dereference",0
    szBufferOverflow db "Potential buffer overflow",0
    szDivisionByZero db "Division by zero risk",0
    
    ; Diagnostic state
    bDiagnosticsEnabled db 1
    dwErrorCount dd 0
    dwWarningCount dd 0
    
.code

; Enable/disable diagnostics
EnableDiagnostics proc bEnable:BOOL
    mov eax, bEnable
    mov bDiagnosticsEnabled, al
    ret
EnableDiagnostics endp

; Report an error
ReportError proc pMessage:PTR BYTE, dwLine:DWORD
    .if bDiagnosticsEnabled
        inc dwErrorCount
        
        ; Format error message
        LOCAL szBuffer[256]:BYTE
        invoke wsprintf, addr szBuffer, addr szErrorFormat, pMessage, dwLine
        
        ; Display error (could be logged to file in production)
        invoke MessageBox, NULL, addr szBuffer, addr szDiagTitle, MB_OK or MB_ICONERROR
    .endif
    ret
ReportError endp

; Report a warning
ReportWarning proc pMessage:PTR BYTE, dwLine:DWORD
    .if bDiagnosticsEnabled
        inc dwWarningCount
        
        ; Format warning message
        LOCAL szBuffer[256]:BYTE
        invoke wsprintf, addr szBuffer, addr szWarningFormat, pMessage, dwLine
        
        ; Display warning
        invoke MessageBox, NULL, addr szBuffer, addr szDiagTitle, MB_OK or MB_ICONWARNING
    .endif
    ret
ReportWarning endp

; Report informational message
ReportInfo proc pMessage:PTR BYTE, dwLine:DWORD
    .if bDiagnosticsEnabled
        ; Format info message
        LOCAL szBuffer[256]:BYTE
        invoke wsprintf, addr szBuffer, addr szInfoFormat, pMessage, dwLine
        
        ; Display info (could be logged)
        invoke MessageBox, NULL, addr szBuffer, addr szDiagTitle, MB_OK or MB_ICONINFORMATION
    .endif
    ret
ReportInfo endp

; Stack imbalance check
CheckStackImbalance proc dwExpected:DWORD, dwLine:DWORD
    LOCAL dwActual:DWORD
    
    ; This would need runtime stack checking
    ; For now, just a placeholder
    
    ret
CheckStackImbalance endp

; Register preservation check
CheckRegisterPreservation proc dwRegisters:DWORD, dwLine:DWORD
    ; Check if critical registers are preserved
    ; EBP, ESP, EDI, ESI should typically be preserved
    
    ret
CheckRegisterPreservation endp

; Memory allocation check
CheckMemoryAllocation proc pMemory:PTR, dwSize:DWORD, dwLine:DWORD
    .if pMemory == 0
        invoke ReportError, addr szNullPointer, dwLine
        xor eax, eax
        ret
    .endif
    
    ; Check for reasonable size
    .if dwSize == 0 || dwSize > 0x1000000 ; 16MB limit
        invoke ReportWarning, addr szBufferOverflow, dwLine
    .endif
    
    mov eax, pMemory
    ret
CheckMemoryAllocation endp

; Pointer validation
ValidatePointer proc pPointer:PTR, dwLine:DWORD
    .if pPointer == 0
        invoke ReportError, addr szNullPointer, dwLine
        xor eax, eax
        ret
    .endif
    
    ; Basic validation - check if pointer is in reasonable range
    mov eax, pPointer
    .if eax < 0x10000 || eax > 0x7FFFFFFF
        invoke ReportWarning, addr szNullPointer, dwLine
    .endif
    
    mov eax, pPointer
    ret
ValidatePointer endp

; Division safety check
CheckDivisionSafety proc dwDivisor:DWORD, dwLine:DWORD
    .if dwDivisor == 0
        invoke ReportError, addr szDivisionByZero, dwLine
        xor eax, eax
        ret
    .endif
    
    mov eax, 1
    ret
CheckDivisionSafety endp

; Buffer bounds check
CheckBufferBounds proc pBuffer:PTR, dwBufferSize:DWORD, dwOffset:DWORD, dwDataSize:DWORD, dwLine:DWORD
    .if dwOffset >= dwBufferSize
        invoke ReportError, addr szBufferOverflow, dwLine
        xor eax, eax
        ret
    .endif
    
    mov eax, dwOffset
    add eax, dwDataSize
    .if eax > dwBufferSize
        invoke ReportError, addr szBufferOverflow, dwLine
        xor eax, eax
        ret
    .endif
    
    mov eax, 1
    ret
CheckBufferBounds endp

; Get diagnostic statistics
GetDiagnosticStats proc pErrorCount:PTR DWORD, pWarningCount:PTR DWORD
    mov eax, pErrorCount
    .if eax
        mov ecx, dwErrorCount
        mov [eax], ecx
    .endif
    
    mov eax, pWarningCount
    .if eax
        mov ecx, dwWarningCount
        mov [eax], ecx
    .endif
    
    ret
GetDiagnosticStats endp

; Reset diagnostic counters
ResetDiagnosticCounters proc
    mov dwErrorCount, 0
    mov dwWarningCount, 0
    ret
ResetDiagnosticCounters endp

; Memory leak detection (basic)
TrackMemoryAllocation proc pMemory:PTR, dwSize:DWORD, dwLine:DWORD
    ; In a full implementation, this would maintain a list of allocations
    invoke CheckMemoryAllocation, pMemory, dwSize, dwLine
    ret
TrackMemoryAllocation endp

TrackMemoryFree proc pMemory:PTR, dwLine:DWORD
    invoke ValidatePointer, pMemory, dwLine
    ret
TrackMemoryFree endp

; String safety functions
SafeStrCopy proc pDest:PTR BYTE, dwDestSize:DWORD, pSrc:PTR BYTE, dwLine:DWORD
    invoke ValidatePointer, pDest, dwLine
    invoke ValidatePointer, pSrc, dwLine
    
    ; Check source string length
    invoke lstrlen, pSrc
    mov ecx, eax
    inc ecx ; Include null terminator
    
    invoke CheckBufferBounds, pDest, dwDestSize, 0, ecx, dwLine
    .if eax == 0
        ret
    .endif
    
    invoke lstrcpyn, pDest, pSrc, dwDestSize
    ret
SafeStrCopy endp

SafeStrCat proc pDest:PTR BYTE, dwDestSize:DWORD, pSrc:PTR BYTE, dwLine:DWORD
    invoke ValidatePointer, pDest, dwLine
    invoke ValidatePointer, pSrc, dwLine
    
    ; Get current length and source length
    invoke lstrlen, pDest
    mov ebx, eax
    invoke lstrlen, pSrc
    mov ecx, eax
    
    add ecx, ebx
    inc ecx ; Include null terminator
    
    invoke CheckBufferBounds, pDest, dwDestSize, ebx, ecx, dwLine
    .if eax == 0
        ret
    .endif
    
    ; Safe concatenation
    add pDest, ebx
    invoke lstrcpyn, pDest, pSrc, dwDestSize - ebx
    ret
SafeStrCat endp

end