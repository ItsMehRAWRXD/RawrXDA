;==========================================================================
; syscall_interface.asm - Zero-Dependency System Call Interface for RawrXD IDE
; ==========================================================================
; Replaces kernel32.lib, user32.lib, gdi32.lib with direct Windows syscalls
; Uses NT system call numbers for Windows 10/11 (x64)
;==========================================================================

option casemap:none

;==========================================================================
; NT SYSTEM CALL NUMBERS (Windows 10/11 x64)
;==========================================================================
NtCreateFile             equ 55h
NtReadFile               equ 6h
NtWriteFile              equ 8h
NtClose                  equ 0Fh
NtCreateSection          equ 4Ah
NtMapViewOfSection       equ 28h
NtUnmapViewOfSection     equ 2Bh
NtAllocateVirtualMemory  equ 18h
NtFreeVirtualMemory      equ 1Ch
NtQuerySystemInformation equ 36h
NtCreateThreadEx         equ 0C7h
NtWaitForSingleObject    equ 4h
NtCreateEvent            equ 1Fh
NtSetEvent               equ 20h
NtResetEvent             equ 21h
NtCreateKeyedEvent       equ 0F9h
NtDelayExecution         equ 24h
NtQueryPerformanceCounter equ 2Fh
NtGetContextThread       equ 0F4h
NtSetContextThread       equ 0F5h

;==========================================================================
; DIRECT SYSTEM CALL MACROS
;==========================================================================

; Generic syscall macro
syscall macro syscall_number
    mov r10, rcx
    mov eax, syscall_number
    syscall
    ret
endm

;==========================================================================
; FILE OPERATIONS (Replaces kernel32.lib)
;==========================================================================

.code

;--------------------------------------------------------------------------
; sys_create_file - Create or open file (replaces CreateFileA)
; rcx = file path, rdx = access, r8 = share, r9 = security
; stack: creation, attributes, template
; Returns: handle in rax
;--------------------------------------------------------------------------
sys_create_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 128
    
    ; Prepare OBJECT_ATTRIBUTES
    lea rax, [rsp + 32]
    mov QWORD PTR [rax], 48           ; Length
    mov QWORD PTR [rax + 8], 0        ; RootDirectory
    mov r10, rcx                      ; ObjectName
    mov QWORD PTR [rax + 16], r10
    mov QWORD PTR [rax + 24], 40h     ; Attributes
    mov QWORD PTR [rax + 32], 0       ; SecurityDescriptor
    mov QWORD PTR [rax + 40], 0       ; SecurityQualityOfService
    
    ; Prepare IO_STATUS_BLOCK
    lea r10, [rsp + 64]
    mov QWORD PTR [r10], 0
    mov QWORD PTR [r10 + 8], 0
    
    ; Call NtCreateFile
    mov rcx, rax                      ; ObjectAttributes
    mov rdx, rdx                      ; DesiredAccess
    mov r8, r10                       ; IoStatusBlock
    mov r9, QWORD PTR [rbp + 48]      ; AllocationSize (0)
    mov r10, QWORD PTR [rbp + 56]     ; FileAttributes
    mov QWORD PTR [rsp + 32], r10
    mov r10, QWORD PTR [rbp + 64]     ; ShareAccess
    mov QWORD PTR [rsp + 40], r10
    mov r10, QWORD PTR [rbp + 72]     ; CreateDisposition
    mov QWORD PTR [rsp + 48], r10
    mov r10, QWORD PTR [rbp + 80]     ; CreateOptions
    mov QWORD PTR [rsp + 56], r10
    mov r10, QWORD PTR [rbp + 88]     ; EaBuffer
    mov QWORD PTR [rsp + 64], r10
    mov r10, QWORD PTR [rbp + 96]     ; EaLength
    mov QWORD PTR [rsp + 72], r10
    
    mov eax, NtCreateFile
    syscall
    
    leave
    ret
sys_create_file ENDP

;--------------------------------------------------------------------------
; sys_read_file - Read from file (replaces ReadFile)
; rcx = handle, rdx = buffer, r8 = length, r9 = bytes_read
; Returns: status in rax
;--------------------------------------------------------------------------
sys_read_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Prepare IO_STATUS_BLOCK
    lea rax, [rsp + 32]
    mov QWORD PTR [rax], 0
    mov QWORD PTR [rax + 8], 0
    
    ; Call NtReadFile
    mov r10, rcx                      ; FileHandle
    mov rcx, r10
    mov rdx, 0                        ; Event (NULL)
    mov r8, 0                         ; ApcRoutine (NULL)
    mov r9, 0                         ; ApcContext (NULL)
    mov r10, rax                      ; IoStatusBlock
    mov QWORD PTR [rsp + 32], r10
    mov r10, rdx                      ; Buffer
    mov QWORD PTR [rsp + 40], r10
    mov r10, r8                       ; Length
    mov QWORD PTR [rsp + 48], r10
    mov r10, 0                        ; ByteOffset (NULL)
    mov QWORD PTR [rsp + 56], r10
    mov r10, r9                       ; Key (NULL)
    mov QWORD PTR [rsp + 64], r10
    
    mov eax, NtReadFile
    syscall
    
    leave
    ret
sys_read_file ENDP

;--------------------------------------------------------------------------
; sys_write_file - Write to file (replaces WriteFile)
; rcx = handle, rdx = buffer, r8 = length, r9 = bytes_written
; Returns: status in rax
;--------------------------------------------------------------------------
sys_write_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Prepare IO_STATUS_BLOCK
    lea rax, [rsp + 32]
    mov QWORD PTR [rax], 0
    mov QWORD PTR [rax + 8], 0
    
    ; Call NtWriteFile
    mov r10, rcx                      ; FileHandle
    mov rcx, r10
    mov rdx, 0                        ; Event (NULL)
    mov r8, 0                         ; ApcRoutine (NULL)
    mov r9, 0                         ; ApcContext (NULL)
    mov r10, rax                      ; IoStatusBlock
    mov QWORD PTR [rsp + 32], r10
    mov r10, rdx                      ; Buffer
    mov QWORD PTR [rsp + 40], r10
    mov r10, r8                       ; Length
    mov QWORD PTR [rsp + 48], r10
    mov r10, 0                        ; ByteOffset (NULL)
    mov QWORD PTR [rsp + 56], r10
    mov r10, r9                       ; Key (NULL)
    mov QWORD PTR [rsp + 64], r10
    
    mov eax, NtWriteFile
    syscall
    
    leave
    ret
sys_write_file ENDP

;--------------------------------------------------------------------------
; sys_close_handle - Close handle (replaces CloseHandle)
; rcx = handle
; Returns: status in rax
;--------------------------------------------------------------------------
sys_close_handle PROC
    mov eax, NtClose
    syscall
sys_close_handle ENDP

;==========================================================================
; MEMORY MANAGEMENT (Replaces VirtualAlloc/VirtualFree)
;==========================================================================

;--------------------------------------------------------------------------
; sys_alloc_memory - Allocate virtual memory (replaces VirtualAlloc)
; rcx = base, rdx = size, r8 = type, r9 = protect
; Returns: base in rax
;--------------------------------------------------------------------------
sys_alloc_memory PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Call NtAllocateVirtualMemory
    mov r10, rcx                      ; ProcessHandle (-1 = current)
    mov rcx, -1
    mov rdx, r10                      ; BaseAddress
    mov r8, 0                         ; ZeroBits
    mov r9, rdx                       ; RegionSize
    mov r10, r8                       ; AllocationType
    mov QWORD PTR [rsp + 32], r10
    mov r10, r9                       ; Protect
    mov QWORD PTR [rsp + 40], r10
    
    mov eax, NtAllocateVirtualMemory
    syscall
    
    leave
    ret
sys_alloc_memory ENDP

;--------------------------------------------------------------------------
; sys_free_memory - Free virtual memory (replaces VirtualFree)
; rcx = base, rdx = size, r8 = type
; Returns: status in rax
;--------------------------------------------------------------------------
sys_free_memory PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Call NtFreeVirtualMemory
    mov r10, rcx                      ; ProcessHandle (-1 = current)
    mov rcx, -1
    mov rdx, r10                      ; BaseAddress
    mov r8, rdx                       ; RegionSize
    mov r9, r8                        ; FreeType
    mov QWORD PTR [rsp + 32], r9
    
    mov eax, NtFreeVirtualMemory
    syscall
    
    leave
    ret
sys_free_memory ENDP

;==========================================================================
; THREADING AND SYNCHRONIZATION
;==========================================================================

;--------------------------------------------------------------------------
; sys_sleep - Sleep for milliseconds (replaces Sleep)
; rcx = milliseconds
;--------------------------------------------------------------------------
sys_sleep PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Convert ms to 100-ns intervals
    mov rax, rcx
    mov rdx, 10000
    mul rdx
    
    ; Prepare timeout
    mov QWORD PTR [rsp + 32], rax     ; LowPart
    mov QWORD PTR [rsp + 40], 0       ; HighPart
    
    ; Call NtDelayExecution
    mov rcx, 0                        ; Alertable
    lea rdx, [rsp + 32]               ; DelayInterval
    
    mov eax, NtDelayExecution
    syscall
    
    leave
    ret
sys_sleep ENDP

;==========================================================================
; DIRECT CONSOLE OUTPUT (Alternative to MessageBox/console)
;==========================================================================

;--------------------------------------------------------------------------
; sys_write_console - Write to console using direct output
; rcx = string, rdx = length
;--------------------------------------------------------------------------
sys_write_console PROC
    push rbp
    mov rbp, rsp
    
    ; Use BIOS interrupt for direct console output
    ; This is a simplified version - in practice would use VGA text buffer
    mov rsi, rcx
    mov rcx, rdx
    
console_write_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz console_write_done
    
    ; Direct VGA text buffer write (mode 3, 80x25)
    mov rdi, 0B8000h                  ; VGA text buffer base
    mov rbx, QWORD PTR [console_cursor]
    mov WORD PTR [rdi + rbx*2], ax    ; Write char + attribute
    inc rbx
    cmp rbx, 2000                     ; 80x25 = 2000 chars
    jl console_cursor_ok
    xor rbx, rbx
console_cursor_ok:
    mov QWORD PTR [console_cursor], rbx
    inc rsi
    loop console_write_loop
    
console_write_done:
    leave
    ret
sys_write_console ENDP

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    console_cursor QWORD 0

;==========================================================================
; EXPORTS
;==========================================================================
PUBLIC sys_create_file
PUBLIC sys_read_file
PUBLIC sys_write_file
PUBLIC sys_close_handle
PUBLIC sys_alloc_memory
PUBLIC sys_free_memory
PUBLIC sys_sleep
PUBLIC sys_write_console

END