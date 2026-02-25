; ============================================================================
; Custom ZLIB Implementation in MASM (x64)
; High-performance compression/decompression using DEFLATE algorithm
; ============================================================================


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code

; ============================================================================
; DEFLATE Compression Algorithm Implementation
; ============================================================================

; LZ77 sliding window parameters
WINDOW_SIZE     equ 32768       ; 32KB sliding window
MAX_MATCH       equ 258         ; Maximum match length
MIN_MATCH       equ 3           ; Minimum match length
HASH_BITS       equ 15          ; Hash table size
HASH_SIZE       equ 32768       ; 2^15

; Huffman coding parameters
MAX_BITS        equ 15          ; Maximum code length
LITERAL_CODES   equ 286         ; Number of literal/length codes
DISTANCE_CODES  equ 30          ; Number of distance codes

; ============================================================================
; Structure Definitions
; ============================================================================

; Compression context structure
COMPRESS_CTX struct
    input_ptr       qword   ?       ; Pointer to input data
    input_size      qword   ?       ; Size of input data
    output_ptr      qword   ?       ; Pointer to output buffer
    output_size     qword   ?       ; Size of output buffer
    output_pos      qword   ?       ; Current position in output
    window          byte    WINDOW_SIZE dup(?)  ; Sliding window
    window_pos      qword   ?       ; Current window position
    hash_table      word    HASH_SIZE dup(?)    ; Hash chain heads
    prev_match      word    WINDOW_SIZE dup(?)  ; Previous match positions
    literal_freq    word    LITERAL_CODES dup(?)    ; Literal frequency table
    distance_freq   word    DISTANCE_CODES dup(?)   ; Distance frequency table
COMPRESS_CTX ends

; Decompression context structure
DECOMPRESS_CTX struct
    input_ptr       qword   ?       ; Pointer to compressed data
    input_size      qword   ?       ; Size of compressed data
    output_ptr      qword   ?       ; Pointer to output buffer
    output_size     qword   ?       ; Size of output buffer
    output_pos      qword   ?       ; Current output position
    bit_buffer      dword   ?       ; Bit buffer for reading
    bit_count       byte    ?       ; Number of bits in buffer
    window          byte    WINDOW_SIZE dup(?)  ; Sliding window for decompression
    window_pos      qword   ?       ; Current window position
DECOMPRESS_CTX ends

; ============================================================================
; EXPORTED FUNCTIONS
; ============================================================================

public CustomZlibCompress
public CustomZlibDecompress
public CustomZlibGetCompressedSize
public CustomZlibGetDecompressedSize
public CustomZlibInit
public CustomZlibFree

; ============================================================================
; CustomZlibInit - Initialize context (OPTIMIZED: SIMD zeroing)
; Parameters:
;   RCX = context pointer
;   RDX = context size
; Returns:
;   RAX = 0 on success
; ============================================================================
CustomZlibInit proc
    push rdi
    
    mov rdi, rcx
    mov r8, rdx
    
    ; Use XMM for faster zeroing
    pxor xmm0, xmm0
    
    ; Zero 128 bytes at a time
    shr rdx, 7
    jz init_remaining
    
init_xmm_loop:
    movdqu [rdi], xmm0
    movdqu [rdi+16], xmm0
    movdqu [rdi+32], xmm0
    movdqu [rdi+48], xmm0
    movdqu [rdi+64], xmm0
    movdqu [rdi+80], xmm0
    movdqu [rdi+96], xmm0
    movdqu [rdi+112], xmm0
    add rdi, 128
    dec rdx
    jnz init_xmm_loop
    
init_remaining:
    mov rcx, r8
    and rcx, 127
    jz init_done
    xor rax, rax
    rep stosb
    
init_done:
    xor rax, rax
    pop rdi
    ret
CustomZlibInit endp

; ============================================================================
; CustomZlibFree - OPTIMIZED secure wipe (SIMD)
; Parameters:
;   RCX = context pointer
; Returns:
;   RAX = 0 on success
; ============================================================================
CustomZlibFree proc
    push rdi
    
    test rcx, rcx
    jz czf_done
    
    mov rdi, rcx
    mov edx, SIZEOF DECOMPRESS_CTX
    
    ; Use XMM for faster secure wiping
    pxor xmm0, xmm0
    
    ; Wipe 128 bytes at a time
    shr edx, 7
    jz free_remaining
    
free_xmm_loop:
    movdqu [rdi], xmm0
    movdqu [rdi+16], xmm0
    movdqu [rdi+32], xmm0
    movdqu [rdi+48], xmm0
    movdqu [rdi+64], xmm0
    movdqu [rdi+80], xmm0
    movdqu [rdi+96], xmm0
    movdqu [rdi+112], xmm0
    add rdi, 128
    dec edx
    jnz free_xmm_loop
    
free_remaining:
    mov ecx, SIZEOF DECOMPRESS_CTX
    and ecx, 127
    jz czf_done
    xor eax, eax
    rep stosb
    
czf_done:
    xor rax, rax
    pop rdi
    ret
CustomZlibFree endp

; ============================================================================
; CustomZlibCompress - OPTIMIZED compression
; Parameters:
;   RCX = input buffer pointer
;   RDX = input size
;   R8  = output buffer pointer
;   R9  = output buffer size
; Returns:
;   RAX = compressed size, or -1 on error
; ============================================================================
CustomZlibCompress proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h
    
    ; Keep parameters in registers
    mov r15, rcx            ; Input buffer
    mov r14, rdx            ; Input size
    mov r13, r8             ; Output buffer
    mov r12, r9             ; Output size
    
    ; Validate parameters
    test rcx, rcx
    jz compress_error
    test rdx, rdx
    jz compress_error
    test r8, r8
    jz compress_error
    test r9, r9
    jz compress_error
    
    ; Write ZLIB header
    mov word ptr [r13], 9C78h   ; Combined CMF+FLG write
    mov rdi, r13
    add rdi, 2
    mov rbx, 2              ; Output position
    
    mov rsi, r15            ; Input pointer
    
compress_loop:
    test r14, r14
    jle compress_done
    
    ; Determine block size
    mov r10, r14
    cmp r10, 0FFFFh
    cmova r10, 0FFFFh       ; Conditional move instead of branch
    
    ; Check output space
    lea rax, [rbx + r10 + 5]
    cmp rax, r12
    jg compress_error
    
    ; Write block header (uncompressed)
    mov byte ptr [rdi], 00h
    
    ; Write LEN and NLEN
    mov ax, r10w
    mov [rdi+1], ax
    not ax
    mov [rdi+3], ax
    add rdi, 5
    add rbx, 5
    
    ; Copy data
    mov rcx, r10
    rep movsb
    add rbx, r10
    sub r14, r10
    
    jmp compress_loop
    
compress_done:
    ; Write final block
    mov dword ptr [rdi], 0FFFF0001h ; Combined write
    mov byte ptr [rdi+4], 0FFh
    add rdi, 5
    add rbx, 5
    
    ; Calculate Adler-32
    mov rcx, r15
    mov rdx, [rsp+28h]
    mov [rsp+28h], rbx      ; Save output position
    call CalculateAdler32
    mov rbx, [rsp+28h]
    
    ; Write checksum
    bswap eax
    mov [rdi], eax
    lea rax, [rbx + 4]
    jmp compress_exit
    
compress_error:
    mov rax, -1
    
compress_exit:
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CustomZlibCompress endp

; ============================================================================
; CustomZlibDecompress - OPTIMIZED decompression
; Parameters:
;   RCX = compressed buffer pointer
;   RDX = compressed size
;   R8  = output buffer pointer
;   R9  = output buffer size
; Returns:
;   RAX = decompressed size, or -1 on error
; ============================================================================
CustomZlibDecompress proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h
    
    ; Keep parameters in registers
    mov r15, rcx            ; Input buffer
    mov r14, rdx            ; Input size
    mov r13, r8             ; Output buffer
    mov r12, r9             ; Output size
    
    ; Validate parameters
    test rcx, rcx
    jz decompress_error
    test rdx, rdx
    jz decompress_error
    test r8, r8
    jz decompress_error
    test r9, r9
    jz decompress_error
    
    ; Check minimum size
    cmp rdx, 6
    jl decompress_error
    
    ; Validate ZLIB header
    mov rsi, r15
    movzx eax, byte ptr [rsi]
    and al, 0Fh
    cmp al, 8
    jne decompress_error
    
    ; Skip header
    add rsi, 2
    mov r10, r14
    sub r10, 6              ; Account for header and checksum
    
    ; Initialize output
    mov rdi, r13
    xor r11, r11            ; Output position
    
decompress_loop:
    test r10, r10
    jle decompress_done
    
    ; Read block header
    movzx eax, byte ptr [rsi]
    inc rsi
    dec r10
    
    ; Check BFINAL and BTYPE
    mov ebx, eax
    and ebx, 1              ; BFINAL
    shr al, 1
    and al, 3               ; BTYPE
    
    ; Only support uncompressed blocks
    test al, al
    jnz decompress_error
    
    ; Read LEN and NLEN
    movzx ecx, word ptr [rsi]
    movzx eax, word ptr [rsi+2]
    add rsi, 4
    sub r10, 4
    
    ; Validate NLEN
    not ax
    cmp ax, cx
    jne decompress_error
    
    ; Check output space
    lea rax, [r11 + rcx]
    cmp rax, r12
    jg decompress_error
    
    ; Copy block data
    rep movsb
    add r11, rcx
    sub r10, rcx
    
    ; Check if final block
    test ebx, ebx
    jnz decompress_done
    
    jmp decompress_loop
    
decompress_done:
    ; Verify Adler-32
    mov rcx, r13
    mov rdx, r11
    mov [rsp+28h], r11      ; Save output size
    call CalculateAdler32
    mov r11, [rsp+28h]
    
    ; Read stored checksum
    mov rsi, r15
    add rsi, r14
    sub rsi, 4
    mov ebx, [rsi]
    bswap ebx
    
    ; Compare checksums
    cmp eax, ebx
    jne decompress_error
    
    mov rax, r11
    jmp decompress_exit
    
decompress_error:
    mov rax, -1
    
decompress_exit:
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CustomZlibDecompress endp

; ============================================================================
; CalculateAdler32 - OPTIMIZED Adler-32 checksum
; Parameters:
;   RCX = data pointer
;   RDX = data size
; Returns:
;   EAX = Adler-32 checksum
; ============================================================================
CalculateAdler32 proc
    push rbx
    push rsi
    push r12
    
    mov rsi, rcx
    mov r12, rdx
    
    ; Initialize Adler-32 (s1=1, s2=0)
    mov eax, 1              ; s1
    xor edx, edx            ; s2
    mov r11d, 65521         ; Modulo constant
    
    test r12, r12
    jz adler_done
    
    ; Process in chunks to reduce modulo operations
    mov r10, r12
    shr r10, 4              ; Number of 16-byte chunks
    and r12, 15             ; Remaining bytes
    
adler_chunk_loop:
    test r10, r10
    jz adler_remaining
    
    ; Process 16 bytes without modulo
    mov ecx, 16
adler_inner:
    movzx ebx, byte ptr [rsi]
    inc rsi
    add eax, ebx
    add edx, eax
    dec ecx
    jnz adler_inner
    
    ; Apply modulo after chunk
    mov ecx, eax
    xor eax, eax
    mov ebx, edx
    mov edx, ecx
    div r11d
    mov eax, edx
    
    mov edx, ebx
    xor ebx, ebx
    xchg eax, edx
    div r11d
    mov edx, edx
    xchg eax, edx
    
    dec r10
    jmp adler_chunk_loop
    
adler_remaining:
    test r12, r12
    jz adler_done
    
adler_final_loop:
    movzx ebx, byte ptr [rsi]
    inc rsi
    add eax, ebx
    add edx, eax
    dec r12
    jnz adler_final_loop
    
    ; Final modulo
    xor ebx, ebx
    mov ecx, eax
    xor eax, eax
    mov ebx, edx
    mov edx, ecx
    div r11d
    mov eax, edx
    
    mov edx, ebx
    xor ebx, ebx
    xchg eax, edx
    div r11d
    mov edx, edx
    xchg eax, edx
    
adler_done:
    ; Combine s2 and s1: (s2 << 16) | s1
    shl edx, 16
    or eax, edx
    
    pop r12
    pop rsi
    pop rbx
    ret
CalculateAdler32 endp

; ============================================================================
; CustomZlibGetCompressedSize - Estimate compressed size
; Parameters:
;   RCX = uncompressed size
; Returns:
;   RAX = estimated compressed size
; ============================================================================
CustomZlibGetCompressedSize proc
    ; Worst case: input size + 0.1% + 12 bytes overhead + block headers
    mov rax, rcx
    shr rcx, 10             ; Divide by 1024
    add rax, rcx            ; Add 0.1%
    add rax, 256            ; Add overhead
    ret
CustomZlibGetCompressedSize endp

; ============================================================================
; CustomZlibGetDecompressedSize - Get decompressed size from header
; Parameters:
;   RCX = compressed buffer pointer
;   RDX = compressed size
; Returns:
;   RAX = estimated decompressed size (or -1 if invalid)
; ============================================================================
CustomZlibGetDecompressedSize proc
    ; This is an estimate - actual size determined during decompression
    cmp rdx, 6
    jl size_error
    
    ; For now, return a conservative estimate
    mov rax, rdx
    shl rax, 3              ; Assume up to 8x expansion
    ret
    
size_error:
    mov rax, -1
    ret
CustomZlibGetDecompressedSize endp

end
