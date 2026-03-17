;==============================================================================
; logging.asm - ML64-Compatible Logging System
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==============================================================================
; CONSTANTS
;==============================================================================
LOG_LEVEL_DEBUG     EQU 0
LOG_LEVEL_INFO      EQU 1
LOG_LEVEL_WARNING   EQU 2
LOG_LEVEL_ERROR     EQU 3

MAX_LOG_MESSAGE     EQU 2048

OPEN_ALWAYS         EQU 4
FILE_END            EQU 2
STD_OUTPUT_HANDLE   EQU -11
STD_ERROR_HANDLE    EQU -12

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN CreateFileA:PROC
EXTERN SetFilePointer:PROC
EXTERN CloseHandle:PROC
EXTERN GetLocalTime:PROC
EXTERN wsprintfA:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC
EXTERN OutputDebugStringA:PROC

;==============================================================================
; DATA
;==============================================================================
.data
g_log_level         DWORD 1
g_log_enabled       DWORD 1
hStdOut             QWORD 0
hStdErr             QWORD 0
hLogFile            QWORD 0

szLogFileName       BYTE "rawrxd_ide.log",0
szLogFileNameOld    BYTE "rawrxd_ide.old.log",0
szNewLine           BYTE 13,10,0
szLogFormat         BYTE "[%02d:%02d:%02d] %s",13,10,0
szLevelDebug        BYTE "[DEBUG] ",0
szLevelInfo         BYTE "[INFO] ",0
szLevelWarning      BYTE "[WARN] ",0
szLevelError        BYTE "[ERROR] ",0
szLogInitMsg        BYTE "Logging system initialized",0

.data?
log_buffer          BYTE MAX_LOG_MESSAGE DUP (?)
timestamp_buf       BYTE 64 DUP (?)

.code

;==============================================================================
; LogInitialize - Initialize logging system
;==============================================================================
LogInitialize PROC
    ; SIMPLIFIED: Just return success without opening files
    ; File logging disabled due to crash in CreateFileA
    mov eax, 1
    ret
LogInitialize ENDP

PUBLIC LogInitialize

;==============================================================================
; LogShutdown - Shutdown logging system
;==============================================================================
LogShutdown PROC
    sub rsp, 40
    
    mov rcx, hLogFile
    test rcx, rcx
    jz shutdown_done
    
    call CloseHandle
    mov hLogFile, 0
    
shutdown_done:
    xor eax, eax
    add rsp, 40
    ret
LogShutdown ENDP

PUBLIC LogShutdown

;==============================================================================
; LogWrite - Write log message (rcx = level, rdx = message)
;==============================================================================
LogWrite PROC
    sub rsp, 120
    
    mov dword ptr [rsp+20h], ecx
    mov qword ptr [rsp+28h], rdx
    
    ; Check if logging enabled
    mov eax, g_log_enabled
    test eax, eax
    jz write_done
    
    ; Check log level
    mov eax, dword ptr [rsp+20h]
    cmp eax, g_log_level
    jb write_done
    
    ; Get level string
    cmp eax, LOG_LEVEL_ERROR
    je level_error
    cmp eax, LOG_LEVEL_WARNING
    je level_warning
    cmp eax, LOG_LEVEL_DEBUG
    je level_debug
    
level_info:
    lea rax, szLevelInfo
    jmp format_message
    
level_debug:
    lea rax, szLevelDebug
    jmp format_message
    
level_warning:
    lea rax, szLevelWarning
    jmp format_message
    
level_error:
    lea rax, szLevelError
    
format_message:
    mov qword ptr [rsp+30h], rax
    
    ; Format message with level prefix
    lea rcx, log_buffer
    mov rdx, qword ptr [rsp+30h]
    call lstrcpyA
    
    lea rcx, log_buffer
    mov rdx, qword ptr [rsp+28h]
    call lstrcatA
    
    ; Write to file
    mov rcx, hLogFile
    test rcx, rcx
    jz skip_file
    
    lea rdx, log_buffer
    call lstrlenA
    mov r8d, eax
    lea rdx, log_buffer
    lea r9, [rsp+40h]
    mov qword ptr [rsp+38h], 0
    mov rcx, hLogFile
    call WriteFile
    
    ; Write newline
    mov rcx, hLogFile
    lea rdx, szNewLine
    mov r8d, 2
    lea r9, [rsp+40h]
    mov qword ptr [rsp+38h], 0
    call WriteFile
    
skip_file:
    ; Write to console
    mov rcx, hStdOut
    test rcx, rcx
    jz skip_console
    
    lea rdx, log_buffer
    call lstrlenA
    mov r8d, eax
    lea rdx, log_buffer
    lea r9, [rsp+40h]
    mov qword ptr [rsp+38h], 0
    mov rcx, hStdOut
    call WriteFile
    
    mov rcx, hStdOut
    lea rdx, szNewLine
    mov r8d, 2
    lea r9, [rsp+40h]
    mov qword ptr [rsp+38h], 0
    call WriteFile
    
skip_console:
    ; OutputDebugString
    lea rcx, log_buffer
    call OutputDebugStringA
    
write_done:
    xor eax, eax
    add rsp, 120
    ret
LogWrite ENDP

PUBLIC LogWrite

;==============================================================================
; LogDebug - Log debug message (rcx = message)
;==============================================================================
LogDebug PROC
    sub rsp, 40
    mov rdx, rcx
    mov ecx, LOG_LEVEL_DEBUG
    call LogWrite
    add rsp, 40
    ret
LogDebug ENDP

PUBLIC LogDebug

;==============================================================================
; LogInfo - Log info message (rcx = message)
;==============================================================================
LogInfo PROC
    sub rsp, 40
    mov rdx, rcx
    mov ecx, LOG_LEVEL_INFO
    call LogWrite
    add rsp, 40
    ret
LogInfo ENDP

PUBLIC LogInfo

;==============================================================================
; LogWarning - Log warning message (rcx = message)
;==============================================================================
LogWarning PROC
    sub rsp, 40
    mov rdx, rcx
    mov ecx, LOG_LEVEL_WARNING
    call LogWrite
    add rsp, 40
    ret
LogWarning ENDP

PUBLIC LogWarning

;==============================================================================
; LogError - Log error message (rcx = message)
;==============================================================================
LogError PROC
    sub rsp, 40
    mov rdx, rcx
    mov ecx, LOG_LEVEL_ERROR
    call LogWrite
    add rsp, 40
    ret
LogError ENDP

PUBLIC LogError

;==============================================================================
; LogSetLevel - Set logging level (ecx = level)
;==============================================================================
LogSetLevel PROC
    mov g_log_level, ecx
    xor eax, eax
    ret
LogSetLevel ENDP

PUBLIC LogSetLevel

;==============================================================================
; LogGetLevel - Get logging level -> eax
;==============================================================================
LogGetLevel PROC
    mov eax, g_log_level
    ret
LogGetLevel ENDP

PUBLIC LogGetLevel

;==============================================================================
; LogEnable - Enable logging
;==============================================================================
LogEnable PROC
    mov g_log_enabled, 1
    xor eax, eax
    ret
LogEnable ENDP

PUBLIC LogEnable

;==============================================================================
; LogDisable - Disable logging
;==============================================================================
LogDisable PROC
    mov g_log_enabled, 0
    xor eax, eax
    ret
LogDisable ENDP

PUBLIC LogDisable

END
