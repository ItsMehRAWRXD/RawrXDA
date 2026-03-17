;==============================================================================
; logging.asm - Production-Ready Logging System for RawrXD IDE
; ==============================================================================
; Implements comprehensive logging with multiple levels and output targets.
; Zero C++ runtime dependencies.
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

; Master include gives structured logging + metrics helpers
INCLUDE masm/masm_master_include.asm

;==============================================================================
; LOGGING CONSTANTS
;==============================================================================
LOG_LEVEL_DEBUG     EQU 0
LOG_LEVEL_INFO      EQU 1
LOG_LEVEL_WARNING   EQU 2
LOG_LEVEL_ERROR     EQU 3
LOG_LEVEL_SUCCESS   EQU 4

LOG_TARGET_CONSOLE  EQU 1
LOG_TARGET_FILE     EQU 2
LOG_TARGET_UI       EQU 4
LOG_TARGET_ALL      EQU 7

MAX_LOG_MESSAGE     EQU 2048
MAX_LOG_FILE_SIZE   EQU 10485760  ; 10MB

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN CreateFileA:PROC
EXTERN GetFileSize:PROC
EXTERN SetFilePointer:PROC
EXTERN CloseHandle:PROC
EXTERN GetTickCount64:PROC
EXTERN GetLocalTime:PROC
EXTERN wsprintfA:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC
EXTERN ui_add_chat_message:PROC

;==============================================================================
; STRUCTURES
;==============================================================================
SYSTEMTIME STRUCT
    wYear           WORD ?
    wMonth          WORD ?
    wDayOfWeek      WORD ?
    wDay            WORD ?
    wHour           WORD ?
    wMinute         WORD ?
    wSecond         WORD ?
    wMilliseconds   WORD ?
SYSTEMTIME ENDS

LOG_CONTEXT STRUCT
    log_level       DWORD ?
    log_targets     DWORD ?
    hLogFile        QWORD ?
    log_file_path   QWORD ?
    log_enabled     DWORD ?
LOG_CONTEXT ENDS

;==============================================================================
; DATA SEGMENT
;==============================================================================
.data?
    g_LogContext    LOG_CONTEXT <>
    hStdOut         QWORD ?
    hStdErr         QWORD ?
    hLogFile        QWORD ?
    log_buffer      BYTE MAX_LOG_MESSAGE DUP (?)
    timestamp_buf    BYTE 64 DUP (?)
    log_file_path   BYTE 260 DUP (?)

.data
    szLogFile       BYTE "rawrxd_ide.log",0
    szLogDir        BYTE ".",0
    szNewLine       BYTE 13,10,0
    szLogFormat     BYTE "[%02d:%02d:%02d] [%s] %s",13,10,0
    szLevelDebug    BYTE "DEBUG",0
    szLevelInfo     BYTE "INFO ",0
    szLevelWarning  BYTE "WARN ",0
    szLevelError    BYTE "ERROR",0
    szLevelSuccess  BYTE "SUCCESS",0
    szLogInitMsg    BYTE "Logging system initialized",0
    szLogFileError  BYTE "Failed to open log file",0

;==============================================================================
; CODE SEGMENT
;==============================================================================
.code

;==============================================================================
; PUBLIC: LogInitialize() -> eax
; Initializes the logging system
;==============================================================================
PUBLIC LogInitialize
ALIGN 16
LogInitialize PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    ; Initialize console handles
    mov rcx, -11  ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov hStdOut, rax
    
    mov rcx, -12  ; STD_ERROR_HANDLE
    call GetStdHandle
    mov hStdErr, rax
    
    ; Initialize log context
    mov [g_LogContext.log_level], LOG_LEVEL_INFO
    mov [g_LogContext.log_targets], LOG_TARGET_ALL
    mov [g_LogContext.log_enabled], 1
    
    ; Setup log file path
    lea rax, szLogFile
    mov [g_LogContext.log_file_path], rax
    
    ; Open log file for append
    lea rcx, szLogFile
    mov rdx, GENERIC_WRITE
    mov r8, FILE_SHARE_READ or FILE_SHARE_WRITE
    xor r9, r9
    mov [rsp + 32], r9  ; lpSecurityAttributes
    mov [rsp + 40], OPEN_ALWAYS
    mov [rsp + 48], FILE_ATTRIBUTE_NORMAL
    mov [rsp + 56], 0   ; hTemplateFile
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je log_init_fail
    
    mov hLogFile, rax
    mov [g_LogContext.hLogFile], rax
    
    ; Move to end of file
    mov rcx, rax
    xor rdx, rdx
    xor r8, r8
    mov r9, FILE_END
    call SetFilePointer
    
    ; Log initialization message
    lea rcx, szLogInitMsg
    call LogInfo

    mov rcx, LOG_LEVEL_INFO
    lea rdx, szLogInitMsg
    call Logger_LogStructured
    mov ecx, 1
    call Metrics_IncrementMissionCounter
    
    mov eax, 1
    jmp log_init_done
    
log_init_fail:
    mov [g_LogContext.hLogFile], 0
    xor eax, eax
    
log_init_done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
LogInitialize ENDP

;==============================================================================
; INTERNAL: GetCurrentTimestamp() -> rcx (pointer to timestamp string)
;==============================================================================
GetCurrentTimestamp PROC
    LOCAL st:SYSTEMTIME
    
    push rbx
    push rsi
    push rdi
    sub rsp, 80
    
    ; Get local time
    lea rcx, st
    call GetLocalTime
    
    ; Format timestamp: HH:MM:SS
    lea rcx, timestamp_buf
    lea rdx, szLogFormat
    movzx r8, st.wHour
    movzx r9, st.wMinute
    movzx rax, st.wSecond
    mov [rsp + 32], rax
    call wsprintfA
    
    lea rax, timestamp_buf
    
    add rsp, 80
    pop rdi
    pop rsi
    pop rbx
    ret
GetCurrentTimestamp ENDP

;==============================================================================
; INTERNAL: WriteLogMessage(level: ecx, message: rdx) -> eax
;==============================================================================
WriteLogMessage PROC
    LOCAL st:SYSTEMTIME
    LOCAL bytesWritten:QWORD
    LOCAL levelStr:QWORD
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 256
    
    mov r12d, ecx        ; level
    mov r13, rdx         ; message
    
    ; Check if logging enabled
    cmp [g_LogContext.log_enabled], 0
    je write_log_done
    
    ; Get level string
    cmp r12d, LOG_LEVEL_DEBUG
    je set_debug
    cmp r12d, LOG_LEVEL_INFO
    je set_info
    cmp r12d, LOG_LEVEL_WARNING
    je set_warning
    cmp r12d, LOG_LEVEL_ERROR
    je set_error
    cmp r12d, LOG_LEVEL_SUCCESS
    je set_success
    lea rax, szLevelInfo
    jmp level_set
    
set_debug:
    lea rax, szLevelDebug
    jmp level_set
set_info:
    lea rax, szLevelInfo
    jmp level_set
set_warning:
    lea rax, szLevelWarning
    jmp level_set
set_error:
    lea rax, szLevelError
    jmp level_set
set_success:
    lea rax, szLevelSuccess
    
level_set:
    mov levelStr, rax
    
    ; Get timestamp using GetLocalTime
    lea rcx, [rsp + 100] ; SYSTEMTIME structure
    call GetLocalTime
    lea rbx, [rsp + 100]
    
    ; Format log message
    lea rcx, log_buffer
    lea rdx, szLogFormat
    movzx r8, WORD PTR [rbx + 8]   ; wHour
    movzx r9, WORD PTR [rbx + 10]  ; wMinute
    movzx rax, WORD PTR [rbx + 12] ; wSecond
    mov [rsp + 32], rax
    mov rax, levelStr
    mov [rsp + 40], rax
    mov [rsp + 48], r13
    call wsprintfA
    
    ; Get message length
    lea rcx, log_buffer
    call lstrlenA
    mov rsi, rax
    
    ; Write to console if enabled
    test [g_LogContext.log_targets], LOG_TARGET_CONSOLE
    jz skip_console
    
    ; Write to stdout
    mov rcx, hStdOut
    lea rdx, log_buffer
    mov r8, rsi
    lea r9, [rsp + 200]  ; bytesWritten
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    
skip_console:
    ; Write to file if enabled
    test [g_LogContext.log_targets], LOG_TARGET_FILE
    jz skip_file
    
    cmp [g_LogContext.hLogFile], 0
    je skip_file
    
    ; Check file size and rotate if needed
    mov rcx, [g_LogContext.hLogFile]
    xor rdx, rdx
    call GetFileSize
    cmp rax, MAX_LOG_FILE_SIZE
    jb write_file
    
    ; Rotate log file: Rename current to .old and create new
    mov rcx, [g_LogContext.hLogFile]
    call CloseHandle
    
    lea rcx, szLogFileName
    lea rdx, szLogFileNameOld
    call MoveFileExA ; Move with REPLACE_EXISTING
    
    ; Re-open log file
    lea rcx, szLogFileName
    mov edx, GENERIC_WRITE
    mov r8d, FILE_SHARE_READ
    xor r9, r9
    mov qword ptr [rsp + 32], CREATE_ALWAYS
    mov qword ptr [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 48], 0
    call CreateFileA
    mov [g_LogContext.hLogFile], rax
    
write_file:
    mov rcx, [g_LogContext.hLogFile]
    lea rdx, log_buffer
    mov r8, rsi
    lea r9, [rsp + 200]  ; bytesWritten
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    
skip_file:
    ; Write to UI if enabled
    test [g_LogContext.log_targets], LOG_TARGET_UI
    jz write_log_done
    
    lea rcx, log_buffer
    call ui_add_chat_message
    
write_log_done:
    mov eax, 1
    add rsp, 256
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
WriteLogMessage ENDP

;==============================================================================
; PUBLIC: LogDebug(message: rcx)
;==============================================================================
PUBLIC LogDebug
ALIGN 16
LogDebug PROC
    mov edx, LOG_LEVEL_DEBUG
    jmp LogMessageCommon
LogDebug ENDP

;==============================================================================
; PUBLIC: LogInfo(message: rcx)
;==============================================================================
PUBLIC LogInfo
ALIGN 16
LogInfo PROC
    mov edx, LOG_LEVEL_INFO
    jmp LogMessageCommon
LogInfo ENDP

;==============================================================================
; PUBLIC: LogWarning(message: rcx)
;==============================================================================
PUBLIC LogWarning
ALIGN 16
LogWarning PROC
    mov edx, LOG_LEVEL_WARNING
    jmp LogMessageCommon
LogWarning ENDP

;==============================================================================
; PUBLIC: LogError(message: rcx)
;==============================================================================
PUBLIC LogError
ALIGN 16
LogError PROC
    mov edx, LOG_LEVEL_ERROR
    jmp LogMessageCommon
LogError ENDP

;==============================================================================
; PUBLIC: LogSuccess(message: rcx)
;==============================================================================
PUBLIC LogSuccess
ALIGN 16
LogSuccess PROC
    mov edx, LOG_LEVEL_SUCCESS
    jmp LogMessageCommon
LogSuccess ENDP

;==============================================================================
; COMMON: LogMessageCommon(message: rcx, level: edx)
;==============================================================================
LogMessageCommon PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx        ; message
    mov ecx, edx        ; level
    
    ; Check log level threshold
    cmp ecx, [g_LogContext.log_level]
    jb log_common_done
    
    ; Write log message
    mov rdx, rbx
    call WriteLogMessage
    
log_common_done:
    add rsp, 32
    pop rbx
    ret
LogMessageCommon ENDP

;==============================================================================
; PUBLIC: LogShutdown()
;==============================================================================
PUBLIC LogShutdown
ALIGN 16
LogShutdown PROC
    push rbx
    sub rsp, 32
    
    ; Close log file
    cmp [g_LogContext.hLogFile], 0
    je shutdown_done
    
    mov rcx, [g_LogContext.hLogFile]
    call CloseHandle
    mov [g_LogContext.hLogFile], 0
    
shutdown_done:
    add rsp, 32
    pop rbx
    ret
LogShutdown ENDP

END

