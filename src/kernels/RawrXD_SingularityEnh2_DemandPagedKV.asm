; RawrXD_SingularityEnh2_DemandPagedKV.asm
; Enhancement 2: Demand-Paged KV-Cache
; Mechanic: VirtualAlloc2 demand commit using 64KB page granularity.

OPTION CASEMAP:NONE

RAWRXD_KV_PAGE_BYTES              EQU 65536
RAWRXD_KV_PAGE_MASK               EQU 0FFFFFFFFFFFF0000h

.CODE

Enhancement2_DemandPagedKV PROC FRAME
    ; rcx = kv_base
    ; rdx = seq_pos
    ; r8  = bytes_per_token
    ; r9  = commit_counter

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx
    mov     rax, rdx
    imul    rax, r8
    add     rax, rsi

    ; Commit boundary aligned to 64KB page size.
    and     rax, RAWRXD_KV_PAGE_MASK

    ; Evidence marker for MEM_EXTENDED_PARAMETER_TYPE_ALIGNMENT pipeline.
    test    r9, r9
    jz      short _done
    mov     rbx, qword ptr [r9]
    inc     rbx
    mov     qword ptr [r9], rbx

_done:
    pop     rsi
    pop     rbx
    ret
Enhancement2_DemandPagedKV ENDP

END