; ═══════════════════════════════════════════════════════════════════
; node_heartbeat.asm — RawrXD RDTSC-Based Failure Detection
; ═══════════════════════════════════════════════════════════════════

; External Monolithic API (Beaconism)
EXTERN BeaconSend:PROC

PUBLIC rawrxd_emit_heartbeat
PUBLIC rawrxd_check_heartbeat_timeout

.data
szNodeFailure db "NODE FAILURE: Node %s heartbeat TIMEOUT after %llu cycles", 0
szHeartbeatMsg db "HEARTBEAT: Timestamp %llu emitted", 0

.code

; ────────────────────────────────────────────────────────────────
; rawrxd_emit_heartbeat
; Returns RAX = Current RDTSC Timestamp
; ────────────────────────────────────────────────────────────────
rawrxd_emit_heartbeat PROC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    
    ; Optional: Send heartbeat beacon for debug audit
    ; mov     ecx, 10
    ; lea     rdx, szHeartbeatMsg
    ; mov     r8, rax
    ; call    BeaconSend
    ret
rawrxd_emit_heartbeat ENDP

; ────────────────────────────────────────────────────────────────
; rawrxd_check_heartbeat_timeout
; RCX = Last Heartbeat Timestamp
; RDX = Timeout Threshold (cycles)
; R8  = Pointer to NodeID (String)
; Returns RAX = 1 if TIMEOUT (Node Down), 0 if Up
; ────────────────────────────────────────────────────────────────
rawrxd_check_heartbeat_timeout PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    rdtsc                   ; Current cycles
    shl     rdx, 32
    or      rax, rdx        ; Current TSC in RAX
    
    mov     r9, rax         ; Backup current TSC
    sub     rax, rcx        ; Delta = Current - Last
    
    cmp     rax, rdx        ; Delta > Threshold?
    jbe     @is_alive
    
    ; TIMEOUT DETECTED
    mov     ecx, 8          ; Beacon ID
    lea     rdx, szNodeFailure
    mov     r8, r8          ; NodeID string
    mov     r9, rax         ; Delta cycles
    call    BeaconSend
    
    mov     rax, 1          ; Signal failure
    jmp     @exit

@is_alive:
    xor     rax, rax        ; Still up

@exit:
    leave
    ret
rawrxd_check_heartbeat_timeout ENDP

END
