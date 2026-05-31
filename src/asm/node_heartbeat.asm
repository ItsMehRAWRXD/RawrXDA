; ═══════════════════════════════════════════════════════════════════
; node_heartbeat.asm — RawrXD RDTSC-Based Failure Detection
; ═══════════════════════════════════════════════════════════════════

.data
; g_last_zmm_heartbeat: defined in Sentinel.cpp
EXTERN g_last_zmm_heartbeat:QWORD

.code

; ────────────────────────────────────────────────────────────────
; rawrxd_emit_heartbeat
; Returns RAX = Current RDTSC Timestamp
; Side effect: Updates g_last_zmm_heartbeat (atomic-ish on x64)
; ────────────────────────────────────────────────────────────────
rawrxd_emit_heartbeat PROC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    
    ; Update global heartbeat tracker for Sentinel to monitor
    mov     [g_last_zmm_heartbeat], rax
    ret
rawrxd_emit_heartbeat ENDP

; ────────────────────────────────────────────────────────────────
; rawrxd_check_heartbeat_timeout
; RCX = Last Heartbeat Timestamp
; RDX = Timeout Threshold (cycles)
; R8  = Pointer to NodeID (String)
; Returns RAX = Delta if OK, 0xFFFFFFFFFFFFFFFF if TIMEOUT (Node Down)
; ────────────────────────────────────────────────────────────────
rawrxd_check_heartbeat_timeout PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    push    rdx             ; backup threshold
    rdtsc                   ; Current cycles
    shl     rdx, 32
    or      rax, rdx        ; Current TSC in RAX
    pop     r11             ; threshold in r11
    
    sub     rax, rcx        ; RAX = Delta = Current - Last
    
    cmp     rax, r11        ; Delta > Threshold?
    jbe     @is_alive
    
    ; TIMEOUT DETECTED
    mov     rax, 0FFFFFFFFFFFFFFFFh ; Signal failure
    jmp     @exit

@is_alive:
    ; return Delta in RAX (already there)

@exit:
    leave
    ret
rawrxd_check_heartbeat_timeout ENDP

END
