; ═══════════════════════════════════════════════════════════════════
; shard_router_hot_path.asm — RawrXD Low-Latency Shard Selection
; ═══════════════════════════════════════════════════════════════════

EXTERN g_hHeap:QWORD
EXTERN BeaconSend:PROC

PUBLIC rawrxd_shard_select_hotpath

.data
szRoutingMsg db "Routing Activity: Layer %d -> Node [%s]", 0
szLatencyErr db "Routing Warning: Node %s Latency SPIKE (%llu cycles)", 0

.code

; ────────────────────────────────────────────────────────────────
; rawrxd_shard_select_hotpath
; RCX = Next Layer ID
; RDX = Pointer to Node Health Array (NodeID + Latency)
; R8  = Node Count
; Returns RAX = Pointer to Selected Node ID
; ────────────────────────────────────────────────────────────────
rawrxd_shard_select_hotpath PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; 1. Performance Check (RDTSC)
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r10, rax            ; Current Cycle Start

    ; 2. Iterate and Select Best Latency (Simplified Min-Search)
    mov     r11, 0FFFFFFFFFFFFFFFFh ; Min Latency Found
    xor     r9, r9              ; Loop Index
    xor     rax, rax            ; Result Node Ptr

@node_loop:
    cmp     r9, r8
    jge     @found_node
    
    ; Load Node Latency (Assuming 64-bit latency at offset 64 of Node struct)
    mov     rcx, [rdx + r9*8 + 64]
    cmp     rcx, r11
    jae     @skip_node
    
    mov     r11, rcx            ; Update Min Latency
    mov     rax, rdx            ; Update Best Node Ptr (simplified)
    
@skip_node:
    inc     r9
    jmp     @node_loop

@found_node:
    ; 3. Log Routing Decision (Optional Beacon)
    ; ... (Logic to call BeaconSend omitted for brevity)

    leave
    ret
rawrxd_shard_select_hotpath ENDP

END
