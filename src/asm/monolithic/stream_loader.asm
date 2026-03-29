; ═══════════════════════════════════════════════════════════════════
; stream_loader.asm — Phase 5: Persistent Weight Paging (VMM)
; Implements demand-paged GGUF tensor loading for large models.
; ═══════════════════════════════════════════════════════════════════

; NOTE: Do NOT include rawrxd.inc here — it declares our own symbols
;       as EXTERN which conflicts with PUBLIC below.

; External Win32 API
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN MapViewOfFileEx:PROC
EXTERN UnmapViewOfFile:PROC

; External Globals
EXTERN g_modelbase:QWORD

PUBLIC StreamLoaderInit
PUBLIC StreamMapModel
PUBLIC StreamUnmap
PUBLIC StreamPrefetch
PUBLIC StreamEvictCold
PUBLIC StreamGetStats
PUBLIC g_vmm_base
PUBLIC g_vmm_size

.data
align 8
g_vmm_base      dq 0        ; Base address of reserved VMM space
g_vmm_size      dq 0        ; Total size of reserved space
g_page_mask     dq 0FFFFFFFFFFFFF000h ; 4KB page alignment mask
g_committed_pages dd 0      ; Count of committed 4KB pages
g_evicted_pages  dd 0       ; Count of evicted/decommitted 4KB pages
g_mapped_regions    dq 0    ; Total mapped regions
g_total_bytes_mapped dq 0   ; Total bytes mapped across all regions
g_page_faults       dq 0    ; Demand-page fault events serviced
g_prefetch_requests dq 0    ; Prefetch requests issued
g_prefetch_hits     dq 0    ; Prefetched pages actually accessed
g_avg_fault_latency dd 0    ; Average fault latency (microseconds)
g_peak_fault_latency dd 0   ; Peak fault latency (microseconds)
g_eviction_policy   dd 0    ; 0=LRU, 1=CLOCK, 2=LFU
g_working_set_size  dq 0    ; Current working set in bytes
g_high_water_mark   dq 0    ; Peak working set in bytes
g_tensor_slots_active dd 0  ; Tensor slots currently active
g_tensor_slots_total  dd 0  ; Total tensor slots available

.data?
align 16
g_slot_map      db 4096 dup(?) ; Status of each 2MB model chunk (0=empty, 1=mapped)

.code

; ────────────────────────────────────────────────────────────────
; StreamLoaderInit — Reserve virtual address space for the model
;   RCX = ReserveSize (e.g., 64GB for Titan/70B models)
; ────────────────────────────────────────────────────────────────
StreamLoaderInit PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     g_vmm_size, rcx
    
    ; VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS)
    xor     rcx, rcx                ; lpAddress
    mov     rdx, g_vmm_size         ; dwSize
    mov     r8d, 2000h              ; MEM_RESERVE
    mov     r9d, 01h                ; PAGE_NOACCESS
    call    VirtualAlloc
    
    test    rax, rax
    jz      @vmm_fail
    
    mov     g_vmm_base, rax
    mov     g_modelbase, rax        ; Set global model base to VMM start
    xor     eax, eax
    jmp     @vmm_done

@vmm_fail:
    mov     eax, -1

@vmm_done:
    add     rsp, 28h
    ret
StreamLoaderInit ENDP

; ────────────────────────────────────────────────────────────────
; StreamMapModel — Map GGUF file into the reserved VMM space
;   RCX = hFileMapping
;   RDX = FileOffset
;   R8  = ViewSize
;   R9  = TargetVA (must be inside VMM range)
; ────────────────────────────────────────────────────────────────
StreamMapModel PROC FRAME
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    ; MapViewOfFileEx(hFileMapping, FILE_MAP_READ, offset_hi, offset_lo, size, TargetVA)
    ; RCX already has hFileMapping
    mov     rax, r9                 ; TargetVA
    mov     [rsp+28h], rax          ; 6th argument: lpBaseAddress
    
    mov     rax, r8                 ; ViewSize
    mov     [rsp+20h], rax          ; 5th argument: dwNumberOfBytesToMap
    
    mov     r8, rdx                 ; offset
    shr     r8, 32                  ; offset_hi (R8)
    mov     r9, rdx
    and     r9, 0FFFFFFFFh          ; offset_lo (R9)
    
    mov     edx, 4                  ; FILE_MAP_READ
    call    MapViewOfFileEx

    test    rax, rax
    jz      @map_fail
    xor     eax, eax
    jmp     @map_done

@map_fail:
    mov     eax, -1

@map_done:
    add     rsp, 38h
    ret
StreamMapModel ENDP

; ────────────────────────────────────────────────────────────────
; StreamUnmap — Unmap a view from the VMM space (evict a chunk)
;   RCX = lpBaseAddress (start of the view to unmap)
; ────────────────────────────────────────────────────────────────
StreamUnmap PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; UnmapViewOfFile(lpBaseAddress)
    call    UnmapViewOfFile

    add     rsp, 28h
    ret
StreamUnmap ENDP

; ────────────────────────────────────────────────────────────────
; StreamPrefetch — Commit and pre-fault a range of pages
;   RCX = StartAddress (within VMM range)
;   RDX = Size (bytes to prefetch/commit)
; ────────────────────────────────────────────────────────────────
StreamPrefetch PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; VirtualAlloc(StartAddress, Size, MEM_COMMIT, PAGE_READWRITE)
    mov     r8d, 1000h              ; MEM_COMMIT
    mov     r9d, 04h                ; PAGE_READWRITE
    call    VirtualAlloc

    test    rax, rax
    jz      @prefetch_fail

    ; Pre-fault by touching first byte of each 4KB page
    mov     rbx, rax                ; committed base
    mov     rcx, rdx                ; size
    shr     rcx, 12                 ; number of 4KB pages
    test    rcx, rcx
    jz      @prefetch_ok

@fault_loop:
    mov     al, byte ptr [rbx]       ; Touch page (force page-in)
    add     rbx, 1000h               ; Next 4KB page
    loop    @fault_loop

@prefetch_ok:
    ; Track committed pages: size / 4KB
    shr     edx, 12
    lock add g_committed_pages, edx
    xor     eax, eax
    jmp     @prefetch_done

@prefetch_fail:
    mov     eax, -1

@prefetch_done:
    add     rsp, 20h
    pop     rbx
    ret
StreamPrefetch ENDP

; ────────────────────────────────────────────────────────────────
; StreamEvictCold — Decommit pages that haven't been accessed
;   RCX = StartAddress
;   RDX = Size (bytes to decommit)
; ────────────────────────────────────────────────────────────────
StreamEvictCold PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Save size for page count tracking
    mov     rax, rdx
    shr     eax, 12                 ; pages = size / 4KB
    mov     [rsp + 20h], eax        ; save page count

    ; VirtualFree(StartAddress, Size, MEM_DECOMMIT)
    mov     r8d, 4000h              ; MEM_DECOMMIT
    call    VirtualFree

    test    rax, rax
    jz      @evict_fail

    ; Track evicted pages
    mov     eax, [rsp + 20h]
    lock add g_evicted_pages, eax
    xor     eax, eax
    jmp     @evict_done

@evict_fail:
    mov     eax, -1

@evict_done:
    add     rsp, 28h
    ret
StreamEvictCold ENDP

; ────────────────────────────────────────────────────────────────
; StreamGetStats — Collect demand-paged VMM statistics
;   RCX = pOutStats (pointer to buffer, >= 92 bytes)
;   Returns EAX = 92 (bytes written) or 0 if RCX is NULL
;
;   StreamStats layout:
;     +0   QWORD  Total mapped regions
;     +8   QWORD  Total bytes mapped
;     +16  QWORD  Page faults serviced
;     +24  DWORD  Pages currently committed
;     +28  DWORD  Pages evicted
;     +32  QWORD  Prefetch requests issued
;     +40  QWORD  Prefetch hits
;     +48  DWORD  Average fault latency (us)
;     +52  DWORD  Peak fault latency (us)
;     +56  DWORD  Memory pressure (0-100 %)
;     +60  DWORD  Eviction policy (0=LRU,1=CLOCK,2=LFU)
;     +64  QWORD  Working set size (bytes)
;     +72  QWORD  High water mark (bytes)
;     +80  DWORD  Tensor slots active
;     +84  DWORD  Tensor slots total
;     +88  DWORD  Cache line utilization (%)
;     Total: 92 bytes
; ────────────────────────────────────────────────────────────────
StreamGetStats PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; ── NULL guard ──────────────────────────────────────────
    test    rcx, rcx
    jz      @stats_null

    mov     rsi, rcx                        ; RSI = output buffer

    ; +0: Total mapped regions (QWORD)
    mov     rax, g_mapped_regions
    mov     [rsi], rax

    ; +8: Total bytes mapped (QWORD)
    mov     rax, g_total_bytes_mapped
    mov     [rsi+08h], rax

    ; +16: Page faults serviced (QWORD)
    mov     rax, g_page_faults
    mov     [rsi+10h], rax

    ; +24: Pages currently committed (DWORD)
    mov     eax, g_committed_pages
    mov     dword ptr [rsi+18h], eax

    ; +28: Pages evicted (DWORD)
    mov     eax, g_evicted_pages
    mov     dword ptr [rsi+1Ch], eax

    ; +32: Prefetch requests issued (QWORD)
    mov     rax, g_prefetch_requests
    mov     [rsi+20h], rax

    ; +40: Prefetch hits (QWORD)
    mov     rax, g_prefetch_hits
    mov     [rsi+28h], rax

    ; +48: Average fault latency (DWORD, microseconds)
    mov     eax, g_avg_fault_latency
    mov     dword ptr [rsi+30h], eax

    ; +52: Peak fault latency (DWORD, microseconds)
    mov     eax, g_peak_fault_latency
    mov     dword ptr [rsi+34h], eax

    ; +56: Memory pressure = (committed * PAGE_SIZE * 100) / mapped_total
    mov     eax, g_committed_pages
    imul    rax, rax, 1000h                 ; * 4096 (PAGE_SIZE)
    imul    rax, rax, 64h                   ; * 100
    mov     rbx, g_total_bytes_mapped
    test    rbx, rbx
    jz      @pressure_zero
    xor     edx, edx
    div     rbx                             ; RAX = pressure 0-100+
    cmp     eax, 64h
    jbe     @pressure_store
    mov     eax, 64h                        ; clamp to 100
    jmp     @pressure_store
@pressure_zero:
    xor     eax, eax
@pressure_store:
    mov     dword ptr [rsi+38h], eax

    ; +60: Eviction policy (DWORD)
    mov     eax, g_eviction_policy
    mov     dword ptr [rsi+3Ch], eax

    ; +64: Working set size (QWORD)
    mov     rax, g_working_set_size
    mov     [rsi+40h], rax

    ; +72: High water mark (QWORD)
    mov     rax, g_high_water_mark
    mov     [rsi+48h], rax

    ; +80: Tensor slots active (DWORD)
    mov     eax, g_tensor_slots_active
    mov     dword ptr [rsi+50h], eax

    ; +84: Tensor slots total (DWORD)
    mov     eax, g_tensor_slots_total
    mov     dword ptr [rsi+54h], eax

    ; +88: Cache utilization = (active_slots * 100) / total_slots
    mov     eax, g_tensor_slots_active
    imul    eax, eax, 64h                   ; * 100
    mov     ebx, g_tensor_slots_total
    test    ebx, ebx
    jz      @cache_zero
    xor     edx, edx
    div     ebx                             ; EAX = utilization %
    jmp     @cache_store
@cache_zero:
    xor     eax, eax
@cache_store:
    mov     dword ptr [rsi+58h], eax

    ; Return bytes written = 92 (5Ch)
    mov     eax, 5Ch
    jmp     @stats_done

@stats_null:
    xor     eax, eax

@stats_done:
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
StreamGetStats ENDP

END
