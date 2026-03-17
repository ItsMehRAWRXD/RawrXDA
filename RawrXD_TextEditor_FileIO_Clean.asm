; ===============================================================================
; RawrXD_TextEditor_FileIO.asm - Cleaned
; File operations for .asm editing
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
    szFileOpenError db "[FILE] Open failed", 0
    szFileReadError db "[FILE] Read failed", 0
    szFileWriteError db "[FILE] Write failed", 0

    g_hCurrentFile dq 0
    g_CurrentFilePath dq 0
    g_FileSize dq 0
    g_FileModified dd 0

.code

FileIO_OpenRead PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx
    mov rcx, rbx
    mov edx, 0x80000000
    xor r8d, r8d
    xor r9, r9
    mov rax, 3
    mov qword ptr [rsp + 32], rax
    xor rax, rax
    mov qword ptr [rsp + 40], rax
    call CreateFileA
    
    cmp rax, -1
    je OpenFail
    
    mov g_hCurrentFile, rax
    mov g_CurrentFilePath, rbx
    jmp OpenOK
    
OpenFail:
    lea rcx, szFileOpenError
    call OutputDebugStringA
    xor eax, eax
    
OpenOK:
    add rsp, 40
    pop rbx
    ret
FileIO_OpenRead ENDP

FileIO_OpenWrite PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx
    mov rcx, rbx
    mov edx, 0xC0000000
    xor r8d, r8d
    xor r9, r9
    mov rax, 2
    mov qword ptr [rsp + 32], rax
    xor rax, rax
    mov qword ptr [rsp + 40], rax
    call CreateFileA
    
    cmp rax, -1
    je WriteFail
    
    mov g_hCurrentFile, rax
    jmp WriteOK
    
WriteFail:
    lea rcx, szFileOpenError
    call OutputDebugStringA
    xor eax, eax
    
WriteOK:
    add rsp, 40
    pop rbx
    ret
FileIO_OpenWrite ENDP

FileIO_Read PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx
    mov r12, rdx
    mov rcx, [g_hCurrentFile]
    mov rdx, rbx
    mov r8, r12
    lea r9, [rsp + 32]
    xor rax, rax
    mov qword ptr [rsp + 32], rax
    call ReadFile
    
    test eax, eax
    jz ReadFail
    
    mov rax, [rsp + 32]
    jmp ReadOK
    
ReadFail:
    lea rcx, szFileReadError
    call OutputDebugStringA
    xor eax, eax
    
ReadOK:
    add rsp, 40
    pop r12
    pop rbx
    ret
FileIO_Read ENDP

FileIO_Write PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx
    mov r12, rdx
    mov rcx, [g_hCurrentFile]
    mov rdx, rbx
    mov r8, r12
    lea r9, [rsp + 32]
    xor rax, rax
    mov qword ptr [rsp + 32], rax
    call WriteFile
    
    test eax, eax
    jz WriteFail
    
    mov rax, [rsp + 32]
    jmp WriteOK
    
WriteFail:
    lea rcx, szFileWriteError
    call OutputDebugStringA
    xor eax, eax
    
WriteOK:
    add rsp, 40
    pop r12
    pop rbx
    ret
FileIO_Write ENDP

FileIO_Close PROC FRAME
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    cmp qword ptr [g_hCurrentFile], 0
    je CloseSkip
    
    mov rcx, [g_hCurrentFile]
    call CloseHandle
    
    mov qword ptr [g_hCurrentFile], 0
    mov dword ptr [g_FileModified], 0
    
CloseSkip:
    add rsp, 32
    ret
FileIO_Close ENDP

FileIO_SetModified PROC FRAME
    mov dword ptr [g_FileModified], 1
    ret
FileIO_SetModified ENDP

FileIO_ClearModified PROC FRAME
    mov dword ptr [g_FileModified], 0
    ret
FileIO_ClearModified ENDP

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
