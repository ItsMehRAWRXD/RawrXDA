; ═══════════════════════════════════════════════════════════════════
; async_pager.asm — Phase 9B: Background Paging Worker Thread
;
; Removes VirtualAlloc/VirtualFree latency from the inference hot path
; by running commit/decommit on a dedicated background thread with a
; lock-free request queue.
;
; Architecture:
;   - 256-entry ring buffer of PageRequest structs
;   - Producer: inference thread enqueues commit/decommit via AsyncPage_Submit
;   - Consumer: background thread processes with VirtualAlloc/VirtualFree
;   - Completion: caller can poll or wait (EventObject) for individual ops
;   - The background thread runs at BELOW_NORMAL priority to avoid
;     starving the inference pipeline
;
; Exports:
;   AsyncPage_Init          — Create worker thread + queue
;   AsyncPage_Shutdown      — Signal exit, wait for thread, cleanup
;   AsyncPage_Submit        — Enqueue a commit/decommit request
;   AsyncPage_Poll          — Check if a specific request completed
;   AsyncPage_Flush         — Block until queue drains
;   AsyncPage_GetStats      — Return queue depth + ops completed
; ═══════════════════════════════════════════════════════════════════

EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN CreateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN CreateEventW:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN Sleep:PROC
EXTERN SetThreadPriority:PROC
EXTERN WaitForMultipleObjects:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN GetLastError:PROC
EXTERN QueryPerformanceCounter:PROC

PUBLIC AsyncPage_Init
PUBLIC AsyncPage_Shutdown
PUBLIC AsyncPage_Submit
PUBLIC AsyncPage_Poll
PUBLIC AsyncPage_Flush
PUBLIC AsyncPage_GetStats

; ── Constants ────────────────────────────────────────────────────
ASYNC_QUEUE_SIZE        equ 256     ; Must be power of 2
ASYNC_QUEUE_MASK        equ 0FFh    ; QUEUE_SIZE - 1

; Request types
ASYNC_OP_COMMIT         equ 1       ; VirtualAlloc MEM_COMMIT
ASYNC_OP_DECOMMIT       equ 2       ; VirtualFree MEM_DECOMMIT
ASYNC_OP_PREFETCH       equ 3       ; Commit + zero-fill prefetch

; Request states
REQ_STATE_FREE          equ 0
REQ_STATE_PENDING       equ 1
REQ_STATE_COMPLETE      equ 2
REQ_STATE_FAILED        equ 3

; Thread priority
THREAD_PRIORITY_BELOW_NORMAL equ -1

; Wait constants
INFINITE_WAIT           equ 0FFFFFFFFh

; VirtualAlloc flags
MEM_COMMIT_             equ 1000h
MEM_DECOMMIT_           equ 4000h
PAGE_READWRITE_         equ 4

; ── Pager context struct offsets ─────────────────────────────
CTX_hEvent          equ 0
CTX_hShutdown       equ 8
CTX_pReqRing        equ 16
CTX_reqHead         equ 24
CTX_reqTail         equ 32
CTX_pCompRing       equ 40
CTX_compHead        equ 48
CTX_pPagePool       equ 56
CTX_pages_in        equ 64
CTX_pages_out       equ 72
CTX_prefetches      equ 80
CTX_evictions       equ 88
CTX_total_lat       equ 96
CTX_hCompEvent      equ 104
CTX_hFile           equ 112

; ── Request entry layout (48 bytes) ──────────────────────────
REQ_id              equ 0
REQ_type            equ 8
REQ_flags           equ 12
REQ_addr            equ 16
REQ_size            equ 24
REQ_fileoff         equ 32
REQ_pte             equ 40
REQ_ENTRY_SIZE      equ 48

; ── Completion entry layout (24 bytes) ───────────────────────
COMP_req_id         equ 0
COMP_status         equ 8
COMP_latency        equ 16
COMP_ENTRY_SIZE     equ 24

; Request types (pager context)
PAGE_IN_REQ         equ 0
PAGE_OUT_REQ        equ 1
PREFETCH_REQ        equ 2
EVICT_REQ           equ 3

; Page table entry states
PTE_COMMITTED       equ 1
PTE_PREFETCHING     equ 2

; Completion status
STATUS_OK           equ 0
STATUS_ERROR        equ 1
STATUS_CANCELLED    equ 2

; Page size
PAGE_SIZE_          equ 4096

.data
align 8
; Queue state (producer/consumer indices)
align 16
g_asyncHead     dq 0                ; Producer write index (atomic)
align 16
g_asyncTail     dq 0                ; Consumer read index (atomic)
align 16

; Worker thread state
g_asyncThread   dq 0                ; Thread HANDLE
g_asyncEvent    dq 0                ; Wake event HANDLE
g_asyncExitFlag dd 0                ; 1 = shutdown requested
g_asyncReady    dd 0                ; 1 = init complete

; Telemetry
g_asyncOpsCommit    dq 0            ; Total commits processed
g_asyncOpsDecommit  dq 0            ; Total decommits processed
g_asyncOpsFailed    dq 0            ; Total failures

.data?
align 16
; Request queue: 256 entries × 48 bytes each = 12 KB
; Layout per entry:
;   +0   opType   dd      (ASYNC_OP_*)
;   +4   state    dd      (REQ_STATE_*)
;   +8   pAddr    dq      (target virtual address)
;   +16  size     dq      (region size in bytes)
;   +24  result   dq      (returned pointer or error)
;   +32  seqNum   dq      (sequence number for Poll)
;   +40  pad      dq      (alignment pad)
g_asyncQueue    db (ASYNC_QUEUE_SIZE * 48) dup(?)

; Global sequence counter
g_asyncSeqNum   dq ?

.code

; ────────────────────────────────────────────────────────────────
; AsyncPage_Init — Create the background paging worker
;   No parameters. Returns 0 on success, -1 on failure.
; ────────────────────────────────────────────────────────────────
AsyncPage_Init PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    ; Already initialized?
    cmp     g_asyncReady, 1
    je      @api_already

    ; Zero the queue
    lea     rdi, g_asyncQueue
    mov     rcx, (ASYNC_QUEUE_SIZE * 48) / 8
    xor     eax, eax
    rep     stosq

    mov     g_asyncHead, 0
    mov     g_asyncTail, 0
    mov     g_asyncExitFlag, 0
    mov     g_asyncSeqNum, 0
    mov     g_asyncOpsCommit, 0
    mov     g_asyncOpsDecommit, 0
    mov     g_asyncOpsFailed, 0

    ; Create wake event (auto-reset, non-signaled)
    xor     ecx, ecx                ; lpEventAttributes
    xor     edx, edx                ; bManualReset = FALSE (auto-reset)
    xor     r8d, r8d                ; bInitialState = FALSE
    xor     r9, r9                  ; lpName = NULL
    call    CreateEventW
    test    rax, rax
    jz      @api_fail
    mov     g_asyncEvent, rax

    ; Create worker thread
    xor     ecx, ecx                ; lpThreadAttributes
    xor     edx, edx                ; dwStackSize (default)
    lea     r8, AsyncPage_Worker    ; lpStartAddress
    xor     r9, r9                  ; lpParameter
    mov     qword ptr [rsp+20h], 0  ; dwCreationFlags
    mov     qword ptr [rsp+28h], 0  ; lpThreadId
    call    CreateThread
    test    rax, rax
    jz      @api_fail_close_event
    mov     g_asyncThread, rax

    ; Set to BELOW_NORMAL priority
    mov     rcx, rax
    mov     edx, THREAD_PRIORITY_BELOW_NORMAL
    call    SetThreadPriority

    mov     g_asyncReady, 1
    xor     eax, eax
    jmp     @api_ret

@api_already:
    xor     eax, eax
    jmp     @api_ret

@api_fail_close_event:
    mov     rcx, g_asyncEvent
    call    CloseHandle

@api_fail:
    mov     eax, -1

@api_ret:
    add     rsp, 40h
    pop     rdi
    ret
AsyncPage_Init ENDP

; ────────────────────────────────────────────────────────────────
; AsyncPage_Shutdown — Signal worker exit, wait, cleanup
; ────────────────────────────────────────────────────────────────
AsyncPage_Shutdown PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_asyncReady, 0
    je      @aps_done

    ; Signal exit
    mov     g_asyncExitFlag, 1

    ; Wake the worker
    mov     rcx, g_asyncEvent
    call    SetEvent

    ; Wait for thread to finish (5 second timeout)
    mov     rcx, g_asyncThread
    mov     edx, 5000
    call    WaitForSingleObject

    ; Close handles
    mov     rcx, g_asyncThread
    call    CloseHandle
    mov     rcx, g_asyncEvent
    call    CloseHandle

    mov     g_asyncReady, 0
    mov     g_asyncThread, 0
    mov     g_asyncEvent, 0

@aps_done:
    add     rsp, 28h
    ret
AsyncPage_Shutdown ENDP

; ────────────────────────────────────────────────────────────────
; AsyncPage_Submit — Enqueue a commit/decommit request
;   ECX = opType    (ASYNC_OP_COMMIT / ASYNC_OP_DECOMMIT / ASYNC_OP_PREFETCH)
;   RDX = pAddr     (target virtual address)
;   R8  = size      (bytes)
;   Returns: RAX = sequence number (for Poll), or -1 if queue full
; ────────────────────────────────────────────────────────────────
AsyncPage_Submit PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Check initialized
    cmp     g_asyncReady, 1
    jne     @sub_fail

    ; Check queue not full: (head + 1) & mask != tail
    mov     rax, g_asyncHead
    lea     rbx, [rax + 1]
    and     rbx, ASYNC_QUEUE_MASK
    cmp     rbx, g_asyncTail
    je      @sub_fail               ; Queue full

    ; Compute slot pointer: &queue[head * 48]
    imul    r9, rax, 48
    lea     r10, g_asyncQueue
    add     r10, r9                 ; r10 = slot ptr

    ; Fill the slot
    mov     dword ptr [r10 + 0], ecx    ; opType
    mov     dword ptr [r10 + 4], REQ_STATE_PENDING  ; state
    mov     qword ptr [r10 + 8], rdx    ; pAddr
    mov     qword ptr [r10 + 16], r8    ; size
    mov     qword ptr [r10 + 24], 0     ; result (cleared)

    ; Assign sequence number
    mov     rax, g_asyncSeqNum
    inc     rax
    mov     g_asyncSeqNum, rax
    mov     qword ptr [r10 + 32], rax   ; seqNum
    mov     rbx, rax                    ; save for return

    ; Advance head (store release — memory fence before publish)
    mfence
    mov     rax, g_asyncHead
    lea     rax, [rax + 1]
    and     rax, ASYNC_QUEUE_MASK
    mov     g_asyncHead, rax

    ; Wake worker
    push    rbx
    mov     rcx, g_asyncEvent
    call    SetEvent
    pop     rbx

    mov     rax, rbx                    ; return seqNum
    jmp     @sub_ret

@sub_fail:
    mov     rax, -1

@sub_ret:
    add     rsp, 30h
    pop     rbx
    ret
AsyncPage_Submit ENDP

; ────────────────────────────────────────────────────────────────
; AsyncPage_Poll — Check completion of a request by sequence number
;   RCX = seqNum
;   Returns: EAX = REQ_STATE_* (PENDING / COMPLETE / FAILED)
; ────────────────────────────────────────────────────────────────
AsyncPage_Poll PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Linear scan (queue is small — 256 entries)
    lea     rdx, g_asyncQueue
    xor     eax, eax                ; index

@poll_loop:
    cmp     eax, ASYNC_QUEUE_SIZE
    jae     @poll_notfound

    imul    r8d, eax, 48
    cmp     qword ptr [rdx + r8 + 32], rcx   ; seqNum match?
    jne     @poll_next

    mov     eax, dword ptr [rdx + r8 + 4]    ; return state
    jmp     @poll_ret

@poll_next:
    inc     eax
    jmp     @poll_loop

@poll_notfound:
    mov     eax, REQ_STATE_FREE             ; not found = already reaped

@poll_ret:
    add     rsp, 28h
    ret
AsyncPage_Poll ENDP

; ────────────────────────────────────────────────────────────────
; AsyncPage_Flush — Block until all pending requests complete
;   Spins on head == tail with Sleep(0) yields.
; ────────────────────────────────────────────────────────────────
AsyncPage_Flush PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

@flush_loop:
    mov     rax, g_asyncHead
    cmp     rax, g_asyncTail
    je      @flush_done

    ; Yield
    xor     ecx, ecx
    call    Sleep
    jmp     @flush_loop

@flush_done:
    add     rsp, 28h
    ret
AsyncPage_Flush ENDP

; ────────────────────────────────────────────────────────────────
; AsyncPage_GetStats — Return telemetry
;   RCX = pOutStats (struct: +0 commits QWORD, +8 decommits QWORD,
;                    +16 failures QWORD, +24 queueDepth DWORD)
; ────────────────────────────────────────────────────────────────
AsyncPage_GetStats PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rax, g_asyncOpsCommit
    mov     qword ptr [rcx], rax
    mov     rax, g_asyncOpsDecommit
    mov     qword ptr [rcx + 8], rax
    mov     rax, g_asyncOpsFailed
    mov     qword ptr [rcx + 16], rax

    ; Queue depth = (head - tail) & mask
    mov     eax, dword ptr [g_asyncHead]
    sub     eax, dword ptr [g_asyncTail]
    and     eax, ASYNC_QUEUE_MASK
    mov     dword ptr [rcx + 24], eax

    add     rsp, 28h
    ret
AsyncPage_GetStats ENDP

; ────────────────────────────────────────────────────────────────
; AsyncPage_Worker — Background paging worker thread
;   RCX = pointer to pager context struct
;   Returns 0 on clean shutdown.
;
;   Context layout: hEvent(0) hShutdown(8) pReqRing(16) reqHead(24)
;   reqTail(32) pCompRing(40) compHead(48) pPagePool(56) pages_in(64)
;   pages_out(72) prefetches(80) evictions(88) total_lat(96)
;   hCompEvent(104) hFile(112)
; ────────────────────────────────────────────────────────────────
AsyncPage_Worker PROC FRAME
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
    sub     rsp, 80h                        ; shadow+locals+OVERLAPPED
    .allocstack 80h
    .endprolog

    mov     r12, rcx                        ; R12 = pager context ptr

; ── Main loop ────────────────────────────────────────────────
@aw_loop:
    ; Build handle array on stack: [hEvent, hShutdownEvent]
    mov     rax, [r12 + CTX_hEvent]
    mov     [rsp+30h], rax
    mov     rax, [r12 + CTX_hShutdown]
    mov     [rsp+38h], rax

    ; WaitForMultipleObjects(2, &hArray, FALSE, INFINITE)
    mov     ecx, 2
    lea     rdx, [rsp+30h]
    xor     r8d, r8d                        ; bWaitAll = FALSE
    mov     r9d, INFINITE_WAIT
    call    WaitForMultipleObjects

    cmp     eax, 1                          ; WAIT_OBJECT_0+1 → shutdown
    je      @aw_shutdown
    test    eax, eax                        ; WAIT_OBJECT_0 → work
    jnz     @aw_loop                        ; timeout / error → retry

; ── Drain request ring ───────────────────────────────────────
@aw_drain:
    mov     rax, [r12 + CTX_reqTail]
    cmp     rax, [r12 + CTX_reqHead]
    je      @aw_loop                        ; empty → back to wait

    ; Compute request slot pointer
    mov     ebx, eax
    and     ebx, ASYNC_QUEUE_MASK
    imul    rsi, rbx, REQ_ENTRY_SIZE
    add     rsi, [r12 + CTX_pReqRing]       ; RSI = current request

    ; Timestamp start for latency measurement
    lea     rcx, [rsp+48h]
    call    QueryPerformanceCounter
    mov     r14, [rsp+48h]                  ; R14 = start ticks

    mov     r13d, dword ptr [rsi + REQ_type]; R13D = request type
    mov     r15, [rsi + REQ_id]             ; R15  = request id

    cmp     r13d, PAGE_IN_REQ
    je      @aw_page_in
    cmp     r13d, PAGE_OUT_REQ
    je      @aw_page_out
    cmp     r13d, PREFETCH_REQ
    je      @aw_prefetch
    cmp     r13d, EVICT_REQ
    je      @aw_evict
    jmp     @aw_error                       ; unknown type

; ── PAGE_IN: VirtualAlloc + ReadFile ─────────────────────────
@aw_page_in:
    mov     rcx, [rsi + REQ_addr]
    mov     rdx, [rsi + REQ_size]
    mov     r8d, MEM_COMMIT_
    mov     r9d, PAGE_READWRITE_
    call    VirtualAlloc
    test    rax, rax
    jz      @aw_error
    mov     rdi, rax                        ; RDI = committed page

    ; ReadFile(hFile, pPage, size, &bytesRead, NULL)
    mov     rcx, [r12 + CTX_hFile]
    mov     rdx, rdi
    mov     r8d, dword ptr [rsi + REQ_size]
    lea     r9, [rsp+40h]
    mov     qword ptr [rsp+20h], 0          ; lpOverlapped = NULL
    call    ReadFile
    test    eax, eax
    jz      @aw_error

    ; Update page table entry → COMMITTED
    mov     rax, [rsi + REQ_pte]
    test    rax, rax
    jz      @pi_no_pte
    mov     dword ptr [rax], PTE_COMMITTED
@pi_no_pte:
    lock inc qword ptr [r12 + CTX_pages_in]
    xor     ebx, ebx                       ; STATUS_OK
    jmp     @aw_complete

; ── PAGE_OUT: WriteFile (if dirty) + VirtualFree ─────────────
@aw_page_out:
    test    dword ptr [rsi + REQ_flags], 1  ; dirty flag?
    jz      @po_decommit

    ; WriteFile(hFile, addr, size, &bytesWritten, NULL)
    mov     rcx, [r12 + CTX_hFile]
    mov     rdx, [rsi + REQ_addr]
    mov     r8d, dword ptr [rsi + REQ_size]
    lea     r9, [rsp+40h]
    mov     qword ptr [rsp+20h], 0
    call    WriteFile
    test    eax, eax
    jz      @aw_error

@po_decommit:
    mov     rcx, [rsi + REQ_addr]
    mov     rdx, [rsi + REQ_size]
    mov     r8d, MEM_DECOMMIT_
    call    VirtualFree
    test    eax, eax
    jz      @aw_error
    lock inc qword ptr [r12 + CTX_pages_out]
    xor     ebx, ebx
    jmp     @aw_complete

; ── PREFETCH: Overlapped async ReadFile ──────────────────────
@aw_prefetch:
    mov     rcx, [rsi + REQ_addr]
    mov     rdx, [rsi + REQ_size]
    mov     r8d, MEM_COMMIT_
    mov     r9d, PAGE_READWRITE_
    call    VirtualAlloc
    test    rax, rax
    jz      @aw_error
    mov     rdi, rax

    ; Build OVERLAPPED struct at [rsp+50h]
    xor     eax, eax
    mov     [rsp+50h], rax                  ; Internal
    mov     [rsp+58h], rax                  ; InternalHigh
    mov     rax, [rsi + REQ_fileoff]
    mov     dword ptr [rsp+60h], eax        ; Offset (low 32)
    shr     rax, 32
    mov     dword ptr [rsp+64h], eax        ; OffsetHigh
    mov     rax, [r12 + CTX_hEvent]
    mov     [rsp+68h], rax                  ; hEvent for overlap

    ; ReadFile with OVERLAPPED
    mov     rcx, [r12 + CTX_hFile]
    mov     rdx, rdi
    mov     r8d, dword ptr [rsi + REQ_size]
    lea     r9, [rsp+40h]
    lea     rax, [rsp+50h]
    mov     [rsp+20h], rax                  ; lpOverlapped
    call    ReadFile

    ; Mark page table entry as PREFETCHING
    mov     rax, [rsi + REQ_pte]
    test    rax, rax
    jz      @pf_no_pte
    mov     dword ptr [rax], PTE_PREFETCHING
@pf_no_pte:
    lock inc qword ptr [r12 + CTX_prefetches]
    xor     ebx, ebx
    jmp     @aw_complete

; ── EVICT: Find coldest page, decommit ───────────────────────
@aw_evict:
    mov     rcx, [r12 + CTX_pPagePool]
    test    rcx, rcx
    jz      @aw_error
    xor     edx, edx                        ; best_idx
    mov     r8, 7FFFFFFFFFFFFFFFh           ; min access count
    xor     r9d, r9d                        ; scan index
@ev_scan:
    cmp     r9d, ASYNC_QUEUE_SIZE
    jae     @ev_do
    imul    eax, r9d, 24                    ; pool entry = 24 bytes
    mov     r10, [rcx + rax + 8]            ; access_count
    cmp     r10, r8
    jae     @ev_next
    mov     r8, r10                         ; new minimum
    mov     edx, r9d                        ; new best index
@ev_next:
    inc     r9d
    jmp     @ev_scan
@ev_do:
    imul    eax, edx, 24
    mov     rdi, [r12 + CTX_pPagePool]
    add     rdi, rax                        ; RDI = pool entry ptr
    mov     rcx, [rdi]                      ; victim page address
    mov     rdx, [rsi + REQ_size]
    test    rdx, rdx
    jnz     @ev_has_sz
    mov     edx, PAGE_SIZE_                 ; default page size
@ev_has_sz:
    mov     r8d, MEM_DECOMMIT_
    call    VirtualFree
    test    eax, eax
    jz      @aw_error
    mov     qword ptr [rdi + 8], 0          ; reset access count
    lock inc qword ptr [r12 + CTX_evictions]
    xor     ebx, ebx
    jmp     @aw_complete

; ── Error handler ────────────────────────────────────────────
@aw_error:
    call    GetLastError                    ; EAX = Win32 error code
    mov     ebx, STATUS_ERROR

; ── Write completion + update stats ──────────────────────────
@aw_complete:
    ; Timestamp end → compute latency
    lea     rcx, [rsp+48h]
    call    QueryPerformanceCounter
    mov     rax, [rsp+48h]
    sub     rax, r14                        ; latency in ticks
    lock add [r12 + CTX_total_lat], rax

    ; Write completion ring entry
    mov     rcx, [r12 + CTX_compHead]
    mov     edx, ecx
    and     edx, ASYNC_QUEUE_MASK
    imul    edx, edx, COMP_ENTRY_SIZE
    add     rdx, [r12 + CTX_pCompRing]
    mov     [rdx + COMP_req_id], r15        ; request_id
    mov     dword ptr [rdx + COMP_status], ebx ; status
    mov     [rdx + COMP_latency], rax       ; latency

    ; Advance completion head
    lock inc qword ptr [r12 + CTX_compHead]

    ; Advance request tail (store-release)
    mfence
    lock inc qword ptr [r12 + CTX_reqTail]

    ; Signal completion event for waiters
    mov     rcx, [r12 + CTX_hCompEvent]
    test    rcx, rcx
    jz      @aw_drain
    call    SetEvent
    jmp     @aw_drain

; ── Shutdown: drain remaining requests as CANCELLED ──────────
@aw_shutdown:
    mov     rax, [r12 + CTX_reqTail]
    cmp     rax, [r12 + CTX_reqHead]
    je      @aw_exit

    mov     ebx, eax
    and     ebx, ASYNC_QUEUE_MASK
    imul    rsi, rbx, REQ_ENTRY_SIZE
    add     rsi, [r12 + CTX_pReqRing]
    mov     r15, [rsi + REQ_id]

    ; Write CANCELLED completion
    mov     rcx, [r12 + CTX_compHead]
    mov     edx, ecx
    and     edx, ASYNC_QUEUE_MASK
    imul    edx, edx, COMP_ENTRY_SIZE
    add     rdx, [r12 + CTX_pCompRing]
    mov     [rdx + COMP_req_id], r15
    mov     dword ptr [rdx + COMP_status], STATUS_CANCELLED
    mov     qword ptr [rdx + COMP_latency], 0
    lock inc qword ptr [r12 + CTX_compHead]
    lock inc qword ptr [r12 + CTX_reqTail]
    jmp     @aw_shutdown

@aw_exit:
    xor     eax, eax                       ; return 0
    add     rsp, 80h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AsyncPage_Worker ENDP

END
