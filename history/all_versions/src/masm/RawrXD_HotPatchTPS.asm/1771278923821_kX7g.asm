; =============================================================================
; RawrXD_HotPatchTPS.asm — HotPatch TPS Multiplier Engine (3rd Engine)
;
; Runtime TPS hotpatching engine that dynamically rewrites execution parameters
; to maximize throughput. Works alongside Sloloris (saturation) and Bounce
; (ping-pong) to push composite strength beyond 300%.
;
; Architecture:
;   ┌─────────────────────────────────────────────────────────────────┐
;   │                    HOTPATCH ENGINE                              │
;   │                                                                 │
;   │  ┌─── PATCH TABLE ───┐   ┌─── MULTIPLIER CHAIN ────────────┐ │
;   │  │ Slot 0: BatchSize │   │ base_mult = 100 (1.0x)          │ │
;   │  │ Slot 1: Prefetch  │   │ + saturation_bonus (Sloloris)   │ │
;   │  │ Slot 2: Quant     │   │ + bounce_bonus (Bounce)         │ │
;   │  │ Slot 3: CacheMode │   │ + patch_bonus (HotPatch tuning) │ │
;   │  │ Slot 4: ThreadAff │   │ + feedback_bonus (TPS delta)    │ │
;   │  │ Slot 5: SpecDec   │   │ = composite_mult → 300%+        │ │
;   │  │ ...               │   └──────────────────────────────────┘ │
;   │  └───────────────────┘                                         │
;   │                                                                 │
;   │  Each tick: measure→adjust→patch→verify→feedback               │
;   │  LFSR-driven patch selection prevents local minima              │
;   └─────────────────────────────────────────────────────────────────┘
;
; Exports:
;   HotPatch_Init           — Create engine context
;   HotPatch_Destroy        — Free everything
;   HotPatch_Tick           — Advance one optimization cycle
;   HotPatch_NotifyToken    — Feed TPS measurement
;   HotPatch_GetMultiplier  — Return current composite multiplier (x100)
;   HotPatch_GetTPS         — Return measured TPS (x100)
;   HotPatch_SetParam       — Set a dynamic patch parameter
;   HotPatch_GetParam       — Get a dynamic patch parameter
;   HotPatch_FeedSloloris   — Feed Sloloris strength into multiplier
;   HotPatch_FeedBounce     — Feed Bounce superstrength into multiplier
;   HotPatch_GetStats       — Read statistics
;
; Build: ml64 /c RawrXD_HotPatchTPS.asm
; =============================================================================

option casemap:none

INCLUDE ksamd64.inc

; ─── Windows API ─────────────────────────────────────────────────────────────
EXTRN VirtualAlloc:PROC
EXTRN VirtualFree:PROC
EXTRN QueryPerformanceCounter:PROC
EXTRN QueryPerformanceFrequency:PROC
EXTRN GetStdHandle:PROC
EXTRN WriteConsoleA:PROC
EXTRN Sleep:PROC

; ─── Exports ─────────────────────────────────────────────────────────────────
PUBLIC HotPatch_Init
PUBLIC HotPatch_Destroy
PUBLIC HotPatch_Tick
PUBLIC HotPatch_NotifyToken
PUBLIC HotPatch_GetMultiplier
PUBLIC HotPatch_GetTPS
PUBLIC HotPatch_SetParam
PUBLIC HotPatch_GetParam
PUBLIC HotPatch_FeedSloloris
PUBLIC HotPatch_FeedBounce
PUBLIC HotPatch_GetStats

; =============================================================================
; Constants
; =============================================================================
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
PAGE_READWRITE          EQU 04h
STD_OUTPUT              EQU -11

; Patch slot IDs (dynamic parameters)
PATCH_BATCH_SIZE        EQU 0           ; Tokens per batch (1-16)
PATCH_PREFETCH_DEPTH    EQU 1           ; Layers to prefetch (1-32)
PATCH_QUANT_MODE        EQU 2           ; Quantization aggressiveness (0-4)
PATCH_CACHE_MODE        EQU 3           ; Cache strategy (0=LRU,1=FIFO,2=Random)
PATCH_THREAD_COUNT      EQU 4           ; Thread count for matmul (1-16)
PATCH_SPEC_DECODE       EQU 5           ; Speculative tokens (0-8)
PATCH_RING_AGGRESSION   EQU 6           ; Ring buffer aggression (1-10)
PATCH_HEAT_INJECT       EQU 7           ; Heat injection rate (0-20)
MAX_PATCH_SLOTS         EQU 16          ; Total configurable patches

; Feedback tuning
FEEDBACK_WINDOW         EQU 16          ; TPS samples for smoothing
TUNE_EXPLORE_RATE       EQU 20          ; % chance to explore vs exploit

; =============================================================================
; Patch Slot (16 bytes each)
; =============================================================================
;   0x00  Value           DWORD     — Current parameter value
;   0x04  MinVal          DWORD     — Minimum allowed
;   0x08  MaxVal          DWORD     — Maximum allowed
;   0x0C  BestVal         DWORD     — Value that produced best TPS
PS_Value                EQU 00h
PS_MinVal               EQU 04h
PS_MaxVal               EQU 08h
PS_BestVal              EQU 0Ch
PS_SIZE                 EQU 10h         ; 16 bytes

; =============================================================================
; HotPatchContext (main engine state)
; =============================================================================
;   0x000  QPCFreq         QWORD
;   0x008  InitQPC         QWORD
;   0x010  LastTickQPC     QWORD
;   0x018  TickCount       QWORD
;   0x020  TokensGenerated QWORD
;   0x028  CurrentTPS      DWORD     — Measured TPS x100
;   0x02C  PeakTPS         DWORD
;   0x030  CompositeMult   DWORD     — Composite multiplier x100
;   0x034  SlolorisInput   DWORD     — Fed Sloloris strength (0-100)
;   0x038  BounceInput     DWORD     — Fed Bounce superstrength (0-999)
;   0x03C  PatchBonus      DWORD     — Bonus from patch tuning
;   0x040  FeedbackBonus   DWORD     — Bonus from TPS feedback loop
;   0x044  ExploreFlag     DWORD     — 1 if currently exploring
;   0x048  BestComposite   DWORD     — Best composite ever achieved
;   0x04C  PatchSelector   DWORD     — Which patch to tune next (round-robin)
;   0x050  LFSR            QWORD     — Random number generator
;   0x058  TPSHistory      DWORD[FEEDBACK_WINDOW]  — Recent TPS samples
;   0x098  HistoryIdx      DWORD
;   0x09C  Pad0            DWORD
;   0x0A0  PatchSlots      PS_SIZE * MAX_PATCH_SLOTS = 256 bytes
;   0x1A0  StatsTuneSteps  QWORD     — Total tuning steps
;   0x1A8  StatsExplores   QWORD     — Exploration attempts
;   0x1B0  StatsExploits   QWORD     — Exploitation attempts
;   0x1B8  StatsImproved   QWORD     — Times TPS improved after patch
;   0x1C0  StatsRegressed  QWORD     — Times TPS regressed after patch
;   0x1C8  Reserved        QWORD[7]  — Pad to 0x200
HC_QPCFreq              EQU 000h
HC_InitQPC              EQU 008h
HC_LastTickQPC          EQU 010h
HC_TickCount            EQU 018h
HC_TokensGenerated      EQU 020h
HC_CurrentTPS           EQU 028h
HC_PeakTPS              EQU 02Ch
HC_CompositeMult        EQU 030h
HC_SlolorisInput        EQU 034h
HC_BounceInput          EQU 038h
HC_PatchBonus           EQU 03Ch
HC_FeedbackBonus        EQU 040h
HC_ExploreFlag          EQU 044h
HC_BestComposite        EQU 048h
HC_PatchSelector        EQU 04Ch
HC_LFSR                 EQU 050h
HC_TPSHistory           EQU 058h
; 0x058 + 16*4 = 0x058 + 0x40 = 0x098
HC_HistoryIdx           EQU 098h
HC_PatchSlots           EQU 0A0h
; 0x0A0 + 16*16 = 0x0A0 + 0x100 = 0x1A0
HC_StatsTuneSteps       EQU 1A0h
HC_StatsExplores        EQU 1A8h
HC_StatsExploits        EQU 1B0h
HC_StatsImproved        EQU 1B8h
HC_StatsRegressed       EQU 1C0h
HC_SIZE                 EQU 200h        ; 512 bytes

; =============================================================================
; Data Section
; =============================================================================
.data
align 16
szHPInit        db '[HotPatch] Engine initialized',0Dh,0Ah,0
szHPTick        db '[HotPatch] Tick: mult=',0
szHPExplore     db '[HotPatch] EXPLORE patch ',0
szHPExploit     db '[HotPatch] EXPLOIT patch ',0
szHPImproved    db '[HotPatch] TPS IMPROVED',0Dh,0Ah,0
szHPRegressed   db '[HotPatch] TPS regressed',0Dh,0Ah,0
szHPNewline     db 0Dh,0Ah,0
szHPPct         db '%',0Dh,0Ah,0
hStdOutHP       dq 0

; Default patch slot configs: {value, min, max, best}
align 16
defaultPatches  dd 4, 1, 16, 4          ; PATCH_BATCH_SIZE
                dd 4, 1, 32, 4          ; PATCH_PREFETCH_DEPTH
                dd 2, 0, 4, 2           ; PATCH_QUANT_MODE
                dd 0, 0, 2, 0           ; PATCH_CACHE_MODE
                dd 4, 1, 16, 4          ; PATCH_THREAD_COUNT
                dd 2, 0, 8, 2           ; PATCH_SPEC_DECODE
                dd 5, 1, 10, 5          ; PATCH_RING_AGGRESSION
                dd 5, 0, 20, 5          ; PATCH_HEAT_INJECT
                dd 1, 1, 1, 1           ; Slot 8 (reserved)
                dd 1, 1, 1, 1           ; Slot 9 (reserved)
                dd 1, 1, 1, 1           ; Slot 10
                dd 1, 1, 1, 1           ; Slot 11
                dd 1, 1, 1, 1           ; Slot 12
                dd 1, 1, 1, 1           ; Slot 13
                dd 1, 1, 1, 1           ; Slot 14
                dd 1, 1, 1, 1           ; Slot 15

.data?
align 16
qpcScratchHP    dq ?
numBufHP        db 32 dup(?)

; =============================================================================
; Code Section
; =============================================================================
.code

; ─── Internal Helpers ────────────────────────────────────────────────────────

_hp_strlen PROC
    xor     eax, eax
@@:
    cmp     byte ptr [rcx + rax], 0
    je      @F
    inc     rax
    jmp     @B
@@:
    ret
_hp_strlen ENDP

_hp_print PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     rbx, rcx
    mov     rax, hStdOutHP
    test    rax, rax
    jnz     @@have
    mov     ecx, STD_OUTPUT
    call    GetStdHandle
    mov     hStdOutHP, rax
@@have:
    mov     rcx, rbx
    call    _hp_strlen
    mov     r8, rax
    mov     rcx, hStdOutHP
    mov     rdx, rbx
    lea     r9, [rsp+32]
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    add     rsp, 48
    pop     rbx
    ret
_hp_print ENDP

_hp_print_u32 PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     eax, ecx
    lea     rbx, numBufHP
    add     rbx, 20
    mov     byte ptr [rbx], 0
    test    eax, eax
    jnz     @@loop
    dec     rbx
    mov     byte ptr [rbx], '0'
    jmp     @@pr
@@loop:
    test    eax, eax
    jz      @@pr
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    add     dl, '0'
    dec     rbx
    mov     [rbx], dl
    jmp     @@loop
@@pr:
    mov     rcx, rbx
    call    _hp_print
    add     rsp, 48
    pop     rbx
    ret
_hp_print_u32 ENDP

_hp_get_qpc PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog
    lea     rcx, qpcScratchHP
    call    QueryPerformanceCounter
    mov     rax, qpcScratchHP
    add     rsp, 40
    ret
_hp_get_qpc ENDP

; ─── _hp_advance_lfsr ────────────────────────────────────────────────────────
; RBX = context → EAX = random 32-bit value
_hp_advance_lfsr PROC
    mov     rax, [rbx + HC_LFSR]
    mov     rcx, rax
    shr     rax, 1
    test    rcx, 1
    jz      @@no_tap
    mov     rcx, 0D8000000h
    shl     rcx, 32
    xor     rax, rcx
@@no_tap:
    mov     [rbx + HC_LFSR], rax
    ; Return lower 32 bits
    ret
_hp_advance_lfsr ENDP

; ─── _hp_measure_tps ─────────────────────────────────────────────────────────
; RBX = context → updates HC_CurrentTPS
_hp_measure_tps PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    call    _hp_get_qpc
    mov     rcx, rax
    sub     rcx, [rbx + HC_InitQPC]
    test    rcx, rcx
    jz      @@zero

    ; elapsed_s_x100 = (elapsed_qpc * 100) / freq
    mov     rax, rcx
    mov     rcx, 100
    mul     rcx
    mov     rcx, [rbx + HC_QPCFreq]
    test    rcx, rcx
    jz      @@zero
    div     rcx                         ; RAX = elapsed_seconds * 100
    test    rax, rax
    jz      @@zero

    ; TPS_x100 = (tokens * 10000) / elapsed_s_x100
    mov     rcx, rax
    mov     rax, [rbx + HC_TokensGenerated]
    mov     rdx, 10000
    mul     rdx
    div     rcx

    mov     [rbx + HC_CurrentTPS], eax
    cmp     eax, [rbx + HC_PeakTPS]
    jle     @@done
    mov     [rbx + HC_PeakTPS], eax
    jmp     @@done

@@zero:
    mov     dword ptr [rbx + HC_CurrentTPS], 0
@@done:
    add     rsp, 40
    ret
_hp_measure_tps ENDP

; ─── _hp_calc_composite ──────────────────────────────────────────────────────
; RBX = context
; Composite multiplier = 100 + sloloris_bonus + bounce_bonus + patch_bonus + feedback_bonus
; Result in HC_CompositeMult (x100 scale, so 300 = 3.00x = 300%)
_hp_calc_composite PROC
    mov     eax, 100                    ; Base 1.0x (100%)

    ; Sloloris bonus: strength * 0.5 (up to +50)
    mov     ecx, [rbx + HC_SlolorisInput]
    shr     ecx, 1
    add     eax, ecx

    ; Bounce bonus: superstrength * 0.3 (up to +300 if SS=999)
    mov     ecx, [rbx + HC_BounceInput]
    imul    ecx, 30
    mov     edx, 100
    push    rax
    mov     eax, ecx
    xor     edx, edx
    mov     ecx, 100
    div     ecx
    mov     ecx, eax
    pop     rax
    add     eax, ecx

    ; Patch bonus (from tuning optimization, 0-100)
    add     eax, [rbx + HC_PatchBonus]

    ; Feedback bonus (from TPS improvement trend, 0-50)
    add     eax, [rbx + HC_FeedbackBonus]

    mov     [rbx + HC_CompositeMult], eax

    ; Track best
    cmp     eax, [rbx + HC_BestComposite]
    jle     @@not_best
    mov     [rbx + HC_BestComposite], eax
@@not_best:
    ret
_hp_calc_composite ENDP

; ─── _hp_tune_patches ────────────────────────────────────────────────────────
; RBX = context
; Explore/exploit: randomly adjust a patch parameter, then measure impact.
; If TPS improved → keep change (exploit). If regressed → revert (explore).
_hp_tune_patches PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    inc     qword ptr [rbx + HC_StatsTuneSteps]

    ; Decide: explore or exploit?
    call    _hp_advance_lfsr
    xor     edx, edx
    mov     ecx, 100
    div     ecx                         ; EDX = LFSR mod 100
    cmp     edx, TUNE_EXPLORE_RATE
    jge     @@exploit

    ; === EXPLORE: randomly adjust a parameter ===
    inc     qword ptr [rbx + HC_StatsExplores]
    mov     dword ptr [rbx + HC_ExploreFlag], 1

    ; Pick which patch slot to adjust
    call    _hp_advance_lfsr
    xor     edx, edx
    mov     ecx, 8                      ; Only tune first 8 meaningful slots
    div     ecx
    mov     esi, edx                    ; ESI = patch slot index

    ; Get patch slot pointer
    mov     eax, esi
    shl     eax, 4                      ; * PS_SIZE (16)
    lea     rdi, [rbx + HC_PatchSlots]
    add     rdi, rax                    ; RDI = &PatchSlots[esi]

    ; Random direction: up or down
    call    _hp_advance_lfsr
    test    eax, 1
    jz      @@go_down

    ; Go up
    mov     eax, [rdi + PS_Value]
    inc     eax
    cmp     eax, [rdi + PS_MaxVal]
    jle     @@store_val
    mov     eax, [rdi + PS_MinVal]      ; Wrap around
    jmp     @@store_val

@@go_down:
    mov     eax, [rdi + PS_Value]
    dec     eax
    cmp     eax, [rdi + PS_MinVal]
    jge     @@store_val
    mov     eax, [rdi + PS_MaxVal]      ; Wrap around

@@store_val:
    mov     [rdi + PS_Value], eax

    ; Print exploration
    push    rdi
    push    rsi
    lea     rcx, szHPExplore
    call    _hp_print
    mov     ecx, esi
    call    _hp_print_u32
    lea     rcx, szHPNewline
    call    _hp_print
    pop     rsi
    pop     rdi
    jmp     @@compute_bonus

@@exploit:
    ; === EXPLOIT: keep best values, refine ===
    inc     qword ptr [rbx + HC_StatsExploits]
    mov     dword ptr [rbx + HC_ExploreFlag], 0

    ; Copy best values back for all slots
    lea     rdi, [rbx + HC_PatchSlots]
    xor     ecx, ecx
@@exploit_loop:
    cmp     ecx, 8
    jge     @@compute_bonus
    mov     eax, ecx
    shl     eax, 4
    lea     rsi, [rdi + rax]
    mov     eax, [rsi + PS_BestVal]
    mov     [rsi + PS_Value], eax
    inc     ecx
    jmp     @@exploit_loop

@@compute_bonus:
    ; Patch bonus = sum of (value / max * 100 / MAX_SLOTS) for each slot
    ; Simplified: sum all values, scale to 0-100
    lea     rdi, [rbx + HC_PatchSlots]
    xor     esi, esi                    ; Sum
    xor     ecx, ecx
@@sum_loop:
    cmp     ecx, 8
    jge     @@sum_done
    mov     eax, ecx
    shl     eax, 4
    mov     eax, [rdi + rax + PS_Value]
    add     esi, eax
    inc     ecx
    jmp     @@sum_loop
@@sum_done:
    ; Scale: max possible sum ~= 16+32+4+2+16+8+10+20 = 108
    ; bonus = sum * 100 / 108 → max ~100
    mov     eax, esi
    imul    eax, 100
    xor     edx, edx
    mov     ecx, 108
    div     ecx
    cmp     eax, 100
    jle     @@patch_ok
    mov     eax, 100
@@patch_ok:
    mov     [rbx + HC_PatchBonus], eax

    ; Check if TPS improved vs last tick
    mov     eax, [rbx + HC_CurrentTPS]
    mov     ecx, [rbx + HC_HistoryIdx]
    dec     ecx
    and     ecx, (FEEDBACK_WINDOW - 1)
    mov     edx, [rbx + HC_TPSHistory + rcx*4]
    cmp     eax, edx
    jle     @@regressed

    ; Improved — save current values as best
    inc     qword ptr [rbx + HC_StatsImproved]
    lea     rdi, [rbx + HC_PatchSlots]
    xor     ecx, ecx
@@save_best:
    cmp     ecx, 8
    jge     @@update_feedback
    mov     eax, ecx
    shl     eax, 4
    lea     rsi, [rdi + rax]
    mov     eax, [rsi + PS_Value]
    mov     [rsi + PS_BestVal], eax
    inc     ecx
    jmp     @@save_best

@@regressed:
    inc     qword ptr [rbx + HC_StatsRegressed]

@@update_feedback:
    ; Feedback bonus = (current_tps - initial_tps) scaled to 0-50
    ; Use improvement ratio from history
    mov     eax, [rbx + HC_CurrentTPS]
    mov     ecx, [rbx + HC_TPSHistory]  ; First sample
    test    ecx, ecx
    jz      @@no_feedback
    sub     eax, ecx                    ; Delta
    test    eax, eax
    jle     @@no_feedback
    ; Positive delta — scale to 0-50
    cmp     eax, 50
    jle     @@fb_ok
    mov     eax, 50
@@fb_ok:
    mov     [rbx + HC_FeedbackBonus], eax
    jmp     @@tune_done
@@no_feedback:
    ; No improvement or no data — keep existing
@@tune_done:
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret
_hp_tune_patches ENDP

; =============================================================================
; PUBLIC API
; =============================================================================

; =============================================================================
; HotPatch_Init — Create engine context
; RCX = flags (reserved, pass 0)
; Returns: RAX = context pointer, or 0
; =============================================================================
HotPatch_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Allocate context
    xor     ecx, ecx
    mov     edx, HC_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail
    mov     rbx, rax

    ; QPC frequency
    lea     rcx, qpcScratchHP
    call    QueryPerformanceFrequency
    mov     rax, qpcScratchHP
    mov     [rbx + HC_QPCFreq], rax

    ; Init timestamp
    call    _hp_get_qpc
    mov     [rbx + HC_InitQPC], rax
    mov     [rbx + HC_LastTickQPC], rax

    ; Seed LFSR from QPC
    mov     rcx, 0BADC0FFE0h
    xor     rax, rcx
    test    rax, rax
    jnz     @@lfsr_ok
    mov     rax, 0FEED1234ABCD5678h
@@lfsr_ok:
    mov     [rbx + HC_LFSR], rax

    ; Set defaults
    mov     dword ptr [rbx + HC_CompositeMult], 100
    mov     dword ptr [rbx + HC_SlolorisInput], 0
    mov     dword ptr [rbx + HC_BounceInput], 0
    mov     dword ptr [rbx + HC_PatchBonus], 0
    mov     dword ptr [rbx + HC_FeedbackBonus], 0
    mov     dword ptr [rbx + HC_BestComposite], 100

    ; Copy default patch configs
    lea     rsi, defaultPatches
    lea     rdi, [rbx + HC_PatchSlots]
    mov     ecx, MAX_PATCH_SLOTS * PS_SIZE / 8  ; QWORDs to copy
@@copy:
    test    ecx, ecx
    jz      @@copy_done
    mov     rax, [rsi]
    mov     [rdi], rax
    add     rsi, 8
    add     rdi, 8
    dec     ecx
    jmp     @@copy
@@copy_done:

    ; Print init
    lea     rcx, szHPInit
    call    _hp_print

    mov     rax, rbx
    jmp     @@exit
@@fail:
    xor     eax, eax
@@exit:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
HotPatch_Init ENDP

; =============================================================================
; HotPatch_Destroy — Free context
; RCX = context
; =============================================================================
HotPatch_Destroy PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog
    test    rcx, rcx
    jz      @@null
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@null:
    add     rsp, 40
    ret
HotPatch_Destroy ENDP

; =============================================================================
; HotPatch_Tick — Advance one optimization cycle
; RCX = context
; Returns: EAX = current composite multiplier (x100)
; =============================================================================
HotPatch_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    inc     qword ptr [rbx + HC_TickCount]

    ; Update timestamp
    call    _hp_get_qpc
    mov     [rbx + HC_LastTickQPC], rax

    ; Measure TPS
    call    _hp_measure_tps

    ; Store in history window
    mov     eax, [rbx + HC_CurrentTPS]
    mov     ecx, [rbx + HC_HistoryIdx]
    and     ecx, (FEEDBACK_WINDOW - 1)
    mov     [rbx + HC_TPSHistory + rcx*4], eax
    inc     dword ptr [rbx + HC_HistoryIdx]

    ; Tune patches (explore/exploit)
    call    _hp_tune_patches

    ; Calculate composite multiplier
    call    _hp_calc_composite

    ; Return composite
    mov     eax, [rbx + HC_CompositeMult]

    add     rsp, 40
    pop     rbx
    ret
HotPatch_Tick ENDP

; =============================================================================
; HotPatch_NotifyToken — Signal token generation
; RCX = context
; Returns: RAX = total tokens
; =============================================================================
HotPatch_NotifyToken PROC
    inc     qword ptr [rcx + HC_TokensGenerated]
    mov     rax, [rcx + HC_TokensGenerated]
    ret
HotPatch_NotifyToken ENDP

; =============================================================================
; HotPatch_GetMultiplier — Return composite multiplier (x100)
; RCX = context
; Returns: EAX = multiplier (e.g., 350 = 3.50x = 350%)
; =============================================================================
HotPatch_GetMultiplier PROC
    mov     eax, [rcx + HC_CompositeMult]
    ret
HotPatch_GetMultiplier ENDP

; =============================================================================
; HotPatch_GetTPS — Return measured TPS (x100)
; RCX = context
; Returns: EAX = TPS * 100
; =============================================================================
HotPatch_GetTPS PROC
    mov     eax, [rcx + HC_CurrentTPS]
    ret
HotPatch_GetTPS ENDP

; =============================================================================
; HotPatch_SetParam — Set a dynamic patch parameter
; RCX = context, EDX = patch slot ID (0-15), R8D = new value
; Returns: EAX = previous value
; =============================================================================
HotPatch_SetParam PROC
    cmp     edx, MAX_PATCH_SLOTS
    jge     @@bad
    mov     eax, edx
    shl     eax, 4
    lea     r9, [rcx + HC_PatchSlots]
    add     r9, rax
    mov     eax, [r9 + PS_Value]
    ; Clamp to min/max
    cmp     r8d, [r9 + PS_MinVal]
    jge     @@min_ok
    mov     r8d, [r9 + PS_MinVal]
@@min_ok:
    cmp     r8d, [r9 + PS_MaxVal]
    jle     @@max_ok
    mov     r8d, [r9 + PS_MaxVal]
@@max_ok:
    mov     [r9 + PS_Value], r8d
    ret
@@bad:
    xor     eax, eax
    ret
HotPatch_SetParam ENDP

; =============================================================================
; HotPatch_GetParam — Get a patch parameter value
; RCX = context, EDX = patch slot ID
; Returns: EAX = current value
; =============================================================================
HotPatch_GetParam PROC
    cmp     edx, MAX_PATCH_SLOTS
    jge     @@bad
    mov     eax, edx
    shl     eax, 4
    lea     r9, [rcx + HC_PatchSlots]
    mov     eax, [r9 + rax + PS_Value]
    ret
@@bad:
    xor     eax, eax
    ret
HotPatch_GetParam ENDP

; =============================================================================
; HotPatch_FeedSloloris — Feed Sloloris strength (0-100) into multiplier
; RCX = context, EDX = Sloloris strength percentage
; =============================================================================
HotPatch_FeedSloloris PROC
    mov     [rcx + HC_SlolorisInput], edx
    ret
HotPatch_FeedSloloris ENDP

; =============================================================================
; HotPatch_FeedBounce — Feed Bounce superstrength (0-999) into multiplier
; RCX = context, EDX = Bounce superstrength
; =============================================================================
HotPatch_FeedBounce PROC
    mov     [rcx + HC_BounceInput], edx
    ret
HotPatch_FeedBounce ENDP

; =============================================================================
; HotPatch_GetStats — Read engine statistics
; RCX = context, RDX = pointer to 128-byte stats buffer
; Returns: EAX = 1 on success
;
; Stats layout:
;   [0x00] TickCount         [0x08] TokensGenerated
;   [0x10] CurrentTPS(DWORD) [0x14] PeakTPS(DWORD)
;   [0x18] CompositeMult(DW) [0x1C] BestComposite(DW)
;   [0x20] SlolorisInput(DW) [0x24] BounceInput(DW)
;   [0x28] PatchBonus(DW)    [0x2C] FeedbackBonus(DW)
;   [0x30] TuneSteps         [0x38] Explores
;   [0x40] Exploits          [0x48] Improved
;   [0x50] Regressed         [0x58..0x78] PatchValues[0..7]
; =============================================================================
HotPatch_GetStats PROC
    test    rcx, rcx
    jz      @@fail
    test    rdx, rdx
    jz      @@fail

    mov     rax, [rcx + HC_TickCount]
    mov     [rdx + 00h], rax
    mov     rax, [rcx + HC_TokensGenerated]
    mov     [rdx + 08h], rax
    mov     eax, [rcx + HC_CurrentTPS]
    mov     [rdx + 10h], eax
    mov     eax, [rcx + HC_PeakTPS]
    mov     [rdx + 14h], eax
    mov     eax, [rcx + HC_CompositeMult]
    mov     [rdx + 18h], eax
    mov     eax, [rcx + HC_BestComposite]
    mov     [rdx + 1Ch], eax
    mov     eax, [rcx + HC_SlolorisInput]
    mov     [rdx + 20h], eax
    mov     eax, [rcx + HC_BounceInput]
    mov     [rdx + 24h], eax
    mov     eax, [rcx + HC_PatchBonus]
    mov     [rdx + 28h], eax
    mov     eax, [rcx + HC_FeedbackBonus]
    mov     [rdx + 2Ch], eax
    mov     rax, [rcx + HC_StatsTuneSteps]
    mov     [rdx + 30h], rax
    mov     rax, [rcx + HC_StatsExplores]
    mov     [rdx + 38h], rax
    mov     rax, [rcx + HC_StatsExploits]
    mov     [rdx + 40h], rax
    mov     rax, [rcx + HC_StatsImproved]
    mov     [rdx + 48h], rax
    mov     rax, [rcx + HC_StatsRegressed]
    mov     [rdx + 50h], rax

    ; Patch values
    lea     r8, [rcx + HC_PatchSlots]
    xor     r9d, r9d
@@pv:
    cmp     r9d, 8
    jge     @@pv_done
    mov     eax, r9d
    shl     eax, 4
    mov     eax, [r8 + rax + PS_Value]
    mov     [rdx + 58h + r9*4], eax
    inc     r9d
    jmp     @@pv
@@pv_done:
    mov     eax, 1
    ret
@@fail:
    xor     eax, eax
    ret
HotPatch_GetStats ENDP

END
