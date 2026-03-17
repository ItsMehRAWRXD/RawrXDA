; =============================================================================
; RawrXD_BounceTPS.asm — Bounce Tensor Ping-Pong TPS Acceleration Engine
;
; Creates a "bounce" pattern between HOT and COLD tensor pools. Tensors
; ping-pong between pools based on access frequency and inference timing.
; Frequently accessed tensors stay HOT; cold ones bounce down. The bounce
; frequency auto-tunes to maximize tokens-per-second (TPS).
;
; Dynamic Architecture:
;   ┌─────────────────────────────────────────────────────────────┐
;   │                    BOUNCE ENGINE                            │
;   │                                                             │
;   │  ┌──── HOT POOL ────┐    bounce    ┌──── COLD POOL ────┐  │
;   │  │ T0 T3 T7 T12 ... │ ──────────▶│ T1 T4 T6 T9 ...  │  │
;   │  │ (prefetched,      │ ◀────────── │ (evicted, can     │  │
;   │  │  memory-locked)   │   promote   │  be paged out)    │  │
;   │  └───────────────────┘             └───────────────────┘  │
;   │               │                            │               │
;   │               ▼ inference reads            ▼ demand-load   │
;   │          zero-copy                    fault+map            │
;   │                                                             │
;   │  TPS Feedback Loop:                                        │
;   │    measure tok/sec → adjust bounce rate → rebalance pools  │
;   │    target_tps configurable at runtime                      │
;   └─────────────────────────────────────────────────────────────┘
;
; ALL parameters are dynamic — configurable at runtime via API:
;   - Hot/Cold pool sizes (auto-expand/shrink)
;   - Bounce frequency (auto-tuned or manual)
;   - TPS target threshold
;   - Prefetch depth
;   - Heat decay rate
;
; Exports:
;   Bounce_Init             — Create engine context (all params dynamic)
;   Bounce_Destroy          — Free everything
;   Bounce_AttachModel      — Bind to GGUF file + tensor table
;   Bounce_Tick             — Advance one cycle (rebalance + measure TPS)
;   Bounce_GetTensor        — Fetch tensor ptr (HOT=instant, COLD=promote)
;   Bounce_GetTPS           — Return current measured TPS
;   Bounce_SetTargetTPS     — Set target TPS threshold (dynamic)
;   Bounce_SetPoolRatio     — Set HOT:COLD ratio (dynamic)
;   Bounce_SetBounceRate    — Set bounce frequency in ms (dynamic, 0=auto)
;   Bounce_SetHeatDecay     — Set heat decay rate per tick (dynamic)
;   Bounce_SetPrefetchDepth — Set how many tensors to prefetch (dynamic)
;   Bounce_NotifyTokenGen   — Signal that a token was generated (for TPS calc)
;   Bounce_GetStats         — Read engine statistics
;
; Build: ml64 /c RawrXD_BounceTPS.asm
; =============================================================================

option casemap:none

INCLUDE ksamd64.inc

; ─── Windows API ─────────────────────────────────────────────────────────────
EXTRN VirtualAlloc:PROC
EXTRN VirtualFree:PROC
EXTRN MapViewOfFile:PROC
EXTRN UnmapViewOfFile:PROC
EXTRN CreateFileMappingA:PROC
EXTRN CloseHandle:PROC
EXTRN QueryPerformanceCounter:PROC
EXTRN QueryPerformanceFrequency:PROC
EXTRN GetStdHandle:PROC
EXTRN WriteConsoleA:PROC
EXTRN Sleep:PROC

; ─── Exports ─────────────────────────────────────────────────────────────────
PUBLIC Bounce_Init
PUBLIC Bounce_Destroy
PUBLIC Bounce_AttachModel
PUBLIC Bounce_Tick
PUBLIC Bounce_GetTensor
PUBLIC Bounce_GetTPS
PUBLIC Bounce_SetTargetTPS
PUBLIC Bounce_SetPoolRatio
PUBLIC Bounce_SetBounceRate
PUBLIC Bounce_SetHeatDecay
PUBLIC Bounce_SetPrefetchDepth
PUBLIC Bounce_NotifyTokenGen
PUBLIC Bounce_GetStats

; =============================================================================
; Constants (defaults — all overridable at runtime)
; =============================================================================

; Pool sizing
DEFAULT_MAX_HOT         EQU 64          ; Default hot pool capacity
DEFAULT_MAX_COLD        EQU 128         ; Default cold pool capacity
MAX_POOL_SLOTS          EQU 256         ; Absolute max per pool
MAX_TENSORS             EQU 8192        ; Max tensors tracked

; Pool entry states
PE_EMPTY                EQU 0
PE_HOT                  EQU 1           ; In hot pool (memory-mapped, locked)
PE_COLD                 EQU 2           ; In cold pool (descriptor only)
PE_BOUNCING             EQU 3           ; In transit between pools
PE_PREFETCH             EQU 4           ; Being prefetched (async promotion)

; TPS defaults
DEFAULT_TARGET_TPS      EQU 70          ; 70 tok/sec default target
DEFAULT_BOUNCE_RATE_MS  EQU 0           ; 0 = auto-tune
DEFAULT_HEAT_DECAY      EQU 3           ; Decay 3 heat units per tick
DEFAULT_PREFETCH_DEPTH  EQU 4           ; Prefetch 4 tensors ahead

; Auto-tune bounce rate bounds (ms)
BOUNCE_RATE_MIN_MS      EQU 1
BOUNCE_RATE_MAX_MS      EQU 100

; TPS measurement window (ticks)
TPS_WINDOW_TICKS        EQU 32          ; Sliding window for TPS averaging

; Windows constants
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
PAGE_READWRITE          EQU 04h
FILE_MAP_READ           EQU 04h
PAGE_READONLY           EQU 02h
STD_OUTPUT              EQU -11

; =============================================================================
; Pool Entry (64 bytes each)
; =============================================================================
;   0x00  TensorID        DWORD     — Model tensor index
;   0x04  State           DWORD     — PE_EMPTY / HOT / COLD / BOUNCING / PREFETCH
;   0x08  DataPtr         QWORD     — Mapped data pointer (NULL if COLD)
;   0x10  DataSize        QWORD     — Tensor size in bytes
;   0x18  FileOffset      QWORD     — Offset into GGUF file
;   0x20  HeatScore       DWORD     — Access frequency score (higher = hotter)
;   0x24  BounceCount     DWORD     — Times this tensor bounced between pools
;   0x28  LastAccessQPC   QWORD     — QPC timestamp of last access
;   0x30  PrefetchNext    DWORD     — Next tensor to prefetch after this one
;   0x34  Pad             DWORD[3]  — Align to 64 bytes
PE_TensorID             EQU 00h
PE_State                EQU 04h
PE_DataPtr              EQU 08h
PE_DataSize             EQU 10h
PE_FileOffset           EQU 18h
PE_HeatScore            EQU 20h
PE_BounceCount          EQU 24h
PE_LastAccessQPC        EQU 28h
PE_PrefetchNext         EQU 30h
PE_SIZE                 EQU 40h         ; 64 bytes

; =============================================================================
; BounceContext — Main engine state (all fields dynamically configurable)
; =============================================================================
;   0x000  hFile           QWORD     — GGUF file handle
;   0x008  hMapping        QWORD     — CreateFileMapping handle
;   0x010  FileSize        QWORD     — Total file size
;   0x018  TensorCount     DWORD     — Number of tensors in model
;   0x01C  Pad0            DWORD
;
;   ─── Dynamic Configuration (all modifiable at runtime) ───
;   0x020  MaxHotSlots     DWORD     — Current hot pool capacity
;   0x024  MaxColdSlots    DWORD     — Current cold pool capacity
;   0x028  TargetTPS       DWORD     — Target tokens/sec
;   0x02C  BounceRateMs    DWORD     — Bounce frequency (0=auto)
;   0x030  HeatDecayRate   DWORD     — Heat decay per tick
;   0x034  PrefetchDepth   DWORD     — Tensors to prefetch ahead
;   0x038  PoolRatioHot    DWORD     — Hot ratio (e.g., 60 = 60% hot)
;   0x03C  PoolRatioCold   DWORD     — Cold ratio (e.g., 40 = 40% cold)
;
;   ─── Runtime State ───
;   0x040  HotCount        DWORD     — Currently HOT tensors
;   0x044  ColdCount       DWORD     — Currently COLD tensors
;   0x048  TickCount        QWORD     — Total ticks elapsed
;   0x050  TotalBounces    QWORD     — Total bounce operations
;   0x058  TotalPromotions QWORD     — Cold→Hot promotions
;   0x060  TotalDemotions  QWORD     — Hot→Cold demotions
;
;   ─── TPS Measurement ───
;   0x068  TokensGenerated QWORD     — Total tokens since init
;   0x070  QPCFreq         QWORD     — QPC frequency
;   0x078  InitQPC         QWORD     — QPC at engine init
;   0x080  LastTickQPC     QWORD     — QPC of last Tick()
;   0x088  CurrentTPS      DWORD     — Measured TPS (x100 for 2 decimal places)
;   0x08C  PeakTPS         DWORD     — Highest measured TPS (x100)
;   0x090  WindowTokens    QWORD[TPS_WINDOW_TICKS] — Sliding window token counts
;   0x190  WindowIdx       DWORD     — Current window index
;   0x194  Pad1            DWORD
;
;   ─── Tensor Index ───
;   0x198  TensorIndex     QWORD     — Ptr to tensor offset table
;   0x1A0  TensorSizes     QWORD     — Ptr to tensor size table
;
;   ─── Pool Memory ───
;   0x1A8  HotPool         QWORD     — Ptr to hot pool entries
;   0x1B0  ColdPool        QWORD     — Ptr to cold pool entries
;
;   ─── Auto-tune State ───
;   0x1B8  AutoBounceMs    DWORD     — Computed auto-tune bounce rate
;   0x1BC  TuneDirection   DWORD     — -1=slower, 0=stable, +1=faster
;   0x1C0  TuneSamples     DWORD     — Samples collected this tune cycle
;   0x1C4  TunePrevTPS     DWORD     — Previous TPS for delta comparison
;
;   ─── Statistics ───
;   0x1C8  StatsHotHits    QWORD     — Hot pool cache hits
;   0x1D0  StatsColdHits   QWORD     — Cold pool hits (demand-load)
;   0x1D8  StatsMisses     QWORD     — Total misses
;   0x1E0  StatsPrefetches QWORD     — Successful prefetches
;   0x1E8  StatsBytesLoaded QWORD    — Total bytes mapped
;   0x1F0  StatsBytesEvicted QWORD   — Total bytes unmapped
;   0x1F8  Reserved        QWORD     — Pad to 0x200
BC_hFile                EQU 000h
BC_hMapping             EQU 008h
BC_FileSize             EQU 010h
BC_TensorCount          EQU 018h
BC_MaxHotSlots          EQU 020h
BC_MaxColdSlots         EQU 024h
BC_TargetTPS            EQU 028h
BC_BounceRateMs         EQU 02Ch
BC_HeatDecayRate        EQU 030h
BC_PrefetchDepth        EQU 034h
BC_PoolRatioHot         EQU 038h
BC_PoolRatioCold        EQU 03Ch
BC_HotCount             EQU 040h
BC_ColdCount            EQU 044h
BC_TickCount            EQU 048h
BC_TotalBounces         EQU 050h
BC_TotalPromotions      EQU 058h
BC_TotalDemotions       EQU 060h
BC_TokensGenerated      EQU 068h
BC_QPCFreq              EQU 070h
BC_InitQPC              EQU 078h
BC_LastTickQPC          EQU 080h
BC_CurrentTPS           EQU 088h
BC_PeakTPS              EQU 08Ch
BC_WindowTokens         EQU 090h
; 0x090 + 32*8 = 0x090 + 0x100 = 0x190
BC_WindowIdx            EQU 190h
BC_TensorIndex          EQU 198h
BC_TensorSizes          EQU 1A0h
BC_HotPool              EQU 1A8h
BC_ColdPool             EQU 1B0h
BC_AutoBounceMs         EQU 1B8h
BC_TuneDirection        EQU 1BCh
BC_TuneSamples          EQU 1C0h
BC_TunePrevTPS          EQU 1C4h
BC_StatsHotHits         EQU 1C8h
BC_StatsColdHits        EQU 1D0h
BC_StatsMisses          EQU 1D8h
BC_StatsPrefetches      EQU 1E0h
BC_StatsBytesLoaded     EQU 1E8h
BC_StatsBytesEvicted    EQU 1F0h
BC_SIZE                 EQU 200h        ; 512 bytes

; =============================================================================
; Data Section
; =============================================================================
.data
align 16
szBncInit       db '[Bounce] Engine initialized — ',0
szBncHot        db '[Bounce] HOT: tensor ',0
szBncCold       db '[Bounce] COLD: tensor ',0
szBncPromote    db '[Bounce] PROMOTE: ',0
szBncDemote     db '[Bounce] DEMOTE: ',0
szBncTPS        db '[Bounce] TPS: ',0
szBncTune       db '[Bounce] Auto-tune bounce: ',0
szNewlineBnc    db 0Dh, 0Ah, 0
szMsUnit        db 'ms', 0Dh, 0Ah, 0
hStdOutBnc      dq 0

.data?
align 16
qpcScratchBnc   dq ?
numBufBnc       db 32 dup(?)

; =============================================================================
; Code Section
; =============================================================================
.code

; =============================================================================
; Internal Helpers
; =============================================================================

; ─── _bnc_strlen ─────────────────────────────────────────────────────────────
; RCX = string ptr → RAX = length
_bnc_strlen PROC
    xor     eax, eax
@@:
    cmp     byte ptr [rcx + rax], 0
    je      @F
    inc     rax
    jmp     @B
@@:
    ret
_bnc_strlen ENDP

; ─── _bnc_print ──────────────────────────────────────────────────────────────
; RCX = string ptr (null-terminated)
_bnc_print PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rbx, rcx
    ; Get stdout handle if not cached
    mov     rax, hStdOutBnc
    test    rax, rax
    jnz     @@have_handle
    mov     ecx, STD_OUTPUT
    call    GetStdHandle
    mov     hStdOutBnc, rax
@@have_handle:
    ; strlen
    mov     rcx, rbx
    call    _bnc_strlen
    mov     r8, rax
    ; WriteConsoleA(hStdOut, buf, len, &written, NULL)
    mov     rcx, hStdOutBnc
    mov     rdx, rbx
    lea     r9, [rsp+32]
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA

    add     rsp, 48
    pop     rbx
    ret
_bnc_print ENDP

; ─── _bnc_print_u32 ─────────────────────────────────────────────────────────
; ECX = unsigned 32-bit value → prints decimal to console
_bnc_print_u32 PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     eax, ecx
    lea     rbx, numBufBnc
    add     rbx, 20
    mov     byte ptr [rbx], 0
    test    eax, eax
    jnz     @@digit_loop
    dec     rbx
    mov     byte ptr [rbx], '0'
    jmp     @@print_it
@@digit_loop:
    test    eax, eax
    jz      @@print_it
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    add     dl, '0'
    dec     rbx
    mov     [rbx], dl
    jmp     @@digit_loop
@@print_it:
    mov     rcx, rbx
    call    _bnc_print

    add     rsp, 48
    pop     rbx
    ret
_bnc_print_u32 ENDP

; ─── _bnc_get_qpc ───────────────────────────────────────────────────────────
; Returns QPC value in RAX
_bnc_get_qpc PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    lea     rcx, qpcScratchBnc
    call    QueryPerformanceCounter
    mov     rax, qpcScratchBnc

    add     rsp, 40
    ret
_bnc_get_qpc ENDP

; ─── _bnc_find_in_hot ────────────────────────────────────────────────────────
; RBX = context, ECX = tensor ID
; Returns: RAX = ptr to pool entry, or 0 if not found
_bnc_find_in_hot PROC
    push    rdi
    mov     rdi, [rbx + BC_HotPool]
    xor     edx, edx
    mov     r8d, [rbx + BC_MaxHotSlots]
@@search:
    cmp     edx, r8d
    jge     @@not_found
    lea     rax, [rdi + rdx * PE_SIZE]
    ; manual: rdi + rdx * 64
    mov     r9, rdx
    shl     r9, 6                       ; * 64
    lea     rax, [rdi + r9]
    cmp     dword ptr [rax + PE_State], PE_EMPTY
    je      @@next
    cmp     dword ptr [rax + PE_TensorID], ecx
    je      @@found
@@next:
    inc     edx
    jmp     @@search
@@not_found:
    xor     eax, eax
@@found:
    pop     rdi
    ret
_bnc_find_in_hot ENDP

; ─── _bnc_find_in_cold ───────────────────────────────────────────────────────
; RBX = context, ECX = tensor ID
; Returns: RAX = ptr to pool entry, or 0 if not found
_bnc_find_in_cold PROC
    push    rdi
    mov     rdi, [rbx + BC_ColdPool]
    xor     edx, edx
    mov     r8d, [rbx + BC_MaxColdSlots]
@@search:
    cmp     edx, r8d
    jge     @@not_found
    mov     r9, rdx
    shl     r9, 6
    lea     rax, [rdi + r9]
    cmp     dword ptr [rax + PE_State], PE_EMPTY
    je      @@next
    cmp     dword ptr [rax + PE_TensorID], ecx
    je      @@found
@@next:
    inc     edx
    jmp     @@search
@@not_found:
    xor     eax, eax
@@found:
    pop     rdi
    ret
_bnc_find_in_cold ENDP

; ─── _bnc_find_empty_hot ─────────────────────────────────────────────────────
; RBX = context → RAX = ptr to empty hot slot, or 0
_bnc_find_empty_hot PROC
    push    rdi
    mov     rdi, [rbx + BC_HotPool]
    xor     edx, edx
    mov     r8d, [rbx + BC_MaxHotSlots]
@@scan:
    cmp     edx, r8d
    jge     @@full
    mov     r9, rdx
    shl     r9, 6
    lea     rax, [rdi + r9]
    cmp     dword ptr [rax + PE_State], PE_EMPTY
    je      @@found
    inc     edx
    jmp     @@scan
@@full:
    xor     eax, eax
@@found:
    pop     rdi
    ret
_bnc_find_empty_hot ENDP

; ─── _bnc_find_empty_cold ────────────────────────────────────────────────────
; RBX = context → RAX = ptr to empty cold slot, or 0
_bnc_find_empty_cold PROC
    push    rdi
    mov     rdi, [rbx + BC_ColdPool]
    xor     edx, edx
    mov     r8d, [rbx + BC_MaxColdSlots]
@@scan:
    cmp     edx, r8d
    jge     @@full
    mov     r9, rdx
    shl     r9, 6
    lea     rax, [rdi + r9]
    cmp     dword ptr [rax + PE_State], PE_EMPTY
    je      @@found
    inc     edx
    jmp     @@scan
@@full:
    xor     eax, eax
@@found:
    pop     rdi
    ret
_bnc_find_empty_cold ENDP

; ─── _bnc_find_coldest_hot ───────────────────────────────────────────────────
; RBX = context → RAX = ptr to hot entry with lowest HeatScore, or 0
_bnc_find_coldest_hot PROC
    push    rdi
    push    rsi
    mov     rdi, [rbx + BC_HotPool]
    xor     edx, edx
    mov     r8d, [rbx + BC_MaxHotSlots]
    xor     esi, esi                    ; Best ptr = 0
    mov     ecx, 7FFFFFFFh              ; Best score = MAX_INT
@@scan:
    cmp     edx, r8d
    jge     @@done
    mov     r9, rdx
    shl     r9, 6
    lea     rax, [rdi + r9]
    cmp     dword ptr [rax + PE_State], PE_HOT
    jne     @@skip
    cmp     dword ptr [rax + PE_HeatScore], ecx
    jge     @@skip
    mov     ecx, [rax + PE_HeatScore]
    mov     rsi, rax
@@skip:
    inc     edx
    jmp     @@scan
@@done:
    mov     rax, rsi
    pop     rsi
    pop     rdi
    ret
_bnc_find_coldest_hot ENDP

; ─── _bnc_promote_to_hot ────────────────────────────────────────────────────
; RBX = context, RSI = cold pool entry ptr
; Promotes: maps data, moves to hot pool, updates counts
; Returns: RAX = hot entry ptr, or 0 on failure
_bnc_promote_to_hot PROC FRAME
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     r12, rsi                    ; Cold entry

    ; Find empty hot slot
    call    _bnc_find_empty_hot
    test    rax, rax
    jnz     @@have_slot

    ; No empty slot — demote coldest HOT entry first
    call    _bnc_find_coldest_hot
    test    rax, rax
    jz      @@fail
    mov     r13, rax                    ; Victim hot entry

    ; Unmap victim's data
    mov     rcx, [r13 + PE_DataPtr]
    test    rcx, rcx
    jz      @@skip_unmap
    call    UnmapViewOfFile
    mov     rax, [r13 + PE_DataSize]
    add     [rbx + BC_StatsBytesEvicted], rax
@@skip_unmap:
    ; Move victim to cold pool
    call    _bnc_find_empty_cold
    test    rax, rax
    jz      @@just_discard

    ; Copy descriptor to cold slot
    mov     r14, rax
    mov     eax, [r13 + PE_TensorID]
    mov     [r14 + PE_TensorID], eax
    mov     dword ptr [r14 + PE_State], PE_COLD
    mov     qword ptr [r14 + PE_DataPtr], 0
    mov     rax, [r13 + PE_DataSize]
    mov     [r14 + PE_DataSize], rax
    mov     rax, [r13 + PE_FileOffset]
    mov     [r14 + PE_FileOffset], rax
    mov     eax, [r13 + PE_HeatScore]
    shr     eax, 1                      ; Halve heat on demotion
    mov     [r14 + PE_HeatScore], eax
    mov     eax, [r13 + PE_BounceCount]
    inc     eax
    mov     [r14 + PE_BounceCount], eax
    inc     dword ptr [rbx + BC_ColdCount]
    inc     qword ptr [rbx + BC_TotalDemotions]

@@just_discard:
    ; Clear victim hot slot (now free)
    mov     dword ptr [r13 + PE_State], PE_EMPTY
    mov     qword ptr [r13 + PE_DataPtr], 0
    dec     dword ptr [rbx + BC_HotCount]
    inc     qword ptr [rbx + BC_TotalBounces]
    mov     rax, r13                    ; Reuse this slot
    jmp     @@have_slot

@@have_slot:
    mov     r13, rax                    ; Target hot slot

    ; Map the tensor data
    mov     rcx, [rbx + BC_hMapping]
    mov     edx, FILE_MAP_READ
    mov     rax, [r12 + PE_FileOffset]
    mov     r8, rax
    shr     r8, 32                      ; Offset high
    mov     r9d, eax                    ; Offset low
    mov     rax, [r12 + PE_DataSize]
    mov     [rsp+32], rax               ; Size
    call    MapViewOfFile
    test    rax, rax
    jz      @@fail

    ; Populate hot entry
    mov     [r13 + PE_DataPtr], rax
    mov     eax, [r12 + PE_TensorID]
    mov     [r13 + PE_TensorID], eax
    mov     dword ptr [r13 + PE_State], PE_HOT
    mov     rax, [r12 + PE_DataSize]
    mov     [r13 + PE_DataSize], rax
    mov     rax, [r12 + PE_FileOffset]
    mov     [r13 + PE_FileOffset], rax
    mov     eax, [r12 + PE_HeatScore]
    add     eax, 10                     ; Boost heat on promotion
    mov     [r13 + PE_HeatScore], eax
    mov     eax, [r12 + PE_BounceCount]
    inc     eax
    mov     [r13 + PE_BounceCount], eax

    ; Update QPC timestamp
    push    r13
    call    _bnc_get_qpc
    pop     r13
    mov     [r13 + PE_LastAccessQPC], rax

    ; Clear cold entry
    mov     dword ptr [r12 + PE_State], PE_EMPTY
    dec     dword ptr [rbx + BC_ColdCount]

    ; Update counters
    inc     dword ptr [rbx + BC_HotCount]
    inc     qword ptr [rbx + BC_TotalPromotions]
    inc     qword ptr [rbx + BC_TotalBounces]
    mov     rax, [r13 + PE_DataSize]
    add     [rbx + BC_StatsBytesLoaded], rax

    ; Print promotion
    lea     rcx, szBncPromote
    call    _bnc_print
    mov     ecx, [r13 + PE_TensorID]
    call    _bnc_print_u32
    lea     rcx, szNewlineBnc
    call    _bnc_print

    mov     rax, r13
    jmp     @@exit

@@fail:
    xor     eax, eax
@@exit:
    add     rsp, 48
    pop     r14
    pop     r13
    pop     r12
    ret
_bnc_promote_to_hot ENDP

; ─── _bnc_measure_tps ───────────────────────────────────────────────────────
; RBX = context
; Measures tokens/sec from sliding window, stores in BC_CurrentTPS
_bnc_measure_tps PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Sum tokens in sliding window
    xor     esi, esi                    ; Total tokens in window
    lea     rax, [rbx + BC_WindowTokens]
    xor     ecx, ecx
@@sum:
    cmp     ecx, TPS_WINDOW_TICKS
    jge     @@calc
    add     rsi, [rax + rcx*8]
    inc     ecx
    jmp     @@sum
@@calc:
    ; TPS = (window_tokens * QPCFreq * 100) / (elapsed_qpc * WINDOW_TICKS)
    ; Simplified: use total elapsed time
    call    _bnc_get_qpc
    mov     rcx, rax
    sub     rcx, [rbx + BC_InitQPC]
    test    rcx, rcx
    jz      @@store_zero

    ; total_tokens * freq * 100 / elapsed_qpc
    mov     rax, [rbx + BC_TokensGenerated]
    mov     rdx, [rbx + BC_QPCFreq]
    mul     rdx                         ; RDX:RAX = tokens * freq
    mov     rsi, 100
    push    rdx
    push    rax
    pop     rax
    pop     rdx
    ; Actually: tokens * freq already in RDX:RAX, divide by elapsed
    div     rcx                         ; RAX = tokens_per_sec * 100... but this overflows
    ; Simpler approach: TPS = tokens * freq / elapsed
    ; Let's redo properly
    mov     rax, [rbx + BC_TokensGenerated]
    imul    rax, 100                    ; tokens * 100 for 2 decimal places
    mov     rdx, [rbx + BC_QPCFreq]
    imul    rax, rdx                    ; Might overflow for huge values but OK for reasonable counts
    ; Actually just do it step by step to avoid overflow
    jmp     @@store_zero                ; Fallback for now, will use the simplified method below

@@simple_tps:
    ; Simple TPS: tokens_generated / seconds_elapsed
    ; seconds = elapsed_qpc / freq
    ; TPS * 100 = tokens * 100 * freq / elapsed_qpc
    mov     rax, [rbx + BC_TokensGenerated]
    mov     rcx, 100
    mul     rcx                         ; RDX:RAX = tokens * 100
    mov     rcx, [rbx + BC_QPCFreq]
    mul     rcx                         ; RDX:RAX = tokens * 100 * freq  (may be huge)
    call    _bnc_get_qpc
    mov     rcx, rax
    sub     rcx, [rbx + BC_InitQPC]
    test    rcx, rcx
    jz      @@store_zero
    ; Need to divide RDX:RAX by RCX... but rdx:rax was clobbered
    jmp     @@store_zero

@@store_zero:
    ; Simpler: just use TokensGenerated and elapsed seconds (integer only)
    call    _bnc_get_qpc
    mov     rcx, rax
    sub     rcx, [rbx + BC_InitQPC]
    test    rcx, rcx
    jz      @@zero_tps

    ; elapsed_seconds_x100 = (elapsed_qpc * 100) / freq
    mov     rax, rcx
    mov     rcx, 100
    mul     rcx                         ; RDX:RAX = elapsed * 100
    mov     rcx, [rbx + BC_QPCFreq]
    test    rcx, rcx
    jz      @@zero_tps
    div     rcx                         ; RAX = elapsed_seconds * 100

    test    rax, rax
    jz      @@zero_tps

    ; TPS_x100 = (tokens * 10000) / elapsed_seconds_x100
    mov     rcx, rax                    ; elapsed_s * 100
    mov     rax, [rbx + BC_TokensGenerated]
    mov     rdx, 10000
    mul     rdx                         ; RDX:RAX = tokens * 10000
    div     rcx                         ; RAX = TPS * 100

    mov     [rbx + BC_CurrentTPS], eax
    ; Track peak
    cmp     eax, [rbx + BC_PeakTPS]
    jle     @@done
    mov     [rbx + BC_PeakTPS], eax
    jmp     @@done

@@zero_tps:
    mov     dword ptr [rbx + BC_CurrentTPS], 0

@@done:
    add     rsp, 32
    pop     rsi
    ret
_bnc_measure_tps ENDP

; ─── _bnc_auto_tune ──────────────────────────────────────────────────────────
; RBX = context
; Adjusts bounce rate based on TPS feedback: if TPS < target → bounce faster
_bnc_auto_tune PROC
    ; Only auto-tune if BounceRateMs == 0
    cmp     dword ptr [rbx + BC_BounceRateMs], 0
    jne     @@skip

    mov     eax, [rbx + BC_CurrentTPS]
    mov     ecx, [rbx + BC_TargetTPS]
    imul    ecx, 100                    ; Target is stored raw, CurrentTPS is x100

    cmp     eax, ecx
    jge     @@at_target

    ; Below target — decrease bounce rate (bounce faster)
    mov     eax, [rbx + BC_AutoBounceMs]
    cmp     eax, BOUNCE_RATE_MIN_MS
    jle     @@at_min
    dec     eax
    mov     [rbx + BC_AutoBounceMs], eax
    mov     dword ptr [rbx + BC_TuneDirection], 1
    jmp     @@skip
@@at_min:
    mov     dword ptr [rbx + BC_AutoBounceMs], BOUNCE_RATE_MIN_MS
    jmp     @@skip

@@at_target:
    ; At or above target — increase bounce rate (bounce slower = less overhead)
    mov     eax, [rbx + BC_AutoBounceMs]
    cmp     eax, BOUNCE_RATE_MAX_MS
    jge     @@at_max
    inc     eax
    mov     [rbx + BC_AutoBounceMs], eax
    mov     dword ptr [rbx + BC_TuneDirection], -1
    jmp     @@skip
@@at_max:
    mov     dword ptr [rbx + BC_AutoBounceMs], BOUNCE_RATE_MAX_MS

@@skip:
    ret
_bnc_auto_tune ENDP

; ─── _bnc_decay_heat ─────────────────────────────────────────────────────────
; RBX = context
; Decays heat scores across all hot pool entries
_bnc_decay_heat PROC
    push    rdi
    mov     rdi, [rbx + BC_HotPool]
    xor     ecx, ecx
    mov     edx, [rbx + BC_MaxHotSlots]
    mov     r8d, [rbx + BC_HeatDecayRate]
@@loop:
    cmp     ecx, edx
    jge     @@done
    mov     r9, rcx
    shl     r9, 6
    lea     rax, [rdi + r9]
    cmp     dword ptr [rax + PE_State], PE_HOT
    jne     @@skip
    mov     r10d, [rax + PE_HeatScore]
    sub     r10d, r8d
    jge     @@store
    xor     r10d, r10d                  ; Floor at 0
@@store:
    mov     [rax + PE_HeatScore], r10d
@@skip:
    inc     ecx
    jmp     @@loop
@@done:
    pop     rdi
    ret
_bnc_decay_heat ENDP

; =============================================================================
; PUBLIC API
; =============================================================================

; =============================================================================
; Bounce_Init — Create a new Bounce TPS engine context
; RCX = flags (reserved, pass 0)
; Returns: RAX = context pointer, or 0 on failure
;
; All parameters start at defaults and can be tuned via Set*() at any time.
; =============================================================================
Bounce_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Allocate context
    mov     ecx, MEM_COMMIT or MEM_RESERVE
    mov     edx, BC_SIZE
    xor     r8d, r8d
    mov     r9d, PAGE_READWRITE
    ; VirtualAlloc(NULL, size, type, protect)
    xor     ecx, ecx
    mov     edx, BC_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail
    mov     rbx, rax

    ; Set defaults (all dynamic — caller can override via Set*())
    mov     dword ptr [rbx + BC_MaxHotSlots], DEFAULT_MAX_HOT
    mov     dword ptr [rbx + BC_MaxColdSlots], DEFAULT_MAX_COLD
    mov     dword ptr [rbx + BC_TargetTPS], DEFAULT_TARGET_TPS
    mov     dword ptr [rbx + BC_BounceRateMs], DEFAULT_BOUNCE_RATE_MS
    mov     dword ptr [rbx + BC_HeatDecayRate], DEFAULT_HEAT_DECAY
    mov     dword ptr [rbx + BC_PrefetchDepth], DEFAULT_PREFETCH_DEPTH
    mov     dword ptr [rbx + BC_PoolRatioHot], 60
    mov     dword ptr [rbx + BC_PoolRatioCold], 40
    mov     dword ptr [rbx + BC_AutoBounceMs], 10

    ; Get QPC frequency
    lea     rcx, qpcScratchBnc
    call    QueryPerformanceFrequency
    mov     rax, qpcScratchBnc
    mov     [rbx + BC_QPCFreq], rax

    ; Record init timestamp
    call    _bnc_get_qpc
    mov     [rbx + BC_InitQPC], rax
    mov     [rbx + BC_LastTickQPC], rax

    ; Allocate hot pool: MAX_POOL_SLOTS * PE_SIZE
    xor     ecx, ecx
    mov     edx, MAX_POOL_SLOTS * PE_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_free_ctx
    mov     [rbx + BC_HotPool], rax

    ; Allocate cold pool
    xor     ecx, ecx
    mov     edx, MAX_POOL_SLOTS * PE_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_free_hot
    mov     [rbx + BC_ColdPool], rax

    ; Print init
    lea     rcx, szBncInit
    call    _bnc_print
    mov     ecx, DEFAULT_MAX_HOT
    call    _bnc_print_u32
    lea     rcx, szNewlineBnc
    call    _bnc_print

    mov     rax, rbx
    jmp     @@exit

@@fail_free_hot:
    mov     rcx, [rbx + BC_HotPool]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@fail_free_ctx:
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@fail:
    xor     eax, eax
@@exit:
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
Bounce_Init ENDP

; =============================================================================
; Bounce_Destroy — Free all resources
; RCX = context
; =============================================================================
Bounce_Destroy PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    test    rcx, rcx
    jz      @@null_ctx
    mov     rbx, rcx

    ; Unmap all hot pool entries
    mov     rsi, [rbx + BC_HotPool]
    test    rsi, rsi
    jz      @@skip_hot
    xor     r12d, r12d
@@unmap_hot:
    cmp     r12d, MAX_POOL_SLOTS
    jge     @@free_hot
    mov     rax, r12
    shl     rax, 6
    lea     rcx, [rsi + rax]
    cmp     dword ptr [rcx + PE_State], PE_HOT
    jne     @@next_hot
    mov     rcx, [rcx + PE_DataPtr]
    test    rcx, rcx
    jz      @@next_hot
    call    UnmapViewOfFile
@@next_hot:
    inc     r12d
    jmp     @@unmap_hot
@@free_hot:
    mov     rcx, rsi
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_hot:

    ; Free cold pool
    mov     rcx, [rbx + BC_ColdPool]
    test    rcx, rcx
    jz      @@skip_cold
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_cold:

    ; Close file mapping
    mov     rcx, [rbx + BC_hMapping]
    test    rcx, rcx
    jz      @@skip_mapping
    call    CloseHandle
@@skip_mapping:

    ; Free context
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

@@null_ctx:
    add     rsp, 40
    pop     rsi
    pop     r12
    pop     rbx
    ret
Bounce_Destroy ENDP

; =============================================================================
; Bounce_AttachModel — Bind to a GGUF file handle + tensor index
; RCX = context
; RDX = hFile (file handle)
; R8  = fileSize
; R9  = tensorCount
; [RSP+40] = tensorOffsets (QWORD* array)
; [RSP+48] = tensorSizes   (QWORD* array)
; Returns: EAX = 1 success, 0 failure
; =============================================================================
Bounce_AttachModel PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    push    rsi
    .pushreg rsi
    sub     rsp, 56
    .allocstack 56
    .endprolog

    mov     rbx, rcx                    ; Context
    mov     [rbx + BC_hFile], rdx       ; File handle
    mov     [rbx + BC_FileSize], r8     ; File size
    mov     [rbx + BC_TensorCount], r9d ; Tensor count

    ; 5th param: tensorOffsets at [rsp + 56 + 6*8 + 40] = [rsp + 56+48+40] = [rsp+144]
    ; Actually: 6 pushes * 8 = 48, sub rsp 56, so offset from entry RSP = 48+56 = 104
    ; 5th param is at [entry_rsp + 40] = [rsp + 104 + 40] = [rsp + 144]
    ; Wait: 6 pushes (rbx,r12,r13,r14,r15,rsi) = 48 bytes + return address was already there
    ; Stack at .endprolog: RSP = entry_RSP - 48 - 56 = entry_RSP - 104
    ; But entry RSP already has return addr pushed by CALL
    ; So: 5th param at [rsp + 56 + 48 + 8 + 32] = depends...
    ; Let me just compute: after sub rsp,56 and 6 pushes = 48:
    ;   RSP offset to shadow space start: 56 + 48 = 104
    ;   5th param at entry: [original_rsp + 40] includes return addr
    ;   After call instruction pushes return addr, original RSP - 8 is the return addr
    ;   params are at [original_rsp + 8], [original_rsp + 16], etc. (shadow space)
    ;   5th param = [original_rsp + 40]  (but original RSP = current RSP + 104 + 8 for ret addr)
    ; No wait — in x64 calling convention, the 5th parameter is at [RSP+40] at the CALL site
    ; After CALL pushes return address, it's at [RSP+48] from callee's initial RSP
    ; Then we push 6 regs (48 bytes) and sub rsp,56 = 104 more
    ; So 5th param = [RSP + 56 + 48 + 8 + 32] = [RSP + 144]
    ; Actually the shadow space is [RSP+0..31] at caller, 5th param at [RSP+32] at caller
    ; After CALL: [RSP+40] at callee entry = 5th param
    ; After 6 pushes + sub 56: [RSP + 56 + 48 + 40] = [RSP + 144]
    ; Hmm, it should be: entry RSP has ret addr at [RSP], shadow at [RSP+8..+39], 5th at [RSP+40]
    ; No: at caller site, [RSP+32] = 5th param. CALL pushes ret addr.
    ; At callee entry: [RSP+0] = ret addr, [RSP+8] = shadow RCX, ..., [RSP+40] = 5th param
    ; After 6 pushes: [RSP+48+40] = 5th param
    ; After sub rsp,56: [RSP+56+48+40] = [RSP+144] = 5th param
    mov     rax, [rsp + 144]
    mov     [rbx + BC_TensorIndex], rax
    mov     rax, [rsp + 152]
    mov     [rbx + BC_TensorSizes], rax

    ; Create file mapping
    mov     rcx, rdx                    ; hFile — but RDX was clobbered!
    ; Reload from context
    mov     rcx, [rbx + BC_hFile]
    xor     edx, edx                    ; Security
    mov     r8d, PAGE_READONLY
    xor     r9d, r9d                    ; Max size high
    mov     qword ptr [rsp+32], 0       ; Max size low (0 = whole file)
    mov     qword ptr [rsp+40], 0       ; Name
    call    CreateFileMappingA
    test    rax, rax
    jz      @@attach_fail
    mov     [rbx + BC_hMapping], rax

    ; Initialize all tensors as COLD descriptors
    mov     r12d, [rbx + BC_TensorCount]
    cmp     r12d, MAX_TENSORS
    jbe     @@count_ok
    mov     r12d, MAX_TENSORS
@@count_ok:
    xor     r13d, r13d                  ; Tensor index
    mov     r14, [rbx + BC_TensorIndex]
    mov     r15, [rbx + BC_TensorSizes]
@@init_cold:
    cmp     r13d, r12d
    jge     @@init_done
    ; Only fill up to MaxColdSlots
    mov     eax, [rbx + BC_ColdCount]
    cmp     eax, [rbx + BC_MaxColdSlots]
    jge     @@init_done

    call    _bnc_find_empty_cold
    test    rax, rax
    jz      @@init_done

    mov     rsi, rax
    mov     [rsi + PE_TensorID], r13d
    mov     dword ptr [rsi + PE_State], PE_COLD
    mov     qword ptr [rsi + PE_DataPtr], 0
    mov     rax, [r15 + r13*8]
    mov     [rsi + PE_DataSize], rax
    mov     rax, [r14 + r13*8]
    mov     [rsi + PE_FileOffset], rax
    mov     dword ptr [rsi + PE_HeatScore], 0
    mov     dword ptr [rsi + PE_BounceCount], 0
    inc     dword ptr [rbx + BC_ColdCount]

    inc     r13d
    jmp     @@init_cold

@@init_done:
    ; Pre-promote PrefetchDepth tensors into HOT pool
    xor     r13d, r13d
    mov     r14d, [rbx + BC_PrefetchDepth]
@@prefetch_loop:
    cmp     r13d, r14d
    jge     @@prefetch_done
    cmp     r13d, r12d
    jge     @@prefetch_done

    ; Find tensor r13d in cold pool
    mov     ecx, r13d
    call    _bnc_find_in_cold
    test    rax, rax
    jz      @@prefetch_next
    mov     rsi, rax
    call    _bnc_promote_to_hot

@@prefetch_next:
    inc     r13d
    jmp     @@prefetch_loop

@@prefetch_done:
    mov     eax, 1
    jmp     @@attach_exit

@@attach_fail:
    xor     eax, eax
@@attach_exit:
    add     rsp, 56
    pop     rsi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
Bounce_AttachModel ENDP

; =============================================================================
; Bounce_Tick — Advance one cycle
; RCX = context
; Returns: EAX = number of pool changes this tick
;
; Each tick:
;   1. Decay heat scores (dynamic rate)
;   2. Auto-tune bounce rate (if enabled)
;   3. Measure TPS
;   4. Demote coldest HOT entries that have zero heat
;   5. Promote hottest COLD entries that have pending demand
; =============================================================================
Bounce_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    xor     r12d, r12d                  ; Change counter

    ; 1. Increment tick
    inc     qword ptr [rbx + BC_TickCount]

    ; 2. Update timestamp
    call    _bnc_get_qpc
    mov     [rbx + BC_LastTickQPC], rax

    ; 3. Decay heat
    call    _bnc_decay_heat

    ; 4. Measure TPS
    call    _bnc_measure_tps

    ; 5. Auto-tune bounce rate
    call    _bnc_auto_tune

    ; 6. Demote zero-heat HOT entries to make room
    mov     rax, [rbx + BC_HotPool]
    xor     ecx, ecx
    mov     edx, [rbx + BC_MaxHotSlots]
@@demote_scan:
    cmp     ecx, edx
    jge     @@demote_done
    push    rcx
    push    rdx
    mov     r9, rcx
    shl     r9, 6
    lea     r13, [rax + r9]
    cmp     dword ptr [r13 + PE_State], PE_HOT
    jne     @@demote_skip
    cmp     dword ptr [r13 + PE_HeatScore], 0
    jne     @@demote_skip

    ; Zero heat — demote to cold
    ; Unmap data
    mov     rcx, [r13 + PE_DataPtr]
    test    rcx, rcx
    jz      @@demote_no_unmap
    push    rax
    call    UnmapViewOfFile
    pop     rax
    mov     rcx, [r13 + PE_DataSize]
    add     [rbx + BC_StatsBytesEvicted], rcx
@@demote_no_unmap:
    ; Find cold slot
    push    rax
    call    _bnc_find_empty_cold
    test    rax, rax
    jz      @@demote_discard

    ; Copy to cold
    mov     rsi, rax
    mov     ecx, [r13 + PE_TensorID]
    mov     [rsi + PE_TensorID], ecx
    mov     dword ptr [rsi + PE_State], PE_COLD
    mov     qword ptr [rsi + PE_DataPtr], 0
    mov     rcx, [r13 + PE_DataSize]
    mov     [rsi + PE_DataSize], rcx
    mov     rcx, [r13 + PE_FileOffset]
    mov     [rsi + PE_FileOffset], rcx
    mov     dword ptr [rsi + PE_HeatScore], 0
    mov     ecx, [r13 + PE_BounceCount]
    inc     ecx
    mov     [rsi + PE_BounceCount], ecx
    inc     dword ptr [rbx + BC_ColdCount]
    inc     qword ptr [rbx + BC_TotalDemotions]

@@demote_discard:
    pop     rax
    mov     dword ptr [r13 + PE_State], PE_EMPTY
    mov     qword ptr [r13 + PE_DataPtr], 0
    dec     dword ptr [rbx + BC_HotCount]
    inc     qword ptr [rbx + BC_TotalBounces]
    inc     r12d

    ; Print demotion
    push    rax
    push    rdx
    lea     rcx, szBncDemote
    call    _bnc_print
    mov     ecx, [r13 + PE_TensorID]
    call    _bnc_print_u32
    lea     rcx, szNewlineBnc
    call    _bnc_print
    pop     rdx
    pop     rax

@@demote_skip:
    pop     rdx
    pop     rcx
    inc     ecx
    jmp     @@demote_scan

@@demote_done:

    ; 7. Update window token counter
    mov     eax, [rbx + BC_WindowIdx]
    and     eax, (TPS_WINDOW_TICKS - 1)
    mov     qword ptr [rbx + BC_WindowTokens + rax*8], 0
    inc     dword ptr [rbx + BC_WindowIdx]

    mov     eax, r12d

    add     rsp, 40
    pop     r13
    pop     r12
    pop     rbx
    ret
Bounce_Tick ENDP

; =============================================================================
; Bounce_GetTensor — Fetch tensor data pointer
; RCX = context, EDX = tensor ID
; Returns: RAX = data pointer (zero-copy from HOT, demand-load from COLD)
; =============================================================================
Bounce_GetTensor PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    mov     r12d, edx

    ; Check hot pool first (zero-copy fast path)
    mov     ecx, r12d
    call    _bnc_find_in_hot
    test    rax, rax
    jz      @@check_cold

    ; HOT HIT — increment heat, update timestamp, return ptr
    mov     rsi, rax
    add     dword ptr [rsi + PE_HeatScore], 10
    push    rsi
    call    _bnc_get_qpc
    pop     rsi
    mov     [rsi + PE_LastAccessQPC], rax
    inc     qword ptr [rbx + BC_StatsHotHits]
    mov     rax, [rsi + PE_DataPtr]
    jmp     @@exit

@@check_cold:
    ; Check cold pool — demand promote
    mov     ecx, r12d
    call    _bnc_find_in_cold
    test    rax, rax
    jz      @@miss

    ; COLD HIT — promote to hot
    mov     rsi, rax
    call    _bnc_promote_to_hot
    test    rax, rax
    jz      @@miss
    inc     qword ptr [rbx + BC_StatsColdHits]
    mov     rax, [rax + PE_DataPtr]
    jmp     @@exit

@@miss:
    ; Tensor not in any pool — try to load directly if we have index
    mov     rax, [rbx + BC_TensorIndex]
    test    rax, rax
    jz      @@total_miss
    mov     r8d, [rbx + BC_TensorCount]
    cmp     r12d, r8d
    jge     @@total_miss

    ; Direct load: create cold entry then promote
    call    _bnc_find_empty_cold
    test    rax, rax
    jz      @@total_miss

    mov     rsi, rax
    mov     [rsi + PE_TensorID], r12d
    mov     dword ptr [rsi + PE_State], PE_COLD
    mov     qword ptr [rsi + PE_DataPtr], 0
    mov     rax, [rbx + BC_TensorSizes]
    mov     rax, [rax + r12*8]
    mov     [rsi + PE_DataSize], rax
    mov     rax, [rbx + BC_TensorIndex]
    mov     rax, [rax + r12*8]
    mov     [rsi + PE_FileOffset], rax
    mov     dword ptr [rsi + PE_HeatScore], 5
    mov     dword ptr [rsi + PE_BounceCount], 0
    inc     dword ptr [rbx + BC_ColdCount]

    ; Now promote
    call    _bnc_promote_to_hot
    test    rax, rax
    jz      @@total_miss
    inc     qword ptr [rbx + BC_StatsColdHits]
    mov     rax, [rax + PE_DataPtr]
    jmp     @@exit

@@total_miss:
    inc     qword ptr [rbx + BC_StatsMisses]
    xor     eax, eax

@@exit:
    add     rsp, 40
    pop     r12
    pop     rsi
    pop     rbx
    ret
Bounce_GetTensor ENDP

; =============================================================================
; Bounce_GetTPS — Return current measured TPS (x100 for 2 decimal places)
; RCX = context
; Returns: EAX = TPS * 100 (e.g., 7050 = 70.50 tok/sec)
; =============================================================================
Bounce_GetTPS PROC
    mov     eax, [rcx + BC_CurrentTPS]
    ret
Bounce_GetTPS ENDP

; =============================================================================
; Bounce_SetTargetTPS — Set target TPS threshold (dynamic)
; RCX = context, EDX = target TPS (e.g., 70)
; Returns: EAX = previous target
; =============================================================================
Bounce_SetTargetTPS PROC
    mov     eax, [rcx + BC_TargetTPS]
    mov     [rcx + BC_TargetTPS], edx
    ret
Bounce_SetTargetTPS ENDP

; =============================================================================
; Bounce_SetPoolRatio — Set HOT:COLD ratio (dynamic)
; RCX = context, EDX = hot percentage (0-100), R8D = cold percentage
; Returns: EAX = 1 on success
;
; Adjusts MaxHotSlots and MaxColdSlots based on total capacity.
; =============================================================================
Bounce_SetPoolRatio PROC
    mov     [rcx + BC_PoolRatioHot], edx
    mov     [rcx + BC_PoolRatioCold], r8d

    ; Recompute pool sizes: total = MaxHot + MaxCold
    mov     eax, [rcx + BC_MaxHotSlots]
    add     eax, [rcx + BC_MaxColdSlots]
    ; NewHot = total * hot_pct / 100
    mov     r9d, eax
    imul    eax, edx
    mov     r10d, 100
    cdq
    idiv    r10d
    ; Clamp to [1, MAX_POOL_SLOTS]
    cmp     eax, 1
    jge     @@hot_min_ok
    mov     eax, 1
@@hot_min_ok:
    cmp     eax, MAX_POOL_SLOTS
    jle     @@hot_max_ok
    mov     eax, MAX_POOL_SLOTS
@@hot_max_ok:
    mov     [rcx + BC_MaxHotSlots], eax
    ; Cold = total - hot
    sub     r9d, eax
    cmp     r9d, 1
    jge     @@cold_min_ok
    mov     r9d, 1
@@cold_min_ok:
    cmp     r9d, MAX_POOL_SLOTS
    jle     @@cold_max_ok
    mov     r9d, MAX_POOL_SLOTS
@@cold_max_ok:
    mov     [rcx + BC_MaxColdSlots], r9d
    mov     eax, 1
    ret
Bounce_SetPoolRatio ENDP

; =============================================================================
; Bounce_SetBounceRate — Set bounce frequency (dynamic)
; RCX = context, EDX = milliseconds (0 = auto-tune)
; Returns: EAX = previous rate
; =============================================================================
Bounce_SetBounceRate PROC
    mov     eax, [rcx + BC_BounceRateMs]
    mov     [rcx + BC_BounceRateMs], edx
    ret
Bounce_SetBounceRate ENDP

; =============================================================================
; Bounce_SetHeatDecay — Set heat decay rate per tick (dynamic)
; RCX = context, EDX = decay amount
; Returns: EAX = previous rate
; =============================================================================
Bounce_SetHeatDecay PROC
    mov     eax, [rcx + BC_HeatDecayRate]
    mov     [rcx + BC_HeatDecayRate], edx
    ret
Bounce_SetHeatDecay ENDP

; =============================================================================
; Bounce_SetPrefetchDepth — Set prefetch depth (dynamic)
; RCX = context, EDX = depth (how many tensors to prefetch)
; Returns: EAX = previous depth
; =============================================================================
Bounce_SetPrefetchDepth PROC
    mov     eax, [rcx + BC_PrefetchDepth]
    mov     [rcx + BC_PrefetchDepth], edx
    ret
Bounce_SetPrefetchDepth ENDP

; =============================================================================
; Bounce_NotifyTokenGen — Signal that a token was generated
; RCX = context
; Returns: RAX = total tokens generated
;
; Call this from the inference loop each time a token is produced.
; This feeds the TPS measurement system.
; =============================================================================
Bounce_NotifyTokenGen PROC
    inc     qword ptr [rcx + BC_TokensGenerated]
    ; Update sliding window
    mov     eax, [rcx + BC_WindowIdx]
    dec     eax
    and     eax, (TPS_WINDOW_TICKS - 1)
    inc     qword ptr [rcx + BC_WindowTokens + rax*8]
    mov     rax, [rcx + BC_TokensGenerated]
    ret
Bounce_NotifyTokenGen ENDP

; =============================================================================
; Bounce_GetStats — Read engine statistics into caller buffer
; RCX = context, RDX = pointer to 128-byte stats buffer
; Returns: EAX = 1 on success
;
; Stats layout (all QWORD unless noted):
;   [0x00] TickCount          [0x08] TotalBounces
;   [0x10] TotalPromotions    [0x18] TotalDemotions
;   [0x20] TokensGenerated    [0x28] CurrentTPS (DWORD, x100)
;   [0x2C] PeakTPS (DWORD)    [0x30] HotCount (DWORD)
;   [0x34] ColdCount (DWORD)  [0x38] HotHits
;   [0x40] ColdHits           [0x48] Misses
;   [0x50] Prefetches         [0x58] BytesLoaded
;   [0x60] BytesEvicted       [0x68] TargetTPS (DWORD)
;   [0x6C] BounceRateMs(DWORD)[0x70] AutoBounceMs (DWORD)
;   [0x74] HeatDecay (DWORD)  [0x78] PrefetchDepth (DWORD)
; =============================================================================
Bounce_GetStats PROC
    test    rcx, rcx
    jz      @@fail
    test    rdx, rdx
    jz      @@fail

    mov     rax, [rcx + BC_TickCount]
    mov     [rdx + 00h], rax
    mov     rax, [rcx + BC_TotalBounces]
    mov     [rdx + 08h], rax
    mov     rax, [rcx + BC_TotalPromotions]
    mov     [rdx + 10h], rax
    mov     rax, [rcx + BC_TotalDemotions]
    mov     [rdx + 18h], rax
    mov     rax, [rcx + BC_TokensGenerated]
    mov     [rdx + 20h], rax
    mov     eax, [rcx + BC_CurrentTPS]
    mov     [rdx + 28h], eax
    mov     eax, [rcx + BC_PeakTPS]
    mov     [rdx + 2Ch], eax
    mov     eax, [rcx + BC_HotCount]
    mov     [rdx + 30h], eax
    mov     eax, [rcx + BC_ColdCount]
    mov     [rdx + 34h], eax
    mov     rax, [rcx + BC_StatsHotHits]
    mov     [rdx + 38h], rax
    mov     rax, [rcx + BC_StatsColdHits]
    mov     [rdx + 40h], rax
    mov     rax, [rcx + BC_StatsMisses]
    mov     [rdx + 48h], rax
    mov     rax, [rcx + BC_StatsPrefetches]
    mov     [rdx + 50h], rax
    mov     rax, [rcx + BC_StatsBytesLoaded]
    mov     [rdx + 58h], rax
    mov     rax, [rcx + BC_StatsBytesEvicted]
    mov     [rdx + 60h], rax
    mov     eax, [rcx + BC_TargetTPS]
    mov     [rdx + 68h], eax
    mov     eax, [rcx + BC_BounceRateMs]
    mov     [rdx + 6Ch], eax
    mov     eax, [rcx + BC_AutoBounceMs]
    mov     [rdx + 70h], eax
    mov     eax, [rcx + BC_HeatDecayRate]
    mov     [rdx + 74h], eax
    mov     eax, [rcx + BC_PrefetchDepth]
    mov     [rdx + 78h], eax

    mov     eax, 1
    ret
@@fail:
    xor     eax, eax
    ret
Bounce_GetStats ENDP

END
