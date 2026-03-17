;======================================================================
; RawrXD IDE - File Operations Module
; File I/O, encoding detection, save/load, auto-save functionality
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; File encoding types
ENCODING_UTF8           EQU 0
ENCODING_ANSI           EQU 1
ENCODING_UTF16          EQU 2

; Line ending types
LINEEND_CRLF            EQU 0
LINEEND_LF              EQU 1
LINEEND_CR              EQU 2

; File operation status
FILE_OK                 EQU 0
FILE_NOT_FOUND          EQU 1
FILE_READ_ERROR         EQU 2
FILE_WRITE_ERROR        EQU 3
FILE_ACCESS_DENIED      EQU 4

g_lastOpenPath[260]     DB 260 DUP(0)
g_lastSavePath[260]     DB 260 DUP(0)
g_fileModified          DQ 0
g_autoSaveInterval      DQ 60000  ; 60 seconds
g_autoSaveEnabled       DQ 1

; BOM markers for encoding detection
BOM_UTF8                DB 0EFh, 0BBh, 0BFh, 0
BOM_UTF16LE             DB 0FFh, 0FEh, 0
BOM_UTF16BE             DB 0FEh, 0FFh, 0

.CODE

;----------------------------------------------------------------------
; RawrXD_FileOps_LoadFile - Load file from disk
;----------------------------------------------------------------------
RawrXD_FileOps_LoadFile PROC pszFilename:QWORD, ppBuffer:QWORD, pFileSize:QWORD
    LOCAL hFile:QWORD
    LOCAL fileSize:QWORD
    LOCAL bytesRead:QWORD
    LOCAL pBuffer:QWORD
    
    ; Open file
    INVOKE CreateFileA,
        pszFilename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    
    cmp rax, INVALID_HANDLE_VALUE
    je @@not_found
    
    mov hFile, rax
    
    ; Get file size
    INVOKE GetFileSize, hFile, NULL
    mov fileSize, rax
    
    ; Allocate buffer
    mov rcx, fileSize
    add rcx, 1  ; Extra byte for null terminator
    INVOKE HeapAlloc, GetProcessHeap(), 0, rcx
    test rax, rax
    jz @@memory_error
    
    mov pBuffer, rax
    
    ; Read file
    INVOKE ReadFile, hFile, pBuffer, fileSize, ADDR bytesRead, NULL
    test eax, eax
    jz @@read_error
    
    ; Add null terminator
    mov al, 0
    mov [pBuffer + fileSize], al
    
    ; Close file
    INVOKE CloseHandle, hFile
    
    ; Return buffer and size
    mov rax, ppBuffer
    mov rcx, pBuffer
    mov [rax], rcx
    
    mov rax, pFileSize
    mov [rax], fileSize
    
    xor eax, eax
    ret
    
@@not_found:
    mov eax, FILE_NOT_FOUND
    ret
    
@@read_error:
    INVOKE CloseHandle, hFile
    INVOKE HeapFree, GetProcessHeap(), 0, pBuffer
    mov eax, FILE_READ_ERROR
    ret
    
@@memory_error:
    INVOKE CloseHandle, hFile
    mov eax, FILE_READ_ERROR
    ret
    
RawrXD_FileOps_LoadFile ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_SaveFile - Save file to disk
;----------------------------------------------------------------------
RawrXD_FileOps_SaveFile PROC pszFilename:QWORD, pBuffer:QWORD, bufferSize:QWORD
    LOCAL hFile:QWORD
    LOCAL bytesWritten:QWORD
    LOCAL szTempName[260]:BYTE
    
    ; Create temp file first
    INVOKE lstrcpyA, ADDR szTempName, pszFilename
    INVOKE lstrcatA, ADDR szTempName, ".tmp"
    
    ; Create file
    INVOKE CreateFileA,
        ADDR szTempName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    
    cmp rax, INVALID_HANDLE_VALUE
    je @@access_denied
    
    mov hFile, rax
    
    ; Write buffer
    INVOKE WriteFile, hFile, pBuffer, bufferSize, ADDR bytesWritten, NULL
    test eax, eax
    jz @@write_error
    
    ; Close file
    INVOKE CloseHandle, hFile
    
    ; Replace original with temp
    INVOKE DeleteFileA, pszFilename
    INVOKE MoveFileA, ADDR szTempName, pszFilename
    
    ; Save path for next use
    INVOKE lstrcpyA, ADDR g_lastSavePath, pszFilename
    
    ; Clear modified flag
    mov g_fileModified, 0
    
    xor eax, eax
    ret
    
@@write_error:
    INVOKE CloseHandle, hFile
    INVOKE DeleteFileA, ADDR szTempName
    mov eax, FILE_WRITE_ERROR
    ret
    
@@access_denied:
    mov eax, FILE_ACCESS_DENIED
    ret
    
RawrXD_FileOps_SaveFile ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_DetectEncoding - Detect file encoding from BOM
;----------------------------------------------------------------------
RawrXD_FileOps_DetectEncoding PROC pBuffer:QWORD
    LOCAL byteA:QWORD
    LOCAL byteB:QWORD
    LOCAL byteC:QWORD
    
    ; Check BOM markers
    mov al, [pBuffer]
    mov byteA, rax
    
    mov al, [pBuffer + 1]
    mov byteB, rax
    
    mov al, [pBuffer + 2]
    mov byteC, rax
    
    ; Check UTF-8 BOM (EF BB BF)
    cmp byteA, 0EFh
    jne @@not_utf8
    cmp byteB, 0BBh
    jne @@not_utf8
    cmp byteC, 0BFh
    jne @@not_utf8
    
    mov eax, ENCODING_UTF8
    ret
    
@@not_utf8:
    ; Check UTF-16 LE BOM (FF FE)
    cmp byteA, 0FFh
    jne @@not_utf16le
    cmp byteB, 0FEh
    jne @@not_utf16le
    
    mov eax, ENCODING_UTF16
    ret
    
@@not_utf16le:
    ; Check UTF-16 BE BOM (FE FF)
    cmp byteA, 0FEh
    jne @@not_utf16be
    cmp byteB, 0FFh
    jne @@not_utf16be
    
    mov eax, ENCODING_UTF16
    ret
    
@@not_utf16be:
    ; Default to ANSI
    mov eax, ENCODING_ANSI
    ret
    
RawrXD_FileOps_DetectEncoding ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_DetectLineEnding - Detect line ending style
;----------------------------------------------------------------------
RawrXD_FileOps_DetectLineEnding PROC pBuffer:QWORD
    LOCAL pPos:QWORD
    LOCAL charCode:QWORD
    
    mov pPos, pBuffer
    
@@scan:
    mov al, [pPos]
    mov charCode, rax
    
    ; Check for CR LF
    cmp al, 13  ; CR
    jne @@check_lf
    
    mov al, [pPos + 1]
    cmp al, 10  ; LF
    jne @@is_cr
    
    mov eax, LINEEND_CRLF
    ret
    
@@is_cr:
    mov eax, LINEEND_CR
    ret
    
@@check_lf:
    cmp al, 10  ; LF
    jne @@next
    
    mov eax, LINEEND_LF
    ret
    
@@next:
    ; Continue scanning
    cmp al, 0
    je @@default
    
    inc pPos
    jmp @@scan
    
@@default:
    ; Default to CRLF
    mov eax, LINEEND_CRLF
    ret
    
RawrXD_FileOps_DetectLineEnding ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_MarkModified - Mark file as modified
;----------------------------------------------------------------------
RawrXD_FileOps_MarkModified PROC
    mov g_fileModified, 1
    ret
RawrXD_FileOps_MarkModified ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_IsModified - Check if file is modified
;----------------------------------------------------------------------
RawrXD_FileOps_IsModified PROC
    mov eax, g_fileModified
    ret
RawrXD_FileOps_IsModified ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_GetExtension - Get file extension
;----------------------------------------------------------------------
RawrXD_FileOps_GetExtension PROC pszFilename:QWORD, pszExt:QWORD
    LOCAL pStart:QWORD
    LOCAL idx:QWORD
    
    ; Find last dot
    mov pStart, pszFilename
    xor idx, idx
    
@@scan:
    mov al, [pszFilename + idx]
    test al, al
    jz @@not_found
    
    cmp al, '.'
    jne @@next
    
    ; Found dot - save position
    mov pStart, pszFilename
    add pStart, idx
    
@@next:
    inc idx
    jmp @@scan
    
@@not_found:
    ; Copy extension
    INVOKE lstrcpyA, pszExt, pStart
    ret
    
RawrXD_FileOps_GetExtension ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_IsSourceFile - Check if file is source code
;----------------------------------------------------------------------
RawrXD_FileOps_IsSourceFile PROC pszFilename:QWORD
    LOCAL szExt[16]:BYTE
    
    ; Get extension
    INVOKE RawrXD_FileOps_GetExtension, pszFilename, ADDR szExt
    
    ; Check extensions
    INVOKE lstrcmpA, ADDR szExt, ".asm"
    test eax, eax
    jz @@yes
    
    INVOKE lstrcmpA, ADDR szExt, ".c"
    test eax, eax
    jz @@yes
    
    INVOKE lstrcmpA, ADDR szExt, ".h"
    test eax, eax
    jz @@yes
    
    INVOKE lstrcmpA, ADDR szExt, ".inc"
    test eax, eax
    jz @@yes
    
    INVOKE lstrcmpA, ADDR szExt, ".cpp"
    test eax, eax
    jz @@yes
    
    INVOKE lstrcmpA, ADDR szExt, ".hpp"
    test eax, eax
    jz @@yes
    
    xor eax, eax
    ret
    
@@yes:
    mov eax, 1
    ret
    
RawrXD_FileOps_IsSourceFile ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_CreateBackup - Create backup of file
;----------------------------------------------------------------------
RawrXD_FileOps_CreateBackup PROC pszFilename:QWORD
    LOCAL szBackupName[260]:BYTE
    LOCAL timeStamp[32]:BYTE
    
    ; Create backup filename with timestamp
    INVOKE lstrcpyA, ADDR szBackupName, pszFilename
    INVOKE lstrcatA, ADDR szBackupName, ".bak"
    
    ; Copy original to backup
    INVOKE CopyFileA, pszFilename, ADDR szBackupName, FALSE
    
    xor eax, eax
    ret
    
RawrXD_FileOps_CreateBackup ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_SetAutoSave - Enable/disable auto-save
;----------------------------------------------------------------------
RawrXD_FileOps_SetAutoSave PROC enabled:QWORD, interval:QWORD
    mov g_autoSaveEnabled, enabled
    mov g_autoSaveInterval, interval
    ret
RawrXD_FileOps_SetAutoSave ENDP

;----------------------------------------------------------------------
; RawrXD_FileOps_FreeBuffer - Free allocated file buffer
;----------------------------------------------------------------------
RawrXD_FileOps_FreeBuffer PROC pBuffer:QWORD
    INVOKE HeapFree, GetProcessHeap(), 0, pBuffer
    ret
RawrXD_FileOps_FreeBuffer ENDP

END
