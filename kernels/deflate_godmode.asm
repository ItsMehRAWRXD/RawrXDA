option casemap:none

.data
ALIGN 4
len_base DWORD 3,4,5,6,7,8,9,10, 11,13,15,17, 19,23,27,31, 35,43,51,59, 67,83,99,115, 131,163,195,227, 258
len_xtra BYTE  0,0,0,0,0,0,0,0,  1, 1, 1, 1,   2, 2, 2, 2,   3, 3, 3, 3,   4, 4, 4, 4,    5, 5, 5, 5,   0
dist_base DWORD 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
dist_xtra BYTE  0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6, 7, 7, 8, 8,  9, 9,10,10, 11,11,12,12, 13,13

gz_hdr BYTE 1Fh,8Bh,08h,00h,00h,00h,00h,00h,00h,03h

.code
extern malloc:PROC
extern memcpy:PROC

PUBLIC deflate_godmode
; void* deflate_godmode(const void* src, size_t len, size_t* out_len, void* hash_buf)
deflate_godmode PROC
    ; rcx=src, rdx=len, r8=out_len*, r9=hash
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rsi, rcx            ; src
    mov     rbx, rdx            ; len
    mov     rdi, r8             ; out_len*
    mov     r15, r9             ; hash base

    ; alloc: len + len/2 + 128
    mov     rax, rbx
    shr     rax, 1
    add     rax, rbx
    add     rax, 128
    sub     rsp, 32
    mov     rcx, rax
    call    malloc
    add     rsp, 32
    test    rax, rax
    jz      _fail
    mov     r12, rax            ; out base
    mov     r11, rax            ; p

    ; zero hash table (HASH_SIZE=32768 entries)
    xor     ecx, ecx
    mov     edx, 0FFFFFFFFh
    mov     r14d, 32768
_zh:
    mov     [r15 + rcx*4], edx
    inc     ecx
    cmp     ecx, r14d
    jl      _zh

    ; gzip header
    mov     rax, QWORD PTR [gz_hdr]
    mov     QWORD PTR [r11], rax
    mov     WORD PTR [r11+8], 03h
    add     r11, 10

    ; DEFLATE fixed block header: BFINAL=1, BTYPE=01 -> 0b101
    ; Bit writer state: r13=bitbuf, r14d=bitcnt
    xor     r13, r13
    xor     r14d, r14d
    mov     eax, 1
    mov     ecx, 1
    call    emit_bits
    mov     eax, 1
    mov     ecx, 2
    call    emit_bits

    ; LZ77 main loop
    xor     r10d, r10d          ; i
_lz:
    cmp     r10, rbx
    jae     _lz_end
    ; if less than 3 remain, literal
    mov     rax, rbx
    sub     rax, r10
    cmp     rax, 3
    jb      _lit

    ; hash 3 bytes
    movzx   eax, BYTE PTR [rsi + r10]
    shl     eax, 8
    movzx   edx, BYTE PTR [rsi + r10 + 1]
    shl     edx, 4
    xor     eax, edx
    movzx   edx, BYTE PTR [rsi + r10 + 2]
    xor     eax, edx
    and     eax, 32767          ; HASH_MASK
    mov     edx, DWORD PTR [r15 + rax*4]
    mov     DWORD PTR [r15 + rax*4], r10d
    cmp     edx, 0FFFFFFFFh
    je      _lit
    ; distance = i - last
    mov     eax, r10d
    sub     eax, edx
    cmp     eax, 32768
    ja      _lit
    test    eax, eax
    jz      _lit
    mov     r9d, eax            ; dist
    ; match length up to 258 or remaining
    xor     ecx, ecx            ; len
    mov     r8d, 258
    mov     rax, rbx
    sub     rax, r10
    cmp     r8d, eax
    cmova   r8d, eax
_len:
    cmp     ecx, r8d
    jae     _have
    mov     al, BYTE PTR [rsi + r10 + rcx]
    mov     dl, BYTE PTR [rsi + r10 - r9 + rcx]
    cmp     al, dl
    jne     _have
    inc     ecx
    jmp     _len
_have:
    cmp     ecx, 3
    jb      _lit
    ; emit length and distance
    mov     edx, ecx
    mov     ecx, edx
    call    emit_len_fixed
    mov     ecx, r9d
    call    emit_dist_fixed
    add     r10d, edx
    jmp     _lz

_lit:
    movzx   ecx, BYTE PTR [rsi + r10]
    call    emit_lit_fixed
    inc     r10d
    jmp     _lz

_lz_end:
    ; end-of-block (256)
    mov     ecx, 256
    call    emit_lit_fixed
    ; flush bits
    call    flush_bits

    ; footer CRC32=0, ISIZE
    mov     DWORD PTR [r11], 0
    add     r11, 4
    mov     eax, ebx
    mov     DWORD PTR [r11], eax
    add     r11, 4

    ; out_len
    mov     rax, r11
    sub     rax, r12
    test    rdi, rdi
    jz      _ret
    mov     [rdi], rax

_ret:
    mov     rax, r12
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

_fail:
    xor     rax, rax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
deflate_godmode ENDP

; ===== Helpers =====

; emit_bits: RAX=bits (LSB-first), ECX=count; r13=bitbuf, r14d=bitcnt, r11=dst
emit_bits PROC
    push    rdx
    push    r8
    mov     r8, 1
    mov     edx, ecx
    shl     r8, cl
    dec     r8
    and     rax, r8
    mov     ecx, r14d
    shl     rax, cl
    or      r13, rax
    add     r14d, edx
_eb:
    cmp     r14d, 8
    jb      _eb_done
    mov     al, r13b
    mov     BYTE PTR [r11], al
    inc     r11
    shr     r13, 8
    sub     r14d, 8
    jmp     _eb
_eb_done:
    pop     r8
    pop     rdx
    ret
emit_bits ENDP

flush_bits PROC
_fb:
    cmp     r14d, 0
    jle     _fb_done
    mov     al, r13b
    mov     BYTE PTR [r11], al
    inc     r11
    shr     r13, 8
    sub     r14d, 8
    jmp     _fb
_fb_done:
    xor     r13, r13
    xor     r14d, r14d
    ret
flush_bits ENDP

rev_bits PROC
    ; reverse lower ECX bits of RAX -> RAX
    push    rdx
    xor     rdx, rdx
_rb:
    test    ecx, ecx
    jz      _rb_done
    shl     rdx, 1
    bt      rax, 0
    adc     rdx, 0
    shr     rax, 1
    dec     ecx
    jmp     _rb
_rb_done:
    mov     rax, rdx
    pop     rdx
    ret
rev_bits ENDP

emit_lit_fixed PROC
    push    rbx
    mov     eax, ecx
    cmp     eax, 143
    jbe     _l0
    cmp     eax, 255
    jbe     _l1
    cmp     eax, 279
    jbe     _l2
    ; 280..287 -> base 192, len 8
    mov     ebx, eax
    sub     ebx, 280
    add     ebx, 192
    mov     eax, ebx
    mov     ecx, 8
    call    rev_bits
    mov     ecx, 8
    call    emit_bits
    pop     rbx
    ret
_l2:
    mov     ebx, eax
    sub     ebx, 256
    mov     eax, ebx
    mov     ecx, 7
    call    rev_bits
    mov     ecx, 7
    call    emit_bits
    pop     rbx
    ret
_l0:
    mov     ebx, eax
    add     ebx, 48
    mov     eax, ebx
    mov     ecx, 8
    call    rev_bits
    mov     ecx, 8
    call    emit_bits
    pop     rbx
    ret
_l1:
    mov     ebx, eax
    sub     ebx, 144
    add     ebx, 256
    mov     eax, ebx
    mov     ecx, 9
    call    rev_bits
    mov     ecx, 9
    call    emit_bits
    pop     rbx
    ret
emit_lit_fixed ENDP

emit_len_fixed PROC
    ; ECX=len
    push    rbx
    mov     eax, ecx
    cmp     eax, 258
    jne     _ls
    mov     ecx, 285
    call    emit_lit_fixed
    pop     rbx
    ret
_ls:
    xor     ebx, ebx
_loop:
    mov     edx, DWORD PTR [len_base + rbx*4]
    movzx   eax, BYTE PTR [len_xtra + rbx]
    mov     esi, 1
    shl     esi, al
    dec     esi
    lea     edi, [rdx + rsi]
    cmp     ecx, edi
    jbe     _emit
    inc     ebx
    cmp     ebx, 29
    jb      _loop
    mov     ecx, 285
    call    emit_lit_fixed
    pop     rbx
    ret
_emit:
    mov     eax, ebx
    add     eax, 257
    mov     ecx, eax
    call    emit_lit_fixed
    movzx   eax, BYTE PTR [len_xtra + rbx]
    test    eax, eax
    jz      _done
    mov     edx, DWORD PTR [len_base + rbx*4]
    mov     eax, ecx
    sub     eax, edx
    movzx   ecx, BYTE PTR [len_xtra + rbx]
    call    emit_bits
_done:
    pop     rbx
    ret
emit_len_fixed ENDP

emit_dist_fixed PROC
    ; ECX=dist
    push    rbx
    mov     esi, ecx
    xor     ebx, ebx
_dl:
    mov     edi, DWORD PTR [dist_base + rbx*4]
    movzx   eax, BYTE PTR [dist_xtra + rbx]
    mov     edx, 1
    shl     edx, al
    dec     edx
    add     edi, edx
    mov     ecx, esi
    cmp     ecx, edi
    jbe     _de
    inc     ebx
    cmp     ebx, 30
    jb      _dl
    mov     ebx, 29
_de:
    mov     eax, ebx
    mov     ecx, 5
    call    rev_bits
    mov     ecx, 5
    call    emit_bits
    movzx   edx, BYTE PTR [dist_xtra + rbx]
    test    edx, edx
    jz      _dd
    mov     eax, esi
    sub     eax, DWORD PTR [dist_base + rbx*4]
    mov     ecx, edx
    call    emit_bits
_dd:
    pop     rbx
    ret
emit_dist_fixed ENDP

END
