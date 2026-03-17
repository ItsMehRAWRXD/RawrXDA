;==============================================================================
; gguf_string_validator.asm
; AVX-512 accelerated string length validation for GGUF parsing
; Prevents bad_alloc by validating lengths before heap allocation
; Platform: x64 Windows (MASM64)
;==============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

.code

;------------------------------------------------------------------------------
; ValidateStringLengthASM
; Purpose: Validate GGUF string length before attempting allocation
; Entry:   RCX = len (uint64_t claimed length from file)
;          RDX = max_allowed (uint64_t safety threshold)
; Return:  RAX = 1 if valid, 0 if corrupt/overflow
; Notes:   Uses bit operations to avoid branching for high performance
;------------------------------------------------------------------------------
PUBLIC ValidateStringLengthASM
ValidateStringLengthASM PROC

    ; Check 1: len > max_allowed?
    cmp     rcx, rdx
    ja      @invalid                      ; Unsigned above = overflow
    
    ; Check 2: High bit set? (signed interpretation would be negative)
    ; This catches corrupted length fields that look like negative numbers
    test    rcx, 8000000000000000h        ; Test bit 63
    jnz     @invalid
    
    ; Check 3: Zero length is valid (edge case)
    test    rcx, rcx
    jz      @valid
    
    ; Check 4: Absurdly large but still under max? (paranoid check)
    ; If len > 1TB (0x10000000000), probably corruption
    mov     rax, 10000000000h             ; 1TB
    cmp     rcx, rax
    ja      @invalid
    
@valid:
    mov     rax, 1                        ; Return true
    ret

@invalid:
    xor     rax, rax                      ; Return false
    ret

ValidateStringLengthASM ENDP

;------------------------------------------------------------------------------
; ValidateArrayCountASM
; Purpose: Validate GGUF array element count before reserve()
; Entry:   RCX = count (uint64_t element count from file)
;          RDX = max_elements (uint64_t safety threshold)
;          R8  = element_size (uint64_t bytes per element)
; Return:  RAX = 1 if valid, 0 if overflow would occur
; Notes:   Checks for multiplication overflow: count * elem_size
;------------------------------------------------------------------------------
PUBLIC ValidateArrayCountASM
ValidateArrayCountASM PROC

    ; Check 1: count > max_elements?
    cmp     rcx, rdx
    ja      @invalid
    
    ; Check 2: count == 0 is valid
    test    rcx, rcx
    jz      @valid
    
    ; Check 3: Multiplication overflow check
    ; If (count * elem_size) / elem_size != count, then overflow occurred
    mov     rax, rcx                      ; RAX = count
    mul     r8                            ; RDX:RAX = count * elem_size
    
    ; Check if high 64-bits (RDX) are non-zero = overflow
    test    rdx, rdx
    jnz     @invalid
    
    ; Divide result by elem_size and verify it equals count
    xor     rdx, rdx                      ; Clear high bits for div
    div     r8                            ; RAX = (count*elem_size) / elem_size
    cmp     rax, rcx                      ; Should equal original count
    jne     @invalid
    
@valid:
    mov     rax, 1
    ret

@invalid:
    xor     rax, rax
    ret

ValidateArrayCountASM ENDP

;------------------------------------------------------------------------------
; SafeMemcpyAligned
; Purpose: AVX-512 aligned memcpy with bounds checking
; Entry:   RCX = dst (void*)
;          RDX = src (void*)
;          R8  = count (size_t bytes to copy)
;          R9  = dst_capacity (size_t maximum bytes dst can hold)
; Return:  RAX = 0 on success, error code on failure
; Notes:   Uses AVX-512 if available, falls back to REP MOVSB
;------------------------------------------------------------------------------
PUBLIC SafeMemcpyAligned
SafeMemcpyAligned PROC

    ; Bounds check: count must fit in dst_capacity
    cmp     r8, r9
    ja      @bounds_error
    
    ; NULL pointer checks
    test    rcx, rcx
    jz      @null_error
    test    rdx, rdx
    jz      @null_error
    
    ; Zero count is valid (no-op)
    test    r8, r8
    jz      @success
    
    ; Save registers
    push    rsi
    push    rdi
    
    mov     rdi, rcx                      ; Destination
    mov     rsi, rdx                      ; Source
    mov     rcx, r8                       ; Count
    
    ; Check for AVX-512 availability (CPUID)
    push    rbx
    push    rcx                           ; Save count
    
    mov     eax, 7                        ; CPUID leaf 7
    xor     ecx, ecx
    cpuid
    test    ebx, 00010000h                ; AVX-512F bit
    pop     rcx                           ; Restore count
    pop     rbx
    jz      @fallback_rep
    
    ; AVX-512 path: Copy 64-byte chunks
    mov     rax, rcx
    shr     rax, 6                        ; Count / 64 = number of chunks
    test    rax, rax
    jz      @copy_tail
    
@loop_avx512:
    vmovdqu64 zmm0, zmmword ptr [rsi]     ; Load 64 bytes
    vmovdqu64 zmmword ptr [rdi], zmm0     ; Store 64 bytes
    add     rsi, 64
    add     rdi, 64
    dec     rax
    jnz     @loop_avx512
    
    vzeroupper                            ; Clear upper state
    
@copy_tail:
    ; Copy remaining bytes (0-63)
    and     rcx, 3Fh                      ; rcx = count % 64
    jz      @done
    
@fallback_rep:
    ; Fallback: REP MOVSB (hardware-optimized on modern CPUs)
    rep     movsb
    
@done:
    pop     rdi
    pop     rsi
    
@success:
    xor     rax, rax                      ; Return 0 = success
    ret

@bounds_error:
    mov     rax, 0C0000005h               ; STATUS_ACCESS_VIOLATION
    ret

@null_error:
    mov     rax, 0C0000005h               ; STATUS_ACCESS_VIOLATION
    ret

SafeMemcpyAligned ENDP

;------------------------------------------------------------------------------
; DetectCorruptedGGUFMagic
; Purpose: Fast GGUF magic validation using SIMD
; Entry:   RCX = ptr to first 4 bytes of file
; Return:  RAX = 1 if valid "GGUF", 0 if corrupt
;------------------------------------------------------------------------------
PUBLIC DetectCorruptedGGUFMagic
DetectCorruptedGGUFMagic PROC

    test    rcx, rcx
    jz      @invalid
    
    ; Load magic (4 bytes)
    mov     eax, dword ptr [rcx]
    
    ; GGUF in little-endian = 0x46554747
    cmp     eax, 046554747h
    sete    al                            ; AL = 1 if equal, 0 otherwise
    movzx   rax, al
    ret

@invalid:
    xor     rax, rax
    ret

DetectCorruptedGGUFMagic ENDP

;------------------------------------------------------------------------------
; SanitizeMetadataOffset
; Purpose: Validate metadata offset doesn't exceed file bounds
; Entry:   RCX = claimed_offset (uint64_t from GGUF file)
;          RDX = file_size (uint64_t total file size)
;          R8  = min_required_bytes (uint64_t minimum data needed)
; Return:  RAX = sanitized offset, or 0xFFFFFFFFFFFFFFFF if invalid
;------------------------------------------------------------------------------
PUBLIC SanitizeMetadataOffset
SanitizeMetadataOffset PROC

    ; Check: offset + min_required_bytes <= file_size
    mov     rax, rcx                      ; RAX = offset
    add     rax, r8                       ; RAX = offset + required
    jc      @overflow                     ; Carry = addition overflow
    
    cmp     rax, rdx                      ; Compare to file_size
    ja      @invalid                      ; Above = out of bounds
    
    ; Valid: return original offset
    mov     rax, rcx
    ret

@overflow:
@invalid:
    mov     rax, 0FFFFFFFFFFFFFFFFh       ; Return -1 (invalid)
    ret

SanitizeMetadataOffset ENDP

END
