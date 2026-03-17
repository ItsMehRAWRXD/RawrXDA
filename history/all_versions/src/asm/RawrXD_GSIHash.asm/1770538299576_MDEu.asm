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
END
