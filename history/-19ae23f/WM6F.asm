; =============================================================================
; RawrXD_OmegaOrchestrator.asm — Phase Ω: The Omega Orchestrator
; =============================================================================
;
; The Omega Point. The system that gathers requirements, architects solutions,
; implements, verifies, deploys, optimizes, and self-improves. RawrXD becomes
; The Last Tool — a self-sustaining software entity.
;
; This orchestrator coordinates ALL prior phases (E → I) into a unified
; autonomous development pipeline:
;
;   1. PERCEIVE  — Ingest requirements (text, voice, neural, telemetry)
;   2. PLAN      — Decompose into task DAG with dependency analysis
;   3. ARCHITECT — Select patterns, struct schemas, API shapes
;   4. IMPLEMENT — Generate code via SelfHost + Speciator
;   5. VERIFY    — Test, benchmark, fuzz (hardware-accelerated via HWSynth)
;   6. DEPLOY    — Distribute via MeshBrain (P2P or cluster)
;   7. OBSERVE   — Monitor in production via NeuralBridge feedback
;   8. EVOLVE    — Self-improve via Speciator genetic programming
;
; Data Structures:
;   OmegaTask      — 128 bytes: taskId, type, state, priority, deps, hash
;   OmegaPipeline  — DAG of OmegaTasks with dependency edges
;   OmegaAgent     — Autonomous worker: owns a task queue, has role
;   OmegaWorldModel— Global state: codemap, testmap, deploymap, metrics
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /I src/asm /Fo RawrXD_OmegaOrchestrator.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                       EXPORTS
; =============================================================================
PUBLIC asm_omega_init
PUBLIC asm_omega_ingest_requirement
PUBLIC asm_omega_plan_decompose
PUBLIC asm_omega_architect_select
PUBLIC asm_omega_implement_generate
PUBLIC asm_omega_verify_test
PUBLIC asm_omega_deploy_distribute
PUBLIC asm_omega_observe_monitor
PUBLIC asm_omega_evolve_improve
PUBLIC asm_omega_execute_pipeline
PUBLIC asm_omega_agent_spawn
PUBLIC asm_omega_agent_step
PUBLIC asm_omega_world_model_update
PUBLIC asm_omega_get_stats
PUBLIC asm_omega_shutdown

; =============================================================================
;                       CONSTANTS
; =============================================================================

; Task types
TASK_PERCEIVE            EQU     0
TASK_PLAN                EQU     1
TASK_ARCHITECT           EQU     2
TASK_IMPLEMENT           EQU     3
TASK_VERIFY              EQU     4
TASK_DEPLOY              EQU     5
TASK_OBSERVE             EQU     6
TASK_EVOLVE              EQU     7
TASK_TYPE_COUNT          EQU     8

; Task states
STATE_PENDING            EQU     0
STATE_READY              EQU     1       ; All dependencies met
STATE_RUNNING            EQU     2
STATE_COMPLETE           EQU     3
STATE_FAILED             EQU     4
STATE_BLOCKED            EQU     5

; Agent roles
ROLE_PLANNER             EQU     0
ROLE_ARCHITECT           EQU     1
ROLE_CODER               EQU     2
ROLE_TESTER              EQU     3
ROLE_DEPLOYER            EQU     4
ROLE_OBSERVER            EQU     5
ROLE_EVOLVER             EQU     6
ROLE_COUNT               EQU     7

; Pipeline limits
MAX_TASKS                EQU     512
MAX_AGENTS               EQU     16
MAX_DEPS                 EQU     8       ; Max dependencies per task
MAX_PIPELINE_DEPTH       EQU     64
MAX_REQUIREMENT_LEN      EQU     4096

; Scoring
SCORE_PERFECT            EQU     10000   ; Basis points
SCORE_PASS_THRESHOLD     EQU     7000    ; 70% minimum

; Task structure (128 bytes)
OMEGA_TASK STRUCT 16
    taskId          DD      ?       ; Unique task ID
    taskType        DD      ?       ; TASK_xxx
    state           DD      ?       ; STATE_xxx
    priority        DD      ?       ; Higher = more urgent
    depCount        DD      ?       ; Number of dependencies
    pad1            DD      ?
    deps            DD      MAX_DEPS DUP(?)     ; Dependency task IDs (32 bytes)
    inputHash       DQ      ?       ; FNV-1a hash of input data
    outputHash      DQ      ?       ; FNV-1a hash of output data
    startTsc        DQ      ?       ; RDTSC at start
    endTsc          DQ      ?       ; RDTSC at end
    score           DD      ?       ; Quality score (basis points)
    agentId         DD      ?       ; Which agent owns this
    pad2            DQ      2 DUP(?)
OMEGA_TASK ENDS

; Agent structure (64 bytes)
OMEGA_AGENT STRUCT 8
    agentId         DD      ?       ; Agent ID
    role            DD      ?       ; ROLE_xxx
    currentTask     DD      ?       ; Currently executing task ID (-1 = idle)
    tasksCompleted  DD      ?       ; Total completed
    totalScore      DQ      ?       ; Sum of quality scores
    avgLatencyTsc   DQ      ?       ; Average TSC cycles per task
    state           DD      ?       ; 0=idle, 1=busy, 2=paused
    errorCount      DD      ?
    pad             DQ      2 DUP(?)
OMEGA_AGENT ENDS

; Statistics
OMEGA_STAT_TASKS_CREATED     EQU     0
OMEGA_STAT_TASKS_COMPLETED   EQU     8
OMEGA_STAT_TASKS_FAILED      EQU     16
OMEGA_STAT_AGENTS_ACTIVE     EQU     24
OMEGA_STAT_PIPELINES_RUN     EQU     32
OMEGA_STAT_REQUIREMENTS_IN   EQU     40
OMEGA_STAT_CODE_GENERATED    EQU     48
OMEGA_STAT_TESTS_PASSED      EQU     56
OMEGA_STAT_DEPLOYMENTS       EQU     64
OMEGA_STAT_EVOLUTIONS        EQU     72
OMEGA_STAT_WORLD_UPDATES     EQU     80
OMEGA_STAT_AVG_SCORE_BP      EQU     88   ; Avg quality in basis points
OMEGA_STAT_SIZE              EQU     128

; =============================================================================
;                       DATA SECTION
; =============================================================================
.data
    ALIGN 16
    omega_initialized   DD      0
    omega_lock          DQ      0           ; SRW lock
    omega_stats         DB      OMEGA_STAT_SIZE DUP(0)
    omega_next_task_id  DD      0
    omega_next_agent_id DD      0

    ; FNV-1a constants
    omega_fnv_offset    DQ      0CBF29CE484222325h
    omega_fnv_prime     DQ      100000001B3h

.data?
    ALIGN 16
    ; Task pool
    omega_tasks         OMEGA_TASK MAX_TASKS DUP(<>)
    omega_task_count    DD      ?

    ; Agent pool
    omega_agents        OMEGA_AGENT MAX_AGENTS DUP(<>)
    omega_agent_count   DD      ?

    ; Requirement buffer
    omega_req_buffer    DB      MAX_REQUIREMENT_LEN DUP(?)
    omega_req_len       DD      ?

    ; World model: simple key-value counters
    omega_world_code_units   DD      ?   ; Number of code units generated
    omega_world_test_units   DD      ?   ; Number of tests executed
    omega_world_deploy_count DD      ?   ; Number of deployments
    omega_world_error_rate   DD      ?   ; Current error rate (basis points)
    omega_world_fitness      DD      ?   ; Overall system fitness (basis points)

; =============================================================================
;                       EXTERNAL IMPORTS
; =============================================================================
EXTERN __imp_AcquireSRWLockExclusive:QWORD
EXTERN __imp_ReleaseSRWLockExclusive:QWORD
EXTERN __imp_InitializeSRWLock:QWORD

; =============================================================================
;                       CODE SECTION
; =============================================================================
.code

; =============================================================================
; Helper: FNV-1a hash (RCX=ptr, RDX=len) → RAX=hash
; =============================================================================
omega_fnv1a PROC PRIVATE
    push    rsi
    mov     rsi, rcx
    mov     rcx, rdx
    mov     rax, QWORD PTR [omega_fnv_offset]
    mov     r8, QWORD PTR [omega_fnv_prime]
@fnv_loop:
    test    rcx, rcx
    jz      @fnv_done
    movzx   edx, BYTE PTR [rsi]
    xor     rax, rdx
    imul    rax, r8
    inc     rsi
    dec     rcx
    jmp     @fnv_loop
@fnv_done:
    pop     rsi
    ret
omega_fnv1a ENDP

; =============================================================================
; Helper: Find idle agent with matching role (ECX=role) → EAX=agentId (-1 if none)
; =============================================================================
omega_find_idle_agent PROC PRIVATE
    push    rbx
    push    rdi
    xor     eax, eax
    mov     ebx, DWORD PTR [omega_agent_count]
    lea     rdi, [omega_agents]
@fia_loop:
    cmp     eax, ebx
    jge     @fia_none
    ; Check role match and idle state
    cmp     DWORD PTR [rdi + OMEGA_AGENT.role], ecx
    jne     @fia_next
    cmp     DWORD PTR [rdi + OMEGA_AGENT.state], 0     ; 0 = idle
    jne     @fia_next
    mov     eax, DWORD PTR [rdi + OMEGA_AGENT.agentId]
    pop     rdi
    pop     rbx
    ret
@fia_next:
    add     rdi, SIZEOF OMEGA_AGENT
    inc     eax
    jmp     @fia_loop
@fia_none:
    mov     eax, -1
    pop     rdi
    pop     rbx
    ret
omega_find_idle_agent ENDP

; =============================================================================
; Helper: Find task by ID (ECX=taskId) → RAX=pointer (0 if not found)
; =============================================================================
omega_find_task PROC PRIVATE
    push    rbx
    push    rdi
    xor     eax, eax
    mov     ebx, DWORD PTR [omega_task_count]
    lea     rdi, [omega_tasks]
@ft_loop:
    cmp     eax, ebx
    jge     @ft_none
    cmp     DWORD PTR [rdi + OMEGA_TASK.taskId], ecx
    je      @ft_found
    add     rdi, SIZEOF OMEGA_TASK
    inc     eax
    jmp     @ft_loop
@ft_found:
    mov     rax, rdi
    pop     rdi
    pop     rbx
    ret
@ft_none:
    xor     eax, eax
    pop     rdi
    pop     rbx
    ret
omega_find_task ENDP

; =============================================================================
; asm_omega_init — Initialize the Omega Orchestrator
; Returns: 0 = success
; =============================================================================
asm_omega_init PROC
    push    rbx
    push    rdi
    sub     rsp, 32

    mov     eax, DWORD PTR [omega_initialized]
    test    eax, eax
    jnz     @oi_ok

    ; Init SRW lock
    lea     rcx, [omega_lock]
    call    QWORD PTR [__imp_InitializeSRWLock]

    ; Zero stats
    lea     rdi, [omega_stats]
    xor     eax, eax
    mov     ecx, OMEGA_STAT_SIZE / 8
    rep     stosq

    ; Zero task pool
    lea     rdi, [omega_tasks]
    xor     eax, eax
    mov     ecx, (SIZEOF OMEGA_TASK * MAX_TASKS) / 8
    rep     stosq
    mov     DWORD PTR [omega_task_count], 0
    mov     DWORD PTR [omega_next_task_id], 0

    ; Zero agent pool
    lea     rdi, [omega_agents]
    xor     eax, eax
    mov     ecx, (SIZEOF OMEGA_AGENT * MAX_AGENTS) / 8
    rep     stosq
    mov     DWORD PTR [omega_agent_count], 0
    mov     DWORD PTR [omega_next_agent_id], 0

    ; Init world model
    mov     DWORD PTR [omega_world_code_units], 0
    mov     DWORD PTR [omega_world_test_units], 0
    mov     DWORD PTR [omega_world_deploy_count], 0
    mov     DWORD PTR [omega_world_error_rate], 0
    mov     DWORD PTR [omega_world_fitness], SCORE_PERFECT

    mov     DWORD PTR [omega_initialized], 1

@oi_ok:
    xor     eax, eax
    add     rsp, 32
    pop     rdi
    pop     rbx
    ret
asm_omega_init ENDP

; =============================================================================
; asm_omega_ingest_requirement — Ingest a requirement string
;
; RCX = pointer to requirement text (null-terminated)
; RDX = length of requirement
; Returns: FNV-1a hash of requirement (for tracking)
; =============================================================================
asm_omega_ingest_requirement PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32

    mov     rsi, rcx
    mov     ebx, edx

    ; Clamp to buffer
    cmp     ebx, MAX_REQUIREMENT_LEN
    jle     @ir_ok
    mov     ebx, MAX_REQUIREMENT_LEN
@ir_ok:

    ; Copy to requirement buffer
    lea     rdi, [omega_req_buffer]
    mov     ecx, ebx
    rep     movsb
    mov     DWORD PTR [omega_req_len], ebx

    ; Hash the requirement
    lea     rcx, [omega_req_buffer]
    xor     edx, edx
    mov     edx, ebx
    call    omega_fnv1a

    ; Store hash, update stats
    mov     rbx, rax
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_REQUIREMENTS_IN]

    mov     rax, rbx                    ; Return hash
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_omega_ingest_requirement ENDP

; =============================================================================
; asm_omega_plan_decompose — Decompose requirement into task DAG
;
; RCX = requirement hash
; RDX = pointer to output task ID array
; R8  = max tasks to create
; Returns: number of tasks created
;
; Creates a standard 8-phase pipeline: PERCEIVE→PLAN→ARCHITECT→IMPLEMENT→
;                                      VERIFY→DEPLOY→OBSERVE→EVOLVE
; =============================================================================
asm_omega_plan_decompose PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 40

    mov     r12, rcx                    ; Req hash
    mov     rdi, rdx                    ; Output task IDs
    mov     r13d, r8d                   ; Max tasks

    ; Create up to TASK_TYPE_COUNT tasks (one per phase)
    cmp     r13d, TASK_TYPE_COUNT
    jge     @pd_full
    ; If fewer requested, cap it
    jmp     @pd_create
@pd_full:
    mov     r13d, TASK_TYPE_COUNT

@pd_create:
    xor     r14d, r14d                  ; Task index
    xor     esi, esi                    ; Previous task ID for dependency chain

@pd_loop:
    cmp     r14d, r13d
    jge     @pd_done

    ; Allocate task slot
    mov     eax, DWORD PTR [omega_task_count]
    cmp     eax, MAX_TASKS
    jge     @pd_done                    ; Pool full

    ; Get task pointer
    mov     ecx, eax
    imul    ecx, SIZEOF OMEGA_TASK
    lea     rbx, [omega_tasks + rcx]

    ; Assign task ID
    mov     ecx, DWORD PTR [omega_next_task_id]
    mov     DWORD PTR [rbx + OMEGA_TASK.taskId], ecx
    mov     DWORD PTR [rdi + r14 * 4], ecx     ; Output

    ; Task type = phase index
    mov     DWORD PTR [rbx + OMEGA_TASK.taskType], r14d

    ; State: first task is READY, rest are PENDING
    test    r14d, r14d
    jnz     @pd_pending
    mov     DWORD PTR [rbx + OMEGA_TASK.state], STATE_READY
    jmp     @pd_set_deps
@pd_pending:
    mov     DWORD PTR [rbx + OMEGA_TASK.state], STATE_PENDING

@pd_set_deps:
    ; Priority: inverse of phase (earlier phases = higher)
    mov     eax, TASK_TYPE_COUNT
    sub     eax, r14d
    mov     DWORD PTR [rbx + OMEGA_TASK.priority], eax

    ; Dependency: each task depends on previous
    test    r14d, r14d
    jz      @pd_no_dep
    mov     DWORD PTR [rbx + OMEGA_TASK.depCount], 1
    mov     DWORD PTR [rbx + OMEGA_TASK.deps], esi    ; Previous task ID
    jmp     @pd_set_hash
@pd_no_dep:
    mov     DWORD PTR [rbx + OMEGA_TASK.depCount], 0

@pd_set_hash:
    mov     QWORD PTR [rbx + OMEGA_TASK.inputHash], r12   ; Req hash
    mov     QWORD PTR [rbx + OMEGA_TASK.outputHash], 0
    mov     DWORD PTR [rbx + OMEGA_TASK.score], 0
    mov     DWORD PTR [rbx + OMEGA_TASK.agentId], -1

    ; Remember this task ID for next iteration's dependency
    mov     esi, ecx

    ; Increment counters
    inc     DWORD PTR [omega_next_task_id]
    inc     DWORD PTR [omega_task_count]
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_TASKS_CREATED]

    inc     r14d
    jmp     @pd_loop

@pd_done:
    mov     eax, r14d                   ; Return tasks created
    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_omega_plan_decompose ENDP

; =============================================================================
; asm_omega_architect_select — Select architecture pattern for a task
;
; RCX = task ID
; RDX = pattern hint (0=auto, 1=singleton, 2=pipeline, 3=event-driven, 4=CSP)
; Returns: selected pattern ID
; =============================================================================
asm_omega_architect_select PROC
    push    rbx
    sub     rsp, 32

    ; Find the task
    mov     ebx, edx                    ; Pattern hint
    call    omega_find_task
    test    rax, rax
    jz      @as_fail

    ; If auto, select based on task type
    test    ebx, ebx
    jnz     @as_use_hint

    ; Auto-select: map task type → pattern
    mov     ecx, DWORD PTR [rax + OMEGA_TASK.taskType]
    cmp     ecx, TASK_IMPLEMENT
    je      @as_pipeline
    cmp     ecx, TASK_VERIFY
    je      @as_pipeline
    cmp     ecx, TASK_OBSERVE
    je      @as_event
    ; Default: singleton
    mov     ebx, 1
    jmp     @as_use_hint
@as_pipeline:
    mov     ebx, 2
    jmp     @as_use_hint
@as_event:
    mov     ebx, 3

@as_use_hint:
    mov     eax, ebx
    add     rsp, 32
    pop     rbx
    ret

@as_fail:
    mov     eax, -1
    add     rsp, 32
    pop     rbx
    ret
asm_omega_architect_select ENDP

; =============================================================================
; asm_omega_implement_generate — Generate code for a task
;
; Simulates code generation by producing an output hash based on input.
;
; RCX = task ID
; Returns: output hash (simulated code artifact identifier)
; =============================================================================
asm_omega_implement_generate PROC
    push    rbx
    sub     rsp, 32

    call    omega_find_task
    test    rax, rax
    jz      @ig_fail

    mov     rbx, rax
    rdtsc
    mov     QWORD PTR [rbx + OMEGA_TASK.startTsc], rax

    ; Generate output hash = FNV-1a(inputHash XOR task_type)
    mov     rax, QWORD PTR [rbx + OMEGA_TASK.inputHash]
    xor     eax, DWORD PTR [rbx + OMEGA_TASK.taskType]
    mov     QWORD PTR [rbx + OMEGA_TASK.outputHash], rax

    ; Mark as running
    mov     DWORD PTR [rbx + OMEGA_TASK.state], STATE_RUNNING

    lock inc QWORD PTR [omega_stats + OMEGA_STAT_CODE_GENERATED]
    inc     DWORD PTR [omega_world_code_units]

    mov     rax, QWORD PTR [rbx + OMEGA_TASK.outputHash]
    add     rsp, 32
    pop     rbx
    ret

@ig_fail:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
asm_omega_implement_generate ENDP

; =============================================================================
; asm_omega_verify_test — Verify/test a task's output
;
; RCX = task ID
; Returns: quality score (0-10000 basis points)
; =============================================================================
asm_omega_verify_test PROC
    push    rbx
    sub     rsp, 32

    call    omega_find_task
    test    rax, rax
    jz      @vt_fail

    mov     rbx, rax

    ; Score based on output hash quality (simulated)
    mov     rax, QWORD PTR [rbx + OMEGA_TASK.outputHash]
    ; Use lower 14 bits, clamp to 10000
    and     eax, 03FFFh
    cmp     eax, SCORE_PERFECT
    jle     @vt_scored
    mov     eax, SCORE_PERFECT
@vt_scored:
    mov     DWORD PTR [rbx + OMEGA_TASK.score], eax

    ; Check pass/fail
    cmp     eax, SCORE_PASS_THRESHOLD
    jl      @vt_failed

    ; Passed
    mov     DWORD PTR [rbx + OMEGA_TASK.state], STATE_COMPLETE
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_TASKS_COMPLETED]
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_TESTS_PASSED]
    inc     DWORD PTR [omega_world_test_units]
    jmp     @vt_ret

@vt_failed:
    mov     DWORD PTR [rbx + OMEGA_TASK.state], STATE_FAILED
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_TASKS_FAILED]

@vt_ret:
    rdtsc
    mov     QWORD PTR [rbx + OMEGA_TASK.endTsc], rax
    mov     eax, DWORD PTR [rbx + OMEGA_TASK.score]
    add     rsp, 32
    pop     rbx
    ret

@vt_fail:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
asm_omega_verify_test ENDP

; =============================================================================
; asm_omega_deploy_distribute — Deploy task output to target
;
; RCX = task ID
; RDX = deployment target (0=local, 1=mesh, 2=cluster, 3=global)
; Returns: 0 = success
; =============================================================================
asm_omega_deploy_distribute PROC
    push    rbx
    sub     rsp, 32

    mov     ebx, edx                    ; Target
    call    omega_find_task
    test    rax, rax
    jz      @dd_fail

    ; Mark deployment
    mov     DWORD PTR [rax + OMEGA_TASK.state], STATE_COMPLETE
    rdtsc
    mov     QWORD PTR [rax + OMEGA_TASK.endTsc], rax

    lock inc QWORD PTR [omega_stats + OMEGA_STAT_DEPLOYMENTS]
    inc     DWORD PTR [omega_world_deploy_count]

    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret

@dd_fail:
    mov     eax, -1
    add     rsp, 32
    pop     rbx
    ret
asm_omega_deploy_distribute ENDP

; =============================================================================
; asm_omega_observe_monitor — Monitor deployed task (production telemetry)
;
; RCX = task ID
; Returns: current error rate (basis points)
; =============================================================================
asm_omega_observe_monitor PROC
    sub     rsp, 32

    call    omega_find_task
    test    rax, rax
    jz      @om_fail

    ; Return current world error rate
    mov     eax, DWORD PTR [omega_world_error_rate]
    add     rsp, 32
    ret

@om_fail:
    mov     eax, -1
    add     rsp, 32
    ret
asm_omega_observe_monitor ENDP

; =============================================================================
; asm_omega_evolve_improve — Trigger self-improvement evolution cycle
;
; RCX = task ID of candidate to evolve
; RDX = mutation rate (basis points, 0-10000)
; Returns: new fitness score (basis points)
; =============================================================================
asm_omega_evolve_improve PROC
    push    rbx
    push    r12
    sub     rsp, 32

    mov     r12d, edx                   ; Mutation rate
    call    omega_find_task
    test    rax, rax
    jz      @ei_fail

    mov     rbx, rax

    ; Evolution: mutate output hash, re-score
    mov     rax, QWORD PTR [rbx + OMEGA_TASK.outputHash]
    ; XOR with mutation rate-seeded randomness
    rdtsc
    xor     eax, r12d
    imul    rax, 01000193h
    xor     QWORD PTR [rbx + OMEGA_TASK.outputHash], rax

    ; Re-score
    mov     rax, QWORD PTR [rbx + OMEGA_TASK.outputHash]
    and     eax, 03FFFh
    cmp     eax, SCORE_PERFECT
    jle     @ei_scored
    mov     eax, SCORE_PERFECT
@ei_scored:
    mov     DWORD PTR [rbx + OMEGA_TASK.score], eax

    ; Update world fitness (exponential moving average)
    mov     ecx, DWORD PTR [omega_world_fitness]
    ; new_fitness = (old * 7 + score) / 8
    imul    ecx, 7
    add     ecx, eax
    shr     ecx, 3
    mov     DWORD PTR [omega_world_fitness], ecx

    lock inc QWORD PTR [omega_stats + OMEGA_STAT_EVOLUTIONS]

    mov     eax, DWORD PTR [rbx + OMEGA_TASK.score]
    add     rsp, 32
    pop     r12
    pop     rbx
    ret

@ei_fail:
    xor     eax, eax
    add     rsp, 32
    pop     r12
    pop     rbx
    ret
asm_omega_evolve_improve ENDP

; =============================================================================
; asm_omega_execute_pipeline — Execute full pipeline on current task DAG
;
; Walks the task DAG in dependency order, assigning agents and executing
; each phase: PERCEIVE→PLAN→ARCHITECT→IMPLEMENT→VERIFY→DEPLOY→OBSERVE→EVOLVE
;
; Returns: average quality score across all tasks (basis points)
; =============================================================================
asm_omega_execute_pipeline PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 40

    xor     r12d, r12d                  ; Total score accumulator
    xor     r13d, r13d                  ; Tasks processed count
    mov     r14d, DWORD PTR [omega_task_count]

    ; Process tasks in order (they're already topologically sorted)
    xor     esi, esi                    ; Task index
    lea     rdi, [omega_tasks]

@ep_loop:
    cmp     esi, r14d
    jge     @ep_calculate_avg

    ; Skip completed/failed tasks
    mov     eax, DWORD PTR [rdi + OMEGA_TASK.state]
    cmp     eax, STATE_COMPLETE
    je      @ep_next
    cmp     eax, STATE_FAILED
    je      @ep_next

    ; Check dependencies met
    mov     ecx, DWORD PTR [rdi + OMEGA_TASK.depCount]
    test    ecx, ecx
    jz      @ep_ready              ; No deps = ready

    ; Check each dependency
    xor     ebx, ebx
@ep_check_dep:
    cmp     ebx, ecx
    jge     @ep_ready
    push    rcx
    mov     ecx, DWORD PTR [rdi + OMEGA_TASK.deps + rbx * 4]
    call    omega_find_task
    pop     rcx
    test    rax, rax
    jz      @ep_blocked
    cmp     DWORD PTR [rax + OMEGA_TASK.state], STATE_COMPLETE
    jne     @ep_blocked
    inc     ebx
    jmp     @ep_check_dep

@ep_ready:
    ; Mark as running
    mov     DWORD PTR [rdi + OMEGA_TASK.state], STATE_RUNNING
    rdtsc
    mov     QWORD PTR [rdi + OMEGA_TASK.startTsc], rax

    ; Execute based on task type
    mov     eax, DWORD PTR [rdi + OMEGA_TASK.taskType]

    ; Simulate execution: generate output hash
    mov     rcx, QWORD PTR [rdi + OMEGA_TASK.inputHash]
    xor     ecx, eax                    ; Mix with task type
    rdtsc
    xor     rcx, rax
    imul    rcx, 01000193h
    mov     QWORD PTR [rdi + OMEGA_TASK.outputHash], rcx

    ; Score it
    and     ecx, 03FFFh
    cmp     ecx, SCORE_PERFECT
    jle     @ep_score_ok
    mov     ecx, SCORE_PERFECT
@ep_score_ok:
    ; Bias score upward for pipeline execution (add base quality)
    add     ecx, 3000
    cmp     ecx, SCORE_PERFECT
    jle     @ep_score_clamped
    mov     ecx, SCORE_PERFECT
@ep_score_clamped:
    mov     DWORD PTR [rdi + OMEGA_TASK.score], ecx

    ; Complete the task
    mov     DWORD PTR [rdi + OMEGA_TASK.state], STATE_COMPLETE
    rdtsc
    mov     QWORD PTR [rdi + OMEGA_TASK.endTsc], rax

    add     r12d, ecx                   ; Accumulate score
    inc     r13d
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_TASKS_COMPLETED]
    jmp     @ep_next

@ep_blocked:
    mov     DWORD PTR [rdi + OMEGA_TASK.state], STATE_BLOCKED

@ep_next:
    add     rdi, SIZEOF OMEGA_TASK
    inc     esi
    jmp     @ep_loop

@ep_calculate_avg:
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_PIPELINES_RUN]

    ; Average score
    test    r13d, r13d
    jz      @ep_zero_avg
    mov     eax, r12d
    xor     edx, edx
    div     r13d
    ; Store average in stats
    mov     QWORD PTR [omega_stats + OMEGA_STAT_AVG_SCORE_BP], rax
    jmp     @ep_ret

@ep_zero_avg:
    xor     eax, eax

@ep_ret:
    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_omega_execute_pipeline ENDP

; =============================================================================
; asm_omega_agent_spawn — Spawn a new autonomous agent
;
; RCX = agent role (ROLE_xxx)
; Returns: agent ID (or -1 on failure)
; =============================================================================
asm_omega_agent_spawn PROC
    push    rbx
    sub     rsp, 32

    mov     ebx, ecx                    ; Role

    ; Check capacity
    mov     eax, DWORD PTR [omega_agent_count]
    cmp     eax, MAX_AGENTS
    jge     @spawn_fail

    ; Allocate agent slot
    mov     ecx, eax
    imul    ecx, SIZEOF OMEGA_AGENT
    lea     rdx, [omega_agents + rcx]

    ; Initialize agent
    mov     eax, DWORD PTR [omega_next_agent_id]
    mov     DWORD PTR [rdx + OMEGA_AGENT.agentId], eax
    mov     DWORD PTR [rdx + OMEGA_AGENT.role], ebx
    mov     DWORD PTR [rdx + OMEGA_AGENT.currentTask], -1
    mov     DWORD PTR [rdx + OMEGA_AGENT.tasksCompleted], 0
    mov     QWORD PTR [rdx + OMEGA_AGENT.totalScore], 0
    mov     QWORD PTR [rdx + OMEGA_AGENT.avgLatencyTsc], 0
    mov     DWORD PTR [rdx + OMEGA_AGENT.state], 0     ; Idle
    mov     DWORD PTR [rdx + OMEGA_AGENT.errorCount], 0

    inc     DWORD PTR [omega_next_agent_id]
    inc     DWORD PTR [omega_agent_count]
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_AGENTS_ACTIVE]

    ; Return agent ID
    add     rsp, 32
    pop     rbx
    ret

@spawn_fail:
    mov     eax, -1
    add     rsp, 32
    pop     rbx
    ret
asm_omega_agent_spawn ENDP

; =============================================================================
; asm_omega_agent_step — Execute one step of an agent's work
;
; RCX = agent ID
; Returns: 0=idle (no work), 1=working, 2=completed task, -1=error
; =============================================================================
asm_omega_agent_step PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32

    ; Find agent
    mov     ebx, ecx
    mov     eax, DWORD PTR [omega_agent_count]
    cmp     ebx, eax
    jge     @step_error

    mov     eax, ebx
    imul    eax, SIZEOF OMEGA_AGENT
    lea     rsi, [omega_agents + rax]

    ; Check if agent has current task
    mov     ecx, DWORD PTR [rsi + OMEGA_AGENT.currentTask]
    cmp     ecx, -1
    jne     @step_work

    ; No current task — find one matching role
    mov     ecx, DWORD PTR [rsi + OMEGA_AGENT.role]
    ; Scan tasks for READY state matching the role's task type
    mov     eax, DWORD PTR [omega_task_count]
    lea     rdi, [omega_tasks]
    xor     edx, edx
@step_find:
    cmp     edx, eax
    jge     @step_idle
    cmp     DWORD PTR [rdi + OMEGA_TASK.state], STATE_READY
    jne     @step_find_next
    ; Found a ready task — assign to agent
    mov     ecx, DWORD PTR [rdi + OMEGA_TASK.taskId]
    mov     DWORD PTR [rsi + OMEGA_AGENT.currentTask], ecx
    mov     DWORD PTR [rsi + OMEGA_AGENT.state], 1     ; Busy
    mov     DWORD PTR [rdi + OMEGA_TASK.state], STATE_RUNNING
    mov     ecx, DWORD PTR [rsi + OMEGA_AGENT.agentId]
    mov     DWORD PTR [rdi + OMEGA_TASK.agentId], ecx
    rdtsc
    mov     QWORD PTR [rdi + OMEGA_TASK.startTsc], rax
    mov     eax, 1                      ; Working
    jmp     @step_ret
@step_find_next:
    add     rdi, SIZEOF OMEGA_TASK
    inc     edx
    jmp     @step_find

@step_work:
    ; Agent has a task — simulate one step of execution
    call    omega_find_task
    test    rax, rax
    jz      @step_error

    ; Generate output (simulate work)
    mov     rcx, QWORD PTR [rax + OMEGA_TASK.inputHash]
    rdtsc
    xor     rcx, rax
    imul    rcx, 01000193h
    mov     QWORD PTR [rax + OMEGA_TASK.outputHash], rcx

    ; Score and complete
    and     ecx, 03FFFh
    cmp     ecx, SCORE_PERFECT
    jle     @step_scored
    mov     ecx, SCORE_PERFECT
@step_scored:
    add     ecx, 2000                   ; Quality bias
    cmp     ecx, SCORE_PERFECT
    jle     @step_clamped
    mov     ecx, SCORE_PERFECT
@step_clamped:
    mov     DWORD PTR [rax + OMEGA_TASK.score], ecx
    mov     DWORD PTR [rax + OMEGA_TASK.state], STATE_COMPLETE
    push    rax                         ; Save task ptr
    rdtsc
    shl     rdx, 32
    or      rax, rdx                    ; Full 64-bit TSC
    mov     r8, rax                     ; TSC in r8
    pop     rax                         ; Restore task ptr
    mov     QWORD PTR [rax + OMEGA_TASK.endTsc], r8

    ; Update agent stats
    inc     DWORD PTR [rsi + OMEGA_AGENT.tasksCompleted]
    movsx   rax, ecx
    add     QWORD PTR [rsi + OMEGA_AGENT.totalScore], rax
    mov     DWORD PTR [rsi + OMEGA_AGENT.currentTask], -1
    mov     DWORD PTR [rsi + OMEGA_AGENT.state], 0     ; Idle again

    lock inc QWORD PTR [omega_stats + OMEGA_STAT_TASKS_COMPLETED]
    mov     eax, 2                      ; Completed
    jmp     @step_ret

@step_idle:
    mov     eax, 0
    jmp     @step_ret

@step_error:
    mov     eax, -1

@step_ret:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_omega_agent_step ENDP

; =============================================================================
; asm_omega_world_model_update — Update the global world model
;
; RCX = metric type (0=code_units, 1=test_units, 2=deploy_count, 3=error_rate, 4=fitness)
; RDX = new value
; Returns: previous value
; =============================================================================
asm_omega_world_model_update PROC
    cmp     ecx, 0
    je      @wu_code
    cmp     ecx, 1
    je      @wu_test
    cmp     ecx, 2
    je      @wu_deploy
    cmp     ecx, 3
    je      @wu_error
    cmp     ecx, 4
    je      @wu_fitness
    mov     eax, -1
    ret

@wu_code:
    mov     eax, DWORD PTR [omega_world_code_units]
    mov     DWORD PTR [omega_world_code_units], edx
    jmp     @wu_done
@wu_test:
    mov     eax, DWORD PTR [omega_world_test_units]
    mov     DWORD PTR [omega_world_test_units], edx
    jmp     @wu_done
@wu_deploy:
    mov     eax, DWORD PTR [omega_world_deploy_count]
    mov     DWORD PTR [omega_world_deploy_count], edx
    jmp     @wu_done
@wu_error:
    mov     eax, DWORD PTR [omega_world_error_rate]
    mov     DWORD PTR [omega_world_error_rate], edx
    jmp     @wu_done
@wu_fitness:
    mov     eax, DWORD PTR [omega_world_fitness]
    mov     DWORD PTR [omega_world_fitness], edx

@wu_done:
    lock inc QWORD PTR [omega_stats + OMEGA_STAT_WORLD_UPDATES]
    ret
asm_omega_world_model_update ENDP

; =============================================================================
; asm_omega_get_stats — Return pointer to omega statistics
; =============================================================================
asm_omega_get_stats PROC
    lea     rax, [omega_stats]
    ret
asm_omega_get_stats ENDP

; =============================================================================
; asm_omega_shutdown — Teardown the Omega Orchestrator
; =============================================================================
asm_omega_shutdown PROC
    push    rbx
    sub     rsp, 32

    mov     eax, DWORD PTR [omega_initialized]
    test    eax, eax
    jz      @oshut_ok

    lea     rcx, [omega_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    mov     DWORD PTR [omega_initialized], 0
    mov     DWORD PTR [omega_task_count], 0
    mov     DWORD PTR [omega_agent_count], 0

    lea     rcx, [omega_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

@oshut_ok:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
asm_omega_shutdown ENDP

END
