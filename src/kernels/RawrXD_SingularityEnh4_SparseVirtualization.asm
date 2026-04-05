; RawrXD_SingularityEnh4_SparseVirtualization.asm
; Enhancement 4: Sparse-Matrix Virtualization
; Mechanic: Sparse-file backed expert pages, fault in only active experts.

OPTION CASEMAP:NONE

RAWRXD_SPARSE_BLOCK_BYTES         EQU 4096

.CODE

Enhancement4_SparseVirtualization PROC FRAME
    ; rcx = expert_bitmap
    ; rdx = expert_index
    ; r8  = sparse_offset_out

    push    rbx
    .pushreg rbx
    .endprolog

    mov     rbx, rcx
    bt      qword ptr [rbx], rdx
    jc      short _active

    ; Inactive expert stays unmapped.
    xor     rax, rax
    jmp     short _write

_active:
    mov     rax, rdx
    imul    rax, RAWRXD_SPARSE_BLOCK_BYTES

_write:
    test    r8, r8
    jz      short _done
    mov     qword ptr [r8], rax

_done:
    pop     rbx
    ret
Enhancement4_SparseVirtualization ENDP

END