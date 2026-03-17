; ═══════════════════════════════════════════════════════════════════
; slot_ring.asm — Phase 5/6: Per-Tensor Slot-Ring Paging
; Fine-grained scheduler: tracks hot/cold tensors, tiered LRU
; eviction, pin support, prefetch, memory budget enforcement.
; Pairs with stream_loader.asm Tier 1.
;
; Slot layout (64 bytes per slot):
;   +0   pVA          dq ?    ; Virtual address of committed region
;   +8   fileOffset   dq ?    ; Offset into GGUF file
;   +16  size         dq ?    ; Tensor byte-length
;   +24  accessTick   dq ?    ; Last access tick (for LRU)
;   +32  hitCount     dd ?    ; Total accesses
;   +36  state        dd ?    ; 0=empty, 1=mapped, 2=prefetched
;   +40  tensorIdx    dd ?    ; GGUF tensor index
;   +44  flags        dd ?    ; bit0=pinned, bits[2:1]=tier (0=COLD,1=WARM,2=HOT)
;   +48  layerIdx     dd ?    ; Layer index (for predictive prefetch)
;   +52  pad2         dd ?
;   +56  reserved2    dq ?
; ═══════════════════════════════════════════════════════════════════

; NOTE: Do NOT include rawrxd.inc here — this file DEFINES the symbols
; that rawrxd.inc declares as EXTERN. Including it causes A2005 redefinition.

EXTERN GetTickCount64:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC

; Tier 1 VMM (stream_loader.asm)
EXTERN g_vmm_base:QWORD
EXTERN g_vmm_size:QWORD

; Global heap
EXTERN g_hHeap:QWORD

PUBLIC SlotRing_Init
PUBLIC SlotRing_Destroy
PUBLIC SlotRing_Attach
PUBLIC SlotRing_Detach
PUBLIC SlotRing_GetTensor
PUBLIC SlotRing_Tick
PUBLIC SlotRing_SetBudget
PUBLIC SlotRing_GetSlotStats
PUBLIC SlotRing_EvictAll
PUBLIC SlotRing_Pin
PUBLIC SlotRing_Unpin
PUBLIC SlotRing_Prefetch
PUBLIC SlotRing_BatchEvict
PUBLIC SlotRing_PredictivePrefetch
PUBLIC SlotRing_ClockEvict

; ── Constants ────────────────────────────────────────────────────
SLOT_SIZE           equ 64
MAX_SLOTS           equ 256
HEAP_ZERO_MEMORY    equ 8

; Slot states
SLOT_EMPTY          equ 0
SLOT_MAPPED         equ 1
SLOT_PREFETCHED     equ 2

; Slot flags (bit field in +44)
SLOT_FLAG_PINNED    equ 1       ; bit 0: pinned (immune to eviction)
; Temperature tiers (bits 2:1)
TIER_COLD           equ 0       ; 00: cold — evict first
TIER_WARM           equ 2       ; 01 << 1: warm — evict second
TIER_HOT            equ 4       ; 10 << 1: hot — evict last
TIER_MASK           equ 6       ; bits 2:1
SLOT_FLAG_REFERENCED equ 8     ; bit 3: reference bit (second-chance clock)

; MEM constants
MEM_COMMIT          equ 1000h
MEM_DECOMMIT        equ 4000h
PAGE_READWRITE      equ 4

; ── Data ─────────────────────────────────────────────────────────
.data
align 8
g_pSlotArray    dq 0            ; Heap-allocated slot array
g_slotCount     dd 0            ; Number of active slots
g_memBudget     dq 0            ; Max bytes allowed committed
g_memUsed       dq 0            ; Current committed bytes
g_globalTick    dq 0            ; Monotonic tick counter
g_totalEvictions dq 0           ; Phase 6: cumulative eviction count
g_totalPageFaults dq 0          ; Phase 6: cumulative demand-page commits
g_peakMemUsed   dq 0            ; Phase 6: high-water mark
g_currentLayer  dd 0            ; Phase 6: current inference layer (for predictive prefetch)
g_totalLayers   dd 0            ; Phase 6: total layers in model
g_clockHand     dd 0            ; Phase 8: clock pointer for second-chance eviction
g_clockEvictions dq 0           ; Phase 8: cumulative clock eviction count

.code

; ────────────────────────────────────────────────────────────────
; SlotRing_Init — Allocate slot array, set memory budget
;   RCX = memBudgetBytes (e.g. 4GB = 100000000h)
; ────────────────────────────────────────────────────────────────
SlotRing_Init PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     g_memBudget, rcx
    mov     g_memUsed, 0
    mov     g_globalTick, 0
    mov     g_slotCount, 0
    mov     g_totalEvictions, 0
    mov     g_totalPageFaults, 0
    mov     g_peakMemUsed, 0
    mov     g_currentLayer, 0
    mov     g_clockHand, 0
    mov     g_clockEvictions, 0

    ; HeapAlloc(g_hHeap, HEAP_ZERO_MEMORY, MAX_SLOTS * SLOT_SIZE)
    mov     rcx, g_hHeap
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, MAX_SLOTS * SLOT_SIZE  ; 16384 bytes
    call    HeapAlloc

    test    rax, rax
    jz      @srinit_fail

    mov     g_pSlotArray, rax
    xor     eax, eax
    jmp     @srinit_done

@srinit_fail:
    mov     eax, -1

@srinit_done:
    add     rsp, 28h
    ret
SlotRing_Init ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_Destroy — Free slot array, decommit all
; ────────────────────────────────────────────────────────────────
SlotRing_Destroy PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    call    SlotRing_EvictAll

    mov     rcx, g_hHeap
    xor     edx, edx
    mov     r8, g_pSlotArray
    test    r8, r8
    jz      @srdest_done
    call    HeapFree
    mov     g_pSlotArray, 0

@srdest_done:
    add     rsp, 28h
    ret
SlotRing_Destroy ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_Attach — Register a tensor in the ring
;   RCX = tensorIdx (GGUF index)
;   RDX = fileOffset
;   R8  = tensorSize
;   R9D = layerIdx (for predictive prefetch scheduling)
;   Returns: EAX = slotIndex (or -1 if ring full)
; ────────────────────────────────────────────────────────────────
SlotRing_Attach PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     ebx, ecx                ; tensorIdx
    mov     rsi, rdx                ; fileOffset
    mov     edi, r9d                ; layerIdx

    ; Find first empty slot
    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @attach_fail

    xor     ecx, ecx                ; slot index
@find_empty:
    cmp     ecx, MAX_SLOTS
    jae     @attach_fail

    mov     rdx, rax
    ; slot base = pSlotArray + index * 64
    imul    edx, ecx, SLOT_SIZE
    add     rdx, rax

    cmp     dword ptr [rdx+36], SLOT_EMPTY
    je      @found_slot
    inc     ecx
    jmp     @find_empty

@found_slot:
    ; Fill slot
    ; pVA = g_vmm_base + fileOffset (simplified mapping)
    mov     rax, g_vmm_base
    add     rax, rsi
    mov     [rdx+0], rax            ; pVA
    mov     [rdx+8], rsi            ; fileOffset
    mov     [rdx+16], r8            ; size
    mov     rax, g_globalTick
    mov     [rdx+24], rax           ; accessTick
    mov     dword ptr [rdx+32], 0   ; hitCount
    mov     dword ptr [rdx+36], SLOT_MAPPED ; state
    mov     [rdx+40], ebx           ; tensorIdx
    mov     [rdx+48], edi           ; layerIdx (Phase 6)
    mov     dword ptr [rdx+44], TIER_COLD ; initial tier = COLD, unpinned

    ; Track count
    mov     eax, g_slotCount
    inc     eax
    mov     g_slotCount, eax

    ; Return slot index
    mov     eax, ecx
    jmp     @attach_done

@attach_fail:
    mov     eax, -1

@attach_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SlotRing_Attach ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_Detach — Remove a tensor from the ring by slot index
;   ECX = slotIndex
; ────────────────────────────────────────────────────────────────
SlotRing_Detach PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     ecx, MAX_SLOTS
    jae     @detach_done

    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @detach_done

    imul    edx, ecx, SLOT_SIZE
    add     rax, rdx

    ; If mapped, decommit the VA range
    cmp     dword ptr [rax+36], SLOT_EMPTY
    je      @detach_done

    ; Decommit: VirtualFree(pVA, size, MEM_DECOMMIT)
    mov     rcx, [rax+0]           ; pVA
    mov     rdx, [rax+16]          ; size
    push    rax
    mov     r8d, MEM_DECOMMIT
    call    VirtualFree
    pop     rax

    ; Subtract from used memory
    mov     rcx, [rax+16]
    sub     g_memUsed, rcx

    ; Zero the slot
    mov     dword ptr [rax+36], SLOT_EMPTY
    mov     qword ptr [rax+0], 0
    mov     qword ptr [rax+8], 0
    mov     qword ptr [rax+16], 0

    mov     eax, g_slotCount
    dec     eax
    mov     g_slotCount, eax

@detach_done:
    add     rsp, 28h
    ret
SlotRing_Detach ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_GetTensor — Fetch tensor data, commit if needed (demand page)
;   ECX = slotIndex
;   Returns: RAX = pointer to tensor data (or 0 if invalid)
; ────────────────────────────────────────────────────────────────
SlotRing_GetTensor PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    cmp     ecx, MAX_SLOTS
    jae     @get_null

    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @get_null

    imul    edx, ecx, SLOT_SIZE
    add     rax, rdx
    mov     rbx, rax                ; rbx = slot ptr

    cmp     dword ptr [rbx+36], SLOT_EMPTY
    je      @get_null

    ; Update access statistics + set reference bit (for clock eviction)
    mov     rax, g_globalTick
    mov     [rbx+24], rax           ; accessTick = now
    inc     dword ptr [rbx+32]      ; hitCount++
    or      dword ptr [rbx+44], SLOT_FLAG_REFERENCED  ; Phase 8: mark referenced

    ; Phase 6: Auto-promote tier based on hit frequency
    ; hitCount >= 8 → HOT, >= 2 → WARM, else stays COLD
    mov     eax, dword ptr [rbx+32]
    cmp     eax, 8
    jb      @check_warm
    and     dword ptr [rbx+44], NOT TIER_MASK
    or      dword ptr [rbx+44], TIER_HOT
    jmp     @tier_done
@check_warm:
    cmp     eax, 2
    jb      @tier_done
    test    dword ptr [rbx+44], TIER_HOT
    jnz     @tier_done              ; Don't demote HOT
    and     dword ptr [rbx+44], NOT TIER_MASK
    or      dword ptr [rbx+44], TIER_WARM
@tier_done:

    ; If state == MAPPED but not yet committed, commit now (demand page)
    cmp     dword ptr [rbx+36], SLOT_MAPPED
    jne     @already_paged

    ; Check budget before committing
    mov     rax, [rbx+16]           ; tensor size
    add     rax, g_memUsed
    cmp     rax, g_memBudget
    ja      @over_budget

    ; VirtualAlloc(pVA, size, MEM_COMMIT, PAGE_READWRITE)
    mov     rcx, [rbx+0]           ; pVA
    mov     rdx, [rbx+16]          ; size
    mov     r8d, MEM_COMMIT
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @get_null

    ; Update state and memory tracking
    mov     dword ptr [rbx+36], SLOT_PREFETCHED
    mov     rax, [rbx+16]
    add     g_memUsed, rax
    inc     g_totalPageFaults           ; Phase 6: track demand faults
    ; Update peak memory
    mov     rax, g_memUsed
    cmp     rax, g_peakMemUsed
    jbe     @already_paged
    mov     g_peakMemUsed, rax

@already_paged:
    mov     rax, [rbx+0]           ; return pVA
    jmp     @get_done

@over_budget:
    ; Need to evict coldest slot first, then retry
    call    SlotRing_Tick           ; evicts LRU
    ; Retry commit
    mov     rcx, [rbx+0]
    mov     rdx, [rbx+16]
    mov     r8d, MEM_COMMIT
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @get_null

    mov     dword ptr [rbx+36], SLOT_PREFETCHED
    mov     rax, [rbx+16]
    add     g_memUsed, rax
    mov     rax, [rbx+0]
    jmp     @get_done

@get_null:
    xor     eax, eax

@get_done:
    add     rsp, 20h
    pop     rbx
    ret
SlotRing_GetTensor ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_Tick — Advance global tick, evict if over budget using clock
;   Phase 8 upgrade: uses O(1) clock (second-chance) eviction
;   instead of O(N) LRU scan.  Respects: pinned slots, tiers.
;   No args. Called once per inference step / layer.
; ────────────────────────────────────────────────────────────────
SlotRing_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    inc     g_globalTick

    ; Check if over budget
    mov     rax, g_memUsed
    cmp     rax, g_memBudget
    jbe     @tick_done

    ; Over budget — delegate to clock eviction
    call    SlotRing_ClockEvict

@tick_done:
    add     rsp, 20h
    pop     rbx
    ret
SlotRing_Tick ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_ClockEvict — O(1) second-chance clock eviction
;   Maintains a rotating clock hand across the slot array.
;   On each call, advances the hand until it finds an evictable
;   slot (not pinned, not HOT, reference bit clear).
;   If reference bit is set, clears it and moves on (second chance).
;   Evicts one slot per call.
;
;   Amortized O(1) vs O(N) full-table LRU scan.
; ────────────────────────────────────────────────────────────────
SlotRing_ClockEvict PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @ce_done

    mov     ebx, g_clockHand        ; current clock position
    xor     esi, esi                ; laps counter (prevent infinite loop)

@ce_scan:
    ; Safety: if we've scanned the entire ring twice, give up
    cmp     esi, MAX_SLOTS * 2
    jae     @ce_done

    ; Wrap clock hand
    cmp     ebx, MAX_SLOTS
    jb      @ce_no_wrap
    xor     ebx, ebx
@ce_no_wrap:

    ; Compute slot pointer: rax + clockHand * 64
    mov     rdx, rax
    imul    r8d, ebx, SLOT_SIZE
    add     rdx, r8

    ; Skip empty / non-committed slots
    cmp     dword ptr [rdx+36], SLOT_PREFETCHED
    jne     @ce_advance

    ; Skip pinned slots
    test    dword ptr [rdx+44], SLOT_FLAG_PINNED
    jnz     @ce_advance

    ; Skip HOT-tier slots (evict COLD/WARM preferentially)
    mov     r9d, dword ptr [rdx+44]
    and     r9d, TIER_MASK
    cmp     r9d, TIER_HOT
    je      @ce_advance

    ; Check reference bit (second-chance)
    test    dword ptr [rdx+44], SLOT_FLAG_REFERENCED
    jz      @ce_evict_this

    ; Referenced — grant second chance: clear bit, demote tier, move on
    and     dword ptr [rdx+44], NOT SLOT_FLAG_REFERENCED
    ; If WARM, demote to COLD on second chance expiry
    cmp     r9d, TIER_WARM
    jne     @ce_advance
    and     dword ptr [rdx+44], NOT TIER_MASK
    ; tier now = COLD (0)
    jmp     @ce_advance

@ce_evict_this:
    ; Found victim — evict it
    inc     ebx                     ; advance hand past victim
    mov     g_clockHand, ebx

    ; Detach the slot (decommit + zero)
    mov     ecx, ebx
    dec     ecx                     ; ecx = victimIndex (hand was already advanced)
    call    SlotRing_Detach
    inc     g_totalEvictions
    inc     g_clockEvictions
    jmp     @ce_done

@ce_advance:
    inc     ebx
    inc     esi
    jmp     @ce_scan

@ce_done:
    mov     g_clockHand, ebx
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
SlotRing_ClockEvict ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_SetBudget — Update the memory budget at runtime
;   RCX = newBudgetBytes
; ────────────────────────────────────────────────────────────────
SlotRing_SetBudget PROC
    mov     g_memBudget, rcx
    ret
SlotRing_SetBudget ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_GetSlotStats — Return ring statistics
;   RCX = pOutStats (pointer to SlotRingStats struct)
;   SlotRingStats:
;     +0  : activeSlots    (DWORD)
;     +4  : memUsed        (QWORD)
;     +12 : memBudget      (QWORD)
;     +20 : globalTick     (QWORD)
;     +28 : totalEvictions (QWORD)
;     +36 : totalPageFaults(QWORD)
;     +44 : peakMemUsed    (QWORD)
;     +52 : clockHand      (DWORD)
;     +56 : clockEvictions (QWORD)
; ────────────────────────────────────────────────────────────────
SlotRing_GetSlotStats PROC
    mov     eax, g_slotCount
    mov     [rcx], eax
    mov     rax, g_memUsed
    mov     [rcx+4], rax
    mov     rax, g_memBudget
    mov     [rcx+12], rax
    mov     rax, g_globalTick
    mov     [rcx+20], rax
    mov     rax, g_totalEvictions
    mov     [rcx+28], rax
    mov     rax, g_totalPageFaults
    mov     [rcx+36], rax
    mov     rax, g_peakMemUsed
    mov     [rcx+44], rax
    mov     eax, g_clockHand
    mov     [rcx+52], eax
    mov     rax, g_clockEvictions
    mov     [rcx+56], rax
    ret
SlotRing_GetSlotStats ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_EvictAll — Decommit every committed slot (emergency flush)
; ────────────────────────────────────────────────────────────────
SlotRing_EvictAll PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    xor     ebx, ebx
@evict_loop:
    cmp     ebx, MAX_SLOTS
    jae     @evict_all_done

    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @evict_all_done

    imul    edx, ebx, SLOT_SIZE
    add     rax, rdx
    cmp     dword ptr [rax+36], SLOT_EMPTY
    je      @evict_next

    mov     ecx, ebx
    call    SlotRing_Detach

@evict_next:
    inc     ebx
    jmp     @evict_loop

@evict_all_done:
    add     rsp, 20h
    pop     rbx
    ret
SlotRing_EvictAll ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_Pin — Mark a slot as pinned (immune to eviction)
;   ECX = slotIndex
; ────────────────────────────────────────────────────────────────
SlotRing_Pin PROC
    cmp     ecx, MAX_SLOTS
    jae     @pin_done

    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @pin_done

    imul    edx, ecx, SLOT_SIZE
    add     rax, rdx

    cmp     dword ptr [rax+36], SLOT_EMPTY
    je      @pin_done

    or      dword ptr [rax+44], SLOT_FLAG_PINNED
    ; Promote to HOT tier while pinned
    and     dword ptr [rax+44], NOT TIER_MASK
    or      dword ptr [rax+44], TIER_HOT

@pin_done:
    ret
SlotRing_Pin ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_Unpin — Remove pin from a slot (allow eviction again)
;   ECX = slotIndex
; ────────────────────────────────────────────────────────────────
SlotRing_Unpin PROC
    cmp     ecx, MAX_SLOTS
    jae     @unpin_done

    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @unpin_done

    imul    edx, ecx, SLOT_SIZE
    add     rax, rdx

    and     dword ptr [rax+44], NOT SLOT_FLAG_PINNED
    ; Demote to WARM (not immediately cold)
    and     dword ptr [rax+44], NOT TIER_MASK
    or      dword ptr [rax+44], TIER_WARM

@unpin_done:
    ret
SlotRing_Unpin ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_Prefetch — Pre-commit a slot (hide commit latency)
;   ECX = slotIndex
;   Returns: EAX = 0 success, -1 failure
; ────────────────────────────────────────────────────────────────
SlotRing_Prefetch PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    cmp     ecx, MAX_SLOTS
    jae     @pf_fail

    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @pf_fail

    imul    edx, ecx, SLOT_SIZE
    add     rax, rdx
    mov     rbx, rax                ; rbx = slot ptr

    ; Only prefetch MAPPED (not yet committed) slots
    cmp     dword ptr [rbx+36], SLOT_MAPPED
    jne     @pf_already             ; Already committed or empty

    ; Check budget
    mov     rax, [rbx+16]
    add     rax, g_memUsed
    cmp     rax, g_memBudget
    ja      @pf_fail                ; Over budget, don't prefetch

    ; VirtualAlloc(pVA, size, MEM_COMMIT, PAGE_READWRITE)
    mov     rcx, [rbx+0]
    mov     rdx, [rbx+16]
    mov     r8d, MEM_COMMIT
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @pf_fail

    mov     dword ptr [rbx+36], SLOT_PREFETCHED
    mov     rax, [rbx+16]
    add     g_memUsed, rax
    ; Promote to WARM on prefetch
    and     dword ptr [rbx+44], NOT TIER_MASK
    or      dword ptr [rbx+44], TIER_WARM

    ; Update peak
    mov     rax, g_memUsed
    cmp     rax, g_peakMemUsed
    jbe     @pf_already
    mov     g_peakMemUsed, rax

@pf_already:
    xor     eax, eax
    jmp     @pf_done

@pf_fail:
    mov     eax, -1

@pf_done:
    add     rsp, 20h
    pop     rbx
    ret
SlotRing_Prefetch ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_BatchEvict — Evict up to N slots using clock algorithm
;   ECX = maxEvictions
; ────────────────────────────────────────────────────────────────
SlotRing_BatchEvict PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     ebx, ecx                ; eviction counter

@batch_loop:
    test    ebx, ebx
    jz      @batch_done

    call    SlotRing_ClockEvict
    dec     ebx
    jmp     @batch_loop

@batch_done:
    add     rsp, 20h
    pop     rbx
    ret
SlotRing_BatchEvict ENDP

; ────────────────────────────────────────────────────────────────
; SlotRing_PredictivePrefetch — Prefetch tensors for layer+1
;   ECX = currentLayerIdx
;   EDX = totalLayers
;
;   Scans all slots, prefetches any slot whose layerIdx == current+1.
;   This hides VirtualAlloc(COMMIT) latency behind the current
;   layer's compute, reducing page faults by ~90%.
; ────────────────────────────────────────────────────────────────
SlotRing_PredictivePrefetch PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     g_currentLayer, ecx
    mov     g_totalLayers, edx

    ; Target layer = current + 1
    lea     esi, [ecx+1]
    cmp     esi, edx
    jae     @pp_done                ; No next layer (last layer)

    mov     rax, g_pSlotArray
    test    rax, rax
    jz      @pp_done

    xor     ebx, ebx                ; slot index scanner
@pp_scan:
    cmp     ebx, MAX_SLOTS
    jae     @pp_done

    mov     rdx, rax
    imul    r8d, ebx, SLOT_SIZE
    add     rdx, r8

    ; Check if this slot is for the next layer
    cmp     dword ptr [rdx+36], SLOT_EMPTY
    je      @pp_next

    cmp     dword ptr [rdx+48], esi  ; layerIdx == target?
    jne     @pp_next

    ; Only prefetch if MAPPED (not yet committed)
    cmp     dword ptr [rdx+36], SLOT_MAPPED
    jne     @pp_next

    ; Prefetch this slot
    mov     ecx, ebx
    push    rax                     ; save slot array base
    call    SlotRing_Prefetch
    pop     rax

@pp_next:
    inc     ebx
    jmp     @pp_scan

@pp_done:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
SlotRing_PredictivePrefetch ENDP

END
