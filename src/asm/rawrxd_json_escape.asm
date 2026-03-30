; rawrxd_json_escape.asm
; Vector 3: JSON Pulse scaffold with AVX-512-assisted fast-path detection.
;
; Exports:
;   rawrxd_json_pulse_vbmi2(src, len, dst) -> bytes written
;
; Notes:
; - Fast path (len==64, no escapes): single 64-byte vector copy.
; - Dirty path: in-kernel expansion with JSON escaping (no scalar bridge call).

OPTION CASEMAP:NONE

EXTERN g_pulse_lut_base : BYTE

PUBLIC rawrxd_json_pulse_vbmi2

.data
ALIGN 16
json_quote_64 DB 64 DUP(022h)
json_backslash_64 DB 64 DUP(05Ch)
json_ctrl_20_64 DB 64 DUP(020h)
json_zero_64 DB 64 DUP(000h)
json_hex_lut DB "0123456789ABCDEF"

.code

ALIGN 16
rawrxd_json_pulse_vbmi2 PROC FRAME
    sub rsp, 32
    .allocstack 32
    .endprolog

    ; rcx = src, rdx = len, r8 = dst
    test rcx, rcx
    jz return_zero
    test r8, r8
    jz return_zero
    test rdx, rdx
    jz return_zero

    ; Vector fast path is only defined for exactly one 64-byte chunk.
    ; Other lengths are handled by in-kernel generic expansion logic.
    cmp rdx, 64
    jne generic_expand

    ; Load exactly 64 bytes.
    vmovdqu8 zmm0, zmmword ptr [rcx]

    ; Detect '"' and '\\' bytes.
    vmovdqu8 zmm1, zmmword ptr [json_quote_64]
    vmovdqu8 zmm2, zmmword ptr [json_backslash_64]
    vpcmpeqb k1, zmm0, zmm1
    vpcmpeqb k2, zmm0, zmm2

    ; Detect control-ish bytes using saturating subtraction against 0x20.
    ; This intentionally includes 0x20 as conservative fallback trigger.
    vmovdqu8 zmm3, zmmword ptr [json_ctrl_20_64]
    vmovdqu8 zmm4, zmmword ptr [json_zero_64]
    vpsubusb zmm5, zmm0, zmm3
    vpcmpeqb k3, zmm5, zmm4

    korw k1, k1, k2
    korw k1, k1, k3
    kortestw k1, k1
    jnz generic_expand

    ; Escape-free fast path: copy 64 bytes directly.
    vmovdqu8 zmmword ptr [r8], zmm0
    mov rax, 64
    add rsp, 32
    ret

generic_expand:
    ; Generic in-kernel JSON escape expansion for any len.
    ; r10 = src index, r11 = dst bytes written
    xor r10, r10
    xor r11, r11

expand_loop:
    cmp r10, rdx
    jae expand_done

    movzx eax, byte ptr [rcx + r10]

    cmp eax, 022h
    je esc_quote
    cmp eax, 05Ch
    je esc_backslash
    cmp eax, 00Ah
    je esc_nl
    cmp eax, 00Dh
    je esc_cr
    cmp eax, 009h
    je esc_tab
    cmp eax, 020h
    jb esc_ctrl

    ; Normal byte pass-through
    mov byte ptr [r8 + r11], al
    inc r11
    inc r10
    jmp expand_loop

esc_quote:
    mov byte ptr [r8 + r11], 05Ch
    mov byte ptr [r8 + r11 + 1], 022h
    add r11, 2
    inc r10
    jmp expand_loop

esc_backslash:
    mov byte ptr [r8 + r11], 05Ch
    mov byte ptr [r8 + r11 + 1], 05Ch
    add r11, 2
    inc r10
    jmp expand_loop

esc_nl:
    mov byte ptr [r8 + r11], 05Ch
    mov byte ptr [r8 + r11 + 1], 06Eh
    add r11, 2
    inc r10
    jmp expand_loop

esc_cr:
    mov byte ptr [r8 + r11], 05Ch
    mov byte ptr [r8 + r11 + 1], 072h
    add r11, 2
    inc r10
    jmp expand_loop

esc_tab:
    mov byte ptr [r8 + r11], 05Ch
    mov byte ptr [r8 + r11 + 1], 074h
    add r11, 2
    inc r10
    jmp expand_loop

esc_ctrl:
    ; Emit \u00XX for ASCII controls < 0x20.
    mov byte ptr [r8 + r11], 05Ch
    mov byte ptr [r8 + r11 + 1], 075h
    mov byte ptr [r8 + r11 + 2], 030h
    mov byte ptr [r8 + r11 + 3], 030h

    mov r9d, eax
    shr eax, 4
    and eax, 0Fh
    add al, 030h
    cmp al, 039h
    jbe esc_ctrl_hi_ready
    add al, 7
esc_ctrl_hi_ready:
    mov byte ptr [r8 + r11 + 4], al

    mov eax, r9d
    and eax, 0Fh
    add al, 030h
    cmp al, 039h
    jbe esc_ctrl_lo_ready
    add al, 7
esc_ctrl_lo_ready:
    mov byte ptr [r8 + r11 + 5], al

    add r11, 6
    inc r10
    jmp expand_loop

expand_done:
    mov rax, r11
    add rsp, 32
    ret

return_zero:
    xor rax, rax
    add rsp, 32
    ret
rawrxd_json_pulse_vbmi2 ENDP

END
