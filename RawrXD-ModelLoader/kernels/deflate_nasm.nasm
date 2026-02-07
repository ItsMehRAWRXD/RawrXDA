; deflate_nasm.nasm — Gzip with DEFLATE (fixed Huffman + simple LZ77)
; Win64 ABI: rcx=src, rdx=len, r8=out_len*
; Returns: rax = malloc'd buffer with gzip stream (caller frees)

default rel

section .text
    global deflate_nasm
    extern malloc
    extern memcpy

; Constants
%define MIN_MATCH     3
%define MAX_MATCH     258
%define WINDOW_SIZE   32768
%define HASH_BITS     15
%define HASH_SIZE     (1 << HASH_BITS)
%define HASH_MASK     (HASH_SIZE - 1)

section .data
    align 4
    ; Length code tables (indexes 0..28 correspond to codes 257..285)
    len_base:   dd 3,4,5,6,7,8,9,10, 11,13,15,17, 19,23,27,31, 35,43,51,59, 67,83,99,115, 131,163,195,227, 258
    len_xtra:   db 0,0,0,0,0,0,0,0,  1, 1, 1, 1,   2, 2, 2, 2,   3, 3, 3, 3,   4, 4, 4, 4,   5, 5, 5, 5,   0
    dist_base:  dd 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
    dist_xtra:  db 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6, 7, 7, 8, 8,  9, 9,10,10, 11,11,12,12, 13,13

; ------------------------------------------------------------
; void* deflate_masm(const void* src, size_t len, size_t* out_len)
; ------------------------------------------------------------
deflate_nasm:
    ; Prologue: save non-volatiles
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov rsi, rcx        ; src
    mov rbx, rdx        ; len
    mov rdi, r8         ; out_len*
    mov r14, r9         ; hash table base (caller-provided)

    ; alloc = header + footer + len + len/8 + 64 (slack)
    mov rax, rbx
    shr rax, 3
    add rax, rbx
    add rax, 18+64

    ; out = malloc(alloc)
    sub rsp, 40         ; 32B home + align to 16
    mov rcx, rax
    call malloc
    add rsp, 40
    test rax, rax
    jz .fail

    mov r13, rax        ; base
    mov r11, rax        ; p

    ; gzip header (10 bytes)
    mov byte [r11+0], 0x1F
    mov byte [r11+1], 0x8B
    mov byte [r11+2], 0x08        ; deflate
    mov byte [r11+3], 0x00        ; FLG
    mov dword [r11+4], 0          ; MTIME
    mov byte [r11+8], 0x00        ; XFL
    mov byte [r11+9], 0x03        ; OS=Unix
    add r11, 10

    ; Compute output limit pointer r9 = base + len + len/8 + 82 (header+footer+slack)
    mov r9, rbx
    mov rax, rbx
    shr rax, 3
    add r9, rax
    add r9, 82
    add r9, r13

    ; Initialize hash table with -1
    xor ecx, ecx
    mov edi, HASH_SIZE
    mov edx, 0FFFFFFFFh
.clr_loop:
    mov [r14 + rcx*4], edx
    inc ecx
    cmp ecx, edi
    jl .clr_loop

    ; Bit writer state: r10=bitbuf, r15d=bitcnt
    xor r10, r10
    xor r15d, r15d

    ; Write DEFLATE block header: BFINAL=1 (1 bit), BTYPE=01 (2 bits)
    mov eax, 1
    mov ecx, 1
    call emit_bits
    mov eax, 1
    mov ecx, 2
    call emit_bits

    ; LZ77 main loop: i in r12
    xor r12d, r12d
.lz_loop:
    cmp r12, rbx
    jae .lz_done
    ; If fewer than 3 remaining, emit literal
    mov rax, rbx
    sub rax, r12
    cmp rax, MIN_MATCH
    jb .emit_lit

    ; Hash 3 bytes: ((b0<<8) ^ (b1<<4) ^ b2) & MASK
    movzx eax, byte [rsi + r12]
    shl eax, 8
    movzx edx, byte [rsi + r12 + 1]
    shl edx, 4
    xor eax, edx
    movzx edx, byte [rsi + r12 + 2]
    xor eax, edx
    and eax, HASH_MASK
    ; Load last head and update
    mov edx, [r14 + rax*4]
    mov [r14 + rax*4], r12d
    cmp edx, 0FFFFFFFFh
    je .emit_lit
    ; dist = i - last, must be <= WINDOW_SIZE and <= i
    mov eax, r12d
    sub eax, edx
    cmp eax, WINDOW_SIZE
    ja .emit_lit
    cmp eax, r12d
    ja .emit_lit
    test eax, eax
    jz .emit_lit
    mov rbp, rax                 ; dist (64-bit)
    ; Find match length up to MAX_MATCH or remaining
    xor r8d, r8d                 ; len
    mov edx, MAX_MATCH
    mov rax, rbx
    sub rax, r12
    cmp edx, eax
    cmova edx, eax               ; edx = max len
.lenlp:
    cmp r8d, edx
    jae .gotlen
    ; load current and match bytes using temporary pointers
    lea rax, [rsi + r12]
    movzx eax, byte [rax + r8]
    lea rdx, [rsi + r12]
    sub rdx, rbp
    add rdx, r8
    movzx ecx, byte [rdx]
    cmp eax, ecx
    jne .gotlen
    inc r8d
    jmp .lenlp
.gotlen:
    cmp r8d, MIN_MATCH
    jb .emit_lit
    ; Emit length and distance using fixed Huffman
    mov ecx, r8d
    call emit_len_fixed
    mov ecx, ebp
    call emit_dist_fixed
    add r12d, r8d
    jmp .lz_loop

.emit_lit:
    movzx ecx, byte [rsi + r12]
    call emit_lit_fixed
    inc r12d
    jmp .lz_loop

.lz_done:
    ; End-of-block (256)
    mov ecx, 256
    call emit_lit_fixed
    ; Flush remaining bits to bytes
    call flush_bits
    ; footer: CRC32 (0) + ISIZE (len mod 2^32)
    mov dword [r11], 0
    add r11, 4
    mov eax, ebx
    mov dword [r11], eax
    add r11, 4

    ; *out_len = r11 - base
    mov rax, r11
    sub rax, r13
    test rdi, rdi
    jz .ret
    mov [rdi], rax

.ret:
    mov rax, r13
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

.fail:
    xor rax, rax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

; ---------------- Helper routines ----------------

; emit_bits: append ECX bits from low bits of RAX into stream, LSB-first
; uses r10 (bitbuf), r15d (bitcnt), r11 (dst ptr)
emit_bits:
    ; Preserve requested bit count in edx
    mov edx, ecx
    ; mask lower ECX bits of RAX
    mov r8, 1
    mov cl, dl
    shl r8, cl
    dec r8
    and rax, r8
    ; shift by current bit count (r15d)
    mov ecx, r15d
    shl rax, cl
    or  r10, rax
    add r15d, edx
.eb_flush:
    cmp r15d, 8
    jb .eb_done
    ; bounds check before store
    cmp r11, r9
    jae .eb_ovf
    mov al, r10b
    mov [r11], al
    inc r11
    shr r10, 8
    sub r15d, 8
    jmp .eb_flush
.eb_ovf:
    jmp .fail
.eb_done:
    ret

; flush_bits: write remaining partial byte(s)
flush_bits:
.fb_loop:
    cmp r15d, 0
    jle .fb_done
    ; bounds check before store
    cmp r11, r9
    jae .fb_ovf
    mov al, r10b
    mov [r11], al
    inc r11
    shr r10, 8
    sub r15d, 8
    jmp .fb_loop
.fb_ovf:
    jmp .fail
.fb_done:
    xor r10, r10
    xor r15d, r15d
    ret

; revbits: reverse lower ECX bits of RAX -> RAX
revbits:
    xor rdx, rdx
.rb_loop:
    test ecx, ecx
    jz .rb_done
    shl rdx, 1
    bt  rax, 0
    adc rdx, 0
    shr rax, 1
    dec ecx
    jmp .rb_loop
.rb_done:
    mov rax, rdx
    ret

; emit_lit_fixed: ECX=symbol (0..287)
emit_lit_fixed:
    push rbx
    mov eax, ecx
    cmp eax, 143
    jbe .lit0
    cmp eax, 255
    jbe .lit1
    cmp eax, 279
    jbe .lit2
    ; 280..287 -> base 192, len 8
    mov ebx, eax
    sub ebx, 280
    add ebx, 192
    mov eax, ebx
    mov ecx, 8
    call revbits
    mov ecx, 8
    call emit_bits
    pop rbx
    ret
.lit2: ; 256..279 -> base 0, len 7
    mov ebx, eax
    sub ebx, 256
    mov eax, ebx
    mov ecx, 7
    call revbits
    mov ecx, 7
    call emit_bits
    pop rbx
    ret
.lit0: ; 0..143 -> base 48, len 8
    mov ebx, eax
    add ebx, 48
    mov eax, ebx
    mov ecx, 8
    call revbits
    mov ecx, 8
    call emit_bits
    pop rbx
    ret
.lit1: ; 144..255 -> base 256, len 9
    mov ebx, eax
    sub ebx, 144
    add ebx, 256
    mov eax, ebx
    mov ecx, 9
    call revbits
    mov ecx, 9
    call emit_bits
    pop rbx
    ret

; emit_len_fixed: ECX=len (3..258)
emit_len_fixed:
    push rbx
    mov eax, ecx
    cmp eax, 258
    jne .len_search
    mov ecx, 285
    call emit_lit_fixed
    pop rbx
    ret
.len_search:
    xor ebx, ebx
.llen_loop:
    lea rax, [rel len_base]
    mov edx, [rax + rbx*4]
    lea rax, [rel len_xtra]
    movzx eax, byte [rax + rbx]
    mov esi, 1
    shl esi, al
    dec esi
    lea edi, [rdx + rsi]
    cmp ecx, edi
    jbe .len_emit
    inc ebx
    cmp ebx, 29
    jb .llen_loop
    mov ecx, 285
    call emit_lit_fixed
    pop rbx
    ret
.len_emit:
    mov eax, ebx
    add eax, 257                ; symbol code
    mov ecx, eax
    call emit_lit_fixed
    lea rax, [rel len_xtra]
    movzx eax, byte [rax + rbx]
    test eax, eax
    jz .len_done
    lea rax, [rel len_base]
    mov edx, [rax + rbx*4]
    mov eax, ecx                ; len
    sub eax, edx                ; extra value
    lea rax, [rel len_xtra]
    movzx ecx, byte [rax + rbx]
    call emit_bits
.len_done:
    pop rbx
    ret

; emit_dist_fixed: ECX=dist (1..32768)
emit_dist_fixed:
    push rbx
    mov esi, ecx
    xor ebx, ebx
.d_loop:
    lea rax, [rel dist_base]
    mov edi, [rax + rbx*4]
    lea rax, [rel dist_xtra]
    movzx eax, byte [rax + rbx]
    mov edx, 1
    shl edx, al
    dec edx
    add edi, edx
    mov ecx, esi
    cmp ecx, edi
    jbe .d_emit
    inc ebx
    cmp ebx, 30
    jb .d_loop
    mov ebx, 29
.d_emit:
    ; 5-bit distance code = index (canonical), need bit-reversed output
    mov eax, ebx
    mov ecx, 5
    call revbits
    mov ecx, 5
    call emit_bits
    ; extra bits if any
    lea rax, [rel dist_xtra]
    movzx edx, byte [rax + rbx]
    test edx, edx
    jz .d_done
    mov eax, esi
    lea rax, [rel dist_base]
    mov edx, [rax + rbx*4]
    mov eax, esi
    sub eax, edx
    mov ecx, edx
    call emit_bits
.d_done:
    pop rbx
    ret
