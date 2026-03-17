; ===============================================================================
; RawrXD_TextEditor_FileIO.asm
; File operations: Open/Save .asm files
; ===============================================================================

OPTION CASEMAP:NONE

EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN OutputDebugStringA:PROC

.data
    ALIGN 16
    szFileOpenError     db "[FILE] Failed to open file", 0
    szFileReadError     db "[FILE] Failed to read file", 0
    szFileWriteError    db "[FILE] Failed to write file", 0
    szFileSuccess       db "[FILE] Operation successful", 0

    g_hCurrentFile      dq 0        ; File handle
    g_CurrentFilePath   dq 0        ; Path pointer
    g_FileSize          dq 0        ; Size in bytes
    g_FileModified      dd 0        ; 1=dirty, 0=clean
    g_FileBuffer        dq 0        ; Buffer pointer
    g_FileBufferSize    dq 0        ; Bytes in buffer

.code

; ===============================================================================
; FileIO_OpenRead - Open file for reading
; rcx = file path (null-terminated string)
; Returns: rax = file handle (0 = error), rdx = file size
; ===============================================================================
FileIO_OpenRead PROC FRAME USES rbx
    .endprolog
    
    mov rbx, rcx                    ; rbx = file path
    
    ; CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL)
    mov rcx, rbx                    ; lpFileName
    mov edx, 0x80000000            ; dwDesiredAccess = GENERIC_READ
    xor r8d, r8d                    ; dwShareMode = 0
    xor r9, r9                      ; lpSecurityAttributes = NULL
    
    sub rsp, 32
    mov eax, 3
    mov dword ptr [rsp + 0x20], eax ; OPEN_EXISTING
    xor eax, eax
    mov dword ptr [rsp + 0x28], eax ; dwFlagsAndAttributes
    mov qword ptr [rsp + 0x30], rax ; hTemplateFile
    
    call CreateFileA
    add rsp, 32
    
    cmp rax, -1
    je OpenReadFail
    
    mov g_hCurrentFile, rax
    mov g_CurrentFilePath, rbx
    ret
    
OpenReadFail:
    lea rcx, szFileOpenError
    call OutputDebugStringA
    xor eax, eax
    ret
FileIO_OpenRead ENDP

; ===============================================================================
; FileIO_OpenWrite - Open file for writing
; ===============================================================================
FileIO_OpenWrite PROC FRAME
    .endprolog
    
    ; CreateFileA with CREATE_ALWAYS (truncates if exists)
    mov rdx, 0xC0000000            ; GENERIC_READ | GENERIC_WRITE
    xor r8d, r8d                    ; FILE_SHARE_NONE
    xor r9, r9                      ; Security = NULL
    
    sub rsp, 32
    mov eax, 2
    mov dword ptr [rsp + 0x20], eax ; CREATE_ALWAYS
    xor eax, eax
    mov dword ptr [rsp + 0x28], eax
    
    call CreateFileA
    add rsp, 32
    
    cmp rax, -1
    je OpenWriteFail
    
    mov g_hCurrentFile, rax
    ret
    
OpenWriteFail:
    lea rcx, szFileOpenError
    call OutputDebugStringA
    xor eax, eax
    ret
FileIO_OpenWrite ENDP

; ===============================================================================
; FileIO_Read - Read entire file into buffer
; rcx = output buffer
; rdx = max bytes
; Returns: rax = bytes read (0 = error)
; ===============================================================================
FileIO_Read PROC FRAME USES rbx r12
    .endprolog

    mov rbx, rcx                    ; rbx = output buffer
    mov r12, rdx                    ; r12 = max bytes
    
    mov rcx, [g_hCurrentFile]
    mov rdx, rbx                    ; Buffer
    mov r8, r12                     ; Bytes to read
    lea r9, [rsp + 8]               ; Bytes read
    
    sub rsp, 32
    call ReadFile
    add rsp, 32
    
    test eax, eax
    jz ReadFail
    
    mov rax, [rsp + 8]
    ret
    
ReadFail:
    lea rcx, szFileReadError
    call OutputDebugStringA
    xor eax, eax
    ret
FileIO_Read ENDP

; ===============================================================================
; FileIO_Write - Write buffer to file
; rcx = buffer
; rdx = bytes to write
; Returns: rax = bytes written
; ===============================================================================
FileIO_Write PROC FRAME USES rbx r12
    .endprolog

    mov rbx, rcx                    ; rbx = buffer
    mov r12, rdx                    ; r12 = bytes
    
    mov rcx, [g_hCurrentFile]
    mov rdx, rbx
    mov r8, r12
    lea r9, [rsp + 8]
    
    sub rsp, 32
    call WriteFile
    add rsp, 32
    
    test eax, eax
    jz WriteFail
    
    mov rax, [rsp + 8]
    ret
    
WriteFail:
    lea rcx, szFileWriteError
    call OutputDebugStringA
    xor eax, eax
    ret
FileIO_Write ENDP

; ===============================================================================
; FileIO_Close - Close current file
; Returns: rax = 1
; ===============================================================================
FileIO_Close PROC FRAME
    .endprolog

    cmp qword ptr [g_hCurrentFile], 0
    je CloseSkip
    
    mov rcx, [g_hCurrentFile]
    
    sub rsp, 32
    call CloseHandle
    add rsp, 32
    
    mov qword ptr [g_hCurrentFile], 0
    mov dword ptr [g_FileModified], 0
    
CloseSkip:
    mov eax, 1
    ret
FileIO_Close ENDP

; ===============================================================================
; FileIO_SetModified - Mark file as modified
; ===============================================================================
FileIO_SetModified PROC FRAME
    mov dword ptr [g_FileModified], 1
    ret
FileIO_SetModified ENDP

; ===============================================================================
; FileIO_ClearModified - Mark file as unmodified
; ===============================================================================
FileIO_ClearModified PROC FRAME
    mov dword ptr [g_FileModified], 0
    ret
FileIO_ClearModified ENDP

; ===============================================================================
; FileIO_IsModified - Check if file has unsaved changes
; Returns: rax = 0 or 1
; ===============================================================================
FileIO_IsModified PROC FRAME
    mov eax, dword ptr [g_FileModified]
    ret
FileIO_IsModified ENDP

PUBLIC FileIO_OpenRead
PUBLIC FileIO_OpenWrite
PUBLIC FileIO_Read
PUBLIC FileIO_Write
PUBLIC FileIO_Close
PUBLIC FileIO_SetModified
PUBLIC FileIO_ClearModified
PUBLIC FileIO_IsModified
PUBLIC g_hCurrentFile
PUBLIC g_CurrentFilePath
PUBLIC g_FileSize
PUBLIC g_FileModified

END
