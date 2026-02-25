; ============================================================================
; RawrXD_RefProvider.asm — Phase 29.2: Reference Provider MASM64 Kernels
; ============================================================================
;
; PURPOSE:
;   MASM64 accelerated routines for the multi-reference provider system:
;   1. REF_HashReference    — FNV-1a hash for reference deduplication
;   2. REF_PatternMatch     — Wildcard pattern match (case-insensitive)
;   3. REF_ScanSymbolName   — SIMD-accelerated substring search for symbol names
;
; CALLING CONVENTION: x64 Microsoft (RCX, RDX, R8, R9 → return RAX)
; ALIGNMENT:          All data 16-byte aligned for SSE/AVX
; RULE:               NO SOURCE FILE IS TO BE SIMPLIFIED
; ============================================================================

.code

; ============================================================================
; REF_HashReference — FNV-1a hash of a reference location
; ============================================================================
;
; uint64_t REF_HashReference(
;     const char* moduleName,    ; RCX — null-terminated module name
;     const char* symbolName,    ; RDX — null-terminated symbol name
;     uint64_t    rva            ; R8  — RVA
; );
;
; Returns: 64-bit FNV-1a hash (case-insensitive)
;
; Used by ReferenceRouter::deduplicateResults() for O(1) dedup.
; This kernel replaces the C++ loop with tighter register usage.
;
REF_HashReference PROC
    ; FNV-1a offset basis
    mov     rax, 14695981039346656037   ; FNV_OFFSET_BASIS_64
    mov     r9, 1099511628211           ; FNV_PRIME_64

    ; --- Hash module name (case-insensitive) ---
    test    rcx, rcx
    jz      hash_symbol

hash_mod_loop:
    movzx   r10d, BYTE PTR [rcx]
    test    r10d, r10d
    jz      hash_symbol

    ; tolower: if 'A'..'Z', add 0x20
    cmp     r10b, 'A'
    jb      hash_mod_xor
    cmp     r10b, 'Z'
    ja      hash_mod_xor
    add     r10b, 20h

hash_mod_xor:
    xor     rax, r10
    imul    rax, r9                 ; hash *= FNV_PRIME
    inc     rcx
    jmp     hash_mod_loop

    ; --- Hash symbol name (case-insensitive) ---
hash_symbol:
    test    rdx, rdx
    jz      hash_rva

hash_sym_loop:
    movzx   r10d, BYTE PTR [rdx]
    test    r10d, r10d
    jz      hash_rva

    cmp     r10b, 'A'
    jb      hash_sym_xor
    cmp     r10b, 'Z'
    ja      hash_sym_xor
    add     r10b, 20h

hash_sym_xor:
    xor     rax, r10
    imul    rax, r9
    inc     rdx
    jmp     hash_sym_loop

    ; --- Hash RVA (8 bytes) ---
hash_rva:
    ; Unrolled 8-byte hash of R8
    mov     r10, r8

    movzx   r11d, r10b
    xor     rax, r11
    imul    rax, r9
    shr     r10, 8

    movzx   r11d, r10b
    xor     rax, r11
    imul    rax, r9
    shr     r10, 8

    movzx   r11d, r10b
    xor     rax, r11
    imul    rax, r9
    shr     r10, 8

    movzx   r11d, r10b
    xor     rax, r11
    imul    rax, r9
    shr     r10, 8

    movzx   r11d, r10b
    xor     rax, r11
    imul    rax, r9
    shr     r10, 8

    movzx   r11d, r10b
    xor     rax, r11
    imul    rax, r9
    shr     r10, 8

    movzx   r11d, r10b
    xor     rax, r11
    imul    rax, r9
    shr     r10, 8

    movzx   r11d, r10b
    xor     rax, r11
    imul    rax, r9

    ret
REF_HashReference ENDP


; ============================================================================
; REF_PatternMatch — Simple wildcard pattern match (case-insensitive)
; ============================================================================
;
; uint32_t REF_PatternMatch(
;     const char* pattern,       ; RCX — pattern with optional * and ?
;     const char* name           ; RDX — string to match against
; );
;
; Returns: 1 if match, 0 if no match
;
; Implements the classic two-pointer wildcard algorithm:
;   '*' matches zero or more characters
;   '?' matches exactly one character
;   All comparisons are case-insensitive
;
REF_PatternMatch PROC
    push    rbx
    push    rsi
    push    rdi
    push    rbp

    mov     rsi, rcx                ; RSI = pattern pointer
    mov     rdi, rdx                ; RDI = name pointer
    xor     rbp, rbp                ; RBP = star_pattern (0 = no star saved)
    xor     rbx, rbx                ; RBX = star_name (0 = no star saved)

pm_loop:
    ; Load pattern char
    movzx   eax, BYTE PTR [rsi]
    ; Load name char
    movzx   ecx, BYTE PTR [rdi]

    ; Check if pattern char is '?'
    cmp     al, '?'
    je      pm_question

    ; Check if pattern char is '*'
    cmp     al, '*'
    je      pm_star

    ; Compare chars (case-insensitive)
    ; tolower(pattern)
    mov     r8d, eax
    cmp     r8b, 'A'
    jb      pm_cmp_name
    cmp     r8b, 'Z'
    ja      pm_cmp_name
    add     r8b, 20h

pm_cmp_name:
    ; tolower(name)
    mov     r9d, ecx
    cmp     r9b, 'A'
    jb      pm_compare
    cmp     r9b, 'Z'
    ja      pm_compare
    add     r9b, 20h

pm_compare:
    cmp     r8d, r9d
    je      pm_advance

    ; Chars don't match — can we backtrack to a star?
    test    rbp, rbp
    jz      pm_fail                 ; No star saved → fail

    ; Backtrack: restore to star position and advance name by one
    mov     rsi, rbp
    inc     rsi                     ; Skip past the '*'
    inc     rbx                     ; Advance star_name
    mov     rdi, rbx
    jmp     pm_loop

pm_question:
    ; '?' matches one char (but not end of string)
    test    ecx, ecx
    jz      pm_check_star           ; '?' at end of name — check if we can recover

pm_advance:
    inc     rsi
    inc     rdi
    jmp     pm_loop

pm_star:
    ; Save star position
    mov     rbp, rsi                ; star_pattern = current pattern pos
    inc     rsi                     ; Skip '*'
    mov     rbx, rdi                ; star_name = current name pos
    jmp     pm_loop

pm_check_star:
    ; Name ended but pattern has '?' — can't match
    test    rbp, rbp
    jz      pm_fail
    mov     rsi, rbp
    inc     rsi
    inc     rbx
    mov     rdi, rbx
    jmp     pm_loop

pm_end_check:
    ; Name is exhausted. Skip any trailing '*' in pattern.
    movzx   eax, BYTE PTR [rsi]
    test    eax, eax
    jz      pm_match                ; Both exhausted → match
    cmp     al, '*'
    jne     pm_fail
    inc     rsi
    jmp     pm_end_check

pm_fail:
    xor     eax, eax                ; Return 0 (no match)
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

pm_match:
    mov     eax, 1                  ; Return 1 (match)
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

REF_PatternMatch ENDP


; ============================================================================
; REF_ScanSymbolName — Case-insensitive substring search
; ============================================================================
;
; uint32_t REF_ScanSymbolName(
;     const char* haystack,      ; RCX — string to search in
;     uint32_t    haystackLen,   ; EDX — length of haystack
;     const char* needle,        ; R8  — substring to find
;     uint32_t    needleLen      ; R9D — length of needle
; );
;
; Returns: offset of first match, or 0xFFFFFFFF if not found
;
; Simple scalar implementation. Phase 29.3: SSE4.2 PCMPESTRI version.
;
REF_ScanSymbolName PROC
    push    rbx
    push    rsi
    push    rdi
    push    rbp

    mov     rsi, rcx                ; RSI = haystack
    mov     ebx, edx                ; EBX = haystackLen
    mov     rdi, r8                 ; RDI = needle
    mov     ebp, r9d                ; EBP = needleLen

    ; Edge cases
    test    ebp, ebp
    jz      scan_fail               ; Empty needle → not found
    cmp     ebp, ebx
    ja      scan_fail               ; Needle longer than haystack → not found

    ; Calculate last valid start position
    mov     ecx, ebx
    sub     ecx, ebp                ; ECX = max starting offset
    xor     edx, edx                ; EDX = current offset

scan_outer:
    cmp     edx, ecx
    ja      scan_fail               ; Past last valid position

    ; Compare needle at haystack[edx]
    xor     r8d, r8d                ; R8D = needle index

scan_inner:
    cmp     r8d, ebp
    jge     scan_found              ; All chars matched

    ; Load haystack[edx + r8d]
    mov     eax, edx
    add     eax, r8d
    movzx   r10d, BYTE PTR [rsi + rax]

    ; Load needle[r8d]
    movzx   r11d, BYTE PTR [rdi + r8]

    ; tolower both
    cmp     r10b, 'A'
    jb      scan_skip_lower1
    cmp     r10b, 'Z'
    ja      scan_skip_lower1
    add     r10b, 20h
scan_skip_lower1:
    cmp     r11b, 'A'
    jb      scan_skip_lower2
    cmp     r11b, 'Z'
    ja      scan_skip_lower2
    add     r11b, 20h
scan_skip_lower2:

    cmp     r10d, r11d
    jne     scan_next               ; Mismatch — try next offset

    inc     r8d
    jmp     scan_inner

scan_found:
    mov     eax, edx                ; Return matching offset
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

scan_next:
    inc     edx
    jmp     scan_outer

scan_fail:
    mov     eax, 0FFFFFFFFh         ; Return NOT_FOUND
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

REF_ScanSymbolName ENDP

END
