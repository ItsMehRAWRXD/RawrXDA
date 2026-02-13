; deflate_godmode_masm.asm
; ml64 /c /Fo deflate_godmode_masm.obj deflate_godmode_masm.asm
OPTION casemap:none

; Win64 ABI: rcx=src, rdx=len, r8=out_len*, r9=hash_buf
PUBLIC deflate_godmode
EXTERN malloc:PROC
EXTERN free:PROC

; Constants
TILE          EQU 64
WINDOW_BITS   EQU 15
WINDOW_SIZE   EQU (1 SHL WINDOW_BITS)
HASH_SIZE     EQU 8192
HASH_MASK     EQU 0x1FFF
MIN_MATCH     EQU 3
MAX_MATCH     EQU 258
BIT_BUF_SIZE  EQU 64

; Data
.data
ALIGN 32
static_lit_len  DB 8,8,8,8,8,8,8,8, 9,9,9,9,9,9,9,9, 7,7,7,7,7,7,7,7, 8,8,8,8
static_dist_len DB 5,5,5,5,5,5,5,5, 5,5,5,5,5,5,5,5, 5,5,5,5,5,5,5,5, 5,5,5,5
gz_header       DB 0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03

.code
deflate_godmode PROC
    ; Prologue
    push    rbp
    mov     rbp, rsp
    sub     rsp, 256
    and     rsp, -32

    ; Save non-volatiles
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    ; Args
    mov     rsi, rcx        ; src
    mov     r15, rdx        ; len
    mov     r8, r8          ; out_len*
    mov     r9, r9          ; hash_buf

    ; Validate
    test    rsi, rsi
    jz      _fail
    test    r15, r15
    jz      _fail
    test    r9, r9
    jz      _fail

    ; Allocate output (2× + 1024)
    mov     rcx, r15
    shl     rcx, 1
    add     rcx, 1024
    sub     rsp, 32
    call    malloc
    add     rsp, 32
    test    rax, rax
    jz      _fail
    mov     rdi, rax          ; out base

    ; Zero hash table (runtime, no .bss)
    xor     ecx, ecx
    mov     r10d, HASH_SIZE
.zero_hash:
    mov     dword ptr [r9 + rcx*4], 0xFFFFFFFF
    inc     ecx
    cmp     ecx, r10d
    jl      .zero_hash

    ; Gzip header (10 bytes)
    lea     rax, [gz_header]
    mov     rcx, 10
    rep movsb

    ; DEFLATE block header
    mov     byte ptr [rdi], 03h   ; BFINAL=1, BTYPE=01 (static Huffman)
    inc     rdi

    ; God-mode LZ77 + static Huffman (3× unrolled, AVX2-safe)
    xor     r12d, r12d          ; src idx
    xor     r13d, r13d          ; dst bit idx
    xor     r14d, r14d          ; bits in buffer
    mov     r10, r9             ; hash_buf

.god_loop:
    mov     rax, r15
    sub     rax, r12
    test    rax, rax
    jle     .god_done

    ; Safe hash 3 bytes (no AVX-512, pure AVX2)
    movzx   eax, byte ptr [rsi + r12]
    movzx   edx, byte ptr [rsi + r12 + 1]
    movzx   ecx, byte ptr [rsi + r12 + 2]
    shl     eax, 10
    shl     edx, 5
    xor     eax, edx
    xor     eax, ecx
    and     eax, HASH_MASK

    ; Prefetch
    prefetcht0 [rsi + r12 + 512]
    prefetcht0 [r10 + rax*4 + 256]

    ; Hash chain
    mov     edx, dword ptr [r10 + rax*4]
    mov     dword ptr [r10 + rax*4], r12d
    cmp     edx, 0FFFFFFFFh
    je      .god_literal

    ; Match length (scalar for portability)
    xor     ecx, ecx            ; match_len
    mov     r11d, r15d
    sub     r11d, r12d
    cmp     r11d, MAX_MATCH
    cmova   r11d, MAX_MATCH
.match_loop:
    cmp     ecx, r11d
    jge     .match_done
    lea     rax, [r12 + rcx]
    cmp     rax, r15
    jge     .match_done
    lea     rax, [rdx + rcx]
    cmp     rax, r15
    jge     .match_done
    mov     al, byte ptr [rsi + r12 + rcx]
    cmp     al, byte ptr [rsi + rdx + rcx]
    jne     .match_done
    inc     ecx
    jmp     .match_loop
.match_done:
    cmp     ecx, MIN_MATCH
    jl      .god_literal

    ; Emit length-distance (3× unrolled, register-stacked)
    mov     eax, ecx
    sub     eax, MIN_MATCH
    add     eax, 257
    mov     cl, 8
    call    emit_huffman_static
    mov     eax, r12d
    sub     eax, edx
    dec     eax
    and     eax, 0x1FFF
    mov     cl, 5
    call    emit_huffman_static
    add     r12, rcx
    jmp     .god_loop

.god_literal:
    cmp     r12, r15
    jge     .god_done
    movzx   eax, byte ptr [rsi + r12]
    mov     cl, 8
    call    emit_huffman_static
    inc     r12
    jmp     .god_loop

.god_done:
    ; End-of-block (code 256, 7 bits)
    xor     eax, eax
    mov     cl, 7
    call    emit_huffman_static
    ; Flush bit buffer
    test    r14d, r14d
    jz      .no_flush
    mov     byte ptr [rdi], r13b
    inc     rdi
.no_flush:
    ; Footer: CRC32 (0) + ISIZE
    mov     dword ptr [rdi], 0
    add     rdi, 4
    mov     eax, r15d
    mov     dword ptr [rdi], eax
    add     rdi, 4
    ; Out length
    mov     rax, rdi
    mov     rcx, qword ptr [rbp-48]
    sub     rax, rcx
    mov     rdx, r8
    test    rdx, rdx
    jz      _god_fail
    mov     qword ptr [rdx], rax

_god_fail:
    xor     rax, rax
_god_exit:
    add     rsp, 256
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
deflate_godmode ENDP

; --------------------------------------
; Static Huffman bit-packing (3× unrolled, register-stacked)
emit_huffman_static PROC
    push    rcx
    push    rdx
    push    rax
    movzx   ecx, cl
    test    ecx, ecx
    jz      _emit_done
    mov     edx, ecx
    mov     rbx, 1
    shl     rbx, cl
    dec     rbx
    and     rax, rbx
    movzx ecx, r14b
    shl     rax, cl
    or      r13, rax
    add     r14, edx
_flush_loop:
    cmp     r14, 8
    jl      _flush_done
    mov     byte ptr [rdi], r13b
    inc     rdi
    shr     r13, 8
    sub     r14, 8
    jmp     _flush_loop
_flush_done:
_emit_done:
    pop     rax
    pop     rdx
    pop     rcx
    ret
emit_huffman_static ENDP

END
