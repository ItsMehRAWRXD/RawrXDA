; aperture_dispatch_avx512_win64.asm
; Phase 6 dispatcher support for RawrXD_Titan.
; Exposes capability detection and seal-hash bridge symbols used by titan_infer_dll.cpp.

OPTION CASEMAP:NONE

PUBLIC aperture_detect_avx512
PUBLIC aperture_compute_seal_hash
PUBLIC g_ApertureAvx512Supported
PUBLIC g_ApertureHashLatency_tsc

.DATA
ALIGN 8
g_ApertureAvx512Supported BYTE 0
ALIGN 8
g_ApertureHashLatency_tsc QWORD 0

.CODE

aperture_detect_avx512 PROC
    push rbx

    ; CPUID leaf 0: require leaf 7 availability.
    xor eax, eax
    cpuid
    cmp eax, 7
    jb detect_not_supported

    ; CPUID leaf 1: require AVX + OSXSAVE.
    mov eax, 1
    cpuid
    bt ecx, 28
    jnc detect_not_supported
    bt ecx, 27
    jnc detect_not_supported

    ; XGETBV(XCR0): require bits 1,2,5,6,7 => 0xE6.
    xor ecx, ecx
    xgetbv
    and eax, 0E6h
    cmp eax, 0E6h
    jne detect_not_supported

    ; CPUID leaf 7 subleaf 0: require AVX512F + AVX512DQ.
    mov eax, 7
    xor ecx, ecx
    cpuid
    bt ebx, 16
    jnc detect_not_supported
    bt ebx, 17
    jnc detect_not_supported

    mov byte ptr [g_ApertureAvx512Supported], 1
    jmp detect_done

detect_not_supported:
    mov byte ptr [g_ApertureAvx512Supported], 0

detect_done:
    pop rbx
    ret
aperture_detect_avx512 ENDP

; rcx = aperture_result, rdx = subdivision_table
; result layout: final_seal_hash at +24 (32 bytes)
; table layout: entry_count at +4, entries at +104, entry stride = 120
aperture_compute_seal_hash PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    test rcx, rcx
    jz hash_exit
    test rdx, rdx
    jz hash_exit

    mov rbx, rcx                    ; result
    mov r15, rdx                    ; table

    mov r14d, dword ptr [r15 + 4]   ; entry_count
    test r14d, r14d
    jnz hash_have_entries

    xor r8d, r8d
hash_zero_loop:
    cmp r8d, 32
    jae hash_zero_done
    mov byte ptr [rbx + 24 + r8], 0
    inc r8d
    jmp hash_zero_loop

hash_zero_done:
    mov qword ptr [g_ApertureHashLatency_tsc], 0
    jmp hash_exit

hash_have_entries:
    rdtsc
    shl rdx, 32
    mov eax, eax
    or rax, rdx
    mov r11, rax                    ; start_tsc

    mov r12, 1469598103934665603    ; h0
    mov r13, 1099511628211          ; h1
    mov r10, 1099511628211          ; fnv prime A
    mov r9,  1469598103934665603    ; fnv prime B

    lea r15, [r15 + 104]            ; first entry

hash_entry_loop:
    test r14d, r14d
    jz hash_finalize

    mov rdi, qword ptr [r15 + 0]    ; offset_bytes
    mov rsi, qword ptr [r15 + 8]    ; size_bytes
    lea rdx, [r15 + 16]             ; sha256_hash[32]

    xor r8d, r8d
hash_byte_loop:
    cmp r8d, 32
    jae hash_entry_done

    movzx eax, byte ptr [rdx + r8]
    mov rcx, rdi
    and rcx, 0FFh
    add rax, rcx
    xor r12, rax
    imul r12, r10

    movzx eax, byte ptr [rdx + r8]
    mov ecx, r8d
    and ecx, 7
    shl ecx, 3
    shl rax, cl
    xor rax, rsi
    xor r13, rax
    imul r13, r9

    inc r8d
    jmp hash_byte_loop

hash_entry_done:
    add r15, 120
    dec r14d
    jmp hash_entry_loop

hash_finalize:
    xor r8d, r8d
hash_out_h0:
    cmp r8d, 16
    jae hash_out_h1_start
    mov ecx, r8d
    and ecx, 7
    shl ecx, 3
    mov rax, r12
    shr rax, cl
    mov byte ptr [rbx + 24 + r8], al
    inc r8d
    jmp hash_out_h0

hash_out_h1_start:
    mov r8d, 16
hash_out_h1:
    cmp r8d, 32
    jae hash_latency
    mov ecx, r8d
    sub ecx, 16
    and ecx, 7
    shl ecx, 3
    mov rax, r13
    shr rax, cl
    mov byte ptr [rbx + 24 + r8], al
    inc r8d
    jmp hash_out_h1

hash_latency:
    rdtsc
    shl rdx, 32
    mov eax, eax
    or rax, rdx
    sub rax, r11
    mov qword ptr [g_ApertureHashLatency_tsc], rax

hash_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
aperture_compute_seal_hash ENDP

END
