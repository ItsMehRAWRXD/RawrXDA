; ============================================================================
; Workflow Persistence MASM Exports
; RawrXD 14-Day Production Expansion - Phase 1 Agent Polish
; 
; Provides x64 assembly-optimized routines for:
; - High-speed state serialization
; - Memory-mapped I/O operations
; - Hash computation (FNV-1a)
; - Compression/decompression primitives
; ============================================================================

.686p
.xmm
.model flat, c
.option casemap:none

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================

; Enhancement 1: Compression
public workflow_compress_state
public workflow_decompress_state

; Enhancement 2: Diffing
public workflow_compute_diff
public workflow_apply_diff

; Enhancement 3: Memory Mapping
public workflow_mmap_file
public workflow_munmap_file

; Enhancement 4: Hashing
public workflow_fnv1a_hash
public workflow_verify_integrity

; Enhancement 5: Serialization
public workflow_serialize_json
public workflow_deserialize_json

; Enhancement 6: Session Management
public workflow_save_session
public workflow_load_session

; Enhancement 7: Checkpoint Operations
public workflow_create_checkpoint
public workflow_restore_checkpoint

; Enhancement 8: Async I/O
public workflow_async_write
public workflow_flush_wal

; ============================================================================
; CONSTANTS
; ============================================================================

FNV_OFFSET_BASIS equ 14695981039346656037
FNV_PRIME        equ 1099511628211

; Windows API constants
FILE_MAP_READ      equ 000000004h
FILE_MAP_WRITE     equ 000000002h
PAGE_READONLY      equ 000000002h
GENERIC_READ       equ 80000000h
GENERIC_WRITE      equ 40000000h
OPEN_EXISTING      equ 000000003h
CREATE_ALWAYS      equ 000000002h
FILE_ATTRIBUTE_NORMAL equ 000000080h
INVALID_HANDLE_VALUE equ -1

; ============================================================================
; DATA SECTION
; ============================================================================

.data

; Compression lookup tables
align 16
run_length_table db 256 dup(0)  ; Run-length encoding table

; Hash state
align 8
fnv_state dq 0

; Memory map tracking
MAX_MAPPED_FILES equ 64
mapped_handles dq MAX_MAPPED_FILES dup(0)  ; File handles
mapped_views   dq MAX_MAPPED_FILES dup(0)  ; Mapped views
mapped_sizes   dq MAX_MAPPED_FILES dup(0)  ; View sizes

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; Enhancement 1: Compression Primitives
; ============================================================================

; ----------------------------------------------------------------------------
; workflow_compress_state
;   Compresses state data using run-length encoding
;   RCX = input buffer
;   RDX = input length
;   R8  = output buffer
;   R9  = output buffer size
;   Returns: compressed length in RAX, 0 if output too small
; ----------------------------------------------------------------------------
workflow_compress_state proc frame
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .endprolog
    
    mov rsi, rcx        ; RSI = input
    mov r12, rdx        ; R12 = input length
    mov rdi, r8         ; RDI = output
    mov r13, r9         ; R13 = output size
    
    xor rbx, rbx        ; RBX = output position
    xor rcx, rcx        ; RCX = input position
    
compress_loop:
    cmp rcx, r12
    jge compress_done
    
    ; Load current byte
    movzx eax, byte ptr [rsi + rcx]
    mov r8d, eax        ; R8D = current byte
    mov r9, rcx         ; R9 = run start
    inc rcx
    
    ; Count run length (max 255)
    xor edx, edx        ; EDX = run length - 1
run_count_loop:
    cmp rcx, r12
    jge run_count_done
    cmp byte ptr [rsi + rcx], al
    jne run_count_done
    inc edx
    cmp edx, 254        ; Max run - 1
    jge run_count_done
    inc rcx
    jmp run_count_loop
run_count_done:
    inc edx             ; EDX = actual run length
    
    ; Decide: encode run or literal
    cmp edx, 4
    jl literal_encode
    
    ; Encode run: 0x00 + byte + count
    cmp rbx, r13
    jge output_overflow
    mov byte ptr [rdi + rbx], 0
    inc rbx
    
    cmp rbx, r13
    jge output_overflow
    mov byte ptr [rdi + rbx], r8b
    inc rbx
    
    cmp rbx, r13
    jge output_overflow
    mov byte ptr [rdi + rbx], dl
    inc rbx
    
    jmp compress_loop
    
literal_encode:
    ; Output literal bytes
    mov r10, r9         ; R10 = source position
literal_loop:
    cmp rbx, r13
    jge output_overflow
    movzx eax, byte ptr [rsi + r10]
    mov byte ptr [rdi + rbx], al
    inc rbx
    inc r10
    dec edx
    jnz literal_loop
    jmp compress_loop
    
compress_done:
    mov rax, rbx        ; Return output length
    jmp compress_exit
    
output_overflow:
    xor rax, rax        ; Return 0 (failure)
    
compress_exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
workflow_compress_state endp

; ----------------------------------------------------------------------------
; workflow_decompress_state
;   Decompresses RLE-compressed state data
;   RCX = input buffer
;   RDX = input length
;   R8  = output buffer
;   R9  = output buffer size
;   Returns: decompressed length in RAX, 0 if output too small
; ----------------------------------------------------------------------------
workflow_decompress_state proc frame
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .endprolog
    
    mov rsi, rcx        ; RSI = input
    mov r12, rdx        ; R12 = input length
    mov rdi, r8         ; RDI = output
    mov r13, r9         ; R13 = output size
    
    xor rbx, rbx        ; RBX = output position
    xor rcx, rcx        ; RCX = input position
    
decompress_loop:
    cmp rcx, r12
    jge decompress_done
    
    movzx eax, byte ptr [rsi + rcx]
    inc rcx
    
    test al, al
    jnz literal_output
    
    ; Run encoding: 0x00 + byte + count
    cmp rcx, r12
    jge decompress_done
    movzx r8d, byte ptr [rsi + rcx]    ; R8D = byte to repeat
    inc rcx
    
    cmp rcx, r12
    jge decompress_done
    movzx edx, byte ptr [rsi + rcx]    ; EDX = count
    inc rcx
    
    ; Output run
    mov r9, rbx
    add r9, rdx
    cmp r9, r13
    jg output_overflow_decomp
    
    mov eax, r8d
    push rcx
    mov rcx, rdx
    rep stosb
    pop rcx
    mov rdi, r8
    add rbx, rdx
    jmp decompress_loop
    
literal_output:
    cmp rbx, r13
    jge output_overflow_decomp
    mov byte ptr [rdi + rbx], al
    inc rbx
    jmp decompress_loop
    
decompress_done:
    mov rax, rbx
    jmp decompress_exit
    
output_overflow_decomp:
    xor rax, rax
    
decompress_exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
workflow_decompress_state endp

; ============================================================================
; Enhancement 2: Diff Computation
; ============================================================================

; ----------------------------------------------------------------------------
; workflow_compute_diff
;   Computes difference between two state buffers
;   RCX = base state buffer
;   RDX = base state length
;   R8  = current state buffer
;   R9  = current state length
;   [RSP+40] = diff output buffer
;   [RSP+48] = diff buffer size
;   Returns: diff length in RAX
; ----------------------------------------------------------------------------
workflow_compute_diff proc frame
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .pushreg r14
    .endprolog
    
    mov rsi, rcx        ; RSI = base
    mov r12, rdx        ; R12 = base length
    mov rdi, r8         ; RDI = current
    mov r13, r9         ; R13 = current length
    mov r14, [rsp + 64] ; R14 = diff output
    mov rbx, [rsp + 72] ; RBX = diff buffer size (reused as limit)
    
    ; Simple byte-level diff (production would use structured JSON diff)
    xor rcx, rcx        ; Position
    xor r9, r9          ; Diff output position
    
    ; Find minimum length
    mov r8, r12
    cmp r8, r13
    cmova r8, r13       ; R8 = min length
    
diff_loop:
    cmp rcx, r8
    jge diff_done
    
    movzx eax, byte ptr [rsi + rcx]
    movzx edx, byte ptr [rdi + rcx]
    cmp al, dl
    je diff_next
    
    ; Found difference - record position and values
    cmp r9, rbx
    jge diff_overflow
    
    mov word ptr [r14 + r9], cx        ; Position
    mov byte ptr [r14 + r9 + 2], dl    ; New value
    add r9, 3
    
diff_next:
    inc rcx
    jmp diff_loop
    
diff_done:
    mov rax, r9
diff_exit:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
    
diff_overflow:
    xor rax, rax
    jmp diff_exit
workflow_compute_diff endp

; ----------------------------------------------------------------------------
; workflow_apply_diff
;   Applies diff to base state
;   RCX = base state buffer
;   RDX = base state length
;   R8  = diff buffer
;   R9  = diff length
;   [RSP+40] = output buffer
;   [RSP+48] = output buffer size
; ----------------------------------------------------------------------------
workflow_apply_diff proc frame
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .endprolog
    
    mov rsi, rcx        ; RSI = base
    mov r12, rdx        ; R12 = base length
    mov rdi, r8         ; RDI = diff
    mov r13, r9         ; R13 = diff length
    mov rbx, [rsp + 56] ; RBX = output
    
    ; Copy base to output first
    mov rcx, r12
    mov rdx, rbx
    mov r8, rsi
    rep movsb
    
    ; Apply diffs
    xor rcx, rcx        ; Diff position
diff_apply_loop:
    cmp rcx, r13
    jge apply_done
    
    ; Read diff entry: position (2 bytes) + value (1 byte)
    movzx eax, word ptr [rdi + rcx]
    movzx edx, byte ptr [rdi + rcx + 2]
    
    cmp rax, r12
    jae apply_next      ; Skip if out of bounds
    
    mov byte ptr [rbx + rax], dl
    
apply_next:
    add rcx, 3
    jmp diff_apply_loop
    
apply_done:
    mov rax, r12        ; Return output length
    
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
workflow_apply_diff endp

; ============================================================================
; Enhancement 4: FNV-1a Hash
; ============================================================================

; ----------------------------------------------------------------------------
; workflow_fnv1a_hash
;   Computes FNV-1a hash of data
;   RCX = data buffer
;   RDX = data length
;   Returns: 64-bit hash in RAX
; ----------------------------------------------------------------------------
workflow_fnv1a_hash proc frame
    push rsi
    .pushreg rsi
    .endprolog
    
    mov rsi, rcx
    mov rcx, rdx
    
    mov rax, FNV_OFFSET_BASIS
    
    test rcx, rcx
    jz hash_done
    
hash_loop:
    movzx edx, byte ptr [rsi]
    xor rax, rdx
    mov r8, FNV_PRIME
    mul r8              ; RAX = RAX * FNV_PRIME
    inc rsi
    dec rcx
    jnz hash_loop
    
hash_done:
    pop rsi
    ret
workflow_fnv1a_hash endp

; ----------------------------------------------------------------------------
; workflow_verify_integrity
;   Verifies data integrity against stored hash
;   RCX = data buffer
;   RDX = data length
;   R8  = expected hash
;   Returns: 1 if valid, 0 if corrupt
; ----------------------------------------------------------------------------
workflow_verify_integrity proc frame
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, r8         ; Save expected hash
    
    ; Compute actual hash
    call workflow_fnv1a_hash
    
    ; Compare
    cmp rax, rbx
    sete al
    movzx eax, al
    
    pop rbx
    ret
workflow_verify_integrity endp

; ============================================================================
; Enhancement 3: Memory Mapping (Windows-specific)
; ============================================================================

; ----------------------------------------------------------------------------
; workflow_mmap_file
;   Memory-maps a file for fast access
;   RCX = file path (UTF-16)
;   RDX = access mode (0=read, 1=write)
;   Returns: handle in RAX, 0 on failure
; ----------------------------------------------------------------------------
workflow_mmap_file proc frame
    push rbx
    push rsi
    push rdi
    push r12
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .endprolog
    
    ; Find free slot
    xor rbx, rbx
find_slot:
    cmp rbx, MAX_MAPPED_FILES
    jge mmap_fail
    cmp qword ptr [mapped_handles + rbx*8], 0
    je slot_found
    inc rbx
    jmp find_slot
slot_found:
    mov r12, rbx        ; R12 = slot index
    
    ; Call CreateFileW
    mov rdx, GENERIC_READ
    xor r8, r8          ; No sharing
    xor r9, r9          ; No security
    mov qword ptr [rsp + 32], OPEN_EXISTING
    mov qword ptr [rsp + 40], FILE_ATTRIBUTE_NORMAL
    xor rax, rax
    mov qword ptr [rsp + 48], rax
    
    ; Note: Would need actual Windows API imports
    ; This is a stub for the export table
    
    mov rax, r12        ; Return slot index as handle
    inc rax             ; Make it 1-based
    jmp mmap_exit
    
mmap_fail:
    xor rax, rax
    
mmap_exit:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
workflow_mmap_file endp

; ----------------------------------------------------------------------------
; workflow_munmap_file
;   Unmaps a previously mapped file
;   RCX = handle (slot index + 1)
; ----------------------------------------------------------------------------
workflow_munmap_file proc frame
    .endprolog
    
    dec rcx             ; Convert to 0-based index
    cmp rcx, MAX_MAPPED_FILES
    jae munmap_done
    
    mov qword ptr [mapped_handles + rcx*8], 0
    mov qword ptr [mapped_views + rcx*8], 0
    mov qword ptr [mapped_sizes + rcx*8], 0
    
munmap_done:
    xor eax, eax
    ret
workflow_munmap_file endp

; ============================================================================
; Enhancement 5-8: Placeholder Exports
; These would be implemented with full functionality
; ============================================================================

workflow_serialize_json proc frame
    .endprolog
    mov rax, rcx        ; Stub: return input
    ret
workflow_serialize_json endp

workflow_deserialize_json proc frame
    .endprolog
    mov rax, rcx        ; Stub: return input
    ret
workflow_deserialize_json endp

workflow_save_session proc frame
    .endprolog
    mov eax, 1          ; Stub: return success
    ret
workflow_save_session endp

workflow_load_session proc frame
    .endprolog
    mov eax, 1          ; Stub: return success
    ret
workflow_load_session endp

workflow_create_checkpoint proc frame
    .endprolog
    mov rax, rcx        ; Stub: return input
    ret
workflow_create_checkpoint endp

workflow_restore_checkpoint proc frame
    .endprolog
    mov rax, rcx        ; Stub: return input
    ret
workflow_restore_checkpoint endp

workflow_async_write proc frame
    .endprolog
    mov eax, 1          ; Stub: return success
    ret
workflow_async_write endp

workflow_flush_wal proc frame
    .endprolog
    mov eax, 1          ; Stub: return success
    ret
workflow_flush_wal endp

; ============================================================================
; END
; ============================================================================

end
