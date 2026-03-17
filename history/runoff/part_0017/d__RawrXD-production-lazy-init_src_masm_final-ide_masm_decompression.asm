;==========================================================================
; masm_decompression.asm - MASM Decompression Framework
; ==========================================================================

option casemap:none

;==========================================================================
; DECOMPRESSION CONSTANTS
;==========================================================================
MAX_DECOMPRESSED_SIZE    EQU 104857600  ; 100MB max decompressed size
GZIP_HEADER_SIZE         EQU 10
GZIP_FOOTER_SIZE         EQU 8

;==========================================================================
; DECOMPRESSION STRUCTURES
;==========================================================================
DECOMPRESSION_CONTEXT STRUCT
    input_buffer           QWORD ?
    input_size             QWORD ?
    output_buffer          QWORD ?
    output_size            QWORD ?
    bytes_processed        QWORD ?
    bytes_written          QWORD ?
    compression_type       DWORD ?
    error_code             DWORD ?
DECOMPRESSION_CONTEXT ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    msg_decomp_success     BYTE "Decompression completed successfully", 0Ah, 0
    msg_decomp_error       BYTE "Decompression error: %d", 0Ah, 0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;-------------------------------------------------------------------------
; Initialize Decompression Context
;-------------------------------------------------------------------------
PUBLIC masm_decompression_init
masm_decompression_init PROC
    push    rbp
    mov     rbp, rsp

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
    cmp     rdx, 4
    jl      detect_unknown

    mov     ax, WORD PTR [rcx]
    cmp     ax, 8B1Fh
    je      detect_gzip

    mov     eax, 2
    jmp     detect_done

detect_gzip:
    xor     eax, eax
    jmp     detect_done

detect_unknown:
    mov     eax, -1

detect_done:
    ret
masm_decompression_detect_type ENDP

;-------------------------------------------------------------------------
; GZIP Decompression
;-------------------------------------------------------------------------
PUBLIC masm_decompression_gzip
masm_decompression_gzip PROC
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

    ; Skip GZIP header
    add     rsi, GZIP_HEADER_SIZE
    sub     rbx, GZIP_HEADER_SIZE + GZIP_FOOTER_SIZE

    ; Process DEFLATE stored blocks (simplified)
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

    ; Block type (must be stored for this simple implementation)
    mov     ch, al
    shr     ch, 1
    and     ch, 3

    cmp     ch, 0           ; Stored block
    jne     gzip_error

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
    jg      gzip_error

    ; Copy uncompressed data
    mov     r8, rcx
    rep     movsb
    add     r13, r8
    sub     rbx, r8

    ; Check if final block
    test    cl, cl
    jz      gzip_block_loop

    ; Success
    mov     rax, r13
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
; Main Decompression Function
;-------------------------------------------------------------------------
PUBLIC masm_decompression_decompress
masm_decompression_decompress PROC
    push    rbp
    mov     rbp, rsp
    push    rbx

    mov     rbx, rcx        ; context pointer

    ; Detect compression type
    mov     rcx, [rbx + DECOMPRESSION_CONTEXT.input_buffer]
    mov     rdx, [rbx + DECOMPRESSION_CONTEXT.input_size]
    call    masm_decompression_detect_type
    mov     [rbx + DECOMPRESSION_CONTEXT.compression_type], eax

    cmp     eax, -1
    je      decomp_error

    ; Call GZIP decompression
    cmp     eax, 0
    je      decomp_gzip
    jmp     decomp_error

decomp_gzip:
    mov     rcx, [rbx + DECOMPRESSION_CONTEXT.input_buffer]
    mov     rdx, [rbx + DECOMPRESSION_CONTEXT.input_size]
    mov     r8, [rbx + DECOMPRESSION_CONTEXT.output_buffer]
    mov     r9, [rbx + DECOMPRESSION_CONTEXT.output_size]
    call    masm_decompression_gzip

    test    rax, rax
    jz      decomp_error

    ; Success
    mov     [rbx + DECOMPRESSION_CONTEXT.bytes_written], rax
    mov     QWORD PTR [rbx + DECOMPRESSION_CONTEXT.error_code], 0
    xor     rax, rax
    jmp     decomp_done

decomp_error:
    mov     QWORD PTR [rbx + DECOMPRESSION_CONTEXT.error_code], 1
    mov     rax, 1

decomp_done:
    pop     rbx
    pop     rbp
    ret
masm_decompression_decompress ENDP

END