option casemap:none

PUBLIC rawrxd_addr_memcpy_asm
PUBLIC rawrxd_addr_patch_apply_asm
PUBLIC rawrxd_addr_patch_revert_asm
PUBLIC rawrxd_addr_patch_diag_get_asm
PUBLIC rawrxd_addr_patch_diag_reset_asm

.data
    g_addr_patch_applied  dq 0
    g_addr_patch_reverted dq 0
    g_addr_patch_failed   dq 0

.code

rawrxd_addr_memcpy_asm PROC
    test rcx, rcx
    jz   memcpy_fail
    test rdx, rdx
    jz   memcpy_fail

    mov  rax, r8
    test r8, r8
    jz   memcpy_done

    mov  r10, rdi
    mov  r11, rsi
    mov  rdi, rcx
    mov  rsi, rdx
    mov  rcx, r8
    rep movsb
    mov  rsi, r11
    mov  rdi, r10

memcpy_done:
    ret

memcpy_fail:
    xor  rax, rax
    ret
rawrxd_addr_memcpy_asm ENDP

rawrxd_addr_patch_apply_asm PROC
    test rcx, rcx
    jz   apply_fail
    test r8, r8
    jz   apply_fail
    test rdx, rdx
    jz   apply_fail

    mov  r10, rdi
    mov  r11, rsi
    mov  rdi, rcx
    mov  rsi, r8
    mov  rcx, rdx
    rep movsb
    mov  rsi, r11
    mov  rdi, r10

    lock inc qword ptr [g_addr_patch_applied]
    xor  eax, eax
    ret

apply_fail:
    lock inc qword ptr [g_addr_patch_failed]
    mov  eax, 1
    ret
rawrxd_addr_patch_apply_asm ENDP

rawrxd_addr_patch_revert_asm PROC
    test rcx, rcx
    jz   revert_fail
    test r8, r8
    jz   revert_fail
    test rdx, rdx
    jz   revert_fail

    mov  r10, rdi
    mov  r11, rsi
    mov  rdi, rcx
    mov  rsi, r8
    mov  rcx, rdx
    rep movsb
    mov  rsi, r11
    mov  rdi, r10

    lock inc qword ptr [g_addr_patch_reverted]
    xor  eax, eax
    ret

revert_fail:
    lock inc qword ptr [g_addr_patch_failed]
    mov  eax, 1
    ret
rawrxd_addr_patch_revert_asm ENDP

rawrxd_addr_patch_diag_get_asm PROC
    test rcx, rcx
    jz   skip_applied
    mov  rax, qword ptr [g_addr_patch_applied]
    mov  qword ptr [rcx], rax
skip_applied:

    test rdx, rdx
    jz   skip_reverted
    mov  rax, qword ptr [g_addr_patch_reverted]
    mov  qword ptr [rdx], rax
skip_reverted:

    test r8, r8
    jz   diag_done
    mov  rax, qword ptr [g_addr_patch_failed]
    mov  qword ptr [r8], rax

diag_done:
    ret
rawrxd_addr_patch_diag_get_asm ENDP

rawrxd_addr_patch_diag_reset_asm PROC
    mov  qword ptr [g_addr_patch_applied], 0
    mov  qword ptr [g_addr_patch_reverted], 0
    mov  qword ptr [g_addr_patch_failed], 0
    ret
rawrxd_addr_patch_diag_reset_asm ENDP

END
