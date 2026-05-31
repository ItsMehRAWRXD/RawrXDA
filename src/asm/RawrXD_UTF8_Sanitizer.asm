option casemap:none

PUBLIC UTF8_SanitizeBuffer
PUBLIC UTF8_IsValidASCII
PUBLIC UTF8_StripAllNonASCII

.code

; Flags
cfg_inplace          EQU 00000001h
cfg_preserve_latin1  EQU 00000002h
cfg_aggressive_ascii EQU 00000004h

; -----------------------------------------------------------------------------
; UTF8_IsValidASCII
; RCX = input ptr, RDX = length
; RAX = 1 if all bytes are 7-bit ASCII, else 0
; -----------------------------------------------------------------------------
UTF8_IsValidASCII PROC
    xor     rax, rax
    test    rcx, rcx
    jz      isva_exit

    mov     r8, rdx
isva_loop:
    test    r8, r8
    jz      isva_all_ascii
    movzx   r9d, byte ptr [rcx]
    test    r9b, 80h
    jnz     isva_exit
    inc     rcx
    dec     r8
    jmp     isva_loop

isva_all_ascii:
    mov     rax, 1
isva_exit:
    ret
UTF8_IsValidASCII ENDP

; -----------------------------------------------------------------------------
; UTF8_StripAllNonASCII
; RCX = input ptr, RDX = length, R8 = output ptr
; RAX = output length
; -----------------------------------------------------------------------------
UTF8_StripAllNonASCII PROC
    xor     rax, rax
    test    rcx, rcx
    jz      strip_exit
    test    r8, r8
    jz      strip_exit

    mov     r9, rdx
strip_loop:
    test    r9, r9
    jz      strip_exit

    movzx   r10d, byte ptr [rcx]
    test    r10b, 80h
    jz      strip_keep
    mov     r10b, 20h

strip_keep:
    mov     byte ptr [r8], r10b
    inc     rcx
    inc     r8
    inc     rax
    dec     r9
    jmp     strip_loop

strip_exit:
    ret
UTF8_StripAllNonASCII ENDP

; -----------------------------------------------------------------------------
; UTF8_SanitizeBuffer
; RCX = input ptr
; RDX = length
; R8  = output ptr
; R9  = flags
; RAX = output length
; -----------------------------------------------------------------------------
UTF8_SanitizeBuffer PROC
    ; R9 = flags (cfg_*); reserved for future policy branches — same path for now.
    xor     rax, rax
    test    rcx, rcx
    jz      san_exit
    test    r8, r8
    jz      san_exit

    mov     r10, rdx
san_loop:
    test    r10, r10
    jz      san_exit

    movzx   r11d, byte ptr [rcx]
    test    r11b, 80h
    jz      san_keep
    mov     r11b, 20h

san_keep:
    mov     byte ptr [r8], r11b
    inc     rcx
    inc     r8
    inc     rax
    dec     r10
    jmp     san_loop

san_exit:
    ret
UTF8_SanitizeBuffer ENDP

END
