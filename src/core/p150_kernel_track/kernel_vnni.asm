; Phase 150A: VPDPBUSD VNNI Dot-Product Accumulator
; Feature Request: SwarmV150_VNNI_Dequant_Core

.code
SwarmV150_VNNI_Dequant_Core proc
    ; RCX = Target ZMM Accumulator ptr
    ; RDX = Src Weights (unsigned 8-bit packed)
    ; R8  = Src Exxpert/Activations (signed 8-bit packed)
    ; R9  = Unroll Depth

ALIGN 16
@@VnniLoop:
    ; Prefetch next block
    prefetcht0 [rdx + 256]
    prefetcht0 [r8  + 256]

    ; Load operands
    vmovdqu32 zmm1, [rdx]
    vmovdqu32 zmm2, [r8]

    ; Hardware VNNI execution (8-bit int -> 32-bit dword accumulation)
    ; vpdpbusd zmm0, zmm1, zmm2  ; (Assembled statically by MASM as raw hex below if needed)
    db 62h, 0f3h, 075h, 048h, 050h, 0c2h ; vpdpbusd zmm0, zmm1, zmm2

    add rdx, 64
    add r8, 64
    dec r9
    jnz @@VnniLoop

    ; Store accumulation out
    vmovdqu32 [rcx], zmm0
    vzeroupper
    ret
SwarmV150_VNNI_Dequant_Core endp

end
