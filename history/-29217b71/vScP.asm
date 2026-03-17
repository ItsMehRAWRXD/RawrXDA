; deflate_brutal_masm.asm
; ml64 /c /Fo deflate_brutal_masm.obj deflate_brutal_masm.asm
OPTION casemap:none
PUBLIC deflate_brutal_masm
EXTERN malloc:PROC
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
    sub     rsp, 40         ; Shadow space + alignment

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
    
    mov     rcx, rax
    call    malloc
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
    
    ; memcpy(rdi, rsi, rcx)
    ; Win64: rcx=dst, rdx=src, r8=count
    mov     r13, rcx        ; save chunk size
    
    mov     r8, rcx         ; count
    mov     rdx, rsi        ; src
    mov     rcx, rdi        ; dst
    call    memcpy
    
    add     rsi, r13
    add     rdi, r13
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
    add     rsp, 40
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
