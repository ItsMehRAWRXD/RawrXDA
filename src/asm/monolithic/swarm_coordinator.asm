; ═══════════════════════════════════════════════════════════════════
; swarm_coordinator.asm — RawrXD Beacon-Based Work Distribution
; Phase X+5: Distributed Swarm — Worker Threads + Ring All-Reduce
; Assembles: ml64 /c /Fo swarm_coordinator.obj swarm_coordinator.asm
;
; Architecture:
;   - Creates worker threads (one per GPU device)
;   - Dispatches shard computation via Beacon slot messages
;   - Ring all-reduce for gradient merge across devices
;   - Monitors worker health via heartbeat polling
;   - Integrates with swarm.asm for device/shard management
;
; All hex in MASM style. No sizeof, no addr, no high-level call macros.
; Win64 ABI: never push/pop around calls — use frame-allocated
; stack space to preserve registers. Shadow space (20h) always
; guaranteed by allocstack.
; ═══════════════════════════════════════════════════════════════════

; ── Win32 imports ────────────────────────────────────────────────
EXTERN CreateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN WaitForMultipleObjects:PROC
EXTERN Sleep:PROC
EXTERN GetTickCount64:PROC
EXTERN CloseHandle:PROC

; ── Cross-module imports ─────────────────────────────────────────
EXTERN g_hHeap:QWORD
EXTERN BeaconSend:PROC

; ── Swarm module imports ─────────────────────────────────────────
EXTERN Swarm_GetDeviceCount:PROC
EXTERN Swarm_AllocShard:PROC
EXTERN Swarm_DispatchCompute:PROC
EXTERN Swarm_P2PCopy:PROC
EXTERN g_swarmDeviceCount:DWORD
EXTERN g_swarmReady:DWORD

; ── Public exports ───────────────────────────────────────────────
PUBLIC SwarmCoord_Init
PUBLIC SwarmCoord_DistributeWork
PUBLIC SwarmCoord_AllReduce
PUBLIC SwarmCoord_GetWorkerStatus
PUBLIC SwarmCoord_Shutdown
PUBLIC g_coordReady
PUBLIC g_workerCount

; ── Constants ────────────────────────────────────────────────────
MAX_WORKERS         equ 8              ; matches MAX_GPUS
WORKER_STATE_SIZE   equ 64             ; bytes per worker state
COORD_BEACON_SLOT   equ 15             ; Beacon slot for coordinator
HEARTBEAT_INTERVAL  equ 1000           ; ms between heartbeat checks
REDUCE_BUF_SIZE     equ 10000h         ; 64KB per reduce ring buffer
INFINITE_WAIT       equ 0FFFFFFFFh

; Worker states
WORKER_IDLE          equ 0
WORKER_BUSY          equ 1
WORKER_DONE          equ 2
WORKER_ERROR         equ 3
WORKER_DEAD          equ 4

; Coordinator events
COORD_EVT_STARTED    equ 0F1h
COORD_EVT_WORK_SENT  equ 0F2h
COORD_EVT_REDUCE_OK  equ 0F3h
COORD_EVT_ALL_DONE   equ 0F4h
COORD_EVT_SHUTDOWN   equ 0F5h

; ── Data ─────────────────────────────────────────────────────────
.data
align 16
g_coordReady        dd 0
g_workerCount       dd 0
g_heartbeatThread   dq 0

align 16
g_workerThreads     dq MAX_WORKERS dup(0)

; Worker state table: MAX_WORKERS x WORKER_STATE_SIZE
;   Offset 0:   state       (dd)
;   Offset 4:   deviceIdx   (dd)
;   Offset 8:   shardIdx    (dd)
;   Offset 12:  jobCount    (dd)
;   Offset 16:  lastHbeat   (dq)
;   Offset 24:  errorCode   (dd)
;   Offset 28:  reserved    (36 bytes)
align 16
g_workerState       db (MAX_WORKERS * WORKER_STATE_SIZE) dup(0)

align 16
g_workQueue         dd 256 dup(0)
g_workQueueHead     dd 0
g_workQueueTail     dd 0
g_workQueueCount    dd 0

align 16
g_reducePhase       dd 0
g_reduceComplete    dd 0

; ── BSS ──────────────────────────────────────────────────────────
.data?
align 16
g_reduceBuf         db REDUCE_BUF_SIZE dup(?)

.code

; ════════════════════════════════════════════════════════════════════
; SwarmCoord_Init — Initialize coordinator, spawn worker threads
;   No args. Returns EAX = worker count (0 = no GPUs).
;   FRAME: 4 pushes (rbp,rbx,rsi,rdi) + 48h alloc
;     ABI: 8 + 4*8 + 48h = 112 → 112/16=7 exact.
;   Stack layout (from RSP after alloc):
;     [rsp+00h..1Fh]  shadow space for callees
;     [rsp+20h..27h]  local save slot 0
;     [rsp+28h..2Fh]  local save slot 1
;     [rsp+30h..37h]  local save slot 2
;     [rsp+38h..3Fh]  local save slot 3
;     [rsp+40h..47h]  local save slot 4
; ════════════════════════════════════════════════════════════════════
SwarmCoord_Init PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 48h
    .allocstack 48h
    .endprolog

    ; Check if swarm is ready
    cmp     g_swarmReady, 0
    je      @coord_no_gpu

    ; Get device count
    call    Swarm_GetDeviceCount
    test    eax, eax
    jz      @coord_no_gpu
    cmp     eax, MAX_WORKERS
    jbe     @count_ok
    mov     eax, MAX_WORKERS
@count_ok:
    mov     esi, eax
    mov     g_workerCount, eax

    ; Initialize worker state table
    xor     ebx, ebx
@init_workers:
    cmp     ebx, esi
    jge     @spawn_workers

    lea     r10, g_workerState
    imul    eax, ebx, WORKER_STATE_SIZE
    cdqe
    mov     dword ptr [r10 + rax], WORKER_IDLE
    mov     dword ptr [r10 + rax + 4], ebx
    mov     dword ptr [r10 + rax + 8], -1
    mov     dword ptr [r10 + rax + 12], 0

    ; Get heartbeat timestamp — save regs in frame slots, not push/pop
    mov     [rsp+20h], rbx
    mov     [rsp+28h], rsi
    call    GetTickCount64
    mov     rbx, [rsp+20h]
    mov     rsi, [rsp+28h]
    ; Store heartbeat
    lea     r10, g_workerState
    imul    ecx, ebx, WORKER_STATE_SIZE
    movsxd  rcx, ecx
    mov     qword ptr [r10 + rcx + 16], rax

    inc     ebx
    jmp     @init_workers

@spawn_workers:
    xor     ebx, ebx
@spawn_loop:
    cmp     ebx, esi
    jge     @spawn_heartbeat

    ; CreateThread(NULL, 0, WorkerThreadProc, workerIdx, 0, NULL)
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, WorkerThreadProc
    mov     r9d, ebx
    mov     dword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    CreateThread
    test    rax, rax
    jz      @spawn_fail

    lea     r10, g_workerThreads
    mov     [r10 + rbx*8], rax

    inc     ebx
    jmp     @spawn_loop

@spawn_heartbeat:
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, HeartbeatThreadProc
    xor     r9d, r9d
    mov     dword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    CreateThread
    mov     g_heartbeatThread, rax

    mov     g_coordReady, 1

    ; Signal via beacon
    mov     ecx, COORD_BEACON_SLOT
    mov     edx, COORD_EVT_STARTED
    mov     r8d, esi
    call    BeaconSend

    mov     eax, esi
    jmp     @coord_done

@coord_no_gpu:
    mov     g_workerCount, 0
    mov     g_coordReady, 0
    xor     eax, eax
    jmp     @coord_done

@spawn_fail:
    mov     g_coordReady, 0
    xor     eax, eax

@coord_done:
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SwarmCoord_Init ENDP


; ════════════════════════════════════════════════════════════════════
; WorkerThreadProc — Per-GPU worker thread entry
;   RCX = worker index (0-based)
;   FRAME: 3 pushes (rbp,rbx,rsi) + 38h alloc
;     ABI: 8 + 3*8 + 38h = 8+24+56 = 88 → 88/16=5.5 → BAD
;     Fix: use 30h: 8+24+48=80 → 80/16=5. Good.
;   Stack: [rsp+00..1Fh] shadow, [rsp+20h] save slot
; ════════════════════════════════════════════════════════════════════
WorkerThreadProc PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     ebx, ecx

@worker_main_loop:
    cmp     g_coordReady, 0
    je      @worker_exit

    mov     eax, g_workQueueCount
    test    eax, eax
    jz      @worker_sleep

    ; Dequeue
    mov     ecx, g_workQueueHead
    lea     r10, g_workQueue
    mov     esi, dword ptr [r10 + rcx*4]

    inc     ecx
    and     ecx, 0FFh
    mov     g_workQueueHead, ecx
    lock dec g_workQueueCount

    ; BUSY
    lea     r10, g_workerState
    imul    eax, ebx, WORKER_STATE_SIZE
    cdqe
    mov     dword ptr [r10 + rax], WORKER_BUSY
    mov     dword ptr [r10 + rax + 8], esi

    ; Dispatch
    mov     ecx, esi
    call    Swarm_DispatchCompute
    test    eax, eax
    jnz     @worker_error

    ; DONE
    lea     r10, g_workerState
    imul    eax, ebx, WORKER_STATE_SIZE
    cdqe
    mov     dword ptr [r10 + rax], WORKER_DONE
    inc     dword ptr [r10 + rax + 12]

    ; Update heartbeat — save rbx in frame slot
    mov     [rsp+20h], rbx
    call    GetTickCount64
    mov     rbx, [rsp+20h]
    lea     r10, g_workerState
    imul    ecx, ebx, WORKER_STATE_SIZE
    movsxd  rcx, ecx
    mov     qword ptr [r10 + rcx + 16], rax

    ; Signal
    mov     ecx, COORD_BEACON_SLOT
    mov     edx, COORD_EVT_ALL_DONE
    mov     r8d, ebx
    call    BeaconSend

    ; IDLE
    lea     r10, g_workerState
    imul    eax, ebx, WORKER_STATE_SIZE
    cdqe
    mov     dword ptr [r10 + rax], WORKER_IDLE
    jmp     @worker_main_loop

@worker_sleep:
    mov     ecx, 10
    call    Sleep
    jmp     @worker_main_loop

@worker_error:
    lea     r10, g_workerState
    imul    eax, ebx, WORKER_STATE_SIZE
    cdqe
    mov     dword ptr [r10 + rax], WORKER_ERROR
    mov     dword ptr [r10 + rax + 24], -1
    jmp     @worker_main_loop

@worker_exit:
    xor     eax, eax
    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WorkerThreadProc ENDP


; ════════════════════════════════════════════════════════════════════
; HeartbeatThreadProc — Monitor worker health
;   RCX = 0 (unused)
;   FRAME: 3 pushes (rbp,rbx,rsi) + 30h alloc = 80. Good.
; ════════════════════════════════════════════════════════════════════
HeartbeatThreadProc PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

@hb_loop:
    cmp     g_coordReady, 0
    je      @hb_exit

    call    GetTickCount64
    mov     rsi, rax

    mov     ebx, g_workerCount
    xor     ecx, ecx

@hb_check:
    cmp     ecx, ebx
    jge     @hb_sleep

    ; Save ecx in frame slot across state checks
    mov     [rsp+20h], ecx

    lea     r10, g_workerState
    imul    eax, ecx, WORKER_STATE_SIZE
    cdqe
    mov     r11, qword ptr [r10 + rax + 16]
    mov     r9, rsi
    sub     r9, r11

    cmp     r9, 5000
    jb      @hb_next
    mov     r11d, dword ptr [r10 + rax]
    cmp     r11d, WORKER_BUSY
    jne     @hb_next
    mov     dword ptr [r10 + rax], WORKER_DEAD

@hb_next:
    mov     ecx, [rsp+20h]
    inc     ecx
    jmp     @hb_check

@hb_sleep:
    mov     ecx, HEARTBEAT_INTERVAL
    call    Sleep
    jmp     @hb_loop

@hb_exit:
    xor     eax, eax
    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
HeartbeatThreadProc ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmCoord_DistributeWork — Enqueue shard work items
;   RCX = ptr to DWORD array of shard indices
;   EDX = count
;   Returns EAX = enqueued count
;   FRAME: 4 pushes (rbp,rbx,rsi,rdi) + 48h alloc
;     ABI: 8 + 32 + 48h = 112 → 112/16=7. Good.
;   Stack: [rsp+00..1Fh] shadow, [rsp+20h..3Fh] save slots
; ════════════════════════════════════════════════════════════════════
SwarmCoord_DistributeWork PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 48h
    .allocstack 48h
    .endprolog

    mov     rsi, rcx
    mov     ebx, edx
    xor     edi, edi                   ; edi = enqueued count

@enq_loop:
    cmp     edi, ebx
    jge     @enq_done

    mov     eax, g_workQueueCount
    cmp     eax, 256
    jge     @enq_done

    ; Enqueue
    mov     eax, g_workQueueTail
    lea     r10, g_workQueue
    mov     r11d, dword ptr [rsi + rdi*4]
    mov     dword ptr [r10 + rax*4], r11d

    inc     eax
    and     eax, 0FFh
    mov     g_workQueueTail, eax
    lock inc g_workQueueCount

    ; Signal via beacon — save edi in frame slot
    mov     [rsp+20h], rdi
    mov     ecx, COORD_BEACON_SLOT
    mov     edx, COORD_EVT_WORK_SENT
    xor     r8d, r8d
    call    BeaconSend
    mov     rdi, [rsp+20h]

    inc     edi
    jmp     @enq_loop

@enq_done:
    mov     eax, edi

    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SwarmCoord_DistributeWork ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmCoord_AllReduce — Ring all-reduce across worker outputs
;   No args. Returns EAX = 0 success, -1 fail.
;   FRAME: 4 pushes (rbp,rbx,rsi,rdi) + 48h alloc
;     ABI: 8 + 32 + 48h = 112 → exact.
;   Stack: [rsp+20h..3Fh] save slots for loop vars
; ════════════════════════════════════════════════════════════════════
SwarmCoord_AllReduce PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 48h
    .allocstack 48h
    .endprolog

    mov     esi, g_workerCount
    cmp     esi, 2
    jb      @reduce_trivial

    ; Phase 1: Scatter-reduce
    mov     g_reducePhase, 1
    mov     g_reduceComplete, 0

    xor     ebx, ebx
    mov     edi, esi
    dec     edi                        ; N-1 rounds

@scatter_loop:
    cmp     ebx, edi
    jge     @gather_phase

    xor     ecx, ecx                  ; worker index
@scatter_inner:
    cmp     ecx, esi
    jge     @scatter_next

    ; Save loop vars in frame
    mov     [rsp+20h], ecx
    mov     [rsp+28h], ebx
    mov     [rsp+30h], esi
    mov     [rsp+38h], edi

    ; P2P copy: src=ecx, dst=(ecx+1) mod N
    ; ecx already has src
    mov     edx, ecx
    inc     edx
    cmp     edx, esi
    jb      @no_wrap_s
    xor     edx, edx
@no_wrap_s:
    mov     r8d, REDUCE_BUF_SIZE
    call    Swarm_P2PCopy

    ; Restore loop vars
    mov     ecx, [rsp+20h]
    mov     ebx, [rsp+28h]
    mov     esi, [rsp+30h]
    mov     edi, [rsp+38h]

    inc     ecx
    jmp     @scatter_inner

@scatter_next:
    inc     ebx
    jmp     @scatter_loop

@gather_phase:
    mov     g_reducePhase, 2
    xor     ebx, ebx

@gather_loop:
    cmp     ebx, edi
    jge     @reduce_done

    xor     ecx, ecx
@gather_inner:
    cmp     ecx, esi
    jge     @gather_next

    mov     [rsp+20h], ecx
    mov     [rsp+28h], ebx
    mov     [rsp+30h], esi
    mov     [rsp+38h], edi

    mov     edx, ecx
    inc     edx
    cmp     edx, esi
    jb      @no_wrap_g
    xor     edx, edx
@no_wrap_g:
    mov     r8d, REDUCE_BUF_SIZE
    call    Swarm_P2PCopy

    mov     ecx, [rsp+20h]
    mov     ebx, [rsp+28h]
    mov     esi, [rsp+30h]
    mov     edi, [rsp+38h]

    inc     ecx
    jmp     @gather_inner

@gather_next:
    inc     ebx
    jmp     @gather_loop

@reduce_done:
    mov     g_reducePhase, 0

    mov     ecx, COORD_BEACON_SLOT
    mov     edx, COORD_EVT_REDUCE_OK
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    jmp     @reduce_ret

@reduce_trivial:
    xor     eax, eax

@reduce_ret:
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SwarmCoord_AllReduce ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmCoord_GetWorkerStatus — Get status of a specific worker
;   ECX = workerIdx
;   Returns EAX = worker state
;   Leaf — no FRAME.
; ════════════════════════════════════════════════════════════════════
SwarmCoord_GetWorkerStatus PROC
    cmp     ecx, g_workerCount
    jge     @status_invalid
    lea     r10, g_workerState
    imul    eax, ecx, WORKER_STATE_SIZE
    cdqe
    mov     eax, dword ptr [r10 + rax]
    ret
@status_invalid:
    mov     eax, -1
    ret
SwarmCoord_GetWorkerStatus ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmCoord_Shutdown — Stop all workers, clean up
;   FRAME: 3 pushes (rbp,rbx,rsi) + 30h alloc = 80. Good.
; ════════════════════════════════════════════════════════════════════
SwarmCoord_Shutdown PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     g_coordReady, 0

    mov     esi, g_workerCount
    xor     ebx, ebx

@wait_workers:
    cmp     ebx, esi
    jge     @wait_hb

    lea     r10, g_workerThreads
    mov     rcx, [r10 + rbx*8]
    test    rcx, rcx
    jz      @next_wait
    mov     edx, 2000
    call    WaitForSingleObject

    lea     r10, g_workerThreads
    mov     rcx, [r10 + rbx*8]
    call    CloseHandle
    lea     r10, g_workerThreads
    mov     qword ptr [r10 + rbx*8], 0

@next_wait:
    inc     ebx
    jmp     @wait_workers

@wait_hb:
    mov     rcx, g_heartbeatThread
    test    rcx, rcx
    jz      @coord_cleanup
    mov     edx, 2000
    call    WaitForSingleObject
    mov     rcx, g_heartbeatThread
    call    CloseHandle
    mov     g_heartbeatThread, 0

@coord_cleanup:
    mov     g_workerCount, 0
    mov     g_workQueueHead, 0
    mov     g_workQueueTail, 0
    mov     g_workQueueCount, 0
    mov     g_reducePhase, 0
    mov     g_reduceComplete, 0

    mov     ecx, COORD_BEACON_SLOT
    mov     edx, COORD_EVT_SHUTDOWN
    xor     r8d, r8d
    call    BeaconSend

    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SwarmCoord_Shutdown ENDP

END
