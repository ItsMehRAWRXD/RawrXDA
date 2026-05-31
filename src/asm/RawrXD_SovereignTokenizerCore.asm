OPTION CASEMAP:NONE

EXTERN rawr_cpu_has_avx512:PROC

PUBLIC SovTok_Init
PUBLIC SovTok_TokenizeBPE_AVX512
PUBLIC SovTok_TokenizeTrie
PUBLIC SovTok_RenderTokensToGlyphs
PUBLIC SovTok_SPSC_EmitToken
PUBLIC SovTok_SPSC_ConsumeToken

.DATA
ALIGN 8

; [len:1][token_id:4][bytes:len], bucket-terminated by len=0
sovtok_bucket_a LABEL BYTE
    DB 1
    DD 3
    DB 'a'
    DB 0

sovtok_bucket_i LABEL BYTE
    DB 2
    DD 2
    DB 'i','n'
    DB 0

; Sorted by descending length for deterministic longest-match preference.
sovtok_bucket_t LABEL BYTE
    DB 3
    DD 1
    DB 't','h','e'
    DB 2
    DD 4
    DB 't','h'
    DB 0

ALIGN 8
sovtok_bpe_index DQ 256 DUP (0)

; Trie node layout: [token_id:8][child_ptr[256]:8 each]
SOVTOK_TRIE_NODE_SIZE EQU (8 + (256 * 8))

ALIGN 8
sovtok_trie_root  DB SOVTOK_TRIE_NODE_SIZE DUP (0)
sovtok_trie_n_a   DB SOVTOK_TRIE_NODE_SIZE DUP (0)
sovtok_trie_n_i   DB SOVTOK_TRIE_NODE_SIZE DUP (0)
sovtok_trie_n_in  DB SOVTOK_TRIE_NODE_SIZE DUP (0)
sovtok_trie_n_t   DB SOVTOK_TRIE_NODE_SIZE DUP (0)
sovtok_trie_n_th  DB SOVTOK_TRIE_NODE_SIZE DUP (0)
sovtok_trie_n_the DB SOVTOK_TRIE_NODE_SIZE DUP (0)

; Token -> glyph map (flat O(1)). Default identity map.
ALIGN 8
sovtok_token_table DD 65536 DUP (0)

; SPSC ring buffer (single producer, single consumer)
ALIGN 8
sovtok_ring_capacity EQU 1024
sovtok_ring_mask EQU (sovtok_ring_capacity - 1)
sovtok_ring_buffer DD sovtok_ring_capacity DUP (0)
sovtok_ring_head   DQ 0
sovtok_ring_tail   DQ 0

sovtok_has_avx512 DD 0
sovtok_initialized DQ 0

.CODE

SovTok_EnsureInit PROC
    CMP QWORD PTR [sovtok_initialized], 0
    JNE short _sovtok_init_done
    CALL SovTok_Init
_sovtok_init_done:
    RET
SovTok_EnsureInit ENDP

; One-time static table wiring; no heap, no CRT.
SovTok_Init PROC
    PUSH RBX
    PUSH RSI

    ; BPE first-byte index buckets.
    LEA RAX, sovtok_bucket_a
    MOV QWORD PTR [sovtok_bpe_index + ('a' * 8)], RAX

    LEA RAX, sovtok_bucket_i
    MOV QWORD PTR [sovtok_bpe_index + ('i' * 8)], RAX

    LEA RAX, sovtok_bucket_t
    MOV QWORD PTR [sovtok_bpe_index + ('t' * 8)], RAX

    ; Trie links and terminal token IDs.
    LEA RAX, sovtok_trie_n_a
    MOV QWORD PTR [sovtok_trie_root + 8 + ('a' * 8)], RAX
    MOV QWORD PTR [sovtok_trie_n_a + 0], 3

    LEA RAX, sovtok_trie_n_i
    MOV QWORD PTR [sovtok_trie_root + 8 + ('i' * 8)], RAX
    LEA RBX, sovtok_trie_n_in
    MOV QWORD PTR [sovtok_trie_n_i + 8 + ('n' * 8)], RBX
    MOV QWORD PTR [sovtok_trie_n_in + 0], 2

    LEA RAX, sovtok_trie_n_t
    MOV QWORD PTR [sovtok_trie_root + 8 + ('t' * 8)], RAX
    LEA RBX, sovtok_trie_n_th
    MOV QWORD PTR [sovtok_trie_n_t + 8 + ('h' * 8)], RBX
    MOV QWORD PTR [sovtok_trie_n_th + 0], 4
    LEA RSI, sovtok_trie_n_the
    MOV QWORD PTR [sovtok_trie_n_th + 8 + ('e' * 8)], RSI
    MOV QWORD PTR [sovtok_trie_n_the + 0], 1

    ; Identity token->glyph map for 16-bit token range.
    LEA RDX, sovtok_token_table
    XOR EAX, EAX
_sovtok_map_loop:
    MOV DWORD PTR [RDX + RAX*4], EAX
    INC EAX
    CMP EAX, 65536
    JL _sovtok_map_loop

    MOV QWORD PTR [sovtok_ring_head], 0
    MOV QWORD PTR [sovtok_ring_tail], 0

    ; Latch AVX-512 support once; runtime path stays deterministic and safe.
    CALL rawr_cpu_has_avx512
    MOV DWORD PTR [sovtok_has_avx512], EAX

    MOV QWORD PTR [sovtok_initialized], 1

    POP RSI
    POP RBX
    RET
SovTok_Init ENDP

; RCX=input bytes, RDX=input length, R8=output token dwords, R9=vocab index base (0=default)
; Returns RAX=token_count.
SovTok_TokenizeBPE_AVX512 PROC
    PUSH RBX
    PUSH RSI
    PUSH RDI
    PUSH R12
    PUSH R13
    PUSH R14
    PUSH R15

    CALL SovTok_EnsureInit

    MOV RSI, RCX
    MOV RBX, RDX
    MOV RDI, R8
    XOR RAX, RAX

    TEST R9, R9
    JNZ short _sovtok_bpe_index_ready
    LEA R9, sovtok_bpe_index
_sovtok_bpe_index_ready:

_sovtok_bpe_next:
    TEST RBX, RBX
    JZ _sovtok_bpe_done

    MOVZX R11D, BYTE PTR [RSI]
    MOV R10D, R11D               ; default token = raw byte
    XOR R13D, R13D               ; bestLen

    MOV R14, QWORD PTR [R9 + R11*8]
    TEST R14, R14
    JZ _sovtok_bpe_emit_default

_sovtok_bpe_scan_bucket:
    MOVZX R12D, BYTE PTR [R14]   ; len
    TEST R12D, R12D
    JZ _sovtok_bpe_emit_selected

    CMP R12, RBX
    JA _sovtok_bpe_next_entry

    LEA R15, [R14 + 5]           ; entry bytes

    ; AVX-512 compare path for medium/long entries where a 64B window is safe.
    CMP DWORD PTR [sovtok_has_avx512], 0
    JE _sovtok_bpe_scalar_compare
    CMP R12D, 16
    JB _sovtok_bpe_scalar_compare
    CMP RBX, 64
    JB _sovtok_bpe_scalar_compare

    VMOVDQU8 ZMM0, ZMMWORD PTR [RSI]
    VMOVDQU8 ZMM1, ZMMWORD PTR [R15]
    VPCMPEQB K1, ZMM0, ZMM1
    KMOVQ R11, K1

    CMP R12D, 64
    JE _sovtok_bpe_check_fullmask

    MOV ECX, R12D
    MOV R8, 1
    SHL R8, CL
    DEC R8
    MOV R15, R11
    AND R15, R8
    CMP R15, R8
    JE _sovtok_bpe_matched
    JMP _sovtok_bpe_next_entry

_sovtok_bpe_check_fullmask:
    CMP R11, -1
    JE _sovtok_bpe_matched
    JMP _sovtok_bpe_next_entry

_sovtok_bpe_scalar_compare:
    XOR ECX, ECX
_sovtok_bpe_scalar_loop:
    CMP ECX, R12D
    JGE _sovtok_bpe_matched
    MOV R8B, BYTE PTR [RSI + RCX]
    CMP R8B, BYTE PTR [R15 + RCX]
    JNE _sovtok_bpe_next_entry
    INC ECX
    JMP _sovtok_bpe_scalar_loop

_sovtok_bpe_matched:
    CMP R12D, R13D
    JBE _sovtok_bpe_next_entry
    MOV R13D, R12D
    MOV R10D, DWORD PTR [R14 + 1]

_sovtok_bpe_next_entry:
    MOVZX R12D, BYTE PTR [R14]
    LEA R14, [R14 + 5 + R12]
    JMP _sovtok_bpe_scan_bucket

_sovtok_bpe_emit_selected:
    TEST R13D, R13D
    JNZ _sovtok_bpe_emit_match

_sovtok_bpe_emit_default:
    MOV R13D, 1

_sovtok_bpe_emit_match:
    MOV DWORD PTR [RDI], R10D
    ADD RDI, 4
    ADD RSI, R13
    SUB RBX, R13
    INC RAX
    JMP _sovtok_bpe_next

_sovtok_bpe_done:
    POP R15
    POP R14
    POP R13
    POP R12
    POP RDI
    POP RSI
    POP RBX
    RET
SovTok_TokenizeBPE_AVX512 ENDP

; RCX=input bytes, RDX=input length, R8=output token dwords, R9=trie root (0=default)
; Returns RAX=token_count.
SovTok_TokenizeTrie PROC
    PUSH RBX
    PUSH RSI
    PUSH RDI
    PUSH R12
    PUSH R13
    PUSH R14

    CALL SovTok_EnsureInit

    MOV RSI, RCX
    MOV RBX, RDX
    MOV RDI, R8
    XOR RAX, RAX

    TEST R9, R9
    JNZ short _sovtok_trie_root_ready
    LEA R9, sovtok_trie_root
_sovtok_trie_root_ready:

_sovtok_trie_next:
    TEST RBX, RBX
    JZ _sovtok_trie_done

    MOV R12, R9        ; node
    XOR R13D, R13D     ; best token id
    XOR R14D, R14D     ; best length
    XOR R10D, R10D     ; consumed in current walk

_sovtok_trie_walk:
    CMP R10, RBX
    JAE _sovtok_trie_emit

    MOVZX R11D, BYTE PTR [RSI + R10]
    MOV R12, QWORD PTR [R12 + 8 + R11*8]
    TEST R12, R12
    JZ _sovtok_trie_emit

    INC R10D
    MOV ECX, DWORD PTR [R12 + 0]
    TEST ECX, ECX
    JZ _sovtok_trie_walk

    MOV R13D, ECX
    MOV R14D, R10D
    JMP _sovtok_trie_walk

_sovtok_trie_emit:
    TEST R13D, R13D
    JNZ _sovtok_trie_emit_match

    MOVZX R13D, BYTE PTR [RSI]
    MOV R14D, 1

_sovtok_trie_emit_match:
    MOV DWORD PTR [RDI], R13D
    ADD RDI, 4
    ADD RSI, R14
    SUB RBX, R14
    INC RAX
    JMP _sovtok_trie_next

_sovtok_trie_done:
    POP R14
    POP R13
    POP R12
    POP RDI
    POP RSI
    POP RBX
    RET
SovTok_TokenizeTrie ENDP

; RCX=token dword stream, RDX=token count, R8=glyph dword out, R9=token->glyph table (0=default)
; Returns RAX=glyph_count.
SovTok_RenderTokensToGlyphs PROC
    PUSH RSI
    PUSH RDI

    CALL SovTok_EnsureInit

    MOV RSI, RCX
    MOV RDI, R8
    MOV R10, RDX
    XOR RAX, RAX

    TEST R9, R9
    JNZ short _sovtok_render_table_ready
    LEA R9, sovtok_token_table
_sovtok_render_table_ready:

_sovtok_render_loop:
    TEST R10, R10
    JZ _sovtok_render_done

    MOV EDX, DWORD PTR [RSI]
    MOV ECX, DWORD PTR [R9 + RDX*4]
    MOV DWORD PTR [RDI], ECX

    ADD RSI, 4
    ADD RDI, 4
    DEC R10
    INC RAX
    JMP _sovtok_render_loop

_sovtok_render_done:
    POP RDI
    POP RSI
    RET
SovTok_RenderTokensToGlyphs ENDP

; RCX=token dword, returns EAX=1 on success, 0 when ring full.
SovTok_SPSC_EmitToken PROC
    CALL SovTok_EnsureInit

    LEA R8, sovtok_ring_buffer
    MOV R10, QWORD PTR [sovtok_ring_head]
    MOV R11, QWORD PTR [sovtok_ring_tail]

    LEA RAX, [R10 + 1]
    AND RAX, sovtok_ring_mask
    CMP RAX, R11
    JE _sovtok_emit_full

    MOV DWORD PTR [R8 + R10*4], ECX
    MFENCE
    MOV QWORD PTR [sovtok_ring_head], RAX
    MOV EAX, 1
    RET

_sovtok_emit_full:
    XOR EAX, EAX
    RET
SovTok_SPSC_EmitToken ENDP

; Returns EDX=1 + EAX=token on success, EDX=0 when ring empty.
SovTok_SPSC_ConsumeToken PROC
    CALL SovTok_EnsureInit

    LEA R8, sovtok_ring_buffer
    MOV R10, QWORD PTR [sovtok_ring_tail]
    MOV R11, QWORD PTR [sovtok_ring_head]
    CMP R10, R11
    JE _sovtok_consume_empty

    MOV EAX, DWORD PTR [R8 + R10*4]
    LEA R10, [R10 + 1]
    AND R10, sovtok_ring_mask
    MFENCE
    MOV QWORD PTR [sovtok_ring_tail], R10
    MOV EDX, 1
    RET

_sovtok_consume_empty:
    XOR EAX, EAX
    XOR EDX, EDX
    RET
SovTok_SPSC_ConsumeToken ENDP

END
