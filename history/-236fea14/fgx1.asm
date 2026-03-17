; ============================================================================
; LOGGING PHASE 3 - Enhanced logging system with file rotation and levels
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ==================== CONSTANTS ====================
LOG_LEVEL_DEBUG        equ 0
LOG_LEVEL_INFO         equ 1
LOG_LEVEL_WARNING      equ 2
LOG_LEVEL_ERROR        equ 3
LOG_LEVEL_CRITICAL     equ 4

MAX_LOG_SIZE           equ 1048576    ; 1MB
MAX_LOG_ENTRIES        equ 10000
LOG_BUFFER_SIZE        equ 65536      ; 64KB

; ==================== STRUCTURES ====================
LOG_ENTRY struct
    dwLevel            dd ?
    dwTimestamp        dd ?
    szMessage          db 256 dup(?)
LOG_ENTRY ends

LOG_STATE struct
    hLogFile           dd ?
    dwCurrentLevel     dd ?
    dwEntryCount       dd ?
    dwFileSize         dd ?
    bInitialized       dd ?
    szLogPath          db MAX_PATH dup(?)
    szBuffer           db LOG_BUFFER_SIZE dup(?)
    dwBufferPos        dd ?
LOG_STATE ends

; ==================== DATA ====================
.data
    g_LogState         LOG_STATE <>
    
    szDefaultLogPath   db "C:\RawrXD\logs\ide.log", 0
    szBackupLogPath    db "C:\RawrXD\logs\ide_backup.log", 0
    szLogDir           db "C:\RawrXD\logs", 0
    
    szLevelNames       dd offset szDebug, offset szInfo, offset szWarning, offset szError, offset szCritical
    szDebug            db "DEBUG", 0
    szInfo             db "INFO", 0
    szWarning          db "WARNING", 0
    szError            db "ERROR", 0
    szCritical         db "CRITICAL", 0
    
    szLogFormat        db "[%s] %08X: %s", 13, 10, 0
    szTimestampFormat  db "%02d:%02d:%02d", 0

; Prototypes
CreateDirectoryA PROTO :DWORD,:DWORD
CreateFileA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
WriteFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
CloseHandle PROTO :DWORD
GetFileSize PROTO :DWORD,:DWORD
MoveFileA PROTO :DWORD,:DWORD
DeleteFileA PROTO :DWORD
GetTickCount PROTO
wsprintfA PROTO C :VARARG
lstrcatA PROTO :DWORD,:DWORD
lstrlenA PROTO :DWORD

; ==================== CODE ====================
.code

; ============================================================================
; LogSystem_Initialize - Initialize logging system
; ============================================================================
public LogSystem_Initialize
LogSystem_Initialize proc
    LOCAL dwAttributes:DWORD
    
    ; Check if already initialized
    cmp g_LogState.bInitialized, 1
    je @@success
    
    ; Create log directory
    invoke GetFileAttributesA, addr szLogDir
    cmp eax, INVALID_FILE_ATTRIBUTES
    jne @@dir_exists
    
    invoke CreateDirectoryA, addr szLogDir, NULL
    
    @@dir_exists:
    ; Set default log path
    mov esi, offset szDefaultLogPath
    lea edi, g_LogState.szLogPath
    @@copy_path:
        mov al, BYTE PTR [esi]
        mov BYTE PTR [edi], al
        test al, al
        jz @@path_done
        inc esi
        inc edi
        jmp @@copy_path
    @@path_done:
    
    ; Initialize state
    mov g_LogState.dwCurrentLevel, LOG_LEVEL_INFO
    mov g_LogState.dwEntryCount, 0
    mov g_LogState.dwFileSize, 0
    mov g_LogState.dwBufferPos, 0
    mov g_LogState.bInitialized, 1
    
    ; Open log file
    invoke CreateFileA, addr g_LogState.szLogPath, GENERIC_WRITE, FILE_SHARE_READ, 0, 
           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @@fail
    
    mov g_LogState.hLogFile, eax
    
    ; Get current file size
    invoke GetFileSize, eax, NULL
    mov g_LogState.dwFileSize, eax
    
    ; Log startup
    invoke LogMessage, LOG_LEVEL_INFO, addr szStartupMsg
    
    @@success:
    mov eax, TRUE
    ret
    
    @@fail:
    mov g_LogState.bInitialized, 0
    xor eax, eax
    ret
    
szStartupMsg db "RawrXD IDE logging system initialized", 0
LogSystem_Initialize endp

; ============================================================================
; LogMessage - Write a log entry
; ============================================================================
public LogMessage
LogMessage proc dwLevel:DWORD, pszMessage:DWORD
    LOCAL dwTimestamp:DWORD
    LOCAL szFormattedMsg[512]:BYTE
    LOCAL dwBytesToWrite:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL pLevelName:DWORD
    
    ; Check if initialized and level
    cmp g_LogState.bInitialized, 1
    jne @@exit
    
    mov eax, dwLevel
    cmp eax, g_LogState.dwCurrentLevel
    jl @@exit
    
    ; Get timestamp
    invoke GetTickCount
    mov dwTimestamp, eax
    
    ; Get level name
    mov eax, dwLevel
    cmp eax, 4
    jg @@use_error
    mov ecx, szLevelNames
    mov eax, [ecx + eax*4]
    mov pLevelName, eax
    jmp @@format_msg
    
    @@use_error:
    mov pLevelName, offset szError
    
    @@format_msg:
    ; Format message
    invoke wsprintfA, addr szFormattedMsg, addr szLogFormat, 
           pLevelName, dwTimestamp, pszMessage
    
    ; Get message length
    invoke lstrlenA, addr szFormattedMsg
    mov dwBytesToWrite, eax
    
    ; Check file size for rotation
    mov eax, g_LogState.dwFileSize
    add eax, dwBytesToWrite
    cmp eax, MAX_LOG_SIZE
    jl @@write_direct
    
    ; Rotate log file
    call @@rotate_log
    
    @@write_direct:
    ; Write to file
    invoke WriteFile, g_LogState.hLogFile, addr szFormattedMsg, 
           dwBytesToWrite, addr dwBytesWritten, NULL
    
    ; Update file size
    mov eax, dwBytesToWrite
    add g_LogState.dwFileSize, eax
    inc g_LogState.dwEntryCount
    
    @@exit:
    mov eax, TRUE
    ret
    
    @@rotate_log:
    ; Close current file
    invoke CloseHandle, g_LogState.hLogFile
    
    ; Move current to backup
    invoke DeleteFileA, addr szBackupLogPath
    invoke MoveFileA, addr g_LogState.szLogPath, addr szBackupLogPath
    
    ; Create new file
    invoke CreateFileA, addr g_LogState.szLogPath, GENERIC_WRITE, FILE_SHARE_READ, 0,
           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    mov g_LogState.hLogFile, eax
    mov g_LogState.dwFileSize, 0
    
    ret
LogMessage endp

; ============================================================================
; SetLogLevel - Change logging level
; ============================================================================
public SetLogLevel
SetLogLevel proc dwLevel:DWORD
    mov eax, dwLevel
    cmp eax, 4
    jg @@exit
    
    mov g_LogState.dwCurrentLevel, eax
    mov eax, TRUE
    ret
    
    @@exit:
    xor eax, eax
    ret
SetLogLevel endp

; ============================================================================
; FlushLogs - Force write buffered logs
; ============================================================================
public FlushLogs
FlushLogs proc
    cmp g_LogState.bInitialized, 1
    jne @@exit
    
    cmp g_LogState.hLogFile, 0
    je @@exit
    
    invoke FlushFileBuffers, g_LogState.hLogFile
    
    @@exit:
    mov eax, TRUE
    ret
FlushLogs endp

; ============================================================================
; ShutdownLogging - Clean shutdown
; ============================================================================
public ShutdownLogging
ShutdownLogging proc
    cmp g_LogState.bInitialized, 1
    jne @@exit
    
    ; Log shutdown
    invoke LogMessage, LOG_LEVEL_INFO, addr szShutdownMsg
    
    ; Flush and close
    invoke FlushLogs
    invoke CloseHandle, g_LogState.hLogFile
    
    ; Reset state
    mov g_LogState.bInitialized, 0
    mov g_LogState.hLogFile, 0
    
    @@exit:
    mov eax, TRUE
    ret
    
szShutdownMsg db "RawrXD IDE logging system shutdown", 0
ShutdownLogging endp

; ============================================================================
; GetLogStats - Get logging statistics
; ============================================================================
public GetLogStats
GetLogStats proc pEntryCount:DWORD, pFileSize:DWORD
    cmp g_LogState.bInitialized, 1
    jne @@fail
    
    mov eax, pEntryCount
    test eax, eax
    jz @@skip_count
    mov ecx, g_LogState.dwEntryCount
    mov [eax], ecx
    
    @@skip_count:
    mov eax, pFileSize
    test eax, eax
    jz @@success
    mov ecx, g_LogState.dwFileSize
    mov [eax], ecx
    
    @@success:
    mov eax, TRUE
    ret
    
    @@fail:
    xor eax, eax
    ret
GetLogStats endp

end