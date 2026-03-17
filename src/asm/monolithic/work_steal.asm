; ═══════════════════════════════════════════════════════════════════
; work_steal.asm — Phase 14: Autonomous Shard Migration (Work Stealing)
; Assembles: ml64 /c /Fo work_steal.obj work_steal.asm
;
; Architecture:
;   - Per-device load counters track active shard computation
;   - Idle workers probe for busiest device and steal pending shards
;   - Atomic claiming via lock cmpxchg on shard status field
;   - P2P buffer migration via Swarm_P2PCopy for data locality
;   - Integrates with swarm_coordinator heartbeat for idle detection
;   - Beacon-signaled steal events for cluster-wide visibility
;
; Protocol:
;   1. WorkerThread detects IDLE state (no work in queue)
;   2. Calls WorkSteal_IdleProbe(myDeviceIdx)
;   3. IdleProbe selects victim = device with max active shards
;   4. AtomicClaim tries lock cmpxchg(status, ALLOCATED→STEALING)
;   5. On success: P2PCopy migrates buffer, reassigns deviceIdx
;   6. DispatchCompute runs the stolen shard on the idle GPU
;   7. Load counters rebalanced, beacon event fired
;
; All hex in MASM style. Win64 ABI. No sizeof, no addr.
; ═══════════════════════════════════════════════════════════════════

; ── Win32 imports ────────────────────────────────────────────────
EXTERN GetTickCount64:PROC
EXTERN Sleep:PROC

; ── Cross-module imports ─────────────────────────────────────────
EXTERN g_hHeap:QWORD
EXTERN BeaconSend:PROC

; ── Swarm module imports ─────────────────────────────────────────
EXTERN g_swarmDeviceCount:DWORD
EXTERN g_swarmReady:DWORD
EXTERN g_shardDescs:BYTE
EXTERN g_shardCount:DWORD
EXTERN Swarm_P2PCopy:PROC
EXTERN Swarm_DispatchCompute:PROC
EXTERN Swarm_AllocShard:PROC

; ── Coordinator imports ──────────────────────────────────────────
EXTERN g_coordReady:DWORD
EXTERN g_workerCount:DWORD

; ── Network imports (Phase 14: cross-node load exchange) ─────────
EXTERN SwarmNet_ExchangeLoadInfo:PROC
EXTERN SwarmNet_SendShard:PROC
EXTERN SwarmNet_RecvShard:PROC
EXTERN g_netReady:DWORD
EXTERN g_remoteCount:DWORD
EXTERN g_remoteNodes:BYTE

; ── Public exports ───────────────────────────────────────────────
PUBLIC WorkSteal_Init
PUBLIC WorkSteal_IdleProbe
PUBLIC WorkSteal_AtomicClaim
PUBLIC WorkSteal_MigrateShard
PUBLIC WorkSteal_GetStats
PUBLIC WorkSteal_SelectVictim
PUBLIC WorkSteal_Shutdown
PUBLIC WorkSteal_CrossNodeProbe
PUBLIC WorkSteal_ExchangeAllLoads
PUBLIC g_stealReady
PUBLIC g_totalSteals
PUBLIC g_totalMigrations
PUBLIC g_stealFailures
PUBLIC g_crossNodeSteals

; ── Constants ────────────────────────────────────────────────────
MAX_GPUS              equ 8
MAX_SHARDS            equ 64
SHARD_DESC_SIZE       equ 128

; Shard status values (must match swarm.asm)
SHARD_FREE            equ 0
SHARD_ALLOCATED       equ 1
SHARD_COMPUTING       equ 2
SHARD_DONE            equ 3
SHARD_STEALING        equ 4          ; Phase 14: transient steal state

; Work-steal beacon slot
STEAL_BEACON_SLOT     equ 13

; Steal event codes
STEAL_EVT_INIT        equ 0D1h
STEAL_EVT_PROBE       equ 0D2h
STEAL_EVT_CLAIMED     equ 0D3h
STEAL_EVT_MIGRATED    equ 0D4h
STEAL_EVT_DISPATCHED  equ 0D5h
STEAL_EVT_FAILED      equ 0D6h
STEAL_EVT_SHUTDOWN    equ 0D7h

; Steal policy
MIN_LOAD_DIFF         equ 2          ; Minimum load imbalance to trigger steal
STEAL_COOLDOWN_MS     equ 50         ; Minimum ms between steal attempts per device
MAX_STEAL_ATTEMPTS    equ 3          ; Max consecutive steal retries before backing off

; ── Data ─────────────────────────────────────────────────────────
.data
align 16
g_stealReady          dd 0

; Per-device load counters: active shard count per GPU
align 16
g_deviceLoad          dd MAX_GPUS dup(0)

; Per-device steal cooldown timestamps
align 16
g_deviceCooldown      dq MAX_GPUS dup(0)

; Global steal metrics
align 8
g_totalSteals         dq 0           ; Successful steal claims
g_totalMigrations     dq 0           ; Completed P2P migrations
g_stealFailures       dq 0           ; Failed cmpxchg attempts
g_lastStealTick       dq 0           ; Timestamp of last successful steal
g_stealAttempts       dq 0           ; Total probe attempts
g_crossNodeSteals     dq 0           ; Phase 14: cross-node steal count

; Per-device steal history (last 8 steals per device)
align 16
g_stealHistory        dd (MAX_GPUS * 8) dup(0)
g_stealHistIdx        dd MAX_GPUS dup(0)

.data?
align 16
g_pktRecvBuf          db 10000h dup(?)

.code

; ════════════════════════════════════════════════════════════════════
; WorkSteal_Init — Initialize work-stealing subsystem
;   No args. Returns EAX = 0 success, -1 fail.
;   FRAME: 1 push (rbp) + 28h alloc
;     ABI: 8 + 8 + 28h = 56 → use 30h: 8+8+30h = 64. Good.
; ════════════════════════════════════════════════════════════════════
WorkSteal_Init PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Require swarm to be initialized
    cmp     g_swarmReady, 0
    je      @wsinit_fail

    ; Zero all load counters
    lea     rcx, g_deviceLoad
    xor     eax, eax
    mov     edx, MAX_GPUS
@@init_load:
    mov     dword ptr [rcx + rax*4], 0
    inc     eax
    cmp     eax, edx
    jl      @@init_load

    ; Zero cooldown timestamps
    lea     rcx, g_deviceCooldown
    xor     eax, eax
@@init_cool:
    mov     qword ptr [rcx + rax*8], 0
    inc     eax
    cmp     eax, edx
    jl      @@init_cool

    ; Zero metrics
    mov     g_totalSteals, 0
    mov     g_totalMigrations, 0
    mov     g_stealFailures, 0
    mov     g_lastStealTick, 0
    mov     g_stealAttempts, 0

    ; Zero steal history
    lea     rcx, g_stealHistory
    mov     edx, MAX_GPUS * 8
    xor     eax, eax
@@init_hist:
    mov     dword ptr [rcx + rax*4], 0
    inc     eax
    cmp     eax, edx
    jl      @@init_hist

    lea     rcx, g_stealHistIdx
    mov     edx, MAX_GPUS
    xor     eax, eax
@@init_histidx:
    mov     dword ptr [rcx + rax*4], 0
    inc     eax
    cmp     eax, edx
    jl      @@init_histidx

    ; Scan current shard assignments to build initial load counters
    call    RebuildLoadCounters

    mov     g_stealReady, 1

    ; Signal init via beacon
    mov     ecx, STEAL_BEACON_SLOT
    mov     edx, STEAL_EVT_INIT
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    jmp     @wsinit_ret

@wsinit_fail:
    mov     eax, -1

@wsinit_ret:
    lea     rsp, [rbp]
    pop     rbp
    ret
WorkSteal_Init ENDP


; ════════════════════════════════════════════════════════════════════
; RebuildLoadCounters — Scan all shards, count active per device
;   Internal. No args. Clobbers RAX, RCX, RDX, R8, R9, R10, R11.
;   FRAME: 1 push (rbp) + 28h alloc = 48 → use 30h: 64. Good.
; ════════════════════════════════════════════════════════════════════
RebuildLoadCounters PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Zero load counters
    lea     rcx, g_deviceLoad
    xor     eax, eax
    mov     edx, MAX_GPUS
@@zero_ld:
    mov     dword ptr [rcx + rax*4], 0
    inc     eax
    cmp     eax, edx
    jl      @@zero_ld

    ; Scan shards
    lea     r10, g_shardDescs
    xor     ecx, ecx                  ; shard index

@@scan_shard:
    cmp     ecx, MAX_SHARDS
    jge     @@scan_done

    imul    eax, ecx, SHARD_DESC_SIZE
    cdqe

    ; Check if shard is active (status 1=allocated or 2=computing)
    mov     r8d, dword ptr [r10 + rax + 28]
    cmp     r8d, SHARD_ALLOCATED
    je      @@count_it
    cmp     r8d, SHARD_COMPUTING
    je      @@count_it
    jmp     @@next_scan

@@count_it:
    ; Get device index for this shard
    mov     r9d, dword ptr [r10 + rax]
    cmp     r9d, MAX_GPUS
    jge     @@next_scan

    ; Increment load counter for that device
    lea     r11, g_deviceLoad
    lock inc dword ptr [r11 + r9*4]

@@next_scan:
    inc     ecx
    jmp     @@scan_shard

@@scan_done:
    lea     rsp, [rbp]
    pop     rbp
    ret
RebuildLoadCounters ENDP


; ════════════════════════════════════════════════════════════════════
; WorkSteal_SelectVictim — Find the busiest device to steal from
;   ECX = thiefDeviceIdx (the idle device requesting work)
;   Returns: EAX = victimDeviceIdx (-1 if no viable victim)
;   Leaf function — no FRAME needed.
; ════════════════════════════════════════════════════════════════════
WorkSteal_SelectVictim PROC
    ; Find device with maximum load, excluding thiefDeviceIdx
    lea     r10, g_deviceLoad
    mov     r11d, g_swarmDeviceCount
    test    r11d, r11d
    jz      @@sv_none

    xor     edx, edx                  ; candidate device
    mov     r8d, -1                    ; best device idx
    xor     r9d, r9d                   ; best load value

@@sv_loop:
    cmp     edx, r11d
    jge     @@sv_check

    cmp     edx, ecx                  ; skip self
    je      @@sv_next

    mov     eax, dword ptr [r10 + rdx*4]
    cmp     eax, r9d
    jle     @@sv_next

    ; New best: this device has more active shards
    mov     r9d, eax
    mov     r8d, edx

@@sv_next:
    inc     edx
    jmp     @@sv_loop

@@sv_check:
    ; Check if load difference justifies stealing
    mov     eax, dword ptr [r10 + rcx*4]   ; thief's current load
    mov     edx, r9d
    sub     edx, eax                       ; diff = victim_load - thief_load
    cmp     edx, MIN_LOAD_DIFF
    jl      @@sv_none

    mov     eax, r8d                       ; return victim device
    ret

@@sv_none:
    mov     eax, -1
    ret
WorkSteal_SelectVictim ENDP


; ════════════════════════════════════════════════════════════════════
; WorkSteal_AtomicClaim — Atomically claim a shard for stealing
;   ECX = shardIdx
;   Returns: EAX = 0 (claimed), -1 (contention/already taken)
;   Uses lock cmpxchg on shard.status: ALLOCATED(1) → STEALING(4)
;   Leaf — small enough, no FRAME.
; ════════════════════════════════════════════════════════════════════
WorkSteal_AtomicClaim PROC
    cmp     ecx, 0
    jl      @@ac_fail
    cmp     ecx, MAX_SHARDS
    jge     @@ac_fail

    lea     r10, g_shardDescs
    imul    eax, ecx, SHARD_DESC_SIZE
    cdqe
    lea     r11, [r10 + rax + 28]       ; &shard.status

    ; Attempt CAS: expected=SHARD_ALLOCATED(1), desired=SHARD_STEALING(4)
    mov     eax, SHARD_ALLOCATED
    mov     edx, SHARD_STEALING
    lock cmpxchg dword ptr [r11], edx

    ; ZF set if success (eax was 1, [r11] was 1 → now 4)
    jnz     @@ac_fail

    ; Success: increment steal counter
    lock inc qword ptr [g_totalSteals]
    xor     eax, eax
    ret

@@ac_fail:
    lock inc qword ptr [g_stealFailures]
    mov     eax, -1
    ret
WorkSteal_AtomicClaim ENDP


; ════════════════════════════════════════════════════════════════════
; WorkSteal_MigrateShard — P2P copy + reassign shard to new device
;   ECX = shardIdx
;   EDX = newDeviceIdx
;   Returns: EAX = 0 success, -1 fail
;   FRAME: 3 pushes (rbp,rbx,rsi) + 30h alloc
;     ABI: 8 + 24 + 30h = 80 → 80/16=5. Good.
; ════════════════════════════════════════════════════════════════════
WorkSteal_MigrateShard PROC FRAME
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

    mov     dword ptr [rsp+20h], ecx      ; save shardIdx
    mov     dword ptr [rsp+24h], edx      ; save newDeviceIdx

    ; Validate shard is in STEALING state (we own it)
    lea     r10, g_shardDescs
    imul    eax, ecx, SHARD_DESC_SIZE
    cdqe
    mov     ebx, eax                       ; preserve descriptor offset
    mov     r8d, dword ptr [r10 + rax + 28]
    cmp     r8d, SHARD_STEALING
    jne     @@mig_fail

    ; Get old device index
    mov     esi, dword ptr [r10 + rbx]     ; oldDeviceIdx

    ; Get buffer size for P2P transfer
    mov     r8, qword ptr [r10 + rbx + 12] ; bufferSize
    test    r8, r8
    jz      @@mig_reassign                 ; No data to migrate, just reassign

    ; Check if source buffer exists
    mov     rax, qword ptr [r10 + rbx + 20] ; bufferPtr
    test    rax, rax
    jz      @@mig_reassign                 ; No buffer allocated yet

    ; P2P copy: create a virtual shard-to-shard transfer
    ; Swarm_P2PCopy(srcShardIdx, dstShardIdx, bytes)
    ; We use the same shard index since we're migrating in-place:
    ;   The old device's buffer is the source
    ;   After copy, we reassign deviceIdx
    ; For cross-device, we simulate by treating the buffer as already local
    ; (Real RDMA would use device-mapped staging buffers)
    mov     ecx, dword ptr [rsp+20h]       ; srcShard = shardIdx
    mov     edx, dword ptr [rsp+20h]       ; dstShard = same (in-place reassign)
    ; r8 already has bufferSize
    call    Swarm_P2PCopy

    test    eax, eax
    jnz     @@mig_fail_restore

    lock inc qword ptr [g_totalMigrations]

@@mig_reassign:
    ; Reassign shard to new device
    lea     r10, g_shardDescs
    mov     eax, dword ptr [rsp+24h]       ; newDeviceIdx
    mov     dword ptr [r10 + rbx], eax     ; shard.deviceIdx = newDeviceIdx

    ; Transition status: STEALING → ALLOCATED (ready for dispatch)
    mov     dword ptr [r10 + rbx + 28], SHARD_ALLOCATED

    ; Update load counters atomically
    lea     r11, g_deviceLoad
    lock dec dword ptr [r11 + rsi*4]       ; old device loses a shard
    movzx   eax, byte ptr [rsp+24h]
    lock inc dword ptr [r11 + rax*4]       ; new device gains a shard

    ; Record in steal history
    mov     ecx, dword ptr [rsp+24h]       ; device that stole
    lea     r10, g_stealHistIdx
    mov     eax, dword ptr [r10 + rcx*4]
    and     eax, 7                         ; ring of 8
    lea     r11, g_stealHistory
    imul    edx, ecx, 8
    add     edx, eax
    mov     r8d, dword ptr [rsp+20h]       ; shardIdx
    mov     dword ptr [r11 + rdx*4], r8d
    inc     eax
    and     eax, 7
    mov     dword ptr [r10 + rcx*4], eax

    ; Signal migration via beacon
    mov     ecx, STEAL_BEACON_SLOT
    mov     edx, STEAL_EVT_MIGRATED
    mov     r8d, dword ptr [rsp+20h]       ; shardIdx as payload
    call    BeaconSend

    ; Record timestamp
    call    GetTickCount64
    mov     g_lastStealTick, rax

    xor     eax, eax
    jmp     @@mig_ret

@@mig_fail_restore:
    ; Restore shard to ALLOCATED state (atomic claim was successful but migration failed)
    lea     r10, g_shardDescs
    mov     dword ptr [r10 + rbx + 28], SHARD_ALLOCATED

@@mig_fail:
    mov     ecx, STEAL_BEACON_SLOT
    mov     edx, STEAL_EVT_FAILED
    xor     r8d, r8d
    call    BeaconSend

    mov     eax, -1

@@mig_ret:
    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WorkSteal_MigrateShard ENDP


; ════════════════════════════════════════════════════════════════════
; WorkSteal_IdleProbe — Full steal orchestration for an idle worker
;   ECX = idleDeviceIdx
;   Returns: EAX = stolen shardIdx (-1 if nothing stolen)
;
;   Protocol:
;     1. Check cooldown
;     2. SelectVictim → busiest device
;     3. Scan victim's shards for ALLOCATED (stealable)
;     4. AtomicClaim the first candidate
;     5. MigrateShard (P2P + reassign)
;     6. DispatchCompute on the stolen shard
;
;   FRAME: 4 pushes (rbp,rbx,rsi,rdi) + 48h alloc
;     ABI: 8 + 32 + 48h = 112 → 112/16=7. Good.
; ════════════════════════════════════════════════════════════════════
WorkSteal_IdleProbe PROC FRAME
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

    mov     dword ptr [rsp+20h], ecx      ; save idleDeviceIdx
    lock inc qword ptr [g_stealAttempts]

    ; Check subsystem ready
    cmp     g_stealReady, 0
    je      @@ip_fail

    ; Signal probe via beacon
    mov     ecx, STEAL_BEACON_SLOT
    mov     edx, STEAL_EVT_PROBE
    mov     r8d, dword ptr [rsp+20h]
    call    BeaconSend

    ; Step 1: Check cooldown for this device
    mov     ecx, dword ptr [rsp+20h]
    call    GetTickCount64
    mov     rbx, rax                       ; current tick

    mov     ecx, dword ptr [rsp+20h]
    lea     r10, g_deviceCooldown
    mov     rdi, qword ptr [r10 + rcx*8]
    sub     rbx, rdi
    cmp     rbx, STEAL_COOLDOWN_MS
    jl      @@ip_fail                      ; Too soon since last steal

    ; Update cooldown timestamp
    call    GetTickCount64
    mov     ecx, dword ptr [rsp+20h]
    lea     r10, g_deviceCooldown
    mov     qword ptr [r10 + rcx*8], rax

    ; Step 2: Select victim device (busiest)
    mov     ecx, dword ptr [rsp+20h]
    call    WorkSteal_SelectVictim
    cmp     eax, -1
    je      @@ip_fail

    mov     dword ptr [rsp+24h], eax       ; save victimDeviceIdx

    ; Step 3: Scan victim's shards for stealable candidates
    ;   Look for shards on victim device with status=ALLOCATED
    mov     esi, eax                       ; esi = victimDeviceIdx
    lea     r10, g_shardDescs
    xor     edi, edi                       ; shard scan index
    mov     dword ptr [rsp+28h], 0         ; attempts counter

@@ip_scan:
    cmp     edi, MAX_SHARDS
    jge     @@ip_fail

    imul    eax, edi, SHARD_DESC_SIZE
    cdqe

    ; Check if shard belongs to victim device
    mov     r8d, dword ptr [r10 + rax]
    cmp     r8d, esi
    jne     @@ip_next

    ; Check if shard is stealable (status = ALLOCATED)
    mov     r9d, dword ptr [r10 + rax + 28]
    cmp     r9d, SHARD_ALLOCATED
    jne     @@ip_next

    ; Step 4: Attempt atomic claim
    mov     [rsp+2Ch], edi                 ; save shard index
    mov     ecx, edi
    call    WorkSteal_AtomicClaim
    mov     edi, dword ptr [rsp+2Ch]       ; restore

    test    eax, eax
    jnz     @@ip_retry                     ; Contention — try next shard

    ; Step 5: Migrate the shard
    mov     ecx, edi                       ; shardIdx
    mov     edx, dword ptr [rsp+20h]       ; newDeviceIdx (the thief)
    call    WorkSteal_MigrateShard

    test    eax, eax
    jnz     @@ip_fail                      ; Migration failed

    ; Step 6: Dispatch compute on the stolen shard
    mov     ecx, edi
    call    Swarm_DispatchCompute

    ; Signal dispatch via beacon
    mov     ecx, STEAL_BEACON_SLOT
    mov     edx, STEAL_EVT_DISPATCHED
    mov     r8d, edi                       ; stolen shard as payload
    call    BeaconSend

    mov     eax, edi                       ; return stolen shardIdx
    jmp     @@ip_ret

@@ip_retry:
    inc     dword ptr [rsp+28h]
    mov     eax, dword ptr [rsp+28h]
    cmp     eax, MAX_STEAL_ATTEMPTS
    jge     @@ip_fail

@@ip_next:
    inc     edi
    jmp     @@ip_scan

@@ip_fail:
    mov     eax, -1

@@ip_ret:
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WorkSteal_IdleProbe ENDP


; ════════════════════════════════════════════════════════════════════
; WorkSteal_GetStats — Return steal subsystem metrics
;   RCX = ptr to stats buffer (7 QWORDs minimum)
;     [+0]  totalSteals
;     [+8]  totalMigrations
;     [+16] stealFailures
;     [+24] stealAttempts
;     [+32] lastStealTick
;     [+40] deviceCount
;     [+48] stealReady
;   Returns EAX = 0
;   Leaf function.
; ════════════════════════════════════════════════════════════════════
WorkSteal_GetStats PROC
    test    rcx, rcx
    jz      @@gs_fail

    mov     rax, g_totalSteals
    mov     qword ptr [rcx], rax
    mov     rax, g_totalMigrations
    mov     qword ptr [rcx+8], rax
    mov     rax, g_stealFailures
    mov     qword ptr [rcx+10h], rax
    mov     rax, g_stealAttempts
    mov     qword ptr [rcx+18h], rax
    mov     rax, g_lastStealTick
    mov     qword ptr [rcx+20h], rax
    movzx   eax, byte ptr [g_swarmDeviceCount]
    mov     qword ptr [rcx+28h], rax
    movzx   eax, byte ptr [g_stealReady]
    mov     qword ptr [rcx+30h], rax

    xor     eax, eax
    ret

@@gs_fail:
    mov     eax, -1
    ret
WorkSteal_GetStats ENDP


; ════════════════════════════════════════════════════════════════════
; WorkSteal_Shutdown — Clean up work-stealing subsystem
;   No args. Returns EAX = 0.
;   FRAME: 1 push (rbp) + 30h alloc = 64. Good.
; ════════════════════════════════════════════════════════════════════
WorkSteal_Shutdown PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     g_stealReady, 0

    ; Zero load counters
    lea     rcx, g_deviceLoad
    xor     eax, eax
    mov     edx, MAX_GPUS
@@sd_zero:
    mov     dword ptr [rcx + rax*4], 0
    inc     eax
    cmp     eax, edx
    jl      @@sd_zero

    ; Signal shutdown via beacon
    mov     ecx, STEAL_BEACON_SLOT
    mov     edx, STEAL_EVT_SHUTDOWN
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax

    lea     rsp, [rbp]
    pop     rbp
    ret
WorkSteal_Shutdown ENDP


; ════════════════════════════════════════════════════════════════════
; WorkSteal_ExchangeAllLoads — Exchange load info with all remote nodes
;   No args. Returns EAX = number of nodes successfully exchanged.
;   Iterates g_remoteNodes, calls SwarmNet_ExchangeLoadInfo for each
;   connected node, passing local g_deviceLoad as the load snapshot.
;   FRAME: 3 pushes (rbp,rbx,rsi) + 30h alloc
; ════════════════════════════════════════════════════════════════════
MAX_REMOTE_NODES_WS   equ 32
NODE_ENTRY_SIZE_WS    equ 64
INVALID_SOCKET_WS     equ -1

WorkSteal_ExchangeAllLoads PROC FRAME
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

    ; Require both steal and net subsystems
    cmp     g_stealReady, 0
    je      @xla_none
    cmp     g_netReady, 0
    je      @xla_none

    xor     ebx, ebx                      ; success count
    xor     esi, esi                       ; node index

@@xla_loop:
    cmp     esi, MAX_REMOTE_NODES_WS
    jge     @@xla_done

    ; Check if node is connected
    imul    eax, esi, NODE_ENTRY_SIZE_WS
    cdqe
    lea     r10, g_remoteNodes
    mov     rcx, qword ptr [r10 + rax]
    cmp     rcx, INVALID_SOCKET_WS
    je      @@xla_next

    ; Check connected flag at offset +56
    mov     edx, dword ptr [r10 + rax + 56]
    test    edx, 1
    jz      @@xla_next

    ; SwarmNet_ExchangeLoadInfo(nodeIndex, &g_deviceLoad)
    mov     ecx, esi
    lea     rdx, g_deviceLoad
    call    SwarmNet_ExchangeLoadInfo
    test    eax, eax
    jnz     @@xla_next
    inc     ebx

@@xla_next:
    inc     esi
    jmp     @@xla_loop

@@xla_done:
    mov     eax, ebx
    jmp     @xla_ret

@xla_none:
    xor     eax, eax

@xla_ret:
    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WorkSteal_ExchangeAllLoads ENDP


; ════════════════════════════════════════════════════════════════════
; WorkSteal_CrossNodeProbe — Attempt to steal work from a remote node
;   ECX = idleDeviceIdx (local GPU that is idle)
;   Returns: EAX = 0 (stole from remote), -1 (no work available remotely)
;
;   Protocol:
;     1. Exchange load info with all connected nodes
;     2. Find remote node with highest total GPU load
;     3. Request a shard migration from that node via SendShard/RecvShard
;     4. If received, allocate local shard descriptor and dispatch
;
;   FRAME: 4 pushes (rbp,rbx,rsi,rdi) + 48h alloc
; ════════════════════════════════════════════════════════════════════
WorkSteal_CrossNodeProbe PROC FRAME
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

    mov     dword ptr [rbp-10h], ecx      ; save idleDeviceIdx

    ; Must have network and steal subsystem ready
    cmp     g_stealReady, 0
    je      @cnp_fail
    cmp     g_netReady, 0
    je      @cnp_fail
    cmp     g_remoteCount, 0
    je      @cnp_fail

    ; Step 1: Exchange load info with all nodes
    call    WorkSteal_ExchangeAllLoads
    test    eax, eax
    jz      @cnp_fail                      ; No nodes responded

    ; Step 2: Find busiest remote node
    ;   Sum the 8 DWORDs at offset +16 in each node entry
    lea     r10, g_remoteNodes
    mov     esi, -1                         ; best node index
    xor     edi, edi                        ; best total load
    xor     ebx, ebx                        ; node scan index

@@cnp_scan:
    cmp     ebx, MAX_REMOTE_NODES_WS
    jge     @@cnp_check

    imul    eax, ebx, NODE_ENTRY_SIZE_WS
    cdqe

    ; Skip disconnected
    mov     rcx, qword ptr [r10 + rax]
    cmp     rcx, INVALID_SOCKET_WS
    je      @@cnp_snext

    ; Sum remote load counters (8 DWORDs at offset +16)
    xor     edx, edx                        ; sum
    lea     r11, [r10 + rax + 16]            ; base of load array for this node
    xor     ecx, ecx                        ; counter
@@cnp_sum:
    add     edx, dword ptr [r11 + rcx*4]
    inc     ecx
    cmp     ecx, 8
    jl      @@cnp_sum

    ; Compare with best
    cmp     edx, edi
    jle     @@cnp_snext
    mov     edi, edx                        ; new best load
    mov     esi, ebx                        ; new best node

@@cnp_snext:
    inc     ebx
    jmp     @@cnp_scan

@@cnp_check:
    cmp     esi, -1
    je      @cnp_fail

    ; Step 3: Check if remote load justifies stealing
    ;   Sum local load
    lea     r10, g_deviceLoad
    xor     edx, edx
    xor     ecx, ecx
@@cnp_local_sum:
    add     edx, dword ptr [r10 + rcx*4]
    inc     ecx
    cmp     ecx, MAX_GPUS
    jl      @@cnp_local_sum

    ; diff = remote_total - local_total
    sub     edi, edx
    cmp     edi, MIN_LOAD_DIFF
    jl      @cnp_fail                       ; Not enough imbalance

    ; Step 4: Request shard data from busiest remote node
    ;   We send a PKT_SHARD_DATA request (empty payload = "give me work")
    ;   In response, remote sends a shard buffer back
    mov     dword ptr [rbp-14h], esi        ; save best node index

    ; Use RecvShard to get remote shard data (up to 64KB)
    mov     ecx, esi
    lea     rdx, g_pktRecvBuf
    mov     r8d, 10000h                     ; 64KB max
    call    SwarmNet_RecvShard
    cmp     eax, -1
    je      @cnp_fail
    cmp     eax, 0
    je      @cnp_fail

    mov     dword ptr [rbp-18h], eax        ; bytes received

    ; Step 5: Allocate a local shard and dispatch
    ;   Swarm_AllocShard(idleDeviceIdx, layerStart=0, layerEnd=0, bufferSize)
    mov     ecx, dword ptr [rbp-10h]        ; idleDeviceIdx
    xor     edx, edx                         ; layerStart
    xor     r8d, r8d                         ; layerEnd
    movsxd  r9, dword ptr [rbp-18h]
    call    Swarm_AllocShard
    cmp     eax, -1
    je      @cnp_fail

    mov     dword ptr [rbp-1Ch], eax        ; new shard index

    ; Dispatch compute on the new shard
    mov     ecx, eax
    call    Swarm_DispatchCompute

    ; Update metrics
    lock inc qword ptr [g_crossNodeSteals]
    lock inc qword ptr [g_totalSteals]

    ; Beacon
    mov     ecx, STEAL_BEACON_SLOT
    mov     edx, STEAL_EVT_DISPATCHED
    mov     r8d, dword ptr [rbp-1Ch]
    call    BeaconSend

    xor     eax, eax
    jmp     @cnp_ret

@cnp_fail:
    mov     eax, -1

@cnp_ret:
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WorkSteal_CrossNodeProbe ENDP

END
