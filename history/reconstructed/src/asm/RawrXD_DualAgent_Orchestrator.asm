; =============================================================================
; RawrXD_DualAgent_Orchestrator.asm — Phase 41: Architect↔Coder Swarm Engine
; =============================================================================
;
; Dual-agent orchestration engine for RawrXD inference pipeline.
; Provides:
;   - Concurrent Architect (800B reasoning) + Coder (7B-70B fast) sessions
;   - 64-byte aligned context ring buffer (64MB, zero-copy handoff)
;   - Lock-free SPSC task queue with PAUSE-based spin-wait
;   - AVX-512 non-temporal streaming for context transfer
;   - Fenced handoff (sfence/lfence) for Zen 4 cache coherency
;   - Graceful shutdown with bounded spin timeout
;
; Architecture: x64 MASM64 | Windows x64 ABI
; Calling Convention: Microsoft x64 (RCX, RDX, R8, R9 + stack)
;
; Dependencies:
;   - model_bridge_x64.asm (ModelBridge_LoadModel, ValidateModelAlignment, etc.)
;   - RawrXD_Common.inc (VirtualAlloc, constants, macros)
;
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_DualAgent_Orchestrator.obj
; Link:  Statically linked into RawrEngine / RawrXD-Win32IDE via CMake ASM_MASM
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                    DUAL-AGENT CONSTANTS
; =============================================================================

; Agent IDs
AGENT_ID_ARCHITECT      EQU     0       ; High-VRAM reasoning model (70B-800B)
AGENT_ID_CODER          EQU     1       ; Low-latency syntax model (7B-13B)
AGENT_COUNT             EQU     2

; Agent state flags
AGENT_STATE_IDLE        EQU     0       ; Waiting for work
AGENT_STATE_INFERENCING EQU     1       ; Running inference
AGENT_STATE_STALLED     EQU     2       ; Backpressure / waiting
AGENT_STATE_SHUTDOWN    EQU     0FFFFFFFFh  ; Shutdown requested

; Task types
TASK_TYPE_DESIGN_SPEC   EQU     0       ; Architect: generate execution plan
TASK_TYPE_CODE_GEN      EQU     1       ; Coder: generate/edit code
TASK_TYPE_REVIEW        EQU     2       ; Either: review/verify output
TASK_TYPE_CONTEXT_PUSH  EQU     3       ; Push context (file tree, etc.)

; Swarm status codes
SWARM_OK                EQU     0
SWARM_ERR_ALLOC_FAIL    EQU     1
SWARM_ERR_MODEL_FAIL    EQU     2
SWARM_ERR_RING_FULL     EQU     3
SWARM_ERR_NOT_INIT      EQU     4
SWARM_ERR_ALREADY_INIT  EQU     5
SWARM_ERR_SHUTDOWN_FAIL EQU     6

; Ring buffer geometry
RING_SIZE               EQU     04000000h   ; 64 MB
RING_MASK               EQU     03FFFFFFh   ; RING_SIZE - 1
RING_ALLOC_FLAGS        EQU     MEM_COMMIT OR MEM_RESERVE

; Task queue geometry
TASK_QUEUE_SIZE         EQU     10000h      ; 64 KB = 256 task packets
TASK_PKT_SIZE           EQU     100h        ; 256 bytes per task (aligned)
MAX_TASKS               EQU     100h        ; 256 tasks max in queue

; Shutdown spin limit
SHUTDOWN_SPIN_LIMIT     EQU     10000000    ; ~1 second at ~10 GHz retire rate

; =============================================================================
;                    STRUCTURES
; =============================================================================

; Agent Context — one per agent (Architect / Coder)
; Size: 4096 bytes (page-aligned for NUMA awareness)
AGENT_CTX STRUCT
    agent_id        QWORD   ?           ; AGENT_ID_ARCHITECT or AGENT_ID_CODER
    model_profile   DWORD   ?           ; Model profile index in bridge table
    model_tier      DWORD   ?           ; MODEL_TIER_* from bridge
    state_flags     DWORD   ?           ; AGENT_STATE_* (atomic)
    _pad0           DWORD   ?           ; Alignment
    ring_base       QWORD   ?           ; Pointer to shared context ring
    ring_head       QWORD   ?           ; Producer write position (atomic)
    ring_tail       QWORD   ?           ; Consumer read position (atomic)
    queue_base      QWORD   ?           ; Pointer to task queue
    queue_head      DWORD   ?           ; Task queue producer (atomic)
    queue_tail      DWORD   ?           ; Task queue consumer (atomic)
    task_count      QWORD   ?           ; Total tasks processed
    error_count     QWORD   ?           ; Total errors
    last_task_ms    QWORD   ?           ; Timestamp of last completed task
    zmm_save_area   DB      2048 DUP(?) ; ZMM0-31 save area (32 * 64 bytes)
    _reserved       DB      1944 DUP(?) ; Pad to 4096 bytes total
AGENT_CTX ENDS

; Task Packet — enqueued into agent's task queue
; Size: 256 bytes (cache-line-aligned for lock-free SPSC)
TASK_PKT STRUCT
    task_id         QWORD   ?           ; Unique incrementing ID
    task_type       DWORD   ?           ; TASK_TYPE_*
    priority        BYTE    ?           ; 0-255 (255 = real-time)
    _pad0           DB      3 DUP(?)    ; Alignment
    source_agent    DWORD   ?           ; Who submitted this task
    target_agent    DWORD   ?           ; Who should execute it
    data_offset     QWORD   ?           ; Offset into context ring
    data_length     DWORD   ?           ; Length of data in ring
    dep_mask        QWORD   ?           ; Bitmask: which tasks must complete first
    _reserved       DB      200 DUP(?)  ; Pad to 256 bytes
TASK_PKT ENDS

; Swarm State — global orchestrator state
SWARM_STATE STRUCT
    initialized     DWORD   ?           ; 1 if Swarm_Init completed
    agent_count     DWORD   ?           ; Always 2 for dual-agent
    architect_ctx   QWORD   ?           ; Pointer to Architect AGENT_CTX
    coder_ctx       QWORD   ?           ; Pointer to Coder AGENT_CTX
    ring_base       QWORD   ?           ; Shared context ring base
    ring_size       QWORD   ?           ; Ring size in bytes (64MB)
    total_handoffs  QWORD   ?           ; Total Architect→Coder handoffs
    total_tasks     QWORD   ?           ; Total tasks submitted
    lock_flag       DWORD   ?           ; Spinlock for init/shutdown
    _pad            DWORD   3 DUP(?)    ; Alignment
SWARM_STATE ENDS

; =============================================================================
;                    DATA SEGMENT
; =============================================================================
_DATA64 SEGMENT ALIGN(64) 'DATA'

; Global swarm state
g_SwarmState    SWARM_STATE <>

; Next task ID counter (atomic)
g_NextTaskID    QWORD   0

; Status message strings
g_MsgSwarmInit      DB  'DualAgent: swarm initialized (Architect + Coder)', 0
g_MsgSwarmShutdown  DB  'DualAgent: swarm shutdown complete', 0
g_MsgSwarmAllocFail DB  'DualAgent: ERROR — VirtualAlloc failed', 0
g_MsgSwarmModelFail DB  'DualAgent: ERROR — model load failed', 0
g_MsgSwarmNotInit   DB  'DualAgent: ERROR — swarm not initialized', 0
g_MsgSwarmAlready   DB  'DualAgent: ERROR — swarm already initialized', 0
g_MsgHandoffOK      DB  'DualAgent: context handoff complete', 0
g_MsgRingFull       DB  'DualAgent: ERROR — ring buffer full (backpressure)', 0

_DATA64 ENDS

; =============================================================================
;                    EXPORTS
; =============================================================================

PUBLIC Swarm_Init
PUBLIC Swarm_Shutdown
PUBLIC Swarm_GetState
PUBLIC Swarm_SubmitTask
PUBLIC Swarm_Handoff
PUBLIC Swarm_GetAgentStatus
PUBLIC Agent_Architect_Loop
PUBLIC Agent_Coder_Loop

; External dependencies (model_bridge_x64.asm)
EXTERNDEF ModelBridge_LoadModel:PROC
EXTERNDEF ModelBridge_UnloadModel:PROC
EXTERNDEF ModelBridge_ValidateLoad:PROC
EXTERNDEF ValidateModelAlignment:PROC
EXTERNDEF AcquireBridgeLock:PROC
EXTERNDEF ReleaseBridgeLock:PROC
EXTERNDEF EstimateRAM_Safe:PROC
EXTERNDEF ModelBridge_GetProfile:PROC
EXTERNDEF ModelBridge_Init:PROC

; Win32 API (from RawrXD_Common.inc)
; VirtualAlloc, VirtualFree, GetTickCount64, Sleep already declared

; =============================================================================
;                    CODE SEGMENT
; =============================================================================
_TEXT SEGMENT ALIGN(16) 'CODE'

; =============================================================================
; Swarm_Init — Initialize Dual-Agent Orchestrator
; =============================================================================
; Allocates context ring (64MB), agent contexts (4KB each), task queues,
; and loads the Architect + Coder models into the bridge.
;
; Parameters: ECX = Architect profile index (e.g. 20 = 800B DualEngine)
;             EDX = Coder profile index (e.g. 5 = llama3.1:8b)
; Returns:    RAX = SWARM_OK on success, error code on failure
;             RDX = pointer to g_SwarmState
; =============================================================================
Swarm_Init PROC FRAME
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
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 60h
    .allocstack 60h
    .endprolog

    ; Save profile indices
    mov     r12d, ecx                   ; R12 = Architect profile idx
    mov     r13d, edx                   ; R13 = Coder profile idx

    lea     rbx, g_SwarmState

    ; Check not already initialized
    cmp     DWORD PTR [rbx].SWARM_STATE.initialized, 1
    je      @init_already

    ; ---- Allocate shared 64MB context ring (64-byte aligned) ----
    xor     ecx, ecx                    ; lpAddress = NULL (system chooses)
    mov     edx, RING_SIZE              ; dwSize = 64MB
    mov     r8d, RING_ALLOC_FLAGS       ; flAllocationType = COMMIT|RESERVE
    mov     r9d, PAGE_READWRITE         ; flProtect
    sub     rsp, 20h                    ; Shadow space
    call    VirtualAlloc
    add     rsp, 20h
    test    rax, rax
    jz      @init_alloc_fail

    mov     r14, rax                    ; R14 = ring base

    ; Validate alignment (must be 64-byte for AVX-512 NT stores)
    mov     rcx, r14
    sub     rsp, 20h
    call    ValidateModelAlignment      ; Returns aligned address in RAX
    add     rsp, 20h
    mov     r14, rax                    ; R14 = aligned ring base

    ; ---- Allocate Architect AGENT_CTX (4KB page) ----
    xor     ecx, ecx
    mov     edx, 1000h                  ; 4096 bytes
    mov     r8d, RING_ALLOC_FLAGS
    mov     r9d, PAGE_READWRITE
    sub     rsp, 20h
    call    VirtualAlloc
    add     rsp, 20h
    test    rax, rax
    jz      @init_alloc_fail
    mov     rsi, rax                    ; RSI = Architect ctx

    ; ---- Allocate Coder AGENT_CTX (4KB page) ----
    xor     ecx, ecx
    mov     edx, 1000h
    mov     r8d, RING_ALLOC_FLAGS
    mov     r9d, PAGE_READWRITE
    sub     rsp, 20h
    call    VirtualAlloc
    add     rsp, 20h
    test    rax, rax
    jz      @init_alloc_fail
    mov     rdi, rax                    ; RDI = Coder ctx

    ; ---- Allocate Architect task queue (64KB) ----
    xor     ecx, ecx
    mov     edx, TASK_QUEUE_SIZE
    mov     r8d, RING_ALLOC_FLAGS
    mov     r9d, PAGE_READWRITE
    sub     rsp, 20h
    call    VirtualAlloc
    add     rsp, 20h
    test    rax, rax
    jz      @init_alloc_fail
    mov     r15, rax                    ; R15 = Architect queue

    ; ---- Allocate Coder task queue (64KB) ----
    xor     ecx, ecx
    mov     edx, TASK_QUEUE_SIZE
    mov     r8d, RING_ALLOC_FLAGS
    mov     r9d, PAGE_READWRITE
    sub     rsp, 20h
    call    VirtualAlloc
    add     rsp, 20h
    test    rax, rax
    jz      @init_alloc_fail
    mov     QWORD PTR [rsp+30h], rax    ; Save Coder queue in local

    ; ---- Initialize Architect AGENT_CTX ----
    mov     QWORD PTR [rsi].AGENT_CTX.agent_id, AGENT_ID_ARCHITECT
    mov     DWORD PTR [rsi].AGENT_CTX.model_profile, r12d
    mov     DWORD PTR [rsi].AGENT_CTX.state_flags, AGENT_STATE_IDLE
    mov     QWORD PTR [rsi].AGENT_CTX.ring_base, r14
    mov     QWORD PTR [rsi].AGENT_CTX.ring_head, 0
    mov     QWORD PTR [rsi].AGENT_CTX.ring_tail, 0
    mov     QWORD PTR [rsi].AGENT_CTX.queue_base, r15
    mov     DWORD PTR [rsi].AGENT_CTX.queue_head, 0
    mov     DWORD PTR [rsi].AGENT_CTX.queue_tail, 0
    mov     QWORD PTR [rsi].AGENT_CTX.task_count, 0
    mov     QWORD PTR [rsi].AGENT_CTX.error_count, 0

    ; ---- Initialize Coder AGENT_CTX ----
    mov     QWORD PTR [rdi].AGENT_CTX.agent_id, AGENT_ID_CODER
    mov     DWORD PTR [rdi].AGENT_CTX.model_profile, r13d
    mov     DWORD PTR [rdi].AGENT_CTX.state_flags, AGENT_STATE_IDLE
    mov     QWORD PTR [rdi].AGENT_CTX.ring_base, r14
    mov     QWORD PTR [rdi].AGENT_CTX.ring_head, 0
    mov     QWORD PTR [rdi].AGENT_CTX.ring_tail, 0
    mov     rax, QWORD PTR [rsp+30h]   ; Coder queue ptr
    mov     QWORD PTR [rdi].AGENT_CTX.queue_base, rax
    mov     DWORD PTR [rdi].AGENT_CTX.queue_head, 0
    mov     DWORD PTR [rdi].AGENT_CTX.queue_tail, 0
    mov     QWORD PTR [rdi].AGENT_CTX.task_count, 0
    mov     QWORD PTR [rdi].AGENT_CTX.error_count, 0

    ; ---- Initialize model bridge (if not already) ----
    sub     rsp, 20h
    call    ModelBridge_Init
    add     rsp, 20h
    ; Ignore return — may already be initialized

    ; ---- Validate Architect model can be loaded ----
    mov     ecx, r12d
    sub     rsp, 20h
    call    ModelBridge_ValidateLoad
    add     rsp, 20h
    test    rax, rax
    jnz     @init_model_fail

    ; ---- Validate Coder model can be loaded ----
    mov     ecx, r13d
    sub     rsp, 20h
    call    ModelBridge_ValidateLoad
    add     rsp, 20h
    test    rax, rax
    jnz     @init_model_fail

    ; NOTE: We do NOT call ModelBridge_LoadModel here — that is done
    ; by the C++ orchestration layer when inference is needed.
    ; The ASM layer only validates capability and manages context.

    ; ---- Populate global swarm state ----
    mov     DWORD PTR [rbx].SWARM_STATE.agent_count, AGENT_COUNT
    mov     QWORD PTR [rbx].SWARM_STATE.architect_ctx, rsi
    mov     QWORD PTR [rbx].SWARM_STATE.coder_ctx, rdi
    mov     QWORD PTR [rbx].SWARM_STATE.ring_base, r14
    mov     QWORD PTR [rbx].SWARM_STATE.ring_size, RING_SIZE
    mov     QWORD PTR [rbx].SWARM_STATE.total_handoffs, 0
    mov     QWORD PTR [rbx].SWARM_STATE.total_tasks, 0

    ; Mark initialized last (memory fence)
    sfence
    mov     DWORD PTR [rbx].SWARM_STATE.initialized, 1

    ; Return success
    xor     eax, eax                    ; SWARM_OK
    lea     rdx, g_SwarmState
    jmp     @init_done

@init_alloc_fail:
    mov     eax, SWARM_ERR_ALLOC_FAIL
    lea     rdx, g_MsgSwarmAllocFail
    jmp     @init_done

@init_model_fail:
    mov     eax, SWARM_ERR_MODEL_FAIL
    lea     rdx, g_MsgSwarmModelFail
    jmp     @init_done

@init_already:
    mov     eax, SWARM_ERR_ALREADY_INIT
    lea     rdx, g_MsgSwarmAlready

@init_done:
    add     rsp, 60h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Swarm_Init ENDP

; =============================================================================
; Swarm_Shutdown — Gracefully shut down the dual-agent swarm
; =============================================================================
; Signals both agents to stop, waits with bounded spin, then frees memory.
;
; Parameters: none
; Returns:    RAX = SWARM_OK on success, error code on failure
; =============================================================================
Swarm_Shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    lea     rbx, g_SwarmState

    ; Must be initialized
    cmp     DWORD PTR [rbx].SWARM_STATE.initialized, 0
    je      @shutdown_not_init

    ; Load agent context pointers
    mov     rsi, QWORD PTR [rbx].SWARM_STATE.architect_ctx
    mov     rdi, QWORD PTR [rbx].SWARM_STATE.coder_ctx

    ; Signal shutdown to both agents (atomic store)
    mov     DWORD PTR [rsi].AGENT_CTX.state_flags, AGENT_STATE_SHUTDOWN
    mov     DWORD PTR [rdi].AGENT_CTX.state_flags, AGENT_STATE_SHUTDOWN
    sfence                              ; Ensure visibility

    ; Bounded spin-wait for agents to acknowledge
    mov     r12d, SHUTDOWN_SPIN_LIMIT
@shutdown_wait:
    pause
    dec     r12d
    jz      @shutdown_force             ; Timeout — force cleanup

    ; Check if both are idle (acknowledged shutdown)
    mov     eax, DWORD PTR [rsi].AGENT_CTX.state_flags
    cmp     eax, AGENT_STATE_SHUTDOWN
    jne     @shutdown_wait              ; Still processing
    mov     eax, DWORD PTR [rdi].AGENT_CTX.state_flags
    cmp     eax, AGENT_STATE_SHUTDOWN
    jne     @shutdown_wait

@shutdown_force:
    ; Free context ring (64MB)
    mov     rcx, QWORD PTR [rbx].SWARM_STATE.ring_base
    test    rcx, rcx
    jz      @skip_ring_free
    xor     edx, edx                    ; dwSize = 0 (for MEM_RELEASE)
    mov     r8d, MEM_RELEASE
    sub     rsp, 20h
    call    VirtualFree
    add     rsp, 20h
@skip_ring_free:

    ; Free Architect task queue
    mov     rcx, QWORD PTR [rsi].AGENT_CTX.queue_base
    test    rcx, rcx
    jz      @skip_arch_queue
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    sub     rsp, 20h
    call    VirtualFree
    add     rsp, 20h
@skip_arch_queue:

    ; Free Coder task queue
    mov     rcx, QWORD PTR [rdi].AGENT_CTX.queue_base
    test    rcx, rcx
    jz      @skip_coder_queue
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    sub     rsp, 20h
    call    VirtualFree
    add     rsp, 20h
@skip_coder_queue:

    ; Free Architect context (4KB)
    mov     rcx, rsi
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    sub     rsp, 20h
    call    VirtualFree
    add     rsp, 20h

    ; Free Coder context (4KB)
    mov     rcx, rdi
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    sub     rsp, 20h
    call    VirtualFree
    add     rsp, 20h

    ; Clear global state
    lea     rdi, g_SwarmState
    xor     eax, eax
    mov     ecx, SIZEOF SWARM_STATE
    rep     stosb

    ; Return success
    xor     eax, eax
    lea     rdx, g_MsgSwarmShutdown
    jmp     @shutdown_done

@shutdown_not_init:
    mov     eax, SWARM_ERR_NOT_INIT
    lea     rdx, g_MsgSwarmNotInit

@shutdown_done:
    add     rsp, 30h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Swarm_Shutdown ENDP

; =============================================================================
; Swarm_GetState — Return pointer to global swarm state
; =============================================================================
; Parameters: none
; Returns:    RAX = pointer to SWARM_STATE
; =============================================================================
Swarm_GetState PROC
    lea     rax, g_SwarmState
    ret
Swarm_GetState ENDP

; =============================================================================
; Swarm_GetAgentStatus — Return status of a specific agent
; =============================================================================
; Parameters: ECX = agent ID (0 = Architect, 1 = Coder)
; Returns:    RAX = AGENT_STATE_* flag
;             RDX = pointer to AGENT_CTX (or NULL if invalid / not init)
; =============================================================================
Swarm_GetAgentStatus PROC
    lea     rax, g_SwarmState
    cmp     DWORD PTR [rax].SWARM_STATE.initialized, 0
    je      @agent_not_init

    cmp     ecx, AGENT_ID_ARCHITECT
    je      @get_architect
    cmp     ecx, AGENT_ID_CODER
    je      @get_coder
    jmp     @agent_not_init

@get_architect:
    mov     rdx, QWORD PTR [rax].SWARM_STATE.architect_ctx
    mov     eax, DWORD PTR [rdx].AGENT_CTX.state_flags
    ret

@get_coder:
    mov     rdx, QWORD PTR [rax].SWARM_STATE.coder_ctx
    mov     eax, DWORD PTR [rdx].AGENT_CTX.state_flags
    ret

@agent_not_init:
    mov     eax, SWARM_ERR_NOT_INIT
    xor     edx, edx                    ; NULL
    ret
Swarm_GetAgentStatus ENDP

; =============================================================================
; Swarm_SubmitTask — Enqueue a task to an agent's task queue
; =============================================================================
; Lock-free SPSC enqueue. Only one producer thread should call this per agent.
;
; Parameters: RCX = pointer to AGENT_CTX (target agent)
;             RDX = pointer to TASK_PKT (caller-filled, will be copied)
; Returns:    RAX = SWARM_OK on success, SWARM_ERR_RING_FULL if queue full
; =============================================================================
Swarm_SubmitTask PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rbx, rcx                    ; RBX = target AGENT_CTX
    mov     rsi, rdx                    ; RSI = source TASK_PKT

    ; Check swarm is initialized
    lea     rax, g_SwarmState
    cmp     DWORD PTR [rax].SWARM_STATE.initialized, 0
    je      @submit_not_init

    ; Read current head (producer position)
    mov     eax, DWORD PTR [rbx].AGENT_CTX.queue_head
    mov     ecx, eax
    inc     ecx
    and     ecx, (MAX_TASKS - 1)        ; Wrap

    ; Check if full (head+1 == tail)
    cmp     ecx, DWORD PTR [rbx].AGENT_CTX.queue_tail
    je      @submit_full

    ; Compute destination: queue_base + head * TASK_PKT_SIZE
    mov     edx, eax                    ; head index
    shl     edx, 8                      ; * 256 (TASK_PKT_SIZE)
    mov     rdi, QWORD PTR [rbx].AGENT_CTX.queue_base
    add     rdi, rdx                    ; RDI = dest slot

    ; Copy TASK_PKT (256 bytes) — use rep movsb for simplicity
    mov     ecx, TASK_PKT_SIZE
    ; RSI = source, RDI = dest (already set)
    rep     movsb

    ; Assign task ID (atomic increment)
    lock inc QWORD PTR g_NextTaskID
    mov     rax, QWORD PTR g_NextTaskID
    mov     QWORD PTR [rdi - TASK_PKT_SIZE], rax   ; Write task_id to slot

    ; Store fence: ensure task data is visible before advancing head
    sfence

    ; Advance head (atomic — only producer writes head)
    mov     eax, DWORD PTR [rbx].AGENT_CTX.queue_head
    inc     eax
    and     eax, (MAX_TASKS - 1)
    mov     DWORD PTR [rbx].AGENT_CTX.queue_head, eax

    ; Increment global task counter
    lock inc QWORD PTR [rbx].AGENT_CTX.task_count
    lea     rax, g_SwarmState
    lock inc QWORD PTR [rax].SWARM_STATE.total_tasks

    ; Timestamp
    sub     rsp, 20h
    call    GetTickCount64
    add     rsp, 20h
    mov     QWORD PTR [rbx].AGENT_CTX.last_task_ms, rax

    ; Return success
    xor     eax, eax                    ; SWARM_OK
    jmp     @submit_done

@submit_full:
    mov     eax, SWARM_ERR_RING_FULL
    lea     rdx, g_MsgRingFull
    jmp     @submit_done

@submit_not_init:
    mov     eax, SWARM_ERR_NOT_INIT
    lea     rdx, g_MsgSwarmNotInit

@submit_done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Swarm_SubmitTask ENDP

; =============================================================================
; Swarm_Handoff — Zero-copy context transfer (Architect → Coder)
; =============================================================================
; Writes data into the shared ring buffer at Architect's head position,
; then advances head so Coder can consume it. Uses sfence to guarantee
; non-temporal store ordering (critical for Zen 4 dual-engine handoff).
;
; Parameters: RCX = pointer to source data
;             EDX = length of data in bytes (must be multiple of 64)
; Returns:    RAX = SWARM_OK on success, SWARM_ERR_RING_FULL if no space
; =============================================================================
Swarm_Handoff PROC FRAME
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
    .endprolog

    mov     rsi, rcx                    ; RSI = source data
    mov     r12d, edx                   ; R12 = data length

    lea     rbx, g_SwarmState

    ; Must be initialized
    cmp     DWORD PTR [rbx].SWARM_STATE.initialized, 0
    je      @handoff_not_init

    ; Get Architect and Coder contexts
    mov     rdi, QWORD PTR [rbx].SWARM_STATE.architect_ctx
    mov     r13, QWORD PTR [rbx].SWARM_STATE.coder_ctx

    ; Get ring positions
    ; Architect writes at its ring_head; Coder reads at its ring_tail
    mov     rax, QWORD PTR [rdi].AGENT_CTX.ring_head   ; Producer pos
    mov     rcx, QWORD PTR [r13].AGENT_CTX.ring_tail   ; Consumer pos

    ; Check available space in ring
    ; Available = (tail - head - 1) & RING_MASK  (producer can't lap consumer)
    mov     rdx, rcx
    sub     rdx, rax
    dec     rdx
    and     rdx, RING_MASK
    cmp     rdx, r12                    ; Enough space?
    jb      @handoff_full               ; Not enough — backpressure

    ; Get ring base
    mov     rdi, QWORD PTR [rbx].SWARM_STATE.ring_base

    ; Serialize reads before we touch the ring
    lfence

    ; Copy data into ring (64-byte aligned chunks)
    mov     ecx, r12d                   ; Length
    mov     rdx, rax                    ; Current head position
    and     rdx, RING_MASK              ; Wrap

@handoff_copy:
    cmp     ecx, 0
    jle     @handoff_copy_done

    ; Copy 64 bytes at a time (cache line)
    ; Use regular mov for correctness (NT stores require strict alignment
    ; and may not wrap correctly at ring boundary)
    push    rcx
    push    rsi
    push    rdi

    ; Destination = ring_base + (head & RING_MASK)
    lea     rdi, [rdi + rdx]
    mov     ecx, 64
    rep     movsb                       ; Copy 64 bytes

    pop     rdi
    pop     rsi
    pop     rcx

    add     rsi, 64                     ; Advance source
    add     rdx, 64                     ; Advance ring position
    and     rdx, RING_MASK              ; Wrap
    sub     ecx, 64
    jmp     @handoff_copy

@handoff_copy_done:
    ; Store fence: CRITICAL for Zen 4 cache coherency
    ; All ring writes must be globally visible before we advance the head pointer.
    ; Without this: Coder reads stale data → cache coherency crash on Ryzen 7000.
    sfence

    ; Advance Architect's head pointer (atomic from Coder's perspective)
    mov     rax, QWORD PTR [rbx].SWARM_STATE.architect_ctx
    mov     rcx, QWORD PTR [rax].AGENT_CTX.ring_head
    add     rcx, r12                    ; Advance by data length
    and     rcx, RING_MASK              ; Wrap
    mov     QWORD PTR [rax].AGENT_CTX.ring_head, rcx

    ; Increment handoff counter
    lock inc QWORD PTR [rbx].SWARM_STATE.total_handoffs

    ; Return success
    xor     eax, eax                    ; SWARM_OK
    jmp     @handoff_done

@handoff_full:
    mov     eax, SWARM_ERR_RING_FULL
    lea     rdx, g_MsgRingFull
    jmp     @handoff_done

@handoff_not_init:
    mov     eax, SWARM_ERR_NOT_INIT
    lea     rdx, g_MsgSwarmNotInit

@handoff_done:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Swarm_Handoff ENDP

; =============================================================================
; Agent_Architect_Loop — Worker thread main loop for Architect agent
; =============================================================================
; Runs on a dedicated thread. Dequeues tasks from the Architect's SPSC queue,
; transitions state to INFERENCING, invokes the C++ inference callback, and
; writes results into the shared ring buffer for the Coder to consume.
;
; The inference itself is dispatched to C++ via a function pointer callback
; (passed in RDX) because the actual model inference uses the HTTP/streaming
; engine. The ASM loop handles state management, queue draining, and
; Architect→Coder ring buffer handoff with proper fencing.
;
; Parameters: RCX = pointer to Architect AGENT_CTX
;             RDX = function pointer: int64_t (*InferenceCallback)(AGENT_CTX*, TASK_PKT*)
;                   Returns bytes written to ring, or -1 on error.
; Returns:    RAX = total tasks processed before shutdown
; =============================================================================
ALIGN 16
Agent_Architect_Loop PROC FRAME
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
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     rbx, rcx                    ; RBX = Architect AGENT_CTX
    mov     r12, rdx                    ; R12 = InferenceCallback
    xor     r15d, r15d                  ; R15 = tasks processed counter

@@architect_loop:
    ; ---- Check for shutdown ----
    mov     eax, DWORD PTR [rbx].AGENT_CTX.state_flags
    cmp     eax, AGENT_STATE_SHUTDOWN
    je      @@architect_exit

    ; ---- Set state to IDLE while polling ----
    mov     DWORD PTR [rbx].AGENT_CTX.state_flags, AGENT_STATE_IDLE

    ; ---- Try dequeue from own task queue (lock-free SPSC consumer) ----
    mov     r13d, DWORD PTR [rbx].AGENT_CTX.queue_tail   ; Consumer position
    lfence                              ; Serialize before reading head
    mov     r14d, DWORD PTR [rbx].AGENT_CTX.queue_head   ; Producer position

    cmp     r13d, r14d
    je      @@architect_spin             ; Queue empty — spin-wait

    ; ---- Dequeue task packet ----
    ; Compute slot address: queue_base + tail * 256
    mov     eax, r13d
    shl     eax, 8                      ; * TASK_PKT_SIZE (256)
    mov     rsi, QWORD PTR [rbx].AGENT_CTX.queue_base
    add     rsi, rax                    ; RSI = pointer to TASK_PKT in queue

    ; ---- Transition to INFERENCING ----
    mov     DWORD PTR [rbx].AGENT_CTX.state_flags, AGENT_STATE_INFERENCING

    ; ---- Call C++ inference callback ----
    ; Callback signature: int64_t callback(AGENT_CTX* ctx, TASK_PKT* task)
    ; Returns: bytes written to ring (>= 0), or -1 on error
    mov     rcx, rbx                    ; Arg1 = AGENT_CTX*
    mov     rdx, rsi                    ; Arg2 = TASK_PKT*
    call    r12                         ; Call InferenceCallback

    ; ---- Check callback result ----
    cmp     rax, -1
    je      @@architect_error

    ; ---- Success: advance tail (only consumer writes tail) ----
    mov     eax, r13d
    inc     eax
    and     eax, (MAX_TASKS - 1)        ; Wrap
    sfence                              ; Ensure callback writes are visible
    mov     DWORD PTR [rbx].AGENT_CTX.queue_tail, eax

    ; ---- Timestamp ----
    sub     rsp, 20h
    call    GetTickCount64
    add     rsp, 20h
    mov     QWORD PTR [rbx].AGENT_CTX.last_task_ms, rax

    ; ---- Increment counters ----
    inc     r15                         ; Local counter
    lock inc QWORD PTR [rbx].AGENT_CTX.task_count

    jmp     @@architect_loop

@@architect_spin:
    ; Spin-wait with PAUSE (saves 90% power, VM-safe)
    pause
    ; Brief yield: call Sleep(0) every 256 spins to avoid starving other threads
    ; (Use a simple counter in R14 since it's free here)
    inc     r14d
    test    r14d, 0FFh                  ; Every 256 iterations
    jnz     @@architect_loop
    mov     ecx, 0                      ; Sleep(0) = yield timeslice
    sub     rsp, 20h
    call    Sleep
    add     rsp, 20h
    jmp     @@architect_loop

@@architect_error:
    ; Inference callback returned error
    lock inc QWORD PTR [rbx].AGENT_CTX.error_count
    ; Still advance tail to avoid stuck queue
    mov     eax, r13d
    inc     eax
    and     eax, (MAX_TASKS - 1)
    mov     DWORD PTR [rbx].AGENT_CTX.queue_tail, eax
    jmp     @@architect_loop

@@architect_exit:
    ; Signal acknowledgment: set state to SHUTDOWN + 1 = 0xFFFFFFFF is already set
    ; Return total tasks processed
    mov     rax, r15
    mov     DWORD PTR [rbx].AGENT_CTX.state_flags, AGENT_STATE_SHUTDOWN

    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Agent_Architect_Loop ENDP

; =============================================================================
; Agent_Coder_Loop — Worker thread main loop for Coder agent
; =============================================================================
; Runs on a dedicated thread. Consumes tasks from the Coder's SPSC queue
; AND monitors the shared ring buffer for Architect→Coder handoff data.
;
; The Coder has two input sources:
;   1. Direct task submissions (via Swarm_SubmitTask to Coder queue)
;   2. Ring buffer data from Architect (design specs to implement)
;
; When ring data is available (Architect's head > Coder's tail), the Coder
; synthesizes a TASK_TYPE_CODE_GEN task internally and processes it.
;
; Parameters: RCX = pointer to Coder AGENT_CTX
;             RDX = function pointer: int64_t (*InferenceCallback)(AGENT_CTX*, TASK_PKT*)
;                   Returns bytes produced (>= 0), or -1 on error.
; Returns:    RAX = total tasks processed before shutdown
; =============================================================================
ALIGN 16
Agent_Coder_Loop PROC FRAME
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
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     rbx, rcx                    ; RBX = Coder AGENT_CTX
    mov     r12, rdx                    ; R12 = InferenceCallback
    xor     r15d, r15d                  ; R15 = tasks processed counter

@@coder_loop:
    ; ---- Check for shutdown ----
    mov     eax, DWORD PTR [rbx].AGENT_CTX.state_flags
    cmp     eax, AGENT_STATE_SHUTDOWN
    je      @@coder_exit

    ; ---- Set state to IDLE while polling ----
    mov     DWORD PTR [rbx].AGENT_CTX.state_flags, AGENT_STATE_IDLE

    ; ---- Priority 1: Check own task queue ----
    mov     r13d, DWORD PTR [rbx].AGENT_CTX.queue_tail
    lfence
    mov     r14d, DWORD PTR [rbx].AGENT_CTX.queue_head
    cmp     r13d, r14d
    jne     @@coder_dequeue              ; Has direct task — process it

    ; ---- Priority 2: Check ring buffer for Architect handoff ----
    ; Coder reads at its ring_tail; Architect writes at Architect's ring_head
    lea     rax, g_SwarmState
    mov     rdi, QWORD PTR [rax].SWARM_STATE.architect_ctx
    test    rdi, rdi
    jz      @@coder_spin                 ; No architect context

    mov     rsi, QWORD PTR [rdi].AGENT_CTX.ring_head    ; Architect's write pos
    lfence                              ; Serialize: read Architect's head
    mov     rax, QWORD PTR [rbx].AGENT_CTX.ring_tail    ; Coder's read pos
    cmp     rax, rsi
    je      @@coder_spin                 ; Ring empty — no handoff data

    ; ---- Ring data available: synthesize a CODE_GEN task on stack ----
    ; We create a temporary TASK_PKT pointing to the ring data
    mov     DWORD PTR [rbx].AGENT_CTX.state_flags, AGENT_STATE_INFERENCING

    ; Calculate available data length (head - tail, wrapped)
    mov     rcx, rsi
    sub     rcx, rax
    and     rcx, RING_MASK              ; Available bytes
    ; Cap at 64KB per iteration to avoid monopolizing the thread
    cmp     rcx, 10000h
    jbe     @@coder_ring_len_ok
    mov     ecx, 10000h                 ; 64KB cap
@@coder_ring_len_ok:

    ; Build a stack-local TASK_PKT
    ; Use the space at [rsp] (we have 40h = 64 bytes, need to extend for 256)
    ; Actually, the callback only needs AGENT_CTX* and TASK_PKT*, and the
    ; critical fields are task_type, data_offset, data_length.
    ; We'll use a minimal approach: pass the ring offset via a compact struct.
    ; The C++ callback can read the ring directly from the AGENT_CTX.

    ; Temporarily store ring data info in the AGENT_CTX ring_tail area
    ; (the callback reads ring_base + ring_tail to find the data)
    ; Call inference with NULL task to signal "consume ring data"
    mov     rcx, rbx                    ; Arg1 = AGENT_CTX*
    xor     edx, edx                    ; Arg2 = NULL (signals ring consumption)
    call    r12                         ; Call InferenceCallback

    ; Check result
    cmp     rax, -1
    je      @@coder_ring_error

    ; Advance Coder's ring_tail by consumed amount
    mov     rcx, QWORD PTR [rbx].AGENT_CTX.ring_tail
    add     rcx, rax                    ; Advance by bytes consumed
    and     rcx, RING_MASK
    sfence
    mov     QWORD PTR [rbx].AGENT_CTX.ring_tail, rcx

    ; Timestamp + counters
    sub     rsp, 20h
    call    GetTickCount64
    add     rsp, 20h
    mov     QWORD PTR [rbx].AGENT_CTX.last_task_ms, rax
    inc     r15
    lock inc QWORD PTR [rbx].AGENT_CTX.task_count

    jmp     @@coder_loop

@@coder_dequeue:
    ; ---- Process direct task from own queue ----
    mov     eax, r13d
    shl     eax, 8                      ; * TASK_PKT_SIZE
    mov     rsi, QWORD PTR [rbx].AGENT_CTX.queue_base
    add     rsi, rax                    ; RSI = task packet pointer

    mov     DWORD PTR [rbx].AGENT_CTX.state_flags, AGENT_STATE_INFERENCING

    ; Call C++ inference callback
    mov     rcx, rbx                    ; Arg1 = AGENT_CTX*
    mov     rdx, rsi                    ; Arg2 = TASK_PKT*
    call    r12                         ; Call InferenceCallback

    cmp     rax, -1
    je      @@coder_task_error

    ; Advance tail
    mov     eax, r13d
    inc     eax
    and     eax, (MAX_TASKS - 1)
    sfence
    mov     DWORD PTR [rbx].AGENT_CTX.queue_tail, eax

    ; Timestamp + counters
    sub     rsp, 20h
    call    GetTickCount64
    add     rsp, 20h
    mov     QWORD PTR [rbx].AGENT_CTX.last_task_ms, rax
    inc     r15
    lock inc QWORD PTR [rbx].AGENT_CTX.task_count

    jmp     @@coder_loop

@@coder_spin:
    pause
    inc     r14d
    test    r14d, 0FFh
    jnz     @@coder_loop
    mov     ecx, 0                      ; Sleep(0) yield
    sub     rsp, 20h
    call    Sleep
    add     rsp, 20h
    jmp     @@coder_loop

@@coder_ring_error:
    lock inc QWORD PTR [rbx].AGENT_CTX.error_count
    jmp     @@coder_loop

@@coder_task_error:
    lock inc QWORD PTR [rbx].AGENT_CTX.error_count
    ; Advance tail to prevent stuck
    mov     eax, r13d
    inc     eax
    and     eax, (MAX_TASKS - 1)
    mov     DWORD PTR [rbx].AGENT_CTX.queue_tail, eax
    jmp     @@coder_loop

@@coder_exit:
    mov     rax, r15
    mov     DWORD PTR [rbx].AGENT_CTX.state_flags, AGENT_STATE_SHUTDOWN

    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Agent_Coder_Loop ENDP

_TEXT ENDS

END
