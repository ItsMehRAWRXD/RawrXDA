; ============================================================
; SDMA RING / TENSOR SLOT ALLOCATOR (PHASE 1)
; Manages 256MB BAR ring buffer + tensor slot bitmap
; ============================================================


; ============================================================
; Type definitions (must be before .code to avoid block nesting errors)
; ============================================================
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
    spin_latch          DD ?                ; Spinlock
    _pad                DD ?
TENSOR_SLOT_MAP ENDS



; Struct for ring allocator state (type definition only, no storage)
SDMA_RING_STATE STRUCT
    entries_allocated   DQ ?
    total_bytes         DQ ?
    ring_overflow_count DQ ?
SDMA_RING_STATE ENDS

; ============================================================
; Data segment for BSS allocations
; ============================================================
.data?
ALIGN 16
g_tensor_slot_map TENSOR_SLOT_MAP <>
ALIGN 16
g_sdma_ring_allocator SDMA_RING_STATE <>

.code

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
sdma_ring_advance_head ENDP

; ============================================================
; ALLOCATE_TENSOR_SLOT
; Input:  RCX = size_bytes
; Output: RAX = slot_index (or -1 if allocation failed)
; Clobbers: rdx, r8, r9, r10, r11, zmm0, k0
; ============================================================
ALIGN 16
allocate_tensor_slot PROC
    ; Convert size to slot count (minimum 1)
    add     rcx, TENSOR_SLOT_GRANULARITY - 1
    shr     rcx, 16
    test    rcx, rcx
    jnz     alloc_slots_ready
    mov     rcx, 1
alloc_slots_ready:
    mov     r8, rcx                        ; r8 = slots needed

    ; Acquire spin lock
    lea     r11, [g_tensor_slot_map.spin_latch]
    mov     r10d, 1
alloc_lock_spin:
    xchg    dword ptr [r11], r10d
    test    r10d, r10d
    jz      alloc_lock_acquired
    pause
    mov     r10d, 1
    jmp     alloc_lock_spin

alloc_lock_acquired:
    lea     r10, [g_tensor_slot_map.bitmap] ; bitmap base
    mov     rdx, [g_tensor_slot_map.next_scan]
    cmp     rdx, TENSOR_ARENA_SLOTS
    jb      alloc_scan_start
    xor     rdx, rdx

alloc_scan_start:
    mov     r9, TENSOR_ARENA_SLOTS          ; max candidates

alloc_candidate_loop:
    mov     rax, rdx                        ; candidate start slot
    mov     rcx, r8                         ; remaining slots to verify

alloc_verify_loop:
    cmp     rax, TENSOR_ARENA_SLOTS
    jae     alloc_candidate_fail

    mov     rdx, rax
    shr     rdx, 6                          ; qword index
    mov     r8, rax
    and     r8, 63                          ; bit index within qword
    bt      qword ptr [r10 + rdx*8], r8
    jc      alloc_candidate_fail

    inc     rax
    dec     rcx
    jnz     alloc_verify_loop

    ; Mark the verified run as allocated
    mov     rax, rdx
    mov     rcx, r8
alloc_mark_loop:
    mov     rdx, rax
    shr     rdx, 6
    mov     r8, rax
    and     r8, 63
    bts     qword ptr [r10 + rdx*8], r8
    inc     rax
    dec     rcx
    jnz     alloc_mark_loop

    ; Save next scan and release lock
    mov     [g_tensor_slot_map.next_scan], rax
    mov     dword ptr [r11], 0
    mov     rax, rdx                        ; return start slot index
    ret

alloc_candidate_fail:
    inc     rdx
    cmp     rdx, TENSOR_ARENA_SLOTS
    jb      alloc_candidate_advance_done
    xor     rdx, rdx
alloc_candidate_advance_done:
    dec     r9
    jnz     alloc_candidate_loop

    ; Allocation failed
    mov     dword ptr [r11], 0
    mov     rax, -1
    ret

allocate_tensor_slot ENDP

; ============================================================
; FREE_TENSOR_SLOT
; Input:  RCX = slot_index, RDX = slot_count
; ============================================================
ALIGN 16
free_tensor_slot PROC
    ; Acquire spinlock
    lea     r9, [g_tensor_slot_map.spin_latch]
    mov     r10d, 1
free_spin_lock:
    xchg    [r9], r10d
    test    r10d, r10d
    jz      free_lock_acq
    pause
    jmp     free_spin_lock

free_lock_acq:
    lea     r10, [g_tensor_slot_map.bitmap]
    mov     rax, rcx                       ; slot_index
    mov     ecx, edx                       ; slot_count
    
free_clear_loop:
    ; Clear bit in bitmap
    mov     rdx, rax
    shr     rdx, 6
    mov     r8, rax
    and     r8, 63
    btr     qword ptr [r10 + rdx*8], r8
    
    inc     rax
    dec     ecx
    jne     free_clear_loop
    
    ; Release spinlock
    mov     dword ptr [r9], 0
    ret

free_tensor_slot ENDP

END
