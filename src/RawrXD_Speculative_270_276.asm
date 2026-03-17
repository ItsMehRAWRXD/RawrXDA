; ============================================================================
; RawrXD_Speculative_270_276.asm — Enhancements 270-276
; Speculative Execution Engine — Draft/Verifier Single-Stream Decode
; ============================================================================
; 270: Spec_Init               — Initialize speculative engine state
; 271: Spec_Register_Pair      — Register draft/verifier pair + policy
; 272: Spec_Draft_Step         — Generate draft burst tokens
; 273: Spec_Verify_Step        — Verify burst, return accepted prefix length
; 274: Spec_Commit             — Commit accepted tokens to output context
; 275: Spec_Telemetry          — Emit speculation metrics / query stats
; 276: Spec_Fallback           — Disable speculation, revert to normal decode
; ============================================================================
; Architecture:
;   Small fast draft model proposes burst of 4-8 tokens.
;   Stronger verifier model validates contiguous accepted prefix.
;   Accepted tokens committed to shared output.
;   Fallback reverts to standard single-token decode if acceptance poor.
;
; ADDR32-safe: all indexed access uses lea+add pattern (no [label+reg])
; ============================================================================

extrn GetTickCount64 : proc

; ── External references from v260-266 ──
EXTERN SwarmV260_Benchmark_Sentinel:PROC
EXTERN SwarmV262_Regression_Gate:PROC
EXTERN SwarmV265_Variance_Clamp:PROC
EXTERN SwarmV266_Production_Gate:PROC

PUBLIC SwarmV270_Spec_Init
PUBLIC SwarmV271_Spec_Register_Pair
PUBLIC SwarmV272_Spec_Draft_Step
PUBLIC SwarmV273_Spec_Verify_Step
PUBLIC SwarmV274_Spec_Commit
PUBLIC SwarmV275_Spec_Telemetry
PUBLIC SwarmV276_Spec_Fallback

.data

; ══════════════════════════════════════════════════════════════════════════════
; Speculative Engine Global State
; ══════════════════════════════════════════════════════════════════════════════

; Engine state machine
SPEC_STATE_DISABLED     EQU 0       ; speculation completely off
SPEC_STATE_IDLE         EQU 1       ; ready, no active cycle
SPEC_STATE_DRAFTING     EQU 2       ; draft model generating burst
SPEC_STATE_VERIFYING    EQU 3       ; verifier checking draft
SPEC_STATE_COMMITTING   EQU 4       ; accepting tokens
SPEC_STATE_FALLBACK     EQU 5       ; reverted to normal decode

ALIGN 16
g_spec_state            DD SPEC_STATE_DISABLED
g_spec_engine_version   DD 270
g_spec_enabled          DD 0        ; master enable flag
g_spec_active_pair_id   DD -1       ; currently active pair (-1=none)
g_spec_total_cycles     DQ 0        ; lifetime cycle count
g_spec_init_timestamp   DQ 0        ; engine init time

; ── Pair Registry ──
MAX_SPEC_PAIRS          EQU 8
SPEC_PAIR_ENTRY_SIZE    EQU 128     ; 128 bytes per pair entry

; Pair entry layout (128 bytes):
;   [0-3]     pair_id             (DWORD)  — unique pair key
;   [4-7]     draft_model_hash    (DWORD)  — FNV-1a hash of draft model name
;   [8-11]    verifier_model_hash (DWORD)  — FNV-1a hash of verifier model name
;   [12-15]   task_class          (DWORD)  — 0=chat,1=code,2=bench,3=reason
;   [16-19]   max_burst_len       (DWORD)  — max draft tokens per cycle (4-8)
;   [20-23]   min_accept_rate     (DWORD)  — minimum accept% (x100, e.g. 4500=45%)
;   [24-27]   best_accept_rate    (DWORD)  — historical best accept% (x100)
;   [28-31]   median_accept_rate  (DWORD)  — running median accept% (x100)
;   [32-35]   best_effective_tps  (DWORD)  — peak effective TPS (x100)
;   [36-39]   median_effective_tps(DWORD)  — operational median effective TPS (x100)
;   [40-43]   baseline_tps        (DWORD)  — verifier-only decode TPS (x100)
;   [44-47]   sample_count        (DWORD)  — total measurement samples
;   [48-51]   total_drafted       (DWORD)  — lifetime drafted tokens
;   [52-55]   total_accepted      (DWORD)  — lifetime accepted tokens
;   [56-59]   total_rejected      (DWORD)  — lifetime rejected tokens
;   [60-63]   total_cycles        (DWORD)  — lifetime cycles for this pair
;   [64-67]   consecutive_bad     (DWORD)  — consecutive bad cycles (accept<min)
;   [68-71]   quarantine_until_lo (DWORD)  — quarantine timestamp low DWORD
;   [72-75]   quarantine_until_hi (DWORD)  — quarantine timestamp high DWORD
;   [76-79]   quarantine_count    (DWORD)  — times quarantined
;   [80-83]   last_effective_tps  (DWORD)  — most recent effective TPS (x100)
;   [84-87]   last_accept_rate    (DWORD)  — most recent accept rate (x100)
;   [88-91]   last_burst_len      (DWORD)  — most recent burst length
;   [92-95]   last_accepted_len   (DWORD)  — most recent accepted prefix len
;   [96-99]   score_bonus         (DWORD)  — planner score adjustment (signed)
;   [100-103]  flags              (DWORD)  — bit0=active,1=quarantined,2=preferred
;   [104-107]  fallback_count     (DWORD)  — times fallback triggered
;   [108-111]  gain_pct           (DWORD)  — effective/baseline * 100
;   [112-119]  last_use_timestamp  (QWORD) — last time this pair was used
;   [120-123]  reserved0          (DWORD)
;   [124-127]  reserved1          (DWORD)

; Pair flags
PAIR_ACTIVE         EQU 1
PAIR_QUARANTINED    EQU 2
PAIR_PREFERRED      EQU 4
PAIR_AUTO_BURST     EQU 8           ; adaptive burst size enabled

; Task classes
TASK_CHAT           EQU 0
TASK_CODE           EQU 1
TASK_BENCHMARK      EQU 2
TASK_REASONING      EQU 3

ALIGN 16
g_spec_pairs        DB (MAX_SPEC_PAIRS * SPEC_PAIR_ENTRY_SIZE) DUP(0)
g_spec_pair_count   DD 0

; ── Per-Cycle Transient State ──
SPEC_MAX_BURST      EQU 8           ; maximum burst length supported

ALIGN 16
g_cycle_id          DQ 0            ; monotonic cycle counter
g_cycle_pair_id     DD -1           ; which pair is running
g_cycle_burst_len   DD 4            ; requested burst length
g_cycle_drafted_len DD 0            ; actual drafted tokens
g_cycle_accepted_len DD 0           ; accepted prefix length
g_cycle_rejected_len DD 0           ; rejected suffix length
g_cycle_verifier_extra DD 0         ; verifier's own next token (correction)
g_cycle_start_ts    DQ 0            ; cycle start timestamp
g_cycle_end_ts      DQ 0            ; cycle end timestamp
g_cycle_elapsed_us  DD 0            ; cycle duration microseconds
g_cycle_effective_tps DD 0          ; this cycle's effective TPS (x100)

; Draft/Verified token buffers (8 slots each, DWORD token IDs)
ALIGN 16
g_draft_tokens      DD SPEC_MAX_BURST DUP(0)
g_verified_tokens   DD SPEC_MAX_BURST DUP(0)

; ── Acceptance Logic State ──
ALIGN 8
g_accept_total_drafted  DQ 0        ; lifetime all-pair drafted
g_accept_total_accepted DQ 0        ; lifetime all-pair accepted
g_accept_total_rejected DQ 0        ; lifetime all-pair rejected
g_accept_total_fallbacks DD 0       ; lifetime fallback count
g_accept_rate_global    DD 0        ; global accept rate (x100)

; ── Fallback Policy ──
FALLBACK_MAX_BAD_CYCLES EQU 2       ; 2 consecutive zero-accept → fallback
QUARANTINE_DURATION_MS  EQU 1800000 ; 30 minutes quarantine
FALLBACK_ACCEPT_FLOOR   EQU 4500    ; 45% (x100) minimum accept rate
FALLBACK_GAIN_FLOOR     EQU 105     ; must beat baseline by at least 5%

ALIGN 8
g_fallback_reason       DD 0        ; 0=none,1=zero_accept,2=low_rate,3=no_gain,4=manual
g_fallback_timestamp    DQ 0
g_fallback_pair_id      DD -1

; ── Telemetry Accumulator ──
ALIGN 8
g_telem_spec_enabled    DD 0
g_telem_pair_id         DD -1
g_telem_burst_len       DD 0
g_telem_accepted_len    DD 0
g_telem_rejected_len    DD 0
g_telem_accept_rate     DD 0        ; x100
g_telem_effective_tps   DD 0        ; x100
g_telem_baseline_tps    DD 0        ; x100
g_telem_gain_pct        DD 0        ; effective/baseline * 100
g_telem_fallback        DD 0        ; 1 if fallback occurred
g_telem_cycle_count     DQ 0
g_telem_reason          DD 0        ; spec_reason code

; Telemetry reason codes
SPEC_REASON_NONE        EQU 0
SPEC_REASON_PAIR_HIT    EQU 1       ; pair matched task
SPEC_REASON_PREFERRED   EQU 2       ; pair is preferred
SPEC_REASON_AUTO        EQU 3       ; auto-selected by planner
SPEC_REASON_QUARANTINED EQU 4       ; pair quarantined
SPEC_REASON_FALLBACK    EQU 5       ; fell back to normal
SPEC_REASON_DISABLED    EQU 6       ; speculation disabled

; ── Planner Score Adjustments ──
SCORE_BONUS_STRONG      EQU 40      ; effective TPS > baseline by 25%+
SCORE_BONUS_GOOD_RATE   EQU 20      ; accept rate > 65%
SCORE_BONUS_CLEAN       EQU 10      ; no fallback occurred
SCORE_PENALTY_WEAK      EQU -30     ; gain < 5%
SCORE_PENALTY_LOW_RATE  EQU -60     ; accept rate < 45%
SCORE_PENALTY_FALLBACK  EQU -120    ; fallback after repeated bad cycles


.code

; ============================================================================
; Enhancement 270: SwarmV270_Spec_Init
; ============================================================================
; Initialize speculative execution engine.
; Input:  ECX = operation: 0=init, 1=enable, 2=disable, 3=get_state, 4=reset
; Output: EAX = 0 on success, -1 on error
;         EDX = current state (for get_state)
; ============================================================================
SwarmV270_Spec_Init PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 28h

    mov     ebx, ecx

    cmp     ebx, 0
    je      @si_init
    cmp     ebx, 1
    je      @si_enable
    cmp     ebx, 2
    je      @si_disable
    cmp     ebx, 3
    je      @si_get_state
    cmp     ebx, 4
    je      @si_reset
    jmp     @si_bad

@si_init:
    ; Full engine initialization
    ; Zero pair registry
    lea     rdi, [g_spec_pairs]
    xor     eax, eax
    mov     ecx, MAX_SPEC_PAIRS * SPEC_PAIR_ENTRY_SIZE
@si_zero_pairs:
    mov     BYTE PTR [rdi], al
    inc     rdi
    dec     ecx
    jnz     @si_zero_pairs

    ; Zero cycle state
    mov     QWORD PTR [g_cycle_id], 0
    mov     DWORD PTR [g_cycle_pair_id], -1
    mov     DWORD PTR [g_cycle_burst_len], 4
    mov     DWORD PTR [g_cycle_drafted_len], 0
    mov     DWORD PTR [g_cycle_accepted_len], 0
    mov     DWORD PTR [g_cycle_rejected_len], 0
    mov     DWORD PTR [g_cycle_verifier_extra], 0

    ; Zero draft/verify buffers
    lea     rdi, [g_draft_tokens]
    xor     eax, eax
    mov     ecx, SPEC_MAX_BURST
@si_zero_draft:
    mov     DWORD PTR [rdi], eax
    add     rdi, 4
    dec     ecx
    jnz     @si_zero_draft

    lea     rdi, [g_verified_tokens]
    xor     eax, eax
    mov     ecx, SPEC_MAX_BURST
@si_zero_verify:
    mov     DWORD PTR [rdi], eax
    add     rdi, 4
    dec     ecx
    jnz     @si_zero_verify

    ; Zero acceptance accumulators
    mov     QWORD PTR [g_accept_total_drafted], 0
    mov     QWORD PTR [g_accept_total_accepted], 0
    mov     QWORD PTR [g_accept_total_rejected], 0
    mov     DWORD PTR [g_accept_total_fallbacks], 0
    mov     DWORD PTR [g_accept_rate_global], 0

    ; Zero fallback state
    mov     DWORD PTR [g_fallback_reason], 0
    mov     QWORD PTR [g_fallback_timestamp], 0
    mov     DWORD PTR [g_fallback_pair_id], -1

    ; Reset counters
    mov     DWORD PTR [g_spec_pair_count], 0
    mov     DWORD PTR [g_spec_active_pair_id], -1
    mov     QWORD PTR [g_spec_total_cycles], 0

    ; Set state to idle (initialized but not yet enabled)
    mov     DWORD PTR [g_spec_state], SPEC_STATE_IDLE
    mov     DWORD PTR [g_spec_enabled], 0

    ; Stamp init time
    call    GetTickCount64
    mov     QWORD PTR [g_spec_init_timestamp], rax

    xor     eax, eax            ; success
    jmp     @si_done

@si_enable:
    ; Enable speculation (must be init'd first)
    cmp     DWORD PTR [g_spec_state], SPEC_STATE_DISABLED
    je      @si_bad             ; not initialized
    mov     DWORD PTR [g_spec_enabled], 1
    mov     DWORD PTR [g_spec_state], SPEC_STATE_IDLE
    xor     eax, eax
    jmp     @si_done

@si_disable:
    mov     DWORD PTR [g_spec_enabled], 0
    mov     DWORD PTR [g_spec_state], SPEC_STATE_DISABLED
    xor     eax, eax
    jmp     @si_done

@si_get_state:
    mov     eax, [g_spec_state]
    mov     edx, [g_spec_enabled]
    mov     r8d, [g_spec_pair_count]
    mov     r9d, [g_spec_active_pair_id]
    jmp     @si_done

@si_reset:
    ; Reset stats without clearing registry
    mov     QWORD PTR [g_accept_total_drafted], 0
    mov     QWORD PTR [g_accept_total_accepted], 0
    mov     QWORD PTR [g_accept_total_rejected], 0
    mov     DWORD PTR [g_accept_total_fallbacks], 0
    mov     DWORD PTR [g_accept_rate_global], 0
    mov     QWORD PTR [g_spec_total_cycles], 0
    ; Reset per-pair stats
    xor     esi, esi
    mov     r12d, [g_spec_pair_count]
@si_reset_loop:
    cmp     esi, r12d
    jge     @si_reset_done
    imul    r10d, esi, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     DWORD PTR [rdi + 44], 0     ; sample_count
    mov     DWORD PTR [rdi + 48], 0     ; total_drafted
    mov     DWORD PTR [rdi + 52], 0     ; total_accepted
    mov     DWORD PTR [rdi + 56], 0     ; total_rejected
    mov     DWORD PTR [rdi + 60], 0     ; total_cycles
    mov     DWORD PTR [rdi + 64], 0     ; consecutive_bad
    mov     DWORD PTR [rdi + 76], 0     ; quarantine_count
    ; Clear quarantine
    mov     DWORD PTR [rdi + 68], 0
    mov     DWORD PTR [rdi + 72], 0
    mov     eax, DWORD PTR [rdi + 100]
    and     eax, NOT PAIR_QUARANTINED
    mov     DWORD PTR [rdi + 100], eax
    inc     esi
    jmp     @si_reset_loop
@si_reset_done:
    xor     eax, eax
    jmp     @si_done

@si_bad:
    mov     eax, -1
@si_done:
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV270_Spec_Init ENDP


; ============================================================================
; Enhancement 271: SwarmV271_Spec_Register_Pair
; ============================================================================
; Register or manage a draft/verifier pair.
; Input:  ECX = operation: 0=register, 1=get_pair, 2=set_baseline,
;                          3=quarantine, 4=unquarantine, 5=get_count,
;                          6=find_by_verifier
;         EDX = pair_id or index (for ops 1-4, 6)
;         R8D = draft_model_hash (for register)
;         R9D = verifier_model_hash (for register)
;         Stack [rsp+28h+28h] = task_class (for register)
;         Stack [rsp+28h+30h] = max_burst_len (for register)
;         Stack [rsp+28h+38h] = min_accept_rate (for register, x100)
;         Stack [rsp+28h+40h] = baseline_tps (for set_baseline, x100)
; Output: EAX = pair_id on success, -1 on error
;         EDX = flags (for get_pair)
; ============================================================================
SwarmV271_Spec_Register_Pair PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 30h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @rp_register
    cmp     ebx, 1
    je      @rp_get
    cmp     ebx, 2
    je      @rp_set_baseline
    cmp     ebx, 3
    je      @rp_quarantine
    cmp     ebx, 4
    je      @rp_unquarantine
    cmp     ebx, 5
    je      @rp_count
    cmp     ebx, 6
    je      @rp_find_verifier
    jmp     @rp_bad

@rp_register:
    ; Check capacity
    mov     eax, [g_spec_pair_count]
    cmp     eax, MAX_SPEC_PAIRS
    jge     @rp_full

    ; Calculate entry offset: eax * SPEC_PAIR_ENTRY_SIZE
    imul    r10d, eax, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10

    ; Populate entry
    mov     DWORD PTR [rdi + 0], eax    ; pair_id = index
    mov     DWORD PTR [rdi + 4], r8d    ; draft_model_hash
    mov     DWORD PTR [rdi + 8], r9d    ; verifier_model_hash

    ; Read stack params
    mov     r12d, DWORD PTR [rsp + 30h + 30h]   ; task_class
    mov     DWORD PTR [rdi + 12], r12d
    mov     r12d, DWORD PTR [rsp + 30h + 38h]   ; max_burst_len
    ; Clamp burst to [1, SPEC_MAX_BURST]
    cmp     r12d, 1
    jl      @rp_clamp_low
    cmp     r12d, SPEC_MAX_BURST
    jle     @rp_burst_ok
    mov     r12d, SPEC_MAX_BURST
    jmp     @rp_burst_ok
@rp_clamp_low:
    mov     r12d, 1
@rp_burst_ok:
    mov     DWORD PTR [rdi + 16], r12d  ; max_burst_len
    mov     r12d, DWORD PTR [rsp + 30h + 40h]   ; min_accept_rate
    mov     DWORD PTR [rdi + 20], r12d

    ; Initialize tracking
    mov     DWORD PTR [rdi + 24], 0     ; best_accept_rate
    mov     DWORD PTR [rdi + 28], 0     ; median_accept_rate
    mov     DWORD PTR [rdi + 32], 0     ; best_effective_tps
    mov     DWORD PTR [rdi + 36], 0     ; median_effective_tps
    mov     DWORD PTR [rdi + 40], 0     ; baseline_tps
    mov     DWORD PTR [rdi + 44], 0     ; sample_count
    mov     DWORD PTR [rdi + 48], 0     ; total_drafted
    mov     DWORD PTR [rdi + 52], 0     ; total_accepted
    mov     DWORD PTR [rdi + 56], 0     ; total_rejected
    mov     DWORD PTR [rdi + 60], 0     ; total_cycles
    mov     DWORD PTR [rdi + 64], 0     ; consecutive_bad
    mov     DWORD PTR [rdi + 68], 0     ; quarantine_until_lo
    mov     DWORD PTR [rdi + 72], 0     ; quarantine_until_hi
    mov     DWORD PTR [rdi + 76], 0     ; quarantine_count
    mov     DWORD PTR [rdi + 80], 0     ; last_effective_tps
    mov     DWORD PTR [rdi + 84], 0     ; last_accept_rate
    mov     DWORD PTR [rdi + 88], 0     ; last_burst_len
    mov     DWORD PTR [rdi + 92], 0     ; last_accepted_len
    mov     DWORD PTR [rdi + 96], 0     ; score_bonus
    mov     DWORD PTR [rdi + 100], PAIR_ACTIVE  ; flags = active
    mov     DWORD PTR [rdi + 104], 0    ; fallback_count
    mov     DWORD PTR [rdi + 108], 0    ; gain_pct
    mov     QWORD PTR [rdi + 112], 0    ; last_use_timestamp
    mov     DWORD PTR [rdi + 120], 0    ; reserved0
    mov     DWORD PTR [rdi + 124], 0    ; reserved1

    ; Increment count and return pair_id
    mov     eax, [g_spec_pair_count]
    inc     DWORD PTR [g_spec_pair_count]
    jmp     @rp_done

@rp_get:
    ; Get pair info by index
    cmp     esi, [g_spec_pair_count]
    jge     @rp_bad
    imul    r10d, esi, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 0]    ; pair_id
    mov     edx, DWORD PTR [rdi + 100]  ; flags
    mov     r8d, DWORD PTR [rdi + 80]   ; last_effective_tps
    mov     r9d, DWORD PTR [rdi + 84]   ; last_accept_rate
    jmp     @rp_done

@rp_set_baseline:
    ; Set verifier-only baseline TPS for this pair
    cmp     esi, [g_spec_pair_count]
    jge     @rp_bad
    imul    r10d, esi, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     r12d, DWORD PTR [rsp + 30h + 30h]  ; baseline_tps (x100)
    mov     DWORD PTR [rdi + 40], r12d
    xor     eax, eax
    jmp     @rp_done

@rp_quarantine:
    ; Quarantine a pair
    cmp     esi, [g_spec_pair_count]
    jge     @rp_bad
    imul    r10d, esi, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    ; Set quarantine flag
    mov     eax, DWORD PTR [rdi + 100]
    or      eax, PAIR_QUARANTINED
    and     eax, NOT PAIR_ACTIVE
    mov     DWORD PTR [rdi + 100], eax
    ; Set quarantine expiry = now + QUARANTINE_DURATION_MS
    call    GetTickCount64
    add     rax, QUARANTINE_DURATION_MS
    mov     DWORD PTR [rdi + 68], eax       ; lo
    shr     rax, 32
    mov     DWORD PTR [rdi + 72], eax       ; hi
    inc     DWORD PTR [rdi + 76]            ; quarantine_count++
    xor     eax, eax
    jmp     @rp_done

@rp_unquarantine:
    ; Remove quarantine from a pair
    cmp     esi, [g_spec_pair_count]
    jge     @rp_bad
    imul    r10d, esi, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 100]
    and     eax, NOT PAIR_QUARANTINED
    or      eax, PAIR_ACTIVE
    mov     DWORD PTR [rdi + 100], eax
    mov     DWORD PTR [rdi + 68], 0         ; clear quarantine timestamp
    mov     DWORD PTR [rdi + 72], 0
    mov     DWORD PTR [rdi + 64], 0         ; reset consecutive_bad
    xor     eax, eax
    jmp     @rp_done

@rp_count:
    mov     eax, [g_spec_pair_count]
    jmp     @rp_done

@rp_find_verifier:
    ; Find first active, non-quarantined pair matching verifier hash
    ; EDX = verifier_model_hash to search for
    xor     ecx, ecx
    mov     r12d, [g_spec_pair_count]
@rp_find_loop:
    cmp     ecx, r12d
    jge     @rp_notfound
    imul    r10d, ecx, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    ; Check verifier hash match
    cmp     esi, DWORD PTR [rdi + 8]
    jne     @rp_find_next
    ; Check flags: must be active, not quarantined
    mov     eax, DWORD PTR [rdi + 100]
    test    eax, PAIR_ACTIVE
    jz      @rp_find_next
    test    eax, PAIR_QUARANTINED
    jnz     @rp_find_next
    ; Found! Return pair_id
    mov     eax, DWORD PTR [rdi + 0]
    mov     edx, DWORD PTR [rdi + 100]     ; flags
    jmp     @rp_done
@rp_find_next:
    inc     ecx
    jmp     @rp_find_loop
@rp_notfound:
    mov     eax, -1
    jmp     @rp_done

@rp_full:
    mov     eax, -2
    jmp     @rp_done
@rp_bad:
    mov     eax, -1
@rp_done:
    add     rsp, 30h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV271_Spec_Register_Pair ENDP


; ============================================================================
; Enhancement 272: SwarmV272_Spec_Draft_Step
; ============================================================================
; Generate draft burst: stores proposed tokens into the draft buffer.
; In real integration, draft model inference happens externally;
; this function manages the cycle state and draft buffer.
;
; Input:  ECX = operation: 0=begin_draft, 1=store_token, 2=finalize_draft,
;                          3=get_draft_buffer
;         EDX = pair_id (for begin_draft) or token_id (for store_token)
;         R8D = burst_len (for begin_draft) or slot_index (for store_token)
; Output: EAX = 0 on success, -1 on error
;         EDX = drafted_len (for finalize_draft)
; ============================================================================
SwarmV272_Spec_Draft_Step PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 28h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @ds_begin
    cmp     ebx, 1
    je      @ds_store
    cmp     ebx, 2
    je      @ds_finalize
    cmp     ebx, 3
    je      @ds_get_buf
    jmp     @ds_bad

@ds_begin:
    ; Begin a new draft cycle
    ; EDX = pair_id, R8D = burst_len
    ; Validate engine is enabled
    cmp     DWORD PTR [g_spec_enabled], 0
    je      @ds_bad

    ; Validate pair_id
    cmp     esi, [g_spec_pair_count]
    jge     @ds_bad

    ; Check pair is active and not quarantined
    imul    r10d, esi, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 100]     ; flags
    test    eax, PAIR_ACTIVE
    jz      @ds_bad
    test    eax, PAIR_QUARANTINED
    jnz     @ds_quarantined

    ; Clamp burst_len
    mov     r12d, r8d
    cmp     r12d, 1
    jl      @ds_burst_clamp_lo
    ; Also respect pair's max_burst_len
    cmp     r12d, DWORD PTR [rdi + 16]     ; pair max_burst_len
    jle     @ds_burst_ok
    mov     r12d, DWORD PTR [rdi + 16]
    jmp     @ds_burst_ok
@ds_burst_clamp_lo:
    mov     r12d, 1
@ds_burst_ok:

    ; Set up cycle state
    mov     DWORD PTR [g_cycle_pair_id], esi
    mov     DWORD PTR [g_cycle_burst_len], r12d
    mov     DWORD PTR [g_cycle_drafted_len], 0
    mov     DWORD PTR [g_cycle_accepted_len], 0
    mov     DWORD PTR [g_cycle_rejected_len], 0
    mov     DWORD PTR [g_cycle_verifier_extra], 0
    mov     DWORD PTR [g_spec_active_pair_id], esi
    mov     DWORD PTR [g_spec_state], SPEC_STATE_DRAFTING

    ; Clear draft buffer
    lea     rdi, [g_draft_tokens]
    xor     eax, eax
    mov     ecx, SPEC_MAX_BURST
@ds_clear:
    mov     DWORD PTR [rdi], eax
    add     rdi, 4
    dec     ecx
    jnz     @ds_clear

    ; Increment cycle counter
    inc     QWORD PTR [g_cycle_id]

    ; Record start timestamp
    call    GetTickCount64
    mov     QWORD PTR [g_cycle_start_ts], rax

    xor     eax, eax
    jmp     @ds_done

@ds_store:
    ; Store a draft token: EDX = token_id, R8D = slot_index
    cmp     DWORD PTR [g_spec_state], SPEC_STATE_DRAFTING
    jne     @ds_bad
    cmp     r8d, SPEC_MAX_BURST
    jge     @ds_bad
    cmp     r8d, [g_cycle_burst_len]
    jge     @ds_bad

    ; Write token to draft buffer at slot_index
    ; g_draft_tokens[R8D] = ESI (token_id)
    lea     rdi, [g_draft_tokens]
    movsxd  r10, r8d
    mov     DWORD PTR [rdi + r10 * 4], esi

    ; Track drafted count (always max of slot+1 and current)
    lea     eax, [r8d + 1]
    cmp     eax, [g_cycle_drafted_len]
    jle     @ds_store_ok
    mov     [g_cycle_drafted_len], eax
@ds_store_ok:
    xor     eax, eax
    jmp     @ds_done

@ds_finalize:
    ; Finalize draft step — move to verifying state
    cmp     DWORD PTR [g_spec_state], SPEC_STATE_DRAFTING
    jne     @ds_bad
    mov     DWORD PTR [g_spec_state], SPEC_STATE_VERIFYING
    mov     eax, 0
    mov     edx, [g_cycle_drafted_len]
    jmp     @ds_done

@ds_get_buf:
    ; Return pointer to draft buffer in RAX, count in EDX
    lea     rax, [g_draft_tokens]
    mov     edx, [g_cycle_drafted_len]
    jmp     @ds_done

@ds_quarantined:
    mov     eax, -2             ; pair is quarantined
    jmp     @ds_done
@ds_bad:
    mov     eax, -1
@ds_done:
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV272_Spec_Draft_Step ENDP


; ============================================================================
; Enhancement 273: SwarmV273_Spec_Verify_Step
; ============================================================================
; Verify draft burst against verifier results.
; Computes longest contiguous accepted prefix.
;
; Input:  ECX = operation: 0=store_verified, 1=compute_acceptance,
;                          2=get_accepted_len, 3=get_verified_buffer
;         EDX = token_id (for store_verified) or not used
;         R8D = slot_index (for store_verified)
; Output: EAX = accepted_prefix_length (for compute_acceptance)
;         EDX = accept_rate_x100 (for compute_acceptance)
; ============================================================================
SwarmV273_Spec_Verify_Step PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 30h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @vs_store
    cmp     ebx, 1
    je      @vs_compute
    cmp     ebx, 2
    je      @vs_get_len
    cmp     ebx, 3
    je      @vs_get_buf
    jmp     @vs_bad

@vs_store:
    ; Store verifier result: EDX = token_id, R8D = slot_index
    cmp     DWORD PTR [g_spec_state], SPEC_STATE_VERIFYING
    jne     @vs_bad
    cmp     r8d, SPEC_MAX_BURST
    jge     @vs_bad

    lea     rdi, [g_verified_tokens]
    movsxd  r10, r8d
    mov     DWORD PTR [rdi + r10 * 4], esi
    xor     eax, eax
    jmp     @vs_done

@vs_compute:
    ; Compute contiguous accepted prefix
    ; Compare g_draft_tokens[i] == g_verified_tokens[i] for i=0..drafted_len-1
    cmp     DWORD PTR [g_spec_state], SPEC_STATE_VERIFYING
    jne     @vs_bad

    lea     r12, [g_draft_tokens]
    lea     r13, [g_verified_tokens]
    xor     ecx, ecx            ; i = 0
    mov     esi, [g_cycle_drafted_len]

@vs_cmp_loop:
    cmp     ecx, esi
    jge     @vs_cmp_done        ; all matched!
    mov     eax, DWORD PTR [r12 + rcx * 4]
    cmp     eax, DWORD PTR [r13 + rcx * 4]
    jne     @vs_cmp_done        ; mismatch at position ecx
    inc     ecx
    jmp     @vs_cmp_loop

@vs_cmp_done:
    ; ECX = accepted prefix length
    mov     [g_cycle_accepted_len], ecx
    ; Compute rejected = drafted - accepted
    mov     eax, [g_cycle_drafted_len]
    sub     eax, ecx
    mov     [g_cycle_rejected_len], eax

    ; If verifier disagrees at position ecx and ecx < drafted_len,
    ; store verifier's token as the correction token
    cmp     ecx, [g_cycle_drafted_len]
    jge     @vs_no_extra
    mov     eax, DWORD PTR [r13 + rcx * 4]
    mov     [g_cycle_verifier_extra], eax
    jmp     @vs_compute_rate

@vs_no_extra:
    mov     DWORD PTR [g_cycle_verifier_extra], 0

@vs_compute_rate:
    ; Compute accept rate = (accepted * 10000) / drafted
    mov     eax, ecx            ; accepted
    imul    eax, 10000
    mov     r10d, [g_cycle_drafted_len]
    test    r10d, r10d
    jz      @vs_zero_rate
    cdq
    idiv    r10d                ; eax = accept_rate x100 (actually x10000 for precision)
    ; Normalize to x100 (percentage * 100): divide by 100
    mov     r10d, 100
    cdq
    idiv    r10d                ; eax = accept_rate x100 (e.g. 7500 = 75%)
    jmp     @vs_store_rate

@vs_zero_rate:
    xor     eax, eax

@vs_store_rate:
    mov     edx, eax            ; accept_rate_x100 in EDX
    mov     eax, [g_cycle_accepted_len]  ; accepted_len in EAX

    ; Move state to committing
    mov     DWORD PTR [g_spec_state], SPEC_STATE_COMMITTING
    jmp     @vs_done

@vs_get_len:
    mov     eax, [g_cycle_accepted_len]
    mov     edx, [g_cycle_rejected_len]
    mov     r8d, [g_cycle_verifier_extra]
    jmp     @vs_done

@vs_get_buf:
    lea     rax, [g_verified_tokens]
    mov     edx, [g_cycle_drafted_len]
    jmp     @vs_done

@vs_bad:
    mov     eax, -1
@vs_done:
    add     rsp, 30h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV273_Spec_Verify_Step ENDP


; ============================================================================
; Enhancement 274: SwarmV274_Spec_Commit
; ============================================================================
; Commit accepted tokens and update pair stats.
; Handles: acceptance accounting, pair stats update, score adjustment,
;          consecutive-bad tracking, and auto-fallback trigger.
;
; Input:  ECX = operation: 0=commit_cycle, 1=get_cycle_result,
;                          2=get_effective_tps, 3=get_score_adjustment
;         EDX = elapsed_ms (for commit_cycle — total cycle wall time)
;         R8D = committed_output_tokens (for commit_cycle — user-visible count)
; Output: EAX = 0 on success, 1 if fallback triggered, -1 on error
;         EDX = effective_tps_x100 (for commit_cycle / get_effective_tps)
; ============================================================================
SwarmV274_Spec_Commit PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 38h

    mov     ebx, ecx
    mov     esi, edx            ; elapsed_ms for commit

    cmp     ebx, 0
    je      @cc_commit
    cmp     ebx, 1
    je      @cc_get_result
    cmp     ebx, 2
    je      @cc_get_eff_tps
    cmp     ebx, 3
    je      @cc_get_score
    jmp     @cc_bad

@cc_commit:
    ; Commit cycle results
    cmp     DWORD PTR [g_spec_state], SPEC_STATE_COMMITTING
    jne     @cc_bad

    ; Record end timestamp
    call    GetTickCount64
    mov     QWORD PTR [g_cycle_end_ts], rax

    ; Calculate effective TPS: (committed_tokens * 100000) / elapsed_ms
    ; This gives TPS * 100 (our standard x100 format)
    mov     eax, r8d            ; committed_output_tokens
    test    eax, eax
    jz      @cc_zero_tps
    imul    eax, 100000         ; tokens * 100000
    test    esi, esi
    jz      @cc_zero_tps
    cdq
    idiv    esi                 ; eax = effective_tps * 100
    jmp     @cc_store_tps
@cc_zero_tps:
    xor     eax, eax
@cc_store_tps:
    mov     [g_cycle_effective_tps], eax
    mov     r12d, eax           ; save effective_tps_x100

    ; Update global acceptance accumulators
    mov     eax, [g_cycle_drafted_len]
    cdqe
    add     QWORD PTR [g_accept_total_drafted], rax
    mov     eax, [g_cycle_accepted_len]
    cdqe
    add     QWORD PTR [g_accept_total_accepted], rax
    mov     eax, [g_cycle_rejected_len]
    cdqe
    add     QWORD PTR [g_accept_total_rejected], rax

    ; Increment global cycle count
    inc     QWORD PTR [g_spec_total_cycles]

    ; Update pair stats
    mov     ecx, [g_cycle_pair_id]
    cmp     ecx, [g_spec_pair_count]
    jge     @cc_no_pair

    imul    r10d, ecx, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10

    ; Update pair totals
    mov     eax, [g_cycle_drafted_len]
    add     DWORD PTR [rdi + 48], eax       ; total_drafted
    mov     eax, [g_cycle_accepted_len]
    add     DWORD PTR [rdi + 52], eax       ; total_accepted
    mov     eax, [g_cycle_rejected_len]
    add     DWORD PTR [rdi + 56], eax       ; total_rejected
    inc     DWORD PTR [rdi + 60]            ; total_cycles

    ; Compute pair accept rate = (total_accepted * 10000) / total_drafted / 100
    mov     eax, DWORD PTR [rdi + 52]       ; total_accepted
    imul    eax, 10000
    mov     r13d, DWORD PTR [rdi + 48]      ; total_drafted
    test    r13d, r13d
    jz      @cc_pair_zero_rate
    cdq
    idiv    r13d
    mov     r13d, 100
    cdq
    idiv    r13d
    jmp     @cc_pair_store_rate
@cc_pair_zero_rate:
    xor     eax, eax
@cc_pair_store_rate:
    mov     DWORD PTR [rdi + 84], eax       ; last_accept_rate

    ; Update median accept rate (simple running average for now)
    mov     r14d, DWORD PTR [rdi + 28]      ; current median
    test    r14d, r14d
    jz      @cc_first_median
    ; median = (old_median + new_rate) / 2
    add     r14d, eax
    shr     r14d, 1
    mov     DWORD PTR [rdi + 28], r14d
    jmp     @cc_check_best_rate
@cc_first_median:
    mov     DWORD PTR [rdi + 28], eax

@cc_check_best_rate:
    ; Update best accept rate
    cmp     eax, DWORD PTR [rdi + 24]
    jle     @cc_store_effective
    mov     DWORD PTR [rdi + 24], eax       ; new best

@cc_store_effective:
    ; Store effective TPS for pair
    mov     DWORD PTR [rdi + 80], r12d      ; last_effective_tps

    ; Update best/median effective TPS
    cmp     r12d, DWORD PTR [rdi + 32]
    jle     @cc_eff_median
    mov     DWORD PTR [rdi + 32], r12d      ; new best effective
@cc_eff_median:
    mov     r14d, DWORD PTR [rdi + 36]
    test    r14d, r14d
    jz      @cc_first_eff_median
    add     r14d, r12d
    shr     r14d, 1
    mov     DWORD PTR [rdi + 36], r14d
    jmp     @cc_compute_gain
@cc_first_eff_median:
    mov     DWORD PTR [rdi + 36], r12d

@cc_compute_gain:
    ; Compute gain_pct = (effective_tps * 100) / baseline_tps
    mov     eax, r12d
    imul    eax, 100
    mov     r13d, DWORD PTR [rdi + 40]      ; baseline_tps
    test    r13d, r13d
    jz      @cc_no_gain
    cdq
    idiv    r13d
    mov     DWORD PTR [rdi + 108], eax      ; gain_pct
    jmp     @cc_check_fallback
@cc_no_gain:
    mov     DWORD PTR [rdi + 108], 0

@cc_check_fallback:
    ; Check for fallback conditions
    ; 1) zero-accept for FALLBACK_MAX_BAD_CYCLES
    mov     eax, [g_cycle_accepted_len]
    test    eax, eax
    jnz     @cc_accept_ok
    inc     DWORD PTR [rdi + 64]            ; consecutive_bad++
    mov     eax, DWORD PTR [rdi + 64]
    cmp     eax, FALLBACK_MAX_BAD_CYCLES
    jge     @cc_trigger_fallback_zero
    jmp     @cc_compute_score

@cc_accept_ok:
    mov     DWORD PTR [rdi + 64], 0         ; reset consecutive_bad

    ; 2) accept_rate < FALLBACK_ACCEPT_FLOOR
    mov     eax, DWORD PTR [rdi + 84]       ; last_accept_rate
    cmp     eax, FALLBACK_ACCEPT_FLOOR
    jl      @cc_trigger_fallback_rate

    ; 3) no gain vs baseline
    mov     eax, DWORD PTR [rdi + 108]      ; gain_pct
    cmp     eax, FALLBACK_GAIN_FLOOR
    jl      @cc_trigger_fallback_gain

    jmp     @cc_compute_score

@cc_trigger_fallback_zero:
    mov     DWORD PTR [g_fallback_reason], 1
    jmp     @cc_do_fallback
@cc_trigger_fallback_rate:
    mov     DWORD PTR [g_fallback_reason], 2
    jmp     @cc_do_fallback
@cc_trigger_fallback_gain:
    mov     DWORD PTR [g_fallback_reason], 3
    ; Fall through

@cc_do_fallback:
    inc     DWORD PTR [rdi + 104]           ; fallback_count++
    inc     DWORD PTR [g_accept_total_fallbacks]
    call    GetTickCount64
    mov     QWORD PTR [g_fallback_timestamp], rax
    mov     eax, [g_cycle_pair_id]
    mov     [g_fallback_pair_id], eax
    mov     DWORD PTR [g_spec_state], SPEC_STATE_FALLBACK
    ; Apply penalty score
    mov     DWORD PTR [rdi + 96], SCORE_PENALTY_FALLBACK
    ; Set return: 1 = fallback triggered
    mov     eax, 1
    mov     edx, r12d           ; effective_tps_x100
    jmp     @cc_done

@cc_compute_score:
    ; Compute planner score bonus
    ; Check strong gain (>25%): gain_pct > 125
    mov     eax, DWORD PTR [rdi + 108]
    cmp     eax, 125
    jge     @cc_score_strong
    ; Check good rate (>65%): last_accept_rate > 6500
    mov     eax, DWORD PTR [rdi + 84]
    cmp     eax, 6500
    jge     @cc_score_good
    ; Check clean: no fallback in this cycle — we're here so it's clean
    mov     DWORD PTR [rdi + 96], SCORE_BONUS_CLEAN
    jmp     @cc_score_done

@cc_score_strong:
    mov     DWORD PTR [rdi + 96], SCORE_BONUS_STRONG
    jmp     @cc_score_done
@cc_score_good:
    mov     DWORD PTR [rdi + 96], SCORE_BONUS_GOOD_RATE
@cc_score_done:

    ; Update sample count
    inc     DWORD PTR [rdi + 44]

    ; Store cycle metrics
    mov     eax, [g_cycle_burst_len]
    mov     DWORD PTR [rdi + 88], eax       ; last_burst_len
    mov     eax, [g_cycle_accepted_len]
    mov     DWORD PTR [rdi + 92], eax       ; last_accepted_len

    ; Stamp last use
    call    GetTickCount64
    mov     QWORD PTR [rdi + 112], rax

@cc_no_pair:
    ; Return to idle state
    mov     DWORD PTR [g_spec_state], SPEC_STATE_IDLE
    mov     DWORD PTR [g_spec_active_pair_id], -1
    xor     eax, eax            ; success, no fallback
    mov     edx, r12d           ; effective_tps_x100
    jmp     @cc_done

@cc_get_result:
    ; Get last cycle result
    mov     eax, [g_cycle_accepted_len]
    mov     edx, [g_cycle_rejected_len]
    mov     r8d, [g_cycle_effective_tps]
    mov     r9d, [g_cycle_verifier_extra]
    jmp     @cc_done

@cc_get_eff_tps:
    mov     eax, [g_cycle_effective_tps]
    ; Also return baseline for comparison
    mov     ecx, [g_cycle_pair_id]
    cmp     ecx, [g_spec_pair_count]
    jge     @cc_no_baseline
    imul    r10d, ecx, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     edx, DWORD PTR [rdi + 40]      ; baseline_tps
    mov     r8d, DWORD PTR [rdi + 108]      ; gain_pct
    jmp     @cc_done
@cc_no_baseline:
    xor     edx, edx
    xor     r8d, r8d
    jmp     @cc_done

@cc_get_score:
    mov     ecx, [g_cycle_pair_id]
    cmp     ecx, [g_spec_pair_count]
    jge     @cc_bad
    imul    r10d, ecx, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 96]       ; score_bonus
    mov     edx, DWORD PTR [rdi + 108]      ; gain_pct
    jmp     @cc_done

@cc_bad:
    mov     eax, -1
@cc_done:
    add     rsp, 38h
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV274_Spec_Commit ENDP


; ============================================================================
; Enhancement 275: SwarmV275_Spec_Telemetry
; ============================================================================
; Emit and query speculative execution metrics.
; Populates g_telem_* fields and returns summary stats.
;
; Input:  ECX = operation: 0=snapshot, 1=get_global, 2=get_pair_stats,
;                          3=get_acceptance_summary, 4=get_fallback_info,
;                          5=get_gain_summary
;         EDX = pair_id (for get_pair_stats)
; Output: EAX/EDX/R8D/R9D = depends on operation
; ============================================================================
SwarmV275_Spec_Telemetry PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 28h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @st_snapshot
    cmp     ebx, 1
    je      @st_global
    cmp     ebx, 2
    je      @st_pair
    cmp     ebx, 3
    je      @st_acceptance
    cmp     ebx, 4
    je      @st_fallback
    cmp     ebx, 5
    je      @st_gain
    jmp     @st_bad

@st_snapshot:
    ; Capture current cycle state into telemetry fields
    mov     eax, [g_spec_enabled]
    mov     [g_telem_spec_enabled], eax
    mov     eax, [g_cycle_pair_id]
    mov     [g_telem_pair_id], eax
    mov     eax, [g_cycle_burst_len]
    mov     [g_telem_burst_len], eax
    mov     eax, [g_cycle_accepted_len]
    mov     [g_telem_accepted_len], eax
    mov     eax, [g_cycle_rejected_len]
    mov     [g_telem_rejected_len], eax
    mov     eax, [g_cycle_effective_tps]
    mov     [g_telem_effective_tps], eax
    mov     eax, [g_fallback_reason]
    test    eax, eax
    jz      @st_snap_no_fb
    mov     DWORD PTR [g_telem_fallback], 1
    jmp     @st_snap_fb_done
@st_snap_no_fb:
    mov     DWORD PTR [g_telem_fallback], 0
@st_snap_fb_done:
    mov     rax, [g_spec_total_cycles]
    mov     [g_telem_cycle_count], rax

    ; Compute accept rate for telemetry
    mov     eax, [g_cycle_accepted_len]
    imul    eax, 10000
    mov     ecx, [g_cycle_drafted_len]
    test    ecx, ecx
    jz      @st_snap_zero_rate
    cdq
    idiv    ecx
    mov     ecx, 100
    cdq
    idiv    ecx
    jmp     @st_snap_store_rate
@st_snap_zero_rate:
    xor     eax, eax
@st_snap_store_rate:
    mov     [g_telem_accept_rate], eax

    ; Get baseline and compute gain
    mov     ecx, [g_cycle_pair_id]
    cmp     ecx, [g_spec_pair_count]
    jge     @st_snap_no_pair
    imul    r10d, ecx, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 40]
    mov     [g_telem_baseline_tps], eax
    mov     eax, DWORD PTR [rdi + 108]
    mov     [g_telem_gain_pct], eax
    jmp     @st_snap_done
@st_snap_no_pair:
    mov     DWORD PTR [g_telem_baseline_tps], 0
    mov     DWORD PTR [g_telem_gain_pct], 0
@st_snap_done:
    xor     eax, eax
    jmp     @st_done

@st_global:
    ; Return global engine stats
    mov     eax, [g_spec_state]
    mov     edx, [g_spec_pair_count]
    mov     r8d, [g_spec_enabled]
    ; Truncate total_cycles to 32-bit for return
    mov     r9d, DWORD PTR [g_spec_total_cycles]
    jmp     @st_done

@st_pair:
    ; Return pair-level stats
    cmp     esi, [g_spec_pair_count]
    jge     @st_bad
    imul    r10d, esi, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 80]       ; last_effective_tps
    mov     edx, DWORD PTR [rdi + 84]       ; last_accept_rate
    mov     r8d, DWORD PTR [rdi + 60]       ; total_cycles
    mov     r9d, DWORD PTR [rdi + 44]       ; sample_count
    jmp     @st_done

@st_acceptance:
    ; Return overall acceptance summary
    ; EAX = global accept rate
    mov     rax, [g_accept_total_accepted]
    imul    rax, 10000
    mov     rcx, [g_accept_total_drafted]
    test    rcx, rcx
    jz      @st_accept_zero
    cqo
    idiv    rcx
    mov     rcx, 100
    cqo
    idiv    rcx
    jmp     @st_accept_store
@st_accept_zero:
    xor     eax, eax
@st_accept_store:
    mov     [g_accept_rate_global], eax
    ; EDX = total fallbacks
    mov     edx, [g_accept_total_fallbacks]
    ; R8D = total_drafted (low 32)
    mov     r8d, DWORD PTR [g_accept_total_drafted]
    ; R9D = total_accepted (low 32)
    mov     r9d, DWORD PTR [g_accept_total_accepted]
    jmp     @st_done

@st_fallback:
    ; Return fallback info
    mov     eax, [g_fallback_reason]
    mov     edx, [g_fallback_pair_id]
    mov     r8d, [g_accept_total_fallbacks]
    ; R9D = low 32 of fallback timestamp
    mov     r9d, DWORD PTR [g_fallback_timestamp]
    jmp     @st_done

@st_gain:
    ; Return best gain across all pairs
    xor     r12d, r12d          ; best gain
    xor     ecx, ecx
    mov     esi, [g_spec_pair_count]
@st_gain_loop:
    cmp     ecx, esi
    jge     @st_gain_result
    imul    r10d, ecx, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 108]      ; gain_pct
    cmp     eax, r12d
    jle     @st_gain_next
    mov     r12d, eax
@st_gain_next:
    inc     ecx
    jmp     @st_gain_loop
@st_gain_result:
    mov     eax, r12d           ; best gain
    mov     edx, [g_spec_pair_count]
    jmp     @st_done

@st_bad:
    mov     eax, -1
@st_done:
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV275_Spec_Telemetry ENDP


; ============================================================================
; Enhancement 276: SwarmV276_Spec_Fallback
; ============================================================================
; Manage speculative fallback: disable speculation for current request,
; quarantine bad pairs, and handle re-enable logic.
;
; Input:  ECX = operation: 0=force_fallback, 1=check_quarantine_expiry,
;                          2=get_fallback_state, 3=clear_fallback,
;                          4=auto_quarantine_bad_pairs
;         EDX = pair_id (for force_fallback)
;         R8D = reason (for force_fallback: 1-4)
; Output: EAX = 0 on success, count of quarantined (for auto_quarantine)
;         EDX = fallback_reason (for get_fallback_state)
; ============================================================================
SwarmV276_Spec_Fallback PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 30h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @fb_force
    cmp     ebx, 1
    je      @fb_check_expiry
    cmp     ebx, 2
    je      @fb_get_state
    cmp     ebx, 3
    je      @fb_clear
    cmp     ebx, 4
    je      @fb_auto_quarantine
    jmp     @fb_bad

@fb_force:
    ; Force fallback for a specific pair
    ; EDX = pair_id, R8D = reason code
    cmp     esi, [g_spec_pair_count]
    jge     @fb_bad

    mov     DWORD PTR [g_fallback_reason], r8d
    mov     DWORD PTR [g_fallback_pair_id], esi
    mov     DWORD PTR [g_spec_state], SPEC_STATE_FALLBACK
    mov     DWORD PTR [g_spec_active_pair_id], -1

    call    GetTickCount64
    mov     QWORD PTR [g_fallback_timestamp], rax

    inc     DWORD PTR [g_accept_total_fallbacks]

    ; Increment pair fallback count
    imul    r10d, esi, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10
    inc     DWORD PTR [rdi + 104]           ; fallback_count

    ; Apply score penalty
    mov     DWORD PTR [rdi + 96], SCORE_PENALTY_FALLBACK

    xor     eax, eax
    jmp     @fb_done

@fb_check_expiry:
    ; Check all quarantined pairs and release expired ones
    call    GetTickCount64
    mov     r12, rax            ; current tick64

    xor     r13d, r13d          ; released count
    xor     ecx, ecx
    mov     esi, [g_spec_pair_count]
@fb_expiry_loop:
    cmp     ecx, esi
    jge     @fb_expiry_done
    imul    r10d, ecx, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10

    ; Check if quarantined
    mov     eax, DWORD PTR [rdi + 100]
    test    eax, PAIR_QUARANTINED
    jz      @fb_expiry_next

    ; Reconstruct quarantine_until from lo/hi
    mov     eax, DWORD PTR [rdi + 72]       ; hi
    shl     rax, 32
    or      eax, DWORD PTR [rdi + 68]       ; lo

    ; If current time >= quarantine_until, release
    cmp     r12, rax
    jl      @fb_expiry_next

    ; Release quarantine
    mov     eax, DWORD PTR [rdi + 100]
    and     eax, NOT PAIR_QUARANTINED
    or      eax, PAIR_ACTIVE
    mov     DWORD PTR [rdi + 100], eax
    mov     DWORD PTR [rdi + 68], 0
    mov     DWORD PTR [rdi + 72], 0
    mov     DWORD PTR [rdi + 64], 0         ; reset consecutive_bad
    inc     r13d

@fb_expiry_next:
    inc     ecx
    jmp     @fb_expiry_loop
@fb_expiry_done:
    mov     eax, r13d           ; return released count
    jmp     @fb_done

@fb_get_state:
    mov     eax, [g_spec_state]
    mov     edx, [g_fallback_reason]
    mov     r8d, [g_fallback_pair_id]
    mov     r9d, [g_accept_total_fallbacks]
    jmp     @fb_done

@fb_clear:
    ; Clear fallback state and return to idle
    mov     DWORD PTR [g_fallback_reason], 0
    mov     DWORD PTR [g_fallback_pair_id], -1
    mov     DWORD PTR [g_spec_state], SPEC_STATE_IDLE
    xor     eax, eax
    jmp     @fb_done

@fb_auto_quarantine:
    ; Scan all pairs and quarantine those with repeated bad performance
    ; Criteria: fallback_count >= 3 AND median_accept_rate < FALLBACK_ACCEPT_FLOOR
    ;           AND pair is currently active (not already quarantined)
    xor     r13d, r13d          ; quarantine count
    xor     ecx, ecx
    mov     esi, [g_spec_pair_count]
@fb_aq_loop:
    cmp     ecx, esi
    jge     @fb_aq_done
    imul    r10d, ecx, SPEC_PAIR_ENTRY_SIZE
    lea     rdi, [g_spec_pairs]
    add     rdi, r10

    ; Skip already quarantined
    mov     eax, DWORD PTR [rdi + 100]
    test    eax, PAIR_QUARANTINED
    jnz     @fb_aq_next

    ; Check fallback_count >= 3
    cmp     DWORD PTR [rdi + 104], 3
    jl      @fb_aq_next

    ; Check median_accept_rate < FALLBACK_ACCEPT_FLOOR
    mov     eax, DWORD PTR [rdi + 28]
    cmp     eax, FALLBACK_ACCEPT_FLOOR
    jge     @fb_aq_next

    ; Quarantine this pair
    mov     eax, DWORD PTR [rdi + 100]
    or      eax, PAIR_QUARANTINED
    and     eax, NOT PAIR_ACTIVE
    mov     DWORD PTR [rdi + 100], eax

    ; Set quarantine expiry
    push    rcx                 ; preserve loop counter
    push    rsi
    call    GetTickCount64
    pop     rsi
    pop     rcx
    add     rax, QUARANTINE_DURATION_MS
    mov     DWORD PTR [rdi + 68], eax
    shr     rax, 32
    mov     DWORD PTR [rdi + 72], eax
    inc     DWORD PTR [rdi + 76]            ; quarantine_count++
    inc     r13d

@fb_aq_next:
    inc     ecx
    jmp     @fb_aq_loop
@fb_aq_done:
    mov     eax, r13d           ; return quarantine count
    jmp     @fb_done

@fb_bad:
    mov     eax, -1
@fb_done:
    add     rsp, 30h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV276_Spec_Fallback ENDP

END
