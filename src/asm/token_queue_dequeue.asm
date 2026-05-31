; =============================================================================
; token_queue_dequeue.asm - AVX2 Batch Token Dequeue for Inference Dispatch
; =============================================================================
;
; Exports IC_TokenBatchDequeue - hot-path bulk pop from a cache-line-padded
; SPSC ring buffer into a flat destination array.
;
; Ring buffer layout (must match TokenQueueFast in token_queue_fast.h):
;   +0   : head      (DWORD, consumer read pointer, 64-byte-aligned cache line)
;   +64  : tail      (DWORD, producer write pointer, 64-byte-aligned cache line)
;   +128 : capacity  (DWORD, power-of-2 ring size)
;   +192 : tokens[]  (int32_t ring data, capacity elements)
;
; ABI: Microsoft x64
;   RCX = TokenQueueFast*  (ring descriptor)
;   RDX = int32_t*         (destination array, caller guarantees capacity >= maxCount)
;   R8D = int32_t          (maxCount, tokens to dequeue; must be > 0)
; Returns:
;   EAX = number of tokens actually dequeued (0..min(available, maxCount))
;
; x86 TSO provides acquire semantics on plain loads and release semantics on
; plain stores from normal WB memory.  LOCK XCHG is used for the head commit
; solely to give the producer a full memory barrier (MFENCE equivalent).
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

TQ_HEAD      EQU 0
TQ_TAIL      EQU 64
TQ_CAPACITY  EQU 128
TQ_TOKENS    EQU 192

PUBLIC IC_TokenBatchDequeue

.code

IC_TokenBatchDequeue PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    .endprolog

    test    r8d, r8d
    jle     @@ret_zero
    test    rcx, rcx
    jz      @@ret_zero
    test    rdx, rdx
    jz      @@ret_zero

    mov     ebx,  dword ptr [rcx + TQ_HEAD]
    mov     esi,  dword ptr [rcx + TQ_TAIL]

    mov     eax,  esi
    sub     eax,  ebx
    test    eax,  eax
    jle     @@ret_zero

    cmp     eax,  r8d
    jbe     @@count_ok
    mov     eax,  r8d
@@count_ok:
    mov     r12d, eax

    mov     edi,  dword ptr [rcx + TQ_CAPACITY]
    mov     r8d,  edi
    dec     r8d
    and     ebx,  r8d

    mov     eax,  ebx
    add     eax,  r12d
    cmp     eax,  edi
    jbe     @@contiguous

    ; Wrap-around: segment 1 then segment 2
    mov     r10d, edi
    sub     r10d, ebx

    lea     r9,  [rcx + TQ_TOKENS]
    lea     r9,  [r9 + rbx * 4]
    mov     eax, r10d
@@seg1_avx2:
    cmp     eax, 8
    jb      @@seg1_scalar
    vmovdqu ymm0, ymmword ptr [r9]
    vmovdqu ymmword ptr [rdx], ymm0
    add     r9,  32
    add     rdx, 32
    sub     eax, 8
    jmp     @@seg1_avx2
@@seg1_scalar:
    test    eax, eax
    jz      @@seg2_start
@@seg1_one:
    mov     r11d, dword ptr [r9]
    mov     dword ptr [rdx], r11d
    add     r9,  4
    add     rdx, 4
    dec     eax
    jnz     @@seg1_one
@@seg2_start:
    lea     r9,  [rcx + TQ_TOKENS]
    mov     eax, r12d
    sub     eax, r10d
    test    eax, eax
    jz      @@update_head
@@seg2_avx2:
    cmp     eax, 8
    jb      @@seg2_scalar
    vmovdqu ymm0, ymmword ptr [r9]
    vmovdqu ymmword ptr [rdx], ymm0
    add     r9,  32
    add     rdx, 32
    sub     eax, 8
    jmp     @@seg2_avx2
@@seg2_scalar:
    test    eax, eax
    jz      @@update_head
@@seg2_one:
    mov     r11d, dword ptr [r9]
    mov     dword ptr [rdx], r11d
    add     r9,  4
    add     rdx, 4
    dec     eax
    jnz     @@seg2_one
    jmp     @@update_head

@@contiguous:
    lea     r9,  [rcx + TQ_TOKENS]
    lea     r9,  [r9 + rbx * 4]
    mov     eax, r12d
@@con_avx2:
    cmp     eax, 8
    jb      @@con_scalar
    vmovdqu ymm0, ymmword ptr [r9]
    vmovdqu ymmword ptr [rdx], ymm0
    add     r9,  32
    add     rdx, 32
    sub     eax, 8
    jmp     @@con_avx2
@@con_scalar:
    test    eax, eax
    jz      @@update_head
@@con_one:
    mov     r11d, dword ptr [r9]
    mov     dword ptr [rdx], r11d
    add     r9,  4
    add     rdx, 4
    dec     eax
    jnz     @@con_one

@@update_head:
    mov     eax, dword ptr [rcx + TQ_HEAD]
    add     eax, r12d
    lock xchg dword ptr [rcx + TQ_HEAD], eax
    mov     eax, r12d
    jmp     @@done

@@ret_zero:
    xor     eax, eax

@@done:
    vzeroupper
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
IC_TokenBatchDequeue ENDP

END
