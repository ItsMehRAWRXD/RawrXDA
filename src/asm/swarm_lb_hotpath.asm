; ═══════════════════════════════════════════════════════════════════
; swarm_lb_hotpath.asm — RawrXD RDTSC-Optimized Fast-Path Load Balancer
; ═══════════════════════════════════════════════════════════════════

; External Monolithic API (Beaconism)
EXTERN BeaconSend:PROC

PUBLIC rawrxd_lb_hotpath_select

.data
szLBBackpressure db "LB BRK: %s Node %s Saturated! (Latency %llu)", 0
szCritical    db "CRITICAL", 0
szWarning     db "WARNING", 0

.code

; ────────────────────────────────────────────────────────────────
; rawrxd_lb_hotpath_select
; RCX = Node Array Pointer (NodeCapacity struct)
; RDX = Node Count
; R8  = Latency Threshold (in cycles)
; Returns RAX = Node Index or -1 if all saturated
; ────────────────────────────────────────────────────────────────
rawrxd_lb_hotpath_select PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    xor     rax, rax            ; Best Node Index
    mov     r10, -1             ; Best Score (Min Latency)
    mov     r11, 0              ; Current Index

@search_loop:
    cmp     r11, rdx
    jge     @found_best

    ; Load NodeCapacity struct [RCX + R11 * StructSize]
    ; Assume StructSize = 32 bytes (aligned)
    ; offset 24 = last_rdtsc_latency (uint64_t)
    mov     r9, [rcx + r11*8 + 24] 
    
    ; RDTSC-based pressure filter
    cmp     r9, r8
    ja      @skip_node          ; Too high latency

    cmp     r9, r10
    jae     @skip_node
    
    mov     r10, r9             ; New min found
    mov     rax, r11            ; Update best index
    
@skip_node:
    inc     r11
    jmp     @search_loop

@found_best:
    cmp     r10, -1
    jne     @exit
    
    ; TRIGGER BACKPRESSURE SIGNAL (Emergency Halt)
    mov     ecx, 8              ; Beacon ID for SLB
    lea     rdx, szLBBackpressure
    lea     r8, szCritical
    mov     r9, r11             ; Index
    ; mov     [rsp+32], r10     ; Latency - Omitted for quick stack safety
    call    BeaconSend
    
    mov     rax, -1             ; Signal Saturation

@exit:
    leave
    ret
rawrxd_lb_hotpath_select ENDP

END
