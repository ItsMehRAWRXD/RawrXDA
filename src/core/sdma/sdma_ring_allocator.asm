; ============================================================
; SDMA RING / TENSOR SLOT ALLOCATOR (PHASE 1)
; Manages 256MB BAR ring buffer + tensor slot bitmap
; ============================================================

.code

; Constants
SDMA_DESCRIPTOR_SIZE        EQU 32
SDMA_MAX_IN_FLIGHT          EQU 256
BAR_RING_SIZE               EQU 268435456    ; 256MB
BAR_RING_MASK               EQU 268435455

TENSOR_SLOT_GRANULARITY     EQU 65536        ; 64KB slots
TENSOR_ARENA_SLOTS          EQU 262144       ; 16GB / 64KB

; ============================================================
; TENSOR SLOT MAP STRUCTURE
; ============================================================
TENSOR_SLOT_MAP STRUCT
    bitmap              DQ ?                ; 32KB bitmap (262144 bits)
    next_scan           DQ ?                ; Hint for O(1) allocation
    lock                DD ?                ; Spinlock
    _pad                DD ?
TENSOR_SLOT_MAP ENDS

ALIGN 64
g_tensor_slot_map TENSOR_SLOT_MAP <>

; ============================================================
; RING BUFFER ALLOCATOR STATE
; ============================================================
ALIGN 64
g_sdma_ring_allocator STRUCT
    entries_allocated   DQ ?                ; Count of in-flight descriptors
    total_bytes         DQ ?                ; Total bytes submitted
    ring_overflow_count DQ ?                ; Wrap-around counter
ENDS

; PUBLIC EXPORTS
PUBLIC allocate_tensor_slot
PUBLIC free_tensor_slot
PUBLIC sdma_ring_advance_head
PUBLIC g_tensor_slot_map

; ============================================================
; SDMA_RING_ADVANCE_HEAD
; Input:  RCX = current head, RDX = count (descriptors to insert)
; Output: RAX = new head (wrapped)
; ============================================================
ALIGN 16
sdma_ring_advance_head PROC
    mov     rax, rdx
    shl     rax, 5                         ; ×32 bytes per descriptor
    add     rax, rcx
    and     rax, BAR_RING_MASK              ; Wrap at ring boundary
    ret
ENDP

; ============================================================
; ALLOCATE_TENSOR_SLOT
; Input:  RCX = size_bytes
; Output: RAX = slot_index (or -1 if allocation failed)
; Clobbers: rdx, r8, r9, r10, r11, zmm0, k0
; ============================================================
ALIGN 16
allocate_tensor_slot PROC
    ; Convert size to slot count
    add     rcx, TENSOR_SLOT_GRANULARITY - 1
    shr     rcx, 16                        ; rcx = slots needed
    mov     r8, rcx                        ; r8 = slot count
    
    ; Acquire spinlock
    lea     r9, [g_tensor_slot_map.lock]
    mov     r10d, 1
.spin_lock:
    xchg    [r9], r10d
    test    r10d, r10d
    jz      .lock_acquired
    pause
    jmp     .spin_lock

.lock_acquired:
    ; Preload bitmap base
    lea     rsi, [g_tensor_slot_map.bitmap]
    mov     rdx, [g_tensor_slot_map.next_scan]
    
    ; Validate scan hint
    cmp     rdx, TENSOR_ARENA_SLOTS
    jb      @f
    xor     rdx, rdx                       ; Wrap hint if invalid
@@:
    
    mov     rdi, TENSOR_ARENA_SLOTS         ; Loop count

.scan_loop:
    ; Load 64 bytes (512 slots) at once using AVX-512
    vmovdqu64 zmm0, [rsi + rdx * 8]
    vpternlogd zmm0, zmm0, zmm0, 0xFF      ; Invert: 1=free, 0=allocated
    vpmovmskb k0, zmm0                      ; Extract to bitmask
    
    ; Find first run of r8 consecutive 1s
    mov     r10, FFFFFFFFFFFFFFFF h
    cmp     r8d, 64
    ja      .need_multiword
    
    ; Single-word search (up to 64 slots)
    kmovq   rax, k0
    
    ; Find first set bit run of length r8
    mov     rcx, r8
    mov     r11, rax
    
.find_run:
    test    rax, rax
    jz      .advance_scan
    
    ; Check if lowest r8 bits are all set
    mov     r10, (1 << 0)
    mov     r9d, r8d
.check_bits:
    cmp     r9d, 0
    je      .run_found
    test    rax, r10
    jz      .try_next_bit
    shl     r10, 1
    dec     r9d
    jmp     .check_bits
    
.try_next_bit:
    shr     rax, 1
    mov     r9d, r8d
    jmp     .find_run
    
.run_found:
    ; Calculate slot index: rdx * 512 + bit_position
    mov     rax, rdx
    shl     rax, 9                         ; ×512 slots per 64B chunk
    
    ; Extract bit position
    mov     rcx, r11
    sub     rcx, rax                       ; Remaining bits
    bsf     r10, rcx
    add     rax, r10
    
    ; Mark slots as allocated in bitmap
    mov     r9d, r8d
.mark_loop:
    ; Set bit in bitmap
    mov     r10, rax
    shr     r10, 3                         ; Byte offset
    mov     r11b, al
    and     r11b, 7                        ; Bit offset
    mov     r12b, 1
    shl     r12b, cl
    or      byte ptr [rsi + r10], r12b
    
    inc     rax
    dec     r9d
    jne     .mark_loop
    
    ; Update next_scan hint
    mov     [g_tensor_slot_map.next_scan], rax
    
    ; Release spinlock
    mov     dword ptr [r9], 0
    
    ; Return original slot index
    sub     rax, r8
    ret

.need_multiword:
    ; Multi-word search (>64 slots)
    ; Stub: implement if needed for >4MB allocations
    jmp     .advance_scan

.advance_scan:
    add     rdx, 512                       ; Next 64-byte chunk
    cmp     rdx, TENSOR_ARENA_SLOTS
    jb      @f
    xor     rdx, rdx                       ; Wrap around
@@:
    dec     rdi
    jnz     .scan_loop
    
    ; Allocation failed (bitmap full)
    mov     dword ptr [r9], 0              ; Release lock
    mov     rax, -1
    ret

ENDP

; ============================================================
; FREE_TENSOR_SLOT
; Input:  RCX = slot_index, RDX = slot_count
; ============================================================
ALIGN 16
free_tensor_slot PROC
    ; Acquire spinlock
    lea     r9, [g_tensor_slot_map.lock]
    mov     r10d, 1
.spin_lock_free:
    xchg    [r9], r10d
    test    r10d, r10d
    jz      .lock_acq_free
    pause
    jmp     .spin_lock_free

.lock_acq_free:
    lea     rsi, [g_tensor_slot_map.bitmap]
    mov     rax, rcx                       ; slot_index
    mov     r8d, edx                       ; slot_count
    
.clear_loop:
    ; Clear bit in bitmap
    mov     r10, rax
    shr     r10, 3
    mov     r11b, al
    and     r11b, 7
    mov     r12b, 1
    shl     r12b, cl
    not     r12b
    and     byte ptr [rsi + r10], r12b
    
    inc     rax
    dec     r8d
    jne     .clear_loop
    
    ; Release spinlock
    mov     dword ptr [r9], 0
    ret

ENDP

END
