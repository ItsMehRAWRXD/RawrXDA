; ============================================================================
; RawrXD_Performance_253_259.asm — Enhancements 253-259
; Performance Optimization + Smoke Test Kernel
; ============================================================================
; 253: KV Cache Prewarmer       — Eliminate cold-start prompt penalty
; 254: Batch Prompt Coalescer   — Merge small prompts for throughput
; 255: VRAM Pressure Monitor    — Track VRAM to prevent OOM
; 256: Token Pipeline Prefetch  — Double-buffer token decode pipeline
; 257: Adaptive Context Window  — Dynamic context sizing per VRAM
; 258: Inference Latency Profiler — Per-phase timing instrumentation
; 259: Sovereign Smoke Test     — Automated multi-model benchmark kernel
; ============================================================================

extrn GetTickCount64 : proc

; ── External references (from RawrXD_Sovereign_Router.asm) ──
EXTERN SwarmV239_Backend_Router:PROC
EXTERN SwarmV244_iGPU_Firewall:PROC
EXTERN SwarmV245_Hybrid_Mode_Switch:PROC
EXTERN SwarmV246_Route_Reason_Query:PROC

; ── External references (from RawrXD_Swarm_v23.asm) ──
EXTERN SwarmV247_Swarm_Coordinator:PROC
EXTERN SwarmV250_Verifier:PROC
EXTERN SwarmV252_Feedback_Loop:PROC

PUBLIC SwarmV253_KV_Cache_Prewarmer
PUBLIC SwarmV254_Batch_Prompt_Coalescer
PUBLIC SwarmV255_VRAM_Pressure_Monitor
PUBLIC SwarmV256_Token_Pipeline_Prefetch
PUBLIC SwarmV257_Adaptive_Context_Window
PUBLIC SwarmV258_Inference_Latency_Profiler
PUBLIC SwarmV259_Sovereign_Smoke_Test

.data

; ── KV Cache Prewarmer State (Enhancement 253) ──
KV_STATE_COLD       EQU 0
KV_STATE_WARMING    EQU 1
KV_STATE_HOT        EQU 2
KV_STATE_EVICTED    EQU 3

ALIGN 8
g_kv_state          DD KV_STATE_COLD
g_kv_model_hash     DQ 0
g_kv_warm_tick      DQ 0
g_kv_cold_pp_tps    DD 0          ; prompt TPS before warmup
g_kv_hot_pp_tps     DD 0          ; prompt TPS after warmup
g_kv_warmup_count   DD 0          ; number of warmup rounds executed
g_kv_ttl_ms         DD 300000     ; 5 min TTL before eviction

; ── Batch Coalescer State (Enhancement 254) ──
MAX_BATCH_ENTRIES   EQU 16
BATCH_ENTRY_SIZE    EQU 32        ; 32 bytes per entry

; Batch entry layout (32 bytes):
;   [0-7]   prompt_ptr (QWORD)
;   [8-11]  prompt_len (DWORD)
;   [12-15] priority (DWORD)
;   [16-19] model_hash_lo (DWORD)
;   [20-23] status (DWORD)  ; 0=pending,1=running,2=done,3=failed
;   [24-31] result_tps (DOUBLE as QWORD)
ALIGN 16
g_batch_queue       DB (MAX_BATCH_ENTRIES * BATCH_ENTRY_SIZE) DUP(0)
g_batch_count       DD 0
g_batch_total_tps   DD 0
g_batch_completed   DD 0

; ── VRAM Pressure Monitor State (Enhancement 255) ──
VRAM_TOTAL_MB       EQU 16368     ; RX 7800 XT actual
VRAM_WARN_PCT       EQU 85        ; Warning at 85%
VRAM_CRITICAL_PCT   EQU 95        ; Critical at 95%
VRAM_OK             EQU 0
VRAM_WARNING        EQU 1
VRAM_CRITICAL       EQU 2
VRAM_OOM            EQU 3

ALIGN 8
g_vram_used_mb      DD 0
g_vram_free_mb      DD VRAM_TOTAL_MB
g_vram_pressure     DD VRAM_OK
g_vram_peak_mb      DD 0
g_vram_model_mb     DD 0          ; current model VRAM footprint
g_vram_kv_mb        DD 0          ; KV cache VRAM usage
g_vram_sample_tick  DQ 0

; ── Token Pipeline Prefetch State (Enhancement 256) ──
PIPE_IDLE           EQU 0
PIPE_PREFETCHING    EQU 1
PIPE_READY          EQU 2
PIPE_STALLED        EQU 3

ALIGN 8
g_pipe_state        DD PIPE_IDLE
g_pipe_depth        DD 2          ; double-buffer depth
g_pipe_hits         DD 0          ; successful prefetch hits
g_pipe_misses       DD 0          ; prefetch misses (stalls)
g_pipe_total_tokens DD 0
g_pipe_saved_ms     DD 0          ; estimated ms saved by prefetch

; ── Adaptive Context Window State (Enhancement 257) ──
CTX_MIN             EQU 2048      ; minimum context
CTX_DEFAULT         EQU 8192      ; default context
CTX_MAX             EQU 131072    ; max for phi3

ALIGN 8
g_ctx_current       DD CTX_DEFAULT
g_ctx_optimal       DD 0
g_ctx_model_max     DD CTX_MAX
g_ctx_vram_limit    DD 0          ; context limited by VRAM
g_ctx_history       DD 8 DUP(0)   ; last 8 context sizes used

; ── Inference Latency Profiler State (Enhancement 258) ──
; Phase IDs
PHASE_LOAD          EQU 0
PHASE_PROMPT        EQU 1
PHASE_GENERATE      EQU 2
PHASE_FINALIZE      EQU 3
PHASE_TOTAL         EQU 4
MAX_PHASES          EQU 5

; Profile entry: 24 bytes per phase
;   [0-7]   start_tick (QWORD)
;   [8-15]  end_tick (QWORD)
;   [16-19] duration_ms (DWORD)
;   [20-23] tokens (DWORD)
PROFILE_ENTRY_SIZE  EQU 24

ALIGN 16
g_profile_data      DB (MAX_PHASES * PROFILE_ENTRY_SIZE) DUP(0)
g_profile_active    DD 0          ; currently profiling?
g_profile_run_count DD 0
g_profile_best_ms   DD 0FFFFFFFFh ; best total time
g_profile_worst_ms  DD 0          ; worst total time
g_profile_avg_ms    DD 0

; ── Smoke Test State (Enhancement 259) ──
SMOKE_IDLE          EQU 0
SMOKE_RUNNING       EQU 1
SMOKE_PASS          EQU 2
SMOKE_FAIL          EQU 3

MAX_SMOKE_MODELS    EQU 8
SMOKE_ENTRY_SIZE    EQU 64

; Smoke entry: 64 bytes
;   [0-7]   model_name_ptr (QWORD)
;   [8-11]  model_size_mb (DWORD)
;   [12-15] backend_used (DWORD)
;   [16-19] pp_tps (DWORD)
;   [20-23] tg_tps (DWORD)
;   [24-27] wall_ms (DWORD)
;   [28-31] verdict (DWORD)  ; SMOKE_PASS/FAIL
;   [32-35] expected_tps (DWORD)
;   [36-39] deviation_pct (DWORD, signed)
;   [40-43] vram_used_mb (DWORD)
;   [44-47] ctx_used (DWORD)
;   [48-55] timestamp (QWORD)
;   [56-59] reason_code (DWORD)
;   [60-63] reserved (DWORD)

ALIGN 16
g_smoke_results     DB (MAX_SMOKE_MODELS * SMOKE_ENTRY_SIZE) DUP(0)
g_smoke_count       DD 0
g_smoke_passed      DD 0
g_smoke_failed      DD 0
g_smoke_state       DD SMOKE_IDLE
g_smoke_start_tick  DQ 0
g_smoke_total_ms    DD 0

.code

; ============================================================================
; Enhancement 253: SwarmV253_KV_Cache_Prewarmer
; ============================================================================
; Tracks KV cache state and optimizes cold-start prompt processing.
; Input:  ECX = model_hash (low 32 bits)
;         EDX = 0=query_state, 1=mark_warming, 2=mark_hot, 3=evict
;         R8D = measured pp_tps (for state transitions)
; Output: EAX = current KV state (COLD/WARMING/HOT/EVICTED)
;         EDX = warmup gain % (hot_pp_tps vs cold_pp_tps)
; ============================================================================
SwarmV253_KV_Cache_Prewarmer PROC
    push    rbx
    push    rsi
    sub     rsp, 28h

    mov     ebx, ecx            ; model hash
    mov     esi, edx            ; operation

    cmp     esi, 0
    je      @kv_query
    cmp     esi, 1
    je      @kv_warming
    cmp     esi, 2
    je      @kv_hot
    cmp     esi, 3
    je      @kv_evict
    jmp     @kv_query

@kv_warming:
    ; Transition to WARMING — record cold-start TPS
    mov     DWORD PTR [g_kv_state], KV_STATE_WARMING
    mov     DWORD PTR [g_kv_model_hash], ebx
    mov     DWORD PTR [g_kv_cold_pp_tps], r8d
    lock inc DWORD PTR [g_kv_warmup_count]
    call    GetTickCount64
    mov     QWORD PTR [g_kv_warm_tick], rax
    jmp     @kv_query

@kv_hot:
    ; Transition to HOT — record warmed-up TPS
    mov     DWORD PTR [g_kv_state], KV_STATE_HOT
    mov     DWORD PTR [g_kv_hot_pp_tps], r8d
    call    GetTickCount64
    mov     QWORD PTR [g_kv_warm_tick], rax
    jmp     @kv_query

@kv_evict:
    ; Evict cache
    mov     DWORD PTR [g_kv_state], KV_STATE_EVICTED
    mov     DWORD PTR [g_kv_model_hash], 0
    mov     DWORD PTR [g_kv_hot_pp_tps], 0
    jmp     @kv_query

@kv_query:
    ; Check TTL expiry
    mov     eax, [g_kv_state]
    cmp     eax, KV_STATE_HOT
    jne     @kv_no_ttl_check
    call    GetTickCount64
    sub     rax, QWORD PTR [g_kv_warm_tick]
    mov     ecx, [g_kv_ttl_ms]
    cmp     eax, ecx
    jl      @kv_no_ttl_check
    ; TTL expired — auto-evict
    mov     DWORD PTR [g_kv_state], KV_STATE_EVICTED

@kv_no_ttl_check:
    ; Return state + gain percentage
    mov     eax, [g_kv_state]
    xor     edx, edx
    mov     ecx, [g_kv_cold_pp_tps]
    test    ecx, ecx
    jz      @kv_done
    mov     edx, [g_kv_hot_pp_tps]
    sub     edx, ecx            ; hot - cold
    imul    edx, 100
    cdq
    idiv    ecx                 ; (hot - cold) * 100 / cold
    mov     edx, eax
    mov     eax, [g_kv_state]

@kv_done:
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
SwarmV253_KV_Cache_Prewarmer ENDP


; ============================================================================
; Enhancement 254: SwarmV254_Batch_Prompt_Coalescer
; ============================================================================
; Manages a batch queue of prompts for coalesced processing.
; Input:  RCX = operation (0=add, 1=process_next, 2=get_stats, 3=clear)
;         RDX = prompt_ptr (for add)
;         R8D = prompt_len (for add)
;         R9D = priority (for add)
; Output: EAX = batch_count (add) / entry index (process) / completed (stats)
;         EDX = total queued / avg TPS (stats)
; ============================================================================
SwarmV254_Batch_Prompt_Coalescer PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 20h

    mov     ebx, ecx            ; operation

    cmp     ebx, 0
    je      @bc_add
    cmp     ebx, 1
    je      @bc_process
    cmp     ebx, 2
    je      @bc_stats
    cmp     ebx, 3
    je      @bc_clear
    xor     eax, eax
    jmp     @bc_done

@bc_add:
    mov     eax, [g_batch_count]
    cmp     eax, MAX_BATCH_ENTRIES
    jge     @bc_full
    ; Calculate entry offset
    imul    esi, eax, BATCH_ENTRY_SIZE
    lea     rdi, [g_batch_queue]
    add     rdi, rsi
    ; Store entry
    mov     QWORD PTR [rdi], rdx        ; prompt_ptr
    mov     DWORD PTR [rdi + 8], r8d    ; prompt_len
    mov     DWORD PTR [rdi + 12], r9d   ; priority
    mov     DWORD PTR [rdi + 20], 0     ; status = pending
    lock inc DWORD PTR [g_batch_count]
    mov     eax, [g_batch_count]
    mov     edx, eax
    jmp     @bc_done
@bc_full:
    mov     eax, -1
    jmp     @bc_done

@bc_process:
    ; Find next pending entry (highest priority)
    lea     rdi, [g_batch_queue]
    mov     ecx, [g_batch_count]
    test    ecx, ecx
    jz      @bc_empty
    xor     esi, esi            ; best index
    mov     eax, -1             ; best priority (lower = higher)
    xor     r10d, r10d          ; current index
@bc_scan:
    cmp     r10d, ecx
    jge     @bc_found
    imul    ebx, r10d, BATCH_ENTRY_SIZE
    cmp     DWORD PTR [rdi + rbx + 20], 0  ; status == pending?
    jne     @bc_skip
    mov     r11d, DWORD PTR [rdi + rbx + 12] ; priority
    cmp     r11d, eax
    jge     @bc_skip
    mov     eax, r11d
    mov     esi, r10d
@bc_skip:
    inc     r10d
    jmp     @bc_scan
@bc_found:
    ; Mark as running
    imul    ebx, esi, BATCH_ENTRY_SIZE
    mov     DWORD PTR [rdi + rbx + 20], 1  ; status = running
    mov     eax, esi
    mov     edx, [g_batch_count]
    jmp     @bc_done
@bc_empty:
    mov     eax, -1
    jmp     @bc_done

@bc_stats:
    mov     eax, [g_batch_completed]
    mov     edx, [g_batch_count]
    jmp     @bc_done

@bc_clear:
    mov     DWORD PTR [g_batch_count], 0
    mov     DWORD PTR [g_batch_completed], 0
    mov     DWORD PTR [g_batch_total_tps], 0
    xor     eax, eax
    jmp     @bc_done

@bc_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV254_Batch_Prompt_Coalescer ENDP


; ============================================================================
; Enhancement 255: SwarmV255_VRAM_Pressure_Monitor
; ============================================================================
; Monitors VRAM pressure and returns allocation recommendations.
; Input:  ECX = operation: 0=update, 1=query, 2=record_model_load, 3=reset
;         EDX = model_size_mb (for record_model_load)
;         R8D = kv_cache_mb (for record_model_load)
; Output: EAX = pressure level (OK/WARNING/CRITICAL/OOM)
;         EDX = free VRAM MB
;         R8D = recommended max layers to offload
; ============================================================================
SwarmV255_VRAM_Pressure_Monitor PROC
    push    rbx
    sub     rsp, 20h

    cmp     ecx, 0
    je      @vp_update
    cmp     ecx, 1
    je      @vp_query
    cmp     ecx, 2
    je      @vp_record
    cmp     ecx, 3
    je      @vp_reset
    jmp     @vp_query

@vp_record:
    ; Record model + KV load
    mov     DWORD PTR [g_vram_model_mb], edx
    mov     DWORD PTR [g_vram_kv_mb], r8d
    add     edx, r8d
    mov     DWORD PTR [g_vram_used_mb], edx
    mov     eax, VRAM_TOTAL_MB
    sub     eax, edx
    mov     DWORD PTR [g_vram_free_mb], eax
    ; Track peak
    cmp     edx, DWORD PTR [g_vram_peak_mb]
    jle     @vp_no_peak
    mov     DWORD PTR [g_vram_peak_mb], edx
@vp_no_peak:
    jmp     @vp_update

@vp_update:
    ; Calculate pressure level
    mov     eax, [g_vram_used_mb]
    imul    eax, 100
    xor     edx, edx
    mov     ecx, VRAM_TOTAL_MB
    div     ecx                 ; usage_pct = used * 100 / total

    cmp     eax, VRAM_CRITICAL_PCT
    jge     @vp_crit
    cmp     eax, VRAM_WARN_PCT
    jge     @vp_warn
    mov     DWORD PTR [g_vram_pressure], VRAM_OK
    jmp     @vp_query
@vp_warn:
    mov     DWORD PTR [g_vram_pressure], VRAM_WARNING
    jmp     @vp_query
@vp_crit:
    cmp     eax, 100
    jge     @vp_oom
    mov     DWORD PTR [g_vram_pressure], VRAM_CRITICAL
    jmp     @vp_query
@vp_oom:
    mov     DWORD PTR [g_vram_pressure], VRAM_OOM
    jmp     @vp_query

@vp_reset:
    mov     DWORD PTR [g_vram_used_mb], 0
    mov     DWORD PTR [g_vram_free_mb], VRAM_TOTAL_MB
    mov     DWORD PTR [g_vram_pressure], VRAM_OK
    mov     DWORD PTR [g_vram_model_mb], 0
    mov     DWORD PTR [g_vram_kv_mb], 0

@vp_query:
    mov     eax, [g_vram_pressure]
    mov     edx, [g_vram_free_mb]
    ; Recommend GPU layers: free_mb / 256 (approx layer size)
    mov     r8d, edx
    shr     r8d, 8              ; free_mb / 256

    ; Stamp sample tick
    push    rax
    push    rdx
    call    GetTickCount64
    mov     QWORD PTR [g_vram_sample_tick], rax
    pop     rdx
    pop     rax

    add     rsp, 20h
    pop     rbx
    ret
SwarmV255_VRAM_Pressure_Monitor ENDP


; ============================================================================
; Enhancement 256: SwarmV256_Token_Pipeline_Prefetch
; ============================================================================
; Manages a prefetch pipeline for token decode.
; Input:  ECX = operation: 0=start, 1=prefetch_hit, 2=prefetch_miss, 3=stats
;         EDX = tokens_this_batch (for hit/miss)
; Output: EAX = pipe state
;         EDX = hit ratio (0-100)
;         R8D = estimated ms saved
; ============================================================================
SwarmV256_Token_Pipeline_Prefetch PROC
    push    rbx
    sub     rsp, 20h

    cmp     ecx, 0
    je      @tp_start
    cmp     ecx, 1
    je      @tp_hit
    cmp     ecx, 2
    je      @tp_miss
    cmp     ecx, 3
    je      @tp_stats
    jmp     @tp_stats

@tp_start:
    mov     DWORD PTR [g_pipe_state], PIPE_PREFETCHING
    mov     DWORD PTR [g_pipe_hits], 0
    mov     DWORD PTR [g_pipe_misses], 0
    mov     DWORD PTR [g_pipe_total_tokens], 0
    mov     DWORD PTR [g_pipe_saved_ms], 0
    jmp     @tp_stats

@tp_hit:
    mov     DWORD PTR [g_pipe_state], PIPE_READY
    lock add DWORD PTR [g_pipe_hits], edx
    lock add DWORD PTR [g_pipe_total_tokens], edx
    ; Estimate: each prefetch hit saves ~0.5ms per token
    mov     eax, edx
    shr     eax, 1              ; tokens / 2 ≈ ms saved
    lock add DWORD PTR [g_pipe_saved_ms], eax
    jmp     @tp_stats

@tp_miss:
    mov     DWORD PTR [g_pipe_state], PIPE_STALLED
    lock add DWORD PTR [g_pipe_misses], edx
    lock add DWORD PTR [g_pipe_total_tokens], edx
    jmp     @tp_stats

@tp_stats:
    mov     eax, [g_pipe_state]
    ; Hit ratio
    mov     ecx, [g_pipe_hits]
    add     ecx, [g_pipe_misses]
    test    ecx, ecx
    jz      @tp_zero
    mov     edx, [g_pipe_hits]
    imul    edx, 100
    push    rax
    mov     eax, edx
    xor     edx, edx
    div     ecx
    mov     edx, eax
    pop     rax
    jmp     @tp_out
@tp_zero:
    xor     edx, edx
@tp_out:
    mov     r8d, [g_pipe_saved_ms]

    add     rsp, 20h
    pop     rbx
    ret
SwarmV256_Token_Pipeline_Prefetch ENDP


; ============================================================================
; Enhancement 257: SwarmV257_Adaptive_Context_Window
; ============================================================================
; Dynamically sizes the context window based on model + VRAM.
; Input:  ECX = model_size_mb
;         EDX = model_max_context (from GGUF metadata)
;         R8D = available_vram_mb
; Output: EAX = recommended context window size
;         EDX = VRAM headroom after context allocation (MB)
; ============================================================================
SwarmV257_Adaptive_Context_Window PROC
    push    rbx
    push    rsi
    sub     rsp, 20h

    mov     ebx, ecx            ; model_size_mb
    mov     esi, edx            ; model_max_context
    mov     ecx, r8d            ; available VRAM

    ; Store model max
    mov     DWORD PTR [g_ctx_model_max], esi

    ; Calculate KV cache cost: ~2 bytes per token per layer
    ; Rough: ctx * model_layers * 2 / 1024 / 1024
    ; Simplified: ctx_vram_mb ≈ ctx * model_size_mb / 8192
    ; We want: model_size + kv_cache < available_vram * 0.9

    ; Available for KV = (available * 90%) - model_size
    mov     eax, ecx
    imul    eax, 90
    xor     edx, edx
    push    rcx
    mov     ecx, 100
    div     ecx                 ; avail * 90 / 100
    pop     rcx
    sub     eax, ebx            ; available_for_kv = avail*0.9 - model
    test    eax, eax
    jle     @ac_min             ; no room? use minimum

    ; max_ctx = available_for_kv * 8192 / model_size
    imul    eax, 8192
    xor     edx, edx
    mov     ecx, ebx
    test    ecx, ecx
    jz      @ac_min
    div     ecx

    ; Clamp to model max
    cmp     eax, esi
    jle     @ac_clamp_floor
    mov     eax, esi
@ac_clamp_floor:
    cmp     eax, CTX_MIN
    jge     @ac_store
@ac_min:
    mov     eax, CTX_MIN

@ac_store:
    mov     DWORD PTR [g_ctx_current], eax
    mov     DWORD PTR [g_ctx_optimal], eax

    ; Calculate VRAM headroom
    ; kv_cost ≈ ctx * model_size / 8192
    mov     ecx, eax
    imul    ecx, ebx
    shr     ecx, 13             ; / 8192
    mov     DWORD PTR [g_ctx_vram_limit], ecx
    mov     edx, VRAM_TOTAL_MB
    sub     edx, ebx
    sub     edx, ecx            ; headroom = total - model - kv

    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
SwarmV257_Adaptive_Context_Window ENDP


; ============================================================================
; Enhancement 258: SwarmV258_Inference_Latency_Profiler
; ============================================================================
; Instruments per-phase latency for inference runs.
; Input:  ECX = operation: 0=start_phase, 1=end_phase, 2=get_phase, 3=summary
;         EDX = phase_id (PHASE_LOAD/PROMPT/GENERATE/FINALIZE/TOTAL)
;         R8D = tokens_this_phase (for end_phase)
; Output: EAX = duration_ms (for get_phase/end_phase)
;         EDX = total_duration_ms (for summary)
;         R8D = run_count
; ============================================================================
SwarmV258_Inference_Latency_Profiler PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 28h

    mov     ebx, ecx            ; operation
    mov     esi, edx            ; phase_id

    ; Validate phase
    cmp     esi, MAX_PHASES
    jge     @lp_bad
    
    ; Calculate entry offset (RIP-relative safe)
    imul    r10d, esi, PROFILE_ENTRY_SIZE
    lea     rdi, [g_profile_data]
    add     rdi, r10

    cmp     ebx, 0
    je      @lp_start
    cmp     ebx, 1
    je      @lp_end
    cmp     ebx, 2
    je      @lp_get
    cmp     ebx, 3
    je      @lp_summary
    jmp     @lp_bad

@lp_start:
    mov     DWORD PTR [g_profile_active], 1
    call    GetTickCount64
    mov     QWORD PTR [rdi], rax        ; start_tick
    xor     eax, eax
    jmp     @lp_done

@lp_end:
    call    GetTickCount64
    mov     QWORD PTR [rdi + 8], rax    ; end_tick
    sub     rax, QWORD PTR [rdi]        ; duration = end - start
    mov     DWORD PTR [rdi + 16], eax   ; duration_ms
    mov     DWORD PTR [rdi + 20], r8d   ; tokens
    ; Return duration
    jmp     @lp_done

@lp_get:
    mov     eax, DWORD PTR [rdi + 16]   ; duration_ms
    mov     edx, DWORD PTR [rdi + 20]   ; tokens
    jmp     @lp_done

@lp_summary:
    ; Sum all phase durations
    xor     eax, eax
    lea     rdi, [g_profile_data]
    xor     ecx, ecx
@lp_sum:
    cmp     ecx, MAX_PHASES
    jge     @lp_sum_done
    add     eax, DWORD PTR [rdi + 16]
    add     rdi, PROFILE_ENTRY_SIZE
    inc     ecx
    jmp     @lp_sum
@lp_sum_done:
    mov     edx, eax            ; total_ms
    ; Update best/worst
    cmp     eax, [g_profile_best_ms]
    jge     @lp_nb
    mov     [g_profile_best_ms], eax
@lp_nb:
    cmp     eax, [g_profile_worst_ms]
    jle     @lp_nw
    mov     [g_profile_worst_ms], eax
@lp_nw:
    lock inc DWORD PTR [g_profile_run_count]
    mov     r8d, [g_profile_run_count]
    ; Update running average
    mov     ecx, [g_profile_avg_ms]
    imul    ecx, r8d
    sub     ecx, [g_profile_avg_ms]
    add     ecx, eax
    xor     edx, edx
    push    rax
    mov     eax, ecx
    div     r8d
    mov     [g_profile_avg_ms], eax
    pop     rax
    mov     edx, eax
    jmp     @lp_done

@lp_bad:
    xor     eax, eax
    xor     edx, edx

@lp_done:
    add     rsp, 28h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV258_Inference_Latency_Profiler ENDP


; ============================================================================
; Enhancement 259: SwarmV259_Sovereign_Smoke_Test
; ============================================================================
; Automated multi-model smoke test kernel.
; Input:  ECX = operation: 0=init, 1=record_result, 2=get_result, 3=summarize
;         RDX = model_name_ptr (for record)
;         R8D = model_size_mb (for record)
;         R9D = measured_tps (for record)
; Output: EAX = smoke state / pass count / result
;         EDX = total tested / fail count
; For record: additional stack params at [RSP+28h]:
;   +28h: backend (DWORD)
;   +30h: expected_tps (DWORD)
;   +38h: wall_ms (DWORD)
; ============================================================================
SwarmV259_Sovereign_Smoke_Test PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 28h

    mov     ebx, ecx

    cmp     ebx, 0
    je      @st_init
    cmp     ebx, 1
    je      @st_record
    cmp     ebx, 2
    je      @st_get
    cmp     ebx, 3
    je      @st_summary
    jmp     @st_bad

@st_init:
    mov     DWORD PTR [g_smoke_state], SMOKE_RUNNING
    mov     DWORD PTR [g_smoke_count], 0
    mov     DWORD PTR [g_smoke_passed], 0
    mov     DWORD PTR [g_smoke_failed], 0
    call    GetTickCount64
    mov     QWORD PTR [g_smoke_start_tick], rax
    mov     eax, SMOKE_RUNNING
    jmp     @st_done

@st_record:
    mov     eax, [g_smoke_count]
    cmp     eax, MAX_SMOKE_MODELS
    jge     @st_full
    ; Calculate offset
    imul    esi, eax, SMOKE_ENTRY_SIZE
    lea     rdi, [g_smoke_results]
    add     rdi, rsi
    ; Populate entry
    mov     QWORD PTR [rdi], rdx        ; model_name_ptr
    mov     DWORD PTR [rdi + 8], r8d    ; model_size_mb
    mov     DWORD PTR [rdi + 20], r9d   ; tg_tps (measured)
    ; Read stack params
    mov     eax, DWORD PTR [rsp + 28h + 28h]  ; backend
    mov     DWORD PTR [rdi + 12], eax
    mov     r12d, DWORD PTR [rsp + 28h + 30h] ; expected_tps
    mov     DWORD PTR [rdi + 32], r12d
    mov     eax, DWORD PTR [rsp + 28h + 38h]  ; wall_ms
    mov     DWORD PTR [rdi + 24], eax
    ; Calculate deviation %: (measured - expected) * 100 / expected
    mov     eax, r9d
    sub     eax, r12d
    imul    eax, 100
    test    r12d, r12d
    jz      @st_no_dev
    cdq
    idiv    r12d
@st_no_dev:
    mov     DWORD PTR [rdi + 36], eax   ; deviation_pct
    ; Verdict: pass if measured >= expected * 65%
    mov     eax, r12d
    imul    eax, 65
    xor     edx, edx
    push    rcx
    mov     ecx, 100
    div     ecx
    pop     rcx
    cmp     r9d, eax
    jge     @st_pass
    mov     DWORD PTR [rdi + 28], SMOKE_FAIL
    lock inc DWORD PTR [g_smoke_failed]
    jmp     @st_rec_done
@st_pass:
    mov     DWORD PTR [rdi + 28], SMOKE_PASS
    lock inc DWORD PTR [g_smoke_passed]
@st_rec_done:
    ; Timestamp
    call    GetTickCount64
    mov     QWORD PTR [rdi + 48], rax
    lock inc DWORD PTR [g_smoke_count]
    mov     eax, [g_smoke_count]
    mov     edx, [g_smoke_passed]
    jmp     @st_done

@st_full:
    mov     eax, -1
    jmp     @st_done

@st_get:
    ; Get result for index EDX
    cmp     edx, [g_smoke_count]
    jge     @st_bad
    imul    esi, edx, SMOKE_ENTRY_SIZE
    lea     rdi, [g_smoke_results]
    add     rdi, rsi
    mov     eax, DWORD PTR [rdi + 28]   ; verdict
    mov     edx, DWORD PTR [rdi + 36]   ; deviation_pct
    jmp     @st_done

@st_summary:
    ; Calculate total elapsed
    call    GetTickCount64
    sub     rax, QWORD PTR [g_smoke_start_tick]
    mov     DWORD PTR [g_smoke_total_ms], eax
    ; Set final state
    mov     ecx, [g_smoke_failed]
    test    ecx, ecx
    jnz     @st_has_fails
    mov     DWORD PTR [g_smoke_state], SMOKE_PASS
    jmp     @st_sum_ret
@st_has_fails:
    mov     DWORD PTR [g_smoke_state], SMOKE_FAIL
@st_sum_ret:
    mov     eax, [g_smoke_passed]
    mov     edx, [g_smoke_failed]
    mov     r8d, [g_smoke_total_ms]
    jmp     @st_done

@st_bad:
    xor     eax, eax
    xor     edx, edx

@st_done:
    add     rsp, 28h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV259_Sovereign_Smoke_Test ENDP

END
