OPTION CASEMAP:NONE

EXTERN ExitProcess:PROC
EXTERN GetModuleHandleA:PROC
EXTERN VALIDATE_TOKEN_SIMD:PROC
EXTERN BUILD_SYMBOL_HASH_TABLE:PROC
EXTERN RESOLVE_SYMBOL_HASH:PROC
EXTERN RESOLVE_SYMBOL_FROM_PE:PROC

.DATA
kernel32_name    BYTE "kernel32.dll",0
sym_exitprocess  BYTE "ExitProcess",0
sym_good_a       BYTE "NtReadVirtualMemory",0
sym_good_b       BYTE "NtReadVirtualMemory",0
sym_bad          BYTE "NtWriteVirtualMemory",0
len_exitprocess  DWORD 0
mod_kernel32     QWORD 0

.CODE

; Simple strlen limited to 32
strlen32 PROC FRAME
    .endprolog
    xor eax, eax
slen_loop:
    cmp eax, 32
    jae slen_done
    cmp BYTE PTR [rcx+rax], 0
    je slen_done
    inc eax
    jmp slen_loop
slen_done:
    ret
strlen32 ENDP

PUBLIC main
main PROC FRAME
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    xor ebx, ebx

    ; Test 1: SIMD equal token
    lea rcx, sym_good_a
    call strlen32
    mov r8d, eax
    lea rcx, sym_good_a
    lea rdx, sym_good_b
    call VALIDATE_TOKEN_SIMD
    test eax, eax
    jz test_fail
    inc ebx

    ; Test 2: SIMD different token
    lea rcx, sym_good_a
    call strlen32
    mov r8d, eax
    lea rcx, sym_good_a
    lea rdx, sym_bad
    call VALIDATE_TOKEN_SIMD
    test eax, eax
    jnz test_fail
    inc ebx

    ; Test 3: Resolve ExitProcess from kernel32 exports
    lea rcx, kernel32_name
    call GetModuleHandleA
    test rax, rax
    jz test_fail
    mov mod_kernel32, rax

    lea rdx, sym_exitprocess
    mov rcx, rdx
    call strlen32
    mov len_exitprocess, eax

    ; Test 3: Build hash table
    mov rcx, mod_kernel32
    call BUILD_SYMBOL_HASH_TABLE
    test rax, rax
    jz test_fail
    inc ebx

    ; Test 4: Resolve via hash table lookup
    lea rcx, sym_exitprocess
    mov edx, len_exitprocess
    call RESOLVE_SYMBOL_HASH
    test rax, rax
    jz test_fail
    inc ebx

    ; Test 5: Resolve via integrated API (build + lookup path)
    mov rcx, mod_kernel32
    lea rdx, sym_exitprocess
    mov r8d, len_exitprocess
    call RESOLVE_SYMBOL_FROM_PE
    test rax, rax
    jz test_fail
    inc ebx

    ; Expect 5 tests passed
    cmp ebx, 5
    jne test_fail

    xor ecx, ecx
    call ExitProcess

test_fail:
    mov ecx, 1
    call ExitProcess
main ENDP

END