; ============================================================================
; ERROR_LOGGING_ENHANCED.ASM - Full Logging System Implementation
; ============================================================================

.686
.model flat, stdcall
option casemap:none

includelib kernel32.lib

PUBLIC InitializeLogging
PUBLIC ShutdownLogging
PUBLIC LogMessage
PUBLIC GetLogLevelString
PUBLIC CheckLogRotation
PUBLIC OpenLogViewer

; ============================================================================
; CONSTANTS
; ============================================================================
NULL equ 0
TRUE equ 1
FALSE equ 0
MAX_LOG_SIZE equ 10485760  ; 10MB before rotation

; Log levels
LOG_DEBUG equ 0
LOG_INFO equ 1
LOG_WARNING equ 2
LOG_ERROR equ 3

; File API
GENERIC_READ equ 80000000h
GENERIC_WRITE equ 40000000h
FILE_SHARE_READ equ 1
OPEN_EXISTING equ 3
CREATE_ALWAYS equ 2
FILE_APPEND_DATA equ 4

; Win32 APIs
CreateFileA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
WriteFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
CloseHandle PROTO :DWORD
GetFileSize PROTO :DWORD,:DWORD
GetSystemTimeAsFileTime PROTO :DWORD
SetFilePointer PROTO :DWORD,:DWORD,:DWORD,:DWORD
CopyFileA PROTO :DWORD,:DWORD,:DWORD
DeleteFileA PROTO :DWORD
ShellExecuteA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD

.data
    g_hLogFile dd 0
    g_LogInitialized dd 0
    g_LogRotationCount dd 0
    
    szLogPath db "C:\masm_ide.log",0
    szLogPathBak db "C:\masm_ide.log.bak",0
    szLogHeader db "[MASM IDE LOG]",13,10,0
    szLogFormat db "[%s] %s",13,10,0
    
    szDebugLevel db "DEBUG",0
    szInfoLevel db "INFO",0
    szWarningLevel db "WARN",0
    szErrorLevel db "ERROR",0
    szUnknown db "UNKNOWN",0

.code

InitializeLogging proc
    ; Initialize logging system
    mov g_LogInitialized, TRUE
    
    ; Create/open log file
    invoke CreateFileA, addr szLogPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 80h, NULL
    mov g_hLogFile, eax
    
    ; Write header
    cmp eax, -1
    je @@init_fail
    
    invoke WriteFile, g_hLogFile, addr szLogHeader, 14, NULL, NULL
    mov eax, TRUE
    ret
    
@@init_fail:
    xor eax, eax
    ret
InitializeLogging endp

ShutdownLogging proc
    ; Close and finalize logging
    test g_hLogFile, g_hLogFile
    jz @@already_closed
    
    invoke CloseHandle, g_hLogFile
    mov g_hLogFile, 0
    
@@already_closed:
    mov eax, TRUE
    ret
ShutdownLogging endp

LogMessage proc level:dword, pMessage:dword
    LOCAL hFile:dword
    LOCAL bytesWritten:dword
    LOCAL levelStr:dword
    LOCAL pBuffer:dword
    LOCAL bufferSize:dword
    
    test g_hLogInitialized, g_hLogInitialized
    jz @@not_init
    
    ; Get log file handle or open it
    mov hFile, g_hLogFile
    test hFile, hFile
    jnz @@have_file
    
    invoke CreateFileA, addr szLogPath, GENERIC_WRITE or FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, 80h, NULL
    mov hFile, eax
    
@@have_file:
    ; Get level string
    mov eax, level
    cmp eax, LOG_DEBUG
    je @@debug_level
    cmp eax, LOG_INFO
    je @@info_level
    cmp eax, LOG_WARNING
    je @@warning_level
    cmp eax, LOG_ERROR
    je @@error_level
    
    mov levelStr, offset szUnknown
    jmp @@write_message
    
@@debug_level:
    mov levelStr, offset szDebugLevel
    jmp @@write_message
@@info_level:
    mov levelStr, offset szInfoLevel
    jmp @@write_message
@@warning_level:
    mov levelStr, offset szWarningLevel
    jmp @@write_message
@@error_level:
    mov levelStr, offset szErrorLevel
    
@@write_message:
    ; Write message with level prefix
    invoke WriteFile, hFile, levelStr, 8, addr bytesWritten, NULL
    invoke WriteFile, hFile, pMessage, 256, addr bytesWritten, NULL  ; Simplified: fixed size
    invoke WriteFile, hFile, addr szLogHeader, 2, addr bytesWritten, NULL  ; CRLF
    
    ; Close if we opened it
    cmp hFile, g_hLogFile
    je @@skip_close
    invoke CloseHandle, hFile
    
@@skip_close:
    mov eax, TRUE
    ret
    
@@not_init:
    xor eax, eax
    ret
LogMessage endp

GetLogLevelString proc level:dword
    mov eax, level
    cmp eax, LOG_DEBUG
    je @@ret_debug
    cmp eax, LOG_INFO
    je @@ret_info
    cmp eax, LOG_WARNING
    je @@ret_warning
    cmp eax, LOG_ERROR
    je @@ret_error
    
    mov eax, offset szUnknown
    ret
    
@@ret_debug:
    mov eax, offset szDebugLevel
    ret
@@ret_info:
    mov eax, offset szInfoLevel
    ret
@@ret_warning:
    mov eax, offset szWarningLevel
    ret
@@ret_error:
    mov eax, offset szErrorLevel
    ret
GetLogLevelString endp

CheckLogRotation proc
    LOCAL fileSize:dword
    
    ; Check if log file needs rotation
    test g_hLogFile, g_hLogFile
    jz @@no_rotate
    
    invoke GetFileSize, g_hLogFile, NULL
    mov fileSize, eax
    
    cmp fileSize, MAX_LOG_SIZE
    jl @@no_rotate
    
    ; Rotate: close current, backup, create new
    invoke CloseHandle, g_hLogFile
    invoke CopyFileA, addr szLogPath, addr szLogPathBak, FALSE
    invoke DeleteFileA, addr szLogPath
    
    ; Create new log file
    invoke CreateFileA, addr szLogPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 80h, NULL
    mov g_hLogFile, eax
    
    inc g_LogRotationCount
    mov eax, TRUE
    ret
    
@@no_rotate:
    xor eax, eax
    ret
CheckLogRotation endp

OpenLogViewer proc
    ; Open log file in default viewer (Notepad)
    invoke ShellExecuteA, NULL, offset szInfoLevel, addr szLogPath, NULL, NULL, 1
    mov eax, TRUE
    ret
OpenLogViewer endp

end
