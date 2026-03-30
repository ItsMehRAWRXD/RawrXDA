OPTION CASEMAP:NONE

EXTERNDEF VirtualProtect:PROC
EXTERNDEF FlushInstructionCache:PROC
EXTERNDEF GetCurrentProcess:PROC

PUBLIC rawrxd_addr_patch_apply
PUBLIC rawrxd_addr_patch_revert
PUBLIC rawrxd_addr_patch_revert_all
PUBLIC rawrxd_addr_patch_is_active
PUBLIC rawrxd_addr_patch_dump
PUBLIC rawrxd_addr_patch_diag_get
PUBLIC rawrxd_addr_patch_diag_reset
PUBLIC rawrxd_addr_patch_active_count

PAGE_EXECUTE_READWRITE EQU 040h
MAX_RECORDS           EQU 64
MAX_PATCH_BYTES       EQU 64
LABEL_BYTES           EQU 64

ADDR_PATCH_OK             EQU 0
ADDR_PATCH_INVALID_ARGS   EQU 1
ADDR_PATCH_ALREADY_ACTIVE EQU 2
ADDR_PATCH_NOT_ACTIVE     EQU 3
ADDR_PATCH_NO_FREE_SLOT   EQU 4
ADDR_PATCH_PROTECT_FAILED EQU 5
ADDR_PATCH_TOO_LARGE      EQU 6
ADDR_PATCH_DUMP_FAILED    EQU 7

REC_ADDRESS EQU 0
REC_LENGTH  EQU 8
REC_SEQ     EQU 16
REC_ACTIVE  EQU 24
REC_LABEL   EQU 32
REC_ORIG    EQU 96
REC_PATCH   EQU 160
REC_SIZE    EQU 224

.data
    align 8
    g_addr_patch_lock      dq 0
    g_addr_patch_seq       dq 1
    g_diag_applied         dq 0
    g_diag_reverted        dq 0
    g_diag_failed          dq 0
    g_addr_patch_default   db "addr_patch", 0
    g_addr_patch_hex       db "0123456789ABCDEF", 0
    g_dump_hdr_records     db "AddressHotpatcher records=", 0
    g_dump_hdr_active      db " active=", 0
    g_dump_line_seq        db "  seq=", 0
    g_dump_line_addr       db " addr=0x", 0
    g_dump_line_state      db " active=", 0
    g_dump_line_len        db " len=", 0
    g_dump_line_label      db " label=", 0
    g_dump_crlf            db 13, 10, 0
    align 8
    g_addr_patch_records   db MAX_RECORDS * REC_SIZE dup (0)

.code

addr_patch_lock_acquire PROC
APL_retry:
    xor     eax, eax
    mov     rcx, 1
    lock cmpxchg qword ptr [g_addr_patch_lock], rcx
    je      APL_done
    pause
    jmp     APL_retry
APL_done:
    ret
addr_patch_lock_acquire ENDP

addr_patch_lock_release PROC
    mov     qword ptr [g_addr_patch_lock], 0
    ret
addr_patch_lock_release ENDP

addr_patch_copy_bytes PROC
    test    r8, r8
    jz      APCB_done
APCB_loop:
    mov     al, byte ptr [rdx]
    mov     byte ptr [rcx], al
    inc     rcx
    inc     rdx
    dec     r8
    jnz     APCB_loop
APCB_done:
    ret
addr_patch_copy_bytes ENDP

addr_patch_copy_label PROC
    test    r8, r8
    jz      APCL_done
    dec     r8
APCL_loop:
    test    r8, r8
    jz      APCL_zero
    mov     al, byte ptr [rdx]
    test    al, al
    jz      APCL_zero
    mov     byte ptr [rcx], al
    inc     rcx
    inc     rdx
    dec     r8
    jmp     APCL_loop
APCL_zero:
    mov     byte ptr [rcx], 0
APCL_done:
    ret
addr_patch_copy_label ENDP

addr_patch_append_cstr PROC
    mov     r9, qword ptr [rdx]
    mov     r10, qword ptr [r8]
    test    r10, r10
    jz      APAS_done
APAS_loop:
    mov     al, byte ptr [rcx]
    test    al, al
    jz      APAS_store
    test    r10, r10
    jz      APAS_store
    mov     byte ptr [r9], al
    inc     r9
    inc     rcx
    dec     r10
    jmp     APAS_loop
APAS_store:
    mov     qword ptr [rdx], r9
    mov     qword ptr [r8], r10
APAS_done:
    ret
addr_patch_append_cstr ENDP

addr_patch_append_dec PROC
    push    rbx
    sub     rsp, 40
    mov     r11, rdx
    mov     r9, qword ptr [r11]
    mov     r10, qword ptr [r8]
    test    r10, r10
    jz      APAD_store
    mov     rax, rcx
    xor     ecx, ecx
    test    rax, rax
    jnz     APAD_collect
    mov     byte ptr [rsp], '0'
    mov     ecx, 1
    jmp     APAD_emit
APAD_collect:
APAD_loop:
    xor     rdx, rdx
    mov     rbx, 10
    div     rbx
    add     dl, '0'
    mov     byte ptr [rsp+rcx], dl
    inc     ecx
    test    rax, rax
    jnz     APAD_loop
APAD_emit:
    test    ecx, ecx
    jz      APAD_store
APAD_emit_loop:
    test    r10, r10
    jz      APAD_store
    dec     ecx
    mov     al, byte ptr [rsp+rcx]
    mov     byte ptr [r9], al
    inc     r9
    dec     r10
    test    ecx, ecx
    jnz     APAD_emit_loop
APAD_store:
    mov     qword ptr [r11], r9
    mov     qword ptr [r8], r10
    add     rsp, 40
    pop     rbx
    ret
addr_patch_append_dec ENDP

addr_patch_append_hex64 PROC
    push    rdi
    mov     r11, rdx
    mov     r9, qword ptr [r11]
    mov     r10, qword ptr [r8]
    test    r10, r10
    jz      APAH_store
    mov     rax, rcx
    mov     ecx, 16
    xor     edi, edi
APAH_loop:
    mov     rdx, rax
    shr     rdx, 60
    and     edx, 0Fh
    test    edi, edi
    jnz     APAH_emit
    test    edx, edx
    jnz     APAH_mark
    cmp     ecx, 1
    jne     APAH_shift
APAH_mark:
    mov     edi, 1
APAH_emit:
    test    r10, r10
    jz      APAH_store
    movzx   edx, byte ptr [g_addr_patch_hex+rdx]
    mov     byte ptr [r9], dl
    inc     r9
    dec     r10
APAH_shift:
    shl     rax, 4
    dec     ecx
    jnz     APAH_loop
APAH_store:
    mov     qword ptr [r11], r9
    mov     qword ptr [r8], r10
    pop     rdi
    ret
addr_patch_append_hex64 ENDP

rawrxd_addr_patch_diag_get PROC FRAME
    .endprolog
    test    rcx, rcx
    jz      RPDG_skip_applied
    mov     rax, qword ptr [g_diag_applied]
    mov     qword ptr [rcx], rax
RPDG_skip_applied:
    test    rdx, rdx
    jz      RPDG_skip_reverted
    mov     rax, qword ptr [g_diag_reverted]
    mov     qword ptr [rdx], rax
RPDG_skip_reverted:
    test    r8, r8
    jz      RPDG_done
    mov     rax, qword ptr [g_diag_failed]
    mov     qword ptr [r8], rax
RPDG_done:
    ret
rawrxd_addr_patch_diag_get ENDP

rawrxd_addr_patch_diag_reset PROC FRAME
    .endprolog
    mov     qword ptr [g_diag_applied], 0
    mov     qword ptr [g_diag_reverted], 0
    mov     qword ptr [g_diag_failed], 0
    ret
rawrxd_addr_patch_diag_reset ENDP

rawrxd_addr_patch_active_count PROC FRAME
    .endprolog
    call    addr_patch_lock_acquire
    xor     rax, rax
    lea     rdx, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPAC_loop:
    cmp     qword ptr [rdx+REC_ACTIVE], 0
    je      RPAC_next
    inc     rax
RPAC_next:
    add     rdx, REC_SIZE
    dec     ecx
    jnz     RPAC_loop
    call    addr_patch_lock_release
    ret
rawrxd_addr_patch_active_count ENDP

rawrxd_addr_patch_apply PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     r13, rcx
    mov     r14, rdx
    mov     r15, r8
    mov     r12, r9

    test    r13, r13
    jz      RPAP_invalid
    test    r14, r14
    jz      RPAP_invalid
    test    r15, r15
    jz      RPAP_invalid
    cmp     r15, MAX_PATCH_BYTES
    ja      RPAP_toolarge

    call    addr_patch_lock_acquire

    lea     rsi, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPAP_find_active:
    cmp     qword ptr [rsi+REC_ACTIVE], 0
    je      RPAP_next_active
    cmp     qword ptr [rsi+REC_ADDRESS], r13
    je      RPAP_already
RPAP_next_active:
    add     rsi, REC_SIZE
    dec     ecx
    jnz     RPAP_find_active

    lea     rbx, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPAP_find_slot:
    cmp     qword ptr [rbx+REC_ACTIVE], 0
    je      RPAP_slot_found
    add     rbx, REC_SIZE
    dec     ecx
    jnz     RPAP_find_slot
    lock inc qword ptr [g_diag_failed]
    call    addr_patch_lock_release
    mov     eax, ADDR_PATCH_NO_FREE_SLOT
    jmp     RPAP_exit

RPAP_slot_found:
    lea     r9, [rsp+32]
    mov     rcx, r13
    mov     rdx, r15
    mov     r8d, PAGE_EXECUTE_READWRITE
    call    VirtualProtect
    test    eax, eax
    jz      RPAP_protect_fail

    lea     rcx, [rbx+REC_ORIG]
    mov     rdx, r13
    mov     r8, r15
    call    addr_patch_copy_bytes

    lea     rcx, [rbx+REC_PATCH]
    mov     rdx, r14
    mov     r8, r15
    call    addr_patch_copy_bytes

    mov     rcx, r13
    mov     rdx, r14
    mov     r8, r15
    call    addr_patch_copy_bytes

    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, r13
    mov     r8, r15
    call    FlushInstructionCache

    lea     r9, [rsp+40]
    mov     rcx, r13
    mov     rdx, r15
    mov     r8d, dword ptr [rsp+32]
    call    VirtualProtect

    mov     qword ptr [rbx+REC_ADDRESS], r13
    mov     qword ptr [rbx+REC_LENGTH], r15
    mov     rax, qword ptr [g_addr_patch_seq]
    mov     qword ptr [rbx+REC_SEQ], rax
    inc     qword ptr [g_addr_patch_seq]
    mov     qword ptr [rbx+REC_ACTIVE], 1
    lock inc qword ptr [g_diag_applied]

    lea     rcx, [rbx+REC_LABEL]
    test    r12, r12
    jz      RPAP_default_label
    cmp     byte ptr [r12], 0
    je      RPAP_default_label
    mov     rdx, r12
    jmp     RPAP_copy_label
RPAP_default_label:
    lea     rdx, g_addr_patch_default
RPAP_copy_label:
    mov     r8, LABEL_BYTES
    call    addr_patch_copy_label
    call    addr_patch_lock_release
    xor     eax, eax
    jmp     RPAP_exit

RPAP_already:
    call    addr_patch_lock_release
    mov     eax, ADDR_PATCH_ALREADY_ACTIVE
    jmp     RPAP_exit

RPAP_protect_fail:
    call    addr_patch_lock_release
    lock inc qword ptr [g_diag_failed]
    mov     eax, ADDR_PATCH_PROTECT_FAILED
    jmp     RPAP_exit

RPAP_invalid:
    lock inc qword ptr [g_diag_failed]
    mov     eax, ADDR_PATCH_INVALID_ARGS
    jmp     RPAP_exit

RPAP_toolarge:
    lock inc qword ptr [g_diag_failed]
    mov     eax, ADDR_PATCH_TOO_LARGE

RPAP_exit:
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_addr_patch_apply ENDP

; ===========================================================================
; rawrxd_addr_patch_revert -- revert one patch by address
;   rcx = void* address
;   returns eax: 0=OK, 1=INVALID_ARGS, 3=NOT_ACTIVE, 5=PROTECT_FAILED
; ===========================================================================
rawrxd_addr_patch_revert PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 64
    .allocstack 64
    .endprolog

    test    rcx, rcx
    jz      RPRV_invalid

    mov     r12, rcx
    call    addr_patch_lock_acquire

    lea     rbx, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPRV_search:
    cmp     qword ptr [rbx+REC_ACTIVE], 0
    je      RPRV_next
    cmp     qword ptr [rbx+REC_ADDRESS], r12
    je      RPRV_found
RPRV_next:
    add     rbx, REC_SIZE
    dec     ecx
    jnz     RPRV_search

    call    addr_patch_lock_release
    mov     eax, ADDR_PATCH_NOT_ACTIVE
    jmp     RPRV_exit

RPRV_found:
    mov     r13, qword ptr [rbx+REC_LENGTH]

    lea     r9,  [rsp+32]
    mov     rcx, r12
    mov     rdx, r13
    mov     r8d, PAGE_EXECUTE_READWRITE
    call    VirtualProtect
    test    eax, eax
    jz      RPRV_protect_fail

    mov     rdi, r12
    lea     rsi, [rbx+REC_ORIG]
    mov     rcx, r13
    rep movsb

    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, r12
    mov     r8,  r13
    call    FlushInstructionCache

    lea     r9,  [rsp+40]
    mov     rcx, r12
    mov     rdx, r13
    mov     r8d, dword ptr [rsp+32]
    call    VirtualProtect

    mov     qword ptr [rbx+REC_ACTIVE],  0
    mov     qword ptr [rbx+REC_ADDRESS], 0
    lock inc qword ptr [g_diag_reverted]

    call    addr_patch_lock_release
    xor     eax, eax
    jmp     RPRV_exit

RPRV_protect_fail:
    call    addr_patch_lock_release
    lock inc qword ptr [g_diag_failed]
    mov     eax, ADDR_PATCH_PROTECT_FAILED
    jmp     RPRV_exit

RPRV_invalid:
    lock inc qword ptr [g_diag_failed]
    mov     eax, ADDR_PATCH_INVALID_ARGS

RPRV_exit:
    add     rsp, 64
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_addr_patch_revert ENDP

; ===========================================================================
; rawrxd_addr_patch_revert_all -- revert every active patch
;   returns eax: 0=OK, 5=PROTECT_FAILED (at least one revert failed)
; ===========================================================================
rawrxd_addr_patch_revert_all PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 64
    .allocstack 64
    .endprolog

    call    addr_patch_lock_acquire

    lea     rbx, g_addr_patch_records
    mov     r12d, MAX_RECORDS
    xor     r13d, r13d

RPRA_loop:
    cmp     qword ptr [rbx+REC_ACTIVE], 0
    je      RPRA_next

    lea     r9,  [rsp+32]
    mov     rcx, qword ptr [rbx+REC_ADDRESS]
    mov     rdx, qword ptr [rbx+REC_LENGTH]
    mov     r8d, PAGE_EXECUTE_READWRITE
    call    VirtualProtect
    test    eax, eax
    jz      RPRA_fail_one

    mov     rdi, qword ptr [rbx+REC_ADDRESS]
    lea     rsi, [rbx+REC_ORIG]
    mov     rcx, qword ptr [rbx+REC_LENGTH]
    rep movsb

    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, qword ptr [rbx+REC_ADDRESS]
    mov     r8,  qword ptr [rbx+REC_LENGTH]
    call    FlushInstructionCache

    lea     r9,  [rsp+40]
    mov     rcx, qword ptr [rbx+REC_ADDRESS]
    mov     rdx, qword ptr [rbx+REC_LENGTH]
    mov     r8d, dword ptr [rsp+32]
    call    VirtualProtect

    mov     qword ptr [rbx+REC_ACTIVE],  0
    mov     qword ptr [rbx+REC_ADDRESS], 0
    lock inc qword ptr [g_diag_reverted]
    jmp     RPRA_next

RPRA_fail_one:
    lock inc qword ptr [g_diag_failed]
    inc     r13d

RPRA_next:
    add     rbx, REC_SIZE
    dec     r12d
    jnz     RPRA_loop

    call    addr_patch_lock_release

    test    r13d, r13d
    jz      RPRA_ok
    mov     eax, ADDR_PATCH_PROTECT_FAILED
    jmp     RPRA_exit
RPRA_ok:
    xor     eax, eax
RPRA_exit:
    add     rsp, 64
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_addr_patch_revert_all ENDP

; ===========================================================================
; rawrxd_addr_patch_is_active -- query whether an address is currently patched
;   rcx = void* address
;   returns eax: 1=active, 0=not active / invalid
; ===========================================================================
rawrxd_addr_patch_is_active PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    test    rcx, rcx
    jz      RPIA_earlyfail

    mov     r12, rcx
    call    addr_patch_lock_acquire

    lea     rbx, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPIA_loop:
    cmp     qword ptr [rbx+REC_ACTIVE], 0
    je      RPIA_next
    cmp     qword ptr [rbx+REC_ADDRESS], r12
    je      RPIA_found
RPIA_next:
    add     rbx, REC_SIZE
    dec     ecx
    jnz     RPIA_loop

    call    addr_patch_lock_release
    xor     eax, eax
    jmp     RPIA_exit

RPIA_found:
    call    addr_patch_lock_release
    mov     eax, 1
    jmp     RPIA_exit

RPIA_earlyfail:
    xor     eax, eax

RPIA_exit:
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
rawrxd_addr_patch_is_active ENDP

; ===========================================================================
; rawrxd_addr_patch_dump -- format all active records into a text buffer
;   rcx = char* outBuffer
;   rdx = size_t outBufferLen
;   returns eax: 0=OK, 7=DUMP_FAILED
;
; Header:   "AddressHotpatcher records=64 active=N\r\n"
; Per-rec:  "  seq=S addr=0x<hex> active=1 len=L label=<label>\r\n"
;
; Stack frame (sub rsp,48): [rsp+32]=pdest [rsp+40]=premaining
; ===========================================================================
rawrxd_addr_patch_dump PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 48
    .allocstack 48
    .endprolog

    test    rcx, rcx
    jz      RPDD_fail
    test    rdx, rdx
    jz      RPDD_fail
    cmp     rdx, 2
    jb      RPDD_fail

    mov     qword ptr [rsp+32], rcx
    dec     rdx
    mov     qword ptr [rsp+40], rdx

    call    addr_patch_lock_acquire

    ; Count active records -> r13d
    xor     r13d, r13d
    lea     rbx, g_addr_patch_records
    mov     r12d, MAX_RECORDS
RPDD_count:
    cmp     qword ptr [rbx+REC_ACTIVE], 0
    je      RPDD_count_next
    inc     r13d
RPDD_count_next:
    add     rbx, REC_SIZE
    dec     r12d
    jnz     RPDD_count

    ; Header line
    lea     rcx, g_dump_hdr_records
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    mov     rcx, MAX_RECORDS
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_dec

    lea     rcx, g_dump_hdr_active
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    mov     ecx, r13d
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_dec

    lea     rcx, g_dump_crlf
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    ; Per-record lines
    lea     rbx, g_addr_patch_records
    mov     r12d, MAX_RECORDS
RPDD_iter:
    cmp     qword ptr [rbx+REC_ACTIVE], 0
    je      RPDD_iter_next

    lea     rcx, g_dump_line_seq
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    mov     rcx, qword ptr [rbx+REC_SEQ]
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_dec

    lea     rcx, g_dump_line_addr
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    mov     rcx, qword ptr [rbx+REC_ADDRESS]
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_hex64

    lea     rcx, g_dump_line_state
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    mov     rcx, 1
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_dec

    lea     rcx, g_dump_line_len
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    mov     rcx, qword ptr [rbx+REC_LENGTH]
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_dec

    lea     rcx, g_dump_line_label
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    lea     rcx, [rbx+REC_LABEL]
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

    lea     rcx, g_dump_crlf
    lea     rdx, [rsp+32]
    lea     r8,  [rsp+40]
    call    addr_patch_append_cstr

RPDD_iter_next:
    add     rbx, REC_SIZE
    dec     r12d
    jnz     RPDD_iter

    mov     rax, qword ptr [rsp+32]
    mov     byte ptr [rax], 0

    call    addr_patch_lock_release
    xor     eax, eax
    jmp     RPDD_exit

RPDD_fail:
    mov     eax, ADDR_PATCH_DUMP_FAILED

RPDD_exit:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rbx
    ret
rawrxd_addr_patch_dump ENDP

END
RPAP_copy_label:
    mov     r8, LABEL_BYTES
    call    addr_patch_copy_label

    call    addr_patch_lock_release
    xor     eax, eax
    jmp     RPAP_exit

RPAP_protect_fail:
    lock inc qword ptr [g_diag_failed]
    call    addr_patch_lock_release
    mov     eax, ADDR_PATCH_PROTECT_FAILED
    jmp     RPAP_exit
RPAP_already:
    lock inc qword ptr [g_diag_failed]
    call    addr_patch_lock_release
    mov     eax, ADDR_PATCH_ALREADY_ACTIVE
    jmp     RPAP_exit
RPAP_invalid:
    lock inc qword ptr [g_diag_failed]
    mov     eax, ADDR_PATCH_INVALID_ARGS
    jmp     RPAP_exit
RPAP_toolarge:
    lock inc qword ptr [g_diag_failed]
    mov     eax, ADDR_PATCH_TOO_LARGE
RPAP_exit:
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_addr_patch_apply ENDP

rawrxd_addr_patch_revert PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     r12, rcx
    test    r12, r12
    jz      RPR_invalid

    call    addr_patch_lock_acquire
    lea     rbx, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPR_find:
    cmp     qword ptr [rbx+REC_ACTIVE], 0
    je      RPR_next
    cmp     qword ptr [rbx+REC_ADDRESS], r12
    je      RPR_found
RPR_next:
    add     rbx, REC_SIZE
    dec     ecx
    jnz     RPR_find
    lock inc qword ptr [g_diag_failed]
    call    addr_patch_lock_release
    mov     eax, ADDR_PATCH_NOT_ACTIVE
    jmp     RPR_exit

RPR_found:
    lea     r9, [rsp+32]
    mov     rcx, r12
    mov     rdx, qword ptr [rbx+REC_LENGTH]
    mov     r8d, PAGE_EXECUTE_READWRITE
    call    VirtualProtect
    test    eax, eax
    jz      RPR_protect_fail

    mov     rcx, r12
    lea     rdx, [rbx+REC_ORIG]
    mov     r8, qword ptr [rbx+REC_LENGTH]
    call    addr_patch_copy_bytes

    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, r12
    mov     r8, qword ptr [rbx+REC_LENGTH]
    call    FlushInstructionCache

    lea     r9, [rsp+40]
    mov     rcx, r12
    mov     rdx, qword ptr [rbx+REC_LENGTH]
    mov     r8d, dword ptr [rsp+32]
    call    VirtualProtect

    mov     qword ptr [rbx+REC_ACTIVE], 0
    lock inc qword ptr [g_diag_reverted]
    call    addr_patch_lock_release
    xor     eax, eax
    jmp     RPR_exit

RPR_protect_fail:
    lock inc qword ptr [g_diag_failed]
    call    addr_patch_lock_release
    mov     eax, ADDR_PATCH_PROTECT_FAILED
    jmp     RPR_exit
RPR_invalid:
    lock inc qword ptr [g_diag_failed]
    mov     eax, ADDR_PATCH_INVALID_ARGS
RPR_exit:
    add     rsp, 48
    pop     r12
    pop     rsi
    pop     rbx
    ret
rawrxd_addr_patch_revert ENDP

rawrxd_addr_patch_revert_all PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    .endprolog

    xor     ebx, ebx
    call    addr_patch_lock_acquire
    lea     r12, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPRA_loop:
    cmp     qword ptr [r12+REC_ACTIVE], 0
    je      RPRA_next

    lea     r9, [rsp+32]
    mov     rcx, qword ptr [r12+REC_ADDRESS]
    mov     rdx, qword ptr [r12+REC_LENGTH]
    mov     r8d, PAGE_EXECUTE_READWRITE
    call    VirtualProtect
    test    eax, eax
    jz      RPRA_fail

    mov     rcx, qword ptr [r12+REC_ADDRESS]
    lea     rdx, [r12+REC_ORIG]
    mov     r8, qword ptr [r12+REC_LENGTH]
    call    addr_patch_copy_bytes

    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, qword ptr [r12+REC_ADDRESS]
    mov     r8, qword ptr [r12+REC_LENGTH]
    call    FlushInstructionCache

    lea     r9, [rsp+40]
    mov     rcx, qword ptr [r12+REC_ADDRESS]
    mov     rdx, qword ptr [r12+REC_LENGTH]
    mov     r8d, dword ptr [rsp+32]
    call    VirtualProtect

    mov     qword ptr [r12+REC_ACTIVE], 0
    lock inc qword ptr [g_diag_reverted]
    jmp     RPRA_next

RPRA_fail:
    lock inc qword ptr [g_diag_failed]
    mov     ebx, ADDR_PATCH_PROTECT_FAILED
RPRA_next:
    add     r12, REC_SIZE
    dec     ecx
    jnz     RPRA_loop

    call    addr_patch_lock_release
    mov     eax, ebx
    add     rsp, 48
    pop     r12
    pop     rbx
    ret
rawrxd_addr_patch_revert_all ENDP

rawrxd_addr_patch_is_active PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rbx, rcx
    test    rbx, rbx
    jz      RPIA_false

    call    addr_patch_lock_acquire
    lea     rsi, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPIA_loop:
    cmp     qword ptr [rsi+REC_ACTIVE], 0
    je      RPIA_next
    cmp     qword ptr [rsi+REC_ADDRESS], rbx
    je      RPIA_true
RPIA_next:
    add     rsi, REC_SIZE
    dec     ecx
    jnz     RPIA_loop
    call    addr_patch_lock_release
RPIA_false:
    xor     eax, eax
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
RPIA_true:
    call    addr_patch_lock_release
    mov     eax, 1
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
rawrxd_addr_patch_is_active ENDP

rawrxd_addr_patch_dump PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 80
    .allocstack 80
    .endprolog

    test    rcx, rcx
    jz      RPD_fail
    test    rdx, rdx
    jz      RPD_fail

    mov     qword ptr [rsp+32], rcx
    mov     rax, rdx
    dec     rax
    mov     qword ptr [rsp+40], rax

    call    addr_patch_lock_acquire

    xor     r14d, r14d
    xor     r15d, r15d
    lea     rsi, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPD_count_loop:
    cmp     qword ptr [rsi+REC_SEQ], 0
    je      RPD_count_next
    inc     r14
    cmp     qword ptr [rsi+REC_ACTIVE], 0
    je      RPD_count_next
    inc     r15
RPD_count_next:
    add     rsi, REC_SIZE
    dec     ecx
    jnz     RPD_count_loop

    lea     rcx, g_dump_hdr_records
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr
    mov     rcx, r14
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_dec
    lea     rcx, g_dump_hdr_active
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr
    mov     rcx, r15
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_dec
    lea     rcx, g_dump_crlf
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr

    lea     r12, g_addr_patch_records
    mov     ecx, MAX_RECORDS
RPD_line_loop:
    cmp     qword ptr [r12+REC_SEQ], 0
    je      RPD_line_next

    lea     rcx, g_dump_line_seq
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr
    mov     rcx, qword ptr [r12+REC_SEQ]
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_dec

    lea     rcx, g_dump_line_addr
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr
    mov     rcx, qword ptr [r12+REC_ADDRESS]
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_hex64

    lea     rcx, g_dump_line_state
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr
    mov     rcx, qword ptr [r12+REC_ACTIVE]
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_dec

    lea     rcx, g_dump_line_len
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr
    mov     rcx, qword ptr [r12+REC_LENGTH]
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_dec

    lea     rcx, g_dump_line_label
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr
    lea     rcx, [r12+REC_LABEL]
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr
    lea     rcx, g_dump_crlf
    lea     rdx, [rsp+32]
    lea     r8, [rsp+40]
    call    addr_patch_append_cstr

RPD_line_next:
    add     r12, REC_SIZE
    dec     ecx
    jnz     RPD_line_loop

    mov     rax, qword ptr [rsp+32]
    mov     byte ptr [rax], 0
    call    addr_patch_lock_release
    xor     eax, eax
    jmp     RPD_exit

RPD_fail:
    mov     eax, ADDR_PATCH_DUMP_FAILED
RPD_exit:
    add     rsp, 80
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_addr_patch_dump ENDP

END
