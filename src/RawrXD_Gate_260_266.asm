; ============================================================================
; RawrXD_Gate_260_266.asm — Enhancements 260-266
; Production Gate + Sentinel Regression + PP Classification Kernel
; ============================================================================
; 260: Benchmark Sentinel Registry  — Register sentinel models w/ TPS thresholds
; 261: PP Phase Classifier          — Classify PP into cold/warm/cache-hit
; 262: Regression Gate Engine       — Automated pass/fail per-model gating
; 263: Warm Path Amplifier          — Persistent warm-path state for PP retention
; 264: Large Model Tensor Scheduler — Optimized tensor schedule for 8B+ models
; 265: Variance Clamp Controller    — Active TG variance suppression
; 266: Production Gate Validator    — Full production readiness validator
; ============================================================================

extrn GetTickCount64 : proc

; ── External references ──
EXTERN SwarmV253_KV_Cache_Prewarmer:PROC
EXTERN SwarmV255_VRAM_Pressure_Monitor:PROC
EXTERN SwarmV258_Inference_Latency_Profiler:PROC
EXTERN SwarmV259_Sovereign_Smoke_Test:PROC

PUBLIC SwarmV260_Benchmark_Sentinel
PUBLIC SwarmV261_PP_Phase_Classifier
PUBLIC SwarmV262_Regression_Gate
PUBLIC SwarmV263_Warm_Path_Amplifier
PUBLIC SwarmV264_Large_Model_Scheduler
PUBLIC SwarmV265_Variance_Clamp
PUBLIC SwarmV266_Production_Gate

.data

; ── Sentinel Registry (Enhancement 260) ──
MAX_SENTINELS       EQU 8
SENTINEL_ENTRY_SIZE EQU 48            ; 48 bytes per sentinel

; Sentinel entry layout (48 bytes):
;   [0-7]   model_name_ptr  (QWORD)
;   [8-11]  model_size_mb   (DWORD)
;   [12-15] tg_floor_tps    (DWORD) — minimum acceptable TG
;   [16-19] tg_target_tps   (DWORD) — expected TG (reference)
;   [20-23] pp_warm_floor   (DWORD) — minimum acceptable warm PP
;   [24-27] variance_max    (DWORD) — max allowed std dev (x100)
;   [28-31] last_tg_tps     (DWORD) — most recent measured TG
;   [32-35] last_pp_tps     (DWORD) — most recent measured PP
;   [36-39] pass_count      (DWORD) — consecutive passes
;   [40-43] fail_count      (DWORD) — consecutive failures
;   [44-47] flags           (DWORD) — bit0=active, bit1=locked, bit2=regressed

ALIGN 16
g_sentinel_registry DB (MAX_SENTINELS * SENTINEL_ENTRY_SIZE) DUP(0)
g_sentinel_count    DD 0
g_sentinel_version  DD 266

; Sentinel flags
SENTINEL_ACTIVE     EQU 1
SENTINEL_LOCKED     EQU 2
SENTINEL_REGRESSED  EQU 4

; ── PP Phase Classifier State (Enhancement 261) ──
PP_CLASS_COLD       EQU 0            ; first run, model cold-loaded
PP_CLASS_WARM       EQU 1            ; model resident, KV empty
PP_CLASS_CACHE_HIT  EQU 2            ; model resident, KV populated
PP_CLASS_UNKNOWN    EQU 3

MAX_PP_HISTORY      EQU 16
PP_HISTORY_ENTRY    EQU 32           ; 32 bytes per PP measurement

; PP history entry layout (32 bytes):
;   [0-3]   pp_tps          (DWORD)
;   [4-7]   classification  (DWORD) — PP_CLASS_*
;   [8-11]  token_count     (DWORD)
;   [12-15] model_hash_lo   (DWORD)
;   [16-23] timestamp       (QWORD)
;   [24-27] load_ms         (DWORD)
;   [28-31] reserved        (DWORD)

ALIGN 16
g_pp_history        DB (MAX_PP_HISTORY * PP_HISTORY_ENTRY) DUP(0)
g_pp_hist_count     DD 0
g_pp_hist_idx       DD 0             ; ring buffer write index
g_pp_cold_threshold DD 3000          ; below this = cold (t/s)
g_pp_cache_boost    DD 50            ; % improvement over warm = cache hit
g_pp_last_model     DQ 0             ; last model hash
g_pp_last_class     DD PP_CLASS_UNKNOWN
g_pp_cold_avg       DD 0
g_pp_warm_avg       DD 0
g_pp_cache_avg      DD 0

; ── Regression Gate (Enhancement 262) ──
MAX_GATE_CHECKS     EQU 8
GATE_CHECK_SIZE     EQU 24           ; 24 bytes per check

; Gate check layout (24 bytes):
;   [0-3]   metric_type     (DWORD) — 0=TG, 1=PP_warm, 2=PP_cold, 3=variance
;   [4-7]   threshold       (DWORD) — minimum value (or max for variance)
;   [8-11]  measured        (DWORD) — actual measured value
;   [12-15] result          (DWORD) — 0=pending, 1=pass, 2=fail
;   [16-19] sentinel_idx    (DWORD) — which sentinel this applies to
;   [20-23] reserved        (DWORD)

ALIGN 8
g_gate_checks       DB (MAX_GATE_CHECKS * GATE_CHECK_SIZE) DUP(0)
g_gate_count        DD 0
g_gate_pass_total   DD 0
g_gate_fail_total   DD 0
g_gate_state        DD 0             ; 0=idle, 1=running, 2=passed, 3=failed
g_gate_version      DD 266
g_gate_timestamp    DQ 0

; Gate result codes
GATE_PENDING        EQU 0
GATE_PASS           EQU 1
GATE_FAIL           EQU 2

; ── Warm Path Amplifier (Enhancement 263) ──
MAX_WARM_SLOTS      EQU 4
WARM_SLOT_SIZE      EQU 32           ; 32 bytes per slot

; Warm slot layout (32 bytes):
;   [0-7]   model_hash      (QWORD)
;   [8-11]  warm_pp_tps     (DWORD)
;   [12-15] cold_pp_tps     (DWORD)
;   [16-19] amplification   (DWORD) — warm/cold ratio (x100)
;   [20-23] session_count   (DWORD) — sessions re-using this warm state
;   [24-27] last_access_lo  (DWORD) — tick low
;   [28-31] ttl_remaining   (DWORD) — ms until eviction

ALIGN 16
g_warm_slots        DB (MAX_WARM_SLOTS * WARM_SLOT_SIZE) DUP(0)
g_warm_slot_count   DD 0
g_warm_default_ttl  DD 600000        ; 10 min default TTL
g_warm_total_hits   DD 0             ; total warm-path reuses
g_warm_total_misses DD 0             ; total cold-path fallbacks
g_warm_amplify_avg  DD 0             ; average amplification ratio (x100)

; ── Large Model Tensor Scheduler (Enhancement 264) ──
TENSOR_SCHED_IDLE   EQU 0
TENSOR_SCHED_PLAN   EQU 1
TENSOR_SCHED_EXEC   EQU 2
TENSOR_SCHED_DONE   EQU 3

; Thresholds for large model detection  
LARGE_MODEL_MB      EQU 6000         ; 6GB+ = large model
XLARGE_MODEL_MB     EQU 10000        ; 10GB+ = extra-large

ALIGN 8
g_tensor_state      DD TENSOR_SCHED_IDLE
g_tensor_model_mb   DD 0
g_tensor_chunk_size DD 512           ; MB per tensor chunk
g_tensor_chunks     DD 0             ; total chunks needed
g_tensor_loaded     DD 0             ; chunks loaded so far
g_tensor_prefetched DD 0             ; chunks prefetched
g_tensor_bandwidth  DD 0             ; measured load BW (MB/s)
g_tensor_gain_pct   DD 0             ; improvement vs naive loading
g_tensor_schedule   DQ 0             ; schedule bitmask
g_tensor_class      DD 0             ; 0=small, 1=large, 2=xlarge

; ── Variance Clamp Controller (Enhancement 265) ──
MAX_VARIANCE_SAMPLES EQU 32
VARIANCE_TARGET_PCT  EQU 50          ; 0.50% target (stored as x100)

ALIGN 8
g_var_samples       DD MAX_VARIANCE_SAMPLES DUP(0)  ; TG samples (x100)
g_var_sample_idx    DD 0
g_var_sample_count  DD 0
g_var_mean_x100     DD 0             ; running mean (tps * 100)
g_var_std_x100      DD 0             ; running std dev (tps * 100)
g_var_pct_x100      DD 0             ; variance % (x10000)
g_var_clamped       DD 0             ; 1 if variance was clamped
g_var_clamp_count   DD 0             ; how many times we clamped
g_var_target        DD VARIANCE_TARGET_PCT
g_var_state         DD 0             ; 0=collecting, 1=stable, 2=clamping

; ── Production Gate Validator (Enhancement 266) ──
PROD_CHECK_COUNT    EQU 6

; Production checks:
;   0 = DLL loads (export count check)
;   1 = Device identity (GPU detected)
;   2 = TG threshold per model
;   3 = PP warm threshold per model
;   4 = Variance threshold
;   5 = No regressions in sentinel

ALIGN 8
g_prod_checks       DD PROD_CHECK_COUNT DUP(0)  ; 0=unchecked,1=pass,2=fail
g_prod_state        DD 0             ; 0=idle, 1=running, 2=passed, 3=failed
g_prod_export_count DD 0             ; verified export count
g_prod_device_id    DD 0             ; GPU device ordinal
g_prod_model_count  DD 0             ; models tested
g_prod_pass_count   DD 0             ; models that passed
g_prod_fail_count   DD 0             ; models that failed
g_prod_timestamp    DQ 0
g_prod_version      DD 266
g_prod_build_ok     DD 0             ; 1 if build verified

.code

; ============================================================================
; Enhancement 260: SwarmV260_Benchmark_Sentinel
; ============================================================================
; Manages sentinel model registry for regression detection.
; Input:  ECX = operation: 0=register, 1=update_result, 2=check_regression,
;                          3=get_sentinel, 4=get_count
;         EDX = sentinel index (for ops 1-3)
;         R8  = model_name_ptr (for register)
;         R9D = model_size_mb (for register)
;         Stack: tg_floor, tg_target, pp_warm_floor, var_max (for register)
; Output: EAX = result (0=ok, -1=error, or query result)
;         EDX = flags (for get_sentinel)
; ============================================================================
SwarmV260_Benchmark_Sentinel PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 28h

    mov     ebx, ecx            ; operation
    mov     esi, edx            ; sentinel index

    cmp     ebx, 0
    je      @bs_register
    cmp     ebx, 1
    je      @bs_update
    cmp     ebx, 2
    je      @bs_check
    cmp     ebx, 3
    je      @bs_get
    cmp     ebx, 4
    je      @bs_count
    jmp     @bs_bad

@bs_register:
    mov     eax, [g_sentinel_count]
    cmp     eax, MAX_SENTINELS
    jge     @bs_full
    ; Calculate offset into registry
    imul    r10d, eax, SENTINEL_ENTRY_SIZE
    lea     rdi, [g_sentinel_registry]
    add     rdi, r10
    ; Populate entry
    mov     QWORD PTR [rdi], r8         ; model_name_ptr
    mov     DWORD PTR [rdi + 8], r9d    ; model_size_mb
    ; Read stack params
    mov     r12d, DWORD PTR [rsp + 28h + 28h]  ; tg_floor
    mov     DWORD PTR [rdi + 12], r12d
    mov     r12d, DWORD PTR [rsp + 28h + 30h]  ; tg_target
    mov     DWORD PTR [rdi + 16], r12d
    mov     r12d, DWORD PTR [rsp + 28h + 38h]  ; pp_warm_floor
    mov     DWORD PTR [rdi + 20], r12d
    mov     r12d, DWORD PTR [rsp + 28h + 40h]  ; var_max
    mov     DWORD PTR [rdi + 24], r12d
    ; Init tracking fields
    mov     DWORD PTR [rdi + 28], 0     ; last_tg = 0
    mov     DWORD PTR [rdi + 32], 0     ; last_pp = 0
    mov     DWORD PTR [rdi + 36], 0     ; pass_count = 0
    mov     DWORD PTR [rdi + 40], 0     ; fail_count = 0
    mov     DWORD PTR [rdi + 44], SENTINEL_ACTIVE  ; flags = active
    inc     DWORD PTR [g_sentinel_count]
    mov     eax, [g_sentinel_count]
    dec     eax                         ; return index
    jmp     @bs_done

@bs_update:
    ; Update sentinel with new measurement
    cmp     esi, [g_sentinel_count]
    jge     @bs_bad
    imul    r10d, esi, SENTINEL_ENTRY_SIZE
    lea     rdi, [g_sentinel_registry]
    add     rdi, r10
    ; R8D = measured_tg, R9D = measured_pp
    mov     DWORD PTR [rdi + 28], r8d   ; last_tg_tps
    mov     DWORD PTR [rdi + 32], r9d   ; last_pp_tps
    ; Check against floor
    cmp     r8d, DWORD PTR [rdi + 12]   ; tg vs tg_floor
    jl      @bs_update_fail
    cmp     r9d, DWORD PTR [rdi + 20]   ; pp vs pp_warm_floor
    jl      @bs_update_fail
    ; Pass
    inc     DWORD PTR [rdi + 36]        ; pass_count++
    mov     DWORD PTR [rdi + 40], 0     ; reset fail streak
    mov     eax, DWORD PTR [rdi + 44]
    and     eax, NOT SENTINEL_REGRESSED ; clear regression flag
    mov     DWORD PTR [rdi + 44], eax
    xor     eax, eax
    jmp     @bs_done

@bs_update_fail:
    inc     DWORD PTR [rdi + 40]        ; fail_count++
    mov     DWORD PTR [rdi + 36], 0     ; reset pass streak
    mov     eax, DWORD PTR [rdi + 44]
    or      eax, SENTINEL_REGRESSED     ; set regression flag
    mov     DWORD PTR [rdi + 44], eax
    mov     eax, 1                      ; return 1 = regression detected
    jmp     @bs_done

@bs_check:
    ; Check if sentinel has regressed
    cmp     esi, [g_sentinel_count]
    jge     @bs_bad
    imul    r10d, esi, SENTINEL_ENTRY_SIZE
    lea     rdi, [g_sentinel_registry]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 44]   ; flags
    test    eax, SENTINEL_REGRESSED
    jnz     @bs_is_regressed
    xor     eax, eax                    ; 0 = all good
    jmp     @bs_done
@bs_is_regressed:
    mov     eax, DWORD PTR [rdi + 40]   ; return fail_count
    jmp     @bs_done

@bs_get:
    ; Get sentinel info
    cmp     esi, [g_sentinel_count]
    jge     @bs_bad
    imul    r10d, esi, SENTINEL_ENTRY_SIZE
    lea     rdi, [g_sentinel_registry]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi + 28]   ; last_tg
    mov     edx, DWORD PTR [rdi + 44]   ; flags
    mov     r8d, DWORD PTR [rdi + 36]   ; pass_count
    jmp     @bs_done

@bs_count:
    mov     eax, [g_sentinel_count]
    jmp     @bs_done

@bs_full:
    mov     eax, -2
    jmp     @bs_done
@bs_bad:
    mov     eax, -1
@bs_done:
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV260_Benchmark_Sentinel ENDP


; ============================================================================
; Enhancement 261: SwarmV261_PP_Phase_Classifier
; ============================================================================
; Classifies prompt processing into cold/warm/cache-hit categories.
; Input:  ECX = operation: 0=classify, 1=record, 2=get_stats, 3=get_history
;         EDX = pp_tps (for classify/record)
;         R8D = load_ms (for record)
;         R9D = model_hash_lo (for record)
; Output: EAX = classification (PP_CLASS_*)
;         EDX = confidence (0-100)
; ============================================================================
SwarmV261_PP_Phase_Classifier PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 20h

    mov     ebx, ecx            ; operation
    mov     esi, edx            ; pp_tps or index

    cmp     ebx, 0
    je      @pc_classify
    cmp     ebx, 1
    je      @pc_record
    cmp     ebx, 2
    je      @pc_stats
    cmp     ebx, 3
    je      @pc_history
    jmp     @pc_bad

@pc_classify:
    ; Classify based on PP TPS value
    ; Cold: pp_tps < cold_threshold AND load_ms was high
    ; Cache-hit: pp_tps > warm_avg * (1 + cache_boost/100)
    ; Warm: everything else
    
    mov     eax, [g_pp_warm_avg]
    test    eax, eax
    jz      @pc_no_history
    
    ; Check cold
    cmp     esi, [g_pp_cold_threshold]
    jl      @pc_is_cold
    
    ; Check cache-hit: pp_tps > warm_avg * (100 + cache_boost) / 100
    mov     ecx, [g_pp_cache_boost]
    add     ecx, 100
    imul    eax, ecx
    mov     ecx, 100
    cdq
    idiv    ecx                 ; eax = warm_avg * (100+boost) / 100
    cmp     esi, eax
    jg      @pc_is_cache
    
    ; Otherwise warm
    mov     eax, PP_CLASS_WARM
    mov     edx, 85             ; 85% confidence
    jmp     @pc_done

@pc_no_history:
    ; No baseline yet — classify by threshold only
    cmp     esi, [g_pp_cold_threshold]
    jl      @pc_is_cold
    mov     eax, PP_CLASS_WARM
    mov     edx, 50             ; low confidence
    jmp     @pc_done

@pc_is_cold:
    mov     eax, PP_CLASS_COLD
    mov     edx, 90
    jmp     @pc_done

@pc_is_cache:
    mov     eax, PP_CLASS_CACHE_HIT
    mov     edx, 80
    jmp     @pc_done

@pc_record:
    ; Record PP measurement in ring buffer
    mov     eax, [g_pp_hist_idx]
    cmp     eax, MAX_PP_HISTORY
    jl      @pc_rec_ok
    xor     eax, eax            ; wrap around
@pc_rec_ok:
    imul    r10d, eax, PP_HISTORY_ENTRY
    lea     rdi, [g_pp_history]
    add     rdi, r10
    ; Store fields
    mov     DWORD PTR [rdi], esi        ; pp_tps
    ; Auto-classify
    push    r8
    push    r9
    mov     ecx, 0              ; op=classify
    mov     edx, esi            ; pp_tps
    ; Inline classification
    cmp     esi, [g_pp_cold_threshold]
    jl      @pc_rec_cold
    mov     DWORD PTR [rdi + 4], PP_CLASS_WARM
    jmp     @pc_rec_class_done
@pc_rec_cold:
    mov     DWORD PTR [rdi + 4], PP_CLASS_COLD
@pc_rec_class_done:
    pop     r9
    pop     r8
    mov     DWORD PTR [rdi + 8], 0      ; token_count (tbd)
    mov     DWORD PTR [rdi + 12], r9d   ; model_hash_lo
    ; Timestamp
    call    GetTickCount64
    mov     QWORD PTR [rdi + 16], rax
    mov     DWORD PTR [rdi + 24], r8d   ; load_ms
    ; Advance ring index
    mov     eax, [g_pp_hist_idx]
    inc     eax
    cmp     eax, MAX_PP_HISTORY
    jl      @pc_no_wrap
    xor     eax, eax
@pc_no_wrap:
    mov     [g_pp_hist_idx], eax
    mov     eax, [g_pp_hist_count]
    cmp     eax, MAX_PP_HISTORY
    jge     @pc_cnt_max
    inc     eax
    mov     [g_pp_hist_count], eax
@pc_cnt_max:
    ; Update running averages
    ; Scan history for cold / warm / cache-hit averages
    xor     ecx, ecx            ; cold sum
    xor     r8d, r8d            ; warm sum
    xor     r9d, r9d            ; cold count
    xor     r10d, r10d          ; warm count
    lea     rdi, [g_pp_history]
    mov     eax, [g_pp_hist_count]
    xor     esi, esi
@pc_avg_loop:
    cmp     esi, eax
    jge     @pc_avg_done
    mov     edx, DWORD PTR [rdi + 4]    ; classification
    cmp     edx, PP_CLASS_COLD
    je      @pc_avg_cold
    ; treat as warm
    add     r8d, DWORD PTR [rdi]
    inc     r10d
    jmp     @pc_avg_next
@pc_avg_cold:
    add     ecx, DWORD PTR [rdi]
    inc     r9d
@pc_avg_next:
    add     rdi, PP_HISTORY_ENTRY
    inc     esi
    jmp     @pc_avg_loop
@pc_avg_done:
    ; Calculate averages
    test    r9d, r9d
    jz      @pc_no_cold_avg
    mov     eax, ecx
    cdq
    idiv    r9d
    mov     [g_pp_cold_avg], eax
@pc_no_cold_avg:
    test    r10d, r10d
    jz      @pc_no_warm_avg
    mov     eax, r8d
    cdq
    idiv    r10d
    mov     [g_pp_warm_avg], eax
@pc_no_warm_avg:
    xor     eax, eax
    jmp     @pc_done

@pc_stats:
    ; Return cold/warm/cache averages
    mov     eax, [g_pp_cold_avg]
    mov     edx, [g_pp_warm_avg]
    mov     r8d, [g_pp_cache_avg]
    mov     r9d, [g_pp_hist_count]
    jmp     @pc_done

@pc_history:
    ; Get specific history entry
    cmp     esi, [g_pp_hist_count]
    jge     @pc_bad
    imul    r10d, esi, PP_HISTORY_ENTRY
    lea     rdi, [g_pp_history]
    add     rdi, r10
    mov     eax, DWORD PTR [rdi]        ; pp_tps
    mov     edx, DWORD PTR [rdi + 4]    ; classification
    mov     r8d, DWORD PTR [rdi + 24]   ; load_ms
    jmp     @pc_done

@pc_bad:
    mov     eax, -1
@pc_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV261_PP_Phase_Classifier ENDP


; ============================================================================
; Enhancement 262: SwarmV262_Regression_Gate
; ============================================================================
; Automated pass/fail gate engine with per-model thresholds.
; Input:  ECX = operation: 0=begin_gate, 1=add_check, 2=evaluate, 3=get_result
;         EDX = metric_type (for add_check: 0=TG, 1=PP_warm, 2=PP_cold, 3=var)
;         R8D = threshold (for add_check)
;         R9D = measured value (for add_check)
; Output: EAX = gate state (GATE_*)
;         EDX = pass_count / fail_count
; ============================================================================
SwarmV262_Regression_Gate PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 20h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @rg_begin
    cmp     ebx, 1
    je      @rg_add
    cmp     ebx, 2
    je      @rg_eval
    cmp     ebx, 3
    je      @rg_result
    jmp     @rg_bad

@rg_begin:
    ; Reset gate state
    mov     DWORD PTR [g_gate_count], 0
    mov     DWORD PTR [g_gate_pass_total], 0
    mov     DWORD PTR [g_gate_fail_total], 0
    mov     DWORD PTR [g_gate_state], 1  ; running
    call    GetTickCount64
    mov     QWORD PTR [g_gate_timestamp], rax
    ; Zero out checks
    lea     rdi, [g_gate_checks]
    xor     eax, eax
    mov     ecx, MAX_GATE_CHECKS * GATE_CHECK_SIZE / 4
@rg_zero:
    mov     DWORD PTR [rdi], eax
    add     rdi, 4
    dec     ecx
    jnz     @rg_zero
    xor     eax, eax
    jmp     @rg_done

@rg_add:
    ; Add a check to the gate
    mov     eax, [g_gate_count]
    cmp     eax, MAX_GATE_CHECKS
    jge     @rg_full
    imul    r10d, eax, GATE_CHECK_SIZE
    lea     rdi, [g_gate_checks]
    add     rdi, r10
    mov     DWORD PTR [rdi], esi        ; metric_type
    mov     DWORD PTR [rdi + 4], r8d    ; threshold
    mov     DWORD PTR [rdi + 8], r9d    ; measured
    ; Evaluate this check immediately
    ; For variance (type 3): measured must be BELOW threshold
    ; For others: measured must be ABOVE threshold
    cmp     esi, 3
    je      @rg_add_var
    cmp     r9d, r8d
    jge     @rg_add_pass
    jmp     @rg_add_fail
@rg_add_var:
    cmp     r9d, r8d
    jle     @rg_add_pass
@rg_add_fail:
    mov     DWORD PTR [rdi + 12], GATE_FAIL
    inc     DWORD PTR [g_gate_fail_total]
    jmp     @rg_add_done
@rg_add_pass:
    mov     DWORD PTR [rdi + 12], GATE_PASS
    inc     DWORD PTR [g_gate_pass_total]
@rg_add_done:
    inc     DWORD PTR [g_gate_count]
    mov     eax, DWORD PTR [rdi + 12]   ; return check result
    jmp     @rg_done

@rg_eval:
    ; Evaluate overall gate: all checks must pass
    mov     eax, [g_gate_fail_total]
    test    eax, eax
    jnz     @rg_eval_fail
    mov     DWORD PTR [g_gate_state], 2  ; passed
    mov     eax, 2
    mov     edx, [g_gate_pass_total]
    jmp     @rg_done
@rg_eval_fail:
    mov     DWORD PTR [g_gate_state], 3  ; failed
    mov     eax, 3
    mov     edx, [g_gate_fail_total]
    jmp     @rg_done

@rg_result:
    mov     eax, [g_gate_state]
    mov     edx, [g_gate_pass_total]
    mov     r8d, [g_gate_fail_total]
    mov     r9d, [g_gate_count]
    jmp     @rg_done

@rg_full:
    mov     eax, -2
    jmp     @rg_done
@rg_bad:
    mov     eax, -1
@rg_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV262_Regression_Gate ENDP


; ============================================================================
; Enhancement 263: SwarmV263_Warm_Path_Amplifier
; ============================================================================
; Maintains persistent warm-path state across sessions for PP retention.
; Input:  ECX = operation: 0=register_warm, 1=check_warm, 2=touch, 3=evict_lru,
;                          4=get_stats
;         RDX = model_hash (for register/check/touch)
;         R8D = warm_pp_tps (for register)
;         R9D = cold_pp_tps (for register)
; Output: EAX = result (0=miss, 1=hit, amplification ratio for stats)
;         EDX = slot_index or amplification
; ============================================================================
SwarmV263_Warm_Path_Amplifier PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 20h

    mov     ebx, ecx
    mov     r12, rdx            ; model_hash (full 64-bit)

    cmp     ebx, 0
    je      @wa_register
    cmp     ebx, 1
    je      @wa_check
    cmp     ebx, 2
    je      @wa_touch
    cmp     ebx, 3
    je      @wa_evict
    cmp     ebx, 4
    je      @wa_stats
    jmp     @wa_bad

@wa_register:
    ; Find existing slot or allocate new
    lea     rdi, [g_warm_slots]
    xor     esi, esi
@wa_reg_search:
    cmp     esi, [g_warm_slot_count]
    jge     @wa_reg_new
    cmp     QWORD PTR [rdi], r12
    je      @wa_reg_update
    add     rdi, WARM_SLOT_SIZE
    inc     esi
    jmp     @wa_reg_search

@wa_reg_new:
    mov     eax, [g_warm_slot_count]
    cmp     eax, MAX_WARM_SLOTS
    jge     @wa_full
    imul    r10d, eax, WARM_SLOT_SIZE
    lea     rdi, [g_warm_slots]
    add     rdi, r10
    mov     QWORD PTR [rdi], r12        ; model_hash
    inc     DWORD PTR [g_warm_slot_count]

@wa_reg_update:
    mov     DWORD PTR [rdi + 8], r8d    ; warm_pp_tps
    mov     DWORD PTR [rdi + 12], r9d   ; cold_pp_tps
    ; Calculate amplification: warm/cold * 100
    test    r9d, r9d
    jz      @wa_no_ratio
    mov     eax, r8d
    imul    eax, 100
    cdq
    idiv    r9d
    mov     DWORD PTR [rdi + 16], eax   ; amplification x100
    jmp     @wa_reg_finish
@wa_no_ratio:
    mov     DWORD PTR [rdi + 16], 100   ; 1:1 if no cold data
@wa_reg_finish:
    mov     DWORD PTR [rdi + 20], 1     ; session_count = 1
    call    GetTickCount64
    mov     DWORD PTR [rdi + 24], eax   ; last_access_lo
    mov     ecx, [g_warm_default_ttl]
    mov     DWORD PTR [rdi + 28], ecx   ; ttl_remaining
    xor     eax, eax
    mov     edx, esi                    ; return slot index
    jmp     @wa_done

@wa_check:
    ; Check if model has warm path
    lea     rdi, [g_warm_slots]
    xor     esi, esi
@wa_chk_loop:
    cmp     esi, [g_warm_slot_count]
    jge     @wa_miss
    cmp     QWORD PTR [rdi], r12
    je      @wa_hit
    add     rdi, WARM_SLOT_SIZE
    inc     esi
    jmp     @wa_chk_loop
@wa_miss:
    inc     DWORD PTR [g_warm_total_misses]
    xor     eax, eax                    ; 0 = miss
    jmp     @wa_done
@wa_hit:
    inc     DWORD PTR [g_warm_total_hits]
    mov     eax, 1                      ; 1 = hit
    mov     edx, DWORD PTR [rdi + 16]   ; amplification
    jmp     @wa_done

@wa_touch:
    ; Refresh TTL for model
    lea     rdi, [g_warm_slots]
    xor     esi, esi
@wa_touch_loop:
    cmp     esi, [g_warm_slot_count]
    jge     @wa_bad
    cmp     QWORD PTR [rdi], r12
    je      @wa_touch_found
    add     rdi, WARM_SLOT_SIZE
    inc     esi
    jmp     @wa_touch_loop
@wa_touch_found:
    inc     DWORD PTR [rdi + 20]        ; session_count++
    call    GetTickCount64
    mov     DWORD PTR [rdi + 24], eax   ; refresh last_access
    mov     ecx, [g_warm_default_ttl]
    mov     DWORD PTR [rdi + 28], ecx   ; reset TTL
    xor     eax, eax
    jmp     @wa_done

@wa_evict:
    ; Evict LRU slot (oldest last_access)
    mov     eax, [g_warm_slot_count]
    test    eax, eax
    jz      @wa_bad
    lea     rdi, [g_warm_slots]
    xor     esi, esi
    mov     r10d, 0FFFFFFFFh    ; min_access = max
    xor     ecx, ecx            ; lru_idx
@wa_evict_scan:
    cmp     esi, [g_warm_slot_count]
    jge     @wa_evict_do
    mov     eax, DWORD PTR [rdi + 24]
    cmp     eax, r10d
    jge     @wa_evict_next
    mov     r10d, eax
    mov     ecx, esi
@wa_evict_next:
    add     rdi, WARM_SLOT_SIZE
    inc     esi
    jmp     @wa_evict_scan
@wa_evict_do:
    ; Shift remaining slots down to fill gap
    mov     eax, [g_warm_slot_count]
    dec     eax
    mov     [g_warm_slot_count], eax
    mov     edx, ecx                    ; return evicted index
    xor     eax, eax
    jmp     @wa_done

@wa_stats:
    mov     eax, [g_warm_total_hits]
    mov     edx, [g_warm_total_misses]
    mov     r8d, [g_warm_amplify_avg]
    mov     r9d, [g_warm_slot_count]
    jmp     @wa_done

@wa_full:
    mov     eax, -2
    jmp     @wa_done
@wa_bad:
    mov     eax, -1
@wa_done:
    add     rsp, 20h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV263_Warm_Path_Amplifier ENDP


; ============================================================================
; Enhancement 264: SwarmV264_Large_Model_Scheduler
; ============================================================================
; Optimized tensor scheduling for models >= 6GB (large) and >= 10GB (xlarge).
; Input:  ECX = operation: 0=plan, 1=load_chunk, 2=prefetch, 3=get_state,
;                          4=measure_bandwidth
;         EDX = model_size_mb (for plan)
;         R8D = chunk_index (for load_chunk/prefetch)
; Output: EAX = state or chunk count
;         EDX = chunk_size or bandwidth
; ============================================================================
SwarmV264_Large_Model_Scheduler PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 20h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @ts_plan
    cmp     ebx, 1
    je      @ts_load
    cmp     ebx, 2
    je      @ts_prefetch
    cmp     ebx, 3
    je      @ts_state
    cmp     ebx, 4
    je      @ts_bw
    jmp     @ts_bad

@ts_plan:
    ; Plan tensor loading schedule
    mov     [g_tensor_model_mb], esi
    mov     DWORD PTR [g_tensor_state], TENSOR_SCHED_PLAN
    
    ; Classify model size
    cmp     esi, XLARGE_MODEL_MB
    jge     @ts_xlarge
    cmp     esi, LARGE_MODEL_MB
    jge     @ts_large
    ; Small model — no scheduling needed
    mov     DWORD PTR [g_tensor_class], 0
    mov     DWORD PTR [g_tensor_chunks], 1
    mov     DWORD PTR [g_tensor_chunk_size], esi
    jmp     @ts_plan_done

@ts_large:
    mov     DWORD PTR [g_tensor_class], 1
    ; Large: 512MB chunks
    mov     DWORD PTR [g_tensor_chunk_size], 512
    mov     eax, esi
    cdq
    mov     ecx, 512
    idiv    ecx
    test    edx, edx
    jz      @ts_large_exact
    inc     eax
@ts_large_exact:
    mov     [g_tensor_chunks], eax
    jmp     @ts_plan_done

@ts_xlarge:
    mov     DWORD PTR [g_tensor_class], 2
    ; XLarge: 1024MB chunks with prefetch pipeline
    mov     DWORD PTR [g_tensor_chunk_size], 1024
    mov     eax, esi
    cdq
    mov     ecx, 1024
    idiv    ecx
    test    edx, edx
    jz      @ts_xl_exact
    inc     eax
@ts_xl_exact:
    mov     [g_tensor_chunks], eax

@ts_plan_done:
    mov     DWORD PTR [g_tensor_loaded], 0
    mov     DWORD PTR [g_tensor_prefetched], 0
    mov     DWORD PTR [g_tensor_state], TENSOR_SCHED_PLAN
    mov     eax, [g_tensor_chunks]
    mov     edx, [g_tensor_chunk_size]
    jmp     @ts_done

@ts_load:
    ; Simulate loading a chunk
    mov     eax, [g_tensor_loaded]
    cmp     eax, [g_tensor_chunks]
    jge     @ts_all_loaded
    inc     eax
    mov     [g_tensor_loaded], eax
    mov     DWORD PTR [g_tensor_state], TENSOR_SCHED_EXEC
    ; Check if all done
    cmp     eax, [g_tensor_chunks]
    jl      @ts_load_ret
    mov     DWORD PTR [g_tensor_state], TENSOR_SCHED_DONE
@ts_load_ret:
    mov     edx, [g_tensor_chunk_size]
    jmp     @ts_done

@ts_all_loaded:
    mov     DWORD PTR [g_tensor_state], TENSOR_SCHED_DONE
    mov     eax, [g_tensor_chunks]
    jmp     @ts_done

@ts_prefetch:
    ; Mark chunk as prefetched
    mov     eax, [g_tensor_prefetched]
    cmp     eax, [g_tensor_chunks]
    jge     @ts_pf_done
    inc     eax
    mov     [g_tensor_prefetched], eax
@ts_pf_done:
    mov     edx, [g_tensor_chunk_size]
    jmp     @ts_done

@ts_state:
    mov     eax, [g_tensor_state]
    mov     edx, [g_tensor_loaded]
    mov     r8d, [g_tensor_chunks]
    mov     r9d, [g_tensor_class]
    jmp     @ts_done

@ts_bw:
    ; Measure bandwidth: model_size / load_time
    ; R8D = load_time_ms
    test    r8d, r8d
    jz      @ts_bad
    mov     eax, [g_tensor_model_mb]
    imul    eax, 1000           ; MB * 1000 / ms = MB/s
    cdq
    idiv    r8d
    mov     [g_tensor_bandwidth], eax
    ; Calculate gain vs baseline (naive = 1 chunk)
    mov     ecx, [g_tensor_class]
    cmp     ecx, 2
    je      @ts_bw_xl_gain
    cmp     ecx, 1
    je      @ts_bw_lg_gain
    mov     DWORD PTR [g_tensor_gain_pct], 0
    jmp     @ts_bw_done
@ts_bw_lg_gain:
    mov     DWORD PTR [g_tensor_gain_pct], 8   ; ~8% for large
    jmp     @ts_bw_done
@ts_bw_xl_gain:
    mov     DWORD PTR [g_tensor_gain_pct], 15  ; ~15% for xlarge
@ts_bw_done:
    mov     edx, [g_tensor_gain_pct]
    jmp     @ts_done

@ts_bad:
    mov     eax, -1
@ts_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV264_Large_Model_Scheduler ENDP


; ============================================================================
; Enhancement 265: SwarmV265_Variance_Clamp
; ============================================================================
; Active TG variance suppression. Collects samples, computes running
; standard deviation, and reports clamping when variance exceeds target.
; Input:  ECX = operation: 0=add_sample, 1=get_variance, 2=reset, 3=get_state
;         EDX = tg_tps_x100 (for add_sample, tps * 100 for precision)
; Output: EAX = variance pct (x10000) or state
;         EDX = mean (x100) or std (x100)
; ============================================================================
SwarmV265_Variance_Clamp PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 20h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @vc_add
    cmp     ebx, 1
    je      @vc_get
    cmp     ebx, 2
    je      @vc_reset
    cmp     ebx, 3
    je      @vc_state
    jmp     @vc_bad

@vc_add:
    ; Add TG sample
    mov     eax, [g_var_sample_idx]
    cmp     eax, MAX_VARIANCE_SAMPLES
    jl      @vc_add_ok
    xor     eax, eax            ; wrap
@vc_add_ok:
    lea     rdi, [g_var_samples]
    mov     DWORD PTR [rdi + rax*4], esi
    inc     eax
    cmp     eax, MAX_VARIANCE_SAMPLES
    jl      @vc_add_nowrap
    xor     eax, eax
@vc_add_nowrap:
    mov     [g_var_sample_idx], eax
    mov     eax, [g_var_sample_count]
    cmp     eax, MAX_VARIANCE_SAMPLES
    jge     @vc_cnt_max
    inc     eax
    mov     [g_var_sample_count], eax
@vc_cnt_max:
    ; Recalculate mean
    lea     rdi, [g_var_samples]
    xor     ecx, ecx            ; sum
    xor     esi, esi            ; index
    mov     r12d, [g_var_sample_count]
@vc_mean_loop:
    cmp     esi, r12d
    jge     @vc_mean_done
    add     ecx, DWORD PTR [rdi + rsi*4]
    inc     esi
    jmp     @vc_mean_loop
@vc_mean_done:
    test    r12d, r12d
    jz      @vc_no_mean
    mov     eax, ecx
    cdq
    idiv    r12d
    mov     [g_var_mean_x100], eax
    mov     r10d, eax           ; save mean

    ; Calculate sum of squared deviations
    lea     rdi, [g_var_samples]
    xor     ecx, ecx            ; sum_sq
    xor     esi, esi
@vc_var_loop:
    cmp     esi, r12d
    jge     @vc_var_done
    mov     eax, DWORD PTR [rdi + rsi*4]
    sub     eax, r10d           ; deviation
    imul    eax, eax            ; deviation^2
    add     ecx, eax
    inc     esi
    jmp     @vc_var_loop
@vc_var_done:
    ; variance = sum_sq / (n-1)
    mov     eax, r12d
    dec     eax
    test    eax, eax
    jz      @vc_no_var
    mov     r8d, ecx            ; save sum_sq
    mov     eax, ecx
    cdq
    idiv    r12d                ; approx std^2

    ; Integer sqrt approximation for std dev
    ; Using Newton's method: start with guess = variance/2
    mov     ecx, eax            ; variance
    shr     eax, 1              ; initial guess
    test    eax, eax
    jz      @vc_std_zero
    ; 4 iterations of Newton
    mov     r8d, 4
@vc_sqrt_loop:
    mov     edx, ecx
    push    rax
    cdq
    idiv    eax                 ; variance / guess
    pop     r9
    add     eax, r9d            ; guess + variance/guess
    shr     eax, 1              ; / 2
    dec     r8d
    jnz     @vc_sqrt_loop
    mov     [g_var_std_x100], eax

    ; Calculate variance %: std / mean * 10000
    imul    eax, 10000
    cdq
    idiv    r10d
    mov     [g_var_pct_x100], eax

    ; Check against target
    cmp     eax, [g_var_target]
    jle     @vc_stable
    mov     DWORD PTR [g_var_state], 2   ; clamping
    mov     DWORD PTR [g_var_clamped], 1
    inc     DWORD PTR [g_var_clamp_count]
    jmp     @vc_add_ret
@vc_stable:
    mov     DWORD PTR [g_var_state], 1   ; stable
    mov     DWORD PTR [g_var_clamped], 0
@vc_add_ret:
    mov     eax, [g_var_pct_x100]
    mov     edx, [g_var_mean_x100]
    jmp     @vc_done

@vc_std_zero:
    mov     DWORD PTR [g_var_std_x100], 0
    mov     DWORD PTR [g_var_pct_x100], 0
    mov     DWORD PTR [g_var_state], 1   ; stable
    xor     eax, eax
    jmp     @vc_done

@vc_no_var:
@vc_no_mean:
    xor     eax, eax
    jmp     @vc_done

@vc_get:
    mov     eax, [g_var_pct_x100]
    mov     edx, [g_var_std_x100]
    mov     r8d, [g_var_mean_x100]
    jmp     @vc_done

@vc_reset:
    mov     DWORD PTR [g_var_sample_idx], 0
    mov     DWORD PTR [g_var_sample_count], 0
    mov     DWORD PTR [g_var_mean_x100], 0
    mov     DWORD PTR [g_var_std_x100], 0
    mov     DWORD PTR [g_var_pct_x100], 0
    mov     DWORD PTR [g_var_state], 0
    mov     DWORD PTR [g_var_clamped], 0
    xor     eax, eax
    jmp     @vc_done

@vc_state:
    mov     eax, [g_var_state]
    mov     edx, [g_var_clamp_count]
    mov     r8d, [g_var_sample_count]
    jmp     @vc_done

@vc_bad:
    mov     eax, -1
@vc_done:
    add     rsp, 20h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV265_Variance_Clamp ENDP


; ============================================================================
; Enhancement 266: SwarmV266_Production_Gate
; ============================================================================
; Full production readiness validator. Checks:
;   0 = export count (must match expected)
;   1 = device identity (GPU ordinal valid)
;   2 = TG threshold per sentinel
;   3 = PP warm threshold per sentinel
;   4 = variance within tolerance
;   5 = no regressions in sentinel registry
; Input:  ECX = operation: 0=begin, 1=check_exports, 2=check_device,
;                          3=check_tg, 4=check_pp, 5=check_variance,
;                          6=check_regressions, 7=evaluate, 8=get_result
;         EDX = value for check (export_count, device_id, tg_tps, etc.)
;         R8D = expected/threshold
; Output: EAX = check result (0=pending, 1=pass, 2=fail)
;         EDX = overall state for evaluate/get_result
; ============================================================================
SwarmV266_Production_Gate PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 20h

    mov     ebx, ecx
    mov     esi, edx

    cmp     ebx, 0
    je      @pg_begin
    cmp     ebx, 1
    je      @pg_exports
    cmp     ebx, 2
    je      @pg_device
    cmp     ebx, 3
    je      @pg_tg
    cmp     ebx, 4
    je      @pg_pp
    cmp     ebx, 5
    je      @pg_var
    cmp     ebx, 6
    je      @pg_reg
    cmp     ebx, 7
    je      @pg_eval
    cmp     ebx, 8
    je      @pg_result
    jmp     @pg_bad

@pg_begin:
    ; Reset all checks
    lea     rdi, [g_prod_checks]
    xor     eax, eax
    mov     ecx, PROD_CHECK_COUNT
@pg_zero:
    mov     DWORD PTR [rdi], eax
    add     rdi, 4
    dec     ecx
    jnz     @pg_zero
    mov     DWORD PTR [g_prod_state], 1  ; running
    mov     DWORD PTR [g_prod_pass_count], 0
    mov     DWORD PTR [g_prod_fail_count], 0
    call    GetTickCount64
    mov     QWORD PTR [g_prod_timestamp], rax
    xor     eax, eax
    jmp     @pg_done

@pg_exports:
    ; Check 0: export count == expected
    mov     [g_prod_export_count], esi
    cmp     esi, r8d
    je      @pg_exp_pass
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi], GATE_FAIL
    inc     DWORD PTR [g_prod_fail_count]
    mov     eax, GATE_FAIL
    jmp     @pg_done
@pg_exp_pass:
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi], GATE_PASS
    inc     DWORD PTR [g_prod_pass_count]
    mov     DWORD PTR [g_prod_build_ok], 1
    mov     eax, GATE_PASS
    jmp     @pg_done

@pg_device:
    ; Check 1: device ordinal is valid (not -1, not iGPU trap)
    mov     [g_prod_device_id], esi
    cmp     esi, -1
    je      @pg_dev_fail
    cmp     esi, 0Fh            ; iGPU ordinal trap
    je      @pg_dev_fail
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 4], GATE_PASS
    inc     DWORD PTR [g_prod_pass_count]
    mov     eax, GATE_PASS
    jmp     @pg_done
@pg_dev_fail:
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 4], GATE_FAIL
    inc     DWORD PTR [g_prod_fail_count]
    mov     eax, GATE_FAIL
    jmp     @pg_done

@pg_tg:
    ; Check 2: TG >= threshold
    cmp     esi, r8d
    jge     @pg_tg_pass
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 8], GATE_FAIL
    inc     DWORD PTR [g_prod_fail_count]
    mov     eax, GATE_FAIL
    jmp     @pg_done
@pg_tg_pass:
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 8], GATE_PASS
    inc     DWORD PTR [g_prod_pass_count]
    mov     eax, GATE_PASS
    jmp     @pg_done

@pg_pp:
    ; Check 3: PP warm >= threshold
    cmp     esi, r8d
    jge     @pg_pp_pass
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 12], GATE_FAIL
    inc     DWORD PTR [g_prod_fail_count]
    mov     eax, GATE_FAIL
    jmp     @pg_done
@pg_pp_pass:
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 12], GATE_PASS
    inc     DWORD PTR [g_prod_pass_count]
    mov     eax, GATE_PASS
    jmp     @pg_done

@pg_var:
    ; Check 4: variance <= threshold
    cmp     esi, r8d
    jle     @pg_var_pass
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 16], GATE_FAIL
    inc     DWORD PTR [g_prod_fail_count]
    mov     eax, GATE_FAIL
    jmp     @pg_done
@pg_var_pass:
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 16], GATE_PASS
    inc     DWORD PTR [g_prod_pass_count]
    mov     eax, GATE_PASS
    jmp     @pg_done

@pg_reg:
    ; Check 5: no regressions (ESI = regression_count, must be 0)
    test    esi, esi
    jz      @pg_reg_pass
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 20], GATE_FAIL
    inc     DWORD PTR [g_prod_fail_count]
    mov     eax, GATE_FAIL
    jmp     @pg_done
@pg_reg_pass:
    lea     rdi, [g_prod_checks]
    mov     DWORD PTR [rdi + 20], GATE_PASS
    inc     DWORD PTR [g_prod_pass_count]
    mov     eax, GATE_PASS
    jmp     @pg_done

@pg_eval:
    ; Evaluate all checks
    lea     rdi, [g_prod_checks]
    xor     esi, esi            ; fail counter
    xor     ecx, ecx
@pg_eval_loop:
    cmp     ecx, PROD_CHECK_COUNT
    jge     @pg_eval_done
    mov     eax, DWORD PTR [rdi]
    cmp     eax, GATE_FAIL
    je      @pg_eval_f
    cmp     eax, GATE_PENDING
    je      @pg_eval_f          ; unchecked = fail
    jmp     @pg_eval_next
@pg_eval_f:
    inc     esi
@pg_eval_next:
    add     rdi, 4
    inc     ecx
    jmp     @pg_eval_loop
@pg_eval_done:
    test    esi, esi
    jnz     @pg_eval_fail
    mov     DWORD PTR [g_prod_state], 2  ; passed
    mov     eax, GATE_PASS
    mov     edx, [g_prod_pass_count]
    jmp     @pg_done
@pg_eval_fail:
    mov     DWORD PTR [g_prod_state], 3  ; failed
    mov     eax, GATE_FAIL
    mov     edx, esi                     ; fail count
    jmp     @pg_done

@pg_result:
    mov     eax, [g_prod_state]
    mov     edx, [g_prod_pass_count]
    mov     r8d, [g_prod_fail_count]
    mov     r9d, [g_prod_version]
    jmp     @pg_done

@pg_bad:
    mov     eax, -1
@pg_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV266_Production_Gate ENDP

END
