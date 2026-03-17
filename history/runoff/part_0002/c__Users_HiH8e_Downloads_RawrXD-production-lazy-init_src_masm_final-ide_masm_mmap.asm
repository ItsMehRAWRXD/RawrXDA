;==========================================================================
; masm_mmap.asm - Pure MASM Memory Mapped File Utility
; ==========================================================================
; Replaces memory_mapped_file.cpp.
; Provides high-performance file mapping for GGUF models.
;==========================================================================

option casemap:none

include windows.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN CreateFileA:PROC
EXTERN GetFileSizeEx:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetLastError:PROC
EXTERN console_log:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szMmapOpen      BYTE "Mmap: Mapping file: %s", 0
    szMmapSuccess   BYTE "Mmap: Successfully mapped %d bytes at %p", 0
    szMmapFail      BYTE "Mmap: Failed to map file (Error: %d)", 0

.code

;==========================================================================
; masm_mmap_open(path: rcx, out_size: rdx) -> rax (ptr)
;==========================================================================
PUBLIC masm_mmap_open
PUBLIC mmap_open_file
PUBLIC mmap_map_region
PUBLIC mmap_close

masm_mmap_open PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rsi, rcx        ; path
    mov rdi, rdx        ; out_size
    
    lea rcx, szMmapOpen
    mov rdx, rsi
    call console_log
    
    ; 1. CreateFileA
    mov rcx, rsi
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    xor r9, r9
    mov qword ptr [rsp+32], OPEN_EXISTING
    mov qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+48], 0
    call CreateFileA
    mov rbx, rax        ; rbx = hFile
    
    cmp rbx, INVALID_HANDLE_VALUE
    je fail
    
    ; 2. GetFileSizeEx
    mov rcx, rbx
    lea rdx, [rsp+56]   ; LARGE_INTEGER
    call GetFileSizeEx
    mov rax, [rsp+56]
    mov [rdi], rax      ; Store size
    
    ; 3. CreateFileMappingA
    mov rcx, rbx
    xor rdx, rdx
    mov r8, PAGE_READONLY
    xor r9, r9
    mov qword ptr [rsp+32], 0
    mov qword ptr [rsp+40], 0
    call CreateFileMappingA
    mov rsi, rax        ; rsi = hMapping
    
    test rsi, rsi
    jz fail_close_file
    
    ; 4. MapViewOfFile
    mov rcx, rsi
    mov rdx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call MapViewOfFile
    mov rdi, rax        ; rdi = pData
    
    test rdi, rdi
    jz fail_close_mapping
    
    ; Success
    lea rcx, szMmapSuccess
    mov rdx, [rsp+56]
    mov r8, rdi
    call console_log
    
    ; We can close handles now, mapping stays active
    mov rcx, rsi
    call CloseHandle
    mov rcx, rbx
    call CloseHandle
    
    mov rax, rdi
    jmp mm_exit

fail_close_mapping:
    mov rcx, rsi
    call CloseHandle
fail_close_file:
    mov rcx, rbx
    call CloseHandle
fail:
    call GetLastError
    lea rcx, szMmapFail
    mov rdx, rax
    call console_log
    xor rax, rax
mm_exit:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
masm_mmap_open ENDP

; Thin wrapper to match unified bridge extern
mmap_open_file PROC
    ; rcx = path, rdx = out_size
    jmp masm_mmap_open
mmap_open_file ENDP

; Region map wrapper (reuse full-file map for now)
mmap_map_region PROC
    ; rcx = path, rdx = offset, r8 = length, r9 = out_size ptr
    ; For now, map whole file; offset/length ignored in this shim.
    mov rdx, r9
    jmp masm_mmap_open
mmap_map_region ENDP

; Close wrapper (no-op because MapViewOfFile handle already closed)
mmap_close PROC
    ; rcx = mapped_ptr
    ; Unmap view if present
    test rcx, rcx
    jz mmap_close_done
    mov rdx, rcx
    mov rcx, rcx
    call UnmapViewOfFile
mmap_close_done:
    mov eax, 1
    ret
mmap_close ENDP

END
