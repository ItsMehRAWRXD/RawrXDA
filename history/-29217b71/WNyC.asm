; deflate_brutal_masm.asm — Minimal gzip writer using DEFLATE stored blocks (no compression)
; Win64 ABI: rcx=src, rdx=len, r8=out_len*
; Returns: RAX = malloc'd buffer containing gzip stream (caller frees)
OPTION casemap:none

PUBLIC deflate_brutal_masm
EXTERN malloc:PROC
EXTERN memcpy:PROC

.code
deflate_brutal_masm PROC
    push    rbx
    push    rsi
    push    rdi
    push    r14
    push    r15

    mov     rsi, rcx            ; src
    mov     rbx, rdx            ; len
    mov     r15, r8             ; out_len*

    ; block_count = (len + 65534) / 65535
    mov     rax, rbx
    add     rax, 65534
    mov     rcx, 65535
    xor     rdx, rdx
    div     rcx                 ; rax=blocks

    ; alloc = 10 (hdr) + 8 (footer) + len + blocks*5 (stored header per block)
    mov     rcx, rax
    imul    rcx, 5
    add     rcx, rbx
    add     rcx, 18
    sub     rsp, 32
    call    malloc
    add     rsp, 32
    test    rax, rax
    jz      _fail

    mov     r14, rax            ; out base
    mov     rdi, rax            ; p

    ; gzip header (10 bytes)
    mov     word ptr [rdi], 08B1Fh
    mov     byte ptr [rdi+2], 08h
    mov     dword ptr [rdi+4], 0
    mov     word ptr [rdi+8], 0003h
    add     rdi, 10

    ; stored blocks
    mov     r12, rbx            ; remaining
_blk_loop:
    test    r12, r12
    jz      _after_blocks
    mov     rax, r12
    cmp     rax, 65535
    jbe     _chunk_ok
    mov     rax, 65535
_chunk_ok:
    ; header byte: BFINAL if remaining==chunk
    mov     rcx, r12
    sub     rcx, rax
    cmp     rcx, 0
    jne     _not_final
    mov     byte ptr [rdi], 1
    jmp     _hdr_done
_not_final:
    mov     byte ptr [rdi], 0
_hdr_done:
    inc     rdi
    ; LEN + NLEN (LE)
    mov     cx, ax
    mov     word ptr [rdi], cx
    not     cx
    mov     word ptr [rdi+2], cx
    add     rdi, 4
    ; memcpy(p, src, chunk)
    sub     rsp, 32
    mov     rcx, rdi            ; dest
    mov     rdx, rsi            ; src
    mov     r8,  rax            ; size
    call    memcpy
    add     rsp, 32
    add     rdi, rax
    add     rsi, rax
    sub     r12, rax
    jmp     _blk_loop

_after_blocks:
    ; footer: CRC32 (0) + ISIZE (len mod 2^32)
    mov     dword ptr [rdi], 0
    add     rdi, 4
    mov     eax, ebx
    mov     dword ptr [rdi], eax
    add     rdi, 4

    ; *out_len = (p - base)
    mov     rax, rdi
    sub     rax, r14
    test    r15, r15
    jz      _ret
    mov     [r15], rax

_ret:
    mov     rax, r14
    pop     r15
    pop     r14
    pop     rdi
    pop     rsi
    pop     rbx
    ret

_fail:
    xor     rax, rax
    pop     r15
    pop     r14
    pop     rdi
    pop     rsi
    pop     rbx
    ret
deflate_brutal_masm ENDP

END
; deflate_brutal_masm.asm
; ml64 /c /Fo deflate_brutal_masm.obj deflate_brutal_masm.asm
OPTION casemap:none
PUBLIC deflate_brutal_masm
EXTERN GlobalAlloc:PROC
; EXTERN malloc:PROC
EXTERN memcpy:PROC

.code
deflate_brutal_masm PROC
    ; Win64 ABI: rcx=src, rdx=len, r8=out_len*
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 32         ; Shadow space (aligned to 16 bytes since 7 pushes aligned it)

    mov     rsi, rcx        ; src
    mov     rbx, rdx        ; len
    mov     r14, r8         ; out_len*

    ; alloc = len + 18 (header + footer) + 5 bytes per 64k block (header)
    ; Block count = (len + 65535) / 65535
    mov     rax, rbx
    add     rax, 65535
    shr     rax, 16         ; approx block count
    imul    rax, 5          ; 5 bytes overhead per block
    add     rax, rbx
    add     rax, 18         ; global header/footer
    
    mov     rdx, rax        ; size
    mov     rcx, 0          ; GMEM_FIXED
    call    GlobalAlloc
    test    rax, rax
    jz      _fail
    
    mov     rdi, rax        ; current out pointer
    mov     r15, rax        ; save base pointer for return

    ; Gzip header (10 bytes)
    mov     byte ptr [rdi], 1Fh
    mov     byte ptr [rdi+1], 8Bh
    mov     byte ptr [rdi+2], 08h
    mov     byte ptr [rdi+3], 00h
    mov     dword ptr [rdi+4], 0
    mov     byte ptr [rdi+8], 00h
    mov     byte ptr [rdi+9], 03h
    add     rdi, 10

    ; Stored blocks
    mov     r12, rbx        ; remaining bytes
    
_block_loop:
    ; Determine block size (max 65535)
    mov     rcx, 65535
    cmp     r12, rcx
    cmovb   rcx, r12        ; rcx = chunk size
    
    ; Check if final
    mov     rax, r12
    sub     rax, rcx
    ; if rax == 0, final.
    
    ; BFINAL bit (bit 0)
    ; BTYPE = 00 (bits 1-2)
    ; Byte = BFINAL
    xor     edx, edx
    test    rax, rax
    setz    dl              ; dl = 1 if final
    mov     byte ptr [rdi], dl
    inc     rdi
    
    ; LEN (u16 little endian)
    mov     word ptr [rdi], cx
    ; NLEN (u16) = ~LEN
    mov     ax, cx
    not     ax
    mov     word ptr [rdi+2], ax
    add     rdi, 4
    
    ; Copy data using rep movsb (fast on modern CPUs)
    mov     r13, rcx        ; save chunk size for subtraction
    ; rep     movsb           ; rsi+=rcx, rdi+=rcx
    add     rsi, rcx        ; Skip copy for debug
    add     rdi, rcx
    
    sub     r12, r13
    
    cmp     r12, 0
    ja      _block_loop
    
    ; Footer: CRC32 (0 for now)
    mov     dword ptr [rdi], 0
    add     rdi, 4
    ; ISIZE (original len)
    mov     eax, ebx
    mov     dword ptr [rdi], eax
    add     rdi, 4
    
    ; Set out_len
    mov     rax, rdi
    sub     rax, r15
    mov     qword ptr [r14], rax
    
    mov     rax, r15        ; return buffer
    jmp     _exit

_fail:
    xor     rax, rax

_exit:
    add     rsp, 32
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
deflate_brutal_masm ENDP
END
