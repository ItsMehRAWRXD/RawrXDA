; RawrXD_SingularityEnh7_SpeculativeSwarmChain.asm
; Enhancement 7: Speculative Swarm Chaining
; Mechanic: Pre-map next transformer layer based on current routing trajectory.

OPTION CASEMAP:NONE

RAWRXD_SWARM_STRIDE_BYTES         EQU 4096

.CODE

Enhancement7_SpeculativeSwarmChain PROC FRAME
    ; rcx = layer_base
    ; rdx = current_layer
    ; r8  = premap_depth

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx
    mov     rax, rdx
    mov     rbx, r8

_chain:
    cmp     rbx, 0
    je      short _done

    ; Speculative chain prefetch for upcoming layer pages.
    inc     rax
    mov     rcx, rax
    imul    rcx, RAWRXD_SWARM_STRIDE_BYTES
    add     rcx, rsi
    prefetcht1 [rcx]

    dec     rbx
    jmp     short _chain

_done:
    pop     rsi
    pop     rbx
    ret
Enhancement7_SpeculativeSwarmChain ENDP

END