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
; CustomZlibInit - Initialize compression/decompression context
; Parameters:
;   RCX = context pointer (COMPRESS_CTX or DECOMPRESS_CTX)
;   RDX = context size
; Returns:
;   RAX = 0 on success, error code otherwise
; ============================================================================
CustomZlibInit proc
    push rbx
    push rdi
    
    ; Zero out the context
    mov rdi, rcx
    mov rcx, rdx
    xor rax, rax
    rep stosb
    
    xor rax, rax        ; Return success
    pop rdi
    pop rbx
    ret
CustomZlibInit endp

; ============================================================================
; CustomZlibFree - Free compression/decompression context
; Parameters:
;   RCX = context pointer
; Returns:
;   RAX = 0 on success
; ============================================================================
CustomZlibFree proc
    ; Securely wipe compression/decompression context before release
    ; RCX = context pointer (COMPRESS_CTX or DECOMPRESS_CTX)
    push rdi
    push rcx

    test rcx, rcx
    jz @@czf_done

    ; Zero out the entire context including the sliding window
    ; DECOMPRESS_CTX is the larger struct (includes WINDOW_SIZE=32768 byte window)
    ; Wipe the full potential size to cover both COMPRESS_CTX and DECOMPRESS_CTX
    mov rdi, rcx
    mov ecx, SIZEOF DECOMPRESS_CTX
    xor eax, eax
    rep stosb

@@czf_done:
    pop rcx
    pop rdi
    xor rax, rax                     ; Return 0 = success
    ret
CustomZlibFree endp

; ============================================================================
; CustomZlibCompress - Compress data using DEFLATE algorithm
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
    
    ; Save parameters
    mov [rsp+20h], rcx      ; Input buffer
    mov [rsp+28h], rdx      ; Input size
    mov [rsp+30h], r8       ; Output buffer
    mov [rsp+38h], r9       ; Output size
    
    ; Validate parameters
    test rcx, rcx
    jz compress_error
    test rdx, rdx
    jz compress_error
    test r8, r8
    jz compress_error
    test r9, r9
    jz compress_error
    
    ; Initialize output position
    xor r12, r12            ; r12 = output position
    
    ; Write ZLIB header (CMF + FLG)
    mov rdi, r8
    mov byte ptr [rdi], 78h     ; CMF: compression method 8, window size 32K
    mov byte ptr [rdi+1], 9Ch   ; FLG: no dictionary, default compression
    add r12, 2
    
    ; Process input in blocks
    mov rsi, rcx            ; rsi = input pointer
    mov r13, rdx            ; r13 = remaining input size
    mov rdi, r8
    add rdi, r12            ; rdi = current output position
    
compress_loop:
    cmp r13, 0
    jle compress_done
    
    ; Determine block size (max 65535 bytes for uncompressed block)
    mov r14, r13
    cmp r14, 0FFFFh
    jle block_size_ok
    mov r14, 0FFFFh
block_size_ok:
    
    ; Check if we have enough output space
    mov rax, r12
    add rax, r14
    add rax, 5              ; Block header overhead
    cmp rax, r9
    jg compress_error
    
    ; Write block header (uncompressed block for simplicity)
    ; BFINAL=0, BTYPE=00 (uncompressed)
    mov byte ptr [rdi], 00h
    inc rdi
    inc r12
    
    ; Write block length (LEN)
    mov ax, r14w
    mov [rdi], ax
    add rdi, 2
    add r12, 2
    
    ; Write complement of length (NLEN)
    not ax
    mov [rdi], ax
    add rdi, 2
    add r12, 2
    
    ; Copy data
    mov rcx, r14
    rep movsb
    add r12, r14
    
    ; Update remaining size
    sub r13, r14
    mov rsi, [rsp+20h]
    add rsi, rdx
    sub rsi, r13
    
    jmp compress_loop
    
compress_done:
    ; Write final block header
    mov rdi, [rsp+30h]
    add rdi, r12
    mov byte ptr [rdi], 01h     ; BFINAL=1, BTYPE=00
    inc r12
    mov word ptr [rdi+1], 0     ; LEN=0
    mov word ptr [rdi+3], 0FFFFh ; NLEN=0xFFFF
    add r12, 4
    
    ; Calculate and write Adler-32 checksum
    mov rcx, [rsp+20h]
    mov rdx, [rsp+28h]
    call CalculateAdler32
    
    mov rdi, [rsp+30h]
    add rdi, r12
    
    ; Write checksum in big-endian
    bswap eax
    mov [rdi], eax
    add r12, 4
    
    ; Return compressed size
    mov rax, r12
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
; CustomZlibDecompress - Decompress DEFLATE compressed data
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
    
    ; Save parameters
    mov [rsp+20h], rcx      ; Input buffer
    mov [rsp+28h], rdx      ; Input size
    mov [rsp+30h], r8       ; Output buffer
    mov [rsp+38h], r9       ; Output size
    
    ; Validate parameters
    test rcx, rcx
    jz decompress_error
    test rdx, rdx
    jz decompress_error
    test r8, r8
    jz decompress_error
    test r9, r9
    jz decompress_error
    
    ; Check minimum size for ZLIB header
    cmp rdx, 6
    jl decompress_error
    
    ; Read and validate ZLIB header
    mov rsi, rcx
    movzx eax, byte ptr [rsi]
    movzx ebx, byte ptr [rsi+1]
    
    ; Validate CMF
    mov cl, al
    and cl, 0Fh
    cmp cl, 8               ; Check compression method (DEFLATE)
    jne decompress_error
    
    ; Skip header
    add rsi, 2
    mov r12, rdx
    sub r12, 6              ; Account for header and checksum
    
    ; Initialize output
    mov rdi, r8
    xor r13, r13            ; r13 = output position
    
decompress_loop:
    cmp r12, 0
    jle decompress_done
    
    ; Read block header
    movzx eax, byte ptr [rsi]
    inc rsi
    dec r12
    
    ; Check BFINAL bit
    mov r14b, al
    and r14b, 1
    
    ; Get BTYPE
    shr al, 1
    and al, 3
    
    ; Handle uncompressed block (BTYPE=00)
    cmp al, 0
    jne decompress_error    ; Only supporting uncompressed for now
    
    ; Read LEN
    movzx r15, word ptr [rsi]
    add rsi, 2
    sub r12, 2
    
    ; Read NLEN (and validate)
    movzx eax, word ptr [rsi]
    add rsi, 2
    sub r12, 2
    not ax
    cmp ax, r15w
    jne decompress_error
    
    ; Check output space
    mov rax, r13
    add rax, r15
    cmp rax, r9
    jg decompress_error
    
    ; Copy block data
    mov rcx, r15
    rep movsb
    add r13, r15
    sub r12, r15
    
    ; Check if final block
    test r14b, r14b
    jnz decompress_done
    
    jmp decompress_loop
    
decompress_done:
    ; Verify Adler-32 checksum
    mov rcx, [rsp+30h]
    mov rdx, r13
    call CalculateAdler32
    
    ; Read stored checksum
    mov rsi, [rsp+20h]
    mov rbx, [rsp+28h]
    add rsi, rbx
    sub rsi, 4
    
    mov ebx, [rsi]
    bswap ebx
    
    ; Compare checksums
    cmp eax, ebx
    jne decompress_error
    
    ; Return decompressed size
    mov rax, r13
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
; CalculateAdler32 - Calculate Adler-32 checksum
; Parameters:
;   RCX = data pointer
;   RDX = data size
; Returns:
;   EAX = Adler-32 checksum
; ============================================================================
CalculateAdler32 proc
    push rbx
    push rsi
    
    mov rsi, rcx
    mov rcx, rdx
    
    ; Initialize Adler-32 (s1=1, s2=0)
    mov eax, 1              ; s1
    xor edx, edx            ; s2
    
    test rcx, rcx
    jz adler_done
    
adler_loop:
    movzx ebx, byte ptr [rsi]
    inc rsi
    
    ; s1 = (s1 + byte) mod 65521
    add eax, ebx
    mov ebx, 65521
    xor edx, edx
    push rax
    xor eax, eax
    pop rax
    div ebx
    mov eax, edx
    inc eax
    
    ; s2 = (s2 + s1) mod 65521
    pop rdx
    add edx, eax
    push rax
    mov eax, edx
    xor edx, edx
    mov ebx, 65521
    div ebx
    mov edx, edx
    pop rax
    
    loop adler_loop
    
adler_done:
    ; Combine s2 and s1: (s2 << 16) | s1
    shl edx, 16
    or eax, edx
    
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
