; rawrxd_context_search.asm
; Vector 2: Zero-copy context search kernel (AVX-512)
;
; Exports:
;   rawrxd_context_symbols_init_avx512(symbols, symbolCount) -> int (1=ok, 0=fail)
;   rawrxd_find_symbols_avx512(buffer, lengthBytes, outOffsets) -> matchCount
;
; Calling convention (Win64):
;   RCX, RDX, R8, R9 used for first four parameters.

OPTION CASEMAP:NONE

PUBLIC rawrxd_context_search_init
PUBLIC rawrxd_context_search_scan
PUBLIC rawrxd_context_search_find
PUBLIC rawrxd_context_symbols_init_avx512
PUBLIC rawrxd_find_symbols_avx512

.data
ALIGN 16
g_rawrxd_context_symbols dq 64 dup(0)

.code

ALIGN 16
rawrxd_context_search_init PROC
    ; rcx = symbols pointer
    ; rdx = symbol count
    test rcx, rcx
    jz init_fail

    ; Clamp to 64 symbols
    cmp rdx, 64
    jbe init_count_ok
    mov rdx, 64

init_count_ok:
    ; Zero destination table first
    lea r8, g_rawrxd_context_symbols
    xor r9d, r9d

init_zero_loop:
    cmp r9, 64
    jae init_copy
    mov qword ptr [r8 + r9*8], 0
    inc r9
    jmp init_zero_loop

init_copy:
    xor r9d, r9d
init_copy_loop:
    cmp r9, rdx
    jae init_ok
    mov r10, qword ptr [rcx + r9*8]
    mov qword ptr [r8 + r9*8], r10
    inc r9
    jmp init_copy_loop

init_ok:
    mov eax, 1
    ret

init_fail:
    xor eax, eax
    ret
rawrxd_context_search_init ENDP

ALIGN 16
rawrxd_context_symbols_init_avx512 PROC
    jmp rawrxd_context_search_init
rawrxd_context_symbols_init_avx512 ENDP

ALIGN 16
rawrxd_context_search_scan PROC
    ; rcx = pointer to memory-mapped buffer
    ; rdx = buffer length in bytes
    ; r8  = output offsets pointer
    ; returns rax = match count

    xor rax, rax                    ; byte offset within buffer
    xor r9, r9                      ; match count

    ; Load 64-symbol table into ZMM8-ZMM15 once.
    vmovdqu64 zmm8,  zmmword ptr [g_rawrxd_context_symbols + 0]
    vmovdqu64 zmm9,  zmmword ptr [g_rawrxd_context_symbols + 64]
    vmovdqu64 zmm10, zmmword ptr [g_rawrxd_context_symbols + 128]
    vmovdqu64 zmm11, zmmword ptr [g_rawrxd_context_symbols + 192]
    vmovdqu64 zmm12, zmmword ptr [g_rawrxd_context_symbols + 256]
    vmovdqu64 zmm13, zmmword ptr [g_rawrxd_context_symbols + 320]
    vmovdqu64 zmm14, zmmword ptr [g_rawrxd_context_symbols + 384]
    vmovdqu64 zmm15, zmmword ptr [g_rawrxd_context_symbols + 448]

    ; Sliding window bound: last valid start offset = len - 64.
    sub rdx, 64
    js exit_search

search_loop:
    cmp rax, rdx
    ja exit_search

    vmovdqu64 zmm0, zmmword ptr [rcx + rax]

    ; Compare each lane against 64 preloaded qword symbols.
    vpcmpeqq k1, zmm0, zmm8
    vpcmpeqq k2, zmm0, zmm9
    korw k1, k1, k2

    vpcmpeqq k2, zmm0, zmm10
    korw k1, k1, k2

    vpcmpeqq k2, zmm0, zmm11
    korw k1, k1, k2

    vpcmpeqq k2, zmm0, zmm12
    korw k1, k1, k2

    vpcmpeqq k2, zmm0, zmm13
    korw k1, k1, k2

    vpcmpeqq k2, zmm0, zmm14
    korw k1, k1, k2

    vpcmpeqq k2, zmm0, zmm15
    korw k1, k1, k2

    kmovw r10d, k1
    test r10d, r10d
    jz next_iter

    ; Store byte offset for this matching chunk.
    mov qword ptr [r8 + r9*8], rax
    inc r9

next_iter:
    add rax, 8
    jmp search_loop

exit_search:
    mov rax, r9
    ret
rawrxd_context_search_scan ENDP

ALIGN 16
rawrxd_find_symbols_avx512 PROC
    jmp rawrxd_context_search_scan
rawrxd_find_symbols_avx512 ENDP

ALIGN 16
rawrxd_context_search_find PROC
    ; rcx = buffer ptr, rdx = len bytes, r8 = single symbol hash
    ; returns rax = first matching byte offset, -1 when not found
    xor r9, r9
    sub rdx, 64
    js find_miss

find_loop:
    cmp r9, rdx
    ja find_miss

    vmovdqu64 zmm0, zmmword ptr [rcx + r9]
    vpbroadcastq zmm1, r8
    vpcmpeqq k1, zmm0, zmm1
    kmovw r10d, k1
    test r10d, r10d
    jnz find_hit

    add r9, 8
    jmp find_loop

find_hit:
    mov rax, r9
    ret

find_miss:
    mov rax, -1
    ret
rawrxd_context_search_find ENDP

END
