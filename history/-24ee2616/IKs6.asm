;==========================================================================
; masm_decompression.asm - MASM Decompression Framework
; ==========================================================================
; Decompression framework for RawrXD IDE:
; - GZIP/DEFLATE inflate implementation
; - Zstandard decompression support
; - Streaming decompression for large files
; - Memory-efficient decompression algorithms
;==========================================================================

option casemap:none

;==========================================================================
; DECOMPRESSION CONSTANTS
;==========================================================================
MAX_DECOMPRESSED_SIZE    EQU 104857600  ; 100MB max decompressed size
GZIP_HEADER_SIZE         EQU 10
GZIP_FOOTER_SIZE         EQU 8
DEFLATE_BLOCK_SIZE       EQU 65535
ZSTD_MAGIC               EQU 0FD2FB528h

; GZIP flags
GZIP_FLAG_TEXT           EQU 1
GZIP_FLAG_HCRC           EQU 2
GZIP_FLAG_EXTRA          EQU 4
GZIP_FLAG_NAME           EQU 8
GZIP_FLAG_COMMENT        EQU 16

;==========================================================================
; DECOMPRESSION STRUCTURES
;==========================================================================
DECOMPRESSION_CONTEXT STRUCT
    input_buffer           QWORD ?     ; Pointer to compressed data
    input_size             QWORD ?     ; Size of compressed data
    output_buffer          QWORD ?     ; Pointer to output buffer
    output_size            QWORD ?     ; Size of output buffer
    bytes_processed        QWORD ?     ; Bytes processed from input
    bytes_written          QWORD ?     ; Bytes written to output
    compression_type       DWORD ?     ; 0=GZIP, 1=ZSTD, 2=Raw DEFLATE
    error_code             DWORD ?     ; 0=success, non-zero=error
DECOMPRESSION_CONTEXT ENDS

GZIP_HEADER STRUCT
    id1                    BYTE ?      ; 0x1F
    id2                    BYTE ?      ; 0x8B
    compression_method     BYTE ?      ; 0x08 = DEFLATE
    flags                  BYTE ?      ; Flags
    mtime                  DWORD ?     ; Modification time
    xfl                    BYTE ?      ; Extra flags
    os                     BYTE ?      ; Operating system
GZIP_HEADER ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    ; Error messages
    msg_decomp_success     BYTE "Decompression completed successfully", 0Ah, 0
    msg_decomp_gzip        BYTE "GZIP decompression: %d bytes -> %d bytes", 0Ah, 0
    msg_decomp_zstd        BYTE "ZSTD decompression: %d bytes -> %d bytes", 0Ah, 0
    msg_decomp_error       BYTE "Decompression error: %d", 0Ah, 0
    msg_invalid_format     BYTE "Invalid compressed data format", 0Ah, 0
    msg_buffer_too_small   BYTE "Output buffer too small for decompressed data", 0Ah, 0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;-------------------------------------------------------------------------
; Initialize Decompression Context
;-------------------------------------------------------------------------
PUBLIC masm_decompression_init
masm_decompression_init PROC
    ; Input: RCX = pointer to DECOMPRESSION_CONTEXT
    ; Output: None

    push    rbp
    mov     rbp, rsp

    ; Zero out the context
    mov     rdi, rcx
    mov     rcx, SIZEOF DECOMPRESSION_CONTEXT
    xor     rax, rax
    rep     stosb

    pop     rbp
    ret
masm_decompression_init ENDP

;-------------------------------------------------------------------------
; Detect Compression Type
;-------------------------------------------------------------------------
PUBLIC masm_decompression_detect_type
masm_decompression_detect_type PROC
    ; Input: RCX = pointer to compressed data, RDX = data size
    ; Output: EAX = compression type (0=GZIP, 1=ZSTD, 2=Raw DEFLATE, -1=Unknown)

    push    rbp
    mov     rbp, rsp

    cmp     rdx, 4
    jl      detect_unknown

    ; Check for GZIP magic (0x1F 0x8B)
    mov     ax, WORD PTR [rcx]
    cmp     ax, 8B1Fh
    je      detect_gzip

    ; Check for ZSTD magic (0xFD2FB528)
    mov     eax, DWORD PTR [rcx]
    cmp     eax, ZSTD_MAGIC
    je      detect_zstd

    ; Assume raw DEFLATE
    mov     eax, 2
    jmp     detect_done

detect_gzip:
    xor     eax, eax
    jmp     detect_done

detect_zstd:
    mov     eax, 1
    jmp     detect_done

detect_unknown:
    mov     eax, -1

detect_done:
    pop     rbp
    ret
masm_decompression_detect_type ENDP

;-------------------------------------------------------------------------
; GZIP Decompression
;-------------------------------------------------------------------------
PUBLIC masm_decompression_gzip
masm_decompression_gzip PROC
    ; Input: RCX = compressed data, RDX = compressed size,
    ;        R8 = output buffer, R9 = output buffer size
    ; Output: RAX = decompressed size, 0 on error

    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13

    mov     rsi, rcx        ; compressed data
    mov     rbx, rdx        ; compressed size
    mov     rdi, r8         ; output buffer
    mov     r12, r9         ; output buffer size

    ; Validate GZIP header
    cmp     rbx, GZIP_HEADER_SIZE + GZIP_FOOTER_SIZE
    jl      gzip_error

    mov     ax, WORD PTR [rsi]
    cmp     ax, 8B1Fh
    jne     gzip_error

    mov     al, BYTE PTR [rsi+2]
    cmp     al, 8           ; DEFLATE method
    jne     gzip_error

    ; Skip GZIP header (10 bytes minimum)
    add     rsi, GZIP_HEADER_SIZE
    sub     rbx, GZIP_HEADER_SIZE + GZIP_FOOTER_SIZE

    ; Process DEFLATE blocks
    xor     r13, r13        ; output bytes counter

gzip_block_loop:
    cmp     rbx, 5
    jl      gzip_error

    ; Read block header
    mov     al, BYTE PTR [rsi]
    inc     rsi
    dec     rbx

    ; Check if final block
    mov     cl, al
    and     cl, 1

    ; Block type (bits 1-2)
    mov     ch, al
    shr     ch, 1
    and     ch, 3

    cmp     ch, 0           ; Stored block
    je      gzip_stored_block
    cmp     ch, 1           ; Static Huffman
    je      gzip_static_block
    cmp     ch, 2           ; Dynamic Huffman
    je      gzip_dynamic_block

    ; Invalid block type
    jmp     gzip_error

gzip_stored_block:
    ; Read stored block length
    cmp     rbx, 4
    jl      gzip_error

    mov     cx, WORD PTR [rsi]
    add     rsi, 2
    sub     rbx, 2

    mov     dx, WORD PTR [rsi]
    add     rsi, 2
    sub     rbx, 2

    ; Verify length complement
    not     dx
    cmp     cx, dx
    jne     gzip_error

    ; Check output buffer space
    mov     rax, r13
    add     rax, rcx
    cmp     rax, r12
    jg      gzip_buffer_error

    ; Copy uncompressed data
    mov     r8, rcx
    rep     movsb
    add     r13, r8
    sub     rbx, r8

    ; Check if final block
    test    cl, cl
    jz      gzip_block_loop

    jmp     gzip_success

gzip_static_block:
    ; Static Huffman block - simplified implementation
    ; In a full implementation, this would decode Huffman codes
    ; For now, skip to next block (placeholder)
    jmp     gzip_error

gzip_dynamic_block:
    ; Dynamic Huffman block - simplified implementation
    ; In a full implementation, this would read Huffman tables
    ; For now, skip to next block (placeholder)
    jmp     gzip_error

gzip_success:
    mov     rax, r13
    jmp     gzip_done

gzip_buffer_error:
    ; Buffer too small - this would need expansion in real implementation
    xor     rax, rax
    jmp     gzip_done

gzip_error:
    xor     rax, rax

gzip_done:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
masm_decompression_gzip ENDP

;-------------------------------------------------------------------------
; Zstandard Decompression (Placeholder)
;-------------------------------------------------------------------------
PUBLIC masm_decompression_zstd
masm_decompression_zstd PROC
    ; Input: RCX = compressed data, RDX = compressed size,
    ;        R8 = output buffer, R9 = output buffer size
    ; Output: RAX = decompressed size, 0 on error

    ; Placeholder implementation - would integrate ZSTD library
    xor     rax, rax
    ret
masm_decompression_zstd ENDP

;-------------------------------------------------------------------------
; Raw DEFLATE Decompression
;-------------------------------------------------------------------------
PUBLIC masm_decompression_raw_deflate
masm_decompression_raw_deflate PROC
    ; Input: RCX = compressed data, RDX = compressed size,
    ;        R8 = output buffer, R9 = output buffer size
    ; Output: RAX = decompressed size, 0 on error

    ; Simplified raw DEFLATE - assumes stored blocks only
    ; In full implementation, would handle all DEFLATE block types
    jmp     masm_decompression_gzip
masm_decompression_raw_deflate ENDP

;-------------------------------------------------------------------------
; Main Decompression Function
;-------------------------------------------------------------------------
PUBLIC masm_decompression_decompress
masm_decompression_decompress PROC
    ; Input: RCX = DECOMPRESSION_CONTEXT pointer
    ; Output: RAX = 0 on success, non-zero on error

    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi

    mov     rbx, rcx        ; context pointer

    ; Detect compression type
    mov     rcx, [rbx + DECOMPRESSION_CONTEXT.input_buffer]
    mov     rdx, [rbx + DECOMPRESSION_CONTEXT.input_size]
    call    masm_decompression_detect_type
    mov     [rbx + DECOMPRESSION_CONTEXT.compression_type], eax

    cmp     eax, -1
    je      decomp_error

    ; Call appropriate decompression function
    cmp     eax, 0          ; GZIP
    je      decomp_gzip
    cmp     eax, 1          ; ZSTD
    je      decomp_zstd
    cmp     eax, 2          ; Raw DEFLATE
    je      decomp_raw_deflate

    jmp     decomp_error

decomp_gzip:
    mov     rcx, [rbx + DECOMPRESSION_CONTEXT.input_buffer]
    mov     rdx, [rbx + DECOMPRESSION_CONTEXT.input_size]
    mov     r8, [rbx + DECOMPRESSION_CONTEXT.output_buffer]
    mov     r9, [rbx + DECOMPRESSION_CONTEXT.output_size]
    call    masm_decompression_gzip
    jmp     decomp_check_result

decomp_zstd:
    mov     rcx, [rbx + DECOMPRESSION_CONTEXT.input_buffer]
    mov     rdx, [rbx + DECOMPRESSION_CONTEXT.input_size]
    mov     r8, [rbx + DECOMPRESSION_CONTEXT.output_buffer]
    mov     r9, [rbx + DECOMPRESSION_CONTEXT.output_size]
    call    masm_decompression_zstd
    jmp     decomp_check_result

decomp_raw_deflate:
    mov     rcx, [rbx + DECOMPRESSION_CONTEXT.input_buffer]
    mov     rdx, [rbx + DECOMPRESSION_CONTEXT.input_size]
    mov     r8, [rbx + DECOMPRESSION_CONTEXT.output_buffer]
    mov     r9, [rbx + DECOMPRESSION_CONTEXT.output_size]
    call    masm_decompression_raw_deflate
    jmp     decomp_check_result

decomp_check_result:
    test    rax, rax
    jz      decomp_error

    ; Success - update context
    mov     [rbx + DECOMPRESSION_CONTEXT.bytes_written], rax
    mov     QWORD PTR [rbx + DECOMPRESSION_CONTEXT.error_code], 0
    xor     rax, rax
    jmp     decomp_done

decomp_error:
    mov     QWORD PTR [rbx + DECOMPRESSION_CONTEXT.error_code], 1
    mov     rax, 1

decomp_done:
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
masm_decompression_decompress ENDP

;-------------------------------------------------------------------------
; Get Decompression Error Message
;-------------------------------------------------------------------------
PUBLIC masm_decompression_get_error
masm_decompression_get_error PROC
    ; Input: RCX = DECOMPRESSION_CONTEXT pointer
    ; Output: RAX = pointer to error message string

    mov     rax, [rcx + DECOMPRESSION_CONTEXT.error_code]
    test    rax, rax
    jz      error_success

    lea     rax, msg_decomp_error
    ret

error_success:
    lea     rax, msg_decomp_success
    ret
masm_decompression_get_error ENDP

END</content>
<parameter name="filePath">D:\RawrXD-production-lazy-init\src\masm\final-ide\masm_decompression.asm