; ============================================================
; SDMA SCHEDULER - Single-producer ring buffer (PHASE 1)
; Platform: Win64 (AMD64) / Windows Server 2022
; Target: RX 7800 XT (RDNA3), 16GB BAR, GGML inference
; ============================================================

.code

ALIGN 64
SDMA_SCHEDULER_STATE STRUCT
    ring_base           DQ ?                ; Host VA (WC mapped)
    ring_gpu_addr       DQ ?                ; GPU-visible bus address
    head                DQ ?                ; Write pointer (bytes)
    tail_cache          DQ ?                ; Cached read pointer
    mmio_wptr           DQ ?                ; GPU MMIO: SDMA0_QUEUE0_RB_WPTR
    mmio_rptr           DQ ?                ; GPU MMIO: SDMA0_QUEUE0_RB_RPTR
    
    ; Burst scheduling state
    burst_accumulator   DQ ?                ; Bytes pending coalescing
    burst_deadline      DQ ?                ; RDTSC deadline (TSC cycles)
    pending_desc_count  DQ ?                ; Descriptors in current burst
    last_src            DQ ?                ; Last source address (for coalescing)
    
    ; Statistics (perf counters)
    descriptors_submitted   DQ ?
    bytes_moved             DQ ?
    coalescing_hits         DQ ?
    scheduling_stalls       DQ ?
SDMA_SCHEDULER_STATE ENDS

; Constants
SDMA_DESCRIPTOR_SIZE    EQU 32
SDMA_MAX_BURST_BYTES    EQU 2097152         ; 2MB hard limit
BAR_RING_MASK           EQU 268435455       ; 256MB - 1
SDMA_SCHEDULER_CORE     EQU 15              ; Last core, isolated

; ============================================================
; DESCRIPTOR PACKET STRUCTURE (SDMA COPY)
; ============================================================
SDMA_PKT_COPY_LINEAR STRUCT
    header              DD ?                ; [7:0]=0x02, [15:8]=engine
    sub_opcode          DB ?                ; 0x00 = linear
    flags               DB ?                ; [0]=fence, [1]=int, [2]=64b
    _pad0               DW ?
    src_addr_lo         DD ?
    src_addr_hi         DD ?
    dst_addr_lo         DD ?
    dst_addr_hi         DD ?
    count_lo            DD ?                ; [27:0]=bytes-1
    count_hi            DD ?
SDMA_PKT_COPY_LINEAR ENDS

ALIGN 64
g_sdma_scheduler_state  SDMA_SCHEDULER_STATE <>

; ============================================================
; EXTERNAL SYMBOLS (defined by C++ coordinator)
; ============================================================
EXTERN g_sdma_work_queue_head:QWord
EXTERN g_sdma_work_queue_tail:QWord
EXTERN g_tsc_freq_500ns:QWord
EXTERN g_ssot_full_beacon:QWORD             ; Beacon state (0x3 = full)

; ============================================================
; PUBLIC EXPORTS
; ============================================================
PUBLIC sdma_scheduler_entry
PUBLIC g_sdma_scheduler_state
PUBLIC sdma_scheduler_init_state

; ============================================================
; INITIALIZATION - Called once from coordinator
; Input: RCX = ring_base, RDX = ring_gpu_addr, R8 = mmio_wptr, R9 = mmio_rptr
; ============================================================
ALIGN 16
sdma_scheduler_init_state PROC
    mov     [g_sdma_scheduler_state.ring_base], rcx
    mov     [g_sdma_scheduler_state.ring_gpu_addr], rdx
    mov     [g_sdma_scheduler_state.head], 0
    mov     [g_sdma_scheduler_state.tail_cache], 0
    mov     [g_sdma_scheduler_state.mmio_wptr], r8
    mov     [g_sdma_scheduler_state.mmio_rptr], r9
    mov     [g_sdma_scheduler_state.burst_accumulator], 0
    mov     [g_sdma_scheduler_state.descriptors_submitted], 0
    mov     [g_sdma_scheduler_state.bytes_moved], 0
    mov     [g_sdma_scheduler_state.coalescing_hits], 0
    mov     [g_sdma_scheduler_state.scheduling_stalls], 0
    ret
ENDP

; ============================================================
; MAIN SCHEDULER LOOP - Runs on isolated core
; No function calls, no stack spills for performance
; ============================================================
ALIGN 16
sdma_scheduler_entry PROC
    ; Preload state into registers (never spills)
    lea     r15, [g_sdma_scheduler_state]
    mov     r14, [r15 + SDMA_SCHEDULER_STATE.ring_base]
    mov     r13, [r15 + SDMA_SCHEDULER_STATE.head]
    mov     r12, [r15 + SDMA_SCHEDULER_STATE.tail_cache]
    
    ; Precompute MMIO addresses
    mov     rbx, [r15 + SDMA_SCHEDULER_STATE.mmio_wptr]
    mov     rbp, [r15 + SDMA_SCHEDULER_STATE.mmio_rptr]
    
    xor     r10, r10                        ; pending_desc_count = 0

    ; ─────────────────────────────────────────────────────────
    ; MAIN LOOP
    ; ─────────────────────────────────────────────────────────
scheduler_loop:
    ; PHASE 1: Check for completed work (update tail cache)
    mov     rax, [rbp]                      ; Read GPU RPTR (MMIO)
    and     rax, BAR_RING_MASK
    mov     r12, rax                        ; Update tail_cache in r12

    ; PHASE 2: Poll work queue (lock-free MPSC)
    mov     rsi, [g_sdma_work_queue_head]
    mov     rdi, [g_sdma_work_queue_tail]
    cmp     rsi, rdi
    je      .no_work

    ; Prefetch next work item (64 bytes ahead)
    prefetchnta [rsi + 64]                  ; Next item

    ; PHASE 3: Burst coalescing decision
    ; Work item format:
    ;   [0-7]:   src_gpu_va
    ;   [8-15]:  dst_gpu_va
    ;   [16-23]: size_bytes
    ;   [24-31]: flags
    
    ; Extract size field
    mov     rax, [rsi + 16]                 ; size_bytes
    
    ; Check coalescing criteria
    rdtsc                                   ; Read current TSC
    shl     rdx, 32
    or      rax, rdx                        ; rax = current TSC
    
    mov     rcx, [r15 + SDMA_SCHEDULER_STATE.burst_deadline]
    cmp     rax, rcx
    ja      .flush_burst                    ; Deadline expired
    
    ; Check if we can coalesce
    mov     rcx, [r15 + SDMA_SCHEDULER_STATE.burst_accumulator]
    mov     rax, [rsi + 16]                 ; size_bytes
    test    rcx, rcx
    jz      .start_new_burst
    
    ; Check 4MB page alignment match
    mov     rdx, [rsi]                      ; New src
    mov     r8, [r15 + SDMA_SCHEDULER_STATE.last_src]
    shr     rdx, 22                         ; Extract 4MB page
    shr     r8, 22
    cmp     rdx, r8
    jne     .flush_burst
    
    ; Check size limit (avoid exceeding 2MB burst)
    add     rcx, rax
    cmp     rcx, SDMA_MAX_BURST_BYTES
    ja      .flush_burst
    
    ; Coalesce: accumulate
    mov     [r15 + SDMA_SCHEDULER_STATE.burst_accumulator], rcx
    inc     qword [r15 + SDMA_SCHEDULER_STATE.coalescing_hits]
    jmp     .advance_queue

.flush_burst:
    ; Emit accumulated burst descriptor
    ; (stub: full implementation in Phase 2)
    mov     qword [r15 + SDMA_SCHEDULER_STATE.burst_accumulator], 0
    mov     r10, 0                          ; Reset pending count

.start_new_burst:
    ; Initialize new burst tracking
    mov     rax, [rsi + 16]                 ; size_bytes
    mov     [r15 + SDMA_SCHEDULER_STATE.burst_accumulator], rax
    mov     rax, [rsi]
    mov     [r15 + SDMA_SCHEDULER_STATE.last_src], rax
    
    ; Set deadline: 500ns from now
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    add     rax, [g_tsc_freq_500ns]
    mov     [r15 + SDMA_SCHEDULER_STATE.burst_deadline], rax

.advance_queue:
    ; Advance work queue pointer
    add     rsi, 64
    mov     [g_sdma_work_queue_head], rsi
    
    ; PHASE 4: Ring space check and descriptor emission
    mov     rax, r12                        ; Tail
    sub     rax, r13                        ; Head
    dec     rax                             ; Available space
    shr     rax, 5                          ; Convert to entries
    test    rax, rax
    jz      .ring_full

    ; Build descriptor: SDMA_PKT_COPY_LINEAR in zmm1
    ; (stub: full descriptor construction in Phase 2)
    
    ; Non-temporal store to ring (placeholder)
    ; vmovntdq [r14 + r13], zmm1
    
    ; Advance head
    add     r13, 32
    and     r13, BAR_RING_MASK
    
    ; Update stats
    inc     qword [r15 + SDMA_SCHEDULER_STATE.descriptors_submitted]
    
    ; PHASE 5: Periodic WPTR update
    inc     r10                             ; pending_desc_count++
    cmp     r10, 16
    jb      .skip_wptr_update
    
    ; Write to MMIO (GPU WPTR)
    mov     eax, r13d
    mov     [rbx], eax
    mov     qword [r15 + SDMA_SCHEDULER_STATE.head], r13
    xor     r10, r10                        ; Reset pending count

.skip_wptr_update:
    jmp     scheduler_loop

.ring_full:
    inc     qword [r15 + SDMA_SCHEDULER_STATE.scheduling_stalls]
    pause                                   ; Spin-hint
    jmp     scheduler_loop

.no_work:
    ; Flush any pending partial burst
    mov     rax, [r15 + SDMA_SCHEDULER_STATE.burst_accumulator]
    test    rax, rax
    jnz     .flush_final_burst
    
    ; Idle: sleep with exponential backoff
    mov     eax, 1
    pause
    
    ; Check beacon state for adaptive throttling
    movzx   ecx, byte ptr [g_ssot_full_beacon]
    cmp     cl, 3
    jne     scheduler_loop                  ; Not full beacon, loop immediately
    
    ; At full beacon, can yield slightly
    ; (Stub for Windows YieldProcessor or Sleep(0))
    jmp     scheduler_loop

.flush_final_burst:
    ; Flush remaining descriptors before idle
    mov     qword [r15 + SDMA_SCHEDULER_STATE.burst_accumulator], 0
    mov     eax, r13d
    mov     [rbx], eax                      ; Final WPTR
    jmp     .no_work

ENDP

END
