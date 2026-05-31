option casemap:none
.code

; MASM ABI skeleton for sovereign codec fast path.
; Returns 0 to force fail-closed fallback until optimized implementation lands.

PUBLIC ExpandBitstream_Masm
PUBLIC SqueezeBitstream_Masm

ExpandBitstream_Masm PROC
    xor eax, eax
    ret
ExpandBitstream_Masm ENDP

SqueezeBitstream_Masm PROC
    xor eax, eax
    ret
SqueezeBitstream_Masm ENDP

END
