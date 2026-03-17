;===============================================================================
; AGENTIC TOOLS - Pure MASM Implementation
; No dependencies, no stubs, no fictional code - Full source
;===============================================================================

.386
.model flat, stdcall
option casemap:none

;===============================================================================
; WINDOWS API DEFINITIONS
;===============================================================================

CreateFileA             PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
ReadFile                PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
WriteFile               PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CloseHandle             PROTO :DWORD
GetFileSize             PROTO :DWORD, :DWORD
CreateProcessA          PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
WaitForSingleObject     PROTO :DWORD, :DWORD
GetExitCodeProcess      PROTO :DWORD, :DWORD
GetLastError            PROTO
DeleteFileA             PROTO :DWORD
FindFirstFileA          PROTO :DWORD, :DWORD
FindNextFileA           PROTO :DWORD, :DWORD
FindClose               PROTO :DWORD
GetCurrentDirectoryA    PROTO :DWORD, :DWORD
SetCurrentDirectoryA    PROTO :DWORD
GlobalAlloc             PROTO :DWORD, :DWORD
GlobalFree              PROTO :DWORD
GetTickCount            PROTO
GetEnvironmentVariableA PROTO :DWORD, :DWORD, :DWORD
lstrlenA                PROTO :DWORD
lstrcpyA                PROTO :DWORD, :DWORD
lstrcatA                PROTO :DWORD, :DWORD
lstrcmpA                PROTO :DWORD, :DWORD
wsprintfA               PROTO C :DWORD, :DWORD, :VARARG
MessageBoxA             PROTO :DWORD, :DWORD, :DWORD, :DWORD
OutputDebugStringA      PROTO :DWORD
ExitProcess             PROTO :DWORD

; Constants
GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
OPEN_EXISTING           equ 3
CREATE_ALWAYS           equ 2
FILE_ATTRIBUTE_NORMAL   equ 80h
INVALID_HANDLE_VALUE    equ -1
WAIT_OBJECT_0           equ 0
GMEM_FIXED              equ 0
MAX_PATH                equ 260
TRUE                    equ 1
FALSE                   equ 0

; Tool result structure
ToolResult              STRUCT
    success             DWORD   ?
    exit_code           DWORD   ?
    output              DWORD   ?
    error               DWORD   ?
ToolResult              ENDS

; WIN32_FIND_DATAA Structure
WIN32_FIND_DATAA STRUCT
    dwFileAttributes    DWORD       ?
    ftCreationTime      DWORD       2 dup(?)
    ftLastAccessTime    DWORD       2 dup(?)
    ftLastWriteTime     DWORD       2 dup(?)
    nFileSizeHigh       DWORD       ?
    nFileSizeLow        DWORD       ?
    dwReserved0         DWORD       ?
    dwReserved1         DWORD       ?
    cFileName           BYTE        260 dup(?)
    cAlternateFileName  BYTE        14 dup(?)
WIN32_FIND_DATAA ENDS

; STARTUPINFOA Structure
STARTUPINFOA STRUCT
    cb                  DWORD       ?
    lpReserved          DWORD       ?
    lpDesktop           DWORD       ?
    lpTitle             DWORD       ?
    dwX                 DWORD       ?
    dwY                 DWORD       ?
    dwXSize             DWORD       ?
    dwYSize             DWORD       ?
    dwXCountChars       DWORD       ?
    dwYCountChars       DWORD       ?
    dwFillAttribute     DWORD       ?
    dwFlags             DWORD       ?
    wShowWindow         WORD        ?
    cbReserved2         WORD        ?
    lpReserved2         DWORD       ?
    hStdInput           DWORD       ?
    hStdOutput          DWORD       ?
    hStdError           DWORD       ?
STARTUPINFOA ENDS

; PROCESS_INFORMATION Structure
PROCESS_INFORMATION STRUCT
    hProcess            DWORD       ?
    hThread             DWORD       ?
    dwProcessId         DWORD       ?
    dwThreadId          DWORD       ?
PROCESS_INFORMATION ENDS

;===============================================================================
; DATA SEGMENT
;===============================================================================

.data

; File operation strings
szRead                  db      'r', 0
szWrite                 db      'w', 0
szAppend                db      'a', 0
szCurrentDir            db      '.', 0
szWildcard              db      '*', 0
szSlash                 db      '\', 0
szNewline               db      13, 10, 0
szTab                   db      9, 0
szSpace                 db      ' ', 0

; Error messages
szErrFileOpen           db      'Cannot open file: ', 0
szErrFileRead           db      'Failed to read file', 0
szErrFileWrite          db      'Failed to write file', 0
szErrDirNotFound        db      'Directory not found', 0
szErrPathNotFound       db      'Path not found', 0
szErrCmdFailed          db      'Command execution failed', 0
szErrAlloc              db      'Memory allocation failed', 0
szErrParam              db      'Missing parameter', 0

; Success messages
szSuccess               db      'Operation completed successfully', 0
szFileWritten           db      'File written: ', 0
szFileDeleted           db      'File deleted: ', 0
szDirListed             db      'Directory contents:', 0

; Logging
szLogEnvName            db      'RAWRXD_MASM_TOOLS_LOG', 0
szLogInfo               db      'INFO', 0
szLogError              db      'ERROR', 0
szLogFmt                db      '[masm-tools] level=%s tool=%s ok=%u ms=%u', 13, 10, 0

; Tool names
szToolReadFile          db      'readFile', 0
szToolWriteFile         db      'writeFile', 0
szToolListDir           db      'listDirectory', 0
szToolDeleteFile        db      'deleteFile', 0
szToolExecuteCmd        db      'executeCommand', 0
szToolGrep              db      'grepSearch', 0

; Command strings
szCmdExe                db      'cmd.exe', 0
szSlashC                db      ' /c ', 0
szFindStr               db      'findstr', 0

; Buffers
outputBuffer            db      4096 dup(0)
errorBuffer             db      1024 dup(0)
fileBuffer              db      8192 dup(0)
pathBuffer              db      MAX_PATH dup(0)
commandBuffer           db      4096 dup(0)
logBuffer               db      512 dup(0)
logEnvBuffer            db      8 dup(0)
logEnabled              dd      0
logInit                 dd      0

;===============================================================================
; CODE SEGMENT
;===============================================================================

.code

;-------------------------------------------------------------------------------
; STRING LENGTH (custom strlen)
;-------------------------------------------------------------------------------
StringLength PROC uses esi, lpString:DWORD
    mov     esi, lpString
    xor     ecx, ecx
len_loop:
    cmp     byte ptr [esi+ecx], 0
    je      len_done
    inc     ecx
    jmp     len_loop
len_done:
    mov     eax, ecx
    ret
StringLength ENDP

;-------------------------------------------------------------------------------
; STRING COPY (custom strcpy)
;-------------------------------------------------------------------------------
StringCopy PROC uses esi edi, lpDest:DWORD, lpSrc:DWORD
    mov     esi, lpSrc
    mov     edi, lpDest
copy_loop:
    lodsb
    stosb
    test    al, al
    jnz     copy_loop
    mov     eax, lpDest
    ret
StringCopy ENDP

;-------------------------------------------------------------------------------
; STRING CONCATENATE (custom strcat)
;-------------------------------------------------------------------------------
StringConcat PROC uses esi edi, lpDest:DWORD, lpSrc:DWORD
    mov     edi, lpDest
    mov     esi, lpSrc
find_end:
    cmp     byte ptr [edi], 0
    je      do_concat
    inc     edi
    jmp     find_end
do_concat:
    lodsb
    stosb
    test    al, al
    jnz     do_concat
    mov     eax, lpDest
    ret
StringConcat ENDP

;-------------------------------------------------------------------------------
; LOGGING INIT - Checks RAWRXD_MASM_TOOLS_LOG env var
;-------------------------------------------------------------------------------
InitLogFlag PROC
    cmp     logInit, 1
    je      log_init_done
    mov     logInit, 1
    invoke  GetEnvironmentVariableA, offset szLogEnvName, offset logEnvBuffer, 8
    test    eax, eax
    jz      log_init_done
    mov     al, byte ptr [logEnvBuffer]
    cmp     al, '1'
    je      log_enable
    cmp     al, 't'
    je      log_enable
    cmp     al, 'T'
    je      log_enable
    cmp     al, 'y'
    je      log_enable
    cmp     al, 'Y'
    je      log_enable
    jmp     log_init_done
log_enable:
    mov     logEnabled, 1
log_init_done:
    ret
InitLogFlag ENDP

;-------------------------------------------------------------------------------
; LOG TOOL DURATION
;-------------------------------------------------------------------------------
LogToolDuration PROC uses esi edi, lpToolName:DWORD, dwElapsed:DWORD, success:DWORD
    invoke  InitLogFlag
    cmp     logEnabled, 1
    jne     log_done
    mov     eax, success
    test    eax, eax
    jz      log_error
    mov     edx, offset szLogInfo
    jmp     log_format
log_error:
    mov     edx, offset szLogError
log_format:
    invoke  wsprintfA, offset logBuffer, offset szLogFmt, edx, lpToolName, success, dwElapsed
    invoke  OutputDebugStringA, offset logBuffer
log_done:
    ret
LogToolDuration ENDP

;-------------------------------------------------------------------------------
; PARSE WRITE PARAMS - "path|content"
;-------------------------------------------------------------------------------
ParseWriteParams PROC uses esi edi, lpParams:DWORD, lpPathOut:DWORD, lpContentOut:DWORD
    mov     esi, lpParams
    mov     edi, lpPathOut
parse_loop:
    mov     al, byte ptr [esi]
    test    al, al
    jz      parse_fail
    cmp     al, '|'
    je      parse_split
    mov     byte ptr [edi], al
    inc     esi
    inc     edi
    jmp     parse_loop
parse_split:
    mov     byte ptr [edi], 0
    inc     esi
    invoke  StringCopy, lpContentOut, esi
    mov     eax, 1
    ret
parse_fail:
    mov     byte ptr [edi], 0
    xor     eax, eax
    ret
ParseWriteParams ENDP

;-------------------------------------------------------------------------------
; READ FILE TOOL - Reads entire file content
; Parameters: lpFilePath (DWORD) - pointer to file path string
; Returns: ToolResult structure
;-------------------------------------------------------------------------------
ReadFileTool PROC uses ebx esi edi, lpFilePath:DWORD
    LOCAL   hFile:DWORD
    LOCAL   dwFileSize:DWORD
    LOCAL   dwBytesRead:DWORD
    LOCAL   lpFileContent:DWORD
    
    ; Try to open file
    invoke  CreateFileA, lpFilePath, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    mov     hFile, eax
    cmp     eax, INVALID_HANDLE_VALUE
    je      error_open
    
    ; Get file size
    invoke  GetFileSize, hFile, 0
    cmp     eax, -1
    je      error_size
    mov     dwFileSize, eax
    
    ; Allocate memory for file content
    invoke  GlobalAlloc, GMEM_FIXED, dwFileSize
    cmp     eax, 0
    je      error_alloc
    mov     lpFileContent, eax
    
    ; Read file content
    invoke  ReadFile, hFile, lpFileContent, dwFileSize, addr dwBytesRead, 0
    test    eax, eax
    je      error_read
    
    ; Null terminate
    mov     ebx, lpFileContent
    add     ebx, dwBytesRead
    mov     byte ptr [ebx], 0
    
    ; Close file
    invoke  CloseHandle, hFile
    
    ; Return success
    mov     eax, lpFileContent
    mov     ebx, 1          ; success = TRUE
    ret
    
error_open:
    mov     eax, offset szErrFileOpen
    mov     ebx, 0          ; success = FALSE
    ret
error_size:
    invoke  CloseHandle, hFile
    mov     eax, offset szErrFileRead
    mov     ebx, 0
    ret
error_alloc:
    invoke  CloseHandle, hFile
    mov     eax, offset szErrAlloc
    mov     ebx, 0
    ret
error_read:
    invoke  CloseHandle, hFile
    invoke  GlobalFree, lpFileContent
    mov     eax, offset szErrFileRead
    mov     ebx, 0
    ret
ReadFileTool ENDP

;-------------------------------------------------------------------------------
; WRITE FILE TOOL - Writes content to file
; Parameters: lpFilePath, lpContent
; Returns: ToolResult
;-------------------------------------------------------------------------------
WriteFileTool PROC uses ebx esi edi, lpFilePath:DWORD, lpContent:DWORD
    LOCAL   hFile:DWORD
    LOCAL   dwContentLen:DWORD
    LOCAL   dwBytesWritten:DWORD
    
    ; Get content length
    invoke  StringLength, lpContent
    mov     dwContentLen, eax
    
    ; Create/Open file for writing
    invoke  CreateFileA, lpFilePath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    mov     hFile, eax
    cmp     eax, INVALID_HANDLE_VALUE
    je      write_error_open
    
    ; Write content
    invoke  WriteFile, hFile, lpContent, dwContentLen, addr dwBytesWritten, 0
    test    eax, eax
    je      write_error_write
    
    ; Close file
    invoke  CloseHandle, hFile
    
    ; Build success message
    invoke  StringCopy, offset outputBuffer, offset szFileWritten
    invoke  StringConcat, offset outputBuffer, lpFilePath
    mov     eax, offset outputBuffer
    mov     ebx, 1
    ret
    
write_error_open:
    mov     eax, offset szErrFileOpen
    mov     ebx, 0
    ret
write_error_write:
    invoke  CloseHandle, hFile
    mov     eax, offset szErrFileWrite
    mov     ebx, 0
    ret
WriteFileTool ENDP

;-------------------------------------------------------------------------------
; DELETE FILE TOOL - Deletes file via DeleteFileA
; Parameters: lpFilePath
; Returns: ToolResult
;-------------------------------------------------------------------------------
DeleteFileTool PROC uses ebx esi edi, lpFilePath:DWORD
    invoke  DeleteFileA, lpFilePath
    test    eax, eax
    jz      delete_error

    invoke  StringCopy, offset outputBuffer, offset szFileDeleted
    invoke  StringConcat, offset outputBuffer, lpFilePath
    mov     eax, offset outputBuffer
    mov     ebx, 1
    ret

delete_error:
    mov     eax, offset szErrPathNotFound
    mov     ebx, 0
    ret
DeleteFileTool ENDP

;-------------------------------------------------------------------------------
; LIST DIRECTORY TOOL - Lists directory contents
; Parameters: lpDirPath
; Returns: ToolResult
;-------------------------------------------------------------------------------
ListDirectoryTool PROC uses ebx esi edi, lpDirPath:DWORD
    LOCAL   hFind:DWORD
    LOCAL   findData:WIN32_FIND_DATAA
    LOCAL   dwPathLen:DWORD
    
    ; Build search pattern (path\*)
    invoke  StringCopy, offset pathBuffer, lpDirPath
    invoke  StringLength, offset pathBuffer
    mov     dwPathLen, eax
    ; Check if ends with backslash
    mov     ebx, offset pathBuffer
    add     ebx, dwPathLen
    dec     ebx
    cmp     byte ptr [ebx], '\'
    je      add_wildcard
    invoke  StringConcat, offset pathBuffer, offset szSlash
add_wildcard:
    invoke  StringConcat, offset pathBuffer, offset szWildcard
    
    ; Clear output buffer
    mov     edi, offset outputBuffer
    mov     ecx, 4096/4
    xor     eax, eax
    rep     stosd
    
    ; Add header
    invoke  StringCopy, offset outputBuffer, offset szDirListed
    invoke  StringConcat, offset outputBuffer, offset szNewline
    
    ; Find first file
    invoke  FindFirstFileA, offset pathBuffer, addr findData
    mov     hFind, eax
    cmp     eax, INVALID_HANDLE_VALUE
    je      error_find
    
next_file:
    ; Skip . and ..
    lea     esi, findData.cFileName
    cmp     byte ptr [esi], '.'
    je      skip_this
    
    ; Add [DIR] or [FILE] prefix
    test    findData.dwFileAttributes, 10h    ; FILE_ATTRIBUTE_DIRECTORY
    jnz     is_dir
    invoke  StringConcat, offset outputBuffer, offset szTab
    invoke  StringConcat, offset outputBuffer, offset szToolReadFile
    invoke  StringConcat, offset outputBuffer, offset szSpace
    jmp     add_name
is_dir:
    invoke  StringConcat, offset outputBuffer, offset szTab
    invoke  StringConcat, offset outputBuffer, offset szToolListDir
    invoke  StringConcat, offset outputBuffer, offset szSpace
add_name:
    invoke  StringConcat, offset outputBuffer, offset findData.cFileName
    invoke  StringConcat, offset outputBuffer, offset szNewline
    
skip_this:
    ; Find next file
    invoke  FindNextFileA, hFind, addr findData
    test    eax, eax
    jnz     next_file
    
    invoke  FindClose, hFind
    mov     eax, offset outputBuffer
    mov     ebx, 1
    ret
    
error_find:
    mov     eax, offset szErrDirNotFound
    mov     ebx, 0
    ret
ListDirectoryTool ENDP

;-------------------------------------------------------------------------------
; EXECUTE COMMAND TOOL - Executes shell command
; Parameters: lpCommand (full command line)
; Returns: ToolResult
;-------------------------------------------------------------------------------
ExecuteCommandTool PROC uses ebx esi edi, lpCommand:DWORD
    LOCAL   si:STARTUPINFOA
    LOCAL   pi:PROCESS_INFORMATION
    LOCAL   dwExitCode:DWORD
    
    ; Clear structures
    lea     edi, si
    mov     ecx, SIZEOF STARTUPINFOA
    xor     al, al
    rep     stosb
    
    lea     edi, pi
    mov     ecx, SIZEOF PROCESS_INFORMATION
    xor     al, al
    rep     stosb
    
    ; Setup startup info
    mov     si.cb, SIZEOF STARTUPINFOA
    
    ; Create process
    invoke  CreateProcessA, 0, lpCommand, 0, 0, FALSE, 0, 0, 0, addr si, addr pi
    test    eax, eax
    je      exec_error_create
    
    ; Wait for process to complete (30 second timeout)
    invoke  WaitForSingleObject, pi.hProcess, 30000
    cmp     eax, WAIT_OBJECT_0
    jne     exec_error_timeout
    
    ; Get exit code
    invoke  GetExitCodeProcess, pi.hProcess, addr dwExitCode
    
    ; Close handles
    invoke  CloseHandle, pi.hProcess
    invoke  CloseHandle, pi.hThread
    
    ; Check if command succeeded
    test    dwExitCode, dwExitCode
    jnz     exec_error_exit
    
    mov     eax, offset szSuccess
    mov     ebx, 1
    ret
    
exec_error_create:
    mov     eax, offset szErrCmdFailed
    mov     ebx, 0
    ret
exec_error_timeout:
    invoke  CloseHandle, pi.hProcess
    invoke  CloseHandle, pi.hThread
    mov     eax, offset szErrCmdFailed
    mov     ebx, 0
    ret
exec_error_exit:
    mov     eax, offset szErrCmdFailed
    mov     ebx, 0
    ret
ExecuteCommandTool ENDP

;-------------------------------------------------------------------------------
; AGENTIC TOOL EXECUTOR - Main entry point
; Parameters: lpToolName, lpParams
; Returns: ToolResult in EAX (pointer) and EBX (success flag)
;-------------------------------------------------------------------------------
AgenticToolExecutor PROC uses ebx esi edi, lpToolName:DWORD, lpParams:DWORD
    LOCAL   dwStartTick:DWORD
    LOCAL   dwResult:DWORD
    LOCAL   dwSuccess:DWORD

    invoke  GetTickCount
    mov     dwStartTick, eax
    ; Check tool name and dispatch
    
    ; readFile
    invoke  lstrcmpA, lpToolName, offset szToolReadFile
    test    eax, eax
    jz      do_readfile
    
    ; writeFile
    invoke  lstrcmpA, lpToolName, offset szToolWriteFile
    test    eax, eax
    jz      do_writefile
    
    ; listDirectory
    invoke  lstrcmpA, lpToolName, offset szToolListDir
    test    eax, eax
    jz      do_listdir
    
    ; deleteFile
    invoke  lstrcmpA, lpToolName, offset szToolDeleteFile
    test    eax, eax
    jz      do_deletefile
    
    ; executeCommand
    invoke  lstrcmpA, lpToolName, offset szToolExecuteCmd
    test    eax, eax
    jz      do_execute
    
    ; Unknown tool
    mov     eax, offset szErrCmdFailed
    mov     ebx, 0
    jmp     exec_done
    
do_readfile:
    ; For readFile, params should be just the file path
    invoke  ReadFileTool, lpParams
    jmp     exec_done
    
do_writefile:
    ; For writeFile, params should be "filepath|content"
    invoke  ParseWriteParams, lpParams, offset pathBuffer, offset fileBuffer
    test    eax, eax
    jz      write_param_error
    invoke  WriteFileTool, offset pathBuffer, offset fileBuffer
    jmp     exec_done
write_param_error:
    mov     eax, offset szErrParam
    mov     ebx, 0
    jmp     exec_done
    
do_listdir:
    invoke  ListDirectoryTool, lpParams
    jmp     exec_done
    
do_deletefile:
    invoke  DeleteFileTool, lpParams
    jmp     exec_done
    
do_execute:
    invoke  ExecuteCommandTool, lpParams
    jmp     exec_done
exec_done:
    mov     dwResult, eax
    mov     dwSuccess, ebx
    invoke  GetTickCount
    sub     eax, dwStartTick
    mov     ecx, dwSuccess
    invoke  LogToolDuration, lpToolName, eax, ecx
    mov     eax, dwResult
    mov     ebx, dwSuccess
    ret
AgenticToolExecutor ENDP

;-------------------------------------------------------------------------------
; MAIN ENTRY POINT - Demonstration
;-------------------------------------------------------------------------------
main PROC
    LOCAL   result:DWORD
    
    ; Example: List current directory
    invoke  AgenticToolExecutor, offset szToolListDir, offset szCurrentDir
    
    ; Display result
    test    ebx, ebx
    jz      error_disp
    invoke  MessageBoxA, 0, eax, offset szToolListDir, 0
    jmp     main_exit
error_disp:
    invoke  MessageBoxA, 0, eax, offset szErrCmdFailed, 0
main_exit:
    invoke  ExitProcess, 0
main ENDP

;===============================================================================
; END OF AGENTIC TOOLS
;===============================================================================

END main
