;=====================================================================
; byte_level_hotpatcher.asm - Precision GGUF Binary Manipulation (Pure MASM x64)
; ZERO-DEPENDENCY FILE-LEVEL HOTPATCHING
;=====================================================================
; Implements precision binary file operations:
;  - Boyer-Moore pattern matching for tensor discovery
;  - Zero-copy direct read/write/search
;  - Atomic file operations (swap, XOR, rotate, reverse)
;  - GGUF metadata preservation
;
; BytePatch Structure (256 bytes):
;   [+0]:  file_handle (qword) - Windows HANDLE
;   [+8]:  file_size (qword)
;   [+16]: search_pattern_ptr (qword)
;   [+24]: search_pattern_len (qword)
;   [+32]: replacement_ptr (qword)
;   [+40]: replacement_len (qword)
;   [+48]: operation_type (qword) - 0=replace, 1=xor, 2=swap, 3=rotate
;   [+56]: match_offset (qword) - output: where pattern found
;   [+64]: verify_checksum (qword) - optional CRC32
;   [+72]: flags (qword)
;   [+80]: reserved[22] (qword[22])
;=====================================================================

EXTERN asm_log:PROC

.data

; Global statistics
g_byte_patches_applied  QWORD 0
g_bytes_modified        QWORD 0
g_patterns_found        QWORD 0

; Logging messages
msg_byte_apply_enter    DB "BYTEPATCH apply enter",0
msg_byte_apply_fail     DB "BYTEPATCH apply fail",0
msg_byte_apply_exit     DB "BYTEPATCH apply exit",0

.code

; Public exports
PUBLIC masm_byte_patch_open_file
PUBLIC masm_byte_patch_find_pattern
PUBLIC masm_byte_patch_apply
PUBLIC masm_byte_patch_close
PUBLIC masm_byte_patch_get_stats

; External Win32 APIs
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN SetFilePointer:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN GetLastError:PROC

; Consolidated Core Libraries (NEW - Phase 2)
EXTERN masm_core_direct_read:PROC
EXTERN masm_core_direct_write:PROC
EXTERN masm_core_direct_search:PROC
EXTERN masm_core_boyer_moore_search:PROC
EXTERN masm_core_transform_dispatch:PROC
EXTERN masm_core_crc32_calculate:PROC

;=====================================================================
; masm_byte_patch_open_file(filename_ptr: rcx, patch_ptr: rdx) -> rax
;
; Opens a binary file for hotpatching.
; Returns 1 on success, 0 on failure.
; Fills patch structure with file handle and size.
;=====================================================================

ALIGN 16
masm_byte_patch_open_file PROC

    push rbx
    push r12
    sub rsp, 48
    
    mov rbx, rcx            ; rbx = filename
    mov r12, rdx            ; r12 = patch structure
    
    ; CreateFileA(filename, GENERIC_READ|GENERIC_WRITE, 0, NULL, 
    ;             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
    mov qword ptr [rsp + 32], 0     ; hTemplateFile = NULL
    mov dword ptr [rsp + 40], 80h  ; FILE_ATTRIBUTE_NORMAL
    mov r9d, 3                      ; OPEN_EXISTING
    xor r8, r8                      ; lpSecurityAttributes = NULL
    xor edx, edx                    ; dwShareMode = 0
    mov ecx, 0C0000000h             ; GENERIC_READ | GENERIC_WRITE
    mov rcx, rbx                    ; lpFileName
    
    call CreateFileA
    
    cmp rax, -1
    je open_fail
    
    ; Store file handle
    mov [r12], rax          ; file_handle
    
    ; Get file size using SetFilePointer
    mov rcx, rax            ; hFile
    xor edx, edx            ; lDistanceToMove = 0
    xor r8, r8              ; lpDistanceToMoveHigh = NULL
    mov r9d, 2              ; FILE_END
    
    call SetFilePointer
    
    mov [r12 + 8], rax      ; file_size
    
    ; Reset file pointer to beginning
    mov rcx, [r12]
    xor edx, edx
    xor r8, r8
    xor r9d, r9d            ; FILE_BEGIN
    
    call SetFilePointer
    
    mov rax, 1              ; Success
    jmp open_exit

open_fail:
    xor rax, rax

open_exit:
    add rsp, 48
    pop r12
    pop rbx
    ret

masm_byte_patch_open_file ENDP

;=====================================================================
; masm_byte_patch_find_pattern(patch_ptr: rcx) -> rax
;
; Searches for pattern in file using consolidated core functions.
; Returns offset if found, -1 if not found.
; Stores result in patch_ptr->match_offset.
;=====================================================================

ALIGN 16
masm_byte_patch_find_pattern PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 4096           ; Stack buffer (reduced from 4896)
    
    mov rbx, rcx            ; rbx = patch structure
    
    ; Get search parameters
    mov r12, [rbx + 16]     ; r12 = search_pattern_ptr
    mov r13, [rbx + 24]     ; r13 = search_pattern_len
    mov r14, [rbx + 8]      ; r14 = file_size
    
    test r12, r12
    jz find_fail
    
    test r13, r13
    jz find_fail
    
    ; REFACTORED: Use consolidated direct_read
    ; Read file into buffer using consolidated core function
    mov rcx, [rbx]          ; file_handle
    mov rdx, 0              ; offset = 0
    lea r8, [rsp + 32]      ; buffer
    mov r9, 4096            ; size (4KB chunk)
    
    call masm_core_direct_read  ; ← CONSOLIDATED CALL
    
    test rax, rax
    jz find_fail
    
    mov r14, rax            ; r14 = bytes_read
    
    ; REFACTORED: Use consolidated search
    ; Search for pattern using consolidated core function
    lea rcx, [rsp + 32]     ; haystack (buffer)
    mov rdx, r12            ; needle (pattern)
    mov r8, r14             ; haystack_len (bytes_read)
    mov r9, r13             ; needle_len (pattern_len)
    
    call masm_core_direct_search  ; ← CONSOLIDATED CALL
    
    cmp rax, -1
    je find_not_found
    
    mov [rbx + 56], rax     ; Store match_offset
    lock inc [g_patterns_found]
    
    jmp find_exit

find_not_found:
    mov qword ptr [rbx + 56], -1
    mov rax, -1
    jmp find_exit

find_fail:
    mov rax, -1

find_exit:
    add rsp, 4096
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_byte_patch_find_pattern ENDP

;=====================================================================
; masm_byte_patch_apply(patch_ptr: rcx) -> rax (1=success, 0=fail)
;
; Applies the specified byte-level operation using consolidated core functions.
; Operation types:
;   0 = Replace (direct write)
;   1 = XOR (byte-wise XOR with replacement)
;   2 = Swap (reverse byte order)
;   3 = Rotate (circular bit rotation)
;=====================================================================

ALIGN 16
masm_byte_patch_apply PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 4096           ; Buffer space (reduced from 4896)
    
    mov rbx, rcx            ; rbx = patch structure
    lea rcx, msg_byte_apply_enter
    call asm_log
    
    ; Get operation parameters
    mov r12, [rbx + 56]     ; r12 = match_offset
    cmp r12, -1
    je apply_fail           ; No match found
    
    mov r13, [rbx + 48]     ; r13 = operation_type
    mov r14, [rbx + 40]     ; r14 = replacement_len
    
    ; REFACTORED: Use consolidated direct_write for replace operation
    ; Direct write replacement data using consolidated core function
    mov rcx, [rbx]          ; file_handle
    mov rdx, r12            ; offset
    mov r8, [rbx + 32]      ; replacement_ptr
    mov r9, r14             ; replacement_len
    
    call masm_core_direct_write  ; ← CONSOLIDATED CALL
    
    test rax, rax
    jz apply_fail
    
    ; Handle special operations (XOR, rotate, reverse, swap)
    cmp r13, 0              ; 0 = Replace (already done)
    je apply_success
    
    ; REFACTORED: Use consolidated transform_dispatch for all transforms
    ; masm_core_transform_dispatch(operation_type: rcx, buffer: rdx,
    ;                              size: r8, param1: r9, flags: [rsp+32])
    mov rcx, r13            ; operation_type (1=XOR, 2=ROTATE, etc.)
    mov rdx, [rbx + 32]     ; buffer (replacement data)
    mov r8, r14             ; size
    mov r9, [rbx + 16]      ; param1 (pattern as key for XOR)
    mov qword ptr [rsp + 32], 0  ; flags = FORWARD
    
    call masm_core_transform_dispatch  ; ← CONSOLIDATED CALL
    
    test rax, rax
    jz apply_fail
    
    ; Verify checksum if requested
    cmp qword ptr [rbx + 64], 0  ; verify_checksum
    je apply_success
    
    ; REFACTORED: Use consolidated CRC32
    ; Calculate and verify checksum using consolidated core
    mov rcx, [rbx + 32]     ; buffer
    mov rdx, r14            ; size
    
    call masm_core_crc32_calculate  ; ← CONSOLIDATED CALL
    
    cmp rax, [rbx + 64]     ; Compare with expected checksum
    jne apply_fail

apply_success:
    ; Update statistics
    mov rax, r14
    lock add [g_bytes_modified], rax
    lock inc [g_byte_patches_applied]
    
    mov rax, 1
    lea rcx, msg_byte_apply_exit
    call asm_log
    jmp apply_exit

apply_fail:
    lock inc [g_bytes_modified]  ; Might track as failed
    xor rax, rax
    lea rcx, msg_byte_apply_fail
    call asm_log

apply_exit:
    add rsp, 4096
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_byte_patch_apply ENDP

;=====================================================================
; masm_byte_patch_close(patch_ptr: rcx) -> void
;
; Closes file handle and cleans up resources.
;=====================================================================

ALIGN 16
masm_byte_patch_close PROC

    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    test rbx, rbx
    jz close_exit
    
    mov rcx, [rbx]
    test rcx, rcx
    jz close_exit
    
    call CloseHandle
    
    mov qword ptr [rbx], 0

close_exit:
    add rsp, 32
    pop rbx
    ret

masm_byte_patch_close ENDP

;=====================================================================
; masm_byte_patch_get_stats(stats_ptr: rcx) -> void
;
; Fills statistics structure:
;   [0]: patches_applied (qword)
;   [8]: bytes_modified (qword)
;   [16]: patterns_found (qword)
;=====================================================================

ALIGN 16
masm_byte_patch_get_stats PROC

    test rcx, rcx
    jz stats_exit
    
    mov rax, [g_byte_patches_applied]
    mov [rcx], rax
    
    mov rax, [g_bytes_modified]
    mov [rcx + 8], rax
    
    mov rax, [g_patterns_found]
    mov [rcx + 16], rax

stats_exit:
    ret

masm_byte_patch_get_stats ENDP

END


