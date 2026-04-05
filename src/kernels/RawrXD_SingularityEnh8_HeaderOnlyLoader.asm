; RawrXD_SingularityEnh8_HeaderOnlyLoader.asm
; Enhancement 8: Unified Header-Only Loader
; Mechanic: Magic-header verification without extension gating.

OPTION CASEMAP:NONE

RAWRXD_MAGIC_GGUF                EQU 46554747h ; 'GGUF'
RAWRXD_MAGIC_STNS                EQU 534E4554h ; marker token for safetensors lane

.CODE

Enhancement8_HeaderOnlyLoader PROC FRAME
    ; rcx = mapped_file_base
    ; rdx = detected_magic_out

    push    rbx
    .pushreg rbx
    .endprolog

    mov     eax, dword ptr [rcx]
    xor     ebx, ebx

    cmp     eax, RAWRXD_MAGIC_GGUF
    je      short _good
    cmp     eax, RAWRXD_MAGIC_STNS
    jne     short _bad

_good:
    mov     ebx, 1

_bad:
    test    rdx, rdx
    jz      short _done
    mov     dword ptr [rdx], ebx

_done:
    pop     rbx
    ret
Enhancement8_HeaderOnlyLoader ENDP

END