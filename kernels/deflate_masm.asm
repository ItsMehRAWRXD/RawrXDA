; deflate_masm.asm — Minimal gzip writer using DEFLATE stored blocks (no compression)
; Win64 ABI: rcx=src, rdx=len, r8=out_len_ptr
; Returns: RAX = malloc'd buffer containing gzip stream. Caller frees.

option casemap:none

; ---- Constants ----
HASH_BITS    EQU 15
HASH_SIZE    EQU (1 SHL HASH_BITS) ; 32768
HASH_MASK    EQU (HASH_SIZE-1)
MIN_MATCH    EQU 3
MAX_MATCH    EQU 258
WINDOW_SIZE  EQU 32768

.data
ALIGN 8
_gz_id1 db 1Fh
_gz_id2 db 8Bh
_gz_cm  db 08h    ; DEFLATE
_gz_flg db 00h
_gz_zero4 dd 0
_gz_xfl db 00h
_gz_os  db 03h    ; Unix

.code
extern malloc:PROC
extern memcpy:PROC

PUBLIC deflate_masm
deflate_masm PROC
    ; rcx=src, rdx=len, r8=out_len*
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13

    mov     rsi, rcx            ; src ptr
    mov     rbx, rdx            ; total len (preserve)
    mov     rdi, r8             ; out_len*

    ; block_count = (len + 65534) / 65535
    mov     rax, rbx
    add     rax, 65534
    mov     rcx, 65535
    xor     rdx, rdx
    div     rcx                 ; rax = block_count
    mov     r9, rax             ; r9=block_count

    ; alloc = 10 (hdr) + 8 (footer) + len + block_count*5 (stored header per block)
    mov     rax, r9
    imul    rax, 5
    add     rax, rbx
    add     rax, 18             ; 10+8

    ; malloc(alloc)
    sub     rsp, 32             ; shadow space
    mov     rcx, rax
    call    malloc
    add     rsp, 32
    test    rax, rax
    jz      _fail

    ; p = out
    mov     r13, rax            ; out base (callee-saved)
    mov     r11, rax            ; p

    ; gzip header (10 bytes)
    mov     al, [_gz_id1]
    mov     BYTE PTR [r11], al
    mov     al, [_gz_id2]
    mov     BYTE PTR [r11+1], al
    mov     al, [_gz_cm]
    mov     BYTE PTR [r11+2], al
    mov     al, [_gz_flg]
    mov     BYTE PTR [r11+3], al
    mov     DWORD PTR [r11+4], 0
    mov     al, [_gz_xfl]
    mov     BYTE PTR [r11+8], al
    mov     al, [_gz_os]
    mov     BYTE PTR [r11+9], al
    add     r11, 10

    ; ---- Initialize hash table to -1 ----
    lea     rdi, hash_heads
    mov     ecx, HASH_SIZE
    mov     eax, 0FFFFFFFFh
    rep stosd

    ; ---- Bit writer init (r10 = bitbuf, r15d = bitcount) ----
    xor     r10, r10
    xor     r15d, r15d

    ; ---- DEFLATE block header: BFINAL=1, BTYPE=01 (fixed Huffman) ----
    mov     eax, 1              ; 1 (BFINAL) + (01b << 1) = 0b101 -> value 0x05 with 3 LSBs? LSB-first emit 0b101
    mov     ecx, 1              ; first emit 1 bit (BFINAL)
    call    emit_bits
    mov     eax, 1              ; BTYPE=01
    mov     ecx, 2
    call    emit_bits

    ; ---- LZ77 with simple hash (last-hit), fixed Huffman coding ----
    xor     r12d, r12d          ; i = 0
_lz_loop:
    cmp     r12, rbx
    jae     _lz_done
    ; If not enough bytes for match, emit literal
    mov     r8, rbx
    sub     r8, r12
    cmp     r8, MIN_MATCH
    jb      _emit_literal

    ; hash = ((b0<<8) ^ (b1<<4) ^ b2) & HASH_MASK
    movzx   eax, BYTE PTR [rsi + r12]
    shl     eax, 8
    movzx   edx, BYTE PTR [rsi + r12 + 1]
    shl     edx, 4
    xor     eax, edx
    movzx   edx, BYTE PTR [rsi + r12 + 2]
    xor     eax, edx
    and     eax, HASH_MASK
    ; last = hash_heads[hash]
    mov     edx, DWORD PTR [hash_heads + rax*4]
    ; update head
    mov     DWORD PTR [hash_heads + rax*4], r12d
    cmp     edx, 0FFFFFFFFh
    je      _emit_literal
    ; dist = i - last
    mov     eax, r12d
    sub     eax, edx
    cmp     eax, WINDOW_SIZE
    ja      _emit_literal
    test    eax, eax
    jz      _emit_literal
    ; find match length up to MAX_MATCH or remaining
    mov     r9d, 0              ; match len
    mov     r14d, MAX_MATCH
    cmp     r8d, r14d
    cmovb   r14d, r8d           ; r14d = max len
_len_loop:
    cmp     r9d, r14d
    jae     _have_len
    mov     bl, BYTE PTR [rsi + r12 + r9]
    mov     bh, BYTE PTR [rsi + rdx + r9]
    cmp     bl, bh
    jne     _have_len
    inc     r9d
    jmp     _len_loop
_have_len:
    cmp     r9d, MIN_MATCH
    jb      _emit_literal
    ; encode length r9d and distance eax (dist in eax)
    push    rax                 ; save dist
    mov     ecx, r9d
    call    emit_len_fixed      ; consumes ECX=len, uses emit_bits
    pop     rax
    ; distance encode
    mov     ecx, eax            ; ECX=dist
    call    emit_dist_fixed
    add     r12d, r9d
    jmp     _lz_loop

_emit_literal:
    movzx   ecx, BYTE PTR [rsi + r12]
    call    emit_lit_fixed
    inc     r12d
    jmp     _lz_loop

_lz_done:
    ; End-of-block code 256
    mov     ecx, 256
    call    emit_lit_fixed
    ; flush remaining bits
    call    flush_bits
    ; footer: CRC32 (0) + ISIZE (len mod 2^32)
    mov     DWORD PTR [r11], 0
    add     r11, 4
    mov     eax, ebx
    mov     DWORD PTR [r11], eax
    add     r11, 4

    ; *out_len = (p - out)
    mov     rax, r11
    sub     rax, r13
    mov     rcx, rdi
    test    rcx, rcx
    jz      _done
    mov     [rcx], rax

_done:
    mov     rax, r13
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

_fail:
    xor     rax, rax
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
deflate_masm ENDP

; ====================== Helpers ==========================

; emit_bits: append ECX bits from low ECX of RAX value into stream
; in: RAX=bits (LSB-first), ECX=count; uses r10(bitbuf), r15(bitcnt), r11(dst)
emit_bits PROC
    ; r10 |= (rax & ((1<<cl)-1)) << r15
    mov     edx, ecx
    mov     r8, 1
    shl     r8, cl
    dec     r8
    and     rax, r8
    shl     rax, r15
    or      r10, rax
    add     r15d, ecx
_eb_flush:
    cmp     r15d, 8
    jb      _eb_done
    mov     al, BYTE PTR r10
    mov     BYTE PTR [r11], al
    inc     r11
    shr     r10, 8
    sub     r15d, 8
    jmp     _eb_flush
_eb_done:
    ret
emit_bits ENDP

; flush_bits: write remaining full/partial byte(s)
flush_bits PROC
_fb_loop:
    cmp     r15d, 0
    jle     _fb_done
    mov     al, BYTE PTR r10
    mov     BYTE PTR [r11], al
    inc     r11
    shr     r10, 8
    sub     r15d, 8
    jmp     _fb_loop
_fb_done:
    xor     r10, r10
    xor     r15d, r15d
    ret
flush_bits ENDP

; reverse lower ECX bits of RAX -> RAX
rev_bits PROC
    xor     rdx, rdx
_rb_loop:
    test    ecx, ecx
    jz      _rb_done
    shl     rdx, 1
    bt      rax, 0
    adc     rdx, 0
    shr     rax, 1
    dec     ecx
    jmp     _rb_loop
_rb_done:
    mov     rax, rdx
    ret
rev_bits ENDP

; emit fixed Huffman literal/length symbol in ECX
emit_lit_fixed PROC
    push    rbx
    mov     eax, ecx            ; symbol
    cmp     eax, 143
    jbe     _l_0_143
    cmp     eax, 255
    jbe     _l_144_255
    cmp     eax, 279
    jbe     _l_256_279
    ; 280..287
    mov     ebx, eax
    sub     ebx, 280
    add     ebx, 192            ; base
    mov     eax, ebx
    mov     ecx, 8
    call    rev_bits
    mov     ecx, 8
    call    emit_bits
    pop     rbx
    ret
_l_256_279:
    mov     ebx, eax
    sub     ebx, 256
    ; base 0, len 7
    mov     eax, ebx
    mov     ecx, 7
    call    rev_bits
    mov     ecx, 7
    call    emit_bits
    pop     rbx
    ret
_l_0_143:
    ; base 48 (0x30), len 8
    mov     ebx, eax
    add     ebx, 48
    mov     eax, ebx
    mov     ecx, 8
    call    rev_bits
    mov     ecx, 8
    call    emit_bits
    pop     rbx
    ret
_l_144_255:
    mov     ebx, eax
    sub     ebx, 144
    add     ebx, 400
    mov     eax, ebx
    mov     ecx, 9
    call    rev_bits
    mov     ecx, 9
    call    emit_bits
    pop     rbx
    ret
emit_lit_fixed ENDP

; Tables for length/dist
.data
ALIGN 4
len_base DWORD 3,4,5,6,7,8,9,10, 11,13,15,17, 19,23,27,31, 35,43,51,59, 67,83,99,115, 131,163,195,227, 258
len_xtra BYTE  0,0,0,0,0,0,0,0,  1, 1, 1, 1,   2, 2, 2, 2,   3, 3, 3, 3,   4, 4, 4, 4,    5, 5, 5, 5,   0
dist_base DWORD 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
dist_xtra BYTE  0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6, 7, 7, 8, 8,  9, 9,10,10, 11,11,12,12, 13,13

.data?
ALIGN 16
hash_heads DWORD HASH_SIZE DUP(?)

.code
; emit_len_fixed: ECX=len (3..258)
emit_len_fixed PROC
    push    rbx
    mov     eax, ecx
    cmp     eax, 258
    jne     _len_search
    ; code 285, no extra
    mov     ecx, 285
    call    emit_lit_fixed
    pop     rbx
    ret
_len_search:
    xor     ebx, ebx            ; idx
_len_loop2:
    mov     edx, DWORD PTR [len_base + rbx*4]
    movzx   eax, BYTE PTR [len_xtra + rbx]
    mov     esi, 1
    shl     esi, al
    dec     esi
    lea     edi, [rdx + rsi]
    cmp     ecx, edi
    jbe     _len_emit
    inc     ebx
    cmp     ebx, 29
    jb      _len_loop2
    ; fallback to literal? shouldn't happen
    mov     ecx, 285
    call    emit_lit_fixed
    pop     rbx
    ret
_len_emit:
    ; code = 257 + idx
    mov     eax, ebx
    add     eax, 257
    mov     edi, eax            ; save code
    call    emit_lit_fixed      ; emits code
    ; extra bits
    movzx   eax, BYTE PTR [len_xtra + rbx]
    test    eax, eax
    jz      _len_done
    mov     edx, DWORD PTR [len_base + rbx*4]
    mov     eax, ecx
    sub     eax, edx            ; extra value
    mov     ecx, BYTE PTR [len_xtra + rbx]
    call    emit_bits
_len_done:
    pop     rbx
    ret
emit_len_fixed ENDP

; emit_dist_fixed: ECX=dist (1..32768)
emit_dist_fixed PROC
    push    rbx
    mov     esi, ecx            ; save dist in ESI
    xor     ebx, ebx
_d_loop:
    mov     edi, DWORD PTR [dist_base + rbx*4]
    movzx   ecx, BYTE PTR [dist_xtra + rbx]
    mov     eax, 1
    shl     eax, cl
    dec     eax
    add     edi, eax
    mov     ecx, esi            ; dist
    cmp     ecx, edi
    jbe     _d_emit
    inc     ebx
    cmp     ebx, 30
    jb      _d_loop
    mov     ebx, 29
_d_emit:
    ; code = idx (0..29), 5 bits fixed
    mov     eax, ebx
    mov     ecx, 5
    call    rev_bits
    mov     ecx, 5
    call    emit_bits
    ; extra bits
    movzx   edx, BYTE PTR [dist_xtra + rbx]
    test    edx, edx
    jz      _d_done
    mov     eax, esi            ; dist
    sub     eax, DWORD PTR [dist_base + rbx*4]
    mov     ecx, edx            ; extra bit length
    call    emit_bits
_d_done:
    pop     rbx
    ret
emit_dist_fixed ENDP

END
