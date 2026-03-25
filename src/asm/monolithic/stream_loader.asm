; ═══════════════════════════════════════════════════════════════════
<<<<<<< HEAD
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
=======
; RawrXD Stream Loader — Demand-Paged GGUF Memory Virtualization
; P0: Load 200B+ models on 64GB physical via SEC_RESERVE + WriteCopy
;
; Architecture:
;   1. CreateFileMapping with SEC_RESERVE → reserves VA, no physical pages
;   2. MapViewOfFile with FILE_MAP_COPY → WriteCopy semantics
;   3. VEH handler catches EXCEPTION_ACCESS_VIOLATION on cold pages
;   4. VirtualLock commits touched pages; VirtualUnlock evicts cold ones
;   5. LRU eviction ring keeps hot tensor pages resident
;
; Exports: StreamLoaderInit, StreamMapModel, StreamUnmap,
;          StreamPrefetch, StreamEvictCold, StreamGetStats
; ═══════════════════════════════════════════════════════════════════

EXTERN CreateFileW:PROC
EXTERN CreateFileMappingW:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN GetFileSizeEx:PROC
EXTERN VirtualLock:PROC
EXTERN VirtualUnlock:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN AddVectoredExceptionHandler:PROC
EXTERN RemoveVectoredExceptionHandler:PROC
EXTERN GetSystemInfo:PROC
EXTERN BeaconSend:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
>>>>>>> origin/main

PUBLIC StreamLoaderInit
PUBLIC StreamMapModel
PUBLIC StreamUnmap
PUBLIC StreamPrefetch
PUBLIC StreamEvictCold
PUBLIC StreamGetStats
<<<<<<< HEAD
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
=======

; ── Constants ────────────────────────────────────────────────────
GENERIC_READ         equ 80000000h
FILE_SHARE_READ      equ 1
OPEN_EXISTING        equ 3
PAGE_READONLY        equ 02h
PAGE_WRITECOPY       equ 08h
SEC_RESERVE          equ 04000000h
SEC_COMMIT           equ 08000000h
FILE_MAP_READ        equ 04h
FILE_MAP_COPY        equ 01h
MEM_COMMIT           equ 1000h
MEM_RESERVE          equ 2000h
MEM_RELEASE          equ 8000h
PAGE_READWRITE       equ 04h
GGUF_MAGIC           equ 046554747h

; VEH constants
EXCEPTION_ACCESS_VIOLATION equ 0C0000005h
EXCEPTION_CONTINUE_SEARCH  equ 0
EXCEPTION_CONTINUE_EXECUTION equ -1

; Streaming config
STREAM_PAGE_SIZE     equ 10000h          ; 64KB granularity (aligns to large pages)
STREAM_PREFETCH_AHEAD equ 4              ; prefetch 4 chunks ahead of cursor
LRU_RING_ENTRIES     equ 4096            ; track 4096 most-recently-used pages
STREAM_BEACON_SLOT   equ 5              ; beacon slot for streaming events

; ── Data ─────────────────────────────────────────────────────────
.data?
align 16
g_streamFile     dq ?                    ; file handle
g_streamMapping  dq ?                    ; file mapping handle
g_streamBase     dq ?                    ; base of mapped view
g_streamSize     dq ?                    ; total file size in bytes
g_streamPageSize dq ?                    ; system page size (from SYSTEM_INFO)
g_vehHandle      dq ?                    ; VEH handler cookie

; LRU eviction ring — circular buffer of page addresses
g_lruRing        dq LRU_RING_ENTRIES dup(?)
g_lruHead        dd ?                    ; write cursor
g_lruTail        dd ?                    ; eviction cursor

; Statistics
g_pageFaults     dq ?                    ; total page faults handled
g_pagesEvicted   dq ?                    ; total pages evicted
g_pagesResident  dq ?                    ; currently resident pages
g_prefetchHits   dq ?                    ; prefetch prevented fault

; SYSTEM_INFO buffer (48 bytes on x64)
align 8
g_sysInfo        db 48 dup(?)

.data
align 8
g_streamReady    dd 0                    ; 1 = streaming model loaded
g_evictionThresh dq 0                    ; evict when resident > this many pages

.code
; ════════════════════════════════════════════════════════════════
; StreamLoaderInit — one-time initialization
;   Queries system page size, installs VEH, zeros stats.
;   Returns: EAX = 0
; ════════════════════════════════════════════════════════════════
StreamLoaderInit PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Get system page granularity
    lea     rcx, g_sysInfo
    call    GetSystemInfo
    lea     rax, g_sysInfo
    mov     eax, dword ptr [rax + 24]   ; dwAllocationGranularity (offset 24)
    mov     g_streamPageSize, rax

    ; Install Vectored Exception Handler (first = 1 → called before SEH)
    mov     ecx, 1
    lea     rdx, StreamVEHandler
    call    AddVectoredExceptionHandler
    mov     g_vehHandle, rax

    ; Zero statistics
    xor     eax, eax
    mov     g_pageFaults, rax
    mov     g_pagesEvicted, rax
    mov     g_pagesResident, rax
    mov     g_prefetchHits, rax
    mov     g_lruHead, eax
    mov     g_lruTail, eax

    ; Default eviction threshold: 512K pages (32GB at 64KB granularity)
    mov     rax, 80000h
    mov     g_evictionThresh, rax

    mov     g_streamReady, 0

    add     rsp, 30h
    pop     rbx
    xor     eax, eax
    ret
StreamLoaderInit ENDP

; ════════════════════════════════════════════════════════════════
; StreamMapModel — map a GGUF file with demand paging
;   RCX = path (LPCWSTR)
;   Returns: RAX = base address (NULL on failure)
;
; Strategy:
;   CreateFileW → GetFileSizeEx → CreateFileMappingW(PAGE_READONLY)
;   → MapViewOfFile(FILE_MAP_READ) with full VA reservation
;   Pages are physically backed on-demand by Windows VMM.
;   The VEH handler records page faults for LRU tracking.
; ════════════════════════════════════════════════════════════════
StreamMapModel PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     rbx, rcx                    ; rbx = path

    ; Unmap any previous streaming model
    call    StreamUnmap

    ; CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)
    mov     rcx, rbx
    mov     edx, GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    mov     dword ptr [rsp+20h], OPEN_EXISTING
    mov     dword ptr [rsp+28h], 0
    mov     qword ptr [rsp+30h], 0
    call    CreateFileW
    cmp     rax, -1
    je      @smap_fail
    mov     g_streamFile, rax
    mov     rsi, rax                    ; rsi = hFile

    ; GetFileSizeEx(hFile, &fileSize)
    mov     rcx, rsi
    lea     rdx, g_streamSize
    call    GetFileSizeEx

    ; Validate GGUF magic — read first 4 bytes via mapping
    ; CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)
    mov     rcx, rsi
    xor     edx, edx
    mov     r8d, PAGE_READONLY
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    CreateFileMappingW
    test    rax, rax
    jz      @smap_close_file
    mov     g_streamMapping, rax
    mov     rdi, rax                    ; rdi = hMapping

    ; MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0)
    ; Size=0 → maps entire file; pages loaded on demand by VMM
    mov     rcx, rdi
    mov     edx, FILE_MAP_READ
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], 0
    call    MapViewOfFile
    test    rax, rax
    jz      @smap_close_mapping
    mov     g_streamBase, rax

    ; Verify GGUF magic (first dword)
    mov     ecx, dword ptr [rax]
    cmp     ecx, GGUF_MAGIC
    jne     @smap_unmap

    ; Calculate page count for stats
    mov     rax, g_streamSize
    mov     rcx, g_streamPageSize
    xor     edx, edx
    div     rcx
    test    rdx, rdx
    jz      @smap_no_rem
    inc     rax
@smap_no_rem:
    ; rax = total pages in file (virtual, not yet resident)

    mov     g_streamReady, 1

    ; Prefetch first chunk (GGUF header + metadata tensors)
    mov     rcx, g_streamBase
    mov     rdx, g_streamPageSize
    call    StreamPrefetchRange

    mov     rax, g_streamBase
    jmp     @smap_ret

@smap_unmap:
    mov     rcx, g_streamBase
    call    UnmapViewOfFile
    xor     eax, eax
    mov     g_streamBase, rax
@smap_close_mapping:
    mov     rcx, g_streamMapping
    call    CloseHandle
    xor     eax, eax
    mov     g_streamMapping, rax
@smap_close_file:
    mov     rcx, g_streamFile
    call    CloseHandle
    mov     g_streamFile, -1
@smap_fail:
    xor     eax, eax
@smap_ret:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
StreamMapModel ENDP

; ════════════════════════════════════════════════════════════════
; StreamUnmap — release current streaming model
; ════════════════════════════════════════════════════════════════
>>>>>>> origin/main
StreamUnmap PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

<<<<<<< HEAD
    ; UnmapViewOfFile(lpBaseAddress)
    call    UnmapViewOfFile
=======
    mov     g_streamReady, 0

    ; Unmap view
    mov     rcx, g_streamBase
    test    rcx, rcx
    jz      @su_no_view
    call    UnmapViewOfFile
    xor     eax, eax
    mov     g_streamBase, rax
@su_no_view:

    ; Close mapping
    mov     rcx, g_streamMapping
    test    rcx, rcx
    jz      @su_no_mapping
    call    CloseHandle
    xor     eax, eax
    mov     g_streamMapping, rax
@su_no_mapping:

    ; Close file
    mov     rcx, g_streamFile
    cmp     rcx, -1
    je      @su_no_file
    test    rcx, rcx
    jz      @su_no_file
    call    CloseHandle
    mov     g_streamFile, -1
@su_no_file:

    ; Reset LRU ring
    mov     g_lruHead, 0
    mov     g_lruTail, 0
    mov     g_pagesResident, 0
>>>>>>> origin/main

    add     rsp, 28h
    ret
StreamUnmap ENDP

<<<<<<< HEAD
; ────────────────────────────────────────────────────────────────
; StreamPrefetch — Commit and pre-fault a range of pages
;   RCX = StartAddress (within VMM range)
;   RDX = Size (bytes to prefetch/commit)
; ────────────────────────────────────────────────────────────────
StreamPrefetch PROC FRAME
    push    rbx
    .pushreg rbx
=======
; ════════════════════════════════════════════════════════════════
; StreamPrefetch — lock N pages ahead of a given offset
;   RCX = target offset into model (byte offset from g_streamBase)
;   Locks STREAM_PREFETCH_AHEAD pages starting at offset,
;   records them in LRU ring.
; ════════════════════════════════════════════════════════════════
StreamPrefetch PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
>>>>>>> origin/main
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

<<<<<<< HEAD
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
=======
    cmp     g_streamReady, 0
    je      @pf_done

    mov     rsi, rcx                    ; rsi = offset
    mov     rdi, g_streamBase
    add     rdi, rsi                    ; rdi = base + offset

    ; Align down to page boundary
    mov     rax, g_streamPageSize
    dec     rax
    not     rax
    and     rdi, rax                    ; page-aligned address

    mov     ebx, STREAM_PREFETCH_AHEAD
@pf_loop:
    test    ebx, ebx
    jz      @pf_done

    ; Bounds check
    mov     rax, g_streamBase
    add     rax, g_streamSize
    cmp     rdi, rax
    jae     @pf_done

    ; VirtualLock(addr, pageSize) — force page into working set
    mov     rcx, rdi
    mov     rdx, g_streamPageSize
    call    VirtualLock
    test    eax, eax
    jz      @pf_next                   ; failed (quota), skip

    ; Record in LRU ring
    mov     rcx, rdi
    call    LRU_Record

    lock inc qword ptr g_prefetchHits
    lock inc qword ptr g_pagesResident

@pf_next:
    add     rdi, g_streamPageSize
    dec     ebx
    jmp     @pf_loop

@pf_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
>>>>>>> origin/main
    pop     rbx
    ret
StreamPrefetch ENDP

<<<<<<< HEAD
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
=======
; ════════════════════════════════════════════════════════════════
; StreamPrefetchRange — prefetch a specific address range
;   RCX = start address, RDX = length
;   Internal helper for initial GGUF header prefetch.
; ════════════════════════════════════════════════════════════════
StreamPrefetchRange PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rsi, rcx                    ; start address
    mov     rbx, rdx                    ; length

@pfr_loop:
    test    rbx, rbx
    jz      @pfr_done
    cmp     rbx, 0
    jle     @pfr_done

    mov     rcx, rsi
    mov     rdx, g_streamPageSize
    cmp     rbx, rdx
    jae     @pfr_full
    mov     rdx, rbx                   ; last partial page
@pfr_full:
    call    VirtualLock
    test    eax, eax
    jz      @pfr_skip

    ; Record in LRU
    mov     rcx, rsi
    call    LRU_Record
    lock inc qword ptr g_pagesResident

@pfr_skip:
    mov     rax, g_streamPageSize
    add     rsi, rax
    sub     rbx, rax
    jmp     @pfr_loop

@pfr_done:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
StreamPrefetchRange ENDP

; ════════════════════════════════════════════════════════════════
; StreamEvictCold — evict oldest LRU pages when over threshold
;   Walks LRU ring from tail, VirtualUnlock each page.
;   Evicts until pagesResident <= evictionThresh.
;   Returns: EAX = number of pages evicted this call
; ════════════════════════════════════════════════════════════════
StreamEvictCold PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    xor     esi, esi                    ; eviction counter

    mov     rax, g_pagesResident
    cmp     rax, g_evictionThresh
    jbe     @evict_done                 ; under threshold, nothing to do

@evict_loop:
    mov     rax, g_pagesResident
    cmp     rax, g_evictionThresh
    jbe     @evict_done

    ; Check ring empty
    mov     eax, g_lruTail
    cmp     eax, g_lruHead
    je      @evict_done                 ; ring empty

    ; Get page address from tail
    movsxd  rcx, eax
    lea     rdx, g_lruRing
    mov     rcx, [rdx + rcx*8]
    test    rcx, rcx
    jz      @evict_advance              ; null entry, skip

    ; VirtualUnlock(addr, pageSize)
    mov     rdx, g_streamPageSize
    call    VirtualUnlock
    ; VirtualUnlock may "fail" if page already not locked — that's fine

    lock inc qword ptr g_pagesEvicted
    lock dec qword ptr g_pagesResident
    inc     esi

@evict_advance:
    mov     eax, g_lruTail
    inc     eax
    and     eax, LRU_RING_ENTRIES - 1   ; wrap
    mov     g_lruTail, eax
    jmp     @evict_loop

@evict_done:
    mov     eax, esi
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
StreamEvictCold ENDP

; ════════════════════════════════════════════════════════════════
; StreamGetStats — return streaming statistics
;   RCX = pointer to 4-QWORD output buffer:
;     [0] = pageFaults, [1] = pagesEvicted,
;     [2] = pagesResident, [3] = prefetchHits
; ════════════════════════════════════════════════════════════════
StreamGetStats PROC
    mov     rax, g_pageFaults
    mov     [rcx], rax
    mov     rax, g_pagesEvicted
    mov     [rcx+8], rax
    mov     rax, g_pagesResident
    mov     [rcx+10h], rax
    mov     rax, g_prefetchHits
    mov     [rcx+18h], rax
    ret
StreamGetStats ENDP

; ════════════════════════════════════════════════════════════════
; StreamVEHandler — Vectored Exception Handler for page faults
;   Called by Windows VMM on EXCEPTION_ACCESS_VIOLATION.
;   If faulting address is within [g_streamBase, g_streamBase+g_streamSize),
;   we record the fault in LRU, prefetch ahead, and continue.
;   Otherwise pass to next handler.
;
;   RCX = EXCEPTION_POINTERS*
;     [0] = EXCEPTION_RECORD*
;       [0] = ExceptionCode (DWORD)
;       [20h] = ExceptionInformation[1] = faulting VA (on AV)
;     [8] = CONTEXT*
; ════════════════════════════════════════════════════════════════
StreamVEHandler PROC FRAME
>>>>>>> origin/main
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

<<<<<<< HEAD
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
=======
    mov     rbx, rcx                    ; EXCEPTION_POINTERS*
    mov     rsi, [rbx]                  ; EXCEPTION_RECORD*

    ; Check if this is an access violation
    mov     eax, dword ptr [rsi]        ; ExceptionCode
    cmp     eax, EXCEPTION_ACCESS_VIOLATION
    jne     @veh_pass

    ; Get faulting address from ExceptionInformation[1]
    mov     rax, [rsi + 30h]            ; ExceptionInformation[1] = faulting VA

    ; Check if within our stream range
    mov     rcx, g_streamBase
    test    rcx, rcx
    jz      @veh_pass
    cmp     rax, rcx
    jb      @veh_pass
    mov     rdx, rcx
    add     rdx, g_streamSize
    cmp     rax, rdx
    jae     @veh_pass

    ; It's our page fault — record statistics
    lock inc qword ptr g_pageFaults

    ; Calculate offset from base
    sub     rax, g_streamBase

    ; Record in LRU ring (page-aligned address)
    mov     rcx, rax
    mov     rdx, g_streamPageSize
    dec     rdx
    not     rdx
    and     rcx, rdx                    ; page-align the offset
    add     rcx, g_streamBase           ; back to absolute address
    call    LRU_Record

    ; Prefetch ahead from faulting offset
    mov     rcx, rax                    ; offset
    call    StreamPrefetch

    ; The page fault itself is handled by VMM (the mapping exists),
    ; we just track and prefetch. Let VMM resolve it.
    ; Return EXCEPTION_CONTINUE_SEARCH so Windows loads the page normally.
    mov     eax, EXCEPTION_CONTINUE_SEARCH
    jmp     @veh_ret

@veh_pass:
    mov     eax, EXCEPTION_CONTINUE_SEARCH

@veh_ret:
>>>>>>> origin/main
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
<<<<<<< HEAD
StreamGetStats ENDP

END
=======
StreamVEHandler ENDP

; ════════════════════════════════════════════════════════════════
; LRU_Record — add a page address to the LRU ring
;   RCX = page address
;   Internal helper. Lock-free (single producer from VEH context).
; ════════════════════════════════════════════════════════════════
LRU_Record PROC
    mov     eax, g_lruHead
    movsxd  rdx, eax
    lea     r8, g_lruRing
    mov     [r8 + rdx*8], rcx           ; store page address
    inc     eax
    and     eax, LRU_RING_ENTRIES - 1   ; wrap
    mov     g_lruHead, eax

    ; If head caught up to tail, advance tail (overwrite oldest)
    cmp     eax, g_lruTail
    jne     @lr_done
    inc     eax
    and     eax, LRU_RING_ENTRIES - 1
    mov     g_lruTail, eax
@lr_done:
    ret
LRU_Record ENDP


; ═══════════════════════════════════════════════════════════════════════════════
; ═══════════════════════════════════════════════════════════════════════════════
; TIER 2: Per-Tensor Slot-Ring Paging Engine for 200B+ Models
; ═══════════════════════════════════════════════════════════════════════════════
;
; Above: Tier 1 is whole-file VMM demand-paging (StreamMapModel + VEH).
; Below: Tier 2 adds explicit per-tensor sliding-window with budget control.
;
; 200B FP16 ≈ 400 GB. Physical RAM ≤ 64 GB. Tier 2 manages which tensors
; are physically resident via MapViewOfFile per-tensor with a 64-slot ring,
; 2-tier LRU eviction, and SRWLOCK for concurrent inference threads.
;
; The inference loop calls SlotRing_GetTensor(ctx, tensorIdx) which returns
; a pointer to mapped tensor data, paging in on-demand and evicting LRU
; tensors when the memory budget is exceeded.
;
; Exports (Tier 2):
;   SlotRing_Init           — allocate ring + tensor index tables
;   SlotRing_Destroy        — release all resources
;   SlotRing_Attach         — bind to model file + tensor table
;   SlotRing_Detach         — unbind, evict all slots
;   SlotRing_GetTensor      — page tensor by index (demand-fault + LRU)
;   SlotRing_Tick           — prefetch next N tensors, evict if over budget
;   SlotRing_SetBudget      — change memory cap at runtime
;   SlotRing_GetSlotStats   — return stats snapshot
;   SlotRing_EvictAll       — force-evict every slot
; ═══════════════════════════════════════════════════════════════════════════════

EXTERN QueryPerformanceFrequency:PROC
EXTERN InitializeSRWLock:PROC
EXTERN AcquireSRWLockShared:PROC
EXTERN ReleaseSRWLockShared:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC

PUBLIC SlotRing_Init
PUBLIC SlotRing_Destroy
PUBLIC SlotRing_Attach
PUBLIC SlotRing_Detach
PUBLIC SlotRing_GetTensor
PUBLIC SlotRing_Tick
PUBLIC SlotRing_SetBudget
PUBLIC SlotRing_GetSlotStats
PUBLIC SlotRing_EvictAll

; ── Slot Ring Constants ──────────────────────────────────────────────────────
SR_RING_SLOTS       equ 64              ; power-of-2
SR_RING_MASK        equ SR_RING_SLOTS - 1
SR_MAX_TENSORS      equ 8192            ; 200B models ≈ 7000 tensors
SR_SLOT_SIZE        equ 128             ; bytes per ring slot descriptor

; Slot states
SR_EMPTY            equ 0
SR_MAPPED           equ 1               ; data paged in, valid
SR_LIVE             equ 2               ; actively referenced (refCount>0)
SR_EVICTING         equ 3

; Default budget: 1.5 GB
SR_DEFAULT_BUDGET   equ 60000000h

; Beacon messages
SR_PRESSURE_HIGH    equ 2010h
SR_PREFETCH_DONE    equ 2011h

; ── Slot Descriptor Layout (128 bytes) ───────────────────────────────────────
;   +00h  dd  state
;   +04h  dd  tensorIndex
;   +08h  dq  pData          (mapped view base for this tensor)
;   +10h  dq  pViewBase      (actual MapViewOfFile return, for UnmapViewOfFile)
;   +18h  dq  fileOffset     (raw tensor offset in GGUF)
;   +20h  dq  tensorSize     (byte count of this tensor's data)
;   +28h  dq  alignedOffset  (allocation-granularity-aligned file offset)
;   +30h  dq  alignedSize    (aligned mapping size)
;   +38h  dq  qpcLastAccess  (QPC timestamp)
;   +40h  dd  refCount
;   +44h  dd  reserved0
;   +48h  dq  reserved1-5 (40 bytes pad to 128)

SR_SL_STATE         equ 00h
SR_SL_TIDX          equ 04h
SR_SL_PDATA         equ 08h
SR_SL_PVIEW         equ 10h
SR_SL_FOFFSET       equ 18h
SR_SL_TSIZE         equ 20h
SR_SL_AOFFSET       equ 28h
SR_SL_ASIZE         equ 30h
SR_SL_QPC           equ 38h
SR_SL_REFC          equ 40h

; ── Context Layout (page-aligned, 4KB) ───────────────────────────────────────
;   +000h  dq  pRing
;   +008h  dq  pTensorOffsets
;   +010h  dq  pTensorSizes
;   +018h  dd  tensorCount
;   +01Ch  dd  ringHead
;   +020h  dq  hFileMapping
;   +028h  dq  hFile
;   +030h  dq  fileSize
;   +038h  dq  memBudget
;   +040h  dq  memUsed
;   +048h  dq  qpcFreq
;   +050h  dd  allocGran
;   +054h  dd  attached
;   +058h  dq  srwLock          (8 bytes)
;   +060h  dq  statsLoads
;   +068h  dq  statsEvicts
;   +070h  dq  statsTicks
;   +078h  dq  statsPrefetches
;   +080h  dd  prefetchAhead
;   +084h  dd  pressureFlag

SR_CTX_PRING        equ 000h
SR_CTX_PTOFF        equ 008h
SR_CTX_PTSZ         equ 010h
SR_CTX_TCNT         equ 018h
SR_CTX_HEAD         equ 01Ch
SR_CTX_HMAP         equ 020h
SR_CTX_HFILE        equ 028h
SR_CTX_FSIZ         equ 030h
SR_CTX_BUDGET       equ 038h
SR_CTX_USED         equ 040h
SR_CTX_FREQ         equ 048h
SR_CTX_GRAN         equ 050h
SR_CTX_ATT          equ 054h
SR_CTX_LOCK         equ 058h
SR_CTX_SLOADS       equ 060h
SR_CTX_SEVICTS      equ 068h
SR_CTX_STICKS       equ 070h
SR_CTX_SPF          equ 078h
SR_CTX_PFAHEAD      equ 080h
SR_CTX_PFLG         equ 084h

SR_CTX_SIZE         equ 1000h           ; 4KB

; ═══════════════════════════════════════════════════════════════════════════════
; Internal: _SR_GetQPC → RAX = QPC counter
; ═══════════════════════════════════════════════════════════════════════════════
_SR_GetQPC PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    lea     rcx, [rsp+20h]
    call    QueryPerformanceCounter
    mov     rax, [rsp+20h]
    add     rsp, 28h
    ret
_SR_GetQPC ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_Init — allocate context, ring, tensor tables
;   Returns: RAX = context pointer, or NULL on failure
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_Init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; ── Context ───────────────────────────────────────────────────────────
    xor     ecx, ecx
    mov     edx, SR_CTX_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @sri_fail
    mov     rbx, rax

    ; ── Ring (64 * 128 = 8KB) ─────────────────────────────────────────────
    xor     ecx, ecx
    mov     edx, SR_RING_SLOTS * SR_SLOT_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @sri_free_ctx
    mov     [rbx + SR_CTX_PRING], rax

    ; ── Tensor offset table (8192 * 8 = 64KB) ────────────────────────────
    xor     ecx, ecx
    mov     edx, SR_MAX_TENSORS * 8
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @sri_free_ring
    mov     [rbx + SR_CTX_PTOFF], rax

    ; ── Tensor size table (64KB) ──────────────────────────────────────────
    xor     ecx, ecx
    mov     edx, SR_MAX_TENSORS * 8
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @sri_free_off
    mov     [rbx + SR_CTX_PTSZ], rax

    ; ── SRWLOCK ───────────────────────────────────────────────────────────
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    InitializeSRWLock

    ; ── QPC frequency ─────────────────────────────────────────────────────
    lea     rcx, [rbx + SR_CTX_FREQ]
    call    QueryPerformanceFrequency

    ; ── Allocation granularity (reuse g_sysInfo from Tier 1) ──────────────
    lea     rax, g_sysInfo
    mov     eax, dword ptr [rax + 28]   ; dwAllocationGranularity
    mov     [rbx + SR_CTX_GRAN], eax

    ; ── Defaults ──────────────────────────────────────────────────────────
    mov     qword ptr [rbx + SR_CTX_BUDGET], SR_DEFAULT_BUDGET
    mov     dword ptr [rbx + SR_CTX_PFAHEAD], 4
    xor     eax, eax
    mov     dword ptr [rbx + SR_CTX_HEAD], eax
    mov     dword ptr [rbx + SR_CTX_ATT], eax
    mov     qword ptr [rbx + SR_CTX_USED], rax
    mov     qword ptr [rbx + SR_CTX_SLOADS], rax
    mov     qword ptr [rbx + SR_CTX_SEVICTS], rax
    mov     qword ptr [rbx + SR_CTX_STICKS], rax
    mov     qword ptr [rbx + SR_CTX_SPF], rax
    mov     dword ptr [rbx + SR_CTX_PFLG], eax

    mov     rax, rbx
    add     rsp, 30h
    pop     rbx
    ret

@sri_free_off:
    mov     rcx, [rbx + SR_CTX_PTOFF]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@sri_free_ring:
    mov     rcx, [rbx + SR_CTX_PRING]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@sri_free_ctx:
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@sri_fail:
    xor     eax, eax
    add     rsp, 30h
    pop     rbx
    ret
SlotRing_Init ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_Destroy — release all resources
;   RCX = pCtx (NULL-safe)
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_Destroy PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    test    rcx, rcx
    jz      @srd_done
    mov     rbx, rcx

    cmp     dword ptr [rbx + SR_CTX_ATT], 0
    je      @srd_no_detach
    mov     rcx, rbx
    call    SlotRing_Detach
@srd_no_detach:

    mov     rcx, [rbx + SR_CTX_PTSZ]
    test    rcx, rcx
    jz      @srd_1
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@srd_1:
    mov     rcx, [rbx + SR_CTX_PTOFF]
    test    rcx, rcx
    jz      @srd_2
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@srd_2:
    mov     rcx, [rbx + SR_CTX_PRING]
    test    rcx, rcx
    jz      @srd_3
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@srd_3:
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@srd_done:
    add     rsp, 20h
    pop     rbx
    ret
SlotRing_Destroy ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_Attach — bind a GGUF to the slot ring
;   RCX = pCtx
;   RDX = hFile
;   R8  = fileSize
;   R9  = tensorCount
;   [rsp+28h] = pTensorOffsets (QWORD array)
;   [rsp+30h] = pTensorSizes   (QWORD array)
;   Returns: EAX = 1 success, 0 failure
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_Attach PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rbx, rcx
    mov     [rbx + SR_CTX_HFILE], rdx
    mov     [rbx + SR_CTX_FSIZ], r8

    ; Clamp tensor count
    mov     eax, r9d
    cmp     eax, SR_MAX_TENSORS
    jbe     @sra_tc
    mov     eax, SR_MAX_TENSORS
@sra_tc:
    mov     [rbx + SR_CTX_TCNT], eax
    mov     r12d, eax

    ; Stack layout: 4 pushes (20h) + 30h alloc = 50h below original RSP
    ; param5 @ [original + 28h] = [rsp + 50h + 28h] = [rsp + 78h]
    ; param6 @ [original + 30h] = [rsp + 50h + 30h] = [rsp + 80h]

    ; Copy tensor offsets
    mov     rsi, [rsp + 78h]
    mov     rdi, [rbx + SR_CTX_PTOFF]
    mov     ecx, r12d
    rep     movsq

    ; Copy tensor sizes
    mov     rsi, [rsp + 80h]
    mov     rdi, [rbx + SR_CTX_PTSZ]
    mov     ecx, r12d
    rep     movsq

    ; CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)
    mov     rcx, [rbx + SR_CTX_HFILE]
    xor     edx, edx
    mov     r8d, PAGE_READONLY
    xor     r9d, r9d
    xor     eax, eax
    mov     [rsp+20h], rax
    mov     [rsp+28h], rax
    call    CreateFileMappingW
    test    rax, rax
    jz      @sra_fail
    mov     [rbx + SR_CTX_HMAP], rax

    ; Mark attached, zero ring
    mov     dword ptr [rbx + SR_CTX_ATT], 1
    mov     dword ptr [rbx + SR_CTX_HEAD], 0
    mov     qword ptr [rbx + SR_CTX_USED], 0

    mov     rdi, [rbx + SR_CTX_PRING]
    mov     ecx, (SR_RING_SLOTS * SR_SLOT_SIZE) / 8
    xor     eax, eax
    rep     stosq

    mov     eax, 1
    jmp     @sra_ret

@sra_fail:
    xor     eax, eax
@sra_ret:
    add     rsp, 30h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SlotRing_Attach ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_Detach — unbind model, evict all
;   RCX = pCtx
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_Detach PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     rbx, rcx

    mov     rcx, rbx
    call    SlotRing_EvictAll

    mov     rcx, [rbx + SR_CTX_HMAP]
    test    rcx, rcx
    jz      @srd_nm
    call    CloseHandle
    xor     eax, eax
    mov     [rbx + SR_CTX_HMAP], rax
@srd_nm:
    mov     dword ptr [rbx + SR_CTX_ATT], 0
    xor     eax, eax
    mov     [rbx + SR_CTX_HFILE], rax
    mov     [rbx + SR_CTX_FSIZ], rax
    mov     qword ptr [rbx + SR_CTX_USED], rax

    add     rsp, 20h
    pop     rbx
    ret
SlotRing_Detach ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_EvictAll — unmap every slot
;   RCX = pCtx
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_EvictAll PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     rbx, rcx
    mov     rsi, [rbx + SR_CTX_PRING]
    xor     edi, edi

@srea_loop:
    cmp     edi, SR_RING_SLOTS
    jge     @srea_done

    mov     eax, edi
    shl     eax, 7
    lea     rcx, [rsi + rax]

    cmp     dword ptr [rcx + SR_SL_STATE], SR_EMPTY
    je      @srea_next

    ; Unmap the view (use pViewBase, not pData which may be offset)
    push    rcx
    mov     rcx, [rcx + SR_SL_PVIEW]
    test    rcx, rcx
    jz      @srea_clr
    call    UnmapViewOfFile
    inc     qword ptr [rbx + SR_CTX_SEVICTS]
@srea_clr:
    pop     rcx
    mov     rax, [rcx + SR_SL_ASIZE]
    sub     [rbx + SR_CTX_USED], rax
    mov     dword ptr [rcx + SR_SL_STATE], SR_EMPTY
    xor     eax, eax
    mov     [rcx + SR_SL_PDATA], rax
    mov     [rcx + SR_SL_PVIEW], rax
    mov     dword ptr [rcx + SR_SL_TIDX], -1

@srea_next:
    inc     edi
    jmp     @srea_loop

@srea_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SlotRing_EvictAll ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Internal: _SR_EvictLRU — find and evict the least-recently-used slot
;   RCX = pCtx
;   Returns: EAX = freed slot index, -1 if none evictable
; ═══════════════════════════════════════════════════════════════════════════════
_SR_EvictLRU PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rbx, rcx
    mov     rsi, [rbx + SR_CTX_PRING]
    mov     r12d, -1                     ; best slot
    mov     rdi, 7FFFFFFFFFFFFFFFh       ; min QPC
    xor     ecx, ecx

@srlru_scan:
    cmp     ecx, SR_RING_SLOTS
    jge     @srlru_found
    mov     eax, ecx
    shl     eax, 7
    lea     rdx, [rsi + rax]
    mov     eax, [rdx + SR_SL_STATE]
    cmp     eax, SR_MAPPED
    je      @srlru_chk
    cmp     eax, SR_LIVE
    je      @srlru_chk
    jmp     @srlru_nxt
@srlru_chk:
    cmp     dword ptr [rdx + SR_SL_REFC], 0
    jne     @srlru_nxt
    mov     rax, [rdx + SR_SL_QPC]
    cmp     rax, rdi
    jae     @srlru_nxt
    mov     rdi, rax
    mov     r12d, ecx
@srlru_nxt:
    inc     ecx
    jmp     @srlru_scan

@srlru_found:
    cmp     r12d, -1
    je      @srlru_none
    mov     eax, r12d
    shl     eax, 7
    lea     rdx, [rsi + rax]
    mov     dword ptr [rdx + SR_SL_STATE], SR_EVICTING
    push    rdx
    mov     rcx, [rdx + SR_SL_PVIEW]
    test    rcx, rcx
    jz      @srlru_z
    call    UnmapViewOfFile
    inc     qword ptr [rbx + SR_CTX_SEVICTS]
@srlru_z:
    pop     rdx
    mov     rax, [rdx + SR_SL_ASIZE]
    sub     [rbx + SR_CTX_USED], rax
    mov     dword ptr [rdx + SR_SL_STATE], SR_EMPTY
    xor     eax, eax
    mov     [rdx + SR_SL_PDATA], rax
    mov     [rdx + SR_SL_PVIEW], rax
    mov     dword ptr [rdx + SR_SL_TIDX], -1
    mov     eax, r12d
    jmp     @srlru_ret
@srlru_none:
    mov     eax, -1
@srlru_ret:
    add     rsp, 20h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
_SR_EvictLRU ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Internal: _SR_MapSlot — map a tensor into a ring slot
;   RCX = pCtx, EDX = slotIndex, R8D = tensorIndex
;   Returns: RAX = data pointer (past alignment gap), or NULL
; ═══════════════════════════════════════════════════════════════════════════════
_SR_MapSlot PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    mov     rbx, rcx
    mov     esi, edx                     ; slotIndex
    mov     r12d, r8d                    ; tensorIndex

    cmp     r12d, [rbx + SR_CTX_TCNT]
    jae     @srms_fail

    ; Tensor file offset and size
    mov     rax, [rbx + SR_CTX_PTOFF]
    mov     r13, [rax + r12*8]           ; fileOffset
    mov     rax, [rbx + SR_CTX_PTSZ]
    mov     r14, [rax + r12*8]           ; tensorSize

    ; Align offset down to allocation granularity
    mov     eax, [rbx + SR_CTX_GRAN]
    test    eax, eax
    jz      @srms_fail
    mov     ecx, eax
    dec     ecx
    not     rcx                          ; mask = ~(gran-1)
    mov     rax, r13
    and     rax, rcx
    mov     r15, rax                     ; r15 = alignedOffset

    ; Aligned size = (fileOffset - alignedOffset) + tensorSize, round up
    mov     rax, r13
    sub     rax, r15
    add     rax, r14
    mov     ecx, [rbx + SR_CTX_GRAN]
    dec     ecx
    add     rax, rcx
    not     rcx
    and     rax, rcx
    mov     [rsp+28h], rax               ; alignedSize

    ; Evict while over budget
@srms_budget:
    mov     rax, [rbx + SR_CTX_USED]
    add     rax, [rsp+28h]
    cmp     rax, [rbx + SR_CTX_BUDGET]
    jbe     @srms_map
    push    r15
    mov     rcx, rbx
    call    _SR_EvictLRU
    pop     r15
    cmp     eax, -1
    je      @srms_map                    ; can't evict more, map anyway
    jmp     @srms_budget

@srms_map:
    ; MapViewOfFile(hMapping, FILE_MAP_READ, offHigh, offLow, size)
    mov     rcx, [rbx + SR_CTX_HMAP]
    mov     edx, FILE_MAP_READ
    mov     rax, r15                     ; alignedOffset
    shr     rax, 32
    mov     r8d, eax                     ; offsetHigh
    mov     r9d, r15d                    ; offsetLow (low 32)
    mov     rax, [rsp+28h]
    mov     [rsp+20h], rax               ; bytesToMap
    call    MapViewOfFile
    test    rax, rax
    jz      @srms_fail

    ; rax = view base; data = viewBase + (fileOffset - alignedOffset)
    mov     rdi, rax                     ; viewBase
    mov     rcx, r13
    sub     rcx, r15                     ; delta
    lea     rax, [rdi + rcx]            ; data pointer

    ; Fill slot
    mov     ecx, esi
    shl     ecx, 7
    mov     rdx, [rbx + SR_CTX_PRING]
    lea     rdx, [rdx + rcx]

    mov     dword ptr [rdx + SR_SL_STATE], SR_MAPPED
    mov     dword ptr [rdx + SR_SL_TIDX], r12d
    mov     [rdx + SR_SL_PDATA], rax
    mov     [rdx + SR_SL_PVIEW], rdi
    mov     [rdx + SR_SL_FOFFSET], r13
    mov     [rdx + SR_SL_TSIZE], r14
    mov     [rdx + SR_SL_AOFFSET], r15
    mov     rcx, [rsp+28h]
    mov     [rdx + SR_SL_ASIZE], rcx
    mov     dword ptr [rdx + SR_SL_REFC], 0

    ; QPC timestamp
    push    rax
    push    rdx
    call    _SR_GetQPC
    pop     rdx
    mov     [rdx + SR_SL_QPC], rax
    pop     rax

    ; memUsed += alignedSize
    mov     rcx, [rsp+28h]
    add     [rbx + SR_CTX_USED], rcx
    inc     qword ptr [rbx + SR_CTX_SLOADS]

    ; Pressure check (>75% budget)
    mov     rcx, [rbx + SR_CTX_USED]
    mov     rdx, [rbx + SR_CTX_BUDGET]
    shr     rdx, 2
    mov     rdi, [rbx + SR_CTX_BUDGET]
    sub     rdi, rdx                     ; 75% of budget
    cmp     rcx, rdi
    jbe     @srms_nopres
    mov     dword ptr [rbx + SR_CTX_PFLG], 1
    jmp     @srms_ret
@srms_nopres:
    mov     dword ptr [rbx + SR_CTX_PFLG], 0
@srms_ret:
    add     rsp, 38h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
@srms_fail:
    xor     eax, eax
    add     rsp, 38h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
_SR_MapSlot ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_GetTensor — page tensor by index (demand-fault + LRU eviction)
;   RCX = pCtx
;   EDX = tensorIndex
;   Returns: RAX = data pointer, or NULL
;
;   1. Ring scan under shared lock → hit → update QPC, return
;   2. Miss → upgrade to exclusive → double-check → find/evict slot → map
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_GetTensor PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rbx, rcx
    mov     r12d, edx

    cmp     dword ptr [rbx + SR_CTX_ATT], 0
    je      @srgt_fail

    ; ── Shared lock: scan ring ────────────────────────────────────────────
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    AcquireSRWLockShared

    mov     rsi, [rbx + SR_CTX_PRING]
    xor     edi, edi
@srgt_s1:
    cmp     edi, SR_RING_SLOTS
    jge     @srgt_miss
    mov     eax, edi
    shl     eax, 7
    lea     rdx, [rsi + rax]
    cmp     dword ptr [rdx + SR_SL_STATE], SR_EMPTY
    je      @srgt_s1n
    cmp     dword ptr [rdx + SR_SL_TIDX], r12d
    jne     @srgt_s1n
    ; Hit
    mov     dword ptr [rdx + SR_SL_STATE], SR_LIVE
    push    rdx
    call    _SR_GetQPC
    pop     rdx
    mov     [rdx + SR_SL_QPC], rax
    mov     rax, [rdx + SR_SL_PDATA]
    push    rax
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    ReleaseSRWLockShared
    pop     rax
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
@srgt_s1n:
    inc     edi
    jmp     @srgt_s1

@srgt_miss:
    ; ── Upgrade to exclusive ──────────────────────────────────────────────
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    ReleaseSRWLockShared
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    AcquireSRWLockExclusive

    ; Double-check (another thread may have mapped it)
    mov     rsi, [rbx + SR_CTX_PRING]
    xor     edi, edi
@srgt_s2:
    cmp     edi, SR_RING_SLOTS
    jge     @srgt_need
    mov     eax, edi
    shl     eax, 7
    lea     rdx, [rsi + rax]
    cmp     dword ptr [rdx + SR_SL_STATE], SR_EMPTY
    je      @srgt_s2n
    cmp     dword ptr [rdx + SR_SL_TIDX], r12d
    jne     @srgt_s2n
    ; Hit on rescan
    mov     dword ptr [rdx + SR_SL_STATE], SR_LIVE
    push    rdx
    call    _SR_GetQPC
    pop     rdx
    mov     [rdx + SR_SL_QPC], rax
    mov     rax, [rdx + SR_SL_PDATA]
    push    rax
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    ReleaseSRWLockExclusive
    pop     rax
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
@srgt_s2n:
    inc     edi
    jmp     @srgt_s2

@srgt_need:
    ; Find empty slot starting from ring head
    mov     eax, [rbx + SR_CTX_HEAD]
    mov     edi, eax
    xor     ecx, ecx
@srgt_fe:
    cmp     ecx, SR_RING_SLOTS
    jge     @srgt_evict
    mov     eax, edi
    and     eax, SR_RING_MASK
    shl     eax, 7
    lea     rdx, [rsi + rax]
    cmp     dword ptr [rdx + SR_SL_STATE], SR_EMPTY
    je      @srgt_got
    inc     edi
    inc     ecx
    jmp     @srgt_fe

@srgt_evict:
    mov     rcx, rbx
    call    _SR_EvictLRU
    cmp     eax, -1
    je      @srgt_excl_fail
    mov     edi, eax

@srgt_got:
    ; Advance ring head
    mov     eax, edi
    and     eax, SR_RING_MASK
    push    rax
    inc     eax
    and     eax, SR_RING_MASK
    mov     [rbx + SR_CTX_HEAD], eax
    pop     rax                          ; slotIndex

    ; Map tensor
    mov     rcx, rbx
    mov     edx, eax
    mov     r8d, r12d
    call    _SR_MapSlot

    push    rax
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    ReleaseSRWLockExclusive
    pop     rax
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@srgt_excl_fail:
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    ReleaseSRWLockExclusive
@srgt_fail:
    xor     eax, eax
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SlotRing_GetTensor ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_Tick — prefetch next N tensors, check pressure
;   RCX = pCtx
;   Returns: EAX = number of tensors prefetched
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rbx, rcx
    cmp     dword ptr [rbx + SR_CTX_ATT], 0
    je      @srt_zero
    inc     qword ptr [rbx + SR_CTX_STICKS]

    lea     rcx, [rbx + SR_CTX_LOCK]
    call    AcquireSRWLockExclusive

    xor     r13d, r13d                   ; prefetch count
    mov     r12d, [rbx + SR_CTX_HEAD]
    mov     esi, [rbx + SR_CTX_PFAHEAD]

@srt_pf:
    cmp     r13d, esi
    jge     @srt_pfdone

    ; tensorIndex = (head + counter) % tensorCount
    mov     eax, r12d
    add     eax, r13d
    xor     edx, edx
    mov     ecx, [rbx + SR_CTX_TCNT]
    test    ecx, ecx
    jz      @srt_pfdone
    div     ecx
    mov     edi, edx                     ; target tensor

    ; Already in ring?
    mov     rsi, [rbx + SR_CTX_PRING]
    xor     ecx, ecx
@srt_chk:
    cmp     ecx, SR_RING_SLOTS
    jge     @srt_domap
    mov     eax, ecx
    shl     eax, 7
    lea     rdx, [rsi + rax]
    cmp     dword ptr [rdx + SR_SL_STATE], SR_EMPTY
    je      @srt_chkn
    cmp     dword ptr [rdx + SR_SL_TIDX], edi
    je      @srt_skip
@srt_chkn:
    inc     ecx
    jmp     @srt_chk

@srt_domap:
    ; Budget check
    mov     rax, [rbx + SR_CTX_USED]
    cmp     rax, [rbx + SR_CTX_BUDGET]
    jae     @srt_pfdone

    ; Find empty slot
    xor     ecx, ecx
@srt_fe:
    cmp     ecx, SR_RING_SLOTS
    jge     @srt_pfev
    mov     eax, ecx
    shl     eax, 7
    lea     rdx, [rsi + rax]
    cmp     dword ptr [rdx + SR_SL_STATE], SR_EMPTY
    je      @srt_pfslot
    inc     ecx
    jmp     @srt_fe

@srt_pfev:
    push    rdi
    push    r13
    mov     rcx, rbx
    call    _SR_EvictLRU
    pop     r13
    pop     rdi
    cmp     eax, -1
    je      @srt_pfdone
    mov     ecx, eax

@srt_pfslot:
    push    r13
    push    rdi
    mov     r8d, edi
    mov     edx, ecx
    mov     rcx, rbx
    call    _SR_MapSlot
    pop     rdi
    pop     r13
    test    rax, rax
    jz      @srt_pfdone
    inc     qword ptr [rbx + SR_CTX_SPF]

@srt_skip:
    inc     r13d
    mov     esi, [rbx + SR_CTX_PFAHEAD]
    jmp     @srt_pf

@srt_pfdone:
    ; Pressure beacon
    cmp     dword ptr [rbx + SR_CTX_PFLG], 0
    je      @srt_nobc
    mov     ecx, 0
    xor     edx, edx
    mov     r8d, SR_PRESSURE_HIGH
    call    BeaconSend
@srt_nobc:

    lea     rcx, [rbx + SR_CTX_LOCK]
    call    ReleaseSRWLockExclusive

    mov     eax, r13d
    add     rsp, 30h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@srt_zero:
    xor     eax, eax
    add     rsp, 30h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SlotRing_Tick ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_SetBudget — change memory cap
;   RCX = pCtx, RDX = newBudgetBytes
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_SetBudget PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    test    rcx, rcx
    jz      @srsbdone
    mov     rbx, rcx
    mov     rsi, rdx

    lea     rcx, [rbx + SR_CTX_LOCK]
    call    AcquireSRWLockExclusive

    mov     [rbx + SR_CTX_BUDGET], rsi

@srsb_evloop:
    mov     rax, [rbx + SR_CTX_USED]
    cmp     rax, rsi
    jbe     @srsb_ok
    mov     rcx, rbx
    call    _SR_EvictLRU
    cmp     eax, -1
    jne     @srsb_evloop
@srsb_ok:
    lea     rcx, [rbx + SR_CTX_LOCK]
    call    ReleaseSRWLockExclusive
@srsbdone:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
SlotRing_SetBudget ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SlotRing_GetSlotStats — copy stats to caller buffer (64 bytes)
;   RCX = pCtx, RDX = pOut
;   Layout: +0=memUsed,+8=memBudget,+10=loads,+18=evicts,
;           +20=ticks,+28=prefetches,+30=tensorCount(dd),+34=pressure(dd),
;           +38=attached(dd),+3C=pfAhead(dd)
; ═══════════════════════════════════════════════════════════════════════════════
SlotRing_GetSlotStats PROC
    test    rcx, rcx
    jz      @srss_fail
    test    rdx, rdx
    jz      @srss_fail
    mov     rax, [rcx + SR_CTX_USED]
    mov     [rdx], rax
    mov     rax, [rcx + SR_CTX_BUDGET]
    mov     [rdx+8], rax
    mov     rax, [rcx + SR_CTX_SLOADS]
    mov     [rdx+10h], rax
    mov     rax, [rcx + SR_CTX_SEVICTS]
    mov     [rdx+18h], rax
    mov     rax, [rcx + SR_CTX_STICKS]
    mov     [rdx+20h], rax
    mov     rax, [rcx + SR_CTX_SPF]
    mov     [rdx+28h], rax
    mov     eax, [rcx + SR_CTX_TCNT]
    mov     [rdx+30h], eax
    mov     eax, [rcx + SR_CTX_PFLG]
    mov     [rdx+34h], eax
    mov     eax, [rcx + SR_CTX_ATT]
    mov     [rdx+38h], eax
    mov     eax, [rcx + SR_CTX_PFAHEAD]
    mov     [rdx+3Ch], eax
    xor     eax, eax
    ret
@srss_fail:
    mov     eax, -1
    ret
SlotRing_GetSlotStats ENDP

END
>>>>>>> origin/main
