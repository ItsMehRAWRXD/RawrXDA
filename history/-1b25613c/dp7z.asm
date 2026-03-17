; =============================================================================
; RawrXD_PerfCounters.asm — Centralized RDTSC Per-Kernel Cycle Histograms
; =============================================================================
; PURPOSE:
;   Provides RDTSC-based latency measurement for any MASM kernel entrypoint.
;   Each "slot" accumulates cycle counts into a histogram (8 log₂ buckets)
;   plus min/max/sum/count statistics, enabling P50/P90/P99 analysis.
;
; DESIGN:
;   - 64 measurement slots (one per kernel entrypoint)
;   - Each slot: 128 bytes (cache-line pair, avoids false sharing)
;   - Wait-free: uses `lock xadd` for count, simple stores for min/max
;   - Companion C++ bridge reads the raw counters and computes percentiles
;
; SLOT LAYOUT (128 bytes each):
;   +00h: count       (QWORD)  — number of completed measurements
;   +08h: totalCycles  (QWORD)  — sum of all cycle counts
;   +10h: minCycles    (QWORD)  — minimum observed cycles
;   +18h: maxCycles    (QWORD)  — maximum observed cycles
;   +20h: bucket[0]   (QWORD)  — cycles < 256
;   +28h: bucket[1]   (QWORD)  — cycles < 1024
;   +30h: bucket[2]   (QWORD)  — cycles < 4096
;   +38h: bucket[3]   (QWORD)  — cycles < 16384
;   +40h: bucket[4]   (QWORD)  — cycles < 65536
;   +48h: bucket[5]   (QWORD)  — cycles < 262144
;   +50h: bucket[6]   (QWORD)  — cycles < 1048576
;   +58h: bucket[7]   (QWORD)  — cycles >= 1048576
;   +60h: lastCycles   (QWORD) — most recent measurement
;   +68h: flags        (DWORD) — active flag, reserved
;   +6Ch: reserved     (DWORD) — padding to 128 bytes
;   +70h: reserved2    (QWORD) — padding
;   +78h: reserved3    (QWORD) — padding
;
; EXPORTS:
;   asm_perf_init            — Zero all slots
;   asm_perf_begin           — Start measurement for slot N (returns RDTSC)
;   asm_perf_end             — End measurement for slot N (records delta)
;   asm_perf_read_slot       — Copy slot data to caller buffer
;   asm_perf_reset_slot      — Zero a single slot
;   asm_perf_get_slot_count  — Return number of available slots (64)
;   asm_perf_get_table_base  — Return pointer to raw slot table
;
; ABI: Windows x64 | No CRT | No exceptions
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

include RawrXD_Common.inc

; =============================================================================
;  Constants
; =============================================================================

PERF_MAX_SLOTS      EQU 64
PERF_SLOT_SIZE      EQU 128        ; bytes per slot (2 cache lines)
PERF_TABLE_SIZE     EQU PERF_MAX_SLOTS * PERF_SLOT_SIZE  ; 8192 bytes

; Slot field offsets
SLOT_COUNT          EQU 00h
SLOT_TOTAL          EQU 08h
SLOT_MIN            EQU 10h
SLOT_MAX            EQU 18h
SLOT_BUCKET0        EQU 20h        ; < 256 cycles
SLOT_BUCKET1        EQU 28h        ; < 1K
SLOT_BUCKET2        EQU 30h        ; < 4K
SLOT_BUCKET3        EQU 38h        ; < 16K
SLOT_BUCKET4        EQU 40h        ; < 64K
SLOT_BUCKET5        EQU 48h        ; < 256K
SLOT_BUCKET6        EQU 50h        ; < 1M
SLOT_BUCKET7        EQU 58h        ; >= 1M
SLOT_LAST           EQU 60h
SLOT_FLAGS          EQU 68h

; Bucket thresholds
THRESH_0            EQU 256
THRESH_1            EQU 1024
THRESH_2            EQU 4096
THRESH_3            EQU 16384
THRESH_4            EQU 65536
THRESH_5            EQU 262144
THRESH_6            EQU 1048576

; =============================================================================
;  Data Section — 64-byte aligned slot table
; =============================================================================

.data
ALIGN 64
perf_slot_table     DB PERF_TABLE_SIZE DUP(0)
perf_initialized    DD 0

; =============================================================================
;  Code Section
; =============================================================================

.code

; =============================================================================
; asm_perf_init — Zero all measurement slots
; =============================================================================
; Returns: RAX = 0
; =============================================================================
asm_perf_init PROC
    push    rdi

    ; Zero entire table
    lea     rdi, perf_slot_table
    xor     eax, eax
    mov     ecx, PERF_TABLE_SIZE / 8      ; QWORD count
    rep     stosq

    ; Set min fields to MAX_UINT64 (so first measurement always wins)
    lea     rdi, perf_slot_table
    mov     rcx, PERF_MAX_SLOTS
    mov     rax, 0FFFFFFFFFFFFFFFFh
perf_init_min_loop:
    mov     QWORD PTR [rdi + SLOT_MIN], rax
    add     rdi, PERF_SLOT_SIZE
    dec     rcx
    jnz     perf_init_min_loop

    mov     DWORD PTR [perf_initialized], 1

    pop     rdi
    xor     eax, eax
    ret
asm_perf_init ENDP

; =============================================================================
; asm_perf_begin — Start timing for slot N
; =============================================================================
; RCX = slot index (0..63)
; Returns: RAX = current RDTSC value (caller saves this for asm_perf_end)
; =============================================================================
asm_perf_begin PROC
    ; Serialize to prevent out-of-order execution from skewing RDTSC
    ; Use RDTSCP for serialized read (if available), else LFENCE+RDTSC
    rdtscp                  ; EAX:EDX = timestamp, ECX = processor ID
    shl     rdx, 32
    or      rax, rdx        ; RAX = full 64-bit TSC
    ret
asm_perf_begin ENDP

; =============================================================================
; asm_perf_end — End timing for slot N, record delta
; =============================================================================
; RCX = slot index (0..63)
; RDX = start TSC value (from asm_perf_begin)
; Returns: RAX = cycle count delta
; =============================================================================
asm_perf_end PROC
    push    rbx
    push    rsi

    mov     rsi, rdx        ; rsi = start TSC

    ; Validate slot index
    cmp     ecx, PERF_MAX_SLOTS
    jge     perf_end_invalid

    ; Compute slot address
    mov     eax, ecx
    shl     eax, 7          ; * 128 = PERF_SLOT_SIZE
    lea     rbx, perf_slot_table
    add     rbx, rax        ; rbx = &slot[N]

    ; Read end TSC (serialized)
    rdtscp
    shl     rdx, 32
    or      rax, rdx        ; rax = end TSC

    ; Compute delta
    sub     rax, rsi        ; rax = cycles elapsed
    mov     rcx, rax        ; rcx = delta (preserve)

    ; ---- Update slot atomically ----

    ; count++  (lock xadd for thread safety)
    mov     rdx, 1
    lock xadd QWORD PTR [rbx + SLOT_COUNT], rdx

    ; totalCycles += delta  (lock xadd)
    lock xadd QWORD PTR [rbx + SLOT_TOTAL], rcx

    ; lastCycles = delta (simple store, benign race OK)
    mov     QWORD PTR [rbx + SLOT_LAST], rcx

    ; min update (compare-and-swap loop)
    mov     rax, QWORD PTR [rbx + SLOT_MIN]
perf_min_loop:
    cmp     rcx, rax
    jge     perf_min_done           ; delta >= current min, skip
    lock cmpxchg QWORD PTR [rbx + SLOT_MIN], rcx
    jnz     perf_min_loop           ; retry if someone else changed it
perf_min_done:

    ; max update
    mov     rax, QWORD PTR [rbx + SLOT_MAX]
perf_max_loop:
    cmp     rcx, rax
    jle     perf_max_done           ; delta <= current max, skip
    lock cmpxchg QWORD PTR [rbx + SLOT_MAX], rcx
    jnz     perf_max_loop
perf_max_done:

    ; ---- Bucket classification ----
    ; Determine which bucket to increment based on cycle count

    cmp     rcx, THRESH_0
    jl      perf_bucket_0
    cmp     rcx, THRESH_1
    jl      perf_bucket_1
    cmp     rcx, THRESH_2
    jl      perf_bucket_2
    cmp     rcx, THRESH_3
    jl      perf_bucket_3
    cmp     rcx, THRESH_4
    jl      perf_bucket_4
    cmp     rcx, THRESH_5
    jl      perf_bucket_5
    cmp     rcx, THRESH_6
    jl      perf_bucket_6
    jmp     perf_bucket_7

perf_bucket_0:
    lock inc QWORD PTR [rbx + SLOT_BUCKET0]
    jmp     perf_end_done
perf_bucket_1:
    lock inc QWORD PTR [rbx + SLOT_BUCKET1]
    jmp     perf_end_done
perf_bucket_2:
    lock inc QWORD PTR [rbx + SLOT_BUCKET2]
    jmp     perf_end_done
perf_bucket_3:
    lock inc QWORD PTR [rbx + SLOT_BUCKET3]
    jmp     perf_end_done
perf_bucket_4:
    lock inc QWORD PTR [rbx + SLOT_BUCKET4]
    jmp     perf_end_done
perf_bucket_5:
    lock inc QWORD PTR [rbx + SLOT_BUCKET5]
    jmp     perf_end_done
perf_bucket_6:
    lock inc QWORD PTR [rbx + SLOT_BUCKET6]
    jmp     perf_end_done
perf_bucket_7:
    lock inc QWORD PTR [rbx + SLOT_BUCKET7]

perf_end_done:
    mov     rax, rcx        ; return delta
    pop     rsi
    pop     rbx
    ret

perf_end_invalid:
    xor     eax, eax
    pop     rsi
    pop     rbx
    ret
asm_perf_end ENDP

; =============================================================================
; asm_perf_read_slot — Copy 128-byte slot data to caller buffer
; =============================================================================
; RCX = slot index (0..63)
; RDX = pointer to 128-byte output buffer
; Returns: RAX = 0 on success, -1 on invalid index
; =============================================================================
asm_perf_read_slot PROC
    cmp     ecx, PERF_MAX_SLOTS
    jge     perf_read_invalid

    test    rdx, rdx
    jz      perf_read_invalid

    push    rsi
    push    rdi

    ; Source = slot table + index * 128
    mov     eax, ecx
    shl     eax, 7
    lea     rsi, perf_slot_table
    add     rsi, rax

    ; Dest = caller buffer
    mov     rdi, rdx

    ; Copy 128 bytes (16 QWORDs)
    mov     ecx, 16
    rep     movsq

    pop     rdi
    pop     rsi
    xor     eax, eax
    ret

perf_read_invalid:
    mov     eax, -1
    ret
asm_perf_read_slot ENDP

; =============================================================================
; asm_perf_reset_slot — Zero a single slot, reset min to MAX
; =============================================================================
; RCX = slot index (0..63)
; Returns: RAX = 0 on success
; =============================================================================
asm_perf_reset_slot PROC
    cmp     ecx, PERF_MAX_SLOTS
    jge     perf_reset_invalid

    push    rdi

    mov     eax, ecx
    shl     eax, 7
    lea     rdi, perf_slot_table
    add     rdi, rax

    ; Zero the slot
    xor     eax, eax
    mov     ecx, PERF_SLOT_SIZE / 8
    push    rdi
    rep     stosq
    pop     rdi

    ; Reset min to MAX
    mov     rax, 0FFFFFFFFFFFFFFFFh
    mov     QWORD PTR [rdi + SLOT_MIN], rax

    pop     rdi
    xor     eax, eax
    ret

perf_reset_invalid:
    mov     eax, -1
    ret
asm_perf_reset_slot ENDP

; =============================================================================
; asm_perf_get_slot_count — Return number of available slots
; =============================================================================
; Returns: RAX = 64
; =============================================================================
asm_perf_get_slot_count PROC
    mov     eax, PERF_MAX_SLOTS
    ret
asm_perf_get_slot_count ENDP

; =============================================================================
; asm_perf_get_table_base — Return pointer to raw slot table
; =============================================================================
; Returns: RAX = pointer to perf_slot_table
; =============================================================================
asm_perf_get_table_base PROC
    lea     rax, perf_slot_table
    ret
asm_perf_get_table_base ENDP

; =============================================================================
;  PUBLIC EXPORTS
; =============================================================================
PUBLIC asm_perf_init
PUBLIC asm_perf_begin
PUBLIC asm_perf_end
PUBLIC asm_perf_read_slot
PUBLIC asm_perf_reset_slot
PUBLIC asm_perf_get_slot_count
PUBLIC asm_perf_get_table_base

END
