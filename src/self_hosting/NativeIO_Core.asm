; ==============================================================================
; RawrXD Native Core - First Bootstrap Component
; Module: NativeIO_Core.asm
; Purpose: Bare-metal x64 File I/O Operations for IDE Autonomy
; ==============================================================================

EXTERN CreateFileA:PROC
EXTERN GetFileSizeEx:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN CloseHandle:PROC

.code

; ------------------------------------------------------------------------------
; NativeIO_ReadFile
; RCX = szFileName (LPCSTR)
; RDX = lpOutFileSize (PDWORD64)
; Returns: RAX = Pointer to allocated buffer containing file data, or 0 if failed.
; ------------------------------------------------------------------------------
NativeIO_ReadFile PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h            ; Shadow space + locals
    
    ; [rbp+10h] = szFileName
    ; [rbp+18h] = lpOutFileSize
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    
    ; clear variables
    xor rax, rax
    mov [rbp-8h], rax       ; hFile
    mov [rbp-10h], rax      ; fileSize (QWORD but GetFileSizeEx wants PLARGE_INTEGER)
    mov [rbp-18h], rax      ; pBuffer
    mov [rbp-20h], rax      ; read_bytes

    ; CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
    mov rcx, [rbp+10h]
    mov rdx, 80000000h      ; GENERIC_READ
    mov r8, 1               ; FILE_SHARE_READ
    xor r9, r9              ; NULL
    mov dword ptr [rsp+20h], 3 ; OPEN_EXISTING
    mov dword ptr [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0 ; NULL
    call CreateFileA
    
    cmp rax, -1
    je _ReadFile_Fail
    mov [rbp-8h], rax       ; save handle

    ; GetFileSizeEx(hFile, &fileSize)
    mov rcx, rax
    lea rdx, [rbp-10h]
    call GetFileSizeEx
    test eax, eax
    jz _ReadFile_CloseFail

    ; Set output size pointer
    mov rdx, [rbp+18h]
    mov rax, [rbp-10h]
    mov [rdx], rax          ; *lpOutFileSize = fileSize

    ; VirtualAlloc(NULL, fileSize + 1 (for string safety), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
    mov rcx, 0
    mov rdx, [rbp-10h]
    add rdx, 1
    mov r8, 3000h           ; MEM_COMMIT | MEM_RESERVE
    mov r9, 4               ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz _ReadFile_CloseFail
    mov [rbp-18h], rax      ; save pBuffer

    ; ReadFile(hFile, pBuffer, fileSize, &read_bytes, NULL)
    mov rcx, [rbp-8h]
    mov rdx, [rbp-18h]
    mov r8d, dword ptr [rbp-10h] ; lowest 32-bits bounds but okay for text config
    lea r9, [rbp-20h]
    mov qword ptr [rsp+20h], 0
    call ReadFile
    
    ; Null terminate the string if it's text
    mov rax, [rbp-18h]
    mov rdx, [rbp-20h]      ; bytes read
    mov byte ptr [rax + rdx], 0

    ; CloseHandle(hFile)
    mov rcx, [rbp-8h]
    call CloseHandle

    ; Return pBuffer into RAX
    mov rax, [rbp-18h]
    jmp _ReadFile_Exit

_ReadFile_CloseFail:
    mov rcx, [rbp-8h]
    call CloseHandle
_ReadFile_Fail:
    xor rax, rax
_ReadFile_Exit:
    add rsp, 80h
    pop rbp
    ret
NativeIO_ReadFile ENDP

; ------------------------------------------------------------------------------
; NativeIO_WriteFile
; RCX = szFileName (LPCSTR)
; RDX = lpBuffer (LPCVOID)
; R8  = nNumberOfBytesToWrite (DWORD)
; Returns: RAX = Number of bytes written, or 0 if failed.
; ------------------------------------------------------------------------------
NativeIO_WriteFile PROC
    push rbp
    mov rbp, rsp
    sub rsp, 70h            ; Shadow space + locals
    
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    mov [rbp+20h], r8
    
    xor rax, rax
    mov [rbp-8h], rax       ; hFile
    mov [rbp-10h], rax      ; written_bytes

    ; CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
    mov rcx, [rbp+10h]
    mov rdx, 40000000h      ; GENERIC_WRITE
    xor r8, r8              ; FILE_SHARE_NONE
    xor r9, r9              ; NULL
    mov dword ptr [rsp+20h], 2 ; CREATE_ALWAYS
    mov dword ptr [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0 ; NULL
    call CreateFileA
    
    cmp rax, -1
    je _WriteFile_Fail
    mov [rbp-8h], rax       ; save hFile
    
    ; WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &written_bytes, NULL)
    mov rcx, rax
    mov rdx, [rbp+18h]
    mov r8, [rbp+20h]
    lea r9, [rbp-10h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    
    ; CloseHandle
    mov rcx, [rbp-8h]
    call CloseHandle
    
    mov rax, [rbp-10h]      ; return written bytes
    jmp _WriteFile_Exit

_WriteFile_Fail:
    xor rax, rax
_WriteFile_Exit:
    add rsp, 70h
    pop rbp
    ret
NativeIO_WriteFile ENDP

; ------------------------------------------------------------------------------
; NativeIO_FreeBuffer
; RCX = pBuffer
; ------------------------------------------------------------------------------
NativeIO_FreeBuffer PROC
    push rbp
    mov rbp, rsp
    sub rsp, 30h
    
    test rcx, rcx
    jz _FreeBuffer_Exit
    
    mov rdx, 0
    mov r8, 8000h           ; MEM_RELEASE
    call VirtualFree
    
_FreeBuffer_Exit:
    add rsp, 30h
    pop rbp
    ret
NativeIO_FreeBuffer ENDP

; DllMain entry
DllMain PROC
    mov rax, 1
    ret
DllMain ENDP

END
