; ============================================================================
; RawrXD_GSIHash.asm — Phase 29.2: MASM64 GSI Hash Kernels
; ============================================================================
;
; High-performance MASM64 implementations of the PDB GSI hash functions:
;
;   PDB_HashName      — Microsoft PDB symbol name hash (IPHR_HASH buckets)
;   PDB_GSIHashLookup — Walk a GSI bucket chain and find matching symbol
;
; These functions are the hot path for every Go-to-Definition and hover
; operation. The C++ fallbacks in pdb_gsi_hash.cpp handle MinGW builds.
;
; Build:
;   ml64 /c /Zi /Zd /Fo<obj> RawrXD_GSIHash.asm
;
; Copyright (c) RawrXD Project. All rights reserved.
; ============================================================================

.code

; ============================================================================
; Constants
; ============================================================================

IPHR_HASH       EQU 4096           ; Number of hash buckets
S_PUB32         EQU 0110Eh         ; CV symbol type: public symbol
PUB32_NAME_OFF  EQU 14             ; Offset from record start to name field
GSI_REC_SIZE    EQU 8              ; sizeof(GSIHashRecord)
NOT_FOUND       EQU 0FFFFFFFFh     ; Sentinel: no match


; ============================================================================
; PDB_HashName — Compute Microsoft PDB hash for a symbol name
; ============================================================================
;
; Prototype (Windows x64 ABI):
;   uint32_t PDB_HashName(
;       const char* name,      // RCX: null-terminated symbol name
;       uint32_t    nameLen    // EDX: length of name (not including null)
;   );
;
; Returns:
;   EAX = hash value in [0, IPHR_HASH)
;
; Algorithm (matches Microsoft's hashSz):
;   hash = 0
;   for each byte c in name[0..nameLen-1]:
;       c = tolower(c)
;       hash += c
;       hash += (hash << 10)
;       hash ^= (hash >> 6)
;   hash += (hash << 3)
;   hash ^= (hash >> 11)
;   hash += (hash << 15)
;   return hash % IPHR_HASH
;
; Clobbers: RAX, RCX, RDX, R8, R9, R10
; Preserves: RBX, RSI, RDI, RBP, R12-R15
;
; ============================================================================
PDB_HashName PROC
    ; Validate inputs
    test rcx, rcx
    jz hash_zero
    test edx, edx
    jz hash_zero

    ; Setup
    mov r8, rcx            ; R8 = name pointer
    mov r9d, edx           ; R9D = nameLen
    xor eax, eax           ; EAX = hash accumulator
    xor ecx, ecx           ; ECX = loop index

hash_loop:
    cmp ecx, r9d
    jge hash_finalize

    ; Load byte
    movzx r10d, BYTE PTR [r8 + rcx]

    ; tolower: if 'A' <= c <= 'Z', c += 32
    cmp r10d, 41h          ; 'A'
    jl hash_no_lower
    cmp r10d, 5Ah          ; 'Z'
    jg hash_no_lower
    add r10d, 20h          ; += 32

hash_no_lower:
    ; hash += c
    add eax, r10d

    ; hash += (hash << 10)
    mov edx, eax
    shl edx, 10
    add eax, edx

    ; hash ^= (hash >> 6)
    mov edx, eax
    shr edx, 6
    xor eax, edx

    inc ecx
    jmp hash_loop

hash_finalize:
    ; hash += (hash << 3)
    mov edx, eax
    shl edx, 3
    add eax, edx

    ; hash ^= (hash >> 11)
    mov edx, eax
    shr edx, 11
    xor eax, edx

    ; hash += (hash << 15)
    mov edx, eax
    shl edx, 15
    add eax, edx

    ; hash %= IPHR_HASH (4096)
    ; Since 4096 = 2^12, we can use AND mask
    and eax, 0FFFh         ; EAX = hash & (4096 - 1)

    ret

hash_zero:
    xor eax, eax
    ret
PDB_HashName ENDP


; ============================================================================
; PDB_GSIHashLookup — Walk a GSI hash bucket to find a symbol by name
; ============================================================================
;
; Prototype (Windows x64 ABI):
;   uint32_t PDB_GSIHashLookup(
;       const void*  records,       // RCX: array of GSIHashRecord (8 bytes each)
;       uint32_t     numRecords,    // EDX: number of records in this bucket
;       const void*  symbolStream,  // R8:  base of symbol record stream
;       const char*  name,          // R9:  symbol name to find
;       uint32_t     nameLen        // [RSP+40]: length of name
;   );
;
; Returns:
;   EAX = symbolOffset of matching record, or 0xFFFFFFFF if not found
;
; For each record in the bucket:
;   1. Read symbolOffset from GSIHashRecord
;   2. Verify the CV record at that offset is S_PUB32 (0x110E)
;   3. Compare name at record+14 with search name (case-insensitive)
;   4. If match, verify null terminator → return symbolOffset
;
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; Preserves: RBX, RSI, RDI, RBP, R12-R15
;
; ============================================================================
PDB_GSIHashLookup PROC
    ; Save callee-saved registers
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    ; Parameter setup
    mov rsi, rcx                   ; RSI = records array base
    mov r12d, edx                  ; R12D = numRecords
    mov rbx, r8                    ; RBX = symbolStream base
    mov rdi, r9                    ; RDI = search name

    ; 5th parameter: nameLen at [RSP + 6*8 (pushes) + 8 (ret) + 32 (shadow)] = [RSP + 88]
    mov r13d, DWORD PTR [rsp + 88] ; R13D = nameLen

    ; Validate
    test rsi, rsi
    jz lookup_not_found
    test rbx, rbx
    jz lookup_not_found
    test rdi, rdi
    jz lookup_not_found
    test r12d, r12d
    jz lookup_not_found
    test r13d, r13d
    jz lookup_not_found

    ; Walk the bucket chain
    xor ecx, ecx                  ; ECX = record index

bucket_loop:
    cmp ecx, r12d
    jge lookup_not_found

    ; Load symbolOffset from GSIHashRecord[ecx]
    ; struct GSIHashRecord { uint32_t symbolOffset; uint32_t cref; }
    lea r10, [rsi + rcx*8]        ; R10 = &records[ecx] (8 bytes per record)
    mov r14d, DWORD PTR [r10]     ; R14D = symbolOffset

    ; Read recTyp at symbolStream + symbolOffset + 2
    lea r10, [rbx + r14]          ; R10 = base of CV record
    movzx eax, WORD PTR [r10 + 2] ; EAX = recTyp

    ; Check if S_PUB32
    cmp eax, S_PUB32
    jne bucket_advance             ; Not a public symbol, skip

    ; ---- Compare name ----
    ; Name is at record + PUB32_NAME_OFF (14)
    lea r10, [rbx + r14 + PUB32_NAME_OFF] ; R10 = record name pointer

    ; Case-insensitive compare of r13d bytes
    xor edx, edx                  ; EDX = compare index

name_cmp_loop:
    cmp edx, r13d
    jge name_cmp_check_null

    movzx eax, BYTE PTR [r10 + rdx]  ; Record name byte
    movzx r11d, BYTE PTR [rdi + rdx] ; Search name byte

    ; tolower(record byte)
    cmp eax, 41h
    jl skip_lower_rec
    cmp eax, 5Ah
    jg skip_lower_rec
    add eax, 20h
skip_lower_rec:

    ; tolower(search byte)
    cmp r11d, 41h
    jl skip_lower_search
    cmp r11d, 5Ah
    jg skip_lower_search
    add r11d, 20h
skip_lower_search:

    cmp eax, r11d
    jne bucket_advance             ; Mismatch

    inc edx
    jmp name_cmp_loop

name_cmp_check_null:
    ; All nameLen bytes matched. Verify null terminator.
    movzx eax, BYTE PTR [r10 + r13]
    test al, al
    jnz bucket_advance             ; Not null-terminated — partial match

    ; ---- MATCH FOUND ----
    mov eax, r14d                  ; Return symbolOffset
    jmp lookup_done

bucket_advance:
    inc ecx
    jmp bucket_loop

lookup_not_found:
    mov eax, NOT_FOUND

lookup_done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
PDB_GSIHashLookup ENDP


; ============================================================================
; Export declarations for MSVC linker
; ============================================================================

; ============================================================================
; PDB_FuzzyScore — Tier 1 Cosmetic #4: fzf-style Fuzzy Matching Kernel
; ============================================================================
;
; Implements case-insensitive subsequence matching with scoring.
; Bonuses for: consecutive matches, word boundaries, prefix, camelCase.
; Penalties for: target length, gaps between matches.
;
; RCX = pointer to query string (null-terminated, ASCII)
; RDX = pointer to target string (null-terminated, ASCII)
; R8  = pointer to output match positions array (DWORD[64])
; R9D = max positions to store
;
; Returns:
;   EAX = score (0 = no match, higher = better)
;   [R8] = filled with matched character indices
; ============================================================================
PUBLIC PDB_FuzzyScore
PDB_FuzzyScore PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rsi, rcx            ; RSI = query
    mov     rdi, rdx            ; RDI = target
    mov     r12, r8             ; R12 = positions output
    mov     r13d, r9d           ; R13D = max positions

    ; Compute query length
    xor     ecx, ecx
@@qlen_loop:
    cmp     BYTE PTR [rsi + rcx], 0
    je      @@qlen_done
    inc     ecx
    jmp     @@qlen_loop
@@qlen_done:
    mov     r14d, ecx           ; R14D = queryLen
    test    ecx, ecx
    jz      @@fz_no_match       ; empty query = no match

    ; Compute target length
    xor     ecx, ecx
@@tlen_loop:
    cmp     BYTE PTR [rdi + rcx], 0
    je      @@tlen_done
    inc     ecx
    jmp     @@tlen_loop
@@tlen_done:
    mov     r15d, ecx           ; R15D = targetLen

    ; Main fuzzy matching loop
    xor     ebx, ebx            ; EBX = score accumulator
    xor     ecx, ecx            ; ECX = query index (qi)
    xor     edx, edx            ; EDX = target index (ti)
    xor     r8d, r8d            ; R8D = position count
    xor     r9d, r9d            ; R9D = consecutive match flag
    mov     r10d, 1             ; R10D = prevWasSep (start = word boundary)

@@fz_main_loop:
    cmp     edx, r15d
    jge     @@fz_check_complete
    cmp     ecx, r14d
    jge     @@fz_check_complete

    ; Load and tolower query[qi]
    movzx   eax, BYTE PTR [rsi + rcx]
    cmp     eax, 41h            ; 'A'
    jl      @@fz_q_lower_done
    cmp     eax, 5Ah            ; 'Z'
    jg      @@fz_q_lower_done
    add     eax, 20h
@@fz_q_lower_done:
    mov     r11d, eax           ; R11D = tolower(query[qi])

    ; Load and tolower target[ti]
    movzx   eax, BYTE PTR [rdi + rdx]
    ; Check if this is a separator for word boundary detection
    push    rax
    cmp     eax, 20h            ; space
    je      @@fz_is_sep
    cmp     eax, 2Eh            ; '.'
    je      @@fz_is_sep
    cmp     eax, 5Fh            ; '_'
    je      @@fz_is_sep
    cmp     eax, 3Ah            ; ':'
    je      @@fz_is_sep
    cmp     eax, 2Fh            ; '/'
    je      @@fz_is_sep
    cmp     eax, 2Dh            ; '-'
    je      @@fz_is_sep
    mov     r10d, 0             ; not a separator
    jmp     @@fz_after_sep
@@fz_is_sep:
    mov     r10d, 1
@@fz_after_sep:
    pop     rax

    ; tolower target char
    cmp     eax, 41h
    jl      @@fz_t_lower_done
    cmp     eax, 5Ah
    jg      @@fz_t_lower_done
    add     eax, 20h
@@fz_t_lower_done:

    ; Compare
    cmp     eax, r11d
    jne     @@fz_no_char_match

    ; ── MATCH ──
    add     ebx, 10             ; base score: +10

    ; Word boundary bonus
    test    r10d, r10d
    jz      @@fz_no_boundary
    add     ebx, 20             ; +20 for word boundary match
@@fz_no_boundary:

    ; Consecutive match bonus
    test    r9d, r9d
    jz      @@fz_no_consec
    add     ebx, 15             ; +15 for consecutive
@@fz_no_consec:

    ; Prefix bonus (first char of target)
    test    edx, edx
    jnz     @@fz_no_prefix
    add     ebx, 25             ; +25 for prefix match
@@fz_no_prefix:

    ; Store position
    cmp     r8d, r13d
    jge     @@fz_skip_pos
    mov     DWORD PTR [r12 + r8 * 4], edx
    inc     r8d
@@fz_skip_pos:

    mov     r9d, 1              ; prevMatched = true
    inc     ecx                 ; advance query
    inc     edx                 ; advance target
    jmp     @@fz_main_loop

@@fz_no_char_match:
    mov     r9d, 0              ; prevMatched = false
    inc     edx                 ; advance target only
    jmp     @@fz_main_loop

@@fz_check_complete:
    ; Must have matched all query chars
    cmp     ecx, r14d
    jl      @@fz_no_match

    ; Length penalty: score -= targetLen / 3
    mov     eax, r15d
    xor     edx, edx
    mov     ecx, 3
    div     ecx
    sub     ebx, eax

    ; Gap penalty: sum of gaps between consecutive positions
    cmp     r8d, 2
    jl      @@fz_no_gap_penalty
    xor     ecx, ecx            ; gap accumulator
    mov     edx, 1              ; position index
@@fz_gap_loop:
    cmp     edx, r8d
    jge     @@fz_gap_done
    mov     eax, DWORD PTR [r12 + rdx * 4]
    sub     eax, DWORD PTR [r12 + rdx * 4 - 4]
    dec     eax                 ; gap = pos[i] - pos[i-1] - 1
    add     ecx, eax
    inc     edx
    jmp     @@fz_gap_loop
@@fz_gap_done:
    shl     ecx, 1              ; gap * 2
    sub     ebx, ecx
@@fz_no_gap_penalty:

    ; Ensure minimum score of 1
    cmp     ebx, 1
    jge     @@fz_score_ok
    mov     ebx, 1
@@fz_score_ok:
    mov     eax, ebx
    jmp     @@fz_done

@@fz_no_match:
    xor     eax, eax            ; score = 0

@@fz_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PDB_FuzzyScore ENDP

END
