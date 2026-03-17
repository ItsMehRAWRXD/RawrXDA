; log_analysis.asm
; Production-Ready Log Analysis Utilities for RawrXD IDE
; Phase 5 Implementation

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ==================== CONSTANTS ====================
MAX_LINE_LENGTH   equ 4096
MAX_REPORT_SIZE   equ 65536
LOG_FILE_NAME     equ "rawrxd.log"

; ==================== DATA SECTION ====================
.data
hLogFile        HANDLE 0
lineBuffer      db MAX_LINE_LENGTH dup(0)
reportBuffer    db MAX_REPORT_SIZE dup(0)
searchPattern   db 256 dup(0)

; ==================== CODE SECTION ====================
.code

; ----------------------------------------------------
; Parse a log line into components
; Input:  esi = pointer to log line
; Output: ecx = timestamp, edx = level, eax = message
; ----------------------------------------------------
ParseLogLine proc
    ; Find first bracket
    mov edi, esi
    mov ecx, MAX_LINE_LENGTH
    mov al, '['
    repne scasb
    .IF ZERO?
        mov ecx, edi
        ; Find second bracket
        mov al, ']'
        repne scasb
        .IF ZERO?
            mov edx, edi
            ; Find third bracket
            mov al, '['
            repne scasb
            .IF ZERO?
                mov eax, edi
                ; Find fourth bracket
                mov al, ']'
                repne scasb
                .IF ZERO?
                    mov [edi-1], 0
                    mov [edx-1], 0
                    mov [ecx-1], 0
                    ret
                .ENDIF
            .ENDIF
        .ENDIF
    .ENDIF
    xor ecx, ecx
    xor edx, edx
    xor eax, eax
    ret
ParseLogLine endp

; ----------------------------------------------------
; Filter logs by level
; Input:  level = log level to filter
; Output: reportBuffer with filtered entries
; ----------------------------------------------------
FilterByLevel proc level:DWORD
    LOCAL bytesRead:DWORD
    LOCAL lineCount:DWORD
    
    mov lineCount, 0
    invoke CreateFile, addr LOG_FILE_NAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL
    .IF eax != INVALID_HANDLE_VALUE
        mov hLogFile, eax
        
        .WHILE 1
            invoke ReadFile, hLogFile, addr lineBuffer, MAX_LINE_LENGTH-1, addr bytesRead, NULL
            .BREAK .IF !eax || !bytesRead
            mov byte ptr [lineBuffer+bytesRead], 0
            
            ; Parse line
            lea esi, lineBuffer
            invoke ParseLogLine
            .IF ecx && edx && eax
                ; Check level
                invoke lstrcmp, edx, $"INFO"
                .IF eax == 0 && level == LOG_LEVEL_INFO
                    jmp AddToReport
                .ENDIF
                invoke lstrcmp, edx, $"WARNING"
                .IF eax == 0 && level == LOG_LEVEL_WARNING
                    jmp AddToReport
                .ENDIF
                invoke lstrcmp, edx, $"ERROR"
                .IF eax == 0 && level == LOG_LEVEL_ERROR
                    jmp AddToReport
                .ENDIF
                invoke lstrcmp, edx, $"FATAL"
                .IF eax == 0 && level == LOG_LEVEL_FATAL
AddToReport:
                    invoke lstrlen, addr lineBuffer
                    invoke lstrcat, addr reportBuffer, addr lineBuffer
                    inc lineCount
                .ENDIF
            .ENDIF
        .ENDW
        
        invoke CloseHandle, hLogFile
    .ENDIF
    ret
FilterByLevel endp

; ----------------------------------------------------
; Search logs for pattern
; Input:  pattern = search string
; Output: reportBuffer with matching entries
; ----------------------------------------------------
SearchLogs proc pattern:DWORD
    LOCAL bytesRead:DWORD
    LOCAL lineCount:DWORD
    
    mov lineCount, 0
    invoke CreateFile, addr LOG_FILE_NAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL
    .IF eax != INVALID_HANDLE_VALUE
        mov hLogFile, eax
        
        .WHILE 1
            invoke ReadFile, hLogFile, addr lineBuffer, MAX_LINE_LENGTH-1, addr bytesRead, NULL
            .BREAK .IF !eax || !bytesRead
            mov byte ptr [lineBuffer+bytesRead], 0
            
            ; Search for pattern
            invoke lstrstr, addr lineBuffer, pattern
            .IF eax
                invoke lstrlen, addr lineBuffer
                invoke lstrcat, addr reportBuffer, addr lineBuffer
                inc lineCount
            .ENDIF
        .ENDW
        
        invoke CloseHandle, hLogFile
    .ENDIF
    ret
SearchLogs endp

; ----------------------------------------------------
; Generate error summary report
; Output: reportBuffer with summary statistics
; ----------------------------------------------------
GenerateErrorSummary proc
    LOCAL errorCount:DWORD
    LOCAL warningCount:DWORD
    LOCAL fatalCount:DWORD
    
    mov errorCount, 0
    mov warningCount, 0
    mov fatalCount, 0

    invoke CreateFile, addr LOG_FILE_NAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL
    .IF eax != INVALID_HANDLE_VALUE
        mov hLogFile, eax
        
        .WHILE 1
            invoke ReadFile, hLogFile, addr lineBuffer, MAX_LINE_LENGTH-1, addr bytesRead, NULL
            .BREAK .IF !eax || !bytesRead
            mov byte ptr [lineBuffer+bytesRead], 0
            
            ; Parse line
            lea esi, lineBuffer
            invoke ParseLogLine
            .IF ecx && edx && eax
                invoke lstrcmp, edx, $"ERROR"
                .IF eax == 0
                    inc errorCount
                .ENDIF
                invoke lstrcmp, edx, $"WARNING"
                .IF eax == 0
                    inc warningCount
                .ENDIF
                invoke lstrcmp, edx, $"FATAL"
                .IF eax == 0
                    inc fatalCount
                .ENDIF
            .ENDIF
        .ENDW
        
        invoke CloseHandle, hLogFile
        
        ; Format summary
        invoke wsprintf, addr reportBuffer, $"Error Summary:\r\n"\
            "Total Errors: %d\r\n"\
            "Warnings: %d\r\n"\
            "Fatal Errors: %d\r\n",
            errorCount, warningCount, fatalCount
    .ENDIF
    ret
GenerateErrorSummary endp

; ----------------------------------------------------
; Example usage (for documentation)
; ----------------------------------------------------
; invoke FilterByLevel, LOG_LEVEL_ERROR
; invoke SearchLogs, offset $"memory leak"
; invoke GenerateErrorSummary

end