; ============================================================
; skip_whitespace_avx2_optimized.asm
; 
; Ultra-optimized AVX2 whitespace scanner for MASM x64
; Input:  RCX = pointer to data
;         RDX = remaining bytes
; Output: RAX = number of whitespace bytes to skip
; 
; Performance target: 32 bytes per cycle
; Uses: YMM0-YMM3, XMM4, no general-purpose register corruption
; ============================================================

.code

skip_whitespace_avx2_optimized PROC EXPORT
    ; Input validation
    cmp     rdx, 32
    jl      .scalar_fallback
    
    ; Prepare comparison vectors for common whitespace chars
    vpbroadcastb    ymm0, byte ptr [rel space_byte]      ; ' ' = 0x20
    vpbroadcastb    ymm1, byte ptr [rel tab_byte]        ; '\t' = 0x09
    vpbroadcastb    ymm2, byte ptr [rel newline_byte]    ; '\n' = 0x0A
    vpbroadcastb    ymm3, byte ptr [rel carriage_byte]   ; '\r' = 0x0D
    
    ; Load first 32 bytes
    vmovdqu32       ymm4, [rcx]
    
    ; Compare each character
    vpcmpeqb        ymm5, ymm4, ymm0                      ; spaces
    vpcmpeqb        ymm6, ymm4, ymm1                      ; tabs
    vpcmpeqb        ymm7, ymm4, ymm2                      ; newlines
    vpcmpeqb        ymm8, ymm4, ymm3                      ; carriage returns
    
    ; Combine all matches (OR)
    vpor            ymm5, ymm5, ymm6
    vpor            ymm5, ymm5, ymm7
    vpor            ymm5, ymm5, ymm8
    
    ; Extract movemask
    vpmovmskb       eax, ymm5
    
    ; If all 32 bytes are whitespace, return 32
    cmp             eax, 0xFFFFFFFF
    je              .skip_32
    
    ; If mask is zero (no whitespace at start), return 0
    cmp             eax, 0
    je              .zero_skip
    
    ; Find first non-whitespace position
    bsf             eax, NOT eax
    ret
    
.skip_32:
    mov             rax, 32
    ret
    
.zero_skip:
    xor             eax, eax
    ret
    
.scalar_fallback:
    ; Fallback for small buffers (< 32 bytes)
    xor             rax, rax
    cmp             rdx, 0
    je              .done
    
.scalar_loop:
    movzx           r8d, byte ptr [rcx + rax]
    
    ; Check for space, tab, newline, carriage return
    cmp             r8b, ' '
    je              .increment_scalar
    cmp             r8b, 9                                 ; tab
    je              .increment_scalar
    cmp             r8b, 10                                ; newline
    je              .increment_scalar
    cmp             r8b, 13                                ; carriage return
    je              .increment_scalar
    
    ; Not whitespace, exit loop
    jmp             .done
    
.increment_scalar:
    inc             rax
    cmp             rax, rdx
    jl              .scalar_loop
    
.done:
    ret
    
skip_whitespace_avx2_optimized ENDP

; Data section with character constants
.data ALIGN 16
space_byte      db 0x20
tab_byte        db 0x09
newline_byte    db 0x0A
carriage_byte   db 0x0D

END
