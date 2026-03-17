; ============================================================================
; file_io_enterprise.asm - Production-Ready File I/O for Editor
; Implements Editor_LoadFile, Editor_SaveFile with buffering, encoding detection
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
; CONSTANTS
; ============================================================================

FILE_BUFFER_SIZE        equ 65536      ; 64KB chunks
MAX_FILE_SIZE           equ 104857600  ; 100MB max
MAX_LINES               equ 1000000

; Encoding types
ENCODING_ASCII          equ 0
ENCODING_UTF8           equ 1
ENCODING_UTF16LE        equ 2
ENCODING_UTF16BE        equ 3

; ============================================================================
; STRUCTURES
; ============================================================================

FILE_STATE struct
    hFile               dd ?
    dwFileSize          dd ?
    dwBytesRead         dd ?
    dwEncoding          dd ?
    pBuffer             dd ?
    dwBufferSize        dd ?
    dwBufferPos         dd ?
    bDirty              dd ?
    szFilePath          db MAX_PATH dup(?)
FILE_STATE ends

; ============================================================================
; DATA
; ============================================================================

.data
    public g_FileState
    
    g_FileState FILE_STATE <>
    
    ; BOM signatures for encoding detection
    szUtf8BOM           db 0EFh, 0BBh, 0BFh    ; UTF-8 BOM
    szUtf16LEBOM        db 0FFh, 0FEh          ; UTF-16 LE BOM
    szUtf16BEBOM        db 0FEh, 0FFh          ; UTF-16 BE BOM

.code

; ============================================================================
; Encoding Detection - Check file header for BOM
; Input: lpBuffer = buffer start, dwSize = buffer size
; Output: EAX = ENCODING_*
; ============================================================================
DetectEncoding proc lpBuffer:DWORD, dwSize:DWORD
    mov esi, lpBuffer
    mov ecx, dwSize
    
    ; Check for UTF-8 BOM (3 bytes)
    cmp ecx, 3
    jl @checkUtf16
    
    movzx eax, byte ptr [esi]
    cmp eax, 0EFh
    jne @checkUtf16
    
    movzx eax, byte ptr [esi+1]
    cmp eax, 0BBh
    jne @checkUtf16
    
    movzx eax, byte ptr [esi+2]
    cmp eax, 0BFh
    jne @checkUtf16
    
    mov eax, ENCODING_UTF8
    ret

@checkUtf16:
    cmp ecx, 2
    jl @defaultAscii
    
    ; Check UTF-16 LE BOM
    movzx eax, byte ptr [esi]
    cmp eax, 0FFh
    jne @checkUtf16Be
    
    movzx eax, byte ptr [esi+1]
    cmp eax, 0FEh
    jne @checkUtf16Be
    
    mov eax, ENCODING_UTF16LE
    ret

@checkUtf16Be:
    movzx eax, byte ptr [esi]
    cmp eax, 0FEh
    jne @defaultAscii
    
    movzx eax, byte ptr [esi+1]
    cmp eax, 0FFh
    jne @defaultAscii
    
    mov eax, ENCODING_UTF16BE
    ret

@defaultAscii:
    mov eax, ENCODING_ASCII
    ret
DetectEncoding endp

; ============================================================================
; Editor_LoadFile - Load file into editor buffer
; Input: lpszFilePath = path to file
; Output: EAX = TRUE on success, FALSE on failure
; ============================================================================
public Editor_LoadFile
Editor_LoadFile proc lpszFilePath:DWORD
    LOCAL hFile:DWORD
    LOCAL dwFileSize:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL pBuffer:DWORD
    LOCAL dwBytesAlloc:DWORD
    push ebx
    push esi
    push edi
    
    ; Open file for reading
    invoke CreateFileA, lpszFilePath, GENERIC_READ, FILE_SHARE_READ, 0, \
           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    cmp eax, MAX_FILE_SIZE
    jg @file_too_large
    mov dwFileSize, eax
    
    ; Allocate buffer (add 1 for null terminator)
    mov eax, dwFileSize
    add eax, 1
    mov dwBytesAlloc, eax
    invoke GlobalAlloc, GMEM_FIXED, dwBytesAlloc
    test eax, eax
    jz @alloc_fail
    mov pBuffer, eax
    
    ; Read file into buffer
    invoke ReadFile, hFile, pBuffer, dwFileSize, addr dwBytesRead, NULL
    test eax, eax
    jz @read_fail
    
    ; Null-terminate buffer
    mov eax, pBuffer
    add eax, dwBytesRead
    mov byte ptr [eax], 0
    
    ; Detect encoding
    invoke DetectEncoding, pBuffer, dwBytesRead
    mov g_FileState.dwEncoding, eax
    
    ; Store file info
    mov eax, dwFileSize
    mov g_FileState.dwFileSize, eax
    mov eax, dwBytesRead
    mov g_FileState.dwBytesRead, eax
    mov eax, pBuffer
    mov g_FileState.pBuffer, eax
    mov eax, dwBytesAlloc
    mov g_FileState.dwBufferSize, eax
    mov dword ptr g_FileState.dwBufferPos, 0
    mov dword ptr g_FileState.bDirty, 0
    
    ; Store file path
    push edi
    mov esi, lpszFilePath
    lea edi, g_FileState.szFilePath
    mov ecx, MAX_PATH
@@copy_path:
    lodsb
    stosb
    test al, al
    jz @@path_done
    loop @@copy_path
@@path_done:
    pop edi
    
    ; Success
    invoke CloseHandle, hFile
    mov eax, TRUE
    pop edi
    pop esi
    pop ebx
    ret

@read_fail:
    invoke GlobalFree, pBuffer
@alloc_fail:
    invoke CloseHandle, hFile
@file_too_large:
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
Editor_LoadFile endp

; ============================================================================
; Editor_SaveFile - Save editor buffer to file
; Input: lpszFilePath = destination path (if NULL, use current path)
;        dwEncoding = output encoding (ENCODING_*)
; Output: EAX = TRUE on success, FALSE on failure
; ============================================================================
public Editor_SaveFile
Editor_SaveFile proc lpszFilePath:DWORD, dwEncoding:DWORD
    LOCAL hFile:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL pszPath:DWORD
    push ebx
    push esi
    push edi
    
    ; Determine output path
    mov eax, lpszFilePath
    test eax, eax
    jnz @use_given_path
    
    ; Use stored path
    lea eax, g_FileState.szFilePath
    
@use_given_path:
    mov pszPath, eax
    
    ; Validate buffer exists
    mov eax, g_FileState.pBuffer
    test eax, eax
    jz @fail
    
    ; Create file (overwrite if exists)
    invoke CreateFileA, pszPath, GENERIC_WRITE, 0, 0, \
           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov hFile, eax
    
    ; Write BOM if needed
    mov eax, dwEncoding
    cmp eax, ENCODING_UTF8
    jne @skip_utf8_bom
    
    ; Write UTF-8 BOM
    invoke WriteFile, hFile, addr szUtf8BOM, 3, addr dwBytesWritten, NULL
    test eax, eax
    jz @write_fail
    jmp @write_content

@skip_utf8_bom:
    cmp eax, ENCODING_UTF16LE
    jne @skip_utf16le_bom
    
    ; Write UTF-16 LE BOM
    invoke WriteFile, hFile, addr szUtf16LEBOM, 2, addr dwBytesWritten, NULL
    test eax, eax
    jz @write_fail
    jmp @write_content

@skip_utf16le_bom:
    cmp eax, ENCODING_UTF16BE
    jne @write_content
    
    ; Write UTF-16 BE BOM
    invoke WriteFile, hFile, addr szUtf16BEBOM, 2, addr dwBytesWritten, NULL
    test eax, eax
    jz @write_fail

@write_content:
    ; Write file content using LOCAL for buffer storage
    LOCAL dwContentSize:DWORD
    
    mov eax, g_FileState.dwBytesRead
    mov dwContentSize, eax
    
    invoke WriteFile, hFile, g_FileState.pBuffer, dwContentSize, addr dwBytesWritten, NULL
    test eax, eax
    jz @write_fail
    
    ; Verify bytes written
    mov ecx, dwBytesWritten
    cmp ecx, dwContentSize
    jne @write_fail
    
    ; Update state
    mov dword ptr g_FileState.bDirty, 0
    mov eax, dwEncoding
    mov g_FileState.dwEncoding, eax
    
    ; Update path if different
    mov esi, pszPath
    lea edi, g_FileState.szFilePath
    mov ecx, MAX_PATH
@@copy_new_path:
    lodsb
    stosb
    test al, al
    jz @@save_done
    loop @@copy_new_path

@@save_done:
    invoke CloseHandle, hFile
    mov eax, TRUE
    pop edi
    pop esi
    pop ebx
    ret

@write_fail:
    invoke CloseHandle, hFile
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
Editor_SaveFile endp

; ============================================================================
; Editor_GetBuffer - Get current file buffer pointer
; Returns: EAX = buffer pointer, 0 if not loaded
; ============================================================================
public Editor_GetBuffer
Editor_GetBuffer proc
    mov eax, g_FileState.pBuffer
    ret
Editor_GetBuffer endp

; ============================================================================
; Editor_GetFileSize - Get loaded file size
; Returns: EAX = size in bytes
; ============================================================================
public Editor_GetFileSize
Editor_GetFileSize proc
    mov eax, g_FileState.dwBytesRead
    ret
Editor_GetFileSize endp

; ============================================================================
; Editor_GetEncoding - Get detected encoding
; Returns: EAX = ENCODING_*
; ============================================================================
public Editor_GetEncoding
Editor_GetEncoding proc
    mov eax, g_FileState.dwEncoding
    ret
Editor_GetEncoding endp

; ============================================================================
; Editor_SetDirty - Mark buffer as modified
; ============================================================================
public Editor_SetDirty
Editor_SetDirty proc bDirty:DWORD
    mov eax, bDirty
    mov g_FileState.bDirty, eax
    ret
Editor_SetDirty endp

; ============================================================================
; Editor_GetDirty - Check if buffer is modified
; Returns: EAX = TRUE if modified, FALSE if clean
; ============================================================================
public Editor_GetDirty
Editor_GetDirty proc
    mov eax, g_FileState.bDirty
    ret
Editor_GetDirty endp

; ============================================================================
; Editor_GetFilePath - Get current file path
; Returns: EAX = pointer to path string
; ============================================================================
public Editor_GetFilePath
Editor_GetFilePath proc
    lea eax, g_FileState.szFilePath
    ret
Editor_GetFilePath endp

; ============================================================================
; Editor_CloseFile - Free resources and close file
; ============================================================================
public Editor_CloseFile
Editor_CloseFile proc
    mov eax, g_FileState.pBuffer
    test eax, eax
    jz @skip_free
    
    invoke GlobalFree, eax
    mov dword ptr g_FileState.pBuffer, 0
    mov dword ptr g_FileState.dwFileSize, 0
    mov dword ptr g_FileState.dwBytesRead, 0
    mov dword ptr g_FileState.dwBufferPos, 0

@skip_free:
    mov eax, TRUE
    ret
Editor_CloseFile endp

end
