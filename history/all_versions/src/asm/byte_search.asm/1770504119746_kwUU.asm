; =============================================================================
; byte_search.asm — Byte-Level Search ASM Kernel
; =============================================================================
; SIMD-accelerated pattern search for Layer 2 byte-level hotpatching.
; Used by byte_level_hotpatcher.cpp for GGUF binary modification.
;
; Exports:
;   find_pattern_asm     — Linear byte pattern search (baseline)
;   asm_byte_search      — SIMD-accelerated pattern search (SSE2)
;   asm_boyer_moore_search — Boyer-Moore with precomputed skip table
;
; Architecture: x64 MASM | Windows ABI | No exceptions | No CRT
; Build: ml64.exe /c /Zi /Zd /Fo byte_search.obj byte_search.asm
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC find_pattern_asm
PUBLIC asm_byte_search
PUBLIC asm_boyer_moore_search

; =============================================================================
;                            CODE
; =============================================================================
.code

; =============================================================================
; find_pattern_asm
; Linear byte pattern search (baseline, no SIMD).
;
; RCX = haystack pointer
; RDX = haystack length
; R8  = needle pointer
; R9  = needle length
;
; Returns: RAX = pointer to match, or 0 (NULL) if not found
; =============================================================================
find_pattern_asm PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    .endprolog

    ; Validate inputs
    test    rcx, rcx
    jz      @@fp_notfound
    test    r8, r8
    jz      @@fp_notfound
    test    r9, r9
    jz      @@fp_notfound
    cmp     r9, rdx
    ja      @@fp_notfound       ; needle longer than haystack

    mov     rsi, rcx            ; rsi = haystack
    mov     r12, rdx            ; r12 = haystack_len
    mov     rdi, r8             ; rdi = needle
    mov     r13, r9             ; r13 = needle_len

    ; Calculate search limit
    mov     r14, r12
    sub     r14, r13            ; r14 = max starting position
    xor     rbx, rbx            ; rbx = current position

@@fp_loop:
    cmp     rbx, r14
    ja      @@fp_notfound

    ; Compare needle at current position
    xor     rcx, rcx            ; byte index into needle
@@fp_cmp:
    cmp     rcx, r13
    jae     @@fp_found          ; All bytes matched

    movzx   eax, BYTE PTR [rsi + rbx]
    add     rax, rcx            ; adjust: we need [rsi + rbx + rcx]
    ; Redo: load byte at rsi + rbx + rcx
    lea     rax, [rsi + rbx]
    movzx   eax, BYTE PTR [rax + rcx]
    movzx   edx, BYTE PTR [rdi + rcx]
    cmp     al, dl
    jne     @@fp_next

    inc     rcx
    jmp     @@fp_cmp

@@fp_next:
    inc     rbx
    jmp     @@fp_loop

@@fp_found:
    lea     rax, [rsi + rbx]    ; Return pointer to match
    jmp     @@fp_done

@@fp_notfound:
    xor     eax, eax            ; Return NULL

@@fp_done:
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
find_pattern_asm ENDP

; =============================================================================
; asm_byte_search
; SIMD-accelerated pattern search using SSE2 first-byte filtering.
; Falls back to linear comparison after first-byte match.
;
; RCX = haystack pointer
; RDX = haystack length
; R8  = needle pointer
; R9  = needle length
;
; Returns: RAX = pointer to match, or 0 (NULL) if not found
; =============================================================================
asm_byte_search PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    .endprolog

    test    rcx, rcx
    jz      @@bs_notfound
    test    r8, r8
    jz      @@bs_notfound
    test    r9, r9
    jz      @@bs_notfound
    cmp     r9, rdx
    ja      @@bs_notfound

    mov     rsi, rcx            ; haystack
    mov     r12, rdx            ; haystack_len
    mov     rdi, r8             ; needle
    mov     r13, r9             ; needle_len

    ; Broadcast first byte of needle into XMM0
    movzx   eax, BYTE PTR [rdi]
    movd    xmm0, eax
    punpcklbw xmm0, xmm0
    punpcklwd xmm0, xmm0
    pshufd  xmm0, xmm0, 0      ; XMM0 = 16 copies of first byte

    mov     r14, r12
    sub     r14, r13            ; max start position
    xor     rbx, rbx            ; current offset

@@bs_simd_loop:
    ; Can we read 16 bytes from current position?
    lea     rax, [rbx + 16]
    cmp     rax, r12
    ja      @@bs_scalar         ; Not enough bytes for SIMD, fall back

    cmp     rbx, r14
    ja      @@bs_notfound

    ; Load 16 bytes from haystack
    movdqu  xmm1, XMMWORD PTR [rsi + rbx]
    pcmpeqb xmm1, xmm0         ; Compare with first needle byte
    pmovmskb eax, xmm1         ; Extract mask
    test    eax, eax
    jz      @@bs_advance16      ; No matches in this 16-byte chunk

    ; Process each matching bit
@@bs_bit_loop:
    bsf     ecx, eax            ; Find lowest set bit
    jz      @@bs_advance16      ; No more bits

    ; Verify full needle match at this position
    lea     r15, [rbx + rcx]    ; candidate position
    cmp     r15, r14
    ja      @@bs_notfound

    ; Compare full needle
    push    rax
    xor     r8, r8              ; byte index
@@bs_verify:
    cmp     r8, r13
    jae     @@bs_match_pop

    movzx   edx, BYTE PTR [rsi + r15]
    add     rdx, r8             ; need [rsi + r15 + r8]
    lea     rdx, [rsi + r15]
    movzx   edx, BYTE PTR [rdx + r8]
    movzx   ecx, BYTE PTR [rdi + r8]
    cmp     dl, cl
    jne     @@bs_no_match_pop

    inc     r8
    jmp     @@bs_verify

@@bs_match_pop:
    pop     rax
    lea     rax, [rsi + r15]
    jmp     @@bs_done

@@bs_no_match_pop:
    pop     rax
    btr     eax, ecx            ; Clear this bit
    jmp     @@bs_bit_loop

@@bs_advance16:
    add     rbx, 16
    jmp     @@bs_simd_loop

@@bs_scalar:
    ; Fall back to byte-by-byte for remaining bytes
    cmp     rbx, r14
    ja      @@bs_notfound

    movzx   eax, BYTE PTR [rsi + rbx]
    movzx   ecx, BYTE PTR [rdi]
    cmp     al, cl
    jne     @@bs_scalar_next

    ; Verify full needle
    xor     r8, r8
@@bs_scalar_cmp:
    cmp     r8, r13
    jae     @@bs_scalar_found

    lea     rax, [rsi + rbx]
    movzx   eax, BYTE PTR [rax + r8]
    movzx   ecx, BYTE PTR [rdi + r8]
    cmp     al, cl
    jne     @@bs_scalar_next
    inc     r8
    jmp     @@bs_scalar_cmp

@@bs_scalar_found:
    lea     rax, [rsi + rbx]
    jmp     @@bs_done

@@bs_scalar_next:
    inc     rbx
    jmp     @@bs_scalar

@@bs_notfound:
    xor     eax, eax

@@bs_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_byte_search ENDP

; =============================================================================
; asm_boyer_moore_search
; Boyer-Moore search with caller-provided skip table.
;
; RCX = haystack pointer
; RDX = haystack length
; R8  = needle pointer
; R9  = needle length
; [RSP+40] = skip table pointer (256 ints)
;
; Returns: RAX = pointer to match, or 0 (NULL) if not found
; =============================================================================
asm_boyer_moore_search PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    .endprolog

    test    rcx, rcx
    jz      @@bm_notfound
    test    r8, r8
    jz      @@bm_notfound
    test    r9, r9
    jz      @@bm_notfound
    cmp     r9, rdx
    ja      @@bm_notfound

    mov     rsi, rcx            ; haystack
    mov     r12, rdx            ; haystack_len
    mov     rdi, r8             ; needle
    mov     r13, r9             ; needle_len
    mov     r14, QWORD PTR [rsp + 96]  ; skip_table (after 7 pushes + ret addr = 64 bytes)

    test    r14, r14
    jz      @@bm_notfound       ; null skip table

    ; Boyer-Moore main loop
    mov     rbx, r13
    dec     rbx                 ; rbx = needle_len - 1 (last index)

@@bm_loop:
    cmp     rbx, r12
    jae     @@bm_notfound

    ; Compare from right to left
    mov     r15, r13
    dec     r15                 ; r15 = pattern index (start from end)

@@bm_cmp:
    cmp     r15, 0
    jl      @@bm_found          ; All bytes matched (r15 went below 0 = signed)
    ; Actually need to check if we matched all — use unsigned
    ; r15 counts down from needle_len-1 to 0
    mov     rax, rbx
    sub     rax, r13
    add     rax, r15
    inc     rax                 ; rax = haystack position for this needle byte
    movzx   ecx, BYTE PTR [rsi + rax]
    movzx   edx, BYTE PTR [rdi + r15]
    cmp     cl, dl
    jne     @@bm_skip

    dec     r15
    ; Check if r15 went to -1 (unsigned: very large)
    cmp     r15, r13
    ja      @@bm_found          ; Wrapped around = all matched
    jmp     @@bm_cmp

@@bm_skip:
    ; Use skip table: skip = skip_table[haystack[rbx]]
    movzx   ecx, BYTE PTR [rsi + rbx]
    movsxd  rax, DWORD PTR [r14 + rcx * 4]
    cmp     rax, 1
    jge     @@bm_apply_skip
    mov     rax, 1              ; Minimum skip of 1
@@bm_apply_skip:
    add     rbx, rax
    jmp     @@bm_loop

@@bm_found:
    ; Match starts at haystack position (rbx - needle_len + 1)
    mov     rax, rbx
    sub     rax, r13
    inc     rax
    lea     rax, [rsi + rax]
    jmp     @@bm_done

@@bm_notfound:
    xor     eax, eax

@@bm_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_boyer_moore_search ENDP

END
