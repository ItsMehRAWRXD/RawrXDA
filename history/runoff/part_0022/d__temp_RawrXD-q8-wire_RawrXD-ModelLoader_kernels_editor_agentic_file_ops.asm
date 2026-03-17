; agentic_file_ops.asm - Full File System Operations for Agentic AI
; Provides CREATE, READ, WRITE, DELETE, SEARCH operations for AI agents
; Author: RawrXD Team
; Date: December 3, 2025

OPTION CASEMAP:NONE

EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN DeleteFileA:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN SetFilePointer:PROC
EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN GetLastError:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC

; Constants
GENERIC_READ        EQU 80000000h
GENERIC_WRITE       EQU 40000000h
CREATE_ALWAYS       EQU 2
OPEN_EXISTING       EQU 3
FILE_ATTRIBUTE_NORMAL EQU 80h
INVALID_HANDLE_VALUE EQU -1
MEM_COMMIT          EQU 1000h
MEM_RESERVE         EQU 2000h
MEM_RELEASE         EQU 8000h
PAGE_READWRITE      EQU 4

.DATA
    lastErrorCode DWORD 0

.CODE

; AgenticReadFile: Read entire file into allocated buffer
; rcx = filepath (const char*)
; rdx = out_buffer_ptr (void**)
; r8  = out_size (QWORD*)
; Returns: 1 on success, 0 on failure
PUBLIC AgenticReadFile
AgenticReadFile PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40

    mov r12, rcx           ; filepath
    mov r13, rdx           ; out_buffer_ptr
    mov r14, r8            ; out_size

    ; Open file for reading
    mov rcx, r12
    mov edx, GENERIC_READ
    xor r8d, r8d           ; no sharing
    xor r9d, r9d           ; no security
    sub rsp, 32
    mov dword ptr [rsp+32], FILE_ATTRIBUTE_NORMAL
    mov dword ptr [rsp+24], OPEN_EXISTING
    call CreateFileA
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je .read_fail
    mov rbx, rax           ; file handle

    ; Get file size
    mov rcx, rbx
    xor edx, edx
    call GetFileSize
    test rax, rax
    je .close_fail
    mov r15, rax           ; file size

    ; Allocate buffer
    xor rcx, rcx
    mov rdx, r15
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    je .close_fail
    mov rsi, rax           ; buffer

    ; Read file
    mov rcx, rbx
    mov rdx, rsi
    mov r8d, r15d
    lea r9, [rsp+16]       ; bytes read
    sub rsp, 32
    xor eax, eax
    mov qword ptr [rsp+32], rax
    call ReadFile
    add rsp, 32
    test eax, eax
    je .free_fail

    ; Store outputs
    mov qword ptr [r13], rsi
    mov qword ptr [r14], r15

    ; Close handle
    mov rcx, rbx
    call CloseHandle

    mov eax, 1
    add rsp, 40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

.free_fail:
    mov rcx, rsi
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
.close_fail:
    mov rcx, rbx
    call CloseHandle
.read_fail:
    call GetLastError
    mov [lastErrorCode], eax
    xor eax, eax
    add rsp, 40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
AgenticReadFile ENDP

; AgenticWriteFile: Write buffer to file
; rcx = filepath
; rdx = buffer
; r8  = size
; Returns: 1 on success, 0 on failure
PUBLIC AgenticWriteFile
AgenticWriteFile PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 48

    mov rbx, rcx           ; filepath
    mov rsi, rdx           ; buffer
    mov rdi, r8            ; size

    ; Create/overwrite file
    mov rcx, rbx
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    mov dword ptr [rsp+32], FILE_ATTRIBUTE_NORMAL
    mov dword ptr [rsp+24], CREATE_ALWAYS
    call CreateFileA
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je .write_fail
    mov rbx, rax           ; file handle

    ; Write data
    mov rcx, rbx
    mov rdx, rsi
    mov r8d, edi
    lea r9, [rsp+16]       ; bytes written
    sub rsp, 32
    xor eax, eax
    mov qword ptr [rsp+32], rax
    call WriteFile
    add rsp, 32
    test eax, eax
    je .close_write_fail

    ; Close
    mov rcx, rbx
    call CloseHandle

    mov eax, 1
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret

.close_write_fail:
    mov rcx, rbx
    call CloseHandle
.write_fail:
    call GetLastError
    mov [lastErrorCode], eax
    xor eax, eax
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
AgenticWriteFile ENDP

; AgenticDeleteFile: Delete a file
; rcx = filepath
; Returns: 1 on success, 0 on failure
PUBLIC AgenticDeleteFile
AgenticDeleteFile PROC
    push rbx
    sub rsp, 32

    mov rbx, rcx
    call DeleteFileA
    test eax, eax
    jne .delete_ok

    call GetLastError
    mov [lastErrorCode], eax
    xor eax, eax
    jmp .delete_done

.delete_ok:
    mov eax, 1

.delete_done:
    add rsp, 32
    pop rbx
    ret
AgenticDeleteFile ENDP

; AgenticCreateFile: Create empty file
; rcx = filepath
; Returns: 1 on success, 0 on failure
PUBLIC AgenticCreateFile
AgenticCreateFile PROC
    push rbx
    sub rsp, 48

    mov rbx, rcx

    ; Create file
    mov rcx, rbx
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    mov dword ptr [rsp+32], FILE_ATTRIBUTE_NORMAL
    mov dword ptr [rsp+24], CREATE_ALWAYS
    call CreateFileA
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je .create_fail
    
    ; Close immediately
    mov rcx, rax
    call CloseHandle

    mov eax, 1
    add rsp, 48
    pop rbx
    ret

.create_fail:
    call GetLastError
    mov [lastErrorCode], eax
    xor eax, eax
    add rsp, 48
    pop rbx
    ret
AgenticCreateFile ENDP

; AgenticSearchFiles: Search directory for pattern
; rcx = directory
; rdx = pattern
; r8  = callback (void (*)(const char* filename))
; Returns: number of files found
PUBLIC AgenticSearchFiles
AgenticSearchFiles PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 336           ; WIN32_FIND_DATAA is 320 bytes

    mov rbx, rcx           ; directory
    mov rsi, rdx           ; pattern
    mov rdi, r8            ; callback
    xor r12, r12           ; count

    ; FindFirstFile
    mov rcx, rsi
    lea rdx, [rsp+16]
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je .search_done
    mov r13, rax           ; search handle

.search_loop:
    ; Call callback with filename (at offset 44 in WIN32_FIND_DATAA)
    lea rcx, [rsp+16+44]
    call rdi
    inc r12

    ; FindNextFile
    mov rcx, r13
    lea rdx, [rsp+16]
    call FindNextFileA
    test eax, eax
    jne .search_loop

    ; Close search
    mov rcx, r13
    call FindClose

.search_done:
    mov rax, r12
    add rsp, 336
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
AgenticSearchFiles ENDP

; AgenticGetLastError: Get last error code
; Returns: error code
PUBLIC AgenticGetLastError
AgenticGetLastError PROC
    mov eax, [lastErrorCode]
    ret
AgenticGetLastError ENDP

END
