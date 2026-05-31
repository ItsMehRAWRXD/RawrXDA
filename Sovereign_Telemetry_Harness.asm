; ==============================================================================
; Sovereign Telemetry Stress Harness - Cache-Aligned Circular Buffer
; ==============================================================================
; Zero-allocation telemetry path using cache-line-aligned ring buffer.
; Measures hook latency under three conditions:
;   1. No hooks (baseline)
;   2. Hooks + Ghost Engine, telemetry disabled
;   3. Hooks + Ghost Engine, telemetry enabled
;
; Architecture:
;   - TELEMETRY_RING: 64-byte aligned circular buffer
;   - RDPMC counters for L1/L2 cache miss profiling
;   - Non-blocking push (atomic index advancement)
;   - No malloc/HeapAlloc in hot path
;
; Exports:
;   INIT_TELEMETRY, PUSH_TELEMETRY, FLUSH_TELEMETRY
;   GET_TELEMETRY_STATS, ENABLE_TELEMETRY, DISABLE_TELEMETRY
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; External APIs
; ==============================================================================
EXTERN GetCurrentProcessId : PROC
EXTERN GetCurrentThreadId : PROC

; ==============================================================================
; Constants
; ==============================================================================
TELEMETRY_RING_SLOTS    equ 1024        ; Power of 2 for fast mask
TELEMETRY_SLOT_SIZE     equ 64          ; Cache line size
TELEMETRY_BUFFER_SIZE   equ TELEMETRY_RING_SLOTS * TELEMETRY_SLOT_SIZE

; Performance counter events
PMC_L1D_CACHE_MISSES    equ 0x00000001  ; L1 data cache misses
PMC_L2_CACHE_MISSES     equ 0x00000002  ; L2 cache misses
PMC_BRANCH_MISSES       equ 0x00000004  ; Branch mispredictions

; ==============================================================================
; Data Section - Cache-aligned telemetry structures
; ==============================================================================
.data
ALIGN 64

; Telemetry slot structure (64 bytes = 1 cache line)
; Layout:
;   [0-7]   Timestamp (RDTSC)
;   [8-15]  Process ID
;   [16-23] Thread ID
;   [24-31] Event Type
;   [32-39] Latency (cycles)
;   [40-47] Cache Misses (L1)
;   [48-55] Cache Misses (L2)
;   [56-63] Reserved (padding)
TELEMETRY_SLOT struc
    Timestamp       dq ?
    ProcessId       dq ?
    ThreadId        dq ?
    EventType       dq ?
    LatencyCycles   dq ?
    L1Misses        dq ?
    L2Misses        dq ?
    Reserved        dq ?
TELEMETRY_SLOT ends

; Ring buffer control structure (separate cache line)
ALIGN 64
TelemetryRing:
    Head            dq 0        ; Write index (producer)
    Tail            dq 0        ; Read index (consumer)
    SlotMask        dq TELEMETRY_RING_SLOTS - 1
    Enabled         dq 0        ; 1 = enabled, 0 = disabled
    DroppedCount    dq 0        ; Dropped events (ring full)
    PushCount       dq 0        ; Total pushes
    ; Pad to 64 bytes
    Padding         db 64 - 56 dup(0)

; Ring buffer slots
ALIGN 64
TelemetrySlots    TELEMETRY_SLOT TELEMETRY_RING_SLOTS dup(<>)

; Statistics accumulator
ALIGN 64
TelemetryStats:
    TotalLatency    dq 0
    MaxLatency      dq 0
    MinLatency      dq 0FFFFFFFFFFFFFFFFh
    TotalL1Misses   dq 0
    TotalL2Misses   dq 0
    EventCount      dq 0
    ; Pad to 64 bytes
    PadStats        db 64 - 48 dup(0)

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; INIT_TELEMETRY: Initialize telemetry ring buffer
; Returns: RAX = 1 (success)
; ==============================================================================
INIT_TELEMETRY PROC
    push rdi
    push rcx
    
    ; Clear ring buffer control
    lea rdi, TelemetryRing
    mov rcx, 64 / 8
    xor rax, rax
    rep stosq
    
    ; Reset slot mask
    mov qword ptr [TelemetryRing.SlotMask], TELEMETRY_RING_SLOTS - 1
    
    ; Clear all slots
    lea rdi, TelemetrySlots
    mov rcx, (TELEMETRY_RING_SLOTS * TELEMETRY_SLOT_SIZE) / 8
    xor rax, rax
    rep stosq
    
    ; Clear statistics
    lea rdi, TelemetryStats
    mov rcx, 64 / 8
    xor rax, rax
    rep stosq
    mov qword ptr [TelemetryStats.MinLatency], 0FFFFFFFFFFFFFFFFh
    
    ; Enable telemetry by default
    mov qword ptr [TelemetryRing.Enabled], 1
    
    pop rcx
    pop rdi
    mov rax, 1
    ret
INIT_TELEMETRY ENDP

; ==============================================================================
; ENABLE_TELEMETRY: Turn on telemetry collection
; ==============================================================================
ENABLE_TELEMETRY PROC
    mov qword ptr [TelemetryRing.Enabled], 1
    mov rax, 1
    ret
ENABLE_TELEMETRY ENDP

; ==============================================================================
; DISABLE_TELEMETRY: Turn off telemetry collection
; ==============================================================================
DISABLE_TELEMETRY PROC
    mov qword ptr [TelemetryRing.Enabled], 0
    mov rax, 1
    ret
DISABLE_TELEMETRY ENDP

; ==============================================================================
; PUSH_TELEMETRY: Non-blocking push to ring buffer
; RCX = EventType, RDX = LatencyCycles
; Returns: RAX = 1 (pushed), 0 (dropped)
; ==============================================================================
PUSH_TELEMETRY PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; Check if enabled
    mov rax, [TelemetryRing.Enabled]
    test rax, rax
    jz push_drop
    
    ; Get current head (atomic read)
    mov r12, [TelemetryRing.Head]
    
    ; Calculate next head
    mov r13, r12
    inc r13
    and r13, [TelemetryRing.SlotMask]
    
    ; Check if ring is full (next == tail)
    cmp r13, [TelemetryRing.Tail]
    je push_drop
    
    ; Get slot address
    lea rdi, TelemetrySlots
    mov rax, r12
    shl rax, 6              ; Multiply by 64 (TELEMETRY_SLOT_SIZE)
    add rdi, rax
    
    ; Read performance counters before write
    ; L1 cache misses
    mov ecx, 0              ; Counter 0
    rdpmc
    shl rdx, 32
    or rax, rdx
    mov rbx, rax            ; Save L1 misses start
    
    ; Fill slot
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov [rdi + TELEMETRY_SLOT.Timestamp], rax
    
    call GetCurrentProcessId
    mov [rdi + TELEMETRY_SLOT.ProcessId], rax
    
    call GetCurrentThreadId
    mov [rdi + TELEMETRY_SLOT.ThreadId], rax
    
    mov [rdi + TELEMETRY_SLOT.EventType], rcx
    mov [rdi + TELEMETRY_SLOT.LatencyCycles], rdx
    
    ; Read performance counters after write
    mov ecx, 0
    rdpmc
    shl rdx, 32
    or rax, rdx
    sub rax, rbx            ; Delta L1 misses
    mov [rdi + TELEMETRY_SLOT.L1Misses], rax
    
    ; L2 cache misses (counter 1)
    mov ecx, 1
    rdpmc
    shl rdx, 32
    or rax, rdx
    mov [rdi + TELEMETRY_SLOT.L2Misses], rax
    
    ; Memory barrier to ensure write is visible
    mfence
    
    ; Advance head (atomic)
    mov [TelemetryRing.Head], r13
    
    ; Update statistics
    inc qword ptr [TelemetryStats.EventCount]
    add [TelemetryStats.TotalLatency], rdx
    
    ; Max latency
    mov rax, [TelemetryStats.MaxLatency]
    cmp rdx, rax
    jbe skip_max
    mov [TelemetryStats.MaxLatency], rdx
skip_max:
    
    ; Min latency
    mov rax, [TelemetryStats.MinLatency]
    cmp rdx, rax
    jae skip_min
    mov [TelemetryStats.MinLatency], rdx
skip_min:
    
    mov rax, 1
    jmp push_exit
    
push_drop:
    inc qword ptr [TelemetryRing.DroppedCount]
    xor rax, rax
    
push_exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
PUSH_TELEMETRY ENDP

; ==============================================================================
; FLUSH_TELEMETRY: Consume all pending telemetry events
; Returns: RAX = Number of events consumed
; ==============================================================================
FLUSH_TELEMETRY PROC
    push rbx
    
    ; Calculate consumed = head - tail
    mov rax, [TelemetryRing.Head]
    sub rax, [TelemetryRing.Tail]
    and rax, [TelemetryRing.SlotMask]
    
    ; Reset tail to head (all consumed)
    mov rbx, [TelemetryRing.Head]
    mov [TelemetryRing.Tail], rbx
    
    pop rbx
    ret
FLUSH_TELEMETRY ENDP

; ==============================================================================
; GET_TELEMETRY_STATS: Retrieve accumulated statistics
; RCX = Pointer to output buffer (64 bytes)
;   [RCX+0]   = Total latency
;   [RCX+8]   = Max latency
;   [RCX+16]  = Min latency
;   [RCX+24]  = Total L1 misses
;   [RCX+32]  = Total L2 misses
;   [RCX+40]  = Event count
;   [RCX+48]  = Dropped count
;   [RCX+56]  = Enabled flag
; Returns: RAX = 1 always
; ==============================================================================
GET_TELEMETRY_STATS PROC
    mov rax, [TelemetryStats.TotalLatency]
    mov [rcx + 0], rax
    mov rax, [TelemetryStats.MaxLatency]
    mov [rcx + 8], rax
    mov rax, [TelemetryStats.MinLatency]
    mov [rcx + 16], rax
    mov rax, [TelemetryStats.TotalL1Misses]
    mov [rcx + 24], rax
    mov rax, [TelemetryStats.TotalL2Misses]
    mov [rcx + 32], rax
    mov rax, [TelemetryStats.EventCount]
    mov [rcx + 40], rax
    mov rax, [TelemetryRing.DroppedCount]
    mov [rcx + 48], rax
    mov rax, [TelemetryRing.Enabled]
    mov [rcx + 56], rax
    mov eax, 1
    ret
GET_TELEMETRY_STATS ENDP

; ==============================================================================
; RESET_TELEMETRY_STATS: Clear accumulated statistics
; ==============================================================================
RESET_TELEMETRY_STATS PROC
    push rdi
    push rcx
    
    lea rdi, TelemetryStats
    mov rcx, 64 / 8
    xor rax, rax
    rep stosq
    mov qword ptr [TelemetryStats.MinLatency], 0FFFFFFFFFFFFFFFFh
    
    pop rcx
    pop rdi
    mov rax, 1
    ret
RESET_TELEMETRY_STATS ENDP

; ==============================================================================
; GET_TELEMETRY_STATUS: Check if telemetry is enabled
; Returns: RAX = 1 (enabled), 0 (disabled)
; ==============================================================================
GET_TELEMETRY_STATUS PROC
    mov rax, [TelemetryRing.Enabled]
    ret
GET_TELEMETRY_STATUS ENDP

; ==============================================================================
; End
; ==============================================================================
end
