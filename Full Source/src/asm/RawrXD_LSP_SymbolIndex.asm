; =============================================================================
; RawrXD_LSP_SymbolIndex.asm — MASM64 Accelerated Symbol Index Kernels
; =============================================================================
; Three exported routines for the HotpatchSymbolProvider:
;
;   asm_symbol_hash_lookup  — Binary search for a hash in a sorted uint64 array
;   asm_batch_fnv1a         — Batch FNV-1a hash computation for string arrays
;   asm_symbol_prefix_scan  — Linear prefix-match scan over C-string pointers
;
; Calling convention: Microsoft x64 (__fastcall)
;   RCX = arg1, RDX = arg2, R8 = arg3, R9 = arg4
;   Return in RAX
;   Callee-save: RBX, RBP, RSI, RDI, R12-R15, XMM6-XMM15
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
; =============================================================================

.code

; =============================================================================
; asm_symbol_hash_lookup
; =============================================================================
; Binary search for a target hash in a sorted uint64_t array.
;
; Prototype:
;   int64_t asm_symbol_hash_lookup(
;       const uint64_t* hashArray,  ; RCX — pointer to sorted hash array
;       int64_t         count,      ; RDX — number of elements
;       uint64_t        targetHash  ; R8  — hash to search for
;   );
;
; Returns: index (0-based) of the found element, or -1 if not found.
;
; Algorithm: Classic binary search, O(log n). Cache-friendly for L1 since
;            hash arrays are contiguous uint64 values (8 bytes each).
; =============================================================================

asm_symbol_hash_lookup PROC
    ; Preserve callee-saved registers
    push    rbx
    push    rsi
    push    rdi

    ; RCX = hashArray, RDX = count, R8 = targetHash
    mov     rsi, rcx            ; rsi = hashArray base
    mov     rdi, r8             ; rdi = targetHash

    ; Setup binary search bounds: lo=0, hi=count-1
    xor     rax, rax            ; lo = 0
    mov     rbx, rdx            ; rbx = count
    dec     rbx                 ; hi = count - 1

    ; Edge case: count <= 0
    test    rdx, rdx
    jle     @@not_found

@@bsearch_loop:
    ; Check if lo > hi → not found
    cmp     rax, rbx
    jg      @@not_found

    ; mid = (lo + hi) / 2  (unsigned, no overflow for sane counts)
    mov     rcx, rax
    add     rcx, rbx
    shr     rcx, 1              ; rcx = mid

    ; Load hashArray[mid]
    mov     rdx, qword ptr [rsi + rcx*8]

    ; Compare with targetHash
    cmp     rdx, rdi
    je      @@found
    jb      @@go_right

    ; hashArray[mid] > target → hi = mid - 1
    lea     rbx, [rcx - 1]
    jmp     @@bsearch_loop

@@go_right:
    ; hashArray[mid] < target → lo = mid + 1
    lea     rax, [rcx + 1]
    jmp     @@bsearch_loop

@@found:
    ; Return mid index
    mov     rax, rcx
    jmp     @@done

@@not_found:
    ; Return -1
    mov     rax, -1

@@done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_symbol_hash_lookup ENDP


; =============================================================================
; asm_batch_fnv1a
; =============================================================================
; Batch FNV-1a hash computation for an array of C strings.
;
; Prototype:
;   int64_t asm_batch_fnv1a(
;       const char* const* strings,  ; RCX — array of string pointers
;       int64_t             count,   ; RDX — number of strings
;       uint64_t*           outHash  ; R8  — output hash array
;   );
;
; Returns: number of hashes computed (should equal count unless null ptrs).
;
; FNV-1a constants:
;   Offset basis = 14695981039346656037 (0xCBF29CE484222325)
;   Prime        = 1099511628211        (0x00000100000001B3)
; =============================================================================

FNV_OFFSET_BASIS    EQU     0CBF29CE484222325h
FNV_PRIME           EQU     000000100000001B3h

asm_batch_fnv1a PROC
    ; Preserve callee-saved registers
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    ; RCX = strings array, RDX = count, R8 = outHashes
    mov     rsi, rcx            ; rsi = strings[]
    mov     r12, rdx            ; r12 = count
    mov     r13, r8             ; r13 = outHashes[]

    xor     r14, r14            ; r14 = computed count (return value)
    xor     rbx, rbx            ; rbx = loop index i

    ; Edge case
    test    r12, r12
    jle     @@batch_done

@@batch_loop:
    cmp     rbx, r12
    jge     @@batch_done

    ; Load strings[i]
    mov     rcx, qword ptr [rsi + rbx*8]

    ; Skip null pointers
    test    rcx, rcx
    jz      @@skip_null

    ; Compute FNV-1a for this string
    mov     rax, FNV_OFFSET_BASIS   ; hash = offset_basis

@@fnv_char_loop:
    movzx   edx, byte ptr [rcx]    ; load next byte
    test    dl, dl                  ; check for null terminator
    jz      @@fnv_done

    xor     rax, rdx               ; hash ^= byte
    mov     rdi, FNV_PRIME
    imul    rax, rdi               ; hash *= prime

    inc     rcx                    ; advance to next char
    jmp     @@fnv_char_loop

@@fnv_done:
    ; Store hash in outHashes[i]
    mov     qword ptr [r13 + rbx*8], rax
    inc     r14                    ; computed++
    jmp     @@next_string

@@skip_null:
    ; Store 0 for null strings
    mov     qword ptr [r13 + rbx*8], 0

@@next_string:
    inc     rbx
    jmp     @@batch_loop

@@batch_done:
    mov     rax, r14               ; return computed count

    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_batch_fnv1a ENDP


; =============================================================================
; asm_symbol_prefix_scan
; =============================================================================
; Linear scan across an array of C-string pointers, returning the index of
; the first string that starts with the given prefix.
;
; Prototype:
;   int64_t asm_symbol_prefix_scan(
;       const char* const* names,    ; RCX — array of string pointers
;       int64_t             count,   ; RDX — number of strings
;       const char*         prefix,  ; R8  — prefix to match
;       int64_t             prefLen  ; R9  — length of prefix (bytes)
;   );
;
; Returns: index (0-based) of first match, or -1 if none found.
;
; Algorithm: For each names[i], compare first prefLen bytes against prefix.
;            Uses byte-by-byte comparison (REP CMPSB for blocks).
; =============================================================================

asm_symbol_prefix_scan PROC
    ; Preserve callee-saved registers
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    ; RCX = names[], RDX = count, R8 = prefix, R9 = prefLen
    mov     r12, rcx            ; r12 = names[]
    mov     r13, rdx            ; r13 = count
    mov     r14, r8             ; r14 = prefix
    mov     r15, r9             ; r15 = prefixLen

    xor     rbx, rbx            ; rbx = loop index i

    ; Edge cases
    test    r13, r13
    jle     @@scan_not_found
    test    r15, r15
    jle     @@scan_not_found    ; zero-length prefix matches nothing

@@scan_loop:
    cmp     rbx, r13
    jge     @@scan_not_found

    ; Load names[i]
    mov     rcx, qword ptr [r12 + rbx*8]
    test    rcx, rcx
    jz      @@scan_next         ; skip null

    ; Compare first prefLen bytes: names[i] vs prefix
    mov     rsi, rcx            ; source = names[i]
    mov     rdi, r14            ; dest   = prefix (compare target)
    mov     rcx, r15            ; count  = prefLen

    ; Byte-by-byte prefix comparison
@@cmp_loop:
    test    rcx, rcx
    jz      @@scan_found        ; all bytes matched → hit

    movzx   eax, byte ptr [rsi]
    movzx   edx, byte ptr [rdi]

    ; Check for premature null in names[i]
    test    al, al
    jz      @@scan_next         ; names[i] shorter than prefix

    cmp     al, dl
    jne     @@scan_next         ; mismatch

    inc     rsi
    inc     rdi
    dec     rcx
    jmp     @@cmp_loop

@@scan_found:
    mov     rax, rbx            ; return index i
    jmp     @@scan_done

@@scan_next:
    inc     rbx
    jmp     @@scan_loop

@@scan_not_found:
    mov     rax, -1

@@scan_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_symbol_prefix_scan ENDP

END
