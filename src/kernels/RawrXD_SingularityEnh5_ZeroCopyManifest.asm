; RawrXD_SingularityEnh5_ZeroCopyManifest.asm
; Enhancement 5: Zero-Copy Manifest Mapping
; Mechanic: Shared section/object mapping for CPU-GPU page aliasing.

OPTION CASEMAP:NONE

RAWRXD_MANIFEST_PAGE_BYTES        EQU 65536

.CODE

Enhancement5_ZeroCopyManifest PROC FRAME
    ; rcx = section_base
    ; rdx = manifest_offset
    ; r8  = cpu_view_out
    ; r9  = gpu_view_out

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx
    mov     rax, rdx
    and     rax, 0FFFFFFFFFFFF0000h
    lea     rbx, [rsi + rax]

    ; CPU and GPU receive the same physical page-backed mapping address.
    test    r8, r8
    jz      short _skip_cpu
    mov     qword ptr [r8], rbx
_skip_cpu:
    test    r9, r9
    jz      short _done
    mov     qword ptr [r9], rbx

_done:
    pop     rsi
    pop     rbx
    ret
Enhancement5_ZeroCopyManifest ENDP

END