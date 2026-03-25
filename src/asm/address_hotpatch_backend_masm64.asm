option casemap:none

EXTERN VirtualProtect: PROC
EXTERN FlushInstructionCache: PROC
EXTERN GetCurrentProcess: PROC
EXTERN OutputDebugStringA: PROC

PAGE_EXECUTE_READWRITE EQU 040h

.data
addr_patch_begin_msg db "[AddrPatch][MASM] apply begin", 0
addr_patch_ok_msg    db "[AddrPatch][MASM] apply ok", 0
addr_patch_fail_msg  db "[AddrPatch][MASM] apply fail", 0
addr_revert_begin_msg db "[AddrPatch][MASM] revert begin", 0
addr_revert_ok_msg    db "[AddrPatch][MASM] revert ok", 0
addr_revert_fail_msg  db "[AddrPatch][MASM] revert fail", 0
align 8
g_addr_patch_applied dq 0
g_addr_patch_reverted dq 0
g_addr_patch_failed dq 0

.code

PUBLIC rawrxd_addr_memcpy_asm
rawrxd_addr_memcpy_asm PROC FRAME
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    .endprolog

    test rcx, rcx
    jz short copy_invalid
    test rdx, rdx
    jz short copy_invalid
    test r8, r8
    jz short copy_invalid

    mov rdi, rcx
    mov rsi, rdx
    mov rax, r8
    mov rcx, r8
    rep movsb

    pop rdi
    pop rsi
    ret

copy_invalid:
    xor eax, eax
    pop rdi
    pop rsi
    ret
rawrxd_addr_memcpy_asm ENDP

PUBLIC rawrxd_addr_patch_diag_get_asm
rawrxd_addr_patch_diag_get_asm PROC FRAME
    .endprolog
    test rcx, rcx
    jz short diag_skip_applied
    mov rax, qword ptr [g_addr_patch_applied]
    mov qword ptr [rcx], rax

diag_skip_applied:
    test rdx, rdx
    jz short diag_skip_reverted
    mov rax, qword ptr [g_addr_patch_reverted]
    mov qword ptr [rdx], rax

diag_skip_reverted:
    test r8, r8
    jz short diag_done
    mov rax, qword ptr [g_addr_patch_failed]
    mov qword ptr [r8], rax

diag_done:
    ret
rawrxd_addr_patch_diag_get_asm ENDP

PUBLIC rawrxd_addr_patch_diag_reset_asm
rawrxd_addr_patch_diag_reset_asm PROC FRAME
    .endprolog
    mov qword ptr [g_addr_patch_applied], 0
    mov qword ptr [g_addr_patch_reverted], 0
    mov qword ptr [g_addr_patch_failed], 0
    ret
rawrxd_addr_patch_diag_reset_asm ENDP

PUBLIC rawrxd_addr_patch_apply_asm
rawrxd_addr_patch_apply_asm PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 56
    .allocstack 56
    .endprolog

    mov r12, rcx
    mov r13, rdx
    mov r14, r8

    lea rcx, addr_patch_begin_msg
    call OutputDebugStringA

    lea r9, [rsp + 32]
    mov r8d, PAGE_EXECUTE_READWRITE
    mov rdx, r13
    mov rcx, r12
    call VirtualProtect
    test eax, eax
    jz short apply_fail

    mov rdi, r12
    mov rsi, r14
    mov rcx, r13
    rep movsb

    lea r9, [rsp + 40]
    mov r8d, DWORD PTR [rsp + 32]
    mov rdx, r13
    mov rcx, r12
    call VirtualProtect

    call GetCurrentProcess
    mov rcx, rax
    mov rdx, r12
    mov r8, r13
    call FlushInstructionCache

    lea rcx, addr_patch_ok_msg
    call OutputDebugStringA
    lock inc qword ptr [g_addr_patch_applied]

    xor eax, eax
    jmp short apply_done

apply_fail:
    lea rcx, addr_patch_fail_msg
    call OutputDebugStringA
    lock inc qword ptr [g_addr_patch_failed]
    mov eax, -1

apply_done:
    add rsp, 56
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
rawrxd_addr_patch_apply_asm ENDP

PUBLIC rawrxd_addr_patch_revert_asm
rawrxd_addr_patch_revert_asm PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 56
    .allocstack 56
    .endprolog

    mov r12, rcx
    mov r13, rdx
    mov r14, r8

    lea rcx, addr_revert_begin_msg
    call OutputDebugStringA

    lea r9, [rsp + 32]
    mov r8d, PAGE_EXECUTE_READWRITE
    mov rdx, r13
    mov rcx, r12
    call VirtualProtect
    test eax, eax
    jz short revert_fail

    mov rdi, r12
    mov rsi, r14
    mov rcx, r13
    rep movsb

    lea r9, [rsp + 40]
    mov r8d, DWORD PTR [rsp + 32]
    mov rdx, r13
    mov rcx, r12
    call VirtualProtect

    call GetCurrentProcess
    mov rcx, rax
    mov rdx, r12
    mov r8, r13
    call FlushInstructionCache

    lea rcx, addr_revert_ok_msg
    call OutputDebugStringA
    lock inc qword ptr [g_addr_patch_reverted]

    xor eax, eax
    jmp short revert_done

revert_fail:
    lea rcx, addr_revert_fail_msg
    call OutputDebugStringA
    lock inc qword ptr [g_addr_patch_failed]
    mov eax, -1

revert_done:
    add rsp, 56
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
rawrxd_addr_patch_revert_asm ENDP

END
