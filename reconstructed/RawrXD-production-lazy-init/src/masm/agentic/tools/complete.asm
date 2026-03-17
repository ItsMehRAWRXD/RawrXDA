;===============================================================================
; AGENTIC TOOLS - COMPLETE PURE MASM IMPLEMENTATION
; Enhanced / No Dependencies / No Stubs / Full Source / 1-Liner Style
;===============================================================================

.386
.model flat, stdcall
option casemap:none

;===============================================================================
; WINDOWS API EXTERNALS (Pure MASM - No Includes)
;===============================================================================

extern  CreateFileA@28           :PROC
extern  ReadFile@20              :PROC
extern  WriteFile@20             :PROC
extern  CloseHandle@4            :PROC
extern  GetFileSize@8            :PROC
extern  GetLastError@0           :PROC
extern  FindFirstFileA@8         :PROC
extern  FindNextFileA@8          :PROC
extern  FindClose@4              :PROC
extern  GetCurrentDirectoryA@8   :PROC
extern  SetCurrentDirectoryA@4   :PROC
extern  CreateProcessA@40        :PROC
extern  WaitForSingleObject@8    :PROC
extern  GetExitCodeProcess@8     :PROC
extern  GlobalAlloc@8            :PROC
extern  GlobalFree@4             :PROC
extern  DeleteFileA@4            :PROC
extern  MessageBoxA@16           :PROC
extern  ExitProcess@4            :PROC
extern  lstrlenA@4               :PROC
extern  lstrcpyA@8               :PROC
extern  lstrcatA@8               :PROC
extern  lstrcmpA@8               :PROC
extern  wsprintfA@16             :PROC

;===============================================================================
; CONSTANTS
;===============================================================================

GENERIC_READ        equ 80000000h
GENERIC_WRITE       equ 40000000h
OPEN_EXISTING       equ 3
CREATE_ALWAYS       equ 2
FILE_ATTRIBUTE_NORM equ 80h
INVALID_HANDLE_VAL  equ -1
GMEM_FIXED          equ 0
MAX_PATH            equ 260
WAIT_OBJECT_0       equ 0
MB_OK               equ 0
FILE_SHARE_READ     equ 1
FILE_SHARE_WRITE    equ 2

;===============================================================================
; STRUCTURES
;===============================================================================

; Tool Result Structure
ToolResult STRUCT
    success DD 0
    output  DD 0
    error   DD 0
    exitCode DD 0
ToolResult ENDS

; WIN32_FIND_DATAA
WIN32_FIND_DATAA STRUCT
    dwFileAttributes    DWORD   ?
    ftCreationTime      DWORD   2 dup(?)
    ftLastAccessTime    DWORD   2 dup(?)
    ftLastWriteTime     DWORD   2 dup(?)
    nFileSizeHigh       DWORD   ?
    nFileSizeLow        DWORD   ?
    dwReserved0         DWORD   ?
    dwReserved1         DWORD   ?
    cFileName           BYTE    260 dup(?)
    cAlternateFileName  BYTE    14 dup(?)
WIN32_FIND_DATAA ENDS

; STARTUPINFOA
STARTUPINFOA STRUCT
    cb              DWORD   ?
    lpReserved      DWORD   ?
    lpDesktop       DWORD   ?
    lpTitle         DWORD   ?
    dwX             DWORD   ?
    dwY             DWORD   ?
    dwXSize         DWORD   ?
    dwYSize         DWORD   ?
    dwXCountChars   DWORD   ?
    dwYCountChars   DWORD   ?
    dwFillAttribute DWORD   ?
    dwFlags         DWORD   ?
    wShowWindow     WORD    ?
    cbReserved2     WORD    ?
    lpReserved2     DWORD   ?
    hStdInput       DWORD   ?
    hStdOutput      DWORD   ?
    hStdError       DWORD   ?
STARTUPINFOA ENDS

; PROCESS_INFORMATION
PROCESS_INFORMATION STRUCT
    hProcess    DWORD   ?
    hThread     DWORD   ?
    dwProcessId DWORD   ?
    dwThreadId  DWORD   ?
PROCESS_INFORMATION ENDS

; SECURITY_ATTRIBUTES
SECURITY_ATTRIBUTES STRUCT
    nLength             DWORD   ?
    lpSecurityDescriptor DWORD  ?
    bInheritHandle      DWORD   ?
SECURITY_ATTRIBUTES ENDS

;===============================================================================
; DATA SEGMENT
;===============================================================================

.data

; Tool names (1-liner dispatch)
szReadFile      db 'readFile',0
szWriteFile     db 'writeFile',0
szListDir       db 'listDirectory',0
szDeleteFile    db 'deleteFile',0
szExecuteCmd    db 'executeCommand',0

; Strings
szCurrent       db '.',0
szWildcard      db '*',0
szSlash         db '\',0
szNewline       db 13,10,0
szSpace         db ' ',0
szPipe          db '|',0
szCmdExe        db 'cmd.exe /c ',0

; Messages
szErrOpen       db 'Cannot open file: ',0
szErrRead       db 'Read failed',0
szErrWrite      db 'Write failed',0
szErrDir        db 'Directory not found',0
szErrCmd        db 'Command failed',0
szErrTool       db 'Unknown tool',0
szErrParam      db 'Missing parameter',0
szSuccess       db 'Success',0
szFileWritten   db 'File written: ',0
szFileDeleted   db 'File deleted: ',0
szListed        db 'Directory contents:',13,10,0
szDirHeader     db '[DIR]  ',0
szFileHeader    db '[FILE] ',0

; Buffers (all operations use these)
outBuf          db 8192 dup(0)
pathBuf         db MAX_PATH dup(0)
cmdBuf          db 4096 dup(0)
contentBuf      db 8192 dup(0)

;===============================================================================
; CODE SEGMENT - 1-LINER STYLE IMPLEMENTATION
;===============================================================================

.code

;-------------------------------------------------------------------------------
; STRING OPERATIONS - Core string functions
;-------------------------------------------------------------------------------
_strlen PROC str:DWORD
    mov     ecx, str
    xor     eax, eax
strlen_next:
    cmp     byte ptr [ecx+eax], 0
    je      strlen_done
    inc     eax
    jmp     strlen_next
strlen_done:
    ret
_strlen ENDP

_strcpy PROC dst:DWORD, src:DWORD
    push    esi
    push    edi
    mov     esi, src
    mov     edi, dst
strcpy_copy:
    lodsb
    stosb
    test    al, al
    jnz     strcpy_copy
    pop     edi
    pop     esi
    mov     eax, dst
    ret
_strcpy ENDP

_strcat PROC dst:DWORD, src:DWORD
    push    esi
    push    edi
    mov     edi, dst
strcat_find:
    cmp     byte ptr [edi], 0
    je      strcat_cat
    inc     edi
    jmp     strcat_find
strcat_cat:
    mov     esi, src
strcat_loop:
    lodsb
    stosb
    test    al, al
    jnz     strcat_loop
    pop     edi
    pop     esi
    mov     eax, dst
    ret
_strcat ENDP

;-------------------------------------------------------------------------------
; SPLIT PARAMS - "path|content"
;-------------------------------------------------------------------------------
_splitParams PROC lpParams:DWORD, lpPathOut:DWORD, lpContentOut:DWORD
    push    esi
    push    edi
    mov     esi, lpParams
    mov     edi, lpPathOut
split_loop:
    mov     al, byte ptr [esi]
    test    al, al
    jz      split_fail
    cmp     al, '|'
    je      split_found
    mov     byte ptr [edi], al
    inc     esi
    inc     edi
    jmp     split_loop
split_found:
    mov     byte ptr [edi], 0
    inc     esi
    push    esi
    push    lpContentOut
    call    _strcpy
    pop     edi
    pop     esi
    mov     eax, 1
    ret
split_fail:
    mov     byte ptr [edi], 0
    pop     edi
    pop     esi
    xor     eax, eax
    ret
_splitParams ENDP

;-------------------------------------------------------------------------------
; READ FILE - Reads file content into memory
;-------------------------------------------------------------------------------
_readFile PROC lpPath:DWORD
    LOCAL   hFile:DWORD, fSize:DWORD, bytes:DWORD, buf:DWORD
    
    push    0
    push    FILE_ATTRIBUTE_NORM
    push    OPEN_EXISTING
    push    0
    push    FILE_SHARE_READ
    push    GENERIC_READ
    push    lpPath
    call    CreateFileA@28
    mov     hFile, eax
    cmp     eax, -1
    je      read_err_open
    
    push    0
    push    hFile
    call    GetFileSize@8
    cmp     eax, -1
    je      read_err_size
    mov     fSize, eax
    
    push    fSize
    push    GMEM_FIXED
    call    GlobalAlloc@8
    test    eax, eax
    je      read_err_alloc
    mov     buf, eax
    
    push    0
    lea     eax, bytes
    push    eax
    push    fSize
    push    buf
    push    hFile
    call    ReadFile@20
    test    eax, eax
    je      read_err_read
    
    push    hFile
    call    CloseHandle@4
    
    mov     eax, buf
    mov     ebx, 1
    ret
read_err_read:
    push    buf
    call    GlobalFree@4
read_err_alloc:
read_err_size:
    push    hFile
    call    CloseHandle@4
read_err_open:
    mov     eax, offset szErrOpen
    mov     ebx, 0
    ret
_readFile ENDP

;-------------------------------------------------------------------------------
; WRITE FILE - Writes content to file
;-------------------------------------------------------------------------------
_writeFile PROC lpPath:DWORD, lpData:DWORD
    LOCAL   hFile:DWORD, len:DWORD, bytes:DWORD
    
    push    lpData
    call    _strlen
    mov     len, eax
    
    push    0
    push    FILE_ATTRIBUTE_NORM
    push    CREATE_ALWAYS
    push    0
    push    0
    push    GENERIC_WRITE
    push    lpPath
    call    CreateFileA@28
    mov     hFile, eax
    cmp     eax, -1
    je      write_err
    
    push    0
    lea     eax, bytes
    push    eax
    push    len
    push    lpData
    push    hFile
    call    WriteFile@20
    test    eax, eax
    je      write_err_close
    
    push    hFile
    call    CloseHandle@4
    
    push    offset szFileWritten
    push    offset outBuf
    call    _strcpy
    push    lpPath
    push    offset outBuf
    call    _strcat
    mov     eax, offset outBuf
    mov     ebx, 1
    ret
write_err_close:
    push    hFile
    call    CloseHandle@4
write_err:
    mov     eax, offset szErrWrite
    mov     ebx, 0
    ret
_writeFile ENDP

;-------------------------------------------------------------------------------
; LIST DIRECTORY - Lists files in directory
;-------------------------------------------------------------------------------
_listDir PROC lpPath:DWORD
    LOCAL   hFind:DWORD, findData:WIN32_FIND_DATAA
    
    push    lpPath
    push    offset pathBuf
    call    _strcpy
    push    offset szSlash
    push    offset pathBuf
    call    _strcat
    push    offset szWildcard
    push    offset pathBuf
    call    _strcat
    
    push    offset szListed
    push    offset outBuf
    call    _strcpy
    
    lea     eax, findData
    push    eax
    push    offset pathBuf
    call    FindFirstFileA@8
    cmp     eax, -1
    je      listdir_done
    mov     hFind, eax
listdir_next:
    lea     esi, findData.cFileName
    cmp     byte ptr [esi], '.'
    je      listdir_skip
    
    push    offset szNewline
    push    offset outBuf
    call    _strcat
    
    mov     eax, findData.dwFileAttributes
    test    eax, 16         ; FILE_ATTRIBUTE_DIRECTORY
    jnz     listdir_is_dir
    push    offset szFileHeader
    push    offset outBuf
    call    _strcat
    jmp     listdir_add_name
listdir_is_dir:
    push    offset szDirHeader
    push    offset outBuf
    call    _strcat
listdir_add_name:
    lea     eax, findData.cFileName
    push    eax
    push    offset outBuf
    call    _strcat
listdir_skip:
    lea     eax, findData
    push    eax
    push    hFind
    call    FindNextFileA@8
    test    eax, eax
    jnz     listdir_next
    
    push    hFind
    call    FindClose@4
listdir_done:
    mov     eax, offset outBuf
    mov     ebx, 1
    ret
_listDir ENDP

;-------------------------------------------------------------------------------
; DELETE FILE - Deletes a file using system command
;-------------------------------------------------------------------------------
_deleteFile PROC lpPath:DWORD
    push    lpPath
    call    DeleteFileA@4
    test    eax, eax
    jz      delete_err
    
    push    offset szFileDeleted
    push    offset outBuf
    call    _strcpy
    push    lpPath
    push    offset outBuf
    call    _strcat
    mov     eax, offset outBuf
    mov     ebx, 1
    ret
delete_err:
    mov     eax, offset szErrCmd
    mov     ebx, 0
    ret
_deleteFile ENDP

;-------------------------------------------------------------------------------
; EXECUTE COMMAND - Executes shell command
;-------------------------------------------------------------------------------
_executeCmd PROC lpCommand:DWORD
    LOCAL   si:STARTUPINFOA, pi:PROCESS_INFORMATION, exitCode:DWORD
    
    lea     edi, si
    mov     ecx, SIZEOF STARTUPINFOA
    xor     al, al
    rep     stosb
    
    lea     edi, pi
    mov     ecx, SIZEOF PROCESS_INFORMATION
    xor     al, al
    rep     stosb
    
    mov     si.cb, SIZEOF STARTUPINFOA
    
    lea     eax, pi
    push    eax
    lea     eax, si
    push    eax
    push    0
    push    0
    push    0
    push    0
    push    0
    push    0
    push    lpCommand
    push    0
    call    CreateProcessA@40
    test    eax, eax
    je      exec_err_create
    
    push    30000           ; 30 second timeout
    push    pi.hProcess
    call    WaitForSingleObject@8
    cmp     eax, WAIT_OBJECT_0
    jne     exec_err_timeout
    
    lea     eax, exitCode
    push    eax
    push    pi.hProcess
    call    GetExitCodeProcess@8
    
    push    pi.hThread
    call    CloseHandle@4
    push    pi.hProcess
    call    CloseHandle@4
    
    test    exitCode, exitCode
    jnz     exec_err_exit
    
    mov     eax, offset szSuccess
    mov     ebx, 1
    ret
exec_err_create:
    mov     eax, offset szErrCmd
    mov     ebx, 0
    ret
exec_err_timeout:
    push    pi.hThread
    call    CloseHandle@4
    push    pi.hProcess
    call    CloseHandle@4
exec_err_exit:
    mov     eax, offset szErrCmd
    mov     ebx, 0
    ret
_executeCmd ENDP

;-------------------------------------------------------------------------------
; AGENTIC TOOL EXECUTOR - Main dispatcher (1-liner style)
;-------------------------------------------------------------------------------
AgenticToolExecutor PROC lpTool:DWORD, lpParam:DWORD
    push    offset szReadFile
    push    lpTool
    call    lstrcmpA@8
    test    eax, eax
    jz      exec_do_read
    
    push    offset szWriteFile
    push    lpTool
    call    lstrcmpA@8
    test    eax, eax
    jz      exec_do_write
    
    push    offset szListDir
    push    lpTool
    call    lstrcmpA@8
    test    eax, eax
    jz      exec_do_list
    
    push    offset szDeleteFile
    push    lpTool
    call    lstrcmpA@8
    test    eax, eax
    jz      exec_do_delete
    
    push    offset szExecuteCmd
    push    lpTool
    call    lstrcmpA@8
    test    eax, eax
    jz      exec_do_exec
    
    mov     eax, offset szErrTool
    mov     ebx, 0
    ret
    
exec_do_read:
    push    lpParam
    call    _readFile
    ret
exec_do_write:
    push    offset contentBuf
    push    offset pathBuf
    push    lpParam
    call    _splitParams
    test    eax, eax
    jz      exec_param_error
    push    offset contentBuf
    push    offset pathBuf
    call    _writeFile
    ret
exec_do_list:
    push    lpParam
    call    _listDir
    ret
exec_do_delete:
    push    lpParam
    call    _deleteFile
    ret
exec_do_exec:
    push    lpParam
    call    _executeCmd
    ret
exec_param_error:
    mov     eax, offset szErrParam
    mov     ebx, 0
    ret
AgenticToolExecutor ENDP

;-------------------------------------------------------------------------------
; MAIN - Entry point with demonstration
;-------------------------------------------------------------------------------
main PROC
    push    offset szCurrent
    push    offset szListDir
    call    AgenticToolExecutor
    
    push    0
    push    offset szListDir
    push    eax
    push    0
    call    MessageBoxA@16
    
    push    0
    call    ExitProcess@4
main ENDP

;===============================================================================
; END - Complete Pure MASM Agentic Tools
;===============================================================================

END main
