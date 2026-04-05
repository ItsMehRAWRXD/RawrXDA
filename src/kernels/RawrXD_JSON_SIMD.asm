; ============================================================================
; RawrXD_JSON_SIMD.asm
; AVX2-accelerated scanner for JSON key "response":"
; RCX = buffer pointer, RDX = buffer length, R8 = pattern pointer (12 bytes)
; Returns RAX = pointer to value start (match + 12), or 0 when not found.
; ============================================================================

option casemap:none

.code

PUBLIC RawrXD_ScanResponseToken

RawrXD_ScanResponseToken PROC
    xor rax, rax
    test rcx, rcx
    jz not_found
    test r8, r8
    jz not_found
    cmp rdx, 12
    jb not_found

    movzx r9d, byte ptr [r8]
    vmovd xmm0, r9d
    vpbroadcastb ymm0, xmm0

scan_blocks:
    cmp rdx, 32
    jb scan_tail

    vmovdqu ymm1, ymmword ptr [rcx]
    vpcmpeqb ymm2, ymm1, ymm0
    vpmovmskb r10d, ymm2
    test r10d, r10d
    jz next_block

candidate_loop:
    bsf r11d, r10d
    lea rax, [rcx + r11]

    mov r9, rdx
    sub r9, r11
    cmp r9, 12
    jb clear_candidate

    mov r9, qword ptr [rax]
    cmp r9, qword ptr [r8]
    jne clear_candidate

    mov r9d, dword ptr [rax + 8]
    cmp r9d, dword ptr [r8 + 8]
    jne clear_candidate

    add rax, 12
    vzeroupper
    ret

clear_candidate:
    btr r10d, r11d
    test r10d, r10d
    jnz candidate_loop

next_block:
    add rcx, 32
    sub rdx, 32
    jmp scan_blocks

scan_tail:
    cmp rdx, 12
    jb not_found

tail_loop:
    mov r9, qword ptr [rcx]
    cmp r9, qword ptr [r8]
    jne tail_next

    mov r9d, dword ptr [rcx + 8]
    cmp r9d, dword ptr [r8 + 8]
    jne tail_next

    lea rax, [rcx + 12]
    vzeroupper
    ret

tail_next:
    inc rcx
    dec rdx
    cmp rdx, 12
    jae tail_loop

not_found:
    xor rax, rax
    vzeroupper
    ret
RawrXD_ScanResponseToken ENDP

END
