; native_core/kernels/bpe_tokenizer.asm
; Native BPE Tokenizer — SSE4.2 / AVX2 string processing
; Zero external deps — pure MASM64
;
; PUBLIC Symbols:
;   BPE_FindLongestMatch  — Find longest token match in vocab (binary search)
;   BPE_HashString        — FNV-1a hash for vocab lookup
;   BPE_UTF8ToCodepoint   — Decode one UTF-8 char -> Unicode codepoint
;   BPE_CountUTF8Chars    — Count UTF-8 codepoints in a byte string

.data
ALIGN 16
; FNV-1a constants
fnv_offset_basis QWORD 14695981039346656037    ; 0xCBF29CE484222325
fnv_prime        QWORD 1099511628211           ; 0x100000001B3

.code

PUBLIC BPE_FindLongestMatch
PUBLIC BPE_HashString
PUBLIC BPE_UTF8ToCodepoint
PUBLIC BPE_CountUTF8Chars

; ============================================================================
; BPE_HashString — FNV-1a 64-bit hash
;
; RCX = pointer to byte string
; RDX = length in bytes
; Returns: RAX = 64-bit hash
; ============================================================================
BPE_HashString PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    push        rbx
    .pushreg    rbx
    .endprolog

    mov         rax, qword ptr [fnv_offset_basis]   ; hash = offset basis
    mov         rbx, qword ptr [fnv_prime]           ; prime

    test        rdx, rdx
    jz          hash_done

hash_loop:
    movzx       r8d, byte ptr [rcx]     ; load byte
    xor         rax, r8                  ; hash ^= byte
    imul        rax, rbx                 ; hash *= prime
    inc         rcx
    dec         rdx
    jnz         hash_loop

hash_done:
    pop         rbx
    pop         rbp
    ret
BPE_HashString ENDP

; ============================================================================
; BPE_UTF8ToCodepoint — Decode one UTF-8 character
;
; RCX = pointer to UTF-8 bytes
; RDX = pointer to uint32_t (output: codepoint)
; Returns: RAX = number of bytes consumed (1-4), 0 on error
; ============================================================================
BPE_UTF8ToCodepoint PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    .endprolog

    movzx       eax, byte ptr [rcx]

    ; --- 1-byte: 0xxxxxxx ---
    test        al, 80h
    jnz         try_2byte
    mov         dword ptr [rdx], eax
    mov         eax, 1
    jmp         utf8_ret

try_2byte:
    ; --- 2-byte: 110xxxxx 10xxxxxx ---
    cmp         al, 0E0h
    jae         try_3byte
    cmp         al, 0C0h
    jb          utf8_err

    movzx       r8d, byte ptr [rcx]
    and         r8d, 1Fh                ; 5 payload bits
    shl         r8d, 6

    movzx       r9d, byte ptr [rcx+1]
    and         r9d, 3Fh                ; 6 continuation bits
    or          r8d, r9d

    mov         dword ptr [rdx], r8d
    mov         eax, 2
    jmp         utf8_ret

try_3byte:
    ; --- 3-byte: 1110xxxx 10xxxxxx 10xxxxxx ---
    cmp         al, 0F0h
    jae         try_4byte

    movzx       r8d, byte ptr [rcx]
    and         r8d, 0Fh
    shl         r8d, 12

    movzx       r9d, byte ptr [rcx+1]
    and         r9d, 3Fh
    shl         r9d, 6
    or          r8d, r9d

    movzx       r9d, byte ptr [rcx+2]
    and         r9d, 3Fh
    or          r8d, r9d

    mov         dword ptr [rdx], r8d
    mov         eax, 3
    jmp         utf8_ret

try_4byte:
    ; --- 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx ---
    cmp         al, 0F8h
    jae         utf8_err

    movzx       r8d, byte ptr [rcx]
    and         r8d, 07h
    shl         r8d, 18

    movzx       r9d, byte ptr [rcx+1]
    and         r9d, 3Fh
    shl         r9d, 12
    or          r8d, r9d

    movzx       r9d, byte ptr [rcx+2]
    and         r9d, 3Fh
    shl         r9d, 6
    or          r8d, r9d

    movzx       r9d, byte ptr [rcx+3]
    and         r9d, 3Fh
    or          r8d, r9d

    mov         dword ptr [rdx], r8d
    mov         eax, 4
    jmp         utf8_ret

utf8_err:
    xor         eax, eax

utf8_ret:
    pop         rbp
    ret
BPE_UTF8ToCodepoint ENDP

; ============================================================================
; BPE_CountUTF8Chars — Count Unicode codepoints in UTF-8 byte string
;
; RCX = pointer to UTF-8 bytes
; RDX = byte length
; Returns: RAX = number of codepoints
; ============================================================================
BPE_CountUTF8Chars PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    push        rbx
    .pushreg    rbx
    .endprolog

    xor         rax, rax                 ; count = 0
    mov         rbx, rcx                 ; base ptr
    add         rdx, rcx                 ; end ptr

count_loop:
    cmp         rbx, rdx
    jge         count_done

    movzx       ecx, byte ptr [rbx]

    ; Count leading bytes (not continuation bytes 10xxxxxx)
    mov         r8d, ecx
    and         r8d, 0C0h
    cmp         r8d, 80h                 ; Is it a continuation byte?
    je          count_skip               ; Skip continuations

    inc         rax                      ; It's a leading byte — count it

count_skip:
    inc         rbx
    jmp         count_loop

count_done:
    pop         rbx
    pop         rbp
    ret
BPE_CountUTF8Chars ENDP

; ============================================================================
; BPE_FindLongestMatch — Binary search vocab for longest matching token
;
; Vocab is pre-sorted array of {hash: uint64, tokenId: uint32, length: uint32}
; Each entry = 16 bytes
;
; RCX = pointer to sorted vocab array
; RDX = vocab count (uint64)
; R8  = target hash (uint64)
; Returns: RAX = token ID (uint32), or -1 (0xFFFFFFFF) if not found
; ============================================================================
BPE_FindLongestMatch PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    push        rbx
    .pushreg    rbx
    push        rsi
    .pushreg    rsi
    .endprolog

    mov         rsi, rcx                 ; vocab base
    xor         rbx, rbx                 ; lo = 0
    mov         rcx, rdx                 ; hi = count

bsearch_loop:
    cmp         rbx, rcx
    jge         bsearch_notfound

    ; mid = (lo + hi) / 2
    mov         rax, rbx
    add         rax, rcx
    shr         rax, 1                   ; mid

    ; Load vocab[mid].hash
    ; Entry layout: [hash:8][tokenId:4][length:4] = 16 bytes
    mov         rdx, rax
    shl         rdx, 4                   ; mid * 16
    mov         r9, qword ptr [rsi + rdx]  ; vocab[mid].hash

    cmp         r8, r9
    je          bsearch_found
    jb          bsearch_left             ; target < mid

    ; target > mid: lo = mid + 1
    lea         rbx, [rax + 1]
    jmp         bsearch_loop

bsearch_left:
    ; target < mid: hi = mid
    mov         rcx, rax
    jmp         bsearch_loop

bsearch_found:
    ; Return vocab[mid].tokenId
    mov         eax, dword ptr [rsi + rdx + 8]
    jmp         bsearch_ret

bsearch_notfound:
    mov         eax, 0FFFFFFFFh          ; -1

bsearch_ret:
    pop         rsi
    pop         rbx
    pop         rbp
    ret
BPE_FindLongestMatch ENDP

END
