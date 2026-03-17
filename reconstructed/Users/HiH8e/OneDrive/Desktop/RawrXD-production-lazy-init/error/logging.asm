; error_logging.asm
; Production-Ready Error Logging System for RawrXD IDE
; Phase 5 Implementation

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
includelib \masm32\lib\kernel32.lib

; ==================== CONSTANTS ====================
LOG_LEVEL_INFO    equ 1
LOG_LEVEL_WARNING equ 2
LOG_LEVEL_ERROR   equ 3
LOG_LEVEL_FATAL   equ 4

LOG_FILE_NAME     equ "rawrxd.log"
MAX_LOG_SIZE      equ 1048576  ; 1MB
MAX_BACKUPS       equ 5

; ==================== DATA SECTION ====================
.data
hLogFile        HANDLE 0
logBuffer       db 2048 dup(0)
timestampBuffer db 32 dup(0)
backupPattern   db "rawrxd.log.%d",0

; ==================== CODE SECTION ====================
.code

; ----------------------------------------------------
; Get current timestamp in ISO 8601 format
; Output: timestampBuffer
; ----------------------------------------------------
GetTimestamp proc
    LOCAL sysTime:SYSTEMTIME
    
    invoke GetLocalTime, addr sysTime
    invoke wsprintf, addr timestampBuffer, $"%04d-%02d-%02d %02d:%02d:%02d",
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond
    ret
GetTimestamp endp

; ----------------------------------------------------
; Rotate log files when size exceeds MAX_LOG_SIZE
; ----------------------------------------------------
RotateLogs proc
    LOCAL hFile:HANDLE
    LOCAL fileSize:DWORD
    LOCAL i:DWORD
    
    ; Check current log size
    invoke CreateFile, addr LOG_FILE_NAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL
    .IF eax != INVALID_HANDLE_VALUE
        mov hFile, eax
        invoke GetFileSize, hFile, NULL
        mov fileSize, eax
        invoke CloseHandle, hFile
        
        .IF fileSize >= MAX_LOG_SIZE
            ; Rotate existing backups
            mov ecx, MAX_BACKUPS-1
            .WHILE ecx > 0
                invoke wsprintf, addr logBuffer, addr backupPattern, ecx
                invoke DeleteFile, addr logBuffer
                invoke MoveFile, addr logBuffer, addr logBuffer+1
                dec ecx
            .ENDW
            
            ; Rename current log
            invoke MoveFile, addr LOG_FILE_NAME, addr backupPattern
        .ENDIF
    .ENDIF
    ret
RotateLogs endp

; ----------------------------------------------------
; Main logging procedure
; Parameters:
;   level   - Log level (1-4)
;   message - Pointer to message string
; ----------------------------------------------------
LogMessage proc level:DWORD, message:DWORD
    LOCAL bytesWritten:DWORD
    
    ; Ensure log file is open
    .IF hLogFile == 0
        invoke CreateFile, addr LOG_FILE_NAME, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
        mov hLogFile, eax
        .IF hLogFile == INVALID_HANDLE_VALUE
            ret
        .ENDIF
        invoke SetFilePointer, hLogFile, 0, NULL, FILE_END
    .ENDIF

    ; Rotate logs if needed
    invoke RotateLogs

    ; Format timestamp
    invoke GetTimestamp

    ; Format log entry
    invoke wsprintf, addr logBuffer, $"[%s] [%s] %s\r\n",
        addr timestampBuffer,
        $"INFO,WARNING,ERROR,FATAL"[level*7-7],
        message

    ; Write to log
    invoke lstrlen, addr logBuffer
    invoke WriteFile, hLogFile, addr logBuffer, eax, addr bytesWritten, NULL
    
    ; Flush to ensure immediate write
    invoke FlushFileBuffers, hLogFile
    ret
LogMessage endp

; ----------------------------------------------------
; Close log file on application exit
; ----------------------------------------------------
CloseLogger proc
    .IF hLogFile != 0
        invoke CloseHandle, hLogFile
        mov hLogFile, 0
    .ENDIF
    ret
CloseLogger endp

; ----------------------------------------------------
; Example usage (for documentation)
; ----------------------------------------------------
; invoke LogMessage, LOG_LEVEL_ERROR, offset $"Failed to initialize file system"

end