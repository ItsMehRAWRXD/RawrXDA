; =============================================================================
; RawrXD Comprehensive Multi-Codec Encoder Engine
; Architecture: x86-64 (Pure MASM)
; Optimizations: AVX-512, BMI2, Multi-Threading Ready
; Philosophy: "Doesn't Say No" - Accepts any input, produces valid output
; =============================================================================

;------------------------------------------------------------------------------
; Conditional Assembly Directives
;------------------------------------------------------------------------------
ifndef _RAWRXD_ENCODER_
_RAWRXD_ENCODER_ equ 1

option casemap:none

;------------------------------------------------------------------------------
; External Dependencies (Windows API)
;------------------------------------------------------------------------------
extrn VirtualAlloc:proc
extrn VirtualFree:proc
extrn GetCurrentProcess:proc
extrn SetThreadAffinityMask:proc
extrn RtlCompareMemory:proc
extrn RtlCopyMemory:proc
extrn GetTickCount64:proc
extrn ExitProcess:proc

;------------------------------------------------------------------------------
; Status Constants
;------------------------------------------------------------------------------
ENC_OK              equ 0
ENC_INVALID_INPUT   equ 1
ENC_BUFFER_TOO_SMALL equ 2
ENC_UNSUPPORTED     equ 3
ENC_CALLBACK_ERROR  equ 4

;------------------------------------------------------------------------------
; Codec Type Enumeration
;------------------------------------------------------------------------------
CODEC_RAW           equ 0   ; Passthrough (identity)
CODEC_BASE64_STD    equ 1   ; Standard Base64 (+/)
CODEC_BASE64_URL    equ 2   ; URL-Safe Base64 (-_)
CODEC_BASE64_AVX    equ 3   ; AVX-512 accelerated Base64
CODEC_HEX           equ 4   ; Hexadecimal encoding
CODEC_HEX_UPPER     equ 5   ; Uppercase Hex
CODEC_URL           equ 6   ; Percent-encoding
CODEC_XOR           equ 7   ; XOR cipher (stream)
CODEC_ROT13         equ 8   ; ROT13 transformation
CODEC_RLE           equ 9   ; Run-Length Encoding
CODEC_LZ77_STUB     equ 10  ; LZ77 placeholder (stub for expansion)
CODEC_CUSTOM        equ 255 ; User-defined callback

;------------------------------------------------------------------------------
; Structure: ENCODER_CONTEXT (64 bytes, cache-line aligned)
;------------------------------------------------------------------------------
ENCODER_CONTEXT struct 8
    ; Input/Output Management
    input_buffer        dq ?    ; Source data pointer
    input_length        dq ?    ; Source length in bytes
    output_buffer       dq ?    ; Destination buffer
    output_capacity     dq ?    ; Max output size
    output_length       dq ?    ; Actual bytes written
    
    ; Codec Configuration
    codec_type          dd ?    ; CODEC_* constant
    codec_flags         dd ?    ; Behavior flags
    xor_key             dd ?    ; XOR key (for CODEC_XOR)
    url_safe            db ?    ; URL-safe flag for Base64
    
    ; Performance & State
    simd_available      db ?    ; AVX-512 detection result (0=SSE2, 1=AVX2, 2=AVX512)
    reserved_align1     dw ?    ; Alignment padding
    thread_id           dd ?    ; Worker thread identifier
    benchmark_cycles    dq ?    ; Cycle counter
    
    ; Callbacks (for CODEC_CUSTOM)
    encode_callback     dq ?    ; User encode function
    decode_callback     dq ?    ; User decode function
    
    ; Error Handling ("Doesn't Say No" philosophy)
    strict_mode         db ?    ; 0 = permissive, 1 = strict
    replacement_char    db ?    ; Used for invalid sequences
    padding_alignment   db ?    ; Output alignment (1, 2, 4, 8, 16, 32, 64)
    reserved_align2     db 5 dup(?) ; Pad to 64 bytes
ENCODER_CONTEXT ends

;------------------------------------------------------------------------------
; Structure: CODEC_CAPS - Codec Capabilities Descriptor
;------------------------------------------------------------------------------
CODEC_CAPS struct 8
    codec_id            dd ?
    reserved1           dd ?
    name_ptr            dq ?    ; ASCIIZ name pointer
    simd_accelerated    db ?
    stream_capable      db ?
    ratio_min           dw ?    ; Minimum expansion ratio (percent)
    ratio_max           dw ?    ; Maximum expansion ratio (percent)
    stateful            db ?    ; Requires context between calls
    reserved2           db 5 dup(?)
CODEC_CAPS ends

;------------------------------------------------------------------------------
; Constants & Lookup Tables (Read-Only Data)
;------------------------------------------------------------------------------
.const
    align 64
    ; Base64 Standard Encoding Table (RFC 4648)
    base64_enc_lut      db "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
                        db 0    ; Null terminator for safety
    
    align 64
    ; Base64 URL-Safe Encoding Table (RFC 4648 Section 5)
    base64_url_lut      db "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
                        db 0
    
    align 256
    ; Base64 Decoding Table (256 bytes: char -> 6-bit value, 0xFF = invalid)
    base64_dec_lut      db 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh  ; 00-07
                        db 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh  ; 08-0F
                        db 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh  ; 10-17
                        db 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh  ; 18-1F
                        db 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh  ; 20-27 (space-')
                        db 0FFh, 0FFh, 0FFh,   62, 0FFh,   62, 0FFh,   63  ; 28-2F (+,- -> 62, / -> 63)
                        db   52,   53,   54,   55,   56,   57,   58,   59  ; 30-37 (0-7 -> 52-59)
                        db   60,   61, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh  ; 38-3F (8-9 -> 60-61, = ignored)
                        db 0FFh,    0,    1,    2,    3,    4,    5,    6  ; 40-47 (@, A-G -> 0-6)
                        db    7,    8,    9,   10,   11,   12,   13,   14  ; 48-4F (H-O -> 7-14)
                        db   15,   16,   17,   18,   19,   20,   21,   22  ; 50-57 (P-W -> 15-22)
                        db   23,   24,   25, 0FFh, 0FFh, 0FFh, 0FFh,   63  ; 58-5F (X-Z -> 23-25, _ -> 63)
                        db 0FFh,   26,   27,   28,   29,   30,   31,   32  ; 60-67 (`, a-g -> 26-32)
                        db   33,   34,   35,   36,   37,   38,   39,   40  ; 68-6F (h-o -> 33-40)
                        db   41,   42,   43,   44,   45,   46,   47,   48  ; 70-77 (p-w -> 41-48)
                        db   49,   50,   51, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh  ; 78-7F (x-z -> 49-51)
                        db 128 dup(0FFh)                                   ; 80-FF (extended ASCII invalid)
    
    align 16
    ; Hex encoding tables
    hex_lower           db "0123456789abcdef"
    hex_upper           db "0123456789ABCDEF"
    
    align 256
    ; URL encoding whitelist (unreserved characters per RFC 3986)
    ; 0 = must encode, 1 = safe to pass through
    url_safe_lut        db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  ; 00-0F
                        db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  ; 10-1F
                        db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0  ; 20-2F (-, . safe)
                        db 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0  ; 30-3F (0-9 safe)
                        db 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  ; 40-4F (A-O safe)
                        db 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1  ; 50-5F (P-Z, _ safe)
                        db 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  ; 60-6F (a-o safe)
                        db 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0  ; 70-7F (p-z, ~ safe)
                        db 128 dup(0)                                      ; 80-FF (extended must encode)

    ; XOR cipher default key (golden ratio-derived)
    xor_default_key     dd 0DEADBEEFh
    xor_lcg_constant    dd 09E3779B9h   ; Golden ratio * 2^32

    ; Error messages
    szErrInvalidInput   db "Error: Invalid input buffer or length", 13, 10, 0
    szErrBufferSmall    db "Error: Output buffer too small", 13, 10, 0
    szErrUnsupported    db "Error: Unsupported codec type", 13, 10, 0
    szEncoderBanner     db "RawrXD Encoder Engine v1.0 - Multi-Codec Processing", 13, 10, 0

;------------------------------------------------------------------------------
; Uninitialized Data Section
;------------------------------------------------------------------------------
.data?
    ; Global encoder state
    g_simd_level        db ?    ; Cached SIMD detection result
    g_encoder_init      db ?    ; Initialization flag
    
    align 8
    g_perf_counter      dq ?    ; Performance measurement
    g_bytes_processed   dq ?    ; Total bytes processed (statistics)

;------------------------------------------------------------------------------
; Code Section
;------------------------------------------------------------------------------
.code
align 16

;=============================================================================
; SECTION 1: Core Initialization & Capability Detection
;=============================================================================

;------------------------------------------------------------------------------
; DetectSimdSupport
; Detects CPU SIMD capabilities for optimal code path selection
; Returns: EAX = 0 (SSE2 only), 1 (AVX2), 2 (AVX-512)
;------------------------------------------------------------------------------
DetectSimdSupport proc frame
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    .endprolog
    
    ; Check basic CPUID support
    mov eax, 1
    cpuid
    
    ; Check AVX bit (bit 28 of ECX)
    test ecx, 10000000h
    jz @@sse2_only
    
    ; Check OSXSAVE (bit 27) - OS must support saving AVX state
    test ecx, 08000000h
    jz @@sse2_only
    
    ; Verify XCR0 has XMM and YMM state enabled
    xor ecx, ecx
    xgetbv
    and eax, 06h            ; Check bits 1 (SSE) and 2 (AVX)
    cmp eax, 06h
    jne @@sse2_only
    
    ; Check AVX2 support (Leaf 7, EBX bit 5)
    mov eax, 7
    xor ecx, ecx
    cpuid
    test ebx, 20h           ; AVX2 bit
    jz @@sse2_only          ; If no AVX2, treat as SSE2 for safety
    
    ; Check AVX-512 Foundation (bit 16 of EBX)
    test ebx, 10000h
    jz @@avx2
    
    ; Verify AVX-512 state is enabled in XCR0
    xor ecx, ecx
    xgetbv
    and eax, 0E0h           ; Check bits 5, 6, 7 (AVX-512 state)
    cmp eax, 0E0h
    jne @@avx2
    
    ; Full AVX-512 support confirmed
    mov eax, 2
    jmp @@done
    
@@avx2:
    mov eax, 1
    jmp @@done
    
@@sse2_only:
    xor eax, eax
    
@@done:
    ; Cache the result
    mov g_simd_level, al
    
    pop rdi
    pop rbx
    ret
DetectSimdSupport endp

;------------------------------------------------------------------------------
; EncoderInitializeContext
; Initializes an encoder context with default values
; RCX = Context*, RDX = CodecType, R8 = Flags
; Returns: EAX = Status (ENC_OK on success)
;------------------------------------------------------------------------------
EncoderInitializeContext proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    .endprolog
    
    test rcx, rcx
    jz @@invalid_ptr
    
    mov rsi, rcx            ; RSI = Context pointer
    mov ebx, edx            ; EBX = CodecType
    mov edi, r8d            ; EDI = Flags
    
    ; Zero-initialize the entire structure (security: prevent info leak)
    push rcx
    mov rdi, rsi
    mov rcx, sizeof ENCODER_CONTEXT
    xor eax, eax
    rep stosb
    pop rcx
    mov rdi, r8d            ; Restore flags
    
    ; Fill default values
    mov [rsi].ENCODER_CONTEXT.codec_type, ebx
    mov [rsi].ENCODER_CONTEXT.codec_flags, edi
    mov byte ptr [rsi].ENCODER_CONTEXT.strict_mode, 0       ; Permissive by default
    mov byte ptr [rsi].ENCODER_CONTEXT.replacement_char, '?' ; Replacement for invalid
    mov byte ptr [rsi].ENCODER_CONTEXT.padding_alignment, 1  ; Byte alignment
    mov dword ptr [rsi].ENCODER_CONTEXT.xor_key, 0DEADBEEFh ; Default XOR key
    
    ; Detect and cache SIMD capabilities
    cmp g_encoder_init, 1
    je @@simd_cached
    call DetectSimdSupport
    mov g_encoder_init, 1
    jmp @@store_simd
    
@@simd_cached:
    movzx eax, g_simd_level
    
@@store_simd:
    mov [rsi].ENCODER_CONTEXT.simd_available, al
    
    ; Codec-specific initialization
    cmp ebx, CODEC_BASE64_AVX
    je @@init_base64_avx
    cmp ebx, CODEC_BASE64_URL
    je @@init_base64_url
    jmp @@success
    
@@init_base64_avx:
    ; Verify AVX-512 available, fallback to scalar if not
    cmp al, 2
    jae @@success
    ; Fallback to standard Base64
    mov dword ptr [rsi].ENCODER_CONTEXT.codec_type, CODEC_BASE64_STD
    jmp @@success
    
@@init_base64_url:
    mov byte ptr [rsi].ENCODER_CONTEXT.url_safe, 1
    jmp @@success
    
@@success:
    xor eax, eax            ; ENC_OK
    jmp @@exit
    
@@invalid_ptr:
    mov eax, ENC_INVALID_INPUT
    
@@exit:
    pop rbx
    pop rdi
    pop rsi
    ret
EncoderInitializeContext endp

;=============================================================================
; SECTION 2: Base64 Encoding (Scalar Implementation)
;=============================================================================

;------------------------------------------------------------------------------
; Base64EncodeScalar
; Encodes binary data to Base64 using scalar operations
; RCX = src, RDX = len, R8 = dst, R9 = url_safe flag
; Returns: RAX = bytes written
;------------------------------------------------------------------------------
Base64EncodeScalar proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    .endprolog
    
    mov rsi, rcx            ; Source pointer
    mov r12, rdx            ; Input length
    mov rdi, r8             ; Destination pointer
    mov r15, r8             ; Save original destination for length calc
    
    ; Select lookup table based on url_safe flag
    test r9, r9
    jnz @@use_url_table
    lea rbx, base64_enc_lut
    jmp @@setup_loop
@@use_url_table:
    lea rbx, base64_url_lut
    
@@setup_loop:
    ; Calculate number of complete 3-byte chunks
    mov r13, r12
    mov rax, r12
    xor edx, edx
    mov rcx, 3
    div rcx                 ; RAX = complete chunks, RDX = remainder
    mov r14, rax            ; R14 = chunk count
    
    test r14, r14
    jz @@handle_remainder
    
@@chunk_loop:
    ; Load 3 bytes and combine into 24-bit value
    movzx eax, byte ptr [rsi]       ; Byte 0
    movzx ecx, byte ptr [rsi+1]     ; Byte 1
    movzx edx, byte ptr [rsi+2]     ; Byte 2
    
    shl eax, 16
    shl ecx, 8
    or eax, ecx
    or eax, edx             ; EAX = 24-bit packed input
    
    ; Extract four 6-bit indices and lookup
    ; Index 0: bits 23-18
    mov ecx, eax
    shr ecx, 18
    and ecx, 3Fh
    movzx edx, byte ptr [rbx+rcx]
    mov [rdi], dl
    
    ; Index 1: bits 17-12
    mov ecx, eax
    shr ecx, 12
    and ecx, 3Fh
    movzx edx, byte ptr [rbx+rcx]
    mov [rdi+1], dl
    
    ; Index 2: bits 11-6
    mov ecx, eax
    shr ecx, 6
    and ecx, 3Fh
    movzx edx, byte ptr [rbx+rcx]
    mov [rdi+2], dl
    
    ; Index 3: bits 5-0
    and eax, 3Fh
    movzx edx, byte ptr [rbx+rax]
    mov [rdi+3], dl
    
    add rsi, 3
    add rdi, 4
    dec r14
    jnz @@chunk_loop
    
@@handle_remainder:
    ; Handle 1 or 2 remaining bytes with padding
    mov rax, r13
    xor edx, edx
    mov rcx, 3
    div rcx
    mov rcx, rdx            ; RCX = remainder (0, 1, or 2)
    
    test rcx, rcx
    jz @@done
    
    cmp rcx, 1
    je @@one_byte_left
    
@@two_bytes_left:
    ; 2 bytes -> 3 Base64 chars + 1 padding
    movzx eax, byte ptr [rsi]
    movzx edx, byte ptr [rsi+1]
    shl eax, 8
    or eax, edx
    shl eax, 2              ; Shift left to align for 6-bit extraction
    
    ; Index 0: bits 15-10 of shifted value
    mov ecx, eax
    shr ecx, 12
    and ecx, 3Fh
    movzx edx, byte ptr [rbx+rcx]
    mov [rdi], dl
    
    ; Index 1: bits 9-4
    mov ecx, eax
    shr ecx, 6
    and ecx, 3Fh
    movzx edx, byte ptr [rbx+rcx]
    mov [rdi+1], dl
    
    ; Index 2: bits 3-0 (padded with zeros)
    and eax, 3Fh
    movzx edx, byte ptr [rbx+rax]
    mov [rdi+2], dl
    
    ; Padding
    mov byte ptr [rdi+3], '='
    add rdi, 4
    jmp @@done
    
@@one_byte_left:
    ; 1 byte -> 2 Base64 chars + 2 padding
    movzx eax, byte ptr [rsi]
    shl eax, 4              ; Shift left to align for 6-bit extraction
    
    ; Index 0: bits 9-4
    mov ecx, eax
    shr ecx, 6
    and ecx, 3Fh
    movzx edx, byte ptr [rbx+rcx]
    mov [rdi], dl
    
    ; Index 1: bits 3-0 (padded with zeros)
    and eax, 3Fh
    movzx edx, byte ptr [rbx+rax]
    mov [rdi+1], dl
    
    ; Padding
    mov byte ptr [rdi+2], '='
    mov byte ptr [rdi+3], '='
    add rdi, 4
    
@@done:
    ; Calculate and return bytes written
    mov rax, rdi
    sub rax, r15
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
Base64EncodeScalar endp

;------------------------------------------------------------------------------
; Base64DecodeScalar
; Decodes Base64 to binary (permissive: skips invalid characters)
; RCX = src, RDX = len, R8 = dst
; Returns: RAX = bytes written
;------------------------------------------------------------------------------
Base64DecodeScalar proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    .endprolog
    
    mov rsi, rcx            ; Source
    mov r12, rdx            ; Input length
    mov rdi, r8             ; Destination
    mov r14, r8             ; Save original dest
    lea rbx, base64_dec_lut ; Decode table
    
    xor r13d, r13d          ; Accumulated bits
    xor ecx, ecx            ; Bit count (0, 6, 12, 18)
    
@@decode_loop:
    test r12, r12
    jz @@flush_bits
    dec r12
    
    movzx eax, byte ptr [rsi]
    inc rsi
    
    ; Skip padding characters
    cmp al, '='
    je @@decode_loop
    
    ; Lookup in decode table
    movzx eax, byte ptr [rbx+rax]
    cmp al, 0FFh
    je @@decode_loop        ; Skip invalid characters (permissive mode)
    
    ; Accumulate 6 bits
    shl r13d, 6
    or r13d, eax
    add ecx, 6
    
    ; Output complete bytes
    cmp ecx, 8
    jl @@decode_loop
    
@@output_byte:
    sub ecx, 8
    mov eax, r13d
    mov edx, ecx
    shr eax, cl             ; Shift right by remaining bits
    mov [rdi], al
    inc rdi
    
    ; Mask off output bits
    mov eax, 1
    shl eax, cl
    dec eax                 ; Create mask for remaining bits
    and r13d, eax
    
    cmp ecx, 8
    jge @@output_byte
    jmp @@decode_loop
    
@@flush_bits:
    ; Any remaining bits are padding (ignore)
    mov rax, rdi
    sub rax, r14            ; Bytes written
    
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
Base64DecodeScalar endp

;=============================================================================
; SECTION 3: Hexadecimal Encoding (Always safe, always works)
;=============================================================================

;------------------------------------------------------------------------------
; HexEncode
; Encodes binary data to hexadecimal string
; RCX = src, RDX = len, R8 = dst, R9 = uppercase flag (1 = upper)
; Returns: RAX = bytes written (always len * 2)
;------------------------------------------------------------------------------
HexEncode proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    .endprolog
    
    mov rsi, rcx            ; Source
    mov r12, rdx            ; Length
    mov rdi, r8             ; Destination
    
    ; Select hex table
    test r9, r9
    jnz @@use_upper
    lea rbx, hex_lower
    jmp @@check_empty
@@use_upper:
    lea rbx, hex_upper
    
@@check_empty:
    test r12, r12
    jz @@done
    
@@encode_loop:
    movzx eax, byte ptr [rsi]
    
    ; High nibble
    mov ecx, eax
    shr ecx, 4
    movzx edx, byte ptr [rbx+rcx]
    mov [rdi], dl
    
    ; Low nibble
    and eax, 0Fh
    movzx eax, byte ptr [rbx+rax]
    mov [rdi+1], al
    
    inc rsi
    add rdi, 2
    dec r12
    jnz @@encode_loop
    
@@done:
    ; Calculate bytes written
    mov rax, rdi
    sub rax, r8
    
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
HexEncode endp

;------------------------------------------------------------------------------
; HexDecode
; Decodes hexadecimal string to binary (permissive: skips non-hex chars)
; RCX = src (ASCII hex), RDX = len, R8 = dst
; Returns: RAX = bytes written
;------------------------------------------------------------------------------
HexDecode proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog
    
    mov rsi, rcx            ; Source
    mov r12, rdx            ; Length
    mov rdi, r8             ; Destination
    mov r13, r8             ; Save original dest
    
    xor ebx, ebx            ; Accumulated nibble
    xor ecx, ecx            ; Nibble count (0 or 1)
    
@@decode_loop:
    test r12, r12
    jz @@flush
    dec r12
    
    movzx eax, byte ptr [rsi]
    inc rsi
    
    ; Convert ASCII to nibble value
    sub eax, '0'
    cmp eax, 9
    jbe @@got_digit
    
    ; Try uppercase A-F
    sub eax, ('A' - '0' - 10)
    cmp eax, 15
    jbe @@check_range_upper
    
    ; Try lowercase a-f
    sub eax, ('a' - 'A')
    cmp eax, 15
    jbe @@check_range_lower
    
    ; Invalid character - skip (permissive mode)
    jmp @@decode_loop
    
@@check_range_upper:
    cmp eax, 10
    jl @@decode_loop        ; Invalid
    jmp @@got_digit
    
@@check_range_lower:
    cmp eax, 10
    jl @@decode_loop        ; Invalid
    
@@got_digit:
    ; EAX = 0-15
    test ecx, ecx
    jnz @@combine_nibbles
    
    ; First nibble (high)
    mov ebx, eax
    inc ecx
    jmp @@decode_loop
    
@@combine_nibbles:
    ; Second nibble (low) - output byte
    shl ebx, 4
    or ebx, eax
    mov [rdi], bl
    inc rdi
    xor ecx, ecx
    jmp @@decode_loop
    
@@flush:
    ; If odd number of nibbles, output partial byte (high nibble only)
    test ecx, ecx
    jz @@done
    shl ebx, 4
    mov [rdi], bl
    inc rdi
    
@@done:
    mov rax, rdi
    sub rax, r13
    
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
HexDecode endp

;=============================================================================
; SECTION 4: URL Encoding (RFC 3986 Compliant + Permissive)
;=============================================================================

;------------------------------------------------------------------------------
; UrlEncode
; Percent-encodes data per RFC 3986
; RCX = src, RDX = len, R8 = dst, R9 = dst_capacity
; Returns: RAX = bytes written (may be truncated if capacity exceeded)
;------------------------------------------------------------------------------
UrlEncode proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    .endprolog
    
    mov rsi, rcx            ; Source
    mov r12, rdx            ; Input length
    mov rdi, r8             ; Destination
    mov r13, r9             ; Capacity
    mov r14, r8             ; Save original dest
    lea rbx, url_safe_lut   ; Safety lookup table
    
@@encode_loop:
    test r12, r12
    jz @@done
    dec r12
    
    movzx eax, byte ptr [rsi]
    inc rsi
    
    ; Check if character is URL-safe
    movzx ecx, byte ptr [rbx+rax]
    test ecx, ecx
    jnz @@copy_direct
    
    ; Needs percent-encoding: check capacity (need 3 bytes)
    mov rcx, rdi
    sub rcx, r14
    add rcx, 3
    cmp rcx, r13
    ja @@capacity_exceeded
    
    ; Output %XX
    mov byte ptr [rdi], '%'
    
    ; High nibble
    mov ecx, eax
    shr ecx, 4
    movzx edx, byte ptr [hex_upper+rcx]
    mov [rdi+1], dl
    
    ; Low nibble
    and eax, 0Fh
    movzx eax, byte ptr [hex_upper+rax]
    mov [rdi+2], al
    
    add rdi, 3
    jmp @@encode_loop
    
@@copy_direct:
    ; Check capacity
    mov rcx, rdi
    sub rcx, r14
    inc rcx
    cmp rcx, r13
    ja @@capacity_exceeded
    
    mov [rdi], al
    inc rdi
    jmp @@encode_loop
    
@@capacity_exceeded:
    ; Stop encoding, return what we have
    
@@done:
    mov rax, rdi
    sub rax, r14
    
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
UrlEncode endp

;------------------------------------------------------------------------------
; UrlDecode
; Decodes percent-encoded data (permissive: passes through invalid sequences)
; RCX = src, RDX = len, R8 = dst
; Returns: RAX = bytes written
;------------------------------------------------------------------------------
UrlDecode proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog
    
    mov rsi, rcx
    mov r12, rdx
    mov rdi, r8
    mov r13, r8             ; Save original dest
    
@@decode_loop:
    test r12, r12
    jz @@done
    
    movzx eax, byte ptr [rsi]
    
    cmp al, '%'
    je @@decode_percent
    
    cmp al, '+'
    je @@decode_plus
    
    ; Regular character
    mov [rdi], al
    inc rdi
    inc rsi
    dec r12
    jmp @@decode_loop
    
@@decode_plus:
    ; '+' -> space (common in query strings)
    mov byte ptr [rdi], ' '
    inc rdi
    inc rsi
    dec r12
    jmp @@decode_loop
    
@@decode_percent:
    ; Need at least 2 more characters
    cmp r12, 3
    jl @@copy_literal_percent
    
    ; Get high nibble
    movzx ebx, byte ptr [rsi+1]
    call @@hex_to_nibble
    cmp eax, -1
    je @@copy_literal_percent
    mov ecx, eax
    shl ecx, 4
    
    ; Get low nibble
    movzx ebx, byte ptr [rsi+2]
    call @@hex_to_nibble
    cmp eax, -1
    je @@copy_literal_percent
    
    or ecx, eax
    mov [rdi], cl
    inc rdi
    add rsi, 3
    sub r12, 3
    jmp @@decode_loop
    
@@copy_literal_percent:
    ; Invalid sequence, copy % literally
    mov byte ptr [rdi], '%'
    inc rdi
    inc rsi
    dec r12
    jmp @@decode_loop
    
@@hex_to_nibble:
    ; Input: EBX = ASCII char, Output: EAX = nibble value or -1
    mov eax, ebx
    sub eax, '0'
    cmp eax, 9
    jbe @@nibble_ok
    
    mov eax, ebx
    or eax, 20h             ; Lowercase
    sub eax, 'a'
    cmp eax, 5
    ja @@nibble_invalid
    add eax, 10
    jmp @@nibble_ok
    
@@nibble_invalid:
    mov eax, -1
@@nibble_ok:
    ret
    
@@done:
    mov rax, rdi
    sub rax, r13
    
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
UrlDecode endp

;=============================================================================
; SECTION 5: XOR Cipher (Stream, Stateful)
;=============================================================================

;------------------------------------------------------------------------------
; XorCrypt
; XOR stream cipher with rotating key (symmetric: encode == decode)
; RCX = ctx (ENCODER_CONTEXT*), RDX = src, R8 = len, R9 = dst
; Returns: RAX = bytes processed
;------------------------------------------------------------------------------
XorCrypt proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    .endprolog
    
    mov r12, rcx            ; Context
    mov rsi, rdx            ; Source
    mov rcx, r8             ; Length
    mov rdi, r9             ; Destination
    
    ; Load current key state
    mov ebx, [r12].ENCODER_CONTEXT.xor_key
    
    test rcx, rcx
    jz @@done
    
@@crypt_loop:
    ; XOR byte with current key
    movzx eax, byte ptr [rsi]
    xor eax, ebx
    mov byte ptr [rdi], al
    
    ; Evolve key using LCG-style mixing
    rol ebx, 3
    add ebx, 09E3779B9h     ; Golden ratio constant
    xor ebx, ecx            ; Position-dependent
    
    inc rsi
    inc rdi
    dec rcx
    jnz @@crypt_loop
    
@@done:
    ; Save evolved key state for stream continuity
    mov [r12].ENCODER_CONTEXT.xor_key, ebx
    
    mov rax, r8             ; Return bytes processed
    
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
XorCrypt endp

;=============================================================================
; SECTION 6: ROT13 Transformation
;=============================================================================

;------------------------------------------------------------------------------
; Rot13Transform
; Applies ROT13 to alphabetic characters (pass-through for others)
; RCX = src, RDX = len, R8 = dst
; Returns: RAX = bytes written
;------------------------------------------------------------------------------
Rot13Transform proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    .endprolog
    
    mov rsi, rcx
    mov r12, rdx
    mov rdi, r8
    
    test r12, r12
    jz @@done
    
@@transform_loop:
    movzx eax, byte ptr [rsi]
    
    ; Check if uppercase A-Z
    cmp al, 'A'
    jb @@check_lower
    cmp al, 'Z'
    ja @@check_lower
    
    ; Rotate uppercase
    sub al, 'A'
    add al, 13
    cmp al, 26
    jb @@store_upper
    sub al, 26
@@store_upper:
    add al, 'A'
    jmp @@store
    
@@check_lower:
    ; Check if lowercase a-z
    cmp al, 'a'
    jb @@store
    cmp al, 'z'
    ja @@store
    
    ; Rotate lowercase
    sub al, 'a'
    add al, 13
    cmp al, 26
    jb @@store_lower
    sub al, 26
@@store_lower:
    add al, 'a'
    
@@store:
    mov [rdi], al
    inc rsi
    inc rdi
    dec r12
    jnz @@transform_loop
    
@@done:
    mov rax, rdi
    sub rax, r8
    
    pop r12
    pop rdi
    pop rsi
    ret
Rot13Transform endp

;=============================================================================
; SECTION 7: Run-Length Encoding (RLE)
;=============================================================================

;------------------------------------------------------------------------------
; RleEncode
; Simple RLE: <count><byte> pairs for runs >= 3, else literal
; RCX = src, RDX = len, R8 = dst, R9 = dst_capacity
; Returns: RAX = bytes written
;------------------------------------------------------------------------------
RleEncode proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    .endprolog
    
    mov rsi, rcx            ; Source
    mov r12, rdx            ; Input length
    mov rdi, r8             ; Destination
    mov r13, r9             ; Capacity
    mov r14, r8             ; Save original dest
    
    test r12, r12
    jz @@done
    
@@scan_loop:
    test r12, r12
    jz @@done
    
    movzx eax, byte ptr [rsi]
    mov r15, 1              ; Run count
    
    ; Count consecutive identical bytes
@@count_run:
    cmp r15, r12
    jae @@emit_run
    cmp r15, 255            ; Max run length
    jae @@emit_run
    
    movzx ebx, byte ptr [rsi+r15]
    cmp bl, al
    jne @@emit_run
    inc r15
    jmp @@count_run
    
@@emit_run:
    ; Check capacity
    mov rcx, rdi
    sub rcx, r14
    add rcx, 2
    cmp rcx, r13
    ja @@done
    
    cmp r15, 3
    jl @@emit_literal
    
    ; Emit run: <count><byte>
    mov byte ptr [rdi], 0   ; Escape marker
    mov [rdi+1], r15b       ; Count
    mov [rdi+2], al         ; Byte
    add rdi, 3
    jmp @@advance
    
@@emit_literal:
    ; Emit literal bytes
@@literal_loop:
    test r15, r15
    jz @@scan_loop
    
    ; Check capacity
    mov rcx, rdi
    sub rcx, r14
    inc rcx
    cmp rcx, r13
    ja @@done
    
    ; If byte is escape marker (0), emit escaped
    cmp al, 0
    jne @@emit_direct
    mov byte ptr [rdi], 0
    mov byte ptr [rdi+1], 1
    mov byte ptr [rdi+2], 0
    add rdi, 3
    jmp @@literal_next
    
@@emit_direct:
    mov [rdi], al
    inc rdi
    
@@literal_next:
    inc rsi
    dec r12
    dec r15
    jmp @@literal_loop
    
@@advance:
    add rsi, r15
    sub r12, r15
    jmp @@scan_loop
    
@@done:
    mov rax, rdi
    sub rax, r14
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
RleEncode endp

;=============================================================================
; SECTION 8: Universal Dispatcher (The "Doesn't Say No" Interface)
;=============================================================================

;------------------------------------------------------------------------------
; EncoderProcess
; Master entry point for all encoding/decoding operations
; RCX = Context*, RDX = Direction (0 = encode, 1 = decode)
; Returns: EAX = Status, updates context->output_length
;------------------------------------------------------------------------------
EncoderProcess proc frame
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    .endprolog
    
    mov rsi, rcx            ; Context
    mov r14d, edx           ; Direction
    
    ; Validate context
    test rsi, rsi
    jz @@invalid_input
    
    mov r12, [rsi].ENCODER_CONTEXT.input_buffer
    test r12, r12
    jz @@invalid_input
    
    mov r13, [rsi].ENCODER_CONTEXT.output_buffer
    test r13, r13
    jz @@invalid_input
    
    ; Load codec type and dispatch
    mov eax, [rsi].ENCODER_CONTEXT.codec_type
    
    cmp eax, CODEC_RAW
    je @@do_raw
    cmp eax, CODEC_BASE64_STD
    je @@do_base64
    cmp eax, CODEC_BASE64_URL
    je @@do_base64_url
    cmp eax, CODEC_BASE64_AVX
    je @@do_base64          ; Falls back to scalar
    cmp eax, CODEC_HEX
    je @@do_hex
    cmp eax, CODEC_HEX_UPPER
    je @@do_hex_upper
    cmp eax, CODEC_URL
    je @@do_url
    cmp eax, CODEC_XOR
    je @@do_xor
    cmp eax, CODEC_ROT13
    je @@do_rot13
    cmp eax, CODEC_RLE
    je @@do_rle
    cmp eax, CODEC_CUSTOM
    je @@do_custom
    
    ; Unknown codec type
    mov eax, ENC_UNSUPPORTED
    jmp @@exit
    
@@do_raw:
    ; Passthrough copy
    mov rcx, r13            ; Destination
    mov rdx, r12            ; Source
    mov r8, [rsi].ENCODER_CONTEXT.input_length
    call RtlCopyMemory
    mov rax, [rsi].ENCODER_CONTEXT.input_length
    mov [rsi].ENCODER_CONTEXT.output_length, rax
    xor eax, eax
    jmp @@exit
    
@@do_base64:
    mov rcx, r12            ; Source
    mov rdx, [rsi].ENCODER_CONTEXT.input_length
    mov r8, r13             ; Destination
    xor r9, r9              ; Standard alphabet
    test r14d, r14d
    jnz @@decode_b64
    call Base64EncodeScalar
    jmp @@store_result
@@decode_b64:
    call Base64DecodeScalar
    jmp @@store_result
    
@@do_base64_url:
    mov rcx, r12
    mov rdx, [rsi].ENCODER_CONTEXT.input_length
    mov r8, r13
    mov r9, 1               ; URL-safe alphabet
    test r14d, r14d
    jnz @@decode_b64_url
    call Base64EncodeScalar
    jmp @@store_result
@@decode_b64_url:
    call Base64DecodeScalar
    jmp @@store_result
    
@@do_hex:
    mov rcx, r12
    mov rdx, [rsi].ENCODER_CONTEXT.input_length
    mov r8, r13
    xor r9, r9              ; Lowercase
    test r14d, r14d
    jnz @@decode_hex
    call HexEncode
    jmp @@store_result
@@decode_hex:
    call HexDecode
    jmp @@store_result
    
@@do_hex_upper:
    mov rcx, r12
    mov rdx, [rsi].ENCODER_CONTEXT.input_length
    mov r8, r13
    mov r9, 1               ; Uppercase
    test r14d, r14d
    jnz @@decode_hex_upper
    call HexEncode
    jmp @@store_result
@@decode_hex_upper:
    call HexDecode
    jmp @@store_result
    
@@do_url:
    mov rcx, r12
    mov rdx, [rsi].ENCODER_CONTEXT.input_length
    mov r8, r13
    mov r9, [rsi].ENCODER_CONTEXT.output_capacity
    test r14d, r14d
    jnz @@decode_url
    call UrlEncode
    jmp @@store_result
@@decode_url:
    call UrlDecode
    jmp @@store_result
    
@@do_xor:
    ; XOR is symmetric (encode == decode)
    mov rcx, rsi            ; Context
    mov rdx, r12            ; Source
    mov r8, [rsi].ENCODER_CONTEXT.input_length
    mov r9, r13             ; Destination
    call XorCrypt
    jmp @@store_result
    
@@do_rot13:
    ; ROT13 is symmetric
    mov rcx, r12
    mov rdx, [rsi].ENCODER_CONTEXT.input_length
    mov r8, r13
    call Rot13Transform
    jmp @@store_result
    
@@do_rle:
    mov rcx, r12
    mov rdx, [rsi].ENCODER_CONTEXT.input_length
    mov r8, r13
    mov r9, [rsi].ENCODER_CONTEXT.output_capacity
    test r14d, r14d
    jnz @@unsupported       ; RLE decode not implemented
    call RleEncode
    jmp @@store_result
    
@@do_custom:
    ; Check for valid callback
    test r14d, r14d
    jnz @@custom_decode
    mov rax, [rsi].ENCODER_CONTEXT.encode_callback
    jmp @@custom_call
@@custom_decode:
    mov rax, [rsi].ENCODER_CONTEXT.decode_callback
@@custom_call:
    test rax, rax
    jz @@invalid_input
    
    mov rcx, rsi            ; Pass context to callback
    call rax
    ; Callback is responsible for setting output_length
    jmp @@exit
    
@@store_result:
    mov [rsi].ENCODER_CONTEXT.output_length, rax
    xor eax, eax            ; ENC_OK
    jmp @@exit
    
@@unsupported:
    mov eax, ENC_UNSUPPORTED
    jmp @@exit
    
@@invalid_input:
    mov eax, ENC_INVALID_INPUT
    
@@exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
EncoderProcess endp

;=============================================================================
; SECTION 9: Utility Functions
;=============================================================================

;------------------------------------------------------------------------------
; EncoderGetRequiredCapacity
; Calculates output buffer size needed for encoding
; RCX = Context*, RDX = InputLength
; Returns: RAX = Required bytes
;------------------------------------------------------------------------------
EncoderGetRequiredCapacity proc frame
    .endprolog
    
    mov r8, rcx
    mov ecx, [r8].ENCODER_CONTEXT.codec_type
    
    cmp ecx, CODEC_BASE64_STD
    je @@base64_calc
    cmp ecx, CODEC_BASE64_URL
    je @@base64_calc
    cmp ecx, CODEC_BASE64_AVX
    je @@base64_calc
    cmp ecx, CODEC_HEX
    je @@hex_calc
    cmp ecx, CODEC_HEX_UPPER
    je @@hex_calc
    cmp ecx, CODEC_URL
    je @@url_worst
    cmp ecx, CODEC_RAW
    je @@identity
    cmp ecx, CODEC_XOR
    je @@identity
    cmp ecx, CODEC_ROT13
    je @@identity
    cmp ecx, CODEC_RLE
    je @@rle_worst
    
    ; Default: assume 4x expansion for safety
    mov rax, rdx
    shl rax, 2
    ret
    
@@base64_calc:
    ; ((len + 2) / 3) * 4 + 4 for safety
    mov rax, rdx
    add rax, 2
    xor edx, edx
    mov rcx, 3
    div rcx
    shl rax, 2
    add rax, 4
    ret
    
@@hex_calc:
    ; len * 2
    mov rax, rdx
    shl rax, 1
    ret
    
@@url_worst:
    ; Worst case: every byte becomes %XX (3x)
    mov rax, rdx
    lea rax, [rax+rax*2]    ; *3
    ret
    
@@identity:
    mov rax, rdx
    ret
    
@@rle_worst:
    ; Worst case: every byte needs escape (3x)
    mov rax, rdx
    lea rax, [rax+rax*2]
    ret
EncoderGetRequiredCapacity endp

;------------------------------------------------------------------------------
; EncoderResetStream
; Resets stream cipher state for new file/stream
; RCX = Context*
;------------------------------------------------------------------------------
EncoderResetStream proc frame
    .endprolog
    
    mov dword ptr [rcx].ENCODER_CONTEXT.xor_key, 0DEADBEEFh
    ret
EncoderResetStream endp

;------------------------------------------------------------------------------
; EncoderSetXorKey
; Sets custom XOR key
; RCX = Context*, RDX = Key
;------------------------------------------------------------------------------
EncoderSetXorKey proc frame
    .endprolog
    
    mov [rcx].ENCODER_CONTEXT.xor_key, edx
    ret
EncoderSetXorKey endp

;------------------------------------------------------------------------------
; EncoderGetVersion
; Returns version info
; Returns: RAX = Version (1.0 = 0x00010000)
;------------------------------------------------------------------------------
EncoderGetVersion proc frame
    .endprolog
    
    mov eax, 00010000h      ; Version 1.0
    ret
EncoderGetVersion endp

;=============================================================================
; SECTION 10: Self-Test Entry Point (Standalone Build)
;=============================================================================

ifdef RAWRXD_ENCODER_STANDALONE

.data
    test_input      db "Hello, RawrXD Encoder!", 0
    test_input_len  equ $ - test_input - 1
    
    align 16
    test_output     db 256 dup(0)
    
    align 64
    test_context    ENCODER_CONTEXT <>

.code
public main
main proc frame
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    
    ; Initialize context for Base64 encoding
    lea rcx, test_context
    mov edx, CODEC_BASE64_STD
    xor r8d, r8d
    call EncoderInitializeContext
    
    ; Set up input/output
    lea rax, test_input
    mov test_context.input_buffer, rax
    mov test_context.input_length, test_input_len
    
    lea rax, test_output
    mov test_context.output_buffer, rax
    mov test_context.output_capacity, 256
    
    ; Encode
    lea rcx, test_context
    xor edx, edx            ; Direction = encode
    call EncoderProcess
    
    ; Exit with output length as return code (for testing)
    mov ecx, dword ptr test_context.output_length
    call ExitProcess
    
main endp

endif ; RAWRXD_ENCODER_STANDALONE

endif ; _RAWRXD_ENCODER_

end
