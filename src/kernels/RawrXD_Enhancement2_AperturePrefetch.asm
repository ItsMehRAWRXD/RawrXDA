; ============================================================================
; RawrXD Enhancement 2 - Aperture Prefetch
; Win64 ABI:
;   rcx = base pointer
;   rdx = total bytes in aperture
;   r8d = prefetch distance in bytes
;   r9d = prefetch policy (0=t0, 1=t1, 2=t2, 3=nta)
; Return:
;   eax = 1 on success, 0 on invalid args
; ============================================================================

PUBLIC RawrXD_Enhancement2_AperturePrefetch

.code

RawrXD_Enhancement2_AperturePrefetch PROC FRAME
    .endprolog

    test rcx, rcx
    jz prefetch_fail
    test rdx, rdx
    jz prefetch_fail

    mov r10, rcx                     ; current pointer
    lea r11, [rcx + rdx]             ; end pointer (base + bytes)

prefetch_loop:
    cmp r10, r11
    jae prefetch_ok

    ; page-boundary handling: avoid stepping speculative target across a 64KB edge
    mov rax, r10
    and eax, 0FFFFh                  ; offset inside 64KB page
    cmp eax, 0FF00h
    jae near_page_boundary

    lea rax, [r10 + r8]              ; normal lookahead target
    jmp issue_prefetch

near_page_boundary:
    mov rax, r10                     ; stay in-page near boundary

issue_prefetch:
    cmp r9d, 1
    je do_t1
    cmp r9d, 2
    je do_t2
    cmp r9d, 3
    je do_nta

    prefetcht0 BYTE PTR [rax]
    jmp prefetch_advance

do_t1:
    prefetcht1 BYTE PTR [rax]
    jmp prefetch_advance

do_t2:
    prefetcht2 BYTE PTR [rax]
    jmp prefetch_advance

do_nta:
    prefetchnta BYTE PTR [rax]

prefetch_advance:
    add r10, 64                      ; one ZMM cache line stride
    jmp prefetch_loop

prefetch_ok:
    mov eax, 1
    ret

prefetch_fail:
    xor eax, eax
    ret
RawrXD_Enhancement2_AperturePrefetch ENDP

END
