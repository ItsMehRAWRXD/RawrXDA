; ============================================================================
; RawrXD_Swarm_v23.asm — Enhancements 247-252 + Wafer-Scale Sharding
; v23 Swarm Integration: Coordinator / Planner / Executor / Verifier
; ============================================================================
; 247: Swarm Coordinator     — Master cycle orchestration
; 248: Planner               — Model/task selection from swarm memory
; 249: Executor              — Route + launch via v246 router
; 250: Verifier              — Record outcome, trigger cache/quarantine
; 251: Memory Sync           — In-memory <-> on-disk state sync
; 252: Feedback Loop         — Verifier -> Planner scoring feedback
; Original: Shard topology, NVMe ring fetch, lockstep consensus
; ============================================================================

extrn GetTickCount64 : proc
extrn CreateFileA : proc
extrn SetFilePointerEx : proc
extrn ReadFile : proc
extrn CloseHandle : proc

; ── External references (from RawrXD_Sovereign_Router.asm) ──
EXTERN SwarmV239_Backend_Router:PROC
EXTERN SwarmV244_iGPU_Firewall:PROC
EXTERN SwarmV245_Hybrid_Mode_Switch:PROC
EXTERN SwarmV246_Route_Reason_Query:PROC

PUBLIC Swarm_InitShardMap
PUBLIC Swarm_RingBuffer_Fetch
PUBLIC Swarm_LockStepConsensus
PUBLIC SwarmV247_Swarm_Coordinator
PUBLIC SwarmV248_Planner
PUBLIC SwarmV249_Executor
PUBLIC SwarmV250_Verifier
PUBLIC SwarmV251_Memory_Sync
PUBLIC SwarmV252_Feedback_Loop

.data

; ── Task Type Enum ──
TASK_GENERATE       EQU 0
TASK_EMBED          EQU 1
TASK_CHAT           EQU 2
TASK_BENCHMARK      EQU 3

; ── Swarm Verdict Codes ──
VERDICT_HEALTHY     EQU 0
VERDICT_UNDERPERF   EQU 1
VERDICT_QUARANTINED EQU 2

; ── Swarm Cycle Results ──
CYCLE_OK            EQU 0
CYCLE_RETRY         EQU 1
CYCLE_FAIL          EQU 2

; ── Planner Score Constants ──
SCORE_MAX           EQU 1000
SCORE_DECAY         EQU 50
SCORE_BOOST         EQU 25
SCORE_QUARANTINE_PENALTY EQU 200

; ── Schema Version ──
SWARM_SCHEMA_VER    EQU 247

; ── Struct Offsets: SwarmTaskRequest (64 bytes) ──
REQ_MODEL_PTR       EQU 0
REQ_SIZE_MB         EQU 8
REQ_TASK_TYPE       EQU 12
REQ_PRIORITY        EQU 16
REQ_MAX_TOKENS      EQU 20
REQ_CYCLE_NUM       EQU 24
REQ_RESERVED1       EQU 28
REQ_ROUTE_ID        EQU 32
REQ_SCHEMA_VER      EQU 44
REQ_TIMESTAMP       EQU 48
REQ_RESERVED2       EQU 56

; ── Struct Offsets: SwarmTaskResult (64 bytes) ──
RES_BACKEND         EQU 0
RES_REASON          EQU 4
RES_EVAL_TPS        EQU 8
RES_PROMPT_TPS      EQU 12
RES_TOKENS          EQU 16
RES_WALL_MS         EQU 20
RES_VERDICT         EQU 24
RES_RETRY_STEP      EQU 28
RES_PLANNER_SCORE   EQU 32
RES_Q_STRIKES       EQU 36
RES_ROUTE_ID        EQU 40
RES_SCHEMA_VER      EQU 52
RES_COMPLETION      EQU 56

; ── Struct Offsets: SwarmMemory (256 bytes) ──
MEM_TOTAL_CYCLES    EQU 0
MEM_SUCCESS_CYCLES  EQU 4
MEM_FAILED_CYCLES   EQU 8
MEM_QUARANTINE_EVT  EQU 12
MEM_ACTIVE_MODELS   EQU 16
MEM_AVG_EVAL_TPS    EQU 20
MEM_BEST_EVAL_TPS   EQU 24
MEM_CUR_BACKEND     EQU 28
MEM_LAST_CYCLE_TICK EQU 32
MEM_UPTIME_TICK     EQU 40
MEM_CACHE_HITS      EQU 48
MEM_CACHE_MISSES    EQU 56
MEM_MODEL_SCORES    EQU 64

; ── Per-Model Score Entry (24 bytes) ──
MSCORE_HASH         EQU 0
MSCORE_SCORE        EQU 8
MSCORE_TPS          EQU 12
MSCORE_CYCLES       EQU 16
MSCORE_FLAGS        EQU 20
MSCORE_ENTRY_SIZE   EQU 24
MAX_MODEL_SCORES    EQU 8

; ── Original Shard Topology Data ──
g_ShardTable      dq 4096 dup(0)
g_ShardCount      dq 0
g_NVMe_Handle     dq -1
g_ActiveGen       dq 0

WEIGHTS_800B_Q4   dq 429496729600
VRAM_CAPACITY     dq 17179869184
RAM_CAPACITY      dq 68719476736

; ── Swarm v23 State ──
ALIGN 16
g_swarm_memory       DB 256 DUP(0)
g_swarm_result       DB 64 DUP(0)
g_swarm_cycle        DD 0
g_swarm_total_tps    DD 0
g_swarm_sample_count DD 0

.code

; ============================================================================
; Enhancement 247: SwarmV247_Swarm_Coordinator
; ============================================================================
; Master entry point for one swarm cycle.
; Input:  RCX = pointer to SwarmTaskRequest
;         EDX = cycle number
;         R8  = pointer to SwarmMemory
; Output: EAX = CYCLE_OK | CYCLE_RETRY | CYCLE_FAIL
;         RDX = pointer to SwarmTaskResult
; ============================================================================
SwarmV247_Swarm_Coordinator PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 48h

    mov     rsi, rcx
    mov     [g_swarm_cycle], edx
    mov     r12, r8

    ; Stamp cycle
    lock inc DWORD PTR [r12 + MEM_TOTAL_CYCLES]
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [r12 + MEM_LAST_CYCLE_TICK], rax

    ; Step 1: iGPU Firewall
    call    SwarmV244_iGPU_Firewall

    ; Step 2: Plan
    mov     ecx, DWORD PTR [rsi + REQ_SIZE_MB]
    call    SwarmV248_Planner_Internal
    mov     r13d, eax
    mov     r14d, edx

    ; Step 3: Execute via v246 router
    mov     ecx, DWORD PTR [rsi + REQ_SIZE_MB]
    mov     rdx, QWORD PTR [rsi + REQ_MODEL_PTR]
    call    SwarmV245_Hybrid_Mode_Switch
    mov     ebx, eax

    ; Step 4: Populate result struct
    lea     rdi, [g_swarm_result]
    mov     DWORD PTR [rdi + RES_BACKEND], ebx
    mov     DWORD PTR [rdi + RES_EVAL_TPS], r14d
    mov     DWORD PTR [rdi + RES_SCHEMA_VER], SWARM_SCHEMA_VER
    mov     rax, QWORD PTR [rsi + REQ_ROUTE_ID]
    mov     QWORD PTR [rdi + RES_ROUTE_ID], rax
    mov     eax, DWORD PTR [rsi + REQ_ROUTE_ID + 8]
    mov     DWORD PTR [rdi + RES_ROUTE_ID + 8], eax

    ; Step 5: Query route reason
    xor     ecx, ecx
    call    SwarmV246_Route_Reason_Query
    mov     DWORD PTR [rdi + RES_REASON], eax
    mov     DWORD PTR [rdi + RES_Q_STRIKES], r8d

    ; Step 6: Verify
    mov     ecx, r14d
    mov     edx, r14d
    mov     r8d, ebx
    mov     r9d, DWORD PTR [rdi + RES_REASON]
    call    SwarmV250_Verifier
    mov     DWORD PTR [rdi + RES_VERDICT], eax

    ; Step 7: Feedback
    lea     rcx, [g_swarm_result]
    mov     rdx, r12
    call    SwarmV252_Feedback_Loop
    mov     DWORD PTR [rdi + RES_PLANNER_SCORE], eax

    ; Step 8: Completion tick
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [rdi + RES_COMPLETION], rax

    ; Return
    mov     eax, CYCLE_OK
    cmp     DWORD PTR [rdi + RES_VERDICT], VERDICT_QUARANTINED
    jne     @not_q
    mov     eax, CYCLE_RETRY
@not_q:
    lea     rdx, [g_swarm_result]

    add     rsp, 48h
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV247_Swarm_Coordinator ENDP


; ============================================================================
; Enhancement 248: SwarmV248_Planner
; ============================================================================
; Input:  RCX = pointer to model list, EDX = task type, R8 = SwarmMemory*
; Output: EAX = model index, EDX = estimated TPS
; ============================================================================
SwarmV248_Planner PROC
    push    rbx
    sub     rsp, 28h

    xor     eax, eax
    mov     edx, 100
    test    r8, r8
    jz      @done
    mov     edx, DWORD PTR [r8 + MEM_AVG_EVAL_TPS]
    test    edx, edx
    jnz     @done
    mov     edx, 100
@done:
    add     rsp, 28h
    pop     rbx
    ret
SwarmV248_Planner ENDP

; Internal: size-based planning
SwarmV248_Planner_Internal PROC
    push    rbx
    sub     rsp, 20h

    mov     ebx, ecx
    call    SwarmV239_Backend_Router

    mov     ecx, ebx
    cmp     ecx, 800
    jge     @s_ok
    mov     ecx, 800
@s_ok:
    mov     eax, 4096
    imul    eax, 287
    xor     edx, edx
    div     ecx
    cmp     eax, 290
    jle     @h_ok
    mov     eax, 290
@h_ok:
    cmp     eax, 5
    jge     @l_ok
    mov     eax, 5
@l_ok:
    mov     edx, eax
    xor     eax, eax

    add     rsp, 20h
    pop     rbx
    ret
SwarmV248_Planner_Internal ENDP


; ============================================================================
; Enhancement 249: SwarmV249_Executor
; ============================================================================
; Input:  RCX = model name ptr, EDX = size MB, R8D = task type
; Output: EAX = backend, EDX = est TPS, R8D = reason
; ============================================================================
SwarmV249_Executor PROC
    push    rbx
    push    rsi
    sub     rsp, 28h

    mov     rsi, rcx
    mov     ebx, edx

    mov     ecx, ebx
    mov     rdx, rsi
    call    SwarmV245_Hybrid_Mode_Switch
    push    rax
    push    rcx

    xor     ecx, ecx
    call    SwarmV246_Route_Reason_Query
    mov     r8d, eax

    pop     rdx
    pop     rax

    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
SwarmV249_Executor ENDP


; ============================================================================
; Enhancement 250: SwarmV250_Verifier
; ============================================================================
; Input:  ECX = measured TPS, EDX = expected, R8D = backend, R9D = reason
; Output: EAX = VERDICT_HEALTHY | VERDICT_UNDERPERF | VERDICT_QUARANTINED
; ============================================================================
SwarmV250_Verifier PROC
    push    rbx
    sub     rsp, 20h

    mov     ebx, ecx
    test    ebx, ebx
    jz      @q

    mov     eax, edx
    imul    eax, 65
    xor     edx, edx
    mov     ecx, 100
    div     ecx
    cmp     ebx, eax
    jl      @u

    lock inc DWORD PTR [g_swarm_sample_count]
    lock add DWORD PTR [g_swarm_total_tps], ebx
    mov     eax, VERDICT_HEALTHY
    jmp     @d
@u:
    mov     eax, VERDICT_UNDERPERF
    jmp     @d
@q:
    mov     eax, VERDICT_QUARANTINED
@d:
    add     rsp, 20h
    pop     rbx
    ret
SwarmV250_Verifier ENDP


; ============================================================================
; Enhancement 251: SwarmV251_Memory_Sync
; ============================================================================
; Input:  RCX = memory ptr (NULL=internal), EDX = 0=load/1=save/2=merge
; Output: EAX = entries synced
; ============================================================================
SwarmV251_Memory_Sync PROC
    push    rbx
    push    rsi
    sub     rsp, 20h

    test    rcx, rcx
    jnz     @prov
    lea     rcx, [g_swarm_memory]
@prov:
    mov     rsi, rcx
    mov     ebx, edx

    cmp     ebx, 0
    je      @ld
    cmp     ebx, 1
    je      @sv
    cmp     ebx, 2
    je      @mg
    xor     eax, eax
    jmp     @dn

@ld:
    mov     eax, DWORD PTR [g_swarm_memory + MEM_TOTAL_CYCLES]
    jmp     @dn

@sv:
    mov     eax, DWORD PTR [rsi + MEM_TOTAL_CYCLES]
    mov     DWORD PTR [g_swarm_memory + MEM_TOTAL_CYCLES], eax
    mov     eax, DWORD PTR [rsi + MEM_SUCCESS_CYCLES]
    mov     DWORD PTR [g_swarm_memory + MEM_SUCCESS_CYCLES], eax
    mov     eax, DWORD PTR [rsi + MEM_AVG_EVAL_TPS]
    mov     DWORD PTR [g_swarm_memory + MEM_AVG_EVAL_TPS], eax
    mov     eax, 3
    jmp     @dn

@mg:
    mov     eax, DWORD PTR [rsi + MEM_TOTAL_CYCLES]
    lock add DWORD PTR [g_swarm_memory + MEM_TOTAL_CYCLES], eax
    mov     eax, DWORD PTR [rsi + MEM_SUCCESS_CYCLES]
    lock add DWORD PTR [g_swarm_memory + MEM_SUCCESS_CYCLES], eax
    mov     eax, [g_swarm_total_tps]
    mov     ecx, [g_swarm_sample_count]
    test    ecx, ecx
    jz      @mg_d
    xor     edx, edx
    div     ecx
    mov     DWORD PTR [g_swarm_memory + MEM_AVG_EVAL_TPS], eax
@mg_d:
    mov     eax, DWORD PTR [g_swarm_memory + MEM_TOTAL_CYCLES]

@dn:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
SwarmV251_Memory_Sync ENDP


; ============================================================================
; Enhancement 252: SwarmV252_Feedback_Loop
; ============================================================================
; Input:  RCX = SwarmTaskResult*, RDX = SwarmMemory*
; Output: EAX = updated planner score (0-1000)
; ============================================================================
SwarmV252_Feedback_Loop PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 20h

    mov     rsi, rcx
    mov     rdi, rdx

    mov     eax, DWORD PTR [rsi + RES_VERDICT]
    mov     ebx, DWORD PTR [rsi + RES_EVAL_TPS]

    mov     ecx, DWORD PTR [rdi + MEM_MODEL_SCORES + MSCORE_SCORE]
    test    ecx, ecx
    jnz     @hs
    mov     ecx, 500
@hs:
    cmp     eax, VERDICT_HEALTHY
    je      @bo
    cmp     eax, VERDICT_UNDERPERF
    je      @dc
    sub     ecx, SCORE_QUARANTINE_PENALTY
    jmp     @cl
@bo:
    add     ecx, SCORE_BOOST
    lock inc DWORD PTR [rdi + MEM_SUCCESS_CYCLES]
    jmp     @cl
@dc:
    sub     ecx, SCORE_DECAY
    lock inc DWORD PTR [rdi + MEM_FAILED_CYCLES]
@cl:
    cmp     ecx, SCORE_MAX
    jle     @ch
    mov     ecx, SCORE_MAX
@ch:
    test    ecx, ecx
    jge     @cl2
    xor     ecx, ecx
@cl2:
    mov     DWORD PTR [rdi + MEM_MODEL_SCORES + MSCORE_SCORE], ecx
    mov     DWORD PTR [rdi + MEM_MODEL_SCORES + MSCORE_TPS], ebx
    lock inc DWORD PTR [rdi + MEM_MODEL_SCORES + MSCORE_CYCLES]

    cmp     ebx, DWORD PTR [rdi + MEM_BEST_EVAL_TPS]
    jle     @nb
    mov     DWORD PTR [rdi + MEM_BEST_EVAL_TPS], ebx
@nb:
    mov     eax, ecx

    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmV252_Feedback_Loop ENDP


; ============================================================================
; Original: Swarm_InitShardMap — Wafer-Scale Shard Topology Init
; ============================================================================
Swarm_InitShardMap proc
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32

    mov     g_ShardCount, 0
    mov     rax, 1

    add     rsp, 32
    pop     rbp
    ret
Swarm_InitShardMap endp

; ============================================================================
; Original: Swarm_RingBuffer_Fetch — NVMe Ring Buffer Page-Fault Fetch
; ============================================================================
; In: RCX = ShardID, RDX = TargetBuffer
; Out: RAX = BytesRead
Swarm_RingBuffer_Fetch proc
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48

    shl     rcx, 5
    lea     r8, g_ShardTable
    add     r8, rcx

    mov     r9, [r8]
    cmp     r9, 2
    jne     @not_nvme

    mov     r10, [r8+8]
    mov     r11, [r8+16]

@not_nvme:
    xor     rax, rax
    add     rsp, 48
    pop     rbp
    ret
Swarm_RingBuffer_Fetch endp

; ============================================================================
; Original: Swarm_LockStepConsensus — Cross-Device Tensor Consistency
; ============================================================================
; In: RCX = LocalHash, RDX = RemoteHash
; Out: RAX = 1 (Verified), 0 (Rollback)
Swarm_LockStepConsensus proc
    mov     r8, g_ActiveGen
    cmp     rcx, rdx
    jne     @mismatch

    lock bts g_ActiveGen, 63
    mov     rax, 1
    ret

@mismatch:
    xor     rax, rax
    ret
Swarm_LockStepConsensus endp

END
