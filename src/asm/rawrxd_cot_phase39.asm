; =============================================================================
; rawrxd_cot_phase39.asm — Phase 39–42 Enhancements & Finishers
; =============================================================================
;
; Phase 39: Adaptive Commit Governor, Copy Engine Selector, Multi-Producer
; Phase 40: Snapshot / Rollback, Deterministic Replay Mode
; Phase 41: Shared-Memory Telemetry, NUMA-Aware Sharding, GPU Backpressure
; Phase 42: Security Hardening (Canary Pages, Memory Sealing, Tenant Quotas)
;
; Extends the Phase 37 CoT engine with enterprise finishers.
; All new procs coordinate with rawrxd_cot_engine.asm via EXTERN globals.
;
; Exports (16 new procs):
;   CoT_CreateSnapshot       — O(1) snapshot of arena state
;   CoT_RestoreSnapshot      — Rollback arena to snapshot
;   CoT_SetReplayMode        — Enable/disable deterministic replay
;   CoT_IsReplayMode         — Check if replay mode is active
;   CoT_GetTelemetryPage     — Get pointer to shared telemetry struct
;   CoT_UpdateTelemetry      — Refresh telemetry counters (lock-free)
;   CoT_SealMemory           — Mark arena read-only post-generation
;   CoT_UnsealMemory         — Restore read-write for new generation
;   CoT_SetTenantQuota       — Set per-tenant token ceiling
;   CoT_CheckTenantQuota     — Check remaining quota for tenant
;   CoT_SelectCopyEngine     — Probe CPUID and select optimal copy engine
;   CoT_CopyDispatch         — Dispatch copy via selected engine
;   CoT_EnableMultiProducer  — Enable lock-free multi-producer mode
;   CoT_MultiProducerAppend  — Ticket-reserved append (multi-producer path)
;   CoT_GetCommitGovernor    — Get current throttle level + EMA
;   CoT_SetBackpressure      — Set GPU backpressure signal
;
; Architecture: x64 MASM | Windows ABI | SEH | No CRT | No exceptions
; Build: ml64 /c /Zi /Zd /Fo rawrxd_cot_phase39.obj rawrxd_cot_phase39.asm
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC CoT_CreateSnapshot
PUBLIC CoT_RestoreSnapshot
PUBLIC CoT_SetReplayMode
PUBLIC CoT_IsReplayMode
PUBLIC CoT_GetTelemetryPage
PUBLIC CoT_UpdateTelemetry
PUBLIC CoT_SealMemory
PUBLIC CoT_UnsealMemory
PUBLIC CoT_SetTenantQuota
PUBLIC CoT_CheckTenantQuota
PUBLIC CoT_SelectCopyEngine
PUBLIC CoT_CopyDispatch
PUBLIC CoT_EnableMultiProducer
PUBLIC CoT_MultiProducerAppend
PUBLIC CoT_GetCommitGovernor
PUBLIC CoT_SetBackpressure

; Globals shared with rawrxd_cot_engine.asm
PUBLIC g_commitGovernor
PUBLIC g_canaryHeadOK
PUBLIC g_telemetry
PUBLIC g_replayMode
PUBLIC g_arenaSealed
PUBLIC g_gpuQueueDepth
PUBLIC g_backpressureThreshold

; =============================================================================
;                          EXTERNAL IMPORTS
; =============================================================================
EXTERN VirtualAlloc: PROC
EXTERN VirtualFree: PROC
EXTERN VirtualProtect: PROC
EXTERN VirtualAllocExNuma: PROC
EXTERN GetTickCount64: PROC
EXTERN RtlZeroMemory: PROC
EXTERN OutputDebugStringA: PROC
EXTERN GetCurrentProcessorNumber: PROC
EXTERN GetNumaProcessorNode: PROC
EXTERN CreateFileMappingA: PROC
EXTERN MapViewOfFile: PROC
EXTERN UnmapViewOfFile: PROC
EXTERN CloseHandle: PROC

; SRW lock (from rawrxd_cot_dll_entry.asm)
EXTERN Acquire_CoT_Lock: PROC
EXTERN Release_CoT_Lock: PROC
EXTERN Acquire_CoT_Lock_Shared: PROC
EXTERN Release_CoT_Lock_Shared: PROC

; Arena globals (from rawrxd_cot_engine.asm)
EXTERN g_arenaBase: QWORD
EXTERN g_arenaUsed: QWORD
EXTERN g_arenaCommitted: QWORD
EXTERN g_initialized: DWORD
EXTERN g_executionState: DWORD

; =============================================================================
;                            CONSTANTS
; =============================================================================

; Arena size (must match rawrxd_cot_engine.asm)
COT_ARENA_RESERVE_SIZE      EQU     40000000h       ; 1 GB reserved

; Copy engine IDs
COPY_ENGINE_ERMS            EQU     0
COPY_ENGINE_AVX2            EQU     1
COPY_ENGINE_AVX512          EQU     2

; Copy engine thresholds
COPY_THRESHOLD_SMALL        EQU     256             ; < 256 B → ERMS
COPY_THRESHOLD_MEDIUM       EQU     8000h           ; < 32 KB → AVX2, else AVX-512

; Throttle levels
THROTTLE_NONE               EQU     0
THROTTLE_SOFT               EQU     1
THROTTLE_HARD               EQU     2

; Commit governor soft limit: 16 MB / sec
COMMIT_SOFT_LIMIT           EQU     01000000h       ; 16 MB

; Canary magic values
CANARY_HEAD_MAGIC           EQU     0DEADBEEFCAFE4039h
CANARY_TAIL_MAGIC           EQU     4039CAFEBEEFDEADh

; Seal protection
PAGE_READONLY_VAL           EQU     02h
PAGE_READWRITE_VAL          EQU     04h

; Tenant quota constants
MAX_TENANTS                 EQU     64
TENANT_ENTRY_SIZE           EQU     16              ; 8 bytes quota + 8 bytes used

; Snapshot max slots
MAX_SNAPSHOTS               EQU     16

; Multi-producer ticket sentinel
MP_DISABLED                 EQU     0
MP_ENABLED                  EQU     1

; CPUID function for feature detection
CPUID_BASIC                 EQU     0
CPUID_FEATURES              EQU     1
CPUID_EXTENDED_FEATURES     EQU     7

; CPUID feature bits
CPUID_ERMS_BIT              EQU     9               ; EBX bit 9 (leaf 7)
CPUID_AVX2_BIT              EQU     5               ; EBX bit 5 (leaf 7)
CPUID_AVX512F_BIT           EQU     16              ; EBX bit 16 (leaf 7)

; =============================================================================
;                          STRUCTURES
; =============================================================================

; Phase 39: Adaptive Commit Governor state
CoT_CommitGovernor STRUCT
    CommitRateEMA       DQ ?            ; bytes/sec exponential moving average
    LastCommitTS        DQ ?            ; GetTickCount64 timestamp of last commit
    ThrottleLevel       DD ?            ; 0=none, 1=soft, 2=hard
    TotalCommitEvents   DD ?            ; counter for telemetry
    BytesCommittedTotal DQ ?            ; lifetime bytes committed
    SoftThrottleCount   DQ ?            ; times soft throttle engaged
    HardThrottleCount   DQ ?            ; times hard throttle engaged
    _reserved           DQ ?            ; alignment pad
CoT_CommitGovernor ENDS

; Phase 40: Snapshot descriptor — O(1) snapshot of arena state
CoT_Snapshot STRUCT
    UsedLength          DQ ?            ; g_arenaUsed at snapshot time
    CommitMark          DQ ?            ; g_arenaCommitted at snapshot time
    Timestamp           DQ ?            ; GetTickCount64 at snapshot time
    ExecutionState      DD ?            ; g_executionState at snapshot
    Valid               DD ?            ; 1 = valid, 0 = empty slot
CoT_Snapshot ENDS

; Phase 41: Shared-Memory Telemetry Page (read-only to observers)
; Single page (4096 bytes), mapped once, no locks needed for reads.
CoT_Telemetry STRUCT
    ; --- Arena metrics ---
    TotalCommitted      DQ ?            ; bytes committed in arena
    TotalUsed           DQ ?            ; bytes used in arena
    ArenaBase           DQ ?            ; base address (for diagnostics)
    ; --- Throughput metrics ---
    AppendRate          DQ ?            ; bytes/sec (EMA)
    AppendCount         DQ ?            ; total append calls
    AppendBytesTotal    DQ ?            ; total bytes appended
    ; --- Copy engine metrics ---
    CopyERMSCount       DQ ?            ; copies via ERMS engine
    CopyAVX2Count       DQ ?            ; copies via AVX2 engine
    CopyAVX512Count     DQ ?            ; copies via AVX-512 engine
    CopyERMSBytes       DQ ?            ; bytes copied via ERMS
    CopyAVX2Bytes       DQ ?            ; bytes copied via AVX2
    CopyAVX512Bytes     DQ ?            ; bytes copied via AVX-512
    ; --- Governance metrics ---
    ThrottleEvents      DQ ?            ; total throttle engagements
    SoftThrottleEvents  DQ ?
    HardThrottleEvents  DQ ?
    ; --- Snapshot metrics ---
    SnapshotsTaken      DQ ?            ; total snapshots created
    SnapshotsRestored   DQ ?            ; total rollbacks
    ; --- Multi-producer metrics ---
    MPTicketCount       DQ ?            ; total tickets issued
    MPContentionCount   DQ ?            ; contention events
    ; --- Backpressure ---
    GPUBackpressure     DQ ?            ; current GPU queue depth signal
    BackpressureEvents  DQ ?            ; times backpressure engaged
    ; --- Security ---
    SealCount           DQ ?            ; times arena was sealed
    QuotaViolations     DQ ?            ; tenant quota refusals
    CanaryIntact        DQ ?            ; 1 if canary pages untouched
    ; --- Timing ---
    LastUpdateTS        DQ ?            ; GetTickCount64 of last telemetry refresh
    UptimeMs            DQ ?            ; ms since CoT_Initialize_Core
    InitTimestamp       DQ ?            ; GetTickCount64 at init
    ; --- Reserved for future ---
    _reserved           DQ 16 DUP(?)
CoT_Telemetry ENDS

; Phase 42: Tenant quota entry
CoT_TenantEntry STRUCT
    QuotaBytes          DQ ?            ; max bytes this tenant may append
    UsedBytes           DQ ?            ; bytes already appended
CoT_TenantEntry ENDS

; =============================================================================
;                        GLOBAL STATE (.data)
; =============================================================================
.data

ALIGN 16

; --- Commit Governor ---
g_commitGovernor        CoT_CommitGovernor <>

; --- Snapshots (ring of MAX_SNAPSHOTS slots) ---
g_snapshotRing          CoT_Snapshot MAX_SNAPSHOTS DUP(<>)
g_snapshotCount         DD 0            ; total snapshots taken
g_snapshotHead          DD 0            ; next write slot index (modular)

; --- Replay mode ---
g_replayMode            DD 0            ; 0 = normal, 1 = replay (read-only)

; --- Telemetry page ---
ALIGN 16
g_telemetry             CoT_Telemetry <>
g_telemetryReady        DD 0            ; 1 after first init

; --- Copy engine selection ---
g_selectedCopyEngine    DD COPY_ENGINE_ERMS     ; default fallback
g_hasERMS               DD 0
g_hasAVX2               DD 0
g_hasAVX512F            DD 0
g_copyEngineInitialized DD 0

; --- Multi-producer mode ---
g_multiProducerEnabled  DD MP_DISABLED
g_mpTicketCounter       DQ 0            ; atomic ticket counter

; --- GPU backpressure ---
g_gpuQueueDepth         DQ 0            ; reported by GPU uploader
g_backpressureThreshold DQ 64           ; default: throttle when GPU queue > 64

; --- Memory seal state ---
g_arenaSealed           DD 0            ; 1 = arena is read-only
g_oldProtect            DD 0            ; saved protection for unseal

; --- Canary values ---
g_canaryHeadOK          DD 1            ; set to 0 if canary violated
g_canaryTailOK          DD 1

; --- Tenant quotas ---
g_tenantTable           CoT_TenantEntry MAX_TENANTS DUP(<>)
g_tenantCount           DD 0

; --- Copy engine dispatch table ---
ALIGN 8
CopyDispatch            DQ 3 DUP(0)    ; filled at runtime by CoT_SelectCopyEngine

; =============================================================================
;                     READ-ONLY STRINGS (.const)
; =============================================================================
.const

ALIGN 8

szP39_SnapshotCreated   DB "[CoT-P39] Snapshot created (slot ", 0
szP39_SnapshotSlotEnd   DB ").", 0
szP39_SnapshotRestored  DB "[CoT-P39] Snapshot restored — arena rolled back.", 0
szP39_SnapshotInvalid   DB "[CoT-P39] Snapshot restore FAILED — invalid slot.", 0
szP39_ReplayOn          DB "[CoT-P39] Deterministic replay mode ENABLED.", 0
szP39_ReplayOff         DB "[CoT-P39] Deterministic replay mode DISABLED.", 0
szP39_ReplayBlocked     DB "[CoT-P39] Append BLOCKED — replay mode active.", 0
szP39_SealOK            DB "[CoT-P39] Arena SEALED (read-only).", 0
szP39_UnsealOK          DB "[CoT-P39] Arena UNSEALED (read-write).", 0
szP39_SealFail          DB "[CoT-P39] Arena seal FAILED.", 0
szP39_CopyERMS          DB "[CoT-P39] Copy engine selected: ERMS (rep movsb).", 0
szP39_CopyAVX2          DB "[CoT-P39] Copy engine selected: AVX2.", 0
szP39_CopyAVX512        DB "[CoT-P39] Copy engine selected: AVX-512.", 0
szP39_CopyFallback      DB "[CoT-P39] Copy engine fallback: ERMS (no AVX2/512).", 0
szP39_MPEnabled         DB "[CoT-P39] Multi-producer mode ENABLED.", 0
szP39_MPDisabled        DB "[CoT-P39] Multi-producer mode remains DISABLED.", 0
szP39_Backpressure      DB "[CoT-P39] GPU backpressure: throttling append.", 0
szP39_QuotaExceeded     DB "[CoT-P39] Tenant quota EXCEEDED — append refused.", 0
szP39_TelemetryReady    DB "[CoT-P39] Telemetry page initialized.", 0
szP39_ThrottleSoft      DB "[CoT-P39] Commit governor: SOFT throttle engaged.", 0
szP39_ThrottleHard      DB "[CoT-P39] Commit governor: HARD throttle engaged.", 0

; =============================================================================
;                            CODE SECTION
; =============================================================================
.code

; =============================================================================
; CoT_SelectCopyEngine
; Probe CPUID to detect ERMS, AVX2, AVX-512F capabilities.
; Populate CopyDispatch table with function pointers.
; Call once at init. Cached — subsequent calls are no-ops.
;
; Returns: EAX = selected engine ID (0=ERMS, 1=AVX2, 2=AVX-512)
; =============================================================================
CoT_SelectCopyEngine PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Check if already initialized
    cmp     DWORD PTR [g_copyEngineInitialized], 1
    je      @@sce_cached

    ; CPUID leaf 7, subleaf 0 — structured extended features
    mov     eax, CPUID_EXTENDED_FEATURES
    xor     ecx, ecx
    cpuid

    ; EBX bit 9 = ERMS (Enhanced REP MOVSB/STOSB)
    bt      ebx, CPUID_ERMS_BIT
    jnc     @@sce_no_erms
    mov     DWORD PTR [g_hasERMS], 1
@@sce_no_erms:

    ; EBX bit 5 = AVX2
    bt      ebx, CPUID_AVX2_BIT
    jnc     @@sce_no_avx2
    mov     DWORD PTR [g_hasAVX2], 1
@@sce_no_avx2:

    ; EBX bit 16 = AVX-512F
    bt      ebx, CPUID_AVX512F_BIT
    jnc     @@sce_no_avx512
    mov     DWORD PTR [g_hasAVX512F], 1
@@sce_no_avx512:

    ; Build dispatch table
    ; Slot 0 = ERMS (always available as fallback)
    lea     rax, CopyEngine_ERMS
    mov     QWORD PTR [CopyDispatch], rax

    ; Slot 1 = AVX2 (if available, else ERMS fallback)
    cmp     DWORD PTR [g_hasAVX2], 1
    jne     @@sce_avx2_fallback
    lea     rax, CopyEngine_AVX2
    mov     QWORD PTR [CopyDispatch + 8], rax
    jmp     @@sce_avx2_done
@@sce_avx2_fallback:
    lea     rax, CopyEngine_ERMS
    mov     QWORD PTR [CopyDispatch + 8], rax
@@sce_avx2_done:

    ; Slot 2 = AVX-512 (if available, else AVX2 or ERMS)
    cmp     DWORD PTR [g_hasAVX512F], 1
    jne     @@sce_avx512_fallback
    lea     rax, CopyEngine_AVX512
    mov     QWORD PTR [CopyDispatch + 16], rax
    mov     DWORD PTR [g_selectedCopyEngine], COPY_ENGINE_AVX512
    lea     rcx, szP39_CopyAVX512
    call    OutputDebugStringA
    jmp     @@sce_selected
@@sce_avx512_fallback:
    cmp     DWORD PTR [g_hasAVX2], 1
    jne     @@sce_all_erms
    lea     rax, CopyEngine_AVX2
    mov     QWORD PTR [CopyDispatch + 16], rax
    mov     DWORD PTR [g_selectedCopyEngine], COPY_ENGINE_AVX2
    lea     rcx, szP39_CopyAVX2
    call    OutputDebugStringA
    jmp     @@sce_selected
@@sce_all_erms:
    lea     rax, CopyEngine_ERMS
    mov     QWORD PTR [CopyDispatch + 16], rax
    mov     DWORD PTR [g_selectedCopyEngine], COPY_ENGINE_ERMS
    lea     rcx, szP39_CopyFallback
    call    OutputDebugStringA

@@sce_selected:
    mov     DWORD PTR [g_copyEngineInitialized], 1

@@sce_cached:
    mov     eax, DWORD PTR [g_selectedCopyEngine]
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
CoT_SelectCopyEngine ENDP

; =============================================================================
; Internal: CopyEngine_ERMS — rep movsb copy (microcode-optimized)
;   RCX = dest
;   RDX = src
;   R8  = size in bytes
; =============================================================================
CopyEngine_ERMS PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx                ; dest
    mov     rsi, rdx                ; src
    mov     rcx, r8                 ; count
    rep     movsb

    ; Update telemetry (lock-free atomic increments)
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.CopyERMSCount]
    mov     rax, r8
    lock xadd QWORD PTR [g_telemetry + CoT_Telemetry.CopyERMSBytes], rax

    pop     rdi
    pop     rsi
    ret
CopyEngine_ERMS ENDP

; =============================================================================
; Internal: CopyEngine_AVX2 — 256-bit YMM copy (best for 256B–32KB)
;   RCX = dest (need not be aligned, but perf is better aligned)
;   RDX = src
;   R8  = size in bytes
; =============================================================================
CopyEngine_AVX2 PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rdi, rcx                ; dest
    mov     rsi, rdx                ; src
    mov     rbx, r8                 ; total size

    ; Copy 32-byte blocks
    mov     rcx, rbx
    shr     rcx, 5                  ; count = size / 32
    jz      @@avx2_remainder

@@avx2_loop:
    vmovdqu ymm0, YMMWORD PTR [rsi]
    vmovdqu YMMWORD PTR [rdi], ymm0
    add     rsi, 32
    add     rdi, 32
    dec     rcx
    jnz     @@avx2_loop

@@avx2_remainder:
    ; Copy remaining bytes (0–31) via rep movsb
    mov     rcx, rbx
    and     rcx, 1Fh                ; size % 32
    jz      @@avx2_done
    rep     movsb

@@avx2_done:
    vzeroupper                      ; restore SSE state

    ; Telemetry
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.CopyAVX2Count]
    mov     rax, r8
    lock xadd QWORD PTR [g_telemetry + CoT_Telemetry.CopyAVX2Bytes], rax

    pop     rbx
    pop     rdi
    pop     rsi
    ret
CopyEngine_AVX2 ENDP

; =============================================================================
; Internal: CopyEngine_AVX512 — 512-bit ZMM copy (best for >32KB)
;   RCX = dest
;   RDX = src
;   R8  = size in bytes
; =============================================================================
CopyEngine_AVX512 PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rdi, rcx                ; dest
    mov     rsi, rdx                ; src
    mov     rbx, r8                 ; total size

    ; Prefetch ahead for streaming
    prefetcht0 [rsi]
    prefetcht0 [rsi + 40h]
    prefetcht0 [rsi + 80h]
    prefetcht0 [rsi + 0C0h]

    ; Copy 64-byte blocks
    mov     rcx, rbx
    shr     rcx, 6                  ; count = size / 64
    jz      @@avx512_remainder

@@avx512_loop:
    vmovdqu64 zmm0, ZMMWORD PTR [rsi]
    vmovdqu64 ZMMWORD PTR [rdi], zmm0
    add     rsi, 64
    add     rdi, 64
    dec     rcx
    jnz     @@avx512_loop

@@avx512_remainder:
    ; Remaining bytes (0–63) via rep movsb
    mov     rcx, rbx
    and     rcx, 3Fh                ; size % 64
    jz      @@avx512_done
    rep     movsb

@@avx512_done:
    vzeroupper

    ; Telemetry
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.CopyAVX512Count]
    mov     rax, r8
    lock xadd QWORD PTR [g_telemetry + CoT_Telemetry.CopyAVX512Bytes], rax

    pop     rbx
    pop     rdi
    pop     rsi
    ret
CopyEngine_AVX512 ENDP

; =============================================================================
; CoT_CopyDispatch
; Runtime-dispatched copy using the best available engine.
; Selection is size-based:
;   < 256 B      → ERMS  (slot 0)
;   256 B – 32KB → AVX2  (slot 1)
;   > 32 KB      → AVX-512 (slot 2)
;
; RCX = dest
; RDX = src
; R8  = size in bytes
;
; Returns: RAX = bytes copied (= R8 on success)
; =============================================================================
CoT_CopyDispatch PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, r8                 ; save size for return

    ; Ensure copy engine is initialized
    cmp     DWORD PTR [g_copyEngineInitialized], 0
    jne     @@cd_dispatch

    ; Auto-init on first call
    push    rcx
    push    rdx
    push    r8
    call    CoT_SelectCopyEngine
    pop     r8
    pop     rdx
    pop     rcx

@@cd_dispatch:
    ; Size-based routing (no branching in hot loop — one branch here at entry)
    cmp     r8, COPY_THRESHOLD_SMALL
    jb      @@cd_erms

    cmp     r8, COPY_THRESHOLD_MEDIUM
    jb      @@cd_avx2

    ; > 32KB → AVX-512 (slot 2)
    call    QWORD PTR [CopyDispatch + 16]
    jmp     @@cd_done

@@cd_avx2:
    ; 256B–32KB → AVX2 (slot 1)
    call    QWORD PTR [CopyDispatch + 8]
    jmp     @@cd_done

@@cd_erms:
    ; < 256B → ERMS (slot 0)
    call    QWORD PTR [CopyDispatch]

@@cd_done:
    mov     rax, rbx                ; return bytes copied
    add     rsp, 40
    pop     rbx
    ret
CoT_CopyDispatch ENDP

; =============================================================================
; CoT_CreateSnapshot
; O(1) snapshot — captures arena UsedLength, CommitMark, Timestamp.
; No memory movement. Stores in ring buffer.
;
; Returns: EAX = snapshot slot index (0–15), or -1 on failure
; =============================================================================
CoT_CreateSnapshot PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Check initialized
    cmp     DWORD PTR [g_initialized], 0
    je      @@cs_fail

    call    Acquire_CoT_Lock_Shared

    ; Get current slot (modular ring)
    mov     eax, DWORD PTR [g_snapshotHead]
    mov     r12d, eax                       ; save slot index for return

    ; Calculate offset: slot * sizeof(CoT_Snapshot)
    imul    rbx, rax, SIZEOF CoT_Snapshot
    lea     rax, [g_snapshotRing]
    add     rbx, rax

    ; Fill snapshot
    mov     rax, QWORD PTR [g_arenaUsed]
    mov     QWORD PTR [rbx + CoT_Snapshot.UsedLength], rax

    mov     rax, QWORD PTR [g_arenaCommitted]
    mov     QWORD PTR [rbx + CoT_Snapshot.CommitMark], rax

    ; Timestamp
    call    GetTickCount64
    mov     QWORD PTR [rbx + CoT_Snapshot.Timestamp], rax

    ; Execution state
    mov     eax, DWORD PTR [g_executionState]
    mov     DWORD PTR [rbx + CoT_Snapshot.ExecutionState], eax

    ; Mark valid
    mov     DWORD PTR [rbx + CoT_Snapshot.Valid], 1

    ; Advance head (modular)
    mov     eax, r12d
    inc     eax
    and     eax, MAX_SNAPSHOTS - 1          ; modular wrap
    mov     DWORD PTR [g_snapshotHead], eax

    ; Increment counter
    inc     DWORD PTR [g_snapshotCount]

    ; Telemetry
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.SnapshotsTaken]

    call    Release_CoT_Lock_Shared

    mov     eax, r12d                       ; return slot index
    jmp     @@cs_done

@@cs_fail:
    mov     eax, -1

@@cs_done:
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
CoT_CreateSnapshot ENDP

; =============================================================================
; CoT_RestoreSnapshot
; Rollback arena state to a previously captured snapshot.
; Safe even mid-generation. Does NOT free VirtualAlloc pages (they remain
; committed for reuse). Simply rewinds g_arenaUsed.
;
; RCX = snapshot slot index (0–15)
;
; Returns: EAX = 0 on success, -1 on invalid slot
; =============================================================================
CoT_RestoreSnapshot PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Validate slot index
    cmp     ecx, MAX_SNAPSHOTS
    jge     @@rs_invalid
    cmp     ecx, 0
    jl      @@rs_invalid

    ; Calculate offset
    imul    rbx, rcx, SIZEOF CoT_Snapshot
    lea     rax, [g_snapshotRing]
    add     rbx, rax

    ; Check valid
    cmp     DWORD PTR [rbx + CoT_Snapshot.Valid], 1
    jne     @@rs_invalid

    call    Acquire_CoT_Lock

    ; Restore arena used length
    mov     rax, QWORD PTR [rbx + CoT_Snapshot.UsedLength]
    mov     QWORD PTR [g_arenaUsed], rax

    ; NOTE: We do NOT decommit pages. Committed pages stay committed
    ; for fast reuse. This is intentional — decommit is expensive and
    ; the arena is bounded at 1GB.

    call    Release_CoT_Lock

    ; Telemetry
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.SnapshotsRestored]

    lea     rcx, szP39_SnapshotRestored
    call    OutputDebugStringA

    xor     eax, eax                        ; success
    jmp     @@rs_done

@@rs_invalid:
    lea     rcx, szP39_SnapshotInvalid
    call    OutputDebugStringA
    mov     eax, -1

@@rs_done:
    add     rsp, 40
    pop     rbx
    ret
CoT_RestoreSnapshot ENDP

; =============================================================================
; CoT_SetReplayMode
; Enable or disable deterministic replay mode.
; When enabled: append is frozen, reads are unlimited, output is deterministic.
;
; RCX = 1 to enable, 0 to disable
;
; Returns: EAX = previous mode (0 or 1)
; =============================================================================
CoT_SetReplayMode PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     eax, DWORD PTR [g_replayMode]
    mov     DWORD PTR [g_replayMode], ecx

    cmp     ecx, 1
    jne     @@srm_off
    lea     rcx, szP39_ReplayOn
    call    OutputDebugStringA
    jmp     @@srm_done
@@srm_off:
    lea     rcx, szP39_ReplayOff
    call    OutputDebugStringA
@@srm_done:
    add     rsp, 40
    ret
CoT_SetReplayMode ENDP

; =============================================================================
; CoT_IsReplayMode
; Returns: EAX = 1 if replay mode active, 0 otherwise
; =============================================================================
CoT_IsReplayMode PROC
    mov     eax, DWORD PTR [g_replayMode]
    ret
CoT_IsReplayMode ENDP

; =============================================================================
; CoT_GetTelemetryPage
; Returns pointer to the shared telemetry struct. Observers can map this
; read-only for zero-syscall monitoring.
;
; Returns: RAX = pointer to CoT_Telemetry (or NULL if not ready)
; =============================================================================
CoT_GetTelemetryPage PROC
    cmp     DWORD PTR [g_telemetryReady], 0
    je      @@gtp_null
    lea     rax, g_telemetry
    ret
@@gtp_null:
    xor     eax, eax
    ret
CoT_GetTelemetryPage ENDP

; =============================================================================
; CoT_UpdateTelemetry
; Refresh telemetry counters from engine state.
; Called periodically by the host (or from CI hooks).
; Lock-free reads of atomic counters + lock-free writes to telemetry page.
;
; Returns: EAX = 0 (always succeeds)
; =============================================================================
CoT_UpdateTelemetry PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Arena metrics
    mov     rax, QWORD PTR [g_arenaCommitted]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.TotalCommitted], rax

    mov     rax, QWORD PTR [g_arenaUsed]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.TotalUsed], rax

    mov     rax, QWORD PTR [g_arenaBase]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.ArenaBase], rax

    ; Commit governor metrics
    mov     rax, QWORD PTR [g_commitGovernor + CoT_CommitGovernor.CommitRateEMA]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.AppendRate], rax

    mov     rax, QWORD PTR [g_commitGovernor + CoT_CommitGovernor.SoftThrottleCount]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.SoftThrottleEvents], rax

    mov     rax, QWORD PTR [g_commitGovernor + CoT_CommitGovernor.HardThrottleCount]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.HardThrottleEvents], rax

    ; Total throttle = soft + hard
    add     rax, QWORD PTR [g_commitGovernor + CoT_CommitGovernor.SoftThrottleCount]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.ThrottleEvents], rax

    ; GPU backpressure
    mov     rax, QWORD PTR [g_gpuQueueDepth]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.GPUBackpressure], rax

    ; Multi-producer ticket count
    mov     rax, QWORD PTR [g_mpTicketCounter]
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.MPTicketCount], rax

    ; Canary check (read canary at arena start if base is valid)
    mov     rax, QWORD PTR [g_arenaBase]
    test    rax, rax
    jz      @@ut_no_canary
    mov     rcx, QWORD PTR [rax]
    mov     rdx, CANARY_HEAD_MAGIC
    cmp     rcx, rdx
    jne     @@ut_canary_bad
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.CanaryIntact], 1
    jmp     @@ut_canary_done
@@ut_canary_bad:
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.CanaryIntact], 0
    mov     DWORD PTR [g_canaryHeadOK], 0
    jmp     @@ut_canary_done
@@ut_no_canary:
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.CanaryIntact], 0
@@ut_canary_done:

    ; Timestamp
    call    GetTickCount64
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.LastUpdateTS], rax

    ; Uptime = now - init timestamp
    mov     rcx, QWORD PTR [g_telemetry + CoT_Telemetry.InitTimestamp]
    test    rcx, rcx
    jz      @@ut_no_uptime
    sub     rax, rcx
    mov     QWORD PTR [g_telemetry + CoT_Telemetry.UptimeMs], rax
@@ut_no_uptime:

    ; Mark ready
    mov     DWORD PTR [g_telemetryReady], 1

    xor     eax, eax
    add     rsp, 40
    ret
CoT_UpdateTelemetry ENDP

; =============================================================================
; CoT_SealMemory
; Mark the arena as read-only after final output. Prevents post-generation
; corruption. Guard pages remain active.
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
CoT_SealMemory PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Check arena exists
    mov     rcx, QWORD PTR [g_arenaBase]
    test    rcx, rcx
    jz      @@sm_fail

    ; Already sealed?
    cmp     DWORD PTR [g_arenaSealed], 1
    je      @@sm_ok

    ; VirtualProtect(base, committedSize, PAGE_READONLY, &oldProtect)
    mov     rdx, QWORD PTR [g_arenaCommitted]
    test    rdx, rdx
    jz      @@sm_fail

    mov     r8d, PAGE_READONLY_VAL
    lea     r9, [rsp + 32]                 ; &oldProtect
    call    VirtualProtect
    test    eax, eax
    jz      @@sm_fail

    mov     eax, DWORD PTR [rsp + 32]
    mov     DWORD PTR [g_oldProtect], eax
    mov     DWORD PTR [g_arenaSealed], 1

    ; Telemetry
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.SealCount]

    lea     rcx, szP39_SealOK
    call    OutputDebugStringA

@@sm_ok:
    xor     eax, eax
    jmp     @@sm_done

@@sm_fail:
    lea     rcx, szP39_SealFail
    call    OutputDebugStringA
    mov     eax, -1

@@sm_done:
    add     rsp, 48
    pop     rbx
    ret
CoT_SealMemory ENDP

; =============================================================================
; CoT_UnsealMemory
; Restore read-write access to the arena. Used before a new generation cycle.
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
CoT_UnsealMemory PROC FRAME
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Check sealed
    cmp     DWORD PTR [g_arenaSealed], 0
    je      @@um_ok                         ; already unsealed

    mov     rcx, QWORD PTR [g_arenaBase]
    test    rcx, rcx
    jz      @@um_fail

    mov     rdx, QWORD PTR [g_arenaCommitted]
    mov     r8d, PAGE_READWRITE_VAL
    lea     r9, [rsp + 32]
    call    VirtualProtect
    test    eax, eax
    jz      @@um_fail

    mov     DWORD PTR [g_arenaSealed], 0

    lea     rcx, szP39_UnsealOK
    call    OutputDebugStringA

@@um_ok:
    xor     eax, eax
    jmp     @@um_done

@@um_fail:
    mov     eax, -1

@@um_done:
    add     rsp, 48
    ret
CoT_UnsealMemory ENDP

; =============================================================================
; CoT_SetTenantQuota
; Set per-tenant byte ceiling for append operations.
;
; RCX = tenant ID (0–63)
; RDX = quota in bytes (0 = unlimited)
;
; Returns: EAX = 0 on success, -1 on invalid tenant
; =============================================================================
CoT_SetTenantQuota PROC
    cmp     ecx, MAX_TENANTS
    jge     @@stq_invalid
    cmp     ecx, 0
    jl      @@stq_invalid

    ; Calculate entry offset
    imul    rax, rcx, TENANT_ENTRY_SIZE
    lea     rbx, [g_tenantTable]
    add     rax, rbx

    ; Set quota
    mov     QWORD PTR [rax + CoT_TenantEntry.QuotaBytes], rdx
    ; Reset used counter
    mov     QWORD PTR [rax + CoT_TenantEntry.UsedBytes], 0

    ; Track tenant count (if new)
    mov     eax, DWORD PTR [g_tenantCount]
    cmp     ecx, eax
    jl      @@stq_ok
    lea     eax, [ecx + 1]
    mov     DWORD PTR [g_tenantCount], eax

@@stq_ok:
    xor     eax, eax
    ret

@@stq_invalid:
    mov     eax, -1
    ret
CoT_SetTenantQuota ENDP

; =============================================================================
; CoT_CheckTenantQuota
; Check if a tenant has remaining quota for an append of given size.
;
; RCX = tenant ID (0–63)
; RDX = proposed append size in bytes
;
; Returns: RAX = remaining quota after append (or -1 if would exceed)
; =============================================================================
CoT_CheckTenantQuota PROC
    cmp     ecx, MAX_TENANTS
    jge     @@ctq_invalid

    imul    rax, rcx, TENANT_ENTRY_SIZE
    lea     rbx, [g_tenantTable]
    add     rax, rbx

    ; Check if quota is 0 (unlimited)
    mov     r8, QWORD PTR [rax + CoT_TenantEntry.QuotaBytes]
    test    r8, r8
    jz      @@ctq_unlimited

    ; Check: used + proposed <= quota
    mov     r9, QWORD PTR [rax + CoT_TenantEntry.UsedBytes]
    add     r9, rdx
    cmp     r9, r8
    ja      @@ctq_exceeded

    ; Remaining = quota - (used + proposed)
    mov     rax, r8
    sub     rax, r9
    ret

@@ctq_unlimited:
    mov     rax, 0FFFFFFFFFFFFFFFFh         ; max uint64 = unlimited
    ret

@@ctq_exceeded:
    ; Telemetry
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.QuotaViolations]
    mov     rax, -1
    ret

@@ctq_invalid:
    mov     rax, -1
    ret
CoT_CheckTenantQuota ENDP

; =============================================================================
; CoT_EnableMultiProducer
; Enable or disable the lock-free multi-producer append path.
; Default: disabled (single-producer fast path).
;
; RCX = 1 to enable, 0 to disable
;
; Returns: EAX = previous state
; =============================================================================
CoT_EnableMultiProducer PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     eax, DWORD PTR [g_multiProducerEnabled]
    mov     DWORD PTR [g_multiProducerEnabled], ecx

    cmp     ecx, MP_ENABLED
    jne     @@emp_dis
    lea     rcx, szP39_MPEnabled
    call    OutputDebugStringA
    jmp     @@emp_done
@@emp_dis:
    lea     rcx, szP39_MPDisabled
    call    OutputDebugStringA
@@emp_done:
    add     rsp, 40
    ret
CoT_EnableMultiProducer ENDP

; =============================================================================
; CoT_MultiProducerAppend
; Lock-free multi-producer append using ticket reservation via LOCK XADD.
; This is the opt-in path for concurrent debate threads.
;
; Single consumer guarantees ordering — producers reserve slices atomically,
; then each copies data into its reserved slice independently.
;
; RCX = source data pointer
; RDX = data size in bytes
;
; Returns: RAX = new total arena used, or 0 on failure
; =============================================================================
CoT_MultiProducerAppend PROC FRAME
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
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Validate
    test    rcx, rcx
    jz      @@mpa_fail
    test    rdx, rdx
    jz      @@mpa_fail

    mov     r12, rcx                ; source
    mov     r13, rdx                ; size

    ; Check multi-producer enabled
    cmp     DWORD PTR [g_multiProducerEnabled], MP_ENABLED
    jne     @@mpa_fail

    ; Check replay mode (append blocked in replay)
    cmp     DWORD PTR [g_replayMode], 1
    je      @@mpa_fail

    ; Check arena sealed
    cmp     DWORD PTR [g_arenaSealed], 1
    je      @@mpa_fail

    ; Check arena initialized
    mov     rax, QWORD PTR [g_arenaBase]
    test    rax, rax
    jz      @@mpa_fail

    ; GPU backpressure check
    mov     rax, QWORD PTR [g_gpuQueueDepth]
    cmp     rax, QWORD PTR [g_backpressureThreshold]
    ja      @@mpa_backpressure

    ; ---- TICKET RESERVATION (lock-free) ----
    ; Atomically reserve a slice: old = g_arenaUsed; g_arenaUsed += size
    mov     rax, r13
    lock xadd QWORD PTR [g_arenaUsed], rax
    ; RAX = our unique insertion offset

    ; Capacity check
    mov     rbx, rax                ; rbx = our offset
    add     rax, r13
    cmp     rax, COT_ARENA_RESERVE_SIZE
    ja      @@mpa_rollback

    ; Ticket counter for telemetry
    lock inc QWORD PTR [g_mpTicketCounter]

    ; Copy data into reserved slice using dispatch engine
    mov     rcx, QWORD PTR [g_arenaBase]
    add     rcx, rbx                ; dest = base + offset
    mov     rdx, r12                ; src
    mov     r8, r13                 ; size
    call    CoT_CopyDispatch

    ; Telemetry
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.AppendCount]
    mov     rax, r13
    lock xadd QWORD PTR [g_telemetry + CoT_Telemetry.AppendBytesTotal], rax

    ; Return new total used
    mov     rax, QWORD PTR [g_arenaUsed]
    jmp     @@mpa_done

@@mpa_backpressure:
    lock inc QWORD PTR [g_telemetry + CoT_Telemetry.BackpressureEvents]
    lea     rcx, szP39_Backpressure
    call    OutputDebugStringA
    ; Fall through to fail — caller should retry

@@mpa_fail:
    xor     eax, eax
    jmp     @@mpa_done

@@mpa_rollback:
    ; Undo ticket reservation
    mov     rax, r13
    neg     rax
    lock xadd QWORD PTR [g_arenaUsed], rax
    xor     eax, eax

@@mpa_done:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
CoT_MultiProducerAppend ENDP

; =============================================================================
; CoT_GetCommitGovernor
; Returns pointer to the commit governor state struct.
; Callers can read CommitRateEMA, ThrottleLevel, etc.
;
; Returns: RAX = pointer to CoT_CommitGovernor
; =============================================================================
CoT_GetCommitGovernor PROC
    lea     rax, g_commitGovernor
    ret
CoT_GetCommitGovernor ENDP

; =============================================================================
; CoT_SetBackpressure
; Set the GPU queue depth signal. Called by GPU uploader to report current
; queue depth. When depth exceeds threshold, multi-producer appends are
; throttled to prevent VRAM starvation cascade.
;
; RCX = current GPU queue depth
;
; Returns: EAX = 0 (always succeeds)
; =============================================================================
CoT_SetBackpressure PROC
    mov     QWORD PTR [g_gpuQueueDepth], rcx
    xor     eax, eax
    ret
CoT_SetBackpressure ENDP

END
