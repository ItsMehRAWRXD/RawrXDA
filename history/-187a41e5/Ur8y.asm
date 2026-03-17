; compact_wire_asm.asm — Pure-ASM gzip-like (LZ77 POC) for bandwidth optimization (NASM syntax)
; Signature:
;   void* compact_asm(const char* json, size_t len)  ; returns ptr to [uint32 size][uint32 alloc][data...]
;   void  expand_asm(const char* compressed, size_t comp_len, char* out, size_t out_len)

%define TILE 32
%define HASH_BITS 15
%define HASH_SIZE (1 << HASH_BITS)
%define HASH_MASK (HASH_SIZE - 1)

extern malloc

section .data align=16
    one_const: dd 1.0
    align 16
    hash_table: times HASH_SIZE dd 0xFFFFFFFF

section .text

global compact_asm
; void* compact_asm(const char* json, size_t len)
; Win64: rcx=json, rdx=len
compact_asm:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32

    ; Save args
    mov     [rbp-8], rcx
    mov     [rbp-16], rdx

    ; alloc = len + 128
    mov     rcx, rdx
    add     rcx, 128
    sub     rsp, 32
    call    malloc
    add     rsp, 32
    mov     r8, rax              ; base

    ; header: size/alloc (init to 0, will fill size at end)
    xor     eax, eax
    mov     [r8], eax
    mov     [r8+4], eax
    lea     r9, [r8+8]           ; dst data ptr

    ; src ptr and counters
    mov     r10, [rbp-8]         ; src base
    xor     r11d, r11d           ; src idx
    xor     r12d, r12d           ; dst idx

.lzf_loop:
    mov     eax, r11d
    cmp     rax, [rbp-16]
    jae     .lzf_done

    ; Hash 4-byte window if enough bytes remain
    mov     ecx, [rbp-16]
    sub     ecx, r11d
    cmp     ecx, 4
    jb      .emit_lit

    mov     eax, [r10 + r11]
    and     eax, HASH_MASK
    mov     edx, [rel hash_table + rax*4]
    mov     [rel hash_table + rax*4], r11d

    cmp     edx, 0xFFFFFFFF
    je      .emit_lit

    ; Calculate len = min(32, remaining)
    mov     ecx, [rbp-16]
    sub     ecx, r11d
    cmp     ecx, TILE
    jle     .len_ok
    mov     ecx, TILE
.len_ok:

    ; token: 1B 0x80 | len 1B | dist 2B
    mov     byte [r9 + r12], 0x80
    mov     byte [r9 + r12 + 1], cl
    mov     eax, r11d
    sub     eax, edx
    mov     [r9 + r12 + 2], ax
    add     r12, 4
    add     r11, rcx
    jmp     .lzf_loop

.emit_lit:
    mov     al, [r10 + r11]
    mov     [r9 + r12], al
    inc     r11
    inc     r12
    jmp     .lzf_loop

.lzf_done:
    ; write size and alloc
    mov     [r8], r12d
    mov     [r8+4], r12d
    mov     rax, r8              ; return base
    leave
    ret

; constant inlined via TILE

global expand_asm
; void expand_asm(const char* compressed, size_t comp_len, char* out, size_t out_len)
; Win64: rcx=compressed, rdx=comp_len, r8=out, r9=out_len
expand_asm:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32

    ; save args
    mov     [rbp-8], rcx         ; comp base
    mov     [rbp-16], rdx        ; comp len
    mov     [rbp-24], r8         ; out base
    mov     [rbp-32], r9         ; out len

    xor     r10d, r10d           ; src idx
    xor     r11d, r11d           ; dst idx

.lzd_loop:
    mov     eax, r10d
    cmp     rax, [rbp-16]
    jae     .lzd_done

    mov     rcx, [rbp-8]
    mov     al, [rcx + r10]
    test    al, 0x80
    jz      .lit

    ; token
    movzx   ecx, byte [rcx + r10 + 1]    ; len -> RCX (count)
    movzx   edx, word [rcx + r10 + 2]    ; dist -> EDX
    mov     rsi, [rbp-24]                ; out base
    add     rsi, r11                     ; dst idx
    sub     rsi, rdx                     ; src = dst - dist
    mov     rdi, [rbp-24]
    add     rdi, r11
    rep movsb
    add     r10, 4
    add     r11, ecx
    jmp     .lzd_loop

.lit:
    mov     rcx, [rbp-8]
    mov     al, [rcx + r10]
    mov     rdx, [rbp-24]
    mov     [rdx + r11], al
    inc     r10
    inc     r11
    jmp     .lzd_loop

.lzd_done:
    leave
    ret
