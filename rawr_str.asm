; =========================
; FILE: rawr_str.asm  (SYSTEM 2: strings/format)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

.code

; SIZE_T rawr_wcslen(LPCWSTR s)
; RCX=s, RAX=len (WCHAR count)
public rawr_wcslen
rawr_wcslen proc
    ; leaf
    xor     rax, rax
    test    rcx, rcx
    jz      _done
_loop:
    movzx   edx, word ptr [rcx + rax*2]
    test    edx, edx
    jz      _done
    inc     rax
    jmp     _loop
_done:
    ret
rawr_wcslen endp

; LPWSTR rawr_u64_to_hex16_w(UINT64 v, LPWSTR out, BOOL prefix0x)
; RCX=v, RDX=out, R8D=prefix0x
; writes 16 hex chars (or 18 with 0x), null-terminated
; returns RAX=endptr (points to null)
public rawr_u64_to_hex16_w
rawr_u64_to_hex16_w proc
    RAWR_PROLOGUE 0
    RAWR_SAVE_NONVOL

    mov     r12, rcx        ; v
    mov     r13, rdx        ; out
    mov     r14d, r8d       ; prefix

    test    r13, r13
    jz      _fail

    cmp     r14d, 0
    je      _no_prefix
    mov     word ptr [r13+0], '0'
    mov     word ptr [r13+2], 'x'
    add     r13, 4
_no_prefix:

    mov     r15d, 16
_hex_loop:
    ; nibble from high to low
    mov     rax, r12
    mov     ecx, (16-1)
    sub     ecx, r15d
    shl     ecx, 2
    mov     edx, ecx
    shr     rax, cl
    and     eax, 0Fh

    ; convert nibble -> wchar
    cmp     eax, 9
    jbe     _digit
    add     eax, ('a' - 10)
    jmp     _store
_digit:
    add     eax, '0'
_store:
    mov     word ptr [r13], ax
    add     r13, 2
    dec     r15d
    jnz     _hex_loop

    mov     word ptr [r13], 0
    mov     rax, r13
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 0

_fail:
    xor     rax, rax
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 0
rawr_u64_to_hex16_w endp

end