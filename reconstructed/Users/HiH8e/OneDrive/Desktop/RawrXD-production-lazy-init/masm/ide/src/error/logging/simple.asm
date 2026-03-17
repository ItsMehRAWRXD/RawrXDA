;==============================================================================
; ERROR_LOGGING.ASM - Simple Error Logging System
;==============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

;==============================================================================
; DATA SECTION
;==============================================================================

.DATA

LOG_FILE_PATH         db "C:\\RawrXD\\logs\\ide_errors.log", 0

; Log levels
LOG_INFO              equ 1
LOG_WARNING           equ 2
LOG_ERROR             equ 3
LOG_FATAL             equ 4

; Log messages
szLogInit             db "Logging system initialized", 0
szLogShutdown         db "Logging system shutdown", 0

;==============================================================================
; CODE SECTION
;==============================================================================

.CODE

PUBLIC InitializeLogging
PUBLIC ShutdownLogging
PUBLIC LogMessage

;==============================================================================
; InitializeLogging - Setup logging system
;==============================================================================
InitializeLogging PROC PUBLIC
    ; Simple initialization - just return success for now
    mov eax, TRUE
    ret

InitializeLogging ENDP

;==============================================================================
; ShutdownLogging - Cleanup logging system
;==============================================================================
ShutdownLogging PROC PUBLIC
    ; Simple shutdown - nothing to clean up for now
    ret

ShutdownLogging ENDP

;==============================================================================
; LogMessage - Simple message logging
;==============================================================================
LogMessage PROC PUBLIC level:DWORD, message:DWORD
    LOCAL hFile:DWORD
    LOCAL dwWritten:DWORD
    LOCAL szBuffer[256]:BYTE
    LOCAL szTimestamp[32]:BYTE
    
    ; Open log file for appending
    invoke CreateFileA, addr LOG_FILE_PATH, \
                       GENERIC_WRITE, \
                       FILE_SHARE_READ, \
                       0, \
                       OPEN_ALWAYS, \
                       FILE_ATTRIBUTE_NORMAL, \
                       0
    mov hFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    je LogMessage_Error
    
    ; Seek to end of file
    invoke SetFilePointer, hFile, 0, 0, FILE_END
    
    ; Format and write log message with timestamp
    invoke GetLocalTime, addr szTimestamp
    
    invoke wsprintfA, addr szBuffer, \
                      addr szLogFormat, \
                      level, \
                      message
    
    ; Write to file
    invoke WriteFile, hFile, addr szBuffer, 256, addr dwWritten, 0
    
    ; Close file handle
    invoke CloseHandle, hFile
    
    mov eax, TRUE
    ret
    
LogMessage_Error:
    mov eax, FALSE
    ret

LogMessage ENDP

end